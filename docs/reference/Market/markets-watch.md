# Market › Markets Watch

Reference video: `Reference/Market/markets-watch.mp4` (16 s, 32 frames at 0.5 s, 1600x966).
Frames: `frames/Market/markets-watch/tNNN.N.jpg`. Colours measured with `tools/pick` (JPEG noise ±4).
User description: *"More than 100 live markets: currencies, indices, commodities, stocks and crypto."*

## 1. Purpose

A one-screen board of 109 live markets split in five category columns (Forex 28, Indices 23, Commodities 16,
Stocks 30, Crypto 12), with a search box, category filter pills, four "today" summary cards (breadth, average move,
leader, laggard), a change-period switch (1D / 1W / 1M) and an ordering switch (Default / Performance / A-Z).
Clicking any row opens a full-page **instrument detail sheet** (shown for EUR/USD): a TradingView advanced chart,
retail positioning pie, COT positioning donuts, a macro "data compare" table of the two currencies, a technical
heatmap strip, a long-run currency index performance chart, central-bank meeting probability bars for both
currencies, a seasonality-by-month bar chart and a seasonal year-path chart.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–1.0 | Idle on the list page (all categories). Cursor sits on the FOREX header area (≈240,425) then lands on the EUR/USD row. Row at y≈455–487 shows the hover tint. Values do not change during the second. |
| 1.5 | Click on **EUR/USD** → detail sheet opens instantly, replacing the whole page area (no slide animation visible between two frames). **Loading state**: chart area shows grey skeleton candles + a skeleton title bar; right column shows RETAIL POSITION "Retail positioning unavailable / DATA SOURCE NOT CONNECTED.", COT POSITION with "–" placeholders, DATA COMPARE "Loading...", TECHNICAL HEATMAP "Loading...", INDEX PERFORMANCE spinner "Computing relative performance...", MEETING COMPARE with the ECB half "Not connected" and the FED half's empty axes. |
| 2.0 | TradingView chart loaded (4h candles, OANDA), retail pie (84 % long), COT donuts, Data compare table, meeting bars filled. TECHNICAL HEATMAP still "Loading...". Index performance chart drawn. |
| 2.5–3.0 | TECHNICAL HEATMAP loads: the card grows from ~67 px to ~93 px tall and everything under it shifts down 26 px (layout reflow, no animation). TradingView clock bottom-right ticks every second (`23:48:09` → `23:48:15 UTC-5`). |
| 3.5–9.5 | Cursor moves over the chart: TradingView crosshair (dashed lines, price label on the right axis, date label on the time axis `08 Oct '26 08:00`), legend OHLC switches to the hovered bar. Live last candle and the cyan price line tick slightly (diff ≈0.5 per frame). |
| 10.0–11.5 | Smooth scroll of the sheet (motion blur in 10.5/11.0); cursor near the chart bottom shows TradingView's floating zoom/scroll buttons (− + ‹ › ⟲). The sheet header bar (EUR/USD ×) stays fixed. |
| 12.0–15.5 | Scrolled by ≈424 px: technical heatmap strip, index performance + meeting compare, seasonality by month + year path are visible. Idle (14.0: tiny change, the cursor). |

## 3. Layout

### 3.1 List page (1600x966, page bg `#040302`, faint warm glow top-right `#171302` at 1300,20 fading to bg)

```
y   0 ┌────────────────────────────────────────────────────────────────────────────────────────────┐
 27   │ MARKET WATCH (eyebrow)                                                                    │
 52   │ MARKETS (big title)   109 LIVE MARKETS · QUOTES REFRESH EVERY 20S                         │
119   │ ───────────── gold rule fading to the right ─────────────────────────────────────────────  │
137   │ [🔍 Search 109 markets — EUR/USD, Gold, Nvidia, Bitcoin…                             [/]]  │ h46
196   │ [ALL 109][FOREX 28][INDICES 23][COMMODITIES 16][STOCKS 30][CRYPTO 12]                      │ h28
236   │ ┌BREADTH·TODAY──┐┌AVERAGE MOVE·TODAY┐┌LEADER·TODAY────┐┌LAGGARD·TODAY───┐                  │ h81
330   │ CHANGE OVER [1D|1W|1M]   ORDER [DEFAULT|PERFORMANCE|A-Z]                                   │ h24
364   │ ┌FOREX────┐┌INDICES──┐┌COMMODITIES┐┌STOCKS───┐┌CRYPTO───┐                                 │
      │ │ rows…   ││ rows…   ││ rows…     ││ rows…   ││ rows…   │  (5 equal columns, page scrolls) │
966   └────────────────────────────────────────────────────────────────────────────────────────────┘
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Left/right margin | 20 / 21 | | | | content spans x 20–1579 |
| Eyebrow | 20 | 21 | ~95 | 12 | |
| Title + subtitle | 20 | 52 | 1560 | 42 | subtitle baseline y≈94, starts x 253 |
| Gold rule | 20 | 119 | →1595 | 1 | |
| Search box | 21 | 137 | 1557 | 46 | |
| Category pills | 21 | 196 | ~634 | 28 | gap 8–10 |
| Stat cards | 20 / 412 / 803 / 1195 | 236 | 385 each | 81 | gap 7; stretch equally |
| Controls row | 20 | 330 | ~490 | 24 | |
| Category panels | 20 / 333 / 647 / 960 / 1274 | 364 | ≈306 each | content height | gap 8; stretch equally |

The five panels have their own natural height (rows × row height); the whole page scrolls vertically (no panel-level
scroll bar seen). Width grows → the five columns and four cards share the width equally; heights fixed.

### 3.2 Detail sheet (full page area, opened over the list)

```
y   0 ┌ EUR/USD ─────────────────────────────────────────────────────────────────────────── × ┐ h37 fixed
 46   │┌ TradingView chart (x8–1200) ───────────────────────────┐┌ RETAIL POSITION (1208–1592) ┐ │
      ││ toolbar / drawing bar / candles / price axis           ││ 3D pie                      │ │ y46–316
      ││                                                        │├ COT POSITION ───────────────┤ │ y324–503
      ││                                                        │├ DATA COMPARE ───────────────┤ │ y512–690
690   │└────────────────────────────────────────────────────────┘└─────────────────────────────┘ │
700   │┌ TECHNICAL HEATMAP (x8–1592) 6 tiles ─────────────────────────────────────────────────┐ │ h93
801   │┌ INDEX PERFORMANCE (x8–795) ───────────┐┌ MEETING COMPARE (x803–1592) ─────────────────┐ │ h277
1088  │┌ SEASONALITY · BY MONTH (x8–795) ──────┐┌ SEASONALITY · YEAR PATH (x803–1592) ─────────┐ │ h293
      └──────────────────────────────────────────────────────────────────────────────────────────┘
```

- Sheet header: y 0–37, bg `#0f0a02`, bottom border 1–2 px `#242418`/`#2a221b`. Stays fixed when the content scrolls.
- Content: outer margin 8 px, gap 8 px between cards (gap bg `#040300`). Right column fixed width 384 px; chart
  column stretches. Bottom pairs split 50/50 (787 + 8 + 789).
- Content y positions above are for the loaded, unscrolled state (t3.0). Scrolled state t12.0 (offset ≈424):
  heatmap y 278–370, index perf / meeting y 378–655, seasonality pair y 665–958.

## 4. Components — list page

### 4.1 Header
- Eyebrow `MARKET WATCH`: monospace 10 px, letter-spacing ≈3 px, `#5e5956`.
- Title `MARKETS`: heavy sans (Inter/Archivo Black look) ~44 px cap height, weight 800–900, letter-spacing ≈2 px,
  `#f1f1ed`. x 20–230, y 52–92.
- Subtitle on the title baseline: `109` bold white (`#fff9f6`) + ` LIVE MARKETS · QUOTES REFRESH EVERY 20S`
  monospace 13 px, letter-spacing ≈2.5 px, `#8a857c`. The count is the number of markets shown.
- Gold rule y 119: 1 px horizontal line, gradient left→right `#d6d0b7` (x25) → `#aca48f` (x300) → `#726d5e` (x600)
  → `#2d2c24` (x900) → `#151006` (x1500) → transparent. 1 px dark-gold halo above/below (`#2d2600`, `#43381e`).

### 4.2 Search box
- Rect x 21–1578, y 137–183, 1 px border `#1a1b14`, bg `#030302`, square corners.
- Magnifier icon at x 44 (gold `#d4b020`/`#544306`, 16 px).
- Placeholder `Search 109 markets — EUR/USD, Gold, Nvidia, Bitcoin…` monospace 17 px, `#474745`, x 62.
- Right: keyboard hint box `/` (x 1540–1563, y 148–172), 1 px border `#201a15`, text `#5a5650` — pressing `/`
  focuses the search (inferred from the hint; not shown).

### 4.3 Category pills (y 196–224)
| Pill | x | Text |
|---|---|---|
| ALL | 21–101 | `ALL` + count `109` |
| FOREX | 107–191 | `FOREX 28` |
| INDICES | 199–301 | `INDICES 23` |
| COMMODITIES | 311–446 | `COMMODITIES 16` |
| STOCKS | 455–547 | `STOCKS 30` |
| CRYPTO | 557–655 | `CRYPTO 12` |

- Active (ALL): solid gold bg `#fdda11`, label black bold monospace 12 px spacing 1.5 px; count in a darker gold
  inner badge (`#c19e00` bg, black text).
- Inactive: bg `#060303`, 1 px border `#1e1a14`, label `#87847e`, count in a small dark badge (bg `#1d1a14`, text
  `#8a8780`), 4 px padding. No radius (≤2 px).

### 4.4 Stat cards (y 236–317)
Common: bg `#0d0802`, 1 px border `#454225` (dim olive-gold, measured `#343112`–`#454225`), label monospace 10 px
letter-spacing ≈2 px `#463c30`–`#5a5348` at y 252, padding 12 px.

1. **BREADTH · TODAY** — 3 px bar at y 268–270 from x 32 to 394: cyan segment (`#37e1f3`/`#6acdd7`) proportional to
   up count, then red (`#f17763`/`#cd7d72`), touching (no gap). Below (y 285, monospace 13 px): left `▲ 33 up`
   cyan `#2dd5ee` (filled triangle), right `75 down ▼` red `#eb7063`. (33 + 75 = 108; one flat market not shown.)
2. **AVERAGE MOVE · TODAY** — value `-0.41%` monospace bold 24 px, red `#d67a73` (cyan when ≥ 0), sub-text
   `across 109 markets shown` proportional 10 px `#3b372f`.
3. **LEADER · TODAY** — symbol `ADBE` bold sans 22 px white `#f0f0ee` + `+7.59%` monospace 13 px cyan `#2dd5ee`;
   sub `Adobe` 10 px `#3b372f`.
4. **LAGGARD · TODAY** — `META` + `-9.40%` red; sub `Meta Platforms`.

### 4.5 Controls row (y 330–354)
`CHANGE OVER` label (monospace 10 px spaced, `#5a564e`) + segmented control `1D | 1W | 1M` (group border
`#202212`, x 117–221); `ORDER` label + segmented `DEFAULT | PERFORMANCE | A-Z` (x 284–508). Active segment: gold
fill `#fdda11`, black bold text; inactive: transparent, `#87847e`. Active at t0: `1D` and `DEFAULT`. The change
column header (`1D`) and all % values follow the CHANGE OVER selection.

### 4.6 Category panels (y 364 → content height)
- Panel: bg `#0c0601`, 1 px border `#433d22`; gold corner brackets (`#877c50`, L 8 px × 1–2 px) inside the
  top-left and bottom-right corners.
- Header (y 371–392): gold bullet `•` + title `FOREX` (monospace bold 13 px, spacing 2 px, `#e6d35f`), count `28`
  as small superscript `#48453c`; right-aligned `avg -0.14%` (`avg` `#5a564e`, value red/cyan) monospace 11 px.
- Column header (y 400–416): `MARKET` left, `LAST` right-aligned at x≈240 (panel-relative ≈220), `1D` right-aligned
  at panel right − 8. Monospace 9 px spacing 1.5 px `#57534c`. Thin separator `#1a150a` under it.
- Rows: zebra — odd rows `#0c0601`/`#0d0802`, even rows `#140f05` (very subtle). Hover = same as even tint or
  slightly lighter. Separator lines not visible.
  - FX / indices / commodities / crypto rows: ≈33 px pitch (centres y 437, 470, 504, 537 …). Icon 16x11 at x+8
    (pairs: two overlapping flags; indices: country flag or 2-letter grey badge `IX`, `AS`, `SS`, `KO`;
    commodities: coloured glyphs — gold disc, silver disc, blue oil barrel (WTI, Brent), flame (Natural gas),
    copper disc, then 2-letter grey badges `PL PA CO WH SO SU CO CO HE GA`; crypto: coin logos), symbol bold
    monospace 13 px `#edeae3` (names truncated with `…`, e.g. `Nasdaq Compos…`, `Shanghai Compo…`).
  - Stock rows: ≈49 px pitch, circular company logo 16 px, ticker bold 13 px + company name proportional 9–10 px
    `#3b372f` underneath.
  - LAST: monospace 13 px white, right-aligned, thousands separator `,`, decimals per instrument (FX 5 dp, JPY pairs
    3 dp, indices 2 dp, commodities 2 dp, crypto 2–4 dp).
  - Change: monospace 12 px, sign always shown, 2 dp + `%`; red `#e0605a` (measured `#c86c69`–`#e05a52`), cyan
    `#2dd5ee`; `-0.00%` keeps the minus.

## 5. Components — detail sheet

### 5.1 Sheet header
`EUR/USD` bold sans 13 px white `#ebe4da` at (10,18); close `×` at x 1578 `#6e6b64` (hover presumably lighter).

### 5.2 Chart card (x 8–1200, y 46–690)
Embedded **TradingView Advanced Chart** (dark theme), black bg `#000000`, toolbars `#0a0a0a`, 1 px frame
`#2c2c1b` with tiny gold glints in the corners.
- Top toolbar (y 52–88): timeframes `30m 1h 4h` (4h active: grey rounded bg, white text) + dropdown chevron; bar
  type icons (bars, candles active, area) + chevron; `⊕` compare; `Indicators`; camera (snapshot) at x 1175.
- Left drawing toolbar (x 20–60): cursor/crosshair (active), trend line, horizontal/fib, patterns, measure, brush,
  `T` text, emoji, ruler, zoom, magnet, lock, eye, trash.
- Legend (85,115): flag + `Euro / U.S. Dollar · 4h · OANDA` 16 px white + OHLC
  `O1.11934 H1.12044 L1.11895 C1.12032 +0.00097 (+0.09%)` with values cyan when the bar is up.
- Candles: up `#00c4df` (body), wick lighter `#b4feff`; down `#ff6972`/`#dd5c67`.
- Price scale right (x 1135–1192): `1.15000 … 1.11000`, step 0.005; current price tag cyan bg `#22c8e0` with the
  price `1.12032`, dotted cyan horizontal price line.
- Time axis (y 628): `23 25 29 Oct 4 7 9 13 15 18 21 23 27` and a gear icon; TradingView logo bottom-left.
- Bottom bar (y 655–680): `1D 5D 1M 3M 6M YTD 1Y 5Y All` + calendar-range icon; right: clock `23:48:09 UTC-5`
  ticking every second, `%`, `log`, `auto`.
- Loading skeleton (t1.5): dark grey `#141414` placeholder bars + 136x10 skeleton title at (22,66).

### 5.3 RETAIL POSITION (x 1208–1592, y 46–316)
- Card title bold sans 10 px uppercase `#c8c4bc` with letter-spacing 0.5 px, title row 28 px with bottom rule
  `#272618`; card bg `#080603`, border `#211d12`. Same style for every card on the sheet.
- 3D tilted pie (ellipse ~200x110, extruded 18 px): LONG cyan (top `#1ed7ee`, side `#0d626d`) 84 %, SHORT red (top
  `#ff7171`, side darker `#8a3030`) 16 %. Labels on slices: `16%` / `SHORT` and `84%` / `LONG` white bold.
- Legend under the pie (y 300): `● Long 84%` cyan `#4ccfde`, `● Short 16%` red, 10 px.
- Unconnected state: plug icon + `Retail positioning unavailable` (12 px grey) + `DATA SOURCE NOT CONNECTED.`
  (monospace 9 px spaced, dimmer).

### 5.4 COT POSITION (y 324–503)
Two 3D tilted donuts side by side (centres x 1306 and 1494): red arc (short, `#f06563`) + cyan arc (long, `#10d3e7`),
dark side walls. Under each: `L 39%  S 61%` (L cyan, S red) and the currency code `EUR` grey caps; right: `L 55%
S 45%`, `USD`. Placeholder state shows `–`.

### 5.5 DATA COMPARE (y 512–690)
Table, header `INDICATOR | EUR | USD` (monospace 9 px `#767465`), rows 26 px with rules `#252117`:
| INDICATOR | EUR | USD |
|---|---|---|
| Interest Rate | 2.65% | 4.00% |
| GDP (Quarter q/q) | 0.5% | 2.2% |
| Inflation Rate (Annual) | 3.8% | 3.7% |
| Unemployment Rate | 6.4% | 4.2% |
| PMI Composite | 51.9 | 57.6 |
Labels proportional 11 px `#8a8a80`, values monospace 12 px `#ede8df`, right-aligned. No winner colouring here.

### 5.6 TECHNICAL HEATMAP (x 8–1592, h 93)
Six equal tiles in a row (x 15–274, 278–536, 540–798, 802–1060, 1064–1323, 1327–1585; y 737–785, gap 4):
label monospace 8 px caps `#4d4845` + value 12 px.
| Tile | Value | Style |
|---|---|---|
| TREND | Bearish | red tile bg `#471b18`, border `#451f1e`, text `#f05040` |
| MOMENTUM | Neutral | grey tile `#272523`, text `#c8c4bc` |
| VOLATILITY | Normal | grey |
| VOLUME | Below avg | grey |
| STRUCTURE | Bearish | red |
| LIQUIDITY | Pool above | grey |
(bullish values would use the cyan equivalent — not shown.)

### 5.7 INDEX PERFORMANCE (x 8–795, h 277)
Line chart of the two currency indices rebased to 0 % at the start (~2017):
- Legend centred top: `—○— USD` (blue) `—○— EUR` (gold), line + hollow white circle marker.
- Y axis left: `20.0% 15.0% 10.0% 5.0% 0.0% -5.0% -10.0%` (step 31 px), labels 10 px `#a1a1a1`; X axis `2018 2020
  2022 2024 2026`; grid faint `#1a1a17`.
- USD line `#5a86d0` (measured `#4673c2`–`#73a0d7`), EUR line `#f0c860` (`#f6c953`–`#edc46f`), 1.5–2 px, no fill.
- Loading: spinner + `Computing relative performance...` centred.

### 5.8 MEETING COMPARE (x 803–1592)
Two halves, `ECB` (x 803–1197) and `FEDERAL RESERVE` (x 1198–1592), each a bar chart of next-meeting outcome
probabilities: categories (monospace small, x axis) `-50+bps`, `-25bps`, `No change`, `+25bps`, `+50+bps`
(inferred order, labels tiny); Y axis `100% 80% 60% 40% 20% 0%`. Values above bars: ECB `<1% <1% 98.6% 1.3% <1%`,
Fed `<1% <1% 93.5% 6.5% <1%`. Main bar light grey `#aab1ba`, the hike bar teal `#00bfbe`, near-zero bars as thin
lines. Value labels monospace `#8a877e`. Unconnected state: `Not connected` centred.

### 5.9 SEASONALITY · BY MONTH (x 8–795) — grey suffix `2011-2026`
Bars per month Jan–Dec, y ticks `0.9% 0.6% 0.3% 0.0% -0.3% -0.6% -0.9%`. Positive teal `#168c98`, negative dark red
`#a2544d`, **current month (Oct) highlighted** salmon `#ff827e` with orange outline. Approx values: Jan −0.01,
Feb −0.33, Mar +0.02, Apr +0.62, May −0.85, Jun +0.20, Jul −0.06, Aug +0.35, Sep −0.53, Oct −0.15, Nov −0.24,
Dec +0.70.

### 5.10 SEASONALITY · YEAR PATH (x 803–1592) — `2011-2026`
Legend top-right `▬ Average 2011-2026` (gold) `▬ 2026` (cyan). Y `3% … -6%` step 1 %, X `Jan … Dec`. Average line
gold `#efd560` with faint fill to zero (`#181808`); current-year line cyan `#57c8c7` ending at today (~Oct 23,
≈ −4.6 %). Zero line olive.

## 6. Interactions

| Element | Action | Result |
|---|---|---|
| Market row | hover | row bg lightens (`#140f05`), cursor pointer |
| Market row | click | detail sheet opens full-page with skeleton/loading states; each card loads independently |
| Search box | type / `/` | filters rows across all panels (not shown; counts in subtitle/placeholder update) |
| Category pill | click | only that category's panel(s) shown; active pill gold (not shown) |
| CHANGE OVER | click | switches % change period 1D/1W/1M for every row, header label and summary cards (not shown) |
| ORDER | click | DEFAULT (curated order) / PERFORMANCE (sorted by change) / A-Z (not shown) |
| Sheet × | click | closes the sheet, back to the list (not shown) |
| Chart | hover | TradingView crosshair, axis labels, legend values of hovered bar |
| Chart | wheel / drag | TradingView zoom/pan (native) |
| Sheet | wheel | smooth vertical scroll of cards; header fixed |
| TECHNICAL HEATMAP | load | card height grows when data arrives (reflow) |

Live: list prices refresh every 20 s (stated in the subtitle). Detail chart is live (TradingView), clock ticks each
second.

## 7. Data

| Field | Sample | Format | Update | Source on screen |
|---|---|---|---|---|
| Market count | 109 | int | on load | — |
| FX last / change | EUR/USD 1.12230 −0.13 % | 5 dp (JPY 3 dp), signed 2 dp % | 20 s | — |
| Indices | S&P 500 7,623.68 −0.47 %; VIX 19.45 +5.48 % | 2 dp, `,` thousands | 20 s | — |
| Commodities | Gold 4,010.00 −3.61 %; WTI 103.44 −0.21 % | 2 dp | 20 s | — |
| Stocks | AAPL Apple 347.73 +1.24 % | 2 dp | 20 s | — |
| Crypto | BTC/USD 81,289.00 −0.11 %; DOGE/USD 0.0792 | 2–4 dp | 20 s | — |
| Panel avg | FOREX avg −0.14 %, INDICES −0.76 %, COMMODITIES −2.27 %, STOCKS +0.39 %, CRYPTO +0.12 % | mean of the rows' changes | 20 s | computed |
| Breadth | 33 up / 75 down | counts | 20 s | computed |
| Average move | −0.41 % across 109 | mean change | 20 s | computed |
| Leader / Laggard | ADBE +7.59 % / META −9.40 % | max / min change | 20 s | computed |
| Chart | EUR/USD 4h candles | OHLC | live | TradingView, OANDA |
| Retail position | Long 84 % / Short 16 % | % | minutes | (broker sentiment feed; "DATA SOURCE NOT CONNECTED" when absent) |
| COT | EUR L 39 % S 61 %, USD L 55 % S 45 % | % of non-commercial | weekly | CFTC (implied) |
| Data compare | rate, GDP q/q, CPI y/y, unemployment, PMI composite | %, index | on release | economic calendar |
| Technical heatmap | Trend Bearish, Momentum Neutral, Volatility Normal, Volume Below avg, Structure Bearish, Liquidity Pool above | words | per bar | computed (same engine as Market › Technical Heatmap) |
| Index performance | USD, EUR indices % since 2017 | % | daily | computed |
| Meeting compare | ECB 98.6 % no change, Fed 93.5 % | % | daily | rate-probability source |
| Seasonality | by month 2011–2026, year path avg vs 2026 | % | daily | computed from daily history |

Full row lists seen (default order):
- **FOREX (28)**: EUR/USD 1.12230 −0.13, GBP/USD 1.33900 −0.01, USD/JPY 157.160 −0.32, USD/CHF 0.82263 −0.05,
  USD/CAD 1.40140 −0.14, AUD/USD 0.70144 −0.03, NZD/USD 0.56906 +0.01, EUR/GBP 0.83816 −0.12, EUR/JPY 176.370 −0.46,
  EUR/CHF 0.92321 −0.18, EUR/AUD 1.59990 −0.11, EUR/CAD 1.57280 −0.27, EUR/NZD 1.97210 −0.15, GBP/JPY 210.430 −0.33,
  GBP/CHF 1.10150 −0.05, GBP/AUD 1.90890 +0.02, AUD/JPY … (rest below the fold).
- **INDICES (23)**: S&P 500, Nasdaq 100 28,957.53, Dow Jones 52,215.16 +0.41, DAX 25,181.65 −1.72, FTSE 100
  10,690.36 −1.76, VIX, Nikkei 225 62,693.16 −3.43, Hang Seng 23,093.37 −4.03, CAC 40 7,782.82 −1.08, Russell 2000
  2,775.90 +2.00, US Dollar Index 101.75 −0.22, Euro Stoxx 50 6,068.22 −1.25, Nasdaq Composite 25,955.34 −0.71,
  ASX 200 8,795.20 −0.13, Shanghai Composite 3,690.48 −1.08, KOSPI 5,550.30 −2.08, Nifty 50 …
- **COMMODITIES (16)**: Gold, Silver 53.96 −5.29, WTI Crude, Brent Crude 111.66 +1.93, Natural Gas 3.05 −0.36,
  Copper 6.50 −0.63, Platinum 1,555.80 −5.06, Palladium 1,046.50 −6.24, Corn 456.00 −2.36, Wheat 625.00 −4.62,
  Soybeans 1,269.50 +1.28, Sugar 20.26 −1.51, Coffee 306.00 −3.07, Cocoa 5,560.00 −2.23, Heating Oil 4.80 −2.36,
  Gasoline (RBOB) 3.13 −2.00.
- **STOCKS (30)**: AAPL, MSFT 524.51 −0.86, GOOGL 320.21 −5.98, AMZN 248.00 −0.80, NVDA 210.96 −1.48, META 642.62
  −9.40, TSLA 372.22 +4.93, BRK-B 542.19 +1.91, JPM 331.58 −2.40, V 407.92 +1.32, MA 623.60 +1.00, … ADBE.
- **CRYPTO (12)**: BTC/USD, ETH/USD 2,397.55 +0.29, SOL/USD 103.69 +0.24, XRP/USD 1.31 −0.00, BNB/USD 740.11 +1.22,
  ADA/USD 0.2140 +1.06, DOGE/USD 0.0792 +0.78, DOT/USD 1.11 −0.94, LINK/USD 11.90 −0.36, AVAX/USD 9.13 +0.18,
  LTC/USD 57.88 −1.02, MATIC/USD 0.1262.

## 8. Key frames

| Frame | State |
|---|---|
| `t001.0.jpg` | list page, all categories, idle |
| `t001.5.jpg` | detail sheet loading skeletons |
| `t002.0.jpg` | sheet loaded except technical heatmap |
| `t003.0.jpg` | sheet fully loaded (unscrolled) |
| `t006.0.jpg` | TradingView crosshair + axis labels |
| `t010.5.jpg` | scrolling, TradingView floating nav buttons |
| `t012.0.jpg` | scrolled: heatmap, index perf, meeting compare, seasonality pair |

## 9. Open questions

- Behaviour of search, category pills, CHANGE OVER, ORDER and the sheet `×` (never clicked in the video).
- Category filter when one category is chosen: one wide panel or same column width?
- Exact x-axis category labels of MEETING COMPARE (text is ~6 px in the video) and which central bank pair is shown
  for non-FX instruments (indices, stocks, crypto) — probably the sheet adapts (no COT/retail for stocks?).
- What the sheet shows for non-FX instruments in general (data compare EUR/USD is pair-specific).
- Retail positioning source (needs a broker sentiment feed; the loading frame says "DATA SOURCE NOT CONNECTED").
- Whether the list flashes cells on price change (no flash seen in 1 s of list time).
- Rows beyond the first screen of each panel (FOREX shows 16 of 28).
