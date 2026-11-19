# Positioning › Retail Sentiment

Reference video: `Reference/Positioning/retail-sentiment.mp4` (18.5 s, frames every 0.5 s at 1600x966).
Shared style (panel, KPI card, pills, badges, footer, fonts): see `_shared-notes.md`.

## 1. Purpose

Retail traders' long / short positioning on every tracked instrument (34 instruments: 28 FX pairs plus
commodities and crypto), and how it moves. The page is a "crowd surveillance" board: six KPI cards summarise the
crowd (how many instruments are crowded long / short, the most extreme long and short, the number of tracked
positions, the aggregate lean), then one table row per instrument with a long/short bar, the long and short %, the
24 h change of the long %, the number of open positions and a status badge (CROWDED LONG, LEANING SHORT, …).
Clicking a row opens a large **instrument detail modal** (EUR/USD in the video): average retail entry prices, a
full TradingView-style 1 h candlestick chart, a 3D long/short pie, a statistics table, history charts (long/short %
history, CFTC open interest, volume and position history) and a generated narrative paragraph. The source named on
screen is the **Myfxbook community** (retail outlook) plus **CFTC** (open interest, weekly) and **OANDA** (price
chart).

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–0.5 | Page idle, list view. Mouse cursor over the GBP/USD row (x≈700, y≈295): the row is highlighted (warm background `#1b1503`). Footer clock `Terminal Data · 23:17:55`. |
| 1.0 | Cursor moves up to the EUR/USD row (x≈790, y≈255): EUR/USD row highlighted, GBP/USD back to normal. |
| 1.5 | Click on EUR/USD → the **detail modal** is open (the video is zoomed into the modal from here on: the left ~25 % of the modal is outside the recording). Chart area still loading: grey placeholder vertical bars (skeleton, `#141414`-ish on `#0a0a0a`). The long/short history chart is drawing in (bars sweep in from the left, the right part still shorter); the open-interest bars are rising. |
| 2.0 | Candlestick chart loaded (EUR/USD 1 h, OANDA). Modal clock `23:33:10 UTC-5`. Everything is static except the clock. |
| 2.5–3.5 | Idle. The chart clock ticks `23:33:13`, `:16`, `:19` (the recording is sped up ≈6x: 3 s of clock per 0.5 s of video). |
| 4.0–8.5 | Cursor moves over the candle chart: **crosshair** (dashed white/grey lines) with a price label on the right axis (`1.12594`, `1.12528`, `1.12397`, `1.12331`, …) and a date label on the time axis (`22 Oct '26  07:00`, `21 Oct '26  19:00`, `20 Oct '26  19:00`, …); the OHLC legend at the top follows the hovered candle (e.g. `O1.12339 H1.12356 L1.12321 C1.12330 -0.00010 (-0.01%)` in red for a down candle). |
| 8.5 | Cursor at the right price axis: crosshair at `25 Oct '26 19:00`, legend back to the last candle. |
| 9.0–10.0 | Cursor leaves the chart to the right column (over "Average Long Price"): the crosshair disappears, and a `USD ⌄` currency dropdown appears at the top of the price scale and `A` / `L` (auto / log) buttons at its bottom (TradingView price-scale hover controls). |
| 10.5–12.0 | Cursor back into the chart, low (y≈610–680): crosshair price `1.11621`, `1.11444`, `1.11435`, `1.11729`. |
| 12.5–14.5 | **Wheel scroll down inside the modal** (smooth, ≈160 px per frame): the chart moves up, the history charts row (long/short % history + "OPEN INTEREST — EURO FX"), then the second history row ("each side indexed to its own range" + "POSITION"), then the yellow narrative box and the grey footnote come into view. Max scroll ≈750 px. |
| 15.0–16.0 | Idle at the bottom of the modal. |
| 16.5–18.0 | Wheel scroll back up to the top of the modal (≈160–260 px per frame). End state = same as t=2.0 with crosshair at `22 Oct '26 09:00`. |

The modal was never closed in the video. The list view and the modal clocks are 15 min apart (23:17 vs 23:33):
the recording was cut between the click and the modal.

## 3. Layout

### 3.1 List view (1600x966, page area only)

```
y=0    ┌ title bar (h≈34): [icon] ● RETAIL POSITIONING — CROWD SURVEILLANCE ▌                              ┐
y=45   │[CROWDED LONG ][CROWDED SHORT][MOST CROWDED LONG][MOST CROWDED SHORT][TRACKED POSITIONS][CROWD LEAN] │ 6 KPI cards h≈82
y=127  │                                                                                                    │
y=134  │[🔍 Filter…][All|FX|Commodities|Crypto][☰ Custom]                [Majors first|Most extreme|Most active|A-Z]│ toolbar h≈26
y=176  │SYMBOL     POSITIONING              LONG      SHORT      24H Δ LONG       POSITIONS             STATUS │ header
y=199  │──────────────────────────────────────────────────────────────────────────────────────────────────── │
y=202  │FX · 28   (group row, h≈21)                                                                          │
y=223  │EUR/USD    [██████████████████▌▓▓]  84%       16%          flat            41,240      [↗ CROWDED LONG] │ rows, pitch ≈35.7
 …     │ … 20 rows visible (to CAD/JPY at y≈919) — the table scrolls                                         │
y=938  ├──────────────────────────────────────────────────────────────────────────────────────────────────── │
y=944  │Terminal Data · 23:17:55                                                                   [● DELAYED]│ footer h≈26
y=966  └                                                                                                    ┘
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Title bar | 0 | 0 | 1600 | 34 | bottom border 1 px `#2b261a`-ish; bg warm-dark gradient `#110b03` (left) → `#211a05` (right). |
| KPI card 1 CROWDED LONG | 10 | 45 | 257 | 82 | 6 cards, gap ≈7–8 px, outer margin 10 px left/right. Card x: 10, 274, 539, 804, 1068, 1333; each ≈257 wide (stretch equally). |
| Toolbar | 10 | 134 | 1580 | 26 | Filter input 10–132; category segmented control 138–369; Custom button 373–446; sort segmented control 1241–1590 (right-aligned). |
| Table header | 0 | 176 | 1600 | 22 | bottom border at y≈199 (1 px `#363722`). |
| Group row "FX · 28" | 0 | 202 | 1600 | 21 | bg `#140f03`, slightly lighter than the page. |
| Data rows | 0 | 223→ | 1600 | 35.7 each | row text baseline centre y = 241, 277, 312, 348, 383, 419, 455, 490, 526, 562, 597, 633, 669, 705, 740, 776, 812, 848, 883, 919. 1 px separator `#201a0b`–`#2b261a`. |
| Footer | 0 | 940 | 1600 | 26 | separator line on top. |

Column x positions (left edge or right edge of the text):

| Column | Header text | Alignment | Position |
|---|---|---|---|
| SYMBOL | `SYMBOL` | left | x=10 |
| POSITIONING | `POSITIONING` | left | header x=214; bar x=214–477 (263 px), h≈9 px (y 236–245 for row 1), fully rounded ends |
| LONG | `LONG` | right | right edge x≈615 |
| SHORT | `SHORT` | right | right edge x≈773 |
| 24H Δ LONG | `24H Δ LONG` | right | right edge x≈1035 |
| POSITIONS | `POSITIONS` | right | right edge x≈1278 |
| STATUS | `STATUS` | right | badge right edge x≈1589 |

Stretch: KPI cards share the width equally; the table fills the width (the bar column keeps 263 px, the numeric
columns spread proportionally); the rows area is the only vertical scroll area (34 instruments do not fit: only 20
FX rows are visible; the Commodities and Crypto groups follow below).

### 3.2 Detail modal (zoomed recording, coordinates of the frames from t=2.0)

The modal is a large dialog over a near-black backdrop (`#020200`). The recording only shows x≈0–1600 of the zoomed
view, which is the right ~75 % of the modal; the modal's right edge is at x≈1586 and the top edge at y≈14.

```
y=14  ═══ 2 px cyan top accent line (bright at the left, fading to the right) ══════════════════════════════╗
y=18  │[↗ CROWDED LONG] (EUR/USD title on the left, cut off)                                            ×  │ header h≈41
y=59  ├───────────────────────────────────────────────────────────────────────────────────────────────────┤
y=62  │[Avg retail LONG entry 1.14140] [Avg retail SHORT entry 1.10440]  Last 1.12230                      │ chips h≈37
y=100 ├──────────────────────────────── chart panel ──────────────────────────┬─ LONG / SHORT DISTRIBUTION ─┤
      │ TradingView toolbar (h≈40)                                          │  3D pie (y 70–358)          │
      │ legend + candles + price scale (y 145–700)                         ├─ RETAIL POSITIONING STATS ──┤
      │ time axis (y≈713), range bar (y 733–773)                           │  13 rows (y 371–773)        │
y=773 ├─────────────────────────────────────────┬──────────────────────────┴─────────────────────────────┤
y=785 │ long/short % history (stacked bars)     │ OPEN INTEREST — EURO FX               CFTC, weekly     │ h≈385
y=1170├─────────────────────────────────────────┼────────────────────────────────────────────────────────┤
y=1181│ (title cut) each side indexed to its own range │ POSITION      each side indexed to its own range  │ h≈384
y=1565├─────────────────────────────────────────┴────────────────────────────────────────────────────────┤
y=1576│ yellow narrative box (h≈83)                                                                       │
y=1677│ grey footnote (1–2 lines)                                                                         │
```
(y values below 966 are content coordinates; they are reached by scrolling inside the modal.)

| Region | x (visible) | y | w | h |
|---|---|---|---|---|
| Modal header | …–1586 | 18 | — | 41, separator at y=59 |
| Chips row | …–1586 | 62 | — | 37, separator at y=99–101 (`#323228`) |
| Candle chart panel | …–1156 | 100 | ≈1156+ | 673; 1 px border `#333031` |
| Distribution panel | 1167 | 70 | 407 | 288 |
| Statistics panel | 1167 | 371 | 407 | 402 |
| History row 1 left (stacked %) | …–688 | 785 | — | 385 |
| History row 1 right (open interest) | 698 | 785 | 876 | 385 |
| History row 2 left (indexed bars) | …–688 | 1181 | — | 384 |
| History row 2 right (POSITION) | 698 | 1181 | 876 | 384 |
| Narrative box | …–1574 | 1576 | — | 83 |
| Footnote | …–1565 | ≈1670 | — | ≈20 |

Gaps between panels ≈10–12 px. The chart panel and the right column share one row; the right column is fixed
≈407 px, the chart stretches. The two history rows are split ≈44 % / 56 % (left / right) in the visible part (the
real split is probably 50/50 since the left side is cut).

## 4. Components

### 4.1 Title bar
- Left icon: two-person "users" outline icon, 14 px, at x≈12–24, colour `#cbbf96` (pale gold).
- Small round dot, 5 px, at x≈38, olive/gold `#7e7534` (dim gold; probably a status/live dot).
- Title `RETAIL POSITIONING — CROWD SURVEILLANCE` (uppercase, em dash with spaces), monospace ≈11 px, very wide
  letter-spacing (≈4 px), colour `#5f5a52` (muted grey), x≈50, centre y≈16.
- Directly after the title, a solid yellow block cursor `▌` 6x12 px, `#feef85` (terminal-cursor look; probably
  blinks — it is on in all frames seen).

### 4.2 KPI cards (6)
Common: bg `#0c0601`, 1 px border `#3a3715`–`#3e3c20` (dim olive), 2 px coloured top accent line (with a soft
lighter inner row: e.g. cyan `#089aa4` / `#75cdd7`), small corner brackets: top-left `⌜` (≈10 px arms,
`#8c885b`, 1 px) at the card's inner top-left, bottom-right `⌟` (`#464324`) at the inner bottom-right.
Label: monospace ≈10 px uppercase, letter-spacing ≈3 px, `#5f5d57`. Value: monospace ≈24 px bold, coloured, with a
faint glow. Sub-label: monospace ≈10 px uppercase, letter-spacing ≈3 px, `#56534c`.

| # | Accent line | Label | Value (colour) | Sub-label |
|---|---|---|---|---|
| 1 | cyan `#089aa4` | `CROWDED LONG` | `20` (cyan ≈`#22d3ee`, rendered `#83d3dc`) | `OF 34 INSTRUMENTS` |
| 2 | red `#b24233` | `CROWDED SHORT` | `4` (red ≈`#ef5f5f`, rendered `#d27a77`) | `OF 34 INSTRUMENTS` |
| 3 | cyan | `MOST CROWDED LONG` | `LTC/USD` (cyan) | `98% LONG` |
| 4 | red | `MOST CROWDED SHORT` | `AUD/NZD` (red) | `82% SHORT` |
| 5 | yellow `#ba9900` / `#f3db7f` | `TRACKED POSITIONS` | `216,718` (yellow `#f9e291`) | `MYFXBOOK COMMUNITY` |
| 6 | none (border only, `#36301e`) | `CROWD LEAN` | (no number) a horizontal split bar x 1343–1578, y≈90–96 (6 px, rounded): cyan `#1fdbf2` for 65 %, red `#ea5d60` for 35 % | left `65% LONG` (cyan), right-aligned `35% SHORT` (red), same small caps style |

### 4.3 Toolbar
- **Filter input**: x 10–132, y 134–160, bg `#140f05`, 1 px border `#2b2a1d`, radius ≈2 px; magnifier icon 10 px
  + placeholder `Filter…` monospace 12 px `#535047`.
- **Category segmented control**: x 138–369, border 1 px `#2b2a1d`, bg `#140f05`. Items `All`, `FX`,
  `Commodities`, `Crypto` (monospace 12 px, `#817d75`). Active item = filled yellow pill `#ffd706` with dark text
  (`#0c0601`), radius 3 px, padding ≈6 px (here `All` at x 141–175, y 137–157).
- **Custom** button: x 373–446, outlined 1 px `#272215`, icon `☰`-like sliders glyph + `Custom`, `#7d7a74`.
- **Sort segmented control** (right): x 1241–1590, same style; items `Majors first` (active, yellow pill),
  `Most extreme`, `Most active`, `A-Z`.

### 4.4 Table
- Header: monospace ≈11 px bold, uppercase, letter-spacing ≈2 px, gold/cream `#eadb9f`. Texts: `SYMBOL`,
  `POSITIONING`, `LONG`, `SHORT`, `24H Δ LONG`, `POSITIONS`, `STATUS`.
- Group row: `FX` (gold `#f2dc99`, bold 11 px) + ` · 28` (dim `#474439`); bg `#140f03`.
- Row: bg transparent over page `#050402`; hover bg `#1b1503` (whole row). 1 px separators.
- **SYMBOL**: monospace 13 px bold, white `#fdfefa`, format `EUR/USD`.
- **POSITIONING bar**: 263x9 px pill. Left part cyan (gradient from `#1f9caa` at the left end to `#1ed0e1` at the
  junction), right part red (from `#e26256` at the junction to a darker `#ad4a41` at the right end), width split =
  long %. Tiny 1 px darker edge; no gap between segments.
- **LONG**: `84%` integer %, monospace 13 px semi-bold, cyan (`#22d3ee`-ish, rendered `#7bd5e2`).
- **SHORT**: `16%`, red (`#ef5f5f`-ish, rendered `#cb7e7b`).
- **24H Δ LONG**: `flat` grey `#797871`; rising `↗ 2pt` cyan (small diagonal arrow glyph ≈7 px then the value);
  falling `↘ 4pt` red. Unit `pt` = percentage points of the long %. Integer.
- **POSITIONS**: thousands separator comma, `41,240`, grey `#7e7e76`, monospace 13 px.
- **STATUS badge**: right-aligned, h≈16 px, radius 2 px, 1 px border, condensed proportional font (Rajdhani-like)
  ≈10 px bold uppercase, small trend-line icon before the text:
  - `CROWDED LONG` / `LEANING LONG`: text cyan `#7ec5d0`, bg `#062c2f`, border `#266d7e`-ish; icon `↗` zig-zag.
  - `CROWDED SHORT` / `LEANING SHORT`: text red `#bf7169`, bg `#3e2008`, border `#51200e`; icon `↘` zig-zag.
  - `BALANCED`: text grey `#7a766f`, no fill (transparent), thin border `#15140d`; icon `—`.
  - Badge widths differ with the text (CROWDED SHORT widest ≈98 px).

Rows seen (all FX, sort "Majors first"):

| Symbol | Long | Short | 24H Δ | Positions | Status |
|---|---|---|---|---|---|
| EUR/USD | 84% | 16% | flat | 41,240 | CROWDED LONG |
| GBP/USD | 31% | 69% | ↗ 2pt | 22,462 | CROWDED SHORT |
| USD/JPY | 71% | 29% | ↗ 4pt | 7,124 | CROWDED LONG |
| USD/CHF | 56% | 44% | ↘ 4pt | 5,738 | LEANING LONG |
| USD/CAD | 44% | 56% | ↗ 4pt | 10,828 | LEANING SHORT |
| AUD/USD | 37% | 63% | ↘ 6pt | 8,639 | LEANING SHORT |
| NZD/USD | 78% | 22% | ↘ 2pt | 4,737 | CROWDED LONG |
| EUR/GBP | 96% | 4% | flat | 12,471 | CROWDED LONG |
| EUR/JPY | 65% | 35% | ↗ 2pt | 4,237 | CROWDED LONG |
| EUR/CHF | 78% | 22% | ↘ 3pt | 3,885 | CROWDED LONG |
| EUR/AUD | 90% | 10% | ↗ 4pt | 3,186 | CROWDED LONG |
| EUR/CAD | 92% | 8% | ↗ 1pt | 4,349 | CROWDED LONG |
| EUR/NZD | 69% | 31% | ↗ 13pt | 2,586 | CROWDED LONG |
| GBP/JPY | 69% | 31% | ↗ 2pt | 2,617 | CROWDED LONG |
| GBP/CHF | 41% | 59% | ↘ 4pt | 1,742 | LEANING SHORT |
| GBP/AUD | 62% | 38% | ↗ 5pt | 2,076 | LEANING LONG |
| AUD/JPY | 50% | 50% | flat | 2,366 | BALANCED |
| AUD/NZD | 18% | 82% | flat | 10,320 | CROWDED SHORT |
| AUD/CAD | 26% | 74% | ↘ 8pt | 18,218 | CROWDED SHORT |
| CAD/JPY | 71% | 29% | flat | 1,349 | CROWDED LONG |

Status rule inferred from the rows: dominant side ≥ 65 % → CROWDED; 51–64 % → LEANING; exactly 50 % →
BALANCED (the exact balanced band is unknown, see open questions).

### 4.5 Footer
`Terminal Data · 23:17:55` proportional 10 px `#57554e` at x=7, y≈951 (HH:MM:SS, 24 h). Right: `● DELAYED` badge
x 1524–1593, y 942–962, bg `#352a00`, 1 px border `#3f3706`, yellow dot `#f0d04e`, bold condensed text
`DELAYED` `#f3dc8a`.

### 4.6 Detail modal

**Frame**: bg `#020506` (blue-black, cooler than the page), 1 px border `#202420`, 2 px top accent line cyan
(`#609ca3` at the left, `#3a6e6f` mid, fading to `#1d241b` at the right). Close `×` 12 px `#7d7c79` at the top-right
(x≈1560, y≈37).

**Header** (y 18–58): status badge `↗ CROWDED LONG` (same badge style as the table) — the symbol title to its left
is outside the recording.

**Chips row** (y 62–98): three items, monospace 12 px:
- `Avg retail LONG entry 1.14140` (only `…40` visible): cyan text, 1 px cyan border, transparent bg, radius 2 px.
- `Avg retail SHORT entry 1.10440`: red text `#c25e62`-ish, 1 px red border `#6e2a2c`, x 28–244, h 21.
- `Last 1.12230`: plain grey text `#767572`, no border.

**Candle chart panel**: an embedded TradingView Advanced Chart (dark theme, bg `#0a0a0a`, toolbar bg `#000000`):
- Toolbar (y 103–143): candle-style button (selected, bg `#383c4c`), line/area style button, `⌄` dropdown,
  `(+)` compare, `Indicators` (with icon), camera (snapshot) on the right. Toolbar text `#c2c4cc`, 14 px sans.
- Legend (y≈167): `Euro / U.S. Dollar · 1h · OANDA` (white `#e5e9f8`, 16 px), a two-part pill (`—` grey on
  `#383c4c`, `≈` red), then `O1.11948 H1.12044 L1.11911 C1.12032 +0.00084 (+0.08%)` — letters grey-white, values
  cyan for an up candle, red for a down candle.
- Candles: up `#00c2e4` (cyan), down `#ff656b` (red), hollow-less, thin wicks of the same colour. ≈1 week of 1 h bars
  (time axis `16 18 20 21 22 23 25`).
- Last-price line: dotted cyan horizontal line + label `1.12032` white on cyan `#00c4e4` at the right axis.
- Price axis: 5 decimals, step 0.001 (`1.12800 … 1.11500`), text `#d8dbe9`-ish 13 px.
- Crosshair: dashed light grey lines; axis labels on dark `#2a2e39`-like boxes (`1.12594`, `22 Oct '26  07:00`).
- Bottom bar (y 733–773): range buttons `… M 6M YTD 1Y 5Y All` (`#bec2c6`), right `23:33:10 UTC-5` (live clock,
  timezone UTC-5), gear icon at the axis corner.
- On hover of the price scale: `USD ⌄` dropdown at its top, `A` (selected, white bg) and `L` buttons at its bottom.

**LONG / SHORT DISTRIBUTION panel**: title centred, 9 px uppercase letter-spaced `#747471`. A **3D pie** (tilted
ellipse rx≈118, ry≈70, centre ≈(1371, 207), extruded depth ≈20 px — re-measured on t002.0: top face y 137–277, side
wall down to y≈298): long slice cyan (top `#23edf9` → `#1ac5db`,
side wall `#09686e`→`#135357`), short slice red/coral `#f46868` (top), with a darker red side, the short slice
starting at 12 o'clock and going counter-clockwise. Labels inside slices: `16%` (bold 22 px white) over `SHORT`
(bold 11 px white); `84%` over `LONG`. Soft cyan glow/shadow under the pie. Legend below, centred: `● Long 84%`
(cyan dot, text `#688e92`) and `● Short 16%` (red dot, `#725253`), 10 px.

**RETAIL POSITIONING STATISTICS panel**: title 9 px uppercase letter-spaced `#757772` (left, x 1178). Rows pitch
≈26.7 px, separators 1 px `#21211e`, labels proportional 11 px `#575957`, values monospace 12.5 px right-aligned
at x≈1563:

| Label | Value | Value colour |
|---|---|---|
| Long Positions | `30,797` | cyan |
| Short Positions | `10,443` | red |
| Total Positions | `41,240` | white `#f5f5f4` |
| Average Long Price | `1.14140` | cyan |
| Average Short Price | `1.10440` | red |
| Long Position Ratio | `84%` | cyan |
| Short Position Ratio | `16%` | red |
| Long Volume | `16,996.0 lots` | cyan |
| Short Volume | `3,347.8 lots` | red |
| *(extra gap ≈14 px, a separator)* | | |
| Long Price vs Entry | `-0.01910` | cyan |
| Short Price vs Entry | `-0.01790` | red |
| Long Est. avg P&L | `-1.67%` | cyan |
| Short Est. avg P&L | `-1.62%` | red |

The last four labels are dimmer (`#3b3b3a`): they are app-computed estimates.

**History row 1, left — long/short % history** (title outside the recording): ≈150 thin vertical bars (2 px wide,
2 px gap), each a 100 % stacked bar: cyan bottom (`#75ceda` / `#4e9198`, gradient darker toward the bottom) = long
%, red top (`#ac6765`) = short %. A dashed horizontal line at 50 % (`#70b8b4`, faint green-cyan). A weekend gap shows
as bars of unchanged level. Panel bg `#110f06` (re-measured on t015.0; `#140f05` was the input colour), border 1 px
`#232217`.

**History row 1, right — `OPEN INTEREST — EURO FX`** (title 9 px uppercase `#605c55`, top-left) and `CFTC, weekly`
(top-right, `#413d34`): 26 wide yellow bars (≈26 px wide, ≈7 px gap) with a vertical gradient `#f8d910` (top) →
`#cbab0e` (bottom) → `#6c580c` near the base, rounded top corners 1 px. No axes, no labels. Bars are scaled to
min..max (short bars still visible).

**History row 2, left** (title cut) and **right `POSITION`**: both carry the note `each side indexed to its own
range` (top-right, 9 px `#403c33`). Diverging thin bars around a mid baseline: cyan up (`#73c2cb`, gradient) and
red down (`#b46e6a`). Same ≈150-bar time axis as the % history (gap at the weekend shown as tiny flat bars).
Left = most likely long/short **volume** history, right = long/short **positions** history (count), each side
min–max normalised separately.

**Narrative box**: full width, bg `#383008` / `#2e2808` (dark olive-yellow), 1 px border `#473f1e`, radius 2 px,
padding ≈16 px; proportional 12 px text `#888268`:
"[EUR/USD is cr]owded long — 84% long vs 16% short across 41,240 open positions. The average long entry (1.14140)
is currently down 1.67%, while the average short entry (1.10440) is down 1.62% — both sides show a mixed real result
versus their own average entry. Long exposure has increased 36 points since this app's [first snapshot …]".

**Footnote**: proportional 10 px `#545756`: "[… Price vs Entry compares the aver]age entry price for that side to
the current live quote — a real calculation by this app, not a broker-reported figure, and only as accurate as the
underlying average-entry data. Open Interest is the real CFTC futures figure for the matching standalone currency
contract, not a retail-CFD metric — retail CFD positions have no open interest of their [own] …".

## 5. Interactions

- **Row hover**: whole-row background `#1b1503`, no other change. Pointer stays an arrow.
- **Row click**: opens the detail modal for that instrument (centred dialog over a dark backdrop). On open, the
  chart area shows a grey skeleton until the TradingView chart has loaded (≈0.5 s); the history bar charts
  animate in (bars grow / sweep in from the left, ≈0.5 s).
- **Modal scroll**: mouse wheel scrolls the modal body smoothly (no visible scrollbar in the recording); the header
  (badge + ×) stays fixed at the top; the top accent line stays.
- **Candle chart**: full TradingView behaviour — crosshair with price and date labels, OHLC legend follows the
  hovered bar (values coloured by candle direction), price-scale hover controls (`USD ⌄`, `A`, `L`), range buttons,
  snapshot button, indicators.
- **Close**: `×` (not shown being used; Escape presumably too).
- **Filters / sort / Custom / Filter input**: not used in the video; expected behaviour: category pills filter the
  rows (All, FX, Commodities, Crypto) and the group headers; sort pills reorder rows (`Majors first` = majors then
  crosses in a fixed order; `Most extreme` = by |long% − 50|; `Most active` = by positions; `A-Z` alphabetical);
  the text input filters by symbol; `Custom` probably lets the user choose instruments (watch list).
- Title-bar block cursor: probably a blink animation (on in every sampled frame).

## 6. Data

| Field | Sample values | Format | Update | Source on screen |
|---|---|---|---|---|
| Long % / short % per instrument | 84 % / 16 % | integer % | minutes (Myfxbook outlook refresh) | Myfxbook community |
| 24h Δ long | flat, ↗ 2pt, ↘ 8pt, ↗ 13pt | integer points, arrow | computed: long % now − long % 24 h ago from the app's own snapshots | app |
| Positions per instrument | 41,240 | integer, comma thousands | minutes | Myfxbook |
| Status | CROWDED LONG … BALANCED | badge | computed from long % | app |
| Crowded long / short counts | 20 / 4 of 34 | integer | computed | app |
| Most crowded long / short | LTC/USD 98 % long, AUD/NZD 82 % short | symbol + % | computed (max long %, max short %) | app |
| Tracked positions | 216,718 | integer | sum of positions over the 34 instruments | Myfxbook community |
| Crowd lean | 65 % long / 35 % short | integer % | computed aggregate (probably position-weighted mean) | app |
| Long / short positions | 30,797 / 10,443 / total 41,240 | integer | minutes | Myfxbook |
| Average long / short price | 1.14140 / 1.10440 | 5 dp (pair precision) | minutes | Myfxbook |
| Long / short position ratio | 84 % / 16 % | % | = long volume / (long + short volume): 16,996.0 / 20,343.8 = 83.5 % → 84 % (volume-based, not count-based: positions give 74.7 %) | Myfxbook |
| Long / short volume | 16,996.0 / 3,347.8 lots | 1 dp + ` lots` | minutes | Myfxbook |
| Last | 1.12230 | 5 dp | live quote | app price feed |
| Long price vs entry | -0.01910 | 5 dp signed | = last − avg long (1.12230 − 1.14140) | computed |
| Short price vs entry | -0.01790 | 5 dp signed | = avg short − last (1.10440 − 1.12230) (short's view) | computed |
| Long / short est. avg P&L | -1.67 % / -1.62 % | 2 dp % signed | = price-vs-entry / avg entry | computed |
| Candle chart | EUR/USD 1h | OHLC 5 dp | live | OANDA (TradingView widget), clock UTC-5 |
| Long/short % history | ≈150 points | stacked % | the app's own snapshots (≈hourly) | app |
| Open interest | 26 weekly values | bars | weekly | CFTC (EURO FX futures) |
| Volume / position history | ≈150 points | diverging bars, each side min-max normalised | snapshots | app |
| Narrative | see 4.6 | generated text | on open | app (template) |
| Footer time | 23:17:55 | HH:MM:SS | each refresh | "Terminal Data" + DELAYED badge |

Myfxbook's community outlook API returns exactly these fields per symbol (longPercentage, shortPercentage,
longVolume, shortVolume, longPositions, shortPositions, avgLongPrice, avgShortPrice).

## 7. Key frames

| Frame | Shows |
|---|---|
| `t000.0.jpg` | List view, all six KPI cards, toolbar, 20 rows, GBP/USD hover, footer + DELAYED |
| `t001.0.jpg` | EUR/USD row hover (just before the click) |
| `t001.5.jpg` | Modal opening: chart skeleton, history bars animating in |
| `t002.0.jpg` | Modal fully loaded (chart, pie, statistics, top of history row) |
| `t004.5.jpg` | Crosshair with red (down-candle) OHLC legend |
| `t009.0.jpg` | Price-scale hover controls (`USD ⌄`, `A`, `L`) |
| `t013.5.jpg` | Scrolled: OI bars fully visible, second history row appearing |
| `t015.0.jpg` | Bottom of the modal: both history rows, narrative box, footnote |
| `t018.0.jpg` | Scrolled back to the top |

## 8. Open questions

- The left ~25 % of the modal is never visible: symbol title, left chips, the titles of the left history charts
  (probably "LONG / SHORT HISTORY" and "VOLUME"), the time range buttons before `M`. Ask or design them in the same
  style as the visible titles.
- The modal has no visible scrollbar; its exact outer size (probably ~90 % of the window) is unknown.
- Exact thresholds for CROWDED / LEANING / BALANCED (≥65 / 51–64 / 50 fits all rows; the balanced band could be
  48–52).
- How Crowd Lean (65/35) is aggregated (mean of ratios vs position-weighted).
- Rows for the Commodities and Crypto groups (LTC/USD is one of them) were not shown; the order of "Majors first"
  beyond FX is unknown.
- The `Custom` button and the 4 sort modes were not exercised.
- `Last 1.12230` (chip) differs from the chart's last close `1.12032` (OANDA): the app uses its own quote feed for
  the calculations; the build should use one consistent quote.
- Whether the DELAYED badge switches to LIVE, and on what condition.
- Refresh interval of the Myfxbook data (the footer time suggests a periodic poll).

## 9. Builder's check against the frames (2026-11-14)

Re-opened t000.0, t001.5, t002.0, t004.5, t009.0, t013.5 and t015.0. Corrected above: the pie's vertical radius and
centre (ry≈70, not 75) and the history panels' background (`#110f06`). Verified and left unchanged: KPI card x positions
and labels, the 20 table rows and their status badges (the ≥65 / 51–64 / 50 rule fits every row), the column right
edges, the chip texts, the 13 statistics rows and their values, the long slice going clockwise from 12 o'clock (short
counter-clockwise), the 26 open-interest bars (counted on t015.0), the ≈150 stacked/diverging history bars and the
narrative/footnote texts. The modal's left quarter is never visible in the recording (open question stays).
