#include "pages/positioning/RetailModal.h"
#include "pages/positioning/PosKit.h"
#include "pages/positioning/CandleChart.h"
#include "positioning/Cot.h"
#include "market/QuoteHub.h"
#include "market/Instruments.h"
#include "market/Candles.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QTimer>
#include <QTimeZone>
#include <QTextLayout>
#include <QToolTip>
#include <QHelpEvent>
#include <cmath>

using positioning::Retail;
using positioning::RetailSymbol;

namespace positioning {
namespace {
const QColor kModalBg(0x02, 0x05, 0x06), kPanelBg(0x0a, 0x0c, 0x0c), kPanelBorder(0x1f, 0x22, 0x22);
const QColor kCy(0x3c, 0xd6, 0xe6), kRd(0xe6, 0x5f, 0x62);

int decimalsFor(const QString& sym, double v) {
  const QString s = sym.toUpper();
  if (s.contains("JPY")) return 3;
  if (s.startsWith("XAU") || s.startsWith("BTC") || s.startsWith("ETH")) return 2;
  if (s.startsWith("XAG")) return 3;
  if (std::isfinite(v) && v > 20) return 2;
  return 5;
}
QString fullName(const QString& sym) {
  static const QHash<QString, const char*> n{{"EUR", QT_TRANSLATE_NOOP("positioning", "Euro")}, {"USD", QT_TRANSLATE_NOOP("positioning", "U.S. Dollar")}, {"GBP", QT_TRANSLATE_NOOP("positioning", "British Pound")}, {"JPY", QT_TRANSLATE_NOOP("positioning", "Japanese Yen")},
    {"CHF", QT_TRANSLATE_NOOP("positioning", "Swiss Franc")}, {"CAD", QT_TRANSLATE_NOOP("positioning", "Canadian Dollar")}, {"AUD", QT_TRANSLATE_NOOP("positioning", "Australian Dollar")}, {"NZD", QT_TRANSLATE_NOOP("positioning", "New Zealand Dollar")},
    {"XAU", QT_TRANSLATE_NOOP("positioning", "Gold")}, {"XAG", QT_TRANSLATE_NOOP("positioning", "Silver")}, {"BTC", "Bitcoin"}, {"ETH", "Ethereum"}, {"LTC", "Litecoin"}, {"XRP", "XRP"}};
  auto t = [](const char* x) { return QCoreApplication::translate("positioning", x); };   // at display time, never cached
  if (sym.size() == 6 && n.contains(sym.left(3)) && n.contains(sym.mid(3))) return t(n[sym.left(3)]) + " / " + t(n[sym.mid(3)]);
  return sym;
}
void smallTitle(QPainter& p, qreal x, qreal y, const QString& t, const QColor& c = theme::map(0x60, 0x5c, 0x55)) {
  const QFont f = theme::mono(9, QFont::Medium, 1.0);
  p.setFont(f); p.setPen(c); p.drawText(QPointF(x, y + QFontMetricsF(f).capHeight() / 2), t);
}
}

// ------------------------------------------------------------------------------------------------ header
class RetailModalHeader : public QWidget {
public:
  explicit RetailModalHeader(QWidget* p) : QWidget(p) { setMouseTracking(true); }
  std::function<void()> onClose;
  void setSymbol(const RetailSymbol& s) { m_s = s; update(); }
protected:
  QRectF closeRect() const { return QRectF(width() - 40, 4, 32, 32); }
  void mouseMoveEvent(QMouseEvent* e) override { const bool h = closeRect().contains(e->position()); if (h != m_hover) { m_hover = h; setCursor(h ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); } }
  void mousePressEvent(QMouseEvent* e) override { if (closeRect().contains(e->position()) && onClose) onClose(); }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(QRectF(0, height() - 1, width(), 1), theme::map(0x1a, 0x1e, 0x1d));
    const QFont tf = theme::display(20, QFont::Bold, 0.5);
    p.setFont(tf); p.setPen(theme::map(0xf5, 0xf2, 0xeb));
    const QString d = m_s.display();
    p.drawText(QPointF(16, height() / 2.0 + 7), d);
    qreal x = 16 + QFontMetricsF(tf).horizontalAdvance(d) + 12;
    p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(theme::map(0x6c, 0x6e, 0x6c));
    const QString nm = fullName(m_s.name);
    p.drawText(QPointF(x, height() / 2.0 + 5), nm);
    x += QFontMetricsF(theme::condensed(14, QFont::Medium)).horizontalAdvance(nm) + 14;
    const QRectF b = pk::drawStatusBadge(p, -1000, height() / 2.0, pk::statusOf(m_s.longPct), 17);
    pk::drawStatusBadge(p, x + b.width(), height() / 2.0, pk::statusOf(m_s.longPct), 17);
    const QRectF c = closeRect();
    p.setPen(QPen(m_hover ? theme::map(0xe8, 0xe3, 0xda) : theme::map(0x7d, 0x7c, 0x79), 1.3));
    const QPointF m = c.center();
    p.drawLine(m + QPointF(-5, -5), m + QPointF(5, 5)); p.drawLine(m + QPointF(-5, 5), m + QPointF(5, -5));
  }
private:
  RetailSymbol m_s; bool m_hover = false;
};

// ------------------------------------------------------------------------------------------------ chart frame
class ChartFrame : public QWidget {
public:
  explicit ChartFrame(QWidget* p) : QWidget(p) {
    chart = new pos::CandleChart(this);
    chart->setStyle(pos::CandleChart::modalStyle());
    setMouseTracking(true);
  }
  pos::CandleChart* chart;
  int range = 1;   // index in kRanges
  std::function<void()> onRange;
  static QStringList ranges() { return {QCoreApplication::translate("positioning", "5D"), QCoreApplication::translate("positioning", "2W"), QCoreApplication::translate("positioning", "1M"), QCoreApplication::translate("positioning", "3M"), QCoreApplication::translate("positioning", "6M"), QCoreApplication::translate("positioning", "1Y")}; }
  QString clock;
protected:
  QRectF rangeRect(int i) const { return QRectF(14 + i * 38, height() - 34, 34, 28); }
  void resizeEvent(QResizeEvent*) override { chart->setGeometry(1, 41, width() - 2, height() - 41 - 41); }
  void mousePressEvent(QMouseEvent* e) override {
    for (int i = 0; i < ranges().size(); ++i) if (rangeRect(i).contains(e->position()) && i != range) { range = i; update(); if (onRange) onRange(); }
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    bool on = false; for (int i = 0; i < ranges().size(); ++i) on = on || rangeRect(i).contains(e->position());
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), theme::map(0x0a, 0x0a, 0x0a));
    p.fillRect(QRectF(0, 0, width(), 40), theme::map(QColor(Qt::black)));
    p.fillRect(QRectF(0, height() - 40, width(), 40), theme::map(QColor(Qt::black)));
    p.fillRect(QRectF(0, 40, width(), 1), theme::map(0x1d, 0x1e, 0x22));
    p.fillRect(QRectF(0, height() - 41, width(), 1), theme::map(0x1d, 0x1e, 0x22));
    // toolbar: candle style (selected) and the source
    const QRectF sel(8, 6, 32, 28);
    p.setPen(Qt::NoPen); p.setBrush(theme::map(0x2a, 0x2e, 0x39)); p.drawRoundedRect(sel, 4, 4);
    p.setPen(QPen(theme::map(0xd1, 0xd4, 0xdc), 1.2));
    for (int k = 0; k < 2; ++k) {
      const qreal x = sel.left() + 11 + k * 9;
      p.drawLine(QPointF(x, sel.top() + 7), QPointF(x, sel.bottom() - 7));
      p.setBrush(k ? theme::map(0xd1, 0xd4, 0xdc) : Qt::transparent);
      p.drawRect(QRectF(x - 2.5, sel.top() + 10 + k * 2, 5, 8));
    }
    p.setFont(theme::sans(13, QFont::Medium)); p.setPen(theme::map(0xc2, 0xc4, 0xcc));
    p.drawText(QRectF(52, 0, 400, 40), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("positioning", "Candles · 1h · Yahoo Finance (delayed)"));
    p.setPen(theme::map(0x5d, 0x60, 0x68)); p.setFont(theme::sans(11));
    p.drawText(QRectF(0, 0, width() - 14, 40), Qt::AlignVCenter | Qt::AlignRight, QCoreApplication::translate("positioning", "Deepwater chart"));
    p.setFont(theme::sans(13, QFont::DemiBold));
    for (int i = 0; i < ranges().size(); ++i) {
      const QRectF r = rangeRect(i);
      if (i == range) { p.setPen(Qt::NoPen); p.setBrush(theme::map(0x2a, 0x2e, 0x39)); p.drawRoundedRect(r.adjusted(0, 3, 0, -3), 3, 3); }
      p.setPen(i == range ? theme::map(0xf0, 0xf3, 0xfa) : theme::map(0xbe, 0xc2, 0xc6)); p.drawText(r, Qt::AlignCenter, ranges()[i]);
    }
    p.setFont(theme::sans(13, QFont::Medium)); p.setPen(theme::map(0xd1, 0xd4, 0xdc));
    p.drawText(QRectF(0, height() - 40, width() - 14, 40), Qt::AlignVCenter | Qt::AlignRight, clock);
  }
};

// ------------------------------------------------------------------------------------------------ body
class RetailModalBody : public QWidget {
public:
  explicit RetailModalBody(QWidget* p) : QWidget(p) {
    setMouseTracking(true);
    frame = new ChartFrame(this);
    anim = new QVariantAnimation(this);
    anim->setStartValue(0.0); anim->setEndValue(1.0); anim->setDuration(500);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_prog = v.toReal(); update(QRect(0, m_hist1, width(), height() - m_hist1)); });
    setMinimumHeight(kHeight);
  }
  static constexpr int kChartTop = 40, kRightTop = 10, kHeight = 1645;
  // Vertical layout: the chart is 673 px at the reference size and grows with taller windows (up to 900); the
  // history rows, narrative and footnote follow. The narrative and footnote heights follow their wrapped text.
  void setChartHeight(int h) {
    h = std::clamp(h, 673, 900);
    if (h == m_chartH) return;
    m_chartH = h; relayout();
  }
  void relayout() {
    m_hist1 = kChartTop + m_chartH + 12; m_hist2 = m_hist1 + 396; m_narr = m_hist2 + 396;
    frame->setGeometry(12, kChartTop, width() - 12 - 407 - 12 - 11, m_chartH);
    const int need = m_narr + m_narrH + 9 + m_footH + 12;
    if (minimumHeight() != need) setMinimumHeight(need);
    update();
  }
  int m_chartH = 673, m_hist1 = 725, m_hist2 = 1121, m_narr = 1517, m_narrH = 83, m_footH = 44;
  ChartFrame* frame;
  QVariantAnimation* anim;
  QString sym;
  RetailSymbol s;
  QVector<QPair<QDateTime, RetailSymbol>> hist;
  double last = NAN; bool lastFromCandle = false;
  positioning::CotHistory cot; bool cotLoading = false; QString cotNone;
  qreal m_prog = 1;

  void startAnim() { m_prog = 0; anim->stop(); anim->start(); }
protected:
  QRectF rightCol() const { return QRectF(width() - 12 - 407, kRightTop, 407, m_chartH + 30); }
  void resizeEvent(QResizeEvent*) override { relayout(); }
  bool event(QEvent* e) override {
    if (e->type() == QEvent::ToolTip) {
      auto* he = static_cast<QHelpEvent*>(e);
      const QRectF st = rightCol().adjusted(0, 301 + (m_chartH - 673), 0, 0);
      if (st.contains(he->pos()) && he->pos().y() > st.top() + 270)
        QToolTip::showText(he->globalPos(), QCoreApplication::translate("positioning", "Computed by Deepwater from Myfxbook's average entry prices and the live quote (Yahoo Finance) — not a broker figure."), this);
      else QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }
  void paintEvent(QPaintEvent* ev) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const int dec = decimalsFor(sym, s.avgLong);
    // chips
    {
      qreal x = 12; const qreal cy = 22;
      const QFont f = theme::mono(12); const QFontMetricsF fm(f);
      auto chip = [&](const QString& t, const QColor& c, bool border) {
        const qreal w = fm.horizontalAdvance(t) + (border ? 14 : 4);
        if (border) { p.setPen(QPen(theme::alpha(c, 150), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(QRectF(x, cy - 11, w, 22).adjusted(.5, .5, -.5, -.5), 2, 2); }
        p.setFont(f); p.setPen(c); p.drawText(QRectF(x, cy - 11, w, 22), Qt::AlignCenter, t);
        x += w + 7;
      };
      const QString lastV = std::isfinite(last) ? fmt::num(last, dec, false) : fmt::dash();
      const QString lastS = lastFromCandle ? QCoreApplication::translate("positioning", "Last %1 (last 1h close)").arg(lastV) : QCoreApplication::translate("positioning", "Last %1").arg(lastV);
      QString a = QCoreApplication::translate("positioning", "Avg retail LONG entry %1").arg(fmt::num(s.avgLong, dec, false)), b = QCoreApplication::translate("positioning", "Avg retail SHORT entry %1").arg(fmt::num(s.avgShort, dec, false));
      const qreal avail = width() - 12 - 407 - 23 - 12;
      if (fm.horizontalAdvance(a + b + lastS) + 3 * 7 + 32 > avail) {   // small windows: "Avg LONG …" (full words in the stats panel)
        a = QCoreApplication::translate("positioning", "Avg LONG %1").arg(fmt::num(s.avgLong, dec, false)); b = QCoreApplication::translate("positioning", "Avg SHORT %1").arg(fmt::num(s.avgShort, dec, false));
      }
      chip(a, theme::map(kCy), true);
      chip(b, theme::map(kRd), true);
      chip(lastS, theme::map(0x76, 0x75, 0x72), false);
      p.fillRect(QRectF(0, kChartTop - 2, width() - 12 - 407 - 11, 1), theme::map(0x23, 0x24, 0x20));
    }
    const QRectF rc = rightCol();
    const qreal ex = m_chartH - 673;   // extra height of a taller chart goes to the distribution panel
    const qreal k = 1 + std::min(0.3, ex / 288.0 * 0.6);
    // distribution
    {
      const QRectF r(rc.left(), rc.top(), rc.width(), 288 + ex);
      p.setPen(QPen(theme::map(kPanelBorder), 1)); p.setBrush(theme::map(kPanelBg)); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
      const QFont tf = theme::mono(9, QFont::Bold, 1.6);
      p.setFont(tf); p.setPen(theme::map(0x74, 0x74, 0x71));
      p.drawText(QRectF(r.left(), r.top() + 14, r.width(), 14), Qt::AlignCenter, QCoreApplication::translate("positioning", "LONG / SHORT DISTRIBUTION"));
      const QPointF c(r.center().x(), r.top() + 139 + ex / 2);   // measured on t002.0: top face y 137–285
      const qreal rx = 118 * k, ry = 74 * k;
      {  // soft glow under the pie
        QRadialGradient g(c + QPointF(0, 30), 150);
        g.setColorAt(0, theme::map(0x14, 0xc0, 0xd0, 40)); g.setColorAt(1, theme::map(0, 0, 0, 0));
        p.save(); p.translate(c + QPointF(0, 30)); p.scale(1, 0.45); p.translate(-(c + QPointF(0, 30)));
        p.setPen(Qt::NoPen); p.setBrush(g); p.drawEllipse(c + QPointF(0, 30), 150, 150); p.restore();
      }
      pk::drawPie3D(p, c, rx, ry, 15 * k, s.longPct, 0, 0.6);
      auto label = [&](bool lng) {
        const double share = lng ? s.longPct : s.shortPct;
        if (!std::isfinite(share) || share < 4) return;
        const QPointF q = pk::pieLabelPoint(c, rx, ry, s.longPct, lng, share < 25 ? 0.7 : 0.5);
        p.setFont(theme::mono(22, QFont::Bold));
        for (int k = 0; k < 6; ++k) { p.setPen(theme::map(0, 0, 0, 40)); p.drawText(QRectF(q.x() - 60 + std::cos(k) * 1.5, q.y() - 22 + std::sin(k) * 1.5, 120, 28), Qt::AlignCenter, QString("%1%").arg(fmt::whole(share))); }
        p.setPen(theme::isDark() ? theme::map(QColor(Qt::white)) : QColor(Qt::white)); p.drawText(QRectF(q.x() - 60, q.y() - 22, 120, 28), Qt::AlignCenter, QString("%1%").arg(fmt::whole(share)));
        p.setFont(theme::mono(11, QFont::Bold)); p.drawText(QRectF(q.x() - 60, q.y() + 3, 120, 14), Qt::AlignCenter, lng ? QCoreApplication::translate("positioning", "LONG") : QCoreApplication::translate("positioning", "SHORT"));
      };
      label(false); label(true);
      const QFont lf = theme::condensed(12, QFont::Medium); const QFontMetricsF lm(lf);
      const QString a = QCoreApplication::translate("positioning", "Long %1%").arg(fmt::whole(s.longPct)), b = QCoreApplication::translate("positioning", "Short %1%").arg(fmt::whole(s.shortPct));
      const qreal tw = 10 + lm.horizontalAdvance(a) + 18 + 10 + lm.horizontalAdvance(b);
      qreal x = c.x() - tw / 2; const qreal y = r.bottom() - 23;
      p.setPen(Qt::NoPen); p.setBrush(theme::map(kCy)); p.drawEllipse(QPointF(x + 3, y), 3, 3);
      p.setFont(lf); p.setPen(theme::map(0x68, 0x8e, 0x92)); p.drawText(QPointF(x + 10, y + 4), a);
      x += 10 + lm.horizontalAdvance(a) + 18;
      p.setPen(Qt::NoPen); p.setBrush(theme::map(kRd)); p.drawEllipse(QPointF(x + 3, y), 3, 3);
      p.setPen(theme::map(0x8a, 0x5a, 0x5a)); p.drawText(QPointF(x + 10, y + 4), b);
    }
    // statistics
    {
      const QRectF r(rc.left(), rc.top() + 301 + ex, rc.width(), 402);
      p.setPen(QPen(theme::map(kPanelBorder), 1)); p.setBrush(theme::map(kPanelBg)); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
      smallTitle(p, r.left() + 11, r.top() + 17, QCoreApplication::translate("positioning", "RETAIL POSITIONING STATISTICS"), theme::map(0x75, 0x77, 0x72));
      const double lpe = last - s.avgLong, spe = s.avgShort - last;
      struct Row { QString l, v; QColor c; bool computed; };
      const QColor w = theme::map(0xf5, 0xf5, 0xf4);
      const QList<Row> rows{
        {QCoreApplication::translate("positioning", "Long Positions"), fmt::num(s.longPositions, 0), theme::map(kCy), false}, {QCoreApplication::translate("positioning", "Short Positions"), fmt::num(s.shortPositions, 0), theme::map(kRd), false},
        {QCoreApplication::translate("positioning", "Total Positions"), fmt::num(s.totalPositions, 0), w, false}, {QCoreApplication::translate("positioning", "Average Long Price"), fmt::num(s.avgLong, dec, false), theme::map(kCy), false},
        {QCoreApplication::translate("positioning", "Average Short Price"), fmt::num(s.avgShort, dec, false), theme::map(kRd), false},
        {QCoreApplication::translate("positioning", "Long Position Ratio"), QString("%1%").arg(fmt::whole(s.longPct)), theme::map(kCy), false}, {QCoreApplication::translate("positioning", "Short Position Ratio"), QString("%1%").arg(fmt::whole(s.shortPct)), theme::map(kRd), false},
        {QCoreApplication::translate("positioning", "Long Volume"), fmt::num(s.longVolume, 1) + " lots", theme::map(kCy), false}, {QCoreApplication::translate("positioning", "Short Volume"), fmt::num(s.shortVolume, 1) + " lots", theme::map(kRd), false},
        {QCoreApplication::translate("positioning", "Long Price vs Entry"), fmt::signedNum(lpe, dec), theme::map(kCy), true}, {QCoreApplication::translate("positioning", "Short Price vs Entry"), fmt::signedNum(spe, dec), theme::map(kRd), true},
        {QCoreApplication::translate("positioning", "Long Est. avg P&L"), fmt::pct(lpe / s.avgLong * 100, 2), theme::map(kCy), true}, {QCoreApplication::translate("positioning", "Short Est. avg P&L"), fmt::pct(spe / s.avgShort * 100, 2), theme::map(kRd), true}};
      qreal y = r.top() + 32;
      for (int i = 0; i < rows.size(); ++i) {
        if (i == 9) { y += 14; }
        const QRectF row(r.left() + 11, y, r.width() - 22, 26.7);
        p.fillRect(QRectF(row.left(), row.bottom() - 1, row.width(), 1), theme::map(0x21, 0x21, 0x1e));
        // computed rows are dimmer in Deepwater; other themes would make that step unreadable, so they use the label colour
        p.setFont(theme::condensed(12, QFont::Medium)); p.setPen(rows[i].computed && theme::isDefault() ? theme::map(0x3b, 0x3b, 0x3a) : theme::map(0x57, 0x59, 0x57));
        p.drawText(row, Qt::AlignVCenter | Qt::AlignLeft, rows[i].l);
        p.setFont(theme::mono(12, QFont::DemiBold)); p.setPen(rows[i].c);
        p.drawText(row, Qt::AlignVCenter | Qt::AlignRight, rows[i].v);
        y += 26.7;
      }
    }
    if (ev->rect().bottom() < m_hist1) return;
    // history rows
    const qreal lw = (width() - 24 - 10) / 2.0;
    const QRectF h1l(12, m_hist1, lw, 385), h1r(12 + lw + 10, m_hist1, width() - 24 - lw - 10, 385);
    const QRectF h2l(12, m_hist2, lw, 384), h2r(h1r.left(), m_hist2, h1r.width(), 384);
    for (const auto& r : {h1l, h1r, h2l, h2r}) { p.setPen(QPen(theme::map(0x23, 0x22, 0x17), 1)); p.setBrush(theme::map(0x14, 0x0f, 0x05)); p.drawRect(r.adjusted(.5, .5, -.5, -.5)); }
    smallTitle(p, h1l.left() + 10, h1l.top() + 16, QCoreApplication::translate("positioning", "LONG / SHORT HISTORY"));
    smallTitle(p, h2l.left() + 10, h2l.top() + 16, QCoreApplication::translate("positioning", "VOLUME"));
    smallTitle(p, h2r.left() + 10, h2r.top() + 16, QCoreApplication::translate("positioning", "POSITION"));
    smallTitle(p, h1r.left() + 10, h1r.top() + 16, QCoreApplication::translate("positioning", "OPEN INTEREST — %1").arg(cot.ok() ? cot.market.shortName : QString("CFTC")));
    auto note = [&](const QRectF& r, const QString& t) {
      p.setFont(theme::condensed(11, QFont::Medium)); p.setPen(theme::map(0x45, 0x41, 0x37)); p.drawText(QRectF(r.left(), r.top() + 8, r.width() - 10, 16), Qt::AlignVCenter | Qt::AlignRight, t);
    };
    note(h1r, QCoreApplication::translate("positioning", "CFTC, weekly")); note(h2l, QCoreApplication::translate("positioning", "each side indexed to its own range")); note(h2r, QCoreApplication::translate("positioning", "each side indexed to its own range"));
    note(h1l, QCoreApplication::translate("positioning", "Deepwater recordings of Myfxbook"));
    const int n = hist.size();
    if (n < 2) {
      const QString t = n ? QCoreApplication::translate("positioning", "History is recorded by Deepwater from each Myfxbook answer (every 30 minutes while the app runs).\n%n snapshot(s) so far since %1.", nullptr, n)
                                .arg(i18n::locale().toString(hist.first().first.toLocalTime(), "d MMM HH:mm"))
                          : QCoreApplication::translate("positioning", "History is recorded by Deepwater from each Myfxbook answer (every 30 minutes while the app runs).\nNo snapshot so far.");
      for (const auto& r : {h1l, h2l, h2r}) { p.setFont(theme::mono(10)); p.setPen(theme::map(0x6e, 0x6a, 0x5e)); p.drawText(r.adjusted(30, 30, -30, -10), Qt::AlignCenter | Qt::TextWordWrap, t); }
    } else {
      const int show = std::min(n, 150);
      const auto hs = hist.mid(n - show);
      // stacked long / short %
      {
        const QRectF pl = h1l.adjusted(10, 34, -10, -12);
        const qreal bw = pl.width() / show;
        const int upto = int(std::ceil(show * m_prog));
        for (int i = 0; i < upto; ++i) {
          const double L = hs[i].second.longPct;
          if (!std::isfinite(L)) continue;
          const qreal x = pl.left() + i * bw, w = std::max<qreal>(1, bw * 0.55);
          const qreal grow = std::min(1.0, m_prog * 1.4);
          const qreal ly = pl.bottom() - pl.height() * L / 100 * grow, top = pl.bottom() - pl.height() * grow;
          QLinearGradient g(0, ly, 0, pl.bottom()); g.setColorAt(0, theme::map(0x75, 0xce, 0xda)); g.setColorAt(1, theme::map(0x2e, 0x7e, 0x86));
          p.fillRect(QRectF(x, ly, w, pl.bottom() - ly), g);
          p.fillRect(QRectF(x, top, w, ly - top), theme::map(0xac, 0x67, 0x65));
        }
        QPen dp(theme::map(0x70, 0xb8, 0xb4, 170), 1, Qt::CustomDashLine); dp.setDashPattern({4, 4}); p.setPen(dp);
        p.drawLine(QPointF(pl.left(), pl.center().y()), QPointF(pl.right(), pl.center().y()));
      }
      auto diverging = [&](const QRectF& panel, bool volume) {
        const QRectF pl = panel.adjusted(10, 34, -10, -14);
        const qreal mid = pl.center().y(), half = pl.height() / 2 - 4, bw = pl.width() / show;
        double lmin = INFINITY, lmax = -INFINITY, smin = INFINITY, smax = -INFINITY;
        auto lv = [&](const RetailSymbol& x) { return volume ? x.longVolume : x.longPositions; };
        auto sv = [&](const RetailSymbol& x) { return volume ? x.shortVolume : x.shortPositions; };
        for (const auto& h : hs) { lmin = std::min(lmin, lv(h.second)); lmax = std::max(lmax, lv(h.second)); smin = std::min(smin, sv(h.second)); smax = std::max(smax, sv(h.second)); }
        const int upto = int(std::ceil(show * m_prog));
        for (int i = 0; i < upto; ++i) {
          const qreal x = pl.left() + i * bw, w = std::max<qreal>(1, bw * 0.55);
          const double a = lmax > lmin ? (lv(hs[i].second) - lmin) / (lmax - lmin) : 0.5, b = smax > smin ? (sv(hs[i].second) - smin) / (smax - smin) : 0.5;
          const qreal ha = (4 + (half - 4) * a) * std::min(1.0, m_prog * 1.4), hb = (4 + (half - 4) * b) * std::min(1.0, m_prog * 1.4);
          QLinearGradient gu(0, mid - ha, 0, mid); gu.setColorAt(0, theme::map(0x73, 0xc2, 0xcb)); gu.setColorAt(1, theme::map(0x2a, 0x7a, 0x84));
          p.fillRect(QRectF(x, mid - ha, w, ha), gu);
          QLinearGradient gd(0, mid, 0, mid + hb); gd.setColorAt(0, theme::map(0x7a, 0x3e, 0x3c)); gd.setColorAt(1, theme::map(0xb4, 0x6e, 0x6a));
          p.fillRect(QRectF(x, mid, w, hb), gd);
        }
      };
      diverging(h2l, true); diverging(h2r, false);
    }
    // open interest bars
    {
      const QRectF pl = h1r.adjusted(10, 30, -10, -10);
      if (cot.ok()) {
        const auto ws = cot.weeks.mid(std::max(0, int(cot.weeks.size()) - 26));
        double lo = INFINITY, hi = -INFINITY;
        for (const auto& w : ws) { lo = std::min(lo, w.oi); hi = std::max(hi, w.oi); }
        const qreal slot = pl.width() / ws.size(), bw = slot * 0.78;
        for (int i = 0; i < ws.size(); ++i) {
          const double t = hi > lo ? (ws[i].oi - lo) / (hi - lo) : 0.5;
          const qreal h = (pl.height() * (0.08 + 0.92 * t)) * std::min(1.0, m_prog * 1.2);
          const QRectF b(pl.left() + i * slot + (slot - bw) / 2, pl.bottom() - h, bw, h);
          QLinearGradient g(0, pl.bottom() - pl.height(), 0, pl.bottom());
          g.setColorAt(0, theme::map(0xf8, 0xd9, 0x10)); g.setColorAt(0.6, theme::map(0xcb, 0xab, 0x0e)); g.setColorAt(1, theme::map(0x6c, 0x58, 0x0c));
          p.setPen(Qt::NoPen); p.setBrush(g); p.drawRoundedRect(b, 1, 1);
        }
        p.setFont(theme::mono(9)); p.setPen(theme::map(0x5a, 0x55, 0x48));
        p.drawText(QRectF(pl.left(), pl.top() - 6, pl.width(), 14), Qt::AlignRight | Qt::AlignVCenter,
                   QCoreApplication::translate("positioning", "%1 – %2 contracts · %3").arg(fmt::compact(lo, 1), fmt::compact(hi, 1), ws.last().date.toString("yyyy-MM-dd")));
      } else {
        p.setFont(theme::mono(10)); p.setPen(theme::map(0x6e, 0x6a, 0x5e));
        p.drawText(pl.adjusted(30, 0, -30, 0), Qt::AlignCenter | Qt::TextWordWrap, cotLoading ? QCoreApplication::translate("positioning", "Loading CFTC open interest…") : cotNone);
      }
    }
    // narrative
    {
      const QRectF r(12, m_narr, width() - 24, m_narrH);
      p.setPen(QPen(theme::map(0x47, 0x3f, 0x1e), 1)); p.setBrush(theme::map(0x2e, 0x28, 0x08)); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
      p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(theme::map(0x93, 0x8c, 0x6e));
      // prose lines at most as long as at the reference size (1560 px)
      const QRectF tr(r.left() + 16, r.top() + 10, std::min<qreal>(r.width() - 32, 1560), r.height() - 20);
      const QString nt = narrative(dec);
      const int need = int(std::ceil(p.boundingRect(tr, Qt::TextWordWrap, nt).height())) + 20;
      if (std::max(83, need) != m_narrH) { m_narrH = std::max(83, need); QTimer::singleShot(0, this, [this] { relayout(); }); }
      p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, nt);
    }
    {
      const QString foot = QCoreApplication::translate("positioning", "Long / short %, volumes, positions and average entries: Myfxbook community outlook (volume-weighted). "
        "Price vs Entry compares the average entry price for that side to the current live quote — a real calculation by this app, "
        "not a broker-reported figure, and only as accurate as the underlying average-entry data. Open Interest is the real CFTC futures "
        "figure for the matching standalone currency contract, not a retail-CFD metric — retail CFD positions have no open interest of "
        "their own. History charts are this app's own recordings of the Myfxbook answers.");
      p.setFont(theme::condensed(12, QFont::Medium)); p.setPen(theme::map(0x58, 0x5b, 0x5a));
      const QRectF fr(14, m_narr + m_narrH + 9, std::min<qreal>(width() - 28, 1560), m_footH);
      const int need = int(std::ceil(p.boundingRect(fr, Qt::TextWordWrap, foot).height())) + 4;
      if (std::max(44, need) != m_footH) { m_footH = std::max(44, need); QTimer::singleShot(0, this, [this] { relayout(); }); }
      p.drawText(fr, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, foot);
    }
  }
  QString narrative(int dec) const {
    const auto st = pk::statusOf(s.longPct);
    const QString state = st == pk::Status::CrowdedLong ? QCoreApplication::translate("positioning", "crowded long") : st == pk::Status::LeaningLong ? QCoreApplication::translate("positioning", "leaning long")
                        : st == pk::Status::CrowdedShort ? QCoreApplication::translate("positioning", "crowded short") : st == pk::Status::LeaningShort ? QCoreApplication::translate("positioning", "leaning short") : QCoreApplication::translate("positioning", "balanced");
    QString t = QCoreApplication::translate("positioning", "%1 is %2 — %3% long vs %4% short across %5 open positions.")
                    .arg(s.display(), state).arg(fmt::whole(s.longPct)).arg(fmt::whole(s.shortPct)).arg(fmt::num(s.totalPositions, 0));
    if (std::isfinite(last) && std::isfinite(s.avgLong) && std::isfinite(s.avgShort)) {
      const double lp = (last - s.avgLong) / s.avgLong * 100, sp = (s.avgShort - last) / s.avgShort * 100;
      auto ud = [](double v) {
        return (v >= 0 ? QCoreApplication::translate("positioning", "up %1%") : QCoreApplication::translate("positioning", "down %1%")).arg(fmt::num(std::abs(v), 2));
      };
      // one full sentence per case, so each language can phrase it naturally
      const QString sentence = lp >= 0 && sp >= 0
          ? QCoreApplication::translate("positioning", "The average long entry (%1) is currently %2, while the average short entry (%3) is %4 — both sides are ahead of their own average entry.")
          : lp < 0 && sp < 0
          ? QCoreApplication::translate("positioning", "The average long entry (%1) is currently %2, while the average short entry (%3) is %4 — both sides show a mixed real result versus their own average entry.")
          : lp >= 0
          ? QCoreApplication::translate("positioning", "The average long entry (%1) is currently %2, while the average short entry (%3) is %4 — longs are ahead and shorts behind their own average entry.")
          : QCoreApplication::translate("positioning", "The average long entry (%1) is currently %2, while the average short entry (%3) is %4 — shorts are ahead and longs behind their own average entry.");
      t += " " + sentence.arg(fmt::num(s.avgLong, dec, false), ud(lp), fmt::num(s.avgShort, dec, false), ud(sp));
    }
    if (hist.size() >= 2) {
      const double d = s.longPct - hist.first().second.longPct;
      const QString since = i18n::locale().toString(hist.first().first.toLocalTime(), "d MMM HH:mm");
      const int pts = fmt::roundInt(std::abs(d));
      if (pts == 0) t += " " + QCoreApplication::translate("positioning", "Long exposure is unchanged since this app's first snapshot (%1).").arg(since);
      else if (d > 0) t += " " + QCoreApplication::translate("positioning", "Long exposure has increased %n point(s) since this app's first snapshot (%1).", nullptr, pts).arg(since);
      else t += " " + QCoreApplication::translate("positioning", "Long exposure has decreased %n point(s) since this app's first snapshot (%1).", nullptr, pts).arg(since);
    } else t += " " + QCoreApplication::translate("positioning", "Changes over time will appear as the app records more snapshots.");
    return t;
  }
};

// ------------------------------------------------------------------------------------------------ modal
RetailModal::RetailModal(QWidget* page) : QWidget(page) {
  setFocusPolicy(Qt::StrongFocus);
  m_header = new RetailModalHeader(this);
  m_header->onClose = [this] { close(); };
  m_scroll = new pk::Scroll(this);
  m_body = new RetailModalBody(m_scroll);
  m_scroll->setWidget(m_body);
  m_clock = new QTimer(this); m_clock->setInterval(1000);
  connect(m_clock, &QTimer::timeout, this, [this] {
    const QDateTime now = QDateTime::currentDateTime();
    const int off = now.offsetFromUtc() / 3600;
    m_body->frame->clock = now.toString("HH:mm:ss") + QString(" UTC%1%2").arg(off >= 0 ? "+" : "-").arg(std::abs(off));
    m_body->frame->update(QRect(m_body->frame->width() - 200, m_body->frame->height() - 40, 200, 40));
  });
  m_body->frame->onRange = [this] { loadChart(); };
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, [this](const QStringList& syms) {
    if (!isVisible() || !syms.contains(market::find(m_symbol) ? market::find(m_symbol)->symbol : m_symbol)) return;
    const auto q = market::QuoteHub::instance().quote(m_symbol);
    if (q.valid()) { m_body->last = q.price; m_body->lastFromCandle = false; m_body->update(); }
  });
  hide();
}

// 14 px margin; on very wide screens the dialog is capped at 2200 px and centred (a 3400 px-wide dialog would stretch
// the chart and the prose for nothing)
QRect RetailModal::box() const {
  QRect r = rect().adjusted(14, 14, -14, -14);
  if (r.width() > 2200) r = QRect(r.center().x() - 1100, r.top(), 2200, r.height());
  return r;
}

void RetailModal::resizeEvent(QResizeEvent*) {
  const QRect b = box();
  m_header->setGeometry(b.left() + 1, b.top() + 3, b.width() - 2, 44);
  m_scroll->setGeometry(b.left() + 1, b.top() + 48, b.width() - 2, b.height() - 49);
  m_body->setChartHeight(b.height() - 49 - 216);   // 673 at the reference size, up to 900 on tall screens
}

void RetailModal::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), theme::map(0x02, 0x02, 0x00));
  const QRect b = box();
  p.fillRect(b, theme::map(kModalBg));
  p.setPen(QPen(theme::map(0x20, 0x24, 0x20), 1)); p.drawRect(QRectF(b).adjusted(.5, .5, -.5, -.5));
  QLinearGradient g(b.left(), 0, b.right(), 0);
  g.setColorAt(0, theme::map(0x60, 0x9c, 0xa3)); g.setColorAt(0.5, theme::map(0x3a, 0x6e, 0x6f)); g.setColorAt(1, theme::map(0x1d, 0x24, 0x1b));
  p.fillRect(QRectF(b.left(), b.top(), b.width(), 2), g);
}

void RetailModal::mousePressEvent(QMouseEvent* e) { if (!box().contains(e->position().toPoint())) close(); }
void RetailModal::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape) close(); else QWidget::keyPressEvent(e); }
void RetailModal::showEvent(QShowEvent*) {
  m_clock->start();
  market::QuoteHub::instance().setActive(this, true);   // back from another tab: live price again
}
void RetailModal::hideEvent(QHideEvent*) {
  m_clock->stop();
  market::QuoteHub::instance().setActive(this, false);
}

void RetailModal::close() { hide(); market::QuoteHub::instance().unsubscribe(this); emit closed(); }

void RetailModal::open(const QString& symbol) {
  m_symbol = symbol.toUpper();
  m_body->sym = m_symbol;
  m_body->last = NAN; m_body->lastFromCandle = false;
  refreshData();
  // live quote through the app's quote hub (Yahoo spark); only this symbol (subscribe adds to the owner's set)
  market::QuoteHub::instance().unsubscribe(this);
  if (market::find(m_symbol)) market::QuoteHub::instance().subscribe(this, {m_symbol}, true);
  const auto q = market::QuoteHub::instance().quote(m_symbol);
  if (q.valid()) m_body->last = q.price;
  // CFTC open interest of the matching standalone contract
  m_body->cot = {}; m_body->cotLoading = false;
  if (const auto* mk = positioning::Cot::marketForSymbol(m_symbol)) {
    m_body->cotLoading = true;
    const QString want = m_symbol;
    positioning::Cot::instance().history(mk->id, this, [this, want](const positioning::CotHistory& h) {
      if (want != m_symbol) return;
      m_body->cot = h; m_body->cotLoading = false;
      if (!h.ok()) m_body->cotNone = QCoreApplication::translate("positioning", "CFTC unavailable: %1").arg(h.error);
      m_body->update();
    });
  } else m_body->cotNone = QCoreApplication::translate("positioning", "No standalone CFTC futures contract matches %1 (open interest exists for the pairs against the USD, gold, silver and bitcoin).").arg(m_body->s.display());
  m_scroll->verticalScrollBar()->setValue(0);
  show(); raise(); setFocus();
  m_body->frame->chart->setLoading(true);
  m_body->startAnim();
  loadChart();
}

void RetailModal::refreshData() {
  auto& r = Retail::instance();
  if (const auto* s = r.latest().find(m_symbol)) { m_body->s = *s; m_header->setSymbol(*s); }
  m_body->hist.clear();
  for (const auto& snap : r.recordings()) if (const auto* x = snap.find(m_symbol)) m_body->hist << qMakePair(snap.time, *x);
  m_body->update();
}

void RetailModal::loadChart() {
  static const struct { const char* iv; const char* rg; int days; } kR[6] = {
    {"1h", "1mo", 5}, {"1h", "1mo", 14}, {"1h", "1mo", 31}, {"1d", "3mo", 0}, {"1d", "6mo", 0}, {"1d", "1y", 0}};
  const auto& R = kR[qBound(0, m_body->frame->range, 5)];
  auto* ch = m_body->frame->chart;
  ch->setLegendTitle(fullName(m_symbol) + " · " + R.iv + " · Yahoo Finance", m_body->s.display() + " · " + R.iv);
  if (!market::find(m_symbol)) { ch->setMessage(QCoreApplication::translate("positioning", "No price feed for %1").arg(m_symbol)); ch->setLoading(false); return; }
  const QString want = m_symbol;
  const qint64 asked = QDateTime::currentMSecsSinceEpoch();
  const int req = ++m_chartReq;
  market::candles(m_symbol, R.iv, R.rg, this, [this, want, R, asked, req](const market::Series& s) {
    if (want != m_symbol || req != m_chartReq) return;
    // a cached answer arrives at once: keep the skeleton ≈0.5 s, as the reference shows while its chart loads
    if (const qint64 left = 500 - (QDateTime::currentMSecsSinceEpoch() - asked); left > 0 && m_body->frame->chart->isLoading()) {
      QTimer::singleShot(int(left), this, [this, want, R, s, req] { if (want == m_symbol && req == m_chartReq) applyChart(s, R.iv, R.days); });
      return;
    }
    applyChart(s, R.iv, R.days);
  });
}

void RetailModal::applyChart(const market::Series& s, const QString& iv, int days) {
  {
    const QString want = m_symbol;
    struct { QString iv; int days; } R{iv, days};
    auto* ch = m_body->frame->chart;
    if (!s.ok()) { ch->setLoading(false); ch->setMessage(QCoreApplication::translate("positioning", "Yahoo Finance: %1").arg(s.error)); return; }
    QVector<market::Bar> bars = s.bars;
    if (R.iv == "1d" && market::find(want) && market::find(want)->category == market::Category::Forex)
      for (int i = 1; i < bars.size(); ++i) {   // Yahoo's daily FX open ≈ close: a 24 h market opens at the previous close
        bars[i].o = bars[i - 1].c; bars[i].h = std::max(bars[i].h, bars[i].o); bars[i].l = std::min(bars[i].l, bars[i].o);
      }
    int vis = 0;
    if (R.days > 0) { const qint64 from = s.bars.last().t - qint64(R.days) * 86400; for (const auto& b : s.bars) if (b.t >= from) ++vis; }
    ch->setBars(bars, decimalsFor(m_symbol, s.bars.last().c), R.iv != "1d", s.timezone);
    ch->setVisibleCount(vis);
    if (!std::isfinite(m_body->last)) { m_body->last = s.bars.last().c; m_body->lastFromCandle = true; m_body->update(); }
  }
}

void RetailModal::setHover(const QPoint& pg) {
  auto* ch = m_body->frame->chart;
  const QPoint lp = ch->mapFrom(parentWidget(), pg);
  if (ch->rect().contains(lp)) ch->setHover(lp);
}
void RetailModal::setScroll(int px) { m_scroll->verticalScrollBar()->setValue(px); }
}  // namespace positioning
