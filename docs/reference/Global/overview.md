# Global › Overview

Source video: `Reference/Global/overview.mp4` (64.5 s, frames `t000.0`–`t064.0`, 1600x966).
All coordinates are at 1600x966, origin top-left of the page area (the app header is not in the video).
Colours were measured with `tools/pick` (JPEG: flat areas sampled several times; thin text is given as the
brightest core pixel, real glyph colour may be slightly more saturated).

This page is the reference for the whole app's visual language. The common widget kit (panel, corner
brackets, titles, pills, badges, crosshair pills, cursor block, colours, fonts) is described in
`_shared-notes.md`; this file repeats the key values where they matter for the page.

**Playback speed.** The recordings are time-lapsed. On Market Hours the clock seconds advance 6 s per video
second (exactly 6x). On this page the "1h ago → 2h ago" flips of the Squawk feed are consistent with the same
~6x speed. Animation speeds below are given in video time and, in brackets, the estimated real time (÷6).

## 1. Purpose

The command post of the terminal: one screen that answers "what is the market doing right now". A scrolling
market tape with the four FX session clocks runs across the top. Below it, three columns: on the left the
**Squawk** wire (fresh headlines, price moves and data releases, with live machine translation EN/FR); in
the middle the **Risk Sentiment** gauge with its one-year history, the **Currency Strength** chart of the
eight majors (percentage change since the first bar on screen, zoomable) and the **Central Banks** table
(policy rate, next meeting, cut/hold/hike probabilities); on the right a rotating 3D **Global Map** globe
with the session centres marked, and the **Asset Registry** of countries by region with the institution
that sets their rate.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–10.0 | Idle. Squawk in **FR** mode (auto-translated, header tall), filter **DATA** active, Asset Registry on **★ G20** (23 countries, 15 visible). Tape scrolls left continuously, Squawk wire scrolls left, globe rotates, last-point rings pulse on the currency chart. Currency chart at default zoom (1H, 15–23 Oct visible). |
| 10.5–13.0 | Mouse over the currency chart: crosshair appears (vertical dashed line at the hovered bar, horizontal dashed line at the cursor's y, one dot per series on the vertical line, x pill `19 Oct '26  14:00`, y readout pill on the right axis). Subtitle gets ` · 19 OCT, 14:00` appended in white. |
| 13.0–16.0 | Mouse moves over the Squawk feed: hovered row background goes from `#010101` to `#111111` (t13.5 row 15:54:59, t15.5 row 16:37:43). No click. |
| 16.5 | Cursor on the `⇄ FR` button. |
| 17.0 | Click → **EN** mode: feed and wire texts switch to the original English, `AUTO-TRANSLATED` label disappears, button becomes `⇄ EN` (grey), header re-flows from 6 description lines to 3 and gets shorter (wire row moves from y≈197 to y≈140, everything below moves up ~57 px). |
| 18.5–20.0 | Cursor rests on `⇄ EN`: hover state, button border goes from `#1b1b1b` to `#5c5c5c`. |
| 20.0–20.5 | Cursor crosses the gauge (no tooltip) and enters the currency chart at 23 Oct 04:00 (crosshair, CAD pill reads 0.83%). |
| 21.0–24.0 | Cursor rests on the `★ G20` tab of the Asset Registry (no visible change, tab already selected). At t24.0 the feed row 15:47:21 changes from `1h ago` to `2h ago` (live relative time). |
| 24.5 | Hover `EUROPE` tab: text turns yellow. |
| 25.0 | Hover `OCEANIA`: text turns yellow. |
| 26.0 | Click `OCEANIA`: becomes the yellow pill, `★ G20` loses its pill (grey text), count top-right `23` → `7`, list shows 7 rows (Australia and New Zealand highlighted, 5 rows dim with `WB` badge). |
| 26.5–28.0 | Cursor moves back into the currency chart (crosshair at 21 Oct 04:00 at t28.0). |
| 28.5–38.5 | Cursor parked at (808,545) = bar **20 Oct 20:00**; mouse-wheel **zoom out**, anchored at the cursor's bar. Visible range widens to ~20 Sep → 23 Oct. Every line is re-based to 0 % at the new first visible bar, so all values change; y-axis re-scales (labels up to 4.00 %); the last bars move left toward the cursor and an empty "future" area opens on the right of the plot. End labels re-sort and re-stack each step. At t34.0 the feed row 15:48:38 flips to `2h ago`. |
| 39.0–51.0 | Idle at maximum zoom-out (crosshair still drawn at 20 Oct 20:00, rings pulsing at the line ends). Tape, wire and globe keep moving. |
| 51.5–64.0 | Mouse-wheel **zoom in**, same anchor. Fewer bars; lines become visibly segmented (straight segments between hourly points); the last bar moves back to, then past, the right edge (lines run to the plot edge, rings no longer visible). x labels go from days (`13 15 18 … 23`) to hours (`12:00`, `06:00`, `03:00`, day number at midnight). At t59.0 feed row 15:50:51 flips to `2h ago`. Video ends at t64.0 with ~2 days visible. |

Continuous animations during the whole video: tape scroll, wire scroll, globe rotation, pulsing rings at the
currency-chart line ends, blinking cursor block after `ASSET REGISTRY`.

## 3. Layout (1600x966)

```
y=0  +---------------------------------------------------------------------------------------------+
y=7  | [● LIVE]  EURUSD 1.1223 -0.13% | GBPUSD 1.3390 -0.01% | …(scrolls)…  • New York • London • Tokyo • Sydney |
y=41 +---------------------------------------------------------------------------------------------+
y=49 +------------------+ +-------------------------------------------+ +------------------+
     | ⌁• SQUAWK [LIVE] | | RISK SENTIMENT ⓘ                          | | GLOBAL MAP ⓘ     |
     | desc  [⇄EN] 581  | |  (gauge 32)   | HISTORY · 1 YEAR  legend  | |                  |
     |------------------| |  1W ▼ 1 (WAS 33)  | band chart 0-100      | |    (3D globe)    |
     | wire (1 line)    | |  1M ▼ 28 (WAS 60) | NOV … OCT             | |                  |
     |------------------| +-------------------------------------------+ +------------------+ y=413
     | ALL DATA MOVERS… | y=293                                          y=420
     |------------------| +-------------------------------------------+ +------------------+
     | feed rows        | | CURRENCY STRENGTH · UNWEIGHTED ⓘ [FOREX⌄] 5m 15m 1H 4H D W | | • ASSET REGISTRY ▮ 23 |
     | (scroll)         | | % CHANGE SINCE THE FIRST BAR ON SCREEN    | | ★G20 AMERICA …   |
     |                  | |  8 lines                    | end labels  | | 01 🇺🇸 UNITED STATES [FED] |
     |                  | |  x axis                                   | | …  (scroll)      |
     |                  | +-------------------------------------------+ |                  |
     |                  | y=690 / 697                                   |                  |
     |                  | | CENTRAL BANKS ⓘ  (8-row table)            | |                  |
     | Terminal Data ·  | |                                           | |                  |
y=958+------------------+ +-------------------------------------------+ +------------------+
     x=8          x=416  x=423                                   x=1177 x=1185        x=1593
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Page background | 0 | 0 | 1600 | 966 | `#060402`–`#080400` (warm near-black), faint grid texture (see shared notes). |
| Market tape bar | 7 | 7 | 1586 | 35 | 1 px border `#464230`, radius ≈2, bg dark olive `#201804` (left) → `#151602` (right). Full width, stretches. |
| Squawk panel | 8 | 49 | 408 | 910 | Fixed width column (recommended). Feed list stretches vertically. |
| Risk Sentiment panel | 423 | 49 | 754 | 245 | Stretches horizontally (history chart takes the extra width). |
| Currency Strength panel | 423 | 300 | 754 | 391 | Stretches both ways (recommended: takes the extra height). |
| Central Banks panel | 423 | 697 | 754 | 262 | Fixed height (8 rows). |
| Global Map panel | 1185 | 49 | 408 | 365 | Fixed width; globe stays centred and square. |
| Asset Registry panel | 1185 | 420 | 408 | 539 | List stretches vertically, scrolls. |

Gutters: 7–8 px between columns and between stacked panels, 7–8 px page margin left/right, 8 px between
tape and panels, ~7 px at the bottom. Nothing in the video scrolls the page itself.

## 4. Components

Common panel style (see `_shared-notes.md`): bg `#050403`→`#0d0802` (very dark warm), 1 px border `#433d2c`,
radius ≈2 px, top-left corner bracket inset 5 px (L, arms ≈10 px, 2 px thick, `#807752`), bottom-right
bracket (mirror, `#716b3d`→`#9a8c50`). Panel titles: JetBrains Mono Bold ≈13 px, uppercase, letter-spacing
≈3.5 px, yellow `#EBD88A` (core `#FFE37F`), followed by an ⓘ info icon (circle 10 px, 1 px stroke,
`#5f5a50`). Title baseline ≈16 px below the panel top, left at panel x+11.

### 4.1 Market tape (x 7–1593, y 7–41)

- **LIVE pill** at x 14–64, y 13–34: bg `#FCD700`, radius 3, text `● LIVE` (black dot 6 px + `LIVE`),
  JetBrains Mono Bold 12 px, colour `#0b0800`. Static (does not blink).
- **Scrolling strip** from x≈70 to x≈1305: soft fade masks (~20 px) at both ends (text near x 1290–1305 is
  faded). Content scrolls right→left continuously at ≈222 px/s video (≈37 px/s real), loops seamlessly.
  Item format: `SYMBOL PRICE CHANGE`, then a separator `|` (1 px tall bar ≈12 px, `#3a3530`) with ≈14 px
  space each side.
  - Symbol: JetBrains Mono SemiBold 13 px, `#a8a49c` (grey), e.g. `EURUSD`, `S&P 500`.
  - Price: JetBrains Mono Bold 13 px, white `#F7F4F1`. Thousands separator `,`.
  - Change: JetBrains Mono 13 px, signed, 2 decimals, `%`. Negative `#E0666B`-ish (coral red; anti-aliased
    core measured `#d29296`), positive green `#5FBF7F` (`+0.41%`).
  - Order seen (loops): `EURUSD 1.1223 -0.13%` | `GBPUSD 1.3390 -0.01%` | `USDJPY 157.16 -0.32%` |
    `XAUUSD 4,010.00 -3.61%` | `WTI 103.44 -0.21%` | `S&P 500 7,623.7 -0.47%` | `NASDAQ 28,957.5 -0.79%` |
    `DOW 52,215.2 +0.41%` | (back to EURUSD).
  - Decimals: FX majors 4 dp, JPY pairs 2 dp, gold/oil 2 dp, indices 1 dp.
- **Session clocks** (right, fixed, not scrolling), 4 groups at x 1323, 1401, 1466, 1530: a status dot
  (6 px, `#7b7767` grey when closed) vertically centred, then two lines: name `New York` / `London` /
  `Tokyo` / `Sydney` (JetBrains Mono Medium 12 px, `#EBE7D8`) at y≈16 and state `Closed` (12 px,
  `#5f5a49`) at y≈30. All four were closed (Friday evening). Expected when open: coloured dot (session
  colour, see Market Hours) and text `Open` (not seen).

### 4.2 Squawk panel (x 8–416, y 49–958)

**Header** (EN mode: y 49–120; FR mode: y 49–178), contents vertically centred on one row:
- x 22: tiny antenna/wave glyph `⌁` (`#6b6045`, 8 px), x 33: yellow dot 5 px `#FCD700`, x 46: `SQUAWK`
  JetBrains Mono Bold 16 px, letter-spacing ≈4 px, `#F7DB85` (core `#FFE451`).
- x 120–152: `LIVE` badge: bg `#36320f`, radius 3, text JetBrains Mono Bold 10 px `#E6CB4A`, padding 4×3.
- x 159–300: description, proportional (Inter-like) 12 px, `#6b6f71`, line-height 19 px, wraps in a
  ≈140 px column: EN = `DATA headlines + real-time` / `price moves + fresh data` / `releases` (3 lines).
  FR mode wraps narrower (≈75 px column) into 6 lines: `DATA` / `headlines +` / `real-time` /
  `price moves +` / `fresh data` / `releases` because the extra label takes room.
- FR mode only: `AUTO-` / `TRANSLATED` (2 lines, JetBrains Mono 11 px, letter-spacing 1 px, `#646464`).
- Translate button: icon `文A`-style translate glyph + language code. FR (translation on): bg `#2c2508`,
  border 1 px `#4a3f10`, text `⇄ FR` JetBrains Mono Bold 12 px `#FFE477`. EN (translation off): bg
  `#060606`, border 1 px `#1b1b1b`, text `#9a9a9a`; hover border `#5c5c5c`. Size ≈44×28, radius 3.
- Right: `581` / `ITEMS` on two lines, JetBrains Mono 13 px, letter-spacing 1 px, `#626564`.

**Wire** (one line, EN y 125–156 / FR y 182–213, centre y 140 / 197), separated from header and filters by
1 px lines `#101010`. Text scrolls right→left ≈348 px/s video (≈58 px/s real), clipped at the panel edges
(no fade). JetBrains Mono 14 px. Items separated by ≈40 px:
- Price move: `HH:MM:SS` (`#5c5c5c`) ` • ` (dot coloured by sign: red `#E0666B` / cyan `#4FD1DA`) name
  (white Bold `#FFFFFF`) ` ±X.XX%` (red / cyan) ` — now ` (white) value (white Bold, thousands `,`).
  Example: `17:34:00 • Nikkei 225 -3.43% — now 62,693.16`.
- Headline: `HH:MM:SS • text` in white regular, e.g. `16:37:43 • US Secretary of Defense Hegseth: …`.
- Sequence seen: … Chevron +3.65% now 233.21, PepsiCo +3.26% 135.77, Intel -3.22% 86.96, Adobe +7.59%
  263.82, Salesforce +3.27% 248.57, Netflix -6.64% 65.85, Exxon Mobil +2.36% 183.21, Home Depot +3.29%
  319.83, Johnson & Johnson +3.23% 263.81, UnitedHealth +4.15% 418.09, JPMorgan Chase -2.40% 331.58,
  Tesla +4.93% 372.22, Meta Platforms -9.40% 642.62, Alphabet -5.98% 320.21, FTSE MIB -2.60% 48,558.86,
  Hang Seng -4.03% 23,093.37, Nikkei 225 -3.43% 62,693.16, VIX +5.48% 19.45, Coffee -3.07% 306.00,
  Wheat -4.62% 625.00, Palladium -6.24% 1,046.50, Platinum -5.06% 1,555.80, Gold -3.61% 4,010.00,
  Silver -5.29% 53.96 (all 17:34:00), DOT/USD -2.90% 1.09 (17:33:45), then the headlines 16:37:43,
  15:54:59, … Note: the wire shows **positive in cyan**, the tape shows positive in **green**.

**Filter tabs** (EN y 160–195, FR y 217–252), 4 tabs left-aligned from x 20, gap ≈20 px:
`ALL 581` · `DATA 517` · `◇ MOVERS 25` · `RELEASES 39`. Label JetBrains Mono SemiBold 12 px `#83878a`,
count 12 px `#646367`. Active tab (`DATA`): box bg `#332B03`, radius 3, padding 8×6, label `#FFE278`,
count `#66614a`. `◇` is a small outlined diamond.

**Feed list** (EN from y≈200 to 925, scrolls; no scrollbar visible): a 2 px vertical accent rule on the left
edge (x 8–9, `#6b6045` over `#4e3e03`), running the full list height. Rows separated by 1 px `#171717`; row
bg `#010101`, hover `#111111`. Row layout (top padding ≈12 px):
- x 24: time `16:37:43` JetBrains Mono 13 px `#8c8f8e`.
- x 97: relative age `1h ago` / `2h ago` JetBrains Mono 12 px `#575b5c` (live, refreshed periodically —
  flips observed at t24.0, t34.0, t59.0).
- x 165–177: type icon: yellow round "coin/data" glyph 12 px `#F0DB8F` (DATA items).
- x 187 → 405: text, proportional (Inter-like) 14 px regular, `#E8E6E4` (core `#f3f3f3`), line-height
  21 px, wraps; row height = text height + ≈24 px.
- EN rows seen (time / text): 16:37:43 `US Secretary of Defense Hegseth: Defense department designates
  Confucius Institutes as national security threat`; 15:54:59 `France Fin. Min. Lescure: The Moody's call
  recognizes the solidity of the French economy.`; 15:50:51 `Moody's: The recent sharp rise in French
  government bond yields adds to the risks of a further weakening of debt affordability.`; 15:48:38
  `Moody's projects a 5.2% of GDP deficit in 2027 for France.`; 15:48:05 `Moody's: France's rating remains
  at Aa3, assuming that a deficit-reducing budget for 2027 will be adopted.`; 15:47:21 `Moody's since
  previous periodic review of France rating, slower growth and higher deficit have pushed debt burden
  somewhat higer` (sic); 15:15:45 `US bank deposits rose to $19.810 tln from $19.772 tln in the prior
  week.`; 15:08:20 `Venezuela oil output at 1.2m barrels/day - Oil Minister.`; then `USD CFTC Positions…`
  (cut by the footer).
- FR versions (translation on): `Secrétaire américain à la Défense Hegseth : Le département de la Défense
  désigne les Instituts Confucius comme une menace pour la sécurité nationale`; `France Min. Fin. Lescure:
  L'appel de Moody's reconnaît la solidité de l'économie française.`; `Moody's : La hausse récente et
  marquée des rendements des obligations d'État françaises accroît les risques d'un affaiblissement
  supplémentaire de l'accessibilité de la dette.`; `Moody's projette un déficit de 5,2 % du PIB en 2027
  pour la France.`; `Moody's : la notation de la France reste à Aa3, en supposant qu'un budget réducteur
  de déficit pour 2027 sera adopté.`; `Selon Moody's, depuis l'examen périodique précédent de la notation
  de la France, la croissance plus lente et le déficit plus élevé ont fait augmenter quelque peu le
  fardeau de la dette`. Times and ages are not translated.

**Footer** (y 928–958, separated by 1 px `#1a1a1a`): left `Terminal Data · 16:37:43` proportional 10 px
`#656867` (time = timestamp of the newest item, static in the video); right `● LIVE` badge x 358–404,
y 933–953: bg `#342a03`, border 1 px `#7d6e03`→`#968612`, radius 3, text JetBrains Mono Bold 11 px
`#F8E169`, dot 4 px.

### 4.3 Risk Sentiment (x 423–1177, y 49–293)

Title `RISK SENTIMENT ⓘ` at (433, 65). Header separated by 1 px line `#181714` at y≈82. Body bg slightly
lifted olive `#1d1704`–`#211b06`; the gauge occupies x 430–711, the history chart x 715–1170.

**Gauge** (left part, hub at (567,181)):
- 240° dial: 0 at −120° (lower-left), 50 at top, 100 at +120° (lower-right).
- Outer scale: thin arc r≈86 (`#3c382f`), short yellow ticks (`#C8A93A`, 1 px × 5 px) every 10, labels
  `0 10 20 … 100` at r≈96, JetBrains Mono 7 px `#6e6a5f`.
- Segment ring: ≈50 radial bars (one per 2 points), inner r≈56, outer r≈78, bar ≈3 px wide. Bars from 0
  to the value are lit in the zone colour (value 32 → red bars `#E87F78`, with a soft glow); unlit bars
  are dim zone tints: 0–40 dim red `#3a1d1a`, 40–60 dim olive `#433e27`, 60–100 dim teal `#253127`
  → `#1c3a3c`.
- Tiny curved zone labels inside the ring (≈6 px, letter-spaced, dim): left `RISK-OFF` (dim red), top
  `NEUTRAL` (`#5c5740`), right `RISK-ON` (dim teal); and two small numbers near the 40/60 boundaries
  (partially legible: `30`, `70`).
- Needle: 2 px line from the hub to r≈62 at the value angle, colour `#FFB5AF`→`#E4A4A0` (light coral)
  with glow; hub: 9 px black disc with 2 px coral ring `#F08E8A`.
- Value `32`: JetBrains Mono Bold 22 px, `#FE938A` with glow, centred at (568,198). Under it `RISK-OFF`
  JetBrains Mono Bold 7 px, letter-spacing 1.5 px, `#AF7B7C`. Zone word follows the value (`RISK-OFF`
  < 40, `NEUTRAL` 40–60, `RISK-ON` > 60; colour = zone colour).
- Change chips under the gauge, two boxed rows (box bg `#0a0806`, border 1 px `#262219`, radius 2,
  h ≈22): `1W ▼ 1 (WAS 33)` at x 495–640, y 238–259 and `1M ▼ 28 (WAS 60)` at x 491–643, y 267–288.
  `1W`/`1M` JetBrains Mono 12 px `#908d86`; `▼` filled triangle 10 px `#FF908F` (would be `▲` cyan-green
  when up); delta value 12 px `#C68285` (red); `(WAS 33)` 12 px `#676257`, letter-spacing 1.5 px.

**History · 1 year chart** (plot x 727–1120, y 124–262):
- Label `HISTORY · 1 YEAR` at (720,89), JetBrains Mono 10 px, letter-spacing 2.5 px, `#605c55`.
- Legend at y 96, right-aligned to x 1162: `■ RISK-OFF < 40` (`#D48A8F`), `■ NEUTRAL 40-60` (`#FBDE84`),
  `■ RISK-ON > 60` (`#76CAD1`); squares 7 px; JetBrains Mono 10 px, letter-spacing 2 px. In the reference
  the legend overlaps the end of `HISTORY · 1 YEAR` (they collide at 1600 px) — keep them on one row but
  avoid the overlap.
- Background bands (full plot width): 80–100 `#12312C` (teal), 60–80 `#182215`, 40–60 `#14130a`
  (≈neutral), 20–40 `#2B190F`, 0–20 `#3E2015` (red-brown). Faint gridlines at 40 and 60 (1 px
  `#2a2618`) and a dotted line at 50.
- Line: daily values over 12 months, 1.5–2 px, smooth, **coloured by zone per segment**: < 40 red
  `#F2706E`, 40–60 yellow `#F7D950`, > 60 cyan `#35CDE4`; outer glow (blur ≈4 px, 40 % alpha). Last
  point at the right edge (value ≈32, red).
- Y axis on the right: `0 20 40 60 80 100` at x 1130–1150, proportional 11 px `#8a8780`.
- X axis: month labels `NOV DEC JAN FEB MAR APR MAY JUN JUL AUG SEP OCT` at y 277, proportional 11 px
  `#938f83`, uppercase, at each month start.
- No hover/tooltip was triggered in the video.

### 4.4 Currency Strength (x 423–1177, y 300–690)

Header row (y 320): title `CURRENCY STRENGTH · UNWEIGHTED ⓘ` at x 433. Right side:
- Dropdown `FOREX ⌄` at x 888–998, y 310–330: bg `#231d07`, radius 2, text JetBrains Mono Bold 11 px
  `#F4D95D`, chevron yellow at the right.
- Timeframe buttons `5m 15m 1H 4H D W` at x 1008, 1037, 1068, 1102, 1130, 1152; text JetBrains Mono
  11 px `#99968e`; active `1H`: pill bg `#FCD700`, radius 2, 25×20, text `#000` Bold.

Subtitle (y 356): `% CHANGE SINCE ` (`#a9a7a0`) + `THE FIRST BAR ON SCREEN` (yellow `#F4D95D`) in
JetBrains Mono 10 px, letter-spacing 2.5 px, uppercase. While the crosshair is shown it continues with
` · 20 OCT, 20:00` in white `#FFFEF9` (date of the hovered bar, `DD MON, HH:MM`).

**Plot**: x 427–1070, y≈370–650; right column x 1073–1165 for end labels and y-axis labels.
- 8 series, 1.5 px lines, straight segments between bars, no fill, no glow:
  CAD `#F05154` · NZD `#FCEB50` · AUD `#FBA800` · GBP `#00C6DA` · JPY `#F7F6F6` · CHF `#B864C6` ·
  USD `#5DB963` · EUR `#387AF8`.
- Value = % change of the currency's strength since the **first visible bar** (all lines start at 0 % at
  the left edge). The eight values sum to ≈0 (t9: 0.82+0.65+0.56+0.28−0.14−0.28−0.80−1.07 = 0.02), i.e.
  each currency's value is the unweighted mean of its % moves against the 7 others.
- Zero line: 1 px solid `#8f8c86`, full plot width and continuing under the label column to x≈1105.
- Y axis labels on the right (x≈1117–1155, right-aligned): JetBrains Mono 11 px `#928e88`, 2 dp + `%`
  (`1.00%`, `0.00%`, `-0.50%`, `-1.00%`; step 0.10/0.20/0.50/1.00 % chosen to fit). Labels are drawn
  **under** the end-label pills (partly hidden when they collide).
- X axis labels at y 670: JetBrains Mono 11 px `#8d8a83`: day of month (`15 16 19 20 21 22 23` — weekends
  skipped, the axis is in bar index, not calendar time), month short name at a month change (`Oct`),
  `HH:MM` when zoomed in (`06:00`, `12:00`, `03:00`); minimum spacing ≈70 px.
- **End labels** (one per series, at the y of the last visible value), at x 1073–1161, h 17 px, radius 1:
  bg = series colour; text JetBrains Mono Bold 11 px — `CAD` code, a 1 px dark separator at x≈1106, then
  the value right-aligned `0.82%` / `-1.07%` (2 dp, `-` sign, no `+`). Text colour is white on dark
  colours (CAD, AUD, GBP, CHF, USD, EUR) and dark `#3a3200` on light colours (NZD yellow, JPY white).
  Labels are sorted by value and pushed apart when they overlap (stack with 17 px pitch, no gap).
- **Last-point marker**: filled dot r 3 px in series colour at the last bar, plus a ring (1.5 px stroke,
  series colour ~50 % alpha, dark fill `#0a0a0a`), r≈10 px, **pulsing** (ring visible, then fades/grows,
  all series in sync; period ≈1–1.5 s video). Only drawn when the last bar is inside the plot.
- **Crosshair** (on hover):
  - vertical 1 px dashed line (`#C8B04A` at 60 %, dash 4/3) at the hovered bar, full plot height;
  - horizontal 1 px dashed line (`#6b5a1e`, dash 4/4) at the cursor y, from the plot left edge to the
    y-axis column;
  - a dot r 4 px in series colour (1 px dark outline) on every series at the hovered bar;
  - x pill at the bottom, centred on the bar: bg `#4B3D0A`, radius 2, h 22, text `20 Oct '26   20:00`
    JetBrains Mono Bold 11 px `#F2EEDC` (date and time separated by ≈3 spaces); it hides the axis labels
    under it;
  - y readout pill on the right axis at the cursor y: bg `#4C3C04`, border 1 px `#8a7a3a`, h 18,
    text e.g. `-0.30%` JetBrains Mono Bold 11 px white; drawn on top of axis labels and end labels.
- Zoom: mouse wheel, anchored on the bar under the cursor (that bar keeps its x). Zooming out can show an
  empty area to the right of the last bar. Bars visible ranged from ~48 (max zoom-in) to ~550 (max
  zoom-out seen). After each wheel step the series are re-based on the new first visible bar, the y range
  is re-fitted on the visible data (with ≈8 % padding) and the end labels are recomputed.
- Panel bottom-right bracket at (1162–1172, 676–683).

### 4.5 Central Banks (x 423–1177, y 697–958)

Title `CENTRAL BANKS ⓘ` at (433,714). Table:
- Header row y 747: `CENTRAL BANK` (x 434), `RATE` (right-aligned at 676), `NEXT MEETING` (x 752),
  `CUT` (right at 1038), `HOLD` (right at 1103), `HIKE` (right at 1166). JetBrains Mono 9 px,
  letter-spacing 1.5 px, `#66635b`.
- Rows at y 771, 796, 820, 845, 869, 893, 918, 942 (pitch ≈24.4 px), no separators, no zebra.
  - Flag 18×12 at x 434; name `Fed` proportional 12 px `#E7E6E1` + ` (USD)` `#73706a`.
  - Rate `4.00%`: JetBrains Mono Bold 12 px white `#FFFFFC`, 2 dp, right-aligned at x 690.
  - Next meeting `28 Oct 2026`: JetBrains Mono 12 px `#F5F2ED`, format `DD Mon YYYY`.
  - Cut `0.2%`: JetBrains Mono 12 px dim red `#7e6261` (1 dp).
  - Hold / Hike: integer %, the **larger of hold and hike is bold and bright** (hold max → bold white
    `#FFFFFA`; hike max → bold cyan `#65CFE0`); the other is regular dim (hold `#adaaa4`, hike
    `#4f7e81`).
- Data rows: Fed (USD) 4.00% 28 Oct 2026 0.2% **93%** 7% · ECB (EUR) 2.50% 29 Oct 2026 0.1% **98%** 2% ·
  BoE (GBP) 3.75% 05 Nov 2026 0.2% 3% **97%** · BoJ (JPY) 1.23% 30 Oct 2026 0.1% **97%** 3% · RBA (AUD)
  4.60% 03 Nov 2026 0.2% 32% **68%** · RBNZ (NZD) 2.75% 28 Oct 2026 0.1% 12% **88%** · BoC (CAD) 2.25%
  28 Oct 2026 0.1% 19% **81%** · SNB (CHF) 0.00% 10 Dec 2026 0.3% **81%** 19%.
- Flags: US, EU, UK, Japan, Australia, New Zealand, Canada, Switzerland (flat rectangular, 18×12).
- No interaction seen.

### 4.6 Global Map (x 1185–1593, y 49–413)

Title `GLOBAL MAP ⓘ` at (1196,65). Body bg: radial gradient centred on the globe, `#2a1a04` at the globe
edge → `#0a0802` at the panel corners (warm amber haze).
- 3D globe centred at (1388,245), radius ≈148 px (x 1240–1537, y 97–393). Photographic Earth texture
  (Blue Marble style, blue oceans, land relief, white clouds/ice), lit from the upper-left (terminator-like
  darkening toward the lower-right limb).
- Atmosphere: orange-gold rim glow, ≈18 px wide, `#C48A1A` at the limb fading to transparent.
- Rotation: west→east (the surface moves left→right, Asia → Africa/Europe → Atlantic → Americas →
  Pacific), axis tilted slightly (north pole toward the viewer/top), one revolution ≈20 s video
  (≈2 min real). Never stops.
- Markers on the surface (move with the globe, hidden on the far side): small ring + dot, ≈6–8 px, in the
  FX-session colours: New York gold/yellow ring, London teal dot, Tokyo red ring, (Sydney purple, not
  clearly seen). Same colours as the Market Hours page.
- No interaction seen (no drag, no hover).

### 4.7 Asset Registry (x 1185–1593, y 420–958)

- Header (y 436): yellow dot 5 px (`#6b5f26` dim) at x 1200, `ASSET REGISTRY` JetBrains Mono 10 px,
  letter-spacing 3 px, `#75705f`, then a **blinking block cursor** 6×12 px `#FEDE21` (on/off ≈0.5–1 s
  video). Right-aligned count `23` / `7`, JetBrains Mono 10 px `#645c46`.
- Region tabs (y 468): `★ G20` · `AMERICA` · `EUROPE` · `AFRICA` · `ASIA` · `OCEANIA` at x 1195, 1240,
  1299, 1351, 1403, 1442; JetBrains Mono 10 px, letter-spacing 1 px, `#98948a`; hover text `#FCD700`;
  selected = pill bg `#FCD700`, radius 3, text black Bold (`★` included for G20).
- A 2 px vertical accent rule `#6b6045` along the left edge of the list (as in Squawk).
- Rows (pitch ≈31.7 px; first row centre y 499), separated by 1 px `#1e1a0f`; dim-row bg `#140f03`:
  - x 1197: index `01` JetBrains Mono 11 px `#615c45`;
  - x 1218: flag 22×15;
  - x 1248: country name, bold grotesque sans 13 px, uppercase, `#FFFFF1` (e.g. `UNITED STATES`,
    `SOLOMON IS.`);
  - right (x ≈1540–1580): institution badge `• FED`: bg `#594C01`, radius 2, text JetBrains Mono Bold
    9 px `#FBEBAA`.
  - **Highlighted rows** (countries whose rate comes from a central bank or BIS series): row bg `#2B2405`
    plus a 2 px yellow bar `#FAE77C` at the row's left edge, bright badge. **Dim rows** (only World Bank
    data, badge `• WB`): bg transparent, badge bg `#1c1805`, text `#615e51`, name still white.
- G20 list (23 entries, 15 visible, list scrolls): 01 UNITED STATES FED · 02 EURO ZONE ECB · 03 CHINA
  BIS · 04 JAPAN BOJ · 05 GERMANY ECB · 06 UNITED KINGDOM BOE · 07 FRANCE ECB · 08 INDIA BIS · 09 ITALY
  ECB · 10 BRAZIL BIS · 11 CANADA BOC · 12 SOUTH KOREA BIS · 13 RUSSIA BIS · 14 SPAIN ECB · 15 AUSTRALIA
  RBA … (all highlighted).
- OCEANIA list (7): 01 AUSTRALIA RBA (highlighted) · 02 FIJI WB · 03 NEW CALEDONIA WB · 04 NEW ZEALAND
  RBNZ (highlighted) · 05 PAPUA NEW GUINEA WB · 06 SOLOMON IS. WB · 07 VANUATU WB. Empty space below.
- Row click was not shown.

## 5. Interactions

| Element | Action | Result |
|---|---|---|
| Tape | none (auto) | Continuous right→left scroll, seamless loop; clocks fixed. |
| Squawk wire | none (auto) | Continuous scroll; new items appended at the end. |
| Translate button | hover | Border `#1b1b1b` → `#5c5c5c` (EN state). |
| Translate button | click | Toggles FR (auto-translation) ↔ EN (original). FR: button yellow, label `AUTO-TRANSLATED` shown, feed/wire texts in French. Header height changes; list re-flows instantly (no animation). |
| Filter tabs | (click, not shown) | Filters the feed: ALL / DATA / MOVERS / RELEASES. Active tab = olive box. |
| Feed row | hover | Row bg `#111111`. Cursor stays an arrow. |
| Feed ages | auto | `Nh ago` refreshed periodically (seen flipping 1h→2h). |
| Currency chart | hover | Crosshair, per-series dots, x pill, y readout pill, subtitle gets ` · DD MON, HH:MM`. Disappears on leave (assumed). |
| Currency chart | wheel | Zoom in/out anchored at the cursor's bar; re-base on first visible bar; y re-fit; end labels re-stacked. |
| Timeframe buttons | (click, not shown) | Select 5m/15m/1H/4H/D/W bars. |
| FOREX dropdown | (click, not shown) | Choose the universe (only FOREX seen). |
| Line ends | auto | Pulsing rings at each last point. |
| Region tabs | hover | Text turns yellow. |
| Region tabs | click | Selected pill moves; list and count replaced instantly. |
| Gauge, history, central banks, globe | hover | No visible effect. |
| Asset registry cursor | auto | Block cursor blinks. |

## 6. Data

| Field | Sample values | Unit / format | Update | Source on screen / notes |
|---|---|---|---|---|
| Tape quotes | EURUSD 1.1223 −0.13 %, XAUUSD 4,010.00 −3.61 %, DOW 52,215.2 +0.41 % | price + % change vs previous close | live tick | none shown |
| Session status (tape) | New York Closed … | Open/Closed | 1 s | computed from session hours (Market Hours page) |
| Squawk items count | 581 ITEMS; ALL 581, DATA 517, MOVERS 25, RELEASES 39 | integers | live | "Terminal Data" |
| Squawk wire movers | Meta Platforms −9.40 % — now 642.62, 17:34:00 | % move + last price, time HH:MM:SS | live (batches, same timestamp) | computed price-move alerts |
| Headlines | 16:37:43 `US Secretary of Defense Hegseth: …` | text, local time | live | "Terminal Data · 16:37:43" (footer) |
| Translation | FR texts | text | on toggle | machine translation, label `AUTO-TRANSLATED` |
| Risk sentiment | 32 RISK-OFF; 1W ▼1 (WAS 33); 1M ▼28 (WAS 60) | 0–100 index, integer | daily (maybe intraday) | computed composite score (method not visible) |
| Risk history | 1 year daily, range ≈23–80 | 0–100 | daily | same index |
| Currency strength | CAD 0.82 %, EUR −1.07 % (1H, 15–23 Oct) | % change since first visible bar, 2 dp | live (last bar), bars 5m→W | computed from FX pairs, unweighted mean; sums to ≈0 |
| Central bank rate | Fed 4.00 %, BoJ 1.23 %, SNB 0.00 % | %, 2 dp | on decision | policy rates |
| Next meeting | 28 Oct 2026 | date | on calendar change | CB calendars |
| Cut/Hold/Hike | Fed 0.2 % / 93 % / 7 % | market-implied probabilities, cut 1 dp, others integer | intraday/daily | market-implied (OIS / futures), method not visible |
| Globe markers | NY, London, Tokyo, Sydney | positions | static | session centres |
| Registry | G20 = 23, OCEANIA = 7; badge FED/ECB/BOJ/BOE/BOC/RBA/RBNZ/BIS/WB | country list + rate source | static | BIS = BIS policy-rate series, WB = World Bank (inferred) |
| Footer time | Terminal Data · 16:37:43 | HH:MM:SS | on new item | |

Scenario context in the video: Friday 23 Oct 2026 evening (all sessions closed), local time ≈17:47
(relative ages), wire batch at 17:34:00.

## 7. Key frames

| Frame | State |
|---|---|
| `t000.0.jpg` | Initial: FR mode, DATA filter, G20 registry, chart default zoom, no hover. |
| `t011.5.jpg` | Crosshair at 19 Oct 14:00 with dots, pills and subtitle date. |
| `t013.5.jpg` | Feed row hover (15:54:59) in FR mode. |
| `t017.0.jpg` | Just after the switch to EN: header 3 lines, `⇄ EN`. |
| `t019.0.jpg` | `⇄ EN` hover border. |
| `t026.0.jpg` | OCEANIA selected, 7 rows, highlighted vs dim rows. |
| `t034.0.jpg` | Mid zoom-out, future area opening on the right, pulsing rings. |
| `t049.0.jpg` | Max zoom-out, clean state for colour sampling (EN, OCEANIA). |
| `t055.0.jpg` | Mid zoom-in, end labels stacked, rings visible. |
| `t064.0.jpg` | Max zoom-in (hours on the x axis, segmented lines, last bar past the edge). |

## 8. Open questions

- Exact playback speed of this recording (≈6x assumed from Market Hours); real speeds of tape (≈37 px/s),
  wire (≈58 px/s), globe (≈2 min per turn) and ring pulse are estimates.
- Risk Sentiment: inputs and formula of the index; the small curved zone words and the two inner numbers
  are not fully legible; up-move colour of the `▼/▲` chips not seen; history tooltip not shown.
- Currency Strength: other entries of the `FOREX` dropdown; what happens on click; y-axis step rules;
  whether the crosshair stays when the mouse leaves (it stayed during the wheel zoom because the cursor
  did not move); maximum zoom-out; behaviour of the other timeframes.
- Squawk: icons for MOVERS and RELEASES rows (only DATA rows seen); what the RELEASES and MOVERS filters
  show; how relative ages are formatted under 1 h (`m ago`?) and over 24 h; whether the wire is also
  translated in FR mode; a click on a feed row.
- Tape: full instrument list and whether it is configurable; colour of a zero change; open-session dot
  colour and text.
- Central Banks: source of probabilities; whether rows are clickable/sortable; why BoE shows 97 % hike
  (data quirk in the reference).
- Global map: meaning and exact colours of all markers (Sydney marker not clearly seen), whether the globe
  can be dragged or clicked, day/night shading.
- Asset Registry: the full lists of AMERICA/EUROPE/AFRICA/ASIA; row click behaviour; the exact rule for
  highlighted vs dim rows (inferred: has a CB/BIS policy-rate series).
- How the layout behaves at other window sizes (only 1600x966 seen).
