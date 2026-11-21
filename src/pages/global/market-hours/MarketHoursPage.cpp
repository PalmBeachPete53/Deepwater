#include "pages/global/market-hours/MarketHoursPage.h"
#include <QCoreApplication>
#include "pages/global/market-hours/MarketHoursWidgets.h"
#include "app/Pages.h"
#include "global/Look.h"
#include "global/PageScroll.h"
#include "global/Sessions.h"
#include "ui/Theme.h"
#include "ui/Touch.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <cmath>


namespace look = global::look;

namespace hours {

static const char* kDisclaimer = QT_TRANSLATE_NOOP("global",
    "FX SESSIONS ARE THE CONVENTIONAL BUSINESS HOURS OF EACH CENTRE, MONDAY TO FRIDAY, DAYLIGHT-SAVING AWARE · "
    "SHOWN ON YOUR OWN CLOCK");

// Scrolling content under the page strip: page background, the panels and the disclaimer under the cards.
class HoursCanvas : public QWidget {
public:
  int disclaimerY = 0;
protected:
  void paintEvent(QPaintEvent* e) override {
    QPainter p(this);
    p.setClipRegion(e->region());
    p.setRenderHint(QPainter::Antialiasing);
    look::pageBackground(p, QRectF(rect()));
    const QFont f = theme::mono(8, QFont::Normal, 1.2);
    if (ui::compact(width())) {   // phone: the one-line disclaimer ran off the screen, it wraps
      p.setFont(f); p.setPen(theme::map("#65655c"));
      p.drawText(QRectF(14, disclaimerY - QFontMetricsF(f).ascent(), width() - 28, 60), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                 QCoreApplication::translate("global", kDisclaimer));
      return;
    }
    look::text(p, 14, disclaimerY, QCoreApplication::translate("global", kDisclaimer), f, theme::map("#65655c"));
  }
};

MarketHoursPage::MarketHoursPage(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_canvas = new HoursCanvas;
  m_scroll = new global::PageScroll(m_canvas, this);
  m_scroll->layout = [this](int w, int vh) { return layoutChildren(w, vh); };
  m_clock = new ClockPanel(m_canvas);
  m_timeline = new TimelinePanel(m_canvas);
  for (int i = 0; i < global::sessions().size(); ++i) m_cards << new SessionCard(i, m_canvas);
  // half-second ticks aligned on the wall clock: cursors blink, clocks advance on each new second
  m_timer.setTimerType(Qt::PreciseTimer);
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
}

void MarketHoursPage::showEvent(QShowEvent*) { m_lastSec = -1; tick(); }
void MarketHoursPage::hideEvent(QHideEvent*) { m_timer.stop(); }

QRect MarketHoursPage::badgeRect() const {
  const QFont f = theme::mono(9, QFont::Bold, 1);
  const QString t = "• " + QCoreApplication::translate("global", "%1/4 SESSIONS LIVE · YOUR CLOCK %2").arg(0).arg(global::utcOffsetLabel(global::now()));
  const int w = int(look::width(t, f)) + 12;
  return QRect(width() - 14 - w, 9, w, 18);
}

void MarketHoursPage::tick() {
  const QDateTime utc = global::nowUtc();
  const qint64 sec = utc.toSecsSinceEpoch();
  // strip and clock-title cursors
  update(QRect(0, 0, width(), 36).intersected(QRect(40, 0, 420, 36)));
  m_clock->blinkCursor();
  if (sec != m_lastSec) {
    const bool minute = m_lastSec < 0 || sec / 60 != m_lastSec / 60;
    m_lastSec = sec;
    m_clock->setNow(utc);
    for (auto* c : m_cards) c->setNow(utc);
    const int live = global::liveCount(utc);
    if (minute || live != m_lastLive) m_timeline->setNow(utc);
    if (live != m_lastLive || minute) { m_lastLive = live; update(badgeRect().adjusted(-300, 0, 0, 0)); }
  }
  const qint64 ms = QDateTime::currentMSecsSinceEpoch();
  m_timer.start(int(500 - ms % 500) + 2);
}

void MarketHoursPage::resizeEvent(QResizeEvent*) { m_scroll->setGeometry(0, 37, width(), height() - 37); }

// Content layout (content y = page y - 37). Size classes:
//   width ≥ 1300: clock | timeline on top, the four session cards in one row (reference);
//   width < 1300: the cards go 2 x 2 so names, pills and countdowns keep their size.
//   Clock panel 453 px at 1600, 340 px minimum, up to 640 px on big screens (the dial grows with it).
//   Height: 966 = reference (566 px panels, empty band under the disclaimer). Shorter pages give the band up
//   first, then the panels down to 400 px, then the page scrolls. Taller pages grow the panels and the cards
//   (cards scale uniformly up to 1.25x).
//   phone upright (ui::compact): one column, the clock (dial as wide as the screen), the timeline, the four cards
//   (each the full width, at their reference size), then the disclaimer, wrapped; the page scrolls.
int MarketHoursPage::layoutChildren(int W, int VH) {
  const int H = VH + 37;
  const int m = 14, top = 51 - 37, gap = 11, discH = 30;
  if (ui::compact(W)) {
    const int w = W - 2 * m;
    int y = top;
    const int clockH = qBound(420, w - 20 + 136, 640);   // dial scale (w - 20) / 430 fills the width (ClockPanel::geometryFor)
    m_clock->setGeometry(m, y, w, clockH);
    y += clockH + 10;
    m_timeline->setGeometry(m, y, w, 370);   // rows + overlaps with two label lines (TimelinePanel's top clamp)
    y += 370 + 10;
    m_cardsTop = y + 37;
    for (auto* c : m_cards) { c->setGeometry(m, y, w, 176); y += 176 + gap; }
    m_canvas->disclaimerY = y + 6;
    m_canvas->update();
    return y + 6 + 50;
  }
  const bool grid = W < 1300;
  const double cw = grid ? (W - 2 * m - gap + 1) / 2.0 : (W - 2 * m - 3 * gap + 1) / 4.0;
  const double cardScale = qBound(1.0, qMin(cw / 385.0, H / 966.0), 1.25);
  const int cardH = int(std::round(176 * cardScale));
  const int cardsH = grid ? 2 * cardH + gap : cardH;
  int panelH;
  if (H <= 966) panelH = qMax(400, qMin(566, H - 51 - 10 - cardsH - discH + 1));
  else panelH = qMax(566, 566 + int((H - 966) * 0.8) - (cardH - 176));
  const int clockW = W < 1400 ? qMax(340, int(W * 0.30)) : W <= 1600 ? 453 : qMin(640, 453 + (W - 1600) / 5);
  m_clock->setGeometry(m, top, clockW, panelH);
  m_timeline->setGeometry(m + clockW + 10, top, W - 2 * m - clockW - 10 + 1, panelH);
  const int cardsTop = top + panelH + 10;
  m_cardsTop = cardsTop + 37;
  for (int i = 0; i < m_cards.size(); ++i) {
    const int col = grid ? i % 2 : i, row = grid ? i / 2 : 0;
    const int x0 = m + int(std::round(col * (cw + gap)));
    const int x1 = m + int(std::round(col * (cw + gap) + cw));
    m_cards[i]->setGeometry(x0, cardsTop + row * (cardH + gap), x1 - x0, cardH);
  }
  m_canvas->disclaimerY = cardsTop + cardsH + 17;
  m_canvas->update();
  return cardsTop + cardsH + discH;
}

void MarketHoursPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  // page strip (fixed; the content under it scrolls on short windows)
  QLinearGradient g(0, 0, width(), 0);
  g.setColorAt(0, theme::map("#1a1003")); g.setColorAt(0.5, theme::map("#120b04")); g.setColorAt(1, theme::map("#161004"));
  p.fillRect(QRectF(0, 0, width(), 36), g);
  p.fillRect(QRectF(0, 36, width(), 1), theme::map("#4b3e21"));
  // clock icon
  {
    const QPointF c(18.5, 18);
    p.setPen(QPen(theme::map("#8a7a3a"), 1.1)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, 4.8, 4.8);
    p.drawLine(c, c + QPointF(0, -3)); p.drawLine(c, c + QPointF(2.2, 0.8));
  }
  p.setPen(Qt::NoPen);
  p.setBrush(theme::map(QColor(252, 215, 0, 60))); p.drawEllipse(QPointF(40.5, 18), 4, 4);
  p.setBrush(theme::map("#c9a90c")); p.drawEllipse(QPointF(40.5, 18), 2.5, 2.5);
  const QFont tf = theme::mono(10, QFont::Normal, 2.4);
  const QRect br = badgeRect();
  // phone: the title overlapped the badge; with no room for it beside the badge it is left out (the header names
  // the page)
  if (!ui::compact(width()) || br.left() - 12 - 50 >= 120) {
    const QString title = QCoreApplication::translate("global", "MARKET HOURS — THE FOUR FX SESSIONS");
    const qreal tw = look::text(p, 50, 21.5, ui::compact(width()) ? QFontMetricsF(tf).elidedText(title, Qt::ElideRight, br.left() - 12 - 50) : title,
                                tf, theme::map("#6d6a61"));
    if (look::cursorOn()) look::cursorBlock(p, 50 + tw, 12.5);
  }

  // badge: live sessions and the user's offset
  const QDateTime utc = global::nowUtc();
  p.setPen(QPen(theme::map("#8a7e45"), 1)); p.setBrush(theme::map("#1e1803"));
  p.drawRoundedRect(QRectF(br).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  const QFont bf = theme::mono(9, QFont::Bold, 1);
  const int live = global::liveCount(utc);
  const QString bt = QCoreApplication::translate("global", "%1/4 SESSIONS LIVE · YOUR CLOCK %2").arg(live).arg(global::utcOffsetLabel(global::now()));
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#fcd700"));
  p.drawEllipse(QPointF(br.left() + 8, br.center().y() + 0.5), 1.6, 1.6);
  look::text(p, br.left() + 14, br.center().y() + 4, bt, bf, theme::map("#fff2b0"));
}
}  // namespace hours

DW_PAGE("global/market-hours", hours::MarketHoursPage)
