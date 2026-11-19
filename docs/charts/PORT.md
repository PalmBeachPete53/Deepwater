# Charts tab — port of quant-terminal to C++

User's request (2026-11-16): an exact copy of `quant-terminal/` (Node.js + browser app) inside Deepwater, in a new
header tab **Charts**, written in C++. Everything identical except the language, plus these user decisions:
- **No PTQ**: no Academy ribbon, no Help link to part-timequant.com, no PTQ tab, no `lib/ptq.js` / `public/js/ptq.js`
  port. Any UI that only offers PTQ (e.g. "Send to PTQ" buttons) is left out; everything else stays.
- **Deepwater theme**: same layout, same widgets, same behaviour, but colours and fonts follow Deepwater's theme
  (`theme::pal()`, `theme::map()`, `theme::mono/sans`) and its 11 themes. Quant Terminal's own light/dark button
  is kept and switches between a light and a dark rendering of the Charts tab only if it can be done with the
  Deepwater palette (see ui agent notes); chart series colours (#2962ff, #26a69a, …) stay as in the JS.
- Licence: MIT, notice kept in docs/charts/NOTICE.md. Lightweight Charts is NOT ported: the chart renderer is our
  own QPainter code that behaves like it (same scales, crosshair, panes, zoom/scroll, markers, price lines).

"Identical" means: same features, same defaults, same texts (English source strings, French via the usual
translations later), same formulas (indicator values equal to the JS to 1e-9), same data sources and fallback
order, same notes/errors shown to the user, same files and JSON shapes in the data folder, same local HTTP API.

## Module map (JS → C++)

| quant-terminal | C++ | namespace |
|---|---|---|
| public/lib/indicators.js | src/charts/core/Indicators.{h,cpp} (+ more .cpp if large) | charts::ind |
| custom/*.js loader, `ta` for custom code | src/charts/core/CustomJs.{h,cpp} (QuickJS-ng, third_party/quickjs-ng) | charts::custom |
| public/lib/series.js | src/charts/core/Series.{h,cpp} | charts::series |
| public/lib/backtest.js | src/charts/core/Backtest.{h,cpp} | charts::bt |
| public/lib/quant.js | src/charts/core/Quant.{h,cpp} | charts::quant |
| public/lib/seasonals.js | src/charts/core/Seasonals.{h,cpp} | charts::seasonals |
| lib/providers.js, lib/feeds.js | src/charts/data/Providers, Feeds | charts::data |
| lib/market-info.js | src/charts/data/MarketInfo | charts::data |
| lib/imports.js | src/charts/data/Imports | charts::data |
| lib/store.js | src/charts/data/Store | charts::store |
| lib/alerts.js | src/charts/data/Alerts | charts::alerts |
| server.js routes | src/charts/data/ApiServer (QHttpServer, 127.0.0.1:3777, same routes) | charts::api |
| public/js/chart.js, drawings.js, volume-profile.js | src/pages/charts/ChartPane*, Drawings, VolumeProfile | charts::ui |
| public/js/app.js, index.html, css | src/pages/charts/ChartsPage* | charts::ui |
| public/js/panels.js, research.js, quantui.js | src/pages/charts/panels/* | charts::ui |
| test/*.test.js | tests/charts/core/*.cpp, tests/charts/data/*.cpp | — |

`src/charts/core` is pure: QtCore (+ QuickJS-ng for CustomJs) only, no widgets, no network, deterministic.
Contract headers written first and binding for everyone: `src/charts/core/Types.h`, `src/charts/core/Indicators.h`.

## Rules for everyone working on the port
- Read the JS file you port completely before writing C++. Port function by function, keep the JS names
  (camelCase), keep the comments that explain why. Same edge cases (NaN handling, warm-up lengths, rounding).
- Deepwater conventions: docs/ARCHITECTURE.md. Network only through `net::get` (src/net/Http.h), every outside
  host declared with a `sources::Registrar` entry (reuse existing ids "yahoo", "forexfactory", "nasdaq-rss"… by
  declaring the same id again with page "charts/charts" — declare() merges). Never block the GUI thread.
- Durable data under `paths::data("charts")` (the JS `data/` folder: same file names, same JSON shapes).
  Custom indicators folder: `paths::data("charts/custom")` (the JS `custom/`), seeded with
  quant-terminal/custom/ema-ribbon-squeeze.js on first run (embedded resource).
- API keys (Alpaca keyId+secret+feed, Polygon apiKey, OANDA token+account): the JS reads `data/feeds.json`; the
  port reads the same file from `paths::data("charts")` so it stays identical. Never print or log a key.
- Tests: `tests/charts/CMakeLists.txt` (separate project; see its header). Build in YOUR OWN build dir
  (`build-tests-<yourname>`), never in `build/` (the app build) while other agents work. Port the matching
  test/*.test.js cases to C++ with the same inputs and expected values.
- Do not touch files owned by another module unless the brief says so. If a contract header lacks something you
  need, add a declaration (never change existing ones) and say so in your report.
- No git (the repo has none). No commits.

## Wave 2 (UI, alerts, local API) — rules
Done in wave 1 (read the headers, they are the API): src/charts/core/{Types,Indicators,Series,Backtest,Quant,Seasonals,
CustomJs}.h, src/charts/data/{Fetch,Providers,Feeds,LiveFeed,Store,Imports,MarketInfo}.h, chart engine
src/pages/charts/plot/{Plot,Primitive,TimeScale,PriceScale,Series,Chart,Format}.h (Lightweight-Charts-like API; see the
engine demo tests/charts/plot/plot_demo.cpp for usage). CustomJs runs on QuickJS-ng (third_party/quickjs-ng, ES2023).
UI contracts (binding, add members but don't change existing ones): src/pages/charts/ChartsApp.h, ChartPane.h, Panels.h.
- Build and look at your work with the stand-alone harness tests/charts/app (read its CMakeLists.txt and main.cpp), in
  YOUR OWN build dir build-charts-<you>, always with DW_PROFILE=<your scratch dir> (never the user's settings/data).
  Never build in build/ (the app); the integrator does that.
- The harness compiles every file of src/pages/charts and src/charts: keep YOUR files compiling at every moment
  (write a compiling skeleton of your declared functions first, then fill in). If another agent's file breaks the
  build, wait a few minutes and retry; never edit it. If you are blocked >15 min, add the minimal missing definition in
  a file named after you (e.g. src/pages/charts/_stub_<you>.cpp) and delete it before you finish.
- Compare with the original continuously: run it offline
  `cd quant-terminal && QT_OFFLINE=1 QT_DATA_DIR=<scratch>/qt-data PORT=<your port 38xx> node server.js &` (kill it by
  PID when done) and shoot it with `node tests/charts/tools/refshot.mjs http://127.0.0.1:<port>/ out.png 1600 1000 6000
  [script.js]` (script.js = JS run in the page before the shot, e.g. click a tab). Shoot the port with
  `charts_app --offline --size 1600x1000 --screenshot out.png [--eval "..."]`. Look at both PNGs (Read tool), iterate.
  Put the final pairs in docs/charts/shots/<area>/ (orig-*.png, port-*.png).
- Look: same layout, sizes, spacing, texts, icons (the SVG paths from the JS, rendered with QSvgRenderer), states and
  behaviour as the original; colours and fonts from Deepwater's theme (theme::pal(), theme::map(), theme::mono/sans).
  Map quant-terminal's CSS variables (public/css/app.css :root and [data-theme=light]) to palette roles once, in
  src/pages/charts/Style.{h,cpp} (owned by the page-shell agent; others read it and may ask for additions by adding a
  function there in a clearly separated block). Chart series colours from the JS (#2962ff etc.) stay literal.
- English strings exactly as in the JS. Use QCoreApplication::translate("charts", "...") (never tr() in namespaced
  classes) so French can be added later.
- No PTQ anything (no PTQ tab, no "Send to PTQ"/PTQ check buttons, no ribbon, no Help link).
- Never block the GUI thread; timers stop in hideEvent and restart in showEvent (the page is hidden when another
  Deepwater tab is shown).

## Additions beyond quant-terminal

- **Navigator** (2026-11-18, user's spec): a 36 px range slider under every chart pane. Mini curve of the shown closes
  over the whole loaded history, the chart's visible range as a window (drag inside = pan, press outside = centre
  there and keep dragging, handles = horizontal zoom, min 10 bars, never outside the data). Single source of truth:
  the chart's TimeScale visible logical range (the navigator reads it on each range-change notification and writes it
  with setVisibleLogicalRange). Rules: src/charts/core/RangeWindow (tests/charts/core/range_window_tests.cpp); widget:
  src/pages/charts/pane/Navigator; wiring in ChartPane (fresh view constants kFreshBars / kFreshRightPad,
  minBarSpacing 0.05 so the whole history fits). UI step `nav:` (press/move/release/hover/drag/log/perf).
