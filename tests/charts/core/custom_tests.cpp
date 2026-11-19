// Custom JS indicators (src/charts/core/CustomJs): quant-terminal's bundled example, the alert example and the
// template load through QuickJS-ng and give exactly the native values; the malformed-indicator cases of
// test/indicators.test.js run here on real JS objects, with the JS messages; modern syntax, the time limit and
// syntax errors.
#include "Check.h"
#include "charts/core/CustomJs.h"
#include "charts/core/Indicators.h"
#if __has_include("charts/core/Backtest.h")
#include "charts/core/Backtest.h"
#define HAVE_BACKTEST 1
#endif
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <thread>

using namespace charts;

namespace {
const QString QT = QStringLiteral(CHARTS_QT_ROOT);
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
void same(const QString& what, const Series& got, const Series& want) {
  if (got.size() != want.size()) {
    check::fail(__FILE__, __LINE__, QString("%1: %2 values, want %3").arg(what).arg(got.size()).arg(want.size()));
    return;
  }
  for (int i = 0; i < got.size(); ++i)
    if (!(got[i] == want[i] || (std::isnan(got[i]) && std::isnan(want[i])))) {
      check::fail(__FILE__, __LINE__, QString("%1[%2] = %3, want %4").arg(what).arg(i).arg(got[i], 0, 'g', 17).arg(want[i], 0, 'g', 17));
      return;
    }
}
QString write(QTemporaryDir& dir, const QString& name, const QString& code) {
  const QString path = dir.filePath(name);
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) f.write(code.toUtf8());
  return path;
}
// The `good` indicator of test/indicators.test.js with one field replaced: `fields` is JS object text spliced in.
QString goodWith(const QString& fields) {
  return QString(R"JS(
const good = {
  name: 'Ok',
  inputs: [{ key: 'length', type: 'int', default: 14 }],
  outputs: [{ key: 'value', kind: 'line', color: '#2962ff' }],
  calc: (bars, p, ta) => ({ value: ta.sma(ta.source(bars), p.length) }),
};
export default { ...good, %1 };
)JS")
      .arg(fields);
}
void problemMatches(const QString& fields, const QString& text) {
  const QString all = custom::check(goodWith(fields)).join("\n");
  if (!all.contains(text)) check::fail(__FILE__, __LINE__, "expected \"" + text + "\" for { " + fields + " }, got: " + all);
}
}  // namespace

TEST("custom: the bundled example (custom/ema-ribbon-squeeze.js) loads and equals the native EMAs") {
  const custom::Loaded l = custom::loadFile(QT + "/custom/ema-ribbon-squeeze.js");
  CHECK(l.ok());
  CHECK_EQ(l.error, QString());
  CHECK_EQ(l.id, QString("custom:ema-ribbon-squeeze"));
  CHECK_EQ(l.file, QString("ema-ribbon-squeeze.js"));
  const ind::DefPtr d = ind::byId("custom:ema-ribbon-squeeze");
  CHECK(d != nullptr);
  if (!d) return;
  CHECK_EQ(d->name, QString("EMA ribbon squeeze"));
  CHECK_EQ(d->group, QString("Custom"));
  CHECK(d->overlay);
  CHECK(d->custom);
  CHECK_EQ(d->file, QString("ema-ribbon-squeeze.js"));
  CHECK_EQ(d->inputs.size(), 2);
  CHECK_EQ(d->inputs[0].key, QString("fast"));
  CHECK_EQ(d->inputs[0].label, QString("Fast EMA"));
  CHECK_EQ(d->inputs[0].type, QString("int"));
  CHECK_EQ(d->inputs[0].def.toDouble(), 8.0);
  CHECK_EQ(d->inputs[1].def.toDouble(), 34.0);
  CHECK_EQ(d->outputs.size(), 2);
  CHECK_EQ(d->outputs[1].key, QString("slow"));
  CHECK_EQ(d->outputs[1].color, QString("#ef5350"));
  CHECK_EQ(d->outputs[1].kind, QString("line"));
  CHECK_EQ(ind::defaults("custom:ema-ribbon-squeeze"), Params({{"fast", 8}, {"slow", 34}}));

  const ind::Result r = ind::compute("custom:ema-ribbon-squeeze", bars());
  const Series close = ind::source(bars()), fast = ind::ema(close, 8), slow = ind::ema(close, 34);
  CHECK_EQ(r.keys, QStringList({"fast", "slow", "markers"}));
  same("fast", r.value("fast"), fast);
  same("slow", r.value("slow"), slow);
  const QVector<bool> up = ind::crossover(fast, slow);
  QVector<qint64> want;
  for (int i = 0; i < bars().size(); ++i)
    if (up[i]) want << bars()[i].time;
  CHECK_EQ(r.markers.size(), want.size());
  CHECK(!want.isEmpty());
  for (int k = 0; k < r.markers.size() && k < want.size(); ++k) {
    const ind::Marker& m = r.markers[k];
    CHECK(m.time == want[k] && m.position == "belowBar" && m.shape == "arrowUp" && m.color == "#26a69a" && m.text == "Cross");
  }
  // Params reach calc: other lengths, other EMAs.
  const ind::Result r2 = ind::compute("custom:ema-ribbon-squeeze", bars(), {{"fast", 3}, {"slow", 50}});
  same("fast 3", r2.value("fast"), ind::ema(close, 3));
  same("slow 50", r2.value("slow"), ind::ema(close, 50));
  ind::unregisterIndicator("custom:ema-ribbon-squeeze");
}

TEST("custom: the alert example (docs/alert-example.js) follows the format and an alert on its output fires on its signal bars") {
  CHECK_EQ(custom::check(QString::fromUtf8([] {
             QFile f(QT + "/docs/alert-example.js");
             return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
           }())),
           QStringList());
  QTemporaryDir dir;
  QFile::copy(QT + "/docs/alert-example.js", dir.filePath("alert-example.js"));
  const custom::Loaded l = custom::loadFile(dir.filePath("alert-example.js"));
  CHECK(l.ok());
  CHECK_EQ(l.id, QString("custom:alert-example"));
  const ind::DefPtr d = ind::byId("custom:alert-example");
  CHECK(d && !d->overlay && d->outputs.size() == 2 && d->outputs[0].kind == "histogram" && d->inputs[0].min == 1);
  const ind::Result out = ind::compute("custom:alert-example", bars());
  const Series close = ind::source(bars()), rsi = ind::rsi(close, 14);
  const QVector<bool> up = ind::crossover(ind::ema(close, 9), ind::ema(close, 21)), down = ind::crossunder(ind::ema(close, 9), ind::ema(close, 21));
  Series buy, sell;
  for (int i = 0; i < bars().size(); ++i) {
    buy << (up[i] && rsi[i] < 70 ? 1 : 0);
    sell << (down[i] && rsi[i] > 30 ? 1 : 0);
  }
  same("buy", out.value("buy"), buy);
  same("sell", out.value("sell"), sell);
  QVector<int> signalBars;
  for (int i = 0; i < buy.size(); ++i)
    if (buy[i] == 1) signalBars << i;
  CHECK(signalBars.size() > 0);   // the fixture has at least one buy signal
  int markerCount = 0;
  for (int i = 0; i < buy.size(); ++i) markerCount += buy[i] || sell[i];
  CHECK_EQ(out.markers.size(), markerCount);
#ifdef HAVE_BACKTEST
  const auto fired = bt::conditionFn(bars(), QJsonObject{{"sig", QJsonObject{{"id", "custom:alert-example"}, {"params", QJsonObject()}}}})(
      QJsonObject{{"left", "sig.buy"}, {"op", ">"}, {"right", 0}});
  QVector<int> firedBars;
  for (int i = 0; i < bars().size(); ++i)
    if (fired(i)) firedBars << i;
  CHECK_EQ(firedBars, signalBars);
#endif
  ind::unregisterIndicator("custom:alert-example");
}

TEST("custom: the template (docs/indicator-template.js) follows the custom indicator format") {
  QFile f(QT + "/docs/indicator-template.js");
  CHECK(f.open(QIODevice::ReadOnly));
  CHECK_EQ(custom::check(QString::fromUtf8(f.readAll()), "indicator-template.js"), QStringList());
}

TEST("custom: a malformed custom indicator is refused, with each problem named (the JS cases)") {
  CHECK_EQ(custom::check(goodWith("")), QStringList());
  problemMatches("calc: undefined", "needs a `calc(bars, p, ta)` function");
  problemMatches("name: ''", "needs a `name`");
  problemMatches("inputs: [{ key: 'length', type: 'int', default: 2.5 }]", "input \"length\": an int input needs a whole-number default");
  problemMatches("inputs: [{ key: 'ma', type: 'select', options: ['SMA'], default: 'EMA' }]", "input \"ma\": its default must be one of its options");
  problemMatches("inputs: [{ key: 'src', type: 'source', default: 'close' }]", "a source input's key must be \"source\"");
  problemMatches("inputs: [{ key: 'on', type: 'bool', default: true }]", "`type` must be one of int, float, select, source, time");
  problemMatches("outputs: [{ key: 'value', kind: 'area' }]", "`kind` must be \"line\" or \"histogram\"");
  problemMatches("outputs: [{ key: 'a' }, { key: 'a' }], calc: (b) => ({ a: b.map(() => 1) })", "output \"a\": the key is used twice");
  problemMatches("calc: (bars) => ({ value: bars.slice(1).map((b) => b.close) })",
                 "output \"value\": calc() returned 299 values, but there are 300 bars");
  problemMatches("calc: () => ({})", "output \"value\": calc() returned no list for it");
  problemMatches("calc: () => { throw new Error('boom'); }", "calc() stopped with an error on test bars: boom");
  problemMatches("calc: (bars) => ({ value: bars.map((b) => b.close), markers: [{ time: bars[0].time, position: 'top', shape: 'arrowUp' }] })",
                 "markers: each needs");
  problemMatches("levels: ['70']", "`levels` must be a list of numbers");
  problemMatches("calc: (bars) => ({ value: bars.map((b) => b.close), barColors: bars.slice(1).map(() => '#fff') })",
                 "barColors: return one colour per bar");
  // More of the format, with the JS wording.
  problemMatches("name: 'x'.repeat(81)", "its `name` is longer than 80 characters");
  problemMatches("overlay: 'yes'", "`overlay` must be true or false");
  problemMatches("inputs: {}", "`inputs` must be a list");
  problemMatches("inputs: [5]", "input 1: must be an object like { key, type, default }");
  problemMatches("inputs: [{ key: '1x', type: 'int', default: 1 }]", "input \"1x\": `key` must be letters, digits or _ and start with a letter");
  problemMatches("inputs: [{ key: 'f', type: 'float', default: NaN }]", "input \"f\": a float input needs a number default");
  problemMatches("inputs: [{ key: 't', type: 'time', default: -1 }]", "input \"t\": a time input's default is a bar time in seconds, or 0");
  problemMatches("inputs: [{ key: 's', type: 'select', options: [], default: 'a' }]", "input \"s\": a select input needs `options`, a list of text choices");
  problemMatches("inputs: [{ key: 'source', type: 'source', default: 'typical' }]",
                 "input \"source\": a source input's default must be one of close, open, high, low, hl2, hlc3, ohlc4, hlcc4, volume");
  problemMatches("inputs: [{ key: 'n', type: 'int', default: 1, min: '0' }]", "input \"n\": `min` must be a number");
  problemMatches("outputs: []", "`outputs` must be a list with at least one output");
  problemMatches("outputs: [null]", "output 1: must be an object like { key, kind, color }");
  problemMatches("outputs: [{ key: 'value', color: ' ' }]", "output \"value\": `color` must be a colour like #2962ff");
  problemMatches("outputs: [{ key: 'value', lineWidth: 1.5 }]", "output \"value\": `lineWidth` must be 1, 2, 3 or 4");
  problemMatches("calc: () => 5", "calc() must return an object, like { value: [...] }");
  problemMatches("calc: (bars) => ({ value: bars.map(() => 'x') })", "output \"value\": every value must be a number (or NaN/null)");
  problemMatches("calc: () => { throw 'boom'; }", "calc() stopped with an error on test bars: undefined");

  // A file without `export default`: the JS spreads undefined, so only the defaults and the id are left.
  const QStringList none = custom::check("const x = 1;\n");
  CHECK(none.contains("it needs a `name` (text)"));
  CHECK(none.contains("it needs a `calc(bars, p, ta)` function"));
  // Code that does not load gives the JS error.
  const QStringList bad = custom::check("export default { name: 'x', calc(bars) { return { value: [ } } };\n");
  CHECK_EQ(bad.size(), 1);
  CHECK(!bad.isEmpty() && !bad[0].isEmpty());
}

TEST("custom: the ES2019+ library functions custom code may use are there") {
  QTemporaryDir dir;
  write(dir, "modern.js", R"JS(
export default {
  name: 'Modern',
  calc(bars) {
    const checks = [
      JSON.stringify([1, [2, [3]]].flat()) === '[1,2,[3]]',
      JSON.stringify([1, [2, [3]]].flat(Infinity)) === '[1,2,3]',
      JSON.stringify([1, 2].flatMap((x) => [x, [x]])) === '[1,[1],2,[2]]',
      JSON.stringify([1, 2].flatMap((x) => x * 2)) === '[2,4]',
      [1, 2, 3].at(-1) === 3 && [1, 2, 3].at(5) === undefined && 'abc'.at(-1) === 'c',
      [1, 2, 3, 2].findLast((x) => x < 3) === 2 && [1, 2, 3, 2].findLastIndex((x) => x === 2) === 3,
      JSON.stringify([3, 1, 2].toSorted()) === '[1,2,3]',
      Object.fromEntries([['a', 1], ['b', 2]]).b === 2,
      Object.hasOwn({ a: 1 }, 'a') && !Object.hasOwn({}, 'toString'),
      'a.b.c'.replaceAll('.', '-') === 'a-b-c' && 'aXbX'.replaceAll(/X/g, '$&$&') === 'aXXbXX',
      '  x '.trimStart() === 'x ' && ' x  '.trimEnd() === ' x',
      typeof globalThis === 'object',
    ];
    const bad = checks.findIndex((c) => !c);
    return { value: bars.map(() => bad) };
  },
};
)JS");
  CHECK(custom::loadFile(dir.filePath("modern.js")).ok());
  CHECK_EQ(ind::compute("custom:modern", bars().mid(0, 2)).value("value"), Series({-1, -1}));
  ind::unregisterIndicator("custom:modern");
}

TEST("custom: a refused file is not registered and keeps the earlier version; a fixed one replaces it") {
  QTemporaryDir dir;
  const QString path = write(dir, "test-reload.js", goodWith(""));
  CHECK(custom::loadFile(path).ok());
  CHECK(ind::byId("custom:test-reload") != nullptr);
  CHECK_EQ(ind::byId("custom:test-reload")->name, QString("Ok"));
  write(dir, "test-reload.js", goodWith("calc: () => ({})"));
  const custom::Loaded broken = custom::loadFile(path);
  CHECK(!broken.ok());
  CHECK(broken.error.startsWith("This indicator doesn't follow the format (docs/custom-indicators.md):\n- output \"value\""));
  CHECK(ind::byId("custom:test-reload") && ind::byId("custom:test-reload")->name == "Ok");   // the working version stays
  write(dir, "test-reload.js", goodWith("name: 'Ok 2'"));
  CHECK(custom::loadFile(path).ok());
  CHECK_EQ(ind::byId("custom:test-reload")->name, QString("Ok 2"));
  ind::unregisterIndicator("custom:test-reload");

  write(dir, "test-broken.js", goodWith("calc: () => ({})"));
  CHECK(!custom::loadFile(dir.filePath("test-broken.js")).ok());
  CHECK(ind::byId("custom:test-broken") == nullptr);   // a refused indicator is not registered
}

TEST("custom: defaults like registerIndicator (group Custom, overlay on, one orange `value` line named after it)") {
  QTemporaryDir dir;
  write(dir, "plain.js", "export default { name: 'Plain', calc: (bars, p, ta) => ({ value: ta.sma(ta.source(bars), 3) }) };\n");
  CHECK(custom::loadFile(dir.filePath("plain.js")).ok());
  const ind::DefPtr d = ind::byId("custom:plain");
  CHECK(d && d->group == "Custom" && d->overlay && d->outputs.size() == 1 && d->outputs[0].key == "value" &&
        d->outputs[0].label == "Plain" && d->outputs[0].color == "#ff9800" && d->outputs[0].kind == "line" && d->inputs.isEmpty());
  const Series v = ind::compute("custom:plain", bars()).value("value");
  same("plain", v, ind::sma(ind::source(bars()), 3));
  ind::unregisterIndicator("custom:plain");
}

TEST("custom: every ta helper reaches the C++ primitive, with the JS defaults") {
  QTemporaryDir dir;
  write(dir, "ta-all.js", R"JS(
export default {
  name: 'ta', overlay: false,
  inputs: [{ key: 'source', type: 'source', default: 'hlc3' }],
  outputs: [{ key: 'value' }],
  levels: [30, 70],
  noAutoscale: true,
  calc(bars, p, ta) {
    const c = ta.source(bars), s = ta.source(bars, p.source), v = ta.source(bars, 'volume');
    const m = ta.macd(c), st = ta.supertrend(bars), d = ta.dmi(bars);
    const r = ta.compute('bb', bars, { length: 10 });
    return {
      value: s,
      sma: ta.sma(c, 5), ema: ta.ema(c, 5), rma: ta.rma(c, 5), wma: ta.wma(c, 5), vwma: ta.vwma(c, v, 5),
      hma: ta.hma(c, 9), dema: ta.dema(c, 5), tema: ta.tema(c, 5), alma: ta.alma(c, 9), linreg: ta.linreg(c, 9),
      stdev: ta.stdev(c, 5), dev: ta.dev(c, 5), highest: ta.highest(c, 5), lowest: ta.lowest(c, 5),
      change: ta.change(c), sum: ta.sum(c, 5), rsi: ta.rsi(c), atr: ta.atr(bars), tr: ta.tr(bars),
      xo: ta.crossover(c, 60000).map(Number), xu: ta.crossunder(c, ta.ema(c, 5)).map(Number),
      macd: m.macd, hist: m.hist, st: st.st, dir: st.dir, psar: ta.psar(bars), adx: d.adx,
      bbUpper: r.upper, bbKeys: [Object.keys(r).length],
      colors: m.colors,
    };
  },
};
)JS");
  const custom::Loaded l = custom::loadFile(dir.filePath("ta-all.js"));
  CHECK(l.ok());
  if (!l.ok()) {
    std::fprintf(stderr, "%s\n", l.error.toUtf8().constData());
    return;
  }
  const ind::DefPtr def = ind::byId("custom:ta-all");
  CHECK(def && !def->overlay && def->levels == QVector<double>({30, 70}) && def->extra.value("noAutoscale").toBool());
  const ind::Result r = ind::compute("custom:ta-all", bars());
  const Bars& b = bars();
  const Series c = ind::source(b), v = ind::source(b, "volume");
  same("value", r.value("value"), ind::source(b, "hlc3"));
  same("sma", r.value("sma"), ind::sma(c, 5));
  same("ema", r.value("ema"), ind::ema(c, 5));
  same("rma", r.value("rma"), ind::rma(c, 5));
  same("wma", r.value("wma"), ind::wma(c, 5));
  same("vwma", r.value("vwma"), ind::vwma(c, v, 5));
  same("hma", r.value("hma"), ind::hma(c, 9));
  same("dema", r.value("dema"), ind::dema(c, 5));
  same("tema", r.value("tema"), ind::tema(c, 5));
  same("alma", r.value("alma"), ind::alma(c, 9));
  same("linreg", r.value("linreg"), ind::linreg(c, 9));
  same("stdev", r.value("stdev"), ind::stdev(c, 5));
  same("dev", r.value("dev"), ind::dev(c, 5));
  same("highest", r.value("highest"), ind::highest(c, 5));
  same("lowest", r.value("lowest"), ind::lowest(c, 5));
  same("change", r.value("change"), ind::change(c));
  same("sum", r.value("sum"), ind::sum(c, 5));
  same("rsi", r.value("rsi"), ind::rsi(c, 14));
  same("atr", r.value("atr"), ind::atr(b, 14));
  same("tr", r.value("tr"), ind::tr(b));
  same("xo", r.value("xo"), ind::crossNumbers(ind::crossover(c, 60000)));
  same("xu", r.value("xu"), ind::crossNumbers(ind::crossunder(c, ind::ema(c, 5))));
  const ind::Macd m = ind::macdFrom(c);
  same("macd", r.value("macd"), m.macd);
  same("hist", r.value("hist"), m.hist);
  CHECK_EQ(r.colors, m.colors);
  const ind::Supertrend st = ind::supertrendOf(b);
  same("st", r.value("st"), st.st);
  same("dir", r.value("dir"), st.dir);
  same("psar", r.value("psar"), ind::psarOf(b));
  same("adx", r.value("adx"), ind::dmiOf(b).adx);
  same("bbUpper", r.value("bbUpper"), ind::compute("bb", b, {{"length", 10}}).value("upper"));
  CHECK_EQ(r.value("bbKeys"), Series({3}));
  ind::unregisterIndicator("custom:ta-all");
}

TEST("custom: calc errors after loading throw from compute; other threads are refused") {
  QTemporaryDir dir;
  write(dir, "late.js", R"JS(
export default {
  name: 'Late',
  calc(bars) {
    if (bars.length > 500) throw new Error('too many bars');
    return { value: bars.map((b) => b.close) };
  },
};
)JS");
  CHECK(custom::loadFile(dir.filePath("late.js")).ok());
  QString message;
  try {
    ind::compute("custom:late", bars());
  } catch (const std::exception& e) {
    message = e.what();
  }
  CHECK_EQ(message, QString("too many bars"));
  // Fine with few bars; refused off the GUI thread.
  CHECK_EQ(ind::compute("custom:late", bars().mid(0, 10)).value("value").size(), 10);
  QString offThread;
  std::thread t([&] {
    try {
      ind::compute("custom:late", bars().mid(0, 10));
    } catch (const std::exception& e) {
      offThread = e.what();
    }
  });
  t.join();
  CHECK_EQ(offThread, QString("Custom indicators run on the GUI thread only"));
  ind::unregisterIndicator("custom:late");
}

TEST("custom: loadFolder loads every .js file in name order") {
  QTemporaryDir dir;
  write(dir, "b-two.js", "export default { name: 'Two', calc: (bars) => ({ value: bars.map(() => 2) }) };\n");
  write(dir, "a-one.js", "export default { name: 'One', calc: (bars) => ({ value: bars.map(() => 1) }) };\n");
  write(dir, "c-bad.js", "export default { name: 'Bad' };\n");
  write(dir, "notes.txt", "not an indicator");
  const QList<custom::Loaded> all = custom::loadFolder(dir.path());
  CHECK_EQ(all.size(), 3);
  if (all.size() == 3) {
    CHECK(all[0].id == "custom:a-one" && all[0].ok());
    CHECK(all[1].id == "custom:b-two" && all[1].ok());
    CHECK(all[2].id == "custom:c-bad" && !all[2].ok() && all[2].problems.contains("it needs a `calc(bars, p, ta)` function"));
  }
  CHECK_EQ(ind::compute("custom:b-two", bars().mid(0, 3)).value("value"), Series({2, 2, 2}));
  ind::unregisterIndicator("custom:a-one");
  ind::unregisterIndicator("custom:b-two");
  CHECK_EQ(custom::idFor("signal-mine.js"), QString("custom:signal-mine"));
}

// Loads `code` as <name>.js, gives what its `value` output is on the first bar (NaN when it does not load: *error).
static double firstValue(const QString& name, const QString& code, QString* error = nullptr) {
  QTemporaryDir dir;
  const custom::Loaded l = custom::loadFile(write(dir, name + ".js", code));
  if (error) *error = l.error;
  if (!l.ok()) return NaN;
  const Series v = ind::compute("custom:" + name, bars().mid(0, 3)).value("value");
  ind::unregisterIndicator("custom:" + name);
  return v.isEmpty() ? NaN : v[0];
}

TEST("custom: object spread and rest load and run (V4 refused them)") {
  QString error;
  const double v = firstValue("spread", R"JS(
const base = { name: 'Spread', overlay: false };
const extra = { inputs: [{ key: 'length', type: 'int', default: 4 }] };
export default {
  ...base,
  ...extra,
  calc(bars, p) {
    const { length, ...rest } = { ...p, a: 1, b: 2 };
    const [first, ...others] = bars;
    const merged = { ...first, close: 7 };
    return { value: bars.map(() => length * 1000 + Object.keys(rest).length * 100 + others.length * 10 + merged.close - 7) };
  },
};
)JS", &error);
  CHECK_EQ(error, QString());
  CHECK_EQ(v, 4220.0);   // length 4, rest {a, b}, 2 other bars, merged.close 7
}

TEST("custom: class fields (public, private, static) load and run") {
  QString error;
  const double v = firstValue("classes", R"JS(
class Ma {
  len = 3;
  #calls = 0;
  static kind = 'sma';
  static #made = 0;
  constructor(len) { if (len) this.len = len; Ma.#made++; }
  run(ta, src) { this.#calls++; return ta.sma(src, this.len); }
  get calls() { return this.#calls; }
  static made() { return Ma.#made; }
  #secret() { return 1; }
  has(o) { return #calls in o && this.#secret() === 1; }
}
export default {
  name: 'Classes',
  calc(bars, p, ta) {
    const m = new Ma();
    const s = m.run(ta, ta.source(bars));
    const ok = Ma.kind === 'sma' && m.calls === 1 && Ma.made() >= 1 && m.has(m) && !m.has({});
    return { value: s.map((x, i) => (ok ? (i === 0 ? m.len : x) : NaN)) };
  },
};
)JS", &error);
  CHECK_EQ(error, QString());
  CHECK_EQ(v, 3.0);
}

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wtrigraphs"   // the "??=" below is JavaScript, not a C trigraph
#endif
TEST("custom: logical assignment (??=, ||=, &&=) and other ES2020+ syntax load and run") {
  QString error;
  const double v = firstValue("assign", R"JS(
export default {
  name: 'Assign',
  calc(bars) {
    const cache = {};
    cache.len ??= 5;
    cache.len ??= 9;
    let zero = 0;
    zero ||= 2;
    let one = 1;
    one &&= 3;
    const deep = bars[0]?.missing?.value ?? 'none';
    const big = 2n ** 10n === 1024n;
    const sep = 1_000;
    return { value: bars.map(() => (deep === 'none' && big ? cache.len * sep + zero * 10 + one : NaN)) };
  },
};
)JS", &error);
  CHECK_EQ(error, QString());
  CHECK_EQ(v, 5023.0);
}

TEST("custom: a syntax error says what and where") {
  const QStringList bad = custom::check("const a = 1;\nconst b = ;\nexport default { name: 'x' };\n");
  CHECK_EQ(bad, QStringList({"unexpected token in expression: ';' (line 2, column 11)"}));
  const QStringList bad2 = custom::check("export default { name: 'x', calc(bars) { return { value: [ } } };\n");
  CHECK(bad2.size() == 1 && bad2[0].startsWith("unexpected token") && bad2[0].endsWith("(line 1, column 60)"));
  // An error thrown by the file's own top-level code is given as it is (its message), as in the JS.
  CHECK_EQ(custom::check("throw new TypeError('top');\nexport default {};\n"), QStringList({"top"}));
  // Imports: only files, by path.
  CHECK_EQ(custom::check("import x from 'lodash';\nexport default {};\n"),
           QStringList({"Cannot find package 'lodash' (custom indicators can only import files)"}));
}

TEST("custom: an endless loop stops after a few seconds with a clear error, also inside try/catch") {
  QElapsedTimer t;
  t.start();
  CHECK_EQ(custom::check("export default { name: 'Loop', calc() { for (;;) { try { while (true) {} } catch (e) {} } } };\n", "loop.js"),
           QStringList({"Custom indicator loop.js took too long (stopped after 5 s)"}));
  CHECK(t.elapsed() >= 4000 && t.elapsed() < 15000);
  // After loading: compute throws the same message, and the indicator still works afterwards.
  QTemporaryDir dir;
  CHECK(custom::loadFile(write(dir, "late-loop.js", R"JS(
export default {
  name: 'Late loop',
  calc(bars) {
    if (bars.length > 500) for (;;) {}
    return { value: bars.map((b) => b.close) };
  },
};
)JS")).ok());
  QString message;
  try {
    ind::compute("custom:late-loop", bars());
  } catch (const std::exception& e) {
    message = e.what();
  }
  CHECK_EQ(message, QString("Custom indicator late-loop.js took too long (stopped after 5 s)"));
  CHECK_EQ(ind::compute("custom:late-loop", bars().mid(0, 4)).value("value").size(), 4);
  ind::unregisterIndicator("custom:late-loop");
}

TEST("custom: deep recursion throws RangeError instead of overflowing the thread's stack (also a 512 KB thread)") {
  const QString src = "export default { name: 'Deep', calc() { const f = (n) => 1 + f(n + 1); f(0); } };\n";
  const QString want = "calc() stopped with an error on test bars: Maximum call stack size exceeded";
  CHECK_EQ(custom::check(src), QStringList({want}));
  QStringList off;
  std::thread th([&] { off = custom::check(src); });   // macOS gives a secondary thread 512 KB
  th.join();
  CHECK_EQ(off, QStringList({want}));
}

TEST("custom: a length that isn't a number gives NaN like the JS (not 0: highest/lowest/sum were +-Infinity, change 0)") {
  // indicators.js with n = NaN: every `i >= n`, `i < len` from NaN - 1 and `valid === n` test is false, so every value
  // is NaN (node: ta.highest(c, "abc") etc. are all NaN). linreg with a NaN offset: intercept + slope * NaN.
  QTemporaryDir dir;
  write(dir, "nan-len.js", R"JS(
export default {
  name: 'NaN lengths',
  calc(bars, p, ta) {
    const c = ta.source(bars);
    return {
      value: ta.highest(c, 'abc'), lowest: ta.lowest(c, 'abc'), sum: ta.sum(c, 'abc'), change: ta.change(c, 'abc'),
      sma: ta.sma(c, 'abc'), ema: ta.ema(c), wma: ta.wma(c, NaN), hma: ta.hma(c, {}), alma: ta.alma(c, 'abc'),
      stdev: ta.stdev(c, 'x'), rsi: ta.rsi(c, 'x'), atr: ta.atr(bars, 'x'), linreg: ta.linreg(c, 9, 'x'),
      inf: ta.highest(c, Infinity), nullLen: ta.change(c, null),
    };
  },
};
)JS");
  const custom::Loaded l = custom::loadFile(dir.filePath("nan-len.js"));
  CHECK(l.ok());
  if (!l.ok()) return;
  const ind::Result r = ind::compute("custom:nan-len", bars());
  const Series nans(bars().size(), NaN);
  for (const char* k : {"value", "lowest", "sum", "change", "sma", "ema", "wma", "hma", "alma", "stdev", "rsi", "atr", "linreg", "inf"})
    same(k, r.value(k), nans);
  same("nullLen", r.value("nullLen"), Series(bars().size(), 0));   // change(src, null): i >= null is i >= 0 -> v - v
  ind::unregisterIndicator("custom:nan-len");
  // The built-ins read a text length the same way (the dump test compares every one with the JS).
  same("donchian", ind::compute("donchian", bars(), {{"length", "abc"}}).value("upper"), nans);
}

TEST("custom: runaway arrays are refused before C++ copies them (outside the 256 MB QuickJS limit)") {
  // A result list far longer than the bars: an error, at load (the trial) and after.
  QElapsedTimer t;
  t.start();
  const QStringList big = custom::check("export default { name: 'Big', calc: () => ({ value: new Array(5e6) }) };\n", "big.js");
  CHECK_EQ(big.size(), 1);
  CHECK(big.value(0).startsWith("calc() stopped with an error on test bars: a list of 5000000 values is too long for Deepwater: at most 100000"));
  // Lists nested inside the result count together.
  const QStringList nested = custom::check(
      "export default { name: 'Nested', calc: (bars) => ({ value: bars.map((b) => b.close), x: Array.from({ length: 20 }, () => new Array(90000).fill(1)) }) };\n",
      "nested.js");
  CHECK_EQ(nested, QStringList({"calc() stopped with an error on test bars: the lists returned hold too many values together (one value per bar is expected)"}));
  // A ta input far longer than the bars: a RangeError the indicator sees (and may catch).
  const QStringList ta = custom::check(
      "export default { name: 'TaBig', calc: (bars, p, ta) => ({ value: ta.sma(new Array(3e6).fill(1), 5).slice(0, bars.length) }) };\n",
      "ta-big.js");
  CHECK_EQ(ta.size(), 1);
  CHECK(ta.value(0).contains("array too long for ta (3000000 elements, at most 100000: 4 per bar)"));
  CHECK(t.elapsed() < 5000);
  QTemporaryDir dir;
  CHECK(custom::loadFile(write(dir, "late-big.js", R"JS(
export default {
  name: 'Late big',
  calc(bars) {
    if (bars.length > 500) return { value: new Array(bars.length * 5 + 100000) };
    return { value: bars.map((b) => b.close) };
  },
};
)JS")).ok());
  QString message;
  try {
    ind::compute("custom:late-big", bars());
  } catch (const std::exception& e) {
    message = e.what();
  }
  CHECK(message.startsWith(QString("a list of %1 values is too long for Deepwater").arg(bars().size() * 5 + 100000)));
  CHECK_EQ(ind::compute("custom:late-big", bars().mid(0, 4)).value("value").size(), 4);   // still works afterwards
  ind::unregisterIndicator("custom:late-big");
}

TEST("custom: one load runs calc once (the trial), as indicators.js; the top-level code once") {
  QTemporaryDir dir;
  write(dir, "count.js", R"JS(
globalThis.loads = (globalThis.loads ?? 0) + 1;
let calls = 0;
export default {
  name: 'Count',
  calc(bars) {
    calls++;
    return { value: bars.map(() => calls * 10 + globalThis.loads) };
  },
};
)JS");
  CHECK(custom::loadFile(dir.filePath("count.js")).ok());
  // This compute is calc's second call: the first was the format check's trial run.
  const Series v = ind::compute("custom:count", bars().mid(0, 2)).value("value");
  CHECK_EQ(v, Series({21, 21}));
  ind::unregisterIndicator("custom:count");
}
