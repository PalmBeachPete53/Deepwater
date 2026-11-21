// Chart navigator rules (src/charts/core/RangeWindow): spans, hit testing, gestures, plus a fuzz of random gestures
// checking the invariants (window inside the data, width kept by pans, min width kept by resizes).
#include "charts/core/RangeWindow.h"
#include "Check.h"
#include <QRandomGenerator>

using namespace charts::nav;

namespace {
bool near(const Span& x, const Span& y) { return std::fabs(x.a - y.a) < 1e-6 && std::fabs(x.b - y.b) < 1e-6; }
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
  CHECK(hit(400, 600, 395) == Part::Left);
  CHECK(hit(400, 600, 407) == Part::Left);
  CHECK(hit(400, 600, 391) == Part::Outside);
  CHECK(hit(400, 600, 500) == Part::Inside);
  CHECK(hit(400, 600, 593) == Part::Right);
  CHECK(hit(400, 600, 608) == Part::Right);
  CHECK(hit(400, 600, 609) == Part::Outside);
  CHECK(hit(400, 600, 100) == Part::Outside);
  // narrow window (12 px): handles shrink inside, the middle stays grabbable
  CHECK(hit(400, 412, 406) == Part::Inside);
  CHECK(hit(400, 412, 398) == Part::Left);
  CHECK(hit(400, 412, 414) == Part::Right);
}

// ---- zoom map (user's rules 2026-11-19/20): 30 px = the deepest zoom (5 bars), from 120 px the plain map; the curve never zooms ----

namespace {
ZoomMap mapFor(double full, double px = 1450, double minBars = 5) {
  ZoomMap m;
  m.px = px; m.full = full; m.minBars = minBars;
  return m;
}
}  // namespace

TEST("navigator map: 30 px is the deepest zoom, the full width is everything, continuous and invertible between") {
  const ZoomMap m = mapFor(3009);
  CHECK_NEAR(m.windowPx(5), 30, 1e-9);
  CHECK_NEAR(m.barsFor(30), 5, 1e-9);
  CHECK_NEAR(m.barsFor(10), 5, 1e-9);                     // below 30 px: still the deepest zoom
  CHECK_NEAR(m.windowPx(2), 30, 1e-9);                    // a view narrower than that (never from the chart): the stop
  CHECK_NEAR(m.windowPx(3009), 1450, 1e-9);
  const double lin = 120 * 3009 / 1450.0;                  // from here up: the plain map
  CHECK_NEAR(m.windowPx(lin), 120, 1e-6);
  CHECK_NEAR(m.windowPx(lin * (1 - 1e-9)), 120, 1e-3);    // no jump where the two parts meet
  CHECK_NEAR(m.windowPx(1000), 1000 / 3009.0 * 1450, 1e-9);
  double prev = 0;
  for (double b = 5; b <= 3009; b *= 1.07) {
    const double w = m.windowPx(b);
    CHECK(w > prev);                                       // a wider view, a wider window
    prev = w;
    CHECK_NEAR(m.barsFor(w), b, 1e-6 * b);
  }
}

TEST("navigator map: a short history is the plain map, the deepest zoom reached before 30 px") {
  const ZoomMap m = mapFor(60);   // 60 bars in 1450 px: 5 bars are already 121 px
  CHECK_NEAR(m.windowPx(5), 5 / 60.0 * 1450, 1e-9);
  CHECK_NEAR(m.stopPx(), 5 / 60.0 * 1450, 1e-9);
  CHECK_NEAR(m.barsFor(30), 5, 1e-9);
}

TEST("navigator window: the plain map's place when wide, placed by the view's position when narrow, always inside") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  const Span wide{1000, 2000};   // 1000 bars: the plain map, exact
  const Win w1 = windowOf(wide, full, m);
  CHECK_NEAR(w1.x, toPx(wide.a, full, m.px), 1e-9);
  CHECK_NEAR(w1.right(), toPx(wide.b, full, m.px), 1e-9);
  CHECK(near(viewOf(w1.x, w1.w, full, m), wide));
  const Span first{full.a, full.a + 5};   // the first 5 bars: 30 px at the left end
  CHECK_NEAR(windowOf(first, full, m).x, 0, 1e-9);
  CHECK_NEAR(windowOf(first, full, m).w, 30, 1e-9);
  const Span last{full.b - 5, full.b};
  CHECK_NEAR(windowOf(last, full, m).right(), m.px, 1e-9);
  const Span mid{1500, 1505};
  const Win w3 = windowOf(mid, full, m);
  CHECK_NEAR(w3.w, 30, 1e-9);
  CHECK(near(viewOf(w3.x, w3.w, full, m), mid));
  CHECK(windowOf(full, full, m).x == 0);
  CHECK_NEAR(windowOf(full, full, m).w, m.px, 1e-9);
}

namespace {
// the gesture as the widget runs it: press on the handle's pixel, drag until the window is `toWindowPx` wide
Span dragHandle(const Span& full, const ZoomMap& m, const Span& view, Part part, double toWindowPx) {
  const Win w = windowOf(view, full, m);
  Gesture g(part, view, full, m, part == Part::Right ? w.right() : w.x);
  return g.move(part == Part::Right ? w.x + toWindowPx : w.right() - toWindowPx);
}
}  // namespace

TEST("navigator gesture: a handle pulled to 30 px reaches the deepest zoom (5 bars) in one drag, whatever the view before") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  for (double bars : {3000.0, 1000.0, 300.0, 100.0, 40.0, 7.0}) {
    const Span view{full.b - 9 - bars, full.b - 9};   // the latest bars (a fresh chart's view)
    for (Part part : {Part::Right, Part::Left}) {
      const Span st = dragHandle(full, m, view, part, 30);
      CHECK_NEAR(st.width(), 5, 1e-6);
      CHECK_NEAR(windowOf(st, full, m).w, 30, 1e-6);
      const Span past = dragHandle(full, m, view, part, 5);   // further: stopped at 30 px
      CHECK_NEAR(past.width(), 5, 1e-6);
    }
    // back out with the left handle to the widget's side: the whole history
    const Span in = dragHandle(full, m, view, Part::Left, 30);
    const Win wi = windowOf(in, full, m);
    Gesture g(Part::Left, in, full, m, wi.x);
    const Span out = g.move(-500);
    CHECK_NEAR(windowOf(out, full, m).x, 0, 1e-6);       // the left edge at the widget's side: from the first bar
    CHECK_NEAR(out.a, full.a, 1e-6);
    CHECK(out.width() > in.width());
  }
}

TEST("navigator gesture: the other edge does not move on screen, the grabbed one follows the pointer, no jump at press") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  const Span view{2500, 2560};
  const Win w0 = windowOf(view, full, m);
  Gesture g(Part::Right, view, full, m, w0.right() + 3);   // pressed 3 px right of the edge
  const Span same = g.move(w0.right() + 3);
  CHECK_NEAR(same.a, view.a, 1e-6);
  CHECK_NEAR(same.b, view.b, 1e-6);
  for (double x : {w0.right() - 10, w0.right() + 50, w0.right() + 200, w0.right() + 600}) {
    const Win w = windowOf(g.move(x + 3), full, m);
    if (x > m.px) continue;
    CHECK_NEAR(w.x, w0.x, 1e-6);        // left edge: same pixel
    CHECK_NEAR(w.right(), x, 1e-6);     // right edge: under the pointer
  }
}

TEST("navigator gesture: pan keeps the width and stays inside the data; outside press centres the window there") {
  const Span full = domain(2999, 9);
  const ZoomMap m = mapFor(full.width());
  const Span view{1500, 1510};
  Gesture g(Part::Inside, view, full, m, 700);
  const Span far = g.move(-20000);   // far to the left
  CHECK_NEAR(far.width(), 10, 1e-9);
  CHECK_NEAR(far.a, full.a, 1e-9);
  const Win w0 = windowOf(view, full, m);
  const Span moved = g.move(740);    // 40 px right: the window follows the pointer
  CHECK_NEAR(windowOf(moved, full, m).x, w0.x + 40, 1e-6);
  const double cx = 0.25 * m.px;
  Gesture o(Part::Outside, view, full, m, cx);
  const Win wo = windowOf(o.atPress(), full, m);
  CHECK_NEAR(wo.x + wo.w / 2, cx, 1e-6);
  CHECK_NEAR(o.atPress().width(), 10, 1e-9);
  CHECK(o.part() == Part::Inside);
}

TEST("navigator gesture: fuzz — view inside the data, window inside the widget and >= the stop, anchored edge fixed") {
  QRandomGenerator rng(20261120);
  for (int round = 0; round < 300; ++round) {
    const int n = 20 + int(rng.bounded(6000));
    const Span full = domain(n - 1, rng.bounded(12));
    const ZoomMap m = mapFor(full.width(), 400 + rng.bounded(1600));
    const double w = std::max(5.0, std::exp(rng.generateDouble() * std::log(full.width())));
    const double a = full.a + rng.generateDouble() * std::max(0.0, full.width() - w);
    const Span view{a, a + w};
    const Win w0 = windowOf(view, full, m);
    CHECK(w0.x >= -1e-6 && w0.right() <= m.px + 1e-6);
    const Part part = rng.bounded(2) ? Part::Left : Part::Right;
    Gesture g(part, view, full, m, part == Part::Right ? w0.right() : w0.x);
    for (int k = 0; k < 20; ++k) {
      const double x = -200 + rng.generateDouble() * (m.px + 400);
      const Span st = g.move(x);
      CHECK(st.a >= full.a - 1e-6 && st.b <= full.b + 1e-6);
      CHECK(st.width() >= std::min(5.0, full.width()) - 1e-6);
      const Win win = windowOf(st, full, m);
      CHECK(win.x >= -1e-6 && win.right() <= m.px + 1e-6);
      CHECK(win.w >= std::min(m.stopPx(), m.px) - 1e-6);
      if (win.w < m.px - 1e-6)   // the whole history: nothing to anchor
        CHECK_NEAR(part == Part::Right ? win.x : win.right(), part == Part::Right ? w0.x : w0.right(), 1e-6 * m.px);
    }
  }
}
