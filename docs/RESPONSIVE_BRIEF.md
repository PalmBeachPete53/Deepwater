# Responsive brief — every laptop and PC

User's request: the app must work well on every laptop and PC screen. The pages were tuned to the reference size
(window 1600x1042, page area 1600x966). Now every page must work from the smallest supported window to ultrawide
monitors, and keep matching the reference at the reference size.

SCRATCH = /private/tmp/claude-501/-Users-raphael-dev-deepwater/dc71afae-325d-4318-bf2b-f513b5ef5c63/scratchpad

## 0. Fresh copy first
Main changed (adaptive header, window minimum 1024x600, `tools/sizes.sh`, audit merges, a few lead fixes).
Delete your work copy, re-copy `/Users/raphael/dev/deepwater` (without `build/` and `Reference/`), configure and
build. Same ownership and rules as before (`docs/ARCHITECTURE.md`, `docs/AGENT_BRIEF.md`).

## 1. Size matrix (window client size; page area = size minus the 76 px header)
| Size | Typical machine |
|---|---|
| 1024x600 | minimum window (small laptops, split screen) |
| 1280x680 | 13–14" Windows laptop, 1920x1080 at 150 % scaling |
| 1366x700 | budget 1366x768 laptops |
| 1440x820 | MacBook Air 13" |
| 1536x780 | 1920x1080 at 125 % scaling |
| 1600x1042 | reference size: must still match the reference frames |
| 1728x1080 | MacBook Pro 16" |
| 1920x1000 | 1080p desktop monitor |
| 2560x1360 | 1440p monitor |
| 3440x1360 | ultrawide monitor |

`tools/sizes.sh <page> <out dir> [delay] [build dir] [sizes…]` screenshots a page at every size with the offscreen
renderer (1x, any size). Also check the real 2x screen at 1280x800 and 1728x1080 with `tools/shot.sh`. Use your
`DW_*` hooks to check open states (modals, sheets, panels, dropdowns, tooltips, scrolled views) at the small sizes.

## 2. Rules
- **Nothing clipped, overlapping or unreachable** at any size. Text may be elided only where the full text is in a
  tooltip. Every part of the page must be reachable: below the reference height, the page (or its main column)
  scrolls vertically with the same smooth wheel scrolling the pages already use; avoid horizontal page scrolling
  (a wide table may scroll horizontally inside its own panel).
- **Small widths (1024–1440):** reflow instead of squeezing — multi-column layouts may go to fewer columns or stack
  side panels below; tables drop or merge low-priority columns (moved to tooltips); charts keep a usable minimum
  size (e.g. ≥ 260 px tall). Fonts may step down at most one or two px below 1366 wide, never under 9 px.
- **Large sizes (1920–3440):** use the space: charts and tables grow, grids add columns or rows where it makes
  sense, panels keep sensible proportions. No huge empty areas (more than ~25 % of the page blank), no stretched
  text lines (prose max ~900 px wide), no tiny fixed panels lost in a big window. Ultrawide 3440: content may be
  capped and centred if stretching would look wrong, but say why.
- **Modals, sheets, overlays, dropdowns and menus** must fit inside a 1024x600 window (scroll inside them if needed).
- **Live resize:** layouts adapt on resize (resizeEvent), not only at launch. Test at least once by launching at
  one size and resizing (a hook or a QTimer in a temporary test is fine; remove it after).
- **Performance:** animated pages (globes, tickers, sweeps) cost more at 2560x1360 and 3440x1360. Measure CPU there
  (`ps` time, 20 s, after loading) and cap render resolution/frame rate so it stays close to the reference size.
- **Reference size unchanged:** at 1600x1042 the page must still match the reference frames as after the audit.

## 3. Report (≤ 30 lines)
Per page: what changed per size class, any size that still has a problem and why; files changed; shared-file
diffs needed; CPU at 1600x1042 and 2560x1360; paths of the final screenshots (one folder per page with all sizes).
