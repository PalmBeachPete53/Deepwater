#pragma once
// Dev tool: `--perf-audit <dir> [--audit-pages a/b,c/d] [--perf-load ms] [--perf-idle ms] [--size WxH]`.
// Opens every page (or the listed ones) one after the other and writes <dir>/perf.tsv + <dir>/report.txt:
//   build ms       openPage() wall time (page constructor + first layout, on the GUI thread)
//   paint ms       from openPage() to the page's first paint event
//   load stalls    while the page loads (--perf-load, default 6000 ms): the GUI thread's worst stall (a 5 ms timer's
//                  lateness) and the time lost in stalls over 50 ms (what the user feels as a freeze)
//   idle cpu %     process CPU (user + system, every thread) while the page just sits there (--perf-idle, default 8000)
//   idle stalls    worst stall while idle (timers / animations doing too much on the GUI thread)
//   rss MB         physical footprint after the idle window
// Pages stay alive (as in normal use, a page is created once and kept): at the end, with every page created and a
// light page shown, "hidden cpu %" is what all the hidden pages still cost together.
// Run with DW_PROFILE (and DW_CACHE_DIR: an empty one for a first-launch run, the same one again for a warm-cache run).
#include <QList>
#include <QStringList>

class MainWindow;

namespace perfaudit {
void run(MainWindow* w, const QString& dir, const QStringList& pages, int loadMs, int idleMs);
}
