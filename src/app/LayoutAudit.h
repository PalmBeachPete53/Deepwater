#pragma once
// Dev tool: `--layout-audit <dir> [--audit-sizes 1024x600,1280x720] [--audit-pages a/b,c/d] [--audit-load ms]`.
// Opens every page (or the listed ones) alone in the window, at each size: saves <dir>/<page>@<w>x<h>.png and writes
// <dir>/report.txt with what a layout gets wrong at that size:
//   MIN    the page alone needs more than the size (its minimumSizeHint): the window can't go that small
//   CUT    a widget partly outside its clipping parents, not inside a scroll area (cut off, unreachable)
//   TEXT   a label / button whose text is wider (or taller) than the widget (clipped or squeezed text)
//   HSCROLL a scroll area of the page that scrolls sideways at this size
// Kept on purpose: new pages broke the window's minimum size before (memory: responsive-window). It can't see text a
// widget paints itself: look at the captures too.
#include <QList>
#include <QSize>
#include <QStringList>

class MainWindow;

namespace layoutaudit {
void run(MainWindow* w, const QString& dir, const QList<QSize>& sizes, const QStringList& pages, int loadMs);
}
