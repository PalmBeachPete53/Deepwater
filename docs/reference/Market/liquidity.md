# Market › Liquidity

Reference video: `Reference/Market/liquidity.mp4` (42.5 s, 85 frames at 0.5 s, 1600x966).
Frames: `SCRATCH/frames/Market/liquidity/tNNN.N.jpg`.
Colours measured with `tools/pick` (JPEG noise ±4). Shared widgets: see `_shared-notes-2.md`.

## 1. Purpose

"Liquidity mapping — stop pools & sweeps" for one instrument at a time (EUR/USD in the video). From real hourly
OHLC, the page finds 3-bar fractal swing highs/lows, clusters the ones within 0.06 % of each other into equal-high
(buy-side, BSL) and equal-low (sell-side, SSL) **liquidity pools**, draws them as horizontal levels on a candlestick
chart together with the previous day high/low (PDH/PDL), and reports: how likely a stop-run is (a disclosed rule
based on the distance to the nearer untested pool), how many pools are still untested, which pools were **swept**
(pierced then closed back), and the session highs/lows (Sydney, Tokyo, London, New York, previous day, previous
week). An instrument picker in the title strip switches the instrument.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | Page at scroll top. Cursor over the chart at (810,295): crosshair visible — vertical dashed line at x=807, horizontal dashed line at y=286, price label `1.14` on the price axis, time label `09 oct. '26  18:00:00` on the time axis. Tab **LIQUIDITY POOLS** active. |
| 0.5–3.5 | Cursor moves right across the chart; crosshair follows (snaps to candle times). |
| 4.5 | Crosshair at `16 oct. '26  13:00:00`, price label `1.13` (x=1149, y=399). |
| 5.0–6.0 | Cursor continues right. |
| 6.5 | Crosshair at `21 oct. '26  03:00:00`, price `1.12` (x=1333, y=511). |
| 7.0–7.5 | Cursor leaves the chart upward to the instrument button (x≈470, y≈45); crosshair disappears. |
| 8.0 | Click on the button `EUR/USD ⌄` → caret flips to `⌃`, dropdown list opens under it (x407–596, y45–303); search field focused with a text caret. |
| 8.5–9.5 | Dropdown open, cursor on the button. |
| 10.0 | Mouse wheel over the page: the whole content below the title strip scrolls up by ~214 px (KPI cards scroll out; chart top goes under the title strip; the liquidity pool list fills the bottom). Dropdown stays open on top. |
| 10.5–11.0 | Cursor moves into the chart at (323,180): crosshair `01 oct. '26  00:00:00`, price `1.13`. |
| 11.5 | Cursor on the time axis: crosshair hidden. |
| 12.0–13.5 | Cursor moves down to the tab bar. |
| 14.0 | Hover on the active LIQUIDITY POOLS tab: a 1 px rounded box appears around the tab (focus/hover ring). |
| 15.0–15.5 | Cursor to **SWEEP ALERTS**; its text turns gold on hover. |
| 16.5 | Click SWEEP ALERTS: it becomes active (gold text + 2 px gold underline); list replaced by sweep cards. |
| 17.0–20.5 | Cursor rests then moves to **SESSION EXTREMES** (hover box + gold text). |
| 21.0 | Click SESSION EXTREMES: table of session highs/lows. The page is now shorter, so the scroll position clamps (content shifts down ~59 px: chart time axis from y444 to y503); the footnote line appears at y≈916. |
| 21.5–35.0 | Idle (cursor at 810,545 just under the chart). The chart's price-axis labels flicker by 1 px (sub-pixel re-render, diff ≈1). |
| 35.5 | Wheel: page scrolls up another ~17 px (chart bottom now at y≈540, cursor at the chart's bottom edge). |
| 36.0–42.0 | Repeated wheel over the chart: **chart zooms in on the time axis around the cursor x** (≈810). Time ticks go from days (`25 29 oct. 4 7 …`) to `12:00` ticks (37.5) to 3-hour ticks `03:00 06:00 09:00 …` (40.5–42). Candles widen from ~2 px to ~40 px; price axis auto-scales to the visible range (levels move apart); pool lines that leave the visible price range disappear with their tags. |

## 3. Layout (1600x966, at scroll top, frame t000.0)

```
y=0   ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
      │ ≡ ● LIQUIDITY MAPPING — STOP POOLS & SWEEPS▌ [EUR/USD ⌄]                              1.12045 │ fixed title strip
y=46  ├──────────────────────────────────────────────────────────────────────────────────────────────┤
y=61  │ ┌STOP-RUN LIKELIHOOD──────┐ ┌UNTESTED POOLS──────────┐ ┌SWEEPS (VISIBLE HISTORY)─────────┐ │
      │ │High  / Sell-side closer │ │49 / of 62 detected     │ │13 / 9d ago                       │ │ KPI h=85
y=146 │ └─────────────────────────┘ └────────────────────────┘ └──────────────────────────────────┘ │
y=156 │ ┌ legend: BSL BUY-SIDE POOL  SSL SELL-SIDE POOL  PDH / PDL ─────────────────────────┬──────┐ │
      │ │                       candlestick chart (hourly)          pool lines + tags ───▶│price │ │ chart h=521
      │ │                                                                                │axis  │ │
y=645 │ │ 25   29   oct.   4   7   9   13   15   18   21   23   (time axis)             │      │ │
y=677 │ └──────────────────────────────────────────────────────────────────────────────────────┘ │
y=688 │ ┌ LIQUIDITY POOLS | SWEEP ALERTS | SESSION EXTREMES ────────────────────────────────────┐ │
      │ │ | BSL  1.18749     ●●●●● +16                                              +5.98%   │ │ list (scrolls with page)
      │ │ …                                                                                    │ │
y=940 ├──────────────────────────────────────────────────────────────────────────────────────────────┤
      │ Terminal Data · 16:29:27                                                         [● DELAYED] │ fixed footer
y=966 └──────────────────────────────────────────────────────────────────────────────────────────────┘
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Title strip (fixed) | 0 | 0 | 1600 | 46 | bottom border 1 px `#393617` at y=46 |
| Scroll area | 0 | 47 | 1600 | 893 | everything between title strip and footer scrolls vertically (no visible scrollbar) |
| KPI cards (3) | 14 / 540 / 1066 | 61 | 519 each | 85 | gap 7 px, equal widths, stretch with window |
| Chart panel | 14 | 156 | 1572 | 521 | 1 px border `#393322`/`#3f3a2f`, bg `#0a0802` |
| — legend | 22 | 166 | — | 10 | inside panel top-left |
| — plot | 15 | ~180 | 1521 | ~465 | price axis column x1537–1585 (left border 1 px `#312e1b`), time axis y645–676 (top border `#3b3724`) |
| — resize grip | 1568 | 660 | 12 | 12 | small L corner bottom-right (`#7f7d66`) |
| Bottom panel | 14 | 688 | 1572 | content | 1 px border `#3f3b1a`/`#453c22`, bg `#0d0802` with a faint hexagon pattern |
| — tab bar | 15 | 689 | — | 31 | tabs at y≈704, underline y=717–719 |
| — list rows | 15 | 724 | — | 36.3 pitch | |
| Footnote | 14 | ~911 | — | 10 | after bottom panel (seen in Session extremes state) |
| Footer (fixed) | 0 | 940 | 1600 | 26 | `Terminal Data · 16:29:27` left, `● DELAYED` badge right |

When the window grows: KPI cards and both panels stretch horizontally; the chart's height looks fixed (~521);
the bottom panel grows with its content and the page scrolls.

## 4. Components

### 4.1 Title strip
- `≡` menu glyph (x13–25, y17–28), gold `#d7cb88`.
- Dot ● (x34–42, y18–26) gold `#fde45c`, pulsing (period ≈ 2 s, dim `#6d6613` ↔ bright `#f1df05`).
- Title `LIQUIDITY MAPPING — STOP POOLS & SWEEPS`: monospace ~10 px uppercase, letter-spacing ~3.5 px,
  `#676558`, x50–375, y≈22. Blinking gold block cursor `▌` after it (x≈380–388, `#fdd71d`, ~1–2 s steps).
- Instrument button x407–484, y9–36: 1 px border `#47412d`, bg `#0f0704`, text `EUR/USD` monospace ~11 px
  `#e3e0db`, then a small chevron `⌄` (`⌃` when open).
- Right: last price `1.12045`, monospace bold ~18 px, white `#fffffc`, right-aligned at x=1586, y≈13–30.

### 4.2 Instrument dropdown
- Panel x407–596 (w≈190), y45–303, bg `#0d0802`, 1 px border `#413e1e`/`#454222`, drop shadow; list clipped at the
  bottom (9th item cut) → scrollable list.
- Search field x414–591, y52–76: bg `#110b03`/`#060502`, 1 px border `#1b170b`, magnifier icon, placeholder
  `Search...` (proportional ~11 px `#4d4741`), text caret when focused.
- Items, pitch ≈25.5 px, first at y≈93: `EUR/USD`, `GBP/USD`, `USD/JPY`, `USD/CHF`, `USD/CAD`, `AUD/USD`, `NZD/USD`,
  `EUR/GBP`, `EUR/JPY` … Label monospace-ish ~11 px `#dddad6` at x=420; right tag `FX` tiny (~7 px) `#454139` at
  x≈580 (asset class).
- Current item: bg `#29251d`, radius 2 px, label gold `#d1c78a`.
- Opening: caret flips, list appears instantly (no animation seen). It stayed open while scrolling and hovering
  the chart (does not close on outside hover).

### 4.3 KPI cards (3)
Same card as the shared KPI card: bg `#0a0600`, 1 px border `#443522`/`#433b2f`, corner brackets top-left and
bottom-right (`#978567`), 2 px top accent with glow.

| Card | Label | Value | Sub-line | Accent |
|---|---|---|---|---|
| 1 | `STOP-RUN LIKELIHOOD` | `High` (red `#f87171`, measured `#f0a6a5`/`#b13c31`) | `Sell-side closer` | red `#e39e98` |
| 2 | `UNTESTED POOLS` | `49` (white `#fffffa`) | `of 62 detected` | gold `#f6e092` |
| 3 | `SWEEPS (VISIBLE HISTORY)` | `13` (white) | `9d ago` | gold `#f7e396` |

- Label: monospace ~10 px uppercase, letter-spacing ~3.5 px, `#666357`, x = card+13, y≈77.
- Value: monospace bold ~22 px, x = card+13, y≈92–112. (`High` would presumably be gold for Medium / grey for Low.)
- Sub-line: proportional ~9.5 px `#737069`, y≈128.

### 4.4 Chart panel (candlestick + pools)
Looks like TradingView Lightweight Charts with a French locale (month `oct.`, crosshair date `09 oct. '26`).
- Legend (y≈170): `—— BSL BUY-SIDE POOL` (red `#c78e8e` text, red dashed swatch), `—— SSL SELL-SIDE POOL` (cyan
  `#81b8bc`), `··· PDH / PDL` (gold `#e3d27b`). Monospace ~10 px uppercase, letter-spacing ~3 px. Corner bracket
  top-left of the panel.
- Background `#0a0802`; grid lines very faint (`#140f03`, vertical every time tick, horizontal every price tick).
- Candles: hourly. Up = cyan `#22d3ee` (measured `#0095aa` edge / `#63d5e2`), down = red/salmon `#f87171`
  (`#e99793`, edge `#ad3724`); wicks same colour, 1 px. ~1 month visible by default (25 Sep → 23 Oct).
- Price axis (right, x1537–1585): labels `1.14`, `1.13`, `1.12`, `1.11` (2 dp only), monospace ~9 px `#8e8d86`.
- Time axis (bottom, y645–676): labels `25`, `29`, `oct.`, `4`, `7`, `9`, `13`, `15`, `18`, `21`, `23`; when zoomed:
  `12:00`, `6`, `7`; then `03:00 06:00 09:00 12:00 15:00 18:00 20:00 11 01:00`. Monospace ~9 px `#898882`.
- **Pool lines**: horizontal lines across the whole plot width.
  - BSL (equal highs, buy-side): **dotted** red, 1 px, `#f87171` at ~60 % (`#947d7c` on bg).
  - SSL (equal lows, sell-side): **dotted** cyan, 1 px, `#22d3ee` at ~55 % (`#819091`).
  - PDH / PDL: **dashed** gold (dash 6, gap 4), 1–1.5 px, `#facc15` (`#f7bb72` measured at the dash ends).
- **Level tags** on the price axis, right side, h≈14, two parts:
  - left part (label, x≈1481–1536, right-aligned to the axis): `SSL ×7`, `BSL ×14`, `PDH`, `SSL ×2`, `BSL ×44`,
    `SSL ×31`, `PDL`, `SSL ×5`, `SSL ×4`, `BSL ×14` — `×N` = number of swing points in the pool. Monospace ~9 px.
  - right part (value, x1537–1575): price at 2 dp (`1.13`, `1.12`, `1.11`).
  - Colours: SSL bg cyan `#22d3ee` (measured `#03ddec`), text dark `#0b2a33`; BSL bg red `#f87171` (`#ff7f75`),
    text white; PDH/PDL bg gold `#facc15` (`#ffdb00`), text dark. Tags stack without overlap (pushed apart
    vertically when levels are close).
- **Last price tag**: unlabelled value tag `1.12` in cyan (last candle up) or red (down) on the price axis
  (lightweight-charts last-value label); no clear price line seen.
- **Crosshair**: dashed (4/4) vertical and horizontal lines, 1 px `#8b8881`; price label on the axis (bg
  `#1a1d2c`, text `#f5f6ff`, monospace bold ~9 px), time label on the time axis (bg `#172230`, text `#e9edf2`,
  format `DD mmm 'YY  HH:MM:SS`, e.g. `16 oct. '26  13:00:00`). Hidden when the cursor is outside the plot.
- **Zoom**: mouse wheel zooms the time scale around the cursor; price axis auto-fits the visible candles and the
  visible levels; scroll page only when the wheel is outside the chart.

### 4.5 Bottom panel: tabs
- Tabs `LIQUIDITY POOLS`, `SWEEP ALERTS`, `SESSION EXTREMES` at x27, x131, x227, y≈704. Monospace ~9.5 px uppercase,
  letter-spacing ~0.5 px, semibold.
- Inactive: `#68655b`. Hover on an inactive tab: text turns gold (no box). Active: text gold `#e7ddae`/`#bca02c`,
  2 px underline gold `#f3e197` under the text (x22–111 for the first tab).
- Focus box: when the active tab is hovered (t14.0) or has just been clicked (t18.0, t22.5 onward), a 1 px rounded
  (r≈3) box `#575654` with a faint gold tint surrounds it (x214–333, y535–566 in frame t027.0). It is **not** there
  in the first frame after the click (t17.0, t21.0) and appears ~0.5–1 s later, then stays even when the mouse
  leaves → treat it as a keyboard/focus ring with a short fade-in.
- Separator under the tab bar: 1 px `#1e1a0f`.

### 4.6 Tab LIQUIDITY POOLS (list)
Rows pitch 36.3 px (first at y≈742 at scroll top), separated by 1 px `#0f0a02`-ish lines; full panel width.
- Side tag at x28: a 2 px vertical bar + `BSL` (red `#e2a9a7`, bar `#e05656`) or `SSL` (cyan `#9fced6`), monospace
  bold ~8.5 px.
- Price at x102: monospace bold ~14 px white `#fffffa`, 5 dp (`1.18749`).
- Strength dots at x248: up to 5 dots (4 px, gap 3 px) in the side colour, increasing opacity left→right
  (first dim `#641104`, last bright `#e7757d`); then `+N` (monospace ~9 px `#5c5951`) = touches beyond 5
  (`+16`, `+8`, `+486`, `+622`, `+146`, `+2`, `+9`, `+39`, `+26`); pools with fewer touches show 2–3 dots and no `+N`.
- Right (right-aligned at x1572): distance from last price, signed %, 2 dp, monospace ~10 px `#928e88`
  (`+5.98%`, `+0.09%`, `-0.00%`).
- Sorted by price descending. A **LAST** separator row is inserted where the current price falls:
  band bg `#241e01`–`#2a220d` (gold-tinted, full width, h≈30), `LAST` (gold bold ~8.5 px `#eee490`, x28) and
  `1.12045` (gold monospace bold ~14 px `#f9e59b`, x64), then a 1 px gold line `#6a5b28` to the right edge.
- Sample (EUR/USD): BSL 1.18749 ●●●●● +16 +5.98% · SSL 1.18542 ●●●●● +8 +5.80% · BSL 1.16732 +486 +4.18% ·
  SSL 1.15891 +622 +3.43% · BSL 1.14230 +146 +1.95% · BSL 1.13178 ●●● +1.01% · SSL 1.12754 +2 +0.63% ·
  BSL 1.12727 +9 +0.61% · SSL 1.12524 ●● +0.43% · BSL 1.12146 +39 +0.09% · LAST 1.12045 · SSL 1.12042 +26 -0.00% …

### 4.7 Tab SWEEP ALERTS (cards)
Each card spans the panel width, height ≈ 77 (pitch 77), separated by 1 px `#1d180d`; a 2 px vertical colour bar
on the left edge (x15–16): cyan `#7ed7e8` for sell-side sweeps, red `#e8a4a5` for buy-side.
- Line 1 (y+10): `● LIQUIDITY SWEEP DETECTED` — dot + text in the side colour (cyan `#32d2da` / red `#c44f54`),
  monospace bold ~8.5 px uppercase.
- Line 2 (y+30): `Sell-side liquidity taken at ` proportional ~12.5 px `#f2f0eb` + price monospace bold ~13 px white
  `1.11173`.
- Line 3 (y+50): `Reversal confirmation: ` (`#6e6b64`, ~9 px) + `Strong` (bold white `#fdfbf5`) + ` · 9d ago`
  (`#67645c`).
- Order: most recent first. Seen: Sell-side 1.11173 Strong 9d ago; Sell-side 1.19025 Strong 255d ago; Buy-side
  1.11421 Strong 528d ago; Buy-side 1.09833 Strong 562d ago; Buy-side 1.06394 Strong 597d ago; Buy-side 1.06300 …

### 4.8 Tab SESSION EXTREMES (table)
- Header row y≈583: `WINDOW` (left x28), `HIGH` (right-aligned x1452), `LOW` (right-aligned x1572); monospace ~8 px
  uppercase letter-spaced `#6a6760` / `#5c5951`.
- Rows pitch ≈ 50.7 px: name proportional semibold ~12 px, sub-line ~8.5 px `#48453c`; HIGH value red `#f87171`
  (`#de9799`), LOW cyan `#22d3ee` (`#91c4c5`), monospace ~14 px, 5 dp; missing value `—` `#56524b`.

| Window | Sub-line | High | Low |
|---|---|---|---|
| Sydney (white) | `today · UTC` | — | — |
| Asian (Tokyo) | `today · UTC` | 1.12296 | 1.11894 |
| London | `today · UTC` | 1.12133 | 1.11894 |
| New York | `today · UTC` | 1.12108 | 1.11932 |
| Previous day (gold `#f7e49c`) | `2026-10-22` | 1.12575 | 1.11932 |
| Previous week (gold) | `high / low` | 1.12727 | 1.11173 |

- Footnote below the panel (y≈916), proportional ~9 px `#4c4b42`, one line:
  `Real 3-bar fractal swings clustered within 0.06% into equal-high/low pools, from real hourly OHLC. A pool is "swept" the first real candle that pierces it and closes back on the other side. Stop-run likelihood is a disclosed rule (relative distance to the nearer untested pool) — never a backtested statistical probability.`

### 4.9 Footer (fixed)
- `Terminal Data · 16:29:27` left at x7, y≈950, proportional ~9 px `#6d6d65` (time did not change during the video).
- Badge right x1524–1592, y941–962: 1 px gold-dim border, dark bg, gold dot + `DELAYED` monospace bold ~9 px
  `#ffe278`, letter-spacing 1 px.

## 5. Interactions

| Element | Action | Result |
|---|---|---|
| Chart | hover | dashed crosshair + price and time labels (snapped to candle) |
| Chart | wheel | zoom time axis around cursor, price auto-scale |
| Chart | drag (inferred) | pan (lightweight-charts default) |
| Chart corner grip | drag (inferred) | resize chart height |
| Instrument button | click | dropdown with search, list of instruments with asset tag; caret flips |
| Dropdown item | click (inferred) | load that instrument: chart, pools, KPIs, tabs, last price |
| Tabs | hover | gold text; on the active tab a rounded focus box fades in |
| Tabs | click | switch list (pools / sweeps / sessions), page height changes |
| Page | wheel outside chart | scroll content between fixed title strip and footer |
| Title dot / cursor | idle | pulse / blink |

## 6. Data

| Field | Sample | Format | Update | Source / computation |
|---|---|---|---|---|
| Instrument list | EUR/USD, GBP/USD, USD/JPY, USD/CHF, USD/CAD, AUD/USD, NZD/USD, EUR/GBP, EUR/JPY | text + class tag `FX` | static | — |
| Last price | 1.12045 | 5 dp | stated DELAYED; static in video | latest quote |
| Candles | hourly OHLC, ~1 month visible, history ≥ 2 years (sweeps 597d ago) | | each hour | "real hourly OHLC" |
| Pools | BSL/SSL, price 5 dp, ×N members, touches | | on new candle | 3-bar fractal swings clustered within 0.06 % |
| Pool distance | +5.98% | signed % 2 dp | live with price | (pool − last) / last |
| Untested pools | 49 of 62 detected | integers | on new candle | pools not swept |
| Stop-run likelihood | High · Sell-side closer | word + side | live | rule on relative distance to the nearer untested pool |
| Sweeps | 13 · 9d ago | count + age of last | on new candle | pierce then close back on the other side |
| Sweep confirmation | Strong | word | — | reversal strength after the sweep |
| Session highs/lows | Sydney, Tokyo, London, New York today UTC; previous day (date); previous week | 5 dp | live during session | from hourly candles by UTC session windows |
| PDH / PDL | 1.13 / 1.12 tags | 2 dp on tags | daily | previous day high/low |
| Footer | Terminal Data · 16:29:27, DELAYED | HH:MM:SS | on refresh | |

## 7. Key frames

| Frame | Shows |
|---|---|
| `t000.0.jpg` | Scroll top, crosshair, KPI cards, pools list |
| `t004.5.jpg` | Crosshair near the BSL ×14 level and tags |
| `t007.5.jpg` | Clean chart, cursor on the instrument button |
| `t009.5.jpg` | Dropdown open |
| `t011.0.jpg` | Scrolled page, full pools list with LAST row, crosshair |
| `t017.0.jpg` | Sweep alerts cards |
| `t027.0.jpg` | Session extremes + footnote |
| `t037.5.jpg` | Zoomed chart, intraday ticks |
| `t042.0.jpg` | Max zoom seen, wide candles |

## 8. Open questions

- Colours/labels of the stop-run likelihood for `Medium` / `Low` and for "Buy-side closer".
- Meaning of the dot count vs `+N` (assumed: dots = first 5 touches, `+N` = extra touches), and of `×N` on tags
  (assumed: number of swings clustered).
- Values of `Reversal confirmation` other than `Strong`.
- Whether swept pools still show on the chart (all seen lines look untested) and how the list marks them.
- Whether the dropdown closes on outside click / Escape; how many instruments it holds (only FX seen).
- The chart height is fixed or resizable (grip at bottom-right corner).
- Footer clock does not tick in the video (DELAYED data).
