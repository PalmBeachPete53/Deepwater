#include "pages/market/liquidity/LiquidityPage.h"
#include <QResizeEvent>
#include "pages/market/liquidity/CandleChart.h"
#include "pages/market/liquidity/LiquidityPanel.h"
#include "pages/market/liquidity/InstrumentPicker.h"
#include "pages/market/technical/MarketKit.h"
#include "market/Candles.h"
#include "market/Instruments.h"
#include "market/QuoteHub.h"
#include "core/Settings.h"
#include "app/Pages.h"
#include "ui/Format.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include "ui/Touch.h"
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QLabel>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QTimeZone>
#include <QCoreApplication>

DW_PAGE("market/liquidity", LiquidityPage)

using namespace analytics;

namespace {
constexpr int kTitleH = 46, kFooterH = 26;
struct Computed { QVector<Pool> pools; SessionTable sessions; double atr = NAN; };

// Content widget: paints the backdrop and the method footnote under the bottom panel.
class Content : public QWidget {
public:
  using QWidget::QWidget;
  double footY = 0; int noteW = 1572; QString note;
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    mkit::paintBackdrop(p, rect(), 0);
    p.setFont(theme::sans(9)); p.setPen(theme::map("#555349"));
    p.drawText(QRectF(14, footY, noteW, height() - footY), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, note);
  }
};
}

QString LiquidityPage::footnote() {
  return QCoreApplication::translate("technical", 
      "Real 3-bar fractal swings clustered within 0.06% into equal-high/low pools, from real hourly OHLC (2 years). "
      "A pool is \"swept\" the first real candle that pierces it and closes back on the other side (a close beyond is "
      "\"broken\"); untested = neither yet. Dots = candles that traded through the level (+ beyond 5); ×N = swings "
      "in the pool. The list holds untested pools and those taken in the last 30 days. Reversal confirmation = move back within 6 candles, in ATR(14): ≥ 1.5 strong, ≥ 0.75 moderate. "
      "Stop-run likelihood is a disclosed rule (distance to the nearer untested pool in hourly ATR(14): ≤ 2 high, "
      "≤ 6 medium, else low) — never a backtested statistical probability. Sessions in local hours, daylight saving "
      "followed: Sydney 07–16, Tokyo 09–18, London 08–17, New York 08–17.");
}

LiquidityPage::LiquidityPage(QWidget* parent) : QWidget(parent) {
  m_title = new mkit::TitleStrip(QCoreApplication::translate("technical", "LIQUIDITY MAPPING — STOP POOLS & SWEEPS"), mkit::TitleStrip::Menu, kTitleH, this);
  m_button = new InstrumentButton(m_title);
  m_title->afterTitleLayout()->addWidget(m_button);
  m_price = ui::label(fmt::dash(), theme::mono(18, QFont::Bold), theme::map("#fffffc"), m_title);
  m_title->rightLayout()->addWidget(m_price);

  m_scroll = new QScrollArea(this);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setWidgetResizable(false);
  auto* content = new Content;
  content->note = footnote();
  m_content = content;
  m_scroll->setWidget(m_content);
  // The content is laid out to the viewport's width; when it grows taller than the page a vertical bar appears and
  // the viewport narrows: lay out again then, or the content would stick out a few px on the right (cut).
  m_scroll->viewport()->installEventFilter(this);
  m_scroll->viewport()->setAutoFillBackground(false);

  const char* labels[] = {QT_TRANSLATE_NOOP("technical", "STOP-RUN LIKELIHOOD"), QT_TRANSLATE_NOOP("technical", "UNTESTED POOLS"), QT_TRANSLATE_NOOP("technical", "SWEEPS (VISIBLE HISTORY)")};
  for (int i = 0; i < 3; ++i) {
    auto* k = new mkit::KpiCard(QCoreApplication::translate("technical", labels[i]), m_content);
    k->setLabelPx(10, 2.5); k->setValuePx(22, QFont::Bold);
    k->setValue(fmt::dash(), theme::map("#a3a09a")); k->setSub(" ");
    m_kpi << k;
  }
  m_kpi[1]->setAccent(theme::map("#f6e092")); m_kpi[2]->setAccent(theme::map("#f7e396"));
  m_kpi[1]->setToolTip(QCoreApplication::translate("technical", "Pools whose level no candle has traded through since their last swing"));
  m_kpi[2]->setToolTip(QCoreApplication::translate("technical", "Pools pierced by a candle that closed back on the other side, over the loaded history"));

  m_chart = new CandleChart(m_content);
  m_panel = new LiquidityPanel(m_content);
  connect(m_panel, &LiquidityPanel::heightChanged, this, &LiquidityPage::relayout);
  connect(m_chart, &CandleChart::heightRequested, this, &LiquidityPage::relayout);

  m_drop = new InstrumentDropdown(this);
  m_drop->button = m_button;
  connect(m_button, &InstrumentButton::clicked, this, [this] {
    if (m_drop->isVisible()) { m_drop->dismiss(); return; }
    const QPoint at = m_button->mapTo(this, QPoint(0, m_button->height() + 9));
    m_drop->popup(QPoint(at.x(), kTitleH - 1), m_symbol);
    m_button->setOpen(true);
  });
  connect(m_drop, &InstrumentDropdown::closed, this, [this] { m_button->setOpen(false); });
  connect(m_drop, &InstrumentDropdown::picked, this, [this](const QString& s) {
    settings::setValue("market.liquidity.symbol", s);
    load(s, true);
  });

  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, [this](const QStringList& s) {
    if (s.contains(m_symbol)) updateLast();
  });
  m_refresh.setInterval(5 * 60 * 1000);   // hourly candles' cache TTL
  connect(&m_refresh, &QTimer::timeout, this, [this] { load(m_symbol, false); });

  QString sym = qEnvironmentVariable("DW_LIQ_SYMBOL");
  if (sym.isEmpty()) sym = settings::value("market.liquidity.symbol", "EURUSD").toString();
  if (!market::find(sym)) sym = "EURUSD";
  m_symbol = market::find(sym)->symbol;
  m_button->setText(instrumentLabel(m_symbol));
  if (qEnvironmentVariableIsSet("DW_LIQ_TAB")) m_panel->setTab(qEnvironmentVariableIntValue("DW_LIQ_TAB"));
}

void LiquidityPage::load(const QString& symbol, bool resetView) {
  const auto* ins = market::find(symbol);
  if (!ins) return;
  if (symbol != m_symbol) {
    market::QuoteHub::instance().unsubscribe(this);
    m_bars.clear(); m_pools.clear(); m_last = NAN;
    m_chart->setData({}, {}, NAN, NAN, ins->decimals, true);
    m_panel->setData({}, {}, {}, ins->decimals);
    // updateKpis() / updateLast() return early or keep the old values without bars: blank the previous instrument's
    // price and cards so they never sit beside the new name while it loads
    m_price->setText(fmt::dash());
    for (auto* k : m_kpi) { k->setValue(fmt::dash(), theme::map("#a3a09a")); k->setSub(" "); }
    m_kpi[0]->setToolTip(QString());
    m_dataTime = {};
  }
  m_symbol = ins->symbol; m_dec = ins->decimals;
  m_button->setText(instrumentLabel(m_symbol));
  market::QuoteHub::instance().subscribe(this, {m_symbol}, isVisible());
  const int id = ++m_loadId;
  market::candles(m_symbol, "1h", "2y", this, [this, id, resetView](const market::Series& s) {
    if (id != m_loadId) return;
    if (!s.ok()) {
      m_status = s.error.isEmpty() ? QCoreApplication::translate("technical", "no data") : s.error;
      m_kpi[0]->setValue(fmt::dash(), theme::map("#a3a09a")); m_kpi[0]->setSub(QCoreApplication::translate("technical", "Hourly candles unavailable: %1").arg(m_status));
      m_chart->setStatus(QCoreApplication::translate("technical", "Hourly candles unavailable: %1").arg(m_status));   // else "LOADING…" forever
      return;
    }
    const auto bars = s.bars;
    const QDate today = QDateTime::currentDateTimeUtc().date();
    auto* w = new QFutureWatcher<Computed>(this);
    connect(w, &QFutureWatcher<Computed>::finished, this, [this, w, id, bars, resetView] {
      w->deleteLater();
      if (id != m_loadId) return;
      const Computed c = w->result();
      m_bars = bars; m_pools = c.pools; m_atr = c.atr;
      m_dataTime = QDateTime::currentDateTime();
      m_chart->setData(bars, c.pools, c.sessions.pdh, c.sessions.pdl, m_dec, resetView);
      m_panel->setData(c.pools, bars, c.sessions, m_dec);
      updateLast();
      update(QRect(0, height() - kFooterH, width(), kFooterH));
      applyHooks();
    });
    w->setFuture(QtConcurrent::run([bars, today] {
      Computed c;
      c.pools = findPools(bars);
      c.sessions = sessionExtremes(bars, today);
      c.atr = last(atr(bars, 14));
      return c;
    }));
  });
}

void LiquidityPage::updateLast() {
  const auto q = market::QuoteHub::instance().quote(m_symbol);
  m_last = q.valid() ? q.price : (m_bars.isEmpty() ? NAN : m_bars.last().c);
  m_price->setText(std::isfinite(m_last) ? fmt::num(m_last, m_dec) : fmt::dash());
  m_chart->setLastPrice(m_last);
  m_panel->setLastPrice(m_last);
  updateKpis();
}

void LiquidityPage::updateKpis() {
  if (m_bars.isEmpty()) return;
  const StopRun sr = stopRun(m_pools, m_last, m_atr);
  const QColor red = theme::map("#f29896"), gold = theme::map("#f5d565"), grey = theme::map("#a3a09a");
  const QColor c = sr.level == StopRunLevel::High ? red : sr.level == StopRunLevel::Medium ? gold : grey;
  m_kpi[0]->setValue(stopRunName(sr.level), c);
  m_kpi[0]->setAccent(sr.level == StopRunLevel::High ? theme::map("#e39e98") : sr.level == StopRunLevel::Medium ? theme::map("#f6e092") : theme::map("#6b6a5e"));
  if (sr.poolIndex >= 0) {
    const auto& p = m_pools[sr.poolIndex];
    m_kpi[0]->setSub(sr.closer == PoolSide::SellSide ? QCoreApplication::translate("technical", "Sell-side closer") : QCoreApplication::translate("technical", "Buy-side closer"));
    m_kpi[0]->setToolTip(QCoreApplication::translate("technical", "Nearest untested pool: %1 %2 (×%3), %4 from the last price = %5 × hourly ATR(14)\n"
                                 "≤ 2 ATR high, ≤ 6 medium, otherwise low — a disclosed rule, not a probability")
                             .arg(p.side == PoolSide::BuySide ? "BSL" : "SSL", fmt::num(p.level, m_dec)).arg(p.members)
                             .arg(fmt::pct(sr.distPct, 2)).arg(sr.distAtr, 0, 'f', 1));
  } else {
    m_kpi[0]->setSub(QCoreApplication::translate("technical", "No untested pool"));
    m_kpi[0]->setToolTip(QCoreApplication::translate("technical", "Every detected pool has already been traded through"));
  }
  int untested = 0, swept = 0, lastSweep = -1;
  for (const auto& p : m_pools) {
    untested += p.state == PoolState::Untested;
    if (p.state == PoolState::Swept) { ++swept; lastSweep = std::max(lastSweep, p.eventIndex); }
  }
  m_kpi[1]->setValue(QString::number(untested), theme::map("#fffffa"));
  m_kpi[1]->setSub(QCoreApplication::translate("technical", "of %1 detected").arg(m_pools.size()));
  m_kpi[2]->setValue(QString::number(swept), theme::map("#fffffa"));
  m_kpi[2]->setSub(lastSweep >= 0 ? mkit::ago(QDateTime::fromSecsSinceEpoch(m_bars[lastSweep].t, QTimeZone::UTC), QDateTime::currentDateTimeUtc())
                                  : QCoreApplication::translate("technical", "none in the loaded history"));
}

void LiquidityPage::relayout() {
  const int W = m_scroll->viewport()->width();
  const int inner = W - 28;
  // phones: the stop-run card on its own row (its sub-line needs the width), the two counts side by side under it
  const bool phone = ui::compact(width());
  int top = 109;
  if (phone) {
    m_kpi[0]->setGeometry(14, 14, inner, 85);
    const int half = (inner - 7) / 2;
    m_kpi[1]->setGeometry(14, 106, half, 85);
    m_kpi[2]->setGeometry(14 + half + 7, 106, inner - half - 7, 85);
    top = 201;
  } else
  for (int i = 0; i < 3; ++i) {
    const int x0 = 14 + i * (inner + 7) / 3, x1 = i == 2 ? 14 + inner : 14 + (i + 1) * (inner + 7) / 3 - 7;
    m_kpi[i]->setGeometry(x0, 14, x1 - x0, 85);
  }
  // chart height: 521 at the reference size (viewport 893); shorter windows give the chart what is left above a
  // 250 px peek of the bottom panel (never under 280), taller ones up to 60 % of the viewport; a grip drag wins.
  const int vh = m_scroll->viewport()->height();
  const int ch = m_chart->userSized() ? m_chart->preferredHeight()
                                      : std::clamp(vh - 372, 280, std::max(521, int(vh * 0.6)));
  m_chart->setGeometry(14, top, inner, ch);
  const int py = top + ch + 11;
  m_panel->setGeometry(14, py, inner, std::max(1, m_panel->height()));   // width first: the column count depends on it
  m_panel->resize(inner, m_panel->contentHeight());
  auto* c = static_cast<Content*>(m_content);
  c->footY = py + m_panel->height() + 8;
  const QFontMetricsF fm(theme::sans(9));
  c->noteW = std::min(inner, 1572);     // prose line length capped at the reference width
  const QRectF nb = fm.boundingRect(QRectF(0, 0, c->noteW, 1000), Qt::TextWordWrap, c->note);
  m_content->resize(W, int(c->footY + nb.height() + 16));
  m_content->update();
}

void LiquidityPage::applyHooks() {
  if (m_hooksDone) return;
  m_hooksDone = true;
  if (qEnvironmentVariableIsSet("DW_LIQ_ZOOM")) {
    const int steps = qEnvironmentVariableIntValue("DW_LIQ_ZOOM");
    m_chart->zoomAt(m_chart->width() / 2.0, steps);
  }
  if (qEnvironmentVariableIsSet("DW_LIQ_SCROLL")) m_scroll->verticalScrollBar()->setValue(qEnvironmentVariableIntValue("DW_LIQ_SCROLL"));
  if (qEnvironmentVariable("DW_LIQ_DROPDOWN") == "1") emit m_button->clicked();
  if (qEnvironmentVariableIsSet("DW_LIQ_HOVER")) {
    const auto xy = qEnvironmentVariable("DW_LIQ_HOVER").split(',');
    if (xy.size() == 2) {
      const QPoint pagePt(xy[0].toInt(), xy[1].toInt());
      const QPoint c = m_chart->mapFrom(this, pagePt);
      if (m_chart->rect().contains(c)) m_chart->setHover(QPointF(c));
      else mkit::simulateHover(this, pagePt);   // tabs, button, panel
    }
  }
}

bool LiquidityPage::eventFilter(QObject* o, QEvent* e) {
  if (o == m_scroll->viewport() && e->type() == QEvent::Resize &&
      static_cast<QResizeEvent*>(e)->size().width() != static_cast<QResizeEvent*>(e)->oldSize().width())
    QMetaObject::invokeMethod(this, [this] { relayout(); }, Qt::QueuedConnection);
  return QWidget::eventFilter(o, e);
}

void LiquidityPage::resizeEvent(QResizeEvent*) {
  // phones: no room for the title beside the instrument button and the price (the app header names the page)
  m_title->setTitleRoom(ui::compact(width()) ? 0 : -1);
  m_title->setGeometry(0, 0, width(), kTitleH);
  m_scroll->setGeometry(0, kTitleH + 1, width(), height() - kTitleH - 1 - kFooterH);
  relayout();
}

void LiquidityPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  mkit::paintBackdrop(p, rect(), kTitleH);
  // fixed footer
  const QRect f(0, height() - kFooterH, width(), kFooterH);
  p.fillRect(f, theme::map("#060402"));
  p.fillRect(QRect(0, f.top(), width(), 1), theme::map("#15120a"));
  p.setFont(theme::sans(9)); p.setPen(theme::map("#6d6d65"));
  p.drawText(QPointF(7, f.top() + 17), QCoreApplication::translate("technical", "Terminal Data · ") + (m_dataTime.isValid() ? fmt::hms(m_dataTime) : fmt::dash()));
  const QFont bf = theme::mono(9, QFont::Bold, 1.0);
  const QString t = QCoreApplication::translate("technical", "DELAYED");
  const double bw = mkit::textWidth(bf, t) + 20;
  const QRectF b(width() - 8 - bw, f.top() + 2, bw, 20);
  p.setPen(QPen(theme::map("#5e5222"), 1)); p.setBrush(theme::map("#0e0b03")); p.drawRoundedRect(b.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#ffd23a")); p.drawEllipse(QPointF(b.left() + 7, b.center().y()), 2.3, 2.3);
  p.setFont(bf); p.setPen(theme::map("#ffe278"));
  p.drawText(QPointF(b.left() + 12, b.center().y() + 3.5), t);
}

void LiquidityPage::showEvent(QShowEvent*) {
  market::QuoteHub::instance().setActive(this, true);
  if (!m_started) { m_started = true; load(m_symbol, true); }
  else load(m_symbol, false);
  m_refresh.start();
}
void LiquidityPage::hideEvent(QHideEvent*) {
  market::QuoteHub::instance().setActive(this, false);
  m_refresh.stop();
  m_drop->dismiss();
}
