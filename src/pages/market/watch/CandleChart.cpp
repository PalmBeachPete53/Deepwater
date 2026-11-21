#include "pages/market/watch/CandleChart.h"
#include "pages/market/watch/WatchCalc.h"
#include "pages/market/watch/WatchIcons.h"
#include "pages/market/watch/WatchStyle.h"
#include "market/Instruments.h"
#include "market/QuoteHub.h"
#include "ui/Chart.h"
#include "ui/Format.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QContextMenuEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <QDateTime>
#include <QTimeZone>
#include <QHash>
#include <algorithm>

namespace watch {
namespace {
// TradingView-like dark tones (the embed in the reference keeps its own theme, not the app palette)
// TradingView-like tones designed for Deepwater, mapped to the current theme where painted
QColor kBg() { return theme::map(QColor(0x0a, 0x0a, 0x0a)); }
QColor kBar() { return theme::map(QColor(0x00, 0x00, 0x00)); }
QColor kGap() { return theme::map(QColor(0x1c, 0x1e, 0x21)); }
QColor kText() { return theme::map(QColor(0xd1, 0xd4, 0xdc)); }
QColor kDim() { return theme::map(QColor(0x78, 0x7b, 0x86)); }
QColor kActive() { return theme::map(QColor(0x2a, 0x2e, 0x39)); }
QColor kGrid() { return theme::map(QColor(0x16, 0x17, 0x1a)); }
QColor kLabel() { return theme::map(QColor(0x36, 0x3a, 0x45)); }
QColor kFrame() { return theme::map(QColor(0x2c, 0x2c, 0x1b)); }
QColor kSkel() { return theme::map(QColor(0x14, 0x14, 0x14)); }
QColor kCmp() { return theme::map(QColor(0xf7, 0xa2, 0x1b)); }
const QStringList kFavTf{"1m", "30m", "1h", "4h"};
const QStringList kAllTf{"1m", "5m", "15m", "30m", "1h", "4h", "1D", "1W", "1M"};
const QStringList kRanges{"1D", "5D", "1M", "3M", "6M", "YTD", "1Y", "5Y", "All"};

struct TfDef { QString iv, range; qint64 sec; qint64 agg; };
TfDef tfDef(const QString& tf) {
  if (tf == "1m") return {"1m", "5d", 60, 0};
  if (tf == "5m") return {"5m", "1mo", 300, 0};
  if (tf == "15m") return {"15m", "1mo", 900, 0};
  if (tf == "30m") return {"30m", "1mo", 1800, 0};
  if (tf == "1h") return {"1h", "6mo", 3600, 0};
  if (tf == "4h") return {"1h", "1y", 14400, 14400};
  if (tf == "1D") return {"1d", "5y", 86400, 0};
  if (tf == "1W") return {"1wk", "max", 7 * 86400, 0};
  return {"1mo", "max", 30 * 86400, 0};
}
QString rangeTf(const QString& r) {
  static const QHash<QString, QString> m{{"1D", "1m"}, {"5D", "5m"}, {"1M", "30m"}, {"3M", "1h"}, {"6M", "4h"},
                                         {"YTD", "1D"}, {"1Y", "1D"}, {"5Y", "1W"}, {"All", "1M"}};
  return m.value(r, "4h");
}
qint64 rangeStart(const QString& r, qint64 now) {
  const QDate d = QDateTime::fromSecsSinceEpoch(now, QTimeZone::UTC).date();
  auto at = [](const QDate& x) { return QDateTime(x, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(); };
  if (r == "1D") return now - 86400;
  if (r == "5D") return now - 5 * 86400;
  if (r == "1M") return at(d.addMonths(-1));
  if (r == "3M") return at(d.addMonths(-3));
  if (r == "6M") return at(d.addMonths(-6));
  if (r == "YTD") return at(QDate(d.year(), 1, 1));
  if (r == "1Y") return at(d.addYears(-1));
  if (r == "5Y") return at(d.addYears(-5));
  return 0;
}
QFont tvFont(int px, int w = QFont::Normal) { return theme::sans(px, w); }
// shown labels of timeframes and ranges (ids stay English: 1D 1W 1M, 1Y 5Y All)
QString tfLabel(const QString& tf) {
  if (tf == "1D") return QCoreApplication::translate("watch", "1D", "timeframe: one day");
  if (tf == "1W") return QCoreApplication::translate("watch", "1W", "timeframe: one week");
  if (tf == "1M") return QCoreApplication::translate("watch", "1M", "timeframe: one month");
  return tf;
}
QString rangeLabel(const QString& r) {
  if (r == "1D") return QCoreApplication::translate("watch", "1D", "range: one day");
  if (r == "5D") return QCoreApplication::translate("watch", "5D", "range: five days");
  if (r == "1Y") return QCoreApplication::translate("watch", "1Y", "range: one year");
  if (r == "5Y") return QCoreApplication::translate("watch", "5Y", "range: five years");
  if (r == "YTD") return QCoreApplication::translate("watch", "YTD", "range: year to date");
  if (r == "All") return QCoreApplication::translate("watch", "All", "range: whole history");
  return r;
}
qreal adv(const QFont& f, const QString& s) { return QFontMetricsF(f).horizontalAdvance(s); }

// Phase of the intraday buckets of loaded bars (Yahoo 1h US stocks start at hh:30): the stamp of the bar before the
// last modulo the bar length (the last one can be Yahoo's live point, stamped with the time of the last trade).
qint64 bucketPhase(const QVector<market::Bar>& b, qint64 sec) {
  if (b.isEmpty() || sec <= 0) return 0;
  const qint64 t = b.size() >= 2 ? b[b.size() - 2].t : b.last().t;
  return ((t % sec) + sec) % sec;
}
qint64 bucketOf(qint64 t, qint64 sec, qint64 phase) { return t - ((((t - phase) % sec) + sec) % sec); }
// Yahoo ends intraday series with a one-price point stamped with the last trade time: fold it into its bucket.
void foldLivePoint(QVector<market::Bar>& b, qint64 sec) {
  if (b.size() < 2 || sec <= 0 || sec >= 86400) return;
  const qint64 ph = bucketPhase(b, sec);
  auto& last = b.last();
  const qint64 k = bucketOf(last.t, sec, ph);
  if (k == last.t) return;
  auto& prev = b[b.size() - 2];
  if (k == prev.t) {
    prev.h = std::max(prev.h, last.h); prev.l = std::min(prev.l, last.l); prev.c = last.c; prev.v += last.v;
    b.removeLast();
  } else last.t = k;
}
// Period of a daily / weekly / monthly bar or quote stamped t, by the exchange-local session day.
qint64 periodKey(const QString& tz, qint64 t, qint64 sec) {
  const QDate d = market::sessionDay(tz, t);
  if (sec >= 28 * 86400) return d.year() * 12 + d.month();
  if (sec >= 7 * 86400) return d.addDays(1 - d.dayOfWeek()).toJulianDay();   // week starting Monday
  return d.toJulianDay();
}
}

CandleChart::CandleChart(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_clockTimer.setInterval(1000);
  connect(&m_clockTimer, &QTimer::timeout, this, [this] { update(m_clock.toAlignedRect()); });
  m_toastTimer.setSingleShot(true);
  connect(&m_toastTimer, &QTimer::timeout, this, [this] { m_toast.clear(); update(); });
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, &CandleChart::onQuotes);
}

void CandleChart::setSymbol(const QString& s) {
  m_symbol = s; m_bars.clear(); ++m_barsGen; m_cmp.clear(); m_cmpSym.clear(); m_drawings.clear(); m_error.clear();
  m_range.clear(); m_auto = true;
  // a drawing half-placed (or a brush stroke / measure) on the previous instrument would be completed on this one
  // with the old prices
  m_placing = false; m_brushing = false; m_pending = Drawing{}; m_measuring = false; m_measureShown = false; m_mFrom = m_mTo = {};
  // subscribe() adds to the owner's symbols: drop the previous instrument first, or every instrument browsed in the
  // sheet stays polled for the whole session
  market::QuoteHub::instance().unsubscribe(this);
  market::QuoteHub::instance().subscribe(this, {s}, isVisible());
  load();
}
void CandleChart::showLoading(const QString& s) {
  ++m_req; m_symbol = s; m_bars.clear(); ++m_barsGen; m_loading = true; m_dirty = true; update();
}
void CandleChart::setTimeframe(const QString& tf) {
  if (tf == m_tf && !m_bars.isEmpty()) return;
  m_tf = tf; m_range.clear(); load();
}
void CandleChart::applyRange(const QString& r) {
  const QString tf = rangeTf(r);
  m_range = r;
  if (tf != m_tf || m_bars.isEmpty()) { m_tf = tf; load(r); }
  else { fitInitial(); m_dirty = true; update(); }
}

void CandleChart::load(const QString& range) {
  if (m_symbol.isEmpty()) return;
  m_loading = true; m_error.clear(); m_dirty = true; update();
  const int req = ++m_req;
  const TfDef d = tfDef(m_tf);
  market::candles(m_symbol, d.iv, d.range, this, [this, req, d, range](const market::Series& s) {
    if (req != m_req) return;
    m_loading = false;
    if (!s.ok()) { m_error = s.error.isEmpty() ? QCoreApplication::translate("watch", "No data") : s.error; m_bars.clear(); ++m_barsGen; m_dirty = true; update(); emit loaded(false); return; }
    m_bars = d.agg ? aggregate(s.bars, d.agg) : s.bars;
    if (!d.agg) foldLivePoint(m_bars, d.sec);
    ++m_barsGen;
    m_tz = s.timezone;
    m_range = range;
    fitInitial();
    if (!m_cmpSym.isEmpty()) loadCompare(m_cmpSym);
    m_dirty = true; update();
    emit loaded(true);
  });
}

void CandleChart::loadCompare(const QString& sym) {
  m_cmpSym = sym; m_cmp.clear();
  const TfDef d = tfDef(m_tf);
  const int req = m_req;
  market::candles(sym, d.iv, d.range, this, [this, req, d, sym](const market::Series& s) {
    if (req != m_req || sym != m_cmpSym) return;
    m_cmp = s.ok() ? (d.agg ? aggregate(s.bars, d.agg) : s.bars) : QVector<market::Bar>{};
    if (!d.agg) foldLivePoint(m_cmp, d.sec);
    m_dirty = true; update();
  });
}

void CandleChart::onQuotes(const QStringList& syms) {
  // while a load runs m_tf may already be the new timeframe but m_bars still the old one: bucketing the quote with the
  // new timeframe's rule would append a wrong candle to the old bars
  if (m_loading || m_bars.isEmpty() || !syms.contains(m_symbol)) return;
  const auto q = market::QuoteHub::instance().quote(m_symbol);
  if (!q.valid()) return;
  const TfDef d = tfDef(m_tf);
  auto& last = m_bars.last();
  const qint64 t = q.time.toSecsSinceEpoch();
  const bool intraday = d.sec < 86400;
  const bool following = m_right >= m_bars.size() - 1.5;
  // a quote in a later bucket (intraday: aligned on the loaded bars' phase; 1D / 1W / 1M: a later session day, week
  // or month in the exchange's time zone) starts a new candle; in the last one it updates it
  bool next = false; qint64 stamp = t;
  if (intraday) {
    const qint64 ph = bucketPhase(m_bars, d.sec);
    stamp = bucketOf(t, d.sec, ph);
    next = stamp > bucketOf(last.t, d.sec, ph);
  } else next = periodKey(m_tz, t, d.sec) > periodKey(m_tz, last.t, d.sec);   // stamped like Yahoo's open bar: now
  if (next) {
    m_bars.push_back({stamp, q.price, q.price, q.price, q.price, 0});
    if (following) m_right += 1;
  } else if (t >= last.t) {
    last.c = q.price; last.h = std::max(last.h, q.price); last.l = std::min(last.l, q.price);
  } else return;
  ++m_barsGen;
  if (m_auto) autoScale();
  m_dirty = true; update();
}

// ------------------------------------------------------------------ layout and view
// clampView: a narrower plot shows fewer bars, so the old right edge can leave the allowed range (by up to 20 %
// of the old width) and the first visible index reached past the last bar (read in the compare overlay)
void CandleChart::resizeEvent(QResizeEvent*) { layout(); clampView(); m_dirty = true; }
void CandleChart::layout() {
  const qreal w = width(), h = height();
  // phones (chart under ~560 px): the toolbar takes two rows so compare and indicators stay reachable
  const qreal tb = w < 560 ? 90 : 45, top = tb + 5;
  m_top = QRectF(1, 1, w - 2, tb);
  m_left = QRectF(1, top, 57, h - top - 1);
  m_bottom = QRectF(62, h - 46, w - 63, 45);
  const QRectF area(62, top, w - 62 - 4, h - top - 50);
  m_priceAxis = QRectF(area.right() - 68, area.top(), 68, area.height() - 30);
  m_timeAxis = QRectF(area.left(), area.bottom() - 30, area.width(), 30);
  m_plot = QRectF(area.left(), area.top(), area.width() - 68, area.height() - 30);
  m_clock = QRectF(m_bottom.right() - 250, m_bottom.top(), 240, m_bottom.height());
}
void CandleChart::fitInitial() {
  if (m_bars.isEmpty()) return;
  const int n = m_bars.size();
  if (!m_range.isEmpty() && m_range != "All") {
    const qint64 from = rangeStart(m_range, m_bars.last().t);
    int i0 = 0; while (i0 < n - 1 && m_bars[i0].t < from) ++i0;
    const int count = std::max(10, n - i0);
    m_barW = std::clamp(m_plot.width() / (count + 3.0), 0.6, 40.0);
    m_right = n - 1 + 3;
  } else if (m_range == "All") {
    m_barW = std::clamp(m_plot.width() / (n + 3.0), 0.6, 40.0);
    m_right = n - 1 + 3;
  } else {
    m_barW = 7.2;
    m_right = n - 1 + 8;
  }
  m_auto = true;
  clampView(); autoScale();
}
void CandleChart::clampView() {
  if (m_bars.isEmpty()) return;
  const double vis = m_plot.width() / m_barW;
  m_right = std::clamp(m_right, std::min(vis * 0.2, double(m_bars.size() - 1)), m_bars.size() - 1 + vis * 0.8);
}
double CandleChart::xOf(double idx) const { return m_plot.right() - (m_right - idx) * m_barW - m_barW / 2; }
double CandleChart::idxOf(double x) const { return m_right - (m_plot.right() - m_barW / 2 - x) / m_barW; }
double CandleChart::toScale(double v) const { return m_log ? std::log(std::max(v, 1e-12)) : v; }
double CandleChart::fromScale(double s) const { return m_log ? std::exp(s) : s; }
double CandleChart::yOf(double price) const {
  const qreal top = m_plot.top() + 8, bot = m_plot.bottom() - 8;
  return bot - (toScale(price) - m_lo) / (m_hi - m_lo) * (bot - top);
}
double CandleChart::priceOf(double y) const {
  const qreal top = m_plot.top() + 8, bot = m_plot.bottom() - 8;
  return fromScale(m_lo + (bot - y) / (bot - top) * (m_hi - m_lo));
}
qint64 CandleChart::timeOf(double idx) const {
  if (m_bars.isEmpty()) return 0;
  const int i = int(std::round(idx));
  if (i >= 0 && i < m_bars.size()) return m_bars[i].t;
  const qint64 step = tfDef(m_tf).sec;
  return i < 0 ? m_bars.first().t + i * step : m_bars.last().t + (i - (m_bars.size() - 1)) * step;
}
double CandleChart::idxOfTime(qint64 t) const {
  auto it = std::lower_bound(m_bars.cbegin(), m_bars.cend(), t, [](const market::Bar& b, qint64 v) { return b.t < v; });
  if (it == m_bars.cend()) return m_bars.size() - 1 + double(t - m_bars.last().t) / tfDef(m_tf).sec;
  return double(it - m_bars.cbegin());
}
void CandleChart::autoScale() {
  if (m_bars.isEmpty()) return;
  const auto b = shown();
  const int i0 = std::max(0, int(std::floor(idxOf(m_plot.left())))), i1 = std::min(int(b.size()) - 1, int(std::ceil(m_right)));
  double lo = INFINITY, hi = -INFINITY;
  for (int i = i0; i <= i1; ++i) { lo = std::min(lo, toScale(b[i].l)); hi = std::max(hi, toScale(b[i].h)); }
  if (!std::isfinite(lo)) { lo = toScale(b.last().l); hi = toScale(b.last().h); }
  if (hi - lo < 1e-12) { const double d = std::abs(lo) * 0.001 + 1e-9; lo -= d; hi += d; }
  const double pad = (hi - lo) * 0.08;
  m_lo = lo - pad; m_hi = hi + pad;
  m_pctBase = i0 < b.size() ? b[i0].c : NAN;
}
// The overlay asks for the bars on every mouse move: the Heikin Ashi series is computed once per change of the bars.
const QVector<market::Bar>& CandleChart::shown() const {
  if (m_type != Heikin || m_bars.isEmpty()) return m_bars;
  if (m_heikinGen == m_barsGen && m_heikin.size() == m_bars.size()) return m_heikin;
  m_heikinGen = m_barsGen;
  QVector<market::Bar>& h = m_heikin;
  h.resize(m_bars.size());
  for (int i = 0; i < m_bars.size(); ++i) {
    const auto& b = m_bars[i];
    const double c = (b.o + b.h + b.l + b.c) / 4, o = i ? (h[i - 1].o + h[i - 1].c) / 2 : (b.o + b.c) / 2;
    h[i] = {b.t, o, std::max({b.h, o, c}), std::min({b.l, o, c}), c, b.v};
  }
  return h;
}

// Local wall-clock fields of a UTC instant, like QDateTime::fromSecsSinceEpoch(t).toLocalTime() (a mktime per call,
// twice per visible bar on every redraw of a zoomed-out chart): the UTC offset is looked up once per half hour of UTC
// time (offset changes happen on the hour or half hour) and the fields are read from a UTC date-time shifted by it.
static QDateTime localFields(qint64 t) {
  static QHash<qint64, int> offsets;
  const qint64 key = t >= 0 ? t / 1800 : (t - 1799) / 1800;
  auto it = offsets.constFind(key);
  if (it == offsets.cend()) {
    if (offsets.size() > 200000) offsets.clear();
    it = offsets.insert(key, QDateTime::fromSecsSinceEpoch(key * 1800).toLocalTime().offsetFromUtc());
  }
  return QDateTime::fromSecsSinceEpoch(t + *it, QTimeZone::UTC);
}
QString CandleChart::priceText(double p) const {
  const auto* ins = market::find(m_symbol);
  const int d = ins ? ins->decimals : fmt::priceDecimals(p, m_symbol);
  return fmt::num(p, d);
}
QString CandleChart::timeLabel(qint64 t, bool full) const {
  const QDateTime dt = QDateTime::fromSecsSinceEpoch(t).toLocalTime();
  const bool intraday = tfDef(m_tf).sec < 86400;
  if (full) return i18n::locale().toString(dt, intraday ? "dd MMM ''yy   HH:mm" : "ddd dd MMM ''yy");
  return dt.toString("HH:mm");
}

// ------------------------------------------------------------------ painting
void CandleChart::paintEvent(QPaintEvent*) {
  if (m_dirty || m_cache.size() != size() * devicePixelRatioF()) rebuild();
  QPainter p(this);
  p.drawPixmap(0, 0, m_cache);
  paintOverlay(p);
}

void CandleChart::rebuild() {
  m_dirty = false;
  if (m_plot.isEmpty()) layout();
  // the static layer is redrawn on every drag / wheel step: the pixmap is reused while the size holds (~12 MB at 2x)
  const QSize px = size() * devicePixelRatioF();
  if (m_cache.size() != px || m_cache.devicePixelRatio() != devicePixelRatioF()) {
    m_cache = QPixmap(px);
    m_cache.setDevicePixelRatio(devicePixelRatioF());
  }
  m_cache.fill(kBar());
  QPainter p(&m_cache);
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  m_hits.clear();
  const bool skeleton = m_bars.isEmpty() && m_loading;
  if (skeleton) { paintSkeleton(p); }
  else {
    p.fillRect(QRectF(0, m_top.bottom(), width(), 4), kGap());
    p.fillRect(QRectF(58, m_left.top(), 4, height() - m_left.top()), kGap());
    p.fillRect(QRectF(62, m_bottom.top() - 4, width() - 62, 4), kGap());
    p.fillRect(m_plot.united(m_priceAxis).united(m_timeAxis), kBg());
    paintToolbar(p);
    paintLeftBar(p);
    paintBottom(p, false);
    paintPlot(p);
  }
  // frame with small gold glints in the corners
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(QPen(kFrame(), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(rect()).adjusted(0, 0, -1, -1));
  ui::drawBrackets(p, QRectF(rect()).adjusted(-1, -1, 1, 1), theme::map(QColor(0x6b, 0x5f, 0x2a)), 5, true, true, true, true);
}

void CandleChart::paintSkeleton(QPainter& p) {
  p.fillRect(rect(), theme::map(QColor(0x07, 0x07, 0x07)));
  p.fillRect(QRectF(14, 15, 136, 10), theme::map(QColor(0x1a, 0x1a, 0x1a)));
  // placeholder bars like the reference's loading frame
  static const double hts[] = {.42, .66, .60, .40, .67, .63, .40, .67, .63, .42, .67, .63, .42, .68, .64, .43, .68, .64,
                               .45, .70, .64, .46, .71, .72, .50, .72, .45};
  const qreal top = 60, bot = height() - 14, w = width() - 28;
  const int n = 28; const qreal pitch = w / n, bw = pitch * 0.84;
  for (int i = 0; i < n; ++i) {
    const qreal hh = (bot - top) * hts[i % 27];
    p.fillRect(QRectF(14 + i * pitch, bot - hh, bw, hh), kSkel());
  }
}

void CandleChart::paintToolbar(QPainter& p) {
  const bool twoRows = m_top.height() > 60;
  const qreal cy0 = twoRows ? m_top.top() + 22.5 : m_top.center().y();
  qreal cy = cy0, x = 10;
  const QFont f = tvFont(15, QFont::Medium);
  p.setFont(f);
  auto button = [&](const QString& id, qreal w, bool active) {
    const QRectF r(x, cy - 16, w, 32);
    if (active) { p.setPen(Qt::NoPen); p.setBrush(kActive()); p.drawRoundedRect(r, 4, 4); }
    m_hits.push_back({r, id});
    x += w + 2;
    return r;
  };
  for (const auto& tf : kFavTf) {
    const QRectF r = button("tf:" + tf, adv(f, tfLabel(tf)) + 14, tf == m_tf);
    p.setPen(tf == m_tf ? theme::map(QColor(Qt::white)) : kText()); p.drawText(r, Qt::AlignCenter, tfLabel(tf));
  }
  if (!kFavTf.contains(m_tf)) {   // the active timeframe from the dropdown is shown too
    const QRectF r = button("tf:" + m_tf, adv(f, tfLabel(m_tf)) + 14, true);
    p.setPen(theme::map(QColor(Qt::white))); p.drawText(r, Qt::AlignCenter, tfLabel(m_tf));
  }
  auto chevron = [&](const QString& id) {
    const QRectF r = button(id, 22, false);
    p.setPen(QPen(kText(), 1.4)); const QPointF c = r.center();
    p.drawLine(c + QPointF(-4, -2), c + QPointF(0, 2)); p.drawLine(c + QPointF(0, 2), c + QPointF(4, -2));
  };
  auto sep = [&] { p.fillRect(QRectF(x + 7, cy - 13, 1, 26), theme::map(QColor(0x2a, 0x2e, 0x39))); x += 16; };
  chevron("tfmenu"); sep();
  // chart type icons: bars, candles, area
  {
    QRectF r = button("type:bars", 36, m_type == Bars);
    p.setPen(QPen(kText(), 1.5)); QPointF c = r.center();
    p.drawLine(c + QPointF(-4, -8), c + QPointF(-4, 7)); p.drawLine(c + QPointF(-8, -4), c + QPointF(-4, -4));
    p.drawLine(c + QPointF(-4, 3), c + QPointF(-1, 3));
    p.drawLine(c + QPointF(4, -6), c + QPointF(4, 9)); p.drawLine(c + QPointF(1, 5), c + QPointF(4, 5)); p.drawLine(c + QPointF(4, -2), c + QPointF(8, -2));
    r = button("type:candles", 36, m_type == Candles || m_type == Hollow || m_type == Heikin);
    c = r.center(); p.setBrush(Qt::NoBrush); p.setPen(QPen(kText(), 1.5));
    p.drawLine(c + QPointF(-4, -9), c + QPointF(-4, 9)); p.drawRect(QRectF(c.x() - 7, c.y() - 5, 6, 10));
    p.drawLine(c + QPointF(5, -9), c + QPointF(5, 9)); p.drawRect(QRectF(c.x() + 2, c.y() - 4, 6, 7));
    r = button("type:area", 36, m_type == Area || m_type == Line);
    c = r.center(); p.setPen(QPen(kText(), 1.5));
    QPainterPath a; a.moveTo(c + QPointF(-12, 8)); a.lineTo(c + QPointF(-12, 3)); a.lineTo(c + QPointF(-5, -3));
    a.lineTo(c + QPointF(0, 1)); a.lineTo(c + QPointF(11, -8)); a.lineTo(c + QPointF(11, 8)); a.closeSubpath();
    p.setBrush(QBrush(kText(), Qt::BDiagPattern)); p.drawPath(a); p.setBrush(Qt::NoBrush);
  }
  chevron("typemenu");
  if (twoRows) { x = 10; cy += 45; } else sep();
  {
    const QRectF r = button("compare", 34, !m_cmpSym.isEmpty());
    p.setPen(QPen(kText(), 1.5)); p.setBrush(Qt::NoBrush); p.drawEllipse(r.center(), 10, 10);
    p.drawLine(r.center() + QPointF(-5, 0), r.center() + QPointF(5, 0)); p.drawLine(r.center() + QPointF(0, -5), r.center() + QPointF(0, 5));
  }
  sep();
  {
    const QRectF r = button("indicators", 30 + adv(f, QCoreApplication::translate("watch", "Indicators")) + 10, false);
    const QPointF o(r.left() + 6, r.center().y() + 9);
    p.setPen(QPen(kText(), 1.4)); p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 4; ++i) { const qreal hh = 5 + i * 3.5; p.drawRect(QRectF(o.x() + i * 5.5, o.y() - hh, 3.5, hh)); }
    p.drawPolyline(QPolygonF{o + QPointF(0, -12), o + QPointF(7, -17), o + QPointF(12, -14), o + QPointF(21, -20)});
    p.setPen(kText()); p.drawText(QRectF(r.left() + 30, r.top(), r.width() - 30, r.height()), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("watch", "Indicators"));
  }
  // snapshot (camera) at the right
  {
    const QRectF r(m_top.right() - 44, cy0 - 16, 34, 32);
    m_hits.push_back({r, "snapshot"});
    const QPointF c = r.center();
    p.setPen(QPen(kText(), 1.5)); p.setBrush(Qt::NoBrush);
    QPainterPath cam; cam.addRoundedRect(QRectF(c.x() - 11, c.y() - 6, 22, 15), 3, 3);
    p.drawPath(cam); p.drawEllipse(c + QPointF(0, 1.5), 4.5, 4.5);
    p.drawLine(c + QPointF(-4, -6), c + QPointF(-2, -9)); p.drawLine(c + QPointF(-2, -9), c + QPointF(2, -9)); p.drawLine(c + QPointF(2, -9), c + QPointF(4, -6));
  }
}

void CandleChart::paintLeftBar(QPainter& p) {
  const qreal cx = m_left.center().x();
  auto slot = [&](const QString& id, qreal y, bool active) {
    const QRectF r(cx - 18, y - 18, 36, 36);
    if (active) { p.setPen(Qt::NoPen); p.setBrush(kActive()); p.drawRoundedRect(r, 4, 4); }
    m_hits.push_back({r, id});
    p.setPen(QPen(kText(), 1.5, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
    return r.center();
  };
  qreal y = m_left.top() + 29;
  QPointF c = slot("tool:cross", y, m_tool == Cross);
  p.drawLine(c + QPointF(-10, 0), c + QPointF(-3, 0)); p.drawLine(c + QPointF(3, 0), c + QPointF(10, 0));
  p.drawLine(c + QPointF(0, -10), c + QPointF(0, -3)); p.drawLine(c + QPointF(0, 3), c + QPointF(0, 10));
  y += 43; c = slot("tool:trend", y, m_tool == Trend);
  p.drawLine(c + QPointF(-8, 8), c + QPointF(8, -8)); p.drawEllipse(c + QPointF(-10, 10), 2.2, 2.2); p.drawEllipse(c + QPointF(10, -10), 2.2, 2.2);
  y += 43; c = slot("tool:fib", y, m_tool == Fib);
  for (int i = -2; i <= 2; ++i) p.drawLine(c + QPointF(-8, i * 5), c + QPointF(12, i * 5));
  p.drawEllipse(c + QPointF(-11, -10), 1.8, 1.8); p.drawEllipse(c + QPointF(-11, 10), 1.8, 1.8);
  y += 43; c = slot("tool:pattern", y, m_tool == Pattern);
  { const QPolygonF z{c + QPointF(-11, 9), c + QPointF(-6, -9), c + QPointF(0, 4), c + QPointF(6, -9), c + QPointF(11, 9)};
    p.drawPolyline(z); p.drawLine(z[0], z[2]); p.drawLine(z[2], z[4]); p.drawLine(z[1], z[3]);
    for (const auto& q : z) p.drawEllipse(q, 1.8, 1.8); }
  y += 43; c = slot("tool:position", y, m_tool == Position);
  p.drawLine(c + QPointF(-11, -9), c + QPointF(-11, 9));
  p.drawLine(c + QPointF(-7, -8), c + QPointF(11, -8)); p.drawLine(c + QPointF(-7, 0), c + QPointF(11, 0)); p.drawLine(c + QPointF(-7, 8), c + QPointF(11, 8));
  p.drawLine(c + QPointF(-3, -4), c + QPointF(7, -4)); p.drawLine(c + QPointF(-3, 4), c + QPointF(7, 4));
  y += 43; c = slot("tool:brush", y, m_tool == Brush);
  { QPainterPath b; b.moveTo(c + QPointF(-11, 8)); b.cubicTo(c + QPointF(-6, -2), c + QPointF(-2, 10), c + QPointF(3, 1));
    p.drawPath(b); p.drawLine(c + QPointF(1, 3), c + QPointF(10, -8)); p.drawLine(c + QPointF(4, 6), c + QPointF(12, -5)); }
  y += 43; c = slot("tool:text", y, m_tool == Text);
  p.drawLine(c + QPointF(-8, -9), c + QPointF(8, -9)); p.drawLine(c + QPointF(0, -9), c + QPointF(0, 10));
  p.drawLine(c + QPointF(-8, -9), c + QPointF(-8, -6)); p.drawLine(c + QPointF(8, -9), c + QPointF(8, -6));
  y += 43; c = slot("tool:emoji", y, m_tool == Emoji);
  p.drawEllipse(c, 10, 10); p.drawEllipse(c + QPointF(-3.5, -2.5), 0.8, 0.8); p.drawEllipse(c + QPointF(3.5, -2.5), 0.8, 0.8);
  p.drawArc(QRectF(c.x() - 5, c.y() - 3, 10, 9), -20 * 16, -140 * 16);
  y += 29; p.fillRect(QRectF(cx - 14, y, 28, 1), theme::map(QColor(0x2a, 0x2e, 0x39)));
  y += 29; c = slot("tool:measure", y, m_tool == Measure);
  p.save(); p.translate(c); p.rotate(-45); p.drawRect(QRectF(-12, -4.5, 24, 9));
  for (int i = -8; i <= 8; i += 4) p.drawLine(QPointF(i, -4.5), QPointF(i, i % 8 == 0 ? 0 : -1.5)); p.restore();
  y += 43; c = slot("tool:zoom", y, m_tool == Zoom);
  p.drawEllipse(c + QPointF(-2, -2), 7.5, 7.5); p.drawLine(c + QPointF(3.5, 3.5), c + QPointF(9, 9));
  p.drawLine(c + QPointF(-5.5, -2), c + QPointF(1.5, -2)); p.drawLine(c + QPointF(-2, -5.5), c + QPointF(-2, 1.5));
  y += 29; p.fillRect(QRectF(cx - 14, y, 28, 1), theme::map(QColor(0x2a, 0x2e, 0x39)));
  y += 29; c = slot("magnet", y, m_magnet);
  { QPainterPath u; u.moveTo(c + QPointF(-7, 9)); u.lineTo(c + QPointF(-7, -1)); u.arcTo(QRectF(c.x() - 7, c.y() - 8, 14, 14), 180, -180);
    u.lineTo(c + QPointF(7, 9)); p.drawPath(u); p.drawLine(c + QPointF(-10, 4), c + QPointF(-4, 4)); p.drawLine(c + QPointF(4, 4), c + QPointF(10, 4)); }
}

void CandleChart::paintBottom(QPainter& p, bool) {
  const QFont f = tvFont(15, QFont::Medium);
  p.setFont(f);
  qreal x = m_bottom.left() + 20;
  for (const auto& r : kRanges) {
    const QString lab = rangeLabel(r);
    const QRectF rr(x - 6, m_bottom.center().y() - 14, adv(f, lab) + 12, 28);
    if (r == m_range) { p.setPen(Qt::NoPen); p.setBrush(kActive()); p.drawRoundedRect(rr, 4, 4); }
    p.setPen(r == m_range ? theme::map(QColor(Qt::white)) : kText()); p.drawText(rr, Qt::AlignCenter, lab);
    m_hits.push_back({rr, "range:" + r});
    x += adv(f, lab) + 13;
  }
  // scale toggles left of the clock (left out when the bar is too narrow for them: small windows)
  const QFont sf = tvFont(14, QFont::Medium);
  qreal rx = m_clock.left() - 8;
  if (rx - 110 < x) return;
  const QList<QPair<QString, bool>> toggles{{"auto", m_auto}, {"log", m_log}, {"%", m_pct}};
  for (const auto& t : toggles) {
    const qreal w = adv(sf, t.first) + 12;
    const QRectF rr(rx - w, m_bottom.center().y() - 13, w, 26);
    p.setFont(sf); p.setPen(t.second ? theme::map(QColor(0x29, 0x9a, 0xd0)) : kDim()); p.drawText(rr, Qt::AlignCenter, t.first);
    m_hits.push_back({rr, "scale:" + t.first});
    rx -= w + 2;
  }
  p.fillRect(QRectF(m_clock.left() - 2, m_bottom.center().y() - 12, 1, 24), theme::map(QColor(0x2a, 0x2e, 0x39)));
}

void CandleChart::paintPlot(QPainter& p) {
  if (m_bars.isEmpty()) {
    p.setFont(tvFont(14)); p.setPen(kDim());
    p.drawText(m_plot, Qt::AlignCenter, m_loading ? QCoreApplication::translate("watch", "Loading…") : m_error.isEmpty() ? QCoreApplication::translate("watch", "No data") : QCoreApplication::translate("watch", "No data for %1 · %2").arg(tfLabel(m_tf), m_error));
    return;
  }
  if (m_auto) autoScale();
  const auto b = shown();
  const int n = b.size();
  const int i0 = std::max(0, int(std::floor(idxOf(m_plot.left())))), i1 = std::min(n - 1, int(std::ceil(idxOf(m_plot.right()))));
  const QFont af = tvFont(13, QFont::Medium);
  // price grid + labels
  const auto ticks = chart::niceTicks(fromScale(m_lo), fromScale(m_hi), std::max(3, int(m_plot.height() / 56)));
  p.setFont(af);
  for (double t : ticks) {
    const qreal y = yOf(t);
    if (y < m_plot.top() + 4 || y > m_plot.bottom() - 4) continue;
    p.setPen(QPen(kGrid(), 1)); p.drawLine(QPointF(m_plot.left(), y), QPointF(m_plot.right(), y));
    QString s = priceText(t);
    if (m_pct && std::isfinite(m_pctBase)) s = fmt::pct((t / m_pctBase - 1) * 100, 2);
    p.setPen(kText()); p.drawText(QRectF(m_priceAxis.left(), y - 9, m_priceAxis.width() - 6, 18), Qt::AlignRight | Qt::AlignVCenter, s);
  }
  // time labels: calendar boundaries ranked year > month > day > hours, greedy without overlap
  {
    struct Cand { int i; int rank; QString text; };
    QVector<Cand> cands;
    for (int i = std::max(1, i0); i <= i1; ++i) {
      const QDateTime a = localFields(b[i - 1].t), c = localFields(b[i].t);
      if (a.date().year() != c.date().year()) cands.push_back({i, 6, QString::number(c.date().year())});
      else if (a.date().month() != c.date().month()) cands.push_back({i, 5, i18n::locale().toString(c, "MMM")});
      else if (a.date() != c.date()) cands.push_back({i, 4, QString::number(c.date().day())});
      else {
        const int ha = a.time().hour() * 60 + a.time().minute(), hc = c.time().hour() * 60 + c.time().minute();
        int rank = 0;
        for (int step : {720, 360, 240, 180, 120, 60, 30, 15, 5, 1}) { ++rank; if (hc / step != ha / step && hc % step == 0) break; }
        cands.push_back({i, 3 - rank / 4, c.toString("HH:mm")});
      }
    }
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) { return x.rank > y.rank; });
    QVector<qreal> placed;
    for (const auto& c : cands) {
      if (c.rank < 4 && placed.size() >= 3 && cands.size() && cands.first().rank >= 4) break;   // days are enough
      const qreal x = xOf(c.i);
      if (x < m_plot.left() + 10 || x > m_plot.right() - 10) continue;
      bool ok = true; for (qreal q : placed) if (std::abs(q - x) < 72) { ok = false; break; }
      if (!ok) continue;
      placed << x;
      p.setPen(kText()); p.drawText(QRectF(x - 40, m_timeAxis.top(), 80, m_timeAxis.height()), Qt::AlignCenter, c.text);
      p.setPen(QPen(theme::map(QColor(0x13, 0x14, 0x16)), 1)); p.drawLine(QPointF(x, m_plot.top()), QPointF(x, m_plot.bottom()));
    }
  }
  // gear (scale settings) in the axis corner
  {
    const QPointF c(m_priceAxis.center().x() + 8, m_timeAxis.center().y());
    p.setPen(QPen(kText(), 1.4)); p.setBrush(Qt::NoBrush);
    QPolygonF hex; for (int k = 0; k < 6; ++k) hex << c + QPointF(8 * std::cos(k * M_PI / 3), 8 * std::sin(k * M_PI / 3));
    p.drawPolygon(hex); p.drawEllipse(c, 3, 3);
    m_hits.push_back({QRectF(c.x() - 12, c.y() - 12, 24, 24), "typemenu"});
  }
  p.save(); p.setClipRect(m_plot);
  // volume
  if (m_vol) {
    double vmax = 0; for (int i = i0; i <= i1; ++i) vmax = std::max(vmax, b[i].v);
    if (vmax > 0) for (int i = i0; i <= i1; ++i) {
      const qreal hh = b[i].v / vmax * m_plot.height() * 0.18, x = xOf(i);
      p.fillRect(QRectF(x - m_barW * 0.4, m_plot.bottom() - hh, std::max<qreal>(1, m_barW * 0.8), hh),
                 theme::alpha(b[i].c >= b[i].o ? col::candleUp() : col::candleDown(), 70));
    }
  }
  // series
  const QColor up = col::candleUp(), dn = col::candleDown();
  if (m_type == Line || m_type == Area) {
    QPainterPath path;
    for (int i = i0; i <= i1; ++i) { const QPointF pt(xOf(i), yOf(b[i].c)); if (i == i0) path.moveTo(pt); else path.lineTo(pt); }
    if (m_type == Area) {
      QPainterPath a = path; a.lineTo(xOf(i1), m_plot.bottom()); a.lineTo(xOf(i0), m_plot.bottom()); a.closeSubpath();
      QLinearGradient g(0, m_plot.top(), 0, m_plot.bottom()); g.setColorAt(0, theme::map(QColor(0x29, 0x62, 0xff, 90))); g.setColorAt(1, theme::map(QColor(0x29, 0x62, 0xff, 0)));
      p.fillPath(a, g);
    }
    p.setPen(QPen(theme::map(QColor(0x29, 0x62, 0xff)), 2)); p.setBrush(Qt::NoBrush); p.drawPath(path);
  } else {
    p.setRenderHint(QPainter::Antialiasing, false);
    const qreal bw = std::max<qreal>(1, std::floor(m_barW * 0.72));
    for (int i = i0; i <= i1; ++i) {
      const auto& k = b[i];
      const bool isUp = k.c >= k.o;
      const QColor c = isUp ? up : dn;
      const qreal x = std::round(xOf(i)), yo = yOf(k.o), yc = yOf(k.c), yh = yOf(k.h), yl = yOf(k.l);
      if (m_type == Bars) {
        p.setPen(QPen(c, std::max<qreal>(1, m_barW > 8 ? 2 : 1)));
        p.drawLine(QPointF(x, yh), QPointF(x, yl));
        p.drawLine(QPointF(x - bw / 2, yo), QPointF(x, yo)); p.drawLine(QPointF(x, yc), QPointF(x + bw / 2, yc));
        continue;
      }
      p.setPen(QPen(isUp ? theme::map(QColor(0x6c, 0xe6, 0xf2)) : theme::map(QColor(0xdd, 0x5c, 0x67)), 1));
      p.drawLine(QPointF(x, yh), QPointF(x, std::min(yo, yc))); p.drawLine(QPointF(x, std::max(yo, yc)), QPointF(x, yl));
      const QRectF body(x - std::floor(bw / 2), std::min(yo, yc), bw, std::max<qreal>(1, std::abs(yc - yo)));
      if (m_type == Hollow && isUp) { p.setPen(QPen(c, 1)); p.setBrush(Qt::NoBrush); p.drawRect(body.adjusted(0, 0, -1, -1)); }
      else p.fillRect(body, c);
    }
    p.setRenderHint(QPainter::Antialiasing, true);
  }
  // indicators
  auto line = [&](const QVector<double>& v, const QColor& c, qreal w = 1.3) {
    QPainterPath path; bool st = false;
    for (int i = i0; i <= i1 && i < v.size(); ++i) {
      if (!std::isfinite(v[i])) { st = false; continue; }
      const QPointF pt(xOf(i), yOf(v[i])); if (!st) { path.moveTo(pt); st = true; } else path.lineTo(pt);
    }
    p.setPen(QPen(c, w)); p.setBrush(Qt::NoBrush); p.drawPath(path);
  };
  // The static layer is rebuilt on every pan / zoom step: the averages over the whole series are kept until the bars
  // (or the Heikin Ashi choice, which changes the series) change, instead of being recomputed per frame.
  const bool heikin = m_type == Heikin;
  if (m_ind.gen != m_barsGen || m_ind.heikin != heikin) { m_ind = IndCache{}; m_ind.gen = m_barsGen; m_ind.heikin = heikin; }
  auto cached = [&](QVector<double>& slot, auto compute) -> const QVector<double>& {
    if (slot.size() != b.size()) slot = compute();
    return slot;
  };
  if (m_sma20) line(cached(m_ind.sma20, [&] { return sma(b, 20); }), theme::map(QColor(0xf2, 0xb8, 0x3a)));
  if (m_sma50) line(cached(m_ind.sma50, [&] { return sma(b, 50); }), theme::map(QColor(0xb8, 0x64, 0xc6)));
  if (m_sma200) line(cached(m_ind.sma200, [&] { return sma(b, 200); }), theme::map(QColor(0xe8, 0xe4, 0xdc)));
  if (m_ema21) line(cached(m_ind.ema21, [&] { return ema(b, 21); }), theme::map(QColor(0x5d, 0xb9, 0x63)));
  if (m_boll) {
    const auto& mid = cached(m_ind.sma20, [&] { return sma(b, 20); });
    if (m_ind.bollUp.size() != b.size() || m_ind.bollLo.size() != b.size()) {
      QVector<double> upb(b.size(), NAN), lob(b.size(), NAN);
      for (int i = 19; i < b.size(); ++i) {
        double s = 0; for (int k = i - 19; k <= i; ++k) s += (b[k].c - mid[i]) * (b[k].c - mid[i]);
        const double sd = std::sqrt(s / 20); upb[i] = mid[i] + 2 * sd; lob[i] = mid[i] - 2 * sd;
      }
      m_ind.bollUp = upb; m_ind.bollLo = lob;
    }
    line(mid, theme::map(QColor(0x29, 0x62, 0xff, 200)), 1); line(m_ind.bollUp, theme::map(QColor(0x29, 0x9a, 0xd0)), 1); line(m_ind.bollLo, theme::map(QColor(0x29, 0x9a, 0xd0)), 1);
  }
  // compare overlay, rebased on the first visible bar
  if (!m_cmp.isEmpty() && i0 >= 0 && i0 <= i1 && i1 < b.size()) {
    auto cmpAt = [&](qint64 t) {
      auto it = std::upper_bound(m_cmp.cbegin(), m_cmp.cend(), t, [](qint64 v, const market::Bar& x) { return v < x.t; });
      return it == m_cmp.cbegin() ? NAN : (it - 1)->c;
    };
    const double c0 = cmpAt(b[i0].t), m0 = b[i0].c;
    if (std::isfinite(c0) && c0 > 0) {
      QVector<double> v(b.size(), NAN);
      for (int i = i0; i <= i1; ++i) { const double c = cmpAt(b[i].t); if (std::isfinite(c)) v[i] = m0 * c / c0; }
      line(v, kCmp(), 1.6);
    }
  }
  // drawings
  if (!m_hideDrawings) for (const auto& d : m_drawings) paintDrawing(p, d, nullptr);
  // last price line
  const double last = b.last().c;
  const QColor lc = b.last().c >= b.last().o ? up : dn;
  const qreal ly = yOf(last);
  p.setPen(QPen(lc, 1, Qt::DotLine)); p.drawLine(QPointF(m_plot.left(), ly), QPointF(m_plot.right(), ly));
  p.restore();
  // drawings' price tags and the last price tag on the axis
  if (ly >= m_plot.top() - 2 && ly <= m_plot.bottom() + 2) {
    const QRectF tag(m_priceAxis.left() + 2, ly - 10, m_priceAxis.width() - 4, 20);
    p.setPen(Qt::NoPen); p.setBrush(lc); p.drawRoundedRect(tag, 2, 2);
    p.setFont(tvFont(13, QFont::DemiBold)); p.setPen(lc == up ? QColor(0x02, 0x10, 0x14) : QColor(Qt::white));
    p.drawText(tag.adjusted(0, 0, -4, 0), Qt::AlignRight | Qt::AlignVCenter, m_pct && std::isfinite(m_pctBase) ? fmt::pct((last / m_pctBase - 1) * 100) : priceText(last));
  }
}

void CandleChart::paintOverlay(QPainter& p) {
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  // clock (redrawn every second, outside the cache)
  if (!(m_bars.isEmpty() && m_loading)) {
    p.fillRect(m_clock.adjusted(0, 2, 0, -2), kBar());
    const QDateTime now = QDateTime::currentDateTime();
    const int off = now.offsetFromUtc();
    QString tz = "UTC";
    if (off != 0) tz += (off > 0 ? "+" : "-") + QString::number(std::abs(off) / 3600) + (std::abs(off) % 3600 ? QString(":%1").arg(std::abs(off) % 3600 / 60, 2, 10, QChar('0')) : QString());
    p.setFont(tvFont(15, QFont::Medium)); p.setPen(kText());
    p.drawText(m_clock.adjusted(0, 0, -6, 0), Qt::AlignRight | Qt::AlignVCenter, now.toString("HH:mm:ss") + " " + tz);
  }
  if (m_bars.isEmpty()) {
    if (!m_loading && !m_error.isEmpty() && !m_toast.isEmpty()) {}
    return;
  }
  // hovered button
  if (!m_hoverHit.isEmpty()) for (const auto& h : m_hits) if (h.id == m_hoverHit) {
    p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(255, 255, 255, 16))); p.drawRoundedRect(h.r, 4, 4);
  }
  const auto b = shown();
  const bool inPlot = m_in && m_plot.contains(m_mouse);
  int hi = b.size() - 1;
  if (inPlot) hi = std::clamp(int(std::round(idxOf(m_mouse.x()))), 0, int(b.size()) - 1);
  // legend
  {
    qreal x = m_plot.left() + 14; qreal y = m_plot.top() + 19;
    const auto* ins = market::find(m_symbol);
    if (ins && ins->category == market::Category::Forex) {
      // two round flags, the quote one in front (TradingView's pair badge)
      auto roundFlag = [&](const QString& ccy, const QPointF& c, qreal r) {
        QPainterPath clip; clip.addEllipse(c, r, r);
        p.save(); p.setClipPath(clip);
        p.drawPixmap(QRectF(c.x() - r * 1.45, c.y() - r, r * 2.9, r * 2), ui::flag(ccy, QSize(int(r * 3), int(r * 2) + 1), devicePixelRatioF()), QRectF());
        p.restore();
        p.setPen(QPen(kBg(), 1.2)); p.setBrush(Qt::NoBrush); p.drawEllipse(c, r, r);
      };
      roundFlag(ins->base, QPointF(x + 7, y + 1), 7);
      roundFlag(ins->quote, QPointF(x + 14, y - 3), 6);
      x += 26;
    } else {
      const QPixmap ic = instrumentIcon(m_symbol, 15, devicePixelRatioF());
      p.drawPixmap(QPointF(x, y - 8), ic); x += ic.width() / ic.devicePixelRatio() + 6;
    }
    QString name = ins ? ins->name : m_symbol;
    if (ins && ins->category == market::Category::Forex) name = currencyName(ins->base) + " / " + currencyName(ins->quote);
    const QFont tf = tvFont(17, QFont::Medium);
    QString title = QString("%1 · %2 · YAHOO").arg(name, tfLabel(m_tf));
    // phones (plot under 420 px): the title is elided before the legend pill instead of running under the price axis
    const bool narrow = m_plot.width() < 420;
    if (narrow) title = QFontMetricsF(tf).elidedText(title, Qt::ElideRight, m_plot.right() - 8 - 54 - x);
    p.setFont(tf); p.setPen(theme::map(QColor(0xec, 0xec, 0xee))); p.drawText(QPointF(x, y + 6), title);
    x += adv(tf, title) + 10;
    // "− ≈" pill: collapses the OHLC values (TradingView's legend toggle)
    m_legendPill = QRectF(x, y - 7, 40, 20);
    p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0x2a, 0x2e, 0x39))); p.drawRoundedRect(m_legendPill, 5, 5);
    p.setBrush(theme::map(QColor(0x4a, 0x1c, 0x24))); p.drawRoundedRect(QRectF(x + 20, y - 7, 20, 20), 5, 5);
    p.setPen(QPen(theme::map(QColor(0xe6, 0xe6, 0xe9)), 2.2, Qt::SolidLine, Qt::RoundCap)); p.drawLine(QPointF(x + 6, y + 3), QPointF(x + 14, y + 3));
    p.setPen(QPen(theme::map(QColor(0xf2, 0x5f, 0x6a)), 1.4)); 
    for (int k = 0; k < 2; ++k) { QPainterPath w; const qreal yy = y + 0.5 + k * 5; w.moveTo(x + 24, yy); w.cubicTo(x + 27, yy - 3, x + 30, yy + 3, x + 36, yy - 1); p.drawPath(w); }
    x += 40 + 14;

    const auto& k = b[hi];
    const QColor c = k.c >= k.o ? theme::map(QColor(0x22, 0xc8, 0xd8)) : theme::map(QColor(0xf2, 0x5f, 0x6a));
    QFont vf = tvFont(14, QFont::Medium);
    const double prev = hi > 0 ? b[hi - 1].c : k.o;
    QString chg = QString("%1%2 (%3)").arg(k.c - prev >= 0 ? "+" : "-", priceText(std::abs(k.c - prev)), fmt::pct((k.c / prev - 1) * 100));
    auto ohlcWidth = [&] {
      qreal w = adv(vf, chg);
      for (const QString& v : {priceText(k.o), priceText(k.h), priceText(k.l), priceText(k.c)}) w += adv(vf, v) + adv(vf, "O") + 10;
      return w;
    };
    qreal ohlcW = ohlcWidth();
    if (x + ohlcW > m_plot.right() - 8) { x = m_plot.left() + 14; y += 22; }   // narrow chart: values on a second line
    if (narrow && x + ohlcW > m_plot.right() - 8) {   // phones: smaller values, then the change is left out (crosshair shows it)
      vf = tvFont(12, QFont::Medium); ohlcW = ohlcWidth();
      if (x + ohlcW > m_plot.right() - 8) chg.clear();
    }
    p.setFont(vf);
    if (!m_legendCollapsed) for (const auto& [l, v] : QList<QPair<QString, QString>>{{"O", priceText(k.o)}, {"H", priceText(k.h)}, {"L", priceText(k.l)}, {"C", priceText(k.c)}}) {
      p.setPen(kText()); p.drawText(QPointF(x, y + 6), l); x += adv(vf, l) + 1;
      p.setPen(c); p.drawText(QPointF(x, y + 6), v); x += adv(vf, v) + 9;
    }
    if (!m_legendCollapsed) { p.setPen(c); p.drawText(QPointF(x, y + 6), chg); }
    // indicator / compare lines under the title
    qreal ly = y + 28;
    p.setFont(tvFont(13));
    auto item = [&](const QString& s, const QColor& cc) { p.setPen(cc); p.drawText(QPointF(m_plot.left() + 14, ly), s); ly += 19; };
    if (!m_cmpSym.isEmpty()) item(QCoreApplication::translate("watch", "Compare: %1%2").arg(market::display(m_cmpSym), m_cmp.isEmpty() ? QCoreApplication::translate("watch", " (loading)") : QString()), kCmp());
    if (m_sma20) item("SMA 20", theme::map(QColor(0xf2, 0xb8, 0x3a)));
    if (m_sma50) item("SMA 50", theme::map(QColor(0xb8, 0x64, 0xc6)));
    if (m_sma200) item("SMA 200", theme::map(QColor(0xe8, 0xe4, 0xdc)));
    if (m_ema21) item("EMA 21", theme::map(QColor(0x5d, 0xb9, 0x63)));
    if (m_boll) item("BB 20 2", theme::map(QColor(0x29, 0x9a, 0xd0)));
    if (m_vol) item(QCoreApplication::translate("watch", "Volume"), kDim());
  }
  if (m_loading) { p.setFont(tvFont(13)); p.setPen(kDim()); p.drawText(m_plot.adjusted(0, 0, -10, -8), Qt::AlignRight | Qt::AlignBottom, QCoreApplication::translate("watch", "Loading %1…").arg(tfLabel(m_tf))); }
  // measure box
  if (m_measuring || ((m_tool == Measure || m_measureShown) && !m_mFrom.isNull() && m_mFrom != m_mTo)) {
    const QRectF r = QRectF(m_mFrom, m_mTo).normalized();
    const double p0 = priceOf(m_mFrom.y()), p1 = priceOf(m_mTo.y());
    const bool upm = p1 >= p0;
    const QColor c = upm ? theme::map(QColor(0x29, 0x62, 0xff)) : theme::map(QColor(0xf2, 0x5f, 0x6a));
    p.setPen(Qt::NoPen); p.setBrush(theme::alpha(c, 50)); p.drawRect(r);
    const int bars = int(std::round(idxOf(m_mTo.x()) - idxOf(m_mFrom.x())));
    const QString s = QString("%1 (%2)\n%3 bars").arg(priceText(p1 - p0), fmt::pct((p1 / p0 - 1) * 100)).arg(bars);
    const QRectF box(r.center().x() - 70, (upm ? r.top() - 46 : r.bottom() + 6), 140, 40);
    p.setBrush(c); p.drawRoundedRect(box, 4, 4);
    p.setPen(Qt::white); p.setFont(tvFont(12, QFont::Medium)); p.drawText(box, Qt::AlignCenter, s);
  }
  if (m_tool == Zoom && m_measuring) {
    const QRectF r = QRectF(QPointF(m_mFrom.x(), m_plot.top()), QPointF(m_mTo.x(), m_plot.bottom())).normalized();
    p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0x29, 0x62, 0xff, 40))); p.drawRect(r);
  }
  // pending drawing (preview to the mouse) and brush stroke in progress
  if ((m_placing || m_brushing) && inPlot) {
    p.save(); p.setClipRect(m_plot);
    paintDrawing(p, m_pending, m_placing ? &m_mouse : nullptr);
    p.restore();
  }
  // floating zoom / scroll buttons when the cursor is near the plot bottom (TradingView's navigation)
  for (const auto& [r, id] : navButtons()) {
    p.setPen(Qt::NoPen); p.setBrush(id == m_hoverHit ? theme::map(QColor(0x3a, 0x3e, 0x49, 240)) : theme::map(QColor(0x2a, 0x2e, 0x39, 225))); p.drawRoundedRect(r, 5, 5);
    p.setPen(QPen(kText(), 1.6, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
    const QPointF c = r.center();
    if (id == "nav:out" || id == "nav:in") { p.drawLine(c + QPointF(-6, 0), c + QPointF(6, 0)); if (id == "nav:in") p.drawLine(c + QPointF(0, -6), c + QPointF(0, 6)); }
    else if (id == "nav:left") { p.drawLine(c + QPointF(2, -6), c + QPointF(-3, 0)); p.drawLine(c + QPointF(-3, 0), c + QPointF(2, 6)); }
    else if (id == "nav:right") { p.drawLine(c + QPointF(-2, -6), c + QPointF(3, 0)); p.drawLine(c + QPointF(3, 0), c + QPointF(-2, 6)); }
    else { p.drawArc(QRectF(c.x() - 6.5, c.y() - 6.5, 13, 13), 100 * 16, 290 * 16); p.drawLine(c + QPointF(-1, -6.5), c + QPointF(-4.5, -9)); p.drawLine(c + QPointF(-1, -6.5), c + QPointF(-4, -3.5)); }
  }
  // crosshair
  if (inPlot) {
    const qreal x = xOf(hi);
    double price = priceOf(m_mouse.y());
    qreal y = m_mouse.y();
    if (m_magnet) {
      const auto& k = b[hi]; double best = k.c;
      for (double v : {k.o, k.h, k.l, k.c}) if (std::abs(v - price) < std::abs(best - price)) best = v;
      price = best; y = yOf(best);
    }
    QPen dash(theme::map(QColor(0x9a, 0x9a, 0x9a)), 1, Qt::DashLine); dash.setDashPattern({4, 4});
    p.setPen(dash);
    p.drawLine(QPointF(x, m_plot.top()), QPointF(x, m_plot.bottom()));
    p.drawLine(QPointF(m_plot.left(), y), QPointF(m_plot.right(), y));
    const QFont f = tvFont(13, QFont::DemiBold); p.setFont(f);
    QString ps = priceText(price);
    if (m_pct && std::isfinite(m_pctBase)) ps = fmt::pct((price / m_pctBase - 1) * 100);
    const QRectF pr(m_priceAxis.left() + 2, y - 10, m_priceAxis.width() - 4, 20);
    p.setPen(Qt::NoPen); p.setBrush(kLabel()); p.drawRoundedRect(pr, 2, 2);
    p.setPen(theme::map(QColor(Qt::white))); p.drawText(pr.adjusted(0, 0, -4, 0), Qt::AlignRight | Qt::AlignVCenter, ps);
    const QString ts = timeLabel(timeOf(hi), true);
    const qreal tw = adv(f, ts) + 20;
    const QRectF tr_(std::clamp(x - tw / 2, m_plot.left(), m_plot.right() - tw), m_timeAxis.top() + 3, tw, 24);
    p.setPen(Qt::NoPen); p.setBrush(kLabel()); p.drawRoundedRect(tr_, 2, 2);
    p.setPen(theme::map(QColor(Qt::white))); p.drawText(tr_, Qt::AlignCenter, ts);
  }
  if (!m_toast.isEmpty()) {
    const QFont f = tvFont(13, QFont::Medium); const qreal w = adv(f, m_toast) + 28;
    const QRectF r(m_plot.center().x() - w / 2, m_plot.top() + 50, w, 30);
    p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0x2a, 0x2e, 0x39, 235))); p.drawRoundedRect(r, 4, 4);
    p.setFont(f); p.setPen(theme::map(QColor(Qt::white))); p.drawText(r, Qt::AlignCenter, m_toast);
  }
}

// ------------------------------------------------------------------ interaction
QString CandleChart::hitAt(const QPointF& pt) const {
  for (const auto& h : m_hits) if (h.r.contains(pt)) return h.id;
  return {};
}
void CandleChart::hoverAt(const QPointF& pt) { m_mouse = pt; m_in = true; update(); }

void CandleChart::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pt = e->position();
  m_mouse = pt; m_in = true;
  if (m_axisDrag) {
    // a floor on the half-range: zooming in without one shrank it below the prices' resolution, and the axis ticks
    // (chart::niceTicks) then never ended
    const double f = std::exp((pt.y() - m_dragFrom.y()) / 200.0), mid = (m_dragLo + m_dragHi) / 2,
                 half = std::max((m_dragHi - m_dragLo) / 2 * f, std::max(std::abs(mid), 1.0) * 1e-9);
    m_lo = mid - half; m_hi = mid + half; m_auto = false; m_dirty = true; update(); return;
  }
  if (m_drag) {
    m_right = m_dragRight - (pt.x() - m_dragFrom.x()) / m_barW;
    if (!m_auto) { const double d = (pt.y() - m_dragFrom.y()) / (m_plot.height() - 16) * (m_dragHi - m_dragLo); m_lo = m_dragLo + d; m_hi = m_dragHi + d; }
    clampView(); m_dirty = true; update(); return;
  }
  if (m_measuring) m_mTo = pt;
  if (m_brushing && m_plot.contains(pt)) { m_pending.t << timeOf(idxOf(pt.x())); m_pending.p << priceOf(pt.y()); }
  QString h = hitAt(pt);
  for (const auto& [r, id] : navButtons()) if (r.contains(pt)) h = id;
  if (h != m_hoverHit) { m_hoverHit = h; }
  setCursor(!h.isEmpty() ? Qt::PointingHandCursor : m_priceAxis.contains(pt) ? Qt::SizeVerCursor
            : m_plot.contains(pt) ? Qt::CrossCursor : Qt::ArrowCursor);
  update();
}
void CandleChart::leaveEvent(QEvent*) { m_in = false; m_hoverHit.clear(); update(); }
void CandleChart::mousePressEvent(QMouseEvent* e) {
  const QPointF pt = e->position();
  if (e->button() != Qt::LeftButton) return;
  if (m_measureShown) { m_measureShown = false; update(); }   // a finished measurement stays until the next click
  for (const auto& [r, id] : navButtons()) if (r.contains(pt)) { click(id, e->globalPosition().toPoint()); return; }
  if (m_legendPill.contains(pt)) { m_legendCollapsed = !m_legendCollapsed; update(); return; }
  if (const QString h = hitAt(pt); !h.isEmpty()) { click(h, e->globalPosition().toPoint()); return; }
  if (m_bars.isEmpty()) return;
  if (m_priceAxis.contains(pt)) { m_axisDrag = true; m_dragFrom = pt; m_dragLo = m_lo; m_dragHi = m_hi; return; }
  if (!m_plot.contains(pt)) return;
  const double idx = idxOf(pt.x());
  double price = priceOf(pt.y());
  if (m_magnet) {
    const int i = std::clamp(int(std::round(idx)), 0, int(m_bars.size()) - 1); const auto& k = m_bars[i]; double best = k.c;
    for (double v : {k.o, k.h, k.l, k.c}) if (std::abs(v - price) < std::abs(best - price)) best = v;
    price = best;
  }
  const qint64 t = timeOf(idx);
  switch (m_tool) {
    case Trend: case Fib: case Pattern: {
      if (!m_placing) { m_placing = true; m_pending = Drawing{}; m_pending.kind = m_tool == Trend ? KTrend : m_tool == Fib ? KFib : KPattern; }
      m_pending.t << t; m_pending.p << price;
      if (m_pending.t.size() >= pointsNeeded(m_tool)) { m_drawings << m_pending; m_placing = false; m_tool = Cross; m_dirty = true; }
      update(); return;
    }
    case Position: {
      Drawing d; d.kind = KPosition; d.t << t; d.p << price;
      const double span = fromScale(m_hi) - fromScale(m_lo);
      d.p << price + span * 0.12 << price - span * 0.06;          // target, stop (drawn 2:1, as TradingView's default box)
      d.t << timeOf(idx + std::max(8.0, 120 / m_barW));
      m_drawings << d; m_tool = Cross; m_dirty = true; update(); return;
    }
    case Brush: m_brushing = true; m_pending = Drawing{}; m_pending.kind = KBrush; m_pending.t << t; m_pending.p << price; update(); return;
    case Text: case Emoji: {
      QString txt;
      if (m_tool == Text) {
        bool ok = false;
        txt = QInputDialog::getText(this, QCoreApplication::translate("watch", "Text"), QCoreApplication::translate("watch", "Text on the chart:"), QLineEdit::Normal, {}, &ok);
        if (!ok) txt.clear();
      } else {
        QMenu m(this);
        for (const QString& em : {QStringLiteral("🙂"), QStringLiteral("👍"), QStringLiteral("👎"), QStringLiteral("🚀"), QStringLiteral("⚠️"),
                                 QStringLiteral("🔥"), QStringLiteral("❤️"), QStringLiteral("⭐")}) m.addAction(em);
        if (auto* a = m.exec(e->globalPosition().toPoint())) txt = a->text();
      }
      if (!txt.trimmed().isEmpty()) { Drawing d; d.kind = m_tool == Text ? KText : KEmoji; d.t << t; d.p << price; d.text = txt; m_drawings << d; }
      m_tool = Cross; m_dirty = true; update(); return;
    }
    case Measure: case Zoom: m_measuring = true; m_mFrom = m_mTo = pt; update(); return;
    case Cross: break;
  }
  m_drag = true; m_dragFrom = pt; m_dragRight = m_right; m_dragLo = m_lo; m_dragHi = m_hi;
  setCursor(Qt::ClosedHandCursor);
}
void CandleChart::mouseReleaseEvent(QMouseEvent*) {
  if (m_brushing) { m_brushing = false; if (m_pending.t.size() > 1) m_drawings << m_pending; m_tool = Cross; m_dirty = true; }
  if (m_measuring && m_tool == Zoom) {
    const double a = idxOf(std::min(m_mFrom.x(), m_mTo.x())), b = idxOf(std::max(m_mFrom.x(), m_mTo.x()));
    if (b - a >= 3) { m_barW = std::clamp(m_plot.width() / (b - a), 0.6, 60.0); m_right = b; m_auto = true; clampView(); }
    m_measuring = false; m_tool = Cross; m_mFrom = m_mTo = {}; m_dirty = true;
  } else if (m_measuring) { m_measuring = false; m_tool = Cross; m_measureShown = true; }   // keep the result visible
  m_drag = false; m_axisDrag = false;
  update();
}
void CandleChart::mouseDoubleClickEvent(QMouseEvent* e) {
  if (m_priceAxis.contains(e->position())) { m_auto = true; m_dirty = true; update(); return; }
  if (m_plot.contains(e->position())) { m_range.clear(); fitInitial(); m_mFrom = m_mTo = {}; m_dirty = true; update(); }
}
void CandleChart::wheelEvent(QWheelEvent* e) {
  if (m_bars.isEmpty() || !(m_plot.contains(e->position()) || m_timeAxis.contains(e->position()))) { e->ignore(); return; }
  const QPoint d = e->angleDelta();
  if (std::abs(d.x()) > std::abs(d.y())) {   // horizontal trackpad swipe: pan
    m_right -= d.x() / 8.0 / m_barW * 2; clampView(); m_dirty = true; update(); e->accept(); return;
  }
  const double anchor = idxOf(e->position().x());
  const double f = std::pow(1.0015, d.y());
  m_barW = std::clamp(m_barW * f, 0.6, 60.0);
  m_right = anchor + (m_plot.right() - m_barW / 2 - e->position().x()) / m_barW;
  m_range.clear();
  clampView(); m_dirty = true; update(); e->accept();
}
void CandleChart::showEvent(QShowEvent*) {
  m_clockTimer.start();
  if (!m_symbol.isEmpty()) market::QuoteHub::instance().setActive(this, true);
}
void CandleChart::hideEvent(QHideEvent*) {
  m_clockTimer.stop();
  market::QuoteHub::instance().setActive(this, false);
}

void CandleChart::click(const QString& id, const QPoint& gp) {
  auto menu = [&](const QList<QPair<QString, QString>>& items, const QString& current) {
    QMenu m(this);
    for (const auto& [label, key] : items) {
      if (label.isEmpty()) { m.addSeparator(); continue; }
      auto* a = m.addAction(label); a->setData(key); a->setCheckable(!current.isNull()); a->setChecked(key == current);
    }
    auto* a = m.exec(gp);
    return a ? a->data().toString() : QString();
  };
  if (id.startsWith("tf:")) setTimeframe(id.mid(3));
  else if (id == "tfmenu") {
    QList<QPair<QString, QString>> it; for (const auto& t : kAllTf) it << qMakePair(tfLabel(t), t);
    const QString r = menu(it, m_tf); if (!r.isEmpty()) setTimeframe(r);
  } else if (id.startsWith("type:")) {
    const QString t = id.mid(5);
    m_type = t == "bars" ? Bars : t == "area" ? Area : Candles; m_dirty = true;
  } else if (id == "typemenu") {
    const QString cur = QString::number(int(m_type));
    const QString r = menu({{QCoreApplication::translate("watch", "Bars"), "0"}, {QCoreApplication::translate("watch", "Candles"), "1"}, {QCoreApplication::translate("watch", "Hollow candles"), "2"}, {QCoreApplication::translate("watch", "Line"), "3"},
                            {QCoreApplication::translate("watch", "Area"), "4"}, {QCoreApplication::translate("watch", "Heikin Ashi"), "5"}}, cur);
    if (!r.isEmpty()) { m_type = Type(r.toInt()); m_dirty = true; }
  } else if (id == "compare") {
    QList<QPair<QString, QString>> it;
    const auto* ins = market::find(m_symbol);
    QStringList cands;
    if (ins && ins->category == market::Category::Forex) cands = {"EURUSD", "GBPUSD", "USDJPY", "AUDUSD", "USDCAD", "USDCHF", "NZDUSD", "DXY", "XAUUSD"};
    else if (ins && ins->category == market::Category::Crypto) cands = {"BTCUSD", "ETHUSD", "SOLUSD", "NDX", "XAUUSD"};
    else if (ins && ins->category == market::Category::Commodity) cands = {"XAUUSD", "XAGUSD", "WTI", "BRENT", "COPPER", "DXY", "SPX"};
    else cands = {"SPX", "NDX", "DOW", "RUT", "DAX", "NIKKEI", "DXY", "XAUUSD", "US10Y"};
    for (const auto& c : cands) if (c != m_symbol) it << qMakePair(market::display(c) == c && market::find(c) ? market::find(c)->name : market::display(c), c);
    if (!m_cmpSym.isEmpty()) { it << qMakePair(QString(), QString()) << qMakePair(QCoreApplication::translate("watch", "Remove compare"), QString("-")); }
    const QString r = menu(it, m_cmpSym);
    if (r == "-") { m_cmpSym.clear(); m_cmp.clear(); m_dirty = true; }
    else if (!r.isEmpty()) loadCompare(r);
  } else if (id == "indicators") {
    QMenu m(this);
    auto add = [&](const QString& l, bool& flag) { auto* a = m.addAction(l); a->setCheckable(true); a->setChecked(flag); connect(a, &QAction::toggled, this, [this, &flag](bool on) { flag = on; m_dirty = true; update(); }); };
    add(QCoreApplication::translate("watch", "Volume"), m_vol); add("SMA 20", m_sma20); add("SMA 50", m_sma50); add("SMA 200", m_sma200); add("EMA 21", m_ema21); add(QCoreApplication::translate("watch", "Bollinger Bands 20, 2"), m_boll);
    m.exec(gp);
  } else if (id == "snapshot") snapshot();
  else if (id.startsWith("range:")) applyRange(id.mid(6));
  else if (id.startsWith("scale:")) {
    const QString s = id.mid(6);
    if (s == "log") { m_log = !m_log; m_auto = true; }
    else if (s == "%") m_pct = !m_pct;
    else m_auto = !m_auto;
    m_dirty = true;
  } else if (id.startsWith("tool:")) {
    const QString t = id.mid(5);
    static const QHash<QString, Tool> tools{{"cross", Cross}, {"trend", Trend}, {"fib", Fib}, {"pattern", Pattern}, {"position", Position},
                                            {"brush", Brush}, {"text", Text}, {"emoji", Emoji}, {"measure", Measure}, {"zoom", Zoom}};
    m_tool = tools.value(t, Cross);
    m_placing = false; m_mFrom = m_mTo = {}; m_dirty = true;
  } else if (id == "magnet") { m_magnet = !m_magnet; m_dirty = true; }
  else if (id == "hide") { m_hideDrawings = !m_hideDrawings; m_dirty = true; }
  else if (id.startsWith("nav:") && !m_bars.isEmpty()) {
    const double vis = m_plot.width() / m_barW;
    if (id == "nav:in" || id == "nav:out") { const double anchor = m_right; m_barW = std::clamp(m_barW * (id == "nav:in" ? 1.25 : 0.8), 0.6, 60.0); m_right = anchor; }
    else if (id == "nav:left") m_right -= vis * 0.25;
    else if (id == "nav:right") m_right += vis * 0.25;
    else { m_range.clear(); fitInitial(); }
    m_range.clear(); clampView(); m_dirty = true;
  }
  else if (id == "trash") { m_drawings.clear(); m_dirty = true; }
  update();
}

int CandleChart::pointsNeeded(Tool t) const { return t == Pattern ? 5 : 2; }

void CandleChart::paintDrawing(QPainter& p, const Drawing& d, const QPointF* preview) const {
  const QColor blue(0x29, 0x62, 0xff);
  QVector<QPointF> pts;
  for (int i = 0; i < d.t.size(); ++i) pts << QPointF(xOf(idxOfTime(d.t[i])), yOf(d.p[i]));
  if (preview) pts << *preview;
  if (pts.isEmpty()) return;
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(blue, 1.6)); p.setBrush(Qt::NoBrush);
  switch (d.kind) {
    case KTrend: if (pts.size() >= 2) p.drawLine(pts[0], pts[1]); break;
    case KHLine: p.drawLine(QPointF(m_plot.left(), pts[0].y()), QPointF(m_plot.right(), pts[0].y())); break;
    case KFib: {
      if (pts.size() < 2) break;
      static const double lv[] = {0, 0.236, 0.382, 0.5, 0.618, 0.786, 1};
      // not static: theme::map() follows the current theme, a static table kept the first theme's colours
      const QColor cs[] = {theme::map(QColor(0x78, 0x7b, 0x86)), theme::map(QColor(0xf2, 0x36, 0x45)), theme::map(QColor(0xff, 0x98, 0x00)), theme::map(QColor(0x4c, 0xaf, 0x50)),
                                  theme::map(QColor(0x08, 0x99, 0x81)), theme::map(QColor(0x00, 0xbc, 0xd4)), theme::map(QColor(0x78, 0x7b, 0x86))};
      const double p0 = priceOf(pts[0].y()), p1 = priceOf(pts[1].y());
      const qreal x0 = std::min(pts[0].x(), pts[1].x()), x1 = std::max(pts[0].x(), pts[1].x());
      p.setFont(tvFont(11));
      for (int k = 0; k < 7; ++k) {
        const double pr = p1 + (p0 - p1) * lv[k]; const qreal y = yOf(pr);
        p.setPen(QPen(cs[k], 1.2)); p.drawLine(QPointF(x0, y), QPointF(x1, y));
        p.drawText(QPointF(x0 + 2, y - 3), QString("%1 (%2)").arg(QString::number(lv[k], 'f', 3), priceText(pr)));
      }
      p.setPen(QPen(theme::alpha(blue, 160), 1, Qt::DashLine)); p.drawLine(pts[0], pts[1]);
      break;
    }
    case KPattern: {
      static const char* names[] = {"X", "A", "B", "C", "D"};
      if (pts.size() >= 3) { QPolygonF t1{pts[0], pts[1], pts[2]}; p.setBrush(theme::alpha(blue, 40)); p.setPen(Qt::NoPen); p.drawPolygon(t1); }
      if (pts.size() >= 5) { QPolygonF t2{pts[2], pts[3], pts[4]}; p.drawPolygon(t2); }
      p.setPen(QPen(blue, 1.6)); p.setBrush(Qt::NoBrush);
      p.drawPolyline(QPolygonF(pts));
      p.setFont(tvFont(12, QFont::DemiBold));
      for (int i = 0; i < pts.size() && i < 5; ++i) {
        const QRectF r(pts[i].x() - 9, pts[i].y() + (i % 2 ? -26 : 8), 18, 18);
        p.setPen(Qt::NoPen); p.setBrush(blue); p.drawRoundedRect(r, 3, 3);
        p.setPen(Qt::white); p.drawText(r, Qt::AlignCenter, names[i]);
      }
      break;
    }
    case KPosition: {
      if (d.p.size() < 3 || d.t.size() < 2) break;
      const qreal x0 = pts[0].x(), x1 = xOf(idxOfTime(d.t[1]));
      const qreal ye = yOf(d.p[0]), yt = yOf(d.p[1]), ys = yOf(d.p[2]);
      p.setPen(Qt::NoPen);
      p.fillRect(QRectF(QPointF(x0, yt), QPointF(x1, ye)).normalized(), theme::map(QColor(0x08, 0x99, 0x81, 70)));
      p.fillRect(QRectF(QPointF(x0, ye), QPointF(x1, ys)).normalized(), theme::map(QColor(0xf2, 0x36, 0x45, 70)));
      p.setPen(QPen(theme::map(QColor(0xd1, 0xd4, 0xdc)), 1)); p.drawLine(QPointF(x0, ye), QPointF(x1, ye));
      p.setFont(tvFont(11, QFont::Medium)); p.setPen(theme::map(QColor(Qt::white)));
      p.drawText(QRectF(x0, yt - 18, x1 - x0, 16), Qt::AlignCenter, QCoreApplication::translate("watch", "Target %1 (%2)").arg(priceText(d.p[1]), fmt::pct((d.p[1] / d.p[0] - 1) * 100)));
      p.drawText(QRectF(x0, ys + 2, x1 - x0, 16), Qt::AlignCenter, QCoreApplication::translate("watch", "Stop %1 (%2)").arg(priceText(d.p[2]), fmt::pct((d.p[2] / d.p[0] - 1) * 100)));
      p.drawText(QRectF(x0, ye - 8, x1 - x0, 16), Qt::AlignCenter, QCoreApplication::translate("watch", "Entry %1 · R:R 2.0").arg(priceText(d.p[0])));
      break;
    }
    case KBrush: { QPainterPath b; b.moveTo(pts[0]); for (int i = 1; i < pts.size(); ++i) b.lineTo(pts[i]); p.setPen(QPen(theme::map(QColor(0xf7, 0xa2, 0x1b)), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(b); break; }
    case KText: p.setFont(tvFont(14, QFont::Medium)); p.setPen(blue.lighter(140)); p.drawText(pts[0], d.text); break;
    case KEmoji: { QFont f = p.font(); f.setPixelSize(24); p.setFont(f); p.drawText(QRectF(pts[0].x() - 16, pts[0].y() - 16, 32, 32), Qt::AlignCenter, d.text); break; }
  }
  p.restore();
}

QVector<QPair<QRectF, QString>> CandleChart::navButtons() const {
  QVector<QPair<QRectF, QString>> out;
  if (!m_in || m_bars.isEmpty() || !m_plot.contains(m_mouse) || m_mouse.y() < m_plot.bottom() - 110 || m_drag || m_measuring) return out;
  const qreal cy = m_plot.bottom() - 46, cx = m_plot.center().x();
  const QList<QPair<qreal, QString>> spec{{-69, "nav:out"}, {-33, "nav:in"}, {13, "nav:left"}, {49, "nav:right"}, {94, "nav:reset"}};
  for (const auto& [dx, id] : spec) out.push_back({QRectF(cx + dx - 14, cy - 14, 28, 28), id});
  return out;
}

void CandleChart::contextMenuEvent(QContextMenuEvent* e) {
  if (!m_plot.contains(e->pos()) || m_bars.isEmpty()) return;
  contextMenu(e->globalPos(), priceOf(e->pos().y()));
}
void CandleChart::contextMenu(const QPoint& gp, double price) {
  QMenu m(this);
  auto* hl = m.addAction(QCoreApplication::translate("watch", "Add horizontal line at %1").arg(priceText(price)));
  m.addSeparator();
  auto* hide = m.addAction(m_hideDrawings ? QCoreApplication::translate("watch", "Show drawings") : QCoreApplication::translate("watch", "Hide drawings"));
  auto* clear = m.addAction(QCoreApplication::translate("watch", "Remove all drawings")); clear->setEnabled(!m_drawings.isEmpty());
  m.addSeparator();
  auto* reset = m.addAction(QCoreApplication::translate("watch", "Reset chart view"));
  auto* a = m.exec(gp);
  if (a == hl) { Drawing d; d.kind = KHLine; d.t << 0; d.p << price; m_drawings << d; }
  else if (a == hide) m_hideDrawings = !m_hideDrawings;
  else if (a == clear) m_drawings.clear();
  else if (a == reset) { m_range.clear(); fitInitial(); }
  m_dirty = true; update();
}

void CandleChart::snapshot() {
  m_in = false;
  const QPixmap pm = grab();
  QGuiApplication::clipboard()->setPixmap(pm);
  m_toast = QCoreApplication::translate("watch", "Chart image copied to the clipboard"); m_toastTimer.start(1800); update();
}
}
