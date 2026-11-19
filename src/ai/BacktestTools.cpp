// Skill "backtest" (user's request, 2026-11-18): the cubes' models backtest strategies with Deepwater's own Strategy
// tester and its quant checks, and see how many strategies they have tried on the same data.
//   bt_indicators     the indicators a rule strategy may use (ids, inputs, outputs)
//   bt_run            a rule strategy (the tester's JSON) or a Pine strategy() the model writes, on a market and
//                     timeframe: Data › Technical data' stored bars when it holds them (BTCUSDT, 1m … 1W), Charts' sources
//                     otherwise; stats, yearly returns, last trades
//   bt_quant_checks   the gold box: walk-forward, overfitting (PBO), deflated Sharpe, cost stress, nearby settings,
//                     Monte Carlo of the trades
//   bt_trials         the strategies this environment has backtested on each market and timeframe, and the best one's
//                     Sharpe deflated by how many were tried (the more tries, the higher a lucky best)
// Every figure comes from the engines (charts::bt, charts::quant, PineTS), never from the model. One backtest at a time.
// Nothing is written on the user's side: no strategy saved, no order; only the environment's own trial ledger
// (<env>/backtest-trials.jsonl). A Pine script runs in PineTS's sandbox (no network, no files, time and memory capped).
#include "ai/Tools.h"
#include "ai/Slots.h"
#include "charts/core/Backtest.h"
#include "charts/core/Indicators.h"
#include "charts/core/PineQuant.h"
#include "charts/core/PineStrategy.h"
#include "charts/core/Quant.h"
#include "charts/data/Providers.h"
#include "core/Paths.h"
#include "technicaldata/TechnicalDataBars.h"
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataStore.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThread>
#include <QTimeZone>
#include <cmath>

namespace ai {
namespace {
constexpr int kMaxChars = 6000;
constexpr int RULES_BARS = 200000, PINE_BARS = 50000, RULES_QUANT_BARS = 100000, PINE_QUANT_BARS = 20000, PROVIDER_BARS = 20000;
bool g_busy = false;

ToolResult okText(QString s) {
  if (s.size() > kMaxChars) s = s.left(kMaxChars) + "\n[cut: longer than " + QString::number(kMaxChars) + " characters]";
  return {true, s};
}
ToolResult fail(const QString& s) { return {false, s}; }
QString strArg(const QJsonObject& a, const char* k) { return a.value(k).isString() ? a.value(k).toString().trimmed() : QString(); }
int intArg(const QJsonObject& a, const char* k, int def, int lo, int hi) {
  const QJsonValue v = a.value(k);
  bool ok = v.isDouble();
  int n = ok ? v.toInt() : 0;
  if (v.isString()) n = v.toString().toInt(&ok);
  return qBound(lo, ok ? n : def, hi);
}
QString num(double v, int dec = 2) { return std::isfinite(v) ? QString::number(v, 'f', dec) : QStringLiteral("n/a"); }
QString price(double v) {   // enough digits for a currency pair as for bitcoin
  if (!std::isfinite(v)) return QStringLiteral("n/a");
  const double a = std::fabs(v);
  return QString::number(v, 'f', a >= 1000 ? 2 : a >= 10 ? 3 : 5);
}
QString day(qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm"); }

// ------------------------------------------------------------------------------------------------ what to test
struct Spec {
  QString error;
  bool pine = false;
  QString source;            // Pine
  QJsonObject strategy;      // rules
  QString market, tf;
  int bars = 3000;
  QString key() const { return market + "|" + tf; }
  QString hash() const {     // the same strategy tried twice counts once
    const QByteArray what = pine ? source.toUtf8() : QJsonDocument(strategy).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(what, QCryptographicHash::Sha1).toHex().left(12));
  }
  QString label() const {
    if (pine) {
      static const QRegularExpression title(R"(strategy\s*\(\s*["']([^"']*)["'])");
      const auto m = title.match(source);
      return "Pine: " + (m.hasMatch() ? m.captured(1) : QStringLiteral("untitled"));
    }
    QStringList r;
    for (const char* k : {"entryLong", "exitLong", "entryShort", "exitShort"})
      for (const QJsonValue& x : strategy.value(k).toArray()) {
        const QJsonObject o = x.toObject();
        r << QString("%1: %2 %3 %4").arg(k, charts::bt::jsString(o.value("left")), charts::bt::jsString(o.value("op")),
                                         charts::bt::jsString(o.value("right")));
      }
    QStringList risk;   // what changes the trades besides the rules (a stop the model set must show it was taken)
    for (const char* k : {"stopLossPct", "takeProfitPct", "trailingStopPct", "maxBarsInTrade", "positionPct", "feePct", "slippagePct", "spreadPct", "exitMatch"})
      if (strategy.contains(k)) risk << QString(k) + " " + charts::bt::jsString(strategy.value(k));
    return "rules: " + r.join("; ").left(220) + (risk.isEmpty() ? QString() : " | " + risk.join(", "));
  }
};

const char* const FORMAT =
    "strategy (rules, the Strategy tester's JSON): {\"indicators\": {\"<key>\": {\"id\": \"ema\", \"params\": {\"length\": 9}}, …}, "
    "\"entryLong\": [rule…], \"exitLong\": [rule…], \"entryShort\": [rule…], \"exitShort\": [rule…]} with rule = "
    "{\"left\": operand, \"op\": \">\" | \"<\" | \">=\" | \"<=\" | \"crossesAbove\" | \"crossesBelow\" | \"crosses\", \"right\": operand}; "
    "operand = an indicator key (its first output) or \"key.output\" (e.g. \"m.signal\", \"b.upper\"), a price field (open high low "
    "close volume hl2 hlc3 ohlc4 hlcc4) or a number. Entry rules must all be true; exit rules all too, or one with "
    "\"exitMatch\": \"any\". Optional: stopLossPct, takeProfitPct, trailingStopPct, maxBarsInTrade, positionPct (100), "
    "feePct (0.1 a side), slippagePct (0.05 a side), spreadPct, initialCapital (10000), intrabar (how a stop or target fills "
    "inside a bar: \"worst\" = the stop first when a bar touches both, \"ohlc\" = open, nearer extreme, far extreme, close, "
    "\"magnifier\" = through the stored lower-timeframe bars; default: magnifier on Data › Technical data when a stop, target or "
    "trailing stop is set, else worst). Signals on a bar's close fill at the next bar's open. Indicator ids, inputs and "
    "outputs: bt_indicators.";

Spec specOf(const QJsonObject& a, bool quant) {
  Spec s;
  const QString pine = a.value("pine").isString() ? a.value("pine").toString() : QString();
  const QJsonValue st = a.value("strategy");
  const QString example = strArg(a, "example");
  const int given = (pine.trimmed().isEmpty() ? 0 : 1) + (st.isObject() ? 1 : 0) + (example.isEmpty() ? 0 : 1);
  if (given != 1) { s.error = QString("error: give exactly one of strategy (rules), pine (a strategy() script) or example. ") + FORMAT; return s; }
  if (!example.isEmpty()) {
    s.strategy = charts::bt::exampleStrategy(example);
    if (s.strategy.isEmpty()) {
      QStringList names;
      for (const auto& e : charts::bt::EXAMPLE_STRATEGIES()) names << e.first;
      s.error = "error: no example \"" + example + "\". Examples: " + names.join(", ");
      return s;
    }
  } else if (st.isObject()) {
    s.strategy = st.toObject();
  } else {
    s.pine = true;
    s.source = pine;
    if (!charts::pine::isStrategySource(pine)) { s.error = "error: the Pine script must declare strategy(...) and place orders with strategy.entry / strategy.close"; return s; }
  }
  if (!s.pine && s.strategy.value("entryLong").toArray().isEmpty() && s.strategy.value("entryShort").toArray().isEmpty()) {
    s.error = QString("error: a rule strategy needs at least one entry rule (entryLong or entryShort). ") + FORMAT;
    return s;
  }
  QString m = strArg(a, "market");
  if (m.isEmpty()) m = "BTCUSDT";
  s.market = m;
  s.tf = strArg(a, "timeframe").isEmpty() ? QStringLiteral("1D") : strArg(a, "timeframe");
  const int cap = s.pine ? (quant ? PINE_QUANT_BARS : PINE_BARS) : (quant ? RULES_QUANT_BARS : RULES_BARS);
  s.bars = intArg(a, "bars", 3000, 100, cap);
  return s;
}

// ------------------------------------------------------------------------------------------------ bars
struct Data { charts::Bars bars; QString market, tf, source, error; int tfSec = 0; bool stored = false; };

// Data › Technical data when it holds that market and timeframe (deep history), Charts' sources otherwise. Worker thread
// for the store; the providers answer on the GUI thread.
void loadBars(const Spec& s, std::function<void(const Data&)> then) {
  const QString stored = technicaldata::storedMarket(s.market);
  auto& st = technicaldata::Store::instance();
  int tfSec = 0;
  for (const auto& t : technicaldata::timeframes()) if (t.name == s.tf) tfSec = t.seconds;
  if (tfSec && st.pairs().contains(stored) && st.state(stored, tfSec).count > 0) {
    // up to 200,000 bars from SQLite: read on a worker (the store's reads work from any thread), answered on the GUI
    // thread as the providers' bars are
    QThread* t = QThread::create([s, stored, tfSec, then] {
      auto& st = technicaldata::Store::instance();
      Data d;
      d.market = stored;
      d.tf = s.tf;
      const auto state = st.state(stored, tfSec);
      for (const auto& k : st.before(stored, tfSec, state.last + 1, s.bars)) d.bars << charts::Bar{k.t, k.o, k.h, k.l, k.c, k.v};
      d.source = "Data › Technical data (" + technicaldata::sourceOf(stored) + ", closed bars)";
      d.tfSec = tfSec;
      d.stored = true;
      QMetaObject::invokeMethod(qApp, [d, then] { then(d); }, Qt::QueuedConnection);
    });
    t->setObjectName("ai-backtest-bars");
    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
    return;
  }
  const int limit = std::min(s.bars, PROVIDER_BARS);
  charts::data::getCandles(s.market, s.tf, limit, qApp, [s, then, limit](const charts::data::Result<charts::data::CandlesResult>& r) {
    Data d;
    d.tf = s.tf;
    if (!r.ok()) { d.error = "error: no bars for " + s.market + " " + s.tf + ": " + r.error->message; then(d); return; }
    d.market = r.value.symbol;
    d.bars = r.value.candles;
    d.source = r.value.provider + (s.bars > limit ? QString(" (at most %1 bars from this source)").arg(limit) : QString());
    then(d);
  });
}

// ------------------------------------------------------------------------------------------------ trial ledger
// A link sitting there is removed first: the ledger is read back into tool results and appended to (both follow links).
QString ledgerPath(int env) {
  const QString p = (env >= 0 ? Slots::instance().folder(env) : paths::data("ai/tool-tests")) + "/backtest-trials.jsonl";
  paths::dropLink(p);
  return p;
}
double ppyOf(const charts::Bars& b) {
  return b.size() > 1 ? (365.0 * 86400 * (b.size() - 1)) / double(b.last().time - b.first().time) : 252;
}
QVector<double> perBar(const QVector<charts::bt::EquityPoint>& e) {
  QVector<double> r;
  for (int i = 1; i < e.size(); ++i) r << e[i].value / e[i - 1].value - 1;
  return r;
}
// Probabilistic Sharpe of a per-bar Sharpe `sr` over `n` returns with that skew and kurtosis, against a benchmark
double psrOf(double sr, double n, double skew, double kurt, double benchmark) {
  if (!std::isfinite(sr) || n < 10) return NAN;
  const double denom = std::sqrt(std::max(1e-12, 1 - skew * sr + ((kurt - 1) / 4) * sr * sr));
  return charts::quant::normCdf(((sr - benchmark) * std::sqrt(n - 1)) / denom);
}
struct Trial { QString hash, label; double sr = NAN, n = 0, skew = 0, kurt = 3, sharpeAnn = NAN, netPct = NAN; qint64 at = 0; };
QHash<QString, Trial> trialsFor(int env, const QString& key) {   // hash → its latest run on that data
  QHash<QString, Trial> out;
  QFile f(ledgerPath(env));
  if (!f.open(QIODevice::ReadOnly)) return out;
  while (!f.atEnd()) {
    const QJsonObject o = QJsonDocument::fromJson(f.readLine()).object();
    if (o.value("key").toString() != key) continue;
    Trial t;
    t.hash = o.value("hash").toString(); t.label = o.value("label").toString();
    t.sr = o.value("sr").toDouble(NAN); t.n = o.value("n").toDouble(); t.skew = o.value("skew").toDouble();
    t.kurt = o.value("kurt").toDouble(3); t.sharpeAnn = o.value("sharpe").toDouble(NAN); t.netPct = o.value("net").toDouble(NAN);
    t.at = qint64(o.value("at").toDouble());
    out.insert(t.hash, t);
  }
  return out;
}
// The ledger is appended to at every backtest and read whole at each bt_trials and after each record: past
// LEDGER_MAX_BYTES it keeps only the latest line of each strategy on each data (all trialsFor() reads: nothing it counts
// is lost), then, if that is still over half of it, drops the strategies tried longest ago (those tries stop counting).
// Worker thread (record()).
constexpr qint64 LEDGER_MAX_BYTES = 2 << 20;
void compactLedger(const QString& path) {
  QFile f(path);
  if (f.size() <= LEDGER_MAX_BYTES || !f.open(QIODevice::ReadOnly)) return;
  QList<QByteArray> lines;
  QHash<QString, int> at;   // key|hash → its latest line in `lines`
  while (!f.atEnd()) {
    const QByteArray line = f.readLine().trimmed();
    const QJsonObject o = QJsonDocument::fromJson(line).object();
    if (o.isEmpty()) continue;
    const QString k = o.value("key").toString() + "|" + o.value("hash").toString();
    if (const auto it = at.constFind(k); it != at.cend()) lines[*it].clear();
    at.insert(k, int(lines.size()));
    lines << line;
  }
  f.close();
  lines.removeAll(QByteArray());
  qint64 total = 0;
  for (const auto& l : lines) total += l.size() + 1;
  int from = 0;
  while (from < lines.size() && total > LEDGER_MAX_BYTES / 2) total -= lines[from++].size() + 1;
  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly)) return;
  for (int i = from; i < lines.size(); ++i) out.write(lines[i] + "\n");
  out.commit();
}
// Records one tried strategy; returns how many distinct strategies this environment has tried on that data
int record(int env, const Spec& s, const Data& d, const charts::bt::Result& r) {
  const QVector<double> rets = perBar(r.equity);
  const auto m = charts::quant::moments(rets);
  const double sr = charts::quant::sharpe(rets);
  const QJsonObject o{{"at", double(QDateTime::currentSecsSinceEpoch())}, {"key", d.market + "|" + d.tf}, {"hash", s.hash()},
                      {"label", s.label()}, {"bars", int(d.bars.size())}, {"sr", std::isfinite(sr) ? sr : 0.0}, {"n", double(rets.size())},
                      {"skew", std::isfinite(m.skew) ? m.skew : 0.0}, {"kurt", std::isfinite(m.kurt) ? m.kurt : 3.0},
                      {"sharpe", std::isfinite(sr) ? sr * std::sqrt(ppyOf(d.bars)) : 0.0},
                      {"net", std::isfinite(r.stats.netProfitPct) ? r.stats.netProfitPct : 0.0}};
  {
    QFile f(ledgerPath(env));   // closed (flushed) before it is read again below
    if (f.open(QIODevice::Append)) f.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + "\n");
  }
  compactLedger(ledgerPath(env));
  return int(trialsFor(env, d.market + "|" + d.tf).size());
}
// The deflated Sharpe of the best of these trials (Bailey & López de Prado): its probability of beating the best Sharpe
// pure noise would show after that many tries
QString deflatedText(const QHash<QString, Trial>& t) {
  if (t.isEmpty()) return "no strategy tried yet on this data";
  QVector<double> srs;
  const Trial* best = nullptr;
  for (const auto& x : t) {
    if (std::isfinite(x.sr)) srs << x.sr;
    if (!best || x.sr > best->sr) best = &x;
  }
  double var = 0;
  if (srs.size() > 1) { const auto m = charts::quant::moments(srs); var = m.sd * m.sd * (double(srs.size()) / (srs.size() - 1)); }
  const double sr0 = charts::quant::expectedMaxSharpe(std::max<double>(srs.size(), 1), var);
  const double dsr = psrOf(best->sr, best->n, best->skew, best->kurt, sr0);
  return QString("%1 distinct strategies tried on this data. Best: %2 (Sharpe %3, net %4%). Its deflated Sharpe after %1 tries: %5 "
                 "(the probability its edge is real; needs above 0.95)")
      .arg(t.size()).arg(best->label, num(best->sharpeAnn), num(best->netPct), num(dsr, 3));
}

// ------------------------------------------------------------------------------------------------ texts
QString statsText(const charts::bt::Stats& s) {
  return QString("net %1% | buy & hold %2% | max drawdown %3% | trades %4 | win rate %5% | profit factor %6 | Sharpe %7 | Sortino %8 | "
                 "avg trade %9% | time in market %10%\ncosts: %11 | fills: %12")
      .arg(num(s.netProfitPct), num(s.buyHoldPct), num(s.maxDrawdownPct)).arg(s.totalTrades)
      .arg(num(s.winRatePct), num(s.profitFactor), num(s.sharpe), num(s.sortino), num(s.avgTradePct), num(s.exposurePct),
           s.feesAndSlippage, s.fills);
}
QString yearsText(const charts::bt::Result& r) {
  QMap<int, QPair<double, double>> y;   // year → first, last equity
  double prev = r.equity.isEmpty() ? NAN : r.equity.first().value;
  for (const auto& e : r.equity) {
    const int yr = QDateTime::fromSecsSinceEpoch(e.time, QTimeZone::UTC).date().year();
    if (!y.contains(yr)) y.insert(yr, {prev, e.value});
    y[yr].second = e.value;
    prev = e.value;
  }
  QStringList parts;
  for (auto it = y.cbegin(); it != y.cend(); ++it) parts << QString("%1 %2%").arg(it.key()).arg(num((it->second / it->first - 1) * 100, 1));
  return "by year: " + parts.join(" | ");
}
QString tradesText(const QVector<charts::bt::Trade>& t, int last) {
  QString out = QString("last %1 of %2 trades: side | entry | exit | entry price | exit price | P&L % | bars | exit reason\n").arg(std::min<int>(last, t.size())).arg(t.size());
  for (int i = std::max(0, int(t.size()) - last); i < t.size(); ++i)
    out += t[i].side + " | " + day(t[i].entryTime) + " | " + day(t[i].exitTime) + " | " + price(t[i].entryPrice) + " | " + price(t[i].exitPrice) +
           " | " + num(t[i].pnlPct) + " | " + QString::number(t[i].bars) + " | " + t[i].exitReason + "\n";
  return out;
}
// The rule strategy as it runs: a stop, target or trailing stop on Data › Technical data walks through the stored
// lower-timeframe bars (the magnifier) unless the model chose another intrabar mode; `sub` gets those bars, `note` says so.
QJsonObject withMagnifier(const QJsonObject& strategy, const Data& d, charts::Bars* sub, QString* note) {
  QJsonObject st = strategy;
  const bool exits = charts::bt::jsTruthy(st.value("stopLossPct")) || charts::bt::jsTruthy(st.value("takeProfitPct")) ||
                     charts::bt::jsTruthy(st.value("trailingStopPct"));
  if (!st.contains("intrabar") && exits && d.stored) st["intrabar"] = "magnifier";
  if (st.value("intrabar").toString() != "magnifier") return st;
  if (!d.stored) { *note = "intrabar: magnifier asked, but this market is not stored in Data › Technical data: the OHLC path is used\n"; return st; }
  QString tf;
  *sub = technicaldata::magnifierBars(d.market, d.tfSec, d.bars, 1000000, &tf);
  *note = sub->isEmpty() ? QString("intrabar: no finer stored timeframe fits this test: the OHLC path is used\n")
                         : QString("intrabar: stops and targets walked through %1 stored %2 bars (Data › Technical data)\n").arg(sub->size()).arg(tf);
  return st;
}

QString headText(const Spec& s, const Data& d) {
  return QString("%1 %2 · %3 bars %4 → %5 · data: %6\nstrategy: %7\n")
      .arg(d.market, d.tf).arg(d.bars.size()).arg(day(d.bars.first().time), day(d.bars.last().time), d.source, s.label());
}

// Runs `work` on a worker thread (Pine runs need a big stack) and answers on the GUI thread
void onWorker(std::function<ToolResult()> work, ToolDone done) {
  QThread* t = QThread::create([work, done] {
    ToolResult r;
    try { r = work(); } catch (const std::exception& e) { r = fail("error: " + QString::fromUtf8(e.what())); }
    QMetaObject::invokeMethod(qApp, [r, done] { g_busy = false; done(r); }, Qt::QueuedConnection);
  });
  t->setObjectName("ai-backtest");
  t->setStackSize(16u << 20);
  QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
  t->start();
}

// ------------------------------------------------------------------------------------------------ tools
void indicators(const QJsonObject& a, ToolDone done) {
  const QString q = strArg(a, "query").toLower();
  QStringList lines;
  for (const auto& d : charts::ind::all()) {
    const QString hay = (d.id + " " + d.name + " " + d.group).toLower();
    if (!q.isEmpty() && !hay.contains(q)) continue;
    QStringList in, out;
    for (const auto& i : d.inputs) in << i.key + "=" + (i.def.isString() ? i.def.toString() : QString::number(i.def.toDouble()));
    for (const auto& o : d.outputs) out << o.key;
    lines << d.id + " | " + d.name + " | inputs: " + (in.isEmpty() ? "none" : in.join(", ")) + " | outputs: " + out.join(", ");
  }
  if (lines.isEmpty()) { done(fail("error: no indicator matches \"" + q + "\"; call with an empty query for all")); return; }
  QString out = QString("%1 indicator(s) (Charts' engine). id | name | inputs with defaults | outputs (the first is used when an operand names only the key)\n").arg(lines.size());
  out += lines.join("\n");
  if (out.size() > kMaxChars && q.isEmpty()) out = out.left(kMaxChars - 120) + "\n[cut: ask with a query, e.g. \"moving\", \"volatility\", \"rsi\"]";
  done(okText(out));
}

void run(const QJsonObject& a, ToolDone done) {
  const Spec s = specOf(a, false);
  if (!s.error.isEmpty()) { done(fail(s.error)); return; }
  if (g_busy) { done(fail("refused: a backtest is already running; ask again when it has answered")); return; }
  g_busy = true;
  const int env = currentCaller().env;
  loadBars(s, [s, env, done](const Data& d) {
    if (!d.error.isEmpty() || d.bars.size() < 50) { g_busy = false; done(fail(d.error.isEmpty() ? "error: too few bars (" + QString::number(d.bars.size()) + ")" : d.error)); return; }
    onWorker([s, d, env]() -> ToolResult {
      charts::bt::Result r;
      if (s.pine) {
        charts::pine::Options o;
        o.symbol = d.market;
        o.timeframe = charts::pine::timeframeFromCharts(d.tf);
        const auto pr = charts::pine::runStrategy(s.source, d.bars, o);
        if (!pr.ok()) return fail("error in the Pine script: " + pr.error);
        r = pr.result;
      }
      QString note;
      if (!s.pine) {
        charts::Bars sub;
        const QJsonObject st = withMagnifier(s.strategy, d, &sub, &note);
        r = charts::bt::backtest(d.bars, st, sub);   // throws on an unknown operand or operator: the message is the answer
      }
      const int tried = record(env, s, d, r);
      QString out = headText(s, d) + note + statsText(r.stats) + "\n" + yearsText(r) + "\n" + tradesText(r.trades, 8);
      out += QString("This is strategy %1 tried by this environment on %2 %3: bt_trials deflates the best by the number of tries; "
                     "bt_quant_checks tests this one for overfitting.").arg(tried).arg(d.market, d.tf);
      return okText(out);
    }, done);
  });
}

void quantChecks(const QJsonObject& a, ToolDone done) {
  const Spec s = specOf(a, true);
  if (!s.error.isEmpty()) { done(fail(s.error)); return; }
  if (g_busy) { done(fail("refused: a backtest is already running; ask again when it has answered")); return; }
  g_busy = true;
  const int env = currentCaller().env;
  loadBars(s, [s, env, done](const Data& d) {
    if (!d.error.isEmpty() || d.bars.size() < 200) { g_busy = false; done(fail(d.error.isEmpty() ? "error: the checks need at least 200 bars" : d.error)); return; }
    onWorker([s, d, env]() -> ToolResult {
      QElapsedTimer clock;
      clock.start();
      charts::quant::Checks c;
      charts::bt::Result base;
      if (s.pine) {
        charts::pine::Options o;
        o.symbol = d.market;
        o.timeframe = charts::pine::timeframeFromCharts(d.tf);
        const auto pq = charts::pine::quantChecks(s.source, d.bars, o);
        if (!pq.ok()) return fail("error in the Pine script: " + pq.error);
        c = pq.checks;
        const auto pr = charts::pine::runStrategy(s.source, d.bars, o);
        if (pr.ok()) base = pr.result;
      }
      QString note;
      if (!s.pine) {
        charts::quant::Options qo;
        const QJsonObject st = withMagnifier(s.strategy, d, &qo.subBars, &note);
        c = charts::quant::quantChecks(d.bars, st, qo);
        base = charts::bt::backtest(d.bars, st, qo.subBars);
      }
      record(env, s, d, base);
      QString out = headText(s, d) + note;
      out += QString("Quant checks: passed %1 of %2 (%3 s). Base: Sharpe %4, net %5%, %6 trades.\n")
                 .arg(c.passed).arg(c.gates.size()).arg(num(clock.elapsed() / 1000.0, 1), num(c.base.sharpe), num(c.base.netPct)).arg(c.base.trades);
      for (const auto& g : c.gates) out += (g.pass ? "PASS | " : "FAIL | ") + g.label + " | " + g.value + "\n";   // English for the model
      if (!c.tunables.isEmpty()) {
        QStringList t;
        for (const auto& x : c.tunables) {
          QStringList v;
          for (double y : x.values) v << num(y, 0);
          t << x.label + " (" + num(x.base, 0) + "): " + v.join(" ");
        }
        out += "settings varied: " + t.join(" × ") + " = " + QString::number(c.variants.size()) + " versions\n";
      } else out += "no whole-number setting to vary: walk-forward, overfitting and nearby settings need versions to compare\n";
      if (c.walkForward) {
        out += QString("walk-forward: out-of-sample Sharpe %1, in-sample %2, %3 of %4 stretches made money; stretches: ")
                   .arg(num(c.walkForward->oosSharpe), num(c.walkForward->isSharpe)).arg(c.walkForward->positiveFolds).arg(c.walkForward->folds.size());
        QStringList f;
        for (const auto& x : c.walkForward->folds) {
          QStringList v;
          for (double y : x.chosen) v << num(y, 0);
          f << QDateTime::fromSecsSinceEpoch(x.from, QTimeZone::UTC).toString("yyyy-MM") + "→" + QDateTime::fromSecsSinceEpoch(x.to, QTimeZone::UTC).toString("yyyy-MM") +
                   " picked " + v.join("/") + " return " + num(x.testReturnPct, 1) + "%";
        }
        out += f.join("; ") + "\n";
      }
      if (c.monteCarlo)
        out += QString("Monte Carlo (%1 reshuffles of the trades): final 5th %2% / median %3% / 95th %4%, max drawdown median %5% / 95th %6%, "
                       "chance of a loss %7%\n").arg(c.monteCarlo->runs)
                   .arg(num(c.monteCarlo->p5, 1), num(c.monteCarlo->p50, 1), num(c.monteCarlo->p95, 1), num(c.monteCarlo->ddP50, 1),
                        num(c.monteCarlo->ddP95, 1), num(c.monteCarlo->lossProb, 1));
      out += "Across this environment's tries on this data: " + deflatedText(trialsFor(env, d.market + "|" + d.tf)) + "\n";
      return okText(out);
    }, done);
  });
}

void trials(const QJsonObject& a, ToolDone done) {
  const int env = currentCaller().env;
  QFile f(ledgerPath(env));
  QMap<QString, int> keys;
  if (f.open(QIODevice::ReadOnly))
    while (!f.atEnd()) keys[QJsonDocument::fromJson(f.readLine()).object().value("key").toString()]++;
  const QString m = strArg(a, "market"), tf = strArg(a, "timeframe");
  QString out = "Strategies this environment backtested (bt_run and bt_quant_checks), by market and timeframe:\n";
  int shown = 0;
  for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
    if (!m.isEmpty() && !it.key().contains(m.toUpper().remove('/'))) continue;
    if (!tf.isEmpty() && it.key().section('|', 1) != tf) continue;
    out += "- " + it.key().section('|', 0, 0) + " " + it.key().section('|', 1) + ": " + deflatedText(trialsFor(env, it.key())) + "\n";
    ++shown;
  }
  if (!shown) out += "(none yet)\n";
  out += "Trying many strategies on the same data and keeping the best is how a lucky backtest looks real: the deflated Sharpe "
         "accounts for every try.";
  done(okText(out));
}

QJsonObject schema(std::initializer_list<std::pair<const char*, QJsonObject>> props, QStringList required = {}) {
  QJsonObject p;
  for (const auto& [k, v] : props) p[k] = v;
  QJsonObject o{{"type", "object"}, {"properties", p}};
  if (!required.isEmpty()) o["required"] = QJsonArray::fromStringList(required);
  return o;
}
QJsonObject str(const char* d) { return {{"type", "string"}, {"description", d}}; }
QJsonObject integer(const char* d) { return {{"type", "integer"}, {"description", d}}; }
QJsonObject strategyArgs(bool quant) {
  QJsonObject p{
      {"strategy", QJsonObject{{"type", "object"}, {"description", FORMAT}}},
      {"pine", str("or: a Pine Script v5/v6 source declaring strategy(...) and placing orders with strategy.entry / strategy.close "
                   "(its own sizing, commission and slippage apply; input.int settings are what the quant checks vary)")},
      {"example", str("or: the name of one of the tester's example strategies, e.g. \"EMA cross (9/21)\"")},
      {"market", str("BTCUSDT (default; Data › Technical data' stored history, every timeframe) or a Charts symbol: EURUSD, XAUUSD, SPY, ETHUSDT…")},
      {"timeframe", str("1m, 5m, 15m, 1h, 4h, 1D (default), 1W")},
      {"bars", integer(quant ? "newest bars to test on, default 3000 (rules up to 100000, Pine up to 20000; other sources 20000)"
                             : "newest bars to test on, default 3000 (rules up to 200000, Pine up to 50000; other sources 20000)")}};
  return QJsonObject{{"type", "object"}, {"properties", p}};
}
}  // namespace

void registerBacktestTools(Tools& t) {
  const QString S = "backtest";
  // a Pine run on 50000 bars or the quant checks (walk-forward, PBO, Monte Carlo) take far more than a data read's 30 s
  constexpr int kBacktestTimeoutMs = 10 * 60 * 1000;
  t.add({"bt_indicators", S, "Indicators a rule strategy can use (Charts' engine): id, inputs with defaults, outputs.",
         schema({{"query", str("optional filter: a word of the id, name or group (e.g. moving, rsi, volatility)")}}), indicators});
  t.add({"bt_run", S, "Backtests a strategy with Deepwater's Strategy tester (rules) or PineTS (a Pine strategy you write) on a market and "
                      "timeframe: net, buy & hold, drawdown, trades, win rate, profit factor, Sharpe, Sortino, returns by year, last trades. "
                      "Every figure comes from the engine. One backtest at a time.",
         strategyArgs(false), run, kBacktestTimeoutMs});
  t.add({"bt_quant_checks", S, "The Strategy tester's quant checks on a strategy: walk-forward, overfitting (PBO), deflated Sharpe, cost stress, "
                               "nearby settings (its first two whole-number settings varied), Monte Carlo of the trades; and the deflated "
                               "Sharpe across all strategies this environment tried on the same data.",
         strategyArgs(true), quantChecks, kBacktestTimeoutMs});
  t.add({"bt_trials", S, "The strategies this environment has backtested on each market and timeframe, with the best one's Sharpe deflated "
                         "by the number of tries.",
         schema({{"market", str("optional filter, e.g. BTCUSDT")}, {"timeframe", str("optional filter, e.g. 1h")}}), trials});
}
}  // namespace ai
