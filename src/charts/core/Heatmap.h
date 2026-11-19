#pragma once
// Order flow: what the liquidity heatmap stores — columns of resting quantity per price row, sampled from the local
// order book at a fixed interval (100 ms on screen). Pure: no Qt, no threads.
//
//   DepthColumn   one sample: kRows rows of rowTicks ticks each, from baseTicks up; quantity = bids + asks resting
//                 in the row (base asset). Fixed size, so columns travel through a lock-free ring and are stored and
//                 written to disk as fixed records (step D3).
//   DepthSampler  book → column. The band of rows stays where it is while the mid price is in its middle half, so
//                 consecutive columns line up row for row; it moves (re-anchors) only when the price leaves it.
//                 Rows: rowTicks given, or Auto: about 2e-5 of the price, rounded to 1-2-5 × 10^k ticks
//                 (BTCUSDT ≈ $1.2 rows → 512 rows ≈ ±$300 around the price).
//                 A column taken while the book is not synced (snapshot pending, resync) is flagged Missing: the
//                 heatmap shows a hole there, never an interpolation.
#include "charts/core/OrderBook.h"
#include <cstdint>

namespace charts::flow {

struct DepthColumn {
  static constexpr int kRows = 512;
  enum Flags : uint8_t { Missing = 1, Reanchored = 2 };
  int64_t timeMs = 0;            // exchange clock, on the sampling grid
  int64_t baseTicks = 0;         // lowest tick of row 0
  int64_t bestBid = 0, bestAsk = 0;   // ticks; 0 = none
  int32_t rowTicks = 1;
  uint8_t flags = 0;
  float qty[kRows] = {};
};

class DepthSampler {
public:
  explicit DepthSampler(int64_t rowTicks = 0) : fixedRows_(rowTicks) {}   // 0 = Auto (from the first price seen)
  // Fills `out` from the book at time t. synced = false → a Missing column (no quantities).
  void sample(const OrderBook& book, bool synced, int64_t timeMs, DepthColumn& out);
  int64_t rowTicks() const { return rowTicks_; }
  int64_t baseTicks() const { return base_; }
  static int64_t autoRowTicks(int64_t midTicks);   // ≈ 2e-5 × price in ticks, 1-2-5 rounded, ≥ 1
  void reset() { base_ = 0; rowTicks_ = 0; anchored_ = false; }

private:
  int64_t fixedRows_;
  int64_t rowTicks_ = 0, base_ = 0;
  bool anchored_ = false;
};

}  // namespace charts::flow
