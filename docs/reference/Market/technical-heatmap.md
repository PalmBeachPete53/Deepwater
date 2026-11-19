# Market › Technical Heatmap

Reference video: `Reference/Market/technical-heatmap.mp4` (42 s, 84 frames at 0.5 s, 1600x966).
Frames: `SCRATCH/frames/Market/technical-heatmap/tNNN.N.jpg`.
All colours below are measured with `tools/pick` (JPEG noise: ±4 per channel). Shared widgets (panel, corner
brackets, KPI card, chips, title strip, badge) are described in `_shared-notes-2.md`.

## 1. Purpose

A one-screen "technical surveillance" scan of every instrument the terminal follows (108 instruments: FX, commodities,
indices, crypto, yields), computed from daily candles. Each instrument is one row of six coloured cells
(Trend, Momentum, Volatility, Volume, Structure, Liquidity); the colour hue says the direction of the read
(bullish teal, bearish red, elevated yellow, subdued grey, liquidity violet) and the colour intensity says its
magnitude. A KPI strip on top counts the instruments in each state, chips filter by asset class and trend, and a
right-hand "signal file" shows the selected instrument as a six-axis radar plus the numeric reads behind each cell.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–11.5 | Idle. Category chip **ALL 108** active, trend chip **ANY TREND** active, table sorted by TREND descending. First row **Sugar** is selected (gold row), the side panel shows `SUGAR 20.26`. The title block cursor blinks, the gold dot in the title pulses. Cursor (yellow arrow) rests near the ALL chip (~x75,y158). |
| 12.0 | Cursor on the ALL chip. |
| 12.5–13.0 | Cursor moves right to the **FX 28** chip; chip text turns gold on hover. |
| 13.5 | Click FX: FX chip becomes the gold filled active chip, ALL becomes inactive. Table is replaced by the 28 FX rows, sorted by trend; the first row **GBP/NZD** becomes selected automatically; side panel switches to `GBP/NZD 2.34593`, `SIGNAL FILE · GBPNZD`. Column widths change (auto-fit to content). |
| 14.0–16.0 | Idle on FX. |
| 16.5 | Cursor on **COMMODITIES 16**: text gold (hover). |
| 17.5 | Click COMMODITIES: gold ring (click ripple, ~22 px radius) around the cursor; table switches to 16 commodity rows, first row **Sugar** selected. The rows **fade in top to bottom** (row 1 full, lower rows still dim, e.g. Wheat at ~40 % opacity). The radar redraws **growing from the centre** (polygon tiny at 17.5, full at 18.0). |
| 18.0–20.5 | Idle on COMMODITIES. |
| 21.0–21.5 | Cursor moves to **INDICES 23**. |
| 22.0 | Click INDICES: 23 rows, first row **Bovespa** (`SIGNAL FILE · IBOV`, `BOVESPA 205,082.56`). |
| 22.5–25.0 | Idle. |
| 25.5–26.0 | Cursor to **CRYPTO 11**. |
| 26.5 | Click CRYPTO: 11 rows, first **AVAX/USD** (`SIGNAL FILE · AVAX`, `AVAX/USD 9.119`). The table is short enough that the **legend line** becomes visible under the last row (y≈918). |
| 27.0–29.5 | Idle. |
| 30.5 | Click **YIELDS 0**: no rows. Table body shows the centred message `NO INSTRUMENT MATCHES THESE FILTERS.`; the legend moves up under it (y≈277); the right panel becomes completely empty (no signal file). |
| 31.0–33.0 | Idle on the empty state. |
| 33.5–37.0 | Cursor moves to the trend chips (ANY TREND, then BULLISH). |
| 38.0–38.5 | Hover **BULLISH**: its text turns gold. |
| 39.0 | Click BULLISH: BULLISH becomes the gold filled chip, ANY TREND inactive. Table still empty (YIELDS has 0 rows). |
| 39.5–41.5 | Cursor drifts away; idle. |

No scrolling, no tooltip, no row hover and no row click is shown in the video (selection only changes by the
automatic "first row" rule when the filter changes).

## 3. Layout (1600x966, page area only)

```
y=0   ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
      │ ∿ ● TECHNICAL SURVEILLANCE — MULTI-SIGNAL SCAN▌                 [● 108 INSTRUMENTS · DAILY…]  │ title strip h=36
y=37  ├──────────────────────────────────────────────────────────────┬─┬────────────────────────────┤
y=48  │ [KPI][KPI][KPI][KPI][KPI][MARKET LEAN]                        │ │ SIGNAL FILE · SUGAR         │
y=117 │                                                               │ │ SUGAR 20.26                 │
y=135 │ [ALL 108|FX 28|COMMOD.|INDICES|CRYPTO|YIELDS] [ANY|BULL|BEAR]  [search ]│ │ [TREND BULLISH]       │
y=161 │                                                               │ │ ───────────── gold rule     │
y=175 │ INSTRUMENT   TREND▾  MOMENTUM  VOLATILITY  VOLUME  STRUCT. LIQ │ │        radar (6 axes)      │
y=210 │ [Sugar      ][cell ][cell    ][cell      ][cell  ][cell ][cell]│ │                             │
      │ … one row every 63.5 px …                                    │ │ TREND      Bullish   +6.69% │
      │                                                               │ │ … 6 read rows …             │
      │                                                               │ │ footnote                    │
y=966 └──────────────────────────────────────────────────────────────┴─┴────────────────────────────┘
      x=10                                                      x=1215 1226 1240                1586
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Title strip | 0 | 0 | 1600 | 37 | bottom border 1 px `#453d1b` at y=36 |
| Left main column | 10 | 48 | 1205 | to bottom | stretches horizontally |
| Vertical divider | 1226 | 37 | 1 | to bottom | `#181714` |
| Right signal panel | 1240 | 48 | 346 | to bottom | fixed width ~360 incl. padding |
| KPI cards (6) | 10 / 212 / 415 / 617 / 818 / 1020 | 48 | ~195 each | 70 | gap ~7 px; cards share the width equally |
| Filter row | 10 | 135 | 1205 | 27 | chip group 1 x10–519, chip group 2 x526–767, search x1012–1215 |
| Table header | 15 | 175 | 1196 | 26 | column titles at y≈188 |
| Table body | 15 | 210 | 1196 | rows | row cell height 58, pitch 63.5 (gap ~5.5 px between rows and ~6 px between cells) |
| Legend line | 10 | last row + ~20 | — | 12 | only visible when rows end above the window bottom |
| Background | — | — | — | — | page `#050402` / `#060503` |

Column widths are content-driven and change per filter (all measured at 1600 wide):

| Filter | Instrument | Trend | Momentum | Volatility | Volume | Structure | Liquidity |
|---|---|---|---|---|---|---|---|
| ALL | 15–290 | 296–409 | 415–548 | 554–709 | 715–872 | 878–1021 | 1027–1211 |
| FX | 15–190 | 196–323 | 330–478 | 485–659 | 665–832 | 838–997 | 1003–1211 |
| COMMODITIES | 15–265 | 271–393 | 400–543 | 549–715 | 721–849 | 855–1006 | 1012–1211 |
| CRYPTO | 15–196 | 202–335 | 341–490 | 496–677 | 683–868 | 874–1039 | 1045–1211 |
| empty (YIELDS) | 15–207 | 213–353 | 360–518 | 525–715 | 722–847 | 854–1028 | 1035–1210 |

When the window grows: the left column stretches (the table spreads its extra width over the columns), the right
panel keeps its width. Rows beyond the window height continue below (108 rows for ALL); the table area must
scroll vertically (no scrollbar visible in the video — use the shared thin scrollbar).

## 4. Components

### 4.1 Title strip
- Left icon: pulse/heartbeat glyph `∿` (line icon) at x12–26, y10–24, gold `#e0d69d` / stroke `#a39827`.
- Dot ● at x34–42, y13–21, gold `#f3dc59` (core `#efd427`), **pulses** (brightness cycles between ~`#6e6819` and
  `#fbe32c`, period ≈ 2 s).
- Title `TECHNICAL SURVEILLANCE — MULTI-SIGNAL SCAN`, monospace, ~10 px, uppercase, letter-spacing ~3 px,
  colour `#6b6a5e` (muted), x50–400, baseline y≈21.
- Block cursor `▌` right after the text (x≈405–414, y≈10–22), gold `#fad717`, **blinks** (on/off steps of ~1–2 s).
- Right badge x1299–1586, y9–27: 1 px gold border, dark gold fill, text `• 108 INSTRUMENTS · DAILY CANDLES · COMPUTED`
  monospace bold ~9 px, uppercase, letter-spacing 1 px, colour `#fff189`/`#fbf1bd`.

### 4.2 KPI cards (6)
Card frame: bg `#0c0801`, 1 px border `#3d3820`–`#453d31` (dim gold), corner brackets top-left and bottom-right
(L-shapes ~10 px, 2 px, colour `#6f6d5d`→`#868064`), top accent bar 2 px at y=48–50 in the card's colour, with a soft
glow below it.

| # | Label (exact) | Value | Accent / value colour |
|---|---|---|---|
| 1 | `BULLISH TREND` | `52` | cyan `#22d3ee` (measured `#00d8ed`, bright core `#7bd7df`), accent `#68bdc6` |
| 2 | `BEARISH TREND` | `50` | red `#f87171` (measured `#f49a9b`/`#d6584c`), accent `#d48c87` |
| 3 | `OVERBOUGHT` | `18` | red, same as 2 |
| 4 | `OVERSOLD` | `23` | cyan, same as 1 |
| 5 | `NEAR LIQUIDITY` | `42` | violet `#a78bfa` (measured `#c4b9e4`, edge `#5b48a6`), accent `#ada1cc` |
| 6 | `MARKET LEAN` | bar | no coloured accent (gold border only) |

- Label: monospace ~10 px, uppercase, letter-spacing ~3 px, `#62665d`, x = card x+11, y≈64.
- Value: monospace (JetBrains-Mono-like) ~30 px, weight 500, with a soft glow of the same colour, x = card x+12,
  y≈78–106.
- Card 6 MARKET LEAN: a 6 px rounded bar at y82–88 from x1032 to x1204: cyan part (`#04dcf5`) from the left for
  the bull share (51 %), red part (`#fe817a`) for the rest; under it `51% BULL` (cyan, left, y≈100) and `49% BEAR`
  (red, right-aligned, y≈100), monospace ~10 px, letter-spacing 1 px.
- KPI counts are global (over all 108) and **do not change** when a category or trend chip is selected.

### 4.3 Filter row
- Chip group 1 (category), x10–519, y135–161, 1 px border `#1a1815`, bg `#060503`. Chips, left to right:
  `ALL 108`, `FX 28`, `COMMODITIES 16`, `INDICES 23`, `CRYPTO 11`, `YIELDS 0`. Label monospace ~10 px uppercase,
  letter-spacing 2–3 px, colour `#95928d`; the count is in a dimmer `#62605a`, same font, after one space.
- Active chip: filled gold `#fed80f`/`#fcd515` rectangle (≈ chip size, no radius or 1 px), text black/dark
  `#1a1400`, count dark too.
- Hover: text turns gold `#fce56b`, no fill.
- Chip group 2 (trend), x526–767: `ANY TREND` (neutral `#95928d`), `BULLISH` (cyan `#8cc3cc`, letter-spacing 3 px),
  `BEARISH` (red `#e59a97`). Same active (gold fill, dark text) and hover (gold text) styles.
- Search box x1012–1215, y136–161: 1 px border `#1a1815`, bg `#050402`, magnifier icon at x≈1024, placeholder
  `SEARCH INSTRUMENT…` monospace ~10 px uppercase letter-spaced `#64635e`. Filters rows by name (not used in video).

### 4.4 Table header
- Row y175–201. Each column title sits on a very dark cell bg (`#0a0806`–`#0f0d0a`, visible in the empty state).
- Titles: `INSTRUMENT` (left-aligned, x≈22), `TREND ▾`, `MOMENTUM`, `VOLATILITY`, `VOLUME`, `STRUCTURE`,
  `LIQUIDITY` (centred in their columns). Monospace ~10 px uppercase, letter-spacing ~3 px, colour `#6b6a66`.
- Active sort column `TREND` is bold, bright cream/gold `#fdfbdd` (gold tint `#f3eacc`), followed by a small gold
  `▾`. Sort is descending by the trend % (EMA gap).

### 4.5 Table rows
Every cell is its own rounded rectangle (radius ~2 px), height 58, 1 px border slightly lighter than its fill,
gap ~6 px to the next cell and ~5.5 px to the next row.

**Instrument cell**: bg `#060503`, border `#1a1815`. Line 1: name, monospace bold ~14 px, white `#fffffd`
(x+9, y≈+20). Line 2: last price, monospace ~10 px, `#5b5a56`, y≈+40; format as quoted by the source
(`20.26`, `111.66`, `205,082.56` with thousands comma, `2.34593`, `0.07916`).

**Signal cells**: two text lines centred: line 1 = state word, monospace bold ~11 px uppercase, letter-spacing
~1.5 px; line 2 = detail, monospace ~9.5 px, letter-spacing ~1 px, same hue but dimmer. Cells with a single word
(Structure, N/A, RANGING) centre it vertically.

Colour of a cell = hue by meaning, fill intensity by magnitude. Measured fills (centre of cell) and text colours:

| Column | State | Detail line format | Fill | Text |
|---|---|---|---|---|
| TREND | `BULLISH` | `+6.69%` (sign, 2 dp) | strong `#11909c`–`#198e99`; weak `#06292d` (e.g. +0.12 %) | `#bff4f8` / `#7bd7df` |
| TREND | `BEARISH` | `-2.08%` | strong `#a4544e`; mid `#753932` | `#ffb4ae` |
| TREND | `NEUTRAL` | `+0.05%` / `-0.02%` | teal-dark `#062c2f` if ≥0, red-dark `#2f1a12` if <0 | cyan or red |
| MOMENTUM | `OVERBOUGHT` | `RSI 79` | `#7b3e3b` | `#e5a19c` |
| MOMENTUM | `OVERSOLD` | `RSI 22` | `#085458`–`#0c5b60` | `#7fd7e2` |
| MOMENTUM | `NEUTRAL` | `RSI 43` | RSI ≤ 50: teal-dark `#0b3738`; RSI > 50: red-dark `#5d2d29`–`#63302e` (deeper as RSI rises) | cyan / red |
| VOLATILITY | `HIGH` | `38.8% ann.` (1 dp) | yellow `#8e7807`–`#957f04` | `#fbe571` |
| VOLATILITY | `NORMAL` | `65.6% ann.` | above own baseline: yellow-dark `#312803`–`#352b04` (yellow text); otherwise grey `#313534` (grey text `#c9c9c9`) | |
| VOLATILITY | `LOW` | `28.1% ann.` | grey `#464b4c` (lighter grey = more subdued) | `#d8d8d8` |
| VOLUME | `ABOVE AVG` | `×1.87 · FXY` | yellow `#9a8509` | `#ffe066` |
| VOLUME | `AVERAGE` | `×1.19 · CRM` | ratio > 1: yellow-dark `#4b4003`; ratio < 1: grey `#313638` | yellow / grey |
| VOLUME | `BELOW AVG` | `×0.69 · DOT-USD` | grey `#363b3c`, lighter when lower (×0.14 ≈ `#5a5b5b`) | `#d0d0d0` |
| VOLUME | `N/A` | — | none (`#0b0a06`, border `#1a1815`) | `#73726e` |
| STRUCTURE | `BULLISH` | — | `#14727e`–`#08777f` | `#bdf3f7` |
| STRUCTURE | `BEARISH` | — | `#8a4542`–`#8b4240` | `#ffb0aa` |
| STRUCTURE | `RANGING` | — | none (`#0b0a06`) | `#7d7c76` |
| LIQUIDITY | `POOL ABOVE` | `↑ +12.93%` | violet-dark `#282533`, a little brighter when closer | `#c5bde0` |
| LIQUIDITY | `POOL BELOW` | `↓ -5.22%` | violet-dark `#252134` | `#c5bde0` |
| LIQUIDITY | `NEAR POOL` | `↑ +0.25%` / `↓ -0.61%` | violet `#5f5380`–`#645788` | `#c3b7e9` |
| LIQUIDITY | `RECENTLY SWEPT` | `↓ -1.28%` / `↑ +9.42%` | yellow `#897406` | `#ffea85` |

Volume detail = `×` + ratio (2 dp) + ` · ` + the ticker whose volume is used (a proxy ETF or the instrument
itself): `BNO` (Brent), `USO` (WTI), `UNG` (Natural gas), `CRM`, `CVX`, `MSFT`, `XOM`, `V`, `SPY`, `QQQ`, `DIA`,
`UUP` (US Dollar Index), `^FTSE`, `^N225`, `FXB`, `FXF`, `FXA`, `FXC`, `FXE`, `FXY`, pair proxies `FXB+FXF`,
`FXC+FXF`, `FXA+FXF`, `FXB+FXA`, `FXB+FXC`, `FXA+FXC`, crypto `BTC-USD`, `AVAX-USD`, `DOT-USD`… (Yahoo-style
tickers). Instruments with no usable volume (Sugar, Heating Oil, Soybeans, Cocoa, Copper, Corn, Coffee, Wheat,
most indices, Bovespa, VIX) show `N/A`.

Thresholds seen: RSI ≥ 70 → OVERBOUGHT (RSI 74, 79, 82, 85, 86), RSI ≤ 30 → OVERSOLD (RSI 30, 28, 27, 24, 21, 17, 13);
trend `NEUTRAL` around 0 (+0.06 %, +0.05 %, -0.02 %, -0.09 %) while +0.13 % (FX) is already BULLISH, so the neutral
band is per-instrument (scaled by its volatility) or about ±0.1 %.

**Selected row**: instrument cell bg gold-tinted `#272102`–`#2b2104`, name in gold `#fff7bc`, price gold-dim;
**every cell** of the row gets a 1 px gold border (`#c9a227`-ish, measured on edges ~`#a08a3a`) on top of its fill.
Only one row is selected. When a filter changes, the first row of the new list is selected.

### 4.6 Legend line
`⊙ LEGEND  ■ BULLISH LEAN  ■ BEARISH LEAN  ■ ELEVATED VS OWN BASELINE  ■ SUBDUED  ■ NEAR A LIQUIDITY POOL  INTENSITY = MAGNITUDE OF THE READ`
- Monospace ~9.5 px uppercase, letter-spacing ~2 px. `⊙ LEGEND` and the last phrase in `#5a574d`/`#615f5b`.
- Each item: 6 px square in its colour, then the label in the same colour, bold: cyan `#2cd5ea`, red `#f48284`,
  yellow `#f8d734`, grey `#b0b1b9`, violet `#afa3e5`.
- Placed ~20 px under the last row (or under the empty message), x starts at 10.

### 4.7 Empty state
`NO INSTRUMENT MATCHES THESE FILTERS.` centred in the table width at y≈238, monospace ~11 px uppercase,
letter-spacing ~3 px, `#6a6967`. Column header row still shown. Right panel blank.

### 4.8 Signal file (right panel, x1240–1586)
- Line 1 (y≈57): `SIGNAL FILE · SUGAR` — monospace ~10 px uppercase, letter-spacing 3 px, `#6b695c`. The token after
  `·` is a short id (`SUGAR`, `GBPNZD`, `IBOV`, `AVAX`), followed by the blinking gold block cursor (seen at
  13.5–14.0, 22.5, 27.0).
- Line 2 (y≈72–102): instrument name, monospace bold ~30 px, uppercase (`SUGAR`, `GBP/NZD`, `BOVESPA`,
  `AVAX/USD`), white `#fffffa`; then the last price, monospace ~17 px white (`20.26`, `2.34593`, `205,082.56`).
- Pill (x1241–1368, y114–135): `TREND BULLISH`, monospace bold ~11 px, letter-spacing 2 px, cyan text `#67e3eb`,
  bg `#042224`, 1 px border cyan `#4c6f70`/`#0e8a96`. (Bearish would be red; neutral grey — not seen.)
- Gold rule y=146–147: 1 px `#a19574` at the left fading out to the right (horizontal gradient to transparent by
  x≈1586).
- **Radar** (hexagon, 6 axes), centre ≈ (1413, 283), outer radius ≈ 88 px. Axes clockwise from the top:
  `Trend` (label at 1413,172), `Liquidity` (1530,233), `Structure` (1532,343), `Volume` (1413,404),
  `Volatility` (1297,343), `Momentum` (1289,233). Labels proportional sans ~11 px, `#b6b5b0`.
  Grid: 5 concentric hexagons + 6 spokes, 1 px, dim gold `#231f0a` (outer ring a bit brighter `#474235`).
  Polygon: stroke 2 px gold `#f5dd70` with glow, fill gold at ~20 % (`#3f3304` on bg), small dots at the vertices.
  Values 0–100 with 50 = neutral (footnote). Momentum = RSI directly (Sugar RSI 43 → 43). Volume N/A → 50.
  Animation: on selection change the polygon grows from the centre in ~0.5 s (ease-out).
- **Read rows** (6), y≈440, 472, 504, 536, 568, 600 (pitch 32), separated by 1 px lines `#181714`:
  label left (x1240) monospace ~10 px uppercase letter-spaced `#595854`: `TREND`, `MOMENTUM`, `VOLATILITY`,
  `VOLUME`, `STRUCTURE`, `LIQUIDITY`; value at x1345, monospace ~14 px, sentence case, coloured like the cell
  (`Bullish` cyan `#22d3ee`, `Neutral` cyan or red, `High` gold `#facc15`, `Low`/`Normal`/`N/A`/`Below avg`/`Average`
  grey `#a3a09a`, `Bearish` red, `Pool above`/`Pool below` violet, `Recently swept` gold); right-aligned detail at
  x≤1586, monospace ~12 px `#aaa9a6`: `+6.69%`, `RSI 43`, `38.8% ann.`, `×0.14 · FXB`, (empty for structure),
  `↓ -1.28%`.
- Footnote (y≈632–664), proportional sans ~10.5 px, `#51504e`, 3 lines wrapped at panel width:
  `Radar: each read on a 0–100 scale where 50 is neutral — trend from the EMA20/EMA50 gap, momentum is RSI(14), volatility and volume against the instrument's own baseline.`

## 5. Interactions

| Element | Action | Result |
|---|---|---|
| Category chip | hover | text gold |
| Category chip | click | becomes active (gold fill); table filtered to that asset class; sort kept (trend desc); first row selected; rows fade in top→bottom (stagger ~30 ms/row, ~300 ms each); radar grows from centre; side panel updates |
| `YIELDS 0` | click | empty state (message + legend), right panel cleared |
| Trend chip (ANY/BULLISH/BEARISH) | hover / click | same styles; filters rows to trend state (combined with category) |
| Column header | click (inferred) | sort by that column; active header bold gold with `▾` |
| Row | click (inferred) | select row → gold row + signal file |
| Search | type (inferred) | filter by instrument name |
| Click feedback | any chip click | a gold ring around the cursor for ~0.3 s (may be the screen recorder's click highlight, see open questions) |
| Title cursor / dot | idle | blinking block cursor, pulsing dot |

## 6. Data

| Field | Samples | Format | Update | Notes |
|---|---|---|---|---|
| Instrument name | Sugar, Brent Crude, Bovespa, Salesforce, Chevron, Microsoft, AVAX/USD, Exxon Mobil, WTI Crude, DOT/USD, Visa, Heating Oil, GBP/NZD, USD/CHF, AUD/NZD, USD/CAD, GBP/CHF, CAD/CHF, AUD/CHF, GBP/AUD, GBP/CAD, EUR/NZD, AUD/CAD, USD/JPY, Natural Gas, Soybeans, Cocoa, Copper, Gasoline (RBOB), Corn, Coffee, Wheat, VIX, AEX, US Dollar Index, SMI, S&P 500, Nasdaq Composite, Nasdaq 100, S&P/TSX, FTSE 100, Nikkei 225, Dow Jones, LTC/USD, BTC/USD, LINK/USD, BNB/USD, SOL/USD, ADA/USD, ETH/USD, XRP/USD, DOGE/USD | text | static list | stocks (Salesforce, Microsoft…) are in ALL but not in any visible category chip count (FX 28 + Comm 16 + Ind 23 + Crypto 11 + Yields 0 = 78 < 108) |
| Last price | 20.26, 111.66, 205,082.56, 2.34593, 81,247.26 | source precision, thousands comma | daily close (DAILY CANDLES) | |
| Trend % | +6.69%, -0.02%, -2.08% | signed, 2 dp | daily | EMA20/EMA50 gap in % (footnote) |
| Momentum | RSI 43, RSI 79 | integer | daily | RSI(14) |
| Volatility | 38.8% ann., 186.1% ann. (VIX) | 1 dp, annualised | daily | realised vol vs own baseline → LOW / NORMAL / HIGH |
| Volume | ×0.75 · BNO, ×1.87 · FXY | ratio 2 dp + ticker | daily | volume vs own average; proxy ETF for FX/commodities |
| Structure | BULLISH / BEARISH / RANGING | word | daily | swing structure (HH/HL vs LH/LL) |
| Liquidity | POOL ABOVE ↑ +12.93%, NEAR POOL ↓ -0.61%, RECENTLY SWEPT ↓ -1.28% | arrow + signed % 2 dp | daily | distance to nearest equal-high/low pool (same engine as Liquidity page) |
| KPI counts | 52, 50, 18, 23, 42 | integer | recomputed with data | counts over all 108 |
| Market lean | 51% BULL / 49% BEAR | integer % | recomputed | bullish / (bullish + bearish) trend counts: 52/(52+50)=51 % |
| Badge | 108 INSTRUMENTS · DAILY CANDLES · COMPUTED | | | "COMPUTED" = derived in-app, no vendor name |

No vendor/source name is printed on the page.

## 7. Key frames

| Frame | Shows |
|---|---|
| `t012.0.jpg` | ALL, Sugar selected, full table and radar |
| `t014.0.jpg` | FX filter, GBP/NZD selected, BELOW AVG greys and NEAR POOL violets |
| `t017.5.jpg` | Commodities just clicked: row fade-in and radar growing from centre, click ring |
| `t018.0.jpg` | Commodities settled |
| `t022.5.jpg` | Indices, Bovespa, ABOVE AVG yellow, VIX 186.1 % |
| `t027.0.jpg` | Crypto, 11 rows + legend line |
| `t031.0.jpg` | YIELDS 0 empty state |
| `t039.5.jpg` | BULLISH chip active, empty |

## 8. Open questions

- Exact thresholds for trend NEUTRAL, volatility LOW/NORMAL/HIGH and volume BELOW/AVERAGE/ABOVE (relative to which
  baseline window?).
- How the radar maps Trend, Volatility, Volume, Structure and Liquidity to 0–100 (only "50 = neutral" is stated).
- The 30 instruments in ALL that belong to none of the shown categories (stocks): is there a hidden "STOCKS" chip,
  or do they only appear under ALL?
- Behaviour of row hover and row click (not shown), sort on other columns, search box.
- Whether the gold ring at click is part of the app or the screen recorder.
- Vertical scroll of 108 rows: whole page or table only.
- Colour for `TREND BEARISH` / `TREND NEUTRAL` pill in the side panel (only bullish seen).
