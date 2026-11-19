#pragma once
// Order flow: turning stored heatmap columns into pixels (step D4). Pure: QtCore types only, no QImage, no painter —
// the layer (pages/charts/flow/HeatmapLayer) wraps the buffer into an image.
//
//   HeatmapLut     one ARGB colour per 16-bit quantity code (65 536 entries): codes are already logarithmic, so a
//                  log scale is a straight ramp between two codes (the contrast), a linear scale ramps on the decoded
//                  quantities. Below the low code: transparent (empty book shows the chart's background). Palettes are
//                  colour stops in increasing brightness.
//   contrastOf     the low / high codes of the visible data: chosen percentiles of the non-zero codes (5 % and 99.5 %
//                  by default), so a few huge orders don't wash everything else out.
//   rasterCodes    one pixel column → one quantity code per pixel: the records of its time slice (several when zoomed
//                  out — the largest is kept, never an average that would hide a wall) and the tick range of every pixel
//                  row (several price rows in one pixel or one price step: their largest, or their sum). kNoData where no
//                  record covers the pixel, kHatch where the slice is only Missing records (no data, not "empty").
//   colourise      codes → colours through the table (hatching for kHatch). Two steps so the contrast comes from what is
//                  drawn (sums included) and a new palette or contrast doesn't read the records again.
//   rasterColumn   both, for one column.
#include "charts/core/HeatmapStore.h"
#include <array>
#include <cstdint>
#include <vector>

namespace charts::flow {

struct HeatmapPaletteStop { double at; uint32_t argb; };   // at in [0, 1], ascending
const std::vector<HeatmapPaletteStop>& heatmapPalette(int which);   // 0 Deep (default), 1 Thermal, 2 Mono

class HeatmapLut {
public:
  // low/high: codes at the bottom / top of the ramp; linear: ramp on quantities instead of codes; gamma > 0 bends it.
  // opacity in [0, 1] scales every colour.
  void build(const std::vector<HeatmapPaletteStop>& palette, uint16_t low, uint16_t high, bool linear = false, double gamma = 1.0,
             double opacity = 1.0);
  uint32_t operator[](uint16_t code) const { return lut_[code]; }
  uint16_t low() const { return low_; }
  uint16_t high() const { return high_; }

private:
  std::array<uint32_t, 65536> lut_{};
  uint16_t low_ = 0, high_ = 0;
};

// Contrast from a histogram of codes (index = code; [0] ignored).
struct Contrast { uint16_t low = 1, high = 65535; };
Contrast contrastOf(const std::vector<uint32_t>& histogram, double lowPct = 5, double highPct = 99.5);

// hatching of Missing slices: mid grey at 25 % and 9 % (premultiplied: every channel ≤ alpha)
constexpr uint32_t kMissingA = 0x40202020, kMissingB = 0x180c0c0c;

constexpr uint16_t kNoData = 0, kHatch = 65535;   // codes of real quantities are 1 … 65534

// out: one value per pixel row, top to bottom, `stride` apart. rowTop[y] / rowBottom[y]: the tick range [top ≥ bottom]
// covered by pixel row y (from the price scale, widened to whole price steps). sum: the rows within one pixel add up
// (a price step's total resting quantity) instead of the largest. x: for the hatching phase.
void rasterCodes(const DepthRecord* const* recs, int nRecs, const int64_t* rowTop, const int64_t* rowBottom, int height, bool sum,
                 uint16_t* out, int stride);
void colourise(const uint16_t* codes, int codeStride, int height, const HeatmapLut& lut, uint32_t* out, int stride, int x);
void rasterColumn(const DepthRecord* const* recs, int nRecs, const int64_t* rowTop, const int64_t* rowBottom, int height,
                  const HeatmapLut& lut, uint32_t* out, int stride, int x, bool sum = false);

}  // namespace charts::flow
