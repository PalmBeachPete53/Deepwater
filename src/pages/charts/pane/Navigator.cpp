#include "pages/charts/pane/Navigator.h"
#include "pages/charts/Style.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/TimeScale.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>

namespace charts::ui::pane {

using nav::Part;
using nav::Span;

Navigator::Navigator(plot::Chart* chart, CurveFn curve, QWidget* parent)
    : QWidget(parent), chart_(chart), curve_(std::move(curve)) {
  setObjectName("chartNavigator");
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  // the chart notifies once per change (queued after the change): window moved by a wheel, a drag, a new bar…
  sub_ = chart->timeScale()->subscribeVisibleLogicalRangeChange([this](const std::optional<plot::LogicalRange>&) { update(); });
}

Navigator::~Navigator() {
  if (chart_) chart_->timeScale()->unsubscribeVisibleLogicalRangeChange(sub_);
}

void Navigator::dataChanged(bool lastBarOnly) {
  curveDirty_ = true;
  if (!lastBarOnly) curveFull_ = true;
  update();
}

void Navigator::restyle() {
  curveDirty_ = true;
  curveFull_ = true;
  update();
}

Span Navigator::domain() const {
  if (!chart_) return {};
  return nav::domain(chart_->timeScale()->baseIndex(), rightPad_);
}

nav::ZoomMap Navigator::zoomMap() const {
  nav::ZoomMap m;
  m.px = width();
  m.full = domain().width();
  m.minPx = kMinWindowPx;
  m.linearPx = kLinearPx;
  if (chart_) {   // the chart's deepest zoom: its widest bar spacing (0 = half the plot), as visible bars
    const auto* ts = chart_->timeScale();
    const double w = ts->width(), maxSp = ts->options().maxBarSpacing > 0 ? ts->options().maxBarSpacing : 0.5 * w;
    if (w > 0 && maxSp > 0) m.minBars = std::max(1.0, w / maxSp);
  }
  return m;
}

Span Navigator::display() {
  if (drag_) return dragShown_;
  const Span full = domain();
  const auto v = view();
  disp_ = v ? nav::shownSpan(disp_, full, *v, zoomMap()) : full;
  return *disp_;
}

std::optional<Span> Navigator::view() const {
  if (!chart_ || chart_->timeScale()->isEmpty()) return std::nullopt;
  const auto r = chart_->timeScale()->getVisibleLogicalRange();
  if (!r) return std::nullopt;
  return nav::fromLogical(r->from, r->to);
}

void Navigator::apply(const Span& s) {
  if (!chart_) return;
  double from, to;
  nav::toLogical(s, from, to);
  chart_->timeScale()->setVisibleLogicalRange({from, to});
  update();   // the chart's own notification comes one event later: draw the new window now
}

// ---------- mini curve: closes reduced to one first / low / high / last per device-pixel column ----------
void Navigator::rebuildCurve(bool force, const Span& dom) {
  curveDirty_ = curveFull_ = false;
  curveRev_ = style::revision();
  curveDom_ = dom;
  const double dpr = devicePixelRatioF();
  const int W = std::max(1, int(std::ceil(width() * dpr))), H = std::max(1, int(std::ceil(height() * dpr)));
  QVector<double> xs, cs;
  if (curve_) curve_(xs, cs);
  {   // only the bars of the span shown (zoomed in, a part of the history): its own price range
    int m = 0;
    for (int k = 0; k < std::min(int(xs.size()), int(cs.size())); ++k)
      if (xs[k] >= dom.a && xs[k] <= dom.b) { xs[m] = xs[k]; cs[m] = cs[k]; ++m; }
    xs.resize(m);
    cs.resize(m);
  }
  const int n = std::min(int(xs.size()), int(cs.size()));
  double lo = std::numeric_limits<double>::infinity(), hi = -lo;
  for (int k = 0; k < n; ++k)
    if (std::isfinite(cs[k])) { lo = std::min(lo, cs[k]); hi = std::max(hi, cs[k]); }
  if (std::isfinite(lo) && hi - lo < 1e-12) { lo -= 1; hi += 1; }
  const double top = 5 * dpr, bottom = H - 3 * dpr;
  auto yOf = [&](double c) { return bottom - (c - lo) / (hi - lo) * (bottom - top); };
  // A live tick (dataChanged(true)) only moves the last close or adds bars: with the same bars, range and domain, the picture changes only if that
  // close lands on another device pixel. Redrawing the whole curve (pixmap, path, fill) per tick is skipped otherwise.
  const int lastY = n > 0 && std::isfinite(lo) && std::isfinite(cs[n - 1]) ? int(std::lround(yOf(cs[n - 1]))) : INT_MIN;
  const double lastLogical = n > 0 ? xs[n - 1] : 0;
  if (!force && n == curveN_ && lo == curveLo_ && hi == curveHi_ && lastY == curveLastY_ && lastLogical == curveLastX_) return;
  curveN_ = n;
  curveLo_ = lo;
  curveHi_ = hi;
  curveLastY_ = lastY;
  curveLastX_ = lastLogical;
  curvePix_ = QPixmap(W, H);
  curvePix_.setDevicePixelRatio(dpr);
  curvePix_.fill(Qt::transparent);
  if (n < 1 || !(dom.width() > 0)) return;
  if (!std::isfinite(lo)) return;

  struct Col { double first, low, high, last; bool any = false; };
  QVector<Col> cols(W);
  for (int k = 0; k < n; ++k) {
    if (!std::isfinite(cs[k])) continue;
    const int c = std::clamp(int(std::floor(nav::toPx(xs[k], dom, W))), 0, W - 1);
    Col& col = cols[c];
    if (!col.any) col = {cs[k], cs[k], cs[k], cs[k], true};
    else { col.low = std::min(col.low, cs[k]); col.high = std::max(col.high, cs[k]); col.last = cs[k]; }
  }
  QPainterPath line;
  int firstX = -1, lastX = -1;
  for (int c = 0; c < W; ++c) {
    const Col& col = cols[c];
    if (!col.any) continue;
    const double x = c + 0.5;
    if (firstX < 0) { line.moveTo(x, yOf(col.first)); firstX = c; }
    else line.lineTo(x, yOf(col.first));
    if (col.low != col.high) {
      // order the extremes as they came, so the stroke goes up then down (or down then up) like the data
      const bool upFirst = col.first <= col.last;
      line.lineTo(x, yOf(upFirst ? col.low : col.high));
      line.lineTo(x, yOf(upFirst ? col.high : col.low));
    }
    line.lineTo(x, yOf(col.last));
    lastX = c;
  }
  const auto& v = style::v();
  QPainter p(&curvePix_);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath area = line;
  area.lineTo(lastX + 0.5, H);
  area.lineTo(firstX + 0.5, H);
  area.closeSubpath();
  QColor fill = v.accent;
  fill.setAlphaF(v.light ? 0.10 : 0.14);
  p.fillPath(area, fill);
  QColor stroke = v.accent;
  stroke.setAlphaF(0.85);
  p.strokePath(line, QPen(stroke, 1.0 * dpr));
}

// ---------- painting ----------
void Navigator::paintEvent(QPaintEvent*) {
  const auto& v = style::v();
  QPainter p(this);
  const QRectF r = rect();
  p.fillRect(r, v.bg);
  p.fillRect(QRectF(0, 0, width(), 1), v.border);
  const Span all = domain();
  const Span dom = display();
  // a new theme, span or size redraws in any case; new data only when it shows (see rebuildCurve)
  const bool force = curveFull_ || curveRev_ != style::revision() || !(curveDom_ == dom) ||
                     curvePix_.size() != QSize(int(std::ceil(width() * devicePixelRatioF())), int(std::ceil(height() * devicePixelRatioF())));
  if (force || curveDirty_) rebuildCurve(force, dom);
  p.drawPixmap(0, 0, curvePix_);
  // more history beyond a side (the navigator zoomed in): a fade and a small chevron on that side
  {
    QColor fade = v.backdrop;
    fade.setAlphaF(v.light ? 0.18 : 0.45);
    QColor clear = fade;
    clear.setAlphaF(0);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QColor mark = v.muted;
    auto side = [&](bool left) {
      const double w = 14, x0 = left ? 0 : width() - w;
      QLinearGradient g(QPointF(left ? 0 : width(), 0), QPointF(left ? w : width() - w, 0));
      g.setColorAt(0, fade);
      g.setColorAt(1, clear);
      p.fillRect(QRectF(x0, 1, w, height() - 1), g);
      const double cx = left ? 5 : width() - 5, cy = height() / 2.0, d = left ? 1 : -1;
      p.setPen(QPen(mark, 1.3));
      p.drawLine(QPointF(cx + 2 * d, cy - 4), QPointF(cx - 1.5 * d, cy));
      p.drawLine(QPointF(cx - 1.5 * d, cy), QPointF(cx + 2 * d, cy + 4));
    };
    if (dom.a > all.a + 1e-9) side(true);
    if (dom.b < all.b - 1e-9) side(false);
    p.restore();
  }
  const auto full = view();
  const auto win = full ? nav::clip(*full, dom) : std::nullopt;
  if (!win) return;
  const double W = width();
  const double xa = std::round(nav::toPx(win->a, dom, W)), xb = std::round(nav::toPx(win->b, dom, W));
  // outside the window: slightly darker
  QColor shade = v.backdrop;
  shade.setAlphaF(v.light ? 0.10 : 0.32);
  if (xa > 0) p.fillRect(QRectF(0, 1, xa, height() - 1), shade);
  if (xb < W) p.fillRect(QRectF(xb, 1, W - xb, height() - 1), shade);
  // the window: a light accent wash and an outline
  const bool active = drag_ || hover_ == Part::Inside;
  QColor wash = v.accent;
  wash.setAlphaF(active ? 0.12 : 0.07);
  p.fillRect(QRectF(xa, 1, std::max(1.0, xb - xa), height() - 1), wash);
  QColor edge = v.accent;
  edge.setAlphaF(active ? 0.9 : 0.6);
  p.fillRect(QRectF(xa, 1, std::max(1.0, xb - xa), 1), edge);
  p.fillRect(QRectF(xa, height() - 1, std::max(1.0, xb - xa), 1), edge);
  p.fillRect(QRectF(std::min(xa, W - 1), 1, 1, height() - 1), edge);
  p.fillRect(QRectF(std::max(0.0, xb - 1), 1, 1, height() - 1), edge);
  // handles: small rounded grips centred on the edges (kept inside the widget at the ends of the domain)
  p.setRenderHint(QPainter::Antialiasing);
  const double hw = 7, hh = std::min(18.0, height() - 10.0);
  auto handle = [&](double x, Part part) {
    const bool on = (drag_ && drag_->part() == part) || (!drag_ && hover_ == part);
    const double cx = std::clamp(x, hw / 2 + 1, W - hw / 2 - 1);
    const QRectF h(cx - hw / 2, (height() - hh) / 2, hw, hh);
    p.setPen(QPen(on ? v.accent : v.muted, 1));
    p.setBrush(on ? v.accent : v.panel2);
    p.drawRoundedRect(h.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p.setPen(QPen(on ? v.onAccent : v.muted, 1));
    for (double dy : {-3.0, 0.0, 3.0}) p.drawLine(QPointF(cx - 1.5, h.center().y() + dy), QPointF(cx + 1.5, h.center().y() + dy));
  };
  handle(xa, Part::Left);
  handle(xb, Part::Right);
}

void Navigator::resizeEvent(QResizeEvent* e) {
  QWidget::resizeEvent(e);
  curveDirty_ = true;
}

// ---------- mouse (Qt grabs the mouse for the pressed widget: a drag goes on outside the navigator) ----------
void Navigator::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);
  const Span dom = display();
  const auto full = view();
  const auto win = full ? nav::clip(*full, dom) : std::nullopt;
  if (!win) return;
  const double x = e->position().x();
  const Part part = nav::hit(*win, dom, width(), x);
  if (part == Part::None) return;
  drag_.emplace(part, *full, dom, domain(), zoomMap(), x);
  const nav::Gesture::Step st = drag_->atPress();
  dragShown_ = st.shown;
  if (part == Part::Outside || !(st.view == *full)) apply(st.view);
  updateCursor();
  update();
  e->accept();
}

void Navigator::mouseMoveEvent(QMouseEvent* e) {
  const double x = e->position().x();
  if (drag_) {
    const nav::Gesture::Step st = drag_->move(x);
    dragShown_ = st.shown;
    apply(st.view);
    return;
  }
  const Span dom = display();
  const auto full = view();
  const auto win = full ? nav::clip(*full, dom) : std::nullopt;
  setHover(win ? nav::hit(*win, dom, width(), x) : Part::None);
}

void Navigator::mouseReleaseEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton || !drag_) return QWidget::mouseReleaseEvent(e);
  disp_ = dragShown_;   // kept if it still fits the view (shownSpan), else placed inside the data again
  drag_.reset();
  const Span dom = display();   // recomputed now the gesture is over (it may have moved the view near an edge)
  const auto full = view();
  const auto win = full ? nav::clip(*full, dom) : std::nullopt;
  hover_ = Part::None;
  setHover(win && rect().contains(e->position().toPoint()) ? nav::hit(*win, dom, width(), e->position().x()) : Part::None);
  updateCursor();
  update();
}

void Navigator::leaveEvent(QEvent* e) {
  QWidget::leaveEvent(e);
  if (!drag_) setHover(Part::None);
}

void Navigator::setHover(Part part) {
  if (part == hover_) return;
  hover_ = part;
  updateCursor();
  update();
}

void Navigator::updateCursor() {
  const Part p = drag_ ? drag_->part() : hover_;
  switch (p) {
    case Part::Left:
    case Part::Right: setCursor(Qt::SizeHorCursor); break;
    case Part::Inside: setCursor(drag_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor); break;
    case Part::Outside: setCursor(Qt::PointingHandCursor); break;
    default: unsetCursor(); break;
  }
}

}  // namespace charts::ui::pane
