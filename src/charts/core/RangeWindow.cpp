#include "charts/core/RangeWindow.h"
#include <algorithm>
#include <cmath>

namespace charts::nav {

Span fromLogical(double from, double to) { return {from - 0.5, to + 0.5}; }

void toLogical(const Span& s, double& from, double& to) {
  from = s.a + 0.5;
  to = s.b - 0.5;
}

Span domain(int lastIndex, double rightPad) {
  if (lastIndex < 0) return {0, 0};
  return {-0.5, lastIndex + 0.5 + std::max(0.0, rightPad)};
}

Span clampPan(const Span& s, const Span& dom) {
  const double w = s.width();
  if (w >= dom.width()) return dom;
  if (s.a < dom.a) return {dom.a, dom.a + w};
  if (s.b > dom.b) return {dom.b - w, dom.b};
  return s;
}

std::optional<Span> clip(const Span& s, const Span& dom) {
  const Span c{std::max(s.a, dom.a), std::min(s.b, dom.b)};
  if (!(c.b > c.a)) return std::nullopt;
  return c;
}

// ---- zoom map ----
namespace {
double linearBars(const ZoomMap& m) { return m.px > 0 ? m.linearPx * m.full / m.px : 0; }
// The narrowing only where the plain map draws the deepest zoom under minPx; with a short history the plain map already
// gives it more room (and a narrowed span would be wider than the data, the window no longer exact).
bool logRegime(const ZoomMap& m) {
  return m.px > 0 && m.full > 0 && m.linearPx > m.minPx && linearBars(m) > m.minBars && m.minBars / m.full * m.px < m.minPx;
}
}

double ZoomMap::windowPx(double bars) const {
  if (!(px > 0) || !(full > 0)) return 0;
  const double b = std::clamp(bars, std::min(minBars, full), full);
  const double lin = linearBars(*this);
  if (!logRegime(*this) || b >= lin) return b / full * px;
  const double t = std::log(b / minBars) / std::log(lin / minBars);
  return minPx * std::pow(linearPx / minPx, t);
}

double ZoomMap::barsFor(double w) const {
  if (!(px > 0) || !(full > 0)) return full;
  const double lo = std::min(minBars, full);
  if (!logRegime(*this) || w >= linearPx) return std::clamp(w * full / px, lo, full);
  const double t = std::log(std::max(w, minPx) / minPx) / std::log(linearPx / minPx);
  return std::clamp(minBars * std::pow(linearBars(*this) / minBars, t), lo, full);
}

Win windowOf(const Span& view, const Span& full, const ZoomMap& m) {
  const auto vc = clip(view, full);   // a view running into the empty slots past the data counts as what it covers
  if (!vc || !(m.px > 0) || !(full.width() > 0)) return {0, m.px};
  const double w = std::min(m.px, m.windowPx(vc->width()));
  const double free = full.width() - vc->width();
  const double f = free > 1e-9 ? std::clamp((vc->a - full.a) / free, 0.0, 1.0) : 0.0;
  return {f * (m.px - w), w};
}

Span viewOf(double x, double w, const Span& full, const ZoomMap& m, std::optional<double> bars) {
  const double n = std::clamp(bars ? *bars : m.barsFor(w), std::min(m.minBars, full.width()), full.width());
  const double room = m.px - w;
  const double f = room > 1e-9 ? std::clamp(x / room, 0.0, 1.0) : 0.0;
  const double a = full.a + f * (full.width() - n);
  return {a, a + n};
}

double toPx(double u, const Span& dom, double px) {
  const double w = dom.width();
  return w > 0 ? (u - dom.a) / w * px : 0;
}

double toUnits(double x, const Span& dom, double px) {
  return px > 0 ? dom.a + x / px * dom.width() : dom.a;
}

Part hit(double xa, double xb, double x, double grip) {
  const double inner = std::min(grip, std::max(0.0, xb - xa) / 3);
  const bool onLeft = x >= xa - grip && x <= xa + inner;
  const bool onRight = x <= xb + grip && x >= xb - inner;
  if (onLeft && onRight) {   // only on a window of zero width: the side of the press decides
    if (x < xa) return Part::Left;
    if (x > xb) return Part::Right;
    return std::fabs(x - xa) <= std::fabs(x - xb) ? Part::Left : Part::Right;
  }
  if (onLeft) return Part::Left;
  if (onRight) return Part::Right;
  if (x > xa && x < xb) return Part::Inside;
  return Part::Outside;
}

Gesture::Gesture(Part part, const Span& view, const Span& full, const ZoomMap& m, double x)
    : part_(part), view_(clampPan(view, full)), full_(full), m_(m), x0_(x) {
  win_ = windowOf(view_, full_, m_);
  switch (part_) {
    case Part::Outside:
      win_.x = std::clamp(x - win_.w / 2, 0.0, std::max(0.0, m_.px - win_.w));
      view_ = viewOf(win_.x, win_.w, full_, m_, view_.width());
      part_ = Part::Inside;   // keep dragging without releasing
      break;
    case Part::Left: grab_ = x - win_.x; break;
    case Part::Right: grab_ = x - win_.right(); break;
    default: break;
  }
}

Span Gesture::move(double x) const {
  const double stop = std::min(m_.stopPx(), m_.px);
  switch (part_) {
    case Part::Inside: {
      const double l = std::clamp(win_.x + (x - x0_), 0.0, std::max(0.0, m_.px - win_.w));
      return viewOf(l, win_.w, full_, m_, view_.width());
    }
    case Part::Right: {   // the left edge stays on its pixel
      const double r = std::clamp(x - grab_, std::min(m_.px, win_.x + stop), m_.px);
      return viewOf(win_.x, r - win_.x, full_, m_);
    }
    case Part::Left: {
      const double l = std::clamp(x - grab_, 0.0, std::max(0.0, win_.right() - stop));
      return viewOf(l, win_.right() - l, full_, m_);
    }
    default: return view_;
  }
}

}  // namespace charts::nav
