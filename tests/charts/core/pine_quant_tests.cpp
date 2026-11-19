// Quant checks of a Pine strategy (src/charts/core/PineQuant.cpp) against the rule strategy's (Quant.cpp) for the same
// EMA cross on real BTCUSDT days: the same 25 settings tried, the same trades for each, and the same verdicts on
// walk-forward, overfitting, deflated Sharpe and nearby settings. (Cost stress differs on purpose: a Pine strategy's
// costs are its own commission and slippage in ticks.)
#include "Check.h"
#include "charts/core/PineQuant.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

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

const char* const PINE = R"(//@version=5
strategy("EMA cross", overlay=true, initial_capital=10000, default_qty_type=strategy.percent_of_equity, default_qty_value=100)
fastLen = input.int(9, "Fast")
slowLen = input.int(21, "Slow")
fast = ta.ema(close, fastLen)
slow = ta.ema(close, slowLen)
if ta.crossover(fast, slow)
    strategy.entry("Long", strategy.long)
if ta.crossunder(fast, slow)
    strategy.close("Long")
)";

QJsonObject rules() {
  return QJsonObject{
      {"indicators", QJsonObject{{"fast", QJsonObject{{"id", "ema"}, {"params", QJsonObject{{"length", 9}}}}},
                                 {"slow", QJsonObject{{"id", "ema"}, {"params", QJsonObject{{"length", 21}}}}}}},
      {"entryLong", QJsonArray{QJsonObject{{"left", "fast"}, {"op", "crossesAbove"}, {"right", "slow"}}}},
      {"exitLong", QJsonArray{QJsonObject{{"left", "fast"}, {"op", "crossesBelow"}, {"right", "slow"}}}},
      {"initialCapital", 10000}, {"positionPct", 100}, {"feePct", 0}, {"slippagePct", 0}, {"spreadPct", 0},
  };
}
const quant::Gate* gate(const quant::Checks& c, const QString& key) {
  for (const auto& g : c.gates) if (g.key == key) return &g;
  return nullptr;
}
}  // namespace

TEST("pine quant: an EMA cross in Pine gets the same settings, trades and verdicts as the same rules") {
  pine::Options o;
  o.symbol = "BINANCE:BTCUSDT";
  const auto p = pine::quantChecks(PINE, days(), o, 500);
  if (!p.ok()) { check::fail(__FILE__, __LINE__, p.error); return; }
  quant::Options qo;
  qo.mcRuns = 500;
  qo.indicatorOrder = {"fast", "slow"};
  const quant::Checks r = quant::quantChecks(days(), rules(), qo);
  const quant::Checks& c = p.checks;
  std::printf("    %d PineTS runs in %.1f s\n", p.runs, p.seconds);
  // the same settings tried: Fast 5 7 9 11 13 × Slow 13 17 21 25 29
  CHECK_EQ(c.tunables.size(), 2);
  CHECK_EQ(c.variants.size(), 25);
  CHECK_EQ(c.variants.size(), r.variants.size());
  if (c.tunables.size() == 2) {
    CHECK_EQ(c.tunables[0].values, (QVector<double>{5, 7, 9, 11, 13}));
    CHECK_EQ(c.tunables[1].values, (QVector<double>{13, 17, 21, 25, 29}));
  }
  // each setting: the same number of trades, about the same Sharpe (Pine sizes at the signal's close, Charts at the fill)
  int sameTrades = 0;
  double worstSharpe = 0;
  for (int i = 0; i < std::min(c.variants.size(), r.variants.size()); ++i) {
    CHECK_EQ(c.variants[i].values, r.variants[i].values);
    // Charts closes the last open trade at the end ("End of data"), Pine keeps it open: counted alike here
    if (std::abs(c.variants[i].trades - r.variants[i].trades) <= 0) ++sameTrades;
    worstSharpe = std::max(worstSharpe, std::fabs(c.variants[i].sharpe - r.variants[i].sharpe));
  }
  CHECK_EQ(sameTrades, int(c.variants.size()));
  CHECK(worstSharpe < 0.02);
  // the verdicts
  CHECK(c.walkForward.has_value() && r.walkForward.has_value());
  if (c.walkForward && r.walkForward) {
    for (int k = 0; k < std::min(c.walkForward->folds.size(), r.walkForward->folds.size()); ++k)
      CHECK_EQ(c.walkForward->folds[k].chosen, r.walkForward->folds[k].chosen);
    CHECK_NEAR(c.walkForward->oosSharpe, r.walkForward->oosSharpe, 0.02);
  }
  CHECK(c.pbo.has_value() && r.pbo.has_value());
  if (c.pbo && r.pbo) CHECK_NEAR(c.pbo->pbo, r.pbo->pbo, 0.05);
  CHECK_NEAR(c.dsr, r.dsr, 0.02);
  CHECK(c.robustness.has_value() && r.robustness.has_value());
  if (c.robustness && r.robustness) CHECK_NEAR(c.robustness->share, r.robustness->share, 5);
  for (const char* k : {"wf", "pbo", "dsr", "robust"}) {
    const auto* a = gate(c, k);
    const auto* b = gate(r, k);
    CHECK(a && b);
    if (a && b) CHECK_EQ(a->pass, b->pass);
  }
  // cost stress: a run with the costs raised, worse than the base
  const auto* cost = gate(c, "cost");
  CHECK(cost && cost->value.contains("ticks of slippage"));
  CHECK(c.costStress < c.base.netPct);
  CHECK(c.monteCarlo.has_value());
  std::printf("    WF out-of-sample Sharpe Pine %.2f / rules %.2f · PBO %.2f / %.2f · DSR %.3f / %.3f · gates passed %d / %d\n",
              c.walkForward ? c.walkForward->oosSharpe : NAN, r.walkForward ? r.walkForward->oosSharpe : NAN,
              c.pbo ? c.pbo->pbo : NAN, r.pbo ? r.pbo->pbo : NAN, c.dsr, r.dsr, c.passed, r.passed);
}

TEST("pine quant: a strategy without whole-number inputs still gets the checks that apply; an indicator gets none") {
  const auto p = pine::quantChecks(R"(//@version=5
strategy("Fixed", initial_capital=10000, default_qty_type=strategy.percent_of_equity, default_qty_value=50, commission_type=strategy.commission.percent, commission_value=0.1)
if ta.crossover(ta.sma(close, 10), ta.sma(close, 30))
    strategy.entry("L", strategy.long)
if ta.crossunder(ta.sma(close, 10), ta.sma(close, 30))
    strategy.close("L")
)", days(), {}, 200);
  if (!p.ok()) { check::fail(__FILE__, __LINE__, p.error); return; }
  CHECK(p.checks.tunables.isEmpty());
  CHECK_EQ(p.checks.variants.size(), 1);
  CHECK(gate(p.checks, "cost") && gate(p.checks, "dsr"));
  CHECK(!gate(p.checks, "wf") && !gate(p.checks, "robust"));
  CHECK(gate(p.checks, "cost")->value.contains("0.2% commission"));
  CHECK(!pine::quantChecks("//@version=5\nindicator(\"I\")\nplot(close)\n", days()).ok());
}

// A superseded quant-checks run can stop: the flag is read between the backtests (rules) and by every PineTS run's
// interrupt handler (Pine). The panel sets it when a newer request replaces the run.
TEST("quant checks: a cancel flag stops the rule and the Pine checks") {
  std::atomic<bool> stop{true};
  const quant::Checks r = quant::quantChecks(days(), rules(), {}, &stop);
  CHECK(r.cancelled);
  CHECK(r.variants.isEmpty());
  stop = false;
  quant::Options qo;
  qo.mcRuns = 50;
  CHECK(!quant::quantChecks(days(), rules(), qo, &stop).cancelled);   // not set: the checks run to their end
  // Pine: set while the 25 runs are under way
  std::atomic<bool> later{false};
  std::thread t([&later] { std::this_thread::sleep_for(std::chrono::milliseconds(150)); later = true; });
  QElapsedTimer clock;
  clock.start();
  const auto p = pine::quantChecks(PINE, days(), {}, 50, &later);
  t.join();
  CHECK(p.cancelled);
  CHECK(!p.ok());
  std::printf("    (Pine checks stopped after %lld ms)\n", static_cast<long long>(clock.elapsed()));
}
