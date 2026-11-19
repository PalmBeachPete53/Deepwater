// Port of quant-terminal test/backtest.test.js.
#include "Check.h"
#include "charts/core/Backtest.h"
#include <QFile>
#include <QJsonDocument>

using namespace charts;
using namespace charts::bt;

namespace {
Bars loadBtc() {
  static Bars cached = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES "/btc-1d.json"));
    f.open(QIODevice::ReadOnly);
    Bars out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out.push_back({qint64(o["time"].toDouble()), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(),
                     o["close"].toDouble(), o["volume"].toDouble()});
    }
    return out;
  }();
  return cached;
}
QJsonObject J(const char* json) { return QJsonDocument::fromJson(QByteArray(json)).object(); }
QJsonObject merge(QJsonObject a, const QJsonObject& b) {
  for (auto it = b.begin(); it != b.end(); ++it) a.insert(it.key(), it.value());
  return a;
}
Bars flat(const QVector<double>& closes) {
  Bars out;
  for (int i = 0; i < closes.size(); ++i) {
    const double c = closes[i];
    out.push_back({1700000000LL + i * 86400LL, c, c, c, c, 1});
  }
  return out;
}
Bar day(int i, double o, double h, double l, double c) { return {1700000000LL + i * 86400LL, o, h, l, c, 1}; }
const QJsonObject noCosts{{"feePct", 0}, {"slippagePct", 0}};
}  // namespace

TEST("backtest: fills on the next bar open, never the signal bar") {
  const Bars bars = loadBtc();
  const Result r = backtest(bars, exampleStrategy("EMA cross (9/21)"));
  CHECK(r.trades.size() > 5);
  for (const Trade& t : r.trades) {
    int i = -1;
    for (int k = 0; k < bars.size(); ++k)
      if (bars[k].time == t.entryTime) { i = k; break; }
    CHECK(i > 0);
    CHECK(std::fabs(t.entryPrice - bars[i].open * 1.0005) < 1e-6);   // entry = open x (1 + slippage)
  }
}

TEST("backtest: equity curve accounts for every trade") {
  const Result r = backtest(loadBtc(), merge(exampleStrategy("RSI mean reversion (30/55)"), {{"initialCapital", 10000}}));
  double sum = 0;
  for (const Trade& t : r.trades) sum += t.pnl;
  CHECK(std::fabs(10000 + sum - r.equity.last().value) < 1e-6);
  CHECK_EQ(r.stats.totalTrades, int(r.trades.size()));
}

TEST("backtest: stops cap the loss of a losing trade") {
  const Result r = backtest(loadBtc(), merge(exampleStrategy("EMA cross (9/21)"), {{"stopLossPct", 2}, {"feePct", 0}, {"slippagePct", 0}}));
  int stopped = 0;
  for (const Trade& t : r.trades)
    if (t.exitReason == "Stop") { ++stopped; CHECK(t.pnlPct >= -2 - 15); }   // gap through the stop is the only way past -2%
  CHECK(stopped > 0);
}

TEST("backtest: shorts make money when price falls") {
  Bars down;
  for (int i = 0; i < 60; ++i) down.push_back({i * 86400LL, 100.0 - i, 101.0 - i, 99.0 - i, 100.0 - i, 1});
  const Result r = backtest(down, J(R"({"indicators": {}, "entryShort": [{"left": "close", "op": "<", "right": 95}],
    "exitShort": [{"left": "close", "op": "<", "right": 50}], "feePct": 0, "slippagePct": 0})"));
  CHECK_EQ(r.trades[0].side, QStringLiteral("short"));
  CHECK(r.trades[0].pnl > 0);
}

TEST("backtest: every example strategy runs and the trades export to CSV") {
  const Bars bars = loadBtc();
  for (const auto& [name, s] : EXAMPLE_STRATEGIES()) {
    const Result r = backtest(bars, s);
    CHECK(r.equity.size() == bars.size());
    CHECK(tradesCsv(r.trades).startsWith("side,"));
  }
}

TEST("backtest: a drawn line gives its price at any time, within its reach") {
  const auto trend = linePrice(J(R"({"t1": 100, "p1": 10, "t2": 200, "p2": 20, "extend": "none"})"));
  CHECK_EQ(trend(150), 15.0);
  CHECK(std::isnan(trend(250)));   // a trend line stops at its second point
  const auto ray = linePrice(J(R"({"t1": 100, "p1": 10, "t2": 200, "p2": 20, "extend": "right"})"));
  CHECK_EQ(ray(300), 30.0);
  CHECK(std::isnan(ray(50)));      // a ray has nothing behind its start
  const auto ext = linePrice(J(R"({"t1": 100, "p1": 10, "t2": 200, "p2": 20, "extend": "both"})"));
  CHECK_EQ(ext(0), 0.0);
}

TEST("backtest: price crossing a drawn line is detected bar by bar") {
  Bars series;
  const double cs[] = {5, 6, 7, 12, 13};
  for (int i = 0; i < 5; ++i) series.push_back({i * 10LL, cs[i], cs[i], cs[i], cs[i], 1});
  const QJsonObject rule = J(R"({"left": "close", "op": "crossesAbove",
    "right": {"line": {"t1": 0, "p1": 8, "t2": 40, "p2": 12, "extend": "right"}}})");   // 8, 9, 10, 11, 12
  const auto test = conditionFn(series, {})(rule);
  QVector<bool> got;
  for (int i = 0; i < 5; ++i) got << test(i);
  CHECK((got == QVector<bool>{false, false, false, true, false}));
}

// Jeff's asks: exit on whichever rule fires first, and a time exit when nothing else has.
TEST("backtest: exitMatch any exits on whichever exit condition fires first; all waits for both") {
  const Bars b = flat({100, 100, 120, 130, 125, 90, 80, 80});
  const QJsonObject s = merge(noCosts, J(R"({"entryLong": [{"left": "close", "op": ">", "right": 110}],
    "exitLong": [{"left": "close", "op": "<", "right": 128}, {"left": "close", "op": "<", "right": 100}]})"));
  // Long from bar 3's open (signal on bar 2). Bar 4 closes at 125: only the first rule is true; bar 5 at 90: both.
  CHECK_EQ(backtest(b, merge(s, {{"exitMatch", "any"}})).trades[0].exitTime, b[5].time);
  CHECK_EQ(backtest(b, s).trades[0].exitTime, b[6].time);
  CHECK_EQ(backtest(b, merge(s, {{"exitMatch", "any"}})).trades[0].exitReason, QStringLiteral("Exit long"));
}

TEST("backtest: maxBarsInTrade closes a trade after that many bars unless an exit fires first") {
  QVector<double> closes;
  for (int i = 0; i < 30; ++i) closes << 100 + i;
  const Bars b = flat(closes);
  const QJsonObject s = merge(noCosts, J(R"({"entryLong": [{"left": "close", "op": ">", "right": 0}], "maxBarsInTrade": 5})"));
  const Result r = backtest(b, s);
  CHECK(r.trades.size() > 2);
  for (int i = 0; i + 1 < r.trades.size(); ++i) {
    CHECK_EQ(r.trades[i].bars, 5);
    CHECK_EQ(r.trades[i].exitReason, QStringLiteral("Time exit"));
  }
  const Trade early = backtest(b, merge(s, J(R"({"exitLong": [{"left": "close", "op": ">=", "right": 103}]})"))).trades[0];
  CHECK_EQ(early.exitReason, QStringLiteral("Exit long"));
  CHECK(early.bars < 5);
  CHECK(backtest(b, merge(s, {{"maxBarsInTrade", 0}})).trades.size() == 1);   // 0 turns it off
}

// @mats8038: real tick and spread simulation.
TEST("backtest: the spread fills buys at the ask and sells at the bid") {
  const Bars b = {day(0, 100, 100, 100, 100), day(1, 100, 100, 100, 100), day(2, 100, 100, 100, 100), day(3, 100, 100, 100, 100)};
  const QJsonObject s = merge(noCosts, J(R"({"spreadPct": 0.2, "entryLong": [{"left": "close", "op": ">", "right": 0}],
    "exitLong": [{"left": "close", "op": ">", "right": 0}]})"));
  const Trade t = backtest(b, s).trades[0];
  CHECK(std::fabs(t.entryPrice - 100.1) < 1e-9);
  CHECK(std::fabs(t.exitPrice - 99.9) < 1e-9);
  CHECK(backtest(b, s).stats.feesAndSlippage.contains("0.2% spread"));
}

TEST("backtest: a long's stop triggers on the bid, so the spread can set it off when the mid doesn't") {
  const Bars b = {day(0, 100, 100, 100, 100), day(1, 100, 100, 98.15, 99), day(2, 99, 99, 99, 99)};
  const QJsonObject s = merge(noCosts, J(R"({"stopLossPct": 2, "entryLong": [{"left": "close", "op": ">", "right": 0}]})"));
  CHECK_EQ(backtest(b, s).trades[0].exitReason, QStringLiteral("End of data"));
  CHECK_EQ(backtest(b, merge(s, {{"spreadPct", 0.2}})).trades[0].exitReason, QStringLiteral("Stop"));   // bid 98.05 < stop 98.098
}

TEST("backtest: fills inside a bar: cautious, TradingView path, and the bar magnifier") {
  // Bar 1 touches both the 2% stop (98) and the 2% target (102); its high is nearer the open than its low.
  const Bars b = {day(0, 100, 100, 100, 100), day(1, 100, 102.5, 97, 100), day(2, 100, 100, 100, 100)};
  const QJsonObject s = merge(noCosts, J(R"({"stopLossPct": 2, "takeProfitPct": 2, "entryLong": [{"left": "close", "op": ">", "right": 0}]})"));
  CHECK_EQ(backtest(b, s).trades[0].exitReason, QStringLiteral("Stop"));
  CHECK_EQ(backtest(b, merge(s, {{"intrabar", "ohlc"}})).trades[0].exitReason, QStringLiteral("Target"));
  // The real lower-timeframe bars show it fell first.
  const Bars subBars = {{b[1].time, 100, 100, 97, 97.5, NaN}, {b[1].time + 43200, 97.5, 102.5, 97.5, 100, NaN}};
  const Result m = backtest(b, merge(s, {{"intrabar", "magnifier"}}), subBars);
  CHECK_EQ(m.trades[0].exitReason, QStringLiteral("Stop"));
  CHECK_EQ(m.trades[0].exitPrice, 98.0);
  CHECK_EQ(m.stats.intrabarCovered, 1);
  CHECK(m.stats.fills.contains("inside 1 of 3 bars, the rest on the OHLC path"));
  // Rising first inside the bar: the target fills.
  const Bars up = {{b[1].time, 100, 102.5, 100, 101, NaN}, {b[1].time + 43200, 101, 101, 97, 100, NaN}};
  CHECK_EQ(backtest(b, merge(s, {{"intrabar", "magnifier"}}), up).trades[0].exitReason, QStringLiteral("Target"));
}

TEST("backtest: a bar that opens past the stop fills at the open on every fill setting") {
  const Bars b = {day(0, 100, 100, 100, 100), day(1, 100, 100, 99, 99), day(2, 95, 96, 94, 95), day(3, 95, 95, 95, 95)};
  const QJsonObject s = merge(noCosts, J(R"({"stopLossPct": 2, "entryLong": [{"left": "close", "op": ">", "right": 0}]})"));
  for (const char* intrabar : {"worst", "ohlc", "magnifier"}) {
    const Trade t = backtest(b, merge(s, {{"intrabar", intrabar}})).trades[0];
    CHECK_EQ(t.exitReason, QStringLiteral("Stop"));
    CHECK_EQ(t.exitPrice, 95.0);
  }
}

TEST("backtest: the magnifier picks the finest timeframe that covers the test in 20,000 bars") {
  CHECK_EQ(magnifierTf(14400, 1000 * 14400)->tf, QStringLiteral("15m"));
  CHECK_EQ(magnifierTf(86400, 1000 * 86400)->tf, QStringLiteral("4h"));
  CHECK_EQ(magnifierTf(3600, 300 * 3600)->tf, QStringLiteral("1m"));
  CHECK_EQ(magnifierTf(3600, 500 * 3600)->tf, QStringLiteral("5m"));
  CHECK(!magnifierTf(60, 1000 * 60).has_value());
}

// ---- port-specific checks
TEST("backtest: JS value semantics of strategy fields") {
  CHECK_EQ(jsNum(QJsonValue(" 12 ")), 12.0);
  CHECK_EQ(jsNum(QJsonValue("0x10")), 16.0);
  CHECK_EQ(jsNum(QJsonValue("")), 0.0);
  CHECK(std::isnan(jsNum(QJsonValue("1,5"))));
  CHECK(std::isnan(jsNum(QJsonValue("inf"))));
  CHECK_EQ(jsNum(QJsonValue("-Infinity")), -std::numeric_limits<double>::infinity());
  CHECK_EQ(jsNum(QJsonValue(QJsonValue::Null)), 0.0);
  CHECK(jsTruthy(QJsonValue("0")));
  CHECK(!jsTruthy(QJsonValue(0)));
  CHECK_EQ(jsString(QJsonValue(0.1)), QStringLiteral("0.1"));
  CHECK_EQ(jsString(QJsonValue(QJsonValue::Undefined)), QStringLiteral("undefined"));
}

TEST("backtest: errors carry the JS messages") {
  const Bars bars = loadBtc();
  auto msg = [&](const QJsonObject& rule, const QJsonObject& ind) -> QString {
    try {
      const auto f = conditionFn(bars, ind)(rule);
      for (int i = 0; i < bars.size(); ++i) f(i);
    } catch (const std::exception& e) { return QString::fromUtf8(e.what()); }
    return {};
  };
  CHECK_EQ(msg(J(R"({"left": "nope", "op": ">", "right": 1})"), {}),
           QStringLiteral("Unknown operand \"nope\". Define it under indicators or use a price field or number."));
  CHECK_EQ(msg(J(R"({"left": "close", "op": "=>", "right": 1})"), {}), QStringLiteral("Unknown operator \"=>\""));
  CHECK(msg(J(R"({"left": "x", "op": ">", "right": 1})"), J(R"({"x": {"id": "nope"}})")).startsWith("Unknown indicator"));
  bool threw = false;
  try { backtest(bars, J(R"({"entryLong": null})")); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
}
