#include "pages/account/TrCharts.h"
#include "pages/account/AccountKit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

namespace acct {

static QColor lighter(const QColor& col) { return theme::mix(col, theme::map(QColor(Qt::white)), 0.28); }

int drawVBars(QPainter& p, const VBars& b, double anim, const QPointF& mouse) {
  const int n = b.values.size();
  if (n == 0) return -1;
  double lo = 0, hi = 0;
  for (double v : b.values) { lo = std::min(lo, v); hi = std::max(hi, v); }
  double a0, a1;
  const auto tk = ticks(lo, hi, b.nticks, &a0, &a1);
  const QRectF& r = b.plot;
  auto Y = [&](double v) { return r.bottom() - (v - a0) / (a1 - a0) * r.height(); };
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(chart(11));
  for (double t : tk) {
    const qreal y = std::round(Y(t)) + 0.5;
    p.setPen(t == 0 ? c().zero : c().grid);
    p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    p.setPen(c().axis);
    p.drawText(QRectF(r.left() - 80, y - 8, 72, 16), Qt::AlignRight | Qt::AlignVCenter, b.yfmt ? b.yfmt(t) : QString::number(t));
  }
  const qreal slot = r.width() / n;
  int hov = -1;
  if (std::isfinite(mouse.x()) && r.adjusted(0, -6, 0, 6).contains(mouse)) hov = std::clamp(int((mouse.x() - r.left()) / slot), 0, n - 1);
  if (hov >= 0) p.fillRect(QRectF(r.left() + hov * slot, r.top(), slot, r.height()), theme::alpha(c().hoverBand, 150));
  const qreal bw = b.barWidth < 1 ? std::max(2.0, slot * b.barWidth) : std::min<qreal>(b.barWidth, slot * 0.8);
  const qreal y0 = Y(0);
  for (int i = 0; i < n; ++i) {
    const qreal cx = r.left() + (i + 0.5) * slot;
    const qreal y1 = Y(b.values[i] * anim);
    QRectF bar(cx - bw / 2, std::min(y0, y1), bw, std::abs(y1 - y0));
    QColor col = b.colors.value(i, b.values[i] >= 0 ? c().cyanBar : c().redBar);
    if (i == hov) col = lighter(col);
    p.fillRect(bar.adjusted(0, b.values[i] >= 0 ? 0 : 1, 0, 0), col);
    if (i % std::max(1, b.labelEvery) == 0 && i < b.labels.size()) {
      p.setPen(c().axis);
      p.drawText(QRectF(cx - 50, r.bottom() + 8, 100, 16), Qt::AlignHCenter | Qt::AlignVCenter, b.labels[i]);
    }
  }
  p.restore();
  return hov;
}

int drawHBars(QPainter& p, const HBars& b, double anim, const QPointF& mouse) {
  const int n = b.values.size();
  if (n == 0) return -1;
  double lo = 0, hi = 0;
  for (double v : b.values) { lo = std::min(lo, v); hi = std::max(hi, v); }
  double a0, a1;
  QVector<double> tk;
  if (b.countAxis) {
    const int step = std::max(1, int(std::ceil(hi / 5.0)));
    a0 = 0; a1 = step * 5;
    while (a1 - step >= hi && a1 > step) a1 -= step;
    for (double v = 0; v <= a1 + 1e-9; v += step) tk << v;
  } else tk = ticks(lo, hi, b.nticks, &a0, &a1);
  const QRectF& r = b.plot;
  auto X = [&](double v) { return r.left() + (v - a0) / (a1 - a0) * r.width(); };
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(chart(11));
  for (double t : tk) {
    const qreal x = std::round(X(t)) + 0.5;
    p.setPen(c().grid);
    p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
    p.setPen(c().axis);
    p.drawText(QRectF(x - 40, r.bottom() + 7, 80, 16), Qt::AlignCenter, b.xfmt ? b.xfmt(t) : QString::number(t));
  }
  const qreal pitch = r.height() / n;
  int hov = -1;
  if (std::isfinite(mouse.y()) && QRectF(r.left() - 110, r.top(), r.width() + 110, r.height()).contains(mouse))
    hov = std::clamp(int((mouse.y() - r.top()) / pitch), 0, n - 1);
  const qreal x0 = X(0);
  if (hov >= 0) p.fillRect(QRectF(r.left(), r.top() + hov * pitch, r.width(), pitch), c().hoverBand);
  for (int i = 0; i < n; ++i) {
    const qreal cy = r.top() + (i + 0.5) * pitch;
    const qreal x1 = X(b.values[i] * anim);
    QRectF bar(std::min(x0, x1), cy - 10, std::abs(x1 - x0), 20);
    QColor col = b.colors.value(i, b.values[i] >= 0 ? c().cyanBar : c().redBar);
    if (i == hov) col = lighter(col);
    p.fillRect(bar, col);
    p.setPen(c().category);
    const QRectF lr(r.left() - 160, cy - 14, 151, 28);
    if (i < b.sub.size() && !b.sub[i].isEmpty()) {
      p.drawText(QRectF(lr.left(), cy - 14, lr.width(), 14), Qt::AlignRight | Qt::AlignVCenter, b.labels.value(i));
      p.drawText(QRectF(lr.left(), cy, lr.width(), 14), Qt::AlignRight | Qt::AlignVCenter, b.sub[i]);
    } else {
      p.drawText(lr, Qt::AlignRight | Qt::AlignVCenter, b.labels.value(i));
    }
  }
  // value axis line
  p.setPen(c().axisLine);
  const qreal ax = std::round(b.anchorRight ? r.right() : x0) + 0.5;
  p.drawLine(QPointF(ax, r.top()), QPointF(ax, r.bottom()));
  p.restore();
  return hov;
}
}
