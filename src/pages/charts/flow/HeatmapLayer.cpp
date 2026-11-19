#include "pages/charts/flow/HeatmapLayer.h"
#include "charts/data/DepthStream.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/Style.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Series.h"
#include <QElapsedTimer>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace charts::ui::flow {

using ::charts::flow::ColumnRing;
using ::charts::flow::DepthColumn;
using ::charts::flow::DepthRecord;

HeatmapLayer::HeatmapLayer(ChartPane* pane, std::shared_ptr<data::DepthStream> stream)
    : pane_(pane), stream_(std::move(stream)), lut_(std::make_unique<::charts::flow::HeatmapLut>()), hist_(65536) {}

HeatmapLayer::~HeatmapLayer() = default;

void HeatmapLayer::setLook(const Look& l) {
  if (l == look_) return;
  look_ = l;
  ++lookVersion_;   // part of the view key: the next frame is a full rebuild
  requestRepaint();
}

void HeatmapLayer::draw(QPainter& p, const QSize& size, const ::charts::plot::PrimitiveContext& c) {
  QElapsedTimer timer;
  timer.start();
  stats_.full = false;
  stats_.columnsRedrawn = 0;
  struct Done {
    Stats& s;
    QElapsedTimer& t;
    ~Done() { s.lastMs = double(t.nsecsElapsed()) / 1e6; }
  } done{stats_, timer};
  if (!stream_ || !stream_->ready() || !series_ || !c.chart) return;
  const auto& live = stream_->live();
  const auto& hist = stream_->history();
  if (live.empty() && hist.empty()) return;
  const Bars& bars = pane_->shown;
  const int n = int(bars.size());
  if (n < 1) return;
  const int W = size.width(), H = size.height();
  if (W < 2 || H < 2) return;
  const auto* ts = c.chart->timeScale();
  const auto x0 = ts->logicalToCoordinate(0), x1 = ts->logicalToCoordinate(1);
  if (!x0 || !x1 || *x1 == *x0) return;
  const double spacing = *x1 - *x0;
  const auto& tick = stream_->ticks();

  // ---- time of every pixel column edge (from the bars' own times; beyond them, the usual step)
  const double step = n > 1 ? double(bars[n - 1].time - bars[n - 2].time) : 60.0;
  auto barStartMs = [&](int i) -> double {
    if (i < 0) return (double(bars[0].time) + i * step) * 1000;
    if (i >= n) return (double(bars[n - 1].time) + (i - n + 1) * step) * 1000;
    return double(bars[i].time) * 1000;
  };
  xTime_.resize(size_t(W) + 1);
  for (int x = 0; x <= W; ++x) {
    // logical; bar i spans [i − 0.5, i + 0.5). Clamped before the int conversion (out of range is undefined behaviour)
    const double l = std::clamp((x - *x0) / spacing, -1e9, 1e9);
    const int i = int(std::floor(l + 0.5));
    const double frac = l - (i - 0.5);
    xTime_[size_t(x)] = int64_t(std::clamp(barStartMs(i) + frac * (barStartMs(i + 1) - barStartMs(i)), -9e18, 9e18));
  }
  // ---- tick range of every pixel row
  rowTop_.resize(size_t(H));
  rowBottom_.resize(size_t(H));
  std::optional<double> prev = series_->coordinateToPrice(0);
  if (!prev) return;
  // a price step: every pixel widened to the whole steps it touches (a pixel thinner than a step: the step holding its
  // middle, so the step is a clean block of pixels)
  // capped at 2^40 ticks: a larger step would overflow the int64 row maths below (any value that large is one cell anyway)
  const int64_t stepTicks =
      look_.step > 0 ? std::llround(std::clamp(look_.step / tick.step, 1.0, double(int64_t(1) << 40))) : 0;
  auto floorDiv = [](int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
  for (int y = 0; y < H; ++y) {
    const auto next = series_->coordinateToPrice(y + 1);
    if (!next) return;
    // ticks of the pixel edges, clamped first (a zoomed-out or log scale can give prices far beyond int64 ticks)
    auto toTicks = [&](double v) { return int64_t(std::floor(std::clamp(v / tick.step, -4e18, 4e18) + 0.5)); };
    const int64_t a = toTicks(*prev), b = toTicks(*next);
    int64_t top = std::max(a, b), bottom = std::min(a, b) + (a != b ? 1 : 0);   // a pixel covers (its bottom edge, its top edge]
    if (bottom > top) bottom = top;
    if (stepTicks > 1) {
      if (top - bottom + 1 < stepTicks) {
        bottom = floorDiv(bottom + (top - bottom) / 2, stepTicks) * stepTicks;
        top = bottom + stepTicks - 1;
      } else {
        bottom = floorDiv(bottom, stepTicks) * stepTicks;
        top = floorDiv(top, stepTicks) * stepTicks + stepTicks - 1;
      }
    }
    rowTop_[size_t(y)] = top;
    rowBottom_[size_t(y)] = bottom;
    prev = next;
  }
  // in bar replay, nothing after the replayed bar
  // the end of the replayed bar, from the loaded bars themselves: `shown` holds bricks on Renko, Kagi, P&F, line
  // break or range charts, whose count says nothing about the replayed bar
  int64_t cutoff = INT64_MAX;
  if (pane_->replay) {
    const Bars& all = pane_->bars;
    const int ri = pane_->replay->index;
    if (ri >= 0 && ri + 1 < all.size()) cutoff = int64_t(all[ri + 1].time) * 1000;
    else if (ri >= 0 && ri < all.size()) cutoff = int64_t(double(all[ri].time) * 1000 + step * 1000);
    else cutoff = int64_t(barStartMs(n));
  }
  const int64_t newest = std::min(cutoff - 1, live.empty() ? hist.back().timeMs : live.back().timeMs);

  // ---- the records of a time slice [t0, t1): those starting in it, or the one covering it (zoomed in)
  const int64_t liveStart = live.empty() ? INT64_MAX : live.at(live.begin()).timeMs;
  auto slice = [&](int64_t t0, int64_t t1) {
    recs_.clear();
    const bool useLive = t1 > liveStart;
    const ColumnRing<DepthRecord>& ring = useLive ? live : hist;
    if (ring.empty()) return;
    const int64_t span = useLive ? 250 : 2500;   // how long a record stands for (2.5 sampling periods)
    int64_t a = ring.lowerBoundTime(t0), b = ring.lowerBoundTime(std::min(t1, cutoff));
    if (a == b) {
      if (a > ring.begin() && t0 - ring.at(a - 1).timeMs < span && ring.at(a - 1).timeMs < cutoff) recs_.push_back(&ring.at(a - 1));
      return;
    }
    // several records in one pixel column (zoomed out): every one counts. Runs of records on the same band are merged
    // into one (largest code per row, the newest best bid/ask); the band only moves when the price leaves the middle
    // half, so a column is one or two merged records. Total work is bounded by the ring (≤ 21 600 · 512 row maxima).
    size_t used = 0;
    for (int64_t i = a; i < b;) {
      const DepthRecord& first = ring.at(i);
      int64_t j = i + 1;
      auto same = [&](const DepthRecord& r) {
        return r.baseTicks == first.baseTicks && r.rowTicks == first.rowTicks && (r.flags & DepthColumn::Missing) == (first.flags & DepthColumn::Missing);
      };
      while (j < b && same(ring.at(j))) ++j;
      if (j == i + 1) {
        recs_.push_back(&first);
      } else {
        if (used == merged_.size()) merged_.emplace_back();
        DepthRecord& m = merged_[used++];
        m = first;
        for (int64_t k = i + 1; k < j; ++k) {
          const DepthRecord& r = ring.at(k);
          for (int row = 0; row < DepthRecord::kRows; ++row) m.codes[row] = std::max(m.codes[row], r.codes[row]);
        }
        const DepthRecord& last = ring.at(j - 1);
        m.timeMs = last.timeMs;
        m.bestBid = last.bestBid;
        m.bestAsk = last.bestAsk;
        recs_.push_back(&m);
      }
      i = j;
    }
  };

  // ---- full rebuild or only the newest columns
  QByteArray key;
  key.reserve(96);
  auto add = [&](auto v) { key.append(reinterpret_cast<const char*>(&v), sizeof(v)); };
  add(W); add(H); add(xTime_.front()); add(xTime_.back()); add(rowTop_.front()); add(rowBottom_.back()); add(cutoff); add(lookVersion_);
  const bool full = key != viewKey_ || img_.size() != QSize(W, H);
  if (full) {
    if (img_.size() != QSize(W, H)) img_ = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    codes_.resize(size_t(W) * size_t(H));
    viewKey_ = key;
    stats_.full = true;
  }
  int from = 0;
  if (!full) {
    // only the columns that can hold data newer than what is drawn
    from = W;
    while (from > 0 && xTime_[size_t(from)] > drawnUpTo_ - 1000) --from;
  }
  auto* bits = reinterpret_cast<uint32_t*>(img_.bits());
  const int stride = int(img_.bytesPerLine() / 4);
  int records = 0;
  if (full) {
    bidY_.assign(size_t(W), std::nan(""));
    askY_.assign(size_t(W), std::nan(""));
  }
  // best bid / ask colours, premultiplied, drawn over the cells (source-over)
  const auto& sv = style::v();
  auto premul = [](QColor c, double alpha) {
    const uint32_t a = uint32_t(std::lround(alpha * 255));
    auto ch = [&](int v) { return uint32_t((v * a + 127) / 255); };
    return (a << 24) | (ch(c.red()) << 16) | (ch(c.green()) << 8) | ch(c.blue());
  };
  const uint32_t bidColour = premul(sv.up, 1.0), askColour = premul(sv.down, 1.0), halo = premul(QColor(0, 0, 0), 0.7);
  auto over = [](uint32_t src, uint32_t dst) {
    const uint32_t inv = 255 - (src >> 24);
    auto ch = [&](int s) { return (((src >> s) & 255) + (((dst >> s) & 255) * inv + 127) / 255) << s; };
    return ch(24) | ch(16) | ch(8) | ch(0);
  };
  // the line in column x: a vertical run from the previous column's level to this one (so steps stay joined)
  auto lineIn = [&](int x, double y, double prevY, uint32_t colour) {
    if (std::isnan(y)) return;
    // off-screen prices: clamped just outside the image first (no int overflow), the run then clipped to it
    auto toRow = [&](double v) { return int(std::floor(std::clamp(v, -2.0, double(H) + 1))); };
    int y0 = toRow(y), y1 = y0;
    if (!std::isnan(prevY)) {
      const int py = toRow(prevY);
      if (py < y0) y0 = py + 1;
      else if (py > y1) y1 = py - 1;
    }
    y0 = std::max(y0, 0);
    y1 = std::min(y1, H - 1);
    if (y0 > y1) return;   // entirely above or below the pane
    // a dark pixel above and below, so the line reads over bright cells as well as dark ones
    if (y0 > 0) { uint32_t& px = bits[size_t(y0 - 1) * size_t(stride) + size_t(x)]; px = over(halo, px); }
    if (y1 < H - 1) { uint32_t& px = bits[size_t(y1 + 1) * size_t(stride) + size_t(x)]; px = over(halo, px); }
    for (int y2 = y0; y2 <= y1; ++y2) {
      uint32_t& px = bits[size_t(y2) * size_t(stride) + size_t(x)];
      px = over(colour, px);
    }
  };
  const double tRaster = double(timer.nsecsElapsed());
  // 1. quantity codes of the columns to redraw
  int to = from;
  for (int x = from; x < W; ++x) {
    const int64_t t0 = xTime_[size_t(x)];
    uint16_t* col = codes_.data() + x;
    if (t0 > newest) {
      // the future (right of the newest data): nothing. Cleared once on a rebuild; afterwards those columns are still
      // clear, so an incremental frame stops here
      if (!full) break;
      for (int y = 0; y < H; ++y) col[size_t(y) * size_t(W)] = ::charts::flow::kNoData;
      bidY_[size_t(x)] = askY_[size_t(x)] = std::nan("");
      to = x + 1;
      continue;
    }
    slice(t0, xTime_[size_t(x) + 1]);
    records += int(recs_.size());
    ::charts::flow::rasterCodes(recs_.data(), int(recs_.size()), rowTop_.data(), rowBottom_.data(), H, look_.sum, col, W);
    // the best bid / ask of the column's newest record
    const DepthRecord* r = recs_.empty() ? nullptr : recs_.back();
    double yb = std::nan(""), ya = std::nan("");
    if (r && !(r->flags & DepthColumn::Missing) && r->bestBid && r->bestAsk) {
      if (const auto v = series_->priceToCoordinate(tick.priceOf(r->bestBid))) yb = *v;
      if (const auto v = series_->priceToCoordinate(tick.priceOf(r->bestAsk))) ya = *v;
    }
    bidY_[size_t(x)] = yb;
    askY_[size_t(x)] = ya;
    to = x + 1;
  }
  // 2. on a rebuild, the contrast from what is drawn (a quantity given in the settings wins over its percentile)
  if (full) {
    std::fill(hist_.begin(), hist_.end(), 0u);
    for (uint16_t c : codes_)
      if (c != ::charts::flow::kNoData && c != ::charts::flow::kHatch) ++hist_[c];
    auto ct = ::charts::flow::contrastOf(hist_, look_.lowPct, look_.highPct);
    if (look_.minQty > 0) ct.low = ::charts::flow::encodeQty(look_.minQty);
    if (look_.maxQty > 0) ct.high = ::charts::flow::encodeQty(look_.maxQty);
    lut_->build(::charts::flow::heatmapPalette(look_.palette), ct.low, ct.high, look_.linear, look_.gamma, look_.opacity);
  }
  // 3. colours, then the best bid / ask over them
  for (int x = from; x < to; ++x) {
    ::charts::flow::colourise(codes_.data() + x, W, H, *lut_, bits + x, stride, x);
    if (!look_.lines) continue;
    const double pb = x > 0 ? bidY_[size_t(x) - 1] : std::nan(""), pa = x > 0 ? askY_[size_t(x) - 1] : std::nan("");
    lineIn(x, bidY_[size_t(x)], pb, bidColour);
    lineIn(x, askY_[size_t(x)], pa, askColour);
  }
  stats_.rasterMs = (double(timer.nsecsElapsed()) - tRaster) / 1e6;
  drawnUpTo_ = newest;
  stats_.columnsRedrawn = to - from;
  stats_.records = records;
  stats_.low = lut_->low();
  stats_.high = lut_->high();

  const double tImage = double(timer.nsecsElapsed());
  p.setRenderHint(QPainter::SmoothPixmapTransform, false);
  p.drawImage(QRectF(0, 0, W, H), img_);
  stats_.imageMs = (double(timer.nsecsElapsed()) - tImage) / 1e6;

}

}  // namespace charts::ui::flow
