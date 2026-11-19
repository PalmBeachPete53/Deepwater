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

double ZoomMap::shownWidth(double bars) const {
  const double w = windowPx(bars);
  return w > 0 ? std::min(full, std::clamp(bars, std::min(minBars, full), full) * px / w) : full;
}

Span shownSpan(const std::optional<Span>& prev, const Span& full, const Span& view, const ZoomMap& m) {
  constexpr double eps = 1e-9;
  const auto vc = clip(view, full);   // a view running into the empty slots past the data counts as what it covers
  if (!vc || !(full.width() > 0)) return full;
  const double S = m.shownWidth(vc->width());
  if (S >= full.width() - eps) return full;
  if (prev && std::fabs(prev->width() - S) <= 1e-6 * std::max(1.0, S) && prev->a >= full.a - eps && prev->b <= full.b + eps) {
    const Span& d = *prev;
    const double margin = 0.05 * d.width();
    const bool leftOk = d.a <= full.a + eps || vc->a >= d.a + margin;   // no more data to the left, or room before it
    const bool rightOk = d.b >= full.b - eps || vc->b <= d.b - margin;
    if (vc->a >= d.a - eps && vc->b <= d.b + eps && leftOk && rightOk) return d;
  }
  const double c = (vc->a + vc->b) / 2;
  return clampPan({c - S / 2, c + S / 2}, full);
}

double toPx(double u, const Span& dom, double px) {
  const double w = dom.width();
  return w > 0 ? (u - dom.a) / w * px : 0;
}

double toUnits(double x, const Span& dom, double px) {
  return px > 0 ? dom.a + x / px * dom.width() : dom.a;
}

Part hit(const Span& win, const Span& dom, double px, double x, double grip) {
  if (!(dom.width() > 0) || !(px > 0)) return Part::None;
  const double xa = toPx(win.a, dom, px), xb = toPx(win.b, dom, px);
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

Gesture::Gesture(Part part, const Span& view, const Span& shown, const Span& full, const ZoomMap& m, double x)
    : part_(part), view_(clampPan(view, full)), shown_(shown), full_(full), m_(m), x0_(x) {
  switch (part_) {
    case Part::Outside: {
      const double c = toUnits(x, shown_, m_.px), w = view_.width();
      view_ = clampPan({c - w / 2, c + w / 2}, full_);
      part_ = Part::Inside;   // keep dragging without releasing
      break;
    }
    case Part::Left:   // the right edge stays put; the pointer holds the left one where it grabbed it
      anchor_ = view_.b;
      anchorPx_ = toPx(view_.b, shown_, m_.px);
      grab_ = x - toPx(view_.a, shown_, m_.px);
      break;
    case Part::Right:
      anchor_ = view_.a;
      anchorPx_ = toPx(view_.a, shown_, m_.px);
      grab_ = x - toPx(view_.b, shown_, m_.px);
      break;
    default: break;
  }
}

Gesture::Step Gesture::zoomTo(double windowPx) const {
  // the grabbed edge stops at the widget's side (the pointer may go on past it), and the window at minPx
  const double room = part_ == Part::Right ? m_.px - anchorPx_ : anchorPx_;
  const double w = std::max(std::min(windowPx, room), m_.minPx);
  // The span shown is placed so the anchored edge keeps its pixel, and may run past the data while the button is
  // down (an empty end): kept inside, the anchored edge and the grabbed one would slide away from the pointer. The
  // widget places it inside the data again at the release (shownSpan).
  if (part_ == Part::Right) {
    const double bars = std::min(m_.barsFor(w), full_.b - anchor_);
    const double S = m_.shownWidth(bars);
    const double a = anchor_ - anchorPx_ * S / m_.px;
    return {{anchor_, anchor_ + bars}, {a, a + S}};
  }
  const double bars = std::min(m_.barsFor(w), anchor_ - full_.a);
  const double S = m_.shownWidth(bars);
  const double b = anchor_ + (m_.px - anchorPx_) * S / m_.px;
  return {{anchor_ - bars, anchor_}, {b - S, b}};
}

Gesture::Step Gesture::move(double x) const {
  switch (part_) {
    case Part::Inside: {
      const double d = m_.px > 0 ? (x - x0_) * shown_.width() / m_.px : 0;
      const Span v = clampPan({view_.a + d, view_.b + d}, full_);
      Span s = shown_;   // the span shown follows a view that reaches its edge
      if (v.a < s.a) s = {v.a, v.a + s.width()};
      if (v.b > s.b) s = {v.b - s.width(), v.b};
      return {v, clampPan(s, full_)};
    }
    case Part::Right: return zoomTo(x - grab_ - anchorPx_);
    case Part::Left: return zoomTo(anchorPx_ - (x - grab_));
    default: return {view_, shown_};
  }
}

}  // namespace charts::nav
