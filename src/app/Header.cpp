#include "app/Header.h"
#include "app/Pages.h"
#include "ui/Theme.h"
#include "core/I18n.h"
#include <QToolTip>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QMouseEvent>
#include <QDateTime>
#include <QTimeZone>
#include <cmath>
#include <iterator>

namespace {
constexpr int kRow1 = 44;
QFont mainFont(qreal sp = 2.4) { return theme::mono(12, QFont::Bold, sp); }
QFont subFont(qreal sp = 1.2, int px = 11) { return theme::mono(px, QFont::Medium, sp); }
QFont wordFont() { return theme::mono(15, QFont::ExtraBold, 4.5); }
QFont clockFont() { return theme::mono(12, QFont::Medium, 0.5); }
QFont utcFont() { return theme::mono(11); }
constexpr qreal kWordX = 54;
constexpr qreal kGearW = 34;   // gear button at the right end of the first row

// What the header shows at a given width, most complete first (laptops at 1024–1366 px drop the extras).
struct Fit { bool word; int clock; qreal pad; qreal sp; };   // clock: 2 utc+date+time, 1 date+time, 0 time
const Fit kFits[] = {{true, 2, 30, 2.4}, {true, 1, 30, 2.4}, {true, 1, 20, 1.6}, {true, 0, 20, 1.6},
                     {false, 0, 18, 1.4}, {false, 0, 12, 0.8}};
qreal clockWidth(int mode) {
  const QFontMetricsF fc(clockFont());
  if (mode == 0) return fc.horizontalAdvance("00:00:00");
  const qreal local = fc.horizontalAdvance("Mon 00 Mon  00:00:00");
  return mode == 1 ? local : local + 16 + QFontMetricsF(utcFont()).horizontalAdvance("UTC 00:00");
}
qreal tabsStart(bool word) { return word ? kWordX + QFontMetricsF(wordFont()).horizontalAdvance("DEEPWATER") + 30 : 58; }
qreal tabsWidth(qreal pad, qreal sp) {
  const QFontMetricsF fm(mainFont(sp)); qreal w = 0;
  for (const auto& t : app::tabs()) w += fm.horizontalAdvance(app::label(t.label).toUpper()) + pad + 4;
  return w;
}
Fit fitFor(qreal width) {
  for (const auto& f : kFits)
    if (tabsStart(f.word) + tabsWidth(f.pad, f.sp) + 24 <= width - 16 - kGearW - clockWidth(f.clock)) return f;
  return kFits[std::size(kFits) - 1];
}
struct SubFit { qreal pad; qreal sp; int px; };
SubFit subFitFor(const QList<app::SubTab>& subs, qreal width) {
  const SubFit fits[] = {{24, 1.2, 11}, {16, 0.8, 11}, {12, 0.4, 10}};
  for (const auto& f : fits) {
    const QFontMetricsF fs(subFont(f.sp, f.px)); qreal w = 16;
    for (const auto& s : subs) w += fs.horizontalAdvance(app::label(s.label).toUpper()) + f.pad + 4;
    if (w <= width - 16) return f;
  }
  return fits[2];
}
// The layout (fits and tab rectangles) depends on the width, the current tab, the language (labels) and the theme
// (fonts) only: measured once per change, not on every clock tick (1 Hz) and mouse move (up to 6 font sets × 10 labels).
struct Layout { int width = -1, main = -1, rev = -1; QString lang; Fit fit; SubFit sfit; QList<Header::Hit> hits; };
Layout& layoutMemo() { static Layout l; return l; }
// the options gear (static: three path booleans per paint before)
const QPainterPath& gearPath() {
  static const QPainterPath gear = [] {
    QPainterPath teeth;
    for (int i = 0; i < 8; ++i) {
      QTransform tr; tr.rotate(i * 45.0);
      teeth.addPolygon(tr.map(QPolygonF(QRectF(-1.9, -8.2, 3.8, 3.6))));
    }
    QPainterPath ring; ring.addEllipse(QPointF(0, 0), 5.9, 5.9);
    QPainterPath hole; hole.addEllipse(QPointF(0, 0), 2.4, 2.4);
    return teeth.united(ring).subtracted(hole);
  }();
  return gear;
}
}

Header::Header(QWidget* parent) : QWidget(parent) {
  setFixedHeight(kHeight);
  setMouseTracking(true);
  for (int i = 0; i < app::tabs().size(); ++i) m_lastSub << 0;
  m_clock.setInterval(1000);
  connect(&m_clock, &QTimer::timeout, this, [this] { update(QRect(width() - 360, 0, 360, kRow1)); });
  m_clock.start();
}

void Header::setCurrent(int main, int sub) {
  m_main = main; m_sub = sub;
  if (main >= 0 && main < m_lastSub.size()) m_lastSub[main] = sub;
  update();
}

QList<Header::Hit> Header::hits() const {
  Layout& m = layoutMemo();
  if (m.width == width() && m.main == m_main && m.rev == theme::revision() && m.lang == i18n::code()) return m.hits;
  QList<Hit> out;
  const auto& t = app::tabs();
  const Fit f = fitFor(width());
  QFontMetricsF fm(mainFont(f.sp));
  qreal x = tabsStart(f.word);
  for (int i = 0; i < t.size(); ++i) {
    const qreal w = fm.horizontalAdvance(app::label(t[i].label).toUpper()) + f.pad;
    out << Hit{QRectF(x, 0, w, kRow1), i, -1};
    x += w + 4;
  }
  const auto& subs = t[m_main].subs;
  const SubFit sf = subFitFor(subs, width());
  QFontMetricsF fs(subFont(sf.sp, sf.px));
  x = 16;
  for (int j = 0; j < subs.size(); ++j) {
    const qreal w = fs.horizontalAdvance(app::label(subs[j].label).toUpper()) + sf.pad;
    out << Hit{QRectF(x, kRow1 + 4, w, kHeight - kRow1 - 8), m_main, j};
    x += w + 4;
  }
  m = Layout{width(), m_main, theme::revision(), i18n::code(), f, sf, out};
  return out;
}

void Header::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& c = theme::pal();
  p.fillRect(rect(), theme::map("#050301"));
  p.fillRect(QRect(0, kRow1, width(), kHeight - kRow1), theme::map("#090603"));
  p.fillRect(QRect(0, kRow1 - 1, width(), 1), c.border);
  p.fillRect(QRect(0, kHeight - 1, width(), 1), c.border);

  // wordmark: the Deepwater logo (resources/icon/deepwater.svg, cropped to its icon tile), then DEEPWATER
  {
    static QPixmap logo;
    static int logoRev = -1;
    const qreal dpr = devicePixelRatioF();
    constexpr int kLogo = 30;
    if (logo.isNull() || logo.devicePixelRatio() != dpr || logoRev != theme::revision()) {
      logoRev = theme::revision();
      QSvgRenderer svg(QStringLiteral(":/icon/deepwater.svg"));
      svg.setViewBox(QRectF(100, 100, 824, 824));          // the tile, without the icon-grid margins
      logo = QPixmap(QSize(kLogo, kLogo) * dpr);
      logo.setDevicePixelRatio(dpr);
      logo.fill(Qt::transparent);
      QPainter lp(&logo);
      lp.setRenderHint(QPainter::Antialiasing);
      svg.render(&lp, QRectF(0, 0, kLogo, kLogo));
      lp.end();
      if (!theme::isDefault()) {   // the amber logo takes the theme's colours
        QImage img = theme::mapImage(logo.toImage());
        logo = QPixmap::fromImage(img); logo.setDevicePixelRatio(dpr);
      }
    }
    p.drawPixmap(QPointF(14, (kRow1 - kLogo) / 2.0), logo);
  }
  const auto hs = hits();   // fills layoutMemo()
  const Fit fit = layoutMemo().fit;
  const SubFit sfit = layoutMemo().sfit;
  if (fit.word) { p.setFont(wordFont()); p.setPen(c.text); p.drawText(QPointF(kWordX, kRow1 / 2.0 + 5.5), "DEEPWATER"); }

  const auto& t = app::tabs();
  for (const auto& h : hs) {
    if (h.sub < 0) {
      const bool on = h.main == m_main, hov = h.main == m_hoverMain;
      p.setFont(mainFont(fit.sp));
      p.setPen(on ? c.accent : hov ? c.text : c.textDim);
      p.drawText(h.r, Qt::AlignCenter, app::label(t[h.main].label).toUpper());
      if (on) p.fillRect(QRectF(h.r.left() + 10, kRow1 - 3, h.r.width() - 20, 2), c.accent);
    } else {
      const bool on = h.sub == m_sub, hov = h.sub == m_hoverSub;
      if (on) { p.setPen(QPen(theme::alpha(c.accent, 110), 1)); p.setBrush(c.accentDim); p.drawRoundedRect(h.r.adjusted(.5, .5, -.5, -.5), 2, 2); }
      else if (hov) { p.setPen(Qt::NoPen); p.setBrush(c.panelAlt); p.drawRoundedRect(h.r, 2, 2); }
      p.setFont(subFont(sfit.sp, sfit.px));
      p.setPen(on ? c.accent : hov ? c.text : c.textDim);
      p.drawText(h.r, Qt::AlignCenter, app::label(t[h.main].subs[h.sub].label).toUpper());
    }
  }
  // description of the current page, right of the sub-tabs
  if (!hs.isEmpty()) {
    const qreal x0 = hs.last().r.right() + 24;
    const QRectF dr(x0, kRow1, width() - x0 - 16, kHeight - kRow1);
    if (dr.width() > 120) {
      p.setFont(theme::sans(12)); p.setPen(c.textMute);
      const QString d = QFontMetricsF(p.font()).elidedText(app::label(t[m_main].subs[m_sub].description), Qt::ElideRight, dr.width());
      p.drawText(dr, Qt::AlignVCenter | Qt::AlignRight, d);
    }
  }
  // clock: local time (+ date, + UTC when there is room)
  const QDateTime now = QDateTime::currentDateTime();
  p.setFont(clockFont()); p.setPen(c.text);
  const QString local = fit.clock == 0 ? now.toString("HH:mm:ss")
                                       : i18n::locale().toString(now, "ddd dd MMM  HH:mm:ss");
  const qreal wl = QFontMetricsF(p.font()).horizontalAdvance(local);
  const qreal right = width() - 16 - kGearW;
  p.drawText(QPointF(right - wl, kRow1 / 2.0 + 4.5), local);
  if (fit.clock == 2) {
    const QString utc = "UTC " + now.toUTC().toString("HH:mm");
    p.setFont(utcFont()); p.setPen(c.textMute);
    const qreal wu = QFontMetricsF(p.font()).horizontalAdvance(utc);
    p.drawText(QPointF(right - wl - 16 - wu, kRow1 / 2.0 + 4.5), utc);
  }
  // options gear
  {
    const QRectF g = gearRect();
    if (m_hoverGear) { p.setPen(Qt::NoPen); p.setBrush(c.panelAlt); p.drawRoundedRect(g, 3, 3); }
    const QColor col = m_hoverGear ? c.accent : c.textDim;
    const QPointF o = g.center();
    p.save();
    p.translate(o);
    p.setPen(Qt::NoPen); p.setBrush(col); p.drawPath(gearPath());
    p.restore();
  }
}

QRectF Header::gearRect() const { return QRectF(width() - 16 - 26, (kRow1 - 26) / 2.0, 26, 26); }

void Header::mouseMoveEvent(QMouseEvent* e) {
  const bool gh = gearRect().contains(e->position());
  if (gh != m_hoverGear) { m_hoverGear = gh; update(gearRect().adjusted(-2, -2, 2, 2).toAlignedRect()); }
  int hm = -1, hs = -1;
  for (const auto& h : hits()) if (h.r.contains(e->position())) { if (h.sub < 0) hm = h.main; else hs = h.sub; }
  if (hm != m_hoverMain || hs != m_hoverSub) { m_hoverMain = hm; m_hoverSub = hs; update(); }
  setCursor(hm >= 0 || hs >= 0 || gh ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void Header::mousePressEvent(QMouseEvent* e) {
  if (gearRect().contains(e->position())) { emit optionsRequested(); return; }
  for (const auto& h : hits()) {
    if (!h.r.contains(e->position())) continue;
    if (h.sub < 0) emit navigate(h.main, m_lastSub.value(h.main));
    else emit navigate(m_main, h.sub);
    return;
  }
}
void Header::leaveEvent(QEvent*) { m_hoverMain = m_hoverSub = -1; m_hoverGear = false; update(); }
