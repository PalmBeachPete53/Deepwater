# Positioning › COT

Reference video: `Reference/Positioning/cot.mp4` (≈18 s, frames every 0.5 s at 1600x966).
Shared style (panels, section headings, segmented controls, tables, footer, fonts): see `_shared-notes.md`.

## 1. Purpose

"COT Advanced": the CFTC Commitments of Traders report for one market at a time, by trader category, as charts and
tables. The user picks an asset class (DEVISES / COMMODITIES / INDICES), a market (currency buttons USD, EUR, GBP,
JPY, CHF, CAD, AUD, NZD for DEVISES) and a history range (6M / 1Y / 2Y / 3Y). A **SUMMARY** section shows the
market's price (daily candles) with 5-year open interest, the weekly long:short split of one TFF category as 100 %
stacked bars, and a 3D donut of that category's current long:short. An **ADVANCED** section shows the net position
history per category (Legacy, TFF, Nonreportable) as line charts, the full Legacy and TFF report tables, 3D
long:short donuts per category, net-% bars per category, and an AI-written read ("ALFRED · COT READ") with each
category's net position placed in its 52-week range. Source: CFTC, weekly, official ("CFTC · FREE, WEEKLY,
OFFICIAL"; footer badge `OFFICIAL`).

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | Top of the page, market USD, range 1Y, category DEVISES. Cursor over the price chart near the last price: crosshair (dashed vertical at x≈695 and horizontal), price label `100.64` on the right axis. |
| 0.5 | Cursor moves down: crosshair label `99.66` (vertical line at x≈721). |
| 1.0–4.0 | Mouse-wheel scroll down through the whole page, smooth (the frames show ghosting from the smooth-scroll animation): SUMMARY → ADVANCED (LEGACY NET, LEGACY REPORT COT, LEGACY LONG:SHORT SECTOR, LEGACY STATS) → TRADERS IN FINANCIAL FUTURES NET, TFF REPORT COT, TFF LONG:SHORT SECTOR, TFF STATS → NONREPORTABLE (SMALL TRADERS) NET → ALFRED · COT READ. Scroll ≈200–400 px per 0.5 s. Bottom reached at t=4.0 (max scroll ≈1246 px). |
| 4.0–5.5 | Idle at the bottom (USD Alfred read: "Large specs and funds near extremes, commercials pull back"). |
| 6.0–8.5 | Scroll back up to the top (≈200–250 px per 0.5 s). |
| 9.0 | Hover on the price chart: crosshair at `30 Sep '26` (date label on the time axis) / `101.34` (price label). |
| 9.5 | Cursor on the `EUR` currency button (hover). |
| 10.0 | **EUR selected**: the EUR button becomes active, USD inactive; every panel shows EUR data at once (no visible loading state): price `96.34 ▼ -0.11%` in red, OI `877,940 +3,399`, weekly split / donut `61% SHORT`, tables, etc. |
| 10.5–11.0 | Idle (cursor on EUR). |
| 11.5 | Cursor near the `TradingView` toggle (hover only, no click). |
| 12.0–13.0 | Cursor on the `Leveraged Funds` segment of the weekly split (a click ring from the screen recorder at t=12.0; the segment was already active, nothing changes). |
| 13.5 | Hover on a weekly-split bar: that bar gets a dotted white vertical outline and a **tooltip** appears: `2025-12-23` / `Long: 65%` / `Short: 35%`. |
| 14.0–17.0 | Scroll down to the bottom again (EUR data). |
| 17.0–17.5 | Idle at the bottom (EUR Alfred read: "Commercials near extreme long, dealers at year-end extreme"). |

## 3. Layout

Page area 1600x966. Fixed parts: page header (y 0–36), selector row (y 37–75), footer (y 938–966). Everything in
between scrolls vertically (viewport y 76–938, ≈862 px; content height ≈2108 px measured from y=76). No scrollbar
visible (overlay scrollbar or hidden).

```
y=0    COT Advanced  CFTC · FREE, WEEKLY, OFFICIAL                               [DEVISES|COMMODITIES|INDICES]
y=37   [🇺🇸 USD][🇪🇺 EUR][🇬🇧 GBP][🇯🇵 JPY][🇨🇭 CHF][🇨🇦 CAD][🇦🇺 AUD][🇳🇿 NZD]                         [6M|1Y|2Y|3Y]
y=75   ─────────────────────────────────────────────────────────────────────────────────────────────── (scroll ↓)
y=105  ● SUMMARY ──────────────────────────────────────────────────────────────────────────────────────
y=125  ┌ PRICE … (x 7–796, h 495) ───────────┐┌ WEEKLY LONG:SHORT SPLIT (804–1290)┐┌ LONG:SHORT (1298–1592)┐
       │ 100.42 ▲ +0.09%      [Unweighted|TV]││ [Dealer|AM|LevFunds|Other|Nonrep] ││                        │
       │ daily candles (y 160–465)           ││ 52 stacked bars (y 190–595)       ││   3D donut  55% LONG    │
       │ ■ OPEN INTEREST … (y 500–620)       ││                                   ││  ● Long 55% ● Short 45% │
y=620  └─────────────────────────────────────┘└───────────────────────────────────┘└─────────────────────────┘
y=649  ● ADVANCED ─────────────────────────────────────────────────────────────────────────────────────
y=670  ┌ LEGACY NET (7–796, h≈392) ──────────┐┌ LEGACY REPORT COT (804–1592, h≈172) ────────────────────┐
       │                                     │├ LEGACY LONG:SHORT SECTOR (804–1195) ┬ LEGACY STATS (1203–1592)┤ y≈848
       ├ TRADERS IN FINANCIAL FUTURES NET ───┤│  3 donuts (2 + 1)                    │  3 bar rows            │
       │  (y≈1067 content, h≈388)            │├ TFF REPORT COT (h≈222) ─────────────────────────────────────┤ y≈1198
       ├ NONREPORTABLE (SMALL TRADERS) NET ──┤├ TFF LONG:SHORT SECTOR ──────────────┬ TFF STATS ─────────────┤ y≈1428
       │  (y≈1468 content, h≈706)            ││  5 donuts (2 + 2 + 1)                │  5 bar rows            │
       │                                     │├ ALFRED · COT READ (h≈264) ──────────────────────────────────┤ y≈1910
y≈2174 └─────────────────────────────────────┘└───────────────────────────────────────────────────────────┘
y=938  Terminal Data · 19:00:00                                                                  [● OFFICIAL]
```
(y values after 966 are content coordinates at scroll 0.)

| Region | x | y | w | h |
|---|---|---|---|---|
| Page header | 0 | 0 | 1600 | 36 (bottom border 1 px `#27230f`) |
| Asset-class segmented control | 1392 | 7 | 197 | 22 |
| Selector row | 0 | 37 | 1600 | 38 (bottom border 1 px `#28281a` at y=75) |
| Currency buttons | 10 + n·59 | 43 | 52–56 | 25, gap ≈6 px |
| Range segmented control | 1482 | 43 | 107 | 25 |
| SUMMARY heading | 16 | 99 | — | 12; rule from x≈105 to 1584 at y=106 |
| PRICE panel | 7 | 125 | 789 | 495 |
| WEEKLY LONG:SHORT SPLIT panel | 804 | 125 | 486 | 495 |
| LONG:SHORT panel | 1298 | 125 | 294 | 495 |
| ADVANCED heading | 16 | 643 | — | 12 |
| LEGACY NET | 7 | 670 | 789 | ≈392 |
| TRADERS IN FINANCIAL FUTURES NET | 7 | ≈1067 | 789 | ≈388 |
| NONREPORTABLE (SMALL TRADERS) NET | 7 | ≈1460 | 789 | ≈714 |
| LEGACY REPORT COT | 804 | 670 | 788 | ≈172 |
| LEGACY LONG:SHORT SECTOR | 804 | ≈848 | 391 | ≈344 |
| LEGACY STATS | 1203 | ≈848 | 389 | ≈344 |
| TFF REPORT COT | 804 | ≈1198 | 788 | ≈222 |
| TFF LONG:SHORT SECTOR | 804 | ≈1426 | 391 | ≈462 |
| TFF STATS | 1203 | ≈1426 | 389 | ≈462 |
| ALFRED · COT READ | 804 | ≈1910 | 788 | ≈264 |
| Footer | 0 | 938 | 1600 | 28 |

Columns: left column 789 px, right column 788 px, gutter 8 px, outer margins 7–8 px. In the SUMMARY row the three
panels are ≈50 % / 30 % / 18 %. Width growth: the columns stretch proportionally; chart plot areas stretch; table
columns spread. Gaps between stacked panels ≈6–8 px.

## 4. Components

### 4.1 Page header
- `COT Advanced`: proportional condensed font (Rajdhani-like) 13 px semi-bold, near-white `#f8f5ec`, x=10, centre
  y=17.
- Subtitle `CFTC · FREE, WEEKLY, OFFICIAL`: 9 px uppercase, letter-spacing ≈1 px, very dim `#3d3931`, x=95.
- Asset-class segmented control (right): `DEVISES` (active: solid yellow `#fed307` bg, dark text, bold), `COMMODITIES`,
  `INDICES` (inactive: `#807d74` text, transparent). 10 px uppercase, letter-spacing ≈1 px. Thin outer border
  `#2b2a1d`.

### 4.2 Selector row
- **Currency buttons** (8): flag icon (≈16x11 px emoji-style flag) + code (`USD`, `EUR`, `GBP`, `JPY`, `CHF`, `CAD`,
  `AUD`, `NZD`), 11 px uppercase. Inactive: transparent bg, 1 px border `#2b2a1a`, text `#a09d94`. Active: bg
  `#302404`, border `#43381e` (warm gold), text gold `#d5c788`.
- **Range segmented control** `6M | 1Y | 2Y | 3Y`, mono 11 px; active `1Y`: bg `#242020` (neutral dark grey), text
  `#e8e3da`; inactive `#79776f`; outer border `#2b2a1d`.

### 4.3 Section headings
`● SUMMARY`, `● ADVANCED`: small yellow dot (5 px, `#f4df8d`), text 11 px bold uppercase, letter-spacing ≈2.5 px,
yellow `#f8e4a9`/`#facc15`-ish; then a 1 px horizontal rule `#25241b` to the right edge.

### 4.4 Panels (common)
bg `#080603`, 1 px border `#211e08`–`#242418`, title bar ≈28 px with a 1 px separator `#24250f` under it. Title:
10 px bold uppercase, letter-spacing ≈0.5 px, `#b2aca1`; optional subtitle right after it in very dim `#3c3b34`
(e.g. `LEVERAGED FUNDS · 2026-10-20`, `TFF`). No corner brackets on this page.

### 4.5 PRICE panel
- Title row: `PRICE` + subtitle `USD index · unweighted · daily candles (COT measures ICE US Dollar Index (cash))`
  (proportional 9.5 px, `#59564f`). For EUR: `EUR index · unweighted · daily candles (COT measures EUR FX Futures
  (6E))`. Right: toggle `Unweighted` (active: text `#dfcc80` on `#383004`, radius 2) | `TradingView` (inactive,
  mono 10 px `#727067`).
- Last value: `100.42` mono 18 px white `#fffffc`, then a filled triangle `▲` cyan (`▼` red when down) and
  `+0.09%` mono 11 px cyan (`-0.11%` red).
- Candle chart (x 8–725, y 160–465): daily candles, ≈2 months visible (x labels `Sep  9  17  Oct  9  18`; month names
  in bold-ish `#929086`, 11 px mono). Up candles cyan `#22d3ee`-ish (rendered `#2ad4ea`), down red `#ef5f5f`;
  bodies ≈9 px wide, wicks 1 px. No grid lines. Right price axis (x≈738–782): mono 11 px `#878680`, 2 decimals
  (`102.00 … 98.00`). Last price: dotted horizontal line in the candle colour across the plot + label (`100.42` dark
  text on cyan `#2cd4ec`; `96.34` white on red when down).
- Crosshair: dashed vertical + horizontal lines (`#5c5b45`, olive-grey, 3 px dash), price label on the axis and
  date label on the time axis: cream text `#ebe4ce` on dark olive `#483c04`, e.g. `100.64`, `30 Sep '26`.
- **Open interest strip** (y 500–620, separated by a 1 px rule `#29291b`): header `■ OPEN INTEREST 49,382 -2,434 ·
  CFTC WEEKLY · 2026-10-20` (mono 9.5 px, letter-spaced; label `#9c9990`, value white-ish, change in gold
  `#ddcea7`, rest dim `#4d4a43`; yellow square 6 px). Bars: ≈270 weekly bars (≈2 px wide, 1 px gap) from 2021-08 to
  the latest week (5 years regardless of the range selector), olive gold `#bca84e` (max `#c5ba60`); baseline at the
  axis minimum, not zero. Right axis `80K 60K 40K 20K` (EUR: `1.0M 800K 600K 400K`), 11 px `#a6a7a7`. Time axis
  `2021-08 2022-01 2022-06 2022-11 2023-04 2023-08 2024-01 2024-06 2024-11 2025-04 2025-09 2026-01 2026-06`
  (proportional 11 px `#979797`, every ≈5 months).

### 4.6 WEEKLY LONG:SHORT SPLIT panel
- Title `WEEKLY LONG:SHORT SPLIT` + dim `TFF`.
- Category segmented row (centred, y≈152–168): `Dealer/Intermediary`, `Asset Manager`, `Leveraged Funds` (active:
  text `#c3b273` on `#383004`, 1 px border `#3e3109`), `Other Reportables`, `Nonreportable`; 9.5 px proportional,
  inactive `#615f58`.
- Chart (x 812–1281, y 190–595): 52 weekly 100 % stacked bars (one per week in the range; ≈7 px wide, 2 px gap,
  separated by a dark 1 px line), cyan bottom `#18d8f0` = long %, red top `#e8605c` = short %. No axes, no labels.
- Hover: dotted white vertical line through the hovered bar; tooltip box (≈88x72 px, bg `#0c0804`, 1 px border
  warm brown `#271e0f` (re-measured on t013.5), radius 3): `2025-12-23` / `Long: 65%` / `Short: 35%` (proportional 12 px, `#d3d2cd`), placed to the
  lower right of the cursor.

### 4.7 LONG:SHORT panel
- Title `LONG:SHORT` + dim subtitle `LEVERAGED FUNDS · 2026-10-20` (category = the one selected in the weekly
  split; date = report date).
- **3D donut** centred at ≈(1445, 370): outer ellipse ≈208x125 px, inner hole ≈90x50 px, extrusion ≈20 px; top
  faces cyan `#14c0d0`→`#23edf9` (long) and coral red `#f46868` (short), side walls darker (`#04606c` cyan side,
  dark red side); long starts at 12 o'clock going clockwise; soft dark drop shadow. Centre text: the dominant side
  `55%` (cyan, bold 22 px) over `LONG` (8 px `#5d605f`); for EUR `61%` (red) over `SHORT`.
- Legend at y≈479: `● Long 55%` (cyan, mono 12 px bold) and `● Short 45%` (red).

### 4.8 Net line charts (LEGACY NET, TRADERS IN FINANCIAL FUTURES NET, NONREPORTABLE (SMALL TRADERS) NET)
- Legend centred under the title: glowing diamond markers + proportional 11 px `#a7a49e` labels:
  - Legacy: `Commercials` (cyan marker), `Large Speculators` (yellow-green marker), `Small Traders` (lavender
    marker).
  - TFF: `Dealer/Intermediary` (blue), `Asset Manager` (yellow-green), `Leveraged Funds` (lavender), `Other
    Reportables` (orange).
  - Nonreportable: `Nonreportable` (lavender/white marker).
- Lines (1.5 px, no fill, no points): Legacy Commercials cyan `#22d3ee`-ish (rendered `#88c4c9`), Large
  Speculators yellow `#facc15`-ish (`#eade9f`), Small Traders purple `#a78bfa`-ish (`#a589c1`). TFF: Dealer cyan,
  Asset Manager yellow, Leveraged Funds purple, Other Reportables green (`#4ade80`-ish, muted). Nonreportable cyan.
  (The legend marker colours do not match the line colours for Other Reportables: orange marker vs green line.)
- Y axis left (x≈20–62), proportional 11 px `#706d66`, thousands separator, 7 ticks e.g. `30,000 20,000 10,000 0
  -10,000 -20,000 -30,000` (EUR legacy `200,000 … -300,000`; TFF EUR `600,000 … -600,000`; Nonreportable USD
  `4,000 … -2,000`, EUR `60,000 … 0`). Horizontal grid lines 1 px `#201f14`; the zero line slightly brighter.
- X axis: month abbreviations `Nov Dec 2026 Feb Mar Apr May Jun Jul Aug Sep Oct` (year shown in bold instead of
  `Jan`), proportional 11 px `#706d66`. 52 weekly points for 1Y.

### 4.9 Report tables (LEGACY REPORT COT, TFF REPORT COT)
- Title row: title left; right: `OI 49,382 -2,434 · 120 traders` (mono 11 px, `OI` and `traders` dim `#4c4b45`,
  change red/green).
- Header row 1: `CATEGORY` (left), group labels right-aligned over the last column of their group: `LONG` (cyan
  `#22d3ee`), `SHORT` (red), `SPREAD`, `NET` (grey `#847f76`); 8.5 px bold uppercase letter-spaced.
- Header row 2 (8 px, `#57544c`, right-aligned): `POSITIONS CHG %OI TRDRS` (long) `POSITIONS CHG %OI TRDRS`
  (short) `POSITIONS` (spread) `L-S` (net).
- Rows (pitch ≈25.7 px, 1 px separators `#242415`): label proportional 11 px `#9e9990`; numbers mono 12.5 px
  right-aligned, comma thousands. Positions / %OI / traders in near-white `#f3f0e7`; CHG and NET coloured: negative
  red `#ef5f5f`-ish, positive green `#4ade80`-ish muted (rendered `#6f9276`), no `+` sign; `—` (em dash, dim) for
  not applicable. %OI 1 decimal.
- Column right edges (x): 1002, 1075, 1130, 1180, 1253, 1326, 1381, 1431, 1502, 1585.
- Legacy rows: `Commercials`, `Large Speculators`, `Total Reportable`, `Small Traders`.
- TFF rows: `Dealer/Intermediary`, `Asset Manager`, `Leveraged Funds`, `Other Reportables`, `Total Reportable`,
  `Nonreportable`.

USD values (report 2026-10-20):

| Legacy | L pos | chg | %OI | trdrs | S pos | chg | %OI | trdrs | spread | net |
|---|---|---|---|---|---|---|---|---|---|---|
| Commercials | 11,737 | -2,152 | 23.8 | 10 | 28,751 | 34 | 58.2 | 7 | — | -17,014 |
| Large Speculators | 32,422 | -174 | 65.7 | 76 | 17,378 | -2,299 | 35.2 | 30 | 1,466 | 15,044 |
| Total Reportable | 45,625 | -2,379 | 92.4 | 88 | 47,595 | -2,318 | 96.4 | 40 | — | -1,970 |
| Small Traders | 3,757 | -55 | 7.6 | — | 1,787 | -116 | 3.6 | — | — | 1,970 |

| TFF | L pos | chg | %OI | trdrs | S pos | chg | %OI | trdrs | spread | net |
|---|---|---|---|---|---|---|---|---|---|---|
| Dealer/Intermediary | 5,123 | -41 | 10.4 | 5 | 25,102 | 365 | 50.8 | — | 0 | -19,979 |
| Asset Manager | 19,273 | -1,213 | 39.0 | 18 | 2,253 | -69 | 4.6 | 7 | 1,107 | 17,020 |
| Leveraged Funds | 18,063 | -945 | 36.6 | 45 | 14,880 | -2,052 | 30.1 | 14 | 523 | 3,183 |
| Other Reportables | 1,536 | -145 | 3.1 | 17 | 3,730 | -527 | 7.6 | 11 | 0 | -2,194 |
| Total Reportable | 45,625 | -2,379 | 92.4 | 88 | 47,595 | -2,318 | 96.4 | 40 | — | -1,970 |
| Nonreportable | 3,757 | -55 | 7.6 | — | 1,787 | -116 | 3.6 | — | — | 1,970 |

EUR (Legacy): Commercials 522,461 / 1,100 / 59.5 / 142 / 417,566 / 7,430 / 47.6 / 106 / — / 104,895; Large
Speculators 226,421 / 3,492 / 25.8 / 79 / 360,945 / 443 / 41.1 / 69 / 43,294 / -134,524; Total Reportable 792,176 /
2,201 / 90.2 / 234 / 821,805 / 5,482 / 93.6 / 194 / — / -29,629; Small Traders 85,764 / 1,198 / 9.8 / — / 56,135 /
-2,083 / 6.4 / — / — / 29,629. OI 877,940 +3,399 · 345 traders. EUR TFF: Dealer 57,445 / 2,099 / 6.5 / 16 / 213,163 /
-6,946 / 24.3 / 15 / 8,280 / -155,718; Asset Manager 499,827 / -2,849 / 56.9 / 117 / 309,406 / 6,648 / 35.2 / 47 /
44,704 / 190,421; Leveraged Funds 124,951 / 4,930 / 14.2 / 58 / 193,791 / 5,769 / 22.1 / 60 / 26,650 / -68,840; Other
Reportables 26,117 / -684 / 3.0 / 22 / 21,609 / 1,306 / 2.5 / 18 / 4,202 / 4,508.

### 4.10 LONG:SHORT SECTOR panels (Legacy: 3 donuts; TFF: 5 donuts)
Small 3D donuts (≈110x75 px incl. extrusion), same style as 4.7 but without centre text, in a 2-column grid with the
odd last one centred. Under each: `L 29%  S 71%` (mono 12 px bold; `L` and its % cyan, `S` and its % red), then
the category name uppercase 9 px letter-spaced `#5d5a54` (`COMMERCIALS`, `LARGE SPECULATORS`, `SMALL TRADERS`,
`DEALER/INTERMEDIARY`, `ASSET MANAGER`, `LEVERAGED FUNDS`, `OTHER REPORTABLES`, `NONREPORTABLE`).

### 4.11 STATS panels (LEGACY STATS, TFF STATS)
Rows (pitch ≈32 px): label proportional 11 px `#615f59` (x 1213); a diverging bar on a dark track (x 1360–1523,
h≈14, track `#1b1a14`, centre x≈1441): negative values draw red `#ed615e` leftwards from the centre, positive cyan
`#29d9ea` rightwards, length ∝ |value| (100 % = half the track); value right-aligned at x≈1583, mono 13 px bold,
signed (`-42%`, `+30%`, `+36%`), red / cyan.

### 4.12 ALFRED · COT READ
- Header: sparkle/AI icon (yellow `#ecdc90`), `ALFRED · COT READ` (10 px bold uppercase `#afaca2`); right: `REPORT OF
  2026-10-20 · FROM THE CFTC FIGURES` (9 px uppercase very dim `#3c3a33`) and an `EN | FR` toggle (EN active: yellow
  bg, dark text; FR `#726f68`).
- Left column (x 813–1205): headline proportional 15 px semi-bold white `#fcfaf3`; paragraph proportional 12 px
  `#8c8881`, line-height ≈17.5 px; then 3 bullet blocks, each with a 2 px left bar `#383218` and 12 px text.
- Right column (x 1221–1583): `NET POSITION VS ITS 52-WEEK RANGE` (9 px uppercase `#7c7a70`), 7 rows (pitch ≈21.5
  px): label `Legacy · Commercials`, `Legacy · Large Speculators`, `Legacy · Small Traders`, `TFF ·
  Dealer/Intermediary`, `TFF · Asset Manager`, `TFF · Leveraged Funds`, `TFF · Other Reportables` (11 px
  `#8b8881`); value signed mono 12.5 px (`+15,044` cyan, `-17,014` red); a range track (x 1462–1582, h 6, `#202018`,
  rounded) with a thumb 4x14 px at the percentile position: white `#e5e5e6` normally, yellow `#efdd7c` with glow when
  in the top or bottom 10 % ("crowded"). Footer row: `52-WEEK LOW` (left, dim), `● CROWDED (TOP / BOTTOM 10%)`
  (centre, yellow `#ccb880`), `52-WEEK HIGH` (right, dim).

USD text: headline "Large specs and funds near extremes, commercials pull back"; paragraph "Large speculators and
asset managers maintain heavily bullish positioning near 81% of their 52-week ranges, while leveraged funds hit 82%
of range this week. Commercials reduced net shorts by 2186 contracts, easing from their crowded short position at
19% of range. Dealers remain deeply short at 16% of range."; bullets "Large specs (15044) and asset managers (17020)
both hovering near 81% of 52-week highs—crowded bullish camps", "Leveraged funds jumped to 82% of range (+1107
contracts this week), adding to long positioning near extremes", "Commercials lightened shorts by 2186 contracts;
dealers also net short but TFF dealers remain at only 16% of range".

EUR text: headline "Commercials near extreme long, dealers at year-end extreme"; paragraph "Legacy commercials hold
104,895 net contracts near their 52-week peak at 98% of range, having sold 6,330 contracts this week. TFF dealers
remain pinned at their worst position (-155,718, 100% of range) while asset managers just hit their 52-week low,
reducing longs by 9,497 contracts."; bullets "Commercials crowded long near 52-week extreme (98% range), trimmed
position but remain heavily committed", "Dealers stuck at exact year-end worst positioning (-155,718), no relief
despite 9,045 contract reduction", "Asset managers hit fresh 52-week low (0% range), dumped 9,497 longs; leveraged
funds also near lows at 0% range".

### 4.13 Footer
`Terminal Data · 19:00:00` (10 px `#53514b`) left; right badge `● OFFICIAL`: cyan text `#78ccd4` bold condensed
10 px, bg `#10241c`, 1 px border `#273b38`, radius 2. The time is a fixed publication time (19:00:00), not a ticking
clock.

## 5. Interactions

- **Asset class** (DEVISES / COMMODITIES / INDICES): swaps the market button row (only DEVISES seen).
- **Market buttons**: hover (t=9.5) shows no visible change before the click; click switches every panel to the
  market (t=10.0), instantly, no animation seen.
- **Range** 6M / 1Y / 2Y / 3Y: changes the number of weekly bars / line points (52 for 1Y). Not exercised.
- **Unweighted | TradingView** toggle: switches the price panel between the app's unweighted index candles and a
  TradingView chart. Not exercised.
- **Price chart**: crosshair with price and date labels (no OHLC tooltip seen).
- **Weekly split category segments**: choose which TFF category feeds the stacked bars and the LONG:SHORT donut.
- **Weekly split bar hover**: dotted outline + tooltip (date, Long %, Short %).
- **Page scroll**: smooth wheel scrolling of everything below the selector row.
- **EN | FR** toggle in the Alfred read: switches the language of the generated text.
- No hover effects on table rows were seen.

## 6. Data

| Field | Sample | Format | Update | Source |
|---|---|---|---|---|
| Price (index / futures) | 100.42 ▲ +0.09 %; 96.34 ▼ -0.11 % | 2 dp, % 2 dp signed | daily candles (last one live?) | "USD index · unweighted" = app-computed index; COT contract named (ICE US Dollar Index, EUR FX Futures 6E) |
| Open interest | 49,382 -2,434; 877,940 +3,399 | integer, signed change | weekly (Tuesday data, published Friday) | CFTC |
| Weekly long % per TFF category | 65 % / 35 % on 2025-12-23 | stacked % | weekly | CFTC TFF, long/(long+short) |
| Current long:short | 55 % / 45 % (LevFunds USD), 39 / 61 (EUR) | integer % | weekly | computed |
| Net per category history | -30,000…30,000 | integer contracts | weekly | CFTC Legacy / TFF / Nonreportable, long − short |
| Report table cells | see 4.9 | positions, change, %OI (1 dp), traders | weekly | CFTC (Legacy futures-only and TFF) |
| Long:short sector | L 29 % S 71 % | integer % | weekly | long/(long+short), computed |
| Stats | -42 %, +30 % | signed integer % | weekly | (long − short)/(long + short), computed (verified: Commercials (11,737 − 28,751)/40,488 = −42 %) |
| 52-week range position | thumb at 0–100 % | percentile in min–max of the last 52 net values | weekly | computed; crowded if ≤10 % or ≥90 % |
| Alfred read | headline, paragraph, 3 bullets | text EN/FR | weekly (per report) | AI-generated from the CFTC figures |
| Report date | 2026-10-20 | ISO date | weekly | CFTC |
| Footer | 19:00:00 | HH:MM:SS | fixed | "Terminal Data", badge OFFICIAL |

Consistency checks that hold in the data: Total Reportable long = sum of category longs + spreads (45,625 =
11,737 + 32,422 + 1,466); OI = Total Reportable long + Nonreportable long (49,382); Nonreportable = Small Traders.

## 7. Key frames

| Frame | Shows |
|---|---|
| `t000.0.jpg` | Top of page, USD, crosshair on price chart |
| `t002.0.jpg` | Legacy net chart, legacy table, sector donuts, stats, TFF net start |
| `t002.5.jpg` | TFF net chart, TFF table, TFF stats |
| `t004.5.jpg` | Bottom: nonreportable chart, TFF donuts, Alfred read (USD) |
| `t009.0.jpg` | Crosshair with date label `30 Sep '26` |
| `t010.0.jpg` | EUR selected (red price, 61 % SHORT donut) |
| `t013.5.jpg` | Weekly split bar tooltip |
| `t015.0.jpg` | EUR legacy + TFF tables and charts |
| `t017.5.jpg` | EUR Alfred read with yellow crowded thumbs |

## 8. Open questions

- Content of COMMODITIES and INDICES (market buttons, which contracts).
- What `TradingView` mode of the price panel looks like; what "unweighted" index means for EUR (the EUR price 96.34
  is an index, not EUR/USD).
- Whether the last candle is live or daily close only.
- Legend marker colours vs line colours mismatch in the TFF chart (Other Reportables orange marker, green line;
  Dealer marker blue vs cyan line) — decide one palette.
- The EUR Alfred read calls -155,718 "worst position" at "100% of range" while the thumb is at the right end:
  confirm the meaning of the range position for net-short categories.
- Behaviour of the range selector on the OI strip (it shows 5 years at 1Y).
- Whether the Alfred read is generated locally (local model) or by a cloud model, and when it refreshes.

## 9. Builder's check against the frames (2026-11-14)

Re-opened t000.0, t002.0, t002.5, t004.5, t013.5 and t017.5. Corrected above: the split tooltip's border colour.
Verified and left unchanged: header/selector geometry, the three summary panels (50/30/18 %), the content y
positions of every panel (e.g. TFF NET at content y≈1067 = page 588 at scroll 479 on t002.0), the Legacy/TFF table
values and column right edges, the trader-category colours (and the legend-marker mismatch for Other Reportables /
Dealer, kept as an open point — the build uses one palette: markers = line colours), the donut directions (long
clockwise from 12 o'clock), the stats bar track (x 1360–1523) and the 7-row range block of the read.
Data note: the "ICE US Dollar Index" COT is still in the CFTC Socrata datasets under contract code `098662` (named
"USD INDEX - ICE FUTURES U.S." since 2022; a search on "DOLLAR INDEX" only finds the old rows). The 2026-10-20 OI of
the reference (49,382) matches the CFTC rows exactly.
