#include "pages/news/bank-research/Look.h"
#include "pages/positioning/bank-positioning/Widgets.h"
#include "banks/Registry.h"
#include "ui/Theme.h"
#include "ui/Kit.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLocale>
#include <QCoreApplication>
#include "core/I18n.h"
#include <cmath>

namespace banks::pos {

namespace {
const banks::DwColor kLongText{"#62d2dd"}, kShortText{"#ee8c88"}, kRangeText{"#f6db5e"};
const banks::DwColor kLongBar{"#36dffa"}, kShortBar{"#ff8e87"}, kRangeBar{"#ffdf2e"};
const banks::DwColor kLabel{"#59554d"}, kTitle{"#fffef4"}, kSub{"#57544a"};
}

void drawClassIcon(QPainter& p, banks::AssetClass c, const QRectF& r, const QColor& col) {
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  QPen pen(col, 1.3); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen); p.setBrush(Qt::NoBrush);
  const QPointF m = r.center();
  switch (c) {
    case banks::AssetClass::FX:
      p.drawEllipse(m + QPointF(-1.6, 1.4), 4.2, 4.2);
      p.drawEllipse(m + QPointF(1.8, -1.6), 4.2, 4.2);
      break;
    case banks::AssetClass::AsiaEmFx:
      p.drawEllipse(m, 5.5, 5.5);
      p.drawEllipse(m, 2.4, 5.5);
      p.drawLine(m + QPointF(-5.5, 0), m + QPointF(5.5, 0));
      break;
    case banks::AssetClass::Commodities: {
      QPainterPath d; d.moveTo(m + QPointF(-1.5, -5)); d.cubicTo(m + QPointF(-6, 1), m + QPointF(-4, 5), m + QPointF(-1.5, 5));
      d.cubicTo(m + QPointF(1, 5), m + QPointF(3, 1), m + QPointF(-1.5, -5));
      p.drawPath(d);
      p.drawEllipse(m + QPointF(3.4, 2.2), 1.8, 2.3);
      break; }
    case banks::AssetClass::Indices: {
      p.drawLine(m + QPointF(-5.5, -5), m + QPointF(-5.5, 5)); p.drawLine(m + QPointF(-5.5, 5), m + QPointF(5.5, 5));
      QPolygonF l; l << m + QPointF(-3.5, 2) << m + QPointF(-1, -1) << m + QPointF(1.2, 1) << m + QPointF(4.5, -3);
      p.drawPolyline(l);
      break; }
    case banks::AssetClass::Rates:
      p.setFont(theme::mono(12, QFont::Bold)); p.setPen(col);
      p.drawText(r, Qt::AlignCenter, "%");
      break;
    case banks::AssetClass::Crypto:
      p.setFont(theme::sans(12, QFont::Bold)); p.setPen(col);
      p.drawText(r, Qt::AlignCenter, QString::fromUtf8("₿"));
      break;
  }
  p.restore();
}

// ============================================================================================ MarketCard
MarketCard::MarketCard(banks::AssetClass c, QWidget* parent) : QWidget(parent), m_class(c) {
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover);
  m_anim.setDuration(200);
  m_anim.setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_lift = v.toReal(); update(); });
  const double th = banks::classThreshold(c);
  setToolTip(c == banks::AssetClass::Rates
    ? QCoreApplication::translate("banks", "Calls computed by Deepwater: each bank's forecast yield for the next quarter end vs today's yield.\n"
              "LONG (bonds) = forecast %1 pt or more below, SHORT = %1 pt or more above, RANGE = within.").arg(th, 0, 'f', 2)
    : QCoreApplication::translate("banks", "Calls computed by Deepwater: each bank's forecast for the next quarter end vs the spot price.\n"
              "LONG = forecast %1% or more above spot, SHORT = %1% or more below, RANGE = within.").arg(th, 0, 'f', 1));
}
void MarketCard::animateTo(qreal v) { m_anim.stop(); m_anim.setStartValue(m_lift); m_anim.setEndValue(v); m_anim.start(); }
void MarketCard::setForcedHover(bool on) { m_forced = on; m_anim.stop(); m_lift = on ? kLift : 0; update(); }
void MarketCard::enterEvent(QEnterEvent*) { animateTo(kLift); }
void MarketCard::leaveEvent(QEvent*) { if (!m_forced) animateTo(0); }
void MarketCard::mouseReleaseEvent(QMouseEvent* e) { if (rect().contains(e->position().toPoint()) && onClick) onClick(m_class); }

void MarketCard::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  const qreal t = m_lift / kLift;     // 0 rest … 1 hovered
  const QRectF box(0, kLift - m_lift, width(), height() - kLift);
  QLinearGradient g(box.topLeft(), box.bottomRight());
  g.setColorAt(0, theme::map("#110c02")); g.setColorAt(1, theme::map("#070502"));
  p.fillRect(box, g);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(theme::map("#312e0d")); p.setBrush(Qt::NoBrush);
  p.drawRect(box.adjusted(0, 0, -1, -1));
  p.setPen(theme::mix(theme::map("#433c2b"), theme::map("#6a6136"), t));
  p.drawLine(box.topLeft(), box.topRight() - QPointF(1, 0));
  ui::drawBrackets(p, box.adjusted(4, 4, -4, -4), theme::map("#5f5a3a"), 8, true, false);
  ui::drawBrackets(p, box.adjusted(4, 4, -4, -4), theme::map("#4a4324"), 8, false, true);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.translate(0, box.top());

  // icon tile, title, subtitle, arrow
  const QRectF tile(20, 24, 26, 26);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#2a2302")); p.drawRoundedRect(tile, 3, 3);
  drawClassIcon(p, m_class, tile, theme::map("#e2d28e"));
  QFont tf = theme::display(22, QFont::Bold, 1.0);
  p.setFont(tf); p.setPen(kTitle);
  p.drawText(QPointF(57, 37), banks::className(m_class));
  p.setFont(theme::mono(10, QFont::Normal, 2.2)); p.setPen(kSub);
  p.drawText(QPointF(57, 57), banks::classSubtitle(m_class));
  {
    const QColor ac = theme::mix(theme::map("#686761"), theme::map("#ecdda5"), t);
    QPen ap(ac, 1.3); ap.setCapStyle(Qt::RoundCap); p.setPen(ap);
    const QPointF a(width() - 26, 39);
    p.drawLine(a + QPointF(-4, 4), a + QPointF(4, -4));
    p.drawLine(a + QPointF(-1.5, -4), a + QPointF(4, -4));
    p.drawLine(a + QPointF(4, -4), a + QPointF(4, 1.5));
  }
  // counters at 18, 182, 345 (scaled with the card width)
  const qreal sx = (width() - 36) / 483.0;
  const int counts[3] = {m_s.longs, m_s.shorts, m_s.ranges};
  const QColor cols[3] = {kLongText, kShortText, banks::seriesColour(kRangeText.hex, true)};
  const char* labels[3] = {"LONG", "SHORT", "RANGE"};
  for (int i = 0; i < 3; ++i) {
    const qreal x = 18 + (i == 0 ? 0 : i == 1 ? 164 : 327) * sx;
    p.setFont(theme::mono(28, QFont::Bold)); p.setPen(cols[i]);
    p.drawText(QPointF(x, 100), QString::number(counts[i]));
    p.setFont(theme::mono(9, QFont::Normal, 2.2)); p.setPen(kLabel);
    p.drawText(QPointF(x, 118), labels[i]);
  }
  // proportion bar: long → range → short
  const QRectF bar(18, 135, width() - 36, 5);
  const int total = m_s.positions();
  QPainterPath clip; clip.addRoundedRect(bar, 2.5, 2.5);
  p.save(); p.setClipPath(clip);
  if (total == 0) p.fillRect(bar, theme::map("#1d1a10"));
  else {
    qreal x = bar.left();
    const int seg[3] = {m_s.longs, m_s.ranges, m_s.shorts};
    const QColor sc[3] = {kLongBar, banks::seriesColour(kRangeBar.hex), kShortBar};
    for (int i = 0; i < 3; ++i) {
      const qreal w = bar.width() * seg[i] / total;
      p.fillRect(QRectF(x, bar.top(), w + 0.5, bar.height()), sc[i]);
      x += w;
    }
  }
  p.restore();
  // footer: 43 positions · 11 banks · 11/12 markets covered
  const QFont fb = theme::mono(12, QFont::Bold), fr = theme::mono(12);
  qreal x = 18;
  auto part = [&](const QString& s, bool bold) {
    p.setFont(bold ? fb : fr); p.setPen(bold ? theme::map("#fefdf9") : theme::map("#87847e"));
    p.drawText(QPointF(x, 168), s);
    x += QFontMetricsF(bold ? fb : fr).horizontalAdvance(s);
  };
  const QString dot = QString::fromUtf8(" · ");
  part(QString::number(total), true); part(QCoreApplication::translate("banks", " positions") + dot, false);
  part(QString::number(m_s.banks), true); part(QCoreApplication::translate("banks", " banks") + dot, false);
  part(QString("%1/%2").arg(m_s.covered).arg(m_s.universe), false); part(QCoreApplication::translate("banks", " markets covered"), false);
}

// ============================================================================================ CallsTable
namespace { constexpr int kHead = 30, kGroup = 30, kRow = 30; }

CallsTable::CallsTable(QWidget* parent) : QWidget(parent) {}
QSize CallsTable::sizeHint() const { return QSize(800, m_height); }
void CallsTable::setCalls(const QList<QPair<banks::AssetClass, QList<banks::Call>>>& g) {
  m_groups = g;
  int h = kHead;
  for (const auto& x : g) h += kGroup + std::max<int>(1, int(x.second.size())) * kRow;
  m_height = h + 20;
  setMinimumHeight(m_height);
  update();
}
int CallsTable::groupTop(banks::AssetClass c) const {
  int y = kHead;
  for (const auto& x : m_groups) { if (x.first == c) return y; y += kGroup + std::max<int>(1, int(x.second.size())) * kRow; }
  return 0;
}
void CallsTable::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  const qreal W = width();
  // columns (x of left edge, or right edge for numbers)
  const qreal cMarket = 14, cBank = W * 0.20, cCall = W * 0.38, cHor = W * 0.48, rFc = W * 0.64, rSpot = W * 0.76, rMove = W * 0.86, rRev = W - 14;
  const QFont hf = theme::mono(9, QFont::Bold, 1.6);
  p.setFont(hf); p.setPen(theme::map("#eadb9f"));
  auto head = [&](qreal x, const QString& s, bool right) {
    p.drawText(right ? QRectF(x - 140, 0, 140, kHead) : QRectF(x, 0, 200, kHead), Qt::AlignVCenter | (right ? Qt::AlignRight : Qt::AlignLeft), s);
  };
  head(cMarket, QCoreApplication::translate("banks", "MARKET"), false); head(cBank, QCoreApplication::translate("banks", "BANK"), false); head(cCall, QCoreApplication::translate("banks", "CALL"), false); head(cHor, QCoreApplication::translate("banks", "HORIZON"), false);
  head(rFc, QCoreApplication::translate("banks", "FORECAST"), true); head(rSpot, QCoreApplication::translate("banks", "SPOT"), true); head(rMove, QCoreApplication::translate("banks", "TO TARGET"), true); head(rRev, QCoreApplication::translate("banks", "TABLE REVISED"), true);
  p.fillRect(QRectF(0, kHead - 1, W, 1), theme::map("#2b261a"));
  const QLocale en(QLocale::English);
  int y = kHead;
  for (const auto& [cls, calls] : m_groups) {
    p.fillRect(QRectF(0, y, W, kGroup), theme::map("#140f03"));
    p.setFont(theme::mono(10, QFont::Bold, 1.6)); p.setPen(theme::map("#e2d59c"));
    p.drawText(QRectF(cMarket, y, 400, kGroup), Qt::AlignVCenter, QString::fromUtf8("%1 · %2").arg(banks::className(cls)).arg(calls.size()));
    y += kGroup;
    if (calls.isEmpty()) {
      p.setFont(theme::mono(10, QFont::Normal, 1)); p.setPen(theme::map("#5e5b53"));
      p.drawText(QRectF(cMarket, y, W - 28, kRow), Qt::AlignVCenter,
                 QCoreApplication::translate("banks", "— No forecast from the banks Deepwater may read for these markets"));
      y += kRow; continue;
    }
    for (const auto& c : calls) {
      if (y > e->rect().bottom() + kRow) { y += kRow; continue; }
      const banks::Market* m = banks::market(c.marketId);
      const banks::Bank* b = banks::bank(c.bankId);
      const int dec = m ? m->decimals : 4;
      p.fillRect(QRectF(0, y + kRow - 1, W, 1), theme::map("#1e1a0f"));
      p.setFont(theme::mono(12, QFont::Bold)); p.setPen(theme::map("#fffefb"));
      p.drawText(QRectF(cMarket, y, cBank - cMarket, kRow), Qt::AlignVCenter, m ? m->name() : c.marketId);
      p.setFont(theme::sans(12)); p.setPen(theme::map("#e0ddd8"));
      p.drawText(QRectF(cBank, y, cCall - cBank, kRow), Qt::AlignVCenter, b ? b->researchName : c.bankId);
      const QColor bc = c.bias == banks::Bias::Long ? theme::map("#7ec5d0") : c.bias == banks::Bias::Short ? theme::map("#e08a85") : banks::seriesColour("#e9d27a", true);
      const QColor bb = c.bias == banks::Bias::Long ? theme::map("#062c2f") : c.bias == banks::Bias::Short ? theme::map("#3e1a10") : (theme::isDefault() ? QColor("#2e2804") : theme::alpha(banks::seriesColour("#e9d27a"), 40));
      const QRectF chip(cCall, y + 7, 58, 16);
      p.setPen(QPen(theme::alpha(bc, 110), 1)); p.setBrush(bb); p.drawRoundedRect(chip, 2, 2);
      p.setFont(theme::mono(9, QFont::Bold, 1)); p.setPen(bc);
      p.drawText(chip, Qt::AlignCenter, banks::biasName(c.bias));
      p.setFont(theme::mono(12)); p.setPen(theme::map("#8c8881"));
      p.drawText(QRectF(cHor, y, 100, kRow), Qt::AlignVCenter, c.horizon.label());
      p.setPen(theme::map("#fffdfa"));
      p.drawText(QRectF(rFc - 140, y, 140, kRow), Qt::AlignVCenter | Qt::AlignRight, en.toString(c.forecast, 'f', dec));
      p.setPen(theme::map("#a09d94"));
      p.drawText(QRectF(rSpot - 140, y, 140, kRow), Qt::AlignVCenter | Qt::AlignRight, en.toString(c.spot, 'f', dec));
      p.setPen(theme::signColor(c.change));
      const QString mv = cls == banks::AssetClass::Rates ? QString("%1%2 pt").arg(c.change >= 0 ? "+" : "").arg(c.change, 0, 'f', 2)
                                                         : QString("%1%2%").arg(c.change >= 0 ? "+" : "").arg(c.change, 0, 'f', 2);
      p.drawText(QRectF(rMove - 140, y, 140, kRow), Qt::AlignVCenter | Qt::AlignRight, mv);
      p.setPen(theme::map("#8c8881"));
      p.drawText(QRectF(rRev - 140, y, 140, kRow), Qt::AlignVCenter | Qt::AlignRight, c.revised.isValid() ? i18n::locale().toString(c.revised, "MMM dd").toUpper() : QString::fromUtf8("—"));
      y += kRow;
    }
  }
}
void DeskBody::paintEvent(QPaintEvent*) { QPainter p(this); if (painter) painter(p); }
}
