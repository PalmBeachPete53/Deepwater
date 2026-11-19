#pragma once
// Order flow, chantier E, step E3 (pure part): from chart bars and Binance statistics to the liquidation model's inputs,
// and from its columns to pixels.
//
//   prepareLiq      the chart's CLOSED bars (the one still forming has no final open interest), the last maxBars of
//                   them, each with the open interest and accounts ratio published at or before its close (5 min
//                   statistics; older than 15 min → unknown, the model then changes nothing on that bar). The price
//                   grid covers the bars' range widened by the farthest liquidation the leverages can produce
//                   (1 / lowest leverage, plus 2 %), in `rows` equal rows.
//   liqPixelCodes   one pixel column: for each pixel row the model rows it covers (from the price scale); the value is
//                   the largest of those rows (longs + shorts, or one side), as the heatmap's 16-bit quantity code.
#include "charts/core/HeatmapStore.h"
#include "charts/core/Liquidation.h"
#include "charts/core/FuturesData.h"
#include "charts/core/Types.h"
#include <span>
#include <vector>

namespace charts::flow {

struct LiqInput {
  std::vector<double> open, high, low, close, volume, oi, ratio;
  std::vector<int64_t> openMs;   // bar start times
  int64_t barMs = 0;
  LiqGrid grid;
  int oiKnown = 0;               // bars with an open interest
  LiqSeries series() const { return {open, high, low, close, volume, oi, ratio}; }
};

LiqInput prepareLiq(std::span<const Bar> bars, int64_t barMs, int64_t nowMs, std::span<const OiPoint> oi,
                    std::span<const RatioPoint> ratios, const LiqParams& params, int maxBars, int rows, bool useRatio);

// r0[y] … r1[y]: model rows under pixel row y (r1 < r0: none). Values are base-asset quantities.
void liqPixelCodes(const float* longs, const float* shorts, const int* r0, const int* r1, int height, bool wantLongs, bool wantShorts,
                   uint16_t* out, int stride);

}  // namespace charts::flow
