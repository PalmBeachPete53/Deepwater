#pragma once
// Port of quant-terminal public/lib/backtest.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Strategy tester. A strategy is plain JSON so the UI, alerts and your AI agent can all write one.
//
// {
//   name: 'EMA cross',
//   indicators: { fast: { id: 'ema', params: { length: 9 } }, slow: { id: 'ema', params: { length: 21 } } },
//   entryLong:  [ { left: 'fast', op: 'crossesAbove', right: 'slow' } ],   // all must be true
//   exitLong:   [ { left: 'fast', op: 'crossesBelow', right: 'slow' } ],
//   entryShort: [], exitShort: [],
//   exitMatch: 'all',        // 'any': exit on whichever exit condition fires first
//   maxBarsInTrade: 0,       // time exit: close after this many bars if nothing else has (0 = off)
//   stopLossPct: 0, takeProfitPct: 0, trailingStopPct: 0,
//   initialCapital: 10000, positionPct: 100, feePct: 0.1, slippagePct: 0.05,
//   spreadPct: 0,            // bid to ask: buys fill at the ask, sells at the bid, stops trigger on that side
//   intrabar: 'worst',       // how stops and targets fill inside a bar, see FILLS in Backtest.cpp
// }
//
// Operands: a number, a price field (open/high/low/close/volume/hl2/hlc3/ohlc4), an indicator key
// ('fast'), or an indicator output ('macd.signal'), or a drawn line { line: { t1, p1, t2, p2, extend } }.
// Signals are read on a bar's close and filled at the NEXT bar's open, so the test never trades on
// information it could not have had.
//
// Strategies and rules stay QJsonObject / QJsonValue so the UI, the alerts service and the HTTP API pass the same
// JSON as the JS. Errors are thrown as std::runtime_error with the JS message ("Unknown operand ...").
#include "charts/core/Types.h"
#include "charts/core/Indicators.h"
#include <QJsonArray>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

namespace charts::bt {

// ---- JS value semantics on JSON values (what the JS does with whatever a strategy holds)
double jsNum(const QJsonValue& v);        // Number(v): null -> 0, bool -> 0/1, "12" -> 12, "" -> 0, undefined -> NaN
bool jsTruthy(const QJsonValue& v);       // if (v): 0, NaN, "", null, undefined, false are falsy
QString jsString(const QJsonValue& v);    // `${v}`: numbers as JS prints them, null -> "null", objects "[object Object]"

inline const QStringList OPS = {">", "<", ">=", "<=", "crossesAbove", "crossesBelow", "crosses"};
inline const QStringList INTRABAR = {"worst", "ohlc", "magnifier"};

// A drawn line as an operand: { line: { t1, p1, t2, p2, extend: 'none' | 'right' | 'both' } }.
// Its price at time t, or NaN where the line doesn't reach (a trend line outside its two points,
// a ray behind its start).
std::function<double(double t)> linePrice(const QJsonObject& line);

// Series already computed for one set of bars (JS `cache`): price fields by name, indicators by "@key".
struct OperandCache {
  QHash<QString, Series> prices;
  QHash<QString, ind::Result> indicators;
};
using Operand = std::function<double(int i)>;            // value on bar i; NaN = JS NaN / undefined
using Resolver = std::function<Operand(const QJsonValue& operand)>;
// Throws for an unknown operand or indicator output when an operand is resolved.
Resolver resolveSeries(const Bars& bars, const QJsonObject& indicators = {},
                       std::shared_ptr<OperandCache> cache = nullptr);

using Condition = std::function<bool(int i)>;
// rule = { left, op, right }. Operands resolve when the rule is made; an unknown operator throws when tested
// on a bar where both sides have a value (as the JS).
std::function<Condition(const QJsonObject& rule)> conditionFn(const Bars& bars, const QJsonObject& indicators = {},
                                                              std::shared_ptr<OperandCache> cache = nullptr);

// The lower timeframe the magnifier uses: the finest that covers the whole test in at most `max` bars.
struct MagnifierTf { QString tf; int seconds = 0; };
std::optional<MagnifierTf> magnifierTf(double tfSeconds, double spanSeconds, double max = 20000);

struct Trade {
  QString side;                       // "long" | "short"
  qint64 entryTime = 0, exitTime = 0;
  double entryPrice = 0, exitPrice = 0, qty = 0, pnl = 0, pnlPct = 0;
  int bars = 0;
  QString entryReason, exitReason;    // "Long entry", "Short entry", "Exit long", "Reverse", "Stop", "Target",
                                      // "Time exit", "End of data"...
  QJsonObject toJson() const;
};
struct EquityPoint { qint64 time = 0; double value = 0; };

// Numbers rounded to 2 decimals like the JS (Number(v.toFixed(2))); NaN = JS null.
struct Stats {
  double netProfit = NaN, netProfitPct = NaN, buyHoldPct = NaN, cagrPct = NaN;
  int totalTrades = 0;
  double winRatePct = NaN, profitFactor = NaN, maxDrawdownPct = NaN, avgTradePct = NaN, avgWinPct = NaN,
         avgLossPct = NaN, largestWin = NaN, largestLoss = NaN, sharpe = NaN, sortino = NaN, exposurePct = NaN;
  QString feesAndSlippage;            // "0.1% fee + 0.05% slippage per side[ + 0.2% spread]"
  // Set by backtest() (absent from stats() alone):
  bool hasFills = false;
  QString fills;                      // how stops/targets filled inside bars, shown under the results
  int intrabarCovered = 0;            // bars the magnifier had lower-timeframe bars for
  QJsonObject toJson() const;
};

struct Result {
  QVector<Trade> trades;
  QVector<EquityPoint> equity;        // one point per bar, the last one = final cash
  Stats stats;
  QJsonObject toJson() const;         // { trades, equity, stats } exactly as the JS returns it
};

// subBars: lower-timeframe bars for intrabar 'magnifier' (empty = none, JS null).
Result backtest(const Bars& bars, const QJsonObject& strategy, const Bars& subBars = {});
// s: the strategy merged with the defaults (backtest passes its own); reads initialCapital, feePct, slippagePct,
// spreadPct.
Stats stats(const Bars& bars, const QVector<Trade>& trades, const QVector<EquityPoint>& equity, const QJsonObject& s);
QString tradesCsv(const QVector<Trade>& trades);

// The five examples of the strategy tester, in the JS order (name, strategy).
const QVector<QPair<QString, QJsonObject>>& EXAMPLE_STRATEGIES();
QJsonObject exampleStrategy(const QString& name);   // {} if unknown

}  // namespace charts::bt
