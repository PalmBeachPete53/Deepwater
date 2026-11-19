// A Pine strategy() run as a Strategy tester result. See PineStrategy.h.
#include "charts/core/PineStrategy.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace charts::pine {

namespace {
double num(const QJsonObject& o, const char* k, double def = NaN) {
  const QJsonValue v = o.value(k);
  return v.isDouble() ? v.toDouble() : def;
}
QString text(const QJsonObject& o, const char* k) { return o.value(k).toString(); }

// the bar index of a time (ms), or -1
int barAt(const Bars& bars, double ms) {
  const qint64 t = qint64(std::llround(ms / 1000));
  const auto it = std::lower_bound(bars.cbegin(), bars.cend(), t, [](const Bar& b, qint64 x) { return b.time < x; });
  return it != bars.cend() && it->time == t ? int(it - bars.cbegin()) : -1;
}

bt::Trade toTrade(const QJsonObject& t, const Bars& bars, bool open) {
  bt::Trade x;
  const double size = num(t, "size", 0);
  x.side = size < 0 ? QStringLiteral("short") : QStringLiteral("long");
  x.qty = std::fabs(size);
  x.entryPrice = num(t, "entry_price", 0);
  x.entryTime = qint64(std::llround(num(t, "entry_time", 0) / 1000));
  int entryBar = int(num(t, "entry_bar_index", -1));
  if (entryBar < 0) entryBar = barAt(bars, num(t, "entry_time", 0));
  x.entryReason = text(t, "entry_comment").isEmpty() ? text(t, "entry_id") : text(t, "entry_comment");
  if (open) {
    // valued at the last close, as TradingView's open P&L (entry commission paid)
    const Bar& last = bars.last();
    x.exitPrice = last.close;
    x.exitTime = last.time;
    x.pnl = (size < 0 ? -1 : 1) * x.qty * (last.close - x.entryPrice) - num(t, "entry_commission", num(t, "commission", 0));
    x.bars = entryBar >= 0 ? int(bars.size()) - 1 - entryBar : 0;
    x.exitReason = QStringLiteral(QT_TRANSLATE_NOOP("charts", "Open"));
  } else {
    x.exitPrice = num(t, "exit_price", 0);
    x.exitTime = qint64(std::llround(num(t, "exit_time", 0) / 1000));
    int exitBar = int(num(t, "exit_bar_index", -1));
    if (exitBar < 0) exitBar = barAt(bars, num(t, "exit_time", 0));
    x.pnl = num(t, "profit", 0);
    x.bars = entryBar >= 0 && exitBar >= 0 ? exitBar - entryBar : 0;
    x.exitReason = text(t, "exit_comment").isEmpty() ? text(t, "exit_id") : text(t, "exit_comment");
  }
  const double notional = x.entryPrice * x.qty;
  x.pnlPct = notional > 0 ? x.pnl / notional * 100 : 0;
  return x;
}

QString feesText(const QJsonObject& cfg) {
  const QString type = cfg.value("commission_type").toString();
  const double value = num(cfg, "commission_value", 0);
  const double slip = num(cfg, "slippage", 0);
  QString fee;
  if (value == 0) fee = QCoreApplication::translate("charts", "no commission");
  else if (type == "percent") fee = QCoreApplication::translate("charts", "%1% commission per order").arg(value);
  else if (type == "cash_per_contract") fee = QCoreApplication::translate("charts", "%1 commission per contract").arg(value);
  else fee = QCoreApplication::translate("charts", "%1 commission per order").arg(value);
  return QCoreApplication::translate("charts", "Pine strategy: %1, %2 ticks of slippage").arg(fee).arg(slip);
}
}  // namespace

bool isStrategySource(const QString& source) {
  static const QRegularExpression re(QStringLiteral("^\\s*strategy\\s*\\("), QRegularExpression::MultilineOption);
  return re.match(source).hasMatch();
}

StrategyResult fromRun(const Result& run, const Bars& bars) {
  StrategyResult out;
  out.log = run.log;
  out.ms = run.ms;
  if (!run.ok()) { out.error = run.error; return out; }
  if (run.strategy.isEmpty() || bars.isEmpty()) {
    out.error = QCoreApplication::translate("charts", "This script is not a strategy(): it places no orders. Add it to the chart as an indicator.");
    return out;
  }
  const QJsonObject& st = run.strategy;
  out.config = st.value("config").toObject();
  out.totals = st;
  out.totals.remove("config");
  out.totals.remove("closedtrades");
  out.totals.remove("opentrades");
  const double capital = num(st, "initial_capital", num(out.config, "initial_capital", 1000000));

  const QJsonArray closed = st.value("closedtrades").toArray(), open = st.value("opentrades").toArray();
  bt::Result& r = out.result;
  for (const QJsonValue& v : closed) r.trades << toTrade(v.toObject(), bars, false);
  for (const QJsonValue& v : open) r.trades << toTrade(v.toObject(), bars, true);

  // equity at each close: cash after the trades closed on or before the bar, plus the open ones at that close
  const int n = int(bars.size());
  QVector<double> realized(n + 1, 0);   // profit realized at bar i (added from i on)
  struct Live { int from, to; double size, entry, entryFee; };
  QVector<Live> live;
  for (const QJsonValue& v : closed) {
    const QJsonObject t = v.toObject();
    int e = int(num(t, "entry_bar_index", -1)), x = int(num(t, "exit_bar_index", -1));
    if (e < 0) e = barAt(bars, num(t, "entry_time", 0));
    if (x < 0) x = barAt(bars, num(t, "exit_time", 0));
    if (x >= 0 && x < n) realized[x] += num(t, "profit", 0);
    if (e >= 0 && x > e) live.append({e, x, num(t, "size", 0), num(t, "entry_price", 0), num(t, "entry_commission", 0)});
  }
  for (const QJsonValue& v : open) {
    const QJsonObject t = v.toObject();
    int e = int(num(t, "entry_bar_index", -1));
    if (e < 0) e = barAt(bars, num(t, "entry_time", 0));
    if (e >= 0) live.append({e, n, num(t, "size", 0), num(t, "entry_price", 0), num(t, "entry_commission", num(t, "commission", 0))});
  }
  double cash = capital;
  r.equity.reserve(n);
  for (int i = 0; i < n; ++i) {
    cash += realized[i];
    double unreal = 0;
    for (const Live& l : live)
      if (i >= l.from && i < l.to) unreal += l.size * (bars[i].close - l.entry) - l.entryFee;
    r.equity.append({bars[i].time, cash + unreal});
  }

  // the tester's stats on these trades, with the script's own costs named
  const QJsonObject s{{"initialCapital", capital}, {"feePct", 0}, {"slippagePct", 0}, {"spreadPct", 0}};
  r.stats = bt::stats(bars, r.trades, r.equity, s);
  r.stats.feesAndSlippage = feesText(out.config);
  r.stats.hasFills = true;
  r.stats.fills = QCoreApplication::translate("charts", "orders filled by PineTS's broker emulator, as TradingView fills them");
  return out;
}

StrategyResult runStrategy(const QString& source, const Bars& bars, const Options& options) {
  return fromRun(run(source, bars, options), bars);
}

}  // namespace charts::pine
