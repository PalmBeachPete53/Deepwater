#include "pages/news/bank-research/Look.h"
#include "pages/positioning/bank-positioning/ForecastChart.h"
#include "ui/Theme.h"
#include "ui/Chart.h"
#include <QPainter>
#include <QPainterPath>
#include <QLocale>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QDateTime>
#include <QTimeZone>
#include <cmath>

namespace banks::pos {

ForecastChart::ForecastChart(QWidget* parent) : QWidget(parent) {
  m_anim.setDuration(500);
  m_anim.setStartValue(0.0); m_anim.setEndValue(1.0);
  m_anim.setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_progress = v.toReal(); update(); });
}
void ForecastChart::setBars(const QVector<::market::Bar>& b, int dec) { m_bars = b; m_dec = dec; m_msg.clear(); update(); }
void ForecastChart::setLines(const QList<Line>& l) { m_lines = l; update(); }
void ForecastChart::setLast(double p) { m_last = p; update(); }
void ForecastChart::animateIn() {
  // test hook DW_BANKPOS_PROGRESS=<0..1>: freeze the opening animation (reference frame t = 8.0 s ≈ 0.55)
  bool ok = false;
  const double fixed = qEnvironmentVariable("DW_BANKPOS_PROGRESS").toDouble(&ok);
  if (ok) { m_progress = qBound(0.0, fixed, 1.0); update(); return; }
  m_progress = 0; m_anim.stop(); m_anim.start();
}

QRectF ForecastChart::plotRect() const { return QRectF(10, 24, width() - 90, height() - 24 - 30); }

void ForecastChart::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF pr = plotRect();
  if (m_bars.isEmpty()) {
    p.setFont(theme::mono(10, QFont::Normal, 2)); p.setPen(theme::map("#5b5750"));
    p.drawText(rect(), Qt::AlignCenter, m_msg.isEmpty() ? QCoreApplication::translate("banks", "LOADING PRICES") : m_msg);
    return;
  }
  double lo = 1e300, hi = -1e300;
  for (const auto& b : m_bars) { lo = std::min(lo, b.l); hi = std::max(hi, b.h); }
  for (const auto& l : m_lines) { lo = std::min(lo, l.value); hi = std::max(hi, l.value); }
  if (std::isfinite(m_last)) { lo = std::min(lo, m_last); hi = std::max(hi, m_last); }
  if (hi <= lo) { hi = lo + 1; }
  auto Y = [&](double v) { return pr.bottom() - (v - lo) / (hi - lo) * pr.height(); };
  const int n = int(m_bars.size());
  const qreal step = pr.width() / std::max(1, n);
  auto X = [&](int i) { return pr.left() + (i + 0.5) * step; };
  const QFont axis = theme::mono(11);
  const banks::DwColor axisText{"#84807a"}, grid{"#151006"};

  // grid + right axis: range ends + nice ticks in between
  p.setFont(axis);
  const auto ticks = chart::niceTicks(lo, hi, 5);
  const QFontMetricsF fm(axis);
  QVector<qreal> used;
  auto label = [&](double v, qreal y) {
    for (qreal u : used) if (std::abs(u - y) < 16) return;
    used << y;
    p.setPen(axisText);
    p.drawText(QRectF(pr.right() + 10, y - 8, 70, 16), Qt::AlignVCenter | Qt::AlignLeft, QLocale(QLocale::English).toString(v, 'f', m_dec));
  };
  label(hi, Y(hi) + 2); label(lo, Y(lo) - 2);
  for (double t : ticks) {
    if (t <= lo || t >= hi) continue;
    p.setPen(QPen(QColor(grid), 1)); p.drawLine(QPointF(pr.left(), Y(t)), QPointF(pr.right(), Y(t)));
    label(t, Y(t));
  }
  p.setPen(QPen(theme::map("#3e3b1d"), 1));
  p.drawLine(QPointF(pr.left(), pr.bottom() + 0.5), QPointF(pr.right(), pr.bottom() + 0.5));

  // month labels (every other month, like the reference)
  p.setFont(theme::sans(11)); p.setPen(theme::map("#8e8b85"));
  // first bar's month at the left edge, then every second month (Oct Dec Feb … as in the reference)
  int lastMonth = -1, k = 0;
  for (int i = 0; i < n; ++i) {
    const QDate d = QDateTime::fromSecsSinceEpoch(m_bars[i].t, QTimeZone::UTC).date();
    if (d.month() == lastMonth) continue;
    lastMonth = d.month();
    if ((k++ % 2) != 0) continue;
    const QString s = i18n::locale().toString(d, "MMM");
    const qreal x = i == 0 ? pr.left() + 2 : X(i);
    p.drawText(QRectF(x - 20, pr.bottom() + 6, 40, 18), Qt::AlignCenter, s);
  }

  // candles
  p.setRenderHint(QPainter::Antialiasing, false);
  const banks::DwColor up{"#4cb4c0"}, down{"#c0726d"};
  const qreal bw = std::max<qreal>(1.0, std::min<qreal>(step * 0.6, 7));
  for (int i = 0; i < n; ++i) {
    const auto& b = m_bars[i];
    const bool isUp = b.c >= b.o;
    const QColor c = isUp ? up : down;
    const qreal x = X(i);
    p.setPen(QPen(QColor(c), 1));
    p.drawLine(QPointF(x, Y(b.h)), QPointF(x, Y(b.l)));
    const qreal y1 = Y(std::max(b.o, b.c)), y2 = Y(std::min(b.o, b.c));
    p.fillRect(QRectF(x - bw / 2, y1, bw, std::max<qreal>(1, y2 - y1)), c);
  }
  p.setRenderHint(QPainter::Antialiasing, true);

  // forecast lines: dashed, drawn in from the left; label box at the left end
  const qreal xEnd = pr.left() + (pr.right() - pr.left()) * m_progress;
  QFont lf = theme::sans(11, QFont::Medium);
  for (const auto& l : m_lines) {
    const qreal y = Y(l.value);
    QPen pen(l.color, 2); pen.setDashPattern({3, 2}); pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    p.drawLine(QPointF(pr.left(), y), QPointF(xEnd, y));
  }
  for (const auto& l : m_lines) {
    const qreal y = Y(l.value);
    const qreal bw = std::max<qreal>(125, QFontMetricsF(lf).horizontalAdvance(l.label) + 12);
    const QRectF box(pr.left() + 6, y - 21 < 0 ? y + 5 : y - 21, bw, 14);
    p.fillRect(box, l.color);
    p.save(); p.setClipRect(box);
    p.setFont(lf); p.setPen(banks::inkOn(l.color, "#141008"));
    p.drawText(box.adjusted(6, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, l.label);
    p.restore();
  }
  // last price
  if (std::isfinite(m_last)) {
    const qreal y = Y(m_last);
    QPen pen(theme::map("#54bc80"), 1.6); pen.setDashPattern({3, 2});
    p.setPen(pen);
    p.drawLine(QPointF(pr.left(), y), QPointF(xEnd, y));
    const QString t = QLocale(QLocale::English).toString(m_last, 'f', m_dec);
    const QFont tf = theme::mono(11);
    const qreal tw = QFontMetricsF(tf).horizontalAdvance(t) + 12;
    const qreal tx = std::min(xEnd + 6, pr.right() + 6);
    const QRectF tag(tx, y - 8, tw, 16);
    p.fillRect(tag, theme::map("#54bc80"));
    p.setFont(tf); p.setPen(banks::inkOn(theme::map("#54bc80"), "#ffffff"));
    p.drawText(tag, Qt::AlignCenter, t);
  }
}
}  // namespace banks::pos
