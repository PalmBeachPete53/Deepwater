#include "pages/global/overview/MarketTape.h"
#include <QResizeEvent>
#include <QCoreApplication>
#include "global/Look.h"
#include "global/Sessions.h"
#include "global/FrameClock.h"
#include "market/QuoteHub.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

namespace look = global::look;

namespace overview {

// display label, app symbol, decimals
struct TapeItem { const char* label; const char* symbol; int dp; };
static const TapeItem kItems[] = {
  {"EURUSD", "EURUSD", 4}, {"GBPUSD", "GBPUSD", 4}, {"USDJPY", "USDJPY", 2}, {"XAUUSD", "XAUUSD", 2},
  {"WTI", "WTI", 2}, {"S&P 500", "SPX", 1}, {"NASDAQ", "NDX", 1}, {"DOW", "DOW", 1},
};
static const qreal kSpeed = 37.0;    // px per second (reference: 222 px/s in a 6x time-lapse)

QStringList MarketTape::symbols() {
  QStringList s;
  for (const auto& i : kItems) s << i.symbol;
  return s;
}

MarketTape::MarketTape(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  auto& hub = market::QuoteHub::instance();
  hub.subscribe(this, symbols(), false, true);   // urgent: eight quotes along the top, the first thing seen
  connect(&hub, &market::QuoteHub::updated, this, [this](const QStringList& changed) {
    for (const auto& i : kItems) if (changed.contains(i.symbol)) { rebuildStrip(); update(); return; }
  });
  connect(&global::FrameClock::instance(), &global::FrameClock::tick, this, [this](int frame) {
    Q_UNUSED(frame);
    if (!m_running) return;
    const qint64 ms = m_since.elapsed();
    m_offset += (ms - m_lastMs) / 1000.0 * kSpeed;
    m_lastMs = ms;
    if (m_stripW > 0) m_offset = std::fmod(m_offset, m_stripW);
    update(scrollRect().toAlignedRect());
  });
  m_slow.setInterval(1000);   // session clocks
  connect(&m_slow, &QTimer::timeout, this, [this] {
    QString key;
    for (const auto& s : global::sessions()) key += global::isOpen(s, global::nowUtc()) ? '1' : '0';
    if (key != m_sessionKey) { m_sessionKey = key; update(QRectF(clocksLeft(), 0, width() - clocksLeft(), height()).toAlignedRect()); }
  });
  m_since.start();
  rebuildStrip();
}

void MarketTape::showEvent(QShowEvent*) {
  market::QuoteHub::instance().setActive(this, true);
  m_lastMs = m_since.elapsed();
  m_running = true; global::FrameClock::instance().acquire(this); m_slow.start();
}
void MarketTape::hideEvent(QHideEvent*) {
  market::QuoteHub::instance().setActive(this, false);
  m_running = false; global::FrameClock::instance().release(this); m_slow.stop();
}
// the strip is drawn at the widget's height (built first in the constructor, at the default height): a new height
// rebuilds it, or the items stay off-centre vertically
void MarketTape::resizeEvent(QResizeEvent* e) {
  if (e->size().height() != e->oldSize().height()) rebuildStrip();
  update();
}

qreal MarketTape::clocksLeft() const { return width() - 277; }
QRectF MarketTape::scrollRect() const { return QRectF(64, 1, clocksLeft() - 64 - 8, height() - 2); }

void MarketTape::rebuildStrip() {
  const QFont fs = theme::mono(13, QFont::DemiBold), fp = theme::mono(13, QFont::Bold), fc = theme::mono(13);
  struct Part { QString sym, price, chg; QColor chgC; };
  QList<Part> parts;
  auto& hub = market::QuoteHub::instance();
  for (const auto& i : kItems) {
    const auto q = hub.quote(i.symbol);
    Part p{i.label, fmt::dash(), QString(), theme::map("#8a877f")};
    if (q.valid()) {
      p.price = fmt::num(q.price, i.dp);
      if (std::isfinite(q.changePct)) {
        p.chg = fmt::pct(q.changePct, 2);
        p.chgC = fmt::num(std::abs(q.changePct), 2) == "0.00" ? theme::map("#a9a7a0") : q.changePct > 0 ? theme::map("#5fbf7f") : theme::map("#e0666b");
      }
    }
    parts << p;
  }
  const qreal sep = 14;
  qreal w = 0;
  QList<qreal> widths;
  for (const auto& p : parts) {
    qreal x = look::width(p.sym, fs) + 7 + look::width(p.price, fp);
    if (!p.chg.isEmpty()) x += 7 + look::width(p.chg, fc);
    widths << x;
    w += x + 2 * sep + 1;
  }
  m_stripW = std::ceil(w);
  const qreal dpr = 2.0;
  m_strip = QPixmap(QSize(int(m_stripW * dpr), int(height() * dpr)));
  m_strip.setDevicePixelRatio(dpr);
  m_strip.fill(Qt::transparent);
  QPainter p(&m_strip);
  p.setRenderHint(QPainter::Antialiasing);
  const qreal base = height() / 2.0 + 5;
  qreal x = sep;
  for (int k = 0; k < parts.size(); ++k) {
    const auto& it = parts[k];
    qreal xx = x;
    xx += look::text(p, xx, base, it.sym, fs, theme::map("#a8a49c")) + 7;
    xx += look::text(p, xx, base, it.price, fp, theme::map("#f7f4f1"));
    if (!it.chg.isEmpty()) look::text(p, xx + 7, base, it.chg, fc, it.chgC);
    x += widths[k] + sep;
    p.fillRect(QRectF(x, height() / 2.0 - 6, 1, 12), theme::map("#3a3530"));
    x += 1 + sep;
  }
}

void MarketTape::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  p.setClipRegion(e->region());
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF R(rect());
  QLinearGradient g(0, 0, width(), 0);
  g.setColorAt(0, theme::map("#201804")); g.setColorAt(1, theme::map("#151602"));
  // a scroll frame only touches the inside of the bar: no frame, no rounded corners to redo
  const bool frameOnly = scrollRect().toAlignedRect().contains(e->region().boundingRect());
  if (!frameOnly) {
    p.fillRect(R, theme::map("#080502"));            // page colour behind the rounded corners (opaque widget)
    p.setPen(QPen(theme::map("#464230"), 1));
    p.setBrush(g);
    p.drawRoundedRect(R.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  } else {
    // the bar's gradient under the scrolling quotes, rendered once per size (it was a gradient fill every frame)
    const QRect sa = scrollRect().toAlignedRect();
    const qreal dpr = devicePixelRatioF();
    if (m_scrollBg.size() != sa.size() * dpr || m_scrollBg.devicePixelRatio() != dpr) {
      m_scrollBg = QPixmap(sa.size() * dpr);
      m_scrollBg.setDevicePixelRatio(dpr);
      QPainter b(&m_scrollBg);
      b.setRenderHint(QPainter::Antialiasing);
      b.translate(-sa.topLeft());
      b.fillRect(scrollRect(), g);
    }
    p.drawPixmap(sa.topLeft(), m_scrollBg);
  }

  // scrolling quotes with faded ends
  const QRectF sr = scrollRect();
  if (e->region().intersects(sr.toAlignedRect())) {
    p.save();
    p.setClipRect(sr, Qt::IntersectClip);
    if (m_stripW > 0) {
      const qreal dpr = devicePixelRatioF();
      // snap to device pixels so text stays crisp while it moves
      qreal x = sr.left() - std::round(m_offset * dpr) / dpr;
      while (x < sr.right()) { p.drawPixmap(QPointF(x, 0), m_strip); x += m_stripW; }
    }
    const auto fade = [&](qreal x0, qreal x1, bool leftSide) {
      QLinearGradient f(x0, 0, x1, 0);
      const qreal t = qBound(0.0, x0 / width(), 1.0);
      const QColor bg = theme::mix(theme::map("#201804"), theme::map("#151602"), t);
      f.setColorAt(leftSide ? 0 : 1, bg);
      f.setColorAt(leftSide ? 1 : 0, theme::alpha(bg, 0));
      p.fillRect(QRectF(x0, 1, x1 - x0, height() - 2), f);
    };
    fade(sr.left(), sr.left() + 20, true);
    fade(sr.right() - 22, sr.right(), false);
    p.restore();
  }

  if (!e->region().intersects(QRect(0, 0, 64, height())) && !e->region().intersects(QRectF(clocksLeft(), 0, width(), height()).toAlignedRect())) return;
  // LIVE pill (static)
  const QRectF lp(7, 6, 50, 21);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#fcd700"));
  p.drawRoundedRect(lp, 3, 3);
  p.setBrush(theme::onAccent());
  p.drawEllipse(QPointF(lp.left() + 9.5, lp.center().y()), 2.6, 2.6);
  look::text(p, lp.left() + 16, lp.center().y() + 4.5, QCoreApplication::translate("global", "LIVE"), theme::mono(12, QFont::Bold), theme::onAccent());

  // session clocks: New York, London, Tokyo, Sydney
  const auto& ss = global::sessions();
  const int order[4] = {3, 2, 1, 0};
  const qreal xs[4] = {0, 78, 143, 207};
  const QDateTime utc = global::nowUtc();
  for (int k = 0; k < 4; ++k) {
    const auto& s = ss[order[k]];
    const bool open = global::isOpen(s, utc);
    const qreal x = clocksLeft() + 9 + xs[k];
    p.setPen(Qt::NoPen);
    if (open) { p.setBrush(theme::alpha(global::sessionColour(s, global::Tone::Bright), 70)); p.drawEllipse(QPointF(x + 3, height() / 2.0), 4.5, 4.5); }
    p.setBrush(open ? global::sessionColour(s, global::Tone::Bright) : theme::map("#7b7767"));
    p.drawEllipse(QPointF(x + 3, height() / 2.0), 2.4, 2.4);
    look::text(p, x + 11, 14, global::sessionName(s), theme::mono(12, QFont::Medium), theme::map("#ebe7d8"));
    look::text(p, x + 11, 28, open ? QCoreApplication::translate("global", "Open") : QCoreApplication::translate("global", "Closed"), theme::mono(12), open ? theme::mix(global::sessionColour(s, global::Tone::Bright), theme::map("#ffffff"), 0.2) : theme::map("#5f5a49"));
  }
}
}  // namespace overview
