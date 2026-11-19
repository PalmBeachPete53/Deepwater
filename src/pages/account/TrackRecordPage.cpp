#include "pages/account/TrackRecord.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include "pages/account/TrDialogs.h"
#include "account/BridgeSync.h"
#include "account/Contracts.h"
#include "app/Pages.h"
#include "core/Settings.h"
#include "ui/Format.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QMenu>
#include <QMessageBox>
#include <QHelpEvent>
#include <QToolTip>
#include <QSet>
#include <QTimer>
#include <QApplication>
#include <algorithm>

namespace acct {

// ================================================================ toolbar
TrToolbar::TrToolbar(QWidget* parent) : QWidget(parent) {
  setFixedHeight(kHeight);
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  // the focus ring shows up shortly after the click (1.0–1.5 s after the click in the reference)
  m_focusDelay.setSingleShot(true);
  connect(&m_focusDelay, &QTimer::timeout, this, [this] { m_focusShown = true; update(); });
}

int TrToolbar::hoverAt() const {
  if (PaintPanel::hasFakeCursor()) return hitTest(QPointF(mapFromGlobal(PaintPanel::fakeCursor())));
  return m_hover;
}

void TrToolbar::setAccount(const QString& name, const QString& balance, const QString& last) {
  m_name = name; m_balance = balance; m_last = last; update();
}

QString TrToolbar::tabName(int i) {
  return i == 0 ? QCoreApplication::translate("account", "DASHBOARD")
       : i == 1 ? QCoreApplication::translate("account", "TRADE LOG") : QCoreApplication::translate("account", "ADVANCED DETAILS");
}
QString TrToolbar::filtersText() { return QCoreApplication::translate("account", "FILTERS"); }
QString TrToolbar::logText() { return QCoreApplication::translate("account", "+ LOG TRADE"); }
QString TrToolbar::importText() { return QCoreApplication::translate("account", "IMPORT TRADES"); }

TrToolbar::L TrToolbar::layout() const {
  L l;
  const qreal W = width();
  // tab widths of the reference (English); longer labels (French) widen their tab
  const QFontMetricsF ft(mono(12, true, 1.9));
  const qreal refW[3] = {106, 102, 169};
  qreal tx = 10;
  for (int i = 0; i < 3; ++i) {
    const qreal w = std::max(refW[i], std::ceil(ft.horizontalAdvance(tabName(i)) + 24));
    l.tabs[i] = QRectF(tx, 27, w, 31); tx += w;
  }
  const qreal fw = std::max(87.0, std::ceil(QFontMetricsF(mono(11, false, 1.4)).horizontalAdvance(filtersText()) + 32));
  l.filters = QRectF(tx + 6, 27, fw, 31);
  const qreal logW = std::max(100.0, std::ceil(QFontMetricsF(mono(11, true, 1)).horizontalAdvance(logText()) + 24));
  const qreal impW = std::max(134.0, std::ceil(QFontMetricsF(mono(11, true, 1.3)).horizontalAdvance(importText()) + 37));
  l.log = QRectF(W - 10 - logW, 11, logW, 30);
  l.import = QRectF(W - 10 - impW, 51, impW, 26);
  const qreal lastW = m_last.isEmpty() ? 0 : QFontMetricsF(mono(11)).horizontalAdvance(m_last) + 15;
  l.lastImport = QRectF(l.log.left() - 6 - lastW, 11, lastW, 30);
  qreal accRight = (m_last.isEmpty() ? l.log.left() - 8 : l.lastImport.left() - 8);
  qreal accW = 281;
  // narrow windows: drop the last import text first, then shrink the account box
  // year box: 149 px in the reference, wider when the label is longer ("Toutes années"); on narrow windows the
  // label steps down (19 → 16 → 14 → 12 px) before the account box would go under 160 px
  auto yearW = [&](int px) {
    const qreal tw = QFontMetricsF(mono(px, true)).horizontalAdvance(m_year);
    return px == 19 ? std::max(149.0, std::ceil(tw + 46)) : std::ceil(tw + 34);
  };
  l.yearPx = 19;
  qreal yw = yearW(19);
  auto minLeftFor = [&](qreal w) { return l.filters.right() + 12 + w + 12; };
  if (accRight - accW < minLeftFor(yw) && !m_last.isEmpty()) { l.showImport = false; accRight = l.log.left() - 8; }
  for (int px : {16, 14, 12}) {
    if (accRight - 160 >= minLeftFor(yw)) break;
    l.yearPx = px; yw = yearW(px);
  }
  const qreal minLeft = minLeftFor(yw);
  if (accRight - accW < minLeft) accW = std::max<qreal>(160, accRight - minLeft);
  l.account = QRectF(accRight - accW, 8, accW, 36);
  qreal yx = W / 2.0 - 74 - (yw - 149) / 2;
  yx = std::clamp(yx, l.filters.right() + 12, l.account.left() - 12 - yw);
  l.year = QRectF(yx, 25, yw, 36);
  return l;
}

int TrToolbar::hitTest(const QPointF& p) const {
  const L l = layout();
  for (int i = 0; i < 3; ++i) if (l.tabs[i].contains(p)) return i;
  if (l.filters.contains(p)) return 3;
  if (l.year.contains(p)) return 4;
  if (l.account.contains(p)) return 5;
  if (l.log.contains(p)) return 6;
  if (l.import.contains(p)) return 7;
  return -1;
}

static void chevron(QPainter& p, const QPointF& c0, qreal s, const QColor& col, qreal w = 1.6, bool up = false) {
  QPen pen(col, w); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen); p.setBrush(Qt::NoBrush);
  const qreal d = up ? -1 : 1;
  p.drawPolyline(QPolygonF{c0 + QPointF(-s, -s / 2 * d), c0 + QPointF(0, s / 2 * d), c0 + QPointF(s, -s / 2 * d)});
}

void TrToolbar::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& k = c();
  p.fillRect(rect(), theme::map("#070503"));
  p.fillRect(QRectF(0, height() - 1, width(), 1), theme::map("#2a271c"));
  const L l = layout();
  const int hover = hoverAt();

  // view tabs
  const QRectF box = l.tabs[0].united(l.tabs[2]);
  p.setPen(theme::map("#2f291a")); p.setBrush(theme::map("#110c04"));
  p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
  for (int i = 0; i < 3; ++i) {
    const QRectF r = l.tabs[i];
    const bool on = i == m_view, hov = i == hover;
    if (on) {
      p.fillRect(r.adjusted(1, 1, -1, -1), theme::map("#2a2605"));
      p.fillRect(QRectF(r.left() + 1, r.bottom() - 3, r.width() - 2, 2), k.label);
    }
    p.setFont(mono(12, true, 1.9));
    p.setPen(on ? k.tabText : hov ? theme::map("#e8e4da") : k.dim);
    p.drawText(r.adjusted(0, -1, 0, -1), Qt::AlignCenter, tabName(i));
    if (on && m_focus == i && m_focusShown) {
      p.setPen(QPen(k.label, 1)); p.setBrush(Qt::NoBrush);
      p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    }
  }

  // FILTERS toggle
  {
    const QRectF r = l.filters;
    if (m_focus == 3 && m_focusShown) { p.setPen(QPen(theme::map("#cdb558"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5); }
    const QColor col = hover == 3 ? theme::map("#e8e4da") : theme::map("#6f6a5e");
    const QPointF f(r.left() + 13, r.center().y() - 3.5);
    QPainterPath fun; fun.moveTo(f); fun.lineTo(f + QPointF(8, 0)); fun.lineTo(f + QPointF(5, 4)); fun.lineTo(f + QPointF(5, 8));
    fun.lineTo(f + QPointF(3, 7)); fun.lineTo(f + QPointF(3, 4)); fun.closeSubpath();
    p.setPen(QPen(col, 1)); p.drawPath(fun);
    p.setFont(mono(11, false, 1.4)); p.setPen(col);
    p.drawText(QRectF(r.left() + 24, r.top(), r.width() - 24, r.height()), Qt::AlignVCenter | Qt::AlignLeft, filtersText());
  }

  // year selector
  {
    const QRectF r = l.year;
    p.setPen(theme::map(hover == 4 ? "#5a4f22" : "#3a331a")); p.setBrush(theme::map("#211c02"));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p.setFont(mono(l.yearPx, true));
    p.setPen(k.selector);
    const QFontMetricsF fm(p.font());
    const qreal tw = fm.horizontalAdvance(m_year);
    const qreal x0 = r.center().x() - (tw + 18) / 2;
    p.drawText(QPointF(x0, r.center().y() + fm.ascent() / 2 - 2), m_year);
    chevron(p, QPointF(x0 + tw + 14, r.center().y() + 1), 3.5, k.selector, 1.5);
  }

  // account box
  {
    const QRectF r = l.account;
    p.setPen(theme::map(hover == 5 ? "#5a4f22" : "#3a321a")); p.setBrush(theme::map("#1d1503"));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p.setFont(mono(9, true, 2)); p.setPen(theme::map("#e6b800"));
    p.drawText(QPointF(r.left() + 9, r.top() + 12), QCoreApplication::translate("account", "ACCOUNT"));
    const QFont bf = mono(11);
    const qreal bw = QFontMetricsF(bf).horizontalAdvance(m_balance);
    p.setFont(bf); p.setPen(theme::map("#a59f90"));
    p.drawText(QPointF(r.right() - 26 - bw, r.center().y() + 4), m_balance);
    chevron(p, QPointF(r.right() - 14, r.center().y()), 3, theme::map("#d8d2c4"), 1.3);
    const QFont nf = cond(15, true);
    p.setFont(nf); p.setPen(theme::map("#ece6d8"));
    const QString name = QFontMetricsF(nf).elidedText(m_name.isEmpty() ? QCoreApplication::translate("account", "No account · add or import one") : m_name,
                                                      Qt::ElideRight, r.width() - 9 - bw - 40);
    p.drawText(QPointF(r.left() + 9, r.top() + 29), name);
  }

  // last import
  if (l.showImport && !m_last.isEmpty()) {
    p.setFont(mono(11)); p.setPen(k.faint);
    const QPointF ic(l.lastImport.left() + 5, l.lastImport.center().y());
    p.setPen(QPen(k.faint, 1.1)); p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(ic.x() - 4, ic.y() - 4, 8, 8), 60 * 16, 280 * 16);
    p.drawLine(ic + QPointF(2, -3.5), ic + QPointF(4.2, -3.5)); p.drawLine(ic + QPointF(4.2, -3.5), ic + QPointF(4.2, -1.3));
    p.setPen(k.faint);
    p.drawText(l.lastImport.adjusted(13, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, m_last);
  }

  // + LOG TRADE
  {
    const QRectF r = l.log;
    p.setPen(theme::map(hover == 6 ? "#6a5f3c" : "#3d3618")); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p.setFont(mono(11, true, 1)); p.setPen(hover == 6 ? k.white : theme::map("#a09988"));
    p.drawText(r, Qt::AlignCenter, logText());
  }
  // IMPORT TRADES
  {
    const QRectF r = l.import;
    p.setPen(Qt::NoPen); p.setBrush(hover == 7 ? theme::map("#ffe04a") : k.accent);
    p.drawRoundedRect(r, 3, 3);
    p.setFont(mono(11, true, 1.3)); p.setPen(theme::onAccent());
    const QFontMetricsF fm(p.font());
    const QString t = importText();
    const qreal tw = fm.horizontalAdvance(t);
    const qreal x0 = r.center().x() - (tw + 13) / 2;
    // upload glyph
    QPen pen(theme::onAccent(), 1.3); p.setPen(pen);
    const QPointF g(x0 + 4, r.center().y());
    p.drawLine(g + QPointF(0, 3), g + QPointF(0, -4));
    p.drawLine(g + QPointF(-2.5, -1.5), g + QPointF(0, -4)); p.drawLine(g + QPointF(2.5, -1.5), g + QPointF(0, -4));
    p.drawLine(g + QPointF(-4, 2), g + QPointF(-4, 4)); p.drawLine(g + QPointF(-4, 4), g + QPointF(4, 4)); p.drawLine(g + QPointF(4, 4), g + QPointF(4, 2));
    p.drawText(QPointF(x0 + 13, r.center().y() + fm.ascent() / 2 - 1.5), t);
  }
}

void TrToolbar::mouseMoveEvent(QMouseEvent* e) {
  const int h = hitTest(e->position());
  if (h != m_hover) { m_hover = h; update(); }
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void TrToolbar::leaveEvent(QEvent*) { m_hover = -1; update(); }
void TrToolbar::focusOutEvent(QFocusEvent* e) {
  if (e->reason() == Qt::ActiveWindowFocusReason || e->reason() == Qt::PopupFocusReason) return;   // keeps its ring, like a page
  m_focus = -1; m_focusShown = false; m_focusDelay.stop(); update();
}
void TrToolbar::mousePressEvent(QMouseEvent* e) {
  const int h = hitTest(e->position());
  if (h >= 0 && h <= 3) { m_focus = h; m_focusShown = false; m_focusDelay.start(1300); }
  if (h >= 0 && h <= 2) emit viewClicked(h);
  else if (h == 3) { emit filtersClicked(); }
  else if (h == 4) emit yearClicked();
  else if (h == 5) emit accountClicked();
  else if (h == 6) emit logClicked();
  else if (h == 7) emit importClicked();
  update();
}
bool TrToolbar::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const L l = layout();
    QString tip;
    if (l.account.contains(he->pos())) tip = m_name + (m_last.isEmpty() ? QString() : "\n" + m_last) + "\n" +
                                             QCoreApplication::translate("account", "Starting balance %1").arg(m_balance);
    else if (l.import.contains(he->pos())) tip = QCoreApplication::translate("account", "Import an MT4 / MT5 statement, a cTrader or CSV history, or set up the live bridge");
    if (tip.isEmpty()) QToolTip::hideText(); else QToolTip::showText(he->globalPos(), ui::plainTip(tip), this);   // account names from files
    return true;
  }
  return QWidget::event(e);
}

// ================================================================ smooth scroll
SmoothScroll::SmoothScroll(QWidget* parent) : QScrollArea(parent), m_anim(new QVariantAnimation(this)) {
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidgetResizable(false);
  viewport()->setAutoFillBackground(false);
  m_anim->setDuration(220); m_anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { verticalScrollBar()->setValue(v.toInt()); });
}
void SmoothScroll::scrollTo(int y, bool animated) {
  auto* sb = verticalScrollBar();
  m_target = std::clamp(y, sb->minimum(), sb->maximum());
  m_anim->stop();
  if (!animated) { sb->setValue(m_target); return; }
  m_anim->setStartValue(sb->value()); m_anim->setEndValue(m_target); m_anim->start();
}
void SmoothScroll::wheelEvent(QWheelEvent* e) {
  auto* sb = verticalScrollBar();
  if (!e->pixelDelta().isNull()) { m_anim->stop(); sb->setValue(sb->value() - e->pixelDelta().y()); e->accept(); return; }
  const int base = m_anim->state() == QAbstractAnimation::Running ? m_target : sb->value();
  scrollTo(base - e->angleDelta().y() * 60 / 120);
  e->accept();
}

// ================================================================ page
TrackRecordPage::TrackRecordPage(QWidget* parent) : QWidget(parent) {
  m_bar = new TrToolbar(this);

  m_filterBar = new QWidget(this);
  m_fInstr = new ComboChip(m_filterBar); m_fSide = new ComboChip(m_filterBar); m_fStrat = new ComboChip(m_filterBar);
  m_fSide->setItems({QCoreApplication::translate("account", "Long & short"), QCoreApplication::translate("account", "Long only"),
                     QCoreApplication::translate("account", "Short only")});
  m_counter = new PaintPanel(m_filterBar);
  m_counter->setFrame(false);
  m_counter->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    p.fillRect(QRectF(0, r.height() - 1, r.width(), 1), c().divider);
    p.setFont(mono(10)); p.setPen(theme::map("#3e3a31"));
    p.drawText(r.adjusted(0, 0, -8, -1), Qt::AlignVCenter | Qt::AlignRight,
               QCoreApplication::translate("account", "%1 of %2 trades").arg(m_data.trades.size()).arg(m_data.total));
  });
  m_counter->lower();
  for (auto* cc : {m_fInstr, m_fSide, m_fStrat}) connect(cc, &ComboChip::changed, this, [this] { applyFilters(); });
  m_filterBar->hide();

  m_scroll = new SmoothScroll(this);
  m_content = new QWidget;
  m_scroll->setWidget(m_content);
  m_views[0] = new DashboardView(m_content);
  m_views[1] = new TradeLogView(m_content);
  m_views[2] = new AdvancedView(m_content);
  for (auto* v : m_views) {
    connect(v, &TrView::tradeEdit, this, [this](const QString& id) { logTrade(id); });
    connect(v, &TrView::tradeDelete, this, [this](const QString& id) { deleteTrade(id); });
    connect(v, &TrView::relayoutNeeded, this, [this] { layoutPage(); }, Qt::QueuedConnection);
  }
  m_empty = new PaintPanel(m_content);
  m_empty->setFrame(false);
  m_empty->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    const QRectF box(r.center().x() - 330, 60, 660, 250);
    panelFrame(p, box, true);
    p.setFont(mono(16, true, 3)); p.setPen(c().white);
    const bool noAcc = !m_data.hasAccount, noTrades = m_data.total == 0;
    p.drawText(QRectF(box.left(), box.top() + 40, box.width(), 30), Qt::AlignCenter,
               noAcc ? QCoreApplication::translate("account", "NO ACCOUNT YET") : noTrades ? QCoreApplication::translate("account", "NO TRADES YET") : QCoreApplication::translate("account", "NO TRADE MATCHES THE FILTERS"));
    p.setFont(cond(16, false)); p.setPen(c().grey);
    const QString msg = noTrades
      ? QCoreApplication::translate("account", "Import a statement from MetaTrader 4, MetaTrader 5 or cTrader, connect the live read-only bridge, "
                                               "or log your trades by hand. Your journal stays on this computer.")
      : QCoreApplication::translate("account", "Change the year or the filters to see trades again.");
    p.drawText(QRectF(box.left() + 40, box.top() + 82, box.width() - 80, 60), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, msg);
    if (noTrades) {
      const QRectF imp(box.center().x() + 8, box.top() + 160, 170, 32), log(box.center().x() - 178, box.top() + 160, 170, 32);
      p.setPen(Qt::NoPen); p.setBrush(c().accent); p.drawRoundedRect(imp, 3, 3);
      p.setFont(mono(12, true, 2)); p.setPen(theme::onAccent()); p.drawText(imp, Qt::AlignCenter, TrToolbar::importText());
      p.setPen(theme::map("#3a3424")); p.setBrush(Qt::NoBrush); p.drawRoundedRect(log.adjusted(.5, .5, -.5, -.5), 2, 2);
      p.setPen(theme::map("#a09988")); p.drawText(log, Qt::AlignCenter, TrToolbar::logText());
      p.setFont(mono(10)); p.setPen(c().faint);
      p.drawText(QRectF(box.left(), box.top() + 206, box.width(), 20), Qt::AlignCenter,
                 QCoreApplication::translate("account", "Just looking? Account box › Add demo account with sample trades (invented data)."));
    }
  });
  connect(m_empty, &PaintPanel::clicked, this, [this](const QPointF& pt) {
    const QRectF box(m_empty->width() / 2.0 - 330, 60, 660, 250);
    if (m_data.total != 0) return;
    if (QRectF(box.center().x() + 8, box.top() + 160, 170, 32).contains(pt)) importTrades();
    else if (QRectF(box.center().x() - 178, box.top() + 160, 170, 32).contains(pt)) logTrade();
  });

  connect(m_bar, &TrToolbar::viewClicked, this, [this](int v) { setView(v); });
  connect(m_bar, &TrToolbar::filtersClicked, this, [this] {
    m_filtersOn = !m_filtersOn; m_bar->setFiltersOn(m_filtersOn); layoutPage();
  });
  connect(m_bar, &TrToolbar::yearClicked, this, &TrackRecordPage::yearMenu);
  connect(m_bar, &TrToolbar::accountClicked, this, &TrackRecordPage::accountMenu);
  connect(m_bar, &TrToolbar::logClicked, this, [this] { logTrade(); });
  connect(m_bar, &TrToolbar::importClicked, this, &TrackRecordPage::importTrades);
  connect(&account::Journal::instance(), &account::Journal::changed, this, &TrackRecordPage::reload);
  connect(&account::Journal::instance(), &account::Journal::errorChanged, this, &TrackRecordPage::showJournalError);
  connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
    // a still pointer over moving content: hovered rows change (repaint only the visible view)
    if (m_views[m_view]->isVisible()) for (auto* w : m_views[m_view]->findChildren<PaintPanel*>()) if (w->hasMouseTracking()) w->update();
  });
  account::BridgeSync::instance();   // resumes the live sync folder saved by the user

  // ---- test hooks: DW_ACCOUNT_DEMO=1 adds the demo account, DW_TR_VIEW=dashboard|log|advanced,
  //      DW_TR_FILTERS=1 opens the filter bar, DW_TR_SCROLL=<px>, DW_HOVER=x,y fixes the pointer (page coords)
  if (qEnvironmentVariableIsSet("DW_ACCOUNT_DEMO")) account::Journal::instance().addDemoAccount();
  const QString v = qEnvironmentVariable("DW_TR_VIEW", settings::value("account.track-record.view").toString());
  m_view = v == "log" ? 1 : v == "advanced" ? 2 : 0;
  m_bar->setView(m_view);
  if (qEnvironmentVariable("DW_TR_FILTERS") == "1") { m_filtersOn = true; m_bar->setFiltersOn(true); }
  reload();
  setView(m_view, false);
  if (qEnvironmentVariableIsSet("DW_TR_SCROLL"))
    QTimer::singleShot(900, this, [this] { m_scroll->scrollTo(qEnvironmentVariable("DW_TR_SCROLL").toInt(), false); });
  installTestHooks(this);
}

void TrackRecordPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), c().page);
}

void TrackRecordPage::reload() {
  auto& j = account::Journal::instance();
  const account::Account* a = j.current();
  m_data = TrData();
  m_data.hasAccount = a != nullptr;
  QStringList instr, strat;
  m_years.clear();
  if (a) {
    m_data.accountName = a->name; m_data.currency = a->currency; m_data.balance = a->balance; m_data.demo = a->demo;
    m_data.total = a->trades.size();
    QSet<QString> si, ss; QSet<int> years;
    for (const auto& t : a->trades) {
      si.insert(account::displaySymbol(t.symbol));
      if (!t.strategy.isEmpty()) ss.insert(t.strategy);
      years.insert(t.close.toLocalTime().date().year());
    }
    instr = si.values(); std::sort(instr.begin(), instr.end());
    strat = ss.values(); std::sort(strat.begin(), strat.end());
    m_years = years.values(); std::sort(m_years.begin(), m_years.end(), std::greater<int>());
    const QString bal = fmt::num(a->balance, a->balance == std::floor(a->balance) ? 0 : 2) + " " + a->currency;
    m_bar->setAccount(a->demo ? QCoreApplication::translate("account", "Demo account · sample trades") : a->name, bal, a->lastImport.isValid() ? QCoreApplication::translate("account", "Last import %1").arg(longDate(a->lastImport)) : QString());
  } else {
    m_bar->setAccount(QString(), QString(), QString());
  }
  if (m_year && !m_years.contains(m_year)) m_year = 0;
  m_bar->setYearText(m_year ? QString::number(m_year) : QCoreApplication::translate("account", "All years"));
  const QString ci = m_fInstr->currentText(), cs = m_fStrat->currentText();
  m_fInstr->setItems(QStringList{QCoreApplication::translate("account", "All instruments")} + instr, std::max<int>(0, instr.indexOf(ci) + 1));
  m_fStrat->setItems(QStringList{QCoreApplication::translate("account", "All strategies")} + strat, std::max<int>(0, strat.indexOf(cs) + 1));
  applyFilters();
}

void TrackRecordPage::applyFilters() {
  const account::Account* a = account::Journal::instance().current();
  m_data.trades.clear();
  if (a) {
    const QString instr = m_fInstr->current() > 0 ? m_fInstr->currentText() : QString();
    const QString strat = m_fStrat->current() > 0 ? m_fStrat->currentText() : QString();
    const int side = m_fSide->current();
    for (const auto& t : a->trades) {
      if (m_year && t.close.toLocalTime().date().year() != m_year) continue;
      if (!instr.isEmpty() && account::displaySymbol(t.symbol) != instr) continue;
      if (!strat.isEmpty() && t.strategy != strat) continue;
      if (side == 1 && t.side != account::Side::Long) continue;
      if (side == 2 && t.side != account::Side::Short) continue;
      m_data.trades << t;
    }
  }
  m_data.stats = account::compute(m_data.trades, m_data.balance);
  for (auto* v : m_views) v->setData(m_data);
  m_counter->update();
  layoutPage();
}

void TrackRecordPage::setView(int v, bool animate) {
  m_view = v;
  m_bar->setView(v);
  settings::setValue("account.track-record.view", v == 1 ? "log" : v == 2 ? "advanced" : "dashboard");
  m_scroll->scrollTo(0, false);
  layoutPage();
  if (animate) m_views[v]->playEntry();
}

void TrackRecordPage::layoutPage() {
  const int W = width();
  m_bar->setGeometry(0, 0, W, TrToolbar::kHeight);
  int y = TrToolbar::kHeight;
  m_filterBar->setVisible(m_filtersOn);
  if (m_filtersOn) {
    m_filterBar->setGeometry(0, y, W, 39);
    m_counter->setGeometry(0, 0, W, 39);
    int x = 8;
    for (auto* cc : {m_fInstr, m_fSide, m_fStrat}) {
      const int w = cc->sizeHint().width();
      cc->setGeometry(x, 5, w, 26);
      x += w + 9;
    }
    y += 39;
  }
  m_scroll->setGeometry(0, y, W, height() - y);
  const int vw = m_scroll->viewport()->width();
  const bool empty = m_data.trades.isEmpty();
  int h = 0;
  for (int i = 0; i < 3; ++i) {
    const bool on = i == m_view && !empty;
    m_views[i]->setVisible(on);
    if (on) { h = m_views[i]->relayout(vw, m_scroll->viewport()->height()); m_views[i]->setGeometry(0, 0, vw, h); }
  }
  m_empty->setVisible(empty);
  if (empty) { h = std::max(m_scroll->viewport()->height(), 420); m_empty->setGeometry(0, 0, vw, h); }
  m_content->resize(vw, h);
}

void TrackRecordPage::resizeEvent(QResizeEvent*) { layoutPage(); }
void TrackRecordPage::showEvent(QShowEvent*) {
  layoutPage();
  QTimer::singleShot(0, this, &TrackRecordPage::showJournalError);   // a load problem from before the first visit
}

void TrackRecordPage::showJournalError() {
  const QString e = account::Journal::instance().lastError();
  if (e.isEmpty() || e == m_shownError || !isVisible()) return;
  m_shownError = e;
  auto* mb = new QMessageBox(QMessageBox::Warning, QCoreApplication::translate("account", "Trading journal"), e, QMessageBox::Ok, this);
  mb->setTextFormat(Qt::PlainText);   // errors quote file and account names
  mb->setAttribute(Qt::WA_DeleteOnClose);
  mb->open();                                          // not exec(): no nested event loop
}
void TrackRecordPage::hideEvent(QHideEvent*) {}

void TrackRecordPage::yearMenu() {
  QMenu m(this); styleMenu(&m);
  auto add = [&](const QString& t, int y) { auto* a = m.addAction(t); a->setCheckable(true); a->setChecked(m_year == y); a->setData(y); };
  add(QCoreApplication::translate("account", "All years"), 0);
  for (int y : m_years) add(QString::number(y), y);
  const QRect r = m_bar->yearRect();
  if (auto* a = m.exec(m_bar->mapToGlobal(QPoint(r.left(), r.bottom() + 3)))) {
    m_year = a->data().toInt();
    m_bar->setYearText(m_year ? QString::number(m_year) : QCoreApplication::translate("account", "All years"));
    applyFilters();
  }
}

void TrackRecordPage::accountMenu() {
  auto& j = account::Journal::instance();
  QMenu m(this); styleMenu(&m);
  for (const auto& a : j.accounts()) {
    auto* act = m.addAction(QString("%1   %2 %3").arg(a.demo ? QCoreApplication::translate("account", "Demo account · sample trades") : a.name, fmt::num(a.balance, 0), a.currency));
    act->setCheckable(true); act->setChecked(a.id == j.currentId()); act->setData(a.id);
  }
  if (!j.accounts().isEmpty()) m.addSeparator();
  auto* add = m.addAction(QCoreApplication::translate("account", "Add account…"));
  auto* edit = m.addAction(QCoreApplication::translate("account", "Edit account…")); edit->setEnabled(j.current());
  auto* rem = m.addAction(QCoreApplication::translate("account", "Remove account…")); rem->setEnabled(j.current());
  m.addSeparator();
  bool hasDemo = false; for (const auto& a : j.accounts()) hasDemo |= a.demo;
  auto* demo = m.addAction(hasDemo ? QCoreApplication::translate("account", "Show the demo account (sample trades)") : QCoreApplication::translate("account", "Add demo account with sample trades"));
  const QRect r = m_bar->accountRect();
  QAction* a = m.exec(m_bar->mapToGlobal(QPoint(r.left(), r.bottom() + 3)));
  if (!a) return;
  if (a == add) { account::Account n; n.name = QCoreApplication::translate("account", "My account"); n.currency = "USD"; n.platform = "Manual"; if (accountDialog(this, n, true)) j.addAccount(n); }
  // modal dialogs run the event loop: a bridge file read meanwhile can add or reallocate accounts, so nothing
  // points into the journal across them (A6): copies before, a lookup by id after
  else if (a == edit) {
    if (!j.current()) return;
    account::Account n = *j.current();
    if (accountDialog(this, n, false) && j.account(n.id)) j.updateAccount(n);
  }
  else if (a == rem) {
    const auto* cur = j.current();
    if (!cur) return;
    const QString id = cur->id, name = cur->name;
    const auto count = cur->trades.size();
    const bool live = !cur->bridgeFile.isEmpty();
    QString q = QCoreApplication::translate("account", "Remove \"%1\" and its %2 trades from the journal?\nThis cannot be undone.").arg(name).arg(count);
    if (live) q += "\n" + QCoreApplication::translate("account", "The live sync will not create it again; choose the bridge folder again "
                                                                 "(Import trades › Live sync) to resume it.");
    if (ui::askPlain(this, QCoreApplication::translate("account", "Remove account"), q) == QMessageBox::Yes) j.removeAccount(id);
  } else if (a == demo) j.addDemoAccount();
  else j.setCurrent(a->data().toString());
}

bool TrackRecordPage::ensureAccount() {
  auto& j = account::Journal::instance();
  if (j.current()) return true;
  account::Account n; n.name = QCoreApplication::translate("account", "My account"); n.currency = "USD"; n.platform = "Manual";
  if (!accountDialog(this, n, true)) return false;
  j.addAccount(n);
  return true;
}

void TrackRecordPage::logTrade(const QString& editIdRef) {
  const QString editId = editIdRef;                    // the reference points into the trade log's rows (A6)
  if (!ensureAccount()) return;
  auto& j = account::Journal::instance();
  const auto* a = j.current();
  if (!a) return;
  const QString accountId = a->id, ccy = a->currency;   // copies: the dialog is modal (A6)
  account::Trade t;
  bool isNew = editId.isEmpty();
  if (!isNew) {
    bool found = false;
    for (const auto& x : a->trades) if (x.id == editId) { t = x; found = true; }
    if (!found) return;
  }
  a = nullptr;
  if (!tradeDialog(this, t, ccy, isNew)) return;
  if (!j.account(accountId)) return;                   // removed meanwhile
  if (isNew) { t.id = account::newId("manual"); t.source = "manual"; j.addTrades(accountId, {t}, false); }
  else j.updateTrade(accountId, t);
}

void TrackRecordPage::deleteTrade(const QString& idRef) {
  const QString id = idRef;                            // the reference points into the trade log's rows (A6)
  auto& j = account::Journal::instance();
  const auto* a = j.current();
  if (!a) return;
  for (const auto& t : a->trades) if (t.id == id) {
    const QString what = QCoreApplication::translate("account", "%1 %2 closed %3, %4").arg(account::displaySymbol(t.symbol), account::sideName(t.side),
                                                          tradeDate(t.close), signedMoney(t.net(), a->currency));
    const QString accountId = a->id;                   // a and t may dangle after the modal question (A6)
    if (ui::askPlain(this, QCoreApplication::translate("account", "Delete trade"),
                              QCoreApplication::translate("account", "Delete this trade from the journal?\n%1").arg(what)) == QMessageBox::Yes)
      j.removeTrade(accountId, id);
    return;
  }
}

void TrackRecordPage::importTrades() { importDialog(this); }
}

DW_PAGE("account/track-record", acct::TrackRecordPage)
