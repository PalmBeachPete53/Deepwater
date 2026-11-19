# Deepwater — performance audit (2026-11-16/17)

Scope: the whole app (all 27 pages, launch, background work), third_party/ excluded.

**Status 2026-11-17: lots 1–6 done (each below).** Measured on clones of the user's data, old
binary vs new, same runs (`--perf-audit`, CPU every 2 s, `sample`):

| Lot 1 | Before | After |
|---|---|---|
| Launch → first grab (global/overview, 3 runs) | 786–876 ms | 671–693 ms |
| SSL init on the GUI thread at launch | 143 ms (62 ms with a warm-up alone) | 0 (requests wait in their queue for the pool thread) |
| Facts update pass, quiet page (process CPU, 349 s) | 15.4 CPU-s; 12 % / 10 % at +40 / +60 s | 13.5 CPU-s; 5 % / 5 % |
| Same with Sources opened then hidden (400 s) | 38.3 CPU-s, GUI thread ~30 % in `refreshAll` | 20.4 CPU-s, `refreshAll` gone from the profile |
| Sources page worst stall while loading / Seasonality worst stall | 157 / 74 ms | 44 / 26 ms |

Done: §3.5 SSL warm-up (`net::warmTls`, pump waits for it) and cache reads header-first + writes on one ordered
background thread with an in-memory pending map (reads never see an older file; `invalidate` ordered after writes);
§2 B-F1 writer reads only from the batch's first period (+ the last value before it in changes mode), relinks from the
first new fact, adds to `chain_stats`, rebuilds only the touched months of `facts_monthly` (forecast kinds and chains
without cells: full rebuild); B-F2 `Store::chain(key)` is one query; `Cot::finish` parses on a pool thread (waiters for
the same key keep waiting in `m_loading`); §3.4 Sources page refreshes only when visible and sets style sheets only on
change, Sources › Data repaints only when a status dot changes.
Checked: facts DB invariants recomputed from scratch (prev links, chain_stats, facts_monthly incl. float ranks) — 0
mismatch on 2,195 chains after every new-code pass; offline A/B (network denied, same cache) old vs new writer:
identical facts, stats, monthly cells (only `at` of the implied path = time of the run); online same-day A/B: every
difference traced to the inputs (a Yahoo bar published between runs, live implied path / risk index). charts_data_tests
100/0. Pixel A/B (offline, same data): COT, Sources › Data, Seasonality identical except the header clock; COT online:
identical except the live price chart.
**Lot 2 (globe pages + Market Hours), 2026-11-17: done.** On screen (Retina, on battery, old and new binaries
interleaved, 2 rounds, 20 s idle each): Bank Research 25.6 → 23.4 %, Macro 13.7 → 10.8 %, Overview 18.7 → 15.7 %,
Market Hours 11.4 → 15.8 % (the wedge fix below; the user chose to keep it as is). Footprint about +30 MB with the
three globe pages opened. The first on-screen attempt ran with the screen locked (a hidden window doesn't draw,
numbers meaningless); it was also measured with `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2` (app-side paint work only; the window server's
flush, the part the merged passes save, isn't in it), old vs new binary in the same session, 2 rounds each:

| Page (offscreen) | Before | After | Footprint |
|---|---|---|---|
| news/bank-research | 23.1 % | 20.9 % | +13 MB (background with the sphere) |
| economy/macro | 10.4 % | 7.9 % | small (composed image limited to the disc) |
| global/overview | 12.2 % | 11.2 % | — |
| global/market-hours | 7.7 % | 12.9 % | — (see the wedge bug below) |

Done: `ui::DotGlobe` projection scale per camera + reused dot buckets; Bank Research camera trig/centre/scale once
per frame, label widths once, repaint = one bounding rect of what moved (last painted frame ∪ disc/arcs/chips ∪
wedge, was the 1.5 R square), sphere composed on a background copy and blended only inside the wedge's box; Macro
repaint = disc ± 4 px (was ± 92 px), sphere composed once over the disc area, fast-path blits, camera once per paint
for country outlines; Overview sphere composed into the panel background, strength pulse on `global::FrameClock`
(15 fps, was its own 66 ms timer), view range cached at rebuild, tape gradient cached; `global::FrameClock` and the
Market Hours sweep land on wall-clock 30ths / 20ths of a second (+2 ms) so the pages' half-second blinks and
per-second updates share the animation's pass; Bank Research cursors toggle on globe frames; Market Hours per-second
repaint = hands + digital time (was the whole panel), title cursor as its own opaque child, per-second layer pixmap
reused; session time zones built once (`global::Sessions`).
Checked: pixel identity old vs new pipeline in-process (Macro, Overview sphere, tape gradient: 0 px) and old vs new
binary (Bank Research, frozen `DW_GLOBE_LON`: identical except blinking cursors and the clock); partial repaints
verified frame by frame (previous frame + repainted rect == full render): Bank Research 400 frames, Macro 300,
Market Hours 300 — 0 px outside, except the title cursor's wall-clock blink. Temporary hooks removed.
Lesson: a repaint region of several rects made every fill clip against each rect (Bank Research's 4,500 land dots
cost 2×, Market Hours' conical wedge 2.5×): each widget now asks for one bounding rectangle per pass.
**Bug found (pre-existing): Market Hours' `wedgeBox()` started from a null QRectF at the centre, which `united()`
ignores: the sweep's repaint missed the centre side of the wedge, so between the per-second full repaints the inner
part of the wedge froze (it jumped once a second). Fixed; the correct repaint is what costs +5 points offscreen
(measured: the old box with the new code = 9.95 % vs base 9.65 %). The user kept the fix as is (2026-11-17); not
taken: flat non-AA slices (11.9 % offscreen), 15 fps, old box.**
**Lot 3 (Charts), 2026-11-17: done.** Measured in the real app with a temporary `bench` UI step (removed), default
pane BTCUSDT 1D, 3,000 bars, EMA + RSI, ms per call, old → new (2 runs each, matching within 5 %):
full `render()` 6.9 → 2.6 (78 % of it was `volPoint` parsing "rgba(…)" per bar), repaint 1.82 → 0.70, live tick +
repaint 3.55 → 2.45, Heikin Ashi render 5.0 → 1.27; a crosshair move now repaints its strips only (old + new
vertical strip, horizontal strip with the price label, time axis row) instead of the whole chart.
Done: `cssColor` parses each string once (thread-local cache, bounded) with a static regex, `volPoint` uses two
constant colours, `fmt::fixed` no regex; `Chart::paintEvent` copies only the repainted rects of the base image
(Source mode when the background is opaque, was a SourceOver blend of the whole image); `Chart::crossRegion()` and
crosshair updates by region; the alert checker writes nothing and takes no lock when no alert is active (the JS
rewrote the same list every 15 s from launch and the Alerts tab rebuilt), reads state.json only for watchlist alerts.
Checked: charts_core_tests 107/0, charts_data_tests 100/0, plot_demo builds; crosshair repaints verified on 150
moves (pixels changed outside the strips: only on the two moves that hover the pane separator, which already
repaints everything); screenshot of the tab.
Not done (behaviour would differ from the JS port, user's call): trimming live bars past settings.bars (a 1s chart
grows by 3,600 bars an hour and every render with it), fetching only the missed bars when the tab comes back after a
bar closed (today: full reload of every pane), throttling non-simple chart types to the 1.5 s cadence (now 1.3 ms a
render, not worth it). Skipped (measured small): `renderIndicators` teardown (0.35 ms), `shift()` lookups (2 % of a
render), `merge` detach.

**Lot 4 (Technical Heatmap + Market/Account), 2026-11-17: done.** Offscreen 2x, network denied (same cache for both
binaries), process CPU over 60 s, old → new: Technical Heatmap scan of the 150 series 11.8–17.7 % (4 runs) → 8.6–9.5 %;
coming back to the page (second visit) 28.3 % with a 53 ms stall → 2.2 %, 3 ms. Markets Watch idle online 4.0–4.1 →
3.7–3.9 % (noise level). Instrument sheet chart: the time labels' local time 3.0–3.2 ms → 0.16 ms per redraw for
3,600 bars (each drag / wheel step), no 12 MB pixmap allocated per step.
Done: Technical Heatmap — the loading text repaints only when the table has no rows (it repainted the whole table on
every answer), a rebuild whose rows on screen are unchanged repaints only the scroll bar, elided texts and advances
computed once per rows / layout (not per paint), hover repaints the two instrument tiles, KPI cards / badge / signal
file skip unchanged values (the signal file was repainted on every 120 ms rebuild), the page rescans on show only when
the last scan is older than the 30 min cache TTL (the refresh timer fires when it runs out); `market::candles` parses
on one pool thread (answers keep their order, callbacks on the GUI thread, skipped when the context died); QuoteHub
announces a poll once when all its chunks are in (first loads still per chunk); the risk calculator recomputes only
while visible (and on show); instrument sheet chart: pixmap reused, UTC offset looked up once per half hour of UTC
time, Heikin Ashi bars computed once per change of the bars (the overlay recomputed them on every mouse move); Index
Performance and Year Path cards draw frame + chart once into a layer and only the hover readout on mouse moves;
`account::spec` memoized per raw symbol with static regexes (Track Record per trade / per paint / in the sort).
Checked: partial repaints of the heat table frame by frame during a full scan + a tile hover (73 partial repaints, 0 px
changed outside their region; temporary hook removed); screenshots old vs new: Technical Heatmap after the scan
identical except the animated title pulse (1-level difference), instrument sheet EUR/USD (chart with hover, time
labels, Index Performance, Year Path) identical except 5 px of the tab bar outside the changed widgets; local time
per half-hour offset = `toLocalTime()` on 5.1 M instants 1970–2040 in Paris, New York, Lord Howe, Kathmandu.
Not done: tile glow cache (each cell has its own fill colour: hundreds of images of ~140 KB), `range=5d` refreshes
merged into the 2-year series (network change), Track Record per-paint work beyond `spec` (the user's journal is
empty: nothing to measure), the 1W/1M tooltip text of Markets Watch (one build per poll now), the journal save on a
bridge import (it records the import time).

**Lot 5 (Facts page + AI pages, GL composition A/B), 2026-11-17: done.** GL composition **confirmed** on screen
(battery, 2 rounds, network denied, facts pass held off): Macro idle 10.3–10.5 % → 15.9–16.1 % after a visit to Facts
(control through Seasonality: 10.0–10.3 → 8.5–8.7 %). The 3D views (the app's only OpenGL widgets) were created with
the page, hidden behind the 2D view, which was enough to switch the window to OpenGL composition for the session.
Now created on the first 3D view: Macro after a Facts visit 16.0–16.2 → 8.2–8.4 %, footprint after Facts 590–612 →
241 MB (the "+320 MB kept" of §3.7 was this), Facts first-show stall 198–265 → 99–101 ms. Opening a 3D view still
switches the window (the `QOpenGLWindow` container is not done).
Done: lazy 3D views; Facts loops — the shown keys in a hash for the chain list (per row per paint), lanes built from a
key → chains index (TICK ALL was 2,195²), TICK ALL / UNTICK ALL / shown count / landscape list linear, `withSource`
and `decisionTextShown` regexes compiled once (14 per decision block per paint); AI — `Engine::stop` no longer waits
on the GUI thread (stopped processes exit on their own, killed after 1.5 s; loading a model waits until the previous
one is gone, as the blocking wait ensured; the app's quit still waits), the chat's figures check is computed once
per finished answer (it ran over every answer 20 times a second while one streams), header style sheets only on
change, Local models list: hidden, rebuilt when shown; shown, a download step updates its row's state line and bar
(the rows were rebuilt every ~800 ms).
Measured, not changed: a chat refresh while an answer streams costs 10.8 ms on a 60-message conversation (85 k
characters), ~all in `setHtml` (whole document re-parsed): ~22 % CPU at 20 refreshes/s, growing with the
conversation; header style sheets were 0.07 ms. Replacing only the last message (`QTextCursor::insertHtml`) gives a
different document (first paragraph merged into the previous block, body colour lost): dropped. The mini cube does
animate while text streams (each refresh repaints it): §3.8's "side bug" was wrong.
Checked: screenshots old vs new — Facts 2D (differences = the time range following the clock: two runs of the old
binary differ more, 15,631 px vs 6,589), landscape and cube 3D (header clock only), AI chat with a 60-message
conversation and figures checks, Local models: identical. Not run: EJECT of a running model and a download's
progress (no chat model or download in the test profile: the installed model is an image model).
Chat refresh cadence (user's go, visible on long conversations only): while an answer streams, 20 refreshes a second
as long as one costs under 10 ms, then 8 × its cost apart (≤ 200 ms). Fake runner streaming 400 words in 10 s into the
60-message conversation, 2 runs: process CPU 33.8–34.7 % → 18.9–19.6 %, the whole answer shown in both.
Not done (visible, user's call): lanes' SQL out of `paintEvent` (a lane would show empty for a moment); tiny blocks
merged per device column (pixels change).
Fixed (pre-existing bug, user's go): a question asked while the local model was off (standby, or failed) was lost —
`ask()` set the pending flag, then `start()` → `stop()` cleared it, so the loaded model never received it. `start()`
now keeps it; a failure clears it (so a new question is accepted). Checked with a fake model runner that fails its
first load and answers the second: unfixed, the question stays unanswered; fixed, it gets its answer.

**Lot 6 (marginal batch, memory), 2026-11-17: done.** Offscreen 2x, network denied, idle 10 s, old vs new binary, 2
rounds: Rates 4.0–4.2 → 1.8–1.9 %, Risk Sentiment 6.3–6.4 → 5.1–5.3 %, Retail Sentiment 4.7–4.8 → 3.9–4.0 %,
Liquidity 2.7 → 1.9–2.0 %, Squawk 2.5–2.7 → 2.1–2.2 %; News Feed, Session Recap, Macro unchanged (their remaining cost
is the app-wide network handling and the globe).
Done: M5 Rates — the 500 ms blink (30×40 px) paints the top block only, hover zones kept; M2 Positioning ticker —
block cursors repainted when they turn on/off (was 15×/s), title bar's still part (gradient, icon, title, badge) cached,
text widths measured per layout; M3 Market `TitleStrip` — same still-layer cache for its 10 fps dot, opaque (the page
backdrop no longer repaints under it); M4 Squawk ticker opaque, calendar countdowns repaint their column only (the
whole list when the next event changes); M1 header — layout (fits, tab rectangles) memoized per width / tab / language /
theme, gear path static; `market::sessionDay` — UTC offset per zone looked up once per half hour (whole series no
longer convert bar by bar; same dates as before on 1.7 M instants × 23 exchange zones 1920–2040); M12 regexes
static (AI resolve, facts slug, ML datasets, retail groups, Charts side panel, central-bank HTML); M13 Squawk counts
in one pass; M20 a note's PDF opened once (was 3); §3.10 News images: decoded images bounded at 128 MB (was 160
images, up to ~0.6 GB), thumbnails in a 64 MB LRU (was 300 of any size, an arbitrary one dropped), thumbnail scaling
on a worker; M21 Macro background painted where exposed (no 20 MB pixmap).
Checked: screenshots old vs new, same moment — Rates, Squawk, News Feed identical; Risk / Retail Sentiment, Technical
Heatmap: only the animated dots (1–2 levels, time of capture); Liquidity: the dot and one device row of the title
strip's bottom line, 1 level of 255 (the gradient computed in the cached layer); Macro: inside the rotating globe only.
Not done: M6 Agents cubes (1.2 % with no lit cube), M7 timer phases (a blink rhythm would change), M8 covered window
(macOS stops painting an occluded window; timers alone are cheap), M14 rest (JS parity), M17 rest, M18 font cache,
M19 `ml_status` (counting by file names could count files that do not parse), M22 hard-coded 2x pixmaps (no change on
Retina; on 1x the strips would blur after a move back to a Retina screen), M23 merged translators, M24 style sheets
(pixels), M25 build flags (measured neutral on arm64 on 2026-11-02, ThinLTO + `-mcpu`), M26 (system code).
Side bug fixed (user's go): `WatchHistory` never asked again for a market once its closes had arrived, so the 1W/1M
references of a long session stayed those of its first hour. Every market of a batch is now asked again after 30 min
(the cache's TTL) when it arrived, after 10 min / 1 min when it did not; known closes stay shown meanwhile. Checked
with a temporary 20 s period: the 7 batch requests went out at launch and again at the first quote poll after it.

Not done: B-F3 (async `Store()` open, 15–23 ms once at +20 s: a fresh DB's reader would open before the file exists);
cache eviction and conditional requests (§3.5, later lot); `market::candles` parse off the GUI thread (lot 4).
Unexplained: a ~105 ms GUI-thread delay once during the pass, same before and after; a 125 s `sample` of the whole pass
shows no Deepwater frame holding the main thread for 80 ms+, nor SQLite / disk waits.
Machine: M3 Pro, built-in Retina (2x), window 1600x1042, build `build/` (RelWithDebInfo = `-O2 -g`, no LTO).

Method:
- **Measured in the app** with the built-in harness (`--perf-audit dir`, app/PerfAudit.cpp: build time, GUI-thread
  stalls from a 5 ms timer, process CPU from getrusage, physical footprint) on isolated profiles (`DW_PROFILE`):
  `perf-profile` = APFS clone of the user's data (facts DB 405 MB, HTTP cache 358 MB, no models), `perf-nofacts` = the
  same without the facts DB. Attribution with macOS `sample` (12 s at 1 ms, per thread, per function), memory with
  `vmmap --summary` / `heap`. Launch timed to the first window grab.
- **Read**: five read-only code reviews by area (infra + AI/ML backends, Global/News, Economy/Positioning/Facts/AI
  pages, Market/Account, Charts). Every finding below was checked against the code; the ones also confirmed by a
  measurement say **measured**.

Confidence: **measured** = number from this audit's runs, **read** = confirmed in the code (cost estimated),
**plausible** = needs a measurement before acting.

Caveat found on the way: process CPU is process-wide, so the facts update pass (§2) inflates any page measured
from launch +20 s to ~+2 min. The first harness run had it (Technical Heatmap read 29 %, Session Recap 9 %); the table
below comes from runs without it.

---

## 1. Measured baseline

### Idle CPU per page (page shown alone, data loaded, 10 s window)

| Page | Idle CPU | First-show stall | Notes |
|---|---|---|---|
| news/bank-research | **21–23 %** | 56–62 ms | globe at 15 fps (sample: `GlobeView::paintEvent` dominates the GUI thread) |
| global/overview | **15–20 %** | 36 ms | tape 30 fps + globe 15 fps + strength pulse on its own 66 ms timer |
| global/market-hours | **11 %** | 44–54 ms | conical sweep 20 fps + 1 s clock rebuild (`ClockPanel::rebuildTop`, `glowText`) |
| economy/macro | **11 %** | 71–80 ms | globe 10 fps (`GlobeWidget::paintEvent`, `DotGlobe::paintLand`) |
| news/squawk | 8–10 % | **136–143 ms** | build 98–107 ms; the TV's WebKit process is extra (not counted) |
| news/news-feed | 4–7 % | 37–74 ms | 30 fps ticker strip |
| positioning/risk-sentiment | 5–6.5 % | 76–84 ms | 15 fps ticker (pulse dots + blinking cursors) |
| market/technical-heatmap | 5.4 % (cache warm) | 73–95 ms | ~30 % for the first minute of a cold scan (STATUS.md) |
| positioning/retail-sentiment | 4–5 % | 22 ms | same 15 fps ticker |
| market/liquidity | 4–6 % | 17–28 ms | `TitleStrip` 10 fps (not opaque) |
| economy/rates | 4.3 % | 40 ms | 500 ms blink repaints the whole body |
| account/risk-calculator | 3–4 % | 47–65 ms | 100 ms animation timer |
| market/fundamental-heatmap | 3–3.5 % | 56–67 ms | cursor blink repaints the whole content |
| charts/charts | 3.3 % | 39–51 ms | idle is fine; costs are per action (§3.6) |
| facts/chains | 2.7–3.2 % | **195–262 ms** | build 140–146 ms; **+320 MB footprint, kept** |
| sources/sources | 0.6 % | **316–331 ms** | build 173–180 ms, first paint 301 ms |
| every other page | ≤ 2.8 % | ≤ 122 ms | ai/agents 96–122 ms first-show stall |

All pages created and kept, lightest one shown: **hidden pages cost 2.1–4.0 %** together, worst stall 11–88 ms
(timers do stop on hide; what remains is network-driven work, §3.4).

### Launch
- Launch → first grab of the window: **0.66–0.85 s** (3 start pages × 3 runs). Fine overall; two fixed costs inside:
  - **143 ms: SSL init on the GUI thread** at the first network request (`QSslSocketPrivate::ensureInitialized` →
    system CA bundle parsed into `QSslCertificate`s, each date parsed through ICU). Paid at every launch.
  - **144 ms: `+[NSWritingToolsCoordinator isWritingToolsAvailable]`** inside `-[NSApplication finishLaunching]`
    (`_addTextInputMenuItems`, system code building the Edit menu). Possible workaround unverified.
  - If the last page was Charts: `ChartsApp` built in the MainWindow ctor (186 ms, of which the SSL init above).

### Memory (physical footprint)
- Start ~180–260 MB; after visiting every page **750–940 MB**, and it stays.
- **Facts page: +320 MB** on first open (251 → 571 MB), kept after leaving (control run without Facts: flat).
- Macro page: +110 MB (globe layers, datasets).
- Facts update pass: transient **peak ~500 MB**, back to ~130 MB after (`heap`: footprint 130 MB, peak 503 MB).
  `ps` RSS stays at 760 MB because freed malloc pages stay mapped: they're reclaimable, not a leak.

---

## 2. Background work at launch: the facts update pass  (**measured**)

From launch +20 s (`startFactsAtLaunch`), then every 6 h (`kPassMs`), when the last pass is older than 6 h:
**~25 % CPU for ~110 s** (37 % at peak), **worst GUI stall 138 ms**, transient footprint peak ~500 MB.
Split (sample): GUI thread 13–16 %, `facts-writer` thread 6–10 %.

- **B-F1 Writer re-reads whole chains** — `src/facts/FactStore.cpp` `Writer::write`: `SELECT … WHERE chain=?` over the
  full chain, then `relink` (full ordered scan), `stats`, `monthly` (another full scan). The prices pass calls it
  127 × 4 chains; 3.95 M of the 4.45 M facts are prices (largest chain 24.8 k rows). Sample: `QHash<pair,Slot>`
  emplace and `QMap<long long,double>::operator[]` under `Writer::write`.
  Fix: load only slots with `period >= min(obs.period)` (+ the last row before it for change mode), relink from the
  first changed period, update `chain_stats` incrementally, ranks from `facts_monthly`. Same rows: not visible.
- **B-F2 GUI-thread work in the Importer** — `Importer.cpp`: `Store::chain(key)` re-runs the full `chains()` join
  (2,195 rows, ORDER BY 4 text columns) once per priced central bank; `macro()` builds ~240 k `Obs` (3 QDateTime +
  3 QString each) on the GUI thread; `prices()` reads/parses each instrument's cached body on the GUI thread
  (`net::readCache`, `candlesBetween` seen in the sample). Fix: one `chains()` into a QHash (or `WHERE key=?`), build
  the obs and parse the candles in QtConcurrent. Not visible.
- **B-F3 `Store()` opens the DB with `BlockingQueuedConnection`** (`FactStore.cpp:418`): the GUI thread waits for
  `Writer::open` (15–19 ms in every sample). Fix: open asynchronously, queue reads until open.
- First import for a new user (empty DB, started from the Facts tab): **34–38 % CPU for minutes, footprint +480 MB**
  (measured in the run without facts DB). Same fixes apply.

---

## 3. Bottlenecks (ranked by expected gain)

### 3.1 Globe pages: Bank Research 22 %, Overview 15–20 %, Macro 11 %  (**measured**)
Sample over 12 s: `banks::res::GlobeView::paintEvent` 840 ms, `economy::GlobeWidget::paintEvent` 751 ms,
`overview::GlobePanel::paintEvent` 469 ms (+ `DotGlobe::paintLand` ~190 ms, `paintGraticule` ~90 ms), plus
`_platform_memmove` / Qt blend functions at the top of the stack.

- **Bank Research** (`pages/news/bank-research/GlobeView.cpp`): every frame (15 fps) dirties a square of 1.5R+6 per
  side (≈ 2.7 Mpx at 2x), copies `m_bg` over it, draws the sweep (16 slices), **alpha-blends the 544² sphere pixmap
  (1.18 Mpx) over a sweep that is half hidden by it**, the graticule with a 2-device-px pen (stroker path, not the
  thin-line path), ~4.5 k land rects and 21 antialiased beacon chips; `view()` recomputes cos/sin of spin and tilt on
  each of ~11.5 k calls per frame.
  Fix: dirty only the disc bbox + the old/new wedge bboxes (~−40 % pixels copied and flushed); bake the sphere into
  `m_bg` and draw the sweep as ring slices outside the disc (removes the 1.18 Mpx blend; only a 1-px ring at the edge
  changes, ≤ 70 alpha → A/B screenshot); trig once per frame (as `ui::DotGlobe::setCamera` does); no AA on the
  axis-aligned chips. Expected −35–50 % of the frame.
- **Macro** (`pages/economy/GlobeWidget.cpp:156–178`): 10 fps repaint of the disc **plus a 92 px margin that never
  changes** (halo already baked into `m_base`): ~3.0 M device px per frame; `m_body` ARGB pixmap blended over the 2R
  square each frame; `drawImage(QRectF, img, QRectF)` form. Fix: dirty `disk().adjusted(-3,-3,3,3)` (−37 % px), draw
  the body into `m_base` in `ensureBase()`, blit with `drawImage(QPoint, img, QRect)`. ~2.5× less pixel work.
  Hover (`:227–243`): full-widget `update()` on every mouse move even when the hovered country didn't change, and
  `project()` → `syncCamera()` per ring point. Fix: update label rects only when unchanged; camera once per paint.
- **Overview** (`pages/global/overview/`): ~45 repaint passes per second — `FrameClock` 30 Hz (tape, wire, globe every
  other frame) **plus `StrengthPanel::m_pulse` on its own 66 ms timer, out of phase** (+15 passes/s). `MarketTape`
  builds a full-strip horizontal gradient + 2 fade gradients every frame (30 fps). `GlobePanel` blends `m_body` each
  frame (no sweep: can be baked into `m_bg` in `prepare()`) and calls `global::isOpen` 4×/frame, each building 2
  `QTimeZone` + zone-aware `QDateTime` (sample: `isOpen`/`currentSpan`/`spanOn` ~27 ms per 12 s).
  Fix: drive the pulse from FrameClock (phase already from the wall clock: identical), cache the tape background and
  fades as pixmaps, bake the body, static `QTimeZone` per session (`global/Sessions.cpp:87–99`). ~−3–4 points.
- All three: `DotGlobe::paintLand` rebuilds 10 `QVector<QRectF>` buckets per frame without reserve (sample shows
  `QArrayDataPointer<QRectF>::reallocateAndGrow`): member buffers with `clear()` keep capacity.

### 3.2 Market Hours 11 %  (**measured**)
`pages/global/market-hours/MarketHoursWidgets.cpp:277–292`: the sweep is a `QConicalGradient` on an antialiased wedge
at 20 fps (one atan2 per pixel; `atan2` shows at the top of the stack) — the trap fixed on Bank Research before.
Every second `rebuildTop` (`:218–259`) **allocates a new 4 MB ARGB pixmap**, blends `m_over` (1 Mpx) and strokes
"HH:mm" and ":ss" 3× with 7/11/15 px pens (`glowText`, 71 ms per 12 s in the sample). The 500 ms page tick is out of
phase with the 50 ms sweep timer (extra passes).
Fix: flat non-AA alpha slices (≤ 3 alpha levels of banding), reuse the pixmap, cache the HH:mm glow per minute, drive
the tick from the sweep's phase. ~−4–6 points.

### 3.3 Technical Heatmap: repaint storm while scanning, rescan on every show  (**read**; ~30 % for a minute per STATUS.md)
- `Scanner::start` sends ~150 requests (Yahoo pace 350 ms → ~2.9 answers/s for ~52 s). Per answer:
  `HeatTable::setLoadingText` (`HeatTable.cpp:58`) → full table `update()` although the text only shows with no rows;
  `Badge::setText` → `updateGeometry()` each time (`MarketKit.cpp:145`); `KpiCard::setValue` repaints even when
  unchanged (`:160`, each card draws 25 glow texts at 30 px); rebuild coalesced at only 120 ms recomputes all 762
  cells, measures ~127×13 `horizontalAdvance` twice, sorts, repaints the table (~950 AA path ops per repaint: each
  tile = AA `fillPath` + 7 AA rounded-rect glow strokes) and the whole `SignalFile` (two word-wrap layouts).
  Fix: early-outs on unchanged text/value, `update()` only when no rows, coalesce at ~500 ms while scanning,
  pre-rendered tile glow pixmap per (size, fill, border), cells cached per Scanner item. Not visible.
- `showEvent` (`TechnicalHeatmapPage.cpp:277–282`) restarts a scan on **every** show when none is running: ~150
  synchronous cache reads (60–100 KB each) inside showEvent, 150 parses + `technicalRead` + rebuilds; after the 30 min
  TTL all 150 two-year daily series are re-downloaded (same every 30 min while visible).
  Fix: rescan on show only if the last scan is older than the TTL; refreshes fetch `range=5d` and merge.

### 3.4 Sources: hidden page restyles every row on network activity; Data diagram repaints  (**read + measured**)
- `SourcesPage.cpp:390` connects `sources::Notifier::changed` (fires ≤ every 400 ms whenever any reply arrives,
  `Sources.cpp:113–122`) to `refreshAll()` **with no visibility check**, once the page has been opened. Each refresh:
  ~76 rows × 2 `setStyleSheet` (row subtree + label), `isDisabled()` ×3 (QSettings read + QSet build),
  `sharingHosts()` copying `all()`. Estimated 10–25 ms per refresh at up to 2.5 Hz during network bursts (QuoteHub
  every 20 s, facts pass). This is the "setStyleSheet on refresh" trap again.
  Fix: `if (!isVisible()) { m_stale = true; return; }` + refresh on show; restyle a row only when its status changes.
- Sources › Data shown during network activity: `srcmap::Canvas::paintEvent` took **521 ms of GUI time per 8 s**
  (~6 %) in the facts-pass sample. Fix: repaint only rows whose status changed / cache the static diagram layer.
- First open: build 173–180 ms, first paint 301 ms → 316–331 ms stall (**measured**): ~76 rows of styled widgets.
  Painting the row frames instead of per-row stylesheets fixes both.

### 3.5 HTTP layer on the GUI thread  (**read + measured at launch**)
- **SSL init 143 ms** at the first request (§1). Fix: warm `QSslConfiguration::defaultConfiguration()` /
  `QSslSocket::supportsSsl()` on a pool thread at launch, before the first `net::get` (the old app did this).
- `net/Http.cpp:45–62, 147, 249`: `readCache` reads the **whole body before checking freshness** (up to 14 MB; 104
  files > 1 MB, 227 MB total), in `get()` and again in `pump()` for queued requests (stale entries read twice).
  `writeCache` (`:201–213`) commits a `QSaveFile` (sync) on the GUI thread after every OK response: 1,894 files /
  104 MB written in the last 24 h. Fix: read the header (magic, fetched, status) first, the body only when used; hand
  writes to a single-thread pool (the reply is already in memory). Not visible.
- Cache never evicted (`Http.cpp:41–44`): 358 MB in ~2 days; `candlesBetween`'s key includes `to/86400`
  (`market/Candles.cpp:62`) → ~127 new files a day from the prices pass. Fix: prune on a worker at launch (age > 30 d
  or LRU to ~256 MB).
- No conditional requests (no ETag / If-Modified-Since): expired entries re-download in full (BIS 14 MB, World Bank
  4 MB per indicator, OECD 4.6 MB) and callers re-parse. Fix: store validators, on 304 refresh `fetched` and flag
  `notModified` so callers can skip parsing.
- `market::candles` → `parse()` runs in the network callback on the GUI thread (`Candles.cpp:18–36`): Liquidity reloads
  1h × 2y (~12 k bars, ~1.2 MB JSON) every 5 min while visible; an FX instrument sheet parses 13 daily series since 2017
  in one burst, then `currencyIndex` does ~35 k QMap inserts (`WatchCalc.cpp:49–74`). Fix: parse in QtConcurrent
  (pure function), Liquidity refresh with `range=5d` + merge.

### 3.6 Charts tab  (**read**; idle is fine, costs are per action)
- **`cssColor()` compiles a QRegularExpression per `rgb…` colour** (`pages/charts/plot/Format.cpp:16`):
  `volPoint` (`ChartPane.cpp:367`) calls it **per bar on every `render()`** (volume on by default), `histData`
  (`:109`) per bar for Volume / Net Volume inside `renderIndicators` (every ≤ 1.5 s on a live chart). Estimated
  15–90 ms per render at 3000 bars. Fix: static regex + small QColor memo; two static QColors in `volPoint`.
  Pixel-identical. Shrinks every item below.
- Live bars never trimmed (`ChartPane::merge`, `:1334–1349`): a 1s chart adds 3,600 bars/h; `renderIndicators`,
  `rebuildTimePoints` (concat + sort, `Chart.cpp:331`) and `TimeScale::setPoints` (2 QDateTimes per bar) all grow
  with it (~10× after 8 h). Fix: drop to `settings.bars` in one chunk past 110 %. Visible: oldest bars leave (= what
  a reload shows).
- Crosshair move = full-widget `drawImage` of `base_` (ARGB SourceOver, ~4–5 Mpx at 2x) and full flush, on every pane
  with synced crosshair (`Chart.cpp:618, 643`; `Render.cpp:85–87`). Fix: update only the old/new strips + label
  rects and blit `e->region()`; `CompositionMode_Source` or `Format_RGB32` base. Not visible. Re-measure (the old
  renderer's numbers don't apply).
- Showing the tab after a bar closed **reloads every pane in full** (`ChartPane::showEvent`, `:1491–1510`): settings.bars
  per pane (Binance pages by 1000 → 3 requests), compare, full render. Fix: fetch only the gap and `merge()`.
- Non-simple types (heikin, renko, kagi, pnf, linebreak, range) run a full `render()` per live message
  (`:1374–1379`; 4/s on a Binance perp). Replay 5×/10× = full render per step, followers too (`:1442–1452`).
  Fix: same 1.5 s throttle; incremental last-bar for Heikin Ashi / replay +1.

### 3.7 Facts page  (**measured + read**)
- **+320 MB footprint on first open, kept** (measured, §1). Unverified split: 150-lane LRU of full `Store::facts()`
  rows (12 columns incl. 3 strings) + monthly cells + GL buffers. Fix: lanes keep only what the canvas draws
  (origin/note only for the inspector); bound the LRU by bytes.
- SQL inside `paintEvent`: every lane scrolled into view runs `Store::facts(chainId)` synchronously
  (`FactsPage.cpp:284 → 215–229 → FactStore.cpp:498`), tens of ms for the 24.8 k-row chains. Fix: async like
  `loadMonthly()`, placeholder lane meanwhile (small visible change).
- First-show stall 195–262 ms (build 140–146 ms).
- **Plausible: opening Facts switches the whole window to OpenGL composition** (QOpenGLWidget in `MacroCube`/
  `Landscape`; Qt keeps it for the window's life). A/B on the same profile: Macro idle 11.5 → **16.0 %** after
  visiting Facts vs 11.0 → 11.5 % in the control run; Overview +4.9 vs +2.3; Bank Research no change. Needs a run
  with the fix before acting. Fix if confirmed: host the 3D views in a `QOpenGLWindow` through
  `QWidget::createWindowContainer` (watch overlays: a native container draws above siblings).
- Quadratic loops: `reloadLanes` shown × chains (TICK ALL: 2,195² ≈ 4.8 M compares), `m_shown.contains` on a
  QStringList per row per paint (`:150`), `withSource()` builds a regex per label per paint (`:67`),
  `decisionTextShown()` compiles 14 regexes per call per decision block per paint (`CentralBanks.cpp:115–119`).
  Tiny blocks: up to 24.8 k AA `fillRect` of ~1.2 px per lane per paint (`:391`) → merge per device column, no AA.

### 3.8 AI pages  (**read**)
- Chat streaming (`pages/ai/ChatView.cpp:287–512`): every 50 ms, rebuilds the HTML of **every** message and
  `setHtml`s the whole document (re-parse + relayout = O(conversation)), re-runs the figures-check regex scan over all
  tool outputs, and `syncHeader()` does 5 `setStyleSheet` (20×/s). Side bug: `MiniCube::setState` restarts its 66 ms
  timer each call, so the mini cube never animates while text streams. Fix: update only the last block (QTextCursor),
  cache the figures check, syncHeader on state change only.
- `Engine::stop` (`ai/Engine.cpp:179, 191`): `waitForFinished(1500)` + kill + `waitForFinished(1000)` on the GUI thread
  → up to **2.5 s freeze** when ejecting a model (the helper frees a multi-GB Metal model). Fix: close stdin,
  `finished` → `deleteLater`, `singleShot(1500)` → kill. Same behaviour.
- LLM › Local models: during a download `rebuildLocal()` destroys/recreates every row (~6 `setStyleSheet` each) every
  ~800 ms, even hidden (`ModelHub.cpp:211–215 → AiModelsPage.cpp:230`). Update in place, gate on visibility.

### 3.9 Market / Account interactions  (**read**)
- Markets Watch instrument sheet chart (`watch/CandleChart.cpp:316–341`): every drag/wheel event allocates a new
  full-size 2x pixmap (~12 MB), repaints the static chrome and calls `toLocalTime()` twice per visible bar (`:531`;
  ~3,600 mktime per mouse move zoomed out). Fix: UTC offset once per rebuild (per day across DST, like
  `ChartPane::utcOffset`), reuse `m_cache`, chrome layer.
- Index Performance card (`SheetCards.cpp:495`): full repaint per hover move, rebuilding two AA paths of ~2,400 points;
  `YearPathCard` same with 366. Fix: cached line layer + crosshair on top.
- Track Record: `displaySymbol → spec → cleanSymbol` builds up to 3 non-static regexes per call for suffixed broker
  symbols (`EURUSD.r`), called per trade in reload / filters / `Stats::compute` / refilter / **inside the sort
  comparator** / per visible row per paint; `toLocalTime()` 2–3× per trade per reload and per trade per paint in the
  Advanced scatter (`TrAdvanced.cpp:159`, repaints on every mouse move); top/bottom chart copies + `stable_sort`s
  the trade list per paint (`:228–232`); BridgeSync imports always save the indented journal and emit `changed` even
  with 0 new trades (`Journal.cpp:260`). Fix: memoize `spec()` per raw symbol, precompute local date/hour/symbol per
  trade, sort keys once, skip save when nothing changed. Size depends on the journal.

### 3.10 News images memory  (**plausible**)
`news/Feed.cpp:411–443`: up to 160 fully decoded QImages up to 1400 px wide (3–4.4 MB each → up to ~0.5–0.7 GB) kept
by count; `storyPixmap` (`FeedWidgets.cpp:78–98`) holds 300 scaled copies keyed by size (a window resize fills it with
stale sizes) and evicts `cache.begin()` (arbitrary, may drop on-screen images); the smooth scale of the ≤ 1400 px image
runs on the GUI thread (2–8 ms per thumbnail during a feed load). News Feed measured +34 MB at load: worst case not
seen in the runs. Fix: scale in the worker (`QImageReader::setScaledSize`), `QCache` with cost = `sizeInBytes()`
(e.g. 48 + 64 MB), drop the full-size cache (the disk cache keeps the bytes 7 days).

---

## 4. Marginal improvements (cheap, each small)

Animation passes and blinks
- M1 Header (`app/Header.cpp:62, 96–196`): the 1 Hz clock `update(rect)` reruns the whole paint: `fitFor()` /
  `subFitFor()` twice (up to 6 font sets × 10 labels with QFontMetricsF + translate), gear path booleans rebuilt;
  `hits()` on every mouse move. Seen on every sampled page (9–21 ms per 12 s). Cache Fit/hits per (width, language,
  tab), static gear path.
- M2 Risk / Retail Sentiment ticker 15 fps (`positioning/PosKit.cpp:36–44, 285–290`): two cursors change twice per
  3 s cycle, yet `QRect(200,4,160,26)` repaints 15×/s; `TitleBar` not cached. 100–125 ms ticker, update cursors only
  on change, cache TitleBar → ~5–6 % to ~2 %.
- M3 `mkit::TitleStrip` 10 fps (`MarketKit.cpp:88`) not `WA_OpaquePaintEvent` → the page backdrop (full grid + 220 px
  gradient) repaints under it each tick (Liquidity sample: TitleStrip + LiquidityPage paint). Set the attribute.
- M4 Squawk ticker (`SquawkWidgets.cpp:137–147`) not opaque either: `m_left` and `SquawkPage::paintEvent` repaint under
  it at 30 fps. CalendarPane repaints the whole viewport every second for one countdown column (`CalendarPane.cpp:316`).
- M5 Blinks repainting whole widgets: Rates body (`RatesPage.cpp:103, 715`; crosshair animation at 16 ms full-body
  too, `:46–56`), Macro `AssetFile` / Registry / TopBar (`MacroPage.cpp:53, 63, 322`, TopBar calls `coverage()` and
  `setToolTip()` per paint), Fundamental content (`FundamentalPage.cpp:166–196, 331`), Technical `SignalFile`.
  Early-out when `e->rect()` is inside the blink rect, or cache the text heights.
- M6 Agents cubes (`AgentsPage.cpp:188–189, 265–297, 379`): one 66 ms timer per lit card, non-opaque card (parent
  fills under it), text relaid out per frame; Ready breathing changes ~1/255 alpha per frame. One shared timer, 8 fps
  in Ready, cube rect only, opaque cached background. Measure with `DW_AI_LIGHT=1` (no animation) vs normal.
- M7 Timers on their own phase add passes: Bank Research blink 500 ms, Recap **530 ms** (`RecapPage.cpp:79`), Overview
  registry blink, Market Hours 500 ms tick vs 50 ms sweep. Align on FrameClock / one shared 500 ms phase.
- M8 Animations keep running when the window is fully covered or on another Space (only minimize sends hide):
  check `windowHandle()->isExposed()` in FrameClock and the page timers. Unverified saving; measure by covering the
  window.

Repeated computation
- M9 `global::Sessions.cpp:87, 92, 99`: `QTimeZone(s.tz)` from the IANA id on every `spanOn` / `currentSpan` /
  `nextSpan` (Market Hours, MarketTape, Overview globe at 15 fps). One static zone per session.
- M10 QuoteHub (`market/QuoteHub.cpp:23, 74–125`) emits once per 20-symbol chunk (7 emissions per 20 s, ~350 ms apart);
  each makes Markets Watch run a full `refresh()` (105 rows, masonry, all panels) and Risk Calculator `compute()`
  (unfiltered, even hidden). Emit once per poll; filter Risk Calculator by its symbols. Spark chunks are parsed and
  written to cache on the GUI thread every 20 s (`force` = ttl 0 but staleOnError still writes).
- M11 `StrengthPanel::visible()` (`:137–155`) loops 8 × ~160 `value()` per pulse frame: cache in `rebuild()`.
- M12 Non-static `QRegularExpression` in on-demand paths: `ai/DataTools.cpp:75–77` (one per instrument per `resolve()`),
  `Importer.cpp slug()`, `ml/Datasets.cpp:31–32`, `Retail.cpp:74` (`RetailSymbol::group`), `charts Format.cpp:71`
  (`fmt::fixed` for negative labels, per crosshair move), `SidePanel.cpp:547`, `CentralBanks.cpp:76–78`.
- M13 `Wire::count(f)` rebuilds `items(All)` (`Wire.cpp:350–365`): `SquawkPage::refresh` does ~8 list builds per wire
  change. One pass.
- M14 Charts: `toChart()` → `shift()` two QJsonObject lookups per call (~9,000 per render, `ChartPane.cpp:932–941`);
  `merge()` detaches a full bar copy per tick (`shown = bars`, `:1341, 1358`); `fmt::timeWeight` 2 QDateTimes per
  bar (`Format.cpp:131`); legend rebuilt per crosshair move even on the same bar (`:1147`); `renderIndicators` tears
  down and re-adds every series every ≤ 1.5 s (`:476–482`; also resets a manual indicator-pane zoom); paper
  `checkOrders` copies the orders array per tick (`Paper.cpp:111–145`).
- M15 Charts alert checker (`charts/data/Alerts.cpp:516–548`, from launch, tab never opened): every 15 s reads + parses
  the whole `state.json`, creates/removes `alerts.lock`, re-reads `alerts.json` + `alert-log.json`, rewrites
  `alerts.json` (serialised twice) and emits `changed()`, even with zero alerts (seen in the idle sample). Early-out
  with no active alert, skip the write when bytes are equal. `loadAllCustom()` builds one QuickJS runtime per custom
  file on the GUI thread at +1.5 s.
- M16 HeatTable paint: `elidedText` per cell per paint, `headFont` + QFontMetricsF per column per paint
  (`HeatTable.cpp:225–229, 278`). Compute in `setRows` / `layoutColumns`.
- M17 Markets Watch: `changeOf` builds the 1W/1M tooltip per row per refresh (`WatchPage.cpp:206`, locale date format +
  arg chain) → build lazily on ToolTip; `instrumentIcon` key built with 4 `arg` per row per paint (`WatchIcons.cpp:93`).
- M18 Fonts built per paint / mouse move: `theme::mono()` + QFontMetricsF in `PillBar::layoutRects` (`ui/Kit.cpp:81`),
  chart tooltips/axis tags, Bank Research beacons (21 per frame). Small static font cache.
- M19 `ml_status` AI tool parses every dataset fully to count them (`ai/MlTools.cpp:113`, 16 CSV / 6.4 MB, ~50–100 ms):
  count with `entryList`.
- M20 PdfPanel opens the PDF with `pdf::info()` 2–3× per note open (`PdfPanel.cpp:58, 445, 446`).

Memory
- M21 Macro: `GlobeView::m_bg` is a 2x pixmap of the whole area (~20 MB) almost fully covered by the opaque
  `GlobeWidget`, whose `m_base` holds the same gradient (`MacroPage.cpp:268`). Paint the 2 px gap only.
- M22 Market Hours / strip / thumbnail pixmaps hard-coded at dpr 2.0 (`MarketTape.cpp:98`, `FeedPanels.cpp:190`,
  `FeedWidgets.cpp:81, 363`, `Reader.cpp:276`): on 1x screens (Windows 100 %, external monitors) 4× memory and a
  scaled blit each frame. Use `devicePixelRatioF()`. No change on Retina.

App-wide
- M23 French loads 15 separate translators (`core/I18n.cpp:44–49`): each `translate()` scans up to 15 `.qm`. One merged
  `.qm`.
- M24 The app stylesheet's `QWidget { color: … }` (`ui/Theme.cpp:247`) puts every widget through the stylesheet engine;
  `ui::label` adds a per-label sheet (65 call sites). Raises build time and every `setStyleSheet` cost. Palette
  colours + narrower selectors (can change pixels: screenshot A/B).
- M25 Build: `-O2 -g`, no LTO, no CPU tuning, no release config. For packaging: Release + ThinLTO
  (`CMAKE_INTERPROCEDURAL_OPTIMIZATION`), `-mcpu=apple-m1`, keep `-ffp-contract=off` on Charts; no fast-math (numbers
  unchanged; confirm with the charts tests). Gain unverified (0–10 % on hot loops).
- M26 `+[NSWritingToolsCoordinator isWritingToolsAvailable]` 144 ms at launch (§1): system code; check whether a
  menu-level opt-out exists before spending time on it.

---

## 5. Checked and fine
- Every page timer stops on hide; hidden pages together cost 2–4 %.
- Fonts: bundled names match the theme (no QFontDatabase alias population); `theme::map`/`pal()` hashed and cheap;
  `market::find` static hash; `Candles::sessionDay` caches its zone.
- Off the GUI thread already: RSS/wire XML, image decode, PDF render, ModelHub SHA-256, Technical `technicalRead`,
  Liquidity `findPools`, Seasonality, facts monthly load, Charts JSON > 256 KB, quant checks, imports.
- Charts: painting and autoscale touch only visible bars, line decimation, `update()` merged, hidden tab stops feeds /
  replay / health timers, persist debounced 400 ms, watchlist redraws on the 5 s quotes only.
- VoxelCube cached per size; Facts 3D views redraw on demand; Risk panels and Bank Positioning texture cached.
- BridgeSync: QFileSystemWatcher + 700 ms debounce, no polling.
- Launch: 0.66–0.85 s to the first frame (pages lazy, Charts eager only when it was the last page).

Side bug seen (not perf): `WatchHistory::m_asked` is never cleared after a success, so the 1W/1M reference closes are
never refreshed in a long session.

---

## 6. Suggested order (lots, each measured before/after, pixel A/B where it draws)
1. **Launch + background** — SSL warm-up on a pool thread, facts pass (B-F1..F3), HTTP cache header-first read and
   async writes, Sources hidden-refresh gate. Gains: −143 ms per launch, ~25 % CPU × 110 s per pass → a few %,
   no 138 ms stalls.
2. **Globe pages** — Bank Research, Macro, Overview, Market Hours (§3.1–3.2, M9, M11). Expected: Bank Research 22 →
   ~12 %, Macro 11 → ~5 %, Overview 15–20 → ~11–14 %, Market Hours 11 → ~6 %.
3. **Charts** — `cssColor` regex first (one line, pixel-identical), then live-bar trim, crosshair partial blits, gap
   fetch on show, throttled non-simple types (§3.6, M14, M15).
4. **Technical Heatmap + Market/Account interactions** (§3.3, §3.9, M10, M16, M17).
5. **Facts page + AI pages** (§3.7, §3.8), then the GL composition A/B.
6. **Marginal batch** (M1–M8 animation passes, M12 regexes, M23 translators), memory (§3.10, M21, M22), build flags.

How to re-measure: `DW_PROFILE=<clone> build/Deepwater.app/Contents/MacOS/Deepwater --size 1600x1042 --perf-audit <dir>
--audit-pages <ids> --perf-load 8000 --perf-idle 10000`, then `sample <pid> 12 1` for attribution. Keep the facts pass
out of the window (profile without facts DB, or last pass < 6 h) or measure it on purpose by ageing `last.*` in the
clone's `meta` table.
