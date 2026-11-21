#pragma once
// Rules of the chart navigator (the range slider under a chart pane), kept free of widgets so they can be tested
// alone (tests/charts/core/range_window_tests.cpp). The widget is src/pages/charts/pane/Navigator.
//
// Units: bar EDGES on the chart's logical scale. Bar i covers [i - 0.5, i + 0.5]; a chart logical range {from, to}
// (bar centres, TimeScale::getVisibleLogicalRange) is the span [from - 0.5, to + 0.5]. The domain is everything
// loaded: [-0.5, lastIndex + 0.5 + rightPad], rightPad = the empty slots a fresh chart view keeps after the last bar.
//
// The chart's visible range stays the only source of truth: the navigator reads it, a gesture computes a new span
// from the span at press time, and the widget writes it back to the chart.
#include <optional>

namespace charts::nav {

struct Span {
  double a = 0, b = 0;   // left and right edge
  double width() const { return b - a; }
  bool operator==(const Span&) const = default;
};

Span fromLogical(double from, double to);        // chart logical range (bar centres) → edges
void toLogical(const Span& s, double& from, double& to);
Span domain(int lastIndex, double rightPad);     // lastIndex < 0 → empty domain {0, 0}

// Same width, moved inside the domain; a span at least as wide as the domain becomes the domain (nothing to pan).
Span clampPan(const Span& s, const Span& dom);
// Part of `s` inside the domain (what the navigator draws); nullopt when they don't overlap.
std::optional<Span> clip(const Span& s, const Span& dom);

// The navigator's zoom map (user's rules, 2026-11-19 and 2026-11-20): the mini curve always shows the whole domain,
// only the window changes width, and its width in pixels stands for a zoom level: minPx (30) is always the chart's
// deepest zoom (minBars visible, 5 bars); from linearPx up the window is the plain map (view / domain × width); between
// them its width is geometric in the view's width. Below linearPx the window is wider than the bars it stands for on
// the curve: it is placed by the view's position in the data (windowOf), the plain map's place when it is exact.
struct ZoomMap {
  double px = 0;          // navigator width
  double full = 0;        // width of the whole domain, in bars
  double minBars = 5;     // the chart's deepest zoom (visible bars)
  double minPx = 30;      // the window's width at that zoom
  double linearPx = 120;  // from this window width up, the window is the plain map
  double windowPx(double bars) const;     // the window's width for a view of `bars`
  double barsFor(double windowPx) const;  // the view's width for a window that wide (minBars below minPx)
  double stopPx() const { return windowPx(minBars); }   // the narrowest window (minPx, more with a short history)
};

// The window in navigator pixels: x = left edge, w = width.
struct Win {
  double x = 0, w = 0;
  double right() const { return x + w; }
};
// Where the window of `view` goes: windowPx wide, its left edge at the same fraction of the free room (px - w) as the
// view's left edge in the free room of the domain (full - bars). With the plain map that is the view's own place.
Win windowOf(const Span& view, const Span& full, const ZoomMap& m);
// The view a window stands for: barsFor(w) bars (or `bars` when given: a pan keeps the zoom), placed by the same rule.
Span viewOf(double x, double w, const Span& full, const ZoomMap& m, std::optional<double> bars = std::nullopt);

// Pixels of a widget `px` wide showing the whole domain.
double toPx(double u, const Span& dom, double px);
double toUnits(double x, const Span& dom, double px);

enum class Part { None, Left, Right, Inside, Outside };
// What a press at x grabs on a window drawn from xa to xb (pixels). Handles are caught ±grip px around each edge; on a
// window narrower than three grips the handles shrink on the inside so its middle third stays grabbable for panning.
Part hit(double xa, double xb, double x, double grip = 8);

// One press-drag-release on the navigator, from the view at the press.
//   Left / Right: the other edge of the window stays on its pixel, the grabbed one follows the pointer, stopped at the
//     widget's side and at stopPx (the chart's deepest zoom, in one gesture whatever the zoom before).
//   Inside: pans (same zoom, the window follows the pointer).
//   Outside: centres the window on the press point, then pans like Inside while the button stays down.
class Gesture {
public:
  Gesture(Part part, const Span& view, const Span& full, const ZoomMap& m, double x);
  Span move(double x) const;
  Span atPress() const { return view_; }   // an Outside press already moved the view
  Part part() const { return part_; }
private:
  Part part_;
  Span view_, full_;
  ZoomMap m_;
  Win win_;
  double x0_ = 0, grab_ = 0;
};

}  // namespace charts::nav
