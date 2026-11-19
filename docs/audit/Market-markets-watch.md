# Audit — Market › Markets Watch (+ instrument sheet), 32 frames

Frames `frames/Market/markets-watch/t000.0 … t015.5`. Groups (diff.txt): t0.0–1.0 list idle · t1.5 sheet loading ·
t2.0 loaded except heatmap · t2.5–3.5 loaded · t4.0–9.5 crosshair moving (diff < 0.7) · t10.0–11.5 scroll ·
t12.0–15.5 scrolled, crosshair + chart nav buttons. App states: `DW_WATCH_SHEET`, `DW_WATCH_LOADING=1`,
`DW_WATCH_SCROLL`, `DW_WATCH_HOVER`, `DW_WATCH_STATE` (see WatchPage.h). Screens: `work/watch-shots/audit/`.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t0.0 | list rows | short names `Dow Jones`, `VIX`, `DAX`, `Gasoline (RBOB)` | catalogue names `Dow Jones 30`, `CBOE Volatility`, `DAX 40`, `RBOB Gasoline` | label overrides in `marketLabel()` | fixed |
| 2 | t1.0 | list row hover | hover tint ≈ zebra `#140f05` | `#1b1508` (too bright) | `#171106` | fixed |
| 3 | t1.5 | chart card | skeleton card 46–632 (586 tall), grows to 644 when loaded | 644 from the start | chart height 586 until first data | fixed |
| 4 | t1.5 | right column | compact cards: retail 93, COT 160, data compare 68, then reflow | full heights | compact heights until the chart has data | fixed |
| 5 | t1.5 | retail | plug icon, Rajdhani "Retail positioning unavailable", "DATA SOURCE NOT CONNECTED." (Rajdhani caps, grey) | Inter + spaced mono caps | Rajdhani lines at the reference offsets, plug + spark | fixed |
| 6 | t1.5 | COT loading | "–" at both donut centres, labels at the bottom | extra "CONNECTING · CFTC…" caption, labels too low | caption removed, labels anchored to card bottom | fixed |
| 7 | t1.5 | data compare loading | "Loading..." | "Connecting… / MACRO RELEASES" | initial state Loading | fixed |
| 8 | t1.5 | index performance | spinner + "Computing relative performance..." with NO card frame/title | framed card | frameless while computing, text centred like the frame | fixed |
| 9 | t1.5 | meeting compare | unpriced half "Not connected"; loading half = empty 0–100 % axes | "Connecting…" text | loading half draws empty axes | fixed |
| 10 | t1.5–t3 | sheet placeholders | Rajdhani for every placeholder ("Loading...", "Computing…") | Inter | condensed font | fixed |
| 11 | t2.0 | card titles | Rajdhani semibold, lighter than ours | Bold | DemiBold, spacing 0.7 | fixed |
| 12 | t2.0 | data compare labels | Rajdhani ≈13 px | Inter 11 px | condensed 13 | fixed |
| 13 | t2.0 | legend | `− ≈` pill after the title | missing | pill drawn; click collapses the OHLC values | fixed |
| 14 | t2.0 | drawing bar | crosshair, trend line, fib, XABCD pattern, long position, brush, text, emoji · ruler, zoom · magnet | crosshair, trend, h-line, ruler, zoom, magnet, hide, trash | bar rebuilt in the reference order; every tool works (fib levels, 5-point pattern, position box with target/stop, freehand brush, text input, emoji picker); h-line / hide / remove / reset moved to the plot's right-click menu | fixed |
| 15 | t2.0 | meeting heading | bank name only | name + date | date + pricing source moved to the tooltip | fixed |
| 16 | t3.0/t12 | index-perf and seasonality axes | proportional font (`20.0%`, `0.9%`) | mono | Inter 11 | fixed |
| 17 | t3.0 | technical heatmap | 67 px "Loading..." → 93 px, content shifts 26 px | same | — | ok |
| 18 | t6.0 | crosshair | dashed lines, grey price tag, `08 Oct '26  08:00` time tag, legend follows hovered bar | same | — | ok |
| 19 | t10.5–12 | chart | floating `− + ‹ › ⟲` buttons when the cursor is near the plot bottom | missing | buttons drawn and working (zoom out/in, scroll, reset) | fixed |
| 20 | t10–11.5 | sheet scroll | smooth scroll, header fixed | same | — | ok |
| 21 | t12 | by-month bars | bars inside the axis | bars could exceed the outer tick | symmetric axis covering the largest bar (phase 2) | fixed |
| 22 | t0.0 | counts | 109 markets, 5 pills | 127 markets + BONDS pill (stacked under crypto) | catalogue (real counts) | deviation |
| 23 | t0.0 | stock icons | company logos | monogram discs | trademarks not bundled | deviation |
| 24 | t0.0 | title font | heavy grotesque | JetBrains Mono ExtraBold (closest bundled width) | — | deviation |
| 25 | t2.0 | chart | TradingView embed: `OANDA`, TV logo, bottom bar without `% log auto` | own chart: `YAHOO`, no TV logo, `% log auto` toggles | TradingView → own chart | deviation |
| 26 | t2.0 | data compare row 5 | `PMI Composite` | `Business Confidence (OECD)` | PMI has no free source (MacroStore.pmiNote in the tooltip) | deviation |
| 27 | t2.0 | index badges | 2-letter badges for Nasdaq Comp./ASX/Shanghai/KOSPI | country flags | flags exist for those countries | deviation |
| 28 | t2.0 | volume tile | `Below avg` (OANDA tick volume) | analytics volume read (ETF proxy) | shared engine | data |

Totals: 28 findings — 21 fixed/ok, 7 documented deviations or data differences.
