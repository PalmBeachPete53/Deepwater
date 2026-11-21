#include "pages/news/squawk/CalendarPane.h"
#include <QTimeZone>
#include "pages/news/squawk/SquawkLook.h"
#include "pages/news/squawk/SquawkWidgets.h"
#include "core/Settings.h"
#include "market/Instruments.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QLineEdit>
#include "core/I18n.h"
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>
#include <QVariantAnimation>
#include <algorithm>

namespace wire::squawk {
namespace {
// reference rows (page y − 385): search 393–422, rules 430 / 462 / 496 / 530, header 532–561, strong rule 562, rows from 563
constexpr int kSearchTop = 8, kWeekY = 61, kCountryY = 94, kImpactY = 128, kHeadTop = 146, kHeadH = 25, kRowsTop = 178;
constexpr int kDayH = 37, kExpandH = 34;
inline QColor kGold() { return theme::map("#ffd200"); }
inline QColor kCountdown() { return theme::map("#e0574a"); }
inline QColor kNextTime() { return theme::map("#dab31b"); }

struct Cols { int time = 10, flag = 84, ccy = 111, dot = 164, name = 175, actual, forecast, previous; bool narrow = false, phone = false; };
// Columns narrower than the reference's 665 px (laptops): the three value columns keep their place on a 62 px
// pitch and the time / currency columns close up, so the event name keeps room (it was cut to "Pre…" and
// PREVIOUS was dropped below 560 px).
// Phones (< 520 px): two-line rows. Line 1: time, flag, currency, impact, name; line 2: chips on the left, then the
// three values on an 86 px pitch from the right edge (the header labels sit above them).
Cols cols(int w) {
  Cols c;
  if (w < 520) {
    c.narrow = c.phone = true;
    c.flag = 60; c.ccy = 86; c.dot = 128; c.name = 60;
    c.previous = w - 10; c.forecast = w - 96; c.actual = w - 182;
    return c;
  }
  if (w < 665) {
    c.narrow = true;
    c.flag = 62; c.ccy = 88; c.dot = 126; c.name = 137;
    c.previous = w - 10; c.forecast = w - 72; c.actual = w - 134;
    return c;
  }
  c.previous = w - 10; c.forecast = w - 92; c.actual = w - 175; return c;
}

QString countdown(qint64 s) {
  if (s >= 100 * 3600) return QString("%1d %2h").arg(s / 86400).arg((s % 86400) / 3600, 2, 10, QChar('0'));
  return QString("%1:%2:%3").arg(s / 3600, 2, 10, QChar('0')).arg((s % 3600) / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
}
// "Oct 25 – Oct 31, 2026" / "25 oct. – 31 oct. 2026"
QString weekLabel(const QDate& w) {
  return i18n::locale().toString(w, QCoreApplication::translate("wire", "MMM d")) + " – " +
         i18n::locale().toString(w.addDays(6), QCoreApplication::translate("wire", "MMM d, yyyy"));
}
// an event with only a date (holiday, "All Day"): no clock time, never "next", no countdown
bool untimed(const wire::Release& e) { return e.allDay || e.impact == 0; }
QString impactName(int i) { return i >= 3 ? QCoreApplication::translate("wire", "High") : i == 2 ? QCoreApplication::translate("wire", "Medium") : i == 1 ? QCoreApplication::translate("wire", "Low") : QCoreApplication::translate("wire", "Holiday / none"); }
}

// ============================================================================================ CalendarRows
CalendarRows::CalendarRows(QWidget* parent) : QAbstractScrollArea(parent), m_anim(new QVariantAnimation(this)) {
  setFrameShape(QFrame::NoFrame);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  viewport()->setMouseTracking(true);
  m_anim->setDuration(160); m_anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { verticalScrollBar()->setValue(v.toInt()); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(), [this] { viewport()->update(); });
}
void CalendarRows::setRows(const QList<Row>& rows, const QString& empty) {
  m_rows = rows; m_empty = empty; m_hover = -1;
  relayout();
  viewport()->update();
}
int CalendarRows::rowHeight(int i) const {
  const auto& r = m_rows[i];
  if (r.day) return kDayH;
  return m_eventH + (m_expanded.contains(r.ev.id) ? kExpandH : 0);
}
void CalendarRows::relayout() {
  m_y.resize(m_rows.size() + 1);
  int y = 0;
  for (int i = 0; i < m_rows.size(); ++i) { m_y[i] = y; y += rowHeight(i); }
  m_y[m_rows.size()] = y;
  verticalScrollBar()->setRange(0, qMax(0, y - viewport()->height()));
  verticalScrollBar()->setPageStep(viewport()->height());
}
int CalendarRows::rowAt(int y) const {
  if (m_rows.isEmpty() || y < 0 || y >= m_y.last()) return -1;
  return int(std::upper_bound(m_y.begin(), m_y.end() - 1, y) - m_y.begin()) - 1;
}
void CalendarRows::resizeEvent(QResizeEvent* e) {
  QAbstractScrollArea::resizeEvent(e);
  m_eventH = viewport()->height() < 220 && viewport()->width() >= 520 ? 46 : 59;   // short windows: compact rows so two events stay visible (phones: two-line rows need 59)
  relayout();
}
void CalendarRows::scrollToFirstUpcoming() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  const QDate today = QDate::currentDate();
  int target = -1;
  for (int i = 0; i < m_rows.size(); ++i) {
    if (m_rows[i].day && m_rows[i].date == today) { target = i; break; }
    if (!m_rows[i].day && !untimed(m_rows[i].ev) && m_rows[i].ev.time >= now) { target = qMax(0, i - 1); break; }
  }
  verticalScrollBar()->setValue(target < 0 ? 0 : m_y[target]);
}
// Each second only the countdowns change (ACTUAL column); when an event passes, the next one (bold amber time) and its
// "—" change too: then the whole list.
void CalendarRows::tick() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  QDateTime next;
  for (const auto& r : m_rows) if (!r.day && !untimed(r.ev) && r.ev.time > now && (!next.isValid() || r.ev.time < next)) next = r.ev.time;
  // at midnight the "TODAY" badge moves to the next day header: the per-second repaint covers only the countdowns
  const QDate today = QDate::currentDate();
  if (next != m_next || today != m_today) { m_next = next; m_today = today; viewport()->update(); return; }
  const Cols c = cols(viewport()->width());
  viewport()->update(QRect(c.actual - 170, 0, 174, viewport()->height()));
}
void CalendarRows::scrollBy(int dy) {
  const int base = m_anim->state() == QAbstractAnimation::Running ? m_target : verticalScrollBar()->value();
  m_target = qBound(0, base + dy, verticalScrollBar()->maximum());
  m_anim->stop(); m_anim->setStartValue(verticalScrollBar()->value()); m_anim->setEndValue(m_target); m_anim->start();
}
void CalendarRows::wheelEvent(QWheelEvent* e) {
  if (!e->pixelDelta().isNull()) verticalScrollBar()->setValue(verticalScrollBar()->value() - e->pixelDelta().y());
  else scrollBy(-e->angleDelta().y() * 100 / 120);
  e->accept();
}
void CalendarRows::setHoverAt(const QPoint& p) { m_hover = rowAt(p.y() + verticalScrollBar()->value()); viewport()->update(); }
void CalendarRows::mouseMoveEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()) + verticalScrollBar()->value());
  const int h = r >= 0 && !m_rows[r].day ? r : -1;
  if (h != m_hover) { m_hover = h; viewport()->update(); viewport()->setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); }
}
bool CalendarRows::viewportEvent(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const int r = rowAt(he->pos().y() + verticalScrollBar()->value());
    if (r >= 0 && !m_rows[r].day) {
      const auto& ev = m_rows[r].ev;
      auto v = [](const QString& x) { return x.isEmpty() ? QStringLiteral("—") : x; };
      // frequency and economists' range too: narrow columns leave their chips out
      QString title = ev.title;
      if (!ev.frequency.isEmpty()) title += " · " + ev.frequency.toUpper();
      if (!ev.rangeLow.isEmpty() && !ev.rangeHigh.isEmpty()) title += " · " + ev.rangeLow + " – " + ev.rangeHigh;
      QToolTip::showText(he->globalPos(), ui::plainTip(QCoreApplication::translate("wire", "%1 %2 · %3\nActual %4 · Forecast %5 · Previous %6")
          .arg(ev.currency, title, untimed(ev) ? i18n::locale().toString(ev.time.toTimeZone(QTimeZone("America/New_York")).date(), "ddd d MMM")
                                               : i18n::locale().toString(ev.time.toLocalTime(), "ddd d MMM HH:mm"), v(ev.actual), v(ev.forecast), v(ev.previous))), viewport());
    } else QToolTip::hideText();
    return true;
  }
  return QAbstractScrollArea::viewportEvent(e);
}
void CalendarRows::leaveEvent(QEvent*) { if (m_hover >= 0) { m_hover = -1; viewport()->update(); } }
void CalendarRows::mousePressEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()) + verticalScrollBar()->value());
  if (r < 0 || m_rows[r].day) return;
  const QString id = m_rows[r].ev.id;
  if (m_expanded.contains(id)) m_expanded.remove(id); else m_expanded.insert(id);
  relayout();
  viewport()->update();
}

void CalendarRows::paintEvent(QPaintEvent* e) {
  const auto& L = look();
  QPainter p(viewport());
  p.fillRect(e->rect(), theme::map("#000000"));
  const int w = viewport()->width();
  if (m_rows.isEmpty()) {
    p.setFont(theme::mono(11, QFont::Normal, 1)); p.setPen(L.footer);
    p.drawText(viewport()->rect().adjusted(24, 0, -24, -40), Qt::AlignCenter | Qt::TextWordWrap, m_empty);
    return;
  }
  p.setRenderHint(QPainter::Antialiasing);
  const Cols c = cols(w);
  const int top = verticalScrollBar()->value();
  const QDateTime now = QDateTime::currentDateTimeUtc();
  // the next upcoming event (amber bold time)
  QDateTime next;
  for (const auto& r : m_rows) if (!r.day && !untimed(r.ev) && r.ev.time > now && (!next.isValid() || r.ev.time < next)) next = r.ev.time;
  const QFont fTime = theme::mono(14), fTimeB = theme::mono(14, QFont::Bold), fName = theme::condensed(16, QFont::Medium);
  const QFont fNum = theme::mono(14), fNumB = theme::mono(14, QFont::Bold);
  // day header: band 0–27 (#120d06), rule at 28, 7 px gap, the first row's top rule at 36 (reference 564–600)
  auto paintDay = [&](int y0, const QDate& date) {
    p.fillRect(QRect(0, y0, w, kDayH), theme::map("#000000"));
    p.fillRect(QRect(0, y0, w, 28), theme::map("#120d06"));
    p.fillRect(QRect(0, y0 + 28, w, 1), theme::map("#332a1d"));
    p.fillRect(QRect(0, y0 + 36, w, 1), theme::map("#251e12"));
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#cd9f0c")); p.drawEllipse(QPointF(13, y0 + 13), 2.3, 2.3);
    const QString t = i18n::locale().toString(date, QCoreApplication::translate("wire", "dddd, MMMM d"));
    p.setFont(theme::condensed(14, QFont::Bold)); p.setPen(theme::map("#d9a90f"));
    p.drawText(QPointF(21, y0 + 18), t);
    if (date == QDate::currentDate()) {
      const qreal x = 21 + QFontMetricsF(p.font()).horizontalAdvance(t) + 7;
      const QString today = QCoreApplication::translate("wire", "TODAY");
      const QRectF b(x, y0 + 5, QFontMetricsF(theme::mono(8, QFont::Bold, 0.5)).horizontalAdvance(today) + 9, 15);
      p.setPen(Qt::NoPen); p.setBrush(theme::map("#3a2c03")); p.drawRoundedRect(b, 2, 2);
      p.setFont(theme::mono(8, QFont::Bold, 0.5)); p.setPen(theme::map("#e1b11d"));
      p.drawText(b, Qt::AlignCenter, today);
    }
  };
  // phone row (see cols): same colours and fonts as the desktop row, on two lines
  auto phoneRow = [&](int i, int y0, int rh) {
    const auto& ev = m_rows[i].ev;
    if (i == m_hover) p.fillRect(QRect(0, y0, w, rh - 1), theme::map("#1a1000"));
    p.fillRect(QRect(0, y0 + rh - 1, w, 1), theme::map("#212208"));
    const int cy1 = y0 + 18, cy2 = y0 + 42;
    const bool noTime = untimed(ev);
    const bool isNext = !noTime && next.isValid() && ev.time == next;
    p.setFont(isNext ? fTimeB : fTime); p.setPen(isNext ? kNextTime() : theme::map("#63625c"));
    p.drawText(QPointF(c.time, cy1 + 5), noTime ? QStringLiteral("—") : ev.time.toLocalTime().toString("HH:mm"));
    p.drawPixmap(QPointF(c.flag, cy1 - 6.5), ui::flag(ev.currency, QSize(20, 13), devicePixelRatioF()));
    p.setFont(theme::mono(13)); p.setPen(theme::map("#8b8a81"));
    p.drawText(QPointF(c.ccy, cy1 + 5), ev.currency);
    p.setPen(Qt::NoPen);
    qreal nx = c.ccy + QFontMetricsF(p.font()).horizontalAdvance(ev.currency) + 10;
    if (ev.impact >= 3) { p.setBrush(kGold()); p.drawEllipse(QPointF(nx, cy1), 3.2, 3.2); p.drawEllipse(QPointF(nx + 8, cy1), 3.2, 3.2); nx += 18; }
    else if (ev.impact == 2) { p.setBrush(kGold()); p.drawEllipse(QPointF(nx, cy1), 3.2, 3.2); nx += 10; }
    else { p.setBrush(theme::map("#5d5c56")); p.drawEllipse(QPointF(nx, cy1), 2.6, 2.6); nx += 10; }
    p.setFont(fName); p.setPen(theme::map("#c4c3bf"));
    const QFontMetricsF fmN(fName);
    const QString nm = fmN.elidedText(ev.title, Qt::ElideRight, w - 10 - 16 - nx);
    p.drawText(QPointF(nx, cy1 + 5), nm);
    const qreal chx = nx + fmN.horizontalAdvance(nm) + 8;
    p.setPen(QPen(theme::map("#5a574f"), 1.2)); p.setBrush(Qt::NoBrush);
    const bool open = m_expanded.contains(ev.id);
    QPainterPath ch; const qreal sg = open ? -1 : 1;
    ch.moveTo(chx + 1, cy1 - 1.5 * sg); ch.lineTo(chx + 4.5, cy1 + 1.5 * sg); ch.lineTo(chx + 8, cy1 - 1.5 * sg);
    p.drawPath(ch);
    // line 2, left: frequency chip, then the range chip when it still fits before ACTUAL
    const qint64 secs = now.secsTo(ev.time);
    const bool counting = !ev.released() && secs > 0 && !noTime;
    const QString cd = QCoreApplication::translate("wire", "in") + " " + countdown(secs);
    const qreal valueLeft = c.actual - (counting ? QFontMetricsF(fNum).horizontalAdvance(cd) : 60) - 8;
    const QFont fChip = theme::mono(9, QFont::Bold, 0.5), fChipR = theme::mono(10, QFont::Bold);
    qreal x = c.name;
    if (!ev.frequency.isEmpty()) {
      const QString t = ev.frequency.toUpper();
      const QRectF r(x, cy2 - 9, QFontMetricsF(fChip).horizontalAdvance(t) + 16, 18);
      if (r.right() <= valueLeft) {
        p.setPen(QPen(theme::map("#1f5560"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
        p.setFont(fChip); p.setPen(theme::map("#46c3d7")); p.drawText(QPointF(r.left() + 8, cy2 + 3.5), t);
        x = r.right() + 6;
      }
    }
    if (!ev.rangeLow.isEmpty() && !ev.rangeHigh.isEmpty()) {
      const QString t = ev.rangeLow + " – " + ev.rangeHigh;
      const QRectF r(x, cy2 - 9, QFontMetricsF(fChipR).horizontalAdvance(t) + 16, 18);
      if (r.right() <= valueLeft) {
        p.setPen(QPen(theme::map("#bd9c19"), 1)); p.setBrush(theme::map("#140d02")); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
        p.setFont(fChipR); p.setPen(theme::map("#f0d070")); p.drawText(r, Qt::AlignCenter, t);
      }
    }
    const QRectF v2(0, cy2 - 12, 0, 24);
    auto col = [&](int right, int width) { return QRectF(right - width, v2.top(), width, v2.height()); };
    if (ev.released()) { p.setFont(fNumB); p.setPen(theme::map("#f5f2eb")); p.drawText(col(c.actual, 150), Qt::AlignRight | Qt::AlignVCenter, ev.actual); }
    else if (counting) { p.setFont(fNum); p.setPen(kCountdown()); p.drawText(col(c.actual, 170), Qt::AlignRight | Qt::AlignVCenter, cd); }
    else { p.setFont(fNum); p.setPen(theme::map("#5f5d57")); p.drawText(col(c.actual, 120), Qt::AlignRight | Qt::AlignVCenter, "—"); }
    p.setFont(fNum); p.setPen(theme::map("#898882"));
    p.drawText(col(c.forecast, 80), Qt::AlignRight | Qt::AlignVCenter, ev.forecast.isEmpty() ? "—" : ev.forecast);
    p.setPen(theme::map("#76756e"));
    p.drawText(col(c.previous, 80), Qt::AlignRight | Qt::AlignVCenter, ev.previous.isEmpty() ? "—" : ev.previous);
    if (open) {
      QString d = ev.detail;
      if (d.isEmpty()) {
        d = QCoreApplication::translate("wire", "%1 · %2 impact · %3 local").arg(ev.title, impactName(ev.impact),
              noTime ? i18n::locale().toString(ev.time.toTimeZone(QTimeZone("America/New_York")).date(), "ddd d MMM")
                     : i18n::locale().toString(ev.time.toLocalTime(), "ddd d MMM HH:mm"));
        if (!ev.rangeLow.isEmpty()) d += QCoreApplication::translate("wire", " · economists' range %1 – %2").arg(ev.rangeLow, ev.rangeHigh);
        if (!ev.frequency.isEmpty()) d += " · " + ev.frequency.toLower() + (ev.frequencyInferred ? QCoreApplication::translate("wire", " (inferred)") : QString());
      }
      p.setFont(theme::sans(11)); p.setPen(theme::map("#8a877e"));
      p.drawText(QRectF(c.time, y0 + m_eventH - 4, w - c.time - 12, kExpandH), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, d);
    }
  };
  int i = qMax(0, rowAt(top + e->rect().top()));
  for (; i < m_rows.size() && m_y[i] - top <= e->rect().bottom(); ++i) {
    const auto& row = m_rows[i];
    const int y0 = m_y[i] - top, rh = m_y[i + 1] - m_y[i];
    if (row.day) {
      paintDay(y0, row.date);
      continue;
    }
    if (c.phone) { phoneRow(i, y0, rh); continue; }
    const auto& ev = row.ev;
    if (i == m_hover) p.fillRect(QRect(0, y0, w, rh - 1), theme::map("#1a1000"));
    p.fillRect(QRect(0, y0 + rh - 1, w, 1), theme::map("#212208"));
    const int cy = y0 + m_eventH / 2;
    const bool noTime = untimed(ev);
    const bool isNext = !noTime && next.isValid() && ev.time == next;
    p.setFont(isNext ? fTimeB : fTime); p.setPen(isNext ? kNextTime() : theme::map("#63625c"));
    // a holiday's stamp (midnight New York) is not a time: it read "06:00" with a countdown to it; a dash, as the
    // time column has no room for "all day" in every language
    p.drawText(QPointF(c.time, cy + 5), noTime ? QStringLiteral("—") : ev.time.toLocalTime().toString("HH:mm"));
    p.drawPixmap(QPointF(c.flag, cy - 6.5), ui::flag(ev.currency, QSize(20, 13), devicePixelRatioF()));
    p.setFont(theme::mono(13)); p.setPen(theme::map("#8b8a81"));
    p.drawText(QPointF(c.ccy, cy + 5), ev.currency);
    // impact dots: low = one grey dot, medium = one gold dot, high = two gold dots
    p.setPen(Qt::NoPen);
    qreal nx = c.dot;
    if (ev.impact >= 3) { p.setBrush(kGold()); p.drawEllipse(QPointF(nx, cy), 3.2, 3.2); p.drawEllipse(QPointF(nx + 8, cy), 3.2, 3.2); nx += 18; }
    else if (ev.impact == 2) { p.setBrush(kGold()); p.drawEllipse(QPointF(nx, cy), 3.2, 3.2); nx += 10; }
    else { p.setBrush(theme::map("#5d5c56")); p.drawEllipse(QPointF(nx, cy), 2.6, 2.6); nx += 10; }
    // chips (frequency, range) after the name; the name is elided to leave them room
    QStringList chipText; QList<int> chipKind;
    if (!ev.frequency.isEmpty()) { chipText << ev.frequency.toUpper(); chipKind << 0; }
    if (!ev.rangeLow.isEmpty() && !ev.rangeHigh.isEmpty()) { chipText << ev.rangeLow + " – " + ev.rangeHigh; chipKind << 1; }
    const QFont fChip = theme::mono(9, QFont::Bold, 0.5), fChipR = theme::mono(10, QFont::Bold), fInf = theme::sans(8);
    qreal chipsW = 0;
    for (int k = 0; k < chipText.size(); ++k)
      chipsW += (chipKind[k] == 0 ? QFontMetricsF(fChip).horizontalAdvance(chipText[k]) + (ev.frequencyInferred ? 38 : 0)
                                  : QFontMetricsF(fChipR).horizontalAdvance(chipText[k])) + 16 + 6;
    // room left of ACTUAL: a released value is ≈ 50 px wide, the countdown ("in 00:00:00") ≈ 70 px
    const qreal reserve = ev.released() ? 58 : 72;
    // narrow columns: the chips go (frequency and range stay in the row tooltip) before the name drops under 120 px
    if (!chipText.isEmpty() && c.actual - reserve - nx - chipsW - 16 < 120) { chipText.clear(); chipKind.clear(); chipsW = 0; }
    const qreal nameMax = qMax<qreal>(20, c.actual - reserve - nx - chipsW - 16);
    p.setFont(fName); p.setPen(theme::map("#c4c3bf"));
    const QFontMetricsF fmN(fName);
    const QString nm = fmN.elidedText(ev.title, Qt::ElideRight, nameMax);
    p.drawText(QPointF(nx, cy + 5), nm);
    qreal x = nx + fmN.horizontalAdvance(nm) + 8;
    for (int k = 0; k < chipText.size(); ++k) {
      if (chipKind[k] == 0) {
        const qreal tw = QFontMetricsF(fChip).horizontalAdvance(chipText[k]);
        const QRectF r(x, cy - 9, tw + 16 + (ev.frequencyInferred ? 38 : 0), 18);
        p.setPen(QPen(theme::map("#1f5560"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
        p.setFont(fChip); p.setPen(theme::map("#46c3d7")); p.drawText(QPointF(r.left() + 8, cy + 3.5), chipText[k]);
        if (ev.frequencyInferred) { p.setFont(fInf); p.setPen(theme::map("#4c4a44")); p.drawText(QPointF(r.left() + 13 + tw, cy + 3), "inferred"); }
        x = r.right() + 6;
      } else {
        const qreal tw = QFontMetricsF(fChipR).horizontalAdvance(chipText[k]);
        const QRectF r(x, cy - 9, tw + 16, 18);
        p.setPen(QPen(theme::map("#bd9c19"), 1)); p.setBrush(theme::map("#140d02")); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
        p.setFont(fChipR); p.setPen(theme::map("#f0d070")); p.drawText(r, Qt::AlignCenter, chipText[k]);
        x = r.right() + 6;
      }
    }
    // expander chevron
    p.setPen(QPen(theme::map("#5a574f"), 1.2)); p.setBrush(Qt::NoBrush);
    const bool open = m_expanded.contains(ev.id);
    QPainterPath ch; const qreal s = open ? -1 : 1;
    ch.moveTo(x + 1, cy - 1.5 * s); ch.lineTo(x + 4.5, cy + 1.5 * s); ch.lineTo(x + 8, cy - 1.5 * s);
    p.drawPath(ch);
    // ACTUAL: value once released, countdown before
    const qint64 secs = now.secsTo(ev.time);
    if (ev.released()) {
      p.setFont(fNumB); p.setPen(theme::map("#f5f2eb"));
      p.drawText(QRectF(c.actual - 120, y0, 120, m_eventH), Qt::AlignRight | Qt::AlignVCenter, ev.actual);
    } else if (secs > 0 && !noTime) {
      p.setFont(fNum); p.setPen(kCountdown());
      p.drawText(QRectF(c.actual - 140, y0 + m_eventH / 2 - 21, 140, 20), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "in"));
      p.drawText(QRectF(c.actual - 140, y0 + m_eventH / 2 + 1, 140, 20), Qt::AlignRight | Qt::AlignVCenter, countdown(secs));
    } else {
      p.setFont(fNum); p.setPen(theme::map("#5f5d57"));
      p.drawText(QRectF(c.actual - 120, y0, 120, m_eventH), Qt::AlignRight | Qt::AlignVCenter, "—");
    }
    p.setFont(fNum); p.setPen(theme::map("#898882"));
    p.drawText(QRectF(c.forecast - 110, y0, 110, m_eventH), Qt::AlignRight | Qt::AlignVCenter, ev.forecast.isEmpty() ? "—" : ev.forecast);
    p.setPen(theme::map("#76756e"));
    if (c.previous > 0)
      p.drawText(QRectF(c.previous - 110, y0, 110, m_eventH), Qt::AlignRight | Qt::AlignVCenter, ev.previous.isEmpty() ? "—" : ev.previous);
    if (open) {
      QString d = ev.detail;
      if (d.isEmpty()) {
        d = QCoreApplication::translate("wire", "%1 · %2 impact · %3 local").arg(ev.title, impactName(ev.impact),
              noTime ? i18n::locale().toString(ev.time.toTimeZone(QTimeZone("America/New_York")).date(), "ddd d MMM")   // a date only
                     : i18n::locale().toString(ev.time.toLocalTime(), "ddd d MMM HH:mm"));
        if (!ev.rangeLow.isEmpty()) d += QCoreApplication::translate("wire", " · economists' range %1 – %2").arg(ev.rangeLow, ev.rangeHigh);
        if (!ev.frequency.isEmpty()) d += " · " + ev.frequency.toLower() + (ev.frequencyInferred ? QCoreApplication::translate("wire", " (inferred)") : QString());
      }
      p.setFont(theme::sans(11)); p.setPen(theme::map("#8a877e"));
      p.drawText(QRectF(c.name, y0 + m_eventH - 6, w - c.name - 12, kExpandH), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, d);
    }
  }
  // sticky day header (reference t034.5): the day of the top row stays pinned, pushed up by the next day
  if (top > 0) {
    const int r0 = rowAt(top);
    int dayIdx = -1, nextDay = -1;
    for (int k = qMax(0, r0); k >= 0; --k) if (m_rows[k].day) { dayIdx = k; break; }
    for (int k = qMax(0, r0) + 1; k < m_rows.size(); ++k) if (m_rows[k].day) { nextDay = k; break; }
    if (dayIdx >= 0 && m_y[dayIdx] < top) {
      int y = 0;
      if (nextDay >= 0) y = qMin(0, m_y[nextDay] - top - 29);
      p.setClipRect(QRect(0, 0, w, 29 + y));
      paintDay(y, m_rows[dayIdx].date);
      p.setClipping(false);
    }
  }
}

// ============================================================================================ CalendarPane
CalendarPane::CalendarPane(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  m_search = new QLineEdit(this);
  m_search->setPlaceholderText(QCoreApplication::translate("wire", "Search event, country, or currency..."));
  m_search->setClearButtonEnabled(true);
  m_search->setStyleSheet(QString("QLineEdit{background:%1;border:1px solid %2;border-radius:2px;padding:0 8px 0 28px;"
                          "color:%3;font-family:'Rajdhani';font-size:13px;font-weight:500;}"
                          "QLineEdit:focus{border-color:%4;}")
                          .arg(theme::map("#120d06").name(), theme::map("#2c2218").name(), theme::map("#d8d4c8").name(), theme::map("#5a4a1c").name()));
  QPalette pl = m_search->palette(); pl.setColor(QPalette::PlaceholderText, theme::map("#3d382d")); m_search->setPalette(pl);
  connect(m_search, &QLineEdit::textChanged, this, [this] { m_scrolled = true; rebuild(); });
  m_rows = new CalendarRows(this);
  m_footer = new Footer(this);
  // week of today (Sunday → Saturday, like the reference "Oct 25 – Oct 31")
  const QDate t = QDate::currentDate();
  m_week = t.addDays(-(t.dayOfWeek() % 7));
  const QStringList saved = settings::value("news.squawk.calCountries").toStringList();
  for (const auto& c : saved.isEmpty() ? market::majorCurrencies() : saved) m_countries.insert(c);
  m_impact = settings::value("news.squawk.calImpact", 0).toInt();
  m_tick.setInterval(1000);
  connect(&m_tick, &QTimer::timeout, this, [this] {
    m_rows->tick();
    // roll to the next week on Saturday → Sunday
  });
  connect(&wire::Hooks::instance(), &wire::Hooks::releaseSourceChanged, this, [this] { connectSource(); });
  connectSource();
}

void CalendarPane::connectSource() {
  if (auto* s = wire::releaseSource()) {
    connect(s, &wire::ReleaseSource::changed, this, &CalendarPane::rebuild, Qt::UniqueConnection);
    const QDateTime from(m_week, QTime(0, 0)), to(m_week.addDays(14), QTime(0, 0));
    s->ensure(from.toUTC(), to.toUTC());   // this week and the next (weekend roll-over)
  }
  rebuild();
}

void CalendarPane::setRunning(bool on) {
  if (on) { m_tick.start(); rebuild(); } else m_tick.stop();
}

QList<wire::Release> CalendarPane::weekEvents() const {
  auto* s = wire::releaseSource();
  if (!s) return {};
  const QDateTime from(m_week, QTime(0, 0)), to(m_week.addDays(7), QTime(0, 0));
  auto list = s->releases(from.toUTC(), to.toUTC());
  std::sort(list.begin(), list.end(), [](const wire::Release& a, const wire::Release& b) {
    return a.time != b.time ? a.time < b.time : a.currency < b.currency; });
  return list;
}

void CalendarPane::rebuild() {
  auto* s = wire::releaseSource();
  QList<CalendarRows::Row> rows;
  QString empty;
  if (!s) {
    m_counts = {-1, -1, -1, -1};
    empty = QCoreApplication::translate("wire", "Calendar connecting…\nThe economic calendar of News › Calendar feeds this list and its countdowns.");
    m_footer->setStamp({}, false);
  } else {
    // weekend: once the shown week is over, move to the coming week (until the user navigates)
    if (!m_userWeek) {
      const QDateTime now = QDateTime::currentDateTimeUtc();
      const auto cur = weekEvents();
      const bool pending = std::any_of(cur.cbegin(), cur.cend(), [&](const wire::Release& r) { return r.time > now; });
      const QDateTime nf(m_week.addDays(7), QTime(0, 0)), nt(m_week.addDays(14), QTime(0, 0));
      if (!pending && QDate::currentDate() >= m_week.addDays(5) && !s->releases(nf.toUTC(), nt.toUTC()).isEmpty()) {
        m_week = m_week.addDays(7);   // every weekend, not once per session (m_userWeek is the user's navigation)
        m_scrolled = false;
        QTimer::singleShot(0, this, [this] {   // the week after it too, for the next roll-over
          if (auto* src = wire::releaseSource())
            src->ensure(QDateTime(m_week, QTime(0, 0)).toUTC(), QDateTime(m_week.addDays(14), QTime(0, 0)).toUTC());
        });
      }
    }
    const QString q = m_search->text().trimmed();
    const auto all = weekEvents();
    QStringList extra;
    QList<int> counts{0, 0, 0, 0};
    QList<wire::Release> shown;
    for (const auto& e : all) {
      if (!market::majorCurrencies().contains(e.currency) && !extra.contains(e.currency)) extra << e.currency;
      if (!m_countries.contains(e.currency)) continue;
      if (!q.isEmpty()) {
        const QString hay = e.title + " " + e.currency + " " + wire::currencyRegion(e.currency);
        if (!hay.contains(q, Qt::CaseInsensitive)) continue;
      }
      counts[0]++;
      const int cls = e.impact >= 3 ? 1 : e.impact == 2 ? 2 : 3;
      counts[cls]++;
      if (m_impact == 0 || m_impact == cls) shown << e;
    }
    extra.sort();
    m_extra = extra;
    m_counts = counts;
    // a holiday (impact 0, stamped at midnight New York time) is listed on its own date, not the local previous day
    // west of New York; by listed day, time order kept within a day
    static const QTimeZone ny("America/New_York");
    auto listedDay = [](const wire::Release& e) { return e.impact == 0 ? e.time.toTimeZone(ny).date() : e.time.toLocalTime().date(); };
    std::stable_sort(shown.begin(), shown.end(), [&](const wire::Release& a, const wire::Release& b) { return listedDay(a) < listedDay(b); });
    QDate day;
    for (const auto& e : shown) {
      const QDate d = listedDay(e);
      if (d != day) { CalendarRows::Row r; r.day = true; r.date = d; rows << r; day = d; }
      CalendarRows::Row r; r.ev = e; rows << r;
    }
    empty = all.isEmpty() ? QCoreApplication::translate("wire", "No event loaded for this week yet.") : QCoreApplication::translate("wire", "No event matches these filters.");
    m_footer->setStamp(s->lastRefresh(), true);
  }
  m_rows->setRows(rows, empty);
  if (!m_scrolled && !rows.isEmpty()) { m_rows->scrollToFirstUpcoming(); m_scrolled = true; }
  update();
}

void CalendarPane::resizeEvent(QResizeEvent*) {
  m_search->setGeometry(10, kSearchTop, width() - 20, 30);
  m_rows->setGeometry(0, kRowsTop, width(), height() - kRowsTop - 28);
  m_footer->setGeometry(0, height() - 28, width(), 28);
}

// Phones (< 520 px): finger-sized targets around the same centres, country chips 30 px high from the left edge
// (no COUNTRIES label), impact segments 30 px high; the bands between the rules stay where they are.
bool CalendarPane::phone() const { return width() < 520; }
QRectF CalendarPane::prevRect() const { return phone() ? QRectF(0, kWeekY - 16, 36, 32) : QRectF(8, kWeekY - 11, 20, 22); }
QRectF CalendarPane::nextRect() const {
  const QFontMetricsF fm(theme::mono(13, QFont::Bold));
  const QString t = weekLabel(m_week);
  if (phone()) return QRectF(35 + fm.horizontalAdvance(t), kWeekY - 16, 36, 32);
  return QRectF(35 + fm.horizontalAdvance(t) + 8, kWeekY - 11, 20, 22);
}
QList<QRectF> CalendarPane::countryRects() const {
  QList<QRectF> out;
  const auto majors = market::majorCurrencies();
  if (phone()) {
    qreal x = 10;
    const qreal pitch = (width() - 10 - 80) / qreal(majors.size());
    for (int i = 0; i < majors.size(); ++i) { out << QRectF(x, kCountryY - 15, pitch - 5, 30); x += pitch; }
    return out;
  }
  qreal x = 64;
  const qreal pitch = qMin<qreal>(59.5, (width() - 64 - 80) / qreal(majors.size()));
  for (int i = 0; i < majors.size(); ++i) { out << QRectF(x, kCountryY - 11, pitch - 5, 22); x += pitch; }
  return out;
}
QRectF CalendarPane::moreRect() const {
  const auto r = countryRects();
  if (phone()) return QRectF(r.last().right() + 5, kCountryY - 15, width() - 10 - r.last().right() - 5, 30);
  return QRectF(r.last().right() + 6, kCountryY - 11, 69, 22);
}
QList<QRectF> CalendarPane::impactRects() const {
  QList<QRectF> out;
  const QStringList names{QCoreApplication::translate("wire", "All"), QCoreApplication::translate("wire", "High"), QCoreApplication::translate("wire", "Medium"), QCoreApplication::translate("wire", "Low")};
  const QFontMetricsF fm(theme::condensed(14, QFont::Bold)), fc(theme::condensed(14, QFont::Medium));
  qreal x = 47;
  for (int i = 0; i < 4; ++i) {
    qreal w = fm.horizontalAdvance(names[i]) + 14;
    if (m_counts[i] >= 0) w += fc.horizontalAdvance(QString::number(m_counts[i])) + 4;
    out << (phone() ? QRectF(x, kImpactY - 15, w + 4, 30) : QRectF(x, kImpactY - 11, w, 22));
    x += phone() ? w + 8 : w + 4;
  }
  return out;
}

void CalendarPane::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), theme::map("#0f0a04"));
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(QRect(0, 0, width(), 1), theme::map("#241d0d"));
  for (int y : {45, 77, 111, 145}) p.fillRect(QRect(0, y, width(), 1), theme::map("#2a2114"));
  // search magnifier
  p.setPen(QPen(theme::map("#6a6355"), 1.3)); p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(22, kSearchTop + 14.5), 4.2, 4.2);
  p.drawLine(QPointF(25.2, kSearchTop + 17.7), QPointF(28.2, kSearchTop + 20.7));
  // week navigator
  const QString wk = weekLabel(m_week);
  auto chevron = [&](const QRectF& r, bool left, bool hov) {
    p.setPen(QPen(hov ? theme::map("#d8d2c0") : theme::map("#6f6a60"), 1.3));
    const QPointF c = r.center(); const qreal s = left ? 1 : -1;
    p.drawLine(c + QPointF(2 * s, -4), c + QPointF(-2 * s, 0)); p.drawLine(c + QPointF(-2 * s, 0), c + QPointF(2 * s, 4));
  };
  chevron(prevRect(), true, m_hover == 20);
  p.setFont(theme::mono(13, QFont::Bold)); p.setPen(theme::map("#a39e94"));
  p.drawText(QPointF(35, kWeekY + 5), wk);
  chevron(nextRect(), false, m_hover == 21);
  // COUNTRIES
  p.setFont(theme::mono(8, QFont::Normal, 1)); p.setPen(theme::map("#4a473d"));
  if (!phone()) p.drawText(QPointF(10, kCountryY + 3), QCoreApplication::translate("wire", "COUNTRIES"));
  const auto majors = market::majorCurrencies();
  const auto cr = countryRects();
  const bool flags = cr.first().width() >= 50;
  for (int i = 0; i < cr.size(); ++i) {
    const bool on = m_countries.contains(majors[i]);
    const QRectF r = cr[i];
    p.setPen(QPen(on ? theme::map("#3d3012") : theme::map("#221d12"), 1));
    p.setBrush(on ? theme::map("#2a2003") : theme::map("#0a0704"));
    if (m_hover == i) p.setBrush(on ? theme::map("#3a2c05") : theme::map("#16110a"));
    p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
    qreal tx = r.left() + 6;
    if (flags) {
      QPixmap f = ui::flag(majors[i], QSize(20, 13), devicePixelRatioF());
      if (!on) { QPainter fp(&f); fp.setCompositionMode(QPainter::CompositionMode_DestinationIn); fp.fillRect(f.rect(), theme::map(0, 0, 0, 90)); }
      p.drawPixmap(QPointF(r.left() + 5, r.center().y() - 6.5), f);
      tx = r.left() + 29;
    }
    p.setFont(theme::condensed(13, QFont::Bold)); p.setPen(on ? theme::map("#c4bca8") : theme::map("#5b574d"));
    p.drawText(QRectF(tx, r.top(), r.right() - tx, r.height()), Qt::AlignVCenter | Qt::AlignLeft, majors[i]);
  }
  const QRectF mr = moreRect();
  p.setPen(QPen(theme::map("#2a2418"), 1)); p.setBrush(m_hover == 8 ? theme::map("#16110a") : theme::map("#0a0704"));
  p.drawRoundedRect(mr.adjusted(.5, .5, -.5, -.5), 2, 2);
  p.setPen(QPen(theme::map("#6b675e"), 1)); p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(mr.left() + 11, mr.center().y()), 4.5, 4.5);
  p.drawLine(QPointF(mr.left() + 6.5, mr.center().y()), QPointF(mr.left() + 15.5, mr.center().y()));
  p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(theme::map("#6b675e"));
  const int extraOn = int(std::count_if(m_extra.cbegin(), m_extra.cend(), [&](const QString& c) { return m_countries.contains(c); }));
  p.drawText(QPointF(mr.left() + 20, mr.center().y() + 5), extraOn ? QString("+%1").arg(extraOn) : QCoreApplication::translate("wire", "More..."));
  QPainterPath dch; dch.moveTo(mr.right() - 14, mr.center().y() - 1.5); dch.lineTo(mr.right() - 11, mr.center().y() + 1.5); dch.lineTo(mr.right() - 8, mr.center().y() - 1.5);
  p.drawPath(dch);
  // IMPACT
  p.setFont(theme::mono(8, QFont::Normal, 1)); p.setPen(theme::map("#4a473d"));
  p.drawText(QPointF(10, kImpactY + 3), QCoreApplication::translate("wire", "IMPACT"));
  const QStringList names{QCoreApplication::translate("wire", "All"), QCoreApplication::translate("wire", "High"), QCoreApplication::translate("wire", "Medium"), QCoreApplication::translate("wire", "Low")};
  const auto ir = impactRects();
  for (int i = 0; i < 4; ++i) {
    const bool on = i == m_impact;
    if (on) { p.setPen(Qt::NoPen); p.setBrush(theme::map("#2c2924")); p.drawRoundedRect(ir[i], 3, 3); }
    else if (m_hover == 10 + i) { p.setPen(Qt::NoPen); p.setBrush(theme::map("#15120c")); p.drawRoundedRect(ir[i], 3, 3); }
    const QFont f = theme::condensed(14, on ? QFont::Bold : QFont::Medium);
    p.setFont(f); p.setPen(on ? theme::map("#d6d3cb") : theme::map("#676257"));
    p.drawText(QPointF(ir[i].left() + 7, ir[i].center().y() + 5), names[i]);
    if (m_counts[i] >= 0) {
      p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(on ? theme::map("#4e4b45") : theme::map("#55514a"));
      p.drawText(QPointF(ir[i].left() + 7 + QFontMetricsF(f).horizontalAdvance(names[i]) + 4, ir[i].center().y() + 5),
                 QString::number(m_counts[i]));
    }
  }
  // column header
  p.fillRect(QRect(0, kRowsTop - 1, width(), 1), theme::map("#3b351a"));
  const auto c = cols(width());
  p.setFont(theme::mono(9, QFont::Normal, c.narrow ? 0.3 : 1.2)); p.setPen(theme::map("#6a665d"));   // 62 px pitch: "PRÉCÉDENT" fits
  const int hy = kHeadTop + 18;
  p.drawText(QPointF(c.time, hy), QCoreApplication::translate("wire", "TIME"));
  if (!c.phone) p.drawText(QPointF(c.flag, hy), QCoreApplication::translate("wire", "CURRENCY"));
  p.drawText(QPointF(c.phone ? c.name : c.name - 12, hy), QCoreApplication::translate("wire", "EVENT"));
  p.drawText(QRectF(c.actual - 100, kHeadTop, 100, kHeadH + 5), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "ACTUAL"));
  p.drawText(QRectF(c.forecast - 100, kHeadTop, 100, kHeadH + 5), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "FORECAST"));
  if (c.previous > 0) p.drawText(QRectF(c.previous - 100, kHeadTop, 100, kHeadH + 5), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "PREVIOUS"));
}

void CalendarPane::mousePressEvent(QMouseEvent* e) {
  const QPointF pt = e->position();
  auto changeWeek = [this](int d) {
    m_week = m_week.addDays(7 * d);
    m_scrolled = false;
    m_userWeek = true;
    if (auto* s = wire::releaseSource()) s->ensure(QDateTime(m_week, QTime(0, 0)).toUTC(), QDateTime(m_week.addDays(7), QTime(0, 0)).toUTC());
    rebuild();
  };
  if (prevRect().contains(pt)) return changeWeek(-1);
  if (nextRect().contains(pt)) return changeWeek(1);
  const auto majors = market::majorCurrencies();
  const auto cr = countryRects();
  for (int i = 0; i < cr.size(); ++i)
    if (cr[i].contains(pt)) {
      if (m_countries.contains(majors[i])) m_countries.remove(majors[i]); else m_countries.insert(majors[i]);
      settings::setValue("news.squawk.calCountries", QStringList(m_countries.cbegin(), m_countries.cend()));
      return rebuild();
    }
  if (moreRect().contains(pt)) {
    QMenu menu(this);
    auto* all = menu.addAction(QCoreApplication::translate("wire", "All currencies"));
    auto* majorsOnly = menu.addAction(QCoreApplication::translate("wire", "The 8 majors only"));
    menu.addSeparator();
    for (const auto& c : m_extra) { auto* a = menu.addAction(c); a->setCheckable(true); a->setChecked(m_countries.contains(c)); a->setData(c); }
    if (m_extra.isEmpty()) menu.addAction(QCoreApplication::translate("wire", "No other currency this week"))->setEnabled(false);
    auto* chosen = menu.exec(mapToGlobal(moreRect().bottomLeft().toPoint() + QPoint(0, 2)));
    if (!chosen) return;
    if (chosen == all) { for (const auto& c : majors) m_countries.insert(c); for (const auto& c : m_extra) m_countries.insert(c); }
    else if (chosen == majorsOnly) { m_countries = QSet<QString>(majors.cbegin(), majors.cend()); }
    else { const QString c = chosen->data().toString(); if (chosen->isChecked()) m_countries.insert(c); else m_countries.remove(c); }
    settings::setValue("news.squawk.calCountries", QStringList(m_countries.cbegin(), m_countries.cend()));
    return rebuild();
  }
  const auto ir = impactRects();
  for (int i = 0; i < ir.size(); ++i)
    if (ir[i].contains(pt) && i != m_impact) { m_impact = i; settings::setValue("news.squawk.calImpact", i); return rebuild(); }
}
void CalendarPane::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pt = e->position();
  int h = -1;
  const auto cr = countryRects();
  for (int i = 0; i < cr.size(); ++i) if (cr[i].contains(pt)) h = i;
  if (moreRect().contains(pt)) h = 8;
  const auto ir = impactRects();
  for (int i = 0; i < ir.size(); ++i) if (ir[i].contains(pt)) h = 10 + i;
  if (prevRect().contains(pt)) h = 20;
  if (nextRect().contains(pt)) h = 21;
  if (h != m_hover) { m_hover = h; update(0, 0, width(), kRowsTop); setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); }
  if (h >= 0 && h < 8 && !wire::releaseSource())
    QToolTip::showText(e->globalPosition().toPoint(), QCoreApplication::translate("wire", "Filters apply once the calendar is connected"), this);
}
void CalendarPane::leaveEvent(QEvent*) { if (m_hover != -1) { m_hover = -1; update(0, 0, width(), kRowsTop); } }
}
