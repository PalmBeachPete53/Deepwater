#include "pages/economy/EcoKit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QScrollArea>
#include <QScrollBar>
#include <cmath>

namespace economy {

namespace eco {
using theme::mix;
static const theme::Palette& P() { return theme::pal(); }

QColor pageBg() { return theme::map(mix(QColor("#060402"), QColor(0, 0, 0), 0.2)); }   // Deepwater value, mapped
QColor boxBorder() { return mix(P().bg, P().border, 0.55); }
QColor boxBorderHi() { return mix(P().bg, P().border, 0.95); }
QColor hair() { return mix(P().bg, P().border, 0.45); }
QColor amber() { return mix(P().accent, theme::map("#f5c518"), 1.0); }
QColor amberFill() { return P().accent; }
QColor amberText() { return mix(P().accent, P().accentText, 0.15).darker(108); }
QColor amberPale() { return mix(P().accent, P().text, 0.45); }
QColor amberDimFill() { return mix(P().bg, P().accent, 0.2); }
QColor onAmber() { return theme::onAccent(); }
QColor txt() { return mix(P().text, P().textDim, 0.08); }
QColor txtBody() { return mix(P().text, P().textDim, 0.75); }
QColor txtSec() { return mix(P().textDim, P().textMute, 0.5); }
QColor txtMute() { return mix(P().textMute, P().bg, 0.1); }
QColor txtFaint() { return mix(P().textMute, P().bg, 0.45); }
QColor cyan() { return theme::map("#22d3e0"); }
QColor red() { return theme::map("#f05a50"); }
QColor grey() { return theme::map("#aab1ba"); }
QColor outcome(int b) {
  static const char* const c[5] = {"#912922", "#c84d48", "#aab1ba", "#00c2bd", "#00aa7c"};   // Deepwater, mapped at use
  return theme::map(c[qBound(0, b, 4)]);
}
QColor semantic(int goodWhenUp, double change) {
  if (!std::isfinite(change) || std::fabs(change) < 1e-12 || goodWhenUp == 0) return theme::map("#8f8c86");
  return (change > 0) == (goodWhenUp > 0) ? theme::map("#4edeea") : theme::map("#f27b7e");
}

QFont mono(int px, int w, qreal ls) { return theme::mono(px, w, ls); }
QFont sans(int px, int w) { return theme::sans(px, w); }
QFont cond(int px, int w) { return theme::condensed(px, w); }
QFont display(int px, int w, qreal ls) { return theme::display(px, w, ls); }

void sectionHeading(QPainter& p, qreal x, qreal yMid, qreal right, const QString& text, int px) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen); p.setBrush(amber());
  p.drawEllipse(QPointF(x + 2.5, yMid), 2.6, 2.6);
  const QFont f = mono(px, QFont::Bold, 2.0);
  p.setFont(f); p.setPen(amberText());
  const QFontMetricsF fm(f);
  p.drawText(QPointF(x + 11, yMid + fm.capHeight() / 2), text);
  const qreal tx = x + 11 + fm.horizontalAdvance(text) + 10;
  QLinearGradient g(tx, 0, right, 0);
  g.setColorAt(0, mix(P().bg, P().accent, 0.25)); g.setColorAt(1, mix(P().bg, P().border, 0.35));
  p.fillRect(QRectF(tx, std::floor(yMid), right - tx, 1), g);
  p.restore();
}

qreal micro(QPainter& p, qreal x, qreal yBase, const QString& text, const QColor& c, int px, qreal ls) {
  const QFont f = mono(px, QFont::Normal, ls);
  p.setFont(f); p.setPen(c);
  p.drawText(QPointF(x, yBase), text);
  return QFontMetricsF(f).horizontalAdvance(text);
}

QRectF tooltip(QPainter& p, const QPointF& cur, const QString& title, const QList<TipRow>& rows, const QRectF& bounds,
               bool square, QPointF off) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  const QFont ft = mono(11), fv = mono(11, QFont::Bold), fl = mono(11, QFont::Bold);
  const QFontMetricsF mt(ft), mv(fv), ml(fl);
  qreal wLab = 0, wVal = 0;
  for (const auto& r : rows) { wLab = std::max(wLab, ml.horizontalAdvance(r.label)); wVal = std::max(wVal, mv.horizontalAdvance(r.value)); }
  const qreal sw = 8, gap = 10;
  qreal w = std::max(mt.horizontalAdvance(title), sw + 6 + wLab + (wLab > 0 ? gap : 0) + wVal) + 22;
  const qreal lh = 20;
  const qreal h = 10 + (title.isEmpty() ? 0 : lh) + rows.size() * lh + 8;
  QRectF box(cur.x() + off.x(), cur.y() + off.y(), w, h);
  if (box.right() > bounds.right() - 4) box.moveRight(cur.x() - off.x());
  if (box.bottom() > bounds.bottom() - 4) box.moveBottom(cur.y() - 6);
  if (box.top() < bounds.top() + 2) box.moveTop(bounds.top() + 2);
  if (box.left() < bounds.left() + 2) box.moveLeft(bounds.left() + 2);
  p.setPen(QPen(theme::alpha(mix(P().accent, P().bg, 0.45), 200), 1));
  p.setBrush(theme::map(8, 5, 2, 245));
  p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
  qreal y = box.top() + 10;
  if (!title.isEmpty()) {
    p.setFont(ft); p.setPen(theme::map("#9b9b98"));
    p.drawText(QPointF(box.left() + 11, y + mt.ascent()), title);
    y += lh;
  }
  for (const auto& r : rows) {
    const qreal cy = y + lh / 2 - 2;
    p.setPen(Qt::NoPen); p.setBrush(r.swatch);
    if (square) p.drawRect(QRectF(box.left() + 11, cy - 4, 8, 8));
    else p.drawEllipse(QPointF(box.left() + 15, cy), 4, 4);
    qreal x = box.left() + 11 + sw + 6;
    if (!r.label.isEmpty()) {
      p.setFont(fl); p.setPen(theme::map("#e1dfd9"));
      p.drawText(QPointF(x, cy + ml.capHeight() / 2), r.label);
      x = box.left() + 11 + sw + 6 + wLab + gap;
    }
    p.setFont(fv); p.setPen(theme::map("#f5f2eb"));
    p.drawText(QPointF(x, cy + mv.capHeight() / 2), r.value);
    y += lh;
  }
  p.restore();
  return box;
}

void button(QPainter& p, const QRectF& r, const QString& text, bool sel, bool hover, int px, bool enabled) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  if (sel) { p.setPen(Qt::NoPen); p.setBrush(amberFill()); p.drawRoundedRect(r, 2, 2); }
  p.setFont(mono(px, QFont::Bold, 0.6));
  p.setPen(sel ? onAmber() : !enabled ? txtFaint() : hover ? amber() : theme::map("#8a8781"));
  p.drawText(r, Qt::AlignCenter, text);
  p.restore();
}

void pill(QPainter& p, const QRectF& r, const QString& text, const QColor& fill, const QColor& fg, bool dot, qreal radius) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen); p.setBrush(fill); p.drawRoundedRect(r, radius, radius);
  const QFont f = mono(9, QFont::Bold, 0.4);
  p.setFont(f); p.setPen(fg);
  if (dot) {
    const qreal tw = QFontMetricsF(f).horizontalAdvance(text);
    const qreal x0 = r.center().x() - (tw + 7) / 2;
    p.setBrush(fg); p.setPen(Qt::NoPen); p.drawEllipse(QPointF(x0 + 2, r.center().y()), 2, 2);
    p.setPen(fg); p.drawText(QRectF(x0 + 7, r.top(), tw + 2, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text);
  } else p.drawText(r, Qt::AlignCenter, text);
  p.restore();
}

void bracket(QPainter& p, const QRectF& r, const QColor& c) {
  p.save();
  p.setPen(QPen(c, 1.2));
  const qreal x = r.left() + 5, y = r.top() + 5;
  p.drawLine(QPointF(x, y), QPointF(x + 8, y));
  p.drawLine(QPointF(x, y), QPointF(x, y + 8));
  p.restore();
}

QString pctTrim(double v, int maxDec) {
  if (!std::isfinite(v)) return "—";
  QString s = QString::number(v, 'f', maxDec);
  if (s.contains('.')) { while (s.endsWith('0')) s.chop(1); if (s.endsWith('.')) s.chop(1); }
  if (s == "-0") s = "0";
  return s + "%";
}
QString probText(double pr) {
  if (!std::isfinite(pr)) return "—";
  if (pr < 0.01) return "<1%";
  return QString::number(pr * 100, 'f', 1) + "%";
}
QString signedBp(double bp) {
  if (!std::isfinite(bp)) return "—";
  if (std::fabs(bp) < 0.05) bp = 0;
  return (bp >= 0 ? "+" : "") + QString::number(bp, 'f', 1);
}

// ---- blink
Blink& Blink::instance() { static Blink* b = new Blink; return *b; }
Blink::Blink() {
  m_timer.setInterval(500);
  connect(&m_timer, &QTimer::timeout, this, [this] { ++m_phase; m_on = !m_on; emit tick(); });
}
void Blink::hold(QObject* who, bool visible) {
  m_holders.removeAll(who);
  if (visible) m_holders << who;
  if (m_holders.isEmpty()) m_timer.stop(); else if (!m_timer.isActive()) m_timer.start();
}

void setupScroll(QScrollArea* a) {
  a->setFrameShape(QFrame::NoFrame);
  a->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  a->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  a->verticalScrollBar()->setSingleStep(34);
  a->setWidgetResizable(true);
}
}
}  // namespace economy
