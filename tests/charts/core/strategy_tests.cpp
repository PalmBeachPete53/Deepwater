// The C++ series / backtest / quant / seasonals against reference outputs of the JS libraries:
// tests/charts/core/fixtures/strategy-btc-1d.json, written by tests/charts/tools/dump-strategy.mjs (node).
// Numbers must match to 1e-9 (relative above 1), texts and CSV byte for byte, result objects key for key.
#include "Check.h"
#include "charts/core/Backtest.h"
#include "charts/core/Quant.h"
#include "charts/core/Seasonals.h"
#include "charts/core/Series.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

using namespace charts;

namespace {

const QJsonObject& fx() {
  static const QJsonObject o = [] {
    QFile f(QStringLiteral(CHARTS_QT_ROOT "/../tests/charts/core/fixtures/strategy-btc-1d.json"));
    if (!f.open(QIODevice::ReadOnly)) qFatal("missing fixture strategy-btc-1d.json (run tests/charts/tools/dump-strategy.mjs)");
    return QJsonDocument::fromJson(f.readAll()).object();
  }();
  return o;
}

Bars barsOf(const QJsonArray& a) {
  Bars out;
  for (const QJsonValue& v : a) {
    const QJsonObject o = v.toObject();
    const QJsonValue vol = o.value("volume");
    out.push_back({qint64(o["time"].toDouble()), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(),
                   o["close"].toDouble(), vol.isDouble() ? vol.toDouble() : NaN});
  }
  return out;
}
Bars btc() {
  static Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES "/btc-1d.json"));
    f.open(QIODevice::ReadOnly);
    return barsOf(QJsonDocument::fromJson(f.readAll()).array());
  }();
  return b;
}
// Same synthetic magnifier bars as the dump script: three per day on the OHLC path, none for the last 100 days.
Bars subBars() {
  Bars out;
  const Bars bars = btc();
  for (int i = 0; i < bars.size() - 100; ++i) {
    const Bar& b = bars[i];
    const double p[4] = {b.open, b.close >= b.open ? b.low : b.high, b.close >= b.open ? b.high : b.low, b.close};
    for (int k = 0; k < 3; ++k)
      out.push_back({b.time + k * 28800, p[k], std::max(p[k], p[k + 1]), std::min(p[k], p[k + 1]), p[k + 1], 1});
  }
  return out;
}

// A fixture number: NaN / Infinity were written as strings.
double num(const QJsonValue& v) {
  if (v.isString()) {
    const QString s = v.toString();
    if (s == "NaN") return NaN;
    if (s == "Infinity") return std::numeric_limits<double>::infinity();
    if (s == "-Infinity") return -std::numeric_limits<double>::infinity();
  }
  return v.isDouble() ? v.toDouble() : NaN;
}
bool near(double a, double e) {
  if (std::isnan(a) || std::isnan(e)) return std::isnan(a) && std::isnan(e);
  if (std::isinf(a) || std::isinf(e)) return a == e;
  static const double tol = qEnvironmentVariableIsSet("CHARTS_EXACT") ? 0 : 1e-9;
  return std::fabs(a - e) <= tol * std::max(1.0, std::fabs(e));
}

// Deep comparison of a C++ toJson() (non-finite numbers already null) with the JS output.
void cmp(const QJsonValue& a, const QJsonValue& e, const QString& path, int& fails, const char* file, int line) {
  auto bad = [&](const QString& why) {
    if (++fails <= 8) check::fail(file, line, path + ": " + why);
  };
  if (e.isString() && (e.toString() == "NaN" || e.toString().endsWith("Infinity"))) {
    if (!(a.isNull() || (a.isDouble() && !std::isfinite(a.toDouble())))) bad("expected non-finite, got " + QJsonDocument(QJsonArray{a}).toJson(QJsonDocument::Compact));
    return;
  }
  if (e.type() != a.type()) {
    bad(QStringLiteral("type %1 vs %2 (%3 vs %4)").arg(a.type()).arg(e.type())
            .arg(QString(QJsonDocument(QJsonArray{a}).toJson(QJsonDocument::Compact)),
                 QString(QJsonDocument(QJsonArray{e}).toJson(QJsonDocument::Compact))));
    return;
  }
  switch (e.type()) {
    case QJsonValue::Double:
      if (!near(a.toDouble(), e.toDouble()))
        bad(QStringLiteral("%1 vs %2").arg(a.toDouble(), 0, 'g', 17).arg(e.toDouble(), 0, 'g', 17));
      return;
    case QJsonValue::String:
      if (a.toString() != e.toString()) bad("\"" + a.toString() + "\" vs \"" + e.toString() + "\"");
      return;
    case QJsonValue::Bool:
      if (a.toBool() != e.toBool()) bad("bool differs");
      return;
    case QJsonValue::Array: {
      const QJsonArray aa = a.toArray(), ea = e.toArray();
      if (aa.size() != ea.size()) { bad(QStringLiteral("length %1 vs %2").arg(aa.size()).arg(ea.size())); return; }
      for (int i = 0; i < ea.size(); ++i) cmp(aa[i], ea[i], path + "[" + QString::number(i) + "]", fails, file, line);
      return;
    }
    case QJsonValue::Object: {
      const QJsonObject ao = a.toObject(), eo = e.toObject();
      if (ao.keys() != eo.keys()) { bad("keys " + ao.keys().join(',') + " vs " + eo.keys().join(',')); return; }
      for (const QString& k : eo.keys()) cmp(ao[k], eo[k], path + "." + k, fails, file, line);
      return;
    }
    default: return;
  }
}
#define SAME(actual, expected, path) do { int _f = 0; cmp(actual, expected, path, _f, __FILE__, __LINE__); } while (0)

QJsonObject barJson(const Bar& b, bool withVolume = true) {
  QJsonObject o{{"time", double(b.time)}, {"open", b.open}, {"high", b.high}, {"low", b.low}, {"close", b.close}};
  if (withVolume) o.insert("volume", b.volume);
  return o;
}
QJsonArray barsJson(const Bars& bars, bool withVolume = true) {
  QJsonArray a;
  for (const Bar& b : bars) a.append(barJson(b, withVolume));
  return a;
}
// The JS shapes of the transforms (extra fields on every bar).
QJsonArray transformedJson(const series::Transformed& t, const QString& kind) {
  QJsonArray a;
  for (int i = 0; i < t.bars.size(); ++i) {
    QJsonObject o = barJson(t.bars[i], kind == "range");
    if (kind == "renko") o.insert("box", t.box);
    if (kind == "range") o.insert("range", t.range);
    if (kind == "kagi") { o.insert("color", t.colors[i]); o.insert("reversal", t.reversal); }
    if (kind == "pnf") { o.insert("boxes", t.boxes[i]); o.insert("box", t.box); o.insert("dir", t.dirs[i]); }
    a.append(o);
  }
  return a;
}

QString errorOf(const std::function<void()>& f) {
  try { f(); } catch (const std::exception& e) { return QString::fromUtf8(e.what()); }
  return {};
}

}  // namespace

// ---------------------------------------------------------------- series

TEST("strategy fixture: parseTf, isIntraday, bucketStart, nativeBase") {
  for (const QJsonValue& v : fx()["series"]["parseTf"].toArray()) {
    const QJsonObject c = v.toObject();
    const QString tf = c["tf"].toString();
    if (c.contains("error")) { CHECK_EQ(errorOf([&] { series::parseTf(tf); }), c["error"].toString()); continue; }
    const series::Tf p = series::parseTf(tf);
    SAME((QJsonObject{{"n", p.n}, {"unit", p.unit}, {"seconds", p.seconds}}), c["out"], "parseTf " + tf);
  }
  for (const QJsonValue& v : fx()["series"]["isIntraday"].toArray())
    CHECK_EQ(series::isIntraday(v[0].toString()), v[1].toBool());
  int n = 0;
  for (const QJsonValue& v : fx()["series"]["bucketStart"].toArray()) {
    const QJsonObject c = v.toObject();
    const qint64 got = series::bucketStart(qint64(c["t"].toDouble()), c["tf"].toString());
    if (got != qint64(c["out"].toDouble()) && ++n < 8)
      check::fail(__FILE__, __LINE__, QStringLiteral("bucketStart(%1, %2) = %3, JS %4").arg(c["t"].toDouble()).arg(c["tf"].toString()).arg(got).arg(c["out"].toDouble()));
  }
  CHECK_EQ(n, 0);
  for (const QJsonValue& v : fx()["series"]["nativeBase"].toArray()) {
    const QJsonObject c = v.toObject();
    QStringList natives;
    for (const QJsonValue& x : c["natives"].toArray()) natives << x.toString();
    const QString got = series::nativeBase(c["tf"].toString(), natives);
    CHECK_EQ(got, c["out"].isNull() ? QString() : c["out"].toString());
  }
}

TEST("strategy fixture: JS number formatting, toFixed and ISO dates") {
  for (const QJsonValue& v : fx()["series"]["numStr"].toArray()) CHECK_EQ(series::numStr(v[0].toDouble()), v[1].toString());
  for (const QJsonValue& v : fx()["series"]["toFixed"].toArray())
    CHECK_EQ(series::toFixed(num(v[0]), v[1].toInt()), v[2].toString());
  for (const QJsonValue& v : fx()["series"]["iso"].toArray())
    CHECK_EQ(series::isoString(qint64(v[0].toDouble())), v[1].toString());
  CHECK_EQ(errorOf([] { series::isoString(8640000000001LL); }), QStringLiteral("Invalid time value"));
}

TEST("strategy fixture: resample and heikin ashi") {
  const QJsonObject rs = fx()["series"]["resample"].toObject();
  for (const QString& tf : rs.keys()) SAME(barsJson(series::resample(btc(), tf)), rs[tf], "resample " + tf);
  SAME(barsJson(series::heikinAshi(btc())), fx()["series"]["heikinAshi"], "heikinAshi");
  CHECK(series::resample({}, "banana").isEmpty());   // JS parses the timeframe per bar: no bar, no error
}

TEST("strategy fixture: renko, line break, range bars, kagi, point & figure") {
  const Bars b = btc();
  const QJsonValue s = fx()["series"];
  SAME(transformedJson(series::renko(b), "renko"), s["renko"]["auto"], "renko auto");
  SAME(transformedJson(series::renko(b, 1000), "renko"), s["renko"]["box1000"], "renko 1000");
  SAME(transformedJson(series::renko(b.mid(0, 1)), "renko"), s["renko"]["short"], "renko short");
  SAME(transformedJson(series::renko(b.mid(0, 2)), "renko"), s["renko"]["two"], "renko two");
  SAME(barsJson(series::lineBreak(b), false), s["lineBreak"]["l3"], "lineBreak 3");
  SAME(barsJson(series::lineBreak(b, 2), false), s["lineBreak"]["l2"], "lineBreak 2");
  SAME(barsJson(series::lineBreak(b, 1), false), s["lineBreak"]["l1"], "lineBreak 1");
  SAME(barsJson(series::lineBreak(b, 0), false), s["lineBreak"]["l0"], "lineBreak 0");
  SAME(transformedJson(series::rangeBars(b), "range"), s["rangeBars"]["auto"], "rangeBars auto");
  SAME(transformedJson(series::rangeBars(b, 2000), "range"), s["rangeBars"]["r2000"], "rangeBars 2000");
  SAME(transformedJson(series::rangeBars(b.mid(0, 1)), "range"), s["rangeBars"]["one"], "rangeBars one");
  SAME(transformedJson(series::kagi(b), "kagi"), s["kagi"]["auto"], "kagi auto");
  SAME(transformedJson(series::kagi(b, 3000), "kagi"), s["kagi"]["r3000"], "kagi 3000");
  SAME(transformedJson(series::pointFigure(b), "pnf"), s["pointFigure"]["auto"], "pnf auto");
  SAME(transformedJson(series::pointFigure(b, 1000, 3), "pnf"), s["pointFigure"]["b1000r3"], "pnf 1000/3");
  SAME(transformedJson(series::pointFigure(b, 500, 2), "pnf"), s["pointFigure"]["b500r2"], "pnf 500/2");
}

TEST("strategy fixture: CSV export byte for byte") {
  const QJsonObject c = fx()["series"]["csv"].toObject();
  series::CsvColumns cols;
  const QJsonObject extra = c["extra"].toObject();
  for (const QString& k : {QStringLiteral("rsi"), QStringLiteral("ema_value")}) {
    Series v;
    for (const QJsonValue& x : extra[k].toArray()) v << (x.isNull() ? NaN : num(x));
    cols.push_back({k, v});
  }
  CHECK_EQ(series::toCsv(barsOf(c["bars"].toArray()), cols), c["text"].toString());
  CHECK_EQ(series::toCsv(series::renko(btc().mid(0, 60), 500).bars), c["renkoText"].toString());
}

// ---------------------------------------------------------------- backtest

TEST("strategy fixture: every backtest case, trade for trade, and its CSV") {
  const Bars b = btc(), sub = subBars();
  int n = 0;
  for (const QJsonValue& v : fx()["backtest"].toArray()) {
    const QJsonObject c = v.toObject();
    const bt::Result r = bt::backtest(b, c["strategy"].toObject(), c["sub"].toBool() ? sub : Bars{});
    SAME(r.toJson(), c["result"], c["name"].toString());
    CHECK_EQ(bt::tradesCsv(r.trades), c["csv"].toString());
    ++n;
  }
  CHECK_EQ(n, 26);
  // the examples themselves are the JS ones
  QJsonObject ex;
  for (const auto& [name, s] : bt::EXAMPLE_STRATEGIES()) ex.insert(name, s);
  int k = 0;
  for (const QJsonValue& v : fx()["backtest"].toArray()) {
    const QString name = v["name"].toString();
    if (ex.contains(name)) { SAME(ex[name], v["strategy"], name); ++k; }
  }
  CHECK_EQ(k, 5);
}

TEST("strategy fixture: conditions, errors and the magnifier timeframe") {
  const Bars b = btc();
  const QJsonObject ind = QJsonDocument::fromJson(R"({"macd": {"id": "macd", "params": {}}, "rsi": {"id": "rsi", "params": {"length": 7}}})").object();
  const auto make = bt::conditionFn(b, ind);
  for (const QJsonValue& v : fx()["conditions"].toArray()) {
    const auto f = make(v["rule"].toObject());
    QJsonArray idx;
    for (int i = 0; i < b.size(); ++i)
      if (f(i)) idx.append(i);
    SAME(idx, v["true"], QString(QJsonDocument(v["rule"].toObject()).toJson(QJsonDocument::Compact)));
  }
  for (const QJsonValue& v : fx()["errors"].toArray()) {
    const QJsonObject c = v.toObject();
    QString got;
    if (c["rule"].isString()) {
      got = errorOf([&] { bt::conditionFn(b, c["indicators"].toObject())(QJsonObject{{"left", "x"}, {"op", ">"}, {"right", 1}}); });
    } else {
      const QJsonObject macd{{"macd", QJsonObject{{"id", "macd"}, {"params", QJsonObject{}}}}};
      int count = -1;
      got = errorOf([&] {
        const auto f = bt::conditionFn(b, macd)(c["rule"].toObject());
        count = 0;
        for (int i = 0; i < b.size(); ++i) count += f(i);
      });
      if (c.contains("out")) CHECK_EQ(count, c["out"].toInt());
    }
    CHECK_EQ(got, c["error"].toString());
  }
  for (const QJsonValue& v : fx()["magnifier"].toArray()) {
    const auto m = bt::magnifierTf(v["tf"].toDouble(), v["span"].toDouble());
    SAME(m ? QJsonValue(QJsonObject{{"tf", m->tf}, {"seconds", m->seconds}}) : QJsonValue(QJsonValue::Null), v["out"], "magnifierTf");
  }
}

// ---------------------------------------------------------------- quant

TEST("strategy fixture: normal helpers, moments, sharpe, psr, expected max Sharpe") {
  const QJsonObject q = fx()["quant"].toObject();
  for (const QJsonValue& v : q["normCdf"].toArray()) CHECK_NEAR(quant::normCdf(v[0].toDouble()), num(v[1]), 1e-15);
  for (const QJsonValue& v : q["normInv"].toArray()) {
    const double got = quant::normInv(v[0].toDouble()), want = num(v[1]);
    CHECK(near(got, want));
  }
  auto vec = [](const QJsonValue& a) { QVector<double> r; for (const QJsonValue& x : a.toArray()) r << num(x); return r; };
  const QVector<double> steady = vec(q["series"]["steady"]), noise = vec(q["series"]["noise"]);
  for (const QJsonValue& v : q["moments"].toArray()) {
    const quant::Moments m = quant::moments(vec(v["r"]));
    SAME((QJsonObject{{"mean", m.mean}, {"sd", m.sd}, {"skew", m.skew}, {"kurt", m.kurt}}), v["out"], "moments");
  }
  const QVector<double> sh = {quant::sharpe(steady), quant::sharpe(noise), quant::sharpe({1}), quant::sharpe({2, 2, 2})};
  const QVector<double> ps = {quant::psr(steady), quant::psr(noise), quant::psr(noise, 0.01), quant::psr(steady.mid(0, 9)),
                              quant::psr(QVector<double>(20, 1))};
  auto arr = [](const QVector<double>& v) { QJsonArray a; for (double x : v) a.append(std::isfinite(x) ? QJsonValue(x) : QJsonValue()); return a; };
  SAME(arr(sh), q["sharpe"], "sharpe");
  SAME(arr(ps), q["psr"], "psr");
  for (const QJsonValue& v : q["expectedMaxSharpe"].toArray())
    CHECK(near(quant::expectedMaxSharpe(v[0].toDouble(), v[1].toDouble()), num(v[2])));
}

TEST("strategy fixture: PBO, the seeded random numbers and Monte Carlo") {
  const QJsonObject q = fx()["quant"].toObject();
  for (const QJsonValue& v : q["pbo"].toArray()) {
    QVector<QVector<double>> m;
    for (const QJsonValue& row : v["matrix"].toArray()) {
      QVector<double> r;
      for (const QJsonValue& x : row.toArray()) r << x.toDouble();
      m << r;
    }
    const auto p = quant::pbo(m, v["blocks"].toInt());
    SAME(p ? QJsonValue(QJsonObject{{"pbo", p->pbo}, {"splits", p->splits}}) : QJsonValue(QJsonValue::Null), v["out"], "pbo");
  }
  for (const QJsonValue& v : q["rng"].toArray()) {
    quant::Rng r = quant::rng(v["seed"].toDouble());
    int bad = 0;
    for (const QJsonValue& x : v["seq"].toArray()) bad += r() != x.toDouble();   // bit for bit
    CHECK_EQ(bad, 0);
  }
  for (const QJsonValue& v : q["monteCarlo"].toArray()) {
    QVector<double> t;
    for (const QJsonValue& x : v["trades"].toArray()) t << x.toDouble();
    const auto mc = quant::monteCarlo(t, v["runs"].toInt(), v["seed"].toDouble());
    SAME(mc ? QJsonValue(mc->toJson()) : QJsonValue(QJsonValue::Null), v["out"], "monteCarlo");
  }
}

TEST("strategy fixture: quantChecks on every case") {
  const Bars sub = subBars();
  for (const QJsonValue& v : fx()["quant"]["quantChecks"].toArray()) {
    const QJsonObject c = v.toObject();
    quant::Options o;
    o.mcRuns = c.contains("mcRuns") ? c["mcRuns"].toInt() : 2000;
    if (c["sub"].toBool()) o.subBars = sub;
    if (c.contains("text")) o.indicatorOrder = quant::indicatorOrder(c["text"].toString().toUtf8());
    const quant::Checks r = quant::quantChecks(btc(), c["strategy"].toObject(), o);
    SAME(r.toJson(), c["out"], c["name"].toString());
  }
}

// ---------------------------------------------------------------- seasonals

TEST("strategy fixture: seasonals") {
  const QJsonObject s = fx()["seasonals"].toObject();
  const int year = s["thisYear"].toInt();
  SAME(seasonals::seasonals(btc(), year).toJson(), s["btc"], "seasonals btc");
  SAME(seasonals::seasonals(barsOf(s["synthBars"].toArray()), year).toJson(), s["synth"], "seasonals synth");
}
