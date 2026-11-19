// Writes hours of made-up heatmap records (1 s) ending now, for measuring the heatmap on a long history:
//   DW_PROFILE=<scratch> charts_heatmap_synth [hours=6]   → <scratch>/data/charts/depth/spot-BTCUSDT/*.dwhm
// A random-walk price around 63 000 (tick 0.01, rows of 100 ticks), resting sizes log-normal per row with a few walls
// that last minutes. Only for performance checks; refuses to run without DW_PROFILE (never a real profile).
#include "charts/core/HeatmapStore.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QDateTime>
#include <cmath>
#include <cstdio>
#include <random>

using namespace charts::flow;

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (qEnvironmentVariableIsEmpty("DW_PROFILE")) { std::fprintf(stderr, "set DW_PROFILE to a scratch folder\n"); return 2; }
  const int hours = argc > 1 ? atoi(argv[1]) : 6;
  const QString dir = paths::data("charts/depth/spot-BTCUSDT");
  HeatmapWriter w(dir, HeatmapFileInfo{"BTCUSDT", 0, Decimal{1, -2, true}, 1000});
  std::mt19937 rng(1);
  std::lognormal_distribution<double> size(-1.0, 1.2);
  const int64_t end = QDateTime::currentMSecsSinceEpoch() / 1000 * 1000 - 5000;
  const int64_t start = end - int64_t(hours) * 3600000;
  double mid = 6300000;
  int64_t base = int64_t(mid) / 100 * 100 - 256 * 100;
  std::vector<std::pair<int64_t, double>> walls;   // (tick, qty) lasting a while
  int n = 0;
  for (int64_t t = start; t < end; t += 1000) {
    mid += std::normal_distribution<double>(0, 150)(rng);   // ~$1.5 a second
    if (mid < base + 128 * 100 || mid >= base + 384 * 100) base = int64_t(mid) / 100 * 100 - 256 * 100;
    if (rng() % 30 == 0) walls.push_back({int64_t(mid) + (int64_t(rng() % 20000) - 10000), 20 + double(rng() % 80)});
    if (walls.size() > 40) walls.erase(walls.begin());
    DepthRecord r;
    r.timeMs = t;
    r.baseTicks = base;
    r.rowTicks = 100;
    r.bestBid = int64_t(mid);
    r.bestAsk = int64_t(mid) + 1;
    for (int i = 0; i < DepthRecord::kRows; ++i) r.codes[i] = encodeQty(size(rng));
    for (auto [wt, wq] : walls) {
      const int64_t row = (wt - base) / 100;
      if (row >= 0 && row < DepthRecord::kRows) r.codes[row] = encodeQty(wq);
    }
    if (!w.append(r)) { std::fprintf(stderr, "write failed: %s\n", qPrintable(w.error())); return 1; }
    ++n;
  }
  w.flush();
  std::printf("%d records (%d h) written to %s\n", n, hours, qPrintable(dir));
  return 0;
}
