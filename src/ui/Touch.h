#pragma once
// Phones (2026-11-21, user's choice: hamburger menu, iPhone portrait + landscape).
//
//  compact(width)   a header or page this narrow lays itself out for a phone held upright (one column, menu ☰).
//                   Each widget decides from its own width, so a Mac window made that narrow shows the same.
//  touch            finger scrolling: every scroll area's viewport is given a QScroller (kinetic drag). On iOS with
//                   the finger; on a Mac with DW_TOUCH_SIM=1, the mouse's left button drags instead (to try the phone
//                   layout with `--size 430x932`).
#include <QObject>

namespace ui {
constexpr int kCompactWidth = 760;   // below it: phone layout (an iPhone upright is 390–440 points wide)
inline bool compact(int width) { return width > 0 && width < kCompactWidth; }

namespace touch {
bool enabled();   // iOS, or DW_TOUCH_SIM=1
void install();   // main(), after the QApplication: scroll areas made from now on scroll by dragging
}
}  // namespace ui
