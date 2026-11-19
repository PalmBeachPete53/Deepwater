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

// The navigator's zoom map (user's rule, 2026-11-19): the window's width in pixels stands for a zoom level, whatever
// was done before. minPx (30) is always the chart's deepest zoom (minBars visible); from linearPx up the navigator is the
// plain map of the whole domain (window = view / domain × width); between them the span shown narrows step by step
// (window width geometric in the view's width). The window stays exact: it is the view inside the span shown.
struct ZoomMap {
  double px = 0;          // navigator width
  double full = 0;        // width of the whole domain, in bars
  double minBars = 2;     // the chart's deepest zoom (visible bars)
  double minPx = 30;      // the window's width at that zoom
  double linearPx = 120;  // from this window width up, the navigator shows the whole domain
  double windowPx(double bars) const;     // the window's width for a view of `bars`
  double barsFor(double windowPx) const;  // the view's width for a window that wide (minBars below minPx)
  double shownWidth(double bars) const;   // the width of the span shown for a view of `bars`
};
// The span shown for `view`: shownWidth wide, kept where it was (prev) while it has that width and holds the view away
// from an edge with more data beyond; otherwise centred on the view, inside the domain.
Span shownSpan(const std::optional<Span>& prev, const Span& full, const Span& view, const ZoomMap& m);

// Pixels of a widget `px` wide showing the whole domain.
double toPx(double u, const Span& dom, double px);
double toUnits(double x, const Span& dom, double px);

enum class Part { None, Left, Right, Inside, Outside };
// What a press at x grabs. Handles are caught ±grip px around each edge; on a window narrower than three grips the
// handles shrink on the inside so its middle third stays grabbable for panning.
Part hit(const Span& win, const Span& dom, double px, double x, double grip = 8);

// One press-drag-release on the navigator, from the view and the span shown at the press.
//   Left / Right: the other edge stays where it was, at the same place on screen; the grabbed edge follows the pointer
//     and the window's width gives the zoom (ZoomMap): at minPx the chart's deepest zoom, whatever the span before.
//     The span shown may run past the data during the drag (to keep both edges where they are on screen).
//   Inside: pans (same width); the span shown follows when the view reaches its edge.
//   Outside: centres the window on the press point, then pans like Inside while the button stays down.
class Gesture {
public:
  struct Step { Span view, shown; };
  Gesture(Part part, const Span& view, const Span& shown, const Span& full, const ZoomMap& m, double x);
  Step move(double x) const;
  Step atPress() const { return {view_, shown_}; }   // an Outside press already moved the view
  Part part() const { return part_; }
private:
  Step zoomTo(double windowPx) const;
  Part part_;
  Span view_, shown_, full_;
  ZoomMap m_;
  double x0_ = 0, grab_ = 0, anchor_ = 0, anchorPx_ = 0;
};

}  // namespace charts::nav
