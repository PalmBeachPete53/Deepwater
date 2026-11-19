#pragma once
#include "analytics/Indicators.h"
#include <QString>
#include <QDate>

// Liquidity pools, sweeps and session extremes, computed by Deepwater from OHLC bars. Pure functions.
//
// RULES (shown in the pages' method footnotes; keep both in sync):
//  1. Swings: 3-bar fractals (order 1): a bar whose high is strictly above the highs of the bar before and after it
//     is a swing high; swing lows mirror it.
//  2. Pools: swing highs are sorted by price and grouped greedily from the lowest: a group takes the next swing while
//     it stays within `tolPct` percent of the group's first price. A group of at least 2 swings is a buy-side pool
//     (BSL, "equal highs"); its level is the group's highest price, its size (×N) the number of swings. Swing lows
//     give sell-side pools (SSL) the same way, level = lowest price.
//  3. State: from the bar after the pool's most recent swing, the first bar that trades beyond the level decides:
//     it closes back on the other side → SWEPT at that bar; it closes beyond → BROKEN. No such bar → UNTESTED.
//  4. Touches: number of bars of the loaded history whose low..high range contains the level.
//  5. Reversal confirmation of a sweep: move back from the level over the next `reversalBars` bars, in ATR(14) of
//     the sweep bar: ≥ 1.5 Strong, ≥ 0.75 Moderate, otherwise Weak; Pending while fewer bars exist and the move is
//     still < 1.5.
//  6. Stop-run likelihood: distance from the last price to the nearer UNTESTED pool, in ATR(14) of the series:
//     ≤ 2 High, ≤ 6 Medium, otherwise Low. A disclosed rule, never a backtested probability.
//  7. Sessions (bar open time in [open, close)), local business hours with daylight saving: Sydney 07–16, Tokyo
//     09–18, London 08–17, New York 08–17 (in UTC: Sydney 20/21–05/06, Tokyo 00–09, London 07/08–16/17, New York
//     12/13–21/22). Tokyo, London and New York: the session of the UTC day of the latest bar; Sydney: the session
//     that opened most recently by the latest bar (it opens the evening before in UTC). Previous day = the last earlier UTC
//     day with bars (its high/low are PDH/PDL); previous week = the ISO week (Mon–Sun, UTC) before the latest bar's.
namespace analytics {

enum class PoolSide { BuySide, SellSide };      // BSL: equal highs (stops above), SSL: equal lows (stops below)
enum class PoolState { Untested, Swept, Broken };
enum class Reversal { None, Pending, Weak, Moderate, Strong };

struct Pool {
  PoolSide side = PoolSide::BuySide;
  double level = NAN;
  int members = 0;
  int firstMember = -1, lastMember = -1;   // bar indices
  int touches = 0;
  PoolState state = PoolState::Untested;
  int eventIndex = -1;                     // bar that swept or broke the pool
  Reversal reversal = Reversal::None;
  double reversalAtr = NAN;
};

struct PoolParams {
  double tolPct = 0.06;    // clustering tolerance in percent of price
  int fractalOrder = 1;    // 1 = 3-bar fractal
  int minMembers = 2;
  int reversalBars = 6;
  int atrPeriod = 14;
};

QVector<Pool> findPools(const QVector<Bar>& b, const PoolParams& p = {});
QString reversalName(Reversal r);

enum class StopRunLevel { None, Low, Medium, High };
struct StopRun {
  StopRunLevel level = StopRunLevel::None;
  PoolSide closer = PoolSide::SellSide;
  double distPct = NAN;     // signed (level - last) / last * 100 of the nearer untested pool
  double distAtr = NAN;
  int poolIndex = -1;
};
StopRun stopRun(const QVector<Pool>& pools, double last, double atrAbs);
QString stopRunName(StopRunLevel l);

struct SessionRow {
  QString name, sub;
  double high = NAN, low = NAN;
  bool summary = false;     // previous day / previous week rows (gold names)
};
struct SessionTable {
  QVector<SessionRow> rows;   // Sydney, Tokyo, London, New York, previous day, previous week
  QDate day;                  // UTC day of the latest bar
  double pdh = NAN, pdl = NAN;
  QDate prevDay;
};
SessionTable sessionExtremes(const QVector<Bar>& hourly, const QDate& todayUtc);

// The heatmap's liquidity read on daily bars: same pool engine, tolerance scaled to the instrument.
enum class LiqRead { None, PoolAbove, PoolBelow, NearPool, RecentlySwept };
struct LiquidityRead {
  LiqRead state = LiqRead::None;
  double distPct = NAN;      // signed distance from the last close to the level
  double closeness = 0;      // 0..100 (radar)
  int untested = 0, total = 0;
};
// Rules: pools with tolPct = max(0.06, 0.25 × ATR14%); a pool swept within the last `recentBars` bars →
// RECENTLY SWEPT (distance to that level); else the nearest UNTESTED pool: |distance| ≤ 1 × ATR14% → NEAR POOL,
// otherwise POOL ABOVE / POOL BELOW; no untested pool → None.
LiquidityRead liquidityRead(const QVector<Bar>& daily, int recentBars = 3);
}
