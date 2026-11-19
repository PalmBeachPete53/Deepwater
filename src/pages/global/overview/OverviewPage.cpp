#include "pages/global/overview/OverviewPage.h"
#include "app/Pages.h"
#include "global/Look.h"
#include "global/PageScroll.h"
#include "pages/global/overview/FeedPanels.h"
#include "pages/global/overview/GlobePanel.h"
#include "pages/global/overview/MarketTape.h"
#include "pages/global/overview/OverviewData.h"
#include "pages/global/overview/StrengthPanel.h"
#include "positioning/RiskIndex.h"
#include "macro/CentralBanks.h"
#include "market/QuoteHub.h"
#include <QDateTime>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QApplication>

namespace overview {

// Scrolling content of the page: paints the page background (grid) behind the panels.
class OverviewCanvas : public QWidget {
public:
  using QWidget::QWidget;
protected:
  void paintEvent(QPaintEvent* e) override {
    QPainter p(this);
    p.setClipRegion(e->region());
    global::look::pageBackground(p, QRectF(rect()));
  }
};

OverviewPage::OverviewPage(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_tape = new MarketTape(this);
  auto* canvas = new OverviewCanvas;
  m_scroll = new global::PageScroll(canvas, this);
  m_scroll->layout = [this](int w, int vh) { return layoutPanels(w, vh); };
  m_squawk = new SquawkPanel(canvas);
  m_risk = new RiskPanel(canvas);
  m_strength = new StrengthPanel(canvas);
  m_banks = new CentralBanksPanel(canvas);
  m_globe = new GlobePanel(canvas);
  m_registry = new RegistryPanel(canvas);
  m_data = new OverviewData(m_squawk, m_risk, m_banks, m_registry, this);
  // block cursor of ASSET REGISTRY: flips on the shared half-second phase
  m_blink.setTimerType(Qt::PreciseTimer);
  m_blink.setSingleShot(true);
  connect(&m_blink, &QTimer::timeout, this, [this] {
    m_registry->update(m_registry->cursorRect());
    m_blink.start(int(500 - QDateTime::currentMSecsSinceEpoch() % 500) + 2);
  });
}

// Test hooks (page coordinates, 1600x966 page = reference frames):
//   DW_OVERVIEW_CLICK=x,y[;x,y…]  clicks once, 2.5 s after the page shows (e.g. the EN/FR button, a region tab)
//   DW_OVERVIEW_HOVER=x,y         keeps the mouse there (row hover, tab hover, crosshair), re-sent every second
static void sendMouse(QWidget* page, const QPoint& at, QEvent::Type type) {
  QWidget* w = page->childAt(at);
  if (!w) return;
  const QPointF local = w->mapFrom(page, QPointF(at));
  const QPointF global = page->mapToGlobal(QPointF(at));
  QMouseEvent e(type, local, global, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                type == QEvent::MouseButtonRelease || type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(w, &e);
}
static QList<QPoint> points(const char* env) {
  QList<QPoint> out;
  for (const QString& xy : QString::fromLatin1(qgetenv(env)).split(';', Qt::SkipEmptyParts)) {
    const auto v = xy.split(',');
    if (v.size() == 2) out << QPoint(v[0].toInt(), v[1].toInt());
  }
  return out;
}

void OverviewPage::showEvent(QShowEvent*) {
  m_blink.start(10);
  m_data->setVisible(true);
  static bool clicked = false;
  if (!clicked && qEnvironmentVariableIsSet("DW_OVERVIEW_CLICK")) {
    clicked = true;
    QTimer::singleShot(2500, this, [this] {
      for (const QPoint& p : points("DW_OVERVIEW_CLICK")) { sendMouse(this, p, QEvent::MouseButtonPress); sendMouse(this, p, QEvent::MouseButtonRelease); }
    });
  }
  if (qEnvironmentVariableIsSet("DW_OVERVIEW_HOVER")) {
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this] { for (const QPoint& p : points("DW_OVERVIEW_HOVER")) sendMouse(this, p, QEvent::MouseMove); });
    t->start(1000);
  }
}
void OverviewPage::hideEvent(QHideEvent*) { m_blink.stop(); m_data->setVisible(false); }
void OverviewPage::preload() {
  positioning::RiskIndex::instance().load();
  macro::CentralBanks::instance().ensureLoaded();
  m_strength->preload();
  auto& hub = market::QuoteHub::instance();   // the tape's first quotes, without its polling
  hub.setActive(m_tape, true);
  hub.setActive(m_tape, false);
}

void OverviewPage::resizeEvent(QResizeEvent*) {
  m_tape->setGeometry(7, 7, width() - 14, 35);
  m_scroll->setGeometry(0, 42, width(), height() - 42);   // relayout through PageScroll::resizeEvent
}

// Layout of the panels for a content width W and visible height VH (content coordinates start 42 px under the
// page top, so y = reference y - 42). Returns the content height; the page scrolls when it exceeds VH.
//   ≥ 1340 wide: the reference's three columns (Squawk | Risk, Strength, Central Banks | Globe, Registry);
//     side columns 408 px at 1600, wider on big screens (≤ 760), narrower down to 340 on laptops.
//   < 1340 wide: two columns (Squawk | Risk, Strength, Central Banks) and, below them, a full-width row with
//     the globe and the Asset Registry side by side (reached by scrolling).
// Heights: at the reference height (966) exactly the reference; taller pages give the extra height mostly to
// the Currency Strength chart and the globe; shorter ones shrink down to usable minimums, then scroll.
int OverviewPage::layoutPanels(int W, int VH) {
  const int m = 8, g = 7, top = 7;               // 49 - 42
  const int H = VH + 42;                         // page height the reference proportions refer to
  const int extra = qMax(0, H - 966);
  const int short_ = qMax(0, 966 - H);
  // middle column heights
  int riskH = 245 + int(extra * 0.15) - qMin(45, int(short_ * 0.25));
  int cbH = 262 + int(extra * 0.10) - qMin(40, int(short_ * 0.20));
  const int minStrength = 270;
  const int colH = qMax(H - 42 - top - 7, riskH + 6 + minStrength + g + cbH);   // content height of the columns
  const int bottom = top + colH;
  const bool three = W >= 1340;
  int side, midX, midW;
  if (three) {
    side = W <= 1600 ? qBound(340, 408 - (1600 - W) * 3 / 10, 408) : qMin(760, 408 + (W - 1600) * 3 / 10);
    midX = m + side + g;
    midW = W - 2 * m - 2 * side - 2 * g;
  } else {
    side = qBound(330, int(W * 0.35), 420);
    midX = m + side + g;
    midW = W - m - midX;
  }
  m_squawk->setGeometry(m, top, side, bottom - top);
  if (midW >= 1700) {
    // ultrawide: Risk Sentiment and Central Banks side by side on top, Currency Strength the full width below
    // (a 1900 px wide table would spread its eight rows across the screen)
    const int rowH = qMax(riskH, cbH), riskW = int(midW * 0.55);
    m_risk->setGeometry(midX, top, riskW, rowH);
    m_banks->setGeometry(midX + riskW + g, top, midW - riskW - g, rowH);
    m_strength->setGeometry(midX, top + rowH + 6, midW, bottom - (top + rowH + 6));
  } else {
    m_risk->setGeometry(midX, top, midW, riskH);
    m_strength->setGeometry(midX, top + riskH + 6, midW, bottom - cbH - g - (top + riskH + 6) + 1);
    m_banks->setGeometry(midX, bottom - cbH, midW, cbH);
  }
  if (three) {
    const int mapH = qMax(250, 365 + int(extra * 0.35) - qMin(115, int(short_ * 0.5)));
    m_globe->setGeometry(W - m - side, top, side, mapH);
    m_registry->setGeometry(W - m - side, top + mapH + 6, side, bottom - (top + mapH + 6));
    return bottom + 7;
  }
  // two columns: globe + registry on a row below, full width
  const int rowTop = bottom + g, rowH = 400;
  const int globeW = qBound(320, int((W - 2 * m) * 0.42), 520);
  m_globe->setGeometry(m, rowTop, globeW, rowH);
  m_registry->setGeometry(m + globeW + g, rowTop, W - 2 * m - globeW - g, rowH);
  return rowTop + rowH + 7;
}

void OverviewPage::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  p.setClipRegion(e->region());
  global::look::pageBackground(p, QRectF(rect()));
}
}  // namespace overview

DW_PAGE("global/overview", overview::OverviewPage)
