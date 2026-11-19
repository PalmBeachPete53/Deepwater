#include "ui/Chart.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <cmath>

namespace chart {
QVector<double> niceTicks(double lo, double hi, int n) {
  QVector<double> out;
  if (!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo || n < 1) return out;
  const double raw = (hi - lo) / n;
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double f = raw / mag;
  const double step = (f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * mag;
  // A range of a few ulps (flat prices, an extreme zoom) gives a step below the price's resolution: `v += step` then
  // never moves and the loop filled memory until the app died. Counted on an integer instead, and capped.
  if (!(step > 0) || !std::isfinite(step) || lo + step == lo || hi + step == hi) return out;
  const double k0 = std::ceil(lo / step);
  for (int k = 0; k < 1000; ++k) {
    const double v = (k0 + k) * step;
    if (v > hi + step * 1e-9) break;
    out << (std::abs(v) < step * 1e-9 ? 0.0 : v);
  }
  return out;
}
QPair<double, double> range(const QVector<double>& v, double margin) {
  double lo = INFINITY, hi = -INFINITY;
  for (double x : v) if (std::isfinite(x)) { lo = std::min(lo, x); hi = std::max(hi, x); }
  if (!std::isfinite(lo)) return {0, 1};
  if (hi == lo) { const double d = std::abs(lo) * 0.01 + 1e-9; lo -= d; hi += d; }
  const double m = (hi - lo) * margin;
  return {lo - m, hi + m};
}
void sparkline(QPainter& p, const QRectF& r, const QVector<double>& v, const QColor& c, qreal w, bool fill, double lo, double hi) {
  if (v.size() < 2) return;
  if (!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo) { auto rg = range(v, 0.04); lo = rg.first; hi = rg.second; }
  QPainterPath path; bool started = false;
  const double dx = r.width() / (v.size() - 1);
  for (int i = 0; i < v.size(); ++i) {
    if (!std::isfinite(v[i])) continue;
    const QPointF pt(r.left() + i * dx, r.bottom() - (v[i] - lo) / (hi - lo) * r.height());
    if (!started) { path.moveTo(pt); started = true; } else path.lineTo(pt);
  }
  if (!started) return;   // no finite value: nothing to draw (the fill below reads the path's first point)
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  if (fill) {
    QPainterPath area = path; area.lineTo(path.currentPosition().x(), r.bottom()); area.lineTo(path.elementAt(0).x, r.bottom()); area.closeSubpath();
    QLinearGradient g(r.topLeft(), r.bottomLeft()); g.setColorAt(0, theme::alpha(c, 70)); g.setColorAt(1, theme::alpha(c, 0));
    p.fillPath(area, g);
  }
  p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush); p.drawPath(path);
  p.restore();
}
void hGrid(QPainter& p, const QRectF& plot, const QVector<double>& ticks, double lo, double hi,
           const std::function<QString(double)>& label, bool right) {
  p.save();
  p.setFont(theme::mono(10));
  for (double t : ticks) {
    const qreal y = plot.bottom() - (t - lo) / (hi - lo) * plot.height();
    p.setPen(QPen(theme::pal().grid, 1)); p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    p.setPen(theme::pal().textMute);
    const QString s = label(t);
    if (right) p.drawText(QRectF(plot.right() + 6, y - 8, 70, 16), Qt::AlignLeft | Qt::AlignVCenter, s);
    else p.drawText(QRectF(plot.left() - 76, y - 8, 70, 16), Qt::AlignRight | Qt::AlignVCenter, s);
  }
  p.restore();
}
void tooltip(QPainter& p, const QPointF& at, const QStringList& lines, const QRectF& bounds, const QList<QColor>& colors) {
  if (lines.isEmpty()) return;
  p.save();
  const QFont f = theme::mono(11);
  const QFontMetricsF fm(f);
  qreal w = 0; for (const auto& l : lines) w = std::max(w, fm.horizontalAdvance(l));
  const qreal lh = fm.height() + 3, h = lines.size() * lh + 14; w += 22;
  QRectF box(at.x() + 14, at.y() - h - 10, w, h);
  if (box.right() > bounds.right()) box.moveRight(at.x() - 14);
  if (box.top() < bounds.top()) box.moveTop(at.y() + 12);
  if (box.bottom() > bounds.bottom()) box.moveBottom(bounds.bottom());
  if (box.left() < bounds.left()) box.moveLeft(bounds.left());
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(theme::pal().borderHi, 1)); p.setBrush(theme::map(QColor(14, 11, 6, 240))); p.drawRoundedRect(box, 3, 3);
  p.setFont(f);
  for (int i = 0; i < lines.size(); ++i) {
    p.setPen(i < colors.size() && colors[i].isValid() ? colors[i] : (i == 0 ? theme::pal().textDim : theme::pal().text));
    p.drawText(QPointF(box.left() + 11, box.top() + 7 + fm.ascent() + i * lh), lines[i]);
  }
  p.restore();
}
void axisTag(QPainter& p, const QPointF& a, const QString& text, const QColor& bg, const QColor& fg) {
  p.save();
  const QFont f = theme::mono(10, QFont::Bold);
  const qreal w = QFontMetricsF(f).horizontalAdvance(text) + 10;
  const QRectF r(a.x(), a.y() - 8, w, 16);
  p.fillRect(r, bg); p.setFont(f); p.setPen(fg); p.drawText(r, Qt::AlignCenter, text);
  p.restore();
}
}
