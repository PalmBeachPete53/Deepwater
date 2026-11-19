#include "charts/core/LiqRender.h"
#include "charts/core/HeatmapRender.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace charts::flow {

LiqInput prepareLiq(std::span<const Bar> bars, int64_t barMs, int64_t nowMs, std::span<const OiPoint> oi,
                    std::span<const RatioPoint> ratios, const LiqParams& params, int maxBars, int rows, bool useRatio) {
  LiqInput in;
  in.barMs = barMs;
  // closed bars only: their end has passed
  size_t end = bars.size();
  while (end > 0 && bars[end - 1].time * 1000 + barMs > nowMs) --end;
  const size_t cap = size_t(std::max(maxBars, 1));   // a negative maxBars wrapped around as size_t
  const size_t begin = end > cap ? end - cap : 0;
  const size_t n = end - begin;
  std::vector<int64_t> closeMs(n);
  in.open.resize(n);
  in.high.resize(n);
  in.low.resize(n);
  in.close.resize(n);
  in.volume.resize(n);
  in.openMs.resize(n);
  double lo = std::numeric_limits<double>::infinity(), hi = -lo;
  for (size_t i = 0; i < n; ++i) {
    const Bar& b = bars[begin + i];
    in.openMs[i] = b.time * 1000;
    closeMs[i] = b.time * 1000 + barMs;
    in.open[i] = b.open;
    in.high[i] = b.high;
    in.low[i] = b.low;
    in.close[i] = b.close;
    in.volume[i] = b.volume;
    if (b.low > 0) lo = std::min(lo, b.low);
    hi = std::max(hi, b.high);
  }
  constexpr int64_t kMaxAge = 15 * 60 * 1000;
  in.oi = alignOi(closeMs, oi, kMaxAge);
  in.ratio = useRatio ? alignRatio(closeMs, ratios, kMaxAge) : std::vector<double>(n, std::numeric_limits<double>::quiet_NaN());
  for (double x : in.oi) in.oiKnown += std::isfinite(x);
  // the grid: the bars' range plus the farthest level the lowest leverage can make
  double minLev = 1e9;
  for (const auto& l : params.leverages)
    if (l.weight > 0) minLev = std::min(minLev, l.leverage);
  const double reach = (minLev < 1e9 ? 1 / std::max(minLev, 1.0) : 0.2) + 0.02;
  if (n && std::isfinite(lo) && hi > 0) {
    const double gLo = lo * (1 - reach), gHi = hi * (1 + reach);
    in.grid.rows = std::max(rows, 10);
    in.grid.priceLow = gLo;
    in.grid.rowHeight = (gHi - gLo) / in.grid.rows;
  }
  return in;
}

void liqPixelCodes(const float* longs, const float* shorts, const int* r0, const int* r1, int height, bool wantLongs, bool wantShorts,
                   uint16_t* out, int stride) {
  for (int y = 0; y < height; ++y) {
    uint16_t& px = out[size_t(y) * size_t(stride)];
    if (y > 0 && r0[y] == r0[y - 1] && r1[y] == r1[y - 1]) { px = out[size_t(y - 1) * size_t(stride)]; continue; }
    float best = 0;
    for (int r = r0[y]; r <= r1[y]; ++r) {
      const float v = (wantLongs ? longs[r] : 0.f) + (wantShorts ? shorts[r] : 0.f);
      best = std::max(best, v);
    }
    px = best > 0 ? std::min<uint16_t>(encodeQty(best), kHatch - 1) : kNoData;
  }
}

}  // namespace charts::flow
