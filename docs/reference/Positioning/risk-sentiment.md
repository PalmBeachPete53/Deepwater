# Positioning › Risk Sentiment

Reference video: `Reference/Positioning/risk-sentiment.mp4` (≈14.5 s, frames every 0.5 s at 1600x966).
Shared style (panel, corner brackets, title with dot and block cursor, badges, fonts): see `_shared-notes.md`.

## 1. Purpose

"RISK SENTIMENT — CROSS-ASSET RISK APPETITE": one composite risk-on / risk-off gauge (0–100) built from seven
market components (volatility, equity trend, credit, AUD/JPY, copper/gold, stocks vs safe haven, crypto). Each
component is today's reading expressed as its percentile against its own previous 252 trading days, oriented so
higher = more risk appetite; the composite is the average of the available components. The page shows a large
semicircular gauge with the composite (32, RISK-OFF) and its 1-week / 1-month changes, the composite history over
one year with risk-off / neutral / risk-on zones, the seven component rows with gradient percentile bars, and
1-year charts of the S&P 500 and the VIX. Source: Yahoo Finance daily closes (stated in the footnote), "7 MARKETS ·
DAILY CLOSES · PERCENTILE VS 1 YEAR" (header badge).

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | Page at scroll 0. Cursor idle at (810, 298) in the gauge panel's empty right side. Header block cursor and panel-title block cursor on. |
| 0.5 | Block cursors off. Status dots (header, components header, badge) dim. |
| 1.0–1.5 | Cursor moves left onto the gauge (x≈700 → 530, y≈298): no hover effect on the gauge. Status dots brighten (pulse peak at 1.5). |
| 2.0–3.0 | Cursor over the gauge centre area then back right; block cursors on again at 2.5–3.0. |
| 3.5 | Cursor on the AUD/JPY component row (x≈1015, y≈340): row hover highlight (warm gradient from the left). |
| 4.0 | Same row, cursor moved right (x≈1040). |
| 4.5 | Cursor on the Copper / Gold row: Copper / Gold highlighted, AUD/JPY back to normal. |
| 5.0 | Cursor on the Stocks vs safe haven bar: that row highlighted. |
| 5.5–6.0 | Cursor on the Crypto momentum row: highlighted. |
| 6.5 | Cursor over the panel gap (x≈810, y≈545) at the top of the history panel; page starts scrolling down (wheel). |
| 7.0 | Scroll ≈38 px; cursor over the legend `RISK-ON > 60`. No tooltip. |
| 7.5 | Scroll ≈64 px; cursor now inside the history plot at x≈808: **crosshair** (dashed vertical line through the plot), a dot on the line (cream ring) and a **tooltip** `2026-10-14` / `● 47` (yellow dot = neutral zone colour). |
| 8.0–10.5 | Scroll continues to the bottom (≈212 px total); the crosshair/tooltip stays on 2026-10-14 (the tooltip box keeps its screen position near the cursor; the vertical line and dot move with the chart). At t=10.0–10.5 the footnote line appears at the bottom. |
| 11.0–14.0 | Scroll back to the top (≈40 px per 0.5 s); tooltip still shown until the cursor leaves the plot (t=13.5, cursor on the legend) — then hidden. t=14.0: back at scroll 0, cursor at the gap between gauge and history panels. |

Throughout: the yellow status dots pulse (≈2 s period) and the block cursors blink (≈3 s period); all other values
are static (daily data).

## 3. Layout

Page area 1600x966. The page header (y 0–36) is fixed; the rest scrolls vertically (content height ≈1170 px →
max scroll ≈212 px). No visible scrollbar.

```
y=0   (◠) ● RISK SENTIMENT — CROSS-ASSET RISK APPETITE ▌                 [• 7 MARKETS · DAILY CLOSES · PERCENTILE VS 1 YEAR]
y=36  ───────────────────────────────────────────────────────────────────────────────────────────────────────────
y=51  ┌⌜COMPOSITE RISK APPETITE ▌ ─────────────────────┐ ┌ ● COMPONENTS — PERCENTILE VS OWN PAST YEAR ────────────┐
      │            semicircular gauge                   │ │ Volatility (VIX)                         19.5     22   │
      │                 32                              │ │ ███████░░░░░░░░░░░░│░░░░░░░░░░░░░░░░░░░░░             │
      │              RISK-OFF                            │ │ VIX level – lower volatility = more risk appetite     │
      │     [1W ▼ 1 (WAS 33)  1M ▼ 31 (WAS 63)]          │ │ … 7 rows, pitch 74 px                                 │
y=543 └────────────────────────────────────────────────⌟┘ │                                                        │
y=555 ┌⌜COMPOSITE HISTORY · 1 YEAR    ■RISK-OFF <40 ■NEUTRAL 40-60 ■RISK-ON >60┐ │ (empty below the rows)         │
      │ zone bands + coloured line, y axis 0–100 right                         │ │                                │
y=848 └───────────────────────────────────────────────────────────────────────⌟┘ └────────────────────────────────┘
y=860 ┌⌜S&P 500 · 1 YEAR            7,623.68 +14.6% 1Y┐ ┌⌜VIX · VOLATILITY INDEX · 1 YEAR      19.45 -5.8% 1Y┐
      │ cyan line + area                               │ │ red line + area, 30 · stress / 20 · elevated     │
y≈1140└────────────────────────────────────────────────⌟┘ └───────────────────────────────────────────────────⌟┘
y≈1157 EACH COMPONENT: TODAY'S READ AGAINST ITS OWN PREVIOUS 252 TRADING DAYS (PERCENTILE), … · UPDATED 16:06
```

| Region | x | y (content) | w | h |
|---|---|---|---|---|
| Page header | 0 | 0 | 1600 | 36, bottom border 1 px |
| Header badge | 1268 | 9 | 318 | 19 |
| Gauge panel | 14 | 51 | 867 | 492 |
| Gauge centre (hub) | 447 | 380 | — | — |
| History panel | 14 | 555 | 867 | 293 |
| History plot area | 23–828 | 594–820 | 805 | 226 |
| Components panel | 892 | 51 | 694 | 797 |
| Components header row | 892 | 51 | 694 | 36 (separator y=86–87) |
| Component rows | 892 | 87 + n·74.3 | 694 | 74 |
| S&P 500 panel | 14 | 860 | 781 | ≈280 |
| VIX panel | 806 | 860 | 780 | ≈280 |
| Footnote | 14 | ≈1152 | 1300 | 12 |

Gaps: 11–12 px between panels. Column split: left 867 px (54 %) / right 694 px for the top two rows; the bottom row
is split 50 / 50. When the window grows, the columns stretch proportionally; the gauge stays centred in its panel
(probably scaled to the panel's height); the components rows keep their height (extra space stays empty at the
bottom of the components panel).

## 4. Components

### 4.1 Page header
- Gauge icon `◠` (14 px outline, `#cbbf9e`), status dot 6 px (dim `#7e763c` ↔ bright `#f2dd64`, pulsing),
  title `RISK SENTIMENT — CROSS-ASSET RISK APPETITE` (monospace 11 px uppercase, letter-spacing ≈4 px, `#635f55`),
  block cursor `▌` 7x14 `#fde78f` (blinking).
- Right badge `• 7 MARKETS · DAILY CLOSES · PERCENTILE VS 1 YEAR`: bold mono 9.5 px uppercase letter-spaced ≈1 px,
  text `#f6e29a`, bg `#504000`-ish dark gold (rendered `#3a3004`), 1 px border `#7a6a1a`, radius 2; the leading
  dot pulses with the header dot.
- Header bg gradient `#100800`→`#181004`, bottom border `#3b321d`.

### 4.2 Gauge panel (`COMPOSITE RISK APPETITE`)
- Panel bg `#0c0601` with a soft radial warm glow behind the gauge, 1 px border `#403a28`, corner brackets
  top-left `⌜` and bottom-right `⌟` (gold `#5f5a3a`, 8 px arms).
- Title `COMPOSITE RISK APPETITE` (mono 10 px uppercase, letter-spacing ≈3 px, `#5e5c57`) + blinking block cursor
  (`#f6e383`, 7x12).
- **Gauge geometry**: centre (447, 380); sweep 240°, from 0 at 210° (bottom-left, 30° below horizontal) clockwise
  over the top (50 at 90°) to 100 at −30° (bottom-right).
  - Outer scale arc: thin 1 px line, radius ≈268, colour dim olive `#5e5740` on the left fading to near-invisible on
    the right; minor ticks every 2 units (≈5 px, dim), major ticks every 10 units (≈12 px, 2 px, yellow `#d3bf5f`).
  - Scale numbers `0 10 20 … 100` at radius ≈290, mono 15 px `#9e9b95`.
  - **Segment ring**: ≈60 thick radial bars (each ≈7 px wide, ≈34 px long, between radius ≈185 and ≈220, rounded
    ends, gap ≈4 px). Bars ≤ value are lit red with glow (gradient along the arc: `#bb5d56` at 0 → `#fb8b86` at
    the value); bars above the value are dim and tinted by zone: 32–40 dim brown `#372815`, 40–60 dim olive
    `#353011`, 60–100 dim teal `#1b3026`→`#182c23`. (When the value is risk-on the lit bars would be cyan, neutral
    yellow — inferred from the zone colours.)
  - Zone labels on the inner side, rotated along the arc, mono 9 px letter-spaced: `EXTREME OFF` (≈0–20, red dim
    `#6a3a30`), `RISK-OFF` (≈25–35, red `#d06c66`), `NEUTRAL` (top, olive `#726d43`), `RISK-ON` (≈62–72, teal
    `#2d6f6a`), `EXTREME ON` (≈82–95, teal dim).
  - History markers: small dotted radial ticks inside the ring at the composite value 1 week ago (33) and 1 month
    ago (63), labelled `1W` and `1M` (mono 9 px `#9c9990`).
  - **Needle**: from the hub to radius ≈170 at the value, 3 px, bright pink-white core `#ffdbd1` with red glow
    (`#fc807c`). Hub: ring Ø≈36 px (red `#eb9792` 3 px ring on dark centre with a small cream dot), plus a dotted
    circle Ø≈76 px (dim red).
  - Value: `32` mono/rounded bold ≈64 px, red `#fc807c` with glow, centred at (447, 437). Under it `RISK-OFF` mono
    20 px, letter-spacing ≈6 px, red. (Label = zone of the value: EXTREME OFF / RISK-OFF / NEUTRAL / RISK-ON /
    EXTREME ON presumably.)
  - Change pill (y 509–527, x 293–600): `1W ▼ 1 (WAS 33)    1M ▼ 31 (WAS 63)`; `1W`/`1M` grey `#8e8c81`, `▼` red
    triangle `#d39a99` (▲ cyan for a rise), change red `#b88c87`, `(WAS 33)` dim `#5c594b`; mono 11 px; pill bg
    slightly lighter (`#141000`), 1 px border `#2b2a1d`.

### 4.3 Composite history panel
- Title `COMPOSITE HISTORY · 1 YEAR` (same style as 4.2) left; legend right: `■ RISK-OFF < 40` (red `#e98e8b`),
  `■ NEUTRAL 40-60` (yellow `#f0d962`), `■ RISK-ON > 60` (cyan `#5ccdd9`), mono 10 px bold letter-spaced, 6 px
  squares.
- Plot (x 23–828, y 594–820): background zone bands — 80–100 teal `#0f2d2c`, 60–80 dim teal `#101815`, 40–60 dim
  olive `#1b1503`, 20–40 dim red `#1f1108`, 0–20 red `#371b15`; faint 1 px lines at each 20 boundary; a dotted
  midline at 50 (`#393325`).
- Line: daily composite, ≈252 points, 2 px, coloured by value (cyan `#22d3ee` ≥ 60, yellow `#facc15` 40–60, red
  `#f87171` < 40; segments change colour where the line crosses 40/60), with a soft glow of the same colour.
- Y axis right (x≈838): `100 80 60 40 20 0`, mono 11 px `#7c7972`. X axis: `NOV DEC JAN … OCT` proportional 11 px
  `#86837a`, at month starts.
- Hover: dashed vertical line (cream `#f9f4b5`, 1 px, dash 3/3) at the hovered day, a dot (cream fill, 6 px) on the
  line, tooltip box (≈92x58, bg `#0c0800`, 1 px border `#62553f`, radius 3): date `2026-10-14` (mono 12 px white)
  and below a coloured dot (zone colour) + value `47` right-aligned (bold). Tooltip is placed left of the cursor,
  below the line.

### 4.4 Components panel (`COMPONENTS — PERCENTILE VS OWN PAST YEAR`)
- Header: pulsing yellow dot + title (mono 10 px uppercase letter-spaced ≈3 px `#605b51`), separator below.
- Row (74 px, separators 1 px `#1a150a`):
  - Name (left, x 903): mono 14 px bold white `#fffefb`.
  - Raw reading (right-aligned at x≈1538): mono 12 px `#88857e` (`19.5`, `+0.81%`, `-1.65%`, `-0.01%`, `+4.63%`,
    `+5.64 pts`, `-4.05%`).
  - Percentile (right-aligned at x≈1575): mono 18 px bold, coloured by zone: red `#f87171` (< 40), yellow `#facc15`
    (40–59), cyan `#22d3ee` (≥ 60).
  - Bar: track x 903–1575, 4 px high, `#28231a`, rounded; a 1 px tick at the middle (50 %, x≈1240, `#4f4b43`). Fill
    from the left to the percentile, rounded, with a horizontal gradient that always runs red `#ef7a75` → orange
    `#f1a144` → yellow `#e6c718` → green `#80c797` → cyan `#4ec4c9` over the *filled* length (a 1 % fill is just a
    yellow-ish dot).
  - Description (x 903, 14 px under the bar): proportional 10.5 px `#48443d`.
  - Hover: row background gets a warm gradient from the left (`#2b2104` at the left edge → transparent by ≈60 % of
    the row width); no other change.

| Component | Raw | Percentile | Description (exact) |
|---|---|---|---|
| `Volatility (VIX)` | `19.5` | 22 | `VIX level – lower volatility = more risk appetite` |
| `Equity trend` | `+0.81%` | 11 | `S&P 500 vs its 125-day average` |
| `Credit appetite` | `-1.65%` | 1 | `High-yield vs investment-grade bonds (HYG/LQD), 20-day change` |
| `AUD/JPY` | `-0.01%` | 40 | `Risk-sensitive AUD against the safe-haven JPY, 20-day change` |
| `Copper / Gold` | `+4.63%` | 60 | `Copper (growth) against gold (safety), 20-day change of the ratio` |
| `Stocks vs safe haven` | `+5.64 pts` | 67 | `S&P 500 20-day return minus gold's` |
| `Crypto momentum` | `-4.05%` | 24 | `Bitcoin 20-day change` |

Composite check: (22+11+1+40+60+67+24)/7 = 32.1 → `32` ✓.

### 4.5 S&P 500 panel
- Title `S&P 500 · 1 YEAR` (panel title style); right: last close `7,623.68` (mono 18 px bold white) and `+14.6% 1Y`
  (mono 11 px cyan).
- Line chart (≈252 daily closes): 1.5 px cyan `#22d3ee` (rendered `#71c7d0`) with glow and an area fill (cyan →
  transparent, `#132725` near the line). Plot ends at x≈733; right axis x≈742: `8,100 7,800 7,500 7,200 6,900
  6,600 6,300` (comma thousands, step 300), mono 11 px `#7f7c75`; dotted horizontal grid lines (`#1c180c`). X axis
  `NOV … OCT`.

### 4.6 VIX panel
- Title `VIX · VOLATILITY INDEX · 1 YEAR`; right: `19.45` (white) and `-5.8% 1Y` (red).
- Line chart: 1.5 px coral `#f87171` (rendered `#e5a49e`) with glow, red area fill. Right axis `33 30 27 24 21 18
  15 12` (step 3). Two dashed reference lines with left labels: `30 · stress` (grey dashed, label `#908d87`) and
  `20 · elevated` (dashed, warm, label `#cd9791`). X axis `NOV … OCT`.

### 4.7 Footnote
At the bottom of the content: `EACH COMPONENT: TODAY'S READ AGAINST ITS OWN PREVIOUS 252 TRADING DAYS
(PERCENTILE), ORIENTED SO HIGHER = MORE RISK APPETITE · COMPOSITE = AVERAGE OF AVAILABLE COMPONENTS · YAHOO FINANCE
DAILY CLOSES · UPDATED 16:06` — mono 9 px uppercase letter-spaced ≈1 px, `#514f49`.

## 5. Interactions

- **Component row hover**: warm left gradient highlight (see 4.4). No tooltip seen.
- **History chart hover**: dashed vertical crosshair, dot on the line, tooltip with date and value (zone-coloured
  dot). The tooltip stays while the page scrolls under a stationary cursor.
- **Gauge**: no hover effect.
- **Page scroll**: smooth wheel scroll of everything below the header.
- **Animations**: status dots pulse together (≈2 s period, dim `#7e763c` ↔ bright `#f2dd64`); block cursors (title
  and gauge panel title) blink together (≈3 s period, roughly half on). On first load the needle presumably sweeps
  to the value and the lit segments fill (not shown).
- S&P / VIX charts: hover behaviour not shown (probably the same crosshair + tooltip as the history chart).

## 6. Data

| Field | Sample | Format | Update | Source |
|---|---|---|---|---|
| Composite | 32 RISK-OFF | integer 0–100 + zone label | daily (after close; "UPDATED 16:06") | computed: mean of available component percentiles |
| 1W / 1M change | ▼ 1 (WAS 33), ▼ 31 (WAS 63) | integer, triangle, previous value | daily | composite history |
| Composite history | 1 year daily | 0–100 | daily | computed |
| VIX level | 19.5 → pct 22 | 1 dp | daily | Yahoo Finance ^VIX (inverted: low VIX = high pct) |
| Equity trend | +0.81 % → 11 | % 2 dp signed | daily | S&P 500 close vs its 125-day moving average |
| Credit appetite | -1.65 % → 1 | % | daily | HYG/LQD ratio, 20-day change |
| AUD/JPY | -0.01 % → 40 | % | daily | AUDJPY=X 20-day change |
| Copper / Gold | +4.63 % → 60 | % | daily | HG=F / GC=F ratio, 20-day change |
| Stocks vs safe haven | +5.64 pts → 67 | points 2 dp | daily | S&P 500 20-day return − gold 20-day return |
| Crypto momentum | -4.05 % → 24 | % | daily | BTC-USD 20-day change |
| Percentile | 0–100 | integer | daily | rank of today's reading within the previous 252 trading days |
| S&P 500 | 7,623.68, +14.6 % 1Y | 2 dp, comma thousands | daily | Yahoo Finance ^GSPC |
| VIX | 19.45, -5.8 % 1Y | 2 dp | daily | Yahoo Finance ^VIX |
| Updated time | 16:06 | HH:MM | per refresh | app |

Header badge "7 MARKETS" = the seven component inputs.

## 7. Key frames

| Frame | Shows |
|---|---|
| `t000.0.jpg` | Full top of page at scroll 0, block cursors on |
| `t001.5.jpg` | Status dots at pulse peak, cursors off |
| `t003.5.jpg` | Component row hover (AUD/JPY) |
| `t005.0.jpg` | Hover on Stocks vs safe haven bar |
| `t007.5.jpg` | History tooltip `2026-10-14 · 47` |
| `t010.0.jpg` | Bottom of page: S&P 500 and VIX charts, footnote |
| `t010.5.jpg` | Max scroll |
| `t014.0.jpg` | Back at top |

## 8. Open questions

- Zone boundaries: the legend says RISK-ON > 60 but a percentile of exactly 60 is drawn cyan; decide ≥ 60 or > 60.
- Gauge appearance in neutral / risk-on states (lit segment colour, label text, needle colour) is inferred.
- Exact label thresholds for EXTREME OFF / EXTREME ON (probably < 20 / > 80, as the history bands suggest).
- Whether the S&P / VIX charts have hover tooltips.
- "Composite = average of available components": behaviour when a component is missing (weighting, label).
- Whether the update time refers to New York 16:00 close + fetch delay and its time zone.
- Load animation of the needle (not recorded).
