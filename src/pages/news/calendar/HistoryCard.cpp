#include "pages/news/calendar/HistoryCard.h"
#include "ui/Theme.h"
#include "pages/news/ThemeColor.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include "ui/Chart.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QLocale>
#include <cmath>

namespace news::pages {

namespace {
const news::pages::Dw kCardBg{"#050402"}, kCardBorder{"#272218"}, kPlotBorder{"#1c1b12"}, kGrid{"#100e0a"}, kZero{"#5e583f"};
const news::pages::Dw kGold{"#cfc496"}, kDimLbl{"#4f4d49"}, kAxis{"#94938f"}, kFoot{"#524f44"};
const news::pages::Dw kBarTop{"#ead16e"}, kBarBot{"#8d7d31"}, kNeg{"#a5a9aa"}, kHoverBar{"#ffe585"}, kBand{"#25200d"};
const news::pages::Dw kBtnOn{"#fdd913"}, kBtnOnText{"#1a1502"}, kBtnOff{"#8f8b7a"}, kBtnOffBg{"#120f04"}, kBtnBorder{"#2b2614"};
const news::pages::Dw kPos{"#8bc6cd"}, kNegTxt{"#c89c9c"}, kWhite{"#fefdf8"};
QString en(const QDate& d, const char* f) { return i18n::locale().toString(d, f); }   // dates in the interface language
QString num(double v, int d) { return QLocale(QLocale::English).toString(v, 'f', d); }
QString fmtV(double v, const QString& unit, int d = 1) {
  QString s = num(v, d); if (s == "-" + num(0, d)) s = num(0, d);
  return s + unit;
}
}

void HistoryCard::setLoading(const QString& title, const QString& country) {
  // a zoom drag begun on the previous row's card must not continue on this one (CalendarTable then kept the press)
  m_title = title; m_country = country; m_loaded = false; m_s = {}; m_hover = -1; m_zoomA = m_zoomB = -1; m_dragFrom = m_dragTo = -1;
}
void HistoryCard::setSeries(const news::Series& s) {
  m_s = s; m_loaded = true; m_hover = -1; m_zoomA = m_zoomB = -1; m_dragFrom = m_dragTo = -1;   // indices of the old series
  if (!s.title.isEmpty()) m_title = s.title;
  if (!s.country.isEmpty()) m_country = s.country;
}

QRectF HistoryCard::plotRect(const QRectF& c) const { return QRectF(c.x() + 11, c.y() + 59, c.width() - 23, 382); }
QRectF HistoryCard::barsRect(const QRectF& c) const {
  const QRectF p = plotRect(c);
  return QRectF(p.x() + 25, p.y() + 14, p.width() - 25 - 67, p.height() - 14 - 30);
}

void HistoryCard::visible(int& a, int& b) const {
  const int n = int(m_s.values.size());
  a = 0; b = n;
  if (n == 0) return;
  if (m_zoomA >= 0 && m_zoomB > m_zoomA) { a = std::max(0, m_zoomA); b = std::min(n, m_zoomB); return; }
  if (m_span == Max) return;
  const int years = m_span == Y5 ? 5 : m_span == Y10 ? 10 : 20;
  const QDate from = m_s.dates.last().addYears(-years);
  while (a < n - 1 && m_s.dates[a] <= from) ++a;
}

int HistoryCard::indexAt(const QRectF& card, double x) const {
  int a, b; visible(a, b);
  if (b <= a) return -1;
  const QRectF r = barsRect(card);
  const double pitch = r.width() / (b - a);
  const int i = a + int(std::floor((x - r.left()) / pitch));
  return std::clamp(i, a, b - 1);
}

QList<HistoryCard::Btn> HistoryCard::buttons(const QRectF& c) const {
  QList<Btn> out;
  const double y = c.y() + 22, h = 19;
  double x = c.right() - 12;
  const QList<QPair<int, QString>> spans{{Max, QCoreApplication::translate("news", "MAX")}, {Y20, QCoreApplication::translate("news", "20Y")}, {Y10, QCoreApplication::translate("news", "10Y")}, {Y5, QCoreApplication::translate("news", "5Y")}};
  for (const auto& [id, t] : spans) {
    const double w = std::max(t.size() == 2 ? 29.0 : 37.0, QFontMetricsF(theme::mono(10, QFont::Bold, 1.6)).horizontalAdvance(t) + 12);
    x -= w; out << Btn{QRectF(x, y, w, h), 10 + id, t};
  }
  x -= 9;
  const QFontMetricsF bfm(theme::mono(10, QFont::Bold, 1.6));   // widths follow the text (French is longer)
  const QString line = QCoreApplication::translate("news", "LINE"), bars = QCoreApplication::translate("news", "BARS");
  const double lw = std::max(45.0, bfm.horizontalAdvance(line) + 12), bw = std::max(45.0, bfm.horizontalAdvance(bars) + 12);
  x -= lw; out << Btn{QRectF(x, y, lw, h), 1, line};
  x -= bw; out << Btn{QRectF(x, y, bw, h), 0, bars};
  return out;
}

bool HistoryCard::wantsPointer(const QRectF& card, const QPointF& pos) const {
  for (const auto& b : buttons(card)) if (b.r.contains(pos)) return true;
  return false;
}

void HistoryCard::paint(QPainter& p, const QRectF& c) const {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  p.fillRect(c, kCardBg);
  p.setPen(kCardBorder); p.drawRect(c.adjusted(0.5, 0.5, -0.5, -0.5));
  // subtle warm glow at the top like the reference card
  QLinearGradient glow(c.topLeft(), c.topLeft() + QPointF(0, 60));
  glow.setColorAt(0, theme::map(24, 18, 4, 90)); glow.setColorAt(1, theme::map(0, 0, 0, 0));
  p.fillRect(c.adjusted(1, 1, -1, -c.height() + 60), glow);

  // header
  const QString country = m_country.toUpper();
  p.setFont(theme::mono(10, QFont::Bold, 2.4)); p.setPen(kGold);
  p.drawText(QRectF(c.x() + 11, c.y() + 10, 400, 16), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "%1 · RELEASE HISTORY").arg(country));
  p.setFont(theme::condensed(16, QFont::Bold)); p.setPen(kWhite);
  p.drawText(QRectF(c.x() + 11, c.y() + 28, 200, 20), Qt::AlignLeft | Qt::AlignVCenter, m_title);

  const double lx = c.x() + std::max(216.0, 11 + QFontMetricsF(theme::mono(10, QFont::Bold, 2.4)).horizontalAdvance(QCoreApplication::translate("news", "%1 · RELEASE HISTORY").arg(country)) + 22);
  if (!m_s.values.isEmpty()) {
    const QDate last = m_s.dates.last();
    const QString when = m_s.quarterly ? QCoreApplication::translate("news", "Q%1 %2").arg((last.month() - 1) / 3 + 1).arg(last.year())
                         : m_s.recorded ? en(last, "dd MMM yyyy").toUpper() : en(last, "MMM yyyy").toUpper();
    p.setFont(theme::mono(9, QFont::Normal, 2.4)); p.setPen(kDimLbl);
    p.drawText(QRectF(lx, c.y() + 12, 200, 14), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "LATEST · %1").arg(when));
    const int dec = m_s.unit == "K" ? 0 : 1;
    p.setFont(theme::mono(20, QFont::Bold)); p.setPen(kWhite);
    p.drawText(QRectF(lx, c.y() + 27, 140, 26), Qt::AlignLeft | Qt::AlignVCenter, fmtV(m_s.values.last(), m_s.unit, dec));
    // avg / low / high over the visible range
    int a, b; visible(a, b);
    double sum = 0; int lo = a, hi = a;
    for (int i = a; i < b; ++i) { sum += m_s.values[i]; if (m_s.values[i] < m_s.values[lo]) lo = i; if (m_s.values[i] > m_s.values[hi]) hi = i; }
    if (b > a) {
      QFont f = theme::mono(12); p.setFont(f);
      const QFontMetricsF fm(f); const QFont fb = theme::mono(12, QFont::Bold); const QFontMetricsF fmb(fb);
      double x = lx + 141; const double y = c.y() + 23, h = 18;
      // stop before the BARS/LINE buttons (narrow windows, longer French labels): later items are dropped
      const double limit = buttons(c).last().r.left() - 12;
      bool full = false;
      auto put = [&](const QString& t, const QColor& col, bool bold, double gap = 6) {
        if (full || x + (bold ? fmb : fm).horizontalAdvance(t) > limit) { full = true; return; }
        p.setFont(bold ? fb : f); p.setPen(col);
        p.drawText(QRectF(x, y, 300, h), Qt::AlignLeft | Qt::AlignVCenter, t);
        x += (bold ? fmb : fm).horizontalAdvance(t) + gap;
      };
      auto dl = [&](int i) { return m_s.quarterly ? QCoreApplication::translate("news", "Q%1 %2").arg((m_s.dates[i].month() - 1) / 3 + 1).arg(m_s.dates[i].year())
                                                  : en(m_s.dates[i], "MMM yyyy"); };
      put(QCoreApplication::translate("news", "avg"), theme::map("#a9a7a0"), false); put(fmtV(sum / (b - a), m_s.unit, dec), kWhite, true, 14);
      put(QCoreApplication::translate("news", "low"), theme::map("#a9a7a0"), false); put(fmtV(m_s.values[lo], m_s.unit, dec), kNegTxt, true); put(dl(lo), theme::map("#53514d"), false, 14);
      put(QCoreApplication::translate("news", "high"), theme::map("#a9a7a0"), false); put(fmtV(m_s.values[hi], m_s.unit, dec), kPos, true); put(dl(hi), theme::map("#53514d"), false);
    }
  }

  // buttons
  for (const auto& b : buttons(c)) {
    const bool on = b.id < 10 ? b.id == int(m_mode) : (b.id - 10) == int(m_span) && m_zoomA < 0;
    p.fillRect(b.r, on ? kBtnOn : (m_hoverBtn == b.id ? theme::map("#2a2408") : kBtnOffBg));
    if (!on) { p.setPen(kBtnBorder); p.drawRect(b.r.adjusted(0.5, 0.5, -0.5, -0.5)); }
    p.setFont(theme::mono(10, QFont::Bold, 1.6)); p.setPen(on ? (theme::isDefault() ? kBtnOnText.c() : theme::onAccent()) : kBtnOff.c());
    p.drawText(b.r, Qt::AlignCenter, b.text);
  }

  // plot frame
  const QRectF pr = plotRect(c), br = barsRect(c);
  p.setPen(kPlotBorder); p.drawRect(pr.adjusted(0.5, 0.5, -0.5, -0.5));
  int a, b; visible(a, b);
  if (!m_loaded || m_s.values.isEmpty()) {
    p.setFont(theme::mono(11, QFont::Normal, 1)); p.setPen(theme::map("#6b685f"));
    const QString msg = !m_loaded ? QCoreApplication::translate("news", "Loading release history…") : m_s.error;
    p.drawText(pr.adjusted(40, 0, -40, 0), Qt::AlignCenter | Qt::TextWordWrap, msg);
    p.restore(); return;
  }
  double lo = 0, hi = 0;
  for (int i = a; i < b; ++i) { lo = std::min(lo, m_s.values[i]); hi = std::max(hi, m_s.values[i]); }
  if (hi - lo < 1e-9) hi = lo + 1;
  auto ticks = chart::niceTicks(lo, hi, 6);
  if (!ticks.isEmpty()) { lo = std::min(lo, ticks.first()); hi = std::max(hi, ticks.last()); }
  const double step = ticks.size() > 1 ? ticks[1] - ticks[0] : (hi - lo);
  if (!ticks.isEmpty() && ticks.first() > lo + 1e-9) { ticks.prepend(ticks.first() - step); lo = ticks.first(); }
  if (!ticks.isEmpty() && ticks.last() < hi - 1e-9) { ticks.append(ticks.last() + step); hi = ticks.last(); }
  auto Y = [&](double v) { return br.bottom() - (v - lo) / (hi - lo) * br.height(); };
  const int dec = step < 0.1 ? 2 : step < 1 ? 1 : 0;
  p.setFont(theme::mono(11));
  for (double t : ticks) {
    const double y = std::round(Y(t)) + 0.5;
    p.setPen(std::abs(t) < 1e-9 ? kZero : kGrid);
    p.drawLine(QPointF(pr.left() + 1, y), QPointF(br.right() + 4, y));
    p.setPen(kAxis);
    p.drawText(QRectF(c.right() - 69, y - 9, 60, 18), Qt::AlignLeft | Qt::AlignVCenter, fmtV(t, m_s.unit, dec));
  }
  const int n = b - a;
  const double pitch = br.width() / std::max(1, n);
  const double bw = std::max(1.0, std::min(pitch - (pitch > 3 ? 1.4 : 0.4), 14.0));
  // hover band
  if (m_hover >= a && m_hover < b) {
    const double x = br.left() + (m_hover - a + 0.5) * pitch;
    p.fillRect(QRectF(x - std::max(4.0, pitch / 2 + 1), br.top() - 6, std::max(8.0, pitch + 2), br.height() + 12), kBand);
  }
  // drag selection
  if (m_dragFrom >= 0 && m_dragTo >= 0 && m_dragTo != m_dragFrom) {
    const int s = std::min(m_dragFrom, m_dragTo), e = std::max(m_dragFrom, m_dragTo);
    p.fillRect(QRectF(br.left() + (s - a) * pitch, br.top() - 6, (e - s + 1) * pitch, br.height() + 12), theme::map(253, 217, 19, 34));
  }
  const double zeroY = Y(0);
  if (m_mode == Bars) {
    QLinearGradient g(QPointF(0, br.top()), QPointF(0, zeroY));
    g.setColorAt(0, kBarTop); g.setColorAt(1, kBarBot);
    for (int i = a; i < b; ++i) {
      const double v = m_s.values[i];
      const double x = br.left() + (i - a + 0.5) * pitch - bw / 2;
      const double y = Y(v);
      QRectF r(x, std::min(y, zeroY), bw, std::max(1.0, std::abs(zeroY - y)));
      if (i == m_hover) p.fillRect(r, kHoverBar);
      else if (v < 0) p.fillRect(r, kNeg);
      else {   // each bar: bright at its top, darker at the base (reference)
        QLinearGradient gb(QPointF(0, r.top()), QPointF(0, r.bottom()));
        gb.setColorAt(0, kBarTop); gb.setColorAt(1, theme::mix(kBarBot, kBarTop, 0.35));
        p.fillRect(r, gb);
      }
    }
  } else {
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    for (int i = a; i < b; ++i) {
      const QPointF pt(br.left() + (i - a + 0.5) * pitch, Y(m_s.values[i]));
      if (i == a) path.moveTo(pt); else path.lineTo(pt);
    }
    QPainterPath area = path;
    area.lineTo(br.left() + (b - 1 - a + 0.5) * pitch, zeroY); area.lineTo(br.left() + 0.5 * pitch, zeroY); area.closeSubpath();
    QLinearGradient g(QPointF(0, br.top()), QPointF(0, zeroY));
    g.setColorAt(0, theme::map(234, 209, 110, 90)); g.setColorAt(1, theme::map(234, 209, 110, 0));
    p.fillPath(area, g);
    p.setPen(QPen(kBarTop.c(), 1.6)); p.drawPath(path);
    if (m_hover >= a && m_hover < b) {
      const QPointF pt(br.left() + (m_hover - a + 0.5) * pitch, Y(m_s.values[m_hover]));
      p.setPen(Qt::NoPen); p.setBrush(kHoverBar); p.drawEllipse(pt, 3.5, 3.5);
      p.setBrush(Qt::NoBrush);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
  }
  // x labels: about every 68 px
  p.setFont(theme::mono(11)); p.setPen(theme::map("#969692"));
  const int every = std::max(1, int(std::ceil(64.0 / pitch)));   // reference: a label every 11 months at 20Y
  for (int i = a; i < b; i += every) {
    const double x = br.left() + (i - a + 0.5) * pitch;
    const QString t = m_s.quarterly ? QCoreApplication::translate("news", "Q%1 %2").arg((m_s.dates[i].month() - 1) / 3 + 1).arg(m_s.dates[i].year() % 100, 2, 10, QChar('0'))
                                    : en(m_s.dates[i], "MMM yy");
    if (x + 30 > br.right() + 30) break;
    p.drawText(QRectF(x - 34, br.bottom() + 8, 68, 16), Qt::AlignCenter, t);
  }
  // footer
  const QString fromTo = m_s.quarterly ? QCoreApplication::translate("news", "Q%1 %2 ⟶ Q%3 %4").arg((m_s.dates[a].month() - 1) / 3 + 1).arg(m_s.dates[a].year())
                                             .arg((m_s.dates[b - 1].month() - 1) / 3 + 1).arg(m_s.dates[b - 1].year())
                                       : en(m_s.dates[a], "MMM yyyy").toUpper() + " ⟶ " + en(m_s.dates[b - 1], "MMM yyyy").toUpper();
  p.setFont(theme::mono(9, QFont::Normal, 1.8)); p.setPen(kFoot);
  p.drawText(QRectF(c.x() + 11, c.y() + 447, c.width() * 0.7, 14), Qt::AlignLeft | Qt::AlignVCenter,
             (m_s.recorded ? QCoreApplication::translate("news", "%1 RELEASES · %2 · %3") : m_s.quarterly ? QCoreApplication::translate("news", "%1 QUARTERS · %2 · %3") : QCoreApplication::translate("news", "%1 MONTHS · %2 · %3"))
                 .arg(n).arg(fromTo, QCoreApplication::translate("news", m_s.source.toUtf8().constData()).toUpper()));
  p.drawText(QRectF(c.right() - 400, c.y() + 447, 389, 14), Qt::AlignRight | Qt::AlignVCenter,
             m_zoomA >= 0 ? QCoreApplication::translate("news", "DOUBLE-CLICK TO RESET ZOOM") : QCoreApplication::translate("news", "DRAG ON THE CHART TO ZOOM"));

  // tooltip
  if (m_hover >= a && m_hover < b && m_dragFrom < 0) {
    const double x = br.left() + (m_hover - a + 0.5) * pitch;
    const double y = Y(m_s.values[m_hover]);
    const QString d = m_s.quarterly ? QCoreApplication::translate("news", "Q%1 %2").arg((m_s.dates[m_hover].month() - 1) / 3 + 1).arg(m_s.dates[m_hover].year())
                                    : en(m_s.dates[m_hover], "dd MMM yyyy");
    const QString v = num(m_s.values[m_hover], m_s.unit == "K" ? 0 : 3) + m_s.unit;
    const QFont f1 = theme::mono(11), f2 = theme::mono(11, QFont::Bold);
    const double w = std::max(QFontMetricsF(f1).horizontalAdvance(d), QFontMetricsF(f2).horizontalAdvance("■ " + m_title + " " + v)) + 26;
    QRectF box(x + 24, std::min(y, zeroY) - 82, w, 56);
    if (box.right() > br.right()) box.moveRight(x - 24);
    if (box.top() < br.top()) box.moveTop(br.top());
    p.fillRect(box, theme::map("#0d0802"));
    p.setPen(theme::map("#5f5123")); p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setFont(f1); p.setPen(theme::map("#a3a3a1"));
    p.drawText(QRectF(box.x() + 12, box.y() + 9, w, 16), Qt::AlignLeft | Qt::AlignVCenter, d);
    p.fillRect(QRectF(box.x() + 12, box.y() + 33, 7, 7), kBtnOn);
    p.setFont(f2); p.setPen(kWhite);
    p.drawText(QRectF(box.x() + 25, box.y() + 28, w, 16), Qt::AlignLeft | Qt::AlignVCenter, m_title + "  " + v);
  }
  p.restore();
}

bool HistoryCard::mouseMove(const QRectF& card, const QPointF& pos) {
  int hb = -1;
  for (const auto& b : buttons(card)) if (b.r.contains(pos)) hb = b.id;
  bool ch = hb != m_hoverBtn; m_hoverBtn = hb;
  const QRectF br = barsRect(card);
  int h = -1;
  if (!m_s.values.isEmpty() && br.adjusted(0, -8, 0, 8).contains(pos)) h = indexAt(card, pos.x());
  if (m_dragFrom >= 0) { const int t = indexAt(card, pos.x()); if (t != m_dragTo) { m_dragTo = t; ch = true; } }
  if (h != m_hover) { m_hover = h; ch = true; }
  return ch;
}
bool HistoryCard::mousePress(const QRectF& card, const QPointF& pos) {
  for (const auto& b : buttons(card))
    if (b.r.contains(pos)) {
      if (b.id < 10) m_mode = Mode(b.id); else { m_span = Span(b.id - 10); m_zoomA = m_zoomB = -1; }
      m_hover = -1; return true;
    }
  if (!m_s.values.isEmpty() && barsRect(card).adjusted(0, -8, 0, 8).contains(pos)) {
    m_dragFrom = m_dragTo = indexAt(card, pos.x()); return true;
  }
  return false;
}
bool HistoryCard::mouseRelease(const QRectF& card, const QPointF& pos) {
  if (m_dragFrom < 0) return false;
  const int to = indexAt(card, pos.x());
  const int s = std::min(m_dragFrom, to), e = std::max(m_dragFrom, to);
  if (e - s >= 3) { m_zoomA = s; m_zoomB = e + 1; }
  m_dragFrom = m_dragTo = -1;
  return true;
}
bool HistoryCard::mouseDoubleClick(const QRectF& card, const QPointF& pos) {
  if (!barsRect(card).contains(pos) || m_zoomA < 0) return false;
  m_zoomA = m_zoomB = -1; return true;
}
bool HistoryCard::leave() {
  const bool ch = m_hover >= 0 || m_hoverBtn >= 0;
  m_hover = -1; m_hoverBtn = -1;
  return ch;
}
void HistoryCard::hoverFraction(const QRectF& card, double f) {
  int a, b; visible(a, b);
  if (b > a) m_hover = a + std::clamp(int(f * (b - a)), 0, b - a - 1);
  (void)card;
}
}  // namespace news::pages
