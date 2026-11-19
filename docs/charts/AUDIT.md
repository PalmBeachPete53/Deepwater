# Charts tab — bug audit (2026-11-16)

Five read-only reviews (chart pane + engine, page shell, panels, data layer, API/alerts/custom JS) compared the C++
with quant-terminal's JS, plus an ASan/UBSan build of the tests and of the harness (tests/charts/app) run through 8
stress scenarios (16→1 charts, quick symbol/timeframe changes, every chart type, every panel and dialog, light theme).
Sanitizer runs: no error (104 core + 89 data tests, 8 harness scenarios). Nothing below is fixed yet.

Confidence: **run** = reproduced, **read** = confirmed by reading the code, **plausible** = possible, not reproduced.

## Status (2026-11-16, same day): every item below is fixed, except what "Still open" lists
Regression tests added: tests/charts/data/netfix_tests.cpp (C2, S1, W3, W12), server_tests.cpp (S2, HEAD, PORT,
/custom/100%25.js), custom_tests.cpp (S3, W7, P2), indicators-edge dump (W7: "abc" for every int input, = node).
Engine/pane fixes (C1, C3–C5, W1, W6, W11, P1) checked with ASan repro programs (scratch, not in the repo).
How some were fixed:
- S1: `sources::redactUrls` keeps only scheme://host of URLs in every network error text (net::Http and
  recordResult); an old health.json is cleaned when loaded.
- S2: ApiServer peeks the request head and answers 403/413 before the body is read; body limits follow the JS
  (2 MB, 20 MB for PUT /api/state, 300/100 MB for the CSV imports).
- W8: a QLockFile `<data>/charts/alerts-checker.lock`: one alert checker per data folder.
- W12: `net::Request::preflight` (checked when the request leaves its host queue) + `charts::data::skipIfDown`.
- P3: the modal card's shadow is painted by the backdrop from a cached image (no QGraphicsDropShadowEffect).

Second pass (same day): two instances now share alerts.json (re-read under a lock before every change, watcher →
`Alerts::changed` → side panel redraw); chunked bodies over the limit get the JS 413 JSON; routes that take no body
ignore one of any size; topbar items keep their natural width (overflow scrolls, as `overflow-x: auto`); tab bars
keep labels whole and overflow on the right (`white-space: nowrap`); the text tool goes back to the cursor when its
pane disappeared during the dialog. Checked against the original and left as is (LW does the same): the last-price
label painted over a tick label, the lone day numbers on the Renko time axis.

Renamed to "Deepwater" (user's choice): brand tooltip, alert notification title, webhook test message, Pine
Script refusal, the API's 403 text and "/" message, the RSS User-Agent. `/api/health` keeps `app: "quant-terminal"`
(the identifier client scripts check).

Still open:
- A CSV import under its limit (300/100 MB) is still buffered by Qt: a 290 MB import costs ~2.7 GB of memory.
- If the instance holding the alert checker quits, another running instance takes it over only after a restart.
- At narrow widths the bottom panel's show/hide chevron is cut off with the tabs (the original lets it spill out);
  clicking the active tab still toggles the panel.
- Heatmap: a row with no value is dropped alone (JS: its whole sector disappears). Daily chart shown again just
  after midnight may reload from the 60 s candles cache.

## Crashes
| # | Where | What | Conf. |
|---|---|---|---|
| C1 | plot/Chart.cpp:634-679 | Dragging inside an indicator pane (or on its price axis) for >1.5 s on a live or replaying chart: `renderIndicators` deletes and recreates the indicator panes, `dragScale_` points to a freed PriceScale → use-after-free. Separator drag: `panes_[sepIndex_+1]` past the end if the pane count drops. | run (ASan) |
| C2 | data/Providers.cpp:880 | `SPREAD:FOO` (or `/api/candles?symbol=SPREAD:FOO`) dereferences an empty optional in `spreadCandles` → SIGSEGV. JS answers a 500. | run |
| C3 | ChartPane.cpp:1294-1303 | `merge()` with ≥2 new bars appends one time only: `times` shorter than `shown`, later `T[i]`/`times[i]` read past the end (renderIndicators, setCrosshairAtLogical). Happens on 1s charts polled every 3 s and after each hide/show (see W1). | run |
| C4 | ChartPane.cpp:888-899 | Pane deleted while the cursor is over its replay bar: Leave event reaches `eventFilter` after `delete d`. | plausible |
| C5 | ChartPane.cpp:728-741 | Text-note dialog (nested loop) open while panes are rebuilt (layout change via the API) → freed `q`/DrawingLayer used after. | plausible |

## Security / memory
| # | Where | What | Conf. |
|---|---|---|---|
| S1 | net/Http.cpp + net/Sources.cpp:92 (whole app) | Qt's `errorString()` holds the full URL; `recordResult` stores it in data/sources/health.json and the Sources tab. Webhook tokens (Telegram bot, Discord, Slack) leak — and any key passed in a query string (FRED, EIA, FMP…) in every tab. | run (webhook) |
| S2 | ApiServer.cpp:272 | 300 MB body limit, access check only after the body is buffered: any web page can POST big bodies to 127.0.0.1:3777 and make the app allocate GBs (250 MB POST → +750 MB, kept). JS rejects first and stops at 2 MB. | run |
| S3 | CustomJs.cpp:297-366, 459-487 | The 256 MB QuickJS limit doesn't cover the C++ copies: `return {value: new Array(5e6)}` → +1 GB on the GUI thread in 0.2 s; up to 50 M items per array; the 5 s interrupt isn't checked in those loops. | run |

## Wrong results / data loss
| # | Where | What | Conf. |
|---|---|---|---|
| W1 | ChartPane.cpp:1428-1439 | Live feed stopped while the tab is hidden, only restarted on show: every bar between hide and show is missing for good (indicators computed across the hole). JS never stops. | read |
| W2 | panels/bottom/Tester.cpp:484-533 | Bar-magnifier run: `finishRun` uses `app->active` at the time the lower-tf bars arrive, not the captured pane → markers/header/export on another chart. | read |
| W3 | net/Http.cpp:117 + data/Fetch.cpp:109 (whole app for Http) | A 2xx with a transfer error counts as success and is cached; a 200 that isn't JSON stays in the disk cache. A cut-off Binance exchangeInfo → no crypto symbols for 6 h; calendar in error 30 min. | run |
| W4 | ChartsApp.cpp:544, 1463 | `"layout": 4` (number) in state.json or via the API → 1 chart, and the next save writes `""` and drops the other panes. `null` pane entries → BTC 1D defaults instead of the JS clone of pane 0. | run |
| W5 | ChartsApp.cpp:1446-1453 | `PUT /api/state {"settings":{"theme":…}}`: panes not re-themed (dark canvas in light chrome, legend title invisible); `utc`/`volume`/`bars` changes not reloaded. | run |
| W6 | ChartPane.cpp:450-468, 975 | `load()` doesn't clear `mtfLoading`: an indicator on another timeframe can stay "loading…" after a symbol/timeframe change. | read |
| W7 | IndicatorsDefs.cpp:28-31, CustomJs.cpp:287 | Non-numeric length (`"abc"`) becomes 0 instead of NaN: highest/lowest/sum give ±Infinity, so a scan/alert rule passes where JS fails (alert could fire every check). | run |
| W8 | ApiServer.cpp:704-714 | Second Deepwater instance (port 3777 taken → 3778) still runs its own alert checker on the same alerts.json: "once" webhooks sent twice, each instance overwrites the other's list. | read |
| W9 | data/Store.cpp:33-45 | Write errors not checked before the rename (QFile buffer flushed in the destructor): on a full disk an empty state.json/alerts.json replaces the good one. Same unchecked writes in Imports.cpp and CustomFiles.cpp:124. | read |
| W10 | side panel / ChartsApp.cpp:1576 | Window ≤ 900 px wide hides the side panel, whose hideEvent stops the 5 s quote timer: paper limit/stop orders on symbols not open in a chart never fill, positions' prices freeze. | read |
| W11 | ChartPane.cpp:902-906 | Light/dark button (and a Deepwater theme change) resets zoom and scroll on charts without indicators/compare. | run |
| W12 | data/Fetch.cpp:81,100 + MarketInfo getRaw | Requests already queued to a host that went down each wait a full timeout (one in flight per host): 4 crypto charts behind a silent firewall → 10/20/30/40 s before the Coinbase fallback; JS: 10 s. | run |

## Performance
| # | Where | What | Conf. |
|---|---|---|---|
| P1 | ChartPane.cpp:909-913 | `shift()` calls `QDateTime::currentDateTime().offsetFromUtc()` per bar: intraday renders ~3× slower (~20 ms extra at 3000 bars, ×16 panes or replay 10×). | measured |
| P2 | CustomJs.cpp:944 + IndicatorsDefs.cpp:1184 + ChartsApp.cpp:557 | One `POST /api/custom` runs calc 4× and the file's top level 2× (JS: once); a slow file can freeze the UI up to ~20 s. | run |
| P3 | Style.cpp:631-645 + ChartsApp.cpp:773 | Indicators picker open: each cursor blink repaints the card through its shadow effect and rebuilds ~100 QStaticText → ~4.6 % CPU idle. | measured |
| P4 | Providers.cpp:60 / probeAll | Calendar health check can wait up to 5 min behind Forex Factory's pacing. | read |

## Minor
- Heatmap.cpp:238 — changing market repaints the old market's tiles until the new data arrive (stay there if the load fails).
- Heatmap.cpp:102 — rows with no value get a 1-unit tile (JS drops them).
- Common.cpp:136 — tester/screener number fields accept text (`abc`, `1,5`, `-`) → NaN stored in settings/strategies, screener filter hides every row while typing a minus sign.
- ChartPane.cpp:629-641 — a late compare answer (old timeframe) can overwrite the newer one (same race in JS).
- plot/Chart.cpp:598 — synced crosshair not clamped to the target chart's visible range (LW clamps).
- ChartsApp.cpp:501 — "Remove all drawings" confirm box: an API layout change during it makes it clear another chart's drawings.
- Paper.cpp:358 — second Buy/Sell press while waiting for a price may read the re-rendered form.
- ApiServer: HEAD answers carry a body; `PORT=70000` wraps to 4464; `/custom/100%25.js` → 400.
- Editor help text points at `docs/custom-indicators.md` / `docs/indicator-template.js` (quant-terminal paths, absent from Deepwater) and says "add an indicator to Quant Terminal".

## Checked and clean (summary)
Symbol parsing (27 tricky inputs = node), CSV imports, fallback chains and notes, pagination, feeds keys (never logged),
LiveFeed reconnect, API access control (Host/Origin/x-qt, traversal, 127.0.0.1 only, SSE), alert logic vs alerts.js,
QuickJS sandbox (no std/os, 5 s interrupt, recursion), layouts 1↔16, persistence and DW_PROFILE isolation, shortcuts,
drawings, replay edges, chart-type transforms, price/time scale edge cases, watchlists, alerts panel, rule builder,
calendar, news, paper trading maths, tester/quant checks/screener/seasonals/financials/editor.
