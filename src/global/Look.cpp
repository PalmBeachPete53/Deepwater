#include "global/Look.h"
#include "ui/Theme.h"
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

namespace global::look {

void pageBackground(QPainter& p, const QRectF& r) {
  QLinearGradient g(r.topLeft(), r.bottomLeft());
  g.setColorAt(0, theme::map("#090502"));
  g.setColorAt(1, theme::map("#040201"));
  p.fillRect(r, g);
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(QPen(theme::map("#0d0904"), 1));
  for (qreal x = r.left() + 20; x < r.right(); x += 40) p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
  for (qreal y = r.top() + 20; y < r.bottom(); y += 40) p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
  p.restore();
}

void brackets(QPainter& p, const QRectF& r, qreal inset, qreal arm) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(Qt::NoPen);
  const QRectF q = r.adjusted(inset, inset, -inset, -inset);
  p.setBrush(theme::map("#807752"));
  p.drawRect(QRectF(q.left(), q.top(), arm, 2));
  p.drawRect(QRectF(q.left(), q.top(), 2, arm));
  p.setBrush(theme::map("#8c7f4a"));
  p.drawRect(QRectF(q.right() - arm, q.bottom() - 2, arm, 2));
  p.drawRect(QRectF(q.right() - 2, q.bottom() - arm, 2, arm));
  p.restore();
}

void panel(QPainter& p, const QRectF& r, const QColor& body, const QColor& border) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.fillRect(r, body);
  p.setPen(QPen(border, 1));
  p.setBrush(Qt::NoBrush);
  p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
  p.restore();
  brackets(p, r);
}

QFont titleFont() { return theme::mono(13, QFont::Bold, 1.7); }
qreal width(const QString& s, const QFont& f) { return QFontMetricsF(f).horizontalAdvance(s); }

qreal text(QPainter& p, qreal x, qreal baseline, const QString& s, const QFont& f, const QColor& c) {
  p.setFont(f);
  p.setPen(c);
  p.drawText(QPointF(x, baseline), s);
  return width(s, f);
}
qreal textRight(QPainter& p, qreal right, qreal baseline, const QString& s, const QFont& f, const QColor& c) {
  const qreal w = width(s, f);
  // letter spacing adds trailing space after the last glyph: compensate so the ink ends at `right`
  const qreal ls = f.letterSpacingType() == QFont::AbsoluteSpacing ? f.letterSpacing() : 0;
  text(p, right - w + ls, baseline, s, f, c);
  return w;
}
qreal textCenter(QPainter& p, qreal cx, qreal baseline, const QString& s, const QFont& f, const QColor& c) {
  const qreal w = width(s, f);
  const qreal ls = f.letterSpacingType() == QFont::AbsoluteSpacing ? f.letterSpacing() : 0;
  text(p, cx - (w - ls) / 2, baseline, s, f, c);
  return w;
}

void infoIcon(QPainter& p, const QPointF& c, const QColor& col) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(col, 1));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(c, 5, 5);
  p.setPen(Qt::NoPen);
  p.setBrush(col);
  p.drawRect(QRectF(c.x() - 0.5, c.y() - 1, 1, 3.5));
  p.drawEllipse(QPointF(c.x(), c.y() - 2.6), 0.7, 0.7);
  p.restore();
}

qreal panelTitle(QPainter& p, const QRectF& r, const QString& s, bool info) {
  const QFont f = titleFont();
  const qreal w = text(p, r.left() + 11, r.top() + 21, s, f, theme::map("#ebd88a"));
  qreal x = r.left() + 11 + w;
  if (info) { infoIcon(p, QPointF(x + 8, r.top() + 16.5)); x += 14; }
  return x;
}

bool cursorOn() { return (QDateTime::currentMSecsSinceEpoch() / 500) % 2 == 0; }

void cursorBlock(QPainter& p, qreal x, qreal y, qreal w, qreal h) { p.fillRect(QRectF(x, y, w, h), theme::map("#fcd700")); }

void glowLine(QPainter& p, const QPointF& a, const QPointF& b, const QColor& c, qreal w, qreal spread, int alpha) {
  if (!theme::isDark()) alpha = alpha * 3 / 10;   // glows look dirty on light backgrounds
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  for (int i = 4; i >= 1; --i) {
    QColor g = c; g.setAlpha(alpha * (5 - i) / 10);
    QPen pen(g, w + spread * i / 2.0); pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(a, b);
  }
  p.restore();
}
}
