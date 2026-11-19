// Painting of charts::plot::Chart. Two layers: a cached base image (panes, series, axes) and the crosshair drawn
// over it on every paint, so moving the mouse only blits the cache and draws a few lines.
// Bars, columns, grid and axis lines are drawn in device pixels (crisp on Retina); lines, areas, markers, text and
// primitives in media (logical) pixels with antialiasing.
#include <QPaintEvent>
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Format.h"
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainterPath>
#include <algorithm>
#include <climits>

namespace charts::plot {

namespace {

QVector<qreal> dashFor(LineStyle s) {
  switch (s) {
    case LineStyle::Solid: return {};
    case LineStyle::Dotted: return {1, 1};
    case LineStyle::Dashed: return {2, 2};
    case LineStyle::LargeDashed: return {6, 6};
    case LineStyle::SparseDotted: return {1, 4};
  }
  return {};
}

QPen linePen(const QColor& c, double w, LineStyle st) {
  QPen p(c, w);
  p.setCapStyle(Qt::FlatCap);
  // round joins cost a curve per vertex in Qt's stroker; they only show on thick lines
  p.setJoinStyle(w >= 3 ? Qt::RoundJoin : Qt::BevelJoin);
  if (st != LineStyle::Solid) p.setDashPattern(dashFor(st));
  return p;
}

QTransform mediaAt(double x, double y) { return QTransform::fromTranslate(x, y); }
QTransform deviceAt(double dpr, double x, double y) { return QTransform(1 / dpr, 0, 0, 1 / dpr, x, y); }

// Horizontal or vertical line in device pixels (the painter must be in a device transform). Dashes are drawn as
// rectangles so they stay crisp at every pixel ratio.
void devLine(QPainter& p, bool horizontal, double pos, double a, double b, const QColor& c, int lw, LineStyle st) {
  if (c.alpha() == 0 || b <= a) return;
  const int at = int(std::lround(pos)) - lw / 2;
  const auto rect = [&](double s, double e) {
    if (horizontal) p.fillRect(QRectF(s, at, e - s, lw), c);
    else p.fillRect(QRectF(at, s, lw, e - s), c);
  };
  const QVector<qreal> d = dashFor(st);
  if (d.isEmpty()) return rect(std::round(a), std::round(b));
  const double on = d[0] * lw, off = d[1] * lw;
  for (double s = std::round(a); s < b; s += on + off) rect(s, qMin(b, s + on));
}

int ceilOdd(double v) { const int c = int(std::ceil(v)); return c % 2 ? c : c + 1; }

// Candle body width in device pixels for a bar spacing (media px): close to the spacing when bars are thin, shrinking
// towards 80 % of it as bars get wide; 3 px between 2.5 and 4 px of spacing.
int candleBodyWidth(double spacing, double dpr) {
  if (spacing >= 2.5 && spacing <= 4) return int(std::floor(3 * dpr));
  const double shrink = 1 - 0.2 * std::atan(qMax(4.0, spacing) - 4) / (3.14159265358979323846 / 2);
  const int w = qMin(int(std::floor(spacing * shrink * dpr)), int(std::floor(spacing * dpr)));
  return qMax(int(std::floor(dpr)), w);
}

void drawLabelText(QPainter& p, const QFontMetricsF& fm, double x, double cy, const QString& s, const QColor& c) {
  p.setPen(c);
  p.drawText(QPointF(x, cy + (fm.ascent() - fm.descent()) / 2), s);
}

}  // namespace

// ================= paint entry =================

void Chart::paintEvent(QPaintEvent* e) {
  ensureLayout();
  const double dpr = devicePixelRatioF();
  const QSize dev(int(std::ceil(width() * dpr)), int(std::ceil(height() * dpr)));
  if (baseDirty_ || base_.size() != dev || base_.devicePixelRatio() != dpr) {
    if (base_.size() != dev) base_ = QImage(dev, QImage::Format_ARGB32_Premultiplied);
    base_.setDevicePixelRatio(dpr);
    paintBase(base_);
    baseDirty_ = false;
  }
  QPainter p(this);
  // only what is repainted (a crosshair move asks for its strips): copied as is when the background is opaque
  if (o_.layout.background.alpha() == 255) p.setCompositionMode(QPainter::CompositionMode_Source);
  for (const QRect& r : e->region())
    p.drawImage(QPointF(r.topLeft()), base_, QRectF(QPointF(r.topLeft()) * dpr, QSizeF(r.size()) * dpr));
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  paintOverlay(p);
  ++paints_;
}

void Chart::paintBase(QImage& img) {
  const double dpr = img.devicePixelRatio();
  subtitle_ = {};   // set again by paintPriceAxis when a series still asks for one
  QPainter p(&img);
  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(QRectF(0, 0, width(), height()), o_.layout.background);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  const QFont f = font();
  p.setFont(f);
  marks_ = ts_.marks(f, o_.layout.fontSize, marksMaxWeight_);
  for (auto& pane : panes_) paintPane(p, pane.get(), dpr);
  // separators
  p.setTransform(QTransform());
  for (int i = 0; i + 1 < int(panes_.size()); ++i) {
    const int y = panes_[i]->rect_.bottom() + 1;
    p.fillRect(QRectF(0, y, width(), 1), o_.layout.panes.separatorColor);
    if (hoverSep_ == i) p.fillRect(QRectF(0, y - 3, width(), 7), o_.layout.panes.separatorHoverColor);
  }
  for (auto& pane : panes_) paintPriceAxis(p, pane.get(), dpr);
  paintTimeAxis(p, dpr);
}

// ================= panes =================

void Chart::paintPane(QPainter& p, Pane* pane, double dpr) {
  const QRect r = pane->rect_;
  if (r.width() <= 0 || r.height() <= 0) return;
  p.save();
  p.setClipRect(r);
  const QTransform dev = deviceAt(dpr, r.left(), r.top()), med = mediaAt(r.left(), r.top());
  const double Hd = r.height() * dpr, Wd = r.width() * dpr;
  const int px = qMax(1, int(std::floor(dpr)));
  // grid
  p.setTransform(dev);
  p.setRenderHint(QPainter::Antialiasing, false);
  if (o_.grid.vertLines.visible)
    for (const auto& m : marks_) devLine(p, false, m.x * dpr, 0, Hd, o_.grid.vertLines.color, px, o_.grid.vertLines.style);
  PriceScale* right = pane->priceScale("right");
  if (o_.grid.horzLines.visible)
    for (const auto& t : right->ticks_) devLine(p, true, t.y * dpr, 0, Wd, o_.grid.horzLines.color, px, o_.grid.horzLines.style);

  int from = 0, to = -1;
  const bool vis = ts_.visibleIndexes(from, to);
  drawPrimitives(p, pane, PrimitiveZOrder::Bottom, dpr);
  if (vis)
    for (Series* s : pane->series_)
      if (s->o_.visible) paintSeries(p, pane, s, dpr, from, to);

  // base lines (relative modes), price lines, last-price lines
  p.setTransform(dev);
  p.setRenderHint(QPainter::Antialiasing, false);
  for (Series* s : pane->series_) {
    if (!s->o_.visible) continue;
    PriceScale* sc = s->priceScale();
    if (!sc->hasRange_) continue;
    if (sc->isRelative() && s->o_.baseLineVisible && std::isfinite(s->firstValue_)) {
      const double y = sc->internalToY(sc->o_.mode == PriceScaleMode::Percentage ? 0 : 100);
      devLine(p, true, y * dpr, 0, Wd, s->o_.baseLineColor, qMax(1, int(std::floor(s->o_.baseLineWidth * dpr))), s->o_.baseLineStyle);
    }
    const Series::Row* last = s->lastValueRow();
    for (const auto& pl : s->priceLines_) {
      if (!pl->o_.lineVisible) continue;
      const auto y = sc->priceToY(pl->o_.price, s->firstValue_);
      if (!y) continue;
      const QColor c = pl->o_.color.isValid() ? pl->o_.color : (last ? s->barColor(*last) : s->o_.color);
      devLine(p, true, *y * dpr, 0, Wd, c, qMax(1, int(std::floor(pl->o_.lineWidth * dpr))), pl->o_.lineStyle);
    }
    if (s->o_.priceLineVisible && last) {
      const auto y = sc->priceToY(last->c, s->firstValue_);
      const QColor c = s->o_.priceLineColor.isValid() ? s->o_.priceLineColor : s->barColor(*last);
      if (y) devLine(p, true, *y * dpr, 0, Wd, c, qMax(1, int(std::floor(s->o_.priceLineWidth * dpr))), s->o_.priceLineStyle);
    }
  }
  p.setTransform(med);
  drawPrimitives(p, pane, PrimitiveZOrder::Normal, dpr);
  if (vis)
    for (Series* s : pane->series_)
      if (s->o_.visible && !s->markers_.isEmpty()) paintMarkers(p, s, from, to);
  drawPrimitives(p, pane, PrimitiveZOrder::Top, dpr);
  p.restore();
}

void Chart::drawPrimitives(QPainter& p, Pane* pane, PrimitiveZOrder z, double dpr) {
  const QRect r = pane->rect_;
  const int index = pane->paneIndex();
  auto run = [&](Primitive* pr, Series* s) {
    if (pr->zOrder() != z) return;
    pr->updateAllViews();
    p.save();
    p.setTransform(mediaAt(r.left(), r.top()));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(font());
    PrimitiveContext ctx{this, s, index, dpr};
    pr->draw(p, r.size(), ctx);
    p.restore();
  };
  for (Primitive* pr : pane->primitives_) run(pr, nullptr);
  for (Series* s : pane->series_)
    if (s->o_.visible)
      for (Primitive* pr : s->primitives_) run(pr, s);
}

// ================= series =================

void Chart::paintSeries(QPainter& p, Pane* pane, Series* s, double dpr, int from, int to) {
  PriceScale* sc = s->priceScale();
  if (!sc->hasRange_) return;
  const QRect r = pane->rect_;
  const double fv = s->firstValue_;
  const auto& rows = s->rows_;
  const int k0 = s->rowAtOrAfter(from), k1 = s->rowAtOrAfter(to + 1);   // [k0, k1)
  const SeriesOptions& o = s->o_;
  const double sp = ts_.spacing_;
  auto Y = [&](double price) { return sc->internalToY(sc->toInternal(price, fv)); };
  auto X = [&](int idx) { return ts_.indexToX(idx); };

  switch (s->type_) {
    case SeriesType::Candlestick: {
      p.setTransform(deviceAt(dpr, r.left(), r.top()));
      p.setRenderHint(QPainter::Antialiasing, false);
      const int wickW = qMax(1, int(std::floor(dpr)));
      int bodyW = candleBodyWidth(sp, dpr);
      if (bodyW > wickW && (bodyW - wickW) % 2) --bodyW;
      const int bw = o.borderVisible ? qMax(1, int(std::floor(dpr))) : 0;
      const QColor up = o.upColor, down = o.downColor;
      for (int k = k0; k < k1; ++k) {
        const Series::Row& b = rows[k];
        if (!(b.flags & Series::HasValue)) continue;
        const bool isUp = b.c >= b.o;
        const int xc = int(std::lround(X(b.idx) * dpr));
        const int wl = xc - wickW / 2;
        const int left = wl - (bodyW - wickW) / 2;
        const int yo = int(std::lround(Y(b.o) * dpr)), yc = int(std::lround(Y(b.c) * dpr));
        const int yh = int(std::lround(Y(b.h) * dpr)), yl = int(std::lround(Y(b.l) * dpr));
        const int top = qMin(yo, yc), bot = qMax(yo, yc);
        const QColor body = (b.flags & Series::HasColor) ? QColor::fromRgba(b.color) : (isUp ? up : down);
        const QColor border = (b.flags & Series::HasBorder) ? QColor::fromRgba(b.border) : (isUp ? o.borderUpColor : o.borderDownColor);
        const QColor wick = (b.flags & Series::HasWick) ? QColor::fromRgba(b.wick) : (isUp ? o.wickUpColor : o.wickDownColor);
        if (o.wickVisible) {
          if (top > yh) p.fillRect(QRect(wl, yh, wickW, top - yh), wick);
          if (yl > bot) p.fillRect(QRect(wl, bot + 1, wickW, yl - bot), wick);
          if (bodyW <= wickW && top == bot) p.fillRect(QRect(wl, top, wickW, 1), wick);
        }
        const QRect br(left, top, bodyW, bot - top + 1);
        if (bw > 0) {
          if (bodyW > 2 * bw && br.height() > 2 * bw) {
            p.fillRect(QRect(br.left(), br.top(), br.width(), bw), border);
            p.fillRect(QRect(br.left(), br.bottom() - bw + 1, br.width(), bw), border);
            p.fillRect(QRect(br.left(), br.top() + bw, bw, br.height() - 2 * bw), border);
            p.fillRect(QRect(br.right() - bw + 1, br.top() + bw, bw, br.height() - 2 * bw), border);
            if (body.alpha()) p.fillRect(br.adjusted(bw, bw, -bw, -bw), body);
          } else {
            p.fillRect(br, border);
          }
        } else if (body.alpha()) {
          p.fillRect(br, body);
        }
      }
      break;
    }
    case SeriesType::Bar: {
      p.setTransform(deviceAt(dpr, r.left(), r.top()));
      p.setRenderHint(QPainter::Antialiasing, false);
      const int spd = qMax(1, int(std::floor(sp * dpr)));
      int lw = o.thinBars ? qMax(1, int(std::floor(dpr))) : qMax(int(std::floor(dpr)), int(std::floor(sp * 0.3 * dpr)));
      lw = qMin(lw, spd);
      const int tick = qMax(0, qMin(int(std::floor((sp * dpr - lw) / 2 * 0.9)), int(std::floor(sp * dpr / 2))));
      for (int k = k0; k < k1; ++k) {
        const Series::Row& b = rows[k];
        if (!(b.flags & Series::HasValue)) continue;
        const QColor c = s->barColor(b);
        const int xc = int(std::lround(X(b.idx) * dpr));
        const int left = xc - lw / 2;
        const int yo = int(std::lround(Y(b.o) * dpr)), yc = int(std::lround(Y(b.c) * dpr));
        const int yh = int(std::lround(Y(b.h) * dpr)), yl = int(std::lround(Y(b.l) * dpr));
        p.fillRect(QRect(left, yh, lw, qMax(1, yl - yh + 1)), c);
        if (tick > 0) {
          if (o.openVisible) p.fillRect(QRect(left - tick, yo - lw / 2, tick, lw), c);
          p.fillRect(QRect(left + lw, yc - lw / 2, tick, lw), c);
        }
      }
      break;
    }
    case SeriesType::Histogram: {
      p.setTransform(deviceAt(dpr, r.left(), r.top()));
      p.setRenderHint(QPainter::Antialiasing, false);
      const double spd = sp * dpr;
      const int gap = std::ceil(spd) <= 1 ? 0 : qMax(1, int(std::floor(dpr)));
      const int colW = qMax(1, int(std::lround(spd)) - gap);
      const int baseY = int(std::lround(Y(o.base) * dpr));
      for (int k = k0; k < k1; ++k) {
        const Series::Row& b = rows[k];
        if (!(b.flags & Series::HasValue)) continue;
        const QColor c = s->barColor(b);
        if (!c.alpha()) continue;
        const int xc = int(std::lround(X(b.idx) * dpr));
        const int y = int(std::lround(Y(b.c) * dpr));
        const int top = qMin(y, baseY), bot = qMax(y, baseY);
        p.fillRect(QRect(xc - colW / 2, top, colW, qMax(1, bot - top)), c);
      }
      break;
    }
    case SeriesType::Line:
    case SeriesType::Area:
    case SeriesType::Baseline: {
      p.setTransform(mediaAt(r.left(), r.top()));
      p.setRenderHint(QPainter::Antialiasing, true);
      // one valued point before and after the visible range so the line runs to the edges
      int a = k0, b = k1;
      while (a > 0) { --a; if (rows[a].flags & Series::HasValue) break; }
      while (b < int(rows.size())) { if (rows[b++].flags & Series::HasValue) break; }
      struct Pt { double x, y; QRgb c; };
      QVector<Pt> pts;
      pts.reserve(b - a);
      const QRgb defColor = (s->type_ == SeriesType::Area ? o.lineColor : o.color).rgba();
      // More points than device columns (zoomed out): keep, per column and colour, the first, lowest, highest and
      // last point in their order, which draws the same pixels as the full line.
      const bool decimate = (b - a) > 2 * r.width() * dpr;
      int bucketCol = INT_MIN;
      QRgb bucketColor = 0;
      int bFirst = -1, bMin = -1, bMax = -1, bLast = -1;
      QVector<Pt> all;
      auto flushBucket = [&] {
        if (bFirst < 0) return;
        int ids[4] = {bFirst, bMin, bMax, bLast};
        std::sort(ids, ids + 4);
        int prev = -1;
        for (int id : ids)
          if (id != prev) { pts.append(all[id]); prev = id; }
        bFirst = bMin = bMax = bLast = -1;
      };
      for (int k = a; k < b; ++k) {
        const Series::Row& row = rows[k];
        if (!(row.flags & Series::HasValue)) continue;
        QRgb c = (row.flags & Series::HasColor) ? row.color : defColor;
        const Pt q{X(row.idx), Y(row.c), c};
        if (!decimate) { pts.append(q); continue; }
        const int col = int(std::floor(q.x * dpr));
        if (col != bucketCol || c != bucketColor) { flushBucket(); bucketCol = col; bucketColor = c; }
        all.append(q);
        const int id = int(all.size()) - 1;
        if (bFirst < 0) bFirst = bMin = bMax = id;
        if (q.y < all[bMin].y) bMin = id;
        if (q.y > all[bMax].y) bMax = id;
        bLast = id;
      }
      flushBucket();
      if (pts.isEmpty()) break;
      const bool steps = o.lineType == LineType::WithSteps;
      const double H = r.height(), W = r.width();
      auto pathOf = [&](double closeY) {
        QPainterPath path;
        path.moveTo(pts.first().x, closeY);
        path.lineTo(pts.first().x, pts.first().y);
        for (int j = 1; j < pts.size(); ++j) {
          if (steps) path.lineTo(pts[j].x, pts[j - 1].y);
          path.lineTo(pts[j].x, pts[j].y);
        }
        path.lineTo(pts.last().x, closeY);
        path.closeSubpath();
        return path;
      };
      if (s->type_ == SeriesType::Area && pts.size() > 1) {
        double minY = H;
        for (const Pt& q : pts) minY = qMin(minY, q.y);
        const double closeY = o.invertFilledArea ? 0 : H;
        QLinearGradient g(0, o.invertFilledArea ? H : qMax(0.0, minY), 0, closeY);
        g.setColorAt(0, o.topColor);
        g.setColorAt(1, o.bottomColor);
        p.fillPath(pathOf(closeY), g);
      }
      auto strokeLine = [&](const std::function<QColor(QRgb)>& colorOf) {
        if (!o.lineVisible || pts.size() < 2) return;
        QPolygonF run;
        QRgb runColor = 0;
        auto flush = [&] {
          if (run.size() >= 2) {
            p.setPen(linePen(colorOf(runColor), o.lineWidth, o.lineStyle));
            // short pieces: Qt rasterizes a stroked polyline as one polygon, whose cost grows with the number of
            // edges crossing each scanline; pieces of a few segments keep it linear (joins are kept by overlapping).
            constexpr int piece = 8;
            if (run.size() <= piece + 1 || o.lineStyle != LineStyle::Solid) p.drawPolyline(run);
            else
              for (int i = 0; i + 1 < run.size(); i += piece) p.drawPolyline(run.constData() + i, qMin(piece + 1, int(run.size()) - i));
          }
          run.clear();
        };
        for (int j = 0; j + 1 < pts.size(); ++j) {
          const QRgb c = pts[j].c;
          if (qAlpha(c) == 0) { flush(); continue; }   // transparent point: hide the segment starting here
          if (!run.isEmpty() && c != runColor) flush();
          if (run.isEmpty()) { run.append(QPointF(pts[j].x, pts[j].y)); runColor = c; }
          if (steps) run.append(QPointF(pts[j + 1].x, pts[j].y));
          run.append(QPointF(pts[j + 1].x, pts[j + 1].y));
        }
        flush();
      };
      if (s->type_ == SeriesType::Baseline) {
        const double baseY = qBound(-1e6, Y(o.baseValue), 1e6);
        const QPainterPath path = pathOf(baseY);
        p.save();
        p.setClipRect(QRectF(0, -1e5, W, baseY + 1e5), Qt::IntersectClip);
        QLinearGradient gt(0, 0, 0, baseY);
        gt.setColorAt(0, o.topFillColor1);
        gt.setColorAt(1, o.topFillColor2);
        p.fillPath(path, gt);
        strokeLine([&](QRgb) { return o.topLineColor; });
        p.restore();
        p.save();
        p.setClipRect(QRectF(0, baseY, W, 2e5), Qt::IntersectClip);
        QLinearGradient gb(0, baseY, 0, H);
        gb.setColorAt(0, o.bottomFillColor1);
        gb.setColorAt(1, o.bottomFillColor2);
        p.fillPath(path, gb);
        strokeLine([&](QRgb) { return o.bottomLineColor; });
        p.restore();
      } else {
        strokeLine([](QRgb c) { return QColor::fromRgba(c); });
      }
      if (o.pointMarkersVisible) {
        const double rad = o.pointMarkersRadius > 0 ? o.pointMarkersRadius : o.lineWidth / 2.0 + 2;
        p.setPen(Qt::NoPen);
        for (const Pt& q : pts) {
          if (qAlpha(q.c) == 0) continue;
          p.setBrush(QColor::fromRgba(q.c));
          p.drawEllipse(QPointF(q.x, q.y), rad, rad);
        }
        p.setBrush(Qt::NoBrush);
      }
      break;
    }
  }
}

// ================= markers =================

void Chart::paintMarkers(QPainter& p, Series* s, int from, int to) {
  PriceScale* sc = s->priceScale();
  if (!sc->hasRange_) return;
  const int fs = o_.layout.fontSize;
  const QFontMetricsF fm(font());
  p.setRenderHint(QPainter::Antialiasing, true);
  const double sp = ts_.spacing_;
  int shape = ceilOdd(qBound(12.0, sp, 30.0));
  if (shape % 2) shape -= 1;
  const double margin = qMax(ceilOdd(qBound(12.0, sp, 30.0) * 0.1), 3);
  QHash<int, double> above, below;
  for (const SeriesMarker& m : s->markers_) {
    const auto idx = ts_.timeToIndex(m.time, true);
    if (!idx || *idx < from - 1 || *idx > to + 1) continue;
    const Series::Row* row = s->rowAtIndex(*idx);
    if (!row || !(row->flags & Series::HasValue)) continue;
    double price = row->c;
    if (s->isBarType()) price = m.position == MarkerPosition::AboveBar ? row->h : m.position == MarkerPosition::BelowBar ? row->l : row->c;
    const auto yp = sc->priceToY(price, s->firstValue_);
    if (!yp) continue;
    const double x = ts_.indexToX(*idx);
    const double f = shape * qMax(0.0, m.size), half = f / 2;
    double y = *yp, ty = 0;
    const bool hasText = !m.text.isEmpty();
    switch (m.position) {
      case MarkerPosition::AboveBar: {
        double& off = above[*idx];
        if (off == 0) off = margin;
        y = *yp - half - off;
        ty = y - half - 0.6 * fs;
        if (hasText) off += 1.2 * fs;
        off += f + margin;
        break;
      }
      case MarkerPosition::BelowBar: {
        double& off = below[*idx];
        if (off == 0) off = margin;
        y = *yp + half + off;
        ty = y + half + margin + 0.6 * fs;
        if (hasText) off += 1.2 * fs;
        off += f + margin;
        break;
      }
      case MarkerPosition::InBar:
        y = *yp;
        ty = y + half + margin + 0.6 * fs;
        break;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(m.color);
    const double big = qMax(f, 12.0);
    switch (m.shape) {
      case MarkerShape::Circle: {
        const double rad = (ceilOdd(big * 0.8) - 1) / 2.0;
        p.drawEllipse(QPointF(x, y), rad, rad);
        break;
      }
      case MarkerShape::Square: {
        const double side = ceilOdd(big * 0.7);
        p.drawRect(QRectF(x - (side - 1) / 2, y - (side - 1) / 2, side, side));
        break;
      }
      case MarkerShape::ArrowUp:
      case MarkerShape::ArrowDown: {
        const double e = (ceilOdd(big) - 1) / 2.0, sh = (ceilOdd(f / 2) - 1) / 2.0;
        const double d = m.shape == MarkerShape::ArrowUp ? 1 : -1;   // head direction (up = towards smaller y)
        const QPointF poly[] = {{x - e, y}, {x, y - d * e}, {x + e, y}, {x + sh, y}, {x + sh, y + d * e}, {x - sh, y + d * e}, {x - sh, y}};
        p.drawPolygon(poly, 7);
        break;
      }
    }
    p.setBrush(Qt::NoBrush);
    if (hasText) {
      p.setPen(m.color);
      const double w = fm.horizontalAdvance(m.text);
      p.drawText(QPointF(x - w / 2, ty + (fm.ascent() - fm.descent()) / 2), m.text);
    }
  }
}

// ================= axes =================

void Chart::paintPriceAxis(QPainter& p, Pane* pane, double dpr) {
  if (priceAxisW_ <= 0) return;
  PriceScale* sc = pane->priceScale("right");
  if (!sc->o_.visible) return;
  const QRect r = pane->rect_;
  const int x0 = r.right() + 1;
  const QRect axis(x0, r.top(), priceAxisW_, r.height());
  const int fs = o_.layout.fontSize;
  const double padI = 5.0 * fs / 12, padO = padI;
  const double textX = x0 + 1 + 5 + padI;
  const QFont f = font();
  const QFontMetricsF fm(f);
  const QColor textColor = sc->o_.textColor.isValid() ? sc->o_.textColor : o_.layout.textColor;
  p.save();
  p.setFont(f);
  p.setClipRect(axis);
  p.setTransform(QTransform());
  p.setRenderHint(QPainter::Antialiasing, false);
  // tick labels
  for (const auto& t : sc->ticks_) drawLabelText(p, fm, textX, r.top() + t.y, t.label, textColor);
  if (sc->o_.ticksVisible) {
    p.setTransform(deviceAt(dpr, 0, 0));
    for (const auto& t : sc->ticks_)
      devLine(p, true, (r.top() + t.y) * dpr, (x0 + 1) * dpr, (x0 + 6) * dpr, sc->o_.borderColor, qMax(1, int(std::floor(dpr))), LineStyle::Solid);
    p.setTransform(QTransform());
  }
  // series labels (last values, price lines)
  struct Label { double y; QString text, title; QColor bg, fg; const Series* subtitled = nullptr; };
  QVector<Label> labels;
  for (Series* s : pane->series_) {
    if (!s->o_.visible) continue;
    PriceScale* ssc = s->priceScale();
    if (!ssc->hasRange_) continue;
    const Series::Row* last = s->lastValueRow();
    for (const auto& pl : s->priceLines_) {
      if (!pl->o_.axisLabelVisible) continue;
      const auto y = ssc->priceToY(pl->o_.price, s->firstValue_);
      if (!y) continue;
      const QColor bg = pl->o_.axisLabelColor.isValid() ? pl->o_.axisLabelColor
                        : pl->o_.color.isValid()        ? pl->o_.color
                        : last                          ? s->barColor(*last)
                                                        : s->o_.color;
      const QColor fg = pl->o_.axisLabelTextColor.isValid() ? pl->o_.axisLabelTextColor : fmt::textOn(bg);
      labels.append({*y, formatAxisPrice(ssc, s, pl->o_.price), pl->o_.title, bg, fg});
    }
    if (s->o_.lastValueVisible && last) {
      const auto y = ssc->priceToY(last->c, s->firstValue_);
      if (!y) continue;
      QColor bg = s->barColor(*last);
      if (bg.alpha() == 0) bg = s->type_ == SeriesType::Area ? s->o_.lineColor : s->o_.color;
      labels.append({*y, formatAxisPrice(ssc, s, last->c), s->o_.title, bg, fmt::textOn(bg), s->o_.lastValueSubtitle ? s : nullptr});
    }
  }
  const double lh = std::ceil(fs + 2 * 2.5 * fs / 12) + 1;
  if (sc->o_.alignLabels && labels.size() > 1) {
    std::stable_sort(labels.begin(), labels.end(), [](const Label& a, const Label& b) { return a.y < b.y; });
    for (int i = 1; i < labels.size(); ++i)
      if (labels[i].y < labels[i - 1].y + lh) labels[i].y = labels[i - 1].y + lh;
  }
  for (const Label& L : labels) {
    const double cy = r.top() + L.y;
    const double w = 1 + 5 + padI + fm.horizontalAdvance(L.text) + padO;
    p.fillRect(QRectF(x0, std::round(cy - lh / 2), w, lh), L.bg);
    drawLabelText(p, fm, textX, cy, L.text, L.fg);
    if (L.subtitled) subtitle_ = {true, QRectF(x0, std::round(cy - lh / 2), w, lh), textX, L.bg, L.fg, L.subtitled->o_.lastValueSubtitle};
  }
  // border
  if (sc->o_.borderVisible) {
    p.setTransform(deviceAt(dpr, 0, 0));
    devLine(p, false, x0 * dpr + qMax(1, int(std::floor(dpr))) / 2, r.top() * dpr, (r.bottom() + 1) * dpr, sc->o_.borderColor,
            qMax(1, int(std::floor(dpr))), LineStyle::Solid);
    p.setTransform(QTransform());
  }
  p.setClipping(false);
  // series titles: a box left of the axis label, inside the pane
  for (const Label& L : labels) {
    if (L.title.isEmpty()) continue;
    const double cy = r.top() + L.y;
    const double w = fm.horizontalAdvance(L.title) + 2 * padI;
    p.fillRect(QRectF(x0 - w, std::round(cy - lh / 2), w, lh), L.bg);
    drawLabelText(p, fm, x0 - w + padI, cy, L.title, L.fg);
  }
  p.restore();
}

void Chart::paintTimeAxis(QPainter& p, double dpr) {
  if (timeAxisH_ <= 0) return;
  const int top = height() - timeAxisH_, plotW = ts_.width(), fs = o_.layout.fontSize;
  p.save();
  p.setTransform(QTransform());
  p.setClipRect(QRect(0, top, plotW, timeAxisH_));
  QFont f = font(), bold = f;
  bold.setWeight(QFont::Bold);
  const QFontMetricsF fm(f), fmb(bold);
  const double cy = top + 1 + 5 + 3.0 * fs / 12 + fs / 2.0;
  const bool allowBold = ts_.o_.allowBoldLabels;
  for (int pass = 0; pass < 2; ++pass) {
    const bool boldPass = pass == 1;
    p.setFont(boldPass && allowBold ? bold : f);
    const QFontMetricsF& m = boldPass && allowBold ? fmb : fm;
    for (const auto& mk : marks_) {
      if ((mk.weight >= marksMaxWeight_) != boldPass) continue;
      const double w = m.horizontalAdvance(mk.label);
      drawLabelText(p, m, mk.x - w / 2, cy, mk.label, o_.layout.textColor);
    }
  }
  if (ts_.o_.ticksVisible) {
    p.setTransform(deviceAt(dpr, 0, 0));
    for (const auto& mk : marks_)
      devLine(p, false, mk.x * dpr, top * dpr, (top + 5) * dpr, ts_.o_.borderColor, qMax(1, int(std::floor(dpr))), LineStyle::Solid);
  }
  if (ts_.o_.borderVisible) {
    p.setTransform(deviceAt(dpr, 0, 0));
    devLine(p, true, top * dpr + qMax(1, int(std::floor(dpr))) / 2, 0, plotW * dpr, ts_.o_.borderColor, qMax(1, int(std::floor(dpr))),
            LineStyle::Solid);
  }
  p.restore();
}

// ================= crosshair =================

QRect Chart::subtitleRect() const {
  if (!subtitle_.on) return {};
  return QRectF(subtitle_.label.left(), subtitle_.label.bottom(), width() - subtitle_.label.left(), subtitle_.label.height() + 1)
      .toAlignedRect();
}

void Chart::refreshCountdown() {
  if (subtitle_.on) update(subtitleRect());
}

void Chart::paintSubtitle(QPainter& p) {
  if (!subtitle_.on || !subtitle_.text) return;
  const QString text = subtitle_.text();
  if (text.isEmpty()) return;
  const QFont f = font();
  const QFontMetricsF fm(f);
  const double padO = 5.0 * o_.layout.fontSize / 12;
  const QRectF& L = subtitle_.label;
  // the label's width, or the text's when it is wider (under a short price)
  const double w = std::max(L.width(), subtitle_.textX - L.left() + fm.horizontalAdvance(text) + padO);
  p.save();
  p.setTransform(QTransform());
  p.setFont(f);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.fillRect(QRectF(L.left(), L.bottom(), w, L.height()), subtitle_.bg);
  drawLabelText(p, fm, subtitle_.textX, L.bottom() + L.height() / 2, text, subtitle_.fg);
  p.restore();
}

void Chart::paintOverlay(QPainter& p) {
  paintSubtitle(p);   // under the crosshair's labels
  const CrosshairOptions& ch = o_.crosshair;
  if (!cross_.visible || ts_.isEmpty() || ch.mode == CrosshairMode::Hidden || panes_.empty()) return;
  const double dpr = devicePixelRatioF();
  const int fs = o_.layout.fontSize, plotW = ts_.width();
  const double x = ts_.indexToX(cross_.index);
  const bool xIn = x >= -1 && x <= plotW + 1;
  const int pi = qBound(0, cross_.pane, int(panes_.size()) - 1);
  Pane* pane = panes_[pi].get();
  const QRect pr = pane->rect_;
  const bool yIn = cross_.y >= 0 && cross_.y <= pr.height();
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setTransform(deviceAt(dpr, 0, 0));
  if (ch.vertLine.visible && xIn)
    for (const auto& pn : panes_) {
      const QRect r = pn->rect_;
      p.setClipRect(QRectF(r.left() * dpr, r.top() * dpr, r.width() * dpr, r.height() * dpr));
      devLine(p, false, x * dpr, r.top() * dpr, (r.bottom() + 1) * dpr, ch.vertLine.color, qMax(1, int(std::floor(ch.vertLine.width * dpr))),
              ch.vertLine.style);
    }
  p.setClipping(false);
  if (ch.horzLine.visible && yIn)
    devLine(p, true, (pr.top() + cross_.y) * dpr, 0, plotW * dpr, ch.horzLine.color, qMax(1, int(std::floor(ch.horzLine.width * dpr))),
            ch.horzLine.style);
  // markers on line-like series
  p.setTransform(QTransform());
  p.setRenderHint(QPainter::Antialiasing, true);
  if (xIn && cross_.index >= 0 && cross_.index <= ts_.baseIndex())
    for (const auto& pn : panes_) {
      p.setClipRect(pn->rect_);
      for (Series* s : pn->series_) {
        if (!s->o_.visible || !s->o_.crosshairMarkerVisible) continue;
        if (s->type_ != SeriesType::Line && s->type_ != SeriesType::Area && s->type_ != SeriesType::Baseline) continue;
        const Series::Row* row = s->rowAtIndex(cross_.index);
        if (!row || !(row->flags & Series::HasValue)) continue;
        const auto y = s->priceScale()->priceToY(row->c, s->firstValue_);
        if (!y) continue;
        const QPointF c(x, pn->rect_.top() + *y);
        const double rad = s->o_.crosshairMarkerRadius, bw = s->o_.crosshairMarkerBorderWidth;
        QColor fill = s->o_.crosshairMarkerBackgroundColor.isValid() ? s->o_.crosshairMarkerBackgroundColor : s->barColor(*row);
        const QColor border = s->o_.crosshairMarkerBorderColor.isValid() ? s->o_.crosshairMarkerBorderColor : o_.layout.background;
        p.setPen(Qt::NoPen);
        if (bw > 0) { p.setBrush(border); p.drawEllipse(c, rad + bw, rad + bw); }
        p.setBrush(fill);
        p.drawEllipse(c, rad, rad);
      }
    }
  p.setClipping(false);
  p.setBrush(Qt::NoBrush);
  p.setRenderHint(QPainter::Antialiasing, false);
  const QFont f = font();
  const QFontMetricsF fm(f);
  p.setFont(f);
  // price label
  PriceScale* sc = pane->priceScale("right");
  if (ch.horzLine.labelVisible && yIn && priceAxisW_ > 0 && sc->hasRange_ && sc->o_.visible) {
    const double padI = 5.0 * fs / 12;
    const QString text = sc->formatInternal(sc->yToInternal(cross_.y));
    const double lh = std::ceil(fs + 2 * 2.5 * fs / 12) + 1;
    const double cy = pr.top() + cross_.y;
    const double w = 1 + 5 + padI + fm.horizontalAdvance(text) + padI;
    const int x0 = pr.right() + 1;
    p.setClipRect(QRect(x0, pr.top(), priceAxisW_, pr.height()));
    p.fillRect(QRectF(x0, std::round(cy - lh / 2), w, lh), ch.horzLine.labelBackgroundColor);
    drawLabelText(p, fm, x0 + 1 + 5 + padI, cy, text, fmt::textOn(ch.horzLine.labelBackgroundColor));
    p.setClipping(false);
  }
  // time label
  if (ch.vertLine.labelVisible && xIn && timeAxisH_ > 0 && cross_.index >= 0 && cross_.index <= ts_.baseIndex()) {
    const QString text = ts_.formatCrosshairTime(ts_.times_[cross_.index]);
    const double padH = 9.0 * fs / 12;
    const double w = fm.horizontalAdvance(text) + 2 * padH;
    const int top = height() - timeAxisH_;
    double left = qBound(0.0, std::round(x - w / 2), qMax(0.0, plotW - w));
    const double h = timeAxisH_ - 4.0 * fs / 12;
    p.fillRect(QRectF(left, top, w, std::round(h)), ch.vertLine.labelBackgroundColor);
    drawLabelText(p, fm, left + padH, top + 1 + 5 + 3.0 * fs / 12 + fs / 2.0, text, fmt::textOn(ch.vertLine.labelBackgroundColor));
  }
  p.restore();
}

}  // namespace charts::plot
