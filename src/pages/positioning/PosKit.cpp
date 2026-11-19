#include "pages/positioning/PosKit.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScrollBar>
#include <QElapsedTimer>
#include <QToolTip>
#include <QHelpEvent>
#include <QtMath>
#include <QRegularExpression>
#include <cmath>

namespace positioning::pk {

static QElapsedTimer& clock() { static QElapsedTimer t = [] { QElapsedTimer x; x.start(); return x; }(); return t; }
double pulse() {
  const double ph = std::fmod(clock().elapsed() / 2000.0, 1.0);
  return 0.5 - 0.5 * std::cos(ph * 2 * M_PI);
}
QString mapQss(const QString& s) {
  static const QRegularExpression re("#([0-9a-fA-F]{6})\\b");
  QString out; qsizetype last = 0;
  for (auto it = re.globalMatch(s); it.hasNext();) {
    const auto m = it.next();
    out += s.mid(last, m.capturedStart() - last) + theme::map(QColor("#" + m.captured(1))).name();
    last = m.capturedEnd();
  }
  return out + s.mid(last);
}
bool cursorOn() { return std::fmod(clock().elapsed() / 3000.0, 1.0) < 0.55; }

// ---------------------------------------------------------------- Ticker
Ticker::Ticker(QObject* parent, int ms) : QObject(parent) {
  m_t.setInterval(ms);
  connect(&m_t, &QTimer::timeout, this, [this] {
    for (auto& [w, f] : m_w) if (w && w->isVisible()) { const QRect r = f(); if (r.isEmpty()) w->update(); else w->update(r); }
    // block cursors turn on and off twice per 3 s cycle: their areas were repainted on every tick
    const bool on = cursorOn();
    if (on != m_cursorOn) for (auto& [w, f] : m_cursors) if (w && w->isVisible()) w->update(f());
    m_cursorOn = on;
  });
}
void Ticker::addCursor(QWidget* w, const QRect& r) { m_cursors << qMakePair(QPointer<QWidget>(w), std::function<QRect()>([r] { return r; })); }
void Ticker::add(QWidget* w, const QRect& r) { m_w << qMakePair(QPointer<QWidget>(w), std::function<QRect()>([r] { return r; })); }
void Ticker::add(QWidget* w, std::function<QRect()> f) { m_w << qMakePair(QPointer<QWidget>(w), std::move(f)); }
void Ticker::setRunning(bool on) { if (on) m_t.start(); else m_t.stop(); }

// ---------------------------------------------------------------- painters
QFont monoSpaced(int px, qreal sp, int weight) { return theme::mono(px, weight, sp); }

qreal drawSpaced(QPainter& p, const QPointF& base, const QString& text, const QFont& f, const QColor& c) {
  p.setFont(f); p.setPen(c);
  p.drawText(base, text);
  return QFontMetricsF(f).horizontalAdvance(text);
}

void drawBlockCursor(QPainter& p, const QRectF& r) {
  if (!cursorOn()) return;
  p.fillRect(r, theme::map(0xfd, 0xe7, 0x8f));
}

void drawPulseDot(QPainter& p, const QPointF& c, qreal rad, bool animate) {
  const QColor dim = theme::map(0x7e, 0x76, 0x3c), bright = theme::map(0xf2, 0xdd, 0x64);
  const double t = animate ? pulse() : 0.0;
  p.save(); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen);
  if (t > 0.05) { p.setBrush(theme::alpha(bright, int(70 * t))); p.drawEllipse(c, rad + 2.5 * t, rad + 2.5 * t); }
  p.setBrush(theme::mix(dim, bright, t)); p.drawEllipse(c, rad, rad);
  p.restore();
}

void drawIcon(QPainter& p, TitleBar::Icon icon, const QRectF& r, const QColor& c) {
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(c, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
  if (icon == TitleBar::Users) {
    const qreal x = r.left(), y = r.top(), s = r.width() / 14.0;
    p.drawEllipse(QPointF(x + 5 * s, y + 4.5 * s), 2.4 * s, 2.4 * s);
    QPainterPath b; b.moveTo(x + 0.8 * s, y + 12.5 * s);
    b.cubicTo(x + 0.8 * s, y + 8.6 * s, x + 9.2 * s, y + 8.6 * s, x + 9.2 * s, y + 12.5 * s);
    p.drawPath(b);
    QPainterPath a; a.moveTo(x + 9.6 * s, y + 2.3 * s); a.cubicTo(x + 12.4 * s, y + 2.6 * s, x + 12.4 * s, y + 6.6 * s, x + 9.6 * s, y + 6.9 * s);
    p.drawPath(a);
    QPainterPath d; d.moveTo(x + 11 * s, y + 8.8 * s); d.cubicTo(x + 12.8 * s, y + 9.4 * s, x + 13.4 * s, y + 10.8 * s, x + 13.4 * s, y + 12.5 * s);
    p.drawPath(d);
  } else if (icon == TitleBar::Gauge) {
    const QRectF a(r.left() + 1, r.top() + 2, r.width() - 2, r.width() - 2);
    p.drawArc(a, 20 * 16, 140 * 16);
    const QPointF c0 = a.center();
    p.drawLine(c0, c0 + QPointF(2.6, -2.6));
  }
  p.restore();
}

void drawSectionHeading(QPainter& p, const QRectF& r, const QString& text) {
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  const qreal cy = r.center().y();
  p.setPen(Qt::NoPen); p.setBrush(theme::map(0xf4, 0xdf, 0x8d)); p.drawEllipse(QPointF(r.left() + 2.5, cy), 2.5, 2.5);
  const QFont f = theme::mono(11, QFont::Bold, 2.5);
  const QFontMetricsF fm(f);
  p.setFont(f); p.setPen(theme::map(0xf8, 0xe4, 0xa9));
  p.drawText(QPointF(r.left() + 12, cy + fm.capHeight() / 2), text);
  const qreal x = r.left() + 12 + fm.horizontalAdvance(text) + 10;
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(QPen(theme::map(0x25, 0x24, 0x1b), 1)); p.drawLine(QPointF(x, cy), QPointF(r.right(), cy));
  p.restore();
}

Status statusOf(double l) {
  if (!std::isfinite(l)) return Status::Balanced;
  const int v = fmt::roundInt(l);
  if (v >= 65) return Status::CrowdedLong;
  if (v > 50) return Status::LeaningLong;
  if (v == 50) return Status::Balanced;
  if (v > 35) return Status::LeaningShort;
  return Status::CrowdedShort;
}
QString statusText(Status s) {
  switch (s) {
    case Status::CrowdedLong: return QCoreApplication::translate("positioning", "CROWDED LONG");
    case Status::LeaningLong: return QCoreApplication::translate("positioning", "LEANING LONG");
    case Status::Balanced: return QCoreApplication::translate("positioning", "BALANCED");
    case Status::LeaningShort: return QCoreApplication::translate("positioning", "LEANING SHORT");
    case Status::CrowdedShort: return QCoreApplication::translate("positioning", "CROWDED SHORT");
  }
  return {};
}
QRectF drawStatusBadge(QPainter& p, qreal rightX, qreal cy, Status s, qreal h) {
  const bool lng = s == Status::CrowdedLong || s == Status::LeaningLong;
  const bool sht = s == Status::CrowdedShort || s == Status::LeaningShort;
  // measured on the Retail frames: text #6fbcc8 / #c66a62, fills #071f22 / #3f200d, borders #1f5a63 / #5a2a17
  const QColor fg = lng ? theme::map(0x6f, 0xbc, 0xc8) : sht ? theme::map(0xc6, 0x6a, 0x62) : theme::map(0x7a, 0x76, 0x6f);
  const QColor bg = lng ? theme::map(0x07, 0x1f, 0x22) : sht ? theme::map(0x3f, 0x20, 0x0d) : theme::map(0, 0, 0, 0);
  const QColor bd = lng ? theme::map(0x1f, 0x5a, 0x63) : sht ? theme::map(0x5a, 0x2a, 0x17) : theme::map(0x2a, 0x28, 0x1c);
  QFont f = theme::condensed(int(std::round(h * 0.63)), QFont::Bold);
  f.setLetterSpacing(QFont::AbsoluteSpacing, 0.2);
  const QFontMetricsF fm(f);
  const QString t = statusText(s);
  const qreal icon = 8, w = 5 + icon + 3 + fm.horizontalAdvance(t) + 5;
  const QRectF r(rightX - w, cy - h / 2, w, h);
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(bd, 1)); p.setBrush(bg); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
  p.setPen(QPen(fg, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  const qreal ix = r.left() + 5, iy = cy;
  if (lng) {
    QPolygonF z{{ix, iy + 2.5}, {ix + 3, iy}, {ix + 5, iy + 1.5}, {ix + 8.5, iy - 2.5}};
    p.drawPolyline(z); p.drawLine(QPointF(ix + 8.5, iy - 2.5), QPointF(ix + 6, iy - 2.5));
    p.drawLine(QPointF(ix + 8.5, iy - 2.5), QPointF(ix + 8.5, iy));
  } else if (sht) {
    QPolygonF z{{ix, iy - 2.5}, {ix + 3, iy}, {ix + 5, iy - 1.5}, {ix + 8.5, iy + 2.5}};
    p.drawPolyline(z); p.drawLine(QPointF(ix + 8.5, iy + 2.5), QPointF(ix + 6, iy + 2.5));
    p.drawLine(QPointF(ix + 8.5, iy + 2.5), QPointF(ix + 8.5, iy));
  } else {
    p.drawLine(QPointF(ix + 1, iy), QPointF(ix + 7.5, iy));
  }
  p.setFont(f); p.setPen(fg);
  p.drawText(QPointF(ix + icon + 3, cy + fm.capHeight() / 2), t);
  p.restore();
  return r;
}

// ---------------------------------------------------------------- 3D pie / donut
namespace {
QPointF onEllipse(const QPointF& c, qreal rx, qreal ry, double deg) {
  const double a = qDegreesToRadians(deg);
  return {c.x() + rx * std::cos(a), c.y() - ry * std::sin(a)};
}
QPainterPath sectorTop(const QPointF& c, qreal rx, qreal ry, double a0, double a1) {
  QPainterPath path; path.moveTo(c);
  const int n = std::max(2, int(std::ceil((a1 - a0) / 2.0)));
  for (int i = 0; i <= n; ++i) path.lineTo(onEllipse(c, rx, ry, a0 + (a1 - a0) * i / n));
  path.closeSubpath();
  return path;
}
QPainterPath wallBand(const QPointF& c, qreal rx, qreal ry, qreal depth, double a0, double a1) {
  QPainterPath path;
  const int n = std::max(2, int(std::ceil((a1 - a0) / 2.0)));
  for (int i = 0; i <= n; ++i) {
    const QPointF q = onEllipse(c, rx, ry, a0 + (a1 - a0) * i / n);
    if (i == 0) path.moveTo(q); else path.lineTo(q);
  }
  for (int i = n; i >= 0; --i) path.lineTo(onEllipse(c, rx, ry, a0 + (a1 - a0) * i / n) + QPointF(0, depth));
  path.closeSubpath();
  return path;
}
// intersections of [a0,a1] with the visible-front ranges (180..360 mod 360)
QList<QPair<double, double>> frontParts(double a0, double a1) {
  QList<QPair<double, double>> out;
  for (int k = -2; k <= 2; ++k) {
    const double f0 = 180 + 360.0 * k, f1 = 360 + 360.0 * k;
    const double s = std::max(a0, f0), e = std::min(a1, f1);
    if (e > s + 0.01) out << qMakePair(s, e);
  }
  return out;
}
QList<QPair<double, double>> backParts(double a0, double a1) {
  QList<QPair<double, double>> out;
  for (int k = -2; k <= 2; ++k) {
    const double f0 = 360.0 * k, f1 = 180 + 360.0 * k;
    const double s = std::max(a0, f0), e = std::min(a1, f1);
    if (e > s + 0.01) out << qMakePair(s, e);
  }
  return out;
}
}

QPointF pieLabelPoint(const QPointF& c, qreal rx, qreal ry, double longPct, bool longSlice, qreal frac) {
  const double L = qBound(0.0, longPct / 100.0, 1.0);
  const double mid = longSlice ? 90 - 180 * L : 90 + 180 * (1 - L);
  return onEllipse(c, rx * frac, ry * frac, mid);
}

void drawPie3D(QPainter& p, const QPointF& c, qreal rx, qreal ry, qreal depth, double longPct, double hole, qreal shadowA) {
  if (!std::isfinite(longPct)) return;
  const double L = qBound(0.0, longPct / 100.0, 1.0);
  struct Slice { double a0, a1; QColor topA, topB, wallA, wallB; };
  const QList<Slice> slices{
    {90 - 360 * L, 90, theme::map(0x2c, 0xf2, 0xfb), theme::map(0x14, 0xbf, 0xd2), theme::map(0x0d, 0x83, 0x8e), theme::map(0x07, 0x4f, 0x57)},
    {90, 450 - 360 * L, theme::map(0xf8, 0x74, 0x72), theme::map(0xe2, 0x56, 0x58), theme::map(0xa8, 0x3d, 0x3e), theme::map(0x6a, 0x22, 0x24)},
  };
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  // soft shadow
  if (shadowA > 0) {
    const QPointF sc = c + QPointF(0, depth + ry * 0.12);
    QRadialGradient g(sc, rx * 1.1);
    g.setColorAt(0, theme::map(0, 0, 0, int(170 * shadowA))); g.setColorAt(0.7, theme::map(0, 0, 0, int(70 * shadowA))); g.setColorAt(1, theme::map(0, 0, 0, 0));
    p.save(); p.translate(sc); p.scale(1.0, ry / rx); p.translate(-sc);
    p.setPen(Qt::NoPen); p.setBrush(g); p.drawEllipse(sc, rx * 1.1, rx * 1.1);
    p.restore();
  }
  // outer side walls (front half)
  for (const auto& s : slices) {
    if (s.a1 - s.a0 < 0.05) continue;
    for (const auto& [a0, a1] : frontParts(s.a0, s.a1)) {
      QLinearGradient g(c.x() - rx, 0, c.x() + rx, 0);
      g.setColorAt(0, s.wallB); g.setColorAt(0.45, s.wallA); g.setColorAt(1, s.wallB);
      p.setPen(Qt::NoPen); p.setBrush(g); p.drawPath(wallBand(c, rx, ry, depth, a0, a1));
    }
  }
  // slice edges on the front wall
  p.setPen(QPen(theme::map(0, 0, 0, 90), 1));
  for (double a : {90 - 360 * L, 90.0}) {
    const double n = std::fmod(a + 720, 360);
    if (n > 180 && n < 360 && L > 0.001 && L < 0.999) { const QPointF q = onEllipse(c, rx, ry, a); p.drawLine(q, q + QPointF(0, depth)); }
  }
  // top faces
  for (const auto& s : slices) {
    if (s.a1 - s.a0 < 0.05) continue;
    QRadialGradient g(c + QPointF(-rx * 0.25, -ry * 0.45), rx * 1.25);
    g.setColorAt(0, s.topA); g.setColorAt(1, s.topB);
    p.setPen(Qt::NoPen); p.setBrush(g);
    p.drawPath(sectorTop(c, rx, ry, s.a0, s.a1));
  }
  // rim highlight
  p.setBrush(Qt::NoBrush); p.setPen(QPen(theme::map(255, 255, 255, 28), 1)); p.drawEllipse(c, rx - 0.5, ry - 0.5);
  if (hole > 0) {
    const qreal hx = rx * hole, hy = ry * hole;
    QPainterPath top; top.addEllipse(c, hx, hy);
    QPainterPath floor; floor.addEllipse(c + QPointF(0, depth), hx, hy);
    p.setPen(Qt::NoPen); p.setBrush(theme::map(0x07, 0x07, 0x06)); p.drawPath(top);
    const QPainterPath crescent = top.subtracted(floor);
    for (const auto& s : slices) {
      if (s.a1 - s.a0 < 0.05) continue;
      for (const auto& [a0, a1] : backParts(s.a0, s.a1)) {
        QPainterPath sec = sectorTop(c, rx * 2, ry * 2, a0, a1);
        QLinearGradient g(c.x() - hx, 0, c.x() + hx, 0);
        g.setColorAt(0, s.wallB); g.setColorAt(0.5, s.wallA.darker(115)); g.setColorAt(1, s.wallB);
        p.setBrush(g); p.drawPath(crescent.intersected(sec));
      }
    }
    p.setBrush(Qt::NoBrush); p.setPen(QPen(theme::map(0, 0, 0, 120), 1)); p.drawEllipse(c, hx, hy);
  }
  // slice separators on the top face
  if (L > 0.001 && L < 0.999) {
    p.setPen(QPen(theme::map(0, 0, 0, 110), 1.2));
    for (double a : {90 - 360 * L, 90.0}) p.drawLine(onEllipse(c, rx * hole, ry * hole, a), onEllipse(c, rx, ry, a));
  }
  p.restore();
}

// ---------------------------------------------------------------- TitleBar
TitleBar::TitleBar(Icon icon, const QString& title, QWidget* parent, int h) : QWidget(parent), m_icon(icon), m_title(title) {
  setFixedHeight(h);
  m_right = new QHBoxLayout(this);
  m_right->setContentsMargins(0, 0, 12, 0); m_right->setSpacing(8);
  m_right->addStretch(1);
}
void TitleBar::setBadge(const QString& t, bool dot) { m_badge = t; m_badgeDot = dot; m_static = QImage(); update(); }
void TitleBar::registerAnimation(Ticker* t) {
  const qreal x = 50 + QFontMetricsF(theme::mono(11, QFont::DemiBold, 1.8)).horizontalAdvance(m_title);
  t->add(this, QRect(30, 0, 18, height()));
  t->addCursor(this, QRect(int(x) - 4, 0, 16, height()));
  t->add(this, [this] { return m_badgeDot ? m_badgeRect : QRect(30, 0, 1, 1); });
}
// The ticker repaints the two pulse dots 15 times a second and the cursor when it blinks: what does not move (gradient,
// icon, title, badge) is drawn once into m_static (opaque), the dots and the cursor over it. Neither dot overlaps a
// text (title from x = 50, badge text 7 px right of the badge dot's widest glow).
void TitleBar::paintEvent(QPaintEvent*) {
  const qreal cy = height() / 2.0 - 0.5;
  const QFont f = theme::mono(11, QFont::DemiBold, 1.8);   // measured: 39 caps over 328 px at 1600 wide
  const QFontMetricsF fm(f);
  const QFont bf = theme::mono(9, QFont::Bold, 1.2);
  const QFontMetricsF bm(bf);
  const qreal dpr = devicePixelRatioF();
  const bool rebuild = m_static.isNull() || m_static.size() != size() * dpr || m_static.devicePixelRatio() != dpr;
  if (rebuild) { m_titleW = fm.horizontalAdvance(m_title); m_badgeW = bm.horizontalAdvance(m_badge); }   // measured per layout, not per frame
  const qreal bw = m_badgeW + (m_badgeDot ? 22 : 14);
  const QRectF br(width() - 14 - bw, cy - 9.5, bw, 19);
  if (rebuild) {
    m_static = QImage(size() * dpr, QImage::Format_ARGB32_Premultiplied);
    m_static.setDevicePixelRatio(dpr);
    QPainter q(&m_static);
    QLinearGradient g(0, 0, width(), 0);
    g.setColorAt(0, theme::map(0x10, 0x09, 0x02)); g.setColorAt(1, theme::map(0x1e, 0x17, 0x05));
    q.fillRect(rect(), g);
    q.fillRect(QRect(0, height() - 1, width(), 1), theme::map(0x2e, 0x28, 0x18));
    drawIcon(q, m_icon, QRectF(12, cy - 7, 14, 14), theme::map(0xcb, 0xbf, 0x96));
    q.setFont(f); q.setPen(theme::map(0x63, 0x5f, 0x55));
    q.drawText(QPointF(50, cy + fm.capHeight() / 2), m_title);
    if (!m_badge.isEmpty()) {
      q.setRenderHint(QPainter::Antialiasing);
      q.setPen(QPen(theme::map(0x7a, 0x6a, 0x1a), 1)); q.setBrush(theme::map(0x3a, 0x30, 0x04));
      q.drawRoundedRect(br.adjusted(.5, .5, -.5, -.5), 2, 2);
      q.setFont(bf); q.setPen(theme::map(0xf6, 0xe2, 0x9a));
      q.drawText(QPointF(br.left() + 7 + (m_badgeDot ? 8 : 0), cy + bm.capHeight() / 2), m_badge);
    }
  }
  QPainter p(this);
  p.drawImage(QPointF(0, 0), m_static);
  drawPulseDot(p, QPointF(38, cy), 2.6, m_pulse);
  const qreal x = 50 + m_titleW - 1;
  drawBlockCursor(p, QRectF(x, cy - 6.5, 6.5, 12.5));
  if (!m_badge.isEmpty() && m_badgeDot) {
    p.setRenderHint(QPainter::Antialiasing);
    drawPulseDot(p, QPointF(br.left() + 8, cy), 1.8, m_pulse);
    m_badgeRect = QRect(int(br.left()) + 2, 0, 14, height());
  }
}

// ---------------------------------------------------------------- Footer
Footer::Footer(QWidget* parent, int h) : QWidget(parent) { setFixedHeight(h); setMouseTracking(true); }
void Footer::setBadge(const QString& t, Kind k, const QString& tip) { m_badge = t; m_kind = k; m_tip = tip; update(); }
bool Footer::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip && (!m_tip.isEmpty() || !m_note.isEmpty())) {
    auto* he = static_cast<QHelpEvent*>(e);
    if (he->pos().x() > width() - 160) QToolTip::showText(he->globalPos(), m_tip, this);
    else if (!m_note.isEmpty()) QToolTip::showText(he->globalPos(), m_note, this);   // the note may be elided
    else QToolTip::hideText();
    return true;
  }
  return QWidget::event(e);
}
void Footer::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), col::kPage());
  p.fillRect(QRect(0, 0, width(), 1), theme::map(0x1e, 0x1a, 0x0f));
  const qreal cy = height() / 2.0 + 0.5;
  const QFont f = theme::condensed(12, QFont::Medium);
  const QFontMetricsF fm(f);
  p.setFont(f); p.setPen(theme::map(0x57, 0x55, 0x4e));
  const QString left = QCoreApplication::translate("positioning", "Terminal Data · %1").arg(m_time);
  p.drawText(QPointF(7, cy + fm.capHeight() / 2), left);
  if (!m_note.isEmpty()) {
    p.setPen(theme::map(0x45, 0x42, 0x3b));
    const qreal nx = 7 + fm.horizontalAdvance(left) + 14;
    p.drawText(QPointF(nx, cy + fm.capHeight() / 2), fm.elidedText(m_note, Qt::ElideRight, std::max<qreal>(0, width() - nx - 170)));
  }
  if (m_badge.isEmpty()) return;
  QFont bf = theme::condensed(12, QFont::Bold); bf.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
  const QFontMetricsF bm(bf);
  const qreal w = bm.horizontalAdvance(m_badge) + 20;
  const QRectF r(width() - 7 - w, cy - 10, w, 20);
  QColor fg, bg, bd, dot;
  if (m_kind == Delayed) { fg = theme::map(0xf3, 0xdc, 0x8a); bg = theme::map(0x35, 0x2a, 0x00); bd = theme::map(0x3f, 0x37, 0x06); dot = theme::map(0xf0, 0xd0, 0x4e); }
  else if (m_kind == Official) { fg = theme::map(0x78, 0xcc, 0xd4); bg = theme::map(0x10, 0x24, 0x1c); bd = theme::map(0x27, 0x3b, 0x38); dot = theme::map(0x4f, 0xd8, 0xe4); }
  else { fg = theme::map(0x9e, 0x99, 0x90); bg = theme::map(0x18, 0x15, 0x10); bd = theme::map(0x2b, 0x28, 0x1e); dot = theme::map(0x7e, 0x7a, 0x70); }
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(bd, 1)); p.setBrush(bg); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
  p.setPen(Qt::NoPen); p.setBrush(dot); p.drawEllipse(QPointF(r.left() + 7, cy), 2.3, 2.3);
  p.setFont(bf); p.setPen(fg); p.drawText(QPointF(r.left() + 12.5, cy + bm.capHeight() / 2), m_badge);
}

// ---------------------------------------------------------------- Segmented
Segmented::Segmented(const QStringList& items, Style s, const QFont& f, QWidget* parent, bool frame)
    : QWidget(parent), m_items(items), m_style(s), m_font(f), m_frame(frame) {
  setMouseTracking(true); setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
void Segmented::setItems(const QStringList& items) { m_items = items; m_cur = qBound(0, m_cur, int(items.size()) - 1); updateGeometry(); update(); }
void Segmented::setCurrent(int i) { if (i != m_cur) { m_cur = i; update(); } }
QList<QRectF> Segmented::rects() const {
  QList<QRectF> out;
  const QFontMetricsF fm(m_font);
  const qreal pad = m_frame ? 3 : 0;
  qreal x = pad;
  for (const auto& t : m_items) {
    const qreal w = fm.horizontalAdvance(t) + 2 * m_padH;
    out << QRectF(x, pad, w, m_itemH);
    x += w + m_gap;
  }
  return out;
}
QSize Segmented::sizeHint() const {
  const auto r = rects();
  const qreal pad = m_frame ? 3 : 0;
  return QSize(int(std::ceil((r.isEmpty() ? 0 : r.last().right()) + pad)), int(m_itemH + 2 * pad));
}
void Segmented::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  if (m_frame) {
    p.setPen(QPen(col::kBorderSubtle(), 1)); p.setBrush(col::kInput());
    p.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 2, 2);
  }
  const auto rs = rects();
  const QFontMetricsF fm(m_font);
  for (int i = 0; i < rs.size(); ++i) {
    const bool on = i == m_cur, hov = i == m_hover;
    QColor fg = hov ? theme::map(0xd8, 0xd3, 0xc8) : m_inactive;
    QFont f = m_font;
    if (on) {
      if (m_style == Yellow) { p.setPen(Qt::NoPen); p.setBrush(col::kYellow()); p.drawRoundedRect(rs[i].adjusted(m_inset, 0, -m_inset, 0), 2.5, 2.5); fg = theme::isDefault() ? QColor(0x0c, 0x06, 0x01) : theme::onAccent(); f.setWeight(QFont::Bold); }   // measured pill text in Deepwater
      else if (m_style == Gold) { p.setPen(QPen(theme::map(0x43, 0x38, 0x1e), 1)); p.setBrush(theme::map(0x38, 0x30, 0x04)); p.drawRoundedRect(rs[i].adjusted(.5, .5, -.5, -.5), 2, 2); fg = theme::map(0xdf, 0xcc, 0x80); }
      else { p.setPen(Qt::NoPen); p.setBrush(theme::map(0x24, 0x20, 0x20)); p.drawRoundedRect(rs[i], 2, 2); fg = theme::map(0xe8, 0xe3, 0xda); }
    }
    p.setFont(f); p.setPen(fg);
    p.drawText(rs[i], Qt::AlignCenter, m_items[i]);
  }
}
void Segmented::mousePressEvent(QMouseEvent* e) {
  const auto rs = rects();
  for (int i = 0; i < rs.size(); ++i)
    if (rs[i].adjusted(-2, -3, 2, 3).contains(e->position()) && i != m_cur) { m_cur = i; update(); emit changed(i); return; }
}
void Segmented::mouseMoveEvent(QMouseEvent* e) {
  const auto rs = rects(); int h = -1;
  for (int i = 0; i < rs.size(); ++i) if (rs[i].contains(e->position())) h = i;
  if (h != m_hover) { m_hover = h; update(); }
}
void Segmented::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ---------------------------------------------------------------- Scroll
Scroll::Scroll(QWidget* parent) : QScrollArea(parent) {
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidgetResizable(true);
  viewport()->setAutoFillBackground(false);
  setStyleSheet("QScrollArea{background:transparent;border:none;}");
  verticalScrollBar()->setSingleStep(40);
}
}
