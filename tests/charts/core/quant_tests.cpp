// Port of quant-terminal test/quant.test.js.
#include "Check.h"
#include "charts/core/Quant.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

using namespace charts;
using namespace charts::quant;

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
// const seeded = (s) => () => ((s = (s * 16807) % 2147483647) / 2147483647) - 0.5;
struct Seeded {
  double s;
  double operator()() { s = std::fmod(s * 16807, 2147483647); return s / 2147483647 - 0.5; }
};
}  // namespace

TEST("quant: normal distribution helpers match known values") {
  CHECK(std::fabs(normCdf(0) - 0.5) < 1e-7);
  CHECK(std::fabs(normCdf(1.96) - 0.9750021) < 1e-6);
  CHECK(std::fabs(normInv(0.975) - 1.959964) < 1e-6);
  for (double p : {0.001, 0.2, 0.5, 0.8, 0.999}) CHECK(std::fabs(normCdf(normInv(p)) - p) < 1e-6);
}

TEST("quant: PSR: a strong steady series is near certain, noise around zero is a coin flip") {
  QVector<double> steady;
  for (int i = 0; i < 500; ++i) steady << 0.002 + 0.001 * std::sin(i);
  CHECK(psr(steady) > 0.999);
  Seeded r{42};
  QVector<double> noise;
  for (int i = 0; i < 2000; ++i) noise << r() * 0.02;
  const double p = psr(noise);
  CHECK(p > 0.05 && p < 0.95);
}

TEST("quant: the expected best Sharpe of many noise trials grows with the number of trials") {
  CHECK(expectedMaxSharpe(100, 0.01) > expectedMaxSharpe(10, 0.01));
  CHECK_EQ(expectedMaxSharpe(1, 0.01), 0.0);
}

TEST("quant: PBO: a variant that is better everywhere is not overfit; pure noise is about a coin flip") {
  const int T = 400;
  QVector<double> good;
  for (int i = 0; i < T; ++i) good << 0.01 + 0.002 * std::sin(i);
  QVector<QVector<double>> m = {good};
  for (int k = 0; k < 5; ++k) {
    QVector<double> o;
    for (int i = 0; i < T; ++i) o << 0.001 * std::sin(i * (k + 2));
    m << o;
  }
  CHECK(pbo(m)->pbo < 0.1);
  Seeded r{7};
  QVector<QVector<double>> noise;
  for (int k = 0; k < 12; ++k) {
    QVector<double> o;
    for (int i = 0; i < T; ++i) o << r();
    noise << o;
  }
  const double p = pbo(noise)->pbo;
  CHECK(p > 0.2 && p < 0.8);
  CHECK(!pbo({good}).has_value());   // needs at least two variants
}

TEST("quant: Monte Carlo is reproducible and its percentiles are ordered") {
  const QVector<double> trades = {5, -2, 3, -1, 8, -4, 2, 1, -3, 6};
  const auto a = monteCarlo(trades), b = monteCarlo(trades);
  CHECK(a->toJson() == b->toJson());
  CHECK(a->p5 <= a->p50 && a->p50 <= a->p95);
  CHECK(a->lossProb >= 0 && a->lossProb <= 100);
}

TEST("quant: quant checks run on a real strategy and report every gate") {
  const Checks r = quantChecks(loadBtc(), bt::exampleStrategy("EMA cross (9/21)"), {300});
  CHECK_EQ(r.variants.size(), 25);   // two lengths x five settings
  QStringList keys;
  for (const Gate& g : r.gates) keys << g.key;
  keys.sort();
  CHECK_EQ(keys, (QStringList{"cost", "dsr", "pbo", "robust", "wf"}));
  CHECK(r.pbo->pbo >= 0 && r.pbo->pbo <= 1);
  CHECK_EQ(r.pbo->splits, 70);
  CHECK_EQ(r.walkForward->folds.size(), 5);
  CHECK(r.dsr <= r.psr + 1e-12);   // deflating for 25 tries can only lower the probability
}

TEST("quant: a strategy with no tunable lengths still gets the checks that apply") {
  const Checks r = quantChecks(loadBtc(), J(R"({"indicators": {}, "entryLong": [{"left": "close", "op": "crossesAbove", "right": 30000}],
    "exitLong": [{"left": "close", "op": "crossesBelow", "right": 30000}]})"), {100});
  CHECK_EQ(r.variants.size(), 1);
  bool cost = false;
  for (const Gate& g : r.gates) cost |= g.key == "cost";
  CHECK(cost);
  CHECK(!r.pbo.has_value());
}

TEST("quant: Monte Carlo scales trades by position size, and cost stress adds slippage when it is set to 0") {
  const QJsonObject strat = merge(bt::exampleStrategy("EMA cross (9/21)"), {{"slippagePct", 0}});
  const Checks full = quantChecks(loadBtc(), merge(strat, {{"positionPct", 100}}), {300});
  const Checks part = quantChecks(loadBtc(), merge(strat, {{"positionPct", 20}}), {300});
  CHECK(std::fabs(part.monteCarlo->ddP50) < std::fabs(full.monteCarlo->ddP50) * 0.4);   // 20% sizing gives a much smaller drawdown
  const Checks noCostStress = quantChecks(loadBtc(), merge(strat, {{"feePct", 0}}), {100});
  CHECK(noCostStress.costStress < noCostStress.base.netPct);   // stress still costs something with zero fees and slippage
  for (const Gate& g : noCostStress.gates)
    if (g.key == "cost") CHECK(g.value.contains("0.05% slippage"));
}

// ---- port-specific checks
TEST("quant: indicatorOrder reads the JS key order of strategy.indicators") {
  CHECK_EQ(indicatorOrder(R"({"name":"x","indicators":{"slow":{"id":"sma","params":{"length":[1,{"a":"}"}]}},"fast":{},"2":{},"10":{},"slow":1}})"),
           (QStringList{"2", "10", "slow", "fast"}));
  CHECK(indicatorOrder(R"({"entryLong":[]})").isEmpty());
  CHECK_EQ(indicatorOrder(R"({"indicators":{"a\"b":{}, "z":{}}})"), (QStringList{"a\"b", "z"}));
}
