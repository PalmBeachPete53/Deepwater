// Port of quant-terminal test/indicators.test.js (MIT, Copyright (c) 2026 Lewis Jackson). The custom-format cases
// that need JS objects (calc: undefined, levels: ['70'] ...) run through CustomJs in custom_tests.cpp; the ones a
// C++ Def can express run here too.
#include "Check.h"
#include "charts/core/Indicators.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <numeric>

using namespace charts;

namespace {
const Bars& bars() {
  static const Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    if (!f.open(QIODevice::ReadOnly)) return Bars();
    Bars out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out << Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(), o.value("high").toDouble(),
                 o.value("low").toDouble(), o.value("close").toDouble(), o.value("volume").toDouble()};
    }
    return out;
  }();
  return b;
}
Series closes() {
  Series c;
  for (const Bar& b : bars()) c << b.close;
  return c;
}
bool near(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b)); }

// Independent, deliberately naive versions of the textbook formulas.
Series naiveEma(const Series& src, int n) {
  Series out;
  double prev = 0;
  for (int i = 0; i < src.size(); ++i) {
    if (i < n - 1) {
      out << NaN;
      continue;
    }
    if (i == n - 1) {
      prev = std::accumulate(src.begin(), src.begin() + n, 0.0) / n;
      out << prev;
      continue;
    }
    prev = (src[i] - prev) * (2.0 / (n + 1)) + prev;
    out << prev;
  }
  return out;
}
Series naiveRsi(const Series& src, int n) {
  // Wilder's original method: seed with simple averages, then smooth.
  Series out(src.size(), NaN);
  double g = 0, l = 0;
  for (int i = 1; i <= n; ++i) {
    const double d = src[i] - src[i - 1];
    g += std::max(d, 0.0);
    l += std::max(-d, 0.0);
  }
  g /= n;
  l /= n;
  out[n] = 100 - 100 / (1 + g / l);
  for (int i = n + 1; i < src.size(); ++i) {
    const double d = src[i] - src[i - 1];
    g = (g * (n - 1) + std::max(d, 0.0)) / n;
    l = (l * (n - 1) + std::max(-d, 0.0)) / n;
    out[i] = 100 - 100 / (1 + g / l);
  }
  return out;
}
Series naiveAtr(const Bars& b, int n) {
  Series trs;
  for (int i = 0; i < b.size(); ++i)
    trs << (i ? std::max({b[i].high - b[i].low, std::fabs(b[i].high - b[i - 1].close), std::fabs(b[i].low - b[i - 1].close)})
              : b[i].high - b[i].low);
  Series out(b.size(), NaN);
  double a = std::accumulate(trs.begin(), trs.begin() + n, 0.0) / n;
  out[n - 1] = a;
  for (int i = n; i < b.size(); ++i) {
    a = (a * (n - 1) + trs[i]) / n;
    out[i] = a;
  }
  return out;
}
}  // namespace

TEST("SMA matches a plain average") {
  const Series close = closes(), s = ind::sma(close, 20);
  for (int i : {19, 100, 999}) CHECK(near(s[i], std::accumulate(close.begin() + i - 19, close.begin() + i + 1, 0.0) / 20));
  CHECK(std::isnan(s[18]));
}

TEST("EMA is seeded with the SMA and matches the textbook recursion") {
  const Series a = ind::ema(closes(), 21), b = naiveEma(closes(), 21);
  for (int i : {20, 21, 500, 999}) CHECK(near(a[i], b[i]));
}

TEST("RSI matches Wilder's method") {
  const Series a = ind::compute("rsi", bars(), {{"length", 14}}).value("value"), b = naiveRsi(closes(), 14);
  CHECK(std::isnan(a[13]));
  for (int i : {14, 15, 200, 999}) CHECK(near(a[i], b[i]));
  for (int i = 14; i < a.size(); ++i) CHECK(a[i] >= 0 && a[i] <= 100);
}

TEST("ATR uses Wilder smoothing") {
  const Series a = ind::atr(bars(), 14), b = naiveAtr(bars(), 14);
  for (int i : {13, 14, 500, 999}) CHECK(near(a[i], b[i]));
}

TEST("MACD line is fast EMA minus slow EMA, histogram is MACD minus signal") {
  const ind::Result m = ind::compute("macd", bars());
  const Series f = naiveEma(closes(), 12), s = naiveEma(closes(), 26);
  CHECK(near(m.value("macd")[999], f[999] - s[999]));
  CHECK(near(m.value("hist")[999], m.value("macd")[999] - m.value("signal")[999]));
}

TEST("Bollinger bands use population standard deviation") {
  const ind::Result r = ind::compute("bb", bars(), {{"length", 20}, {"mult", 2}});
  const Series close = closes();
  const double mean = std::accumulate(close.begin() + 980, close.begin() + 1000, 0.0) / 20;
  double v = 0;
  for (int i = 980; i < 1000; ++i) v += (close[i] - mean) * (close[i] - mean);
  const double sd = std::sqrt(v / 20);
  CHECK(near(r.value("basis")[999], mean));
  CHECK(near(r.value("upper")[999], mean + 2 * sd, 1e-5));
}

TEST("every built-in indicator runs on real data and returns full-length series") {
  int builtins = 0;
  for (const ind::Def& def : ind::all()) {
    if (def.custom || def.id.startsWith("custom:")) continue;
    builtins++;
    const ind::Result out = ind::compute(def.id, bars());
    if (def.extra.value("drawnOnly").toBool()) continue;   // colour-only outputs (Big Trades: drawn from trades, no values)
    for (const ind::Output& o : def.outputs) {
      CHECK(out.series.contains(o.key));
      const Series s = out.value(o.key);
      CHECK(s.size() >= bars().size());
      bool any = false;
      for (int i = 0; i < bars().size() && i < s.size(); ++i) any = any || std::isfinite(s[i]);
      if (!any) check::fail(__FILE__, __LINE__, def.id + "." + o.key + " has no values");
    }
  }
  CHECK(builtins >= 50);
}

TEST("supertrend flips direction and stays on the right side of price") {
  const ind::Result r = ind::compute("supertrend", bars());
  const Series dir = r.value("direction"), up = r.value("up");
  int flips = 0;
  for (int i = 1; i < dir.size(); ++i)
    if (dir[i] != dir[i - 1] && std::isfinite(dir[i - 1])) flips++;
  CHECK(flips > 3);
  for (int i = 0; i < bars().size(); ++i)
    if (std::isfinite(up[i])) CHECK(up[i] <= bars()[i].high * 1.0001);
}

TEST("custom indicators can be registered and computed") {
  ind::Def d;
  d.id = "custom:test";
  d.name = "Test";
  d.calc = [](const Bars& b, const Params&) {
    ind::Result r;
    r.put("value", ind::sma(ind::source(b), 3));
    return r;
  };
  CHECK(ind::registerIndicator(d).isEmpty());
  const Series v = ind::compute("custom:test", bars()).value("value");
  const Series c = closes();
  CHECK(near(v[2], (c[0] + c[1] + c[2]) / 3));
  // registerIndicator's defaults: group Custom, one orange `value` line labelled with the name.
  const ind::DefPtr reg = ind::byId("custom:test");
  CHECK(reg && reg->group == "Custom" && reg->outputs.size() == 1 && reg->outputs[0].key == "value" &&
        reg->outputs[0].label == "Test" && reg->outputs[0].color == "#ff9800");
  CHECK(ind::all().last().id == "custom:test");
  // Registering the same id again replaces it in place.
  const int count = ind::all().size();
  d.name = "Test 2";
  CHECK(ind::registerIndicator(d).isEmpty());
  CHECK_EQ(ind::all().size(), count);
  CHECK_EQ(ind::byId("custom:test")->name, QString("Test 2"));
  ind::unregisterIndicator("custom:test");
  CHECK(ind::byId("custom:test") == nullptr);
  CHECK_EQ(ind::all().size(), count - 1);
  ind::Def noId = d;
  noId.id.clear();
  CHECK_EQ(ind::registerIndicator(noId), QStringList({"A custom indicator needs an id"}));
}

TEST("a malformed C++ definition is refused, with each problem named") {
  ind::Def good;
  good.name = "Ok";
  good.inputs = {ind::Input{"length", "", "int", 14, NaN, NaN, NaN, {}}};
  ind::Output out;
  out.key = "value";
  out.color = "#2962ff";
  good.outputs = {out};
  good.calc = [](const Bars& b, const Params& p) {
    ind::Result r;
    r.put("value", ind::sma(ind::source(b), inum(p, "length")));
    return r;
  };
  CHECK_EQ(ind::problems(good), QStringList());
  auto has = [](const ind::Def& d, const QString& text) {
    const QString all = ind::problems(d).join("\n");
    if (!all.contains(text)) check::fail(__FILE__, __LINE__, "expected \"" + text + "\" in: " + all);
  };
  ind::Def d = good;
  d.calc = nullptr;
  has(d, "needs a `calc(bars, p, ta)` function");
  d = good;
  d.name = "";
  has(d, "needs a `name`");
  d = good;
  d.name = QString(81, 'x');
  has(d, "its `name` is longer than 80 characters");
  d = good;
  d.inputs = {ind::Input{"length", "", "int", 2.5, NaN, NaN, NaN, {}}};
  has(d, "input \"length\": an int input needs a whole-number default");
  d = good;
  d.inputs = {ind::Input{"ma", "", "select", "EMA", NaN, NaN, NaN, {"SMA"}}};
  has(d, "input \"ma\": its default must be one of its options");
  d = good;
  d.inputs = {ind::Input{"src", "", "source", "close", NaN, NaN, NaN, {}}};
  has(d, "a source input's key must be \"source\"");
  d = good;
  d.inputs = {ind::Input{"on", "", "bool", true, NaN, NaN, NaN, {}}};
  has(d, "`type` must be one of int, float, select, source, time");
  d = good;
  d.outputs[0].kind = "area";
  has(d, "`kind` must be \"line\" or \"histogram\"");
  d = good;
  d.outputs[0].lineWidth = 5;
  has(d, "`lineWidth` must be 1, 2, 3 or 4");
  d = good;
  d.outputs = {good.outputs[0], good.outputs[0]};
  has(d, "output \"value\": the key is used twice");
  d = good;
  d.calc = [](const Bars& b, const Params&) {
    ind::Result r;
    r.put("value", ind::source(b).mid(1));
    return r;
  };
  has(d, "output \"value\": calc() returned 299 values, but there are 300 bars");
  d = good;
  d.calc = [](const Bars&, const Params&) { return ind::Result(); };
  has(d, "output \"value\": calc() returned no list for it");
  d = good;
  d.calc = [](const Bars&, const Params&) -> ind::Result { throw std::runtime_error("boom"); };
  has(d, "calc() stopped with an error on test bars: boom");
  d = good;
  d.calc = [](const Bars& b, const Params&) {
    ind::Result r;
    r.put("value", ind::source(b));
    r.putMarkers({ind::Marker{b[0].time, "top", "arrowUp", "", "", {}}});
    return r;
  };
  has(d, "markers: each needs");
  d = good;
  d.levels = {NaN};
  has(d, "`levels` must be a list of numbers");
  d = good;
  d.calc = [](const Bars& b, const Params&) {
    ind::Result r;
    r.put("value", ind::source(b));
    r.putBarColors(QStringList(b.size() - 1, "#fff"));
    return r;
  };
  has(d, "barColors: return one colour per bar");

  ind::Def broken = good;
  broken.id = "custom:test-broken";
  broken.calc = [](const Bars&, const Params&) { return ind::Result(); };
  const QStringList pr = ind::registerIndicator(broken);
  CHECK(ind::formatProblems(pr).startsWith("This indicator doesn't follow the format (docs/custom-indicators.md):\n- output \"value\""));
  bool threw = false;
  try {
    ind::validateIndicator(broken);
  } catch (const std::runtime_error& e) {
    threw = QString(e.what()).contains("doesn't follow the format (docs/custom-indicators.md):\n- output \"value\"");
  }
  CHECK(threw);
  CHECK(ind::byId("custom:test-broken") == nullptr);   // a refused indicator is not registered
  ind::Def ok = good;
  ok.id = "custom:test-ok";
  CHECK(ind::registerIndicator(ok).isEmpty());
  CHECK(ind::byId("custom:test-ok") && ind::byId("custom:test-ok")->id == "custom:test-ok");
  ind::unregisterIndicator("custom:test-ok");
}

TEST("every built-in definition follows the custom indicator format, except where the JS built-ins themselves do not") {
  // Built-ins never go through the check in the JS; this documents which ones would fail it and why.
  for (const ind::Def& d : ind::all()) {
    if (d.custom) continue;
    const QStringList pr = ind::problems(d, false);
    const bool noOutputs = d.outputs.isEmpty();   // rsibars, pivotshl, vpvr, svp, patterns, fractals draw no series
    if (noOutputs) CHECK(pr.contains("`outputs` must be a list with at least one output"));
    else if (!pr.isEmpty()) check::fail(__FILE__, __LINE__, d.id + ": " + pr.join("; "));
  }
}

TEST("RSI Candle Colours: each candle takes the colour of the RSI zone it closed in") {
  const QStringList barColors = ind::compute("rsibars", bars()).barColors;
  const Series rsi = ind::compute("rsi", bars(), {{"maLength", 0}}).value("value");
  const QStringList zones = {"#1565c0", "#26c6da", "#ef9a9a", "#a5d6a7", "#ff9800", "#d500f9"};
  CHECK_EQ(barColors.size(), bars().size());
  CHECK(barColors[0].isEmpty());
  for (int i = 0; i < rsi.size(); ++i) {
    const double v = rsi[i];
    if (!std::isfinite(v)) {
      CHECK(barColors[i].isEmpty());
      continue;
    }
    const int z = v < 20 ? 0 : v < 30 ? 1 : v < 50 ? 2 : v < 70 ? 3 : v < 80 ? 4 : 5;
    CHECK_EQ(barColors[i], zones[z]);
  }
  QSet<QString> used;
  for (const QString& c : barColors)
    if (!c.isEmpty()) used << c;
  CHECK(used.size() >= 4);
  // custom indicators can colour candles the same way
  ind::Def ok;
  ok.name = "Up red";
  ind::Output o;
  o.key = "value";
  ok.outputs = {o};
  ok.calc = [](const Bars& b, const Params&) {
    ind::Result r;
    r.put("value", ind::source(b));
    QStringList c;
    for (const Bar& x : b) c << (x.close > x.open ? "#f00" : "");
    r.putBarColors(c);
    return r;
  };
  CHECK_EQ(ind::problems(ok), QStringList());
}

TEST("the trial bars are quant-terminal's 300 made-up bars") {
  const Bars t = ind::trialBars();
  CHECK_EQ(t.size(), 300);
  CHECK_EQ(t[0].time, 1577836800);
  CHECK_EQ(t[0].open, 100.0);
  CHECK_EQ(t[299].time, 1577836800 + 299 * 86400);
  // Values from node: trialBars() in indicators.js (not exported; computed with the same code).
  CHECK_NEAR(t[0].high, 100.52458710201297, 1e-12);
  CHECK_NEAR(t[0].low, 97.28059009891852, 1e-12);
  CHECK_NEAR(t[0].close, 98.00131483003558, 1e-12);
  CHECK_EQ(t[0].volume, 3370.0);
  CHECK_NEAR(t[299].open, 87.96680020665818, 1e-12);
  CHECK_NEAR(t[299].high, 90.25532215952053, 1e-12);
  CHECK_NEAR(t[299].low, 87.16467137624697, 1e-12);
  CHECK_NEAR(t[299].close, 89.66702538724705, 1e-12);
  CHECK_EQ(t[299].volume, 5779.0);
}
