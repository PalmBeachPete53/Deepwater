#include "pages/market/seasonality/SeasonalityPage.h"
#include "pages/market/seasonality/SeasonWidgets.h"
#include "pages/market/seasonality/SeasonStyle.h"
#include "pages/market/seasonality/SmoothScroll.h"
#include "app/Pages.h"
#include "core/Settings.h"
#include "market/Instruments.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <QCoreApplication>
#include <QLineEdit>
#include <algorithm>
#include <cmath>

DW_PAGE("market/seasonality", seasonality::SeasonalityPage)

using namespace seasonality;

namespace seasonality {
// Scrolling content: section labels painted here, cards placed at the reference's positions (1600 px wide page:
// trend card stretches, win-rate card fixed 430 px, ADVANCED cards split 50/50).
class Content : public QWidget {
public:
  explicit Content(QWidget* parent) : QWidget(parent) { setMinimumHeight(kHeight); }
  QWidget *trend = nullptr, *win = nullptr, *weekday = nullptr, *month = nullptr, *years = nullptr, *risk = nullptr;
  static constexpr int kHeight = 1235;
  int viewH = 896;   // visible height of the scroll viewport (set by the page)
  QSize sizeHint() const override { return QSize(1600, kHeight); }
  // Size classes (width of the page):
  //  < 1200   narrow: trend chart full width, win-rate card below it (pie left, figures right), ADVANCED 2 x 2
  //  < 2200   reference layout: trend stretches, win-rate 430 px, ADVANCED 2 x 2 (1600 = reference frames)
  //  ≥ 2200   wide: ADVANCED in one row of 4; the SUMMARY row grows to fill the window height
  void place() {
    const int w = width(), m = 10, gap = 11, advH = 315;
    int y = 51, sumBottom;
    if (w < 1200) {
      trend->setGeometry(m, y, w - 2 * m, 493);
      win->setGeometry(m, y + 493 + gap, w - 2 * m, 320);
      sumBottom = y + 493 + gap + 320;
    } else {
      const int winW = 430;
      int h = 493;
      if (w >= 2200) h = qMax(493, viewH - 51 - 24 - 18 - advH - 20);
      trend->setGeometry(m, y, w - 2 * m - gap - winW, h);
      win->setGeometry(w - m - winW, y, winW, h);
      sumBottom = y + h;
    }
    m_advY = sumBottom + 24;
    const int top = m_advY + 18;
    if (w >= 2200) {
      const int cw = (w - 2 * m - 3 * gap) / 4;
      QWidget* cards[] = {weekday, month, years, risk};
      for (int k = 0; k < 4; ++k) cards[k]->setGeometry(m + k * (cw + gap), top, k == 3 ? w - 2 * m - 3 * (cw + gap) : cw, advH);
      m_height = top + advH + 9;
    } else {
      const int half = (w - 2 * m - gap) / 2;
      weekday->setGeometry(m, top, half + 1, advH);
      month->setGeometry(m + half + 1 + gap, top, w - 2 * m - half - 1 - gap, advH);
      years->setGeometry(m, top + advH + 10, half + 1, advH);
      risk->setGeometry(m + half + 1 + gap, top + advH + 10, w - 2 * m - half - 1 - gap, advH);
      m_height = top + 2 * advH + 10 + 9;
    }
    if (minimumHeight() != m_height) setMinimumHeight(m_height);
    update();
  }
protected:
  void resizeEvent(QResizeEvent*) override { place(); }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), col().page);
    section(p, 34, QCoreApplication::translate("seasonal", "SUMMARY"));
    section(p, m_advY, QCoreApplication::translate("seasonal", "ADVANCED"));
  }
private:
  int m_advY = 568, m_height = kHeight;
  void section(QPainter& p, int y, const QString& text) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(col().gold); p.drawEllipse(QPointF(23, y), 2.2, 2.2);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QFont f = mono(11, QFont::Bold, 2);
    p.setFont(f); p.setPen(col().gold);
    p.drawText(QRectF(32, y - 8, 300, 16), Qt::AlignLeft | Qt::AlignVCenter, text);
    const int x = 32 + int(QFontMetricsF(f).horizontalAdvance(text)) + 8;
    p.fillRect(QRect(x, y, width() - 20 - x, 1), col().sectionRule);
  }
};

static QString categoryTag(market::Category c) {
  switch (c) {
    case market::Category::Forex: return "FX";
    case market::Category::Index: return QCoreApplication::translate("seasonal", "INDEX");
    case market::Category::Commodity: return QCoreApplication::translate("seasonal", "CMDTY");
    case market::Category::Stock: return QCoreApplication::translate("seasonal", "STOCK");
    case market::Category::Crypto: return "CRYPTO";
    case market::Category::Bond: return QCoreApplication::translate("seasonal", "BOND");
  }
  return {};
}
// Asset list order: FX majors as in the reference (EUR/USD, GBP/USD, USD/JPY…), then the rest of the catalogue.
static QList<market::Instrument> orderedInstruments() {
  static const QStringList majors{"EURUSD", "GBPUSD", "USDJPY", "USDCHF", "USDCAD", "AUDUSD", "NZDUSD", "EURGBP", "EURJPY"};
  QList<market::Instrument> out;
  for (const auto& s : majors) if (const auto* i = market::find(s)) out << *i;
  for (const auto& i : market::instruments()) if (!majors.contains(i.symbol)) out << i;
  return out;
}


SeasonalityPage::SeasonalityPage(QWidget* parent) : QWidget(parent) {
  setAutoFillBackground(false);
  m_symbol = settings::value("market.seasonality.asset", "EURUSD").toString();
  if (!market::find(m_symbol)) m_symbol = "EURUSD";
  m_nYears = settings::value("market.seasonality.years", 20).toInt();
  if (m_nYears != 10 && m_nYears != 15 && m_nYears != 20) m_nYears = 20;   // the reference offers 10, 15, 20 years
  m_outliers = settings::value("market.seasonality.outliers", false).toBool();

  m_tool = new ToolBar(this);
  m_tool->setActualLabel(QCoreApplication::translate("seasonal", "%1 actual").arg(QDate::currentDate().year()));
  connect(m_tool, &ToolBar::clicked, this, &SeasonalityPage::toolClicked);

  m_scroll = new SmoothScroll(this);
  m_content = new Content(m_scroll);
  m_trend = new TrendCard(m_content);
  m_win = new WinCard(m_content);
  m_weekday = new BarCard(QCoreApplication::translate("seasonal", "AVERAGE RETURN BY WEEKDAY (%)"), m_content);
  m_month = new BarCard(QCoreApplication::translate("seasonal", "AVERAGE RETURN BY MONTH (%)"), m_content);
  m_years = new BarCard(QCoreApplication::translate("seasonal", "INDIVIDUAL PERIOD RETURNS (%)"), m_content);
  m_years->setGap(10);
  m_risk = new RiskCard(m_content);
  m_content->trend = m_trend; m_content->win = m_win; m_content->weekday = m_weekday;
  m_content->month = m_month; m_content->years = m_years; m_content->risk = m_risk;
  m_scroll->setWidget(m_content);
  m_content->installEventFilter(this);
  for (QWidget* w : {static_cast<QWidget*>(m_trend), static_cast<QWidget*>(m_win), static_cast<QWidget*>(m_weekday),
                     static_cast<QWidget*>(m_month), static_cast<QWidget*>(m_years), static_cast<QWidget*>(m_risk)})
    w->installEventFilter(this);

  connect(m_trend, &TrendCard::periodDragged, this, [this](const Period& p) { setPeriod(p, true); });
  connect(m_trend, &TrendCard::cleared, this, [this] { setPeriod(Period::preset(Period::Full), false); });

  // dropdowns (children of the page so they float above the scrolling content and stay open while it scrolls)
  m_assetMenu = new Menu(this, true);
  m_yearsMenu = new Menu(this);
  m_yearsMenu->setRowFont(mono(13));
  m_yearsMenu->setStrongSelection(true);
  m_filterMenu = new Menu(this);
  m_filterMenu->setRowFont(ui(12));
  m_filterMenu->setNote(QCoreApplication::translate("seasonal", "Affects “Average return by month” only – recomputed from real per-year returns."));
  m_compareMenu = new Menu(this, true);
  connect(m_assetMenu, &Menu::picked, this, [this](const QString& k) {
    closeMenu();
    if (k.isEmpty() || k == m_symbol) return;
    m_symbol = k; settings::setValue("market.seasonality.asset", k);
    load();
  });
  connect(m_yearsMenu, &Menu::picked, this, [this](const QString& k) {
    closeMenu();
    if (k.isEmpty() || k.toInt() == m_nYears) return;
    m_nYears = k.toInt(); settings::setValue("market.seasonality.years", m_nYears);
    rebuild();
  });
  connect(m_filterMenu, &Menu::picked, this, [this](const QString& k) {
    closeMenu();
    if (k.isEmpty()) return;
    m_outliers = k == "outliers"; settings::setValue("market.seasonality.outliers", m_outliers);
    recompute();
  });
  connect(m_compareMenu, &Menu::picked, this, [this](const QString& k) {
    closeMenu();
    if (k.isEmpty()) return;
    m_compareSymbol = k == "none" ? QString() : k;
    m_compareCloses.clear();
    loadCompare();
  });

  m_period = Period::preset(Period::Full);
  m_tool->setSegment(0, false);
  load();
}

void SeasonalityPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  p.fillRect(rect(), c.page);
  p.fillRect(QRect(0, 0, width(), 31), c.strip);
  p.fillRect(QRect(0, 31, width(), 1), c.stripRule);
  const QFont t = ui(14, QFont::Bold);
  p.setFont(t); p.setPen(c.title);
  const QString lab = QCoreApplication::translate("seasonal", "Seasonality Lab");
  p.drawText(QRectF(10, 0, 300, 31), Qt::AlignLeft | Qt::AlignVCenter, lab);
  const int x = 10 + QFontMetrics(t).horizontalAdvance(lab) + 7;
  QFont s = ui(10, QFont::Medium); s.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
  p.setFont(s); p.setPen(c.dim);
  p.drawText(QRectF(x, 1, 600, 31), Qt::AlignLeft | Qt::AlignVCenter,
             QCoreApplication::translate("seasonal", "DAILY HISTORY · MULTI-YEAR SEASONAL STATISTICS"));
}

void SeasonalityPage::resizeEvent(QResizeEvent*) {
  m_tool->setGeometry(0, 31, width(), 39);
  m_scroll->setGeometry(0, 70, width(), height() - 70);
  m_content->viewH = height() - 70;
  m_content->place();
}

void SeasonalityPage::showEvent(QShowEvent*) {
  if (!m_hooksDone) { m_hooksDone = true; QTimer::singleShot(0, this, [this] { applyHooks(); }); }
}

bool SeasonalityPage::eventFilter(QObject* o, QEvent* e) {
  // a click in the content closes an open dropdown (clicks on the toolbar keep it, as in the reference)
  if (e->type() == QEvent::MouseButtonPress && m_openItem >= 0 && o != m_assetMenu && o != m_yearsMenu &&
      o != m_filterMenu && o != m_compareMenu) closeMenu();
  return QWidget::eventFilter(o, e);
}

// ---------------------------------------------------------------- data
void SeasonalityPage::load() {
  const int seq = ++m_loadSeq;
  m_closes.clear(); m_ds = Dataset(); m_raw.clear(); m_smooth.clear();
  const QString disp = market::display(m_symbol);
  m_tool->setTexts(disp, QCoreApplication::translate("seasonal", "%1 years").arg(m_nYears), m_compareSymbol.isEmpty() ? QString() : market::display(m_compareSymbol));
  m_trend->setTitle(QCoreApplication::translate("seasonal", "SEASONAL TREND OF %1 OVER %2 YEARS").arg(disp.toUpper()).arg(m_nYears), QString());
  const QString loading = QCoreApplication::translate("seasonal", "Loading daily history of %1…").arg(disp);
  m_trend->setStatus(loading);
  for (auto* b : {m_weekday, m_month, m_years}) b->setStatus(QCoreApplication::translate("seasonal", "Loading…"));
  m_win->setStatus(QCoreApplication::translate("seasonal", "Loading…"));
  m_risk->setStatus(QCoreApplication::translate("seasonal", "Loading…"));
  loadHistory(m_symbol, 20, this, [this, seq, disp](const QVector<DailyClose>& c, const QString& err, bool) {
    if (seq != m_loadSeq) return;
    if (!err.isEmpty() || c.size() < 30) {
      const QString msg = QCoreApplication::translate("seasonal", "No daily history for %1 (Yahoo Finance: %2)").arg(disp, err.isEmpty() ? QCoreApplication::translate("seasonal", "too short") : err);
      m_trend->setStatus(msg);
      for (auto* b : {m_weekday, m_month, m_years}) b->setStatus(QString::fromUtf8("—"));
      m_win->setStatus(QString::fromUtf8("—")); m_risk->setStatus(QString::fromUtf8("—"));
      return;
    }
    m_closes = c;
    rebuild();
  });
  if (!m_compareSymbol.isEmpty()) loadCompare();
}

void SeasonalityPage::loadCompare() {
  const int seq = ++m_compareSeq;
  m_tool->setTexts(market::display(m_symbol), QCoreApplication::translate("seasonal", "%1 years").arg(m_nYears),
                   m_compareSymbol.isEmpty() ? QString() : market::display(m_compareSymbol));
  if (m_compareSymbol.isEmpty()) { rebuild(); return; }
  m_compareCloses.clear();   // never draw the previous compare symbol's path under the new name
  loadHistory(m_compareSymbol, 20, this, [this, seq](const QVector<DailyClose>& c, const QString& err, bool) {
    if (seq != m_compareSeq) return;
    m_compareCloses = err.isEmpty() ? c : QVector<DailyClose>{};
    m_compareError = err.isEmpty() && c.size() >= 30 ? QString()
                     : QCoreApplication::translate("seasonal", "%1: no daily history (%2)")
                           .arg(market::display(m_compareSymbol), err.isEmpty() ? QCoreApplication::translate("seasonal", "too short") : err);
    rebuild();
  });
}

void SeasonalityPage::rebuild() {
  const QString disp = market::display(m_symbol);
  m_tool->setTexts(disp, QCoreApplication::translate("seasonal", "%1 years").arg(m_nYears), m_compareSymbol.isEmpty() ? QString() : market::display(m_compareSymbol));
  if (m_closes.isEmpty()) return;
  const QDate today = QDate::currentDate();
  // without a dataset recompute() never runs: the five cards below the trend would stay on "Loading…"
  auto notEnough = [this] {
    m_trend->setStatus(QCoreApplication::translate("seasonal", "Not enough daily history for a seasonal average"));
    for (auto* b : {m_weekday, m_month, m_years}) b->setStatus(QString::fromUtf8("—"));
    m_win->setStatus(QString::fromUtf8("—")); m_risk->setStatus(QString::fromUtf8("—"));
  };
  m_ds = build(m_closes, m_nYears, today, m_symbol);
  if (!m_ds.ok()) { notEnough(); return; }
  m_raw = averagePath(m_ds);
  if (std::none_of(m_raw.begin(), m_raw.end(), [](double v) { return std::isfinite(v); })) {   // no finished year yet
    notEnough(); return;
  }
  m_smooth = smoothPath(m_raw);
  const int n = m_ds.years.size();
  m_trend->setTitle(QCoreApplication::translate("seasonal", "SEASONAL TREND OF %1 OVER %2 YEARS").arg(disp.toUpper()).arg(n),
                    QString("(%1 - %2)").arg(dateLabel(m_ds.first), dateLabel(m_ds.last)));
  m_trend->setPaths(m_smooth, m_raw, m_ds.last.year() == today.year() ? slotOf(today) : -1);
  QVector<double> cmp, act;
  if (!m_compareSymbol.isEmpty() && !m_compareCloses.isEmpty()) {
    const Dataset cds = build(m_compareCloses, m_nYears, today, m_compareSymbol);
    if (cds.ok()) cmp = smoothPath(averagePath(cds));
  }
  if (m_actual) act = yearPath(m_ds, today.year());
  // a failed compare was silent (no line, no word): say it in the card's subtitle
  if (!m_compareSymbol.isEmpty() && cmp.isEmpty() && !m_compareError.isEmpty())
    m_trend->setTitle(QCoreApplication::translate("seasonal", "SEASONAL TREND OF %1 OVER %2 YEARS").arg(disp.toUpper()).arg(n),
                      QString("(%1 - %2)  ·  %3").arg(dateLabel(m_ds.first), dateLabel(m_ds.last), m_compareError));
  m_trend->setOverlays(cmp, market::display(m_compareSymbol), col().compare, act, QCoreApplication::translate("seasonal", "%1 actual").arg(today.year()));
  m_trend->setIndexMode(m_index);
  recompute();
}

void SeasonalityPage::recompute() {
  if (!m_ds.ok()) return;
  const Stats st = compute(m_ds, m_raw, m_period, m_outliers);
  m_trend->setPeriod(m_period);
  m_win->setStats(st, m_nYears);
  const QString pill = m_period.isFull() ? QString() : m_period.pill();
  // weekdays
  {
    QVector<double> v; QStringList tips;
    QStringList names;
    for (int d = 1; d <= 5; ++d) names << weekdayAxis(d);
    for (int i = 0; i < 5; ++i) {
      v << st.weekday[i];
      tips << QCoreApplication::translate("seasonal", "%1  %2  (%3 days)").arg(names[i], signedPct(st.weekday[i], 3)).arg(st.weekdayN[i]);
    }
    m_weekday->setBars(names, v, {}, tips);
    m_weekday->setPill(pill);
  }
  // months
  {
    QVector<double> v; QStringList labels, tips;
    for (const auto& ms : st.months) {
      v << ms.mean; labels << monthAxis(ms.month);
      tips << QCoreApplication::translate("seasonal", "%1  %2  (%3 years)").arg(monthAxis(ms.month), signedPct(ms.mean, 2)).arg(ms.n);
    }
    m_month->setBars(labels, v, {}, tips);
    m_month->setPill(pill);
    m_month->setNote(m_outliers ? QCoreApplication::translate("seasonal", "OUTLIER YEARS EXCLUDED (>1.5σ)") : QString());
  }
  // per year
  {
    QVector<double> v; QVector<bool> dim; QStringList labels, tips;
    for (const auto& y : st.perYear) {
      v << y.ret; dim << !y.counted;
      labels << QString::number(y.year % 100).rightJustified(2, '0');
      tips << QString("%1  %2%3").arg(y.year).arg(signedPct(y.ret, 2), y.counted ? QString() : QCoreApplication::translate("seasonal", "  (to date, not counted)"));
    }
    m_years->setBars(labels, v, dim, tips);
    m_years->setPill(pill);
  }
  m_risk->setStats(st);
}

void SeasonalityPage::setPeriod(const Period& p, bool clicked) {
  m_period = p;
  static const Period::Kind kinds[] = {Period::Full, Period::H1, Period::H2, Period::Q1, Period::Q2, Period::Q3, Period::Q4};
  int seg = -1;
  for (int i = 0; i < 7; ++i) if (Period::preset(kinds[i]) == p) seg = i;
  m_tool->setSegment(seg, clicked && seg != 0);
  recompute();
  m_trend->setPeriod(p);
}

// ---------------------------------------------------------------- toolbar
void SeasonalityPage::toolClicked(int item) {
  switch (item) {
    case ToolBar::Asset: case ToolBar::Years: case ToolBar::Filter: case ToolBar::Compare:
      if (m_openItem == item) closeMenu(); else openMenu(item);
      return;
    case ToolBar::Prev: stepAsset(-1); return;
    case ToolBar::Next: stepAsset(1); return;
    case ToolBar::Actual: m_actual = !m_actual; m_tool->setToggles(m_actual, m_index); rebuild(); return;
    case ToolBar::Index: m_index = !m_index; m_tool->setToggles(m_actual, m_index); m_trend->setIndexMode(m_index); return;
    case ToolBar::Refresh: load(); return;
    default: break;
  }
  if (item >= ToolBar::Seg && item < ToolBar::Seg + 7) {
    static const Period::Kind kinds[] = {Period::Full, Period::H1, Period::H2, Period::Q1, Period::Q2, Period::Q3, Period::Q4};
    setPeriod(Period::preset(kinds[item - ToolBar::Seg]), true);
  }
}

void SeasonalityPage::openMenu(int item) {
  closeMenu();
  const QRect r = m_tool->itemRect(item).translated(m_tool->pos());
  if (item == ToolBar::Asset || item == ToolBar::Compare) {
    QList<Menu::Row> rows; int sel = -1;
    const QString cur = item == ToolBar::Asset ? m_symbol : m_compareSymbol;
    if (item == ToolBar::Compare) { rows << Menu::Row{QCoreApplication::translate("seasonal", "None"), "", "none", ""}; if (cur.isEmpty()) sel = 0; }
    for (const auto& in : orderedInstruments()) {
      if (in.symbol == cur) sel = rows.size();
      rows << Menu::Row{market::display(in.symbol), categoryTag(in.category), in.symbol, in.name + " " + in.yahoo};
    }
    Menu* m = item == ToolBar::Asset ? m_assetMenu : m_compareMenu;
    m->setRows(rows, sel);
    m->popup(QPoint(r.left(), 70), 202);
  } else if (item == ToolBar::Years) {
    QList<Menu::Row> rows; int sel = 0;
    for (int y : {10, 15, 20}) { if (y == m_nYears) sel = rows.size(); rows << Menu::Row{QCoreApplication::translate("seasonal", "%1 years").arg(y), "", QString::number(y), ""}; }
    m_yearsMenu->setRows(rows, sel);
    m_yearsMenu->popup(QPoint(r.left() + 1, 70), 202);
  } else if (item == ToolBar::Filter) {
    m_filterMenu->setRows({{QCoreApplication::translate("seasonal", "All years"), "", "all", ""}, {QCoreApplication::translate("seasonal", "Exclude outlier years (>1.5σ)"), "", "outliers", ""}},
                          m_outliers ? 1 : 0);
    {   // as wide as its longest row (French rows are longer than the reference's 202 px)
      const QFontMetrics fm(ui(12));
      const int w = qMax(202, fm.horizontalAdvance(QCoreApplication::translate("seasonal", "Exclude outlier years (>1.5σ)")) + 64);   // row text has 8 + 40 px of padding
      m_filterMenu->popup(QPoint(r.left() + 1, 70), qMin(w, width() - r.left() - 12));
    }
  }
  m_openItem = item;
  m_tool->setOpen(item);
}

void SeasonalityPage::closeMenu() {
  for (auto* m : {m_assetMenu, m_yearsMenu, m_filterMenu, m_compareMenu}) m->hide();
  m_openItem = -1;
  m_tool->setOpen(-1);
}

void SeasonalityPage::stepAsset(int d) {
  const auto list = orderedInstruments();
  int i = 0;
  for (int k = 0; k < list.size(); ++k) if (list[k].symbol == m_symbol) i = k;
  m_symbol = list[(i + d + list.size()) % list.size()].symbol;
  settings::setValue("market.seasonality.asset", m_symbol);
  load();
}

// ---------------------------------------------------------------- test hooks
void SeasonalityPage::applyHooks() {
  const QString asset = qEnvironmentVariable("DW_SEASON_ASSET");
  if (!asset.isEmpty() && market::find(asset) && asset != m_symbol) { m_symbol = market::find(asset)->symbol; load(); }
  const QString cmp = qEnvironmentVariable("DW_SEASON_COMPARE");
  if (!cmp.isEmpty() && market::find(cmp)) { m_compareSymbol = market::find(cmp)->symbol; loadCompare(); }
  const QString tg = qEnvironmentVariable("DW_SEASON_TOGGLES");
  if (tg.contains("actual")) toolClicked(ToolBar::Actual);
  if (tg.contains("index")) toolClicked(ToolBar::Index);
  const QString per = qEnvironmentVariable("DW_SEASON_PERIOD");
  if (!per.isEmpty()) {
    const QStringList k{"FULL", "H1", "H2", "Q1", "Q2", "Q3", "Q4"};
    const int i = k.indexOf(per.toUpper());
    if (i >= 0) toolClicked(ToolBar::Seg + i);
    else if (per.contains('-')) setPeriod(Period::custom(per.section('-', 0, 0).toInt(), per.section('-', 1, 1).toInt()), true);
  }
  const QString menu = qEnvironmentVariable("DW_SEASON_MENU");
  if (menu == "asset") openMenu(ToolBar::Asset);
  else if (menu == "years") openMenu(ToolBar::Years);
  else if (menu == "filter") openMenu(ToolBar::Filter);
  else if (menu == "compare") openMenu(ToolBar::Compare);
  const QString scroll = qEnvironmentVariable("DW_SEASON_SCROLL");
  if (!scroll.isEmpty()) QTimer::singleShot(300, this, [this, scroll] { m_scroll->scrollTo(scroll.toInt(), false); });
  const QString hover = qEnvironmentVariable("DW_SEASON_HOVER");
  if (!hover.isEmpty()) {
    const QPoint pg(hover.section(',', 0, 0).toInt(), hover.section(',', 1, 1).toInt());
    QTimer::singleShot(400, this, [this, pg] {
      if (m_tool->geometry().contains(pg)) {   // toolbar hover (gold ring on a segment)
        const QPointF lp = m_tool->mapFrom(this, pg);
        QMouseEvent me(QEvent::MouseMove, lp, m_tool->mapToGlobal(lp), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(m_tool, &me);
      } else m_trend->setHover(m_trend->mapFrom(this, pg));
    });
  }
}
}  // namespace seasonality
