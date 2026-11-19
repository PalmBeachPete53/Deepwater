#include "pages/market/technical/SignalFile.h"
#include "pages/market/technical/MarketKit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QTextLayout>
#include <QCoreApplication>
#include <cmath>

namespace {
constexpr double kTop = 37;          // widget top in page coordinates; all y below are page y − kTop
double Y(double pageY) { return pageY - kTop; }
}

SignalFile::SignalFile(QWidget* parent) : QWidget(parent) {
  setToolTip(footnote());
  m_blink.setInterval(1000);
  connect(&m_blink, &QTimer::timeout, this, [this] { m_cursor = !m_cursor; update(QRect(0, 0, width(), int(Y(70)))); });
  m_anim.setInterval(33);
  connect(&m_anim, &QTimer::timeout, this, [this] {
    if (m_grow.elapsed() > 620) { m_growing = false; m_anim.stop(); }
    const double R = radius();
    update(QRect(0, int(Y(160)), width(), int(2 * R + 80)));
  });
}

QString SignalFile::footnote() {
  return QCoreApplication::translate("technical", 
      "Radar: each read on a 0–100 scale where 50 is neutral — trend from the EMA20/EMA50 gap, momentum is RSI(14), "
      "volatility and volume against the instrument's own baseline.\n"
      "Rules (daily candles, computed by Deepwater): trend = (EMA20 − EMA50) / EMA50, ±0.10 % neutral band; "
      "RSI ≥ 70 overbought, ≤ 30 oversold; volatility = ATR(14) % percentile over 252 days, ≥ 80 high, ≤ 20 low "
      "(detail: 20-day realised vol, annualised); volume = last full day / 20-day mean, ≥ ×1.50 above, < ×0.75 below "
      "(ETF proxy where shown); structure = last two 5-bar swing highs and lows; liquidity = equal swing highs/lows "
      "clustered within max(0.06 %, ¼ ATR), untested until traded through, near = within 1 ATR, swept = pierced and "
      "closed back within the last 3 days.");
}

void SignalFile::setInstrument(const QString& id, const QString& name, const QString& price,
                               const analytics::TechRead& r, bool animate) {
  const bool changed = id != m_id;
  QVector<heat::Cell> cells;
  for (int c = 0; c < heat::ColumnCount; ++c) cells << heat::cell(r, heat::Column(c));
  // the page re-selects the same instrument on every table rebuild (a scan rebuilds every 120 ms): nothing to repaint
  // when nothing this file draws changed
  auto same = [&] {
    if (changed || name != m_name || price != m_price || r.trend != m_read.trend || cells.size() != m_cells.size()) return false;
    for (int k = 0; k < 6; ++k) if (r.radar[k] != m_read.radar[k]) return false;
    for (int c = 0; c < cells.size(); ++c) {
      const auto &a = cells[c], &b = m_cells[c];
      if (a.word != b.word || a.detail != b.detail || a.fill != b.fill || a.text != b.text || a.fileValue != b.fileValue ||
          a.fileColor != b.fileColor || a.fileDetail != b.fileDetail) return false;
    }
    return true;
  };
  if (same()) { m_read = r; return; }
  m_id = id; m_name = name; m_price = price; m_read = r;
  m_cells = cells;
  if (changed && animate && isVisible()) { m_growing = true; m_grow.start(); m_anim.start(); }
  update();
}
double SignalFile::radius() const { return std::clamp((width() - 170.0) / 2.0, 60.0, 110.0); }
int SignalFile::preferredHeight() const {
  const double rowsY = 195 + 2 * radius() + 69;
  const double foot = QFontMetricsF(theme::sans(10)).boundingRect(QRectF(0, 0, width(), 4000), Qt::TextWordWrap, footnote()).height();
  return int(Y(rowsY + 186) + foot + 16);
}
void SignalFile::setPrice(const QString& p) { if (p == m_price) return; m_price = p; update(QRect(0, int(Y(70)), width(), 40)); }
void SignalFile::clear() { m_id.clear(); m_cells.clear(); m_anim.stop(); m_growing = false; update(); }
void SignalFile::showEvent(QShowEvent*) { m_blink.start(); }
void SignalFile::hideEvent(QHideEvent*) { m_blink.stop(); m_anim.stop(); m_growing = false; }

void SignalFile::paintEvent(QPaintEvent*) {
  if (m_id.isEmpty()) return;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  const double W = width();
  // id line + blinking cursor
  const QFont fid = theme::mono(10, QFont::Normal, 3.0);
  p.setFont(fid); p.setPen(theme::map("#6b695c"));
  const QString idl = QCoreApplication::translate("technical", "SIGNAL FILE · %1").arg(m_id.toUpper());
  p.drawText(QPointF(0, Y(61)), idl);
  if (m_cursor) p.fillRect(QRectF(mkit::textWidth(fid, idl) + 1, Y(51), 7, 12), theme::pal().accent);
  // name + price
  const QFont fn = theme::mono(32, QFont::Bold, 1.0);
  p.setFont(fn); p.setPen(theme::map("#fffffa"));
  QString nm = m_name.toUpper();
  const QFont fpr = theme::mono(17);
  double nw = mkit::textWidth(fn, nm);
  if (nw + 14 + mkit::textWidth(fpr, m_price) > W) {   // long names: shrink to fit the panel
    QFont f2 = fn; f2.setPixelSize(std::max(16, int(32 * (W - 14 - mkit::textWidth(fpr, m_price)) / nw)));
    p.setFont(f2); nw = mkit::textWidth(f2, nm);
  }
  p.drawText(QPointF(0, Y(101)), nm);
  p.setFont(fpr); p.drawText(QPointF(nw + 12, Y(101)), m_price);
  // trend pill
  const auto& tc = m_cells[heat::Trend];
  QColor pc = m_read.trend == analytics::Lean::Bull ? theme::map("#67e3eb") : m_read.trend == analytics::Lean::Bear ? theme::map("#f59a94") : theme::map("#a3a09a");
  const QFont fpl = theme::mono(11, QFont::Bold, 2.0);
  const QString pill = m_read.trend == analytics::Lean::Bull ? QCoreApplication::translate("technical", "TREND BULLISH")
                      : m_read.trend == analytics::Lean::Bear ? QCoreApplication::translate("technical", "TREND BEARISH")
                                                              : QCoreApplication::translate("technical", "TREND NEUTRAL");
  Q_UNUSED(tc);
  const QRectF pr(1, Y(114), mkit::textWidth(fpl, pill) - 2 + 18, 21);
  p.setPen(QPen(theme::alpha(pc, 120), 1)); p.setBrush(theme::mix(theme::map("#050402"), pc, 0.12));
  p.drawRoundedRect(pr.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  p.setFont(fpl); p.setPen(pc);
  p.drawText(QPointF(pr.left() + 8, pr.center().y() + QFontMetricsF(fpl).capHeight() / 2), pill);
  // gold rule fading out
  QLinearGradient g(0, 0, W, 0); g.setColorAt(0, theme::map("#a19574")); g.setColorAt(1, theme::map(161, 149, 116, 0));
  p.fillRect(QRectF(0, Y(146), W, 1), g);

  // radar: R = 88 at the reference width (346), scales with the panel width (60..110)
  const double R = radius();
  const QPointF c(W / 2, Y(195 + R));
  const double rowsY = 195 + 2 * R + 69;            // 440 at the reference size
  auto vtx = [&](int axis, double r) { const double a = -M_PI / 2 + axis * M_PI / 3; return QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a)); };
  for (int ring = 5; ring >= 1; --ring) {
    QPolygonF hex; for (int k = 0; k < 6; ++k) hex << vtx(k, R * ring / 5.0);
    p.setPen(QPen(ring == 5 ? theme::map("#3b3628") : theme::map("#262110"), 1));
    p.setBrush(theme::map(250, 204, 21, ring % 2 ? 5 : 9));
    p.drawPolygon(hex);
  }
  p.setPen(QPen(theme::map("#262110"), 1));
  for (int k = 0; k < 6; ++k) p.drawLine(c, vtx(k, R));
  static const char* labels[] = {QT_TRANSLATE_NOOP("technical", "Trend"), QT_TRANSLATE_NOOP("technical", "Liquidity"), QT_TRANSLATE_NOOP("technical", "Structure"), QT_TRANSLATE_NOOP("technical", "Volume"),
                                 QT_TRANSLATE_NOOP("technical", "Volatility"), QT_TRANSLATE_NOOP("technical", "Momentum")};
  const QFont fl = theme::sans(11);
  p.setFont(fl); p.setPen(theme::map("#b6b5b0"));
  const QFontMetricsF ml(fl);
  for (int k = 0; k < 6; ++k) {
    const QPointF v = vtx(k, R);
    const QString lab = QCoreApplication::translate("technical", labels[k]);
    const double tw = ml.horizontalAdvance(lab);
    QPointF at;
    if (k == 0) at = QPointF(c.x() - tw / 2, c.y() - R - 23 + 4);
    else if (k == 3) at = QPointF(c.x() - tw / 2, c.y() + R + 33 + 4);
    else if (k < 3) at = QPointF(v.x() + 20, v.y() + (k == 1 ? -6 : 16) + 4);
    else at = QPointF(v.x() - 20 - tw, v.y() + (k == 5 ? -6 : 16) + 4);
    p.drawText(at, lab);
  }
  double s = 1;
  if (m_growing) {   // grows from the centre, ease-in-out over 600 ms (tiny while the rows are already in)
    const double t = std::min(1.0, m_grow.elapsed() / 600.0);
    s = t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
  }
  QPolygonF poly;
  for (int k = 0; k < 6; ++k) poly << vtx(k, R * std::clamp(m_read.radar[k], 0.0, 100.0) / 100.0 * s);
  p.setBrush(theme::map(250, 204, 21, 46));
  p.setPen(QPen(theme::map(245, 221, 112, 60), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPolygon(poly);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(theme::map("#f5dd70"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPolygon(poly);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#f8e27a"));
  for (const auto& pt : poly) p.drawEllipse(pt, 2.6, 2.6);

  // read rows
  static const char* names[] = {QT_TRANSLATE_NOOP("technical", "TREND"), QT_TRANSLATE_NOOP("technical", "MOMENTUM"), QT_TRANSLATE_NOOP("technical", "VOLATILITY"),
                                QT_TRANSLATE_NOOP("technical", "VOLUME"), QT_TRANSLATE_NOOP("technical", "STRUCTURE"), QT_TRANSLATE_NOOP("technical", "LIQUIDITY")};
  const QFont flab = theme::mono(10, QFont::Normal, 3.0), fval = theme::mono(14, QFont::Medium), fdet = theme::mono(12);
  for (int k = 0; k < 6; ++k) {
    const double cy = Y(rowsY + 32 * k);
    const auto& cell = m_cells[k];
    p.setFont(flab); p.setPen(theme::map("#595854"));
    p.drawText(QPointF(0, cy + 4), QCoreApplication::translate("technical", names[k]));
    p.setFont(fdet); p.setPen(theme::map("#aaa9a6"));
    const double dw = QFontMetricsF(fdet).horizontalAdvance(cell.fileDetail);
    p.drawText(QRectF(W - dw - 2, cy - 10, dw + 2, 20), Qt::AlignRight | Qt::AlignVCenter, cell.fileDetail);
    p.setFont(fval); p.setPen(cell.fileColor.isValid() ? cell.fileColor : theme::map("#a3a09a"));
    p.drawText(QPointF(105, cy + 5), QFontMetricsF(fval).elidedText(cell.fileValue, Qt::ElideRight, W - 105 - dw - 10));
    p.fillRect(QRectF(0, cy + 15, W, 1), theme::map("#181714"));
  }
  // footnote: the full rules when they fit, else the first paragraph (the rules stay in the tooltip)
  const QFont ff = theme::sans(10);
  p.setFont(ff); p.setPen(theme::map("#56544f"));
  const QRectF fr(0, Y(rowsY + 186), W, height() - Y(rowsY + 186) - 4);
  QString note = footnote();
  if (QFontMetricsF(ff).boundingRect(QRectF(0, 0, W, 4000), Qt::TextWordWrap, note).height() > fr.height())
    note = note.section('\n', 0, 0) + "\n" + QCoreApplication::translate("technical", "Hover the panel for the full rules.");
  p.drawText(fr, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, note);
}
