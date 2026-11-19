#include "pages/global/session-recap/RecapWidgets.h"
#include "ui/Format.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include "core/I18n.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>
#include <QUrl>
#include <QVariantAnimation>

namespace wire::recap {
namespace {
inline QColor kPos() { return theme::map("#8ec4b4"); }
inline QColor kNeg() { return theme::map("#cc9a99"); }
constexpr int kDayH = 22, kRowH = 51;
}

// ============================================================================================ SmoothScroll
SmoothScroll::SmoothScroll(QWidget* parent) : QAbstractScrollArea(parent), m_anim(new QVariantAnimation(this)) {
  setFrameShape(QFrame::NoFrame);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  viewport()->setMouseTracking(true);
  m_anim->setDuration(150); m_anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { verticalScrollBar()->setValue(v.toInt()); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(), [this] { viewport()->update(); });
}
int SmoothScroll::offset() const { return verticalScrollBar()->value(); }
void SmoothScroll::setContentHeight(int h) {
  m_h = h;
  verticalScrollBar()->setRange(0, qMax(0, h - viewport()->height()));
  verticalScrollBar()->setPageStep(viewport()->height());
}
void SmoothScroll::resizeEvent(QResizeEvent* e) { QAbstractScrollArea::resizeEvent(e); setContentHeight(m_h); }
void SmoothScroll::scrollBy(int dy) {
  const int base = m_anim->state() == QAbstractAnimation::Running ? m_target : verticalScrollBar()->value();
  m_target = qBound(0, base + dy, verticalScrollBar()->maximum());
  m_anim->stop(); m_anim->setStartValue(verticalScrollBar()->value()); m_anim->setEndValue(m_target); m_anim->start();
}
void SmoothScroll::wheelEvent(QWheelEvent* e) {
  if (!e->pixelDelta().isNull()) { m_anim->stop(); verticalScrollBar()->setValue(verticalScrollBar()->value() - e->pixelDelta().y()); }
  else scrollBy(-e->angleDelta().y() * 150 / 120);   // reference: ≈150 px per wheel step
  e->accept();
}

void columnHeader(QPainter& p, const QRect& r, const QString& title, const QString& right) {
  p.fillRect(r, theme::map("#140f04"));
  p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), theme::map("#3a3214"));
  const QFont fr = theme::mono(10, QFont::Normal, 1);
  const qreal room = r.width() - 26 - (right.isEmpty() ? 0 : QFontMetricsF(fr).horizontalAdvance(right) + 10);
  // letter spacing tightens before the title is elided (long French titles in narrow columns)
  QFont ft = theme::mono(10, QFont::Normal, 3.6);
  if (QFontMetricsF(ft).horizontalAdvance(title) > room) ft = theme::mono(10, QFont::Normal, 1.6);
  p.setFont(ft); p.setPen(theme::map("#5b5750"));
  p.drawText(r.adjusted(13, 0, -13, 0), Qt::AlignVCenter | Qt::AlignLeft, QFontMetricsF(ft).elidedText(title, Qt::ElideRight, room));
  p.setFont(fr); p.setPen(theme::map("#4e4a42"));
  p.drawText(r.adjusted(13, 0, -12, 0), Qt::AlignVCenter | Qt::AlignRight, right);
}

// ============================================================================================ RegistryHead
RegistryHead::RegistryHead(QWidget* parent) : QWidget(parent) { setFixedHeight(71); setMouseTracking(true); }
void RegistryHead::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), theme::map("#050402"));
  p.fillRect(QRect(0, 3, width(), 31), theme::map("#221b04"));
  p.fillRect(QRect(0, 34, width(), height() - 35), theme::map("#151202"));
  p.fillRect(QRect(0, 33, width(), 1), theme::map("#3a3214"));
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#c9a72a")); p.drawEllipse(QPointF(17, 18.5), 2.6, 2.6);
  p.setFont(theme::mono(11, QFont::Normal, 3)); p.setPen(theme::map("#6e6852"));
  const QString title = QCoreApplication::translate("wire", "RECAP REGISTRY");
  p.drawText(QPointF(29, 23), title);
  const qreal x = 29 + QFontMetricsF(p.font()).horizontalAdvance(title) + 2;
  if (m_blink) p.fillRect(QRectF(x, 11, 7, 14), theme::map("#f5e392"));
  const int half = width() / 2;
  const QStringList t{QCoreApplication::translate("wire", "SESSIONS"), QCoreApplication::translate("wire", "WEEKS")};
  for (int i = 0; i < 2; ++i) {
    const QRect r(i * half, 37, half, 31);
    const bool on = i == m_tab;
    p.setFont(theme::mono(12, on ? QFont::Bold : QFont::Normal, 3));
    p.setPen(on ? theme::map("#e6dfbc") : m_hover == i ? theme::map("#8e8a78") : theme::map("#5a5749"));
    p.drawText(r.adjusted(0, -4, 0, 0), Qt::AlignCenter, t[i]);
    if (on) p.fillRect(QRect(r.left() + 10, 63, r.width() - 20, 3), theme::map("#dbc677"));
  }
  p.fillRect(QRect(0, height() - 1, width(), 1), theme::map("#14110a"));
}
void RegistryHead::mousePressEvent(QMouseEvent* e) {
  if (e->position().y() < 37) return;
  const int t = e->position().x() < width() / 2 ? 0 : 1;
  if (t != m_tab) { m_tab = t; update(); emit tabChanged(t); }
}
void RegistryHead::mouseMoveEvent(QMouseEvent* e) {
  const int h = e->position().y() >= 37 ? (e->position().x() < width() / 2 ? 0 : 1) : -1;
  if (h != m_hover) { m_hover = h; update(); setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); }
}
void RegistryHead::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ============================================================================================ RegistryList
RegistryList::RegistryList(QWidget* parent) : SmoothScroll(parent) {}
void RegistryList::setRows(const QList<Row>& rows) {
  m_rows = rows;
  m_y.resize(rows.size() + 1);
  int y = 0;
  for (int i = 0; i < rows.size(); ++i) { m_y[i] = y; y += rows[i].header ? kDayH : kRowH; }
  m_y[rows.size()] = y;
  setContentHeight(y);
  viewport()->update();
}
void RegistryList::setSelected(const QString& k) { m_sel = k; viewport()->update(); }
int RegistryList::rowAt(int y) const {
  if (m_rows.isEmpty() || y < 0 || y >= m_y.last()) return -1;
  return int(std::upper_bound(m_y.begin(), m_y.end() - 1, y) - m_y.begin()) - 1;
}
void RegistryList::paintEvent(QPaintEvent* e) {
  QPainter p(viewport());
  p.fillRect(e->rect(), theme::map("#050402"));
  const int top = offset(), w = viewport()->width();
  if (m_rows.isEmpty()) {
    p.setFont(theme::mono(10, QFont::Normal, 1)); p.setPen(theme::map("#504e46"));
    p.drawText(viewport()->rect().adjusted(16, 30, -16, 0), Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, QCoreApplication::translate("wire", "Loading 15-minute bars…"));
    return;
  }
  for (int i = qMax(0, rowAt(top + e->rect().top())); i >= 0 && i < m_rows.size() && m_y[i] - top <= e->rect().bottom(); ++i) {
    const auto& r = m_rows[i];
    const int y0 = m_y[i] - top;
    if (r.header) {
      p.fillRect(QRect(0, y0, w, kDayH), theme::map("#1a1506"));
      p.setFont(theme::mono(10, QFont::Normal, 2.4)); p.setPen(theme::map("#504e46"));
      p.drawText(QRect(8, y0, w - 16, kDayH), Qt::AlignVCenter | Qt::AlignLeft, r.label);
      continue;
    }
    const bool sel = r.w.key() == m_sel;
    if (sel) p.fillRect(QRect(0, y0, w, kRowH), theme::map("#433805"));
    else if (i == m_hover) p.fillRect(QRect(0, y0, w, kRowH), theme::map("#1a1503"));
    p.fillRect(QRect(0, y0 + kRowH - 1, w, 1), theme::map("#110f06"));
    qreal tx = 10;
    if (!r.w.week) { p.drawPixmap(QPointF(9, y0 + 19), ui::flag(r.w.flag(), QSize(20, 13), devicePixelRatioF())); tx = 37; }
    QFont fn = theme::condensed(14, QFont::Bold); fn.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    p.setFont(fn); p.setPen(theme::map("#fefae8"));
    p.drawText(QPointF(tx, y0 + 23), r.label);
    if (r.w.live()) {
      const qreal lx = tx + QFontMetricsF(fn).horizontalAdvance(r.label) + 8;
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(Qt::NoPen); p.setBrush(theme::map("#382e08")); p.drawRoundedRect(QRectF(lx, y0 + 11, 34, 14), 2, 2);
      p.setFont(theme::mono(8, QFont::Bold, 0.8)); p.setPen(theme::map("#e8d491"));
      p.drawText(QRectF(lx, y0 + 11, 34, 14), Qt::AlignCenter, "LIVE");
    }
    p.setFont(theme::mono(10)); p.setPen(sel ? (theme::isDark() ? theme::map("#8b7d3c") : theme::roles().textDim) : theme::map("#5f5631"));
    // elided before the change column (narrow windows: "Pétrole WTI" was cut mid-glyph); full line in the tooltip
    p.drawText(QRectF(tx, y0 + 29, w - tx - 80, 16), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetricsF(p.font()).elidedText(r.line2, Qt::ElideRight, w - tx - 80));
    p.setFont(theme::mono(13, QFont::DemiBold));
    p.setPen(std::isfinite(r.chg) ? (r.chg >= 0 ? kPos() : kNeg()) : theme::map("#5f5d57"));
    p.drawText(QRect(w - 120, y0, 112, kRowH), Qt::AlignVCenter | Qt::AlignRight, std::isfinite(r.chg) ? fmt::pct(r.chg) : fmt::dash());
  }
}
void RegistryList::mouseMoveEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()) + offset());
  const int h = r >= 0 && !m_rows[r].header ? r : -1;
  if (h != m_hover) {
    m_hover = h; viewport()->update(); viewport()->setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QString tip;
    if (h >= 0) {   // tooltip only when the second line is elided
      const qreal tx = m_rows[h].w.week ? 10 : 37;
      if (QFontMetricsF(theme::mono(10)).horizontalAdvance(m_rows[h].line2) > viewport()->width() - tx - 80) tip = m_rows[h].line2;
    }
    viewport()->setToolTip(tip);
  }
}
void RegistryList::mousePressEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()) + offset());
  if (r >= 0 && !m_rows[r].header) { m_sel = m_rows[r].w.key(); viewport()->update(); emit picked(m_rows[r].w); }
}
void RegistryList::leaveEvent(QEvent*) { m_hover = -1; viewport()->update(); }

// ============================================================================================ ReleasesBox
ReleasesBox::ReleasesBox(QWidget* parent) : SmoothScroll(parent) {}
void ReleasesBox::setData(const QList<wire::Release>& r, bool connected) {
  m_rel = r; m_connected = connected;
  setContentHeight(33 + qMax(1, int(r.size())) * 52);
  viewport()->update();
}
int ReleasesBox::preferredHeight() const { return 33 + qMax(1, int(m_rel.size())) * 52 + 4; }
void ReleasesBox::paintEvent(QPaintEvent* e) {
  QPainter p(viewport());
  p.fillRect(e->rect(), theme::map("#0d0906"));
  const int top = offset(), w = viewport()->width();
  p.setRenderHint(QPainter::Antialiasing);
  if (m_rel.isEmpty()) {
    p.setFont(theme::mono(10, QFont::Normal, 0.5)); p.setPen(theme::map("#504e46"));
    p.drawText(QRect(13, 33 - top, w - 26, 52), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
               m_connected ? QCoreApplication::translate("wire", "No medium or high impact release in this window.") : QCoreApplication::translate("wire", "Calendar connecting… releases appear once the News calendar is connected."));
  }
  for (int i = 0; i < m_rel.size(); ++i) {
    const int y0 = 33 + i * 52 - top;
    if (y0 > e->rect().bottom() || y0 + 52 < 0) continue;
    const auto& r = m_rel[i];
    p.fillRect(QRect(0, y0 + 51, w, 1), theme::map("#14120c"));
    p.setFont(theme::mono(12)); p.setPen(theme::map("#5f5d57"));
    p.drawText(QPointF(11, y0 + 21), r.time.toLocalTime().toString("HH:mm"));
    p.drawPixmap(QPointF(50, y0 + 10), ui::flag(r.currency, QSize(20, 13), devicePixelRatioF()));
    // impact marker: hollow diamond, gold for high, grey for medium
    const QPointF c(81, y0 + 16.5);
    QPainterPath d; d.moveTo(c + QPointF(0, -3.6)); d.lineTo(c + QPointF(3.6, 0)); d.lineTo(c + QPointF(0, 3.6)); d.lineTo(c + QPointF(-3.6, 0)); d.closeSubpath();
    p.setPen(QPen(r.impact >= 3 ? theme::map("#e8da9b") : theme::map("#3d3f42"), 1.1)); p.setBrush(Qt::NoBrush); p.drawPath(d);
    p.setFont(theme::condensed(15, QFont::Medium)); p.setPen(theme::map("#e9e6d6"));
    p.drawText(QPointF(89, y0 + 21), QFontMetricsF(p.font()).elidedText(r.title, Qt::ElideRight, w - 100));
    qreal x = 79;
    auto part = [&](const QString& k, const QString& v, bool bold) {
      p.setFont(theme::mono(12)); p.setPen(theme::map("#4e4a42")); p.drawText(QPointF(x, y0 + 41), k);
      x += QFontMetricsF(p.font()).horizontalAdvance(k + " ");
      p.setFont(theme::mono(12, bold ? QFont::Bold : QFont::Normal)); p.setPen(bold ? theme::map("#fefdf8") : theme::map("#6a675f"));
      const QString s = v.isEmpty() ? fmt::dash() : v;
      p.drawText(QPointF(x, y0 + 41), s);
      x += QFontMetricsF(p.font()).horizontalAdvance(s) + 12;
    };
    part(QCoreApplication::translate("wire", "A", "actual"), r.actual, true); part(QCoreApplication::translate("wire", "F", "forecast"), r.forecast, false); part(QCoreApplication::translate("wire", "P", "previous"), r.previous, false);
  }
  // header (fixed)
  columnHeader(p, QRect(0, 0, w, 32), QCoreApplication::translate("wire", "RELEASES IN THE WINDOW"), wire::utcLabel());
}

bool ReleasesBox::viewportEvent(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const int i = (he->pos().y() + offset() - 33) / 52;
    if (he->pos().y() + offset() >= 33 && i >= 0 && i < m_rel.size()) {
      const auto& r = m_rel[i];
      auto v = [](const QString& x) { return x.isEmpty() ? fmt::dash() : x; };
      QToolTip::showText(he->globalPos(), QCoreApplication::translate("wire", "%1 %2 · %3\nActual %4 · Forecast %5 · Previous %6").arg(r.currency, r.title,
          i18n::locale().toString(r.time.toLocalTime(), "ddd d MMM HH:mm"), v(r.actual), v(r.forecast), v(r.previous)), viewport());
    } else QToolTip::hideText();
    return true;
  }
  return SmoothScroll::viewportEvent(e);
}

// ============================================================================================ WireBox
WireBox::WireBox(QWidget* parent) : SmoothScroll(parent) {}
void WireBox::setData(const QList<wire::Item>& items, const QString& empty) {
  m_items = items; m_empty = empty; m_hover = -1;
  m_tags.clear();
  m_y.resize(items.size() + 1);
  int y = 36;
  for (int i = 0; i < items.size(); ++i) {
    m_y[i] = y;
    m_tags << assetTag(items[i].text);
    y += m_tags.last().isEmpty() ? 33 : 52;
  }
  m_y[items.size()] = y;
  setContentHeight(y + 8);
  viewport()->update();
}
int WireBox::rowAt(int y) const {
  if (m_items.isEmpty() || y < m_y.first() || y >= m_y.last()) return -1;
  return int(std::upper_bound(m_y.begin(), m_y.end() - 1, y) - m_y.begin()) - 1;
}
void WireBox::paintEvent(QPaintEvent* e) {
  QPainter p(viewport());
  p.fillRect(e->rect(), theme::map("#050402"));
  const int top = offset(), w = viewport()->width();
  if (m_items.isEmpty()) {
    p.setFont(theme::mono(10, QFont::Normal, 0.5)); p.setPen(theme::map("#504e46"));
    p.drawText(QRect(13, 44, w - 26, 80), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, m_empty);
  }
  const QFont ft = theme::mono(12), fh = theme::condensed(15, QFont::Medium);
  const QFontMetricsF fm(fh);
  for (int i = qMax(0, rowAt(top + qMax(36, e->rect().top()))); i >= 0 && i < m_items.size() && m_y[i] - top <= e->rect().bottom(); ++i) {
    const auto& it = m_items[i];
    const int y0 = m_y[i] - top, rh = m_y[i + 1] - m_y[i];
    if (i == m_hover) p.fillRect(QRect(0, y0, w, rh), theme::map("#16120a"));
    p.fillRect(QRect(0, y0 + rh - 1, w, 1), theme::map("#14120c"));
    p.setFont(ft); p.setPen(theme::map("#5f5d57"));
    p.drawText(QPointF(11, y0 + 21), it.time.toLocalTime().toString("HH:mm"));
    p.setFont(fh); p.setPen(theme::map("#e9e6d6"));
    p.drawText(QPointF(49, y0 + 21), fm.elidedText(it.text, Qt::ElideRight, w - 60));
    if (!m_tags[i].isEmpty()) {
      const QFont fc = theme::mono(8, QFont::Bold, 0.8);
      const QRectF r(49, y0 + 30, QFontMetricsF(fc).horizontalAdvance(m_tags[i]) + 12, 14);
      p.setPen(QPen(theme::map("#2e2a1e"), 1)); p.setBrush(theme::map("#0e0b06")); p.drawRect(r.adjusted(.5, .5, -.5, -.5));
      p.setFont(fc); p.setPen(theme::map("#8a8572")); p.drawText(r, Qt::AlignCenter, m_tags[i]);
    }
  }
  p.fillRect(QRect(0, 0, w, 36), theme::map("#050402"));
  columnHeader(p, QRect(0, 2, w, 33), QCoreApplication::translate("wire", "WIRE IN THE WINDOW"), m_items.isEmpty() ? QString() : QString::number(m_items.size()));
}
// The column header covers the viewport's top 36 px (painted over the rows): a point there is on the header, not on
// the row scrolled under it.
void WireBox::mouseMoveEvent(QMouseEvent* e) {
  const int r = e->position().y() < 36 ? -1 : rowAt(int(e->position().y()) + offset());
  if (r != m_hover) { m_hover = r; viewport()->update(); viewport()->setCursor(r >= 0 && !m_items[r].url.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor); }
}
void WireBox::mousePressEvent(QMouseEvent* e) {
  const int r = e->position().y() < 36 ? -1 : rowAt(int(e->position().y()) + offset());
  if (r >= 0 && !m_items[r].url.isEmpty()) ui::openWebLink(m_items[r].url);
}
void WireBox::leaveEvent(QEvent*) { m_hover = -1; viewport()->update(); }
bool WireBox::viewportEvent(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const int r = he->pos().y() < 36 ? -1 : rowAt(he->pos().y() + offset());
    if (r >= 0) QToolTip::showText(he->globalPos(), QString("%1 · %2\n%3").arg(m_items[r].source,
                                   i18n::locale().toString(m_items[r].time.toLocalTime(), "ddd d MMM HH:mm:ss"), m_items[r].text), viewport());
    else QToolTip::hideText();
    return true;
  }
  return SmoothScroll::viewportEvent(e);
}
}
