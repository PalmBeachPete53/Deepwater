#include "pages/charts/flow/BigTradesLayer.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/Style.h"
#include "pages/charts/flow/BigTradesModel.h"
#include "pages/charts/flow/FootprintLayer.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Series.h"
#include "ui/Theme.h"
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>

namespace charts::ui::flow {

using ::charts::flow::Side;

BigTradesLayer::BigTradesLayer(ChartPane* pane, BigTradesModel* model) : pane_(pane), model_(model) {}

const QImage& BigTradesLayer::sprite(Shape shape, const QColor& col, double d, double opacity, double dpr) {
  const int dpx = std::max(2, int(std::lround(d * dpr)));
  const int op = std::clamp(int(std::lround(opacity * 32)), 0, 32);
  const quint64 key = (quint64(col.rgb()) << 32) | (quint64(int(shape)) << 30) | (quint64(std::lround(dpr * 4) & 15) << 26) |
                      (quint64(dpx & 0x3ffff) << 8) | quint64(op);
  auto it = sprites_.constFind(key);
  if (it != sprites_.cend()) return *it;
  if (sprites_.size() > 1024) sprites_.clear();
  const int side = dpx + int(std::ceil(3 * dpr));
  QImage img(side, side, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  {
    QPainter q(&img);
    q.setRenderHint(QPainter::Antialiasing, true);
    QColor fill = col, edge = col;
    fill.setAlphaF(op / 32.0 * 0.75);
    edge.setAlphaF(std::min(1.0, op / 32.0 + 0.2));
    q.setPen(QPen(edge, 1.2 * dpr));
    q.setBrush(fill);
    const QPointF c(side / 2.0, side / 2.0);
    const double r = dpx / 2.0;
    if (shape == Shape::Square) q.drawRect(QRectF(c.x() - r * 0.89, c.y() - r * 0.89, r * 1.78, r * 1.78));   // same area as the circle
    else if (shape == Shape::Diamond) {
      const double k = r * 1.25;
      q.drawPolygon(QPolygonF({QPointF(c.x(), c.y() - k), QPointF(c.x() + k, c.y()), QPointF(c.x(), c.y() + k), QPointF(c.x() - k, c.y())}));
    } else q.drawEllipse(c, r, r);
  }
  img.setDevicePixelRatio(dpr);
  return *sprites_.insert(key, img);
}

const QImage& BigTradesLayer::textSprite(const QString& text, const QColor& col, int px, double dpr) {
  const QString key = text + QLatin1Char('|') + QString::number(col.rgba(), 16) + QLatin1Char('|') + QString::number(px) +
                      QLatin1Char('|') + QString::number(dpr);
  auto it = textSprites_.constFind(key);
  if (it != textSprites_.cend()) return *it;
  if (textSprites_.size() > 2048) textSprites_.clear();
  const QFont f = theme::mono(px, QFont::DemiBold);
  const QFontMetricsF fm(f);
  const int w = int(std::ceil((fm.horizontalAdvance(text) + 2) * dpr)), h = int(std::ceil((fm.height() + 1) * dpr));
  QImage img(std::max(1, w), std::max(1, h), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  img.setDevicePixelRatio(dpr);
  {
    QPainter q(&img);
    q.setRenderHint(QPainter::TextAntialiasing, true);
    q.setFont(f);
    q.setPen(col);
    q.drawText(QRectF(0, 0, w / dpr, h / dpr), Qt::AlignCenter, text);
  }
  return *textSprites_.insert(key, img);
}

long BigTradesLayer::bubbleAt(const QPointF& pt) const {
  for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {   // the last drawn is on top
    const double dx = pt.x() - it->c.x(), dy = pt.y() - it->c.y();
    if (dx * dx + dy * dy <= it->r * it->r) return long(it->idx);
  }
  return -1;
}

void BigTradesLayer::draw(QPainter& p, const QSize& size, const ::charts::plot::PrimitiveContext& c) {
  QElapsedTimer timer;
  timer.start();
  drawn_ = 0;
  hits_.clear();
  struct Done {
    BigTradesLayer* l;
    QElapsedTimer& t;
    ~Done() { l->lastMs_ = double(t.nsecsElapsed()) / 1e6; }
  } done{this, timer};
  if (!model_ || !model_->ready() || !series_ || !c.chart || model_->bubbles().empty()) return;
  const Bars& bars = pane_->shown;
  const int n = int(bars.size());
  const auto* ts = c.chart->timeScale();
  const auto range = ts->getVisibleLogicalRange();
  if (!range || n < 1) return;
  const int from = std::max(0, clampToInt(std::floor(range->from)));
  const int to = std::min(n - 1, clampToInt(std::ceil(range->to), -1));
  if (from > to) return;
  // a bar's duration: up to the next bar, or the usual step for the last one
  const double step = n > 1 ? double(bars[n - 1].time - bars[n - 2].time) : 60.0;
  auto barEndMs = [&](int i) { return int64_t(i + 1 < n ? bars[i + 1].time : bars[i].time + qint64(step)) * 1000; };
  const auto [lo, hi] = model_->range(int64_t(bars[from].time) * 1000, barEndMs(to));
  if (lo >= hi) return;
  const auto& bub = model_->bubbles();
  order_.clear();
  for (size_t k = lo; k < hi; ++k) order_.push_back(k);
  std::sort(order_.begin(), order_.end(), [&](size_t a, size_t b) { return bub[a].a.volume > bub[b].a.volume; });

  const auto& sv = style::v();
  const QColor buy = look_.buy.isValid() ? look_.buy : sv.up, sell = look_.sell.isValid() ? look_.sell : sv.down;
  const double tick = model_->ticks().step;
  const double H = size.height(), W = size.width();
  const double dpr = c.pixelRatio > 0 ? c.pixelRatio : 1;
  for (size_t k : order_) {
    const auto& b = bub[k];
    // the bar holding the order (binary search on the shown bars), and where in it
    const qint64 sec = b.a.startMs / 1000;
    const auto it = std::upper_bound(bars.begin() + from, bars.begin() + to + 1, sec, [](qint64 t, const Bar& x) { return t < x.time; });
    const int bar = int(it - bars.begin()) - 1;
    if (bar < from) continue;
    const double startMs = double(bars[bar].time) * 1000, endMs = double(barEndMs(bar));
    const double frac = endMs > startMs ? std::clamp((double(b.a.startMs) - startMs) / (endMs - startMs), 0.0, 1.0) : 0.5;
    const auto x = ts->logicalToCoordinate(double(bar) - 0.5 + frac);
    const auto y = series_->priceToCoordinate(b.a.vwapTicks * tick);
    if (!x || !y) continue;
    const double d = ::charts::flow::bubbleDiameter(b.a.volume, b.threshold, look_.minD, look_.maxD);
    if (*x + d < 0 || *x - d > W || *y + d < 0 || *y - d > H) continue;
    const double op = ::charts::flow::bubbleOpacity(d, look_.minD, look_.maxD, look_.minOpacity);
    const QColor col = b.a.side == Side::Buy ? buy : b.a.side == Side::Sell ? sell : sv.muted;
    auto place = [&](const QImage& img) {
      const double w = img.width() / dpr, h = img.height() / dpr;
      p.drawImage(QPointF(std::round((*x - w / 2) * dpr) / dpr, std::round((*y - h / 2) * dpr) / dpr), img);
    };
    if (look_.shape == Shape::Text) {
      // the volume itself, its size following the bubble's diameter, its opacity the bubble's
      QColor tc = col;
      tc.setAlphaF(std::clamp(op + 0.25, 0.0, 1.0));
      place(textSprite(cellVolume(b.a.volume), tc, std::clamp(int(std::lround(d * 0.42)), 9, 18), dpr));
    } else {
      place(sprite(look_.shape, col, d, op, dpr));
      if (look_.showVolume && d >= 24) place(textSprite(cellVolume(b.a.volume), sv.text, std::clamp(int(std::lround(d * 0.3)), 8, 13), dpr));
    }
    hits_.push_back({QPointF(*x, *y), std::max(d / 2, 5.0), k});
    ++drawn_;
  }
}

}  // namespace charts::ui::flow
