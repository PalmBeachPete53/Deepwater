#include "charts/core/HeatmapRender.h"
#include <algorithm>
#include <cmath>

namespace charts::flow {

namespace {
uint32_t mix(uint32_t a, uint32_t b, double t) {
  auto ch = [&](int s) { return uint32_t(std::lround(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t)) << s; };
  return ch(24) | ch(16) | ch(8) | ch(0);
}
// QImage::Format_ARGB32_Premultiplied: colour channels scaled by alpha
uint32_t premultiply(uint32_t argb) {
  const uint32_t a = argb >> 24;
  auto ch = [&](int s) { return ((((argb >> s) & 255) * a + 127) / 255) << s; };
  return (a << 24) | ch(16) | ch(8) | ch(0);
}
}  // namespace

const std::vector<HeatmapPaletteStop>& heatmapPalette(int which) {
  // the low end is half transparent so the chart's grid and background still read under thin liquidity
  static const std::vector<HeatmapPaletteStop> deep = {
      {0.00, 0x5010204a}, {0.25, 0xc0124a8c}, {0.50, 0xe01ea7b8}, {0.75, 0xf0f2c94c}, {0.92, 0xffef6b2b}, {1.00, 0xffffffff}};
  static const std::vector<HeatmapPaletteStop> thermal = {
      {0.00, 0x50200a3a}, {0.30, 0xc06a1b9a}, {0.60, 0xe0e0452c}, {0.85, 0xf0f9c33c}, {1.00, 0xfffffde0}};
  static const std::vector<HeatmapPaletteStop> mono = {{0.00, 0x40404040}, {1.00, 0xffffffff}};
  return which == 1 ? thermal : which == 2 ? mono : deep;
}

void HeatmapLut::build(const std::vector<HeatmapPaletteStop>& pal, uint16_t low, uint16_t high, bool linear, double gamma, double opacity) {
  opacity = std::clamp(opacity, 0.0, 1.0);
  low_ = std::max<uint16_t>(1, low);
  high_ = std::max<uint16_t>(uint16_t(low_ + 1), high);
  const double qLow = decodeQty(low_), qHigh = decodeQty(high_);
  lut_[0] = 0;
  for (uint32_t c = 1; c < 65536; ++c) {
    if (c < low_ || c == kHatch) { lut_[c] = 0; continue; }
    double t = linear ? (decodeQty(uint16_t(c)) - qLow) / (qHigh - qLow) : double(c - low_) / double(high_ - low_);
    t = std::clamp(t, 0.0, 1.0);
    if (gamma > 0 && gamma != 1) t = std::pow(t, gamma);
    size_t k = 1;
    while (k + 1 < pal.size() && pal[k].at < t) ++k;
    const auto& a = pal[k - 1];
    const auto& b = pal[k];
    const double u = b.at > a.at ? (t - a.at) / (b.at - a.at) : 1.0;
    uint32_t argb = mix(a.argb, b.argb, std::clamp(u, 0.0, 1.0));
    if (opacity < 1) argb = (uint32_t(std::lround((argb >> 24) * opacity)) << 24) | (argb & 0xffffff);
    lut_[c] = premultiply(argb);
  }
}

Contrast contrastOf(const std::vector<uint32_t>& h, double lowPct, double highPct) {
  uint64_t total = 0;
  for (size_t c = 1; c < h.size(); ++c) total += h[c];
  Contrast out;
  if (!total) return out;
  const uint64_t lo = uint64_t(std::floor(double(total - 1) * lowPct / 100)), hi = uint64_t(std::floor(double(total - 1) * highPct / 100));
  uint64_t seen = 0;
  bool haveLow = false;
  for (size_t c = 1; c < h.size(); ++c) {
    if (!h[c]) continue;
    const uint64_t next = seen + h[c];
    if (!haveLow && next > lo) { out.low = uint16_t(c); haveLow = true; }
    if (next > hi) { out.high = uint16_t(c); break; }
    seen = next;
  }
  if (out.high <= out.low) out.high = uint16_t(std::min<int>(65535, out.low + 512));   // at least one doubling of range
  return out;
}

namespace {
// decoded quantity of every code (sums need quantities, not codes); built once, 256 KB
const float* quantities() {
  static const std::vector<float> t = [] {
    std::vector<float> v(65536);
    for (uint32_t c = 0; c < 65536; ++c) v[c] = float(decodeQty(uint16_t(c)));
    return v;
  }();
  return t.data();
}
int64_t floorDiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
}  // namespace

void rasterCodes(const DepthRecord* const* recs, int nRecs, const int64_t* rowTop, const int64_t* rowBottom, int height, bool sum,
                 uint16_t* out, int stride) {
  bool anyData = false;
  for (int k = 0; k < nRecs; ++k) anyData = anyData || !(recs[k]->flags & DepthColumn::Missing);
  const float* qty = sum ? quantities() : nullptr;
  for (int y = 0; y < height; ++y) {
    uint16_t& px = out[size_t(y) * size_t(stride)];
    if (!nRecs) { px = kNoData; continue; }
    if (!anyData) { px = kHatch; continue; }
    // a pixel covering the same ticks as the one above (inside one price step): same value
    if (y > 0 && rowTop[y] == rowTop[y - 1] && rowBottom[y] == rowBottom[y - 1]) { px = out[size_t(y - 1) * size_t(stride)]; continue; }
    uint16_t best = 0;
    double bestSum = 0;
    bool covered = false;
    for (int k = 0; k < nRecs; ++k) {
      const DepthRecord& r = *recs[k];
      if (r.flags & DepthColumn::Missing) continue;
      const int64_t rt = r.rowTicks > 0 ? r.rowTicks : 1;
      int64_t r0 = floorDiv(rowBottom[y] - r.baseTicks, rt), r1 = floorDiv(rowTop[y] - r.baseTicks, rt);
      if (r1 < 0 || r0 >= DepthRecord::kRows) continue;
      covered = true;
      r0 = std::max<int64_t>(r0, 0);
      r1 = std::min<int64_t>(r1, DepthRecord::kRows - 1);
      if (sum) {
        double t = 0;
        for (int64_t r2 = r0; r2 <= r1; ++r2) t += qty[r.codes[r2]];
        bestSum = std::max(bestSum, t);
      } else {
        for (int64_t r2 = r0; r2 <= r1; ++r2) best = std::max(best, r.codes[r2]);
      }
    }
    if (sum) best = std::min<uint16_t>(encodeQty(bestSum), kHatch - 1);
    px = covered ? best : kNoData;
  }
}

void colourise(const uint16_t* codes, int codeStride, int height, const HeatmapLut& lut, uint32_t* out, int stride, int x) {
  for (int y = 0; y < height; ++y) {
    const uint16_t c = codes[size_t(y) * size_t(codeStride)];
    out[size_t(y) * size_t(stride)] = c == kHatch ? (((x + y) & 3) == 0 ? kMissingA : kMissingB) : lut[c];
  }
}

void rasterColumn(const DepthRecord* const* recs, int nRecs, const int64_t* rowTop, const int64_t* rowBottom, int height,
                  const HeatmapLut& lut, uint32_t* out, int stride, int x, bool sum) {
  std::vector<uint16_t> codes(size_t(std::max(height, 0)));
  rasterCodes(recs, nRecs, rowTop, rowBottom, height, sum, codes.data(), 1);
  colourise(codes.data(), 1, height, lut, out, stride, x);
}

}  // namespace charts::flow
