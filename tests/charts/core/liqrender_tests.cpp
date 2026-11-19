// Order flow, step E3 (pure part): chart bars → the liquidation model's inputs; model columns → pixel codes.
#include "Check.h"
#include "charts/core/HeatmapRender.h"
#include "charts/core/LiqRender.h"
#include <cmath>

using namespace charts;
using namespace charts::flow;

namespace {
constexpr int64_t kMin = 60 * 1000, kFive = 5 * kMin;
std::vector<Bar> fiveMinBars(int n, int64_t firstOpenSec) {
  std::vector<Bar> v;
  for (int i = 0; i < n; ++i) v.push_back({firstOpenSec + i * 300, 100, double(101 + i % 3), double(99 - i % 2), 100.5, 10});
  return v;
}
}  // namespace

TEST("liquidation inputs: closed bars only, the last maxBars, statistics at each bar's close") {
  const int64_t t0 = 1795000200;   // a 5 min boundary (s)
  const auto bars = fiveMinBars(10, t0);
  // OI published at the end of each period: at bar i's close (t0 + 300 (i + 1)), value 1000 + i; one missing
  std::vector<OiPoint> oi;
  for (int i = 0; i < 10; ++i)
    if (i != 4) oi.push_back({(t0 + 300 * (i + 1)) * 1000, 1000.0 + i, 0});
  const std::vector<RatioPoint> ratio = {{(t0 + 300) * 1000, 2.0, 0.66}};
  // now: inside bar 9 (it is still forming)
  const int64_t now = (t0 + 300 * 9 + 100) * 1000;
  const LiqInput in = prepareLiq(bars, kFive, now, oi, ratio, LiqParams{}, 1000, 400, true);
  CHECK_EQ(int(in.close.size()), 9);                 // bar 9 left out
  CHECK_EQ(in.openMs.front(), t0 * 1000);
  CHECK_EQ(in.oi[0], 1000.0);
  CHECK_EQ(in.oi[4], 1003.0);                        // its own point is missing: the previous one (5 min old)
  CHECK_EQ(in.oi[8], 1008.0);
  CHECK_EQ(in.oiKnown, 9);
  CHECK_EQ(in.ratio[0], 2.0);
  CHECK_EQ(in.ratio[3], 2.0);                        // 15 min old: still used
  CHECK(std::isnan(in.ratio[4]));                    // 20 min old: unknown
  // the last maxBars only
  const LiqInput last3 = prepareLiq(bars, kFive, now, oi, ratio, LiqParams{}, 3, 400, false);
  CHECK_EQ(int(last3.close.size()), 3);
  CHECK_EQ(last3.openMs.front(), (t0 + 300 * 6) * 1000);
  CHECK(std::isnan(last3.ratio[0]));                 // ratio switched off
  // grid: bars from 98 to 103, lowest leverage 5× → ±22 %
  CHECK(std::fabs(in.grid.priceLow - 98 * 0.78) < 1e-9);
  CHECK(std::fabs(in.grid.priceLow + in.grid.rowHeight * in.grid.rows - 103 * 1.22) < 1e-9);
  CHECK_EQ(in.grid.rows, 400);
  LiqParams high;
  high.leverages = {{25, 1}, {100, 1}};
  const LiqInput tight = prepareLiq(bars, kFive, now, oi, ratio, high, 1000, 400, true);
  CHECK(std::fabs(tight.grid.priceLow - 98 * 0.94) < 1e-9);   // 1/25 + 2 %
  // no closed bar: empty input, no grid
  const LiqInput none = prepareLiq(std::span<const Bar>(bars.data(), 1), kFive, t0 * 1000 + 1000, oi, ratio, LiqParams{}, 1000, 400, true);
  CHECK(none.close.empty());
  CHECK_EQ(none.grid.rows, 0);
}

TEST("liquidation pixels: largest row under each pixel, sides chosen, empty rows transparent") {
  const float longs[] = {0, 2, 0, 8, 0};
  const float shorts[] = {1, 0, 0, 0, 4};
  // pixel rows: rows 3–4 | row 2 | rows 0–1 | none | none again (same as above: copied)
  const int r0[] = {3, 2, 0, 1, 1};
  const int r1[] = {4, 2, 1, 0, 0};
  uint16_t c[5];
  liqPixelCodes(longs, shorts, r0, r1, 5, true, true, c, 1);
  CHECK_EQ(c[0], encodeQty(8));     // max(8 + 0, 0 + 4)
  CHECK_EQ(c[1], kNoData);          // nothing there
  CHECK_EQ(c[2], encodeQty(2));     // max(0 + 1, 2 + 0)
  CHECK_EQ(c[3], kNoData);          // no model row
  CHECK_EQ(c[4], kNoData);
  liqPixelCodes(longs, shorts, r0, r1, 5, false, true, c, 1);
  CHECK_EQ(c[0], encodeQty(4));     // shorts only
  CHECK_EQ(c[2], encodeQty(1));
  // a long and a short at the same price add up
  const float l2[] = {3}, s2[] = {5};
  const int a[] = {0}, b[] = {0};
  liqPixelCodes(l2, s2, a, b, 1, true, true, c, 1);
  CHECK_EQ(c[0], encodeQty(8));
}
