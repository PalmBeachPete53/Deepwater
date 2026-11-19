// Pine Script files as Charts indicators (src/charts/core/PineIndicators.cpp): loading, inputs, outputs, levels,
// markers, bar colours, per bar colours, and the background cache (pending, then the result; the last result kept
// while a newer one is computed).
#include "Check.h"
#include "charts/core/PineIndicators.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

using namespace charts;

namespace {
const Bars& btc() {
  static const Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    Bars out;
    if (!f.open(QIODevice::ReadOnly)) return out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out << Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(), o.value("high").toDouble(),
                 o.value("low").toDouble(), o.value("close").toDouble(), o.value("volume").toDouble()};
    }
    return out;
  }();
  return b;
}

const char* const SCRIPT = R"(//@version=5
indicator("Test MAs", shorttitle="TMA", overlay=true)
len = input.int(14, "Length", minval=1, maxval=400)
mult = input.float(2.0, "Mult", step=0.5)
src = input.source(close, "Source")
on = input.bool(true, "Show")
kind = input.string("EMA", "Kind", options=["SMA", "EMA"])
col = input.color(color.red, "Colour")
ma = kind == "SMA" ? ta.sma(src, len) : ta.ema(src, len)
plot(on ? ma : na, "MA", color = ma > ma[1] ? color.green : color.red, linewidth=2)
plot(ta.stdev(src, len) * mult, "Dev", style=plot.style_histogram, color=color.gray)
plot(ma, "Dots", style=plot.style_circles)
hline(50000, "Level")
plotshape(ta.crossover(close, ma), "Up", shape.triangleup, location.belowbar, color.green, text="buy")
plotchar(ta.crossunder(close, ma), "Down", "x", location.abovebar, color.red)
barcolor(close > open ? color.teal : na)
bgcolor(close > ma ? color.new(color.green, 90) : na)
)";

QString writeTemp(QTemporaryDir& dir, const QString& name, const QString& text) {
  const QString path = dir.filePath(name);
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) f.write(text.toUtf8());
  return path;
}

// unregisters what a test loaded (other tests walk the whole registry)
struct Unregister {
  QStringList ids;
  ~Unregister() { for (const QString& id : ids) { ind::unregisterIndicator(id); pine::Cache::instance().forget(id); } }
};

// waits for Cache::ready (or the timeout)
bool waitReady(int ms = 30000) {
  QEventLoop loop;
  bool got = false;
  const auto c = QObject::connect(&pine::Cache::instance(), &pine::Cache::ready, &loop, [&] { got = true; loop.quit(); });
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
  QObject::disconnect(c);
  return got;
}
}  // namespace

TEST("pine indicators: a .pine file loads as indicator pine:<name> with its inputs, outputs and levels") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "test-mas.pine", SCRIPT));
  Unregister guard_l{{l.id}};
  if (!l.ok()) { check::fail(__FILE__, __LINE__, l.error); return; }
  CHECK_EQ(l.id, QString("pine:test-mas"));
  const ind::DefPtr d = ind::byId(l.id);
  CHECK(d);
  if (!d) return;
  CHECK_EQ(d->name, QString("Test MAs (TMA)"));
  CHECK(d->overlay);
  CHECK_EQ(d->group, QString("Pine Script"));
  QStringList keys, types;
  for (const auto& in : d->inputs) { keys << in.key; types << in.type; }
  // the colour input is not editable yet; the first source input is "source"
  CHECK_EQ(keys, (QStringList{"Length", "Mult", "source", "Show", "Kind"}));
  CHECK_EQ(types, (QStringList{"int", "float", "source", "select", "select"}));
  CHECK_EQ(d->inputs[0].def.toInt(), 14);
  CHECK_EQ(d->inputs[0].min, 1.0);
  CHECK_EQ(d->inputs[0].max, 400.0);
  CHECK_EQ(d->inputs[3].options, (QStringList{"true", "false"}));
  CHECK_EQ(d->inputs[4].options, (QStringList{"SMA", "EMA"}));
  QStringList outs, kinds;
  for (const auto& o : d->outputs) { outs << o.key; kinds << o.kind; }
  CHECK_EQ(outs, (QStringList{"MA", "Dev", "Dots"}));
  CHECK_EQ(kinds, (QStringList{"line", "histogram", "line"}));
  CHECK(d->outputs[2].extra.value("dots").toBool());
  CHECK_EQ(d->outputs[0].lineWidth, 2);
  CHECK_EQ(d->levels, (QVector<double>{50000}));
}

TEST("pine indicators: off the GUI thread calc runs the script and gives outputs, colours, markers and bar colours") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "test-mas2.pine", SCRIPT));
  Unregister guard_l{{l.id}};
  CHECK(l.ok());
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  ind::Result r;
  QThread* t = QThread::create([&] { r = d->calc(btc(), ind::defaults(l.id)); });
  t->setStackSize(16u << 20);
  t->start();
  t->wait();
  delete t;
  CHECK(r.error.isEmpty());
  CHECK(!r.pending);
  CHECK(r.series.contains("MA") && r.series.contains("Dev") && r.series.contains("Dots"));
  CHECK_EQ(r.series.value("MA").size(), btc().size());
  // EMA 14 of the closes, the first value at bar 13
  CHECK(std::isnan(r.series.value("MA")[12]) && std::isfinite(r.series.value("MA")[13]));
  CHECK_EQ(r.seriesColors.value("MA").size(), btc().size());   // green / red by slope
  CHECK(r.seriesColors.value("MA").contains("#4CAF50") || r.seriesColors.value("MA").contains("#4caf50"));
  CHECK(!r.markers.isEmpty());
  bool up = false, down = false;
  for (const auto& m : r.markers) {
    if (m.shape == "arrowUp" && m.position == "belowBar" && m.text == "buy") up = true;
    if (m.position == "aboveBar" && m.text == "x") down = true;
  }
  CHECK(up);
  CHECK(down);
  CHECK_EQ(r.barColors.size(), btc().size());
  int teal = 0;
  for (int i = 0; i < btc().size(); ++i)
    if (!r.barColors[i].isEmpty()) { ++teal; CHECK(btc()[i].close > btc()[i].open); }
  CHECK(teal > 100);
}

TEST("pine indicators: settings reach the script (length, a switch, a select, the source)") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "test-mas3.pine", SCRIPT));
  Unregister guard_l{{l.id}};
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  Params p = ind::defaults(l.id);
  p["Length"] = 30;
  p["Kind"] = "SMA";
  p["source"] = "high";
  ind::Result a;
  QThread* t = QThread::create([&] { a = d->calc(btc(), p); });
  t->setStackSize(16u << 20);
  t->start(); t->wait(); delete t;
  // SMA 30 of the highs at the last bar
  double s = 0;
  for (int i = btc().size() - 30; i < btc().size(); ++i) s += btc()[i].high;
  CHECK_NEAR(a.series.value("MA").last(), s / 30, 1e-6);
  p["Show"] = "false";
  ind::Result b;
  t = QThread::create([&] { b = d->calc(btc(), p); });
  t->setStackSize(16u << 20);
  t->start(); t->wait(); delete t;
  CHECK(std::isnan(b.series.value("MA").last()));
}

TEST("pine indicators: while a chart draws, the first call is pending, the result comes with ready()") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "test-async.pine", SCRIPT));
  Unregister guard_l{{l.id}};
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  const Params p = ind::defaults(l.id);
  const pine::ChartScope chart("BINANCE:BTCUSDT");
  QElapsedTimer t;
  t.start();
  const ind::Result first = d->calc(btc(), p);
  CHECK(t.elapsed() < 200);   // never waits for the script
  CHECK(first.pending);
  CHECK(waitReady());
  const ind::Result second = d->calc(btc(), p);
  CHECK(!second.pending);
  CHECK(second.series.contains("MA"));
  // one more bar: no wait, the last result padded with a blank at the new bar, then the new one
  Bars more = btc();
  Bar nb = more.last();
  nb.time += 86400;
  more << nb;
  const ind::Result stale = d->calc(more, p);
  CHECK(!stale.pending);
  CHECK_EQ(stale.series.value("MA").size(), more.size());
  CHECK(std::isnan(stale.series.value("MA").last()));
  CHECK(waitReady());
  const ind::Result fresh = d->calc(more, p);
  CHECK(std::isfinite(fresh.series.value("MA").last()));
}

TEST("pine indicators: outside a chart (tester, alerts, screener) calc answers at once, and the chart finds it after") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "test-sync.pine", SCRIPT));
  Unregister guard_l{{l.id}};
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  Params p = ind::defaults(l.id);
  p["Length"] = 21;
  const ind::Result now = d->calc(btc(), p);   // GUI thread, no chart scope
  CHECK(!now.pending);
  CHECK(std::isfinite(now.series.value("MA").last()));
  QElapsedTimer t;
  t.start();
  const pine::ChartScope chart(QString());
  const ind::Result again = d->calc(btc(), p);
  CHECK(!again.pending);
  CHECK(t.elapsed() < 50);   // kept: no second run
  CHECK_EQ(again.series.value("MA").last(), now.series.value("MA").last());
}

TEST("pine indicators: titles with spaces become valid keys, the shorttitle names it in the legend, a 2nd source is a select") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "spaces.pine",
      "//@version=5\nindicator(\"Two EMAs crossing\", shorttitle=\"2EMA\", overlay=true)\n"
      "fastLen = input.int(9, \"Fast EMA\")\nslowLen = input.int(21, \"Slow EMA\")\n"
      "a = input.source(close, \"Source A\")\nb = input.source(open, \"Source B\")\n"
      "plot(ta.ema(a, fastLen), \"Fast line\")\nplot(ta.ema(b, slowLen), \"Slow line\")\n"));
  Unregister guard{{l.id}};
  if (!l.ok()) { check::fail(__FILE__, __LINE__, l.error); return; }
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  CHECK_EQ(d->name, QString("Two EMAs crossing (2EMA)"));
  QStringList keys, types;
  for (const auto& in : d->inputs) { keys << in.key; types << in.type; }
  CHECK_EQ(keys, (QStringList{"Fast_EMA", "Slow_EMA", "source", "Source_B"}));
  CHECK_EQ(types, (QStringList{"int", "int", "source", "select"}));
  CHECK_EQ(d->inputs[0].label, QString("Fast EMA"));
  QStringList outs;
  for (const auto& o : d->outputs) outs << o.key;
  CHECK_EQ(outs, (QStringList{"Fast_line", "Slow_line"}));
  Params p = ind::defaults(l.id);
  p["Slow_EMA"] = 5;
  p["Source_B"] = "high";
  ind::Result r;
  QThread* t = QThread::create([&] { r = d->calc(btc(), p); });
  t->setStackSize(16u << 20);
  t->start(); t->wait(); delete t;
  CHECK(r.series.contains("Fast_line") && r.series.contains("Slow_line"));
  // EMA 5 of the highs: seeded with the SMA of the first 5, then recursive
  double e = 0;
  for (int i = 0; i < 5; ++i) e += btc()[i].high;
  e /= 5;
  for (int i = 5; i < btc().size(); ++i) e += (btc()[i].high - e) * (2.0 / 6);
  CHECK_NEAR(r.series.value("Slow_line").last(), e, 1e-6);
}

TEST("pine indicators: a broken script is refused with the error, a library too") {
  QTemporaryDir dir;
  const auto bad = pine::loadFile(writeTemp(dir, "bad.pine", "//@version=5\nindicator(\"Bad\")\nplot(close +)\n"));
  Unregister guard_bad{{bad.id}};
  CHECK(!bad.ok());
  CHECK(!ind::byId(bad.id));
  const auto lib = pine::loadFile(writeTemp(dir, "lib.pine", "//@version=5\nlibrary(\"Lib\")\nexport f(x) => x * 2\n"));
  Unregister guard_lib{{lib.id}};
  CHECK(!lib.ok());
  CHECK(pine::check("//@version=5\nindicator(\"Fine\")\nplot(close)\n").isEmpty());
}

// Audit P1: outside a chart on the GUI thread (alerts every 15 s, /api/scan) a run had the worker's limits (60 s,
// 1 GB) and the window waited for it. Now short limits, and a script they stopped answers its error at once for a while.
TEST("pine indicators: off-chart runs on the GUI thread have short limits; a stopped script is not run again at once") {
  QTemporaryDir dir;
  // quick on the 400 sample bars of the load, endless on more bars than that
  const QString slow = R"(//@version=5
indicator("Slow on many bars")
var float s = 0.0
if barstate.islast and bar_index > 450
    for i = 0 to 100000000
        s += math.sin(i)
plot(close + s * 0, "v")
)";
  const auto l = pine::loadFile(writeTemp(dir, "test-slow-gui.pine", slow));
  Unregister guard_l{{l.id}};
  CHECK(l.ok());
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  pine::Cache::setGuiLimitsForTests(400, 0);
  QElapsedTimer t;
  t.start();
  const ind::Result r = d->calc(btc(), ind::defaults(l.id));   // GUI thread, no chart scope
  CHECK(!r.error.isEmpty());
  CHECK(t.elapsed() < 3000);
  t.restart();
  Params other = ind::defaults(l.id);
  const ind::Result again = d->calc(btc().mid(0, 900), other);   // other bars: not the cached result, still no run
  CHECK_EQ(again.error, r.error);
  CHECK(t.elapsed() < 100);
  pine::Cache::setGuiLimitsForTests(0, 0);
}

// Audit G10: the synchronous path stored every result and never evicted (an alert's forming bar: a new key each time).
TEST("pine indicators: results of off-chart runs are evicted like the chart's") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "test-evict.pine", "//@version=5\nindicator(\"E\")\nplot(close, \"c\")\n"));
  Unregister guard_l{{l.id}};
  const ind::DefPtr d = ind::byId(l.id);
  if (!d) return;
  for (int i = 0; i < 52; ++i) d->calc(btc().mid(0, 100 + i), ind::defaults(l.id));
  CHECK(pine::Cache::instance().keptForTests() <= 48);
}

// Audit P1: the .pine files of the custom folder were checked on the GUI thread at start-up (a script with an endless
// loop froze every launch for 20 s); loadFilesAsync checks them on a Pine thread and registers them on the caller's.
TEST("pine indicators: loadFilesAsync checks on a worker, registers on the caller's thread") {
  QTemporaryDir dir;
  const QString a = writeTemp(dir, "test-async-a.pine", SCRIPT);
  const QString b = writeTemp(dir, "test-async-b.pine", "//@version=5\nindicator(\"B\")\nplot(ta.nosuchthing(close))\n");
  Unregister guard{{pine::idFor("test-async-a.pine"), pine::idFor("test-async-b.pine")}};
  QObject ctx;
  QVector<pine::Loaded> got;
  Qt::HANDLE where = nullptr;
  QEventLoop loop;
  pine::loadFilesAsync({a, b}, &ctx, [&](const QVector<pine::Loaded>& l) {
    got = l;
    where = QThread::currentThreadId();
    loop.quit();
  });
  CHECK(!ind::byId(pine::idFor("test-async-a.pine")));   // not registered before the check ends
  QTimer::singleShot(60000, &loop, &QEventLoop::quit);
  loop.exec();
  CHECK_EQ(got.size(), 2);
  if (got.size() == 2) {
    CHECK(got[0].ok());
    CHECK(!got[1].ok());
  }
  CHECK(where == QThread::currentThreadId());
  CHECK(ind::byId(pine::idFor("test-async-a.pine")) != nullptr);
  CHECK(!ind::byId(pine::idFor("test-async-b.pine")));
}
