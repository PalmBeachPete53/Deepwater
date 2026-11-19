// Order flow, step D4 (pure parts): colour table, contrast, pixel columns.
#include "Check.h"
#include "charts/core/HeatmapRender.h"
#include <cmath>
#include <memory>

using namespace charts::flow;

namespace {
DepthRecord recAt(int64_t base, int rowTicks, std::initializer_list<std::pair<int, uint16_t>> rows, uint8_t flags = 0) {
  DepthRecord r;
  r.baseTicks = base;
  r.rowTicks = rowTicks;
  r.flags = flags;
  for (auto [i, c] : rows) r.codes[i] = c;
  return r;
}
}  // namespace

TEST("heatmap colours: empty and below-contrast codes are transparent, the top code is the palette's last colour") {
  auto lut = std::make_unique<HeatmapLut>();
  lut->build(heatmapPalette(0), 1000, 3000);
  CHECK_EQ((*lut)[0], 0u);
  CHECK_EQ((*lut)[999], 0u);
  CHECK((*lut)[1000] != 0u);
  CHECK_EQ((*lut)[3000], 0xffffffffu);   // Deep ends white, opaque
  CHECK_EQ((*lut)[65534], 0xffffffffu);  // above the contrast: clamped to the top
  CHECK_EQ((*lut)[kHatch], 0u);          // the hatching marker is never a colour of the table
  // brighter with the quantity: the alpha (and the sum of channels) never goes down along the ramp
  bool rising = true;
  uint32_t prevA = 0;
  for (int c = 1000; c <= 3000; c += 10) {
    const uint32_t a = (*lut)[uint16_t(c)] >> 24;
    rising = rising && a >= prevA;
    prevA = a;
  }
  CHECK(rising);
  // linear scale: the middle quantity sits higher on the ramp than the middle code does in log
  auto lin = std::make_unique<HeatmapLut>();
  lin->build(heatmapPalette(2), 1000, 3000, true);
  auto logm = std::make_unique<HeatmapLut>();
  logm->build(heatmapPalette(2), 1000, 3000, false);
  CHECK(((*lin)[2000] >> 24) < ((*logm)[2000] >> 24));
}

TEST("heatmap contrast: percentiles of the non-zero codes (5 % and 99.5 %)") {
  std::vector<uint32_t> h(65536, 0);
  for (int c = 100; c < 200; ++c) h[size_t(c)] = 1;   // 100 codes once each
  h[0] = 100000;                                      // empty rows never count
  const Contrast ct = contrastOf(h);
  CHECK_EQ(int(ct.low), 104);    // rank floor(99 · 0.05) = 4
  CHECK_EQ(int(ct.high), 198);   // rank floor(99 · 0.995) = 98
  const Contrast none = contrastOf(std::vector<uint32_t>(65536, 0));
  CHECK_EQ(int(none.low), 1);
  std::vector<uint32_t> one(65536, 0);
  one[700] = 50;
  const Contrast flat = contrastOf(one);
  CHECK_EQ(int(flat.low), 700);
  CHECK_EQ(int(flat.high), 700 + 512);   // a single level: one doubling of range above it
}

TEST("heatmap pixels: rows mapped to price rows, the largest quantity per pixel, out of band transparent, Missing hatched") {
  auto lut = std::make_unique<HeatmapLut>();
  lut->build(heatmapPalette(0), 1, 2000);
  // band from tick 1000, 10 ticks a row: row 5 = 1050…1059 (code 500), row 6 = 1060…1069 (code 700)
  const DepthRecord a = recAt(1000, 10, {{5, 500}, {6, 700}});
  // pixel rows (top, bottom ticks): row 6 | row 5 | rows 5–6 | far above the band
  const int64_t top[] = {1069, 1059, 1069, 900000};
  const int64_t bottom[] = {1060, 1050, 1050, 899990};
  uint32_t px[4];
  const DepthRecord* one[] = {&a};
  rasterColumn(one, 1, top, bottom, 4, *lut, px, 1, 0);
  CHECK_EQ(px[0], (*lut)[700]);
  CHECK_EQ(px[1], (*lut)[500]);
  CHECK_EQ(px[2], (*lut)[700]);   // two price rows in one pixel: the larger
  CHECK_EQ(px[3], 0u);            // outside the record's band
  // two records in one pixel column (zoomed out in time): the larger of both, even from a band anchored elsewhere
  const DepthRecord b = recAt(1050, 5, {{0, 900}});   // ticks 1050…1054
  const DepthRecord* two[] = {&a, &b};
  rasterColumn(two, 2, top, bottom, 4, *lut, px, 1, 0);
  CHECK_EQ(px[1], (*lut)[900]);
  CHECK_EQ(px[0], (*lut)[700]);
  // only Missing records: hatched, never "empty"
  const DepthRecord m = recAt(1000, 10, {}, DepthColumn::Missing);
  const DepthRecord* miss[] = {&m};
  rasterColumn(miss, 1, top, bottom, 4, *lut, px, 1, 0);
  CHECK_EQ(px[0], kMissingA);   // (x + y) % 4 == 0
  CHECK_EQ(px[1], kMissingB);
  // Missing next to data: the data wins
  const DepthRecord* mixed[] = {&m, &a};
  rasterColumn(mixed, 2, top, bottom, 4, *lut, px, 1, 0);
  CHECK_EQ(px[0], (*lut)[700]);
  // no record (before the history, or a stream gap): transparent
  rasterColumn(nullptr, 0, top, bottom, 4, *lut, px, 1, 0);
  CHECK_EQ(px[0], 0u);
  // stride: pixels written one image row apart
  uint32_t img[8] = {};
  rasterColumn(one, 1, top, bottom, 4, *lut, img + 1, 2, 1);
  CHECK_EQ(img[1], (*lut)[700]);
  CHECK_EQ(img[3], (*lut)[500]);
  CHECK_EQ(img[0], 0u);
}

TEST("heatmap codes: sum of the price rows in one pixel (a price step's total), hatching marker, repeated pixels, opacity") {
  // rows 5 and 6: 1.0 and 3.0 → sum 4.0; largest 3.0
  const DepthRecord a = recAt(1000, 10, {{5, encodeQty(1.0)}, {6, encodeQty(3.0)}});
  const DepthRecord* one[] = {&a};
  const int64_t top[] = {1069, 1069, 1059};
  const int64_t bottom[] = {1050, 1050, 1050};
  uint16_t c[3];
  rasterCodes(one, 1, top, bottom, 3, true, c, 1);
  CHECK(std::fabs(decodeQty(c[0]) - 4.0) < 4.0 * 7e-4);
  CHECK_EQ(c[1], c[0]);                                  // same ticks as the pixel above: same value
  CHECK(std::fabs(decodeQty(c[2]) - 1.0) < 7e-4);
  rasterCodes(one, 1, top, bottom, 3, false, c, 1);
  CHECK_EQ(c[0], encodeQty(3.0));
  // two records: the sum of each, then the larger of the two sums (sum over price, largest over time)
  const DepthRecord b = recAt(1000, 10, {{5, encodeQty(2.5)}, {6, encodeQty(2.5)}});
  const DepthRecord* two[] = {&a, &b};
  rasterCodes(two, 2, top, bottom, 1, true, c, 1);
  CHECK(std::fabs(decodeQty(c[0]) - 5.0) < 5.0 * 7e-4);
  // only Missing: the hatching marker, coloured as hatching whatever the table
  const DepthRecord m = recAt(1000, 10, {}, DepthColumn::Missing);
  const DepthRecord* miss[] = {&m};
  rasterCodes(miss, 1, top, bottom, 2, false, c, 1);
  CHECK_EQ(c[0], kHatch);
  auto lut = std::make_unique<HeatmapLut>();
  lut->build(heatmapPalette(0), 1, 2000, false, 1.0, 0.5);
  uint32_t px[2];
  colourise(c, 1, 2, *lut, px, 1, 0);
  CHECK_EQ(px[0], kMissingA);
  CHECK_EQ(px[1], kMissingB);
  // opacity 50 %: the top colour (white, opaque) becomes half transparent (premultiplied)
  CHECK_EQ((*lut)[2000] >> 24, 128u);
  CHECK_EQ(((*lut)[2000] >> 16) & 255, 128u);
}
