#pragma once
// Quant checks of a Pine strategy() (the gold box of the Strategy tester): the same checks and gates as a rule strategy
// (charts/core/Quant.h: walk-forward, PBO, deflated Sharpe, cost stress, nearby settings, Monte Carlo of the trades),
// on versions of the script that PineTS runs:
//   - nearby settings: its first two whole-number inputs (input.int with a default of 2 or more), each ×0.6 … ×1.4
//     rounded (at least 2), as the rule strategies' indicator lengths; every combination is one run (25 at most);
//   - cost stress: commission doubled; slippage tripled and at least 0.05 % of the median close a side, in PineTS's
//     ticks (0.01 when the data gives no tick size: the gate says the % really applied);
//   - every run on its own thread with its own QuickJS runtime (pine::run), all at once on the machine's cores.
#include "charts/core/Pine.h"
#include "charts/core/Quant.h"

namespace charts::pine {

struct QuantResult {
  QString error;           // the base run failed, or the script is not a strategy()
  quant::Checks checks;
  int runs = 0;            // PineTS runs made
  double seconds = 0;
  bool cancelled = false;  // stopped by the cancel flag: checks is empty
  bool ok() const { return error.isEmpty(); }
};

// options: the symbol, timeframe and settings of the base run (inputs given there are the base values). Any thread;
// blocks until every run is done.
// cancel (optional, read while it runs): set to true by the caller when this run is superseded (a newer request, the
// panel closed); the PineTS runs under way stop at their next interrupt check (as with Options::cancel, which is used
// when cancel is null), no new run starts, and the result has cancelled = true and an error. It must outlive the call.
QuantResult quantChecks(const QString& source, const Bars& bars, const Options& options = {}, int mcRuns = 2000,
                        const std::atomic<bool>* cancel = nullptr);

}  // namespace charts::pine
