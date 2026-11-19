// Pine strategy() scripts as Strategy tester results (src/charts/core/PineStrategy.cpp), and a cross-check: the same
// EMA cross written in Pine (PineTS's broker emulator) and as Charts rules (bt::backtest) on real BTCUSDT days must
// trade on the same bars at the same prices (both fill a signal at the next bar's open).
#include "Check.h"
#include "charts/core/PineStrategy.h"
#include "charts/core/PineIndicators.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <cmath>

using namespace charts;

namespace {
QString root() { return QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("../../.."); }
const Bars& days() {
  static const Bars b = [] {
    Bars out;
    QFile f(root() + "/tests/charts/core/fixtures/binance-btcusdt-1d.csv");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
    while (!f.atEnd()) {
      const QByteArray line = f.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#') || line.startsWith("time")) continue;
      const QList<QByteArray> c = line.split(',');
      out << Bar{c[0].toLongLong(), c[1].toDouble(), c[2].toDouble(), c[3].toDouble(), c[4].toDouble(), c[5].toDouble()};
    }
    return out;
  }();
  return b;
}

const char* const EMA_CROSS = R"(//@version=5
strategy("EMA cross", overlay=true, initial_capital=10000, default_qty_type=strategy.percent_of_equity, default_qty_value=100)
fast = ta.ema(close, 9)
slow = ta.ema(close, 21)
if ta.crossover(fast, slow)
    strategy.entry("Long", strategy.long)
if ta.crossunder(fast, slow)
    strategy.close("Long", comment="Cross down")
)";

QJsonObject rulesStrategy() {
  return QJsonObject{
      {"indicators", QJsonObject{{"fast", QJsonObject{{"id", "ema"}, {"params", QJsonObject{{"length", 9}}}}},
                                 {"slow", QJsonObject{{"id", "ema"}, {"params", QJsonObject{{"length", 21}}}}}}},
      {"entryLong", QJsonArray{QJsonObject{{"left", "fast"}, {"op", "crossesAbove"}, {"right", "slow"}}}},
      {"exitLong", QJsonArray{QJsonObject{{"left", "fast"}, {"op", "crossesBelow"}, {"right", "slow"}}}},
      {"initialCapital", 10000}, {"positionPct", 100}, {"feePct", 0}, {"slippagePct", 0}, {"spreadPct", 0},
  };
}
}  // namespace

TEST("pine strategy: an EMA cross in Pine trades on the same bars at the same prices as the same rules in Charts") {
  const Bars& bars = days();
  CHECK(bars.size() > 3000);
  pine::Options o;
  o.symbol = "BINANCE:BTCUSDT";
  const auto ps = pine::runStrategy(EMA_CROSS, bars, o);
  if (!ps.ok()) { check::fail(__FILE__, __LINE__, ps.error); return; }
  const bt::Result rules = bt::backtest(bars, rulesStrategy());
  // Charts closes a trade still open at the end ("End of data"); Pine keeps it open (valued at the last close)
  QVector<bt::Trade> a = ps.result.trades, b = rules.trades;
  CHECK(a.size() > 50);
  CHECK_EQ(a.size(), b.size());
  int same = 0;
  for (int i = 0; i < std::min(a.size(), b.size()); ++i) {
    const bool ok = a[i].side == b[i].side && a[i].entryTime == b[i].entryTime && a[i].exitTime == b[i].exitTime &&
                    a[i].entryPrice == b[i].entryPrice && a[i].exitPrice == b[i].exitPrice;
    if (ok) ++same;
    else if (same == i) {
      check::fail(__FILE__, __LINE__, QString("trade %1: Pine %2 %3→%4 %5→%6, rules %7 %8→%9 %10→%11").arg(i)
                                          .arg(a[i].side).arg(a[i].entryTime).arg(a[i].exitTime).arg(a[i].entryPrice).arg(a[i].exitPrice)
                                          .arg(b[i].side).arg(b[i].entryTime).arg(b[i].exitTime).arg(b[i].entryPrice).arg(b[i].exitPrice));
    }
  }
  CHECK_EQ(same, int(a.size()));
  std::printf("    %d trades, same bars and prices: %d; net profit Pine %.2f%%, rules %.2f%%\n", int(a.size()), same,
              ps.result.stats.netProfitPct, rules.stats.netProfitPct);
}

TEST("pine strategy: the equity curve ends at PineTS's own equity; trades, stats and costs are filled in") {
  const Bars& bars = days();
  const auto ps = pine::runStrategy(EMA_CROSS, bars);
  if (!ps.ok()) { check::fail(__FILE__, __LINE__, ps.error); return; }
  const bt::Result& r = ps.result;
  CHECK_EQ(r.equity.size(), bars.size());
  CHECK_EQ(r.equity.first().time, bars.first().time);
  const double pineEquity = ps.totals.value("equity").toDouble() + ps.totals.value("openprofit").toDouble();
  CHECK_NEAR(r.equity.last().value, pineEquity, 1e-6 * std::max(1.0, std::fabs(pineEquity)));
  CHECK_EQ(r.stats.totalTrades, int(r.trades.size()));
  CHECK(std::isfinite(r.stats.netProfitPct));
  CHECK(std::isfinite(r.stats.maxDrawdownPct));
  CHECK(r.stats.feesAndSlippage.contains("no commission"));
  CHECK(r.stats.hasFills);
  for (const auto& t : r.trades) {
    CHECK(t.side == "long");
    CHECK(t.qty > 0);
    CHECK(t.bars >= 0);
    if (t.exitReason != "Open") CHECK_EQ(t.exitReason, QString("Cross down"));
  }
  CHECK_EQ(ps.config.value("initial_capital").toDouble(), 10000.0);
}

TEST("pine strategy: a trade still open at the end is kept as Open, valued at the last close; shorts are shorts") {
  const Bars& bars = days();
  const auto ps = pine::runStrategy(R"(//@version=5
strategy("Short once", initial_capital=1000, commission_type=strategy.commission.percent, commission_value=0.1)
if bar_index == 100
    strategy.entry("S", strategy.short, qty=0.01)
)", bars);
  if (!ps.ok()) { check::fail(__FILE__, __LINE__, ps.error); return; }
  CHECK_EQ(ps.result.trades.size(), 1);
  if (ps.result.trades.isEmpty()) return;
  const bt::Trade& t = ps.result.trades.first();
  CHECK_EQ(t.side, QString("short"));
  CHECK_EQ(t.exitReason, QString("Open"));
  CHECK_EQ(t.exitTime, bars.last().time);
  CHECK_NEAR(t.qty, 0.01, 1e-12);
  // short: gains when the price falls, minus the entry commission (0.1 % of the entry value)
  CHECK_NEAR(t.pnl, 0.01 * (t.entryPrice - bars.last().close) - 0.001 * 0.01 * t.entryPrice, 1e-6);
  CHECK(ps.result.stats.feesAndSlippage.contains("0.1% commission"));
}

TEST("pine strategy: an indicator() is not a strategy; the text check tells them apart") {
  const auto ps = pine::runStrategy("//@version=5\nindicator(\"I\")\nplot(close)\n", days());
  CHECK(!ps.ok());
  CHECK(pine::isStrategySource(EMA_CROSS));
  CHECK(!pine::isStrategySource("//@version=5\nindicator(\"I\")\nplot(close)\n"));
}

TEST("pine strategy: added to a chart as an indicator it takes its strategy() title and overlay") {
  QTemporaryDir dir;
  const QString path = dir.filePath("ema-cross.pine");
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) f.write(QByteArray(EMA_CROSS) + "plot(ta.ema(close, 9), \"Fast\")\n");
  f.close();
  const auto l = pine::loadFile(path);
  if (!l.ok()) { check::fail(__FILE__, __LINE__, l.error); return; }
  const ind::DefPtr d = ind::byId(l.id);
  CHECK(d && d->name == "EMA cross" && d->overlay);
  ind::unregisterIndicator(l.id);
  pine::Cache::instance().forget(l.id);
}

TEST("pine strategy: a Pine indicator in a rule strategy of the tester gives the same trades as the built-in it copies") {
  QTemporaryDir dir;
  const QString path = dir.filePath("my-ema.pine");
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) f.write("//@version=5\nindicator(\"My EMA\", overlay=true)\nlength = input.int(9, \"length\")\nplot(ta.ema(close, length), \"value\")\n");
  f.close();
  const auto l = pine::loadFile(path);
  if (!l.ok()) { check::fail(__FILE__, __LINE__, l.error); return; }
  // the tester runs on the GUI thread, outside any chart: the Pine values come at once, not "pending"
  QJsonObject withPine = rulesStrategy();
  withPine["indicators"] = QJsonObject{{"fast", QJsonObject{{"id", l.id}, {"params", QJsonObject{{"length", 9}}}}},
                                       {"slow", QJsonObject{{"id", l.id}, {"params", QJsonObject{{"length", 21}}}}}};
  const bt::Result a = bt::backtest(days(), withPine), b = bt::backtest(days(), rulesStrategy());
  CHECK(a.trades.size() > 50);
  CHECK_EQ(a.trades.size(), b.trades.size());
  bool same = a.trades.size() == b.trades.size();
  for (int i = 0; same && i < a.trades.size(); ++i)
    same = a.trades[i].entryTime == b.trades[i].entryTime && a.trades[i].exitTime == b.trades[i].exitTime;
  CHECK(same);
  CHECK_NEAR(a.stats.netProfitPct, b.stats.netProfitPct, 0.01);
  ind::unregisterIndicator(l.id);
  pine::Cache::instance().forget(l.id);
}
