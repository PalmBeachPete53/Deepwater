#pragma once
// Order flow: footprint (bid × ask volume per price level, per candle). Pure: no Qt widgets, no network.
//
//   FootprintBook   the candles of one chart, fed trade by trade (Trades.h). A trade touches only its candle and its
//                   price level; trades may come in any order (live, history going back, a filled gap): every sum is
//                   order-free and open/close are the trades with the lowest/highest id.
//   analyze()       the numbers a candle shows, for one level grouping: rows, delta, POC, diagonal imbalances,
//                   stacked imbalances. Done on demand for the candles on screen (it depends on the grouping and the
//                   thresholds, which change with zoom and settings).
//
// Definitions (user's spec 2026-11-18, edge rule decided 2026-11-02):
//   bid volume of a level  = volume of aggressive SELLS there (they hit the bid)
//   ask volume             = volume of aggressive BUYS (they lifted the ask)
//   delta                  = ask − bid (per level and for the candle)
//   POC                    = the level with the largest bid + ask volume; ties → the lowest price
//   buy imbalance at row r : ask(r) ≥ ratio × bid(r − 1)      (diagonal: the buyers at r against the sellers one row below)
//   sell imbalance at row r: bid(r) ≥ ratio × ask(r + 1)
//     · a row below the candle's lowest / above its highest does not exist: no buy imbalance on the lowest row, no sell
//       imbalance on the highest row;
//     · a row inside the candle where nothing traded counts as 0 (so ask(r) > 0 against 0 is an imbalance);
//     · the volume compared (ask(r) or bid(r)) must be > 0 and ≥ minVolume, so thin levels never light up.
//   stacked imbalances     = at least stackMin consecutive rows with an imbalance of the same side → one zone.
// Candle OHLC come from the trades themselves (user's choice 2026-11-18): the candle and its cells always agree, even
// where Binance futures klines date a few trades into the next minute (measured in step A).
// Trades with an unknown side (the very first one under the tick rule) count in the candle's volume, not in a cell.
//
// Volumes are doubles: sums of the same trades in another order can differ in the last bit (≈1e-16 relative).
#include "charts/core/Trades.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace charts::flow {

// One traded price of a candle (native ticks).
struct FootprintCell {
  int64_t ticks = 0;
  double bid = 0, ask = 0;
};

struct FootprintCandle {
  int64_t startMs = 0;                 // the chart bar this candle belongs to
  std::vector<FootprintCell> cells;    // ascending ticks, only traded prices
  int64_t open = 0, high = 0, low = 0, close = 0;   // ticks, from the trades
  int64_t openId = 0, closeId = 0;     // ids of the open and close trades
  double bidVolume = 0, askVolume = 0, unknownVolume = 0;
  int64_t trades = 0;
  int64_t approxTrades = 0;            // sides from the quote or tick rule
  double volume() const { return bidVolume + askVolume + unknownVolume; }
  double delta() const { return askVolume - bidVolume; }
};

class FootprintBook {
public:
  // bucketOf(timeMs) = start (ms) of the chart bar holding that time; e.g. fixedBuckets(60000) for 1-minute bars.
  using BucketFn = std::function<int64_t(int64_t timeMs)>;
  explicit FootprintBook(BucketFn bucketOf);
  static BucketFn fixedBuckets(int64_t barMs);

  // Adds one trade to its candle (made when needed). The same trade added twice counts twice: the caller (the pane's
  // feed) adds each tape trade once.
  void add(const Trade& t);
  void clear();

  const std::deque<FootprintCandle>& candles() const { return candles_; }
  const FootprintCandle* find(int64_t startMs) const;   // nullptr if no trade fell in that bar
  // Bars that started before the first trade we have are partial (history begins inside them): the chart says so.
  void setCoveredFrom(int64_t firstTradeMs) { coveredFrom_ = firstTradeMs; }
  bool partial(const FootprintCandle& c) const { return c.startMs < coveredFrom_; }

private:
  FootprintCandle& candleFor(int64_t startMs);
  BucketFn bucketOf_;
  std::deque<FootprintCandle> candles_;   // ascending startMs
  int64_t coveredFrom_ = INT64_MIN / 2;
};

struct FootprintParams {
  int64_t groupTicks = 1;   // a row = this many ticks (1, 2, 4, …)
  double ratio = 3.0;       // diagonal imbalance ratio
  double minVolume = 0;     // a level's compared volume below this never makes an imbalance
  int stackMin = 3;         // consecutive imbalances of one side that make a stacked zone
};

enum FootprintRowFlag : uint8_t { BuyImbalance = 1, SellImbalance = 2, Poc = 4 };

struct FootprintRow {
  int64_t ticks = 0;        // lowest tick of the row (a multiple of groupTicks)
  double bid = 0, ask = 0;
  uint8_t flags = 0;
  double volume() const { return bid + ask; }
  double delta() const { return ask - bid; }
};

struct FootprintStack {
  int fromRow = 0, toRow = 0;   // indices in FootprintView::rows, inclusive
  Side side = Side::Buy;        // Buy = stacked buy imbalances (support below price), Sell = stacked sell imbalances
};

// What a candle shows at one grouping. Rows are dense from the candle's lowest row to its highest (untraded rows
// inside have 0 × 0). Reuse one view across calls: its buffers are kept, so analyzing allocates nothing once warm.
struct FootprintView {
  std::vector<FootprintRow> rows;     // ascending price
  std::vector<FootprintStack> stacks;
  int poc = -1;                       // index in rows (-1 if no classified volume)
  double maxRowVolume = 0, maxAbsRowDelta = 0;
  double volume = 0, delta = 0;       // candle totals (unknown-side volume included in volume)
  int64_t open = 0, high = 0, low = 0, close = 0;
};

void analyze(const FootprintCandle& c, const FootprintParams& p, FootprintView& out);

}  // namespace charts::flow
