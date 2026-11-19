#include "pages/charts/flow/LiqMapLayer.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/Style.h"
#include "pages/charts/flow/LiqMapModel.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Series.h"
#include <QElapsedTimer>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace charts::ui::flow {

using namespace ::charts::flow;

LiqMapLayer::LiqMapLayer(ChartPane* pane, LiqMapModel* model)
    : pane_(pane), model_(model), lut_(std::make_unique<HeatmapLut>()), hist_(65536) {}

void LiqMapLayer::setLook(const Look& l) {
  if (l == look_) return;
  look_ = l;
  ++lookVersion_;
  requestRepaint();
}

void LiqMapLayer::draw(QPainter& p, const QSize& size, const ::charts::plot::PrimitiveContext& c) {
  QElapsedTimer timer;
  timer.start();
  stats_.full = false;
  struct Done {
    Stats& s;
    QElapsedTimer& t;
    ~Done() { s.lastMs = double(t.nsecsElapsed()) / 1e6; }
  } done{stats_, timer};
  const auto res = model_ ? model_->result() : nullptr;
  if (!res || !series_ || !c.chart || res->model.bars() == 0 || res->model.grid().rows <= 0) return;
  const Bars& bars = pane_->shown;
  const int n = int(bars.size());
  const int W = size.width(), H = size.height();
  if (n < 1 || W < 2 || H < 2) return;
  const auto* ts = c.chart->timeScale();
  const auto x0 = ts->logicalToCoordinate(0), x1 = ts->logicalToCoordinate(1);
  if (!x0 || !x1 || *x1 == *x0) return;
  const double spacing = *x1 - *x0;

  // time at every pixel column edge (the bars' own times; beyond them, the usual step)
  const double step = n > 1 ? double(bars[n - 1].time - bars[n - 2].time) : 60.0;
  auto barStartMs = [&](int i) -> double {
    if (i < 0) return (double(bars[0].time) + i * step) * 1000;
    if (i >= n) return (double(bars[n - 1].time) + (i - n + 1) * step) * 1000;
    return double(bars[i].time) * 1000;
  };
  xTime_.resize(size_t(W) + 1);
  for (int x = 0; x <= W; ++x) {
    // clamped before the integer conversions (out of range is undefined behaviour)
    const double l = std::clamp((x - *x0) / spacing, -1e9, 1e9);
    const int i = int(std::floor(l + 0.5));
    const double frac = l - (i - 0.5);
    xTime_[size_t(x)] = clampToInt64(barStartMs(i) + frac * (barStartMs(i + 1) - barStartMs(i)));
  }
  // model rows under every pixel row
  const LiqGrid& g = res->model.grid();
  r0_.resize(size_t(H));
  r1_.resize(size_t(H));
  std::optional<double> prev = series_->coordinateToPrice(0);
  if (!prev) return;
  for (int y = 0; y < H; ++y) {
    const auto next = series_->coordinateToPrice(y + 1);
    if (!next) return;
    const double top = std::max(*prev, *next), bottom = std::min(*prev, *next);
    int a = clampToInt(std::floor((bottom - g.priceLow) / g.rowHeight)), b = clampToInt(std::ceil((top - g.priceLow) / g.rowHeight) - 1);
    b = std::max(a, b);   // a pixel thinner than a row: that row
    if (b < 0 || a >= g.rows) { a = 1; b = 0; }
    else { a = std::max(a, 0); b = std::min(b, g.rows - 1); }
    r0_[size_t(y)] = a;
    r1_[size_t(y)] = b;
    prev = next;
  }

  QByteArray key;
  auto add = [&](auto v) { key.append(reinterpret_cast<const char*>(&v), sizeof v); };
  add(W); add(H); add(xTime_.front()); add(xTime_.back()); add(r0_.front()); add(r1_.back()); add(r0_.back()); add(res->generation);
  add(lookVersion_); add(clampToInt64(series_->priceToCoordinate(g.priceLow).value_or(0) * 1000));
  if (key != viewKey_ || img_.size() != QSize(W, H)) {
    viewKey_ = key;
    stats_.full = true;
    if (img_.size() != QSize(W, H)) img_ = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    codes_.assign(size_t(W) * size_t(H), kNoData);
    const auto& open = res->openMs;
    const int cols = res->model.bars();
    int lastCol = -2, columns = 0;
    for (int x = 0; x < W; ++x) {
      // the bar under the pixel; after the last closed bar, the levels resting now
      const int64_t t = (xTime_[size_t(x)] + xTime_[size_t(x) + 1]) / 2;
      const int col = std::min(int(std::upper_bound(open.begin(), open.end(), t) - open.begin()) - 1, cols - 1);
      if (col < 0) continue;
      if (col == lastCol) {
        for (int y = 0; y < H; ++y) codes_[size_t(y) * size_t(W) + size_t(x)] = codes_[size_t(y) * size_t(W) + size_t(x - 1)];
        continue;
      }
      lastCol = col;
      ++columns;
      liqPixelCodes(res->model.longColumn(col), res->model.shortColumn(col), r0_.data(), r1_.data(), H, look_.longs, look_.shorts,
                    codes_.data() + x, W);
    }
    stats_.columns = columns;
    std::fill(hist_.begin(), hist_.end(), 0u);
    for (uint16_t code : codes_)
      if (code != kNoData) ++hist_[code];
    const Contrast ct = contrastOf(hist_, look_.lowPct, look_.highPct);
    lut_->build(heatmapPalette(look_.palette), ct.low, ct.high, false, look_.gamma, look_.opacity);
    stats_.low = lut_->low();
    stats_.high = lut_->high();
    auto* bits = reinterpret_cast<uint32_t*>(img_.bits());
    const int stride = int(img_.bytesPerLine() / 4);
    for (int x = 0; x < W; ++x) colourise(codes_.data() + x, W, H, *lut_, bits + x, stride, x);
  }
  p.setRenderHint(QPainter::SmoothPixmapTransform, false);
  p.drawImage(QRectF(0, 0, W, H), img_);

  // real liquidations (Binance's sample): dots sized by quantity among those on screen
  stats_.dots = 0;
  if (!look_.real || !model_->stats()) return;
  const auto& liq = model_->stats()->liquidations();
  int64_t tTo = xTime_.back();
  // in bar replay, nothing after the replayed bar's end (the next loaded bar's start; on Renko etc. shown has other bars)
  if (const auto& r = pane_->replay; r && r->index >= 0 && r->index < pane_->bars.size()) {
    const Bars& all = pane_->bars;
    const int i = r->index;
    const qint64 end = i + 1 < all.size() ? all[i + 1].time : i > 0 ? 2 * all[i].time - all[i - 1].time : all[i].time + 60;
    tTo = std::min<int64_t>(tTo, int64_t(end) * 1000);
  }
  const int64_t tFrom = xTime_.front();
  auto lo = std::lower_bound(liq.begin(), liq.end(), tFrom, [](const LiqEvent& e, int64_t t) { return e.timeMs < t; });
  auto hi = std::lower_bound(lo, liq.end(), tTo, [](const LiqEvent& e, int64_t t) { return e.timeMs < t; });
  if (lo == hi) return;
  double maxQ = 0;
  for (auto it = lo; it != hi; ++it) maxQ = std::max(maxQ, it->qty);
  const auto& sv = style::v();
  p.setRenderHint(QPainter::Antialiasing, true);
  QColor outline(0, 0, 0, 160);
  for (auto it = lo; it != hi; ++it) {
    if ((look_.longs || !it->longLiquidated) && (look_.shorts || it->longLiquidated)) {
      const auto y = series_->priceToCoordinate(it->price);
      if (!y || *y < -10 || *y > H + 10) continue;
      // x: the pixel whose time slice holds the event
      const int x = int(std::upper_bound(xTime_.begin(), xTime_.end(), it->timeMs) - xTime_.begin()) - 1;
      const double r = 2 + 5 * std::sqrt(maxQ > 0 ? it->qty / maxQ : 0);
      QColor col = it->longLiquidated ? sv.down : sv.up;
      col.setAlphaF(0.85);
      p.setPen(QPen(outline, 1));
      p.setBrush(col);
      p.drawEllipse(QPointF(x + 0.5, *y), r, r);
      ++stats_.dots;
    }
  }
}

}  // namespace charts::ui::flow
