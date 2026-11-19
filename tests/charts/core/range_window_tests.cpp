// Chart navigator rules (src/charts/core/RangeWindow): spans, hit testing, gestures, plus a fuzz of random gestures
// checking the invariants (window inside the data, width kept by pans, min width kept by resizes).
#include "charts/core/RangeWindow.h"
#include "Check.h"
#include <QRandomGenerator>

using namespace charts::nav;

namespace {
bool near(const Span& x, const Span& y) { return std::fabs(x.a - y.a) < 1e-9 && std::fabs(x.b - y.b) < 1e-9; }
}

TEST("navigator: logical range <-> edges and domain") {
  const Span s = fromLogical(10, 19);
  CHECK_NEAR(s.a, 9.5, 1e-12);
  CHECK_NEAR(s.b, 19.5, 1e-12);
  CHECK_NEAR(s.width(), 10, 1e-12);   // ten bars
  double f, t;
  toLogical(s, f, t);
  CHECK_NEAR(f, 10, 1e-12);
  CHECK_NEAR(t, 19, 1e-12);
  const Span d = domain(2999, 9);
  CHECK_NEAR(d.a, -0.5, 1e-12);
  CHECK_NEAR(d.b, 3008.5, 1e-12);
  CHECK(domain(-1, 9).width() == 0);
  // ChartPane's fresh view {n-160, n+8} with n = 3000 ends exactly on the domain's right edge
  CHECK_NEAR(fromLogical(3000 - 160, 3000 + 8).b, d.b, 1e-12);
}

TEST("navigator: clampPan and clip") {
  const Span d{-0.5, 99.5};
  CHECK(clampPan({-10, 10}, d) == (Span{-0.5, 19.5}));
  CHECK(clampPan({95, 115}, d) == (Span{79.5, 99.5}));
  CHECK(clampPan({20, 40}, d) == (Span{20, 40}));
  CHECK(clampPan({-50, 200}, d) == d);
  CHECK(clip({-10, 10}, d)->a == -0.5);
  CHECK(!clip({200, 300}, d));
}

TEST("navigator: hit testing with an 8 px grip") {
  const Span d{0, 100};   // 1 unit = 10 px on a 1000 px navigator
  const Span w{40, 60};   // 400 … 600 px
  CHECK(hit(w, d, 1000, 395) == Part::Left);
  CHECK(hit(w, d, 1000, 407) == Part::Left);
  CHECK(hit(w, d, 1000, 391) == Part::Outside);
  CHECK(hit(w, d, 1000, 500) == Part::Inside);
  CHECK(hit(w, d, 1000, 593) == Part::Right);
  CHECK(hit(w, d, 1000, 608) == Part::Right);
  CHECK(hit(w, d, 1000, 609) == Part::Outside);
  CHECK(hit(w, d, 1000, 100) == Part::Outside);
  // narrow window (12 px): handles shrink inside, the middle stays grabbable
  const Span n{40, 41.2};   // 400 … 412 px
  CHECK(hit(n, d, 1000, 406) == Part::Inside);
  CHECK(hit(n, d, 1000, 398) == Part::Left);
  CHECK(hit(n, d, 1000, 414) == Part::Right);
  CHECK(hit(w, Span{0, 0}, 1000, 500) == Part::None);
}

// ---- zoom map (user's rule 2026-11-19): 30 px = the deepest zoom, from 120 px the plain map of the whole history ----

namespace {
ZoomMap mapFor(double full, double px = 1450, double minBars = 2) {
  ZoomMap m;
  m.px = px; m.full = full; m.minBars = minBars;
  return m;
}
}  // namespace

TEST("navigator map: 30 px is the deepest zoom, the full width is everything, continuous and invertible between") {
  const ZoomMap m = mapFor(3009);
  CHECK_NEAR(m.windowPx(2), 30, 1e-9);
  CHECK_NEAR(m.barsFor(30), 2, 1e-9);
  CHECK_NEAR(m.barsFor(10), 2, 1e-9);                     // below 30 px: still the deepest zoom
  CHECK_NEAR(m.windowPx(3009), 1450, 1e-9);
  const double lin = 120 * 3009 / 1450.0;                  // from here up: the plain map
  CHECK_NEAR(m.windowPx(lin), 120, 1e-6);
  CHECK_NEAR(m.windowPx(lin * (1 - 1e-9)), 120, 1e-3);    // no jump where the two parts meet
  CHECK_NEAR(m.windowPx(1000), 1000 / 3009.0 * 1450, 1e-9);
  double prev = 0;
  for (double b = 2; b <= 3009; b *= 1.07) {
    const double w = m.windowPx(b);
    CHECK(w > prev);                                       // a wider view, a wider window
    prev = w;
    CHECK_NEAR(m.barsFor(w), b, 1e-6 * b);
    CHECK_NEAR(b / m.shownWidth(b) * 1450, w, 1e-6 * w);   // the window is the view inside the span shown: exact
    CHECK(m.shownWidth(b) <= 3009 + 1e-9);
    CHECK(m.shownWidth(b) >= b - 1e-9);
  }
  CHECK_NEAR(m.shownWidth(5000), 3009, 1e-9);
}

TEST("navigator map: a short history (no room for the narrowing) is the plain map, the deepest zoom reached before 30 px") {
  const ZoomMap m = mapFor(60);   // 60 bars in 1450 px: 2 bars are already 48 px
  CHECK_NEAR(m.windowPx(2), 2 / 60.0 * 1450, 1e-9);
  CHECK_NEAR(m.shownWidth(2), 60, 1e-9);
  CHECK_NEAR(m.barsFor(30), 2, 1e-9);
}

TEST("navigator shown span: whole domain for a wide view, narrow and kept while the view stays inside, recentred near an edge") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  CHECK(shownSpan(std::nullopt, full, fromLogical(2700, 2999), m) == full);   // 300 bars
  const Span v1{1500, 1510};
  const Span s1 = shownSpan(std::nullopt, full, v1, m);
  CHECK_NEAR(s1.width(), m.shownWidth(10), 1e-9);
  CHECK(v1.a >= s1.a && v1.b <= s1.b);
  CHECK(shownSpan(s1, full, Span{1502, 1512}, m) == s1);           // a small pan: same span, the mini curve stays
  const Span v3{s1.a + 1, s1.a + 11};                              // near its left edge: recentred
  const Span s3 = shownSpan(s1, full, v3, m);
  CHECK(!(s3 == s1));
  CHECK(v3.a >= s3.a + 0.05 * s3.width() - 1e-9);
  const Span s4 = shownSpan(s1, full, Span{1500, 1520}, m);        // another zoom: the width of that zoom
  CHECK_NEAR(s4.width(), m.shownWidth(20), 1e-9);
  const Span end{2995.5, 3008.5};                                  // the last bars and the empty slots after them
  CHECK_NEAR(shownSpan(std::nullopt, full, end, m).b, full.b, 1e-9);
}

namespace {
// the gesture as the widget runs it: shown span from shownSpan, press on the handle's pixel
Gesture::Step dragHandle(const Span& full, const ZoomMap& m, const Span& view, Part part, double toWindowPx, double* pressX = nullptr) {
  const Span shown = shownSpan(std::nullopt, full, view, m);
  const double xa = toPx(view.a, shown, m.px), xb = toPx(view.b, shown, m.px);
  const double x = part == Part::Right ? xb : xa;
  if (pressX) *pressX = x;
  Gesture g(part, view, shown, full, m, x);
  return g.move(part == Part::Right ? xa + toWindowPx : xb - toWindowPx);
}
}  // namespace

TEST("navigator gesture: a handle pulled to 30 px reaches the deepest zoom in one drag, whatever the view before") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  for (double bars : {3000.0, 1000.0, 300.0, 100.0, 40.0, 7.0}) {
    const Span view{full.b - 9 - bars, full.b - 9};   // the latest bars (a fresh chart's view)
    for (Part part : {Part::Right, Part::Left}) {
      const Gesture::Step st = dragHandle(full, m, view, part, 30);
      CHECK_NEAR(st.view.width(), 2, 1e-6);
      CHECK_NEAR(toPx(st.view.b, st.shown, m.px) - toPx(st.view.a, st.shown, m.px), 30, 1e-6);   // drawn 30 px wide
      if (part == Part::Right) CHECK_NEAR(st.view.a, view.a, 1e-9); else CHECK_NEAR(st.view.b, view.b, 1e-9);
    }
    // back out with the left handle (the view ends at the data's end): the whole history shown again once released
    const Gesture::Step in = dragHandle(full, m, view, Part::Left, 30);
    const Gesture::Step out = dragHandle(full, m, in.view, Part::Left, 1450);
    CHECK(shownSpan(std::nullopt, full, out.view, m) == full);
  }
}

TEST("navigator gesture: the grabbed edge stays under the pointer, the other one does not move on screen, no jump at press") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  const Span view{2500, 2560};
  const Span shown = shownSpan(std::nullopt, full, view, m);
  const double xa = toPx(view.a, shown, m.px), xb = toPx(view.b, shown, m.px);
  Gesture g(Part::Right, view, shown, full, m, xb + 3);   // pressed 3 px right of the edge
  const Gesture::Step same = g.move(xb + 3);
  CHECK_NEAR(same.view.a, view.a, 1e-9);
  CHECK_NEAR(same.view.b, view.b, 1e-6);
  for (double x : {xb - 40, xb - 20, xb + 50, xb + 200}) {
    const Gesture::Step st = g.move(x + 3);
    if (x - xa < 30 || st.view.b >= full.b - 1e-9) continue;   // at the stop, or the data's end
    CHECK_NEAR(toPx(st.view.a, st.shown, m.px), xa, 1e-6);       // anchored edge: same pixel
    CHECK_NEAR(toPx(st.view.b, st.shown, m.px), x, 1e-6);        // grabbed edge: under the pointer
  }
}

TEST("navigator gesture: pan keeps the width, stays inside the data, the span shown follows; outside press centres there") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  const Span view{1500, 1510};
  const Span shown = shownSpan(std::nullopt, full, view, m);
  Gesture g(Part::Inside, view, shown, full, m, 700);
  const Gesture::Step st = g.move(-20000);   // far to the left
  CHECK_NEAR(st.view.width(), 10, 1e-9);
  CHECK_NEAR(st.view.a, full.a, 1e-9);
  CHECK(st.view.a >= st.shown.a - 1e-9 && st.view.b <= st.shown.b + 1e-9);
  const double cx = 0.25 * m.px;
  Gesture o(Part::Outside, view, shown, full, m, cx);
  const double c = toUnits(cx, shown, m.px);
  CHECK_NEAR((o.atPress().view.a + o.atPress().view.b) / 2, c, 1e-9);
  CHECK(o.part() == Part::Inside);
}

TEST("navigator gesture: fuzz — view inside the data and drawn whole, window >= 30 px, anchored edge fixed on screen") {
  QRandomGenerator rng(20261119);
  for (int round = 0; round < 300; ++round) {
    const int n = 20 + int(rng.bounded(6000));
    const Span full = domain(n - 1, rng.bounded(12));
    const ZoomMap m = mapFor(full.width(), 400 + rng.bounded(1600));
    const double w = std::max(2.0, std::exp(rng.generateDouble() * std::log(full.width())));
    const double a = full.a + rng.generateDouble() * std::max(0.0, full.width() - w);
    const Span view{a, a + w};
    const Span shown = shownSpan(std::nullopt, full, view, m);
    CHECK(view.a >= shown.a - 1e-6 && view.b <= shown.b + 1e-6);
    const Part part = rng.bounded(2) ? Part::Left : Part::Right;
    const double xa = toPx(view.a, shown, m.px), xb = toPx(view.b, shown, m.px);
    Gesture g(part, view, shown, full, m, part == Part::Right ? xb : xa);
    for (int k = 0; k < 20; ++k) {
      const double x = -200 + rng.generateDouble() * (m.px + 400);
      const Gesture::Step st = g.move(x);
      CHECK(st.view.a >= full.a - 1e-6 && st.view.b <= full.b + 1e-6);
      CHECK(st.view.a >= st.shown.a - 1e-6 && st.view.b <= st.shown.b + 1e-6);
      CHECK(st.view.width() >= std::min(2.0, full.width()) - 1e-6);
      const double win = toPx(st.view.b, st.shown, m.px) - toPx(st.view.a, st.shown, m.px);
      CHECK_NEAR(win, m.windowPx(st.view.width()), 1e-6 * m.px);          // exact: the map's width for that zoom
      CHECK(win >= std::min(30.0, m.windowPx(2)) - 1e-6);
      const double anchorPx = part == Part::Right ? toPx(st.view.a, st.shown, m.px) : toPx(st.view.b, st.shown, m.px);
      CHECK_NEAR(anchorPx, part == Part::Right ? xa : xb, 1e-6 * m.px);    // the other edge never moves on screen
    }
  }
}
