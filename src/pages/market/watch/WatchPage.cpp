#include "pages/market/watch/WatchPage.h"
#include "pages/market/watch/WatchList.h"
#include "pages/market/watch/WatchHistory.h"
#include "pages/market/watch/WatchIcons.h"
#include "pages/market/watch/InstrumentSheet.h"
#include "pages/market/watch/CandleChart.h"
#include "app/Pages.h"
#include "core/Settings.h"
#include "core/I18n.h"
#include "market/QuoteHub.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QRadialGradient>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedLayout>
#include <QLineEdit>
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QVariantAnimation>
#include <QTimer>
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>
#include <climits>

DW_PAGE("market/markets-watch", watch::WatchPage)

namespace watch {
namespace {
// Page background: warm near-black with the faint glow at the top right of the reference.
class Board : public QWidget {
public:
  using QWidget::QWidget;
protected:
  void paintEvent(QPaintEvent* e) override {
    QPainter p(this);
    p.fillRect(e->rect(), theme::map(QColor(0x04, 0x03, 0x02)));
    if (e->rect().top() < 400 && theme::isDark()) {   // warm glow: dropped on light themes (looks dirty there)
      QRadialGradient g(QPointF(width() * 0.8125, 20), 520);
      g.setColorAt(0, theme::map(QColor(0x17, 0x13, 0x02))); g.setColorAt(0.5, theme::map(QColor(0x0d, 0x0a, 0x02, 120))); g.setColorAt(1, theme::map(QColor(0x04, 0x03, 0x02, 0)));
      p.fillRect(QRectF(width() * 0.8125 - 520, 0, 1040, 540).intersected(e->rect()), g);
    }
  }
};
// Default (curated) order of each column, as in the reference: majors first, then crosses, then the rest.
QList<QString> curated(market::Category c, const QStringList& head) {
  QList<QString> out;
  for (const auto& s : head) if (const auto* i = market::find(s); i && i->category == c) out << i->symbol;
  for (const auto& i : market::instruments()) if (i.category == c && !out.contains(i.symbol)) out << i.symbol;
  return out;
}
// translated where shown (pages are re-created on a language change; nothing translated is cached here)
const char* kPeriodWords[] = {QT_TRANSLATE_NOOP("watch", "TODAY"), QT_TRANSLATE_NOOP("watch", "1 WEEK"), QT_TRANSLATE_NOOP("watch", "1 MONTH")};
const char* kPeriodCols[] = {QT_TRANSLATE_NOOP("watch", "1D"), QT_TRANSLATE_NOOP("watch", "1W"), QT_TRANSLATE_NOOP("watch", "1M")};
QString wt(const char* s) { return QCoreApplication::translate("watch", s); }
QString categoryLabel(market::Category c) {
  switch (c) {
    case market::Category::Forex: return QCoreApplication::translate("watch", "Forex");
    case market::Category::Index: return QCoreApplication::translate("watch", "Indices");
    case market::Category::Commodity: return QCoreApplication::translate("watch", "Commodities");
    case market::Category::Stock: return QCoreApplication::translate("watch", "Stocks");
    case market::Category::Crypto: return QCoreApplication::translate("watch", "Crypto");
    case market::Category::Bond: return QCoreApplication::translate("watch", "Bonds");
  }
  return {};
}
}

WatchPage::WatchPage(QWidget* parent) : QWidget(parent) {
  m_stack = new QStackedLayout(this);
  m_stack->setContentsMargins(0, 0, 0, 0);
  m_scroll = new QScrollArea(this);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // the reference shows no scroll bar
  m_scroll->setWidgetResizable(false);
  m_board = new Board;
  m_scroll->setWidget(m_board);
  m_scroll->viewport()->installEventFilter(this);
  m_stack->addWidget(m_scroll);
  m_scrollAnim = new QVariantAnimation(this);
  m_scrollAnim->setDuration(380); m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_scrollAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_scroll->verticalScrollBar()->setValue(v.toInt()); });

  m_hero = new HeroHeader(m_board);
  m_search = new SearchBox(m_board);
  m_search->setCount(market::instruments().size());
  m_pills = new CategoryPills(m_board);
  for (int i = 0; i < 4; ++i) m_cards[i] = new StatCard(StatCard::Kind(i), m_board);
  m_period = new Segmented(QCoreApplication::translate("watch", "CHANGE OVER"), {wt(kPeriodCols[0]), wt(kPeriodCols[1]), wt(kPeriodCols[2])}, m_board);
  m_order = new Segmented(QCoreApplication::translate("watch", "ORDER"), {QCoreApplication::translate("watch", "DEFAULT"), QCoreApplication::translate("watch", "PERFORMANCE"), QCoreApplication::translate("watch", "A-Z")}, m_board);

  using C = market::Category;
  m_lists << curated(C::Forex, {"EURUSD", "GBPUSD", "USDJPY", "USDCHF", "USDCAD", "AUDUSD", "NZDUSD", "EURGBP", "EURJPY",
                                "EURCHF", "EURAUD", "EURCAD", "EURNZD", "GBPJPY", "GBPCHF", "GBPAUD", "AUDJPY", "GBPCAD",
                                "GBPNZD", "AUDNZD", "AUDCAD", "AUDCHF", "NZDJPY", "NZDCAD", "NZDCHF", "CADJPY", "CADCHF", "CHFJPY"})
          << curated(C::Index, {"SPX", "NDX", "DOW", "DAX", "FTSE", "VIX", "NIKKEI", "HSI", "CAC", "RUT", "DXY", "STOXX50",
                                "NASDAQ", "ASX200", "SSEC", "KOSPI", "SENSEX"})
          << curated(C::Commodity, {"XAUUSD", "XAGUSD", "WTI", "BRENT", "NATGAS", "COPPER", "XPTUSD", "XPDUSD", "CORN", "WHEAT",
                                    "SOYBEAN", "SUGAR", "COFFEE", "COCOA", "HEATOIL", "GASOLINE"})
          << curated(C::Stock, {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK.B", "JPM", "V", "MA"})
          << curated(C::Crypto, {"BTCUSD", "ETHUSD", "SOLUSD", "XRPUSD", "BNBUSD", "ADAUSD", "DOGEUSD", "DOTUSD", "LINKUSD", "AVAXUSD", "LTCUSD"})
          << curated(C::Bond, {});
  // panel titles (shorter than the pills where French is long: "MAT. PREMIÈRES")
  const QStringList titles{QCoreApplication::translate("watch", "FOREX"), QCoreApplication::translate("watch", "INDICES"), QCoreApplication::translate("watch", "COMMODITIES", "panel title"), QCoreApplication::translate("watch", "STOCKS"), QCoreApplication::translate("watch", "CRYPTO"), QCoreApplication::translate("watch", "BONDS")};
  for (int i = 0; i < titles.size(); ++i) {
    auto* p = new MarketPanel(titles[i], i == 3, m_board);
    p->setTotal(m_lists[i].size());
    connect(p, &MarketPanel::open, this, &WatchPage::openSheet);
    m_panels << p;
  }
  m_history = new WatchHistory(this);
  connect(m_history, &WatchHistory::updated, this, [this] { if (m_period->current() > 0) refresh(); });

  // state
  m_cat = std::clamp(settings::value("market.markets-watch.category", 0).toInt(), 0, 6);
  m_pills->setCurrent(m_cat);
  m_period->setCurrent(std::clamp(settings::value("market.markets-watch.period", 0).toInt(), 0, 2));
  m_order->setCurrent(std::clamp(settings::value("market.markets-watch.order", 0).toInt(), 0, 2));
  connect(m_pills, &CategoryPills::changed, this, [this](int i) { m_cat = i; settings::setValue("market.markets-watch.category", i); refresh(); });
  connect(m_period, &Segmented::changed, this, [this](int i) {
    settings::setValue("market.markets-watch.period", i);
    if (i > 0) { QStringList all; for (const auto& l : m_lists) all << l; m_history->ensure(all); }
    refresh();
  });
  connect(m_order, &Segmented::changed, this, [this](int i) { settings::setValue("market.markets-watch.order", i); refresh(); });
  connect(m_search, &SearchBox::textChanged, this, [this] { refresh(); });
  m_search->edit()->installEventFilter(this);

  // live quotes
  QStringList all; for (const auto& l : m_lists) all << l;
  market::QuoteHub::instance().subscribe(m_board, all, false);
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, [this] {
    if (m_stack->currentIndex() != 0 || !isVisible()) return;
    if (m_period->current() > 0) ensureHistory();   // asks again for 1W / 1M closes a failed fetch left out
    refresh();
  });
  if (m_period->current() > 0) m_history->ensure(all);

  // "/" focuses the search: an application key filter while the page is visible, matched on the typed text so it
  // works on every keyboard layout (on AZERTY "/" is Shift+:); the line edit keeps "/" once it has the focus.

  // test hooks
  const QString st = qEnvironmentVariable("DW_WATCH_STATE");
  if (!st.isEmpty()) {
    const auto f = st.split(',');
    if (f.size() > 0) { m_cat = f[0].toInt(); m_pills->setCurrent(m_cat); }
    if (f.size() > 1) { m_period->setCurrent(f[1].toInt()); if (f[1].toInt() > 0) m_history->ensure(all); }
    if (f.size() > 2) m_order->setCurrent(f[2].toInt());
    if (f.size() > 3) m_search->edit()->setText(f[3]);
  }
  refresh();
  if (const QString s = qEnvironmentVariable("DW_WATCH_SHEET"); !s.isEmpty()) {
    QTimer::singleShot(0, this, [this, s] {
      openSheet(s);
      if (const int y = qEnvironmentVariableIntValue("DW_WATCH_SCROLL"); y > 0)
        QTimer::singleShot(4000, this, [this, y] { if (m_sheet) m_sheet->scrollTo(y); });
      if (const QString h = qEnvironmentVariable("DW_WATCH_HOVER"); !h.isEmpty())
        QTimer::singleShot(4500, this, [this, h] { const auto xy = h.split(','); if (m_sheet && xy.size() == 2) m_sheet->chart()->hoverAt(QPointF(xy[0].toDouble(), xy[1].toDouble())); });
    });
  }
}

void WatchPage::setActive(bool on) { market::QuoteHub::instance().setActive(m_board, on); }
void WatchPage::ensureHistory() { QStringList all; for (const auto& l : m_lists) all << l; m_history->ensure(all); }
void WatchPage::showEvent(QShowEvent*) {
  qApp->installEventFilter(this);
  setActive(m_stack->currentIndex() == 0);
  if (m_period->current() > 0) ensureHistory();
  if (m_stack->currentIndex() == 0) refresh();
}
void WatchPage::hideEvent(QHideEvent*) { qApp->removeEventFilter(this); setActive(false); }
void WatchPage::preload() {
  setActive(true);    // the board's first quotes are asked (QuoteHub), then the board is left inactive: no polling
  setActive(false);
  if (m_period->current() > 0) ensureHistory();
}

void WatchPage::openSheet(const QString& symbol) {
  if (!m_sheet) {
    m_sheet = new InstrumentSheet(this);
    m_stack->addWidget(m_sheet);
    connect(m_sheet, &InstrumentSheet::closed, this, &WatchPage::closeSheet);
  }
  m_stack->setCurrentWidget(m_sheet);
  setActive(false);
  m_sheet->open(symbol);
  m_sheet->setFocus();
}
void WatchPage::closeSheet() {
  m_stack->setCurrentIndex(0);
  setActive(true);
  refresh();
}

double WatchPage::changeOf(const QString& s, QString* tip) const {
  const auto q = market::QuoteHub::instance().quote(s);
  const int per = m_period->current();
  if (per == 0) return q.valid() ? q.changePct : NAN;
  if (!q.valid()) return NAN;
  const QDate anchor = q.time.isValid() ? q.time.toUTC().date() : QDate::currentDate();
  const QDate target = per == 1 ? anchor.addDays(-7) : anchor.addMonths(-1);
  QDate when;
  const double ref = m_history->closeOnOrBefore(s, target, &when);
  if (!std::isfinite(ref) || ref == 0) return NAN;
  const auto* ins = market::find(s);
  if (tip) *tip = QCoreApplication::translate("watch", "%1 change: last %2 vs close of %3 (%4)").arg(wt(kPeriodCols[per]), fmt::num(q.price, ins ? ins->decimals : 2),
                                                                  i18n::locale().toString(when, "ddd dd MMM yyyy"), fmt::num(ref, ins ? ins->decimals : 2));
  return (q.price / ref - 1.0) * 100.0;
}

void WatchPage::refresh() {
  const QString q = m_search->edit()->text().trimmed().toLower();
  const QString qn = QString(q).remove('/').remove(' ');
  auto match = [&](const market::Instrument& i) {
    if (q.isEmpty()) return true;
    const QString lab = marketLabel(i.symbol).toLower();
    return lab.contains(q) || i.name.toLower().contains(q) || i.symbol.toLower().contains(qn) ||
           QString(lab).remove('/').contains(qn) || i.yahoo.toLower().contains(q);
  };
  const int per = m_period->current(), ord = m_order->current();
  QList<int> counts(7, 0);
  QVector<Row> shownAll;
  QList<QVector<Row>> rows;
  for (int k = 0; k < m_lists.size(); ++k) {
    QVector<Row> rs;
    for (const auto& s : m_lists[k]) {
      const auto* i = market::find(s);
      if (!i || !match(*i)) continue;
      Row r; r.symbol = s; r.label = marketLabel(s); r.decimals = i->decimals;
      if (i->category == market::Category::Stock) r.sub = i->name;
      const auto qt = market::QuoteHub::instance().quote(s);
      r.last = qt.valid() ? qt.price : NAN;
      r.chg = changeOf(s, &r.chgTip);
      rs << r;
    }
    if (ord == 1) std::stable_sort(rs.begin(), rs.end(), [](const Row& a, const Row& b) {
      if (!std::isfinite(a.chg)) return false; if (!std::isfinite(b.chg)) return true; return a.chg > b.chg; });
    else if (ord == 2) std::stable_sort(rs.begin(), rs.end(), [](const Row& a, const Row& b) { return a.label.compare(b.label, Qt::CaseInsensitive) < 0; });
    counts[k + 1] = rs.size(); counts[0] += rs.size();
    if (m_cat == 0 || m_cat == k + 1) shownAll += rs;
    rows << rs;
  }
  m_pills->setItems({QCoreApplication::translate("watch", "ALL"), QCoreApplication::translate("watch", "FOREX"), QCoreApplication::translate("watch", "INDICES"), QCoreApplication::translate("watch", "COMMODITIES"), QCoreApplication::translate("watch", "STOCKS"), QCoreApplication::translate("watch", "CRYPTO"), QCoreApplication::translate("watch", "BONDS")}, counts);
  m_hero->setCount(shownAll.size());
  for (int k = 0; k < m_panels.size(); ++k) {
    m_panels[k]->setPeriod(wt(kPeriodCols[per]));
    m_panels[k]->setRows(rows[k]);
    m_panels[k]->setVisible(m_cat == 0 || m_cat == k + 1);
  }
  // summary cards over the markets shown
  int up = 0, dn = 0, flat = 0, n = 0; double sum = 0; const Row* best = nullptr; const Row* worst = nullptr;
  for (const auto& r : shownAll) {
    if (!std::isfinite(r.chg)) continue;
    const QString t = fmt::pct(r.chg);
    if (t == "0.00%") ++flat; else if (r.chg > 0) ++up; else ++dn;
    sum += r.chg; ++n;
    if (!best || r.chg > best->chg) best = &r;
    if (!worst || r.chg < worst->chg) worst = &r;
  }
  for (auto* c : m_cards) c->setPeriod(wt(kPeriodWords[per]));
  if (n) {
    m_cards[0]->setBreadth(up, dn, flat);
    m_cards[1]->setAverage(sum / n, n);
    auto name = [](const Row* r) {
      const auto* i = market::find(r->symbol);
      if (!i) return r->label;
      return i->name.compare(r->label, Qt::CaseInsensitive) == 0 ? categoryLabel(i->category) : i->name;
    };
    m_cards[2]->setMarket(best->label, name(best), best->chg);
    m_cards[3]->setMarket(worst->label, name(worst), worst->chg);
  } else for (auto* c : m_cards) c->clear();
  layoutBoard();
}

// Number of list columns for a board width: 5 at the reference (1600), fewer on small laptops, more on wide
// monitors (where the tallest panels spread their rows over two columns).
static int columnsFor(int W) {
  if (W < 1180) return 3;
  if (W < 1500) return 4;   // 5 columns of 270 px (1440) elide the index names
  if (W < 1800) return 5;
  if (W < 2300) return 6;
  if (W < 3000) return 7;
  return 8;
}

void WatchPage::layoutBoard() {
  const int W = std::max(600, m_scroll->viewport()->width());
  // short windows (1024x600, 1280x720): a one-line title and a thinner search box, so the lists start ~85 px higher
  const bool cpt = m_scroll->viewport()->height() < 700;
  const int hh = HeroHeader::heightFor(cpt), sh = cpt ? 38 : 46, dy = cpt ? 126 - hh + 4 + 46 - sh : 0;
  m_hero->setCompact(cpt);
  m_hero->setGeometry(0, 0, W, hh);
  m_search->setGeometry(21, 137 - (cpt ? 126 - hh + 4 : 0), W - 43, sh);
  m_pills->setGeometry(21, 196 - dy, std::min(W - 42, m_pills->sizeHint().width()), 28);
  const double cw = (W - 40 - 3 * 7) / 4.0;
  for (int i = 0; i < 4; ++i) m_cards[i]->setGeometry(int(20 + i * (cw + 7)), 236 - dy, int(cw), 81);
  m_period->setGeometry(20, 330 - dy, m_period->sizeHint().width(), 24);
  m_order->setGeometry(20 + m_period->sizeHint().width() + 16, 330 - dy, m_order->sizeHint().width(), 24);
  const int top = 364 - dy, gap = 8;
  int bottom = top;
  if (m_cat == 0) {
    const int C = columnsFor(W);
    const double pw = (W - 40 - (C - 1) * gap) / double(C);
    auto colX = [&](int c) { return int(std::round(20 + c * (pw + gap))); };
    // masonry: each panel goes where its span of columns is lowest; spans > 1 only on wide boards (C ≥ 6),
    // chosen among a few candidates to get the lowest page
    const int P = m_panels.size();
    auto place = [&](const QVector<int>& span, QVector<int>* startOut, QVector<int>* yOut) {
      QVector<int> h(C, top);
      for (int k = 0; k < P; ++k) {
        m_panels[k]->setColumns(span[k]);
        const int ph = m_panels[k]->heightFor();
        int best = -1, bestY = INT_MAX;
        for (int c = 0; c + span[k] <= C; ++c) {
          int y = 0; for (int j = c; j < c + span[k]; ++j) y = std::max(y, h[j]);
          if (y < bestY) { bestY = y; best = c; }
        }
        for (int j = best; j < best + span[k]; ++j) h[j] = bestY + ph + gap;
        if (startOut) (*startOut)[k] = best;
        if (yOut) (*yOut)[k] = bestY;
      }
      return *std::max_element(h.begin(), h.end());
    };
    QVector<int> bestSpan(P, 1);
    if (C >= 6) {
      int bestH = place(bestSpan, nullptr, nullptr);
      for (int mask = 1; mask < 8; ++mask) {          // forex, stocks, indices may take two columns
        QVector<int> sp(P, 1);
        if (mask & 1) sp[0] = 2;
        if (mask & 2) sp[3] = 2;
        if (mask & 4) sp[1] = 2;
        const int h = place(sp, nullptr, nullptr);
        if (h < bestH - 20) { bestH = h; bestSpan = sp; }
      }
    }
    QVector<int> start(P), ys(P);
    place(bestSpan, &start, &ys);
    for (int k = 0; k < P; ++k) {
      auto* p = m_panels[k];
      p->setColumns(bestSpan[k]);
      const int x = colX(start[k]), x2 = colX(start[k] + bestSpan[k]) - gap;
      p->setGeometry(x, ys[k], x2 - x, p->heightFor());
      bottom = std::max(bottom, ys[k] + p->heightFor());
    }
  } else {
    auto* p = m_panels[m_cat - 1];
    const int perCol = m_cat == 4 ? 340 : 300;      // stock rows carry the company name
    p->setColumns(std::clamp((W - 40) / perCol, 1, 8));
    p->setGeometry(20, top, W - 40, p->heightFor());
    bottom = top + p->heightFor();
  }
  m_board->resize(W, bottom + 24);
}

bool WatchPage::eventFilter(QObject* o, QEvent* e) {
  if (e->type() == QEvent::KeyPress && o->isWidgetType() && static_cast<QWidget*>(o)->window() == window()) {
    auto* ke = static_cast<QKeyEvent*>(e);
    if (ke->text() == "/" && m_stack->currentIndex() == 0 && !qobject_cast<QLineEdit*>(QApplication::focusWidget())) {
      m_search->edit()->setFocus(); m_search->edit()->selectAll(); return true;
    }
  }
  if (o == m_scroll->viewport()) {
    if (e->type() == QEvent::Resize) layoutBoard();
    else if (e->type() == QEvent::Wheel) {
      auto* we = static_cast<QWheelEvent*>(e);
      auto* sb = m_scroll->verticalScrollBar();
      if (!we->pixelDelta().isNull()) { m_scrollAnim->stop(); sb->setValue(sb->value() - we->pixelDelta().y()); return true; }
      const int base = m_scrollAnim->state() == QAbstractAnimation::Running ? m_scrollTarget : sb->value();
      m_scrollTarget = std::clamp(base - we->angleDelta().y(), sb->minimum(), sb->maximum());
      m_scrollAnim->stop(); m_scrollAnim->setStartValue(sb->value()); m_scrollAnim->setEndValue(m_scrollTarget); m_scrollAnim->start();
      return true;
    }
  } else if (o == m_search->edit() && e->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(e);
    if (ke->key() == Qt::Key_Escape) { m_search->edit()->clear(); m_search->edit()->clearFocus(); return true; }
    if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !m_search->edit()->text().trimmed().isEmpty()) {
      for (auto* p : m_panels) if (p->isVisible() && !p->firstSymbol().isEmpty()) { openSheet(p->firstSymbol()); return true; }
    }
  }
  return QWidget::eventFilter(o, e);
}
}
