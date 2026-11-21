#include "pages/economy/AssetFile.h"
#include "pages/economy/EcoKit.h"
#include "macro/Registry.h"
#include "macro/CentralBanks.h"
#include "macro/YieldCurves.h"
#include "ui/Theme.h"
#include "ui/Chart.h"
#include "ui/Format.h"
#include "ui/Touch.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QShortcut>
#include <QToolTip>
#include <QLocale>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QRegularExpression>
#include <QKeyEvent>
#include <cmath>

namespace economy {

using namespace macro;
namespace {
QColor C(const char* hex) { return theme::map(QColor(hex)); }   // Deepwater colour → current theme
// "3 Mo" / "10 Yr" / "Long" (curve labels from the services) in the interface language
QString tenor(QString l) {
  l.replace(" Mo", QCoreApplication::translate("economy", " Mo"));
  l.replace(" Yr", QCoreApplication::translate("economy", " Yr"));
  if (l == "Long") l = QCoreApplication::translate("economy", "Long");
  return l;
}

int yearsOfRange(int idx, const QList<int>& yrs) { return idx < yrs.size() ? yrs[idx] : 999; }

QVector<Point> lastYears(const QVector<Point>& pts, int years) {
  if (pts.isEmpty() || years >= 999) return pts;
  const QDate from = pts.last().date.addYears(-years);
  QVector<Point> out;
  for (const auto& p : pts) if (p.date > from) out << p;
  return out;
}

double niceStep(double span, int target) {
  const double raw = span / std::max(1, target);
  const double mag = std::pow(10, std::floor(std::log10(std::max(raw, 1e-9))));
  for (double m : {1.0, 2.0, 2.5, 5.0, 10.0}) if (m * mag >= raw) return m * mag;
  return 10 * mag;
}

QString axisPct(double v, double step) {
  int dec = step >= 1 ? 0 : step >= 0.1 ? (std::fabs(step * 2 - std::round(step * 2)) < 1e-9 && step >= 0.5 ? 1 : 2) : 2;
  QString s = QString::number(v, 'f', dec);
  if (s.contains('.')) { while (s.endsWith('0')) s.chop(1); if (s.endsWith('.')) s.chop(1); }
  return s;
}

QString periodShort(const Point& p, char freq) {
  if (!p.date.isValid()) return "—";
  switch (freq) {
    case 'A': return QString::number(p.date.year());
    case 'Q': return QString("%1-Q%2").arg(p.date.year()).arg((p.date.month() - 1) / 3 + 1);
    case 'D': return p.date.toString("yyyy-MM-dd");
    default: return p.date.toString("yyyy-MM");
  }
}

// direction of the recent trend (last value vs mean of the 3 before)
int trendDir(const QVector<Point>& pts) {
  if (pts.size() < 3) return 0;
  const int n = pts.size();
  double m = 0; int k = 0;
  for (int i = std::max(0, n - 4); i < n - 1; ++i) { m += pts[i].value; ++k; }
  m /= k;
  const double d = pts.last().value - m;
  return std::fabs(d) < 1e-9 ? 0 : d > 0 ? 1 : -1;
}
}

AssetFile::AssetFile(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  m_compareEdit = new QLineEdit(this);
  m_compareEdit->setPlaceholderText(QCoreApplication::translate("economy", "Compare with…"));
  m_compareEdit->setFrame(false);
  m_compareEdit->setFont(eco::mono(12));
  m_compareEdit->setStyleSheet(QString("QLineEdit { background: transparent; color: %1; border: none; padding: 0; }")
                                 .arg(eco::txt().name()));
  m_compareEdit->setClearButtonEnabled(true);
  QStringList names;
  for (const auto& e : economies()) names << displayName(e);
  m_completer = new QCompleter(names, this);
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_completer->setFilterMode(Qt::MatchContains);
  m_compareEdit->setCompleter(m_completer);
  auto pick = [this](const QString& name) {
    m_compareIso.clear(); m_compare = {};
    for (const auto& e : economies()) if (displayName(e).compare(name, Qt::CaseInsensitive) == 0) m_compareIso = e.iso2;
    if (!m_compareIso.isEmpty() && m_compareIso != m_iso) {
      m_compare = MacroStore::instance().series(m_compareIso, m_series.ind);
    }
    update();
  };
  connect(m_completer, qOverload<const QString&>(&QCompleter::activated), this, pick);
  connect(m_compareEdit, &QLineEdit::returnPressed, this, [this, pick] { pick(m_compareEdit->text()); });
  connect(m_compareEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
    if (t.isEmpty() && !m_compareIso.isEmpty()) { m_compareIso.clear(); m_compare = {}; update(); }
  });
  m_compareEdit->hide();
  auto* sc = new QShortcut(QKeySequence("Ctrl+K"), this);
  connect(sc, &QShortcut::activated, this, [this] { if (m_detail) { m_compareEdit->setFocus(); m_compareEdit->selectAll(); } });

  connect(&MacroStore::instance(), &MacroStore::updated, this, [this] {
    if (m_detail) {
      const Row r{m_ind, m_ind, {}};
      bool fb = false; Q_UNUSED(fb);
      for (const auto& row : rows()) if (row.ind == m_ind) m_series = rowSeries(row);
      if (!m_series.ok()) m_series = MacroStore::instance().series(m_iso, m_ind);
      if (!m_compareIso.isEmpty()) m_compare = MacroStore::instance().series(m_compareIso, m_series.ind);
      relayout();
    }
    update();
  });
  connect(&YieldCurves::instance(), &YieldCurves::updated, this, [this](const QString& iso) { if (iso == m_iso) update(); });
  connect(&eco::Blink::instance(), &eco::Blink::tick, this, [this] { update(QRect(0, 0, int(m_x + 200), 50)); });
}

QStringList AssetFile::tabLabels() const {
  return {QCoreApplication::translate("economy", "MAIN INDICATOR"), QCoreApplication::translate("economy", "PRICES"),
          QCoreApplication::translate("economy", "GROWTH"), QCoreApplication::translate("economy", "LABOUR")};
}

QList<AssetFile::Row> AssetFile::rows() const {
  using I = Ind;
  switch (m_tab) {
    case 1: return {{I::Inflation, I::InflationAnnual, QCoreApplication::translate("economy", "Inflation Rate (Annual)")}, {I::CoreInflation, I::CoreInflation, QCoreApplication::translate("economy", "Core Inflation Rate (Annual)")},
                    {I::InflationAnnual, I::InflationAnnual, QCoreApplication::translate("economy", "Inflation (Annual Average)")},
                    {I::Rate3m, I::Rate3m, QCoreApplication::translate("economy", "3M Interbank Rate")}, {I::Yield10y, I::Yield10y, QCoreApplication::translate("economy", "10Y Government Yield")}};
    case 2: return {{I::GdpQoq, I::GdpQoq, QCoreApplication::translate("economy", "GDP (Quarter q/q)")}, {I::GdpYoy, I::GdpYoy, QCoreApplication::translate("economy", "GDP (Annual y/y)")},
                    {I::RealGrowth, I::RealGrowth, QCoreApplication::translate("economy", "Real GDP Growth (Annual)")}, {I::RetailSalesYoy, I::RetailSalesYoy, QCoreApplication::translate("economy", "Retail Sales (Annual y/y)")},
                    {I::BusinessConf, I::BusinessConf, QCoreApplication::translate("economy", "Business Confidence (OECD)")},
                    {I::ConsumerConf, I::ConsumerConf, QCoreApplication::translate("economy", "Consumer Confidence (OECD)")}};
    case 3: return {{I::Unemployment, I::UnemploymentAnnual, QCoreApplication::translate("economy", "Unemployment Rate")}, {I::UnemploymentAnnual, I::UnemploymentAnnual, QCoreApplication::translate("economy", "Unemployment (ILO, Annual)")},
                    {I::Population, I::Population, QCoreApplication::translate("economy", "Population")}};
    default: return {{I::PolicyRate, I::PolicyRate, QCoreApplication::translate("economy", "Interest Rate")}, {I::GdpQoq, I::RealGrowth, QCoreApplication::translate("economy", "GDP (Quarter q/q)")},
                     {I::Inflation, I::InflationAnnual, QCoreApplication::translate("economy", "Inflation Rate (Annual)")}, {I::Unemployment, I::UnemploymentAnnual, QCoreApplication::translate("economy", "Unemployment Rate")},
                     {I::BusinessConf, I::BusinessConf, QCoreApplication::translate("economy", "Business Confidence (OECD)")}};
  }
}

Series AssetFile::rowSeries(const Row& r, bool* fellBack) const {
  auto& ms = MacroStore::instance();
  Series s = ms.series(m_iso, r.ind);
  if (fellBack) *fellBack = false;
  // a first-line series older than two years gives way to the fallback
  const bool stale = s.ok() && s.last().date < QDate::currentDate().addYears(-2);
  if ((!s.ok() || stale) && r.fallback != r.ind) {
    Series f = ms.series(m_iso, r.fallback);
    if (f.ok() && (!s.ok() || f.last().date > s.last().date)) { if (fellBack) *fellBack = true; return f; }
  }
  return s;
}

void AssetFile::setEconomy(const QString& iso2) {
  m_iso = iso2.toLower();
  m_detail = false; m_compareIso.clear(); m_compare = {}; m_compareEdit->clear();
  YieldCurves::instance().request(m_iso);
  relayout(); update();
}
void AssetFile::showDashboard() { m_detail = false; m_compareEdit->hide(); relayout(); update(); }
void AssetFile::showDetail(Ind ind) {
  m_detail = true; m_ind = ind;
  m_series = {};
  for (const auto& r : rows()) if (r.ind == ind) m_series = rowSeries(r);
  if (!m_series.ok()) m_series = MacroStore::instance().series(m_iso, ind);
  m_win0 = 0; m_win1 = -1;
  if (!m_compareIso.isEmpty()) m_compare = MacroStore::instance().series(m_compareIso, m_series.ind);
  relayout(); update();
}
void AssetFile::setHoverPoint(const QPointF& p) { m_mouse = p; m_inside = true; update(); }

void AssetFile::resizeEvent(QResizeEvent*) { relayout(); }

void AssetFile::relayout() {
  const qreal w = width();
  // phones: the column takes the whole width (12 px margins) and the blocks stack (see the m_phone branches)
  m_phone = ui::compact(int(w));
  m_w = m_phone ? w - 24 : w <= 1600 ? std::min(1318.0, w - 64) : std::min(w * 0.824, 2400.0);   // capped on ultrawide: tables and prose stay readable
  m_x = std::floor((w - m_w) / 2);
  m_close = m_phone ? QRectF(w - 44, 10, 34, 34) : QRectF(w - 37, 17, 20, 20);
  const qreal s = m_w / 1318.0;
  Q_UNUSED(s);
  int h = 0;
  if (!m_detail) {
    // phones: policy rate on top, the sovereign curve under it (see paintSummary)
    m_summary = QRectF(m_x, 166, m_w, m_phone ? 396 : 231);
    m_tabRects.clear();
    const QFontMetricsF tfm(eco::mono(12, QFont::Bold, 1.2));
    if (m_phone) {
      // label on its own line, tabs under it, wrapping, 30 px tall
      m_dashY = m_summary.bottom() + 30;
      qreal tx = m_x, ty = m_dashY + 12;
      for (const QString& lab : tabLabels()) {
        const qreal tw = std::ceil(tfm.horizontalAdvance(lab)) + 18;
        if (tx > m_x && tx + tw > m_x + m_w) { tx = m_x; ty += 36; }
        m_tabRects << QRectF(tx, ty, tw, 30); tx += tw + 6;
      }
    } else {
      m_dashY = 427;
      // tabs start after the "MACRO DASHBOARD" label (136 px in English)
      qreal tx = m_x + std::max(136.0, std::ceil(QFontMetricsF(eco::mono(10, QFont::Normal, 2.2)).horizontalAdvance(
                                        QCoreApplication::translate("economy", "MACRO DASHBOARD"))) + 14);
      for (const QString& lab : tabLabels()) {
        const qreal tw = std::ceil(tfm.horizontalAdvance(lab)) + 18;   // 137 / 66 / 70 / 72 px in English
        m_tabRects << QRectF(tx, 411, tw, 24); tx += tw + 6;
      }
    }
    const int n = rows().size();
    const qreal rh = m_phone ? 56 : 36.5;   // phones: two lines per row (name + latest, then trend · prior · period)
    m_table = QRectF(m_x, m_phone ? m_tabRects.last().bottom() + 12 : 447, m_w, 26 + rh * n);
    m_rowRects.clear();
    for (int i = 0; i < n; ++i) m_rowRects << QRectF(m_x, m_table.top() + 26 + rh * i, m_w, rh);
    if (m_phone) {
      const QFontMetricsF ff(eco::mono(9, QFont::Normal, 0.6));
      m_fnH = std::ceil(ff.boundingRect(QRectF(0, 0, m_w, 1000), Qt::TextWordWrap, QCoreApplication::translate("economy",
                "LATEST REAL RECORDED VALUE PER INDICATOR FROM OFFICIAL AGENCIES (BIS · OECD · WORLD BANK · CENTRAL BANKS; "
                "ANNUAL WHERE TAGGED). PERIOD = REFERENCE PERIOD OF THE VALUE. NEVER ESTIMATED. CLICK A ROW FOR ITS FULL HISTORY.")).height()) + 4;
      m_advY = m_table.bottom() + 8 + m_fnH + 22;
    } else {
      m_fnH = 30;
      m_advY = m_table.bottom() + 57;   // heading line
    }
    // the ADVANCED note: one line left of the range buttons; wrapped above them on phones and wherever the line would
    // run under them (under ~960 px, a phone held sideways)
    {
      const QString note = QCoreApplication::translate("economy", "OFFICIAL SOURCES ONLY — WORLD BANK · BIS · OECD. USE THE RANGE BUTTONS TO CHANGE THE WINDOW.");
      const QFontMetricsF af(eco::mono(9, QFont::Normal, 1.2));
      m_advTextH = m_phone || af.horizontalAdvance(note) > m_w - 160
                     ? std::ceil(af.boundingRect(QRectF(0, 0, m_w, 1000), Qt::TextWordWrap, note).height()) : 0;
    }
    const qreal adv = m_advY;
    m_rangeRects.clear();
    qreal rx = m_x + m_w - 4;
    const QList<int> bw = m_phone ? QList<int>{40, 46, 46, 46} : QList<int>{28, 34, 34, 34};
    const qreal ry = m_advTextH > 0 ? adv + 22 + m_advTextH + 8 : adv + 20, rH = m_phone ? 30 : 22;
    QList<QRectF> tmp;
    for (int i = 3; i >= 0; --i) { tmp.prepend(QRectF(rx - bw[i], ry, bw[i], rH)); rx -= bw[i] + 2; }
    m_rangeRects = tmp;
    m_cardsTop = m_advTextH > 0 ? ry + rH + 12 : adv + 54;
    m_cardRects.clear();
    // small windows: 4 cards per row so each keeps ≥ 230 px; phones: one (two when held wide)
    const int cols = m_phone ? (m_w < 600 ? 1 : 2) : m_w >= 1150 ? 5 : 4;
    const qreal cw = (m_w - (cols - 1) * 7) / cols;
    for (int i = 0; i < 10; ++i) m_cardRects << QRectF(m_x + (i % cols) * (cw + 7), m_cardsTop + (i / cols) * 88, cw, 80);
    h = int(m_cardRects.last().bottom() + 34);
    m_compareEdit->hide();
  } else {
    {
      const qreal bw = QFontMetricsF(eco::mono(11, QFont::Normal, 1.5)).horizontalAdvance(
                         QCoreApplication::translate("economy", "← BACK TO DASHBOARD")) + 22;
      m_backBtn = QRectF(m_x, m_phone ? 130 : 132, bw - 18 > 165 ? std::ceil(bw) : 165.0, m_phone ? 34 : 28);   // reference width unless the text needs more
    }
    // phones: name / value + prior / period on three lines
    m_headBox = QRectF(m_x, m_phone ? 172 : 168, m_w, m_phone ? 84 : 36);
    // buttons sized from their (translated) text; English keeps the reference widths
    const QFontMetricsF bfm(eco::mono(11, QFont::Bold, 0.6));
    auto tw = [&](const char* t, qreal minW) {
      const qreal w = std::ceil(bfm.horizontalAdvance(QCoreApplication::translate("economy", t)));
      return w + 4 > minW ? w + 14 : minW;   // reference widths unless the text needs more
    };
    const int vh = m_viewH > 0 ? m_viewH : 930;
    if (m_phone) {
      // toolbar on three rows (ranges + bar/area · std dev + forecast · compare), finger-sized; no drag hint (a finger
      // drag scrolls the page)
      const qreal T = m_headBox.bottom() + 10;
      m_detRangeRects.clear();
      qreal x = m_x + 14;
      for (int wdt : {38, 40, 46, 46}) { m_detRangeRects << QRectF(x, T + 12, wdt, 30); x += wdt + 1; }
      const qreal bw1 = tw("BAR", 46), bw2 = tw("AREA", 52);
      m_typeRects = {QRectF(m_x + m_w - 14 - bw2 - 1 - bw1, T + 12, bw1, 30), QRectF(m_x + m_w - 14 - bw2, T + 12, bw2, 30)};
      m_stdRect = QRectF(m_x + 10, T + 50, tw("STANDARD DEVIATIONS", 148), 32);
      m_fcRect = QRectF(m_stdRect.right() + 5, T + 50, tw("FORECAST", 74), 32);
      m_compareRect = QRectF(m_x + 10, T + 90, m_w - 20, 32);
      m_hintY = -1;
      const qreal ph = std::clamp(vh * 0.42, 220.0, 360.0);
      m_plot = QRectF(m_x + 24, T + 150, m_w - 24 - 74, ph);
      m_chartBox = QRectF(m_x, T, m_w, m_plot.bottom() + 37 - T);
      m_frame = QRectF(m_x + 10, T + 132, m_w - 20, m_chartBox.bottom() - 7 - (T + 132));
      const qreal capH = std::ceil(QFontMetricsF(eco::cond(14, QFont::Medium)).boundingRect(QRectF(0, 0, m_w - 20, 2000), Qt::TextWordWrap,
                                                                                              captionText(window())).height());
      m_caption = QRectF(m_x, m_chartBox.bottom() + 10, m_w, capH + 34);
    } else {
      // chart box: 602 px at the reference size; fits a short window once scrolled to it, grows on tall screens
      const qreal ch = vh < 930 ? std::clamp(vh - 40.0, 380.0, 602.0) : std::clamp(602.0 + vh - 930, 602.0, 1000.0);
      m_chartBox = QRectF(m_x, 214, m_w, ch);
      m_plot = QRectF(m_x + 24, 307, m_w - 24 - 74, ch - 130);
      m_frame = QRectF(m_x + 10, 289, m_w - 20, ch - 82);
      m_caption = QRectF(m_x, m_chartBox.bottom() + 10, m_w, 36);
      m_detRangeRects.clear();
      qreal x = m_x + 14;
      for (int wdt : {26, 28, 34, 34}) { m_detRangeRects << QRectF(x, 224 + 2, wdt, 22); x += wdt + 1; }
      m_stdRect = QRectF(m_x + 147, 224, tw("STANDARD DEVIATIONS", 148), 26);
      m_fcRect = QRectF(m_stdRect.right() + 5, 224, tw("FORECAST", 74), 26);
      const qreal bw1 = tw("BAR", 34), bw2 = tw("AREA", 40);
      m_typeRects = {QRectF(m_fcRect.right() + 10, 226, bw1, 22), QRectF(m_fcRect.right() + 11 + bw1, 226, bw2, 22)};
      m_compareRect = QRectF(m_x + m_w - 303, 224, 292, 25);
      m_hintY = 275;
    }
    const int nCal = std::min(8, int(m_series.pts.size()));
    m_cal = QRectF(m_x, m_caption.bottom() + 8, m_w, 26 + 26 + 27 * std::max(1, nCal) + 3);
    m_compareEdit->setGeometry(m_compareRect.adjusted(24, 3, -60, -3).toRect());
    m_compareEdit->show();
    h = int(m_cal.bottom() + 23);
  }
  if (minimumHeight() != h) { setMinimumHeight(h); emit heightChanged(); }
}

// ------------------------------------------------------------------------------------------------ paint
void AssetFile::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), eco::pageBg());
  paintHeader(p);
  if (m_detail) paintDetail(p); else paintDashboard(p);
}

void AssetFile::paintHeader(QPainter& p) {
  const Economy* e = macro::economy(m_iso);
  const qreal adv = eco::micro(p, m_x - 1, 33, QCoreApplication::translate("economy", "ASSET FILE"), C("#58554d"), 10, 4);
  if (eco::Blink::instance().on()) p.fillRect(QRectF(m_x + adv + 3, 24, 6, 11), eco::amber());
  if (e) {
    p.drawPixmap(QPointF(m_x, 60), macro::flag(e->iso2, QSize(44, 28)));
    QFont f = eco::cond(52, QFont::Bold); f.setLetterSpacing(QFont::AbsoluteSpacing, 2.4);
    // long names on small windows: step the title down until it fits the column
    // phones: down to 18 px (then elided) within what is right of the flag
    const qreal room = m_phone ? m_w - 55 : m_w - 100;
    const int minPx = m_phone ? 18 : 30;
    while (f.pixelSize() > minPx && QFontMetricsF(f).horizontalAdvance(displayLongName(*e).toUpper()) > room) f.setPixelSize(f.pixelSize() - 2);
    p.setFont(f); p.setPen(C("#f5f4ef"));
    p.drawText(QPointF(m_x + 55, m_phone ? 74 + f.pixelSize() * 0.36 : 93),
               m_phone ? QFontMetricsF(f).elidedText(displayLongName(*e).toUpper(), Qt::ElideRight, room) : displayLongName(*e).toUpper());
  }
  QLinearGradient g(m_x, 0, m_x + m_w, 0);
  g.setColorAt(0, C("#8a7a2a")); g.setColorAt(0.5, C("#3a3214")); g.setColorAt(1, theme::map(40, 34, 14, 0));
  p.fillRect(QRectF(m_x, 121, m_w, 1), g);
  // close ×
  const bool hc = m_close.contains(m_mouse);
  p.setPen(QPen(hc ? eco::amber() : C("#8a877e"), 1.3));
  const QPointF c = m_close.center();
  p.drawLine(c + QPointF(-4.5, -4.5), c + QPointF(4.5, 4.5));
  p.drawLine(c + QPointF(4.5, -4.5), c + QPointF(-4.5, 4.5));
}

void AssetFile::paintDashboard(QPainter& p) {
  eco::sectionHeading(p, m_x, 148, m_x + m_w, QCoreApplication::translate("economy", "SUMMARY"));
  paintSummary(p, m_summary);
  eco::micro(p, m_x, m_dashY, QCoreApplication::translate("economy", "MACRO DASHBOARD"), C("#5b5851"), 10, 2.2);
  const QStringList tabs = tabLabels();
  for (int i = 0; i < tabs.size(); ++i) {
    const QRectF r = m_tabRects[i];
    if (i == m_tab) { p.setPen(Qt::NoPen); p.setBrush(C("#ffda00")); p.drawRoundedRect(r, 2, 2); }
    else { p.setPen(QPen(r.contains(m_mouse) ? C("#6b6343") : C("#2e2a1e"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2); }
    p.setFont(eco::mono(12, QFont::Bold, 1.2));
    p.setPen(i == m_tab ? eco::onAmber() : C("#edece9"));
    p.drawText(r, Qt::AlignCenter, tabs[i]);
  }
  paintTable(p);
  // footnote
  const QRectF fn(m_x, m_table.bottom() + 8, m_w, m_fnH);
  p.setFont(eco::mono(9, QFont::Normal, 0.6)); p.setPen(C("#77746d"));
  p.drawText(fn, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
             QCoreApplication::translate("economy", "LATEST REAL RECORDED VALUE PER INDICATOR FROM OFFICIAL AGENCIES (BIS · OECD · WORLD BANK · CENTRAL BANKS; "
             "ANNUAL WHERE TAGGED). PERIOD = REFERENCE PERIOD OF THE VALUE. NEVER ESTIMATED. CLICK A ROW FOR ITS FULL HISTORY."));
  const qreal adv = m_advY;
  eco::sectionHeading(p, m_x, adv, m_x + m_w, QCoreApplication::translate("economy", "ADVANCED"));
  if (m_advTextH > 0) {   // wrapped above the range buttons (one line would run under them)
    p.setFont(eco::mono(9, QFont::Normal, 1.2)); p.setPen(C("#534f47"));
    p.drawText(QRectF(m_x, adv + 22, m_w, m_advTextH + 2), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
               QCoreApplication::translate("economy", "OFFICIAL SOURCES ONLY — WORLD BANK · BIS · OECD. USE THE RANGE BUTTONS TO CHANGE THE WINDOW."));
  } else
    eco::micro(p, m_x, adv + 35, QCoreApplication::translate("economy", "OFFICIAL SOURCES ONLY — WORLD BANK · BIS · OECD. USE THE RANGE BUTTONS TO CHANGE THE WINDOW."),
               C("#534f47"), 9, 1.2);
  const QStringList rl{QCoreApplication::translate("economy", "5Y"), QCoreApplication::translate("economy", "10Y"), QCoreApplication::translate("economy", "20Y"), QCoreApplication::translate("economy", "MAX")};
  for (int i = 0; i < 4; ++i) eco::button(p, m_rangeRects[i], rl[i], i == m_cardRange, m_rangeRects[i].contains(m_mouse), 11);
  paintCards(p);
}

void AssetFile::paintSummary(QPainter& p, const QRectF& box) {
  p.setPen(QPen(eco::boxBorder(), 1)); p.setBrush(Qt::NoBrush);
  p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
  eco::bracket(p, box, C("#6b6343"));
  const Economy* e = macro::economy(m_iso);
  const PolicyRate pr = MacroStore::instance().policyRate(m_iso);
  const qreal lx = box.left() + 17;
  QString who = e && !e->bankName.isEmpty() ? QCoreApplication::translate("economy", "%1 · POLICY RATE").arg(displayBankName(*e).toUpper())
                                              : QCoreApplication::translate("economy", "POLICY RATE");
  eco::micro(p, lx, box.top() + 27, who, C("#5f5b54"), 9, 3);
  if (pr.valid()) {
    const QFont big = eco::mono(64, QFont::Bold, 1);
    p.setFont(big); p.setPen(eco::amberPale());
    const QString v = QString::number(pr.rate, 'f', 2);
    p.drawText(QPointF(lx - 2, box.top() + 95), v);
    const qreal vw = QFontMetricsF(big).horizontalAdvance(v);
    p.setFont(eco::mono(20)); p.setPen(C("#9b968e"));
    p.drawText(QPointF(lx + vw + 2, box.top() + 95), "%");
    const QString t = pr.fetched.isValid() ? pr.fetched.toLocalTime().toString("HH:mm:ss") : QString("—");
    eco::micro(p, lx, box.top() + 120, (pr.badge == "BIS" ? QCoreApplication::translate("economy", "BIS FEED · %1") : QCoreApplication::translate("economy", "PUBLIC FEED · %1")).arg(t), C("#5e5b54"), 9, 1.5);
    eco::micro(p, lx, box.top() + 136, QCoreApplication::translate("economy", "AS OF %1 · %2").arg(pr.asOf.toString("yyyy-MM-dd"), pr.sourceName.section(" (", 0, 0).toUpper()),
               C("#46433c"), 8, 1);
  } else {
    p.setFont(eco::mono(56, QFont::Bold)); p.setPen(C("#5e5b54"));
    p.drawText(QPointF(lx - 2, box.top() + 95), "—");
    eco::micro(p, lx, box.top() + 120, e && !hasPolicyRate(*e) ? QCoreApplication::translate("economy", "NO POLICY-RATE SERIES (WORLD BANK DATA ONLY)") : QCoreApplication::translate("economy", "LOADING…"),
               C("#5e5b54"), 9, 1.5);
  }

  // yield curve (phones: under the policy rate, its date on the line below its title)
  const Curve cv = YieldCurves::instance().curve(m_iso);
  const qreal cx = m_phone ? box.left() + 17 : box.left() + 456;
  const qreal cy = m_phone ? box.top() + 150 : box.top();   // top of the curve block
  if (m_phone) p.fillRect(QRectF(box.left() + 17, cy, box.width() - 34, 1), eco::hair());
  eco::micro(p, cx, cy + 27, cv.monthly ? QCoreApplication::translate("economy", "SOVEREIGN RATES (OECD MONTHLY)") : QCoreApplication::translate("economy", "SOVEREIGN YIELD CURVE"), C("#5a574f"), 9, m_phone ? 1.5 : 3);
  p.setFont(eco::mono(10)); p.setPen(C("#5a574f"));
  if (cv.asOf.isValid())
    p.drawText(m_phone ? QRectF(cx, cy + 32, box.width() - 34, 16) : QRectF(box.left(), box.top() + 16, box.width() - 18, 16),
               (m_phone ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
               (cv.monthly ? QCoreApplication::translate("economy", "avg %1").arg(i18n::locale().toString(cv.asOf, "MMM yyyy"))
                            : QCoreApplication::translate("economy", "as of %1").arg(cv.asOf.toString("yyyy-MM-dd"))));
  m_curvePlot = m_phone ? QRectF(box.left() + 70, cy + 64, box.width() - 70 - 19, 130) : QRectF(box.left() + 509, box.top() + 51, box.width() - 509 - 19, 137);
  const QRectF pl = m_curvePlot;
  if (!cv.ok()) {
    p.setFont(eco::mono(10)); p.setPen(C("#5a574f"));
    p.drawText(pl, Qt::AlignCenter, YieldCurves::instance().pending(m_iso) ? QCoreApplication::translate("economy", "Loading curve…") : QCoreApplication::translate("economy", "No sovereign curve published for this economy"));
    return;
  }
  double lo = 1e9, hi = -1e9;
  for (const auto& pt : cv.pts) { lo = std::min(lo, pt.yield); hi = std::max(hi, pt.yield); }
  const double span = std::max(0.2, hi - lo);
  // reference: ~0.3-wide steps from just under the lowest yield, top tick just over the highest
  double step = 0.05;
  for (double c : {0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.4, 0.5, 0.75, 1.0, 2.0}) { step = c; if (span / c <= 5) break; }
  double ylo = std::floor((lo - step * 0.8) / step) * step, yhi = std::ceil((hi + step * 0.3) / step) * step;
  if (lo >= 0 && ylo < 0) ylo = 0;
  auto Y = [&](double v) { return pl.bottom() - (v - ylo) / (yhi - ylo) * pl.height(); };
  p.setFont(eco::mono(10));
  for (double t = ylo; t <= yhi + 1e-9; t += step) {
    const qreal y = Y(t);
    p.fillRect(QRectF(pl.left(), std::round(y), pl.width(), 1), C("#1b1709"));
    p.setPen(C("#807d76"));
    p.drawText(QRectF(pl.left() - 60, y - 7, 54, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(t, 'f', 2) + "%");
  }
  const int n = cv.pts.size();
  const qreal slot = pl.width() / n, bw = slot * 0.68;
  int hov = -1;
  if (pl.adjusted(0, -10, 0, 0).contains(m_mouse)) hov = std::clamp(int((m_mouse.x() - pl.left()) / slot), 0, n - 1);
  for (int i = 0; i < n; ++i) {
    const qreal x = pl.left() + i * slot + (slot - bw) / 2;
    const qreal y = Y(cv.pts[i].yield);
    if (i == hov) p.fillRect(QRectF(pl.left() + i * slot, pl.top(), slot, pl.height()), theme::map(51, 38, 9, 200));
    QRectF bar(x, y, bw, pl.bottom() - y);
    if (i == hov) p.fillRect(bar, C("#ffe455"));
    else { QLinearGradient g(0, y, 0, pl.bottom()); g.setColorAt(0, C("#fdd50d")); g.setColorAt(1, C("#88741a")); p.fillRect(bar, g); }
    if (i % 2 == 0 || n <= 7) {
      p.setFont(eco::sans(10)); p.setPen(C("#7b7870"));
      p.drawText(QRectF(pl.left() + i * slot - 20, pl.bottom() + 10, slot + 40, 14), Qt::AlignCenter, tenor(cv.pts[i].label));
    }
  }
  if (hov >= 0)
    eco::tooltip(p, m_mouse, tenor(cv.pts[hov].label), {{eco::amber(), "", QString::number(cv.pts[hov].yield, 'f', 3) + "%"}},
                 box, false, {55, -87});
  p.setFont(eco::mono(8)); p.setPen(C("#3e3b34"));
  p.drawText(QRectF(pl.left(), box.bottom() - 16, pl.width(), 12), Qt::AlignRight | Qt::AlignVCenter, cv.source.toUpper());
}

void AssetFile::paintTable(QPainter& p) {
  const QRectF t = m_table;
  const qreal s = m_w / 1318.0;
  p.fillRect(QRectF(t.left(), t.top(), t.width(), 26), C("#170d02"));
  p.setPen(QPen(eco::boxBorder(), 1)); p.setBrush(Qt::NoBrush);
  p.drawRect(t.adjusted(0.5, 0.5, -0.5, -0.5));
  const QStringList tabs = tabLabels();
  p.setFont(eco::mono(9, QFont::Bold, 1)); p.setPen(C("#c8a921"));
  p.drawText(QRectF(t.left() + 7, t.top(), 300, 26), Qt::AlignVCenter | Qt::AlignLeft, tabs[m_tab]);
  if (m_phone) { paintTablePhone(p); return; }
  p.setFont(eco::mono(9, QFont::Normal, 1)); p.setPen(C("#87837a"));
  const qreal xTrend = t.left() + 646 * s, xLatest = t.left() + 922 * s, xPrior = t.left() + 1080 * s, xRel = t.right() - 8;
  p.drawText(QRectF(xTrend, t.top(), 72 * s, 26), Qt::AlignCenter, QCoreApplication::translate("economy", "TREND"));
  p.drawText(QRectF(xLatest - 120, t.top(), 120, 26), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("economy", "LATEST"));
  p.drawText(QRectF(xPrior - 120, t.top(), 120, 26), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("economy", "PRIOR"));
  p.drawText(QRectF(xRel - 120, t.top(), 120, 26), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("economy", "PERIOD"));
  const auto rs = rows();
  for (int i = 0; i < rs.size() && i < m_rowRects.size(); ++i) {
    const QRectF r = m_rowRects[i];
    if (r.contains(m_mouse)) p.fillRect(r.adjusted(1, 0, -1, 0), C("#1a1710"));
    p.fillRect(QRectF(r.left() + 1, r.bottom() - 0.5, r.width() - 2, 1), eco::hair());
    bool fb = false;
    const Series se = rowSeries(rs[i], &fb);
    const auto& inf = info(se.ok() ? se.ind : rs[i].ind);
    p.setFont(eco::cond(16, QFont::Medium)); p.setPen(C("#e0dcd9"));
    const QString name = rs[i].label;
    p.drawText(QRectF(r.left() + 7, r.top(), 500, r.height()), Qt::AlignVCenter | Qt::AlignLeft, name);
    if (fb || (se.ok() && se.freq == 'A')) {
      const qreal nx = r.left() + 7 + QFontMetricsF(eco::cond(16, QFont::Medium)).horizontalAdvance(name) + 10;
      const QRectF tag(nx, r.center().y() - 8, 50, 16);
      p.setPen(QPen(C("#46c3d7"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(tag.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
      p.setFont(eco::mono(8, QFont::Bold, 0.8)); p.drawText(tag, Qt::AlignCenter, QCoreApplication::translate("economy", "ANNUAL"));
    }
    if (!se.ok()) {
      p.setFont(eco::mono(13)); p.setPen(C("#5e5b54"));
      p.drawText(QRectF(xLatest - 200, r.top(), 200, r.height()), Qt::AlignRight | Qt::AlignVCenter, "—");
      p.setFont(eco::mono(9)); p.setPen(C("#46433c"));
      p.drawText(QRectF(xRel - 300, r.top(), 300, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                 MacroStore::instance().loading() ? QCoreApplication::translate("economy", "loading…") : QCoreApplication::translate("economy", "no free official series"));
      continue;
    }
    const QVector<Point> tail = se.pts.mid(std::max(0, int(se.pts.size()) - 24));
    QVector<double> vals; for (const auto& pt : tail) vals << pt.value;
    const int dir = trendDir(se.pts);
    const QColor tc = dir >= 0 ? C("#2cb1bf") : C("#e36062");
    chart::sparkline(p, QRectF(xTrend, r.top() + 6, 72 * s, r.height() - 12), vals, tc, 1.3);
    Latest l = MacroStore::instance().latest(m_iso, se.ind);
    if (se.ind != Ind::PolicyRate) { l.value = se.last().value; l.prior = se.prior().value; }
    const QString lv = formatValue(se.ind, l.value);
    p.setFont(eco::mono(14, QFont::Bold)); p.setPen(C("#f4edb9"));
    p.drawText(QRectF(xLatest - 200, r.top(), 200, r.height()), Qt::AlignRight | Qt::AlignVCenter, lv);
    const qreal lw = QFontMetricsF(eco::mono(14, QFont::Bold)).horizontalAdvance(lv);
    p.setFont(eco::sans(12, QFont::Bold)); p.setPen(tc);
    p.drawText(QRectF(xLatest - lw - 18, r.top(), 16, r.height()), Qt::AlignCenter, dir >= 0 ? QCoreApplication::translate("economy", "↗") : QCoreApplication::translate("economy", "↘"));
    p.setFont(eco::mono(13)); p.setPen(C("#dddad6"));
    p.drawText(QRectF(xPrior - 200, r.top(), 200, r.height()), Qt::AlignRight | Qt::AlignVCenter, formatValue(se.ind, l.prior));
    QString per = periodShort(se.last(), se.freq);
    if (se.ind == Ind::PolicyRate) {
      const Economy* e = macro::economy(m_iso);
      const Bank* b = e && !e->bank.isEmpty() ? CentralBanks::instance().bank(e->bank) : nullptr;
      if (b && !b->decisions.isEmpty()) per = b->decisions.first().date.toString("yyyy-MM-dd");
      else if (l.period.isValid()) per = l.period.toString("yyyy-MM-dd");
    }
    p.setFont(eco::mono(11)); p.setPen(C("#7a7770"));
    p.drawText(QRectF(xRel - 200, r.top(), 200, r.height()), Qt::AlignRight | Qt::AlignVCenter, per);
    Q_UNUSED(inf);
  }
}

// Phones: each row on two lines — name and latest value, then trend sparkline (+ ANNUAL tag), prior and period.
void AssetFile::paintTablePhone(QPainter& p) {
  const QRectF t = m_table;
  p.setFont(eco::mono(9, QFont::Normal, 1)); p.setPen(C("#87837a"));
  p.drawText(QRectF(t.left(), t.top(), t.width() - 8, 26), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("economy", "LATEST"));
  const auto rs = rows();
  for (int i = 0; i < rs.size() && i < m_rowRects.size(); ++i) {
    const QRectF r = m_rowRects[i];
    if (r.contains(m_mouse)) p.fillRect(r.adjusted(1, 0, -1, 0), C("#1a1710"));
    p.fillRect(QRectF(r.left() + 1, r.bottom() - 0.5, r.width() - 2, 1), eco::hair());
    bool fb = false;
    const Series se = rowSeries(rs[i], &fb);
    const QRectF l1(r.left() + 8, r.top() + 4, r.width() - 16, 26), l2(r.left() + 8, r.top() + 30, r.width() - 16, 22);
    QString lv = "—";
    int dir = 0;
    Latest l;
    if (se.ok()) {
      dir = trendDir(se.pts);
      l = MacroStore::instance().latest(m_iso, se.ind);
      if (se.ind != Ind::PolicyRate) { l.value = se.last().value; l.prior = se.prior().value; }
      lv = formatValue(se.ind, l.value);
    }
    const QColor tc = dir >= 0 ? C("#2cb1bf") : C("#e36062");
    const QFont vf = eco::mono(14, QFont::Bold);
    const qreal lw = QFontMetricsF(vf).horizontalAdvance(lv);
    p.setFont(vf); p.setPen(se.ok() ? C("#f4edb9") : C("#5e5b54"));
    p.drawText(l1, Qt::AlignRight | Qt::AlignVCenter, lv);
    if (se.ok()) {
      p.setFont(eco::sans(12, QFont::Bold)); p.setPen(tc);
      p.drawText(QRectF(l1.right() - lw - 18, l1.top(), 16, l1.height()), Qt::AlignCenter, dir >= 0 ? QCoreApplication::translate("economy", "↗") : QCoreApplication::translate("economy", "↘"));
    }
    const QFont nf = eco::cond(15, QFont::Medium);
    p.setFont(nf); p.setPen(C("#e0dcd9"));
    p.drawText(l1, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF(nf).elidedText(rs[i].label, Qt::ElideRight, l1.width() - lw - 28));
    if (!se.ok()) {
      p.setFont(eco::mono(9)); p.setPen(C("#46433c"));
      p.drawText(l2, Qt::AlignRight | Qt::AlignVCenter,
                 MacroStore::instance().loading() ? QCoreApplication::translate("economy", "loading…") : QCoreApplication::translate("economy", "no free official series"));
      continue;
    }
    const QVector<Point> tail = se.pts.mid(std::max(0, int(se.pts.size()) - 24));
    QVector<double> vals; for (const auto& pt : tail) vals << pt.value;
    chart::sparkline(p, QRectF(l2.left(), l2.top() + 3, 64, l2.height() - 6), vals, tc, 1.3);
    if (fb || se.freq == 'A') {
      const QRectF tag(l2.left() + 72, l2.center().y() - 8, 50, 16);
      p.setPen(QPen(C("#46c3d7"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(tag.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
      p.setFont(eco::mono(8, QFont::Bold, 0.8)); p.drawText(tag, Qt::AlignCenter, QCoreApplication::translate("economy", "ANNUAL"));
    }
    QString per = periodShort(se.last(), se.freq);
    if (se.ind == Ind::PolicyRate) {
      const Economy* e = macro::economy(m_iso);
      const Bank* b = e && !e->bank.isEmpty() ? CentralBanks::instance().bank(e->bank) : nullptr;
      if (b && !b->decisions.isEmpty()) per = b->decisions.first().date.toString("yyyy-MM-dd");
      else if (l.period.isValid()) per = l.period.toString("yyyy-MM-dd");
    }
    p.setFont(eco::mono(11)); p.setPen(C("#7a7770"));
    p.drawText(l2, Qt::AlignRight | Qt::AlignVCenter, per);
    qreal x = l2.right() - QFontMetricsF(eco::mono(11)).horizontalAdvance(per) - 16;
    const QString pv = formatValue(se.ind, l.prior);
    p.setFont(eco::mono(13)); p.setPen(C("#dddad6"));
    p.drawText(QRectF(x - 200, l2.top(), 200, l2.height()), Qt::AlignRight | Qt::AlignVCenter, pv);
    x -= QFontMetricsF(eco::mono(13)).horizontalAdvance(pv) + 6;
    p.setFont(eco::mono(9, QFont::Normal, 1)); p.setPen(C("#87837a"));
    p.drawText(QRectF(x - 200, l2.top(), 200, l2.height()), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("economy", "PRIOR"));
  }
}

void AssetFile::paintCards(QPainter& p) {
  static const QList<Ind> inds{Ind::GdpUsd, Ind::GdpPerCapita, Ind::Population, Ind::RealGrowth, Ind::Inflation,
                               Ind::UnemploymentAnnual, Ind::CurrentAccount, Ind::GovDebt, Ind::FxReserves, Ind::Yield10y};
  const QList<int> yrs{5, 10, 20, 999};
  auto& ms = MacroStore::instance();
  for (int i = 0; i < inds.size() && i < m_cardRects.size(); ++i) {
    const QRectF r = m_cardRects[i];
    p.setPen(QPen(eco::boxBorder(), 1)); p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    Ind ind = inds[i];
    Series s = ms.series(m_iso, ind);
    if (!s.ok() && ind == Ind::Inflation) { ind = Ind::InflationAnnual; s = ms.series(m_iso, ind); }
    if (!s.ok() && ind == Ind::UnemploymentAnnual) { ind = Ind::Unemployment; s = ms.series(m_iso, ind); }
    const auto& in = info(inds[i]);
    eco::micro(p, r.left() + 11, r.top() + 19, in.shortName, C("#75726b"), 9, 2);
    p.setFont(eco::mono(9)); p.setPen(C("#5a564c"));
    if (s.ok()) p.drawText(QRectF(r.left(), r.top() + 10, r.width() - 11, 12), Qt::AlignRight | Qt::AlignVCenter,
                           s.freq == 'A' ? QString::number(s.last().date.year()) : i18n::locale().toString(s.last().date, "MMM yyyy"));
    if (!s.ok()) {
      p.setFont(eco::display(22, QFont::Bold)); p.setPen(C("#5e5b54"));
      p.drawText(QPointF(r.left() + 11, r.top() + 50), "—");
      p.setFont(eco::mono(9)); p.setPen(C("#4d4942"));
      p.drawText(QPointF(r.left() + 11, r.top() + 69), ms.loading() ? QCoreApplication::translate("economy", "loading…") : QCoreApplication::translate("economy", "not published for this economy"));
      continue;
    }
    p.setFont(eco::cond(27, QFont::Bold)); p.setPen(C("#faf9f6"));
    p.drawText(QPointF(r.left() + 11, r.top() + 51), formatValue(ind, s.last().value));
    const QVector<Point> w = lastYears(s.pts, yearsOfRange(m_cardRange, yrs));
    QVector<double> v; for (const auto& pt : w) v << pt.value;
    if (v.size() >= 2) {
      const QRectF sr(r.left() + r.width() * 0.62, r.top() + 27, r.width() * 0.38 - 12, 26);
      auto [lo, hi] = chart::range(v, 0.08);
      QPainterPath path;
      for (int k = 0; k < v.size(); ++k) {
        const QPointF pt(sr.left() + sr.width() * k / (v.size() - 1), sr.bottom() - (v[k] - lo) / std::max(1e-12, hi - lo) * sr.height());
        if (k == 0) path.moveTo(pt); else path.lineTo(pt);
      }
      QLinearGradient g(sr.left(), 0, sr.right(), 0); g.setColorAt(0, C("#97892f")); g.setColorAt(1, C("#f2e6c2"));
      p.setPen(QPen(QBrush(g), 1.3)); p.setBrush(Qt::NoBrush); p.drawPath(path);
      p.setPen(Qt::NoPen); p.setBrush(C("#f8eecb")); p.drawEllipse(path.currentPosition(), 2.2, 2.2);
    }
    const double cur = s.last().value, pri = s.prior().value;
    QString chg; double d = NAN;
    if (std::isfinite(pri)) {
      if (in.level && pri != 0) { d = (cur / pri - 1) * 100; chg = (d >= 0 ? "+" : "") + QString::number(d, 'f', 1) + "%"; }
      else { d = cur - pri; chg = (d >= 0 ? "+" : "") + QString::number(d, 'f', 2) + "pp"; }
    }
    const QColor sc = eco::semantic(info(ind).goodWhenUp, d);
    qreal x = r.left() + 11;
    const qreal yb = r.top() + 69;
    if (std::isfinite(d)) {
      p.setPen(Qt::NoPen); p.setBrush(sc);
      QPolygonF tri;
      if (d >= 0) tri << QPointF(x, yb) << QPointF(x + 8, yb) << QPointF(x + 4, yb - 8);
      else tri << QPointF(x, yb - 8) << QPointF(x + 8, yb - 8) << QPointF(x + 4, yb);
      p.drawPolygon(tri);
      x += 12;
      p.setFont(eco::mono(9)); p.setPen(sc);
      p.drawText(QPointF(x, yb), chg);
      x += QFontMetricsF(eco::mono(9)).horizontalAdvance(chg);
    }
    p.setFont(eco::mono(9)); p.setPen(C("#4d4942"));
    QString src = s.source;
    if (s.source == "OECD") src = "OECD";
    p.drawText(QPointF(x, yb), QCoreApplication::translate("economy", " vs prior · %1").arg(src));
  }
}

// ------------------------------------------------------------------------------------------------ detail
QVector<Point> AssetFile::window() const {
  const QList<int> yrs{3, 5, 10, 999};
  QVector<Point> w = lastYears(m_series.pts, yrs[m_detRange]);
  if (m_win1 >= 0 && m_win1 < w.size() && m_win0 <= m_win1) w = w.mid(m_win0, m_win1 - m_win0 + 1);
  return w;
}

void AssetFile::paintDetail(QPainter& p) {
  // back button
  {
    const bool h = m_backBtn.contains(m_mouse);
    p.setPen(QPen(h ? C("#6b6343") : C("#3f3824"), 1)); p.setBrush(C("#1b1501"));
    p.drawRect(m_backBtn.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setFont(eco::mono(11, QFont::Normal, 1.5)); p.setPen(h ? eco::amber() : C("#898475"));
    p.drawText(m_backBtn, Qt::AlignCenter, QCoreApplication::translate("economy", "← BACK TO DASHBOARD"));
  }
  const auto& in = info(m_series.ok() ? m_series.ind : m_ind);
  QString name = in.name;
  for (const auto& r : rows()) if (r.ind == m_ind) name = r.label;
  // header box
  {
    const QRectF b = m_headBox;
    p.setPen(QPen(C("#45412f"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(b.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setFont(eco::cond(17, QFont::Bold)); p.setPen(C("#efece8"));
    if (m_phone) {   // three lines: name / trend, value, prior / period
      p.drawText(QRectF(b.left() + 10, b.top() + 4, b.width() - 20, 26), Qt::AlignVCenter | Qt::AlignLeft,
                 QFontMetricsF(eco::cond(17, QFont::Bold)).elidedText(name, Qt::ElideRight, b.width() - 20));
      if (m_series.ok()) {
        const QRectF l2(b.left() + 10, b.top() + 30, b.width() - 20, 26);
        const int dir = trendDir(m_series.pts);
        p.setFont(eco::sans(12, QFont::Bold)); p.setPen(dir >= 0 ? C("#6ab8bb") : C("#e36062"));
        p.drawText(QRectF(l2.left(), l2.top(), 14, l2.height()), Qt::AlignCenter, dir >= 0 ? QCoreApplication::translate("economy", "↗") : QCoreApplication::translate("economy", "↘"));
        const QString v = formatValue(m_series.ind, m_series.last().value);
        p.setFont(eco::mono(13, QFont::Bold)); p.setPen(C("#fef4d9"));
        p.drawText(QRectF(l2.left() + 18, l2.top(), 200, l2.height()), Qt::AlignVCenter | Qt::AlignLeft, v);
        p.setFont(eco::mono(12)); p.setPen(C("#7f7c75"));
        p.drawText(QRectF(l2.left() + 28 + QFontMetricsF(eco::mono(13, QFont::Bold)).horizontalAdvance(v), l2.top(), 300, l2.height()), Qt::AlignVCenter | Qt::AlignLeft,
                   QCoreApplication::translate("economy", "Prior %1").arg(formatValue(m_series.ind, m_series.prior().value)));
        p.setFont(eco::mono(10)); p.setPen(C("#79766f"));
        p.drawText(QRectF(l2.left(), b.top() + 56, l2.width(), 24), Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetricsF(eco::mono(10)).elidedText(QCoreApplication::translate("economy", "Period %1   ·   Next release —").arg(formatPeriod(m_series.last().date, m_series.freq)),
                                                          Qt::ElideRight, l2.width()));
      }
    }
    qreal x = b.left() + 10 + QFontMetricsF(eco::cond(17, QFont::Bold)).horizontalAdvance(name) + 10;
    if (!m_phone) p.drawText(QRectF(b.left() + 10, b.top(), 600, b.height()), Qt::AlignVCenter | Qt::AlignLeft, name);
    if (m_series.ok() && !m_phone) {
      const int dir = trendDir(m_series.pts);
      p.setFont(eco::sans(12, QFont::Bold)); p.setPen(dir >= 0 ? C("#6ab8bb") : C("#e36062"));
      p.drawText(QRectF(x, b.top(), 14, b.height()), Qt::AlignCenter, dir >= 0 ? QCoreApplication::translate("economy", "↗") : QCoreApplication::translate("economy", "↘"));
      x += 15;
      const QString v = formatValue(m_series.ind, m_series.last().value);
      p.setFont(eco::mono(13, QFont::Bold)); p.setPen(C("#fef4d9"));
      p.drawText(QRectF(x, b.top(), 200, b.height()), Qt::AlignVCenter | Qt::AlignLeft, v);
      x += QFontMetricsF(eco::mono(13, QFont::Bold)).horizontalAdvance(v) + 10;
      p.setFont(eco::mono(12)); p.setPen(C("#7f7c75"));
      p.drawText(QRectF(x, b.top(), 300, b.height()), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("economy", "Prior %1").arg(formatValue(m_series.ind, m_series.prior().value)));
      p.setFont(eco::mono(10)); p.setPen(C("#79766f"));
      p.drawText(QRectF(b.left(), b.top(), b.width() - 12, b.height()), Qt::AlignVCenter | Qt::AlignRight,
                 QCoreApplication::translate("economy", "Period %1   ·   Next release —").arg(formatPeriod(m_series.last().date, m_series.freq)));
    }
  }
  // chart box + toolbar
  {
    const QRectF b = m_chartBox;
    p.setPen(QPen(eco::boxBorderHi(), 1)); p.setBrush(Qt::NoBrush); p.drawRect(b.adjusted(0.5, 0.5, -0.5, -0.5));
    // range group container
    const QRectF grp(m_detRangeRects.first().left() - 4, m_detRangeRects.first().top() - 2, m_detRangeRects.last().right() - m_detRangeRects.first().left() + 8,
                     m_detRangeRects.first().height() + 4);   // desktop: y 224, 26 tall
    p.setPen(QPen(C("#2a2418"), 1)); p.drawRect(grp.adjusted(0.5, 0.5, -0.5, -0.5));
    const QStringList rl{QCoreApplication::translate("economy", "3Y"), QCoreApplication::translate("economy", "5Y"), QCoreApplication::translate("economy", "10Y"), QCoreApplication::translate("economy", "MAX")};
    for (int i = 0; i < 4; ++i) eco::button(p, m_detRangeRects[i], rl[i], i == m_detRange && m_win1 < 0, m_detRangeRects[i].contains(m_mouse), 11);
    p.setPen(QPen(m_std ? eco::amber() : C("#2a2418"), 1)); p.setBrush(m_std ? QBrush(eco::amberDimFill()) : QBrush(Qt::NoBrush));   // (a bare Qt::NoBrush here became a black QColor)
    p.drawRect(m_stdRect.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setFont(eco::mono(11, QFont::Bold, 0.6)); p.setPen(m_std ? eco::amber() : m_stdRect.contains(m_mouse) ? eco::amber() : C("#8a8781"));
    p.drawText(m_stdRect, Qt::AlignCenter, QCoreApplication::translate("economy", "STANDARD DEVIATIONS"));
    p.setPen(QPen(C("#2a2418"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(m_fcRect.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setPen(C("#4e4b45")); p.drawText(m_fcRect, Qt::AlignCenter, QCoreApplication::translate("economy", "FORECAST"));
    const QRectF tg(m_typeRects[0].left() - 4, m_typeRects[0].top() - 2, m_typeRects[1].right() - m_typeRects[0].left() + 8, m_typeRects[0].height() + 4);
    p.setPen(QPen(C("#2a2418"), 1)); p.drawRect(tg.adjusted(0.5, 0.5, -0.5, -0.5));
    eco::button(p, m_typeRects[0], QCoreApplication::translate("economy", "BAR"), !m_area, m_typeRects[0].contains(m_mouse), 11);
    eco::button(p, m_typeRects[1], QCoreApplication::translate("economy", "AREA"), m_area, m_typeRects[1].contains(m_mouse), 11);
    // compare box
    p.setPen(QPen(m_compareEdit->hasFocus() ? C("#6b6343") : C("#2a2418"), 1)); p.setBrush(C("#030200"));
    p.drawRect(m_compareRect.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setPen(QPen(C("#8a877e"), 1.2)); p.setBrush(Qt::NoBrush);
    const QPointF mc(m_compareRect.left() + 13, m_compareRect.center().y() - 1);
    p.drawEllipse(mc, 4, 4); p.drawLine(mc + QPointF(3, 3), mc + QPointF(6, 6));
    const QRectF kc(m_compareRect.right() - 48, m_compareRect.top() + 5, 40, 15);
    p.setPen(QPen(C("#2e2b25"), 1)); p.drawRoundedRect(kc, 2, 2);
    p.setFont(eco::mono(9)); p.setPen(C("#494844")); p.drawText(kc, Qt::AlignCenter, "Ctrl K");
    if (m_hintY > 0)
      eco::micro(p, b.left() + 9, m_hintY, QCoreApplication::translate("economy", "DRAG ACROSS THE CHART TO ZOOM INTO A PERIOD · DOUBLE-CLICK TO RESET"), C("#56524b"), 10, 1.6);
    const QRectF frame = m_frame;
    p.setPen(QPen(C("#1b170d"), 1)); p.drawRect(frame.adjusted(0.5, 0.5, -0.5, -0.5));
    paintChart(p, m_plot);
  }
  // caption
  const QVector<Point> w = window();
  {
    const QRectF b = m_caption;
    p.setPen(QPen(C("#3f3824"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(b.adjusted(0.5, 0.5, -0.5, -0.5));
    const QString text = captionText(w);
    p.setFont(eco::cond(14, QFont::Medium)); p.setPen(C("#c6c4be"));
    const QFontMetricsF fm(eco::cond(14, QFont::Medium));
    m_captionFull = text;
    if (m_phone) {   // wrapped, the source on its own line under it
      p.drawText(QRectF(b.left() + 10, b.top() + 6, b.width() - 20, b.height() - 30), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
      eco::micro(p, b.left() + 10, b.bottom() - 10, QCoreApplication::translate("economy", "— SOURCE: %1 · %2").arg(m_series.source.toUpper(), m_series.dataset.toUpper()),
                 C("#78756e"), 9, 0.6);
    } else {
    const QString el = fm.elidedText(text, Qt::ElideRight, b.width() - 360);
    p.drawText(QRectF(b.left() + 10, b.top(), b.width(), b.height()), Qt::AlignVCenter | Qt::AlignLeft, el);
    const qreal x = b.left() + 10 + fm.horizontalAdvance(el) + 4;
    eco::micro(p, x, b.center().y() + 3, QCoreApplication::translate("economy", "— SOURCE: %1 · %2").arg(m_series.source.toUpper(), m_series.dataset.toUpper()),
               C("#78756e"), 9, 1.2);
    }
  }
  // recorded history
  {
    const QRectF b = m_cal;
    p.setPen(QPen(eco::boxBorder(), 1)); p.setBrush(Qt::NoBrush); p.drawRect(b.adjusted(0.5, 0.5, -0.5, -0.5));
    eco::micro(p, b.left() + 9, b.top() + 17, QCoreApplication::translate("economy", "RECORDED HISTORY (REAL OFFICIAL VALUES)"), C("#59564e"), m_phone ? 9 : 10, m_phone ? 1.2 : 4);
    const QRectF hd(b.left() + 1, b.top() + 26, b.width() - 2, 26);
    p.fillRect(hd, C("#0f0a03"));
    p.setFont(eco::mono(9, QFont::Normal, 1)); p.setPen(C("#85837d"));
    const qreal xa = b.left() + b.width() * 0.677, xc = b.right() - 10;
    p.drawText(QRectF(b.left() + 9, hd.top(), 200, 26), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("economy", "PERIOD"));
    p.drawText(QRectF(xa - 200, hd.top(), 200, 26), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("economy", "ACTUAL"));
    p.drawText(QRectF(xc - 300, hd.top(), 300, 26), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("economy", "CHANGE"));
    const int n = std::min(8, int(m_series.pts.size()));
    for (int i = 0; i < n; ++i) {
      const int k = m_series.pts.size() - 1 - i;
      const QRectF r(b.left() + 1, hd.bottom() + i * 27, b.width() - 2, 27);
      if (i > 0) p.fillRect(QRectF(r.left(), r.top(), r.width(), 1), eco::hair());
      p.setFont(eco::mono(12, QFont::Bold)); p.setPen(C("#fffdf9"));
      p.drawText(QRectF(r.left() + 8, r.top(), 300, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                 periodShort(m_series.pts[k], m_series.freq));
      p.setPen(C("#f8f3d3"));
      p.drawText(QRectF(xa - 200, r.top(), 200, r.height()), Qt::AlignVCenter | Qt::AlignRight, formatValue(m_series.ind, m_series.pts[k].value));
      QString ch = "—";
      if (k > 0) {
        const double d = m_series.pts[k].value - m_series.pts[k - 1].value;
        ch = info(m_series.ind).level ? (d >= 0 ? "+" : "") + QString::number((m_series.pts[k].value / m_series.pts[k - 1].value - 1) * 100, 'f', 1) + "%"
                                      : (d >= 0 ? "+" : "") + QString::number(d, 'f', 2);
      }
      p.setFont(eco::mono(12)); p.setPen(C("#9a9790"));
      p.drawText(QRectF(xc - 300, r.top(), 300, r.height()), Qt::AlignVCenter | Qt::AlignRight, ch);
    }
    if (n == 0) {
      p.setFont(eco::mono(11)); p.setPen(C("#5e5b54"));
      p.drawText(QRectF(b.left() + 9, hd.bottom(), 500, 27), Qt::AlignVCenter | Qt::AlignLeft, "—");
    }
  }
}

QString AssetFile::captionText(const QVector<Point>& w) const {
  const Economy* e = macro::economy(m_iso);
  QString name = info(m_series.ok() ? m_series.ind : m_ind).name;
  for (const auto& r : rows()) if (r.ind == m_ind) name = r.label;
  QString text;
  if (m_series.ok() && e) {
    const double cur = m_series.last().value, pri = m_series.prior().value;
    double lo = 1e18, hi = -1e18, sum = 0;
    for (const auto& pt : w) { lo = std::min(lo, pt.value); hi = std::max(hi, pt.value); sum += pt.value; }
    const QString dir = !std::isfinite(pri) ? QString() : cur > pri ? QCoreApplication::translate("economy", "up from")
                        : cur < pri ? QCoreApplication::translate("economy", "down from") : QCoreApplication::translate("economy", "unchanged from");
    text = QCoreApplication::translate("economy", "Latest %1 for %2: %3, for %4").arg(name, displayName(*e), formatValue(m_series.ind, cur),
                                                       m_series.freq == 'M' ? i18n::locale().toString(m_series.last().date, "MMMM yyyy")
                                                                            : formatPeriod(m_series.last().date, m_series.freq));
    if (!dir.isEmpty()) text += QCoreApplication::translate("economy", ", %1 %2 previously").arg(dir, formatValue(m_series.ind, pri));
    if (!w.isEmpty())
      text += QCoreApplication::translate("economy", ". Real range over the visible window: %1 to %2, averaging %3.")
                .arg(formatValue(m_series.ind, lo), formatValue(m_series.ind, hi)).arg(QString::number(sum / w.size(), 'f', 2));
  } else text = MacroStore::instance().loading() ? QCoreApplication::translate("economy", "Loading the series…") : QCoreApplication::translate("economy", "No official series for this indicator and economy.");
  return text;
}

void AssetFile::paintChart(QPainter& p, const QRectF& pl) {
  const QVector<Point> w = window();
  if (w.isEmpty()) {
    p.setFont(eco::mono(11)); p.setPen(C("#5e5b54"));
    p.drawText(pl, Qt::AlignCenter, MacroStore::instance().loading() ? QCoreApplication::translate("economy", "Loading…") : QCoreApplication::translate("economy", "No official series for this indicator"));
    return;
  }
  // compare series aligned by period
  QHash<QDate, double> cmp;
  for (const auto& pt : m_compare.pts) cmp.insert(pt.date, pt.value);
  double lo = 1e18, hi = -1e18, sum = 0, sq = 0;
  for (const auto& pt : w) { lo = std::min(lo, pt.value); hi = std::max(hi, pt.value); sum += pt.value; }
  for (const auto& pt : w) if (cmp.contains(pt.date)) { lo = std::min(lo, cmp[pt.date]); hi = std::max(hi, cmp[pt.date]); }
  const double mean = sum / w.size();
  for (const auto& pt : w) sq += (pt.value - mean) * (pt.value - mean);
  const double sd = w.size() > 1 ? std::sqrt(sq / (w.size() - 1)) : 0;
  if (m_std) { lo = std::min(lo, mean - 2 * sd); hi = std::max(hi, mean + 2 * sd); }
  const bool isPct = info(m_series.ind).unit == "%";
  const bool big = std::fabs(hi) >= 1e6;
  double span = std::max(hi - lo, std::fabs(hi) * 0.02 + 1e-9);
  const double step = niceStep(span * 1.1, 17);
  double ylo = std::floor(lo / step) * step - step;   // reference: one step below the floor (2.3 → 1.5 at 0.5)
  if (lo >= 0 && ylo < 0) ylo = 0;
  if (lo < 0 && hi > 0) ylo = std::floor(lo / step) * step;
  const double yhi = std::ceil(hi / step) * step + step;   // and one above (9.1 → 10)
  auto Y = [&](double v) { return pl.bottom() - (v - ylo) / (yhi - ylo) * pl.height(); };
  // grid + y labels (right)
  for (double t = ylo; t <= yhi + step * 1e-6; t += step) {
    const qreal y = std::round(Y(t));
    p.fillRect(QRectF(pl.left(), y, pl.width(), 1), C("#16130a"));
    p.setFont(eco::sans(11)); p.setPen(C("#959391"));
    const QString lab = big ? formatValue(m_series.ind, t) : axisPct(t, step) + (isPct ? "%" : "");
    p.drawText(QRectF(pl.right() + 7, y - 8, 70, 16), Qt::AlignVCenter | Qt::AlignLeft, lab);
  }
  const int n = w.size();
  const qreal slot = pl.width() / n;
  const qreal bw = std::max(1.0, slot * 0.76);
  const double base = (ylo < 0 && yhi > 0) ? 0.0 : ylo;
  int hov = -1;
  if (pl.adjusted(0, -8, 0, 0).contains(m_mouse) && !m_dragging) hov = std::clamp(int((m_mouse.x() - pl.left()) / slot), 0, n - 1);
  if (hov >= 0) p.fillRect(QRectF(pl.left() + hov * slot, pl.top(), slot, pl.height()), theme::map(51, 38, 9, 220));
  if (!m_area) {
    for (int i = 0; i < n; ++i) {
      const qreal x = pl.left() + i * slot + (slot - bw) / 2;
      const qreal y0 = Y(base), y1 = Y(w[i].value);
      QRectF bar(x, std::min(y0, y1), bw, std::fabs(y1 - y0));
      if (i == hov) p.fillRect(bar, C("#ffe455"));
      else if (w[i].value < base) p.fillRect(bar, C("#f46a5d"));
      else { QLinearGradient g(0, y1, 0, pl.bottom()); g.setColorAt(0, C("#fad615")); g.setColorAt(1, C("#88780d")); p.fillRect(bar, g); }
    }
  } else {
    QPainterPath line, fill;
    for (int i = 0; i < n; ++i) {
      const QPointF pt(pl.left() + (i + 0.5) * slot, Y(w[i].value));
      if (i == 0) { line.moveTo(pt); fill.moveTo(pt.x(), Y(base)); fill.lineTo(pt); } else { line.lineTo(pt); fill.lineTo(pt); }
    }
    fill.lineTo(pl.left() + (n - 0.5) * slot, Y(base)); fill.closeSubpath();
    QLinearGradient g(0, pl.top(), 0, pl.bottom()); g.setColorAt(0, theme::map(250, 214, 21, 110)); g.setColorAt(1, theme::map(136, 120, 13, 10));
    p.setPen(Qt::NoPen); p.setBrush(g); p.drawPath(fill);
    p.setPen(QPen(C("#fad615"), 1.6)); p.setBrush(Qt::NoBrush); p.drawPath(line);
    if (hov >= 0) { p.setPen(QPen(C("#0a0600"), 1.5)); p.setBrush(C("#ffe455")); p.drawEllipse(QPointF(pl.left() + (hov + 0.5) * slot, Y(w[hov].value)), 4, 4); }
  }
  if (ylo < 0 && yhi > 0) p.fillRect(QRectF(pl.left(), std::round(Y(0)), pl.width(), 1), C("#5a5440"));
  // compare line
  if (!cmp.isEmpty()) {
    QPainterPath path; bool open = false;
    for (int i = 0; i < n; ++i) {
      if (!cmp.contains(w[i].date)) { open = false; continue; }
      const QPointF pt(pl.left() + (i + 0.5) * slot, Y(cmp[w[i].date]));
      if (!open) { path.moveTo(pt); open = true; } else path.lineTo(pt);
    }
    p.setPen(QPen(eco::cyan(), 1.6)); p.setBrush(Qt::NoBrush); p.drawPath(path);
    const Economy* ce = macro::economy(m_compareIso);
    p.setFont(eco::mono(10, QFont::Bold)); p.setPen(eco::cyan());
    p.drawText(QRectF(pl.left() + 6, pl.top() - 2, 400, 14), Qt::AlignLeft | Qt::AlignVCenter,
               QCoreApplication::translate("economy", "— ") + (ce ? displayName(*ce).toUpper() : QString()) + "   " + QCoreApplication::translate("economy", "■ ") + (macro::economy(m_iso) ? displayName(*macro::economy(m_iso)).toUpper() : QString()));
  }
  // standard deviations
  if (m_std && sd > 0) {
    const QList<QPair<double, QString>> lines{{mean, QCoreApplication::translate("economy", "μ")}, {mean + sd, "+1σ"}, {mean - sd, "-1σ"},
                                              {mean + 2 * sd, "+2σ"}, {mean - 2 * sd, "-2σ"}};
    for (const auto& [v, lab] : lines) {
      const qreal y = Y(v);
      QPen pen(lab == QCoreApplication::translate("economy", "μ") ? C("#e9dca1") : C("#8a7e45"), 1, Qt::DashLine); pen.setDashPattern({4, 3});
      p.setPen(pen); p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y));
      p.setFont(eco::mono(10, QFont::Bold)); p.setPen(C("#e9dca1"));
      p.drawText(QRectF(pl.left() + 4, y - 15, 120, 13), Qt::AlignLeft | Qt::AlignVCenter, lab + " " + formatValue(m_series.ind, v));
    }
  }
  // x labels
  {
    const int target = std::max(4, int(pl.width() / 64));
    const char f = m_series.freq;
    int monthsPer = f == 'A' ? 12 : f == 'Q' ? 3 : 1;
    const int spanMonths = n * monthsPer;
    int every = 1;
    for (int c : {1, 2, 3, 6, 12, 24, 36, 60}) { every = c; if (spanMonths / c <= target) break; }
    if (every < monthsPer) every = monthsPer;
    p.setFont(eco::sans(11)); p.setPen(C("#92908d"));
    qreal lastRight = -1e9;
    const int everyBars = std::max(1, every / monthsPer);
    for (int i = 0; i < n; ++i) {
      const QDate d = w[i].date;
      if (i % everyBars != 0) continue;   // reference: labels counted in bars from the first one
      const QString lab = f == 'A' ? QString::number(d.year()) : every >= 12 ? QString::number(d.year()) : i18n::locale().toString(d, "MMM yy");
      const qreal cx = pl.left() + (i + 0.5) * slot;
      const qreal tw = QFontMetricsF(eco::sans(11)).horizontalAdvance(lab);
      if (cx - tw / 2 < lastRight + 8) continue;
      p.drawText(QRectF(cx - 40, pl.bottom() + 9, 80, 14), Qt::AlignCenter, lab);
      lastRight = cx + tw / 2;
    }
  }
  // drag selection
  if (m_dragging && std::fabs(m_dragX1 - m_dragX0) > 2) {
    const QRectF sel(std::min(m_dragX0, m_dragX1), pl.top(), std::fabs(m_dragX1 - m_dragX0), pl.height());
    p.fillRect(sel, theme::map(245, 197, 24, 40));
    p.setPen(QPen(theme::map(245, 197, 24, 140), 1)); p.drawLine(sel.topLeft(), sel.bottomLeft()); p.drawLine(sel.topRight(), sel.bottomRight());
  }
  if (hov >= 0) {
    const Economy* e = macro::economy(m_iso);
    QList<eco::TipRow> tr{{C("#ffd402"), e ? displayName(*e) : m_iso, formatValue(m_series.ind, w[hov].value).replace(QRegularExpression("\\.?0+%$"), "%")}};
    if (cmp.contains(w[hov].date)) {
      const Economy* ce = macro::economy(m_compareIso);
      tr << eco::TipRow{eco::cyan(), ce ? displayName(*ce) : m_compareIso, formatValue(m_series.ind, cmp[w[hov].date])};
    }
    QString t = tr[0].value;
    if (isPct) tr[0].value = eco::pctTrim(w[hov].value, std::max(1, info(m_series.ind).decimals));
    eco::tooltip(p, m_mouse, formatPeriod(w[hov].date, m_series.freq), tr, rect().adjusted(0, 0, -6, 0), true, {25, 30});
  }
}

// ------------------------------------------------------------------------------------------------ input
// Everything the paint draws differently under the mouse (hover outlines, the bar under the pointer and its tooltip),
// in both views: a move that neither leaves nor enters one of these changes nothing on screen.
bool AssetFile::mouseSensitive(const QPointF& pt) const {
  if (m_close.contains(pt) || m_backBtn.contains(pt) || m_stdRect.contains(pt)) return true;
  if (m_curvePlot.adjusted(0, -10, 0, 0).contains(pt) || m_plot.adjusted(0, -8, 0, 0).contains(pt)) return true;
  for (const auto* list : {&m_tabRects, &m_rowRects, &m_rangeRects, &m_detRangeRects, &m_typeRects})
    for (const auto& r : *list) if (r.contains(pt)) return true;
  return false;
}

void AssetFile::mouseMoveEvent(QMouseEvent* e) {
  const bool wasSensitive = mouseSensitive(m_mouse);
  m_mouse = e->position(); m_inside = true;
  if (m_dragging) m_dragX1 = std::clamp(m_mouse.x(), m_plot.left(), m_plot.right());
  bool hand = m_close.contains(m_mouse);
  if (!m_detail) {
    for (const auto& r : m_tabRects) hand |= r.contains(m_mouse);
    for (const auto& r : m_rowRects) hand |= r.contains(m_mouse);
    for (const auto& r : m_rangeRects) hand |= r.contains(m_mouse);
  } else {
    hand |= m_backBtn.contains(m_mouse) || m_stdRect.contains(m_mouse);
    for (const auto& r : m_detRangeRects) hand |= r.contains(m_mouse);
    for (const auto& r : m_typeRects) hand |= r.contains(m_mouse);
  }
  setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (m_detail && m_caption.contains(m_mouse) && !m_captionFull.isEmpty())
    QToolTip::showText(e->globalPosition().toPoint(), m_captionFull, this);
  else if (m_detail && m_fcRect.contains(m_mouse))
    QToolTip::showText(e->globalPosition().toPoint(), QCoreApplication::translate("economy", "No free consensus-forecast source: forecasts are not shown."), this);
  if (m_dragging || wasSensitive || mouseSensitive(m_mouse)) update();
}

void AssetFile::mousePressEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  if (e->button() != Qt::LeftButton) return;
  if (m_close.contains(pos)) { emit closeRequested(); return; }
  if (!m_detail) {
    for (int i = 0; i < m_tabRects.size(); ++i) if (m_tabRects[i].contains(pos)) { m_tab = i; relayout(); update(); return; }
    for (int i = 0; i < m_rangeRects.size(); ++i) if (m_rangeRects[i].contains(pos)) { m_cardRange = i; update(); return; }
    const auto rs = rows();
    for (int i = 0; i < m_rowRects.size() && i < rs.size(); ++i) if (m_rowRects[i].contains(pos)) { showDetail(rs[i].ind); return; }
    return;
  }
  if (m_backBtn.contains(pos)) { showDashboard(); return; }
  for (int i = 0; i < m_detRangeRects.size(); ++i) if (m_detRangeRects[i].contains(pos)) { m_detRange = i; m_win0 = 0; m_win1 = -1; if (m_phone) relayout(); update(); return; }   // phones: the wrapped caption may change height
  if (m_stdRect.contains(pos)) { m_std = !m_std; update(); return; }
  if (m_typeRects[0].contains(pos)) { m_area = false; update(); return; }
  if (m_typeRects[1].contains(pos)) { m_area = true; update(); return; }
  if (m_plot.contains(pos)) { m_dragging = true; m_dragX0 = m_dragX1 = pos.x(); }
}

void AssetFile::mouseReleaseEvent(QMouseEvent*) {
  if (!m_dragging) return;
  m_dragging = false;
  if (std::fabs(m_dragX1 - m_dragX0) > 8) {
    const QVector<Point> w = window();
    const int n = w.size();
    if (n > 2) {
      const qreal slot = m_plot.width() / n;
      int a = std::clamp(int((std::min(m_dragX0, m_dragX1) - m_plot.left()) / slot), 0, n - 1);
      int b = std::clamp(int((std::max(m_dragX0, m_dragX1) - m_plot.left()) / slot), 0, n - 1);
      if (b - a >= 1) {
        const int off = m_win1 >= 0 ? m_win0 : 0;
        m_win0 = off + a; m_win1 = off + b;
      }
    }
  }
  update();
}

void AssetFile::mouseDoubleClickEvent(QMouseEvent* e) {
  if (m_detail && m_plot.contains(e->position())) { m_win0 = 0; m_win1 = -1; update(); }
}
void AssetFile::leaveEvent(QEvent*) {
  if (qEnvironmentVariableIsSet("DW_MACRO_HOVER")) return;
  m_mouse = {-1, -1}; m_inside = false; setCursor(Qt::ArrowCursor); update(); }
void AssetFile::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape) { if (m_detail) showDashboard(); else emit closeRequested(); return; }
  QWidget::keyPressEvent(e);
}
}  // namespace economy
