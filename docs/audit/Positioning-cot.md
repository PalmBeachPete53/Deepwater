# Audit — Positioning › COT (36 frames)

App states: `DW_COT_MARKET` (USD / EUR), `DW_COT_SCROLL` (0, 479, 666, 1246), `DW_COT_HOVER` (price crosshair,
weekly-split tooltip). Live CFTC report 2026-11-03 vs the reference's 2026-10-20: values differ, not counted.
Frames: t000.0–t000.5 crosshair, t001.0–t004.0 scroll down, t004.5–t005.5 bottom, t006.0–t008.5 scroll up,
t009.0 crosshair date, t009.5–t010.0 EUR, t012.0–t013.5 split hover/tooltip, t014.0–t017.5 EUR scroll to bottom.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000.0 | asset classes | `DEVISES` | `CURRENCIES` | lead's decision (English UI) | deviation |
| 2 | t000.0 | currency buttons | USD 10–64, padding 7 | 4 px wider | padding / flag gap | fixed |
| 3 | t000.0 | SUMMARY / ADVANCED headings | y 105 / 649 | 6 px higher | +6 px | fixed |
| 4 | t000.0 | price subtitle | ends with the COT contract | extra text first | contract first, computed note after | fixed |
| 5 | t000.0 | `Unweighted | TradingView` | toggle | `Unweighted | Futures` | own chart (decision); Futures = contract's future | deviation |
| 6 | t000.0 | price axis | 1.00 / 0.50 step, ≈50 px per tick, axis ≈55 px | 0.50 step dense, 70 px | tick spacing 50, axis 58 | fixed |
| 7 | t000.0 | empty space after the last candle | ≈5 bars | none | `rightPadBars = 5` | fixed |
| 8 | t000.0 | crosshair right of the data | line follows the mouse, no date tag | snapped to last bar + tag | handled | fixed |
| 9 | t000.0 | axis labels under tags | hidden | overlapped | hidden within 15 px | fixed |
| 10 | t009.0 | time axis | `Sep 9 17 Oct 9 18` (≈90 px apart) | denser | min gap 80 px | fixed |
| 11 | t000.0 | OI strip axis | 4 ticks | 3–5 | 4 | fixed |
| 12 | t000.0 | weekly split categories | condensed 9.5 px, y≈160 | Inter 10, y≈165 | condensed 11, y +27 | fixed |
| 13 | t002.0 | panel titles | mono bold | ok | — | ok |
| 14 | t002.0 | chart axes, legends, table labels | condensed (Rajdhani-like) | Inter | condensed 13 | fixed |
| 15 | t002.0/t004.5 | right column y | sector/stats y 772 h 331, TFF table 1109, TFF sector 1338 h 472, read 1821 (scroll-area coordinates) | up to 13 px off | geometry re-measured | fixed |
| 16 | t004.5 | read headline / text | condensed 16 bold / 13 | Inter 15 / 12 | condensed | fixed |
| 17 | t004.5 | `ALFRED · COT READ` | — | `AUTOMATED READ` rule-based EN/FR | decision | deviation |
| 18 | t004.5 | read sub-header | `REPORT OF … · FROM THE CFTC FIGURES` | `… · RULES APPLIED TO THE CFTC FIGURES` | honest wording | deviation |
| 19 | t004.5 | footer | `Terminal Data · 19:00:00` + OFFICIAL | + release note | note moved to the badge tooltip | fixed |
| 20 | t013.5 | split tooltip | date / Long / Short, lower right of cursor | same | — | ok |
| 21 | t010.0 | EUR switch instant, red price | same | — | ok |
| 22 | t004.5 | legend marker colours | Other Reportables marker orange, line green | markers = line colours | one palette (spec open point) | deviation |

22 findings: 15 fixed, 2 ok-checked states, 5 deliberate deviations.
