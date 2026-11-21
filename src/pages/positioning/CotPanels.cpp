#include "pages/positioning/CotPanels.h"
#include "pages/positioning/PosKit.h"
#include "pages/positioning/CandleChart.h"
#include "core/I18n.h"
#include "ui/Theme.h"
#include "ui/Chart.h"
#include "ui/Format.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QHelpEvent>
#include <QLocale>
#include <QTextLayout>
#include <cmath>
#include <algorithm>

using positioning::CotHistory;
using positioning::CotWeek;
using positioning::CotCategory;

namespace positioning::cotui {
namespace {
const QColor kCyan(0x22, 0xd3, 0xee), kRed(0xef, 0x5f, 0x5f), kYel(0xf2, 0xd8, 0x6a), kPur(0xa7, 0x8b, 0xfa), kGrn(0x5c, 0xc8, 0x84);
const QColor kPos(0x6f, 0xb2, 0x86), kNeg(0xe0, 0x62, 0x60);
const QColor kRowText(0x9e, 0x99, 0x90), kNum(0xf3, 0xf0, 0xe7), kDim(0x57, 0x54, 0x4c);

QString n0(double v) { return std::isfinite(v) ? fmt::num(v, 0) : fmt::dash(); }
QVector<CotWeek> lastWeeks(const CotHistory& h, int n) { return h.weeks.mid(std::max(0, int(h.weeks.size()) - n)); }
// A cached layer is redrawn when missing or drawn for another pixel ratio (window moved between a Retina and a
// standard screen: it was kept and shown blurry or too sharp-small).
bool cacheStale(const QPixmap& pm, const QWidget* w) { return pm.isNull() || pm.devicePixelRatio() != w->devicePixelRatioF(); }
QPixmap makeCache(QWidget* w) {
  const qreal dpr = w->devicePixelRatioF();
  QPixmap pm(w->size() * dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent);
  return pm;
}
void text(QPainter& p, const QRectF& r, int flags, const QString& t, const QFont& f, const QColor& c) { p.setFont(f); p.setPen(c); p.drawText(r, flags, t); }
// CFTC category / report names are English in positioning::Cot (marked with QT_TRANSLATE_NOOP there): translated here
QString catTr(const QString& n) { return QCoreApplication::translate("positioning", n.toUtf8().constData()); }
QString catUpper(const QString& n) { return catTr(n).toUpper(); }
[[maybe_unused]] const char* kReportNames[] = {QT_TRANSLATE_NOOP("positioning", "TFF"), QT_TRANSLATE_NOOP("positioning", "DISAGGREGATED")};
}

// ------------------------------------------------------------------------------------------------ Panel
Panel::Panel(const QString& title, QWidget* parent) : QWidget(parent), m_title(title) {}
bool Panel::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const QString t = headerTip();
    if (he->pos().y() < kHead && !t.isEmpty()) { QToolTip::showText(he->globalPos(), t, this); return true; }
  }
  return QWidget::event(e);
}
void Panel::paintFrame(QPainter& p) {
  p.fillRect(rect(), pk::col::kPanelCot());
  p.setPen(QPen(theme::map(0x24, 0x22, 0x18), 1)); p.setBrush(Qt::NoBrush);
  p.drawRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5));
  p.fillRect(QRectF(1, kHead, width() - 2, 1), theme::map(0x22, 0x21, 0x12));
  const QFont tf = theme::mono(10, QFont::Bold, 0.6);
  const QFontMetricsF fm(tf);
  p.setFont(tf); p.setPen(theme::map(0xb2, 0xac, 0xa1));
  p.drawText(QPointF(9, kHead / 2.0 + fm.capHeight() / 2), m_title);
  if (!m_sub.isEmpty()) {
    const QFont sf = theme::mono(9, QFont::Normal, 0.8);
    p.setFont(sf); p.setPen(theme::map(0x45, 0x43, 0x3b));
    const qreal x = 9 + fm.horizontalAdvance(m_title) + 9;
    p.drawText(QPointF(x, kHead / 2.0 + QFontMetricsF(sf).capHeight() / 2),
               QFontMetrics(sf).elidedText(m_sub, Qt::ElideRight, int(width() - x - 8)));
  }
}

// ------------------------------------------------------------------------------------------------ Price
PricePanel::PricePanel(QWidget* parent) : Panel(QCoreApplication::translate("positioning", "PRICE"), parent) {
  m_chart = new pos::CandleChart(this);
  m_chart->setStyle(pos::CandleChart::cotStyle());
  m_mode = new pk::Segmented({QCoreApplication::translate("positioning", "Unweighted"), QCoreApplication::translate("positioning", "Futures")}, pk::Segmented::Gold, theme::mono(10), this, false);
  m_mode->setPadding(5, 16); m_mode->setGap(4); m_mode->setInactiveColor(theme::map(0x72, 0x70, 0x67));
  connect(m_mode, &pk::Segmented::changed, this, [this](int mode) { if (m_mode->isVisible()) m_fxMode = mode; emit modeChanged(mode); });
}
bool PricePanel::weighted() const { return m_mode->current() == 1; }
void PricePanel::setMarket(const positioning::CotMarket& m) {
  const bool fx = m.assetClass == "fx";
  m_mode->setVisible(fx);
  // a commodity or index shows its future; back on an FX market, the user's Unweighted / Futures choice returns
  m_mode->setCurrent(fx ? m_fxMode : 1);
  m_chart->setLoading(true);
  update();
}
void PricePanel::setHistory(const CotHistory& h) { m_h = h; update(); }
void PricePanel::setLoading() { m_chart->setLoading(true); m_bars.clear(); update(); }
void PricePanel::setBars(const QVector<market::Bar>& bars, int dec, const QString& note, const QString& timezone) {
  m_bars = bars; m_dec = dec; m_note = note;
  m_chart->setBars(bars, dec, false, timezone);
  m_chart->setVisibleCount(42);
  if (bars.isEmpty()) m_chart->setMessage(QCoreApplication::translate("positioning", "No daily candles (Yahoo Finance)"));
  update();
}
void PricePanel::setHover(const QPoint& p) { m_chart->setHover(m_chart->mapFrom(this, p)); }
void PricePanel::resizeEvent(QResizeEvent*) {
  const bool phone = m_phone;   // finger-sized Unweighted | Futures, still inside the 28 px title bar
  m_mode->setPadding(phone ? 8 : 5, phone ? 24 : 16);
  m_mode->move(width() - m_mode->sizeHint().width() - 9, phone ? 2 : 6);
  m_chart->setGeometry(1, 52, width() - 14, height() - 52 - 125);   // 318 at the reference height 495
}
void PricePanel::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  paintFrame(p);
  {  // subtitle in the title bar (proportional)
    const QFont tf = theme::mono(10, QFont::Bold, 0.6);
    const qreal x = 9 + QFontMetricsF(tf).horizontalAdvance(m_title) + 9;
    const QFont sf = theme::condensed(11, QFont::Medium);
    p.setFont(sf); p.setPen(theme::map(0x59, 0x56, 0x4f));
    const QString s = m_note;
    p.drawText(QRectF(x, 0, width() - x - (m_mode->isVisible() ? m_mode->width() + 20 : 10), kHead), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(sf).elidedText(s, Qt::ElideRight, int(width() - x - 160)));
  }
  if (m_bars.size() >= 2) {
    const auto& b = m_bars.last();
    const double prev = m_bars[m_bars.size() - 2].c, ch = (b.c / prev - 1) * 100;
    const QFont bf = theme::mono(18, QFont::Medium);
    const QString v = fmt::num(b.c, m_dec, false);
    p.setFont(bf); p.setPen(theme::map(0xff, 0xff, 0xfc)); p.drawText(QPointF(8, 52), v);
    qreal x = 8 + QFontMetricsF(bf).horizontalAdvance(v) + 10;
    const QColor c = ch >= 0 ? theme::map(kCyan) : theme::map(kRed);
    QPainterPath tri;
    if (ch >= 0) { tri.moveTo(x, 51); tri.lineTo(x + 12, 51); tri.lineTo(x + 6, 40); }
    else { tri.moveTo(x, 40); tri.lineTo(x + 12, 40); tri.lineTo(x + 6, 51); }
    p.fillPath(tri, c);
    p.setFont(theme::mono(11, QFont::Medium)); p.setPen(c); p.drawText(QPointF(x + 19, 51), fmt::pct(ch, 2));
  }
  // open interest strip (5 years, whatever the range)
  const qreal top = height() - 123;
  p.fillRect(QRectF(1, top, width() - 2, 1), theme::map(0x29, 0x29, 0x1b));
  if (m_h.weeks.isEmpty()) return;
  const auto& w = m_h.weeks.last();
  const QFont hf = theme::mono(9, QFont::Normal, 1.0);
  const QFontMetricsF hm(hf);
  qreal x = 9; const qreal cy = top + 11;
  p.fillRect(QRectF(x, cy - 3, 6, 6), theme::map(0xd8, 0xbc, 0x48)); x += 12;
  auto run = [&](const QString& t, const QColor& c) { p.setFont(hf); p.setPen(c); p.drawText(QPointF(x, cy + hm.capHeight() / 2), t); x += hm.horizontalAdvance(t); };
  run(QCoreApplication::translate("positioning", "OPEN INTEREST") + " ", theme::map(0x9c, 0x99, 0x90)); run(n0(w.oi) + " ", theme::map(0xe6, 0xe2, 0xd6));
  run(fmt::signedNum(w.oiChg, 0), theme::map(0xdd, 0xce, 0xa7)); run("  ·  " + QCoreApplication::translate("positioning", "CFTC WEEKLY") + "  ·  " + w.date.toString("yyyy-MM-dd"), theme::map(0x4d, 0x4a, 0x43));
  const QDate from = w.date.addYears(-5);
  QVector<const CotWeek*> ws; for (const auto& x2 : m_h.weeks) if (x2.date > from && std::isfinite(x2.oi)) ws << &x2;
  if (ws.size() < 2) return;
  double lo = INFINITY, hi = -INFINITY;
  for (auto* q : ws) { lo = std::min(lo, q->oi); hi = std::max(hi, q->oi); }
  const auto ticks = chart::niceTicks(lo * 0.9, hi * 1.02, 4);
  const double alo = ticks.isEmpty() ? lo : std::min(lo, ticks.first()), ahi = ticks.isEmpty() ? hi : std::max(hi, ticks.last());
  const QRectF pl(22, top + 22, width() - 22 - 75, 78);
  const qreal bw = pl.width() / ws.size();
  p.setRenderHint(QPainter::Antialiasing, false);
  for (int i = 0; i < ws.size(); ++i) {
    const double v = ws[i]->oi;
    const qreal h = (v - alo) / (ahi - alo) * pl.height();
    p.fillRect(QRectF(pl.left() + i * bw, pl.bottom() - h, std::max<qreal>(1, bw - 0.8), h), theme::mix(theme::map(0x9c, 0x88, 0x30), theme::map(0xc5, 0xba, 0x60), (v - lo) / std::max(1.0, hi - lo)));
  }
  p.setFont(theme::mono(11)); p.setPen(theme::map(0xa6, 0xa7, 0xa7));
  for (double t : ticks) {
    const qreal y = pl.bottom() - (t - alo) / (ahi - alo) * pl.height();
    if (y < pl.top() - 4) continue;
    p.drawText(QRectF(pl.right() + 10, y - 8, 60, 16), Qt::AlignLeft | Qt::AlignVCenter, fmt::compact(t, t >= 1e6 ? 1 : 0));
  }
  p.setFont(theme::condensed(13, QFont::Medium)); p.setPen(theme::map(0x97, 0x97, 0x97));
  qreal lastX = -1e9;
  for (int i = 0; i < ws.size(); ++i) {
    const QDate d = ws[i]->date;
    if (i > 0 && ws[i - 1]->date.month() == d.month()) continue;
    const qreal xx = pl.left() + i * bw;
    if (xx - lastX < 50 || xx > pl.right() - 10) continue;
    p.drawText(QRectF(xx - 30, pl.bottom() + 5, 60, 16), Qt::AlignCenter, d.toString("yyyy-MM"));
    lastX = xx;
  }
}

// ------------------------------------------------------------------------------------------------ Split
SplitPanel::SplitPanel(QWidget* parent) : Panel(QCoreApplication::translate("positioning", "WEEKLY LONG:SHORT SPLIT"), parent) {
  setMouseTracking(true);
  m_cats = new pk::Segmented({}, pk::Segmented::Gold, theme::condensed(11, QFont::Medium), this, false);
  m_cats->setPadding(5, 16); m_cats->setGap(1); m_cats->setInactiveColor(theme::map(0x61, 0x5f, 0x58));
  connect(m_cats, &pk::Segmented::changed, this, [this](int i) { m_cat = i; update(); emit categoryChanged(i); });
}
// phone: taller category buttons, the plot 10 px lower
QRectF SplitPanel::plot() const { const int top = m_phone ? 75 : 65; return QRectF(8, top, width() - 17, height() - top - 25); }
void SplitPanel::resizeEvent(QResizeEvent*) {
  m_cats->setPadding(5, m_phone ? 26 : 16);
  // short labels when the full names do not fit
  QStringList full; for (const auto& n : m_h.secondNames().mid(0, 4)) full << catTr(n);
  full << catTr("Nonreportable");
  if (full.size() == 5) {
    m_cats->setItems(full);
    if (m_cats->sizeHint().width() > width() - 12) {
      QStringList sh;
      QStringList src = m_h.secondNames().mid(0, 4); src << "Nonreportable";
      for (const auto& n : src) sh << (n.startsWith("Dealer") ? QCoreApplication::translate("positioning", "Dealer") : n.startsWith("Asset") ? QCoreApplication::translate("positioning", "Asset Mgr") : n.startsWith("Leveraged") ? QCoreApplication::translate("positioning", "Lev. Funds")
                                     : n.startsWith("Other") ? QCoreApplication::translate("positioning", "Other") : n.startsWith("Producer") ? QCoreApplication::translate("positioning", "Producer") : n.startsWith("Managed") ? QCoreApplication::translate("positioning", "Mgd Money")
                                     : n.startsWith("Nonrep") ? QCoreApplication::translate("positioning", "Nonrep.") : catTr(n));
      m_cats->setItems(sh);
    }
    m_cats->setCurrent(m_cat);
  }
  m_cats->resize(m_cats->sizeHint());
  m_cats->move((width() - m_cats->sizeHint().width()) / 2, 27);
}
void SplitPanel::setData(const CotHistory& h, int weeks, int cat) {
  m_h = h; m_weeks = weeks; m_cat = cat;
  setSubtitle(catTr(h.secondName()));
  resizeEvent(nullptr); update();
}
void SplitPanel::mouseMoveEvent(QMouseEvent* e) { setHover(e->position()); }
void SplitPanel::leaveEvent(QEvent*) { m_has = false; update(); }
void SplitPanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  paintFrame(p);
  const auto ws = lastWeeks(m_h, m_weeks);
  if (ws.isEmpty()) return;
  const QRectF pl = plot();
  const int n = ws.size();
  const qreal bw = pl.width() / n;
  const int ci = m_cat == 4 ? 5 : m_cat;   // the 5th button is Nonreportable (row 5 of the second report)
  int hov = -1;
  if (m_has && pl.contains(m_hover)) hov = qBound(0, int((m_hover.x() - pl.left()) / bw), n - 1);
  for (int i = 0; i < n; ++i) {
    if (ci >= ws[i].second.size()) continue;
    const double L = ws[i].second[ci].longPct();
    if (!std::isfinite(L)) continue;
    const QRectF col(pl.left() + i * bw, pl.top(), bw - 1, pl.height());
    const qreal ly = pl.bottom() - L / 100.0 * pl.height();
    p.fillRect(QRectF(col.left(), pl.top(), col.width(), ly - pl.top()), theme::map(0xe8, 0x60, 0x5c));
    p.fillRect(QRectF(col.left(), ly, col.width(), pl.bottom() - ly), theme::map(0x18, 0xd8, 0xf0));
  }
  if (hov >= 0 && ci < ws[hov].second.size()) {
    const QRectF col(pl.left() + hov * bw, pl.top(), bw - 1, pl.height());
    QPen dp(theme::map(255, 255, 255, 200), 1, Qt::DotLine); p.setPen(dp); p.setBrush(Qt::NoBrush);
    p.drawRect(col.adjusted(0.5, 0.5, -0.5, -0.5));
    const double L = ws[hov].second[ci].longPct();
    const QStringList lines{ws[hov].date.toString("yyyy-MM-dd"), QString("Long: %1%").arg(fmt::whole(L)), QString("Short: %1%").arg(fmt::whole(100 - L))};
    QRectF box(m_hover.x() + 22, m_hover.y() + 8, 88, 70);
    if (box.right() > width() - 4) box.moveRight(m_hover.x() - 12);
    if (box.bottom() > height() - 4) box.moveBottom(height() - 4);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(theme::map(0x3a, 0x30, 0x22), 1)); p.setBrush(theme::map(0x0c, 0x08, 0x04, 245)); p.drawRoundedRect(box, 3, 3);
    p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(theme::map(0xe0, 0xde, 0xd8));
    for (int k = 0; k < 3; ++k) p.drawText(QPointF(box.left() + 11, box.top() + 21 + k * 18), lines[k]);
  }
}

// ------------------------------------------------------------------------------------------------ Donut
DonutPanel::DonutPanel(QWidget* parent) : Panel(QCoreApplication::translate("positioning", "LONG:SHORT"), parent) {}
void DonutPanel::setData(const CotHistory& h, int cat) {
  m_long = NAN;
  if (h.weeks.isEmpty()) { update(); return; }
  const int ci = cat == 4 ? 5 : cat;
  const auto& w = h.weeks.last();
  if (ci < w.second.size()) {
    m_long = w.second[ci].longPct();
    setSubtitle(catUpper(w.second[ci].name) + " · " + w.date.toString("yyyy-MM-dd"));
  }
  update();
}
void DonutPanel::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  paintFrame(p);
  if (!std::isfinite(m_long)) return;
  // 104 px at the reference panel (294x495); grows with bigger panels on wide screens
  const qreal grow = std::max(1.0, std::min(width() / 294.0, height() / 495.0));
  const qreal rx = std::min<qreal>(104 * grow, (width() - 40) / 2.0), ry = rx * 0.53;
  const QPointF c(width() / 2.0, height() * 0.47);
  pk::drawPie3D(p, c, rx, ry, 20, m_long, 0.47);
  const bool lng = m_long >= 50;
  const int v = fmt::roundInt(lng ? m_long : 100 - m_long);
  text(p, QRectF(c.x() - 50, c.y() - 22, 100, 28), Qt::AlignCenter, QString("%1%").arg(v), theme::mono(int(21 * std::min(1.4, rx / 104)), QFont::Bold), lng ? theme::map(0x3a, 0xd8, 0xee) : theme::map(0xf0, 0x66, 0x66));
  text(p, QRectF(c.x() - 50, c.y() + 4, 100, 12), Qt::AlignCenter, lng ? QCoreApplication::translate("positioning", "LONG") : QCoreApplication::translate("positioning", "SHORT"), theme::mono(8), theme::map(0x5d, 0x60, 0x5f));
  // legend
  const QFont f = theme::mono(12, QFont::Bold);
  const QFontMetricsF fm(f);
  const QString a = QCoreApplication::translate("positioning", "Long %1%").arg(fmt::whole(m_long)), b = QCoreApplication::translate("positioning", "Short %1%").arg(fmt::whole(100 - m_long));
  const qreal w = 12 + fm.horizontalAdvance(a) + 16 + 12 + fm.horizontalAdvance(b);
  qreal x = (width() - w) / 2; const qreal y = height() * 0.715;
  p.setPen(Qt::NoPen); p.setBrush(theme::map(kCyan)); p.drawEllipse(QPointF(x + 4, y), 4, 4);
  text(p, QRectF(x + 12, y - 9, 200, 18), Qt::AlignVCenter | Qt::AlignLeft, a, f, theme::map(kCyan));
  x += 12 + fm.horizontalAdvance(a) + 16;
  p.setPen(Qt::NoPen); p.setBrush(theme::map(kRed)); p.drawEllipse(QPointF(x + 4, y), 4, 4);
  text(p, QRectF(x + 12, y - 9, 200, 18), Qt::AlignVCenter | Qt::AlignLeft, b, f, theme::map(kRed));
}

// ------------------------------------------------------------------------------------------------ Net charts
NetChart::NetChart(const QString& title, QWidget* parent) : Panel(title, parent) {}
void NetChart::setData(const CotHistory& h, int weeks, int which) { m_h = h; m_weeks = weeks; m_which = which; m_cache = {}; update(); }
void NetChart::paintEvent(QPaintEvent*) {
  if (cacheStale(m_cache, this)) { m_cache = makeCache(this); QPainter cp(&m_cache); render(cp); }
  QPainter p(this); p.drawPixmap(0, 0, m_cache);
}
void NetChart::render(QPainter& p) {
  p.setRenderHint(QPainter::Antialiasing);
  paintFrame(p);
  const auto ws = lastWeeks(m_h, m_weeks);
  if (ws.size() < 2) return;
  struct S { QString name; QColor c; bool second; int idx; };
  QList<S> ser;
  if (m_which == 0) ser = {{"Commercials", theme::map(kCyan), false, 0}, {"Large Speculators", theme::map(kYel), false, 1}, {"Small Traders", theme::map(kPur), false, 3}};
  else if (m_which == 1) { const auto n = m_h.secondNames(); ser = {{n[0], theme::map(kCyan), true, 0}, {n[1], theme::map(kYel), true, 1}, {n[2], theme::map(kPur), true, 2}, {n[3], theme::map(kGrn), true, 3}}; }
  else ser = {{"Nonreportable", theme::map(kCyan), true, 5}};
  // legend
  const QFont lf = theme::condensed(13, QFont::Medium);
  const QFontMetricsF lm(lf);
  // one centred line; a phone-wide panel breaks it into lines of what fits (the plot moves down by the extra lines)
  QList<QPair<int, qreal>> lines{{0, 0}};   // (first series of the line, width)
  for (int k = 0; k < ser.size(); ++k) {
    const qreal iw = 16 + lm.horizontalAdvance(catTr(ser[k].name)) + 14;
    if (m_phone && lines.last().second > 0 && lines.last().second + iw > width() - 16) lines << qMakePair(k, 0.0);
    lines.last().second += iw;
  }
  const qreal extra = (lines.size() - 1) * 20;
  int line = 0;
  qreal x = (width() - lines[0].second) / 2, ly = kHead + 16;
  for (int k = 0; k < ser.size(); ++k) {
    const auto& s = ser[k];
    if (line + 1 < lines.size() && lines[line + 1].first == k) { ++line; x = (width() - lines[line].second) / 2; ly += 20; }
    const QPointF c(x + 5, ly);
    p.setPen(Qt::NoPen); p.setBrush(theme::alpha(s.c, 60)); p.drawEllipse(c, 7, 4.5);
    QPolygonF d{{c.x() - 5, c.y()}, {c.x(), c.y() - 3.5}, {c.x() + 5, c.y()}, {c.x(), c.y() + 3.5}};
    p.setBrush(s.c); p.drawPolygon(d);
    p.setBrush(theme::map(0x10, 0x0c, 0x06)); p.drawEllipse(c, 1.4, 1.4);
    text(p, QRectF(x + 15, ly - 9, 300, 18), Qt::AlignVCenter | Qt::AlignLeft, catTr(s.name), lf, theme::map(0xa7, 0xa4, 0x9e));
    x += 16 + lm.horizontalAdvance(catTr(s.name)) + 14;
  }
  QVector<QVector<double>> vals;
  double lo = INFINITY, hi = -INFINITY;
  for (const auto& s : ser) {
    QVector<double> v;
    for (const auto& w : ws) { const auto& cats = s.second ? w.second : w.legacy; v << (s.idx < cats.size() ? cats[s.idx].net() : NAN); }
    for (double d : v) if (std::isfinite(d)) { lo = std::min(lo, d); hi = std::max(hi, d); }
    vals << v;
  }
  if (!std::isfinite(lo)) return;
  lo = std::min(lo, 0.0); hi = std::max(hi, 0.0);
  auto ticks = chart::niceTicks(lo, hi, 6);
  if (ticks.isEmpty()) return;
  const double step = ticks.size() > 1 ? ticks[1] - ticks[0] : 1;
  if (ticks.first() > lo) ticks.prepend(ticks.first() - step);
  if (ticks.last() < hi) ticks.append(ticks.last() + step);
  lo = ticks.first(); hi = ticks.last();
  const QRectF pl(72, kHead + 38 + extra, width() - 72 - 16, height() - kHead - 38 - 30 - extra);
  auto Y = [&](double v) { return pl.bottom() - (v - lo) / (hi - lo) * pl.height(); };
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::condensed(13, QFont::Medium));
  for (double t : ticks) {
    const qreal y = Y(t);
    p.setPen(QPen(t == 0 ? theme::map(0x3a, 0x36, 0x20) : theme::map(0x20, 0x1f, 0x14), 1)); p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y));
    p.setPen(theme::map(0x70, 0x6d, 0x66));
    p.drawText(QRectF(4, y - 8, pl.left() - 12, 16), Qt::AlignRight | Qt::AlignVCenter, fmt::num(t, 0));
  }
  const int n = ws.size();
  auto X = [&](int i) { return pl.left() + pl.width() * i / (n - 1); };
  const QLocale loc = i18n::locale();
  const bool narrow = m_phone;   // phone: month labels closer than 34 px are skipped (they ran into each other)
  qreal lastLabel = -1e9;
  for (int i = 1; i < n; ++i) {
    if (ws[i].date.month() == ws[i - 1].date.month()) continue;
    if (narrow && X(i) - lastLabel < 34) continue;
    lastLabel = X(i);
    const bool jan = ws[i].date.month() == 1;
    QFont f = theme::condensed(13, jan ? QFont::Bold : QFont::Medium);
    QString t = jan ? QString::number(ws[i].date.year()) : loc.monthName(ws[i].date.month(), QLocale::ShortFormat).remove('.');
    if (!jan && !t.isEmpty()) t[0] = t[0].toUpper();
    p.setPen(QPen(theme::map(0x2a, 0x28, 0x1a), 1)); p.drawLine(QPointF(X(i), pl.bottom()), QPointF(X(i), pl.bottom() + 4));
    text(p, QRectF(X(i) - 30, pl.bottom() + 6, 60, 18), Qt::AlignCenter, t, f, jan ? theme::map(0x9a, 0x97, 0x8e) : theme::map(0x70, 0x6d, 0x66));
  }
  p.setRenderHint(QPainter::Antialiasing);
  for (int k = 0; k < ser.size(); ++k) {
    QPainterPath path; bool st = false;
    for (int i = 0; i < n; ++i) {
      if (!std::isfinite(vals[k][i])) continue;
      const QPointF q(X(i), Y(vals[k][i]));
      if (!st) { path.moveTo(q); st = true; } else path.lineTo(q);
    }
    p.setBrush(Qt::NoBrush);
    if (theme::isDark()) { p.setPen(QPen(theme::alpha(ser[k].c, 40), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path); }
    p.setPen(QPen(theme::mix(ser[k].c, theme::map(0x9a, 0x9a, 0x90), 0.15), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path);
  }
}

// ------------------------------------------------------------------------------------------------ Report table
ReportTable::ReportTable(QWidget* parent, bool second) : Panel(second ? QCoreApplication::translate("positioning", "TFF REPORT COT") : QCoreApplication::translate("positioning", "LEGACY REPORT COT"), parent), m_second(second) {}
void ReportTable::setData(const CotHistory& h) {
  m_h = h;
  if (m_second) setTitle(QCoreApplication::translate("positioning", "%1 REPORT COT").arg(catTr(h.secondName())));
  update();
}
void ReportTable::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  paintFrame(p);
  if (m_h.weeks.isEmpty()) return;
  const auto& w = m_h.weeks.last();
  {  // OI · traders, right of the title
    const QFont f = theme::mono(11); const QFontMetricsF fm(f);
    struct R { QString t; QColor c; };
    const QList<R> rs{{QCoreApplication::translate("positioning", "OI") + " ", theme::map(0x4c, 0x4b, 0x45)}, {n0(w.oi) + " ", theme::map(0x6c, 0x69, 0x62)},
                      {fmt::signedNum(w.oiChg, 0), w.oiChg < 0 ? theme::map(kNeg) : theme::map(kPos)}, {"  ·  " + QCoreApplication::translate("positioning", "%1 traders").arg(n0(w.traders)), theme::map(0x4c, 0x4b, 0x45)}};
    qreal tw = 0; for (const auto& r : rs) tw += fm.horizontalAdvance(r.t);
    qreal x = width() - 9 - tw;
    for (const auto& r : rs) { text(p, QRectF(x, 0, 400, kHead), Qt::AlignVCenter | Qt::AlignLeft, r.t, f, r.c); x += fm.horizontalAdvance(r.t); }
  }
  const auto& cats = m_second ? w.second : w.legacy;
  auto chgCol = [](double v) { return !std::isfinite(v) ? theme::map(kDim) : v < 0 ? theme::map(kNeg) : theme::map(kPos); };
  auto chgTxt = [](double v) { return std::isfinite(v) ? fmt::num(v, 0) : fmt::dash(); };
  auto dimIf = [&](double v) { return std::isfinite(v) ? theme::map(kNum) : theme::map(kDim); };
  if (m_phone) {   // phone: name on its own line, five numbers under it (see kPhonePitch)
    const QFont h1 = theme::mono(8, QFont::Bold, 1.0), h2 = theme::mono(7, QFont::Normal, 0.6);
    const qreal cw = (width() - 18) / 5.0;
    auto E = [&](int i) { return 9 + cw * (i + 1); };
    text(p, QRectF(9, 30, 200, 14), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("positioning", "CATEGORY"), h1, theme::map(0x84, 0x80, 0x76));
    text(p, QRectF(E(1) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "LONG"), h1, theme::map(kCyan));
    text(p, QRectF(E(3) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "SHORT"), h1, theme::map(kRed));
    text(p, QRectF(E(4) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "NET"), h1, theme::map(0x84, 0x80, 0x76));
    const QString pos = QCoreApplication::translate("positioning", "POSITIONS"), chg = QCoreApplication::translate("positioning", "CHG");
    const QStringList sub{pos, chg, pos, chg, QCoreApplication::translate("positioning", "L-S")};
    for (int i = 0; i < 5; ++i) text(p, QRectF(E(i) - cw + 4, 50, cw - 4, 14), Qt::AlignVCenter | Qt::AlignRight, sub[i], h2, theme::map(kDim));
    const QFont lf = theme::condensed(13, QFont::Medium), nf = theme::mono(12);
    for (int r = 0; r < cats.size(); ++r) {
      const auto& c = cats[r];
      const qreal y = 66 + r * kPhonePitch;
      p.fillRect(QRectF(1, y, width() - 2, 1), theme::map(0x24, 0x24, 0x15));
      text(p, QRectF(9, y + 2, width() - 18, 18), Qt::AlignVCenter | Qt::AlignLeft, catTr(c.name), lf, theme::map(kRowText));
      auto cell = [&](int i, const QString& t, const QColor& col) { text(p, QRectF(E(i) - cw, y + 20, cw, 20), Qt::AlignVCenter | Qt::AlignRight, t, nf, col); };
      cell(0, n0(c.lng.pos), dimIf(c.lng.pos)); cell(1, chgTxt(c.lng.chg), chgCol(c.lng.chg));
      cell(2, n0(c.sht.pos), dimIf(c.sht.pos)); cell(3, chgTxt(c.sht.chg), chgCol(c.sht.chg));
      const double net = c.net();
      cell(4, chgTxt(net), chgCol(net));
    }
    return;
  }
  static const double kEdges[10] = {198, 271, 326, 376, 449, 522, 577, 627, 698, 781};
  const qreal sx = width() / 788.0;
  auto E = [&](int i) { return kEdges[i] * sx; };
  const QFont h1 = theme::mono(8, QFont::Bold, 1.0), h2 = theme::mono(7, QFont::Normal, 0.6);
  text(p, QRectF(9, 30, 200, 14), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("positioning", "CATEGORY"), h1, theme::map(0x84, 0x80, 0x76));
  text(p, QRectF(E(3) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "LONG"), h1, theme::map(kCyan));
  text(p, QRectF(E(7) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "SHORT"), h1, theme::map(kRed));
  text(p, QRectF(E(8) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "SPREAD"), h1, theme::map(0x84, 0x80, 0x76));
  text(p, QRectF(E(9) - 100, 30, 100, 14), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "NET"), h1, theme::map(0x84, 0x80, 0x76));
  const QString pos = QCoreApplication::translate("positioning", "POSITIONS"), chg = QCoreApplication::translate("positioning", "CHG"), poi = QCoreApplication::translate("positioning", "%OI"), trd = QCoreApplication::translate("positioning", "TRDRS");
  const QStringList sub{pos, chg, poi, trd, pos, chg, poi, trd, pos, QCoreApplication::translate("positioning", "L-S")};
  for (int i = 0; i < 10; ++i) text(p, QRectF(E(i) - 90, 50, 90, 14), Qt::AlignVCenter | Qt::AlignRight, sub[i], h2, theme::map(kDim));
  const QFont lf = theme::condensed(13, QFont::Medium), nf = theme::mono(12);
  const qreal top = 66, pitch = 25.7;
  for (int r = 0; r < cats.size(); ++r) {
    const auto& c = cats[r];
    const qreal y = top + r * pitch;
    p.fillRect(QRectF(1, y, width() - 2, 1), theme::map(0x24, 0x24, 0x15));
    const QRectF row(0, y, width(), pitch);
    text(p, row.adjusted(9, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, catTr(c.name), lf, theme::map(kRowText));
    auto cell = [&](int i, const QString& t, const QColor& col) { text(p, QRectF(E(i) - 100, y, 100, pitch), Qt::AlignVCenter | Qt::AlignRight, t, nf, col); };
    auto chgCol = [](double v) { return !std::isfinite(v) ? theme::map(kDim) : v < 0 ? theme::map(kNeg) : theme::map(kPos); };
    auto chg = [](double v) { return std::isfinite(v) ? fmt::num(v, 0) : fmt::dash(); };
    auto pct = [](double v) { return std::isfinite(v) ? fmt::num(v, 1) : fmt::dash(); };
    auto dimIf = [&](double v) { return std::isfinite(v) ? theme::map(kNum) : theme::map(kDim); };
    cell(0, n0(c.lng.pos), dimIf(c.lng.pos)); cell(1, chg(c.lng.chg), chgCol(c.lng.chg)); cell(2, pct(c.lng.pctOi), dimIf(c.lng.pctOi)); cell(3, n0(c.lng.traders), dimIf(c.lng.traders));
    cell(4, n0(c.sht.pos), dimIf(c.sht.pos)); cell(5, chg(c.sht.chg), chgCol(c.sht.chg)); cell(6, pct(c.sht.pctOi), dimIf(c.sht.pctOi)); cell(7, n0(c.sht.traders), dimIf(c.sht.traders));
    cell(8, n0(c.spread), dimIf(c.spread));
    const double net = c.net();
    cell(9, chg(net), chgCol(net));
  }
}

// ------------------------------------------------------------------------------------------------ Sector donuts
SectorPanel::SectorPanel(QWidget* parent, bool second) : Panel(second ? QCoreApplication::translate("positioning", "TFF LONG:SHORT SECTOR") : QCoreApplication::translate("positioning", "LEGACY LONG:SHORT SECTOR"), parent), m_second(second) {}
void SectorPanel::setData(const CotHistory& h) {
  m_h = h; m_cache = {};
  if (m_second) setTitle(QCoreApplication::translate("positioning", "%1 LONG:SHORT SECTOR").arg(catTr(h.secondName())));
  update();
}
void SectorPanel::paintEvent(QPaintEvent*) {
  if (cacheStale(m_cache, this)) {
    m_cache = makeCache(this); QPainter p(&m_cache); p.setRenderHint(QPainter::Antialiasing);
    paintFrame(p);
    if (!m_h.weeks.isEmpty()) {
      const auto& w = m_h.weeks.last();
      QList<const CotCategory*> cs;
      if (m_second) { for (int i : {0, 1, 2, 3, 5}) if (i < w.second.size()) cs << &w.second[i]; }
      else { for (int i : {0, 1, 3}) if (i < w.legacy.size()) cs << &w.legacy[i]; }
      const qreal pitch = 145, rx = std::min<qreal>(55 * std::clamp(width() / 391.0, 1.0, 1.2), width() / 7.2), ry = rx * 0.52;
      for (int k = 0; k < cs.size(); ++k) {
        const int row = k / 2; const bool last = k == cs.size() - 1 && cs.size() % 2 == 1;
        const qreal cx = last ? width() * 0.5 : (k % 2 == 0 ? width() * 0.256 : width() * 0.74);
        const qreal cy = 80 + row * pitch;
        const double L = cs[k]->longPct();
        pk::drawPie3D(p, QPointF(cx, cy), rx, ry, 12, L, 0.45, 0.8);
        const QFont f = theme::mono(12, QFont::Bold); const QFontMetricsF fm(f);
        const QString a = QCoreApplication::translate("positioning", "L %1%").arg(fmt::whole(L)), b = QCoreApplication::translate("positioning", "S %1%").arg(fmt::whole(100 - L));
        const qreal tw = fm.horizontalAdvance(a) + 10 + fm.horizontalAdvance(b);
        text(p, QRectF(cx - tw / 2, cy + 56, 200, 18), Qt::AlignVCenter | Qt::AlignLeft, a, f, theme::map(kCyan));
        text(p, QRectF(cx - tw / 2 + fm.horizontalAdvance(a) + 10, cy + 56, 200, 18), Qt::AlignVCenter | Qt::AlignLeft, b, f, theme::map(kRed));
        text(p, QRectF(cx - 100, cy + 76, 200, 14), Qt::AlignCenter, catUpper(cs[k]->name), theme::mono(9, QFont::Normal, 0.8), theme::map(0x5d, 0x5a, 0x54));
      }
    }
  }
  QPainter p(this); p.drawPixmap(0, 0, m_cache);
}

// ------------------------------------------------------------------------------------------------ Stats bars
StatsPanel::StatsPanel(QWidget* parent, bool second) : Panel(second ? QCoreApplication::translate("positioning", "TFF STATS") : QCoreApplication::translate("positioning", "LEGACY STATS"), parent), m_second(second) {}
void StatsPanel::setData(const CotHistory& h) { m_h = h; if (m_second) setTitle(QCoreApplication::translate("positioning", "%1 STATS").arg(catTr(h.secondName()))); update(); }
void StatsPanel::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  paintFrame(p);
  if (m_h.weeks.isEmpty()) return;
  const auto& w = m_h.weeks.last();
  QList<const CotCategory*> cs;
  if (m_second) { for (int i : {0, 1, 2, 3, 5}) if (i < w.second.size()) cs << &w.second[i]; }
  else { for (int i : {0, 1, 3}) if (i < w.legacy.size()) cs << &w.legacy[i]; }
  const qreal x0 = width() * 0.404, x1 = width() * 0.823, mid = (x0 + x1) / 2;
  for (int k = 0; k < cs.size(); ++k) {
    const qreal cy = 49 + k * 32.5;
    text(p, QRectF(10, cy - 9, x0 - 14, 18), Qt::AlignVCenter | Qt::AlignLeft, catTr(cs[k]->name), theme::condensed(13, QFont::Medium), theme::map(0x61, 0x5f, 0x59));
    p.fillRect(QRectF(x0, cy - 7, x1 - x0, 14), theme::map(0x1b, 0x1a, 0x14));
    const double v = cs[k]->netPct();
    if (!std::isfinite(v)) continue;
    const qreal len = (x1 - x0) / 2 * std::min(1.0, std::abs(v) / 100.0);
    const QColor c = v < 0 ? theme::map(0xed, 0x61, 0x5e) : theme::map(0x29, 0xd9, 0xea);
    p.fillRect(v < 0 ? QRectF(mid - len, cy - 7, len, 14) : QRectF(mid, cy - 7, len, 14), c);
    text(p, QRectF(x1, cy - 10, width() - 9 - x1, 20), Qt::AlignVCenter | Qt::AlignRight, QString("%1%2%").arg(v >= 0 ? "+" : "-").arg(fmt::whole(std::abs(v))),
         theme::mono(13, QFont::Bold), c);
  }
}

// ------------------------------------------------------------------------------------------------ Automated read
namespace {
struct Cat { QString label, en, fr; double net, prev, p; double L; };
QString nm(double v) { return fmt::num(std::abs(v), 0); }
QString cap(QString s) { if (!s.isEmpty()) s[0] = s[0].toUpper(); return s; }

QList<Cat> readCats(const CotHistory& h) {
  QList<Cat> out;
  if (h.weeks.size() < 2) return out;
  const auto& w = h.weeks.last(); const auto& pw = h.weeks[h.weeks.size() - 2];
  struct D { bool second; int idx; const char* en; const char* fr; };
  QList<D> ds{{false, 0, "commercials", "les commerciaux"}, {false, 1, "large speculators", "les grands spéculateurs"},
              {false, 3, "small traders", "les petits porteurs"}};
  if (h.market.second == positioning::CotReport::Tff)
    ds += QList<D>{{true, 0, "dealers", "les dealers"}, {true, 1, "asset managers", "les gérants d'actifs"},
                   {true, 2, "leveraged funds", "les fonds à effet de levier"}, {true, 3, "other reportables", "les autres déclarants"}};
  else
    ds += QList<D>{{true, 0, "producers/merchants", "les producteurs/négociants"}, {true, 1, "swap dealers", "les swap dealers"},
                   {true, 2, "managed money", "la gestion alternative"}, {true, 3, "other reportables", "les autres déclarants"}};
  for (const auto& d : ds) {
    const auto& c = d.second ? w.second : w.legacy; const auto& pc = d.second ? pw.second : pw.legacy;
    if (d.idx >= c.size()) continue;
    Cat x;
    x.label = (d.second ? h.secondName() == "TFF" ? QString("TFF") : QCoreApplication::translate("positioning", "Disagg.") : QCoreApplication::translate("positioning", "Legacy")) + " · " + catTr(c[d.idx].name);
    x.en = d.en; x.fr = d.fr; x.net = c[d.idx].net(); x.prev = d.idx < pc.size() ? pc[d.idx].net() : NAN;
    x.p = h.rangePosition(d.second, d.idx, 52); x.L = c[d.idx].longPct();
    out << x;
  }
  return out;
}
QString moveEn(const Cat& c) {
  const double d = c.net - c.prev;
  if (!std::isfinite(d) || d == 0) return "unchanged on the week";
  if (d > 0) return c.net > 0 ? "added " + nm(d) + " net longs" : "cut " + nm(d) + " net shorts";
  return c.net < 0 ? "added " + nm(d) + " net shorts" : "cut " + nm(d) + " net longs";
}
QString moveFr(const Cat& c) {
  const double d = c.net - c.prev;
  if (!std::isfinite(d) || d == 0) return "stable sur la semaine";
  if (d > 0) return c.net > 0 ? "+" + nm(d) + " contrats nets acheteurs" : nm(d) + " contrats vendeurs nets rachetés";
  return c.net < 0 ? "+" + nm(d) + " contrats nets vendeurs" : nm(d) + " contrats acheteurs nets allégés";
}
}

CotRead writeRead(const CotHistory& h, int lang) {
  CotRead r;
  auto cats = readCats(h);
  if (cats.isEmpty()) return r;
  const bool fr = lang == 1;
  QList<Cat> byExt = cats;
  std::sort(byExt.begin(), byExt.end(), [](const Cat& a, const Cat& b) { return std::abs((std::isfinite(a.p) ? a.p : 50) - 50) > std::abs((std::isfinite(b.p) ? b.p : 50) - 50); });
  QList<Cat> crowded; for (const auto& c : byExt) if (std::isfinite(c.p) && (c.p >= 90 || c.p <= 10)) crowded << c;
  QList<Cat> byMove = cats;
  // a non-finite move sorts last (-1): with NaN keys the comparator is not a strict weak ordering (undefined behaviour)
  auto move = [](const Cat& c) { const double m = std::abs(c.net - c.prev); return std::isfinite(m) ? m : -1.0; };
  std::sort(byMove.begin(), byMove.end(), [&](const Cat& a, const Cat& b) { return move(a) > move(b); });
  const Cat& mv = byMove.first();
  auto hiLo = [&](const Cat& c) { return c.p >= 90 ? (fr ? "plus haut" : "high") : (fr ? "plus bas" : "low"); };
  auto name = [&](const Cat& c) { return fr ? c.fr : c.en; };
  if (crowded.size() >= 2)
    r.headline = fr ? cap(name(crowded[0])) + " et " + name(crowded[1]) + " aux extrêmes sur 52 semaines"
                    : cap(name(crowded[0])) + " and " + name(crowded[1]) + " at 52-week extremes";
  else if (crowded.size() == 1)
    r.headline = fr ? cap(name(crowded[0])) + " au " + hiLo(crowded[0]) + " de 52 semaines, " + name(mv) + " : " + moveFr(mv)
                    : cap(name(crowded[0])) + " at a 52-week " + hiLo(crowded[0]) + ", " + name(mv) + " " + moveEn(mv);
  else
    r.headline = fr ? cap(name(mv)) + " : " + moveFr(mv) + ", aucun camp à l'extrême"
                    : cap(name(mv)) + " " + moveEn(mv) + ", no camp at an extreme";
  QStringList sent;
  for (int k = 0; k < std::min<int>(2, byExt.size()); ++k) {
    const auto& c = byExt[k];
    const QString side = fr ? (c.net >= 0 ? "acheteurs nets de " : "vendeurs nets de ") : (c.net >= 0 ? "net long " : "net short ");
    const QString pos = std::isfinite(c.p) ? fmt::whole(c.p) + "%" : fmt::dash();
    sent << (fr ? cap(name(c)) + " sont " + side + nm(c.net) + " contrats, à " + pos + " de leur fourchette sur 52 semaines (" + moveFr(c) + ")."
                : cap(name(c)) + " are " + side + nm(c.net) + " contracts, at " + pos + " of their 52-week range (" + moveEn(c) + ").");
  }
  const auto& w = h.weeks.last();
  if (std::isfinite(w.oiChg))
    sent << (fr ? QString("L'open interest %1 de %2 à %3.").arg(w.oiChg >= 0 ? "progresse" : "recule", nm(w.oiChg), fmt::num(w.oi, 0))
                : QString("Open interest %1 %2 to %3.").arg(w.oiChg >= 0 ? "rose" : "fell", nm(w.oiChg), fmt::num(w.oi, 0)));
  r.paragraph = sent.join(' ');
  {
    const auto& c = byExt.first();
    const QString where = c.p >= 90 ? (fr ? "proche de son niveau le plus acheteur de l'année" : "close to its most net-long level of the year")
                        : c.p <= 10 ? (fr ? "proche de son niveau le plus vendeur de l'année" : "close to its most net-short level of the year")
                                    : (fr ? "en milieu de fourchette" : "mid-range");
    r.bullets << (fr ? cap(name(c)) + " à " + fmt::whole(c.p) + "% de la fourchette 52 semaines, " + where
                     : cap(name(c)) + " at " + fmt::whole(c.p) + "% of the 52-week range, " + where);
  }
  r.bullets << (fr ? "Plus gros mouvement de la semaine : " + name(mv) + ", " + moveFr(mv) + " (net " + fmt::num(mv.prev, 0) + " → " + fmt::num(mv.net, 0) + ")"
                   : "Biggest weekly move: " + name(mv) + " " + moveEn(mv) + " (net " + fmt::num(mv.prev, 0) + " → " + fmt::num(mv.net, 0) + ")");
  {
    const Cat* lev = nullptr; for (const auto& c : cats) if (c.en == "leveraged funds" || c.en == "managed money") lev = &c;
    if (lev && std::isfinite(lev->L))
      r.bullets << (fr ? cap(name(*lev)) + " : " + fmt::whole(lev->L) + "% de leurs positions à l'achat, " + fmt::whole(100 - lev->L) + "% à la vente"
                       : cap(name(*lev)) + " hold " + fmt::whole(lev->L) + "% of their positions long, " + fmt::whole(100 - lev->L) + "% short");
  }
  return r;
}

ReadPanel::ReadPanel(QWidget* parent) : Panel(QCoreApplication::translate("positioning", "AUTOMATED READ"), parent) {
  m_langSel = new pk::Segmented({"EN", "FR"}, pk::Segmented::Yellow, theme::mono(9, QFont::Bold), this, true);
  m_langSel->setPadding(5, 15);
  connect(m_langSel, &pk::Segmented::changed, this, [this](int i) { m_lang = i; update(); emit languageChanged(i); });
}
void ReadPanel::setLanguage(int l) { m_lang = l; m_langSel->setCurrent(l); update(); }
void ReadPanel::setData(const CotHistory& h) { m_h = h; update(); }
void ReadPanel::resizeEvent(QResizeEvent*) {
  m_langSel->setPadding(m_phone ? 8 : 5, m_phone ? 22 : 15);   // phone: finger-sized EN | FR
  m_langSel->resize(m_langSel->sizeHint()); m_langSel->move(width() - m_langSel->width() - 8, m_phone ? 0 : 3);
}
void ReadPanel::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  // frame with the sparkle icon before the title
  m_title = "    " + QCoreApplication::translate("positioning", "AUTOMATED READ");   // room for the sparkle icon
  paintFrame(p);
  {
    const QPointF c(15, 14); const QColor y = theme::map(0xec, 0xdc, 0x90);
    QPainterPath s; s.moveTo(c.x(), c.y() - 6); s.quadTo(c.x(), c.y(), c.x() + 6, c.y()); s.quadTo(c.x(), c.y(), c.x(), c.y() + 6);
    s.quadTo(c.x(), c.y(), c.x() - 6, c.y()); s.quadTo(c.x(), c.y(), c.x(), c.y() - 6);
    p.fillPath(s, y);
    p.setPen(QPen(y, 1)); p.drawLine(QPointF(c.x() + 5, c.y() - 6), QPointF(c.x() + 8, c.y() - 6)); p.drawLine(QPointF(c.x() + 6.5, c.y() - 7.5), QPointF(c.x() + 6.5, c.y() - 4.5));
  }
  if (m_h.weeks.size() < 2) return;
  const auto& w = m_h.weeks.last();
  const QString src = (m_lang == 1 ? "RAPPORT DU " : "REPORT OF ") + w.date.toString("yyyy-MM-dd") +
                      (m_lang == 1 ? " · RÈGLES APPLIQUÉES AUX CHIFFRES CFTC" : " · RULES APPLIED TO THE CFTC FIGURES");
  // phone: the texts take the full width and the range bars go under them
  const bool phone = m_phone;
  if (phone) {
    const QFont sf = theme::mono(9, QFont::Normal, 0.8);
    const qreal x0 = 9 + QFontMetricsF(theme::mono(10, QFont::Bold, 0.6)).horizontalAdvance(m_title) + 12;
    text(p, QRectF(x0, 0, std::max<qreal>(0, m_langSel->x() - 10 - x0), kHead), Qt::AlignVCenter | Qt::AlignRight,
         QFontMetricsF(sf).elidedText(src, Qt::ElideRight, std::max<qreal>(0, m_langSel->x() - 10 - x0)), sf, theme::map(0x45, 0x43, 0x3b));
  } else {
    text(p, QRectF(0, 0, m_langSel->x() - 10, kHead), Qt::AlignVCenter | Qt::AlignRight, src, theme::mono(9, QFont::Normal, 0.8), theme::map(0x45, 0x43, 0x3b));
  }
  const CotRead r = writeRead(m_h, m_lang);
  const qreal lw = phone ? width() - 18 : width() * 0.505 - 9;
  qreal y = kHead + 10;
  {
    const QFont hf = theme::condensed(16, QFont::Bold);
    QRectF br = p.boundingRect(QRectF(9, y, lw, 200), Qt::TextWordWrap, r.headline);
    p.setFont(hf); br = p.boundingRect(QRectF(9, y, lw, 200), Qt::TextWordWrap, r.headline);
    p.setPen(theme::map(0xfc, 0xfa, 0xf3)); p.drawText(QRectF(9, y, lw, br.height()), Qt::TextWordWrap, r.headline);
    y += br.height() + 6;
  }
  const QFont pf = theme::condensed(13, QFont::Medium);
  p.setFont(pf);
  auto para = [&](const QString& t, qreal x, qreal width, const QColor& c) {
    QTextLayout tl(t, pf); tl.beginLayout();
    qreal yy = 0; QList<QTextLine> lines;
    for (;;) { QTextLine l = tl.createLine(); if (!l.isValid()) break; l.setLineWidth(width); l.setPosition(QPointF(0, yy)); yy += 17.5; lines << l; }
    tl.endLayout();
    p.setPen(c); tl.draw(&p, QPointF(x, y));
    return yy;
  };
  y += para(r.paragraph, 9, lw, theme::map(0x8c, 0x88, 0x81)) + 6;
  for (const auto& b : r.bullets) {
    const qreal h = para(b, 19, lw - 10, theme::map(0x8c, 0x88, 0x81));
    p.fillRect(QRectF(9, y, 2, h), theme::map(0x38, 0x32, 0x18));
    y += h + 3;
  }
  const auto cats = readCats(m_h);
  // phone: the range block starts under the texts (oy shifts its rows from their reference y under the title bar)
  const qreal oy = phone ? y + 10 - kHead : 0;
  const qreal endY = phone ? kHead + 32 + cats.size() * 21.5 + oy + 8 : y;
  if (const int need = int(std::ceil(endY + 10)); need != m_needed) { m_needed = need; emit neededHeightChanged(); }
  // right column: net position vs its 52-week range
  const qreal rx = phone ? 9 : width() * 0.53, rr = width() - 9;
  text(p, QRectF(rx, oy + kHead + 8, rr - rx, 14), Qt::AlignVCenter | Qt::AlignLeft,
       m_lang == 1 ? "POSITION NETTE DANS SA FOURCHETTE 52 SEMAINES" : "NET POSITION VS ITS 52-WEEK RANGE", theme::mono(8, QFont::Normal, 0.6), theme::map(0x7c, 0x7a, 0x70));
  const qreal tx0 = rr - (rr - rx) * 0.33, tx1 = rr;
  for (int k = 0; k < cats.size(); ++k) {
    const qreal cy = oy + kHead + 32 + k * 21.5;
    text(p, QRectF(rx, cy - 9, tx0 - rx, 18), Qt::AlignVCenter | Qt::AlignLeft, cats[k].label, theme::condensed(13, QFont::Medium), theme::map(0x8b, 0x88, 0x81));
    text(p, QRectF(rx, cy - 9, tx0 - rx - 10, 18), Qt::AlignVCenter | Qt::AlignRight, fmt::signedNum(cats[k].net, 0), theme::mono(12, QFont::Medium),
         cats[k].net < 0 ? theme::map(0xe8, 0x62, 0x60) : theme::map(0x3a, 0xcf, 0xe6));
    p.setPen(Qt::NoPen); p.setBrush(theme::map(0x20, 0x20, 0x18)); p.drawRoundedRect(QRectF(tx0, cy - 3, tx1 - tx0, 6), 3, 3);
    if (std::isfinite(cats[k].p)) {
      const bool hot = cats[k].p >= 90 || cats[k].p <= 10;
      const qreal x = tx0 + 2 + (tx1 - tx0 - 4) * cats[k].p / 100.0;
      if (hot) { p.setBrush(theme::map(0xef, 0xdd, 0x7c, 70)); p.drawRoundedRect(QRectF(x - 4, cy - 9, 8, 18), 3, 3); }
      p.setBrush(hot ? theme::map(0xef, 0xdd, 0x7c) : theme::map(0xe5, 0xe5, 0xe6)); p.drawRoundedRect(QRectF(x - 2, cy - 7, 4, 14), 1.5, 1.5);
    }
  }
  const qreal fy = oy + kHead + 32 + cats.size() * 21.5;
  const QFont ff = theme::mono(8, QFont::Normal, 0.4);
  text(p, QRectF(rx, fy - 6, 200, 14), Qt::AlignVCenter | Qt::AlignLeft, m_lang == 1 ? "BAS 52 SEM." : "52-WEEK LOW", ff, theme::map(0x3c, 0x3a, 0x33));
  text(p, QRectF(rx, fy - 6, rr - rx, 14), Qt::AlignVCenter | Qt::AlignRight, m_lang == 1 ? "HAUT 52 SEM." : "52-WEEK HIGH", ff, theme::map(0x3c, 0x3a, 0x33));
  text(p, QRectF(rx, fy - 6, rr - rx, 14), Qt::AlignCenter, m_lang == 1 ? "● EXTRÊME (10% HAUT / BAS)" : "● CROWDED (TOP / BOTTOM 10%)", ff, theme::map(0xcc, 0xb8, 0x80));
}
}
