// Pine Script runtime (src/charts/core/Pine.cpp): PineTS in QuickJS-ng with Deepwater's Intl over QTimeZone, against
// the same PineTS in Node (V8, real Intl/ICU), value by value: tests/charts/core/fixtures/pine-node.json, written by
// tests/charts/tools/dump-pine.mjs. Then the sandbox, the limits, the errors and the helpers.
#include "Check.h"
#include "charts/core/Pine.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

using namespace charts;

namespace {
QString fixtureDir() { return QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("fixtures"); }
QJsonObject readJson(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) { check::fail(__FILE__, __LINE__, "cannot open " + path); return {}; }
  return QJsonDocument::fromJson(f.readAll()).object();
}
Bars barsOf(const QJsonArray& a) {
  Bars out;
  for (const QJsonValue& v : a) {
    const QJsonObject o = v.toObject();
    out << Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(), o.value("high").toDouble(),
               o.value("low").toDouble(), o.value("close").toDouble(), o.value("volume").toDouble()};
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
const QJsonObject& node() {
  static const QJsonObject o = readJson(fixtureDir() + "/pine-node.json");
  return o;
}
pine::Options optionsOf(const QJsonObject& o) {
  pine::Options opt;
  opt.symbol = o.value("symbol").toString();
  opt.timeframe = o.value("timeframe").toString();
  opt.timezone = o.value("timezone").toString();
  opt.inputs = o.value("inputs").toObject();
  return opt;
}
bool same(double a, double b) { return (std::isnan(a) && std::isnan(b)) || std::memcmp(&a, &b, sizeof a) == 0 || a == b; }

// One case of the Node fixture: every plot, every bar, bit for bit (both sides run the same PineTS release)
void compareCase(const QString& name) {
  QJsonObject c;
  for (const QJsonValue& v : node().value("cases").toArray())
    if (v.toObject().value("name").toString() == name) c = v.toObject();
  if (c.isEmpty()) { check::fail(__FILE__, __LINE__, "no case " + name + " in pine-node.json"); return; }
  const Bars bars = c.value("bars").isString() ? btc() : barsOf(c.value("bars").toArray());
  const pine::Result r = pine::run(c.value("source").toString(), bars, optionsOf(c.value("options").toObject()));
  if (!r.ok()) { check::fail(__FILE__, __LINE__, name + ": " + r.error); return; }
  const QJsonArray keys = c.value("keys").toArray();
  QStringList got;
  for (const auto& p : r.plots) got << p.key;
  QStringList want;
  for (const QJsonValue& k : keys) want << k.toString();
  CHECK_EQ(got, want);
  const QJsonObject plots = c.value("plots").toObject();
  for (const QString& k : want) {
    const pine::Plot* p = r.plot(k);
    if (!p) continue;
    const QJsonObject e = plots.value(k).toObject();
    CHECK_EQ(p->title, e.value("title").toString());
    CHECK_EQ(p->options.value("style").toString(), e.value("style").toString());
    const QJsonArray ev = e.value("values").toArray();
    CHECK_EQ(p->values.size(), ev.size());
    int bad = 0, first = -1;
    for (qsizetype i = 0; i < std::min(p->values.size(), ev.size()); ++i) {
      const double want = ev[i].isNull() ? NaN : ev[i].toDouble();
      if (!same(p->values[i], want)) { if (first < 0) first = int(i); ++bad; }
    }
    if (bad)
      check::fail(__FILE__, __LINE__, QString("%1/%2: %3 values differ, first at bar %4 (%5 vs %6)").arg(name, k).arg(bad).arg(first)
                                          .arg(p->values[first], 0, 'g', 17).arg(ev[first].toDouble(), 0, 'g', 17));
    const QJsonArray ec = e.value("colors").toArray();
    if (e.value("colors").isNull()) {
      CHECK(p->colors.isEmpty());
      if (!e.value("color").toString().isEmpty()) CHECK_EQ(p->options.value("color").toString(), e.value("color").toString());
    } else {
      CHECK_EQ(p->colors.size(), ec.size());
      int badc = 0;
      for (qsizetype i = 0; i < std::min(p->colors.size(), ec.size()); ++i) if (p->colors[i] != ec[i].toString()) ++badc;
      if (badc) check::fail(__FILE__, __LINE__, QString("%1/%2: %3 colours differ").arg(name, k).arg(badc));
    }
  }
}
}  // namespace

TEST("pine: the fixture was written by the PineTS release embedded") {
  CHECK_EQ(node().value("pinets").toString(), pine::pinetsVersion());
  CHECK(node().value("cases").toArray().size() >= 5);
}

TEST("pine: 17 key indicators on BTC daily equal PineTS in Node, bit for bit") { compareCase("key-1d"); }
TEST("pine: time zones over the spring summer-time changes equal Node's real Intl") { compareCase("zones-spring"); }
TEST("pine: time zones over the autumn summer-time changes equal Node's real Intl") { compareCase("zones-autumn"); }
TEST("pine: inputs given by title replace the script's defaults") { compareCase("inputs"); }
TEST("pine: colours, shapes, hline, bgcolor, var, history and bar_index equal Node") { compareCase("styles"); }

TEST("pine: the indicator() arguments and the drawings come back") {
  const auto r = pine::run("//@version=5\nindicator(\"My title\", shorttitle=\"MT\", overlay=true)\n"
                           "if barstate.islast\n    label.new(bar_index, high, \"last\")\nplot(close)\n", btc());
  CHECK(r.ok());
  CHECK_EQ(r.indicator.value("title").toString(), QString("My title"));
  CHECK_EQ(r.indicator.value("shorttitle").toString(), QString("MT"));
  CHECK(r.indicator.value("overlay").toBool());
  CHECK(r.drawings.contains("__labels__"));
  CHECK_EQ(r.plots.size(), 1);
}

TEST("pine: log.info and console lines are kept, at most 1000") {
  const auto r = pine::run("//@version=5\nindicator(\"Log\")\nlog.info(\"bar {0}\", bar_index)\nlog.warning(\"again {0}\", bar_index)\nplot(close)\n", btc());
  CHECK(r.ok());
  CHECK_EQ(r.log.size(), 1001);   // 1000 lines + "(n more lines not kept)"
  CHECK(r.log.first().contains("bar 0"));
  CHECK(r.log.last().contains("not kept"));
}

TEST("pine: a syntax error is an error, not a crash") {
  const auto r = pine::run("//@version=5\nindicator(\"Bad\")\nplot(close +)\n", btc());
  CHECK(!r.ok());
  CHECK(!r.error.isEmpty());
  CHECK(r.plots.isEmpty());
}

TEST("pine: an unknown function is an error") {
  const auto r = pine::run("//@version=5\nindicator(\"Bad\")\nplot(ta.nosuchthing(close, 3))\n", btc());
  CHECK(!r.ok());
}

TEST("pine: a script that runs too long is stopped at the timeout") {
  pine::Options o;
  o.timeoutMs = 1500;
  QElapsedTimer t;
  t.start();
  const auto r = pine::run("//@version=5\nindicator(\"Slow\")\nvar float s = 0.0\nfor i = 0 to 400000\n    s += math.sin(i)\nplot(s)\n", btc(), o);
  CHECK(!r.ok());
  CHECK(t.elapsed() < 8000);
  std::printf("    (stopped with: %s, after %lld ms)\n", r.error.toUtf8().constData(), static_cast<long long>(t.elapsed()));
}

TEST("pine: too little memory fails cleanly") {
  pine::Options o;
  o.memoryMB = 16;   // the floor: PineTS itself fits, a script piling up millions of floats does not
  const auto r = pine::run("//@version=5\nindicator(\"M\")\nvar a = array.new_float(0)\nfor i = 0 to 3000\n    array.push(a, i * 1.5)\nplot(array.size(a))\n", btc(), o);
  CHECK(!r.ok());
  std::printf("    (%s)\n", r.error.toUtf8().constData());
  // and the next run with the default limit works (nothing is shared between runs)
  CHECK(pine::run("//@version=5\nindicator(\"M\")\nplot(close)\n", btc()).ok());
}

TEST("pine: the sandbox has no network, no files, no processes") {
  // PineTS also runs its own JavaScript syntax; that code sees the same globals as the library
  const auto r = pine::run(
      "(context) => { const { plot } = context.pine; "
      "plot(typeof fetch === 'undefined' && typeof WebSocket === 'undefined' && typeof XMLHttpRequest === 'undefined' "
      "&& typeof require === 'undefined' && typeof std === 'undefined' && typeof os === 'undefined' "
      "&& typeof process === 'undefined' ? 1 : 0, 'sealed'); }",
      btc());
  if (!r.ok()) { std::printf("    (PineTS refused the JS form: %s)\n", r.error.toUtf8().constData()); return; }
  const pine::Plot* p = r.plot("sealed");
  CHECK(p);
  if (p) CHECK_EQ(p->values.last(), 1.0);
}

TEST("pine: no bars, no run") { CHECK(!pine::run("//@version=5\nindicator(\"x\")\nplot(close)\n", {}).ok()); }

TEST("pine: runs are independent (a var does not survive into the next run)") {
  const QString src = "//@version=5\nindicator(\"Count\")\nvar int n = 0\nn += 1\nplot(n, \"n\")\n";
  const auto a = pine::run(src, btc()), b = pine::run(src, btc());
  CHECK(a.ok() && b.ok());
  if (a.ok() && b.ok()) { CHECK_EQ(a.plot("n")->values.last(), double(btc().size())); CHECK_EQ(b.plot("n")->values.last(), double(btc().size())); }
}

TEST("pine: runAsync delivers on the caller's thread") {
  QEventLoop loop;
  QObject ctx;
  bool got = false;
  Qt::HANDLE where = nullptr;
  pine::runAsync("//@version=5\nindicator(\"A\")\nplot(ta.sma(close, 5), \"s\")\n", btc(), {}, &ctx, [&](const pine::Result& r) {
    got = r.ok() && r.plot("s");
    where = QThread::currentThreadId();
    loop.quit();
  });
  QTimer::singleShot(30000, &loop, &QEventLoop::quit);
  loop.exec();
  CHECK(got);
  CHECK(where == QThread::currentThreadId());
}

TEST("pine: wallClock follows the zone's rules, summer time included") {
  const auto at = [](int y, int mo, int d, int h, int mi) { return QDateTime(QDate(y, mo, d), QTime(h, mi), QTimeZone::UTC).toMSecsSinceEpoch(); };
  auto w = pine::wallClock("America/New_York", at(2026, 3, 8, 6, 59));   // 01:59 EST
  CHECK(w && w->hour == 1 && w->minute == 59 && !w->dst);
  w = pine::wallClock("America/New_York", at(2026, 3, 8, 7, 0));          // 03:00 EDT
  CHECK(w && w->hour == 3 && w->dst && w->weekday == 0);
  w = pine::wallClock("Europe/Dublin", at(2026, 7, 1, 12, 0));            // "Irish Standard Time" = summer
  CHECK(w && w->hour == 13 && w->dst);
  w = pine::wallClock("Europe/Dublin", at(2026, 1, 15, 12, 0));
  CHECK(w && w->hour == 12 && !w->dst);
  w = pine::wallClock("Australia/Sydney", at(2026, 1, 15, 0, 0));         // southern summer
  CHECK(w && w->hour == 11 && w->dst);
  w = pine::wallClock("Asia/Kolkata", at(2026, 1, 15, 0, 0));
  CHECK(w && w->hour == 5 && w->minute == 30 && !w->dst);
  w = pine::wallClock("UTC", at(2026, 11, 18, 0, 5));
  CHECK(w && w->hour == 0 && w->minute == 5 && w->weekday == 3 && !w->dst);
  CHECK(!pine::wallClock("Not/AZone", 0));
}

TEST("pine: timeframes from the bars and from Charts' names") {
  CHECK_EQ(pine::timeframeOf(btc()), QString("D"));
  Bars h;
  for (int i = 0; i < 5; ++i) h << Bar{1700000000 + i * 3600, 1, 1, 1, 1, 1};
  CHECK_EQ(pine::timeframeOf(h), QString("60"));
  for (auto& b : h) b.time = 1700000000 + (&b - h.data()) * 300;
  CHECK_EQ(pine::timeframeOf(h), QString("5"));
  for (auto& b : h) b.time = 1700000000 + (&b - h.data()) * 7 * 86400;
  CHECK_EQ(pine::timeframeOf(h), QString("W"));
  CHECK_EQ(pine::timeframeOf({}), QString());
  CHECK_EQ(pine::timeframeFromCharts("1m"), QString("1"));
  CHECK_EQ(pine::timeframeFromCharts("4h"), QString("240"));
  CHECK_EQ(pine::timeframeFromCharts("1D"), QString("D"));
  CHECK_EQ(pine::timeframeFromCharts("3D"), QString("3D"));
  CHECK_EQ(pine::timeframeFromCharts("1W"), QString("W"));
  CHECK_EQ(pine::timeframeFromCharts("1M"), QString("M"));
  CHECK_EQ(pine::timeframeFromCharts("15s"), QString("15S"));
  CHECK_EQ(pine::timeframeFromCharts("bogus"), QString());
}

namespace {
const char* const ENDLESS = "//@version=5\nindicator(\"Endless\")\nvar float s = 0.0\nfor i = 0 to 100000000\n    s += math.sin(i)\nplot(s)\n";
}

// A run is stopped by its cancel flag (a superseded request), well before its time limit.
TEST("pine: the cancel flag stops a run") {
  auto stop = std::make_shared<std::atomic<bool>>(false);
  pine::Options o;
  o.timeoutMs = 60000;
  o.cancel = stop;
  std::thread t([stop] { std::this_thread::sleep_for(std::chrono::milliseconds(300)); *stop = true; });
  QElapsedTimer clock;
  clock.start();
  const auto r = pine::run(ENDLESS, btc(), o);
  t.join();
  CHECK(!r.ok());
  CHECK(r.stopped);
  CHECK(clock.elapsed() < 5000);
  CHECK_EQ(r.error, QString("The script was stopped."));
}

// Audit G7: the threads of runAsync were never joined; at quit one still running was torn down with the app around it.
TEST("pine: at quit the runs under way are stopped and waited for") {
  QObject ctx;
  bool delivered = false;
  pine::Options o;
  o.timeoutMs = 60000;
  pine::runAsync(ENDLESS, btc(), o, &ctx, [&](const pine::Result&) { delivered = true; });
  pine::runAsync(ENDLESS, btc(), o, &ctx, [&](const pine::Result&) { delivered = true; });
  CHECK(pine::runningTracked() >= 2);
  QThread::msleep(300);
  QElapsedTimer clock;
  clock.start();
  CHECK(pine::stopRunsForQuit(10000));
  CHECK(clock.elapsed() < 5000);
  CHECK_EQ(pine::runningTracked(), 0);
  pine::resetQuitForTests();
  QCoreApplication::processEvents();
  CHECK(pine::run("//@version=5\nindicator(\"After\")\nplot(close)\n", btc()).ok());   // runs again once reset
  Q_UNUSED(delivered);
}
