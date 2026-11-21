#pragma once
// Port of quant-terminal public/lib/quant.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Quant checks for a strategy tester result: is the backtest real, or luck and overfitting?
// The methods are the published ones quant desks use:
//   - walk-forward: pick the best settings on the past only, then score them on the next stretch
//   - probability of backtest overfitting (PBO) by combinatorially symmetric cross-validation
//   - probabilistic and deflated Sharpe ratio (Bailey & Lopez de Prado), for skew, fat tails and
//     the number of versions tried
//   - cost stress: double the fees, triple the slippage (at least 0.05% a side, so zero isn't left at zero)
//   - Monte Carlo: reshuffle the trades thousands of times to see the spread of outcomes
//   - parameter robustness: do nearby settings hold up, or is it one lucky number?
// Each variant's returns come from one full backtest: signals only use past bars, so a variant's
// return on any bar is the same whether or not later bars exist. Slicing them is look-ahead free.
//
// Numbers that are null in the JS are NaN here (sharpe, psr, efficiency...); toJson() writes them as null.
#include "charts/core/Backtest.h"
#include <atomic>
#include <functional>
#include <optional>

namespace charts::quant {

// ---------- maths ----------
double normCdf(double x);   // Abramowitz & Stegun 7.1.26 on erf, accurate to about 1e-7
double normInv(double p);   // Acklam's rational approximation, accurate to about 1e-9

struct Moments { double mean = NaN, sd = NaN, skew = 0, kurt = 3; };
Moments moments(const QVector<double>& r);
double sharpe(const QVector<double>& r);                        // per period; NaN = null (< 2 values or no spread)
// Probability the true (per-period) Sharpe beats `benchmark`, given the sample's skew and kurtosis. NaN = null.
double psr(const QVector<double>& r, double benchmark = 0);
// The best Sharpe you'd expect from n tries of pure noise with this spread of Sharpes.
double expectedMaxSharpe(double n, double variance);

// Probability of backtest overfitting: split the history into `blocks`, try every half/half split,
// pick the best variant on one half and see where it ranks on the other.
struct Pbo { double pbo = NaN; int splits = 0; };
std::optional<Pbo> pbo(const QVector<QVector<double>>& matrix, int blocks = 8);

// Seeded random numbers (mulberry32), so the same strategy always shows the same Monte Carlo.
// Bit-for-bit the JS sequence.
struct Rng {
  quint32 a = 0;
  double operator()();
};
Rng rng(double seed);

struct MonteCarlo {
  int runs = 0;
  double p5 = NaN, p50 = NaN, p95 = NaN, ddP50 = NaN, ddP95 = NaN, lossProb = NaN;
  QJsonObject toJson() const;
};
std::optional<MonteCarlo> monteCarlo(const QVector<double>& tradePcts, int runs = 2000, double seed = 7);

// ---------- the checks ----------
struct Variant {
  QVector<double> values;     // the tunables' settings
  bool isBase = false;
  double sharpe = NaN;        // annualised; NaN = null
  double netPct = NaN;        // stats.netProfitPct
  int trades = 0;
};
struct Tunable { QString label; double base = 0; QVector<double> values; };
struct Fold {
  qint64 from = 0, to = 0;
  QVector<double> chosen;
  double testSharpe = 0, testReturnPct = 0;
};
struct WalkForward {
  QVector<Fold> folds;
  double oosSharpe = 0, isSharpe = 0, efficiency = NaN;   // efficiency NaN = null
  int positiveFolds = 0;
};
struct Robustness { double share = NaN; QVector<Tunable> params; };
struct Gate { QString key, label, value; bool pass = false; };   // key: wf | pbo | dsr | cost | robust

struct Checks {
  struct { double sharpe = 0, netPct = NaN; int trades = 0; } base;
  QVector<Variant> variants;
  QVector<Tunable> tunables;
  std::optional<WalkForward> walkForward;
  std::optional<Pbo> pbo;
  double psr = NaN, dsr = NaN, sr0 = 0;
  int trials = 0;
  // Deepwater (not in the JS): the per-period Sharpes the deflated Sharpe counted (one per version tried), so the trial
  // registry can add the other strategies tried on the same data (trials::withRegistry). Not in toJson().
  QVector<double> trialSharpes;
  double costStress = NaN;
  std::optional<Robustness> robustness;
  std::optional<MonteCarlo> monteCarlo;
  QVector<Gate> gates;
  int passed = 0;
  bool cancelled = false;   // stopped by quantChecks' cancel flag: the rest is empty
  QJsonObject toJson() const;   // the JS result object (what the HTTP API returns)
};

struct Options {
  int mcRuns = 2000;
  Bars subBars;                 // for intrabar 'magnifier'
  // Keys of strategy.indicators in the JS order (QJsonObject sorts keys; the JS iterates them in insertion order,
  // which picks the tunables). Empty = QJsonObject order. indicatorOrder() reads it from the strategy's JSON text.
  QStringList indicatorOrder;
};
// cancel (optional, read between the backtests): set to true by the caller when this run is superseded; the checks
// then stop before the next backtest and return a Checks with cancelled = true (nothing else filled). It must
// outlive the call.
Checks quantChecks(const Bars& bars, const QJsonObject& strategy, const Options& opt = {}, const std::atomic<bool>* cancel = nullptr);

// The same checks from runs made elsewhere (a Pine strategy, charts/core/PineQuant.h): the strategy at its settings,
// at each nearby setting (the tunables' grid, base included, in grid order), and with costs raised.
struct RunSet {
  bt::Result base;
  QVector<Tunable> tunables;              // up to two settings varied, with the values tried
  struct One { QVector<double> values; bool isBase = false; bt::Result result; };   // result unused for the base
  QVector<One> variants;
  bt::Result stressed;
  std::function<QString(double netPct)> costText;   // the cost gate's text from the stressed net profit
  double sizePct = 100;                   // share of the account in each trade (Monte Carlo of the trades' %)
};
Checks checksFrom(const Bars& bars, const RunSet& runs, int mcRuns = 2000);

// The keys of the top-level "indicators" object in a strategy's JSON text, in JS property order (integer keys
// ascending first, then the others as written). Empty if the text has none.
QStringList indicatorOrder(const QByteArray& strategyJson);

}  // namespace charts::quant
