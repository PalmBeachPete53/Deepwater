#include "pages/market/liquidity/CandleChart.h"
#include "pages/market/technical/MarketKit.h"
#include "ui/Chart.h"
#include "ui/Format.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDateTime>
#include <QTimeZone>
#include <QLocale>
#include <QCoreApplication>
#include "core/I18n.h"
#include <cmath>
#include <algorithm>

using namespace analytics;

namespace {
constexpr int kAxisW = 49, kTimeH = 32;
QColor kUp() { return theme::map("#22d3ee"); }
QColor kDown() { return theme::map("#f87171"); }
QColor kGold() { return theme::map("#facc15"); }
QColor kBg() { return theme::map("#0a0802"); }
QColor kBorder() { return theme::map("#393322"); }
QColor kGrid() { return theme::map("#140f03"); }
QColor kAxisText() { return theme::map("#8e8d86"); }
QDateTime utc(qint64 s) { return QDateTime::fromSecsSinceEpoch(s, QTimeZone::UTC); }
QLocale loc() { return i18n::locale(); }   // month names in the interface language
}

CandleChart::CandleChart(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setCursor(Qt::CrossCursor);
  // paintEvent draws the cached layer over the whole widget (opaque background): Qt need not clear behind it
  setAttribute(Qt::WA_OpaquePaintEvent);
}

void CandleChart::setStatus(const QString& s) { m_status = s; update(); }

void CandleChart::setData(const QVector<market::Bar>& bars, const QVector<Pool>& pools, double pdh, double pdl,
                          int decimals, bool resetView) {
  const bool hadData = !m_bars.isEmpty();
  const int oldN = m_bars.size();
  m_bars = bars; m_pools = pools; m_pdh = pdh; m_pdl = pdl; m_dec = decimals;
  m_status.clear();
  pickNearest();
  if (resetView || !hadData) {
    m_resetPending = true;                              // applied once the widget has its real width
    m_right = bars.size() - 1 + 1.5;
  } else if (std::abs(m_right - (oldN - 1 + 1.5)) < 1.0) {
    m_right = bars.size() - 1 + 1.5;                    // follow new bars when the view sat at the end
  }
  clampView();
  m_dirty = true; update();
}

void CandleChart::setLastPrice(double p) {
  if (p == m_last || (!std::isfinite(p) && !std::isfinite(m_last))) return;
  m_last = p; pickNearest(); m_dirty = true; update();
}

QRectF CandleChart::plot() const { return QRectF(1, 1, width() - 2 - kAxisW, height() - 2 - kTimeH); }
double CandleChart::xOf(double i) const { const QRectF p = plot(); return p.right() - (m_right - i) * m_spacing; }
double CandleChart::indexAt(double x) const { const QRectF p = plot(); return m_right - (p.right() - x) / m_spacing; }
double CandleChart::yOf(double v) const { const QRectF p = plot(); return p.bottom() - (v - m_lo) / (m_hi - m_lo) * p.height(); }
double CandleChart::priceAt(double y) const { const QRectF p = plot(); return m_lo + (p.bottom() - y) / p.height() * (m_hi - m_lo); }
QString CandleChart::priceText(double v) const { return fmt::num(v, m_dec); }

void CandleChart::clampView() {
  if (m_bars.isEmpty()) return;
  const double visible = plot().width() / m_spacing;
  m_right = std::clamp(m_right, std::min(visible * 0.2, double(m_bars.size())), m_bars.size() - 1 + visible * 0.5);
}

// Drawn levels (each with its tag, as in the reference): the 4 nearest untested pools on each side of the last
// price, and the 3 most recent pools swept by a candle that is on screen. Broken pools are not drawn.
bool CandleChart::shown(const Pool& p, int firstVisible) const {
  if (p.state == PoolState::Broken) return false;
  if (p.state == PoolState::Swept) {
    if (p.eventIndex < firstVisible) return false;
    int newer = 0;
    for (const auto& q : m_pools) if (q.state == PoolState::Swept && q.eventIndex > p.eventIndex) ++newer;
    return newer < 3;
  }
  return m_nearest.contains(&p - m_pools.constData());
}
void CandleChart::pickNearest() {
  m_nearest.clear();
  const double last = std::isfinite(m_last) ? m_last : (m_bars.isEmpty() ? NAN : m_bars.last().c);
  if (!std::isfinite(last)) return;
  QVector<QPair<double, int>> up, dn;
  for (int i = 0; i < m_pools.size(); ++i) {
    if (m_pools[i].state != PoolState::Untested) continue;
    const double d = m_pools[i].level - last;
    (d >= 0 ? up : dn).push_back({std::abs(d), i});
  }
  std::sort(up.begin(), up.end()); std::sort(dn.begin(), dn.end());
  for (int k = 0; k < 4 && k < up.size(); ++k) m_nearest.insert(up[k].second);
  for (int k = 0; k < 4 && k < dn.size(); ++k) m_nearest.insert(dn[k].second);
}

void CandleChart::autoscale() {
  double lo = INFINITY, hi = -INFINITY;
  const int a = std::max(0, int(std::floor(indexAt(plot().left())))), b = std::min<int>(m_bars.size() - 1, int(std::ceil(m_right)));
  for (int i = a; i <= b; ++i) { lo = std::min(lo, m_bars[i].l); hi = std::max(hi, m_bars[i].h); }
  if (!std::isfinite(lo)) { lo = 0; hi = 1; }
  // nearby levels widen the range (up to 30 % of the candle range beyond it), as the reference's line series do
  const double r = hi - lo, reach = std::max(r * 0.3, std::abs(hi) * 1e-6);
  auto take = [&](double v) { if (std::isfinite(v) && v > lo - reach && v < hi + reach) { lo = std::min(lo, v); hi = std::max(hi, v); } };
  for (const auto& p : m_pools) if (shown(p, a)) take(p.level);
  take(m_pdh); take(m_pdl);
  if (hi - lo < std::abs(hi) * 1e-6) { hi += std::abs(hi) * 1e-4 + 1e-9; lo -= std::abs(lo) * 1e-4 + 1e-9; }
  const double span = hi - lo;            // scale margins: 20 % top, 10 % bottom (lightweight-charts defaults)
  m_hi = hi + span * 0.2 / 0.7; m_lo = lo - span * 0.1 / 0.7;
}

void CandleChart::applyReset() {
  if (!m_resetPending || width() < 200 || m_bars.isEmpty()) return;
  m_resetPending = false;
  m_spacing = std::clamp(plot().width() / 480.0, 0.6, 60.0);   // ~4 weeks of hourly bars
  m_right = m_bars.size() - 1 + 1.5;
  clampView();
}

void CandleChart::renderStatic() {
  applyReset();
  const qreal dpr = devicePixelRatioF();
  // the static layer is redrawn on every pan / zoom frame: the pixmap (~13 MB at 2x) is reused while the size and the
  // pixel ratio hold, instead of being allocated per frame; the opaque background below covers all of it
  if (m_cache.size() != size() * dpr || m_cache.devicePixelRatio() != dpr) {
    m_cache = QPixmap(size() * dpr); m_cache.setDevicePixelRatio(dpr);
  }
  QPainter p(&m_cache);
  p.setRenderHint(QPainter::Antialiasing, false);
  const QRectF pl = plot();
  p.fillRect(rect(), kBg());
  if (m_bars.isEmpty()) return;
  autoscale();

  // price grid + axis labels
  const auto ticks = chart::niceTicks(m_lo, m_hi, std::max(3, int(pl.height() / 55)));
  const double step = ticks.size() > 1 ? ticks[1] - ticks[0] : 0;
  const int tickDec = step > 0 ? std::clamp(int(std::ceil(-std::log10(step) - 1e-9)), 0, 6) : m_dec;
  const QFont af = theme::mono(9);
  QVector<QPair<double, QString>> tickLabels;     // drawn after the tags, skipping the ones they cover
  for (double t : ticks) {
    const double y = std::round(yOf(t)) + 0.5;
    if (y < pl.top() + 4 || y > pl.bottom() - 2) continue;
    p.setPen(kGrid()); p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y));
    tickLabels.push_back({y, fmt::num(t, tickDec)});
  }

  // time marks: weighted boundaries placed greedily from the strongest (lightweight-charts like)
  const int a = std::max(0, int(std::floor(indexAt(pl.left())))), b = std::min<int>(m_bars.size() - 1, int(std::ceil(m_right)));
  struct Mark { int i; int w; QString text; };
  QVector<Mark> marks;
  for (int i = std::max(a, 1); i <= b; ++i) {
    const QDateTime t = utc(m_bars[i].t), t0 = utc(m_bars[i - 1].t);
    int w = 0; QString s;
    if (t.date().year() != t0.date().year()) { w = 6; s = QString::number(t.date().year()); }
    else if (t.date().month() != t0.date().month()) { w = 5; s = loc().monthName(t.date().month(), QLocale::ShortFormat); }
    else if (t.date() != t0.date()) { w = 4; s = QString::number(t.date().day()); }
    else {
      const int h = t.time().hour(), h0 = t0.time().hour();
      if (h / 12 != h0 / 12) w = 3; else if (h / 6 != h0 / 6) w = 2; else if (h / 3 != h0 / 3) w = 1;
      s = t.toString("HH:mm");
    }
    if (w > 0) marks.push_back({i, w, s});
  }
  QVector<double> placed;
  QVector<Mark> shownMarks;
  const double minGap = 64;
  // hour marks only when a day is wide enough to hold them between day marks; otherwise a lone "12:00" lands in
  // the gap left by a skipped day (narrow charts)
  int days = 0; for (const auto& m : marks) if (m.w >= 4) ++days;
  const double dayPx = days ? (xOf(b) - xOf(a)) / days : INFINITY;
  const int minW = dayPx < 2 * minGap ? 4 : 1;
  for (int w = 6; w >= minW; --w)
    for (const auto& m : marks) {
      if (m.w != w) continue;
      const double x = xOf(m.i);
      if (x < pl.left() + 10 || x > pl.right() - 10) continue;
      bool ok = true; for (double q : placed) if (std::abs(q - x) < minGap) { ok = false; break; }
      if (ok) { placed << x; shownMarks << m; }
    }
  for (const auto& m : shownMarks) {
    const double x = std::round(xOf(m.i)) + 0.5;
    p.setPen(kGrid()); p.drawLine(QPointF(x, pl.top()), QPointF(x, pl.bottom()));
    p.setPen(theme::map("#898882"));
    p.drawText(QRectF(x - 40, pl.bottom() + 6, 80, 18), Qt::AlignCenter, m.text);
  }

  // candles
  p.save(); p.setClipRect(pl);
  const double bw = std::max(1.0, std::ceil(m_spacing * 0.8) - (m_spacing > 6 ? 1 : 0));
  for (int i = a; i <= b; ++i) {
    const auto& k = m_bars[i];
    const bool up = k.c >= k.o;
    const QColor col = up ? kUp() : kDown();
    const double x = std::round(xOf(i));
    const double yh = yOf(k.h), yl = yOf(k.l), yo = yOf(k.o), yc = yOf(k.c);
    p.fillRect(QRectF(x, std::round(yh), 1, std::max(1.0, std::round(yl) - std::round(yh))), col);
    const double top = std::round(std::min(yo, yc)), bot = std::round(std::max(yo, yc));
    if (bw > 1) p.fillRect(QRectF(x - std::floor(bw / 2), top, bw, std::max(1.0, bot - top)), col);
  }
  // levels
  p.setRenderHint(QPainter::Antialiasing, false);
  struct Tag { double y; QString label, value; QColor bg, fg; double dist; };   // empty label = last price
  QVector<Tag> tags;
  for (const auto& pool : m_pools) {
    if (!shown(pool, a)) continue;
    const double y = std::round(yOf(pool.level)) + 0.5;
    if (y < pl.top() || y > pl.bottom()) continue;
    const bool bsl = pool.side == PoolSide::BuySide;
    const bool live = pool.state == PoolState::Untested;
    QColor c = bsl ? kDown() : kUp(); c.setAlpha(live ? 165 : 120);
    QPen pen(c, 1, Qt::CustomDashLine); pen.setDashPattern({1, 2});
    p.setPen(pen); p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y));
    tags.push_back({y, QString("%1 ×%2").arg(bsl ? "BSL" : "SSL").arg(pool.members), priceText(pool.level),
                    bsl ? kDown() : kUp(), bsl ? theme::map("#ffffff") : theme::map("#0b2a33"),
                    std::isfinite(m_last) ? std::abs(pool.level - m_last) : 0});
  }
  for (int k = 0; k < 2; ++k) {
    const double v = k ? m_pdl : m_pdh;
    if (!std::isfinite(v)) continue;
    const double y = std::round(yOf(v)) + 0.5;
    if (y < pl.top() || y > pl.bottom()) continue;
    QPen pen(theme::map(250, 204, 21, 215), 1.2, Qt::CustomDashLine); pen.setDashPattern({5, 3.4});
    p.setPen(pen); p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y));
    tags.push_back({y, k ? "PDL" : "PDH", priceText(v), kGold(), theme::map("#2a2000"), -1});
  }
  p.restore();

  // axis borders
  p.fillRect(QRectF(pl.right() + 1, 0, 1, height() - 1), theme::map("#312e1b"));
  p.fillRect(QRectF(0, pl.bottom() + 1, width() - 1, 1), theme::map("#3b3724"));

  // last price tag (cyan if the last candle closed up) joins the stack and is never dropped
  const double last = std::isfinite(m_last) ? m_last : m_bars.last().c;
  const bool lastUp = m_bars.last().c >= m_bars.last().o;
  if (const double ly0 = yOf(last); ly0 >= pl.top() && ly0 <= pl.bottom())
    tags.push_back({ly0, QString(), priceText(last), lastUp ? kUp() : kDown(), lastUp ? theme::map("#0b2a33") : theme::map("#ffffff"), -2});
  // tags: the last price first, then PDH/PDL, then the pools closest to the price; stacked without overlap. A tag
  // that would push the stack more than kMaxShift px (24) away from a level is left out (its line stays; the pools table
  // lists every level): on short charts a full stack drifted 150 px from its lines and hid every price label.
  const double th = 14, kMaxShift = 24;
  auto stack = [&](QVector<Tag>& ts) {
    std::sort(ts.begin(), ts.end(), [](const Tag& x, const Tag& y) { return x.y < y.y; });
    QVector<double> out; for (const auto& t : ts) out << t.y - th / 2;
    for (int i = 1; i < out.size(); ++i) out[i] = std::max(out[i], out[i - 1] + th + 1);
    if (!out.isEmpty() && out.last() + th > pl.bottom()) {
      out.last() = pl.bottom() - th;
      for (int i = out.size() - 2; i >= 0; --i) out[i] = std::min(out[i], out[i + 1] - th - 1);
    }
    return out;
  };
  std::sort(tags.begin(), tags.end(), [](const Tag& x, const Tag& y) { return x.dist < y.dist; });
  QVector<Tag> kept;
  for (const auto& t : tags) {
    QVector<Tag> trial = kept; trial << t;
    const QVector<double> pos = stack(trial);
    bool ok = true;
    for (int i = 0; i < trial.size(); ++i) if (std::abs(pos[i] - (trial[i].y - th / 2)) > kMaxShift) { ok = false; break; }
    if (ok || kept.isEmpty()) kept = trial;
  }
  tags = kept;
  const QVector<double> ty = stack(tags);
  p.setFont(af); p.setPen(kAxisText());
  for (const auto& [y, txt] : tickLabels) {
    bool covered = false;
    for (double t0 : ty) if (y > t0 - 6 && y < t0 + th + 6) covered = true;
    if (!covered) p.drawText(QRectF(pl.right() + 6, y - 7, kAxisW - 8, 14), Qt::AlignLeft | Qt::AlignVCenter, txt);
  }
  p.setRenderHint(QPainter::Antialiasing, true);
  const QFont tf = theme::mono(9, QFont::Medium);
  p.setFont(tf);
  const QFontMetricsF tm(tf);
  for (int i = 0; i < tags.size(); ++i) {
    const auto& t = tags[i];
    const QRectF vr(pl.right() + 1, ty[i], kAxisW - 3, th);
    p.fillRect(vr, t.bg);
    if (!t.label.isEmpty()) {
      const double lw = tm.horizontalAdvance(t.label) + 10;
      const QRectF lr(pl.right() - lw + 1, ty[i], lw, th);
      // a tag pushed off its level by the stack: a short leader from the level line to the tag
      if (std::abs(lr.center().y() - t.y) > 3) {
        p.setPen(QPen(t.bg, 1));
        p.drawLine(QPointF(lr.left() - 10, t.y), QPointF(lr.left(), lr.center().y()));
      }
      p.fillRect(lr, t.bg);
      p.fillRect(QRectF(pl.right() + 1, ty[i], 1, th), theme::alpha(theme::map("#000000"), 40));
      p.setPen(mkit::readable(t.fg, t.bg)); p.drawText(lr, Qt::AlignCenter, t.label);
    }
    p.setPen(mkit::readable(t.fg, t.bg)); p.drawText(vr, Qt::AlignCenter, t.value);
  }

  // legend (overlay, top-left)
  const QFont lf = theme::mono(10, QFont::Medium, 3.0);
  p.setFont(lf);
  double x = 10; const double ly = 14;
  const struct { const char* t; QColor c; QColor sw; int style; } items[] = {
    {QT_TRANSLATE_NOOP("technical", "BSL BUY-SIDE POOL"), theme::map("#c78e8e"), kDown(), 0}, {QT_TRANSLATE_NOOP("technical", "SSL SELL-SIDE POOL"), theme::map("#81b8bc"), kUp(), 0},
    {"PDH / PDL", theme::map("#e3d27b"), kGold(), 1}};
  for (const auto& it : items) {
    QPen pen(it.sw, it.style ? 2 : 1.4); pen.setCapStyle(Qt::FlatCap);
    if (it.style) { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({1, 2}); }
    else { pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({4, 1.5}); }
    p.setPen(pen); p.drawLine(QPointF(x, ly), QPointF(x + 16, ly));
    x += 24;
    const QString txt = QCoreApplication::translate("technical", it.t);
    p.setPen(it.c); p.drawText(QPointF(x, ly + 3.5), txt);
    x += mkit::textWidth(lf, txt) + 12;
  }
  // panel frame, bracket, resize grip
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(kBorder()); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(0, 0, width() - 1, height() - 1));
  ui::drawBrackets(p, QRectF(3, 3, width() - 6, height() - 6), theme::map("#8a7d60"), 9, true, false);
  p.setPen(QPen(theme::map("#7f7d66"), 1));
  p.drawLine(QPointF(width() - 7, height() - 15), QPointF(width() - 7, height() - 7));
  p.drawLine(QPointF(width() - 15, height() - 7), QPointF(width() - 7, height() - 7));
}

void CandleChart::paintEvent(QPaintEvent*) {
  if (m_dirty || m_cache.size() != size() * devicePixelRatioF() || m_cache.devicePixelRatio() != devicePixelRatioF()) { renderStatic(); m_dirty = false; }
  QPainter p(this);
  p.drawPixmap(0, 0, m_cache);
  if (m_bars.isEmpty()) {
    p.setFont(theme::mono(11, QFont::Normal, 2)); p.setPen(theme::map("#6a6967"));
    p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
               m_status.isEmpty() ? QCoreApplication::translate("technical", "LOADING HOURLY CANDLES…") : m_status);
    return;
  }
  const QRectF pl = plot();
  if (!pl.contains(m_mouse) || m_drag || m_resize) return;
  // crosshair snapped to the candle under the cursor
  const int i = std::clamp(int(std::lround(indexAt(m_mouse.x()))), 0, int(m_bars.size()) - 1);
  const double x = std::round(xOf(i)) + 0.5, y = std::round(m_mouse.y()) + 0.5;
  QPen pen(theme::map("#8b8881"), 1, Qt::CustomDashLine); pen.setDashPattern({4, 4});
  p.setPen(pen);
  p.drawLine(QPointF(x, pl.top()), QPointF(x, pl.bottom()));
  p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y));
  p.setRenderHint(QPainter::Antialiasing);
  const QFont f = theme::mono(9, QFont::Bold);
  p.setFont(f);
  const QRectF pr(pl.right() + 1, y - 7.5, kAxisW - 3, 15);
  p.fillRect(pr, theme::map("#1a1d2c")); p.setPen(theme::map("#f5f6ff"));
  p.drawText(pr, Qt::AlignCenter, priceText(priceAt(m_mouse.y())));
  const QDateTime t = utc(m_bars[i].t);
  const QString ts = QString("%1 %2 '%3  %4").arg(t.toString("dd"), loc().monthName(t.date().month(), QLocale::ShortFormat),
                                                    t.toString("yy"), t.toString("HH:mm:ss"));
  const double tw = QFontMetricsF(f).horizontalAdvance(ts) + 16;
  QRectF trr(x - tw / 2, pl.bottom() + 4, tw, 17);
  if (trr.left() < 1) trr.moveLeft(1);
  if (trr.right() > pl.right()) trr.moveRight(pl.right());
  p.fillRect(trr, theme::map("#172230")); p.setPen(theme::map("#e9edf2"));
  p.drawText(trr, Qt::AlignCenter, ts);
}

void CandleChart::resizeEvent(QResizeEvent*) { clampView(); m_dirty = true; }

void CandleChart::zoomAt(double x, int steps) {
  if (m_bars.isEmpty() || steps == 0) return;
  applyReset();
  const double anchor = indexAt(x);
  const double ns = std::clamp(m_spacing * std::pow(1.15, steps), 0.35, 80.0);
  if (ns == m_spacing) return;
  m_spacing = ns;
  m_right = anchor + (plot().right() - x) / m_spacing;
  clampView();
  m_dirty = true; update();
}

void CandleChart::wheelEvent(QWheelEvent* e) {
  const int d = e->angleDelta().y();
  if (d == 0) { e->ignore(); return; }
  static int acc = 0; acc += d;
  const int steps = acc / 120; acc %= 120;
  if (steps) zoomAt(e->position().x(), steps);
  e->accept();
}

void CandleChart::setHover(const QPointF& p) { m_mouse = p; m_pinned = true; update(); }   // test hook: survives Leave

void CandleChart::mousePressEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  if (pos.x() > width() - 18 && pos.y() > height() - 18) { m_resize = true; m_dragFrom = pos; m_dragH = height(); return; }
  if (plot().contains(pos)) { m_drag = true; m_dragFrom = pos; m_dragRight = m_right; setCursor(Qt::ClosedHandCursor); update(); }
}
void CandleChart::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  m_pinned = false;
  if (m_resize) {
    m_height = std::clamp(int(m_dragH + pos.y() - m_dragFrom.y()), 260, 1400);
    m_userSized = true;
    emit heightRequested(m_height);
    return;
  }
  if (m_drag) {
    applyReset();
    m_right = m_dragRight - (pos.x() - m_dragFrom.x()) / m_spacing;
    clampView(); m_dirty = true; update();
    return;
  }
  setCursor(pos.x() > width() - 18 && pos.y() > height() - 18 ? Qt::SizeVerCursor : Qt::CrossCursor);
  const bool was = plot().contains(m_mouse), now = plot().contains(pos);
  m_mouse = pos;
  if (was || now) update();
}
void CandleChart::mouseReleaseEvent(QMouseEvent*) { m_drag = false; m_resize = false; setCursor(Qt::CrossCursor); update(); }
void CandleChart::leaveEvent(QEvent*) { if (m_pinned) return; m_mouse = QPointF(-1, -1); update(); }
