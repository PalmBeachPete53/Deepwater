#include "pages/market/liquidity/LiquidityPanel.h"
#include "pages/market/technical/MarketKit.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>
#include <QTimeZone>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <QPixmap>

using namespace analytics;

namespace {
constexpr double kTabH = 33, kPitch = 36.3, kCard = 77, kSessPitch = 50.7;
const char* kTabs[] = {QT_TRANSLATE_NOOP("technical", "LIQUIDITY POOLS"), QT_TRANSLATE_NOOP("technical", "SWEEP ALERTS"), QT_TRANSLATE_NOOP("technical", "SESSION EXTREMES")};
QString tabText(int i) { return QCoreApplication::translate("technical", kTabs[i]); }
// Very faint hexagon grid of the reference's bottom panel, tiled from a cached pixmap.
void hexPattern(QPainter& p, const QRect& r) {
  static QPixmap tile; static int rev = -1;        // rebuilt on a theme change
  const double s = 36, w = std::sqrt(3.0) * s;     // pointy-top hexagons, side 36 px
  if (tile.isNull() || rev != theme::revision()) {
    rev = theme::revision();
    tile = QPixmap(int(std::round(w)), int(3 * s)); tile.fill(Qt::transparent);
    QPainter t(&tile); t.setRenderHint(QPainter::Antialiasing);
    t.setPen(QPen(theme::map(32, 24, 8, 70), 1));
    auto hex = [&](double cx, double cy) {
      QPolygonF h; for (int k = 0; k < 6; ++k) { const double a = M_PI / 6 + k * M_PI / 3; h << QPointF(cx + s * std::cos(a), cy + s * std::sin(a)); }
      t.drawPolygon(h);
    };
    hex(w / 2, s); hex(0, 2.5 * s); hex(w, 2.5 * s); hex(0, -0.5 * s); hex(w, -0.5 * s);
  }
  p.drawTiledPixmap(r, tile);
}
QFont tabFont() { return theme::mono(9, QFont::DemiBold, 0.2); }
}

LiquidityPanel::LiquidityPanel(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  m_focus.setStartValue(0.0); m_focus.setEndValue(1.0); m_focus.setDuration(450);
  connect(&m_focus, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_focusA = v.toDouble(); update(QRect(0, 0, width(), int(kTabH) + 2)); });
  m_focusDelay.setSingleShot(true); m_focusDelay.setInterval(500);
  connect(&m_focusDelay, &QTimer::timeout, this, [this] { m_focus.start(); });
}

void LiquidityPanel::setData(const QVector<Pool>& pools, const QVector<market::Bar>& bars, const SessionTable& s, int dec) {
  m_sessions = s; m_dec = dec;
  // list: untested pools, plus pools taken during the last 30 days (720 hourly candles), marked SWEPT / BROKEN
  m_pools.clear();
  for (const auto& p : pools) if (p.state == PoolState::Untested || p.eventIndex >= bars.size() - 720) m_pools << p;
  m_swept.clear(); m_sweptTime.clear();
  for (const auto& p : pools) if (p.state == PoolState::Swept) m_swept << p;
  std::sort(m_swept.begin(), m_swept.end(), [](const Pool& a, const Pool& b) { return a.eventIndex > b.eventIndex; });
  for (const auto& p : m_swept) m_sweptTime << (p.eventIndex >= 0 && p.eventIndex < bars.size() ? bars[p.eventIndex].t : 0);
  emit heightChanged(); update();
}
void LiquidityPanel::setLastPrice(double p) { if (p == m_last) return; m_last = p; if (m_tab == 0) update(); }

void LiquidityPanel::setTab(int t) {
  if (t == m_tab) return;
  m_tab = t; m_focusA = 0; m_focus.stop();
  m_focusDelay.start();
  emit heightChanged(); update();
}

// Wide panels (ultrawide / 1440p) flow the pool list and the sweep cards into 2–3 columns instead of rows that
// leave most of the width empty.
int LiquidityPanel::columns() const { return width() >= 3000 ? 3 : width() >= 2000 ? 2 : 1; }
int LiquidityPanel::contentHeight() const {
  const int cols = columns();
  auto per = [cols](qsizetype n) { return (std::max<qsizetype>(1, n) + cols - 1) / cols; };
  switch (m_tab) {
    case 0: return int(kTabH + 4 + per(m_pools.size() + 1) * kPitch + 6);
    case 1: return int(kTabH + 3 + per(m_swept.size()) * kCard + 4);
    default: return int(kTabH + 30 + std::max<qsizetype>(1, m_sessions.rows.size()) * kSessPitch + 4);
  }
}

QRectF LiquidityPanel::tabRect(int i) const {
  const QFont f = tabFont();
  double x = 13;
  for (int k = 0; k < i; ++k) x += mkit::textWidth(f, tabText(k)) + 22;
  return QRectF(x - 13, 1, mkit::textWidth(f, tabText(i)) + 26, kTabH - 2);
}

void LiquidityPanel::mouseMoveEvent(QMouseEvent* e) {
  int h = -1; for (int i = 0; i < 3; ++i) if (tabRect(i).contains(e->position())) h = i;
  if (h != m_hoverTab) {
    m_hoverTab = h;
    // hovering the active tab shows its focus box, which then stays (reference t14.0 → t15.5)
    if (h == m_tab && m_focusA < 1 && m_focus.state() != QAbstractAnimation::Running) m_focus.start();
    update(QRect(0, 0, width(), int(kTabH) + 2));
  }
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void LiquidityPanel::mousePressEvent(QMouseEvent* e) { for (int i = 0; i < 3; ++i) if (tabRect(i).contains(e->position())) setTab(i); }
void LiquidityPanel::leaveEvent(QEvent*) { m_hoverTab = -1; update(QRect(0, 0, width(), int(kTabH) + 2)); }

void LiquidityPanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.fillRect(rect(), theme::map("#0d0802"));
  hexPattern(p, rect());
  // tabs
  const QFont tf = tabFont();
  for (int i = 0; i < 3; ++i) {
    const QRectF r = tabRect(i);
    const bool on = i == m_tab, hov = i == m_hoverTab;
    if (on && (m_focusA > 0 || hov)) {
      const double a = hov ? 1.0 : m_focusA;
      p.setPen(QPen(theme::alpha(theme::map("#6b6550"), int(255 * a)), 1));
      p.setBrush(theme::alpha(theme::map("#1a1505"), int(200 * a)));
      p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    }
    p.setFont(tf);
    p.setPen(on ? theme::map("#ecd98a") : hov ? mkit::sem().goldText : theme::map("#68655b"));
    p.drawText(QPointF(r.left() + 13, 19.5), tabText(i));
    if (on) p.fillRect(QRectF(r.left() + 8, 28, r.width() - 16, 2), theme::map("#f3e197"));
  }
  p.fillRect(QRectF(1, kTabH, width() - 2, 1), theme::map("#1e1a0f"));
  p.save(); p.setClipRect(QRectF(1, kTabH + 1, width() - 2, height() - kTabH - 2));
  if (m_tab == 2) paintSessions(p, kTabH + 1);
  else {
    const int cols = columns();
    const double gap = 24, cw = (width() - (cols - 1) * gap) / cols;
    const int n = m_tab == 0 ? int(m_pools.size()) + 1 : int(m_swept.size());
    const int per = std::max(1, (n + cols - 1) / cols);
    for (int c = 0; c < cols; ++c) {
      p.save();
      p.translate(c * (cw + gap), 0);
      p.setClipRect(QRectF(0, kTabH + 1, cw, height() - kTabH - 2));
      if (m_tab == 0) paintPools(p, kTabH + 1, cw, c * per, (c + 1) * per);
      else paintSweeps(p, kTabH + 1, cw, c * per, (c + 1) * per);
      p.restore();
      if (c > 0) p.fillRect(QRectF(c * (cw + gap) - gap / 2, kTabH + 8, 1, height() - kTabH - 16), theme::map("#1e1a0f"));
    }
  }
  p.restore();
  p.setPen(QPen(theme::map("#3f3b1a"), 1)); p.setBrush(Qt::NoBrush);
  p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
}

void LiquidityPanel::paintPools(QPainter& p, double y0, double W, int from, int to) {
  if (m_pools.isEmpty()) {
    if (from > 0) return;
    p.setFont(theme::mono(10, QFont::Normal, 2)); p.setPen(theme::map("#6a6967"));
    p.drawText(QRectF(0, y0, W, kPitch), Qt::AlignCenter, QCoreApplication::translate("technical", "NO POOL DETECTED IN THE LOADED HISTORY"));
    return;
  }
  const QFont side = theme::mono(9, QFont::Bold, 1.0), price = theme::mono(14, QFont::Bold), small = theme::mono(9), pct = theme::mono(10);
  const QFont status = theme::mono(8, QFont::Normal, 1.5);
  const double last = m_last;
  // entries in price order with the LAST band (-1) where the price falls
  QVector<int> entries; bool lastDone = !std::isfinite(last);
  for (int i = 0; i < m_pools.size(); ++i) {
    if (!lastDone && m_pools[i].level < last) { entries << -1; lastDone = true; }
    entries << i;
  }
  if (!lastDone) entries << -1;
  double y = y0 + 3;
  for (int e = from; e < to && e < entries.size(); ++e) {
    if (entries[e] < 0) {
      QLinearGradient g(0, 0, W, 0); g.setColorAt(0, theme::map("#2a220d")); g.setColorAt(1, theme::map("#1f1902"));
      p.fillRect(QRectF(1, y + 3, W - 2, kPitch - 6), g);
      const double cy = y + kPitch / 2;
      p.setFont(side); p.setPen(theme::map("#eee490")); p.drawText(QPointF(14, cy + 3.5), "LAST");
      p.setFont(price); p.setPen(theme::map("#f9e59b"));
      const QString s = fmt::num(last, m_dec);
      p.drawText(QPointF(50, cy + 5), s);
      const double x = 50 + mkit::textWidth(price, s) + 10;
      p.fillRect(QRectF(x, cy, W - 14 - x, 1), theme::map("#6a5b28"));
      y += kPitch;
      continue;
    }
    const auto& pl = m_pools[entries[e]];
    const double cy = y + kPitch / 2;
    const bool bsl = pl.side == PoolSide::BuySide, live = pl.state == PoolState::Untested;
    p.fillRect(QRectF(14, cy - 5.5, 2, 11), bsl ? theme::map("#e05656") : theme::map("#1fb8cc"));
    p.setFont(side); p.setPen(bsl ? theme::map("#e2a9a7") : theme::map("#9fced6"));
    p.drawText(QPointF(20, cy + 3.5), bsl ? "BSL" : "SSL");
    p.setFont(price); p.setPen(live ? theme::map("#fffffa") : theme::map("#8b8880"));
    p.drawText(QPointF(88, cy + 5), fmt::num(pl.level, m_dec));
    const int dots = std::min(5, pl.touches);
    const QColor dc = bsl ? theme::map("#e7757d") : theme::map("#3fcfe0");
    for (int k = 0; k < dots; ++k) {
      p.setPen(Qt::NoPen); p.setBrush(theme::alpha(dc, 90 + k * 40));
      p.drawEllipse(QPointF(236 + k * 8, cy), 2.2, 2.2);
    }
    double x = 233 + dots * 8;
    if (pl.touches > 5) { p.setFont(small); p.setPen(theme::map("#5c5951")); p.drawText(QPointF(x + 1, cy + 3.5), "+" + QString::number(pl.touches - 5)); x += mkit::textWidth(small, "+" + QString::number(pl.touches - 5)) + 4; }
    p.setFont(status); p.setPen(theme::map("#4f4c44"));
    const QString st = pl.state == PoolState::Swept ? QCoreApplication::translate("technical", "SWEPT") : pl.state == PoolState::Broken ? QCoreApplication::translate("technical", "BROKEN") : QString();
    if (!st.isEmpty()) p.drawText(QPointF(std::max(x + 8, 330.0), cy + 3), st);
    p.setFont(pct); p.setPen(theme::map("#928e88"));
    const QString d = std::isfinite(last) && last > 0 ? fmt::pct((pl.level - last) / last * 100.0, 2) : fmt::dash();
    p.drawText(QRectF(W - 214, y, 200, kPitch), Qt::AlignRight | Qt::AlignVCenter, d);
    p.fillRect(QRectF(1, y + kPitch - 0.5, W - 2, 1), theme::map("#130d03"));
    y += kPitch;
  }
}
void LiquidityPanel::paintSweeps(QPainter& p, double y0, double W, int from, int to) {
  if (m_swept.isEmpty()) {
    if (from > 0) return;
    p.setFont(theme::mono(10, QFont::Normal, 2)); p.setPen(theme::map("#6a6967"));
    p.drawText(QRectF(0, y0, W, kCard), Qt::AlignCenter, QCoreApplication::translate("technical", "NO SWEEP IN THE LOADED HISTORY"));
    return;
  }
  const QFont head = theme::mono(9, QFont::Bold, 0.3), sent = theme::sans(12), num = theme::mono(13, QFont::Bold);
  const QFont small = theme::sans(9), smallB = theme::sans(9, QFont::Bold);
  const auto now = QDateTime::currentDateTimeUtc();
  double y = y0 + 2;
  for (int i = from; i < to && i < m_swept.size(); ++i) {
    const auto& s = m_swept[i];
    const bool sell = s.side == PoolSide::SellSide;
    const QColor c = sell ? theme::map("#32d2da") : theme::map("#e0585e");
    p.fillRect(QRectF(1, y, 2, kCard - 1), sell ? theme::map("#7ed7e8") : theme::map("#e8a4a5"));
    p.setPen(Qt::NoPen); p.setBrush(c); p.drawEllipse(QPointF(17.5, y + 15), 2.8, 2.8);
    p.setFont(head); p.setPen(c); p.drawText(QPointF(24, y + 18.5), QCoreApplication::translate("technical", "LIQUIDITY SWEEP DETECTED"));
    const QString a = (sell ? QCoreApplication::translate("technical", "Sell-side liquidity taken at") : QCoreApplication::translate("technical", "Buy-side liquidity taken at")) + " ";
    p.setFont(sent); p.setPen(theme::map("#f2f0eb")); p.drawText(QPointF(14, y + 40), a);
    p.setFont(num); p.setPen(theme::map("#fffffa")); p.drawText(QPointF(14 + mkit::textWidth(sent, a), y + 40), fmt::num(s.level, m_dec));
    double x = 14;
    p.setFont(small); p.setPen(theme::map("#6e6b64")); const QString rc = QCoreApplication::translate("technical", "Reversal confirmation:") + " ";
    p.drawText(QPointF(x, y + 61), rc);
    x += mkit::textWidth(small, rc);
    p.setFont(smallB); p.setPen(theme::map("#fdfbf5")); p.drawText(QPointF(x, y + 61), reversalName(s.reversal));
    x += mkit::textWidth(smallB, reversalName(s.reversal));
    p.setFont(small); p.setPen(theme::map("#67645c"));
    const QDateTime t = QDateTime::fromSecsSinceEpoch(m_sweptTime.value(i), QTimeZone::UTC);
    p.drawText(QPointF(x, y + 61), "  ·  " + mkit::ago(t, now));
    p.fillRect(QRectF(1, y + kCard - 1, W - 2, 1), theme::map("#1d180d"));
    y += kCard;
  }
}

void LiquidityPanel::paintSessions(QPainter& p, double y0) {
  const double W = width();
  const QFont hf = theme::mono(8, QFont::Normal, 2.0);
  p.setFont(hf); p.setPen(theme::map("#6a6760"));
  const double hy = y0 + 18;
  p.drawText(QPointF(14, hy), QCoreApplication::translate("technical", "WINDOW"));
  p.drawText(QRectF(W - 334, hy - 10, 200, 14), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("technical", "HIGH", "session high column"));
  p.drawText(QRectF(W - 214, hy - 10, 200, 14), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("technical", "LOW", "session low column"));
  const QFont nf = theme::sans(12, QFont::DemiBold), sf = theme::sans(9), vf = theme::mono(14);
  double y = y0 + 27;
  for (const auto& r : m_sessions.rows) {
    p.setFont(nf); p.setPen(r.summary ? theme::map("#f7e49c") : theme::map("#f2f0eb"));
    p.drawText(QPointF(14, y + 17), r.name);
    p.setFont(sf); p.setPen(theme::map("#4a473e"));
    p.drawText(QPointF(14, y + 33), r.sub);
    p.setFont(vf);
    const bool hasH = std::isfinite(r.high), hasL = std::isfinite(r.low);
    p.setPen(hasH ? theme::map("#e29a9b") : theme::map("#56524b"));
    p.drawText(QRectF(W - 334, y, 200, 46), Qt::AlignRight | Qt::AlignVCenter, hasH ? fmt::num(r.high, m_dec) : fmt::dash());
    p.setPen(hasL ? theme::map("#91c9cc") : theme::map("#56524b"));
    p.drawText(QRectF(W - 214, y, 200, 46), Qt::AlignRight | Qt::AlignVCenter, hasL ? fmt::num(r.low, m_dec) : fmt::dash());
    y += kSessPitch;
  }
}
