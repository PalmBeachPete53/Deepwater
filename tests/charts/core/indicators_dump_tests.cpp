// Every indicator of the C++ port against quant-terminal's own JS, value by value.
// The JS side is dumped by tests/charts/tools/dump-indicators.mjs into tests/charts/core/fixtures/:
//   indicators-btc-1d.json  every indicator with its defaults on quant-terminal/test/fixtures/btc-1d.json (+ the defs)
//   indicators-edge.json    edge cases: lengths 0 / 1 / -1 / 3 / 1000 / "abc" (NaN), every select option and source, tiny, flat,
//                           intraday and pre-1970 bars, ta primitives on a series with NaN holes, periodKey, patterns
// Numbers: equal within 1e-9 x max(1, |js|); NaN, Infinity and -Infinity must match exactly; result keys must be the
// JS keys in the JS order; markers, colours, bar colours and breaks must be identical; a JS throw must be a C++ throw.
#include "Check.h"
#include "charts/core/Indicators.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

using namespace charts;

namespace {
QString fixtureDir() { return QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("fixtures"); }
QJsonObject readJson(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    check::fail(__FILE__, __LINE__, "cannot open " + path);
    return {};
  }
  return QJsonDocument::fromJson(f.readAll()).object();
}
const QJsonObject& full() {
  static const QJsonObject o = readJson(fixtureDir() + "/indicators-btc-1d.json");
  return o;
}
const QJsonObject& edge() {
  static const QJsonObject o = readJson(fixtureDir() + "/indicators-edge.json");
  return o;
}
double dec(const QJsonValue& v) {
  if (v.isDouble()) return v.toDouble();
  const QString s = v.toString();
  if (s == "Inf") return INFINITY;
  if (s == "-Inf") return -INFINITY;
  return NaN;   // "NaN", "undef", null
}
Bars barsOf(const QJsonArray& a) {
  Bars out;
  for (const QJsonValue& v : a) {
    const QJsonObject o = v.toObject();
    Bar b;
    b.time = qint64(o.value("time").toDouble());
    b.open = dec(o.value("open"));
    b.high = dec(o.value("high"));
    b.low = dec(o.value("low"));
    b.close = dec(o.value("close"));
    b.volume = o.contains("volume") ? dec(o.value("volume")) : 0;
    out << b;
  }
  return out;
}
const Bars& btc() {
  static const Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    if (!f.open(QIODevice::ReadOnly)) return Bars();
    return barsOf(QJsonDocument::fromJson(f.readAll()).array());
  }();
  return b;
}
bool sameNumber(double got, double want) {
  if (std::isnan(want)) return std::isnan(got);
  if (std::isinf(want)) return got == want;
  return std::isfinite(got) && std::fabs(got - want) <= 1e-9 * std::max(1.0, std::fabs(want));
}
QString str(const QJsonValue& v) { return v.isString() && v.toString() != "undef" ? v.toString() : QString(); }

int g_values = 0;      // numbers compared, for the summary line
double g_maxRel = 0;   // largest |C++ - JS| / max(1, |JS|) seen

// Compares a numeric series; one failure message per series at most.
bool compareSeries(const QString& where, const Series& got, const QJsonArray& want) {
  if (got.size() != want.size()) {
    check::fail(__FILE__, __LINE__, QString("%1: %2 values, JS has %3").arg(where).arg(got.size()).arg(want.size()));
    return false;
  }
  for (qsizetype i = 0; i < got.size(); ++i) {
    ++g_values;
    const double w = dec(want[i]);
    if (std::isfinite(w) && std::isfinite(got[i])) g_maxRel = std::max(g_maxRel, std::fabs(got[i] - w) / std::max(1.0, std::fabs(w)));
    if (!sameNumber(got[i], w)) {
      check::fail(__FILE__, __LINE__,
                  QString("%1[%2] = %3, JS %4").arg(where).arg(i).arg(got[i], 0, 'g', 17).arg(dec(want[i]), 0, 'g', 17));
      return false;
    }
  }
  return true;
}
bool compareMarkers(const QString& where, const QVector<ind::Marker>& got, const QJsonArray& want) {
  if (got.size() != want.size()) {
    check::fail(__FILE__, __LINE__, QString("%1: %2 markers, JS has %3").arg(where).arg(got.size()).arg(want.size()));
    return false;
  }
  for (qsizetype i = 0; i < got.size(); ++i) {
    const QJsonObject w = want[i].toObject();
    const ind::Marker& m = got[i];
    const bool ok = m.time == qint64(w.value("time").toDouble()) && m.position == str(w.value("position")) &&
                    m.shape == str(w.value("shape")) && m.color == str(w.value("color")) && m.text == str(w.value("text"));
    if (!ok) {
      check::fail(__FILE__, __LINE__,
                  QString("%1 marker %2: %3 %4 %5 %6 \"%7\", JS %8").arg(where).arg(i).arg(m.time).arg(m.position, m.shape, m.color, m.text,
                                                                                       QString::fromUtf8(QJsonDocument(w).toJson(QJsonDocument::Compact))));
      return false;
    }
  }
  return true;
}
bool compareStrings(const QString& where, const QStringList& got, const QJsonArray& want) {
  if (got.size() != want.size()) {
    check::fail(__FILE__, __LINE__, QString("%1: %2 entries, JS has %3").arg(where).arg(got.size()).arg(want.size()));
    return false;
  }
  for (qsizetype i = 0; i < got.size(); ++i)
    if (got[i] != (want[i].isNull() ? QString() : want[i].toString())) {
      check::fail(__FILE__, __LINE__, QString("%1[%2] = %3, JS %4").arg(where).arg(i).arg(got[i], want[i].toString()));
      return false;
    }
  return true;
}

// One compute() against one dumped JS run ({ result } or { error }).
void compareRun(const QString& where, const std::function<ind::Result()>& run, const QJsonObject& js) {
  ind::Result r;
  try {
    r = run();
  } catch (const std::exception& e) {
    if (!js.contains("error")) check::fail(__FILE__, __LINE__, where + ": threw " + e.what() + ", JS did not");
    return;
  }
  if (js.contains("error")) {
    check::fail(__FILE__, __LINE__, where + ": JS threw \"" + js.value("error").toString() + "\", C++ did not");
    return;
  }
  const QJsonObject want = js.value("result").toObject();
  // The result's keys, in the JS order (QJsonObject sorts its keys, so the dump lists them in `keys`).
  QStringList wantKeys;
  for (const QJsonValue& k : js.value("keys").toArray()) wantKeys << k.toString();
  if (wantKeys != r.keys) {
    check::fail(__FILE__, __LINE__, where + ": keys " + r.keys.join(",") + ", JS " + wantKeys.join(","));
    return;
  }
  for (const QString& k : want.keys()) {
    const QString w = where + "." + k;
    const QJsonValue v = want.value(k);
    if (k == "markers") compareMarkers(w, r.markers, v.toArray());
    else if (k == "colors") compareStrings(w, r.colors, v.toArray());
    else if (k == "barColors") compareStrings(w, r.barColors, v.toArray());
    else if (k == "breaks") {
      QVector<bool> b;
      for (const QJsonValue& x : v.toArray()) b << x.toBool();
      if (b != r.breaks) check::fail(__FILE__, __LINE__, w + " differ");
    } else if (k == "volumeProfile") {
      if (r.volumeProfile != v.toObject()) check::fail(__FILE__, __LINE__, w + " differs");
    } else if (k == "sessionProfile") {
      if (r.sessionProfile != v.toObject()) check::fail(__FILE__, __LINE__, w + " differs");
    } else if (v.isArray()) compareSeries(w, r.series.value(k), v.toArray());
    else if (!sameNumber(r.extra.value(k).toDouble(NaN), dec(v)))
      check::fail(__FILE__, __LINE__, QString("%1 = %2, JS %3").arg(w).arg(r.extra.value(k).toDouble()).arg(dec(v)));
  }
}

QJsonObject jsDef(const QString& id) {
  for (const QJsonValue& d : full().value("defs").toArray())
    if (d.toObject().value("id").toString() == id) return d.toObject();
  return {};
}
}  // namespace

TEST("registry: the 88 built-ins in the JS order, with the JS ids, names, groups, inputs, outputs, levels, extras") {
  const QJsonArray defs = full().value("defs").toArray();
  CHECK_EQ(defs.size(), 88);
  const QVector<ind::Def>& all = ind::all();
  CHECK(all.size() >= defs.size());
  for (int k = 0; k < defs.size() && k < all.size(); ++k) {
    const QJsonObject w = defs[k].toObject();
    const ind::Def& d = all[k];
    const QString at = w.value("id").toString();
    CHECK_EQ(d.id, at);
    CHECK_EQ(d.name, w.value("name").toString());
    CHECK_EQ(d.group, w.value("group").toString());
    CHECK_EQ(d.overlay, w.value("overlay").toBool());
    const QJsonArray ins = w.value("inputs").toArray();
    CHECK_EQ(d.inputs.size(), ins.size());
    for (int i = 0; i < ins.size() && i < d.inputs.size(); ++i) {
      const QJsonObject x = ins[i].toObject();
      const ind::Input& in = d.inputs[i];
      if (in.key != x.value("key").toString() || in.label != x.value("label").toString() || in.type != x.value("type").toString() ||
          in.def != x.value("default") || !sameNumber(in.min, x.contains("min") ? x.value("min").toDouble() : NaN) ||
          !std::isnan(in.max) || !std::isnan(in.step))
        check::fail(__FILE__, __LINE__, at + " input " + x.value("key").toString() + " differs");
      QStringList opts;
      for (const QJsonValue& o : x.value("options").toArray()) opts << o.toString();
      CHECK_EQ(in.options, opts);
      QStringList known = {"key", "label", "type", "default", "min", "options"};
      for (const QString& k2 : x.keys())
        if (!known.contains(k2)) check::fail(__FILE__, __LINE__, at + " input field not compared: " + k2);
    }
    const QJsonArray outs = w.value("outputs").toArray();
    CHECK_EQ(d.outputs.size(), outs.size());
    for (int i = 0; i < outs.size() && i < d.outputs.size(); ++i) {
      const QJsonObject x = outs[i].toObject();
      const ind::Output& o = d.outputs[i];
      QJsonObject extra = x;
      for (const char* k2 : {"key", "label", "kind", "color", "lineWidth"}) extra.remove(k2);
      if (o.key != x.value("key").toString() || o.label != x.value("label").toString() || o.kind != x.value("kind").toString() ||
          o.color != x.value("color").toString() || o.lineWidth != x.value("lineWidth").toInt(0) || o.extra != extra)
        check::fail(__FILE__, __LINE__, at + " output " + x.value("key").toString() + " differs");
    }
    QVector<double> levels;
    for (const QJsonValue& l : w.value("levels").toArray()) levels << l.toDouble();
    CHECK_EQ(d.levels, levels);
    QJsonObject extra = w;
    for (const char* k2 : {"id", "name", "group", "overlay", "inputs", "outputs", "levels"}) extra.remove(k2);
    if (d.extra != extra) check::fail(__FILE__, __LINE__, at + " extra fields differ");
    CHECK(ind::byId(at) != nullptr);
    CHECK(ind::byId(at) && ind::byId(at)->id == at);
    CHECK(ind::defaults(at).size() == ins.size());
  }
  CHECK(ind::byId("nope") == nullptr);
  bool threw = false;
  try {
    ind::compute("nope", btc());
  } catch (const std::runtime_error& e) {
    threw = QString(e.what()) == "Unknown indicator: nope";
  }
  CHECK(threw);
}

bool pineSupersedes(const QString& id, const QJsonObject& params);   // below

TEST("all indicators: every output of every indicator on every BTC bar equals the JS (defaults)") {
  const QJsonObject fullRuns = full().value("full").toObject();
  CHECK_EQ(fullRuns.size(), 88);
  const int before = g_values;
  for (const QString& id : fullRuns.keys()) {
    if (pineSupersedes(id, {})) continue;
    compareRun(id, [&] { return ind::compute(id, btc()); }, fullRuns.value(id).toObject());
  }
  std::printf("    %d values compared on btc-1d, largest relative difference %.3g\n", g_values - before, g_maxRel);
}

TEST("all indicators: result keys come in the JS order") {
  // The JS order of the result keys for the outputs follows the calc's return statement; spot-check that the C++ keeps
  // the declared order where the JS returns its outputs in another order than the definition lists them.
  CHECK_EQ(ind::compute("bb", btc()).keys, QStringList({"basis", "upper", "lower"}));
  CHECK_EQ(ind::compute("macd", btc()).keys, QStringList({"macd", "signal", "hist", "colors"}));
  CHECK_EQ(ind::compute("dmi", btc()).keys, QStringList({"plus", "minus", "adx"}));
  CHECK_EQ(ind::compute("pivots", btc()).keys, QStringList({"p", "r1", "s1", "r2", "s2", "r3", "s3", "breaks"}));
  CHECK_EQ(ind::compute("ichimoku", btc()).keys, QStringList({"conversion", "base", "lagging", "spanA", "spanB", "extendBars"}));
  CHECK_EQ(ind::compute("markov", btc()).keys, QStringList({"signal", "pbull", "pbear", "state", "colors"}));
  CHECK_EQ(ind::compute("volume", btc()).keys, QStringList({"value", "colors", "ma"}));
  CHECK_EQ(ind::compute("supertrend", btc()).keys, QStringList({"up", "down", "direction"}));
  CHECK_EQ(ind::compute("smiergodic", btc()).keys, QStringList({"smi", "signal", "osc"}));
  CHECK(jsDef("pivots").value("noAutoscale").toBool());
}

// Pine supersedes quant-terminal (the user's rule, 2026-11-18): stdev is now Pine's two-pass formula. Over a window of
// one bar it is exactly 0, where the JS one-pass E[x²] − E[x]² gave cancellation noise (4.9e-4 for BTC prices), which
// then moved these indicators. Those length-1 cases are not compared with the JS any more; pine_reference_tests.cpp
// compares them with their Pine definitions.
const QSet<QString> STDEV_BASED = {"stdev", "bb", "bbpb", "bbw", "hv", "rvi"};
// SMI Ergodic (audit D19, 2026-11-19): quant-terminal smoothed short then long; Pine's ta.tsi (PineTS, third_party/pinets)
// smooths long then short. Not compared with the JS any more; audit_batch3_tests.cpp compares it with ta.tsi.
bool pineSupersedes(const QString& id, const QJsonObject& params) {
  if (id == "smiergodic") return true;
  return STDEV_BASED.contains(id) && params.value("length").isDouble() && params.value("length").toDouble() == 1;
}

TEST("all indicators: edge cases (n <= 0, n = \"abc\", every option and source, tiny, flat, intraday, pre-1970 bars) equal the JS") {
  const QJsonObject sets = edge().value("sets").toObject();
  QHash<QString, Bars> bars;
  for (const QString& k : sets.keys()) bars.insert(k, barsOf(sets.value(k).toArray()));
  const QJsonArray cases = edge().value("cases").toArray();
  CHECK(cases.size() > 1500);
  int threw = 0;
  const int before = g_values;
  for (const QJsonValue& c : cases) {
    const QJsonObject o = c.toObject();
    const QString id = o.value("id").toString(), set = o.value("set").toString();
    const QJsonObject params = o.value("params").toObject();
    if (o.contains("error")) threw++;
    if (pineSupersedes(id, params)) continue;
    compareRun(QString("%1 %2 %3").arg(id, set, QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))),
               [&] { return ind::compute(id, bars.value(set), params); }, o);
  }
  // pivotshl left/right -1, markov lookback -1, fractals periods -1 throw in the JS too; and new Array(NaN) for a
  // non-numeric ichimoku displacement or alligator offset (3)
  CHECK_EQ(threw, 8);
  std::printf("    %d cases, %d values compared, largest relative difference so far %.3g\n", int(cases.size()), g_values - before, g_maxRel);
}

TEST("ta primitives equal the JS on a series with NaN holes, for lengths 0, 1, -1, 2, 5, 59, 60, 61, NaN") {
  const QJsonObject sets = edge().value("sets").toObject();
  const Bars flat = barsOf(sets.value("flat").toArray()), shortBars = barsOf(sets.value("short").toArray());
  Series S;
  for (const QJsonValue& v : edge().value("S").toArray()) S << dec(v);
  Series vol;
  for (const Bar& b : shortBars) vol << b.volume;
  Series A, B;
  for (qsizetype i = 0; i < S.size(); ++i) {
    A << S[i] + 300 * std::sin(i / 3.0);
    B << shortBars[i].open;
  }
  int n = 0;
  for (const QJsonValue& t : edge().value("ta").toArray()) {
    const QJsonObject o = t.toObject();
    const QString fn = o.value("fn").toString();
    const QJsonArray args = o.value("args").toArray();
    const int len = args.isEmpty() ? 0 : ind::lengthOf(dec(args[0]));   // NaN ("NaN" in the dump): kNaNLength
    const QString where = fn + QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));
    if (fn == "stdev" && len == 1) continue;   // Pine's two-pass stdev: exactly 0 over one value (see pineSupersedes)
    const QJsonValue want = o.value("result");
    if (o.contains("error")) {
      check::fail(__FILE__, __LINE__, where + ": the JS threw, not expected");
      continue;
    }
    n++;
    Series got;
    if (fn == "sum") got = ind::sum(S, len);
    else if (fn == "sma") got = ind::sma(S, len);
    else if (fn == "ema") got = ind::ema(S, len);
    else if (fn == "rma") got = ind::rma(S, len);
    else if (fn == "wma") got = ind::wma(S, len);
    else if (fn == "hma") got = ind::hma(S, len);
    else if (fn == "dema") got = ind::dema(S, len);
    else if (fn == "tema") got = ind::tema(S, len);
    else if (fn == "stdev") got = ind::stdev(S, len);
    else if (fn == "dev") got = ind::dev(S, len);
    else if (fn == "highest") got = ind::highest(S, len);
    else if (fn == "lowest") got = ind::lowest(S, len);
    else if (fn == "change") got = ind::change(S, len);
    else if (fn == "rsiOf") got = ind::rsi(S, len);
    else if (fn == "vwma") got = ind::vwma(S, vol, len);
    else if (fn == "alma") got = ind::alma(S, len, args[1].toDouble(), args[2].toDouble());
    else if (fn == "linreg") got = ind::linreg(S, len, dec(args[1]));
    else if (fn == "atr") got = ind::atr(flat, len);
    else if (fn == "tr") got = ind::tr(flat);
    else if (fn == "source") got = ind::source(flat, args[0].toString());
    else if (fn == "crossover" || fn == "crossunder") {
      QVector<bool> b;
      if (args[0].isString()) b = fn == "crossover" ? ind::crossover(A, B) : ind::crossunder(A, B);
      else b = fn == "crossover" ? ind::crossover(A, args[0].toDouble()) : ind::crossunder(A, args[0].toDouble());
      QVector<bool> w;
      for (const QJsonValue& x : want.toArray()) w << x.toBool();
      if (b != w) check::fail(__FILE__, __LINE__, where + " differs");
      continue;
    } else if (fn == "candlePatterns") {
      const QString which = args[0].toString();
      compareMarkers(where, ind::candlePatterns(which == "btc" ? btc() : which == "flat" ? flat : shortBars), want.toArray());
      continue;
    } else if (fn == "periodKey") {
      const QString period = args[0].toString();
      const QJsonArray times = args[1].toArray(), keys = want.toArray();
      for (int i = 0; i < times.size(); ++i) {
        const double t = times[i].toDouble();
        if (t != std::trunc(t)) continue;   // the C++ takes whole seconds
        if (ind::periodKey(qint64(t), period) != keys[i].toString())
          check::fail(__FILE__, __LINE__, QString("periodKey(%1, %2) = %3, JS %4").arg(qint64(t)).arg(period, ind::periodKey(qint64(t), period), keys[i].toString()));
      }
      continue;
    } else {
      check::fail(__FILE__, __LINE__, "unknown ta fn in the dump: " + fn);
      continue;
    }
    compareSeries(where, got, want.toArray());
  }
  CHECK(n > 150);

  const QJsonObject extra = edge().value("extra").toObject();
  const ind::Macd m = ind::macdFrom(S, 12, 26, 9);
  QJsonObject w = extra.value("macd").toObject().value("result").toObject();
  compareSeries("macd.macd", m.macd, w.value("macd").toArray());
  compareSeries("macd.signal", m.signal, w.value("signal").toArray());
  compareSeries("macd.hist", m.hist, w.value("hist").toArray());
  compareStrings("macd.colors", m.colors, w.value("colors").toArray());
  const ind::Macd m2 = ind::macdFrom(S, 3, 5, 2);
  w = extra.value("macdShort").toObject().value("result").toObject();
  compareSeries("macdShort.hist", m2.hist, w.value("hist").toArray());
  compareStrings("macdShort.colors", m2.colors, w.value("colors").toArray());
  const ind::Supertrend st = ind::supertrendOf(flat, 3, 10);
  w = extra.value("supertrend").toObject().value("result").toObject();
  compareSeries("supertrend.st", st.st, w.value("st").toArray());
  compareSeries("supertrend.dir", st.dir, w.value("dir").toArray());
  compareSeries("psar", ind::psarOf(flat, 0.02, 0.02, 0.2), extra.value("psar").toObject().value("result").toArray());
  const ind::Dmi d = ind::dmiOf(flat, 14, 14);
  w = extra.value("dmi").toObject().value("result").toObject();
  compareSeries("dmi.plus", d.plus, w.value("plus").toArray());
  compareSeries("dmi.minus", d.minus, w.value("minus").toArray());
  compareSeries("dmi.adx", d.adx, w.value("adx").toArray());
}

TEST("String(number) as JS writes it (pivot labels)") {
  CHECK_EQ(ind::numberToString(43198), QString("43198"));
  CHECK_EQ(ind::numberToString(42200.69), QString("42200.69"));
  CHECK_EQ(ind::numberToString(0.1 + 0.2), QString("0.30000000000000004"));
  CHECK_EQ(ind::numberToString(1e21), QString("1e+21"));
  CHECK_EQ(ind::numberToString(123456789012345680000.0), QString("123456789012345680000"));
  CHECK_EQ(ind::numberToString(0.000001), QString("0.000001"));
  CHECK_EQ(ind::numberToString(1e-7), QString("1e-7"));
  CHECK_EQ(ind::numberToString(-2.5e-8), QString("-2.5e-8"));
  CHECK_EQ(ind::numberToString(-0.0), QString("0"));
  CHECK_EQ(ind::numberToString(NaN), QString("NaN"));
  CHECK_EQ(ind::numberToString(-INFINITY), QString("-Infinity"));
  CHECK_EQ(ind::jsRound(-2.5), -2.0);
  CHECK_EQ(ind::jsRound(2.5), 3.0);
  CHECK_EQ(ind::jsRound(0.49999999999999994), 0.0);
}
