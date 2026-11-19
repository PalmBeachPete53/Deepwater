#include "pages/positioning/CandleChart.h"
#include "ui/Theme.h"
#include "ui/Chart.h"
#include "ui/Format.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>
#include <QLocale>
#include <cmath>

namespace positioning::pos {

CandleChart::Style CandleChart::modalStyle() {
  Style s;
  s.bg = theme::map(0x0a, 0x0a, 0x0a); s.up = theme::map(0x00, 0xc2, 0xe4); s.down = theme::map(0xff, 0x65, 0x6b);
  s.axisText = theme::map(0xd1, 0xd4, 0xdc); s.grid = theme::map(0x16, 0x16, 0x16); s.cross = theme::map(0x9a, 0x9a, 0x9a);
  s.tagBg = theme::map(0x2a, 0x2e, 0x39); s.tagFg = theme::map(0xf0, 0xf3, 0xfa);
  s.legendText = theme::map(0xe5, 0xe9, 0xf8); s.legendLabel = theme::map(0xc8, 0xcb, 0xd4);
  s.axisFont = theme::sans(13, QFont::Medium); s.timeFont = theme::sans(13, QFont::DemiBold);
  s.legendFont = theme::sans(16, QFont::Medium); s.tagFont = theme::sans(13, QFont::DemiBold);
  s.axisW = 74; s.timeH = 30; s.legendH = 40; s.padTop = 14;
  return s;
}
CandleChart::Style CandleChart::cotStyle() {
  Style s;
  s.bg = theme::map(0, 0, 0, 0); s.up = theme::map(0x22, 0xd3, 0xee); s.down = theme::map(0xef, 0x5f, 0x5f);
  s.axisText = theme::map(0x87, 0x86, 0x80); s.grid = theme::map(0x1c, 0x1a, 0x10); s.cross = theme::map(0x8a, 0x84, 0x55);
  s.tagBg = theme::map(0x48, 0x3c, 0x04); s.tagFg = theme::map(0xeb, 0xe4, 0xce);
  s.legendText = theme::map(0xf0, 0xec, 0xe0); s.legendLabel = theme::map(0x92, 0x90, 0x86);
  s.axisFont = theme::mono(11); s.timeFont = theme::mono(11, QFont::Medium); s.legendFont = theme::mono(11);
  s.tagFont = theme::mono(11, QFont::Bold);
  s.axisW = 58; s.timeH = 28; s.legendH = 0; s.padTop = 6; s.boldMonths = true; s.darkTagOnUp = true; s.tickSpacing = 50; s.rightPadBars = 5; s.timeLabelGap = 80;
  return s;
}

CandleChart::CandleChart(QWidget* parent) : QWidget(parent) {
  m_s = cotStyle();
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void CandleChart::setBars(const QVector<market::Bar>& b, int dec, bool intraday, const QString& timezone) {
  m_bars = b; m_dec = dec; m_intraday = intraday; m_tz = timezone; m_loading = false; m_msg.clear(); update();
}

// Intraday bars in the viewer's clock; a daily bar is its session's day: its open stamp converted to the local zone
// falls on the day before west of the exchange (a 00:00 UTC bar is the previous evening in New York)
QDate CandleChart::dayOf(qint64 t) const {
  return m_intraday ? QDateTime::fromSecsSinceEpoch(t).toLocalTime().date() : market::sessionDay(m_tz, t);
}

QRectF CandleChart::plotRect() const {
  return QRectF(m_s.padLeft, m_s.legendH + m_s.padTop, width() - m_s.axisW - m_s.padLeft,
                height() - m_s.legendH - m_s.padTop - m_s.timeH);
}

QString CandleChart::timeTag(qint64 t) const {
  const QDateTime d = QDateTime::fromSecsSinceEpoch(t).toLocalTime();
  const QLocale loc = i18n::locale();
  const QString day = loc.toString(dayOf(t), "d MMM ''yy").remove('.');
  if (m_intraday) return day + "  " + d.toString("HH:mm");
  return day;
}

void CandleChart::paintEvent(QPaintEvent*) {
  QPainter p(this);
  if (m_s.bg.alpha() > 0) p.fillRect(rect(), m_s.bg);
  const QRectF plot = plotRect();
  if (m_loading || m_bars.isEmpty()) {
    if (m_loading) {   // skeleton columns while the history loads (t001.5: ≈25 wide warm-grey bars from the bottom)
      p.fillRect(rect(), theme::map(0x0d, 0x0b, 0x07));
      const int n = 25; const qreal w = width() / qreal(n);
      for (int i = 0; i < n; ++i) {
        const qreal h = height() * (0.42 + 0.27 * std::fabs(std::sin(i * 1.7 + 0.4)));
        p.fillRect(QRectF(i * w + w * 0.1, height() - h, w * 0.8, h - 2), theme::map(0x17, 0x14, 0x10));
      }
    }
    if (!m_msg.isEmpty() || !m_loading) {
      p.setFont(theme::mono(11)); p.setPen(m_s.axisText);
      p.drawText(rect(), Qt::AlignCenter, m_msg.isEmpty() ? QCoreApplication::translate("positioning", "No price history") : m_msg);
    }
    return;
  }
  const int n = m_bars.size();
  const int first = m_visible > 0 ? std::max(0, n - m_visible) : 0;
  const int cnt = n - first;
  double lo = INFINITY, hi = -INFINITY;
  for (int i = first; i < n; ++i) { lo = std::min(lo, m_bars[i].l); hi = std::max(hi, m_bars[i].h); }
  // relative floor on the pad: with flat candles (hi == lo) an absolute 1e-12 vanishes on prices above ~1,000, the
  // range stayed empty and the axis ticks below never ended
  const double pad = std::max((hi - lo) * 0.08, std::max(std::abs(hi), 1.0) * 1e-6); lo -= pad; hi += pad;
  const qreal slot = plot.width() / std::max(1, cnt + m_s.rightPadBars);
  auto X = [&](int i) { return plot.left() + (i - first + 0.5) * slot; };
  auto Y = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };

  // price axis
  const auto ticks = chart::niceTicks(lo, hi, std::max(3, int(plot.height() / m_s.tickSpacing)));
  p.setFont(m_s.axisFont);
  const QFontMetricsF am(m_s.axisFont);
  for (double t : ticks) {
    const qreal y = Y(t);
    if (y < plot.top() + 4 || y > plot.bottom() - 4) continue;
    // labels under the last-price tag or the crosshair tag are hidden (TradingView behaviour)
    if (std::abs(y - Y(m_bars.last().c)) < 15) continue;
    if (m_hasHover && plot.contains(m_hover) && std::abs(y - m_hover.y()) < 15) continue;
    if (m_s.gridLines) { p.setPen(QPen(m_s.grid, 1)); p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y)); }
    p.setPen(m_s.axisText);
    p.drawText(QRectF(plot.right() + 4, y - 9, m_s.axisW - 10, 18), Qt::AlignRight | Qt::AlignVCenter, fmt::num(t, m_dec, false));
  }

  // time axis: month starts (daily) / day starts (intraday), thinned to keep ≥ 54 px between labels
  p.setFont(m_s.timeFont);
  const QFontMetricsF tm(m_s.timeFont);
  const QLocale loc = i18n::locale();
  QList<QPair<qreal, QString>> labels; QList<bool> major;
  qreal lastX = -1e9;
  for (int i = first; i < n; ++i) {
    const QDate d = dayOf(m_bars[i].t);
    const QDate pd = i > first ? dayOf(m_bars[i - 1].t) : QDate();
    QString s; bool mj = false;
    if (m_intraday) {
      if (pd.isValid() && d != pd) s = QString::number(d.day());
    } else {
      if (pd.isValid() && d.month() != pd.month()) { s = d.month() == 1 ? QString::number(d.year()) : [&] { QString m = loc.monthName(d.month(), QLocale::ShortFormat).remove('.'); if (!m.isEmpty()) m[0] = m[0].toUpper(); return m; }(); mj = true; }
      else if (i > first && (i - first) % std::max(1, int(std::ceil(60 / slot))) == 0) s = QString::number(d.day());
    }
    if (s.isEmpty()) continue;
    const qreal x = X(i);
    if (x - lastX < m_s.timeLabelGap) { if (mj && !labels.isEmpty() && !major.last()) { labels.removeLast(); major.removeLast(); } else continue; }
    if (x > plot.right() - 14) continue;
    labels << qMakePair(x, s); major << mj; lastX = x;
  }
  QRectF hoverTag;   // time labels under the crosshair's date tag are hidden, as on TradingView
  if (m_hasHover && plot.contains(m_hover)) {
    const int hb = qBound(first, first + int((m_hover.x() - plot.left()) / slot), n - 1);
    const qreal w = QFontMetricsF(m_s.tagFont).horizontalAdvance(timeTag(m_bars[hb].t)) + 16;
    if (first + int((m_hover.x() - plot.left()) / slot) <= n - 1) hoverTag = QRectF(X(hb) - w / 2 - 6, plot.bottom(), w + 12, m_s.timeH);
  }
  for (int k = 0; k < labels.size(); ++k) {
    if (!hoverTag.isNull() && std::abs(labels[k].first - hoverTag.center().x()) < hoverTag.width() / 2 + tm.horizontalAdvance(labels[k].second) / 2) continue;
    QFont f = m_s.timeFont; if (m_s.boldMonths && major[k]) f.setWeight(QFont::Bold);
    p.setFont(f); p.setPen(major[k] && m_s.boldMonths ? m_s.legendLabel : m_s.axisText);
    p.drawText(QRectF(labels[k].first - 40, plot.bottom() + 2, 80, m_s.timeH - 2), Qt::AlignCenter, labels[k].second);
  }

  // candles
  p.setRenderHint(QPainter::Antialiasing, false);
  const qreal bw = std::max<qreal>(1.0, slot * m_s.bodyFrac);
  for (int i = first; i < n; ++i) {
    const auto& b = m_bars[i];
    const QColor c = b.c >= b.o ? m_s.up : m_s.down;
    const qreal x = X(i);
    p.setPen(QPen(c, 1)); p.drawLine(QPointF(x, Y(b.h)), QPointF(x, Y(b.l)));
    const qreal y0 = Y(std::max(b.o, b.c)), y1 = Y(std::min(b.o, b.c));
    p.fillRect(QRectF(x - bw / 2, y0, bw, std::max<qreal>(1.0, y1 - y0)), c);
  }

  // last price line + tag
  const auto& last = m_bars.last();
  const QColor lc = last.c >= last.o ? m_s.up : m_s.down;
  const qreal ly = Y(last.c);
  p.setPen(QPen(lc, 1, Qt::DotLine)); p.drawLine(QPointF(plot.left(), ly), QPointF(plot.right(), ly));
  auto tag = [&](qreal y, const QString& t, const QColor& bg, const QColor& fg) {
    const QFontMetricsF fm(m_s.tagFont);
    const QRectF r(plot.right() + 1, y - 10, m_s.axisW - 3, 20);
    p.fillRect(r, bg); p.setFont(m_s.tagFont); p.setPen(fg);
    p.drawText(r.adjusted(0, 0, -5, 0), Qt::AlignRight | Qt::AlignVCenter, t);
  };
  const QColor lastFg = (m_s.darkTagOnUp && last.c >= last.o) ? theme::map(0x05, 0x08, 0x0a) : theme::map(QColor(Qt::white));
  tag(ly, fmt::num(last.c, m_dec, false), lc, lastFg);

  // crosshair + legend
  int hi_ = n - 1;
  if (m_hasHover && plot.contains(m_hover)) {
    const int raw = first + int((m_hover.x() - plot.left()) / slot);
    hi_ = qBound(first, raw, n - 1);
    const bool beyond = raw > n - 1;   // in the empty space right of the last candle: no date tag, line follows the mouse
    const qreal cx = beyond ? m_hover.x() : X(hi_);
    QPen dp(m_s.cross, 1, Qt::CustomDashLine); dp.setDashPattern({3, 3});
    p.setPen(dp);
    p.drawLine(QPointF(cx, plot.top()), QPointF(cx, plot.bottom()));
    p.drawLine(QPointF(plot.left(), m_hover.y()), QPointF(plot.right(), m_hover.y()));
    const double pv = lo + (plot.bottom() - m_hover.y()) / plot.height() * (hi - lo);
    tag(m_hover.y(), fmt::num(pv, m_dec, false), m_s.tagBg, m_s.tagFg);
    const QString tt = beyond ? QString() : timeTag(m_bars[hi_].t);
    const QFontMetricsF fm(m_s.tagFont);
    const qreal w = fm.horizontalAdvance(tt) + 16;
    QRectF r(cx - w / 2, plot.bottom() + 2, w, m_s.timeH - 4);
    if (r.right() > plot.right()) r.moveRight(plot.right());
    if (r.left() < plot.left()) r.moveLeft(plot.left());
    if (!tt.isEmpty()) { p.fillRect(r, m_s.tagBg); p.setFont(m_s.tagFont); p.setPen(m_s.tagFg); p.drawText(r, Qt::AlignCenter, tt); }
  }
  if (m_s.legendH > 0) {
    p.setRenderHint(QPainter::Antialiasing);
    const auto& b = m_bars[hi_];
    const double prev = hi_ > 0 ? m_bars[hi_ - 1].c : b.o;
    const QColor vc = b.c >= b.o ? m_s.up : m_s.down;
    qreal x = m_s.padLeft + 10;
    const qreal base = m_s.legendH / 2.0 + 7;
    const QFont lf = theme::sans(m_s.legendFont.pixelSize() - 2, QFont::Medium);
    const QFontMetricsF lm(lf);
    const double ch = b.c - prev;
    const QStringList vals{fmt::num(b.o, m_dec, false), fmt::num(b.h, m_dec, false), fmt::num(b.l, m_dec, false),
                           fmt::num(b.c, m_dec, false), fmt::signedNum(ch, m_dec) + " (" + fmt::pct(prev != 0 ? ch / prev * 100 : 0, 2) + ")"};
    qreal partsW = 0; for (const auto& v : vals) partsW += lm.horizontalAdvance("OO" + v) + 7;
    // narrow charts: shorter title, then no change part, so the OHLC values are never cut
    QString title = m_title;
    const QFontMetricsF tm2(m_s.legendFont);
    if (x + tm2.horizontalAdvance(title) + 14 + partsW > width() - 8 && !m_shortTitle.isEmpty()) title = m_shortTitle;
    const bool showChange = x + tm2.horizontalAdvance(title) + 14 + partsW <= width() - 8;
    p.setFont(m_s.legendFont); p.setPen(m_s.legendText);
    p.drawText(QPointF(x, base), title); x += tm2.horizontalAdvance(title) + 14;
    p.setFont(lf);
    auto part = [&](const QString& k, const QString& v) {
      p.setPen(m_s.legendLabel); p.drawText(QPointF(x, base), k); x += lm.horizontalAdvance(k);
      p.setPen(vc); p.drawText(QPointF(x, base), v); x += lm.horizontalAdvance(v) + 7;
    };
    // O H L C (French: O H B C)
    part(QCoreApplication::translate("positioning", "O"), vals[0]); part(QCoreApplication::translate("positioning", "H"), vals[1]); part(QCoreApplication::translate("positioning", "L"), vals[2]); part(QCoreApplication::translate("positioning", "C"), vals[3]);
    if (showChange) part("", vals[4]);
  }
}

void CandleChart::mouseMoveEvent(QMouseEvent* e) { m_hover = e->position(); m_hasHover = true; update(); }
void CandleChart::leaveEvent(QEvent*) { m_hasHover = false; update(); }
}
