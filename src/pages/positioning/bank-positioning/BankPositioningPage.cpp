#include "pages/news/bank-research/Look.h"
#include "pages/positioning/bank-positioning/BankPositioningPage.h"
#include "pages/positioning/bank-positioning/Widgets.h"
#include "pages/positioning/bank-positioning/ForecastOverlay.h"
#include "banks/Forecasts.h"
#include "banks/Research.h"
#include "banks/Registry.h"
#include "market/QuoteHub.h"
#include "ui/Theme.h"
#include "app/Pages.h"
#include <QPainter>
#include <QTimer>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QLocale>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QVariantAnimation>
#include <QHelpEvent>
#include <QToolTip>
#include <QWheelEvent>
#include <cmath>

DW_PAGE("positioning/bank-positioning", banks::pos::BankPositioningPage)

namespace banks::pos {

namespace {
const banks::DwColor kBg{"#050402"}, kHeadBorder{"#4b482c"};
constexpr int kHeaderH = 44;

double spotOf(const QString& sym) {
  const auto q = ::market::QuoteHub::instance().quote(sym);
  return q.valid() ? q.price : NAN;
}
}

BankPositioningPage::BankPositioningPage(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_deskScroll = new QScrollArea(this);
  m_deskScroll->setFrameShape(QFrame::NoFrame);
  m_deskScroll->setWidgetResizable(true);
  m_deskScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_deskScroll->viewport()->setAutoFillBackground(false);
  m_desk = new DeskBody(m_deskScroll);
  m_desk->painter = [this](QPainter& p) { paintNotes(p); };
  m_deskScroll->setWidget(m_desk);
  m_deskScroll->viewport()->installEventFilter(this);
  m_wheel = new QVariantAnimation(this);
  m_wheel->setDuration(150); m_wheel->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_wheel, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_deskScroll->verticalScrollBar()->setValue(v.toInt()); });
  m_deskCalls = new CallsTable(m_desk);
  m_deskCalls->hide();
  for (auto c : banks::assetClasses()) {
    auto* card = new MarketCard(c, m_desk);
    card->onClick = [this](banks::AssetClass cls) {
      setView(1);
      m_callsScroll->verticalScrollBar()->setValue(m_calls->groupTop(cls));
    };
    m_cards << card;
  }
  m_callsScroll = new QScrollArea(this);
  m_callsScroll->setFrameShape(QFrame::NoFrame);
  m_callsScroll->setWidgetResizable(true);
  m_callsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_calls = new CallsTable(m_callsScroll);
  m_callsScroll->setWidget(m_calls);
  m_callsScroll->hide();
  m_overlay = new ForecastOverlay(this);
  m_overlay->onClose = [this] { update(); };

  auto& f = banks::Forecasts::instance();
  connect(&f, &banks::Forecasts::updated, this, [this] { recompute(); m_overlay->refresh(); });
  connect(&banks::Research::instance(), &banks::Research::updated, this, [this] { update(QRect(width() - 420, 0, 420, kHeaderH)); });
  // quotes arrive in bursts (one reply per 20 symbols): recompute at most every 300 ms
  m_throttle.setSingleShot(true); m_throttle.setInterval(300);
  connect(&m_throttle, &QTimer::timeout, this, [this] { recompute(); m_overlay->refresh(); });
  // hidden: nothing (showEvent recomputes); other pages' quotes would otherwise recompute this one all day
  connect(&::market::QuoteHub::instance(), &::market::QuoteHub::updated, this, [this] { if (isVisible() && !m_throttle.isActive()) m_throttle.start(); });

  m_blink.setInterval(500);
  connect(&m_blink, &QTimer::timeout, this, [this] { m_cursorOn = !m_cursorOn; update(QRect(0, 0, 500, kHeaderH)); });

  bool ok = false;
  const int hov = qEnvironmentVariable("DW_BANKPOS_HOVER").toInt(&ok);
  if (ok && hov >= 0 && hov < m_cards.size()) m_cards[hov]->setForcedHover(true);
  if (qEnvironmentVariable("DW_BANKPOS_BTNHOVER") == "1") m_hoverBtn = 0;
  if (qEnvironmentVariable("DW_BANKPOS_VIEW") == "1") setView(1);
}

// Title, then the buttons right after it (x = 505 / 701 at the reference size); narrow windows use the short title.
QString BankPositioningPage::titleText() const {
  return width() < 1200 ? QCoreApplication::translate("banks", "BANK POSITIONING") : QCoreApplication::translate("banks", "BANK POSITIONING — RESEARCH DESK CALLS & FORECASTS");
}
// Button widths follow their labels (175 / 68 / 110 px in English, as in the reference; wider in French).
QRectF BankPositioningPage::forecastBtn() const {
  const qreal x = 50 + QFontMetricsF(theme::mono(11, QFont::Normal, 1.7)).horizontalAdvance(titleText()) + 38;
  const qreal w = QFontMetricsF(theme::mono(12, QFont::Bold, 1.1)).horizontalAdvance(QCoreApplication::translate("banks", "FX BANK FORECASTS")) + 35;
  return QRectF(qRound(x), 10, qMax<qreal>(175, std::ceil(w)), 24);
}
QRectF BankPositioningPage::deskBtn() const {
  const qreal w = QFontMetricsF(theme::mono(12, QFont::Bold, 1.1)).horizontalAdvance(QCoreApplication::translate("banks", "DESK")) + 32;
  return QRectF(forecastBtn().right() + 21, 10, qMax<qreal>(68, std::ceil(w)), 24);
}
QRectF BankPositioningPage::callsBtn() const {
  const qreal w = QFontMetricsF(theme::mono(12, QFont::Bold, 1.1)).horizontalAdvance(QCoreApplication::translate("banks", "ALL CALLS")) + 34;
  return QRectF(deskBtn().right(), 10, qMax<qreal>(110, std::ceil(w)), 24);
}

bool BankPositioningPage::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip && titleText().size() < 20) {
    auto* he = static_cast<QHelpEvent*>(e);
    if (QRectF(0, 0, forecastBtn().left(), kHeaderH).contains(he->pos())) {
      QToolTip::showText(he->globalPos(), QCoreApplication::translate("banks", "Bank positioning — research desk calls & forecasts"), this);
      return true;
    }
  }
  return QWidget::event(e);
}

bool BankPositioningPage::eventFilter(QObject* o, QEvent* e) {
  if (o == m_deskScroll->viewport() && e->type() == QEvent::Wheel) {   // smooth wheel scrolling (≈150 ms)
    auto* we = static_cast<QWheelEvent*>(e);
    QScrollBar* sb = m_deskScroll->verticalScrollBar();
    if (!we->pixelDelta().isNull()) { sb->setValue(sb->value() - we->pixelDelta().y()); return true; }
    const int target = qBound(0, (m_wheel->state() == QAbstractAnimation::Running ? m_wheel->endValue().toInt() : sb->value())
                                    - we->angleDelta().y() * 80 / 120, sb->maximum());
    m_wheel->stop(); m_wheel->setStartValue(sb->value()); m_wheel->setEndValue(target); m_wheel->start();
    return true;
  }
  return QWidget::eventFilter(o, e);
}

void BankPositioningPage::resizeEvent(QResizeEvent*) {
  m_deskScroll->setGeometry(0, 83, width(), height() - 83);
  m_callsScroll->setGeometry(10, 86, width() - 20, height() - 96);
  m_overlay->setGeometry(rect());
  m_texture = {};
  layoutDesk();
}

// Cards: 3 columns × 2 rows as in the reference; 2 columns below 1280 px; one row of 6 from 2400 px.
// Under them the "how to read / banks" notes (≤ 1000 px wide) and, from 1900 px, the full list of calls.
void BankPositioningPage::layoutDesk() {
  const int W = width();
  const int cols = W >= 2400 ? 6 : W >= 1280 ? 3 : 2;
  const int rows = (int(m_cards.size()) + cols - 1) / cols;
  const qreal cw = (W - 20 - 11.0 * (cols - 1)) / cols;
  for (int i = 0; i < m_cards.size(); ++i) {
    const int r = i / cols, c = i % cols;
    const int x = int(10 + c * (cw + 11)), y = 3 + r * 203, h = r == 0 ? 190 : 192;
    m_cards[i]->setGeometry(x, y - MarketCard::kLift, int(cw), h + MarketCard::kLift);
  }
  m_notesTop = 3 + rows * 203 - 13 + 25;
  m_notesWidth = qMin(W - 24, 1000);
  qreal y = m_notesTop + 160;
  const bool wide = W >= 1900;
  m_deskCalls->setVisible(wide && m_view == 0);
  if (wide) {
    m_deskCalls->setGeometry(10, int(y + 24), W - 20, m_deskCalls->sizeHint().height());
    y += 24 + m_deskCalls->sizeHint().height();
  }
  m_desk->setMinimumHeight(int(y + 16));
  m_desk->update();
}

void BankPositioningPage::setView(int v) {
  m_view = v;
  m_deskScroll->setVisible(v == 0);
  m_callsScroll->setVisible(v == 1);
  recompute();
  update();
}

void BankPositioningPage::recompute() {
  auto& f = banks::Forecasts::instance();
  for (auto* c : m_cards) c->setSummary(f.summary(c->assetClass(), spotOf));
  if (m_view == 1 || width() >= 1900) {
    QList<QPair<banks::AssetClass, QList<banks::Call>>> g;
    for (auto c : banks::assetClasses()) g << qMakePair(c, f.calls(c, spotOf));
    if (m_view == 1) m_calls->setCalls(g);
    if (width() >= 1900) { m_deskCalls->setCalls(g); layoutDesk(); }
  }
  m_desk->update();
}

void BankPositioningPage::showEvent(QShowEvent*) {
  m_blink.start();
  auto& f = banks::Forecasts::instance();
  f.refresh();
  banks::Research::instance().refresh();
  ::market::QuoteHub::instance().subscribe(this, f.spotSymbols());
  ::market::QuoteHub::instance().setActive(this, true);
  recompute();
  const QString ov = qEnvironmentVariable("DW_BANKPOS_OVERLAY");
  if (!ov.isEmpty() && !m_overlay->isVisible()) {
    QTimer::singleShot(1500, this, [this, ov] { openOverlay(ov == "1" ? QString() : ov); });
  }
}
void BankPositioningPage::hideEvent(QHideEvent*) {
  m_blink.stop(); m_throttle.stop();
  ::market::QuoteHub::instance().setActive(this, false);
}

void BankPositioningPage::openOverlay(const QString& pair) {
  m_overlay->setGeometry(rect());
  m_overlay->open(pair);
}

void BankPositioningPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  if (m_texture.isNull() || m_texture.size() != size() * devicePixelRatioF()) {
    // faint hexagon texture of the reference background
    m_texture = QPixmap(size() * devicePixelRatioF()); m_texture.setDevicePixelRatio(devicePixelRatioF());
    m_texture.fill(kBg);
    QPainter t(&m_texture); t.setRenderHint(QPainter::Antialiasing);
    t.setPen(QPen(theme::map(255, 220, 140, 4), 1));
    const qreal r = 34, hw = std::sqrt(3.0) * r;
    for (int row = 0; row * r * 1.5 < height() + r; ++row)
      for (int col = -1; col * hw < width() + hw; ++col) {
        const QPointF c(col * hw + (row % 2 ? hw / 2 : 0), row * r * 1.5);
        QPolygonF hex;
        for (int k = 0; k < 6; ++k) { const double a = M_PI / 180 * (60 * k - 30); hex << c + QPointF(r * std::cos(a), r * std::sin(a)); }
        t.drawPolygon(hex);
      }
    QRadialGradient vg(QPointF(width() / 2.0, height() * 0.2), width() * 0.8);
    vg.setColorAt(0, theme::map(30, 22, 4, 40)); vg.setColorAt(1, theme::map(0, 0, 0, 0));
    t.fillRect(rect(), vg);
  }
  p.drawPixmap(0, 0, m_texture);
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);

  // header bar
  QLinearGradient hg(0, 0, 0, kHeaderH); hg.setColorAt(0, theme::map("#181004")); hg.setColorAt(1, theme::map("#100800"));
  p.fillRect(QRect(0, 0, width(), kHeaderH), hg);
  p.fillRect(QRect(0, kHeaderH - 1, width(), 1), kHeadBorder);
  {
    // bank building icon
    QPen ip(theme::map("#dfd28a"), 1.2); p.setPen(ip); p.setBrush(Qt::NoBrush);
    const QRectF r(13, 15, 12, 12);
    QPolygonF roof; roof << QPointF(r.left(), r.top() + 4) << QPointF(r.center().x(), r.top()) << QPointF(r.right(), r.top() + 4);
    p.drawPolygon(roof);
    for (int i = 0; i < 3; ++i) p.drawLine(QPointF(r.left() + 2 + i * 4, r.top() + 6), QPointF(r.left() + 2 + i * 4, r.bottom() - 1));
    p.drawLine(QPointF(r.left(), r.bottom()), QPointF(r.right(), r.bottom()));
  }
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#827e3b")); p.drawEllipse(QPointF(38, 21), 2.5, 2.5);
  const QFont tf = theme::mono(11, QFont::Normal, 1.7);
  p.setFont(tf); p.setPen(theme::map("#615d54"));
  const QString title = titleText();
  p.drawText(QPointF(50, 25), title);
  if (m_cursorOn) p.fillRect(QRectF(50 + QFontMetricsF(tf).horizontalAdvance(title) + 3, 14, 7, 13), theme::map("#f5e692"));
  // FX BANK FORECASTS button
  {
    const QRectF b = forecastBtn();
    p.setPen(QPen(m_hoverBtn == 0 ? theme::map("#8a7a30") : theme::map("#4a3f0c"), 1)); p.setBrush(theme::map("#383004"));
    p.drawRoundedRect(b.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    QPen lp(theme::map("#f6df97"), 1.2); lp.setJoinStyle(Qt::RoundJoin); p.setPen(lp); p.setBrush(Qt::NoBrush);
    const QPointF o(b.left() + 9, b.center().y() + 5);
    p.drawLine(o, o + QPointF(0, -10)); p.drawLine(o, o + QPointF(10, 0));
    QPolygonF l; l << o + QPointF(2, -3) << o + QPointF(5, -6) << o + QPointF(7, -4) << o + QPointF(10, -8);
    p.drawPolyline(l);
    p.setFont(theme::mono(12, QFont::Bold, 1.1)); p.setPen(theme::map("#f6df97"));
    p.drawText(b.adjusted(25, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("banks", "FX BANK FORECASTS"));
  }
  // DESK | ALL CALLS
  {
    const QRectF all = deskBtn().united(callsBtn());
    p.setPen(QPen(theme::map("#2b2a1d"), 1)); p.setBrush(theme::map("#140f05"));
    p.drawRoundedRect(all.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    const QRectF on = m_view == 0 ? deskBtn() : callsBtn();
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#fcd808")); p.drawRoundedRect(on, 2, 2);
    for (int i = 0; i < 2; ++i) {
      const QRectF r = i == 0 ? deskBtn() : callsBtn();
      const bool active = (i == m_view);
      const QColor c = active ? (theme::isDefault() ? QColor("#0c0601") : theme::onAccent()) : (m_hoverBtn == i + 1 ? theme::map("#d8d2c4") : theme::map("#979289"));
      QPen ip(c, 1.1); p.setPen(ip); p.setBrush(Qt::NoBrush);
      const QPointF ic(r.left() + 13, r.center().y());
      if (i == 0) { for (int k = 0; k < 4; ++k) p.drawRect(QRectF(ic.x() - 5 + (k % 2) * 5.5, ic.y() - 5 + (k / 2) * 5.5, 4, 4)); }
      else { p.drawRect(QRectF(ic.x() - 5, ic.y() - 5, 10, 10)); p.drawLine(ic + QPointF(-5, -1.5), ic + QPointF(5, -1.5)); p.drawLine(ic + QPointF(-1.5, -1.5), ic + QPointF(-1.5, 5)); }
      p.setFont(theme::mono(12, active ? QFont::Bold : QFont::Medium, 1.1)); p.setPen(c);
      p.drawText(r.adjusted(24, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, i == 0 ? QCoreApplication::translate("banks", "DESK") : QCoreApplication::translate("banks", "ALL CALLS"));
    }
  }
  // badge: notes archived · forecast tables revision
  {
    auto& f = banks::Forecasts::instance();
    QDate rev; for (const auto& x : f.forecasts()) if (x.revised > rev) rev = x.revised;
    const QString t = QCoreApplication::translate("banks", "• %1 NOTES ARCHIVED · %2").arg(banks::Research::instance().librarySize())
        .arg(rev.isValid() ? QCoreApplication::translate("banks", "FORECAST TABLES REVISED %1").arg(i18n::locale().toString(rev, "d MMM").toUpper())
                           : QCoreApplication::translate("banks", "UPDATED WITH EVERY NEW BANK TABLE"));
    QFont bf = theme::mono(9, QFont::Bold, 0.8); bf.setPixelSize(10);
    QString t2 = t;
    if (width() - 14 - QFontMetricsF(bf).horizontalAdvance(t) - 16 < callsBtn().right() + 16)   // narrow window
      t2 = QCoreApplication::translate("banks", "• %1 NOTES · %2").arg(banks::Research::instance().librarySize())
               .arg(rev.isValid() ? QCoreApplication::translate("banks", "REVISED %1").arg(i18n::locale().toString(rev, "d MMM").toUpper()) : QString());
    const qreal w = QFontMetricsF(bf).horizontalAdvance(t2) + 16;
    const QRectF b(width() - 14 - w, 13, w, 20);
    p.setPen(QPen(theme::map("#7a6a1a"), 1)); p.setBrush(theme::map("#3f3500"));
    p.drawRoundedRect(b.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p.setFont(bf); p.setPen(theme::map("#fae8a1"));
    p.drawText(b, Qt::AlignCenter, t2);
  }
  // Markets label
  p.setFont(theme::mono(12, QFont::Medium, 2.0)); p.setPen(theme::map("#e2d59c"));
  p.drawText(QPointF(10, 70), m_view == 0 ? QCoreApplication::translate("banks", "Markets") : QCoreApplication::translate("banks", "All calls"));

}

void BankPositioningPage::paintNotes(QPainter& p) {
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
    // under the cards: how calls are computed, which banks are read and which are not
    auto& f = banks::Forecasts::instance();
    const qreal y0 = m_notesTop;
    const qreal tw = m_notesWidth;
    {
      p.setFont(theme::mono(9, QFont::Bold, 2.0)); p.setPen(theme::map("#8a7e4a"));
      p.drawText(QPointF(12, y0), QCoreApplication::translate("banks", "HOW TO READ"));
      p.setFont(theme::mono(10, QFont::Normal, 0.4)); p.setPen(theme::map("#6b675e"));
      QString how = QCoreApplication::translate("banks",
          "A call is computed by Deepwater, not taken from a bank: each bank's published forecast for the next quarter end "
          "is compared with today's price. LONG = forecast above spot by more than the class threshold (G10 FX 1%, Asia & EM FX 1.5%, "
          "commodities and indices 3%, crypto 5%), SHORT = below, RANGE = within. Rates: forecast yield 0.15 pt below today's = LONG bonds.");
      p.drawText(QRectF(12, y0 + 8, tw, 60), Qt::TextWordWrap, how);
      p.setFont(theme::mono(9, QFont::Bold, 2.0)); p.setPen(theme::map("#8a7e4a"));
      p.drawText(QPointF(12, y0 + 76), QCoreApplication::translate("banks", "BANKS"));
      QStringList inc;
      for (const auto& id : f.banksIncluded()) if (const banks::Bank* b = banks::bank(id)) inc << b->researchName;
      p.setFont(theme::mono(10, QFont::Normal, 0.4)); p.setPen(theme::map("#6b675e"));
      const QString banksTxt = QCoreApplication::translate("banks",
          "Read: %1 (forecast tables: FX, commodities, US Treasuries; reuse allowed with attribution, © ING Bank N.V.). "
          "Not read: TD Economics, MUFG Research, KBC Economics (terms forbid automated access, quoting or deep links), "
          "Scotiabank, Commerzbank (forecast tables public but no reproduction allowed). %2 other banks checked: their terms allow "
          "links only — see News › Bank Research › DATA.%3")
          .arg(inc.isEmpty() ? QString("ING THINK") : inc.join(", "))
          .arg(banks::researchBanks(banks::Use::LinkOnly).size() + banks::researchBanks(banks::Use::Excluded).size() - 5)
          .arg(f.error().isEmpty() ? QString() : "  ⚠ " + f.error());
      p.drawText(QRectF(12, y0 + 84, tw, 76), Qt::TextWordWrap, banksTxt);
    }
  if (m_deskCalls->isVisible()) {
    p.setFont(theme::mono(12, QFont::Medium, 2.0)); p.setPen(theme::map("#e2d59c"));
    p.drawText(QPointF(10, m_deskCalls->y() - 8), QCoreApplication::translate("banks", "All calls"));
  }
}

void BankPositioningPage::mouseMoveEvent(QMouseEvent* e) {
  int h = -1;
  if (forecastBtn().contains(e->position())) h = 0;
  else if (deskBtn().contains(e->position())) h = 1;
  else if (callsBtn().contains(e->position())) h = 2;
  if (h != m_hoverBtn) { m_hoverBtn = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(QRect(0, 0, width(), kHeaderH)); }
}
void BankPositioningPage::mousePressEvent(QMouseEvent* e) {
  if (forecastBtn().contains(e->position())) openOverlay();
  else if (deskBtn().contains(e->position())) setView(0);
  else if (callsBtn().contains(e->position())) setView(1);
}
void BankPositioningPage::leaveEvent(QEvent*) { if (m_hoverBtn >= 0) { m_hoverBtn = -1; update(QRect(0, 0, width(), kHeaderH)); } }
}  // namespace banks::pos
