# Audit brief — app vs reference videos, 0.5 s by 0.5 s

Goal (user's request): check the current Deepwater app against the `Reference/` videos 0.5 s by 0.5 s, add every
missing part and correct every UI discrepancy.

SCRATCH = /private/tmp/claude-501/-Users-raphael-dev-deepwater/dc71afae-325d-4318-bf2b-f513b5ef5c63/scratchpad

## 0. Fresh copy first
The main tree changed since your last pass (header logo, settings key fix, globe perf fixes in Bank Research and
Macro, Market Hours 20 fps, Session Recap fixes, News feed source defaults, everyone's phase 2). Delete your work
copy and re-copy `/Users/raphael/dev/deepwater` (without `build/` and `Reference/`) into the same folder, then
`cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos && cmake --build build`.
Same ownership and rules as before (`docs/ARCHITECTURE.md`, `docs/AGENT_BRIEF.md`): edit only your own folders,
namespaces, real data only, no fake values, `DW_PROFILE` for every launch. Don't undo the perf fixes (measure CPU
if you touch animations).

## 1. Watch every frame again, and reproduce each state in the app
Frames: `SCRATCH/frames/<Tab>/<sub>/tNNN.N.jpg` (one every 0.5 s, 1600x966; `diff.txt` shows where changes happen).
For EVERY frame of your videos, in order:
- open the reference frame;
- put the app in the same state (same page, view, tab, selection, scroll, hover position, open panel/modal,
  dropdown, tooltip, crosshair…) using your `DW_*` hooks (add hooks if a state can't be reached; document them),
  screenshot at 1600x1042 (`tools/shot.sh … 1600x1042 <build dir>`; the page area = rows 76..1042, i.e. the
  bottom 1600x966 of the 1x image, or 152..2084 of the 2x capture), and compare side by side. A crop tool:
  `SCRATCH/tools/crop in.png out.png x y w h` (pixels of the 2x capture); colour picker `SCRATCH/tools/pick`.
  Consecutive identical frames (diff ≈ 0) can share one app screenshot, but still look at each reference frame.
- list every difference: missing component, label/text, column, order, position/size (> 3 px at 1x), font
  (family, size, weight, spacing, case), colour, icon, border/bracket, spacing, number format, state/interaction,
  animation, empty/loading state. Data VALUES differ (live data): that is not a discrepancy, but the format,
  layout and the kind of value must match.

Write the findings to `docs/audit/<Tab>-<sub>.md` in your copy: one row per finding (frame, region, reference,
app, fix, status). Keep it short but complete.

## 2. Fix everything
Fix every finding in your own folders. A missing part = build it (with real data or an honest "—" + tooltip when
no source exists). Interactions shown in the video must work the same way (click, hover, scroll, drag, keyboard).
If a fix needs a shared file (src/app, src/core, src/net, src/ui, src/market, CMakeLists.txt) or another package's
folder, do it in your copy and list the exact diff in your report.

## 3. Verify
Re-screenshot every state, compare again with the frames, until no visible difference remains except live data
values and the deliberate, documented deviations (TradingView → own chart; "Alfred" → AUTOMATED READ; English UI;
sources that don't exist — keep those documented in the audit file). Check 1280x800 still works. Measure CPU of
your pages (idle, 20 s, `ps` time) and report it.

## 4. Report (≤ 40 lines)
Number of findings per page and how many fixed; the remaining ones and why; files changed; shared-file diffs
needed; hooks added; CPU; screenshot paths of the final states (one per key reference frame).
