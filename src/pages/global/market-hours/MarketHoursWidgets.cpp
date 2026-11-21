#include "pages/global/market-hours/MarketHoursWidgets.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include "global/Look.h"
#include "global/Sessions.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <algorithm>
#include <QPainterPath>
#include <QPaintEvent>
#include <QLocale>
#include <QtMath>

namespace hours {
namespace look = global::look;

static QPointF polar(const QPointF& c, qreal r, qreal deg) {   // deg clockwise from 12 o'clock
  const qreal a = qDegreesToRadians(deg);
  return {c.x() + r * std::sin(a), c.y() - r * std::cos(a)};
}
// Text with a soft halo (digital clock digits).
static void glowText(QPainter& p, const QPointF& baseline, const QString& s, const QFont& f, const QColor& c, int glowAlpha) {
  if (!theme::isDark()) glowAlpha = glowAlpha * 3 / 10;
  QPainterPath path;
  path.addText(baseline, f, s);
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(Qt::NoBrush);
  for (int i = 3; i >= 1; --i) {
    QColor g = c; g.setAlpha(glowAlpha / (i + 2));
    QPen pen(g, 3.0 + i * 4.0); pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(path);
  }
  p.setPen(Qt::NoPen);
  p.setBrush(c);
  p.drawPath(path);
  p.restore();
}

// Which sessions draw bright on the dial: the open ones, or the next to open when all are closed.
static QList<bool> brightSessions(const QDateTime& utc) {
  const auto& ss = global::sessions();
  QList<bool> b(ss.size(), false);
  int next = -1; QDateTime best;
  bool any = false;
  for (int i = 0; i < ss.size(); ++i) {
    if (global::isOpen(ss[i], utc)) { b[i] = true; any = true; continue; }
    const auto n = global::nextSpan(ss[i], utc);
    if (n.valid() && (!best.isValid() || n.open < best)) { best = n.open; next = i; }
  }
  if (!any && next >= 0) b[next] = true;
  return b;
}

// ================================================================= ClockPanel
static QRectF wedgeBox(const QPointF& c, qreal r, qreal lead);
static const qreal kArcR[4] = {183, 175, 165, 155};

ClockPanel::ClockPanel(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  // 20 fps: the sweep turns once per 7 s (2.6° per frame); 30 fps cost ~14 % CPU. Frames land on the wall clock's
  // 20ths of a second (+2 ms), the phase of the page's half-second tick: the clock's second and the cursors repaint
  // in a sweep frame's pass instead of a pass of their own.
  m_sweep.setSingleShot(true);
  m_sweep.setTimerType(Qt::PreciseTimer);
  connect(&m_sweep, &QTimer::timeout, this, [this] {
    QPointF c; qreal s; geometryFor(c, s);
    const qreal lead = std::fmod(m_clock.elapsed() / 7000.0 * 360.0, 360.0);
    const QRectF now = wedgeBox(c, 190 * s, lead);
    updateRect((now | m_lastWedge).toAlignedRect());   // only where the wedge was and is
    m_lastWedge = now;
    scheduleSweep();
  });
  m_clock.start();
  m_utc = global::nowUtc();
}

void ClockPanel::geometryFor(QPointF& c, qreal& s) const {
  const qreal avail = height() - 26 - 110;
  s = qBound(0.35, qMin(avail / 430.0, (width() - 20) / 430.0), 1.35);   // grows on big screens
  c = QPointF(width() / 2.0 + 0.5, 26 + avail / 2.0);
}

QRect ClockPanel::cursorRect() const {
  const QString t = QCoreApplication::translate("global", "YOUR CLOCK · %1").arg(global::userZoneId().toUpper());
  const qreal x = 14 + look::width(t, theme::mono(10, QFont::Normal, 2.4));
  return QRectF(x, 15, 8, 13).toAlignedRect();
}

void ClockPanel::setNow(const QDateTime& utc) {
  m_utc = utc;
  const auto b = brightSessions(utc);
  QString key = utc.toTimeZone(global::userZone()).toString("yyyyMMddHHmm");
  for (bool x : b) key += x ? '1' : '0';
  const bool dial = key != m_arcKey;   // new minute or session change: the dial layers too
  if (dial) { m_arcKey = key; rebuild(); }
  rebuildTop();                         // after rebuild(): it copies the dial layer
  updateRect(dial ? rect() : secondRect());
}

// A new second moves the hands and the digital time only (rebuildTop): the dial's top, arcs and markers stay.
QRect ClockPanel::secondRect() const {
  QPointF c; qreal s; geometryFor(c, s);
  const qreal hr = 125 * s + 16;   // the second hand's tip (125 s) plus its glow and cap; the other hands are shorter
  return QRectF(c.x() - hr, c.y() - hr, 2 * hr, 2 * hr).toAlignedRect() | QRect(0, height() - 130, width(), 130);
}

// Every repaint request of a pass (sweep frame, new second, cursor) grows one rectangle: Qt would keep them as a
// region of several rects, and the conical wedge then clips against each (2.5x its cost).
void ClockPanel::updateRect(const QRect& r) {
  m_dirty |= r;
  update(m_dirty);
}

void ClockPanel::scheduleSweep() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  // next 20th of a second after now (a timer firing a little early never yields two frames in a row)
  const qint64 next = (qint64(std::floor((now - 2) / 50.0 + 0.5)) + 1) * 50 + 2;
  m_sweep.start(int(qMax<qint64>(1, next - now)));
}
void ClockPanel::resizeEvent(QResizeEvent*) { rebuild(); rebuildTop(); }
void ClockPanel::showEvent(QShowEvent*) { scheduleSweep(); }
void ClockPanel::hideEvent(QHideEvent*) { m_sweep.stop(); }

void ClockPanel::rebuild() {
  if (width() <= 0 || height() <= 0) return;
  const qreal dpr = devicePixelRatioF();
  QPointF c; qreal s; geometryFor(c, s);
  const QDateTime local = m_utc.toTimeZone(global::userZone());

  // ---- under: panel, title, face glow
  m_under = QPixmap(size() * dpr); m_under.setDevicePixelRatio(dpr);
  {
    QPainter p(&m_under);
    p.setRenderHint(QPainter::Antialiasing);
    look::panel(p, QRectF(rect()), theme::map("#070503"), theme::map("#413c2f"));
    look::text(p, 14, 25, QCoreApplication::translate("global", "YOUR CLOCK · %1").arg(global::userZoneId().toUpper()), theme::mono(10, QFont::Normal, 2.4), theme::map("#656460"));
    QRadialGradient g(c, 150 * s);
    g.setColorAt(0.0, theme::map("#6a5a08"));
    g.setColorAt(0.3, theme::map("#675707"));
    g.setColorAt(0.5, theme::map("#5a4c06"));
    g.setColorAt(0.55, theme::map("#504405"));
    g.setColorAt(0.75, theme::map("#282306"));
    g.setColorAt(0.96, theme::map("#0c0901"));
    g.setColorAt(1.0, theme::map(QColor(12, 9, 1, 0)));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(c, 150 * s, 150 * s);
  }

  // ---- over: dial, rings, star dust, session arcs, now dot
  m_over = QPixmap(size() * dpr); m_over.setDevicePixelRatio(dpr);
  m_over.fill(Qt::transparent);
  {
    QPainter p(&m_over);
    p.setRenderHint(QPainter::Antialiasing);
    // star dust (deterministic jitter)
    p.setPen(Qt::NoPen);
    quint32 seed = 1234567;
    auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / double(1 << 24); };
    for (int i = 0; i < 64; ++i) {
      const qreal ang = i * 360.0 / 64 + (rnd() - 0.5) * 4;
      const qreal r = (205 + rnd() * 11) * s;
      const qreal d = 0.5 + rnd() * 0.6;
      p.setBrush(theme::map(QColor(92, 84, 62, 150 + int(rnd() * 105))));
      p.drawEllipse(polar(c, r, ang), d, d);
    }
    // dotted outer circle
    {
      const qreal r = 200 * s;
      const int n = int(2 * M_PI * r / 4);
      p.setBrush(theme::map("#3a3528"));
      for (int i = 0; i < n; ++i) p.drawEllipse(polar(c, r, i * 360.0 / n), 0.55, 0.55);
    }
    // session arcs (current or next session of each centre, in the user's time)
    const auto& ss = global::sessions();
    const auto bright = brightSessions(m_utc);
    for (int i = 0; i < ss.size(); ++i) {
      const auto sp = global::currentOrNext(ss[i], m_utc);
      if (!sp.valid()) continue;
      const qreal a0 = global::dayMinutes(sp.open, global::userZone()) / 4.0;
      const qreal span = sp.open.secsTo(sp.close) / 60.0 / 4.0;
      const qreal r = kArcR[i] * s;
      const QRectF box(c.x() - r, c.y() - r, 2 * r, 2 * r);
      const int qa = int((90 - a0) * 16), qs = int(-span * 16);
      if (bright[i]) {
        for (int k = 4; k >= 1; --k) {
          QColor g = global::sessionColour(ss[i], global::Tone::Bright); g.setAlpha(18 + 8 * (4 - k));
          QPen pen(g, (5 + 4 * k) * s); pen.setCapStyle(Qt::RoundCap);
          p.setPen(pen); p.drawArc(box, qa, qs);
        }
        QPen pen(global::sessionColour(ss[i], global::Tone::Bright), 5 * s); pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen); p.drawArc(box, qa, qs);
        QPen core(theme::mix(global::sessionColour(ss[i], global::Tone::Bright), theme::map("#ffffff"), 0.25), 1.6 * s); core.setCapStyle(Qt::RoundCap);
        p.setPen(core); p.drawArc(box, qa, qs);
      } else {
        QPen pen(global::sessionColour(ss[i], global::Tone::Arc), 3 * s); pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen); p.drawArc(box, qa, qs);
      }
    }
    // now dot on the 24 h dial
    {
      const qreal ang = global::dayMinutes(m_utc, global::userZone()) / 4.0;
      const QPointF d = polar(c, 198 * s, ang);
      p.setPen(Qt::NoPen);
      for (int k = 2; k >= 1; --k) { p.setBrush(theme::map(QColor(255, 220, 22, 14 * (3 - k)))); p.drawEllipse(d, (5 + 3 * k) * s, (5 + 3 * k) * s); }
      p.setBrush(theme::map("#ffdc16"));
      p.drawEllipse(d, 5 * s, 5 * s);
    }
    // dial ring and ticks
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme::map("#6a5a1a"), 1));
    p.drawEllipse(c, 144 * s, 144 * s);
    for (int m = 0; m < 60; ++m) {
      if (m % 5 == 0) continue;
      const qreal a = m * 6;
      p.setPen(QPen(theme::map("#6a5c2a"), 1));
      p.drawLine(polar(c, 135 * s, a), polar(c, 139.5 * s, a));
    }
    for (int h = 0; h < 12; ++h) {
      const qreal a = h * 30;
      const bool q = h % 3 == 0;
      QPen pen(theme::map("#dec558"), 2.2); pen.setCapStyle(Qt::FlatCap);
      p.setPen(pen);
      p.drawLine(polar(c, (q ? 123 : 127) * s, a), polar(c, 138 * s, a));
    }
    const QFont nf = theme::sans(qRound(18 * s));
    const QFontMetricsF fm(nf);
    p.setFont(nf);
    p.setPen(theme::map("#e7dec1"));
    const QStringList nums{"12", "3", "6", "9"};
    for (int k = 0; k < 4; ++k) {
      const QPointF at = polar(c, 103 * s, k * 90);
      const qreal w = fm.horizontalAdvance(nums[k]);
      p.drawText(QPointF(at.x() - w / 2, at.y() + fm.capHeight() / 2), nums[k]);
    }
  }
}

// Per-second layer: dial and arcs (m_over) + hands + digital time, so a sweep frame is two blits and a wedge.
void ClockPanel::rebuildTop() {
  if (m_over.isNull()) return;
  const qreal dpr = devicePixelRatioF();
  if (m_top.size() != size() * dpr || m_top.devicePixelRatio() != dpr) { m_top = QPixmap(size() * dpr); m_top.setDevicePixelRatio(dpr); }
  m_top.fill(Qt::transparent);   // reused every second (was a new 4 MB pixmap each time)
  QPainter p(&m_top);
  p.drawPixmap(0, 0, m_over);
  QPointF c; qreal s; geometryFor(c, s);
  p.setRenderHint(QPainter::Antialiasing);
  // hands (12 h analogue time in the user's zone)
  const QDateTime l = m_utc.toTimeZone(global::userZone());
  const QTime t = l.time();
  const qreal hAng = ((t.hour() % 12) + t.minute() / 60.0 + t.second() / 3600.0) * 30;
  const qreal mAng = (t.minute() + t.second() / 60.0) * 6;
  const qreal sAng = t.second() * 6;
  const QColor white = theme::map("#fbf6eb");
  look::glowLine(p, c, polar(c, 66 * s, hAng), white, 5 * s, 6 * s, 70);
  QPen hp(white, 5 * s); hp.setCapStyle(Qt::RoundCap);
  p.setPen(hp); p.drawLine(c, polar(c, 66 * s, hAng));
  look::glowLine(p, c, polar(c, 106 * s, mAng), white, 3 * s, 6 * s, 60);
  QPen mp(white, 3 * s); mp.setCapStyle(Qt::RoundCap);
  p.setPen(mp); p.drawLine(c, polar(c, 106 * s, mAng));
  look::glowLine(p, polar(c, 15 * s, sAng + 180), polar(c, 125 * s, sAng), theme::map("#e9d36c"), 1.5, 4, 50);
  QPen sp(theme::map("#f0d860"), 1.5 * qMax<qreal>(s, 0.8)); sp.setCapStyle(Qt::RoundCap);
  p.setPen(sp); p.drawLine(polar(c, 15 * s, sAng + 180), polar(c, 125 * s, sAng));
  p.setPen(QPen(theme::map("#fcd700"), 2));
  p.setBrush(theme::map("#050400"));
  p.drawEllipse(c, 6 * s, 6 * s);

  // digital time, date, status
  const qreal cx = width() / 2.0;
  const QFont fh = theme::mono(48, QFont::Bold, 3.2), fs = theme::mono(26, QFont::Bold, 2);
  const QString hm = l.toString("HH:mm"), ss = l.toString(":ss");
  const qreal w1 = look::width(hm, fh), w2 = look::width(ss, fs);
  const qreal x0 = cx - (w1 + w2) / 2;
  const qreal base = height() - 69;
  glowText(p, QPointF(x0, base), hm, fh, theme::map("#fffef9"), 26);
  glowText(p, QPointF(x0 + w1, base), ss, fs, theme::map("#f8dd55"), 30);
  const QString date = i18n::locale().toString(l.date(), QCoreApplication::translate("global", "dddd, d MMMM yyyy")).toUpper() + " · " + global::utcOffsetLabel(l);
  look::textCenter(p, cx, height() - 44, date, theme::mono(12, QFont::Normal, 2.6), theme::map("#999991"));
  look::textCenter(p, cx, height() - 20, global::statusLine(m_utc), theme::mono(10, QFont::Normal, 2.4), theme::map("#64635c"));

}

static QRectF wedgeBox(const QPointF& c, qreal r, qreal lead) {
  // the wedge starts at the centre: a 1 px box there (QRectF(c, 0×0) is null, and united() skips null rects, so
  // the centre side of the wedge was left out and its old frames stayed on screen until the next full repaint)
  QRectF b(c, QSizeF(1, 1));
  for (qreal a = lead - 36; a <= lead + 1; a += 6) b |= QRectF(polar(c, r, a), QSizeF(1, 1));
  b |= QRectF(polar(c, r, lead + 1), QSizeF(1, 1));
  return b.adjusted(-3, -3, 3, 3);
}

void ClockPanel::paintEvent(QPaintEvent* e) {
  m_dirty = QRect();
  QPainter p(this);
  p.setClipRegion(e->region());
  p.drawPixmap(0, 0, m_under);
  QPointF c; qreal s; geometryFor(c, s);
  p.setRenderHint(QPainter::Antialiasing);

  // radar sweep: 35° wedge, brightest at the leading edge, one turn per 7 s
  {
    const qreal lead = std::fmod(m_clock.elapsed() / 7000.0 * 360.0, 360.0);
    QConicalGradient g(c, 90 - lead);         // Qt: counter-clockwise from 3 o'clock
    g.setColorAt(0.0, theme::map(QColor(252, 215, 0, 34)));
    g.setColorAt(0.03, theme::map(QColor(252, 215, 0, 20)));
    g.setColorAt(35.0 / 360.0, theme::map(QColor(252, 215, 0, 0)));
    g.setColorAt(1.0, theme::map(QColor(252, 215, 0, 0)));
    QPainterPath wedge;
    const qreal r = 190 * s;
    wedge.moveTo(c);
    wedge.arcTo(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 90 - lead, 35);
    wedge.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawPath(wedge);
  }
  p.drawPixmap(0, 0, m_top);
  paintCursor(p);
}

void ClockPanel::paintCursor(QPainter& p) const {
  if (!look::cursorOn()) return;
  const QRect cr = cursorRect();
  look::cursorBlock(p, cr.x() + 1, 16);
}

// The title's blinking cursor repaints as a child of its own: its half-second update stays an 8×13 rectangle
// instead of stretching the clock's repaint (wedge, hands) up to the title. It draws what the panel draws there:
// the layers and the block (the wedge never reaches the title row).
class ClockCursor : public QWidget {
public:
  explicit ClockCursor(ClockPanel* panel) : QWidget(panel), m_panel(panel) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_TransparentForMouseEvents);
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.translate(-pos());
    m_panel->paintLayersAt(p);
  }
private:
  ClockPanel* m_panel;
};

void ClockPanel::paintLayersAt(QPainter& p) const {
  p.drawPixmap(0, 0, m_under);
  p.setRenderHint(QPainter::Antialiasing);
  p.drawPixmap(0, 0, m_top);
  paintCursor(p);
}

void ClockPanel::blinkCursor() {
  if (!m_cursor) { m_cursor = new ClockCursor(this); m_cursor->show(); }
  if (m_cursor->geometry() != cursorRect()) m_cursor->setGeometry(cursorRect());
  m_cursor->update();
}

// ================================================================= TimelinePanel
TimelinePanel::TimelinePanel(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_utc = global::nowUtc();
}
void TimelinePanel::setNow(const QDateTime& utc) { m_utc = utc; update(); }

void TimelinePanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  look::panel(p, QRectF(rect()), theme::map("#070503"), theme::map("#413c2f"));
  const QTimeZone& zone = global::userZone();
  const QDateTime l = m_utc.toTimeZone(zone);
  look::text(p, 11, 20, QCoreApplication::translate("global", "24-HOUR TIMELINE · YOUR TIME (%1)").arg(global::utcOffsetLabel(l)), theme::mono(10, QFont::Normal, 2.3),
             theme::map("#68655d"));

  // bars start after the longest session name and the OVERLAPS label (narrow panels: "NEW YORK" and the French
  // "CHEVAUCHEMENTS" touched the bars), at most a quarter of the panel; the reference keeps its 159 px
  const QFont nf = theme::mono(12, QFont::Normal, 2.5), bf = theme::mono(10, QFont::Medium, 0.4);
  const auto& ss = global::sessions();
  qreal nameW = 0;
  for (const auto& sn : ss) nameW = qMax(nameW, look::width(global::sessionName(sn).toUpper(), nf));
  QFont ovf = theme::mono(9, QFont::Normal, 2.5);
  const QString ovText = QCoreApplication::translate("global", "OVERLAPS");
  // phone (panel < 520 px): the names went under the bars' start; they move above their bar and the bars take the
  // whole width (the OVERLAPS label likewise above its row)
  const bool phone = width() < 520;
  const qreal x0 = phone ? 11 : qMin<qreal>(width() * 0.25, std::max({qMin<qreal>(159, width() * 0.15 + 20), 39 + nameW + 14, 11 + look::width(ovText, ovf) + 12}));
  const qreal x1 = width() - 12;
  if (!phone && 11 + look::width(ovText, ovf) + 8 > x0) ovf.setLetterSpacing(QFont::AbsoluteSpacing, 1);   // tighten before eliding
  const qreal ppm = (x1 - x0) / 1440.0;
  const qreal pitch = 55.6 * qBound(1.0, height() / 566.0, 1.3);
  // axis labels row: a third of the panel down as the reference, higher on short panels so the rows and the
  // overlaps row (≈ 3 pitches + 180 px under it) always fit
  const qreal top = qMax<qreal>(36, qMin<qreal>(height() * 0.327, height() - (3 * pitch + 180)));
  const qreal g0 = top + 9, rowsTop = g0;
  const qreal rowC0 = top + 38 + (phone ? 14 : 0);
  const qreal g1 = rowC0 + 3 * pitch + 29;                    // gridline bottom
  const auto overlaps = global::overlapsOnDay(l.date(), zone);
  const qreal ovC = g1 + 30;                                  // overlaps row centre

  // gridlines and hour labels
  p.setRenderHint(QPainter::Antialiasing, false);
  for (int h = 0; h <= 24; ++h) {
    const qreal x = std::round(x0 + h * 60 * ppm);
    p.setPen(QPen(theme::map(h % 3 == 0 ? "#272218" : "#1e1a0f"), 1));
    p.drawLine(QPointF(x + 0.5, rowsTop), QPointF(x + 0.5, g1));
  }
  p.setRenderHint(QPainter::Antialiasing);
  const QFont af = theme::mono(10);
  for (int h = 0; h <= 24; h += 3) {
    const QString s = QString("%1:00").arg(h, 2, 10, QChar('0'));
    // spread like the reference: 00:00 on its gridline, 24:00 ending on its gridline, the rest in between
    const qreal lw = look::width(s, af);
    look::text(p, x0 + h / 24.0 * (x1 - x0 - lw), top + 3.5, s, af, theme::map("#69665e"));
  }

  // rows
  for (int i = 0; i < ss.size(); ++i) {
    const qreal cy = rowC0 + i * pitch;
    if (!phone) {   // phone: the names go above their bar, after the now line (below)
      p.drawPixmap(QPointF(11, cy - 6.5), ui::flag(ss[i].flag, QSize(20, 13), devicePixelRatioF()));
      look::text(p, 39, cy + 4.5, global::sessionName(ss[i]).toUpper(), nf, theme::map("#a4a199"));
    }
    p.fillRect(QRectF(x0, cy - 10, x1 - x0, 20), theme::map("#19140a"));
    for (const auto& seg : global::segmentsOnDay(ss[i], l.date(), zone)) {
      const bool live = seg.span.contains(m_utc);
      const QRectF r(x0 + seg.startMin * ppm, cy - 10, (seg.endMin - seg.startMin) * ppm, 20);
      if (live) {
        for (int k = 3; k >= 1; --k) {
          QColor g = global::sessionColour(ss[i], global::Tone::Bright); g.setAlpha(22 * (4 - k));
          p.setPen(Qt::NoPen); p.setBrush(g);
          p.drawRoundedRect(r.adjusted(-2 * k, -2 * k, 2 * k, 2 * k), 2 + 2 * k, 2 + 2 * k);
        }
      }
      p.setPen(Qt::NoPen);
      p.setBrush(live ? global::sessionColour(ss[i], global::Tone::Bright) : global::sessionColour(ss[i], global::Tone::Bar));
      p.drawRoundedRect(r, 2, 2);
      const QString lbl = global::spanHours(seg.span, zone);
      if (look::width(lbl, bf) + 10 < r.width()) {
        p.save(); p.setClipRect(r);
        look::text(p, r.left() + 7, cy + 4, lbl, bf, live ? theme::mix(global::sessionColour(ss[i], global::Tone::Bright), theme::map("#000000"), 0.72) : global::sessionColour(ss[i], global::Tone::BarText));
        p.restore();
      }
    }
  }

  // overlaps row: windows where two or more centres trade together today
  if (phone) look::text(p, 11, ovC - 10, ovText, ovf, theme::map("#55524a"));
  else look::text(p, 11, ovC + 3.5, QFontMetricsF(ovf).elidedText(ovText, Qt::ElideRight, x0 - 8 - 11), ovf, theme::map("#55524a"));
  p.fillRect(QRectF(x0, ovC - 4, x1 - x0, 8), theme::map("#120f08"));
  if (overlaps.isEmpty()) {
    look::text(p, x0 + 8, ovC + 18, QCoreApplication::translate("global", "NO OVERLAP TODAY"), theme::mono(9, QFont::Normal, 2), theme::map("#4a473f"));
  }
  qreal lastRight[2] = {-1e9, -1e9};   // right end of the last label on each of the two label lines
  for (const auto& o : overlaps) {
    const QRectF r(x0 + o.startMin * ppm, ovC - 4, (o.endMin - o.startMin) * ppm, 8);
    const QDateTime d0(l.date(), QTime(0, 0), zone);
    const bool live = global::dayMinutes(l, zone) >= o.startMin && global::dayMinutes(l, zone) < o.endMin;
    QLinearGradient g(r.topLeft(), r.topRight());
    const int n = o.sessions.size();
    for (int k = 0; k < n; ++k) {
      const QColor c = global::sessionColour(ss[o.sessions[k]], live ? global::Tone::Bright : global::Tone::Bar);
      g.setColorAt(n == 1 ? 0 : double(k) / (n - 1), c);
    }
    p.setPen(Qt::NoPen); p.setBrush(g);
    p.drawRoundedRect(r, 2, 2);
    QStringList names; for (int k : o.sessions) names << ss[k].shortName;
    const QString lbl = names.join(" · ") + "  " + QString("%1:%2-%3:%4").arg(o.startMin / 60, 2, 10, QChar('0'))
        .arg(o.startMin % 60, 2, 10, QChar('0')).arg(o.endMin / 60, 2, 10, QChar('0')).arg(o.endMin % 60, 2, 10, QChar('0'));
    const QFont of = theme::mono(9, QFont::Normal, 1);
    // labels under their bar, pulled left to end inside the panel (the evening SYD · NY label ran past the border);
    // a label that would touch the previous one drops to a second line
    const qreal lw = look::width(lbl, of);
    const qreal lx = qMax(x0, qMin(r.left(), x1 - lw));
    const int line = lx <= lastRight[0] + 8 ? 1 : 0;
    look::text(p, lx, ovC + (line ? 31 : 18), lbl, of, live ? theme::map("#e9e3c8") : theme::map("#77735f"));
    lastRight[line] = lx + lw;
  }

  // now line
  const qreal xn = x0 + global::dayMinutes(l, zone) * ppm;
  look::glowLine(p, QPointF(xn, rowsTop), QPointF(xn, g1), theme::map("#f0d356"), 2, 6, 60);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.fillRect(QRectF(xn - 1, rowsTop, 2, g1 - rowsTop), theme::map("#f0d356"));
  // phone: name rows above the bars, on the panel's colour so the now line and the gridlines stop under them
  if (phone) {
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < ss.size(); ++i) {
      const qreal ny = rowC0 + i * pitch - 22;
      const QString n = global::sessionName(ss[i]).toUpper();
      p.fillRect(QRectF(7, ny - 10, 32 + look::width(n, nf) + 6, 20), theme::map("#070503"));
      p.drawPixmap(QPointF(11, ny - 6.5), ui::flag(ss[i].flag, QSize(20, 13), devicePixelRatioF()));
      look::text(p, 39, ny + 4.5, n, nf, theme::map("#a4a199"));
    }
  }
}

// ================================================================= SessionCard
SessionCard::SessionCard(int session, QWidget* parent) : QWidget(parent), m_idx(session) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_utc = global::nowUtc();
}
void SessionCard::setNow(const QDateTime& utc) { m_utc = utc; update(); }

void SessionCard::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& s = global::sessions()[m_idx];
  const QTimeZone tz(s.tz);
  const QTimeZone& user = global::userZone();
  const auto cur = global::currentSpan(s, m_utc);
  const bool open = cur.valid();
  // taller cards (big screens) scale their whole content uniformly; the reference card is 176 px tall
  const qreal k = qMax<qreal>(1.0, height() / 176.0);
  p.scale(k, k);
  const QRectF R(0, 0, width() / k, height() / k);
  const qreal W = R.width();

  // frame + top accent
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.fillRect(R, theme::map("#0c0601"));
  p.setPen(QPen(theme::map("#413a2d"), 1));
  p.drawRect(R.adjusted(0.5, 0.5, -0.5, -0.5));
  p.fillRect(QRectF(1, 0, W - 2, 4), open ? global::sessionColour(s, global::Tone::Bright) : global::sessionColour(s, global::Tone::Accent));
  p.restore();
  if (open) {
    QLinearGradient g(0, 4, 0, 40);
    g.setColorAt(0, theme::alpha(global::sessionColour(s, global::Tone::Bright), 40)); g.setColorAt(1, theme::alpha(global::sessionColour(s, global::Tone::Bright), 0));
    p.fillRect(QRectF(1, 4, W - 2, 36), g);
  }
  look::brackets(p, R.adjusted(0, 1, 0, 0));

  const qreal sc = qMin<qreal>(1.0, W / 385.0);
  // name row
  p.drawPixmap(QPointF(18, 25), ui::flag(s.flag, QSize(20, 13), devicePixelRatioF()));
  look::text(p, 48, 41, global::sessionName(s).toUpper(), theme::display(qRound(28 * qMax<qreal>(sc, 0.8)), QFont::Bold, 2), theme::map("#faf8f3"));
  // state pill
  {
    const QFont pf = theme::mono(11, QFont::Normal, 2.2);
    const QString t = open ? QCoreApplication::translate("global", "OPEN") : QCoreApplication::translate("global", "CLOSED");
    const qreal tw = look::width(t, pf) - 2.2;
    const QRectF pr(W - 18 - (tw + 30), 21, tw + 30, 21);
    p.setPen(QPen(open ? theme::alpha(global::sessionColour(s, global::Tone::Bright), 170) : theme::map("#3a372e"), 1));
    p.setBrush(open ? theme::alpha(global::sessionColour(s, global::Tone::Bright), 34) : theme::map("#0f0d09"));
    p.drawRoundedRect(pr.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p.setPen(Qt::NoPen);
    p.setBrush(open ? global::sessionColour(s, global::Tone::Bright) : theme::map("#5f5c55"));
    p.drawEllipse(QPointF(pr.left() + 11, pr.center().y()), 2.5, 2.5);
    look::text(p, pr.left() + 19, pr.center().y() + 3.5, t, pf, open ? theme::mix(global::sessionColour(s, global::Tone::Bright), theme::map("#ffffff"), 0.35) : theme::map("#8e8b84"));
  }
  // centre time
  const QDateTime lt = m_utc.toTimeZone(tz);
  look::text(p, 18, 74, QCoreApplication::translate("global", "%1 TIME").arg(global::sessionName(s).toUpper()), theme::mono(9, QFont::Normal, 2.5), theme::map("#615d56"));
  const QFont tf = theme::mono(qRound(42 * qMax<qreal>(sc, 0.75)), QFont::Bold, 1.2), sf = theme::mono(qRound(20 * qMax<qreal>(sc, 0.75)));
  const qreal adv = look::text(p, 16, 114, lt.toString("HH:mm"), tf, theme::map("#fffefb"));
  look::text(p, 16 + adv, 114, lt.toString(":ss"), sf, theme::map("#716e67"));
  // countdown
  const auto next = global::nextSpan(s, m_utc);
  const QString label = open ? QCoreApplication::translate("global", "CLOSES IN") : QCoreApplication::translate("global", "OPENS IN");
  const qint64 secs = open ? m_utc.secsTo(cur.close) : (next.valid() ? m_utc.secsTo(next.open) : 0);
  look::textRight(p, W - 18, 88, label, theme::mono(9, QFont::Normal, 2.5), theme::map("#635f58"));
  look::textRight(p, W - 18, 113, global::countdown(secs), theme::mono(qRound(18 * qMax<qreal>(sc, 0.8)), QFont::Bold, 0.9),
                  open ? theme::mix(global::sessionColour(s, global::Tone::Bright), theme::map("#ffffff"), 0.55) : theme::map("#fffefb"));
  // progress of the open session
  const QRectF track(18, 131, W - 36, 4);
  p.setPen(Qt::NoPen);
  p.setBrush(theme::map("#231e14"));
  p.drawRoundedRect(track, 2, 2);
  if (open) {
    const double f = qBound(0.0, double(cur.open.secsTo(m_utc)) / cur.open.secsTo(cur.close), 1.0);
    const QRectF done(track.left(), track.top(), track.width() * f, 4);
    p.setBrush(theme::alpha(global::sessionColour(s, global::Tone::Bright), 60));
    p.drawRoundedRect(done.adjusted(-1, -2, 1, 2), 3, 3);
    p.setBrush(global::sessionColour(s, global::Tone::Bright));
    p.drawRoundedRect(done, 2, 2);
  }
  // hours: centre's local hours and the same (current or next) session in the user's zone
  const auto shown = open ? cur : next;
  const QFont ff = theme::mono(11);
  look::text(p, 18, 156, QCoreApplication::translate("global", "%1 local").arg(s.open.toString("HH:mm") + "-" + s.close.toString("HH:mm")), ff, theme::map("#9b9690"));
  look::textRight(p, W - 18, 156, QCoreApplication::translate("global", "%1 your time").arg(global::spanHours(shown, user)), ff, theme::map("#9b9690"));
}
}
