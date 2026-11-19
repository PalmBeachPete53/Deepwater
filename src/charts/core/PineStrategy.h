#pragma once
// A Pine strategy() run as a Strategy tester result. The orders, fills, sizes and commissions are PineTS's broker
// emulator (TradingView's rules: an order placed on a bar's close fills at the next bar's open, unless the script
// sets process_orders_on_close; default_qty_type, pyramiding, commission and slippage as the script declares them).
// Deepwater turns its trades into bt::Trade, rebuilds the equity curve bar by bar (cash after the closed trades, plus
// the open trades valued at each close, minus their entry commission) and computes the tester's stats on them with
// bt::stats, so a Pine strategy shows like a rule strategy: stats, equity curve, trades, marks on the chart.
#include "charts/core/Backtest.h"
#include "charts/core/Pine.h"

namespace charts::pine {

struct StrategyResult {
  QString error;           // the run failed, or the script is not a strategy()
  bt::Result result;       // trades (closed, then the open ones valued at the last close, exit reason "Open"),
                           // equity, stats
  QJsonObject config;      // strategy() arguments as PineTS read them (initial_capital, commission…)
  QJsonObject totals;      // PineTS's own totals: netprofit, equity, grossprofit, grossloss, max_drawdown…
  QStringList log;
  double ms = 0;
  bool ok() const { return error.isEmpty(); }
};

// The tester result of a run (the run must come from these bars)
StrategyResult fromRun(const Result& run, const Bars& bars);
// Runs and converts. Any thread.
StrategyResult runStrategy(const QString& source, const Bars& bars, const Options& options = {});

// Does a source declare a strategy()? (a quick look at the text, no run)
bool isStrategySource(const QString& source);

}  // namespace charts::pine
