#include "pages/market/technical/TechnicalHeatmapPage.h"
#include "pages/market/technical/MarketKit.h"
#include "pages/market/technical/HeatTable.h"
#include "pages/market/technical/SignalFile.h"
#include "analytics/Scanner.h"
#include "market/QuoteHub.h"
#include "app/Pages.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "ui/Touch.h"
#include "global/PageScroll.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QCoreApplication>
#include <algorithm>

DW_PAGE("market/technical-heatmap", TechnicalHeatmapPage)

using namespace analytics;
using market::Category;

namespace {
const Category kCats[] = {Category::Forex, Category::Commodity, Category::Index, Category::Stock, Category::Crypto, Category::Bond};
QString displayName(const market::Instrument& i) {
  if (i.category == Category::Forex) return i.base + "/" + i.quote;
  if (i.category == Category::Crypto && i.symbol.endsWith("USD")) return i.symbol.left(i.symbol.size() - 3) + "/USD";
  return i.name;
}
QString shortId(const market::Instrument& i) {
  if (i.category == Category::Crypto && i.symbol.endsWith("USD")) return i.symbol.left(i.symbol.size() - 3);
  return i.symbol;
}
// Phones: the signal file shown over the page (opened by tapping a row): page backdrop, the file scrolling, a ✕
// floating at the top right beside the file's id line.
class SheetClose : public QWidget {
public:
  std::function<void()> onClose;
  explicit SheetClose(QWidget* parent) : QWidget(parent) { setCursor(Qt::PointingHandCursor); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QRectF c = QRectF(rect()).adjusted(4, 4, -4, -4);
    p.setPen(QPen(mkit::sem().panelBorder, 1)); p.setBrush(mkit::sem().panel); p.drawRoundedRect(c.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p.setPen(QPen(mkit::sem().goldText, 1.6, Qt::SolidLine, Qt::RoundCap));
    const QPointF m = c.center();
    p.drawLine(m + QPointF(-6, -6), m + QPointF(6, 6)); p.drawLine(m + QPointF(-6, 6), m + QPointF(6, -6));
  }
  void mousePressEvent(QMouseEvent*) override { if (onClose) onClose(); }
};
class FileSheet : public QWidget {
public:
  using QWidget::QWidget;
protected:
  void paintEvent(QPaintEvent*) override { QPainter p(this); mkit::paintBackdrop(p, rect(), 0); }
};
}


constexpr int kScanTtlMs = 30 * 60 * 1000;   // market::candles' TTL for daily candles

TechnicalHeatmapPage::TechnicalHeatmapPage(QWidget* parent) : QWidget(parent), m_scan(new Scanner(this)) {
  const auto& S = mkit::sem();
  m_title = new mkit::TitleStrip(QCoreApplication::translate("technical", "TECHNICAL SURVEILLANCE — MULTI-SIGNAL SCAN"), mkit::TitleStrip::Pulse, 37, this);
  m_badge = new mkit::Badge(m_title);
  m_badge->setText(QCoreApplication::translate("technical", "%1 INSTRUMENTS · DAILY CANDLES · LOADING").arg(market::instruments().size()));
  m_title->rightLayout()->addWidget(m_badge);

  const char* labels[] = {QT_TRANSLATE_NOOP("technical", "BULLISH TREND"), QT_TRANSLATE_NOOP("technical", "BEARISH TREND"), QT_TRANSLATE_NOOP("technical", "OVERBOUGHT"),
                          QT_TRANSLATE_NOOP("technical", "OVERSOLD"), QT_TRANSLATE_NOOP("technical", "NEAR LIQUIDITY"), QT_TRANSLATE_NOOP("technical", "MARKET LEAN")};
  const QColor accents[] = {theme::map("#5fc2cc"), theme::map("#d48c87"), theme::map("#d48c87"), theme::map("#5fc2cc"), theme::map("#ada1cc"), QColor()};
  for (int i = 0; i < 6; ++i) {
    auto* k = new mkit::KpiCard(QCoreApplication::translate("technical", labels[i]), this);
    k->setAccent(accents[i]);
    k->setValue(fmt::dash(), S.grey);
    m_kpi << k;
  }
  m_kpi[0]->setToolTip(QCoreApplication::translate("technical", "Instruments whose EMA20 is at least 0.10 % above their EMA50 (daily)"));
  m_kpi[1]->setToolTip(QCoreApplication::translate("technical", "Instruments whose EMA20 is at least 0.10 % below their EMA50 (daily)"));
  m_kpi[2]->setToolTip(QCoreApplication::translate("technical", "RSI(14) ≥ 70 on daily closes"));
  m_kpi[3]->setToolTip(QCoreApplication::translate("technical", "RSI(14) ≤ 30 on daily closes"));
  m_kpi[4]->setToolTip(QCoreApplication::translate("technical", "Nearest untested daily liquidity pool within 1 × ATR(14)"));
  m_kpi[5]->setToolTip(QCoreApplication::translate("technical", "Bullish trends / (bullish + bearish trends), over every loaded instrument"));
  m_kpi[5]->setCustom([this](QPainter& p, const QRectF& r) {
    const double y = r.top() + 8;
    const QRectF bar(r.left() + 1, y, r.width() - 1, 6);
    QPainterPath clip; clip.addRoundedRect(bar, 3, 3);
    p.save(); p.setClipPath(clip);
    p.fillRect(QRectF(bar.left(), bar.top(), bar.width() * m_bull, bar.height()), theme::map("#04dcf5"));
    p.fillRect(QRectF(bar.left() + bar.width() * m_bull, bar.top(), bar.width() * (1 - m_bull), bar.height()), theme::map("#fe817a"));
    p.restore();
    const QFont f = theme::mono(10, QFont::Normal, 1.0);
    p.setFont(f);
    const int bull = qRound(m_bull * 100);
    // compact cards: "41%" / "59%" (the card tooltip says bull / bear) when both words don't fit with a gap
    const bool narrow = mkit::textWidth(f, QCoreApplication::translate("technical", "%1% BULL").arg(bull)) +
                        mkit::textWidth(f, QCoreApplication::translate("technical", "%1% BEAR").arg(100 - bull)) + 16 > r.width();
    p.setPen(theme::map("#22d3ee")); p.drawText(QPointF(r.left() + 1, y + 23), narrow ? QString("%1%").arg(bull) : QCoreApplication::translate("technical", "%1% BULL").arg(bull));
    p.setPen(theme::map("#f87171"));
    const QString b = narrow ? QString("%1%").arg(100 - bull) : QCoreApplication::translate("technical", "%1% BEAR").arg(100 - bull);
    p.drawText(QPointF(r.right() - mkit::textWidth(f, b) + 1, y + 23), b);
  });

  m_cat = new mkit::ChipGroup(this);
  QList<mkit::ChipGroup::Chip> chips{{QCoreApplication::translate("technical", "ALL"), 0, {}}, {QCoreApplication::translate("technical", "FX"), 0, {}}, {QCoreApplication::translate("technical", "COMMODITIES"), 0, {}}, {QCoreApplication::translate("technical", "INDICES"), 0, {}},
                                     {QCoreApplication::translate("technical", "STOCKS"), 0, {}}, {QCoreApplication::translate("technical", "CRYPTO"), 0, {}}, {QCoreApplication::translate("technical", "YIELDS"), 0, {}}};
  chips[0].count = market::instruments().size();
  for (int i = 0; i < 6; ++i) chips[i + 1].count = market::byCategory(kCats[i]).size();
  m_cat->setChips(chips);
  m_trend = new mkit::ChipGroup(this);
  m_trend->setChips({{QCoreApplication::translate("technical", "ANY TREND"), -1, {}}, {QCoreApplication::translate("technical", "BULLISH"), -1, theme::map("#8cc3cc")}, {QCoreApplication::translate("technical", "BEARISH"), -1, theme::map("#e59a97")}});
  m_search = new mkit::SearchBox(QCoreApplication::translate("technical", "SEARCH INSTRUMENT…"), true, this);

  m_table = new HeatTable(this);
  m_fileScroll = new QScrollArea(this);   // the signal file scrolls on short windows
  m_fileScroll->setFrameShape(QFrame::NoFrame);
  m_fileScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_fileScroll->setWidgetResizable(false);
  m_fileScroll->viewport()->setAutoFillBackground(false);
  m_file = new SignalFile;
  m_fileScroll->setWidget(m_file);

  connect(m_cat, &mkit::ChipGroup::changed, this, [this] { rebuild(true); });
  connect(m_trend, &mkit::ChipGroup::changed, this, [this] { rebuild(true); });
  connect(m_search, &mkit::SearchBox::textChanged, this, [this] { rebuild(true); });
  connect(m_table, &HeatTable::rowClicked, this, [this](const QString& s) {
    m_userPick = true; select(s, true);
    if (m_phone) openSheet();   // phones: the file is not beside the table, it opens over the page
  });
  connect(m_table, &HeatTable::headerClicked, this, [this](int c) {
    m_table->setSort(c, c == m_table->sortColumn() ? !m_table->sortDesc() : c != 0);
    rebuild(false);
  });

  m_rebuildSoon.setSingleShot(true); m_rebuildSoon.setInterval(120);
  connect(&m_rebuildSoon, &QTimer::timeout, this, [this] { rebuild(false); updateKpis(); updateBadge(); });
  connect(m_scan, &Scanner::itemUpdated, this, [this] { if (!m_rebuildSoon.isActive()) m_rebuildSoon.start(); });
  connect(m_scan, &Scanner::progress, this, [this](int, int) { updateBadge(); });
  m_refresh.setInterval(kScanTtlMs);    // daily candles' cache TTL
  connect(&m_refresh, &QTimer::timeout, this, [this] { m_refresh.setInterval(kScanTtlMs); startScan(); });

  QStringList syms; for (const auto& i : market::instruments()) syms << i.symbol;
  market::QuoteHub::instance().subscribe(this, syms, false);
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, &TechnicalHeatmapPage::updatePrices);

  // test hooks
  if (qEnvironmentVariableIsSet("DW_TH_CATEGORY")) m_cat->setCurrent(qEnvironmentVariableIntValue("DW_TH_CATEGORY"));
  if (qEnvironmentVariableIsSet("DW_TH_TREND")) m_trend->setCurrent(qEnvironmentVariableIntValue("DW_TH_TREND"));
  if (qEnvironmentVariableIsSet("DW_TH_SEARCH")) m_search->setText(qEnvironmentVariable("DW_TH_SEARCH"));
  // DW_TH_CATEGORY_AT=<ms>:<chip>  switch category <ms> after the page is created (to capture the transition)
  if (const auto v = qEnvironmentVariable("DW_TH_CATEGORY_AT").split(':'); v.size() == 2)
    QTimer::singleShot(v[0].toInt(), this, [this, c = v[1].toInt()] { m_cat->setCurrent(c); rebuild(true); });
  // DW_TH_HOVER=x,y  hover a point of the page (page coordinates) once the data is in
  if (const auto v = qEnvironmentVariable("DW_TH_HOVER").split(','); v.size() == 2)
    QTimer::singleShot(qEnvironmentVariableIntValue("DW_TH_HOVER_AT") > 0 ? qEnvironmentVariableIntValue("DW_TH_HOVER_AT") : 5000, this,
                       [this, pt = QPoint(v[0].toInt(), v[1].toInt())] { mkit::simulateHover(this, pt); });
  // DW_TH_FILE=<ms>  phones: open the signal file sheet of the selected row <ms> after the page is created
  if (qEnvironmentVariableIntValue("DW_TH_FILE") > 0)
    QTimer::singleShot(qEnvironmentVariableIntValue("DW_TH_FILE"), this, [this] { if (m_phone) openSheet(); });
}

QString TechnicalHeatmapPage::priceText(int idx) const {
  const auto& it = m_scan->items()[idx];
  const auto q = market::QuoteHub::instance().quote(it.ins.symbol);
  const double v = q.valid() ? q.price : it.read.last;
  return fmt::num(v, it.ins.decimals);
}

void TechnicalHeatmapPage::rebuild(bool filterChanged) {
  const int cat = m_cat->current(), tr = m_trend->current();
  const QString q = m_search->text().trimmed();
  const auto& items = m_scan->items();
  QVector<int> idx;
  for (int i = 0; i < items.size(); ++i) {
    const auto& it = items[i];
    if (!it.loaded) continue;
    if (cat > 0 && it.ins.category != kCats[cat - 1]) continue;
    if (tr == 1 && it.read.trend != Lean::Bull) continue;
    if (tr == 2 && it.read.trend != Lean::Bear) continue;
    if (!q.isEmpty() && !displayName(it.ins).contains(q, Qt::CaseInsensitive) && !it.ins.symbol.contains(q, Qt::CaseInsensitive) &&
        !it.ins.name.contains(q, Qt::CaseInsensitive)) continue;
    idx << i;
  }
  const int sc = m_table->sortColumn(); const bool desc = m_table->sortDesc();
  std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
    if (sc == 0) { const int c = displayName(items[a].ins).compare(displayName(items[b].ins), Qt::CaseInsensitive); return desc ? c > 0 : c < 0; }
    const double ka = heat::sortKey(items[a].read, heat::Column(sc - 1)), kb = heat::sortKey(items[b].read, heat::Column(sc - 1));
    return desc ? ka > kb : ka < kb;
  });
  QVector<HeatTable::Row> rows;
  for (int i : idx) rows.push_back({items[i].ins.symbol, displayName(items[i].ins), priceText(i), items[i].read, {}});
  m_table->setRows(rows, filterChanged);
  if (rows.isEmpty()) { m_selected.clear(); m_table->setSelected({}); m_file->clear(); return; }
  bool present = false; for (const auto& r : rows) if (r.symbol == m_selected) present = true;
  if (filterChanged) m_userPick = false;
  // the first row stays selected (reference rule) until the user picks a row
  if (filterChanged || !present || (!m_userPick && rows.first().symbol != m_selected)) select(rows.first().symbol, true);
  else select(m_selected, false);
}

void TechnicalHeatmapPage::select(const QString& s, bool animate) {
  const auto& items = m_scan->items();
  for (int i = 0; i < items.size(); ++i) {
    if (items[i].ins.symbol != s) continue;
    m_selected = s;
    m_table->setSelected(s);
    m_file->setInstrument(shortId(items[i].ins), displayName(items[i].ins), priceText(i), items[i].read, animate);
    return;
  }
}

void TechnicalHeatmapPage::updateBadge() {
  const int n = market::instruments().size();
  const int total = m_scan->requested(), done = total - m_scan->pending();
  if (m_scan->running()) {
    m_badge->setText(QCoreApplication::translate("technical", "%1 / %2 · DAILY CANDLES · LOADING").arg(done).arg(total));
    m_badge->setProgress(double(done) / std::max(1, total));
    m_table->setLoadingText(QCoreApplication::translate("technical", "LOADING DAILY CANDLES… %1 / %2").arg(done).arg(total));
    if (m_phone) m_badge->resize(std::min(width() - 20, m_badge->sizeHint().width()), 18);   // no layout manages it there
    return;
  }
  int ok = 0; for (const auto& it : m_scan->items()) ok += it.loaded;
  m_badge->setText(QCoreApplication::translate("technical", "%1 INSTRUMENTS · DAILY CANDLES · COMPUTED").arg(ok));
  QStringList missing; for (const auto& it : m_scan->items()) if (!it.loaded) missing << it.ins.name;
  m_badge->setToolTip(ok < n ? QCoreApplication::translate("technical", "%1 of the %2 catalogue instruments have no usable daily history from the source: %3")
                                   .arg(n - ok).arg(n).arg(missing.join(", "))
                             : QCoreApplication::translate("technical", "All %1 catalogue instruments; reads computed by Deepwater from daily candles").arg(n));
  m_badge->setProgress(-1);
  m_table->setLoadingText({});
  if (m_phone) m_badge->resize(std::min(width() - 20, m_badge->sizeHint().width()), 18);
}

void TechnicalHeatmapPage::updateKpis() {
  int bull = 0, bear = 0, ob = 0, os = 0, near = 0;
  for (const auto& it : m_scan->items()) {
    if (!it.loaded) continue;
    bull += it.read.trend == Lean::Bull; bear += it.read.trend == Lean::Bear;
    ob += it.read.momentum == Momentum::Overbought; os += it.read.momentum == Momentum::Oversold;
    near += it.read.liq.state == LiqRead::NearPool;
  }
  m_kpi[0]->setValue(QString::number(bull), theme::map("#22d3ee"));
  m_kpi[1]->setValue(QString::number(bear), theme::map("#f87171"));
  m_kpi[2]->setValue(QString::number(ob), theme::map("#f87171"));
  m_kpi[3]->setValue(QString::number(os), theme::map("#22d3ee"));
  m_kpi[4]->setValue(QString::number(near), theme::map("#a78bfa"));
  const double b = bull + bear > 0 ? double(bull) / (bull + bear) : 0.5;
  if (b != m_bull) { m_bull = b; m_kpi[5]->update(); }
}

void TechnicalHeatmapPage::updatePrices(const QStringList& syms) {
  if (!isVisible()) return;
  const auto& items = m_scan->items();
  for (int i = 0; i < items.size(); ++i) {
    if (!items[i].loaded || !syms.contains(items[i].ins.symbol)) continue;
    const QString pr = priceText(i);
    m_table->setPrice(items[i].ins.symbol, pr);
    if (items[i].ins.symbol == m_selected) m_file->setPrice(pr);
  }
}

// Responsive layout. Reference (1600 wide): left column 10..W-385, signal file 346 px at W-360.
//  - side panel: 290 px below 1280, 346 (reference) up to 1919, 400 from 1920, 440 from 2400; it scrolls vertically
//    inside its own area when the page is shorter than its content;
//  - KPI cards: compact labels/values when a card is narrower than 175 px;
//  - filter row: the trend chips wrap to a second row, the search box goes to the title strip when there is no room;
//  - table: low-priority columns are dropped by HeatTable when it is too narrow.
void TechnicalHeatmapPage::resizeEvent(QResizeEvent*) {
  const int W = width(), H = height();
  m_title->setGeometry(0, 0, W, 37);
  // phones upright, and windows too short for the table (a phone held sideways): the page scrolls
  setScrollMode(ui::compact(W) || H < 420);
  const int sw = W < 1280 ? 290 : W < 1920 ? 346 : W < 2400 ? 400 : 440;
  if (m_pageScroll) {
    if (ui::compact(W)) m_pageScroll->setGeometry(0, 37, W, H - 37);
    else m_pageScroll->setGeometry(0, 37, W - sw - 28, H - 37);   // the signal file stays at the right, fixed
    m_pageScroll->relayout();
    if (!m_phone) {
      m_fileScroll->setGeometry(W - sw - 14, 37, sw + 6, H - 37);
      m_file->resize(sw, 100);
      m_file->resize(sw, std::max(H - 37, m_file->preferredHeight()));
      m_divX = W - sw - 28;
    } else {
      m_divX = -1;
      layoutSheet();
    }
    update();
    return;
  }
  const int leftW = W - sw - 49;
  const int tableY = layoutLeft(W, 0);
  m_table->setGeometry(10, tableY, leftW, H - tableY);
  m_fileScroll->setGeometry(W - sw - 14, 37, sw + 6, H - 37);
  m_file->resize(sw, 100);
  m_file->resize(sw, std::max(H - 37, m_file->preferredHeight()));
  m_divX = W - sw - 28;
  update();
}

// KPI cards, category / trend chips and search of the desktop columns, `dy` px lower than on the page (-37 inside the
// page scroller, whose content starts under the title strip). Returns the table's top.
int TechnicalHeatmapPage::layoutLeft(int W, int dy) {
  QWidget* host = m_content ? m_content : this;
  const int sw = W < 1280 ? 290 : W < 1920 ? 346 : W < 2400 ? 400 : 440;
  const int leftW = W - sw - 49;
  const int cardW = (leftW - 5 * 7) / 6;
  const bool compact = cardW < 175;
  for (int i = 0; i < 6; ++i) {
    const int x0 = 10 + i * (leftW + 7) / 6;
    const int x1 = i == 5 ? 10 + leftW : 10 + (i + 1) * (leftW + 7) / 6 - 7;
    m_kpi[i]->setLabelPx(compact ? 9 : 10, compact ? 1.2 : 2.6);
    if (i < 5) m_kpi[i]->setValuePx(compact ? 26 : 30, 500);
    m_kpi[i]->setGeometry(x0, 48 + dy, x1 - x0, 70);
  }
  const QSize cs = m_cat->sizeHint(), ts = m_trend->sizeHint();
  const int catH = m_cat->height();   // 27; more when the chips wrap (phone held sideways, see layoutScroll)
  m_cat->setGeometry(10, 135 + dy, cs.width(), catH);
  int rowY = 135 + catH - 27;
  if (10 + cs.width() + 7 + ts.width() <= 10 + leftW) m_trend->setGeometry(10 + cs.width() + 7, rowY + dy, ts.width(), 27);
  else { rowY += 33; m_trend->setGeometry(10, rowY + dy, ts.width(), 27); }
  const int searchRoom = 10 + leftW - (m_trend->geometry().right() + 12);
  if (searchRoom >= 150) {
    if (m_search->parentWidget() != host) { m_search->setParent(host); m_search->show(); }
    const int w = std::min(203, searchRoom);
    m_search->setGeometry(10 + leftW - w, rowY + 1 + dy, w, 25);
  } else {   // no room: the search box moves into the title strip
    if (m_search->parentWidget() != m_title) { m_search->setParent(m_title); m_search->show(); }
    m_search->setGeometry(m_title->titleRight() + 16, 6, std::clamp(W - m_badge->sizeHint().width() - 40 - (m_title->titleRight() + 16), 120, 220), 25);
  }
  return rowY + 40 + dy;
}

void TechnicalHeatmapPage::setScrollMode(bool on) {
  if (on == (m_pageScroll != nullptr)) return;
  QList<QWidget*> moved{m_cat, m_trend, m_table};
  for (auto* k : m_kpi) moved << k;
  if (on) {
    m_content = new QWidget;
    for (auto* w : moved) { w->setParent(m_content); w->show(); }
    if (m_search->parentWidget() == this) { m_search->setParent(m_content); m_search->show(); }
    m_table->setFullHeight(true);
    m_pageScroll = new global::PageScroll(m_content, this);
    m_pageScroll->layout = [this](int w, int vh) { return layoutScroll(w, vh); };
    // the table pins its header at the top of the part the page shows and paints only that part
    auto band = [this] {
      if (!m_pageScroll) return;
      m_table->setVisibleBand(m_pageScroll->verticalScrollBar()->value() - m_table->y(), m_pageScroll->viewport()->height());
    };
    connect(m_pageScroll->verticalScrollBar(), &QScrollBar::valueChanged, this, band);
    connect(m_pageScroll->verticalScrollBar(), &QScrollBar::rangeChanged, this, band);
    connect(m_table, &HeatTable::fullHeightChanged, m_pageScroll, [this] { if (m_pageScroll) m_pageScroll->relayout(); }, Qt::QueuedConnection);
    m_pageScroll->show();
    m_fileScroll->raise();
  } else {
    setPhone(false);
    m_pageScroll->takeWidget();
    for (auto* w : moved) { w->setParent(this); w->show(); }
    if (m_search->parentWidget() == m_content) { m_search->setParent(this); m_search->show(); }
    m_table->setFullHeight(false);
    m_cat->setWrap(0, 0);
    m_pageScroll->hide();
    m_pageScroll->deleteLater();
    m_pageScroll = nullptr;
    m_content->deleteLater();
    m_content = nullptr;
    m_fileScroll->raise();
  }
}

// Phones upright: the badge, which does not fit beside the title, leads the content; the signal file leaves the
// page for the sheet. Off: both go back where the desktop layout has them.
void TechnicalHeatmapPage::setPhone(bool on) {
  if (on == m_phone) return;
  m_phone = on;
  if (on) {
    m_title->rightLayout()->removeWidget(m_badge);
    m_badge->setParent(m_content); m_badge->show();
    if (!m_sheet) {
      m_sheet = new FileSheet(this);
      m_sheet->hide();
      auto* x = new SheetClose(m_sheet);
      x->setObjectName("sheetClose");
      x->onClose = [this] { m_sheet->hide(); };
    }
    m_fileScroll->setParent(m_sheet); m_fileScroll->show();
    m_sheet->findChild<QWidget*>("sheetClose")->raise();
    if (m_search->parentWidget() != m_content) { m_search->setParent(m_content); m_search->show(); }
  } else {
    m_badge->setParent(m_title);
    m_title->rightLayout()->addWidget(m_badge);
    m_badge->show();
    m_title->setTitleRoom(-1);
    m_cat->setWrap(0, 0); m_trend->setWrap(0, 0);
    m_fileScroll->setParent(this); m_fileScroll->show();
    if (m_sheet) m_sheet->hide();
  }
}

int TechnicalHeatmapPage::layoutScroll(int w, int vh) {
  setPhone(ui::compact(width()));   // the page's width: sideways, the scroller holds only the left column
  int y;
  if (!m_phone) {   // sideways: the desktop left column, the table as tall as its rows
    const int W = width();
    const int sw = W < 1280 ? 290 : W < 1920 ? 346 : W < 2400 ? 400 : 440;
    const int leftW = W - sw - 49;
    // category chips wider than the column: on rows (else the last ones are cut by the signal file)
    m_cat->setWrap(0, 0);
    if (m_cat->sizeHint().width() > leftW) m_cat->setWrap(leftW, 34);
    y = layoutLeft(W, -37);
    m_table->resize(leftW, std::max(1, m_table->height()));
    const int th = std::max(m_table->fullHeight(), vh - y);
    m_table->setGeometry(10, y, leftW, th);
    y += th;
  } else {
    const int inner = w - 20;
    // title: the badge has moved out of the strip, the title is elided to the width
    m_title->setTitleRoom(std::max(0, w - 50 - 14 - 13));
    m_badge->setGeometry(10, 9, std::min(inner, m_badge->sizeHint().width()), 18);
    y = 37;
    // KPI cards: 3 per row, two rows
    for (int i = 0; i < 6; ++i) {
      const int c = i % 3, r = i / 3;
      const int x0 = 10 + c * (inner + 7) / 3, x1 = c == 2 ? 10 + inner : 10 + (c + 1) * (inner + 7) / 3 - 7;
      m_kpi[i]->setLabelPx(9, 1.2);
      if (i < 5) m_kpi[i]->setValuePx(26, 500);
      m_kpi[i]->setGeometry(x0, y + r * 77, x1 - x0, 70);
    }
    y += 2 * 77 + 8;
    // chips on rows a finger can hit, the search box on its own row
    m_cat->setWrap(inner, 36);
    m_cat->setGeometry(10, y, inner, m_cat->height());
    y += m_cat->height() + 8;
    m_trend->setWrap(inner, 36);
    m_trend->setGeometry(10, y, inner, m_trend->height());
    y += m_trend->height() + 8;
    m_search->setGeometry(10, y, inner, 34);
    y += 34 + 14;
    m_table->resize(inner, std::max(1, m_table->height()));   // two-band rows at this width first
    const int th = std::max(m_table->fullHeight(), vh - y);
    m_table->setGeometry(10, y, inner, th);
    y += th;
  }
  m_table->setVisibleBand(m_pageScroll->verticalScrollBar()->value() - m_table->y(), vh);
  return y + 10;
}

void TechnicalHeatmapPage::layoutSheet() {
  if (!m_sheet) return;
  const int W = width(), H = height();
  m_sheet->setGeometry(0, 37, W, H - 37);
  m_fileScroll->setGeometry(14, 0, W - 20, H - 37);
  m_sheet->findChild<QWidget*>("sheetClose")->setGeometry(W - 52, 4, 44, 44);   // beside the file's id line
  m_file->resize(W - 28, 100);
  m_file->resize(W - 28, std::max(m_fileScroll->height(), m_file->preferredHeight()));
}

void TechnicalHeatmapPage::openSheet() {
  if (!m_sheet || m_file->isEmpty()) return;
  layoutSheet();
  m_fileScroll->verticalScrollBar()->setValue(0);
  m_sheet->raise(); m_sheet->show();
}

void TechnicalHeatmapPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  mkit::paintBackdrop(p, rect(), 37);
  p.fillRect(QRect(m_divX, 37, 1, height() - 37), theme::map("#181714"));
}

void TechnicalHeatmapPage::startScan() {
  if (m_scan->running()) return;
  m_lastScan.start();
  m_scan->start();
}

void TechnicalHeatmapPage::showEvent(QShowEvent*) {
  market::QuoteHub::instance().setActive(this, true);
  // A scan younger than the cache TTL would only read the same ~150 cached series again (synchronous reads, parses,
  // reads recomputed, table rebuilt): the page rescans when its data can have changed, and the refresh timer fires
  // when the TTL of the last scan runs out.
  const qint64 age = m_lastScan.isValid() ? m_lastScan.elapsed() : kScanTtlMs;
  if (age >= kScanTtlMs) startScan();
  m_refresh.start(int(age >= kScanTtlMs ? kScanTtlMs : kScanTtlMs - age));
}

// the ~150 daily series: a first visit finds the table built (showEvent skips a scan younger than the cache TTL)
void TechnicalHeatmapPage::preload() { if (!m_lastScan.isValid()) startScan(); }

void TechnicalHeatmapPage::hideEvent(QHideEvent*) {
  market::QuoteHub::instance().setActive(this, false);
  m_refresh.stop();
}
