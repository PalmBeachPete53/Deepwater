#include "pages/global/overview/StrengthPanel.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include "core/Settings.h"
#include "global/FrameClock.h"
#include "global/Look.h"
#include "global/Sessions.h"
#include "ui/Chart.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QDateTime>
#include <QHelpEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace look = global::look;
using global::FxStrength;

namespace overview {

// Timeframe buttons as displayed ("D" / "W" become "J" / "S" in French); FxStrength::timeframeLabels() stays the
// settings value.
static QStringList tfLabels() {
  static const char* const k[] = {QT_TRANSLATE_NOOP("global", "5m"), QT_TRANSLATE_NOOP("global", "15m"),
    QT_TRANSLATE_NOOP("global", "1H"), QT_TRANSLATE_NOOP("global", "4H"), QT_TRANSLATE_NOOP("global", "D"),
    QT_TRANSLATE_NOOP("global", "W")};
  QStringList out;
  for (const char* x : k) out << QCoreApplication::translate("global", x);
  return out;
}

static const int kDefaultBars = 160;
static const double kPulseMs = 6000;      // ring pulse period (reference ≈1 s in a 6x time-lapse)
static const char* kSettingTf = "global.overview.strengthTf";

StrengthPanel::StrengthPanel(QWidget* parent) : QWidget(parent), m_fx(new FxStrength(this)) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMouseTracking(true);
  const int tf = FxStrength::timeframeLabels().indexOf(settings::value(kSettingTf, "1H").toString());
  connect(m_fx, &FxStrength::changed, this, [this] {
    const int n = m_fx->size();
    if (!m_viewSet && n > 1) resetView();
    else if (n > 1 && m_lastN > 1 && n != m_lastN) {
      // keep the zoom; when the right edge sat on the last bar, follow the new bars
      if (m_x1 >= m_lastN - 1.5) { m_x0 += n - m_lastN; m_x1 += n - m_lastN; }
      if (m_x0 < 0 || m_x0 > n - 2) resetView();
    }
    m_lastN = n;
    m_dirty = true; update();
    static bool hooked = false;
    if (!hooked && m_fx->size() > 1 && qEnvironmentVariableIsSet("DW_STRENGTH_ZOOM")) {
      hooked = true;
      const int steps = qEnvironmentVariableIntValue("DW_STRENGTH_ZOOM");
      const double anchor = m_hover ? barAt(m_mouse.x()) : m_x1;
      for (int i = 0; i < std::abs(steps); ++i) zoomAt(anchor, steps > 0 ? 0.87 : 1.0 / 0.87);
    }
  });
  connect(m_fx, &FxStrength::progress, this, [this] { m_dirty = true; update(); });
  connect(m_fx, &FxStrength::ticked, this, [this] {
    const int n = m_fx->size();
    if (m_lastN > 1 && n > m_lastN && m_x1 >= m_lastN - 1.5) { m_x0 += n - m_lastN; m_x1 += n - m_lastN; }
    m_lastN = n;
    m_dirty = true; update();
  });
  // pulse at 15 fps on the page's frame clock (every other frame), in the same repaint pass as the tape and the globe
  connect(&global::FrameClock::instance(), &global::FrameClock::tick, this, [this](int frame) {
    if (!m_pulsing || frame % 2) return;
    QVector<QRectF> rs; pulseRects(rs);
    for (const auto& r : rs) update(r.toAlignedRect());
  });
  const QByteArray hv = qgetenv("DW_STRENGTH_HOVER");
  if (!hv.isEmpty()) {
    const auto xy = QString::fromLatin1(hv).split(',');
    if (xy.size() == 2) { m_mouse = QPointF(xy[0].toDouble(), xy[1].toDouble()); m_hover = true; }
  }
  m_fx->setTimeframe(FxStrength::Timeframe(tf < 0 ? FxStrength::H1 : tf));
}

void StrengthPanel::showEvent(QShowEvent*) { m_fx->setActive(true); m_pulsing = true; global::FrameClock::instance().acquire(this); }
void StrengthPanel::hideEvent(QHideEvent*) { m_fx->setActive(false); m_pulsing = false; global::FrameClock::instance().release(this); }
void StrengthPanel::resizeEvent(QResizeEvent*) { m_dirty = true; }
void StrengthPanel::preload() { m_fx->setActive(true); m_fx->setActive(false); }

// ---------------------------------------------------------------- geometry
QRectF StrengthPanel::plotRect() const { return QRectF(4, 70, width() - 111, height() - 111); }
// The crosshair follows the mouse over the plot and the subtitle row above it (reference t001.0, t004.0).
QRectF StrengthPanel::hoverRect() const { const QRectF p = plotRect(); return QRectF(p.left(), 42, p.width(), p.bottom() - 42); }
QList<QRectF> StrengthPanel::tfRects() const {
  QList<QRectF> out;
  const QFont f = theme::mono(11);
  qreal x = width() - 17;
  const QStringList labels = tfLabels();
  for (int i = labels.size() - 1; i >= 0; --i) {
    const qreal w = qMax<qreal>(look::width(labels[i], f) + 12, 20);
    out.prepend(QRectF(x - w, 10, w, 20));
    x -= w + 3;
  }
  return out;
}
QRectF StrengthPanel::dropdownRect() const {
  const auto t = tfRects();
  const qreal w = width() >= 700 ? 110 : 76;      // narrower on small windows so the title keeps its room
  return QRectF(t.first().left() - 10 - w, 10, w, 20);
}
QString StrengthPanel::title() const {
  const QString full = QCoreApplication::translate("global", "CURRENCY STRENGTH · UNWEIGHTED");
  return 11 + look::width(full, look::titleFont()) + 24 < dropdownRect().left() ? full : QCoreApplication::translate("global", "CURRENCY STRENGTH");
}
QRectF StrengthPanel::infoRect() const {
  const qreal w = look::width(title(), look::titleFont());
  return QRectF(13 + w, 11, 14, 16);
}

void StrengthPanel::resetView() {
  const int n = m_fx->size();
  if (n < 2) return;
  m_x1 = n - 1;
  // same bar width as the reference (160 bars on a 643 px plot): wider charts show more history
  const double bars = kDefaultBars * qMax(1.0, plotRect().width() / 643.0);
  m_x0 = qMax(0.0, m_x1 - bars);
  m_viewSet = true;
}

double StrengthPanel::xOf(double bar) const {
  const QRectF p = plotRect();
  return p.left() + (bar - m_x0) / qMax(1e-9, m_x1 - m_x0) * p.width();
}
double StrengthPanel::barAt(double x) const {
  const QRectF p = plotRect();
  return m_x0 + (x - p.left()) / p.width() * (m_x1 - m_x0);
}

StrengthPanel::Vis StrengthPanel::visible() const {
  Vis v;
  const int n = m_fx->size();
  if (n < 2 || !m_viewSet) return v;
  v.first = qBound(0, int(std::ceil(m_x0 - 1e-9)), n - 1);
  v.last = qBound(0, int(std::floor(m_x1 + 1e-9)), n - 1);
  if (v.last <= v.first) return v;
  double lo = 0, hi = 0;
  for (int c = 0; c < 8; ++c)
    for (int i = v.first; i <= v.last; ++i) {
      const double y = m_fx->value(c, v.first, i);
      if (!std::isfinite(y)) continue;
      lo = qMin(lo, y); hi = qMax(hi, y);
    }
  if (hi - lo < 0.02) { hi += 0.01; lo -= 0.01; }
  const double pad = (hi - lo) * 0.08;
  v.lo = lo - pad; v.hi = hi + pad;
  v.ok = true;
  return v;
}
double StrengthPanel::yOf(double val, const Vis& v) const {
  const QRectF p = plotRect();
  return p.bottom() - (val - v.lo) / (v.hi - v.lo) * p.height();
}

QString StrengthPanel::barTime(int i, bool pill) const {
  const bool daily = m_fx->timeframe() >= FxStrength::D1;
  const QDateTime t = daily ? QDateTime::fromSecsSinceEpoch(m_fx->time(i), QTimeZone::UTC)
                            : QDateTime::fromSecsSinceEpoch(m_fx->time(i), global::userZone());
  const QLocale en = i18n::locale();
  if (pill) return daily ? en.toString(t, "dd MMM ''yy") : en.toString(t, "dd MMM ''yy   HH:mm");
  return daily ? en.toString(t, "dd MMM yyyy").toUpper() : en.toString(t, "dd MMM, HH:mm").toUpper();
}

void StrengthPanel::zoomAt(double anchor, double factor) {
  const int n = m_fx->size();
  if (n < 2) return;
  const double span = m_x1 - m_x0;
  const double maxSpan = qMin(1100.0, qMax(60.0, n * 1.35));
  const double ns = qBound(qMin(40.0, n - 1.0), span * factor, maxSpan);
  double x0 = anchor - (anchor - m_x0) * ns / span;
  double x1 = x0 + ns;
  if (x0 < 0) { x0 = 0; x1 = ns; }
  const double maxRight = n - 1 + ns * 0.6;          // zooming out may open an empty "future" area
  if (x1 > maxRight) { x1 = maxRight; x0 = qMax(0.0, x1 - ns); }
  m_x0 = x0; m_x1 = x1;
  m_dirty = true;
  update();
}

void StrengthPanel::pulseRects(QVector<QRectF>& out) const {
  for (const auto& p : m_ends) if (std::isfinite(p.x())) out << QRectF(p.x() - 14, p.y() - 14, 28, 28);
}

// ---------------------------------------------------------------- static layer
void StrengthPanel::rebuild() {
  const qreal dpr = devicePixelRatioF();
  m_cache = QPixmap(size() * dpr);
  m_cache.setDevicePixelRatio(dpr);
  QPainter p(&m_cache);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF R(rect());
  look::panel(p, R, theme::map("#080502"));
  // header
  p.save();
  look::text(p, 11, 24, title(), look::titleFont(), theme::map("#ebd88a"));
  look::infoIcon(p, infoRect().center() + QPointF(-1, 0.5));
  const QRectF dd = dropdownRect();
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#231d07"));
  p.drawRoundedRect(dd, 2, 2);
  look::text(p, dd.left() + 10, dd.center().y() + 4, "FOREX", theme::mono(11, QFont::Bold), theme::map("#f4d95d"));
  QPen cp(theme::map("#f4d95d"), 1.6); cp.setCapStyle(Qt::RoundCap); cp.setJoinStyle(Qt::RoundJoin);
  p.setPen(cp); p.setBrush(Qt::NoBrush);
  const QPointF cv(dd.right() - 11, dd.center().y());
  p.drawPolyline(QPolygonF{cv + QPointF(-4, -2), cv + QPointF(0, 2), cv + QPointF(4, -2)});
  const auto rs = tfRects();
  const QStringList labels = tfLabels();
  for (int i = 0; i < rs.size(); ++i) {
    const bool on = i == m_fx->timeframe();
    if (on) { p.setPen(Qt::NoPen); p.setBrush(theme::map("#fcd700")); p.drawRoundedRect(rs[i], 2, 2); }
    const bool hov = !on && i == m_hoverTf;   // hover: white bold (reference t005.0)
    look::textCenter(p, rs[i].center().x(), rs[i].center().y() + 4, labels[i], theme::mono(11, on || hov ? QFont::Bold : QFont::Normal),
                     on ? theme::onAccent() : hov ? theme::map("#f4f1ea") : theme::map("#99968e"));
  }
  p.restore();

  const QRectF pr = plotRect();
  m_vis = visible();   // the view's range, kept for the frames painted until the next rebuild
  const Vis v = m_vis;
  // subtitle
  const QFont sf = theme::mono(10, QFont::Normal, 2.0);
  qreal sx = 11;
  sx += look::text(p, sx, 60, QCoreApplication::translate("global", "% CHANGE SINCE "), sf, theme::map("#a9a7a0"));
  sx += look::text(p, sx, 60, QCoreApplication::translate("global", "THE FIRST BAR ON SCREEN"), sf, theme::map("#f4d95d"));
  m_ends.clear();

  if (!v.ok) {
    const QString msg = !m_fx->error().isEmpty() ? m_fx->error().toUpper()
                        : QCoreApplication::translate("global", "LOADING %1/%2 PAIRS…").arg(m_fx->loaded()).arg(m_fx->total());
    look::textCenter(p, pr.center().x() + 40, pr.center().y(), msg, theme::mono(10, QFont::Normal, 2.5),
                     m_fx->error().isEmpty() ? theme::map("#605c55") : theme::map("#e0666b"));
    m_dirty = false;
    return;
  }
  const int n = m_fx->size();
  const auto ccys = FxStrength::currencies();

  // end labels: sorted by value, pushed apart (17 px pitch, no gap); placed first so the axis labels can avoid them
  struct Lab { int c; double v; double y; };
  QList<Lab> labs;
  const int endBar = qMin(n - 1, v.last);
  for (int c = 0; c < ccys.size(); ++c) {
    const double val = m_fx->value(c, v.first, endBar);
    if (std::isfinite(val)) labs << Lab{c, val, yOf(val, v)};
  }
  std::sort(labs.begin(), labs.end(), [](const Lab& a, const Lab& b) { return a.y < b.y; });
  const double h = 17;
  for (int i = 1; i < labs.size(); ++i) labs[i].y = qMax(labs[i].y, labs[i - 1].y + h);
  const double bottom = pr.bottom() + 6;
  if (!labs.isEmpty() && labs.last().y + h / 2 > bottom) {
    labs.last().y = bottom - h / 2;
    for (int i = labs.size() - 2; i >= 0; --i) labs[i].y = qMin(labs[i].y, labs[i + 1].y - h);
  }
  // y axis labels: an axis label the end labels would half cover is left out (it peeked out beside the tags)
  const QFont af = theme::mono(11);
  const auto ticks = chart::niceTicks(v.lo, v.hi, 5);
  for (double t : ticks) {
    const double y = yOf(t, v);
    if (y < pr.top() + 4 || y > pr.bottom() - 2) continue;
    bool covered = false;
    for (const auto& l : labs) if (std::abs(std::round(l.y - h / 2) + h / 2 - y) < h / 2 + 7) { covered = true; break; }
    if (covered) continue;
    look::textRight(p, width() - 22, y + 4, fmt::num(t, 2) + "%", af, theme::map("#928e88"));
  }
  // zero line
  {
    const double y0 = std::round(yOf(0, v)) + 0.5;
    p.save(); p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(theme::map("#8f8c86"), 1));
    p.drawLine(QPointF(pr.left(), y0), QPointF(width() - 72, y0));
    p.restore();
  }
  // x axis labels: most significant boundaries first, at least 70 px apart
  {
    const bool daily = m_fx->timeframe() >= FxStrength::D1;
    const QTimeZone tz = daily ? QTimeZone(QTimeZone::UTC) : global::userZone();
    struct Cand { int i; int level; QString text; };
    QList<Cand> cands;
    const int a = v.first, b = qMin(n - 1, int(std::floor(m_x1)));
    const double pxPerDay = 86400.0 / FxStrength::timeframeSeconds(m_fx->timeframe()) * pr.width() / qMax(1e-9, m_x1 - m_x0);
    QDateTime prev = QDateTime::fromSecsSinceEpoch(m_fx->time(qMax(0, a - 1)), tz);
    const QLocale en = i18n::locale();
    for (int i = a; i <= b; ++i) {
      const QDateTime t = QDateTime::fromSecsSinceEpoch(m_fx->time(i), tz);
      if (i == 0) { prev = t; continue; }
      int level = -1; QString text;
      if (t.date().year() != prev.date().year()) { level = 6; text = QString::number(t.date().year()); }
      else if (t.date().month() != prev.date().month()) { level = 5; text = en.toString(t.date(), "MMM"); }
      else if (m_fx->timeframe() == FxStrength::W1) { level = -1; }
      else if (t.date() != prev.date()) {
        level = daily ? (t.date().dayOfWeek() < prev.date().dayOfWeek() ? 4 : 2) : 4;
        text = QString::number(t.date().day());
      } else if (!daily) {
        const int h = t.time().hour(), m = t.time().minute();
        if (t.time().hour() != prev.time().hour() || m == 0) {
          level = h % 12 == 0 && m == 0 ? 3 : h % 6 == 0 && m == 0 ? 2 : h % 3 == 0 && m == 0 ? 1 : m == 0 ? 0 : -1;
        }
        if (level < 0 && m % 30 == 0 && m != prev.time().minute()) level = 0;
        if (level < 0 && m % 15 == 0) level = 0;
        text = t.toString("HH:mm");
        if (level >= 0 && m != 0 && m % 30 != 0) level = -1;
      }
      prev = t;
      if (level == 4) {
        // a short day (Sunday evening open) yields to the next full day
        int len = 0;
        const QDate d = t.date();
        for (int k = i; k < n && len < 99; ++k, ++len)
          if (QDateTime::fromSecsSinceEpoch(m_fx->time(k), tz).date() != d) break;
        const int perDay = 86400 / FxStrength::timeframeSeconds(m_fx->timeframe());
        if (len < perDay / 3) level = 1;
      }
      // hours only when zoomed in enough for them to read as hours (≥ 200 px per day), as the reference does
      if (level >= 0 && level < 4 && !daily && pxPerDay < 200) level = -1;
      if (level >= 0) cands << Cand{i, level, text};
    }
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) { return x.level > y.level; });
    QList<qreal> placed;
    for (const auto& c : cands) {
      const qreal x = xOf(c.i);
      if (x < pr.left() + 8 || x > pr.right() - 8) continue;
      bool ok = true;
      for (qreal q : placed) if (std::abs(q - x) < 70) { ok = false; break; }
      if (!ok) continue;
      placed << x;
      look::textCenter(p, x, height() - 17, c.text, af, theme::map("#8d8a83"));
    }
  }
  // lines
  const int drawLast = qMin(n - 1, int(std::ceil(m_x1)));
  p.save();
  p.setClipRect(pr.adjusted(0, -2, 0, 2));
  for (int c = ccys.size() - 1; c >= 0; --c) {
    QPolygonF poly;
    for (int i = v.first; i <= drawLast; ++i) {
      const double y = m_fx->value(c, v.first, i);
      if (std::isfinite(y)) poly << QPointF(xOf(i), yOf(y, v));
    }
    QPen pen(theme::currencyColor(ccys[c]), 1.8); pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPolyline(poly);
  }
  p.restore();
  // end labels (placed above)
  const QFont lf = theme::mono(11, QFont::Bold);
  const qreal lx = width() - 104;
  for (const auto& l : labs) {
    const QColor col = theme::currencyColor(ccys[l.c]);
    const bool light = col.lightnessF() > 0.6;
    const QColor txt = light ? QColor("#3a3200") : QColor("#ffffff");
    const QRectF box(lx, std::round(l.y - h / 2), 88, h);
    p.setPen(Qt::NoPen); p.setBrush(col);
    p.drawRoundedRect(box, 1, 1);
    p.fillRect(QRectF(lx + 32, box.top(), 1, h), theme::alpha(QColor("#000000"), 120));
    look::text(p, lx + 4, box.center().y() + 4, ccys[l.c], lf, txt);
    look::textRight(p, box.right() - 4, box.center().y() + 4, fmt::num(l.v, 2) + "%", lf, txt);
  }
  // last points (pulsing rings are drawn per frame)
  if (xOf(n - 1) < pr.right() - 10) {      // rings only when the last bar is inside the plot
    for (int c = 0; c < ccys.size(); ++c) {
      const double val = m_fx->value(c, v.first, n - 1);
      m_ends << (std::isfinite(val) ? QPointF(xOf(n - 1), yOf(val, v)) : QPointF(NAN, NAN));
    }
  }
  m_dirty = false;
}

// ---------------------------------------------------------------- paint
void StrengthPanel::paintEvent(QPaintEvent* e) {
  if (m_dirty || m_cache.size() != size() * devicePixelRatioF()) rebuild();
  QPainter p(this);
  p.setClipRegion(e->region());
  p.drawPixmap(0, 0, m_cache);
  p.setRenderHint(QPainter::Antialiasing);
  const auto ccys = FxStrength::currencies();
  const Vis v = m_vis;   // set by rebuild(), which every change of data or view goes through (m_dirty)
  if (!v.ok) return;
  const QRectF pr = plotRect();

  // pulsing rings + dots at the last points
  const double ph = std::fmod(double(QDateTime::currentMSecsSinceEpoch()), kPulseMs) / kPulseMs;
  for (int c = 0; c < m_ends.size() && c < ccys.size(); ++c) {
    const QColor col = theme::currencyColor(ccys[c]);
    const QPointF at = m_ends[c];
    if (!std::isfinite(at.x())) continue;
    p.setPen(QPen(theme::alpha(col, 128), 1.5)); p.setBrush(theme::map(QColor(10, 10, 10, 200)));
    p.drawEllipse(at, 9.0, 9.0);
    const double r = 9 + 5 * ph;
    p.setPen(QPen(theme::alpha(col, int(110 * (1 - ph))), 1.2)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(at, r, r);
    p.setPen(Qt::NoPen); p.setBrush(col);
    p.drawEllipse(at, 3.0, 3.0);
  }

  // crosshair
  if (!m_hover || !hoverRect().contains(m_mouse)) return;
  // above the plot (subtitle row) the crosshair stays and the y pill reads the extrapolated value (t001.0)
  const qreal mouseY = m_mouse.y();
  const int n = m_fx->size();
  const int hb = qBound(v.first, int(std::lround(barAt(m_mouse.x()))), qMin(n - 1, v.last));
  const qreal x = std::round(xOf(hb)) + 0.5;
  QPen vp(theme::map(QColor(200, 176, 74, 155)), 1); vp.setDashPattern({4, 3});
  p.setPen(vp);
  p.drawLine(QPointF(x, pr.top()), QPointF(x, pr.bottom() + 8));
  QPen hp(theme::map("#6b5a1e"), 1); hp.setDashPattern({4, 4});
  p.setPen(hp);
  const qreal my = std::round(mouseY) + 0.5;
  p.drawLine(QPointF(pr.left(), my), QPointF(width() - 12, my));
  for (int c = 0; c < ccys.size(); ++c) {
    const double val = m_fx->value(c, v.first, hb);
    if (!std::isfinite(val)) continue;
    p.setPen(QPen(theme::map("#0a0a0a"), 1)); p.setBrush(theme::currencyColor(ccys[c]));
    p.drawEllipse(QPointF(x, yOf(val, v)), 4, 4);
  }
  // x pill
  const QFont pf = theme::mono(11, QFont::Bold);
  const QString xt = barTime(hb, true);
  const qreal pw = look::width(xt, pf) + 20;
  const qreal px = qBound(pr.left(), x - pw / 2, pr.right() - pw);
  const QRectF xr(px, height() - 32, pw, 22);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#4b3d0a"));
  p.drawRoundedRect(xr, 2, 2);
  look::text(p, xr.left() + 10, xr.center().y() + 4, xt, pf, theme::map("#f2eedc"));
  // y pill on the value axis
  const double yv = v.lo + (pr.bottom() - mouseY) / pr.height() * (v.hi - v.lo);
  const QString yt = fmt::num(yv, 2) + "%";
  const QRectF yr(width() - 70, my - 9, 58, 18);
  p.setPen(QPen(theme::map("#8a7a3a"), 1)); p.setBrush(theme::map("#4c3c04"));
  p.drawRoundedRect(yr.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  look::textRight(p, yr.right() - 5, yr.center().y() + 4, yt, pf, theme::map("#ffffff"));
  // subtitle echo of the hovered bar
  const QFont sf = theme::mono(10, QFont::Normal, 2.0);
  const qreal sx = 11 + look::width(QCoreApplication::translate("global", "% CHANGE SINCE ") + QCoreApplication::translate("global", "THE FIRST BAR ON SCREEN"), sf);
  look::text(p, sx + 4, 60, "· " + barTime(hb, false), sf, theme::map("#fffef9"));
}

// ---------------------------------------------------------------- input
void StrengthPanel::mouseMoveEvent(QMouseEvent* e) {
  const bool was = m_hover;
  m_mouse = e->position();
  m_hover = hoverRect().contains(m_mouse);
  bool overControl = dropdownRect().contains(m_mouse);
  int tf = -1;
  const auto rs = tfRects();
  for (int i = 0; i < rs.size(); ++i) if (rs[i].adjusted(-2, -4, 2, 4).contains(m_mouse)) tf = i;
  overControl |= tf >= 0;
  if (tf != m_hoverTf) { m_hoverTf = tf; m_dirty = true; update(); }
  setCursor(overControl ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (m_hover || was) update();
}
void StrengthPanel::leaveEvent(QEvent*) {
  if (qEnvironmentVariableIsSet("DW_STRENGTH_HOVER")) return;
  if (m_hover || m_hoverTf >= 0) { m_hover = false; m_hoverTf = -1; m_dirty = true; update(); }
}
void StrengthPanel::mousePressEvent(QMouseEvent* e) {
  const auto rs = tfRects();
  for (int i = 0; i < rs.size(); ++i) {
    if (!rs[i].contains(e->position())) continue;
    if (i != m_fx->timeframe()) {
      settings::setValue(kSettingTf, FxStrength::timeframeLabels()[i]);
      m_viewSet = false;
      m_fx->setTimeframe(FxStrength::Timeframe(i));
      m_dirty = true; update();
    }
    return;
  }
  if (dropdownRect().contains(e->position())) {
    QMenu menu(this);
    auto* a = menu.addAction(QCoreApplication::translate("global", "FOREX · 8 majors, 28 pairs"));
    a->setCheckable(true); a->setChecked(true);
    menu.exec(mapToGlobal(dropdownRect().bottomLeft().toPoint() + QPoint(0, 2)));
  }
}
void StrengthPanel::mouseDoubleClickEvent(QMouseEvent* e) {
  if (plotRect().contains(e->position())) { resetView(); m_dirty = true; update(); }
}
void StrengthPanel::wheelEvent(QWheelEvent* e) {
  if (!plotRect().contains(e->position()) || m_fx->size() < 2) { e->ignore(); return; }
  const double steps = e->angleDelta().y() / 120.0;
  if (steps == 0) return;
  zoomAt(barAt(e->position().x()), std::pow(0.87, steps));
  m_mouse = e->position(); m_hover = true;
  e->accept();
}
bool StrengthPanel::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    if (infoRect().contains(he->pos())) {
      QToolTip::showText(he->globalPos(),
        QCoreApplication::translate("global", "Strength of each major currency: the unweighted mean of its % moves against the seven others,\n"
        "measured from the first bar on screen (zooming re-bases every line at 0 %).\n%1\n"
        "The eight values sum to about zero. Double-click the chart to reset the zoom.").arg(m_fx->sourceNote()), this);
    } else {
      QToolTip::hideText();
    }
    return true;
  }
  return QWidget::event(e);
}
}  // namespace overview
