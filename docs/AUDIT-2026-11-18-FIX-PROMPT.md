# Prompt for Claude Opus — fix the findings of the Deepwater audit of 2026-11-18

Copy everything below the line into a new Claude Code session opened in `/Users/raphael/dev/deepwater`.

---

You are working on **Deepwater**, a C++20 / Qt 6.12 desktop market terminal (macOS first, Windows port in CI), in
`/Users/raphael/dev/deepwater`. A whole-application audit was done on 2026-11-18 in two passes: the full app built
with ASan + UBSan and run through every page (two configurations) and a Charts order-flow scenario; the clang static
analyzer over all 262 source files; a leak measurement at exit; six read-only code reviews by area, then ten more that
read every file line by line (coverage table at the end). Your job is to **fix the findings listed here**, in the order
given, and prove each fix. Batches 1–5 are the first pass; **"Second pass — additional findings" below adds to each
batch (ids M10+, S8+, D14+, G8+, P7+, U1+): do a batch's first-pass and second-pass items together.**

Every finding below was confirmed by reading the code path (or by a sanitizer run, where marked **ASan**). Line numbers
are those of 2026-11-18: re-locate each one before editing, the code may have moved. If a finding turns out to be wrong
when you read the current code, do not "fix" it: say so in your report with the reason.

## Rules of this project (binding)

- **Find and read code with CodeGraph first** (`mcp__codegraph__codegraph_explore`, or `codegraph explore "<names>"` in
  the shell). A hook blocks grep/Bash on the repo until one CodeGraph call has been made (then allowed for 20 minutes).
- **Never touch the user's real data or preferences.** Every run of the app or of a tool uses a scratch profile:
  `DW_PROFILE=<scratch dir> DW_CACHE_DIR=<scratch dir>`. Screenshots: `tools/shot.sh <page id> <out.png> [delay ms] [size] [build dir]`.
  Never print API keys or the content of the preferences.
- **No git commit, no push** unless the user asks. Nothing is committed at the moment; do not stash or reset anything.
- **No new dependency.** Follow the conventions of the surrounding code (comments in English that state the why,
  `QCoreApplication::translate("<context>", …)` for user-visible strings, French translations in `translations/*.ts`).
- The user is French-speaking: **write your reports to the user in French**, plain language, results first.
- Work **step by step**: one batch (below) at a time; after each batch show what was fixed, the tests and the
  measurements, then wait for the user's go-ahead ("feu vert") before the next batch.
- Third-party code (`third_party/`) is out of scope.

## How to build and verify

- App: `cmake --build build` (after adding a `Q_OBJECT` header: `rm -rf build/Deepwater_autogen && cmake -S . -B build`).
- Tests (pure and services): `cmake --build build-tests-flow && ./build-tests-flow/charts_core_tests && ./build-tests-flow/charts_data_tests`
  (261 and 120 tests pass today). Other suites live in `tests/technicaldata` and the `build-tests-*` directories.
  Test harness: `tests/charts/Check.h`; allocation counter: `tests/charts/AllocCount.h`.
- Sanitizers: `build-tests-flow-asan` (ASan + UBSan; 5 Pine tests fail there with "Maximum call stack size exceeded" —
  known, unrelated), `build-tests-flow-tsan`, and the whole app in `build-asan`
  (`cmake --build build-asan -j 10`). Timing checks are skipped under sanitizers (see `DW_SANITIZED` in
  `tests/charts/core/liquidation_tests.cpp`).
- **The two sanitizer runs that must end with no report after your fixes** (S = a scratch directory):
  ```sh
  export ASAN_OPTIONS=detect_leaks=0:log_path=$S/asan UBSAN_OPTIONS=print_stacktrace=1:log_path=$S/ubsan
  B=build-asan/Deepwater.app/Contents/MacOS/Deepwater
  # 1. every page
  DW_PROFILE=$S/p1 DW_CACHE_DIR=$S/c1 $B --lang fr --size 1600x1000 --layout-audit $S/layout --audit-sizes 1600x1000 --audit-load 6000
  # 2. Charts order flow (today this one aborts in HeatmapLayer::draw: finding M1)
  DW_PROFILE=$S/p2 DW_CACHE_DIR=$S/c2 DW_FUTURES_COLLECT=1 DW_CHARTS_STEPS="symbol:BINANCE:BTCUSDT;wait:4000;tf:1m;wait:4000;heat:add;big:min:0;smc:add;liq:add;wait:12000;type:footprint;wait:6000;tf:5m;wait:6000;symbol:BINANCE:ETHUSDT;wait:8000;symbol:BINANCE:BTCUSDT.P;wait:8000;heat:replay:50;wait:4000;tf:1h;wait:5000;type:candles;wait:3000;layout:4;wait:6000;layout:1;wait:4000;symbol:AAPL;wait:6000;symbol:BINANCE:BTCUSDT;wait:5000;theme:light;wait:3000" \
    $B --page charts/charts --size 1600x1000 --screenshot $S/charts.png --delay 120000
  ```
  Wrap launches in a timeout (`perl -e 'alarm 200; exec @ARGV' …`). Result on 2026-11-18: run 1 clean on all 29
  pages (French 1600×1000, and English / light theme / 1024×600); run 2 clean when `heat:add` is left out.
  `leaks --atExit` on the normal build: ~29 KB in ~450 small blocks for both scenarios (no large leak at exit).
- Static analyzer (optional re-run): `clang --analyze` with the flags of `build/compile_commands.json`; its 54
  `NewDeleteLeaks` reports on `std::function` fields are false positives of the analyzer with libc++; the real ones
  are in M10 below.
- For every fix: add or extend a test that fails before the fix and passes after it whenever the code is testable
  without the GUI (pure code in `src/charts/core`, services with the fake feeds in `tests/charts/data`). For GUI-only
  fixes, verify in the running app on a scratch profile and say exactly what you did and saw. Never claim a fix
  verified if you did not run it.

## What the audit found clean (do not re-audit, do not refactor)

Http engine (context-guarded callbacks, merge, redirects), sources registry, settings/paths isolation; feed parsing
and link opening (`ui::openWebLink`); SPSC ring (`Spsc.h`) and DepthStream threading/shutdown; `BookSync`, `OrderBook`,
`TradeTape`, `BinanceMsg` scanners, heatmap readers; QuickJS value ownership in `CustomJs.cpp` / `Pine.cpp`; ApiServer
write rules; Alerts; Store; Backtest engine; liquidation model; primitive lifetime in the Charts flow layers
(`Series::~Series` detaches); load tokens in ChartPane; persistence debounce; technical-data store threading; macro
parsers; GL resource handling; MCP server (127.0.0.1, token, Host/Origin); chat view (no HTML); journal writes
(QSaveFile); statistics and importers; bridges; risk calculator; timers pausing on hidden pages except where listed.
The items of `docs/AUDIT-2026-11-16.md` were re-checked and are still fixed.

---

# Batch 1 — memory errors and crashes

**M1. Use-after-free in the order-book heatmap (ASan, reproduced)**
- `src/pages/charts/flow/HeatmapLayer.cpp:129-130` (lambda `slice` in `HeatmapLayer::draw`), read at `src/charts/core/HeatmapRender.cpp:87`.
- `merged_` is a `std::vector<DepthRecord>`; `recs_` receives `&merged_[k]`. When one slice needs a second merged group
  and `used == merged_.size()`, `merged_.emplace_back()` reallocates and the pointer pushed for the first group dangles;
  `rasterCodes` then reads freed memory. ASan: `heap-use-after-free … rasterCodes HeatmapRender.cpp:87 ← HeatmapLayer::draw HeatmapLayer.cpp:223`,
  freed by the vector's growth. Reproduced by sanitizer run 2 above (a slice spanning a band re-anchor).
- Fix: references must survive growth — make `merged_` a `std::deque<DepthRecord>` (stable on `emplace_back`), or keep
  indexes during the loop and resolve them to pointers after it. Keep "no allocation per frame in steady state".
- Proof: sanitizer run 2 completes with no report; add a pure regression test if you move the merge into
  `src/charts/core` (recommended: a function `mergeSlice(ring range) → records` testable with records alternating
  between two bands).

**M2. Crash in Data › Macro data: select a block, then "✕ UNTICK ALL"**
- `src/pages/data/MacroDataPage.cpp:1064-1074` (`reloadLanes`) and `:1099-1101` (`inspect`).
- `sel` is read before `setLanes()` and passed to `inspect()`; line 1100 evaluates `m_canvas->lanes()[lane]` before the
  range test of line 1101. With no lane left: read through a null data pointer.
- Fix: read the selection after `setLanes()`; in `inspect()` do the range test first.
- Proof: in the app (scratch profile): click a block, UNTICK ALL, no crash; also under `build-asan`.

**M3. Use-after-free: `ind::byId()` hands out a raw `Def*` that a reload destroys**
- `src/charts/core/IndicatorsDefs.cpp:1288-1292` (lock released, pointer returned), `:1335-1341` (`registerIndicator`
  erases the old Def). Worker-side readers: `src/charts/core/Quant.cpp:276-287`, `Backtest.cpp:182-184`.
- Scenario: a backtest / quant check runs on a worker with a `pine:` or custom indicator while the user saves that
  file (or `POST /api/custom`): the GUI thread frees the Def the worker is reading.
- Fix: store `std::shared_ptr<const Def>` and return it (or a copy) from `byId`; `all()` must return by value.
  Update the callers. Run the TSan build of the core tests; add a test that re-registers an id while another thread
  computes with it.

**M4. Indicator parameters allocate up to 8–16 GB (process killed)**
- `src/charts/core/IndicatorsDefs.cpp:598-601` (rci) and `:835-838` (trendstrength): `for (k < n) time << k + 1` with
  `n = I(p, "length")` unbounded; `src/charts/core/Indicators.cpp:403-406` `shiftForward` extends by any `k`
  (callers: ichimoku displacement `IndicatorsDefs.cpp:896-902`, alligator offsets `:914-916`, dpo `:722-726`).
  `lengthOf` (`Indicators.h:146`) allows ±1e9 and maps NaN to `1<<30`. Reachable from pane settings, the local API
  (`/api/backtest`, `/api/scan`), alerts and AI tools. Same family, CPU only: fisher `length: -1e9` loops 1e9 times (`:549`).
- Fix: clamp lengths and offsets once in `compute()` (`IndicatorsDefs.cpp:1304-1318`) to `bars.size() + 1`, keeping
  `kNaNLength` as "never fills"; cap `shiftForward`. Results for sane parameters must not change: the reference tests
  (`reference_tests.cpp`, `pine_reference_tests.cpp`) must still pass. Add tests with absurd parameters (NaN, 1e9, −1e9).

**M5. Out-of-bounds read in the euro-area yield curve parser**
- `src/macro/YieldCurves.cpp:116-120`: `ik/it/iv = indexOf(…)` unchecked; with the three headers missing,
  `rows[i].size() <= -1` is false and `rows[i][-1]` is read. Trigger: ECB answers HTTP 200 with something that is not
  the CSV. Fix: `if (ik < 0 || it < 0 || iv < 0) { fallback("ez"); return; }`.

**M6. Stale hover indexes after a list changes (out-of-bounds reads)**
- `src/pages/data/MacroCube.cpp:37-50`: `setData` does not reset `m_hoverS` / `m_hoverC` (used at `:148-150`). Reset
  them and call `onInfo({})`.
- `src/pages/news/calendar/CalendarTable.cpp:82-91`: `setEvents` does not reset `m_hoverItem`, read at `:231` when a
  row is pinned. Reset in `setEvents`, and bound-check at 231.

**M7. Division by zero / signed overflow on a malformed exchange price**
- `src/charts/core/Ticks.h:36-64` (`parseDecimal` lets `exp` fall without bound) and `:77-88` (`ticksOf` multiplies
  `den` by 10 `-e` times: `__int128` overflow, then `num / den` with `den == 0`). Fix: `parseDecimal` fails when the
  exponent leaves ±30; `ticksOf` returns a failure value instead of dividing. Test with `"0." + 130 zeros + "1"`.

**M8. Possible null dereference when `claude` fails to start**
- `src/ai/Engine.cpp:550-554`, `:558-560`: the `errorOccurred` handler deletes `m_cc` on FailedToStart; if Qt reports
  it inside `start()`, the next `m_cc->write(…)` dereferences null. Fix: `if (!m_cc) return;` after `start()`.

**M9. Undefined behaviour in two sorts and conversions**
- `src/pages/positioning/CotPanels.cpp:533`: comparator on `abs(net − prev)` is not a strict weak ordering with NaN.
  Map non-finite keys to −1 as the `byExt` comparator above does.
- `src/pages/positioning/RetailModal.cpp:270`, `:403` (and the split hover box in `CotPanels.cpp`): `std::lround(NaN)`.
  Show a dash when not finite.
- `src/charts/core/Series.cpp:176-181` `parseTf` accepts any digit count → +inf → NaN→qint64 in `bucketStart`, and
  `limit * ratio` overflows `int` in `src/charts/data/Providers.cpp:167/373/466/504`, `Feeds.cpp:227-370`,
  `Resampler.cpp:30`. Reject counts above ~1e6 in `parseTf`.
- `src/charts/core/LiqRender.cpp:16`: `end - size_t(maxBars)` wraps for a negative `maxBars`; clamp once.
- `src/charts/data/Imports.cpp:222-226`, `:286`, `:300`: oversized CSV timestamps cast to qint64 unchecked; return a
  row error outside ±8.64e12 s.

# Batch 2 — security and untrusted input

**S1. The unsandboxed app follows symlinks inside folders its sandboxed children can write**
- `src/ml/Runs.cpp:186-187` (`log.txt` appended), `:95-97` (`run.json`), `src/ai/MlTools.cpp:583` (`ml_run_rows`
  returns rows of `<run>/predictions.csv` to the model), `src/ai/Engine.cpp:94` (the runner's profile grants
  `file-write*` on the whole environment folder), `:289`, `:463`, `:480`, `src/ai/McpServer.cpp:66`.
- A compromised trainer or model runner can plant a symlink where the app later writes or reads (append to
  `~/.zshrc`; leak a private file's last 200 lines to a model).
- Fix: drop `file-write*` on the environment folder from the runner profile (`helpers/llm/main.cpp` writes no file —
  verify); keep `log.txt` / `run.json` outside the trainer-writable folder (`runs/<id>/` for the app,
  `runs/<id>/work/` for the trainer); before reading or writing anything in a sandbox-writable folder refuse symlinks
  (`QFileInfo::isSymLink()` + canonical path under the folder). Existing runs must still open.

**S2. Untrusted text rendered as rich text**
- `src/ui/Kit.cpp:154-159` (`ui::label` leaves QLabel on AutoText) and QMessageBox. Inputs: a run name chosen by a
  model (`src/ai/MlTools.cpp:358-362` → `src/pages/ai/MlPage.cpp:936`, ~`:1284`, ~`:1330`), a bridge file's account
  name (`src/account/Importers.cpp:469-480` → `src/pages/account/TrackRecordPage.cpp:543-546`), Hugging Face licence
  and file names (`src/pages/ai/AiModelsPage.cpp:321`, `:341`, `:362-363`, `:493`), stderr tails
  (`src/ai/Engine.cpp:233-236`, `:639-640` → `ChatView.cpp:349`).
- Fix: `setTextFormat(Qt::PlainText)` by default in `ui::label` and the text helpers, RichText only where the app
  itself writes the HTML (audit every caller that relies on HTML: e.g. `IntegrationPage.cpp:77`, the Macro data
  inspector); plain text in those message boxes; strip `<` `>` in `cleanField` and in run names. Check the pages
  visually afterwards (a label that silently showed HTML will now show tags).

**S3. Model downloads trust names and redirects** — `src/ai/ModelHub.cpp:136-138` (`modelsDir()/repo/file` from API
JSON; used at `:173`, `:194`, `:258`, `:286`, `:374`), `:228-232`. Reject `.`/`..`/absolute/backslash components and
check the canonical parent; cap redirects at 5; require `https` before following and before attaching the token.

**S4. Requests of any scheme are sent** — `src/news/Feed.cpp:422-423` (feed image URLs), `src/net/Http.cpp:249-257`
(first hop unchecked; redirects are checked at `:302-304`). In `Engine::enqueue` refuse anything but http/https.
Also `src/pages/economy/RatesPage.cpp:703`: `QDesktopServices::openUrl` on a URL from the Fed RSS → use `ui::openWebLink`.

**S5. Local Charts API: GET routes accept cross-site requests** — `src/charts/data/ApiServer.cpp:548-570`. A web page
can make Deepwater send provider requests in a loop (`/api/candles`, `/api/quotes`, `/api/probe`). Refuse requests whose
`Sec-Fetch-Site` is `cross-site`, or whose `Origin` is present and differs from Host, for GET as well. Keep the
existing tests (`tests/charts/data/server_tests.cpp`) green and add cases.

**S6. Code injection into a generated indicator file** — `src/charts/data/Imports.cpp:455-459`: only `\r\n` are
replaced before the file name goes into a `//` comment of `custom/signal-<name>.js`; U+2028/U+2029 start a new code
line. Emit the name through the JSON string writer or strip `\p{C}\p{Zl}\p{Zp}`. (Impact limited to the QuickJS
sandbox.) Related: `src/charts/core/CustomJs.cpp:669-680` lets a custom indicator `import` any absolute or `../`
path — confine imports to the custom folder.

**S7. Limits missing on inputs from other programs**
- `src/account/BridgeSync.cpp:64-67`: bridge files read whole on the GUI thread without the 64 MB cap that
  `importFile` has (`Importers.cpp:547`); skip non-regular files.
- `src/ai/McpServer.cpp:74-93`: body buffered (1 MB) before the token check, no idle timeout: check path and
  Authorization as soon as the headers are complete; 30 s idle timeout. `:150-158`: a refused call still writes a
  "TOOL ✕" line into the conversation — reply without `recordTool`.
- `src/ai/Engine.cpp:242` (`m_buf`), `:536` (`m_ccBuf`), `src/ml/Runs.cpp:179`: no cap when a child never sends a
  newline; fail the process above ~8 MB.
- Platform: `src/ai/Engine.cpp:173-177` starts the runner with no sandbox outside macOS while
  `src/pages/ai/ChatView.cpp:357-361` still says "no internet, no access to your files"; `src/ml/Runs.cpp:225`
  hard-codes `/usr/bin/sandbox-exec`. Refuse to start unsandboxed or make the text true per platform (ask the user
  which, the Windows port matters to them).

# Batch 3 — wrong or lost data

**D1. Leaving bar replay leaves a permanent hole in the chart** — `src/pages/charts/ChartPane.cpp:2099-2107`
(`stopReplay` only does `render(true); startLive()`), `:1969-1984` (`merge` appends any newer bar whatever the gap).
After 20 minutes of replay on 1m, 20 bars are missing and one bar stays half-built; indicators, SMC levels and the
liquidation map compute across the hole. Fix: in `merge`, when the first new bar is more than one step after
`bars.last()`, call `load(true)` (also covers socket reconnects); in `stopReplay`, reload when a bar boundary has passed.

**D2. With a compared symbol on another calendar, the wrong bar is used everywhere** — `ChartPane.cpp:1397`,
`:1541-1582` (`logicalOfF` / `timeOfLogicalF` / `timeToXF`), `:1589-1593`, `:1680-1683`; `plot/Chart.cpp:319-346`.
The logical index is on the union time scale but is used as an index into `shown` / `times` (AAPL 1D compared with
BTCUSDT: weekend bars shift everything — legend values, drawings, SMC levels, replay pick, synced crosshair). The
Navigator already converts (`ChartPane.cpp:1415-1420`). Fix: take the bar from the time (binary search in `times`),
convert through the time scale's points when their count differs from `times.size()`.

**D3. One heatmap's settings delete another's recordings** — `ChartPane.cpp:479` + `src/charts/data/DepthStream.cpp:36-43`.
Every render of every heatmap calls `stream->record(on, keepDays)` on the market's shared stream, and `record` prunes
as soon as `keepDays` decreases, even with `on == false`. Pane A "keep 30 days" + pane B default 7 → days 8–30
deleted. Fix: each layer registers its wish (`{record, keepDays}`) on attach and removes it on destruction; the stream
uses any(record) and max(keepDays); never prune from a call with recording off. Test in `depthstream_tests.cpp`.

**D4. Replay leaks the future** — `src/pages/charts/flow/LiqMapLayer.cpp:120-147`: real-liquidation dots are drawn
after the replayed bar (cut at the replayed bar's end, as `HeatmapLayer.cpp:99` does). `ChartPane.cpp:352` and `:419`:
the cut for SMC and the liquidation map uses `shown.size()`, wrong on Renko / Kagi / P&F / line break / range charts —
use `q->bars.mid(0, q->replay->index + 1)`.

**D5. Replay state machine** — `ChartPane.cpp:1619-1621`, `ChartsApp.cpp:822-826`, `:1746`: a leader that reloads or
is deleted leaves its followers frozen in "Synced replay" (call `app->onReplayStop(this)` from `load()`'s success path
and `~ChartPane` when it led). `ChartPane.cpp:2025-2027`: `startReplay` on a follower keeps the follower tag (remove
it first; `setPlaying(false)`).

**D6. Macro facts dated 1970-01-01 are never stored** — `src/macrodata/MacroDataStore.cpp:20`, `:101`, `:126-127`:
epoch 0 is the "no period" sentinel but is a real period (the 1970 point of every annual chain, January 1970 of every
monthly one). Test `o->period.isValid()` instead; on the read side treat 0 as "none" only for `target` and `at`
(`fromMs()` `:44`, `dateText()` in `MacroDataPage.cpp:93-95`). The next import pass adds the missing facts.

**D7. False MOVERS wire items at 00:00 UTC** — `src/wire/Wire.cpp:419-425`: the "already emitted" level is keyed
`symbol:yyyyMMdd`; at UTC midnight every instrument still beyond its threshold emits again, archived and fed to
Session Recap. Key on the symbol; drop the entry when the level returns to 0; rebuild `m_level` at start from the
newest mover per symbol (`:195-203`).

**D8. A partial COT history is cached as good for 6 hours** — `src/positioning/Cot.cpp:229-235`, `:250-254`: when the
second report fails, the legacy-only history has `ok() == true`; the COT page shows empty panels with no message.
Cache only when `ld.error.isEmpty()`; carry a `warning` and show it at `src/pages/positioning/CotPage.cpp:276`.

**D9. Order-flow data shifted or stalled**
- `src/charts/data/DepthStream.cpp:168-174` (`exchangeNow`): when the PC clock is ahead of Binance, every column of
  the session is stamped that much late (the first sample uses the local clock, then `lastSample_ + step` forever).
  Produce no column (or a Missing one) until the first event time is known.
- `src/charts/data/TradeStream.cpp:184-188`, `:207-214`, `:225-229`: a large gap (laptop asleep 8 h) is refilled
  oldest-first without budget — most pages are dropped again by the tape and the recent trades arrive last; and a gap
  Binance refuses (HTTP 400) is retried forever and blocks history. Fill from the newest end, trim the gap to what
  the tape can keep, forget a refused gap. Tests with the fake exchange in `tradestream_tests.cpp`.
- `src/pages/charts/flow/BigTradesModel.cpp:122-123`: Big Trades stay empty for good when the REST history never
  lands (first rebuild on an empty tape leaves `lastId_ < 0`). In `addNew`, schedule a rebuild when
  `lastId_ < 0 && !tape.empty()` (as `FootprintFeed.cpp:49-51`).
- `src/charts/data/BinanceFeed.cpp:275-297`: the tick-size request has no host fallback — where api.binance.com is
  blocked, no order flow ever starts although the mirror works. Try the other base and remember it.
- `src/charts/data/LiveFeed.cpp:70-109`: the kline socket has no ping / staleness watchdog (charts freeze after
  sleep); reuse BinanceFeed's ping logic.

**D10. Sources that fail are not asked again**
- `src/macro/MacroStore.cpp:277-281`: `due()` stamps `m_started` before the request and the error paths (`:314`,
  `:328`, `:350`, `:364-367`, `:386`, `:504`) never clear it: a data set that failed stays empty 12–24 h; and
  `src/macrodata/Importer.cpp:192`, `:207`, `:284` still marks the pass done. Clear the stamp (or a 5 min retry) on
  failure; don't call `done(f)` when the awaited source is not ready.
- `ChartPane.cpp:1608-1613`, `:2132`: a failed chart load is never retried (app started offline). Single-shot retry
  with backoff, cancelled by the load token; reload on show when `bars.isEmpty()`; clear `provider` / `notes`.
- `src/charts/data/Providers.cpp:810-814`: one failed Nasdaq screener call stores an empty ticker set for the
  session (US tickers then resolve to crypto). Store only when `r.ok()`.
- `src/news/Feed.cpp:359-363`: a feed served from stale cache after a failure records no error. Record one.
- `src/pages/news/feed/FeedWidgets.cpp:87-109` + `src/news/Feed.cpp:435-436`: thumbnails stuck on the placeholder
  after a theme/language change (the static `pending` set is only cleared in a callback skipped when the widget
  died; `failed` never expires). Load with a long-lived context, expire `failed` after ~5 minutes.

**D11. Stale data shown as current**
- `src/market/QuoteHub.cpp:44-54`: a page coming back shows prices from its last visit for up to 20 s (the code
  requests only symbols with no quote at all; the comment says otherwise). Request what is older than one interval.
- Risk index never refreshed while a page stays open (`src/pages/global/overview/OverviewData.cpp:126-135`,
  `src/pages/positioning/RiskSentimentPage.cpp:597`): 30-minute timer while visible.
- `src/pages/positioning/CotPage.cpp:247-251`: COT loads once per page lifetime; reload on show when older than 6 h
  or failed.
- `src/news/History.cpp:174-185`: release-history chart never refreshes in a session; honour the TTL or drop on
  `Calendar::actualArrived`.
- `ChartPane.cpp:2016`, `:1686`, `:2192-2196`: the legend jumps to the last bar every 1.5 s while the crosshair rests
  on an older one — `updateLegend(d->hoverIndex)`, reset it in the legend-hover branch.

**D12. Look-ahead outputs usable in rules** — `src/charts/core/IndicatorsDefs.cpp:900` (ichimoku lagging),
`:725-726` (dpo centered), `Indicators.cpp:817-826` (zigzag): a backtest or alert rule can read future bars
(`Backtest.cpp:188-189`). Flag these outputs draw-only and make `resolveSeries` refuse them with a clear message.

**D13. Files and state that can be lost or corrupted**
- `src/charts/data/FuturesStats.cpp:151-152`, `:247`: rewrite by `QFile::remove` + write — a crash in between loses
  the open-interest history kept past Binance's 30 days. Temp file + rename.
- `src/charts/core/HeatmapStore.cpp:106-147`, `src/charts/data/DepthStream.cpp:355`, `:376-379`: two Deepwater
  instances on one data folder overwrite each other's depth records (and duplicate FuturesStats points): a QLockFile
  per market folder. Flush results are ignored (disk full misaligns the hour). An existing file's header is not
  validated before appending and `loadRange` ignores the header's tick (wrong prices after a tick-size change).
- `src/pages/news/bank-research/PdfPanel.cpp:466-467`: "Save a copy" deletes the existing file before copying,
  result unchecked. Copy to `.part`, rename on success, report failure.
- `src/ai/ModelHub.cpp:314`, `:273-277`, `:322`, `:339`: `finish()` always clears `m_activeKey` — after model A
  finishes verifying while B downloads, PAUSE on B does nothing and CANCEL deletes the `.part` without aborting the
  reply (a multi-GB download runs to the end into an unlinked file). Clear only when `key == m_activeKey`; call
  `startNext()` on the verify-finished early return.
- `src/ai/Engine.cpp:311-351`, `:314`, `:226-239`: a tool round is not tied to the process/question that started
  it (dropping another model on the cube during a tool wait interleaves requests; Failed replaced by Off). Capture a
  generation counter in the round.
- `src/pages/data/PairDialog.cpp:216`: a forex pair's first day cannot be set before 2017 although Dukascopy starts
  in 2003. Widen the minimum per instrument.
- `helpers/llm/main.cpp:65`, `:137`: a "stop" read just after its chat line is discarded. Give stop the request id.
- `src/ai/Engine.cpp:347`, `src/ai/McpServer.cpp:166` vs `src/ai/BacktestTools.cpp:301-311`, `:335-428`: a
  backtest longer than 30 s loses its answer and keeps `g_busy`. Longer limit or a job id to poll.

# Batch 4 — unbounded growth (disk, memory) and request storms

**G1. The HTTP disk cache is never pruned** — `src/net/Http.cpp:47-49`, `:104-119`, `:285`. Every successful answer
is written when `ttlSec > 0 || staleOnError` (default true); nothing deletes by age or size. Measured on the user's
machine: 390 MB in 3,251 files in under 7 days. Add `prune()` on the DiskCache pool thread at start-up and every 24 h:
delete files older than ~30 days, then oldest-first above a cap (1 GB); skip paths in `pending`.
- Main feeder: `src/market/QuoteHub.cpp:59-64`, `:78` — every 20 s quote poll is rewritten to disk (QSaveFile syncs)
  under cache keys that change at each launch (chunk URLs built from a `QSet` in iteration order). Set
  `r.staleOnError = !force` and sort the symbols before chunking.
- `src/pages/market/watch/CandleChart.cpp:136-141`: the Watch sheet's chart never unsubscribes its previous symbols
  from the quote hub (60 instruments browsed → 60 symbols polled for the session). `unsubscribe(this)` first.

**G2. Depth recordings of markets no longer opened are never deleted** — `DepthStream.cpp:355`: pruning runs only for
the market whose stream starts; recording is on by default (~91 MB/day per market). Prune every folder under
`charts/depth/` at start-up with the saved retention.

**G3. Request loops**
- `src/charts/data/FuturesStats.cpp:161-163`: on HTTP 400 the start is moved to `now() − 29 days` and `next()` runs
  at once; on the next 400 `now()` has advanced so the condition is true again — back-to-back requests forever
  (delisted or non-futures symbol), then 429/418. Allow the correction once per job, otherwise back off; 418 must wait
  minutes (`:165`). Test with the fake Binance in `futuresstats_tests.cpp`. `liq_` / `liquidations.dat` also grow
  without limit: cap or archive by age.
- `src/charts/data/DepthStream.cpp:100-101`: "snapshot too old" re-asks every second outside the backoff (spot: weight
  250 each). Count it in the backoff (≥ 3 s, doubling).

**G4. Technical-data indicators: memory spike and exit hang** — `src/technicaldata/TechnicalDataIndicators.cpp:216-243`,
`CollectorStartup.cpp:20-24`: pieces of 200,000 rows are queued to the writer with no backpressure (a full BTCUSDT 1m
recompute can hold hundreds of MB to ~1 GB), and at quit `Store::wait()` blocks the GUI until everything is written.
Call `m_store->wait()` inside the loop so one piece is in flight. `:145-149`, `:206-207`: moving a pair's first day
earlier recomputes the whole later history after every backfill page (`Collector.cpp:238-260`,
`ForexCollector.cpp:186-191`) — don't schedule while a backfill is in progress, schedule once at its end. Tests in
`tests/technicaldata`.

**G5. The ML skill can fill the disk and hold the trainer** — `src/ml/Datasets.cpp:255-257`, `src/ai/MlTools.cpp:214`,
`:285-374`, `src/ml/Runs.cpp:160`: a looping model creates a new large CSV per distinct `rows` value, each training
copies the set, no time limit. Snap `rows` to the page's fixed choices; cap the datasets folder; reference instead of
copying; wall-clock limit for AI-started runs.

**G6. Slow growth** — `src/pages/news/feed/FeedPage.cpp:141` (reading queue in the preferences never pruned; drop ids
`Feed::find` no longer knows); `src/wire/Wire.cpp:106-107`, `:307`, `:327-338` (`m_days` / `m_ids` keep every archive
day read); `src/pages/news/squawk/SquawkWidgets.cpp:347-353` (`m_heights` keyed by width, cleared only by
translations); `src/ai/Engine.cpp:386-465` (conversation never trimmed, rewritten whole on every event);
`src/ai/BacktestTools.cpp:200-215`, `:432-446` (ledgers never rotated); `src/macrodata/Importer.cpp:370-371`,
`:479-481` (self-referencing `step` closure leaks per pass: reset it as the Collector does);
`src/pages/data/MacroDataPage.cpp:216-231` (lanes capped by count not by facts: TICK ALL on prices can hold hundreds
of MB — evict by total facts).

**G7. Processes and threads at quit** — `src/ml/Environment.cpp:110-141` (a running `pip install` survives the app:
kill it on `aboutToQuit`); `src/ml/Runs.cpp:56`, `:195-213` (a run killed at quit is saved "Failed" instead of
interrupted); `src/macrodata/MacroDataStore.cpp:519` (writer thread `wait(10000)` then teardown while it may still be
inside SQLite: wait without timeout or add a stop flag); `src/charts/core/Pine.cpp:579-587` (`runAsync` threads never
joined: track them and wait briefly, or a cancel flag read by the interrupt handler).

# Batch 5 — the GUI thread and work done while hidden

**P1. Pine scripts run synchronously on the GUI thread outside a chart** — `src/charts/core/PineIndicators.cpp:324-340`
(default limits 60 s / 1 GB, `Pine.h:42-43`), `:140-142`. An alert with a `pine:` rule re-runs the script on the GUI
thread every 15 s (`Alerts.cpp:579`; the forming bar defeats the cache); `/api/scan` runs 150 of them; a `.pine`
file with an endless loop freezes every launch for 20 s (`loadAllCustom`). Off-chart GUI callers must use a worker or
a short timeout (5 s) and a memory cap; load `.pine` files on a worker at start-up.

**P2. Order-flow streams and timers keep running while the Charts tab is hidden** — `ChartPane.cpp:2153-2158`
(`hideEvent` stops only the kline feed), `flow/BigTradesModel.cpp:24`, `flow/FootprintFeed.cpp:34-39`,
`ChartPane.cpp:458-462`, `:662-685`. Sockets, the depth thread, footprint folding and series updates run whatever tab
is shown; Big Trades toasts and sounds fire from a hidden tab. On hide: keep the DepthStream only when a heatmap
records, the BigTradesModel only when an alert is set; pause the rest and rebuild on show. This is the rule the other
pages follow ("hidden pages pause").

**P3. Any primitive repaint relayouts and repaints the whole chart** — `plot/Series.cpp:230`, `plot/Chart.cpp:81`,
`:296-300`, `:350-394`: `requestUpdate` = `invalidate()` which also sets `layoutDirty_`; each depth column (100 ms)
runs `ensureLayout` (up to 3 scale passes + font metrics per pane). Give primitives a `requestUpdate` that only sets
`baseDirty_`. Measure before/after with the dev steps `heat:perf`, `fp:perf`, `liq:perf`, `smc:perf`
(baseline 2026-11-18, Retina 1600×1000: heatmap 1.7 ms still / 5.7 ms panning; liquidation map 1.9 / 4.9 ms).

**P4. Heavy work on the GUI thread**
- `src/pages/global/session-recap/RecapPage.cpp:201-213`, `:75`, `RecapModel.cpp:67-80`: Session Recap loads and sorts
  the whole 80-day wire archive at every refresh only to decide a note (and the second test can never be true); the
  model emits `changed()` 12 times per load and overlapping loads share one counter. Cheap existence test; one
  `changed()` per load; a generation counter.
- `src/ml/Datasets.cpp:234-298`, `src/ai/MlTools.cpp:118`, `:169`, `:243-261`, `:291`, `src/ml/Runs.cpp:160`,
  `src/ai/BacktestTools.cpp:157-166`: ML tools parse whole data sets on the GUI thread (`ml_status` describes every
  CSV to count them). Keep a `<id>.meta.json` written at build time; count files; tail-read; move copies and the bar
  load to a worker.
- `src/news/Calendar.cpp:472-485` (called from `History.cpp:164-184`, `CalendarTable.cpp:172`): expanding a calendar
  row re-parses every recorded week file; build an index once on a worker.
- `src/pages/charts/VolumeProfile.cpp:73`: Session Volume Profile recomputes its sessions (a QString key per bar) on
  every frame; cache per bars signature.
- `src/pages/positioning/RetailSentimentPage.cpp:595` + `src/positioning/Retail.cpp:303-312`: the 24 h delta scans the
  whole history per row; binary search.
- `src/pages/news/bank-research/PdfPanel.cpp:138-181`: a window resize starts a full-page PDF render per pixel step
  (not cancellable); relayout at once, render after a 150 ms single-shot timer (same on Ctrl+wheel).
- `src/pages/global/overview/OverviewData.cpp:114-115`, `FeedPanels.cpp:79`, `:167-203`: the Overview wire strip
  (a 14,000 px image) is re-rendered on every `Wire::changed`, with nothing new and while hidden. Return early when
  hidden; rebuild only when the content differs.
- `src/macro/MacroStore.cpp:253-260`, `:321-342`: the deep store downloads the whole BIS daily policy-rate history
  every pass and uses none of it; skip it when `m_deep`.

**P5. Blocking waits and resets the user feels** — `flow/BigTradesModel.cpp:43-47`, `flow/LiqMapModel.cpp:46-50`
(`waitForFinished()` in destructors: removing or hiding an indicator blocks the GUI for the whole run, e.g. a
calibration of 72 model runs; and `ChartPane.cpp:1036`, `:1131-1134` throw a hidden indicator's model away). Hand the
watcher to a detached holder or add a cancel flag; keep hidden indicators' entries alive.
`src/pages/data/Landscape.cpp:56`, `MacroCube.cpp:49`, `MacroDataPage.cpp:674-675`, `:832-834`, `:882-885`: the 3D
views lose camera and slice on every store change, and a reload requested during a load is dropped — reset the camera
only when the row set changes; remember a pending reload.

**P6. Cosmetic** — `src/pages/global/overview/MarketTape.cpp:54`, `:66`, `:99-104` (tape strip built at the default
height, not rebuilt on resize).

---

## What to deliver after each batch

1. The list of findings fixed (id → files changed), and any finding you judged wrong, with the reason.
2. Tests added (name, what they check) and the totals of every suite you ran; the two sanitizer runs when the batch
   touches code they exercise (Batch 1 must end with both runs clean).
3. For GUI fixes: what you did in the app on a scratch profile and what you saw (screenshots when a picture helps).
4. Anything you could not verify, said plainly.

Do not widen the scope: no refactoring beyond what a fix needs, no reformatting, no new features. When a fix needs a
decision that is the user's (marked "ask the user" above, or any change of visible behaviour not described here), stop
and ask.

---

# Second pass — additional findings (2026-11-18, exhaustive line-by-line reviews, static analyzer)

All confirmed by reading unless marked *plausible*. Same rules, same proofs. Do each group with the batch of the
same letter.

## Batch 1 additions — memory, crashes and hangs

**M10. Objective-C++ is compiled without ARC and leaks whole documents (static analyzer + reading)**
- `CMakeLists.txt:20-23` adds `src/*.mm` with no `-fobjc-arc` (confirmed in `build/compile_commands.json`).
  `src/banks/Pdf.mm:74`, `:84` (`PDFDocument` alloc/init never released, per `text()` / `print()`; check the render
  function too), `src/macro/Ocr.mm:11-12` (`VNImageRequestHandler`, `VNRecognizeTextRequest` never released: one
  leak, holding the image, per FinancialJuice rate picture — `CentralBanksMore.cpp:459`). `NativeTv.mm` is correct
  under manual retain/release (it has `#if !__has_feature(objc_arc)` guards).
- Fix: compile the `.mm` files with ARC (`set_source_files_properties(${DW_OBJC} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")`;
  NativeTv's guards then switch off — verify it still builds and the TV pane works), or add the missing releases.
- `Pdf.mm:47`: `int(sz.width() * scale * dpr + 0.5)` on a crop box read from the PDF, unbounded (overflow; a
  zero-width box divides by zero at `:58`). Reject pages whose pixel size is outside 1…16384.

**M11. Endless loops that freeze the app and eat memory (four places, one family)**
- `src/ui/Chart.cpp:16` `chart::niceTicks`: `for (v = …; v <= hi; v += step) out << v` — when the range is a few ulps
  of the price, `step` is below half an ulp, `v` never advances, the vector grows until the process dies. 12 callers.
  Reached from `src/pages/positioning/CandleChart.cpp:85`, `:91` (all visible candles flat: pad is
  `(hi-lo)*0.08 + 1e-12`, lost on prices ≥ ~1,000 — Retail modal on XAUUSD/ETHUSD/an index, COT futures chart) and
  from `src/pages/market/watch/CandleChart.cpp:846-848`, `:541` (price-axis zoom with no floor, ~13 zoom-in drags).
  Fix `niceTicks` itself: iterate on an integer (`n0 = ceil(lo/step)`, `k < 1000`), return when
  `step <= 0 || lo + step == lo`. Then: relative pad in the positioning CandleChart
  (`max((hi-lo)*0.08, max(|hi|,1)*1e-6)`), a floor on the Watch chart's half-range (`|mid|·1e-9`), and the copy
  `niceTicks3` at `src/pages/positioning/RiskSentimentPage.cpp:31-38`. Add a unit test for `niceTicks` with ulp-wide
  ranges.
- `src/pages/charts/plot/PriceScale.cpp:209-220` (`scaleTo`), `:123-160` (`tickSpan`), `:162-197` (`buildTicks`):
  on a **log** price scale, dragging the price axis out four times (each full drag ×6; ×1000 when the pointer goes
  below the pane, because `x` is not clamped at 0) pushes `max_` (a log10) past 308: `pow(10, …)` is inf,
  `tickSpan(inf, …)` loops forever (`e /= h` stays inf). Before that the labels are 60–300 digits and the axis eats
  the plot. Fix: `x = qMax(0.0, x)`, `coeff` in [0.1, 6]; clamp log `min_`/`max_` to about [−15, 18] in `scaleTo`,
  `scrollTo`, `setVisibleRange`; `buildTicks` returns when high/low are not finite and counts iterations, not ticks;
  `tickSpan` returns when `e` is not finite.
- `src/charts/core/Series.cpp:298-311` (renko), `:394-407` (range bars), called on the GUI thread from
  `src/pages/charts/ChartPane.cpp:837`, `:839` with the default box (ATR of the last 14 bars): no cap on generated
  bricks, and `while (true)` never ends when `base + box == base` in doubles (flat last bars, or one absurd print).
  Return empty when `!(base + box > base)`; brick budget (200,000) by raising the box. `:482`: clamp before the
  `int(...)` cast.
- `src/pages/data/MacroDataPage.cpp:306`, `:318-325` (`drawAxis`): with `m_t0 == 0`, `fromMs(0)` is an invalid date
  and the tick loop never ends (unreachable today; one line; and the D6 fix touches `fromMs`). Use
  `QDateTime::fromMSecsSinceEpoch(m_t0, QTimeZone::UTC)`, return when a date is invalid. Same guard:
  `src/pages/global/session-recap/CentreView.cpp:343-347` (`m_w.start.isValid()`).

**M12. Out-of-bounds read in the Watch sheet chart after a resize with Compare on**
- `src/pages/market/watch/CandleChart.cpp:218` (`resizeEvent` → `layout()` only), read at `:659` (`b[i0]`).
  `clampView()` allows `m_right` up to `n-1 + 0.8·vis`; when the plot narrows by more than 20 % (crossing the sheet's
  1180 px layout switch does it in one step, `InstrumentSheet.cpp:414-440`), `i0` reaches `n`. Call `clampView()` in
  `resizeEvent` (as `liquidity/CandleChart.cpp:353` does) and guard the compare block with `i0 <= i1`.

**M13. More undefined conversions and unchecked indexes (memory-safe today only by a later clamp or by luck)**
- `src/pages/charts/ChartPane.cpp:1563-1567` (`timeOfLogicalF`): a NaN logical becomes the index of `T[i]`
  (`INT_MIN` on x86-64). `if (!std::isfinite(l)) return std::nullopt;` — same in `logicalOfF`.
- `src/charts/core/Liquidation.cpp:67-68`, `:77-78` (`consume`): row index range-checked but not NaN-checked (with
  `rowHeight == 0`); return when `!(h > 0) || R <= 0`; `prepareLiq` (`LiqRender.cpp:47-51`) leaves `rows = 0` unless
  `gHi > gLo`.
- Float→int casts clamped only after the cast: `flow/HeatmapLayer.cpp:66`, `:68`, `:82`; `flow/LiqMapLayer.cpp:56`,
  `:58`, `:70`, `:82`; `flow/FootprintLayer.cpp:95`, `:134-135`; `flow/BigTradesLayer.cpp:97-98`;
  `ChartPane.cpp:1397`, `:1591`; `src/charts/core/FuturesData.cpp:25`; `src/charts/core/CustomJs.cpp:370`, `:549`;
  `src/charts/core/Pine.cpp:316`, `:449` (`n * 60` overflows for "99999999h");
  `src/charts/data/Providers.cpp:391`, `:394`, `:485`, `:488`, `:694` (NaN kline times → qint64). Clamp in double
  first (as `lineIn::toRow` in HeatmapLayer does) or skip non-finite values.
- `ChartPane.cpp:475` + `flow/HeatmapLayer.cpp:77`, `:87-91`: the heatmap "step" has no upper bound (int64 overflow
  in the row maths; garbage picture). Clamp `stepTicks` to `[1, 1<<40]`.
- `flow/SmcLayer.cpp:236`: `std::clamp` with `lo > hi` (asserts under libc++ hardening) for a level starting near the
  right edge.
- `src/charts/core/Trades.cpp:163-174`: a merge run longer than the capacity makes `size_ > capacity` (unreachable
  with today's sizes): `min(runEnd - k, buf_.size())`.
- `src/charts/core/Ticks.h:87` (with M7): the `__int128` quotient is truncated to int64 silently; fail instead;
  `Heatmap.cpp:27`, `:42` narrow `rowTicks_` to int32.
- More `lround`/`qRound` of possibly non-finite values than M9 lists: `CotPanels.cpp:231`, `:265`, `:271`, `:443`,
  `:564-573`; `RetailModal.cpp:249`, `:422`; `RetailSentimentPage.cpp:128-129`, `:140-141`, `:597-598`;
  `PosKit.cpp:112`; `RiskSentimentPage.cpp:206-209`; `src/pages/global/overview/FeedPanels.cpp:521-553`;
  `src/pages/ai/MlPage.cpp:304`, `:1406`. One helper (dash when not finite), used everywhere.
- `src/pages/charts/panels/bottom/Paper.cpp:101` (`rp->bars[rp->replay->index]` unchecked; `price()` at `:61-62` has
  the test); `panels/bottom/Heatmap.cpp:151` (sector thinner than 15 px → negative sizes, division by zero);
  `src/pages/positioning/bank-positioning/ForecastOverlay.cpp:374-375` (`shown - 1` when no pill fits).
- `src/pages/ai/MlPage.cpp:388-393`: `std::sort` of coordinates that may contain NaN (file written by the trainer).

**M14. Lifetimes that hold only by today's callers**
- `src/charts/data/TradeStream.cpp:230-231`: `feed_` dereferenced after `emit historyChanged()` (a slot releasing the
  last reference would null it). Read the pace before the emit.
- `src/charts/data/BinanceFeed.cpp:122-126`, `:205-208`, `:363-365` (*plausible*): the ping timer's own `timeout`
  slot can delete the timer through `ws->abort()` → `onClose` → `pingTimer_.reset()`. `release()` + `deleteLater()`.
- `ChartPane.cpp:652`, `:662-666`, `:688`, `:692`: a `FootprintFeed` could be replaced while its `changed()` is on
  the stack; queue the `render(false)`.
- `flow/BigTradesModel.cpp:27`, `flow/LiqMapModel.cpp:26`: `watcher_.result()` rethrows a worker `bad_alloc` on the
  GUI thread; catch in the worker.
- **When you apply M3**: `src/pages/charts/ChartsApp.cpp:1170-1190` keeps `&def` pointers taken while iterating
  `ind::all()`; if `all()` returns by value this becomes a use-after-free — hold the vector in a local.
- `src/charts/data/Alerts.cpp:570-637` (*plausible*): the checker recurses once per alert of a symbol|timeframe
  group; thousands of alerts on one symbol (creatable through `POST /api/alerts`) overflow the stack. Loop instead.

## Batch 2 additions — security and untrusted input

**S8. More link openers and rich-text sinks**
- `src/pages/charts/panels/SidePanel.cpp:1059-1060`: `QDesktopServices::openUrl` on the raw `<link>` of an RSS item
  (`src/charts/data/MarketInfo.cpp:475`). Use `ui::openWebLink`; drop non-http(s) links in `parseRss`.
- Tooltips auto-detect HTML and receive external text unescaped: `src/pages/news/feed/FeedWidgets.cpp:433`, `:674`,
  `:791`; `src/pages/news/calendar/CalendarTable.cpp:441`, `:446`; `calendar/RangePanel.cpp:240`;
  `squawk/SquawkWidgets.cpp:453`; `squawk/CalendarPane.cpp:141`; `src/pages/economy/RatesPage.cpp:693`;
  `SidePanel.cpp:1045`; `src/pages/ai/AiModelsPage.cpp:341`; `src/pages/account/TrackRecordPage.cpp:257-260`. One
  helper `ui::plainTip(text)` (`Qt::convertFromPlainText`).
- Labels outside `ui::label`: `src/pages/account/TrDialogs.cpp:330-339` (import preview: `res.currency`,
  `res.login`), `src/pages/data/MacroDataPage.cpp:804-806`, `:818-819`, `:841-843` (`fit::TextBox`),
  `src/pages/news/bank-research/PdfPanel.cpp:447` (PDF title). Plain text.
- `src/pages/news/bank-research/Columns.cpp:499-503`, `:323`, `:676-677`: SEC JSON fields (`f.form`, `f.url`,
  `a.form`, `a.currency`) unescaped in a RichText label with `setOpenExternalLinks(true)`. Escape; route
  `linkActivated` through `ui::openWebLink`; require `https://www.sec.gov/`.
- `src/account/Importers.cpp:285`, `:292-294`: statement fields (login, company, name, currency) never go through
  `cleanField`.

**S9. Paths and URLs built from outside strings**
- `src/charts/data/MarketInfo.cpp:657-659`: `sym.ticker` into api.nasdaq.com paths without `encodeUriComponent`
  (`US:A/../../X` rewrites the path). `src/charts/data/Imports.cpp:77`: `CSV:<name>` not passed through `cleanName`
  (`CSV:DIR/../X` leaves `imports/`). Encode; refuse names that are not `[A-Z0-9_]+`.
- `src/charts/data/Feeds.cpp:158-159`: Polygon `next_url` followed with the Bearer key after a host check only —
  also require https.
- `src/charts/data/ApiServer.cpp:176-183`, `:733`, `:783`: per-connection gate state keyed by peer port only (two
  local clients with the same source port share one state). Key by socket.

**S10. Child processes and sandbox scope**
- `src/ml/Runs.cpp:113-117` (*plausible*): the trainer's profile grants read and exec on `<pyvenv.cfg home>/..` —
  with a Homebrew Python that is all of `/opt/homebrew` or `/usr/local`, while the page says "no access to your
  files" (`MlPage.cpp:1288`). Grant only the interpreter's own framework / Cellar folder.
- `src/ai/Engine.cpp:158-177`: the model runner inherits the app's whole environment; give it a clean one as the
  trainer has (`Runs.cpp:169-173`).
- `src/ml/Environment.cpp:103-105`: `pip install` pins versions, not hashes (`--require-hashes` with a bundled
  requirements file). `:100`: `findPython()` (5 s per candidate, up to 12) runs on the GUI thread when INSTALL is
  pressed early.

**S11. Custom JS time limit does not hold across native calls** — `src/charts/core/CustomJs.cpp:344-349`,
`:697-706`: `ta.compute / atr / tr / source / supertrend / psar / dmi` on the calc's own `bars` never check the 5 s
deadline (a loop of slow `ta.compute("rci", …)` blocks the GUI for minutes). Check the deadline in `callNative`
before and after the native. With it — `src/charts/core/IndicatorsDefs.cpp:602-606` + `Indicators.cpp:446-457`:
`rci` is O(bars × length²) and stays a hang after the M4 clamp (length 2000 on 20,000 bars ≈ 7e10 comparisons):
rank by sorting the window, cap the length (1000).

## Batch 3 additions — wrong or lost data

**D14. Work the user typed is thrown away by a re-render (Charts bottom panel)**
- `src/pages/charts/panels/bottom/Editor.cpp:164-226` (`render()` always sets the template, `:211-212`), `:149-153`:
  unsaved indicator code is replaced by the template when the panel is collapsed, another tab is clicked, a file is
  deleted, the theme changes, or `PUT /api/state` arrives (`BottomPanel.cpp:59-62`, `:125-134`, `:161`,
  `ChartsApp.cpp:1764-1770`). Keep a draft (name + code) in `EditorTab`, restore it in `render()`; Delete calls
  `renderList()`.
- `bottom/Tester.cpp:206-210`, `:177-195`: rule and settings edits are lost the same way (only Run / Save read the
  form). Same in `bottom/Screener.cpp:196-203`. Read the form on change.
- `bottom/Paper.cpp:201-250`, `:463-470`, `:139`: the order ticket is wiped by every re-render — up to four times a
  second while replay plays. Refresh only the right column on those paths.
- `src/pages/charts/ChartsApp.cpp:822-826`, `:862`: shrinking the layout 4 → 1 discards the other charts'
  configuration (they come back as clones). Keep the trimmed configs.

**D15. Number fields swallow the decimal comma** — `bottom/Common.cpp:142-150` (validator `[0-9eE+\-.]*`),
`:152-161`: on a French keyboard "0,5" becomes "05" = 5 in every tester setting, the paper ticket and the screener
filters (`ChartsApp.cpp:1529` accepts `[.,]` in its own field). Accept `,` and convert before parsing; check
`RuleBuilder.cpp:260-271`, `:453`, `:458` too (*plausible*: a comma gives NaN, a rule never true).

**D16. Paper trading fills wrong**
- `bottom/Paper.cpp:394`, `:399-412`, `:416-425`, `:111-138`, `:433-460`: the bracket (SL/TP) of a pending limit/stop
  entry is live before the entry fills, and sized with `amt / cur` instead of `amt / entry` (Buy limit 90,000 with
  TP 3 % at price 100,000: the TP sells at once and opens a short). Create the bracket when the entry fills, with
  the filled quantity.
- `:437-460`: in replay, SL and TP of one bracket can both fill on the same bar (the loop iterates a snapshot).
  Skip orders no longer in the state.
- `:121`, `:134`: a marketable limit fills at its limit price, not at the market (the replay path is right, `:443-444`).
- Stops are only checked while the Charts page is visible (`SidePanel.cpp:237-244`) — a product decision: ask the user.

**D17. Results attributed to the wrong symbol or run**
- `bottom/Tester.cpp:544-609`, `bottom/Paper.cpp:58-64`, `:367-372`; root cause `ChartsApp.cpp:906-916` (`cfg["symbol"]`
  set at once while `bars` are still the old symbol's): a magnifier run started on BTC is titled and marked on ETH
  after a switch; a market order on a new symbol fills at the previous symbol's last close. Capture symbol and
  timeframe at start; let ChartPane expose the symbol its bars belong to.
- `bottom/Tester.cpp:581-589`, `:657-680`, `:665`: no run generation — an older run overwrites a newer result;
  `runStored` uses the Pine file as it is when the bars arrive.
- `bottom/Screener.cpp:233-259`, `:115-120`, `:135-139`, `:178`: a scan's answer is applied whatever happened
  meanwhile (other market → empty table; two scans, last one wins); the Sector filter is built from the previous
  rows. Sequence number + universe check; clear rows on universe change.
- `bottom/QuantChecks.cpp:307-346`: result lost and the worker left running when the box is rebuilt; a second click
  starts a second full computation.
- `src/pages/charts/ChartsApp.cpp:1102-1117`: symbol search — a late reply to an older query replaces the results,
  and Enter picks `results[sel]` whatever query they belong to (type "A", then "APL", Enter → the first hit for
  "A"). Keep the query with the results.
- `src/pages/positioning/RetailModal.cpp:531-549`: a late answer for the previous range overwrites the chart
  (1h candles under a selected "1Y"). Request counter.
- `src/pages/data/PairDialog.cpp:194-203`, `:382-395`, `:432-478`, `:755-766`: a stale Binance answer validates a
  symbol that is no longer the one typed (ETHUSDT → ETHUSD stored; empty field keeps ADD enabled → `"BINANCE:"`).
  Bump the generation on every edit; refuse in `save()` when the checked symbol differs. `:547-601`, `:643-667`:
  ADD within 250 ms of a parameter change adds the previous parameters.

**D18. Rule builder drops indicator parameters** — `src/pages/charts/panels/RuleBuilder.cpp:446-464`
(`OperandEdit::operand()` rebuilds `params` from the main input only): a strategy loaded from JSON with Bollinger
`{length: 20, mult: 2.5}` or MACD `{fast, slow, signal}` runs — and is saved — with the secondary parameters at
their defaults. Start from `op_.params`.

**D19. Wrong data from providers**
- `src/charts/data/Providers.cpp:281-351`, `:1192`, `:1341`; `LiveFeed.cpp:60`: after one failure of the two global
  hosts, Binance.US becomes the sticky preferred host: charts show Binance.US prices under the name "Binance", live
  klines from the global socket are merged onto them, pairs not listed there fail, a whole quotes batch falls back.
  Treat .us as a named last resort; never sticky; apply the 400 short-circuit to hosts 0 and 1 only. Test with the
  fake transport.
- `Providers.cpp:969-980` (*plausible*): Yahoo weekly/monthly bars dated one day early across a DST change.
- `Fetch.cpp:61-65`, `:134-137`, `Providers.cpp:307-336`, `:361-367` (*plausible*): no back-off on HTTP 429/418 —
  the failure clears `binanceKnownGood`, so the next call asks all three hosts at once. Mark the host down for
  `Retry-After`.
- `src/charts/data/LiveFeed.cpp:91-124`: one failed reconnect downgrades a chart from WebSocket to REST polling for
  good. Retry the socket with backoff while polling.
- `src/charts/data/Alerts.cpp:563-566`: an alert whose candles cannot be loaded (bad timeframe — `validate` never
  checks it —, symbol down) fails silently forever. Validate `tf`; record the load error on the alert.
- `src/charts/core/PineIndicators.cpp:318`, `:310-313`, `:344-345`, `:377-386`: the Pine result cache serves the old
  script's output after a reload (key has no script version; a run in flight stores its result under the new
  script's key). Put a source hash in the key; drop waiting/queued entries in `forget`.
- `src/charts/core/PineIndicators.cpp:172`, `:151-152`, `:215-216` vs `IndicatorsValidate.cpp:209`, `:284`: a valid
  Pine script is refused at registration (`plot(…, linewidth=5)`, or a long title) while `pine::check` says it is
  fine. Clamp / truncate; make `check()` run the same structural rules.
- `PineIndicators.cpp:324-330` + `JsStack.cpp:36-38` (*plausible*): Pine indicators inside rule strategies run on
  pool threads with a 512 KB stack ("Maximum call stack size exceeded" in the tester, quant checks,
  `/api/backtest`) — callers `ApiServer.cpp:1004-1005`, `Tester.cpp:682-688`, `QuantChecks.cpp:335-344`,
  `PineQuant.cpp:52`. Run on a 16 MB-stack thread, as `PineQuant.cpp:90` and `BacktestTools.cpp:308` do. (The 5
  Pine tests failing under ASan show the same error.)
- `src/charts/core/IndicatorsDefs.cpp:664` (*to verify first*): SMI Ergodic smooths `ema(ema(pc, short), long)`;
  Pine's `ta.tsi` smooths long first then short (as the `tsi` indicator at `:649` does). Check against TradingView's
  definition before changing; then align and extend `pine_reference_tests`.

**D20. ML trainer results (resources/ml/train.py)**
- `:78-85`, `:221-223`, `:237`: with a horizon above 1, the last `h` training rows carry targets computed inside the
  test period and `TimeSeriesSplit` has no gap → the test metric and "beats the baseline" are optimistic. Train on
  rows `< cut - h`; `TimeSeriesSplit(n_splits=folds, gap=h)` (the self-supervised probe already does, `:754`).
- `:516`, `:529-530`, `:543-557`: in semi-supervised learning a real class −1 is treated as "no label". Map classes
  to 0…k−1 (and back).
- `:61-62` (unparsable dates sorted last become the test tail and the "latest row"), `:82` (a zero in the target
  gives ±inf and scikit-learn stops): drop both kinds of rows with a log line.

**D21. Track record and bridges**
- `src/pages/account/TrDialogs.cpp:318-325`, `:372-397`: a statement in one currency can be imported into an account
  in another, silently (EUR P&L summed as USD). Show each target's currency; block or confirm on a mismatch.
- `resources/account/bridge/DeepwaterBridge.mq4:117-118`, `.mq5:65-66`, `:77`: one server offset for the whole
  history — trades of the other DST season are exported one hour off (Deepwater corrects only shifts above 90 min).
  `.mq5:64`, `.mq4:116`, `.cs:65`: volumes written with 2 decimals (0.001 lot → 0.00). `.cs:78-79`: delete then
  move (the file does not exist in between). Not compiled here: say what you could not test.
- `src/pages/account/TrDashboard.cpp:347`: "profit factor ∞" when it is NaN (only breakeven trades).

**D22. State and files that can be lost (more)**
- `src/charts/data/Store.cpp:17-27`, `:29-62`, `:65-80`: a `state.json` that cannot be read or parsed is treated as
  `{}` and then overwritten (layouts, watchlists, drawings, paper account gone); the write renames without fsync and
  falls back to in-place. Same for `alerts.json` (`Alerts.cpp:159-162`). Distinguish missing from unreadable: copy
  to `state.json.bad-<timestamp>` and refuse the merge; use `QSaveFile`.
- `src/charts/data/CustomFiles.cpp:119-146`, `Imports.cpp:318-327`, `:494-511`: saving a custom indicator / re-importing
  a file truncates the user's file before the new content is validated or safely written. Validate from a temp file
  (`CustomJs.cpp:1047` has one), then `QSaveFile`.

**D23. Displays that are wrong or stale**
- `src/pages/data/MacroDataPage.cpp:208-214`, `:1054-1075`, `:1097-1107` (the non-crash side of M2): after every
  reload the inspector empties, and unticking a chain above the selected one moves the highlight to another chain.
  Remember the selection by chain key + slot, re-select after `setLanes`.
- `src/pages/news/calendar/CalendarTable.cpp:448-457`: the release-history chart never shows its hover — line 457
  clears it (`hi` is only set for rows). Keep an `onCard` flag.
- `src/pages/news/bank-research/PdfPanel.cpp:329` and `src/pages/news/feed/FeedWidgets.cpp:441`: a statement
  swallowed by a trailing `//` comment on the same line (`m_toolbar->setPalette(tp);` and the second gradient stop).
  Put each back on its own line (a search of `src/` finds only these two).
- `src/pages/charts/plot/PriceScale.cpp:58-63`, `plot/Chart.cpp:426-428`, `:62-71`, `ChartPane.cpp:1671-1675`: Log
  mode reaches every pane and values ≤ 0 are not drawable — MACD/CCI/momentum panes lose their negative half or go
  blank. Fall back to Normal for a scale whose visible values include one ≤ 0 (or a sign-preserving log).
- `src/pages/charts/VolumeProfile.cpp:24-30`, `:73-76`: the D2 bug (logical index used as bar index) at two more
  sites.
- `src/pages/market/liquidity/LiquidityPage.cpp:126-142`, `liquidity/CandleChart.cpp:322-325`: the old instrument's
  price and KPIs stay beside the new name; "LOADING…" forever after a failed load.
- `src/pages/market/watch/CandleChart.cpp:924` vs `:770` (the measure result disappears on mouse release),
  `:136-141` (a half-placed drawing survives a change of instrument), `:145-148`, `:193-207` (quotes build candles
  with the new timeframe's rule on the old bars), `:1038` (Fibonacci colours frozen at the first theme: `static`);
  `watch/InstrumentSheet.cpp:189` (5 decimals for NASDAQ, NIKKEI…).
- `src/pages/market/seasonality/SeasonalityPage.cpp:227-229`, `:250-267` (five cards stay on "Loading…"; a failed
  compare is silent); `SeasonWidgets.cpp:388-395`, `:433` (grid step from the raw range: up to 290 grid lines).
- `src/pages/market/fundamental/FundamentalPage.cpp:233`, `:248` ("strongest currency by default" never applies on a
  fresh profile), `:73` (cursor offset above 2560 px).
- `src/global/FxStrength.cpp:79-92`, `:182`: Currency Strength shows stale lines as live after coming back (same
  family as D11): load at once when older than the refresh period.
- `src/pages/global/session-recap/RecapModel.cpp:217-226` (K/M/B suffix dropped: "1.02M … below the 980K
  forecast"), `RecapPage.cpp:156-161` (a session that just closed keeps its pre-close bars for hours — *plausible*,
  keep reloading until end + 20 min), `RecapWidgets.cpp:292-309` (clicking the header opens the link scrolled under
  it), `RecapModel.cpp:52-53` (current week missing west of Tokyo's Monday).
- `src/pages/positioning/CotPanels.cpp:80-86` (Unweighted silently becomes Futures),
  `bank-positioning/ForecastOverlay.cpp:136-146` (lines not rebuilt when forecasts change),
  `RetailModal.cpp:412-413`, `:442-443`, `src/pages/positioning/CandleChart.cpp:54`, `:112-113` (daily bars dated in
  the local zone: one day early west of UTC), `src/pages/global/overview/OverviewData.cpp:225-227` (untranslated
  tooltip).
- `src/pages/news/squawk/CalendarPane.cpp:109`, `:174`, `:208-210`, `:262-269` (bank holidays shown with a clock time
  and a countdown), `SquawkPage.cpp:137-156` + `SquawkWidgets.cpp:451` ("FR" lit when no translation is possible),
  `calendar/RangePanel.cpp:52-54` (all-day event one day early west of New York), `CalendarPane.cpp:106-113`,
  `Columns.cpp:166-171` ("Today" labels stale at midnight), `feed/FeedWidgets.cpp:703-725` (headlines blank after the
  list shrinks), `Columns.cpp:461`, `:141-152` (list loses its place; open dropdown picks the wrong entry),
  `PdfPanel.cpp:67`, `:433-438` (previous note's thumbnails), `calendar/HistoryCard.cpp:29-31` (drag state survives
  its row), `src/pages/economy/GlobeWidget.cpp:134-137`, `:161-163` (globe frozen after hovering a registry row).
- `src/pages/charts/ChartsApp.cpp:1599-1610` (synced crosshair never cleared: `clearCrosshair()` has no caller),
  `:858-873` (a layout change moves the active chart to a stale index), `plot/Chart.cpp:736-748` vs
  `ChartPane.cpp:1146` (a dragged pane separator snaps back within 1.5 s), `plot/PriceScale.cpp:168-174` +
  `ChartPane.cpp:1087-1096` (indicator panes with values under 0.01 have no ticks), `:64-67` (percentage mode with a
  first value of 0), `bottom/Tester.cpp:446-454`, `:785-788`.
- `src/pages/ai/ChatView.cpp:276-279`, `:362`, `:374-378` + `src/ai/Engine.cpp:384`, `:428-430` (a question cannot
  be cancelled while the model loads; SEND does nothing), `ChatView.cpp:308` (reasoning folds not cleared),
  `MlPage.cpp:917-918` (CSV parsed twice).
- `resources/macro/meetings.json`: the RBA list ends on 2027-02-09 (four RBA dates still unverified); FED 2027 has
  no December meeting (*to re-check on the Fed's page*). Do not invent dates: verify on the central banks' sites or
  ask the user.
- `src/technicaldata/TechnicalDataIndicators.cpp:81`: an indicator that throws is stored as empty values with no
  message.

## Batch 4 additions — growth and request storms

**G8.** `src/pages/charts/flow/FootprintFeed.cpp:45-66` + `src/charts/core/Footprint.cpp:29-43`: footprint candles
are never dropped once the trade tape rolls (the book only clears on a rebuild) — tens of MB a day on a busy 1m
chart left open. Drop candles older than `tape.front()` when the tape has dropped trades. Same, smaller:
`flow/BigTradesModel.cpp:116-118` (`bubbles_`).

**G9.** `src/news/bank-research` viewer: `src/pages/news/bank-research/PdfPanel.cpp:111-116`, `:145-181`,
`src/banks/Pdf.mm:47-48`: at 400 % on Retina an A4 page is ~228 MB and five are kept (> 1 GB). Cap rendered pixels
per page (16–24 MP, upscale beyond) and keep only wanted pages at high zoom.

**G10.** `src/charts/data/Imports.cpp:389` (one full resampled copy of an import per timeframe string ever asked;
entries of deleted files never removed), `src/charts/core/PineIndicators.cpp:331-339` (no eviction on the
synchronous path), `src/pages/global/overview/OverviewData.cpp:182-209` (`m_fr` / `m_asked` never pruned; a failed
translation never retried), `src/pages/market/watch/WatchIcons.cpp:92-93` (~400 pixmaps added per theme change),
`src/pages/charts/ChartsApp.cpp:1349-1358` (a parentless combo box leaks per opening of a drawn-only indicator's
settings).

**G11.** `src/charts/data/Providers.cpp:316-336`, `:418-430`: while no Binance host is known good (every launch),
each request is sent to the three hosts at once — the ~10 MB symbol list is downloaded, parsed and cached three
times. Probe with a small request. `:508-531` (*plausible*): Coinbase pagination has no page cap (hundreds of
requests for a thinly traded product). `src/charts/data/Alerts.cpp:551-554`, `:260-264`: the checker rewrites
`alerts.json` every 15 s when nothing changed (disk writes, watcher callbacks, view refresh). Skip identical content.

**G12.** `resources/ml/train.py:646` (`np.stack` materialises every window: 2.4 GB at window 20, 14 GB at 120 on a
million rows — use `sliding_window_view`), `:348`, `:407`, `:436`, `:146-155` (no row cap for the quadratic
methods), `:311-320`, `:158` + `src/ml/Catalog.cpp:26` (`hidden_layers` unbounded). The sandbox sets no memory limit.

## Batch 5 additions — GUI thread and hidden work

**P7.** `src/app/MainWindow.cpp:98-104`: on a language or theme change, removing the current page from the stack
shows the next old page, whose `showEvent` runs in full before it is deleted in turn — Squawk creates its web view
and loads the player, News Feed polls every source, Calendar fetches. Remove and delete every non-current page first,
the current one last.

**P8.** `src/charts/data/Imports.cpp:76-91`, `:335-356`, `:377`; callers `Providers.cpp:1415`, `:1160`,
`ApiServer.cpp:1028-1040`: imported CSV files are parsed on the GUI thread (seconds for a 100 MB file, on the first
symbol search), a file that fails is re-parsed on every keystroke, and `importBars` discards what it just parsed.
Parse on a worker; cache failures; keep the parsed bars.

**P9. Work per paint, per mouse move or per tick**
- `src/pages/global/overview/FeedPanels.cpp:305-311`, `:367-374`, `:418-424`, `:41-52`: the Squawk list re-wraps
  every headline on each mouse move, wheel event and paint. Cache wrapped lines and prefix sums.
- `src/pages/global/session-recap/CentreView.cpp:179-188`, `:514-518`: full repaint (six text layouts, 12 sparklines)
  per crosshair pixel.
- `src/pages/news/feed/FeedPage.cpp:50-51`, `:88-126` (everything rebuilt on each search keystroke and while
  hidden); `FeedWidgets.cpp:136`, `:585`, `:701`, `Reader.cpp:49`, `:77-143` (each translation batch re-lays out the
  whole page even hidden, and wipes the find highlights); `FeedWidgets.cpp:86-128`, `:455-458`, `Reader.cpp:119`,
  `:181-182` (images flash to the placeholder and are rescaled at every resize step).
- `src/pages/sources/DataMapPage.cpp:289-321`, `:386-395` (~6,000 `matches()` per paint while searching).
- `src/pages/economy/GlobeWidget.cpp:261`, `AssetFile.cpp:834`, `RatesPage.cpp:694` (full repaint on every mouse
  move).
- `src/pages/ai/ChatView.cpp:307-309`, `:412-531` (the transcript re-renders while hidden, up to 20 times a second);
  `MlPage.cpp:128-140`, `:277-290`, `:352-393`, `:1318`, `:1485`, `:1796` (result CSVs re-read and per-paint sorts).
- `src/pages/data/Landscape.cpp:60`, `:72-106` (the slice slider rebuilds and re-uploads every box);
  `src/pages/account/TrAdvanced.cpp:190-195`, `:228-234` (copy + sort of all trades per paint).
- `src/pages/market/liquidity/CandleChart.cpp:126` (a ~13 MB pixmap allocated per pan frame; no opaque-paint
  attribute); `fundamental/FundWidgets.cpp:176-182`, `:216-218`, `:290-293`, `:708-729`,
  `FundamentalPage.cpp:66-72`, `:192-197`; `watch/CandleChart.cpp:640-651` (SMA/EMA/Bollinger recomputed per frame).
- Charts: `plot/Chart.cpp:319-346` (`rebuildTimePoints` sorts every row of every series on each indicator render),
  `plot/TimeScale.cpp:18-24`, `:274-296`, `pane/Navigator.cpp:61-122` (mini curve rebuilt per tick),
  `Drawings.cpp:61-71` (SVG parsed in paint), `panels/bottom/Paper.cpp:142-169`, `panels/RuleBuilder.cpp:468-487`.
- Cached pixmaps that ignore a change of device pixel ratio (blurry after moving to a Retina screen):
  `src/pages/positioning/CotPanels.cpp:285`, `:427`, `RiskSentimentPage.cpp:94`, `:249`, `:372`, `:438`.
Measure before and after what you change here; do not "optimise" what you did not measure.

## Batch 6 — build and platform (ask the user before changing behaviour)

**B1.** `CMakeLists.txt`: no `-Wall -Wextra`, no `_LIBCPP_HARDENING_MODE` (it would have turned M1 into a clean
trap), release builds define `QT_NO_DEBUG` (Qt's container bounds checks off: an out-of-range `QList` index
continues silently). Propose: hardening mode "fast" in release, warnings on, and say what it costs (measure a frame).
*Done 2026-11-19 (user's go-ahead):* `_LIBCPP_HARDENING_MODE_FAST` (MSVC: `_MSVC_STL_HARDENING`) and `QT_FORCE_ASSERTS`
on the app in every build type (`-DDW_HARDENING=OFF` to compare); `-Wall -Wextra -Wno-missing-field-initializers`
(MSVC `/W4`), 303 → 0 warnings. Cost, Charts 1600×1000, BTCUSDT 1m, four alternated runs each, ms per frame (mean):
heatmap panning 5.13 without / 5.03 with, liquidation map panning 8.46 / 8.74, footprint 8.60 / 8.80 — inside the
run-to-run spread (8.24–8.94 without). No check fired on the 29 pages nor in the order-flow scenario.
**B2.** `-ffp-contract=off` is applied to `src/charts/**` and `src/pages/charts/**` in the app (`CMakeLists.txt:34-37`)
but to every file in `tests/technicaldata/CMakeLists.txt:16`: the tests do not run exactly the arithmetic shipped
for `src/technicaldata`. Add it to the app's list.
**B3.** The bundle built here is a development bundle: template `Info.plist` (empty version and minimum system,
though `dw-translate` needs macOS 26; no icon), ad-hoc signature, no hardened runtime, links against `~/Qt`. There
is no `.github`, CI, signing or update code in this tree. Report only; packaging is the user's decision.
**B4.** `src/app/PerfAudit.cpp:13`, `:21-25` (`<sys/resource.h>`, `getrusage` unguarded) and `banks::pdf` existing
only as `.mm`: the tree does not build on Windows today. The user wants the Windows port kept alive: guard these
and list what else is macOS-only.

---

## Coverage of the audit (so you know what was and was not looked at)

Read in full, line by line, in the second pass (file lists and line counts are in the reviewers' reports):
`src/pages/charts` — everything: ChartsApp, Style, Drawings, DataText, Format, Icons, VolumeProfile, `plot/`, `pane/`
(8,558 lines), `panels/` (7,046), ChartPane + `flow/` (re-read for memory safety); `src/charts/core` — every file
(indicators, backtest, quant, Pine, custom JS: 8,054; order flow: re-read); `src/charts/data` — every file;
`src/pages/market` (10,265); `src/pages/global` + `src/pages/positioning` (10,471); `src/pages/news`,
`src/pages/economy`, `src/pages/sources`, `src/ui`, `src/app`, `src/main.cpp` (14,173); `src/pages/ai`,
`src/pages/account`, `src/pages/data` (11,684); the build files, `.mm` / Swift / Python / MQL / C# sources, the three
JSON resources and all 15 translation files (4,532 messages: none unfinished, no placeholder mismatch).
Read in the first pass (end to end by its reviewers, not re-read in the second): `src/core`, `src/net`, `src/wire`,
`src/news`, `src/macro`, `src/macrodata`, `src/banks`, `src/fundamental`, `src/technicaldata`, `src/global`,
`src/market`, `src/seasonality`, `src/analytics`, `src/positioning`, `src/ai`, `src/ml`, `src/account`,
`helpers/llm/main.cpp`.
Limits, stated plainly: (1) the services of the first pass were read once, with fewer tool calls per line than the
second pass — if you find something there while fixing, add it; (2) two cross-cutting sweeps were sampled, not
exhaustive: parentless `new` (45 sites listed, about 14 read) and general `int(` casts across the tree; (3) nothing
was run by the reviewers — only the sanitizer, leak and analyzer runs above executed code, and those scenarios do
not open a PDF, the OCR path, the ML trainer, a model, the bridges or Windows; (4) `third_party/`, `quant-terminal/`
and the tests themselves were not audited; (5) Qt's own behaviour was assumed from memory where marked *plausible*.

