# Market › Seasonality

Reference video: `Reference/Market/seasonality.mp4` (48.5 s, 97 frames at 0.5 s, 1600x966).
Frames: `frames/Market/seasonality/tNNN.N.jpg`. Colours measured with `tools/pick` (JPEG noise ±4).
User description: *"Seasonal behaviour over up to 20 years: average path, win rate, weekday and month effects."*

## 1. Purpose

"Seasonality Lab": for one instrument (EUR/USD in the video) it averages the daily cumulative return path of each
calendar year over the last N years (10/15/20) and shows the average seasonal curve, where today falls on it, the
share of positive years, best/worst month, a volatility ratio, and in an ADVANCED section the average return by
weekday and by month, the return of the selected period in every year, and risk metrics. The user can restrict
everything to a period (Full / H1 / H2 / Q1–Q4 buttons, or by dragging on the chart); all cards then recompute for
that period.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | Default state: EUR/USD, 20 years, Full. Cursor on the trend chart at Sep 13 → crosshair + tooltip `Sep 13 / +0.21%`. |
| 0.5–4.0 | Cursor moves right along the curve; tooltip follows (values change). |
| 4.5 | Hover Dec 30 → tooltip `Dec 30 / +0.49%`, placed left of the line (near the right edge). |
| 5.0–8.0 | Cursor goes to the toolbar. |
| 8.5 | Click **Seasonality: EUR/USD** → asset dropdown opens: search field (gold focus border) + list (EUR/USD highlighted, GBP/USD, USD/JPY, USD/CHF, USD/CAD, AUD/USD, NZD/USD, EUR/GBP, EUR/JPY…) each with a grey `FX` tag. Button text turns cyan, chevron up. |
| 9.0–12.0 | Dropdown stays open while the cursor moves to `20 years`. |
| 12.5 | Click **20 years** → asset dropdown closes, years dropdown opens: `10 years`, `15 years`, `20 years` (selected, highlighted). |
| 13.0–16.5 | Idle with years dropdown open; cursor to Filter. |
| 17.0 | Click **Filter** → years dropdown closes, filter menu opens: `All years` (selected), `Exclude outlier years (>1.5σ)`, note `Affects "Average return by month" only – recomputed from real per-year returns.` Button text cyan, chevron up. Chart title is partly hidden by the menu. |
| 17.5–24.5 | Filter menu stays open; cursor visits Full (21.0: gold hover ring on `Full`), then H1. |
| 25.0–25.5 | Click **H1** → H1 gets the gold ring; the chart shows a teal shaded band Jan 1 → Jul 2 between dotted cyan lines, badge `Jan 1 ⟶ Jul 2: +0.15%` + `× Clear`; win-rate pie, rows, volatility ratio, weekday and month bars all recompute (month chart shows only JAN–JUN), purple `Jan-Jun` pills appear on cards. The "DRAG TO SELECT A PERIOD" hint disappears. |
| 26.0–29.0 | Hover over the segment buttons (no change). |
| 29.5 | Click **H2** → band Jun 30 → Dec 31, badge `+0.70%`; cards recompute (`Jul-Dec`). |
| 30.0–33.5 | Hover. |
| 34.0 | Click **Q1** → band Jan 1 → Apr 2, badge **red** `Jan 1 ⟶ Apr 2: -0.39%`; cards `Jan-Mar`. |
| 34.5–37.5 | Hover. |
| 38.0 | Click **Q2** → band Mar 31 → Jul 2, badge `+0.55%`; cards `Apr-Jun`. |
| 40.0 | Hover on the trend chart (Jun 28): grey vertical crosshair, dot on the curve, tooltip `Jun 28 / -0.33%`. |
| 40.5 | Hover Sep 13 `+0.21%`. |
| 41.0–44.5 | Smooth wheel scroll down to the bottom (offset ≈339 px): INDIVIDUAL PERIOD RETURNS and RISK METRICS become visible. Toolbar stays fixed; filter menu stays open on top. |
| 45.0–48.0 | Smooth scroll back up to the top. |

## 3. Layout (1600x966, page bg `#040302`)

```
y  0 ┌ Seasonality Lab  DAILY HISTORY · MULTI-YEAR SEASONAL STATISTICS ─────────────────────────────┐ h31 fixed
  37 │ [Seasonality: EUR/USD ▾] (?) │ ‹ › [20 years ▾] [Filter ▾] [Full|H1|H2|Q1|Q2|Q3|Q4] [Compare ▾]   │ h34 fixed
     │ [2026 actual] [Index (100)]                                                              ⟳    │
  70 ├─────────────────────────────────────────────────────────────────────────────────────────────── ┤
 104 │ • SUMMARY ─────────────────────────────────────────────────────────────────────────────────── │
 121 │┌ SEASONAL TREND (x10–1148) ─────────────────────────────────┐┌ SEASONAL WIN RATE (1159–1589) ┐│
     ││ title, subtitle, chart (plot x60–1128, y198–582), footnote  ││ 3D pie, caption, 3 rows,      ││
 614 │└─────────────────────────────────────────────────────────────┘│ VOLATILITY RATIO sub-card     ││
 638 │ • ADVANCED ──────────────────────────────────────────────────────────────────────────────────  │
 656 │┌ AVERAGE RETURN BY WEEKDAY (%) (x10–795) ┐┌ AVERAGE RETURN BY MONTH (%) (x806–1589) ┐         │ h~315
 981 │┌ INDIVIDUAL PERIOD RETURNS (%) ──────────┐┌ RISK METRICS ────────────────────────────┐         │ h~315
1305 └────────────────────────────────────────────────────────────────────────────────────────────────┘
```

- Header bar y 0–31 (bg `#0f0a03`, 1 px bottom line `#2d2a1a` at y 31) and toolbar y 31–70 (1 px bottom line
  `#2b2814` at y 70) are **sticky**; the content area below scrolls (total content ≈1305 px; max scroll 339 px at
  966 px window height).
- Content margin 10 px left/right; card gap 11 px horizontally, ~10 px vertically; section labels 18 px above cards.
- Trend card stretches horizontally; win-rate card fixed ≈430 px wide. ADVANCED cards split 50/50.

## 4. Components

### 4.1 Header bar
- `Seasonality Lab` sans semibold 13 px `#e8e6e0` at x 10, y 15.
- `DAILY HISTORY · MULTI-YEAR SEASONAL STATISTICS` monospace/sans caps 9 px, letter-spacing 0.5 px, `#6a665e`, x 106.

### 4.2 Toolbar (y 37–63, buttons 26 px high, 1 px border `#251f12`, bg `#0f0a03`, text 12 px proportional `#cbcab4`, radius 2)
| Control | x | Content |
|---|---|---|
| Asset button | 14–151 | `Seasonality: EUR/USD` + `⌄` |
| Help | 160–176 | `ⓘ`-style `?` in a circle, grey `#8a877e` |
| divider | 186 | 1 px vertical `#251f12` |
| Prev / next | 195 / 217 | `‹` `›` (step the year window or asset — not used) |
| Years | 237–308 | `20 years ⌄` |
| Filter | 315–368 | `Filter ⌄` |
| Period segmented | 376–545 | `Full H1 H2 Q1 Q2 Q3 Q4` (group border `#251f12`) |
| Compare | 552–624 | `Compare ⌄` |
| 2026 actual | 631–705 | toggle button (overlay the current year path; not used) |
| Index (100) | 711–780 | toggle button (rebase to 100; not used) |
| Refresh | 1572–1588 | `⟳` icon grey |

States:
- Open dropdown button: text cyan `#3ec6d8`, chevron `⌃`, gold 1 px border on the years button.
- Period segment default (`Full` at t0): dark teal bg `#00324a`, cyan text `#3cc8d8`, no ring.
- Period segment chosen by click (H1, H2, Q1, Q2): cyan text, **gold rounded ring** (1 px `#c2b78a`/`#d4b22a`,
  radius 4) around the label. Hover on another segment: same gold ring appears on the hovered one (t21.0).

### 4.3 Dropdowns (bg `#110b03`, 1 px border `#28281a`, no shadow, radius 2)
- **Asset** (x 14–216, y 70–328): search input `Search assets...` (magnifier, gold focus border `#d4b020`), list rows
  25.5 px: symbol monospace 12 px (selected bold white on `#352b05` row bg), right tag `FX` monospace 9 px
  `#6a665e`. Scrollable list.
- **Years** (x 238–440, y 70–155): `10 years`, `15 years`, `20 years` monospace 13 px; selected row bg `#352b05`.
- **Filter** (x 316–518, y 70–167): rows `All years` (selected, `#2a2010` bg), `Exclude outlier years (>1.5σ)`
  (13 px `#cbcab4`), then a note in 10 px `#6a665e`: `Affects "Average return by month" only – recomputed from
  real per-year returns.`
- Only one dropdown is open at a time; clicking another toolbar button closes the open one and opens its own. The
  menu stays open while the page scrolls and while other controls (segments) are clicked.

### 4.4 Section labels
`• SUMMARY` / `• ADVANCED`: gold dot + text monospace bold 11 px, letter-spacing 2 px, `#f2d632`, followed by a 1 px
rule `#1e1d17` to x 1580.

### 4.5 Card (common)
bg `#080603`, 1 px border `#232217`, no radius; title monospace bold 15 px uppercase, letter-spacing ≈3 px,
`#e8e6e0`, at (21,142); optional period pill at the top-right.
- **Period pill** (on all four ADVANCED cards and next to the win-rate title when a period is selected): bg
  `#1f1741` (deep violet), text `#9490a9` monospace 10 px letter-spacing 1.5 px, e.g. `Jan-Jun`, `Jul-Dec`, `Jan-Mar`,
  `Apr-Jun`; radius 2. Hidden in Full.

### 4.6 SEASONAL TREND card (x 10–1148, y 121–614)
- Title `SEASONAL TREND OF EUR/USD OVER 20 YEARS`; subtitle `(Oct 23 2006 - Oct 23 2026)` 10 px `#8a877e` at y 162.
- Top-right hint (Full only, no selection): `DRAG TO SELECT A PERIOD` monospace 8 px caps `#62605a` at (1033,186).
- **Plot** x 60–1128, y 198–582. Y labels left (x 16–54) monospace 10 px `#8a877e`: `+1.00% +0.50% +0.00% -0.50%
  -1.00% -1.50%` at y 213, 280, 346, 412, 478, 545 (66.5 px per 0.5 %). Range auto from data (min −1.49, max +0.84).
- Month columns: alternating vertical bands, odd months `#0f0d06`, even `#080603`; month labels `JAN … DEC`
  centred at y 583 (x 105, 191, 278, 367, 457, 546, 635, 726, 815, 904, 994, 1083), 9 px `#8a877e`.
- Faint horizontal grid at label ticks (`#14120a`). Zero line dashed (3/3) olive `#656345`.
- Curve: cyan `#3bc8de` core / `#6ec9d4` highlight, 1.5 px, smooth; fill below the line down to the plot bottom,
  gradient teal `#265b63` (≈35 % alpha) at the line → `#101206` at the bottom.
- Extremes: max label `+0.84%` cyan monospace 10 px above the peak with a 3 px dot; min label `-1.49%` grey-white
  below the trough with a dot.
- **TODAY** marker: vertical dashed line (2/3) `#57605d` at today's date (x 923 = Oct 23), label `TODAY` cyan
  `#406b6c` monospace 8 px at y 190.
- Left-edge dot: a small grey dot on the y axis (x 60) at the hovered value's y (t0 at y 319) — a y-axis marker of
  the crosshair.
- Footnote (y 596, 9 px `#6a665e`): `Shown as a 3-day Gaussian-weighted moving average of the real cumulative return,
  for readability.`
- **Crosshair** (hover): solid 1 px vertical line `#8b8b83` full plot height at the cursor date, 4 px dot on the
  curve (cyan with white core), tooltip box bg `#0f0a03`, 1 px border `#2a2518`, padding 8: date `Sep 13` 10 px
  `#a8a49c`, value `+0.21%` monospace bold 11 px cyan (red if negative? seen `-0.33%` in cyan-grey). Tooltip placed
  right of the line, flipped to the left near the right edge.
- **Selection band** (period chosen): fill teal `#112524`–`#1b2d2c` over the period, boundaries dotted cyan
  `#425e59` 1 px; badge at top-left of the plot (x 228–360, y 202–220): `Jan 1 ⟶ Jul 2: +0.15%` monospace 11 px on
  `#000f11` with 1 px border; text cyan when ≥ 0, red `#e0605a` (bg `#2a0a05`) when negative; next to it `× Clear`
  (grey `#afada2`, 1 px border). Period ends are one day past the month end (`Jul 2`, `Apr 2`, `Dec 31`, H2/Q2 start
  `Jun 30`/`Mar 31`) — the band includes the boundary day.

### 4.7 SEASONAL WIN RATE card (x 1159–1589, y 121–614)
- Title centred `SEASONAL WIN RATE` + period pill when selected.
- 3D pie (tilted ellipse ≈280x170, extrusion 22 px, centre ≈1374,285): red slice NEGATIVE (top `#f96c6b`, side
  `#672121`/`#71292d`), cyan slice POSITIVE (top `#1cd5ec`, side `#05474e`), thin dark separator, soft drop shadow.
  Slice labels: percentage monospace bold 28 px white (`55.0%`), word `NEGATIVE` / `POSITIVE` bold 12 px spaced,
  `11 years` / `9 years` 11 px.
- Caption centred 10 px `#96938d` (y 421): Full → `Years with a positive annual return – 20 years`; period →
  `Years where the selected period was positive – 19 years` (the current, incomplete year is excluded when the
  period isn't over).
- Rows (y 456, 477, 497; 1 px rule `#242418` above the first): label 11 px `#696862` left; value monospace 11 px
  right — `Annual return` (Full) / `Period return` (period) `+0.51%` cyan; `Best month` `December +0.86%` cyan;
  `Worst month` `May -0.81%` red `#e9605b`.
- **VOLATILITY RATIO** sub-card (x 1171–1578, y 516–600): bg `#0f0805`, 1 px border `#2c2617`, 2 px left accent bar
  coloured by state; title monospace bold 10 px spaced `#e8e6e0`; pill at right; big value monospace bold 26 px
  `1.00×`; right text monospace 8 px caps `#737069` — **in French**: `VOLATILITÉ NORMALE`, `11% MOINS VOLATILE QUE LA
  NORMALE`, `10% PLUS VOLATILE QUE LA NORMALE`; bottom line monospace 11 px `σ Full year 0.70% ÷ σ global 0.70%`
  (values bold white).
  - State colours: ratio ≈1 (0.95–1.05?) yellow (accent `#f8da3c`, value `#f8da3c`, pill bg `#3a2d00` text `#c0a030`);
    < 1 cyan (`#26ddee`, pill bg `#013130` text `#4e7174`); > 1 red (`#e97065`, pill bg `#1a0805` text `#9c4a40`).
  - Pill text: `Full year`, `Jan-Jun`, `Jul-Dec`, `Jan-Mar`, `Apr-Jun`.

### 4.8 AVERAGE RETURN BY WEEKDAY (%) (x 10–795, y 656–~971)
Bar chart, 5 equal-width categories `MON TUE WED THU FRI` (labels 10 px `#bdbcb8` at the bottom), bars nearly full
category width (gap ~10 px), cyan `#1acde1` positive / red `#e4665c` negative, flat fill, no labels. Zero line 1 px
olive `#3e3a20`; two dashed grid lines (`#27261b`) at ± one step. No y labels. Y range auto-symmetric.

### 4.9 AVERAGE RETURN BY MONTH (%) (x 806–1589)
Same style; categories = months of the selected period (12 in Full, 6 for halves, 3 for quarters), bar width
adapts. Affected by the Filter (outlier exclusion).

### 4.10 INDIVIDUAL PERIOD RETURNS (%) (below, x 10–795, y ≈981 unscrolled)
One bar per year `07 … 26` (20 bars, labels 2-digit year 10 px), cyan/red, narrow bars (~28 px, gap ~10).

### 4.11 RISK METRICS (x 806–1589)
2-column grid of tiles (each ≈377x50, gap 8): bg `#040302`, 1 px border `#252118`; label monospace 8 px caps
`#6f6c66`; value monospace 15 px (cyan `#5dc0d0`, red `#e05a50`, neutral `#b8b6b0`).
| Tile | Q2 value |
|---|---|
| CUMULATIVE PROFIT | +0.47% (cyan) |
| AVG PERIOD RETURN | +0.21% (cyan) |
| VOLATILITY | 4.53% (neutral) |
| MAX DRAWDOWN | -1.59% (red) |
| RETURN / RISK | 0.05 (cyan) |
| BEST PERIOD (2025) | +9.25% (cyan) |
| WORST PERIOD (2010) | -8.89% (red) |
Footnote 9 px `#6a665e`: `Volatility = std. dev. of real Apr-Jun period returns. Max drawdown = largest
peak-to-trough drop on the real average seasonal path, restricted to this period. Return/Risk is a plain ratio, not
a risk-free-rate-adjusted Sharpe ratio.`

## 5. Interactions

| Element | Action | Result |
|---|---|---|
| Trend chart | hover | crosshair line + curve dot + y-axis dot + tooltip (date, cumulative return) |
| Trend chart | drag | selects a custom period (hint "DRAG TO SELECT A PERIOD"; not performed in the video) |
| Selection badge `× Clear` | click | back to Full (not shown) |
| Period segment | click | selects Full / H1 / H2 / Q1–Q4; band + badge on chart; every card recomputes; pills appear |
| Period segment | hover | gold ring on hovered segment |
| Asset button | click | asset dropdown with search + FX list (`FX` tags), scrollable |
| Years button | click | 10 / 15 / 20 years; changes title `OVER 20 YEARS`, subtitle range, all stats |
| Filter | click | All years / Exclude outlier years (>1.5σ) — affects month chart only |
| Compare, 2026 actual, Index (100), ‹ ›, ⟳ | — | present but not used in the video |
| Page | wheel | smooth scroll (≈0.5 s ease), header + toolbar sticky |

No animation on period change: the new state is drawn in one frame.

## 6. Data

| Field | Sample | Format | Update | Note |
|---|---|---|---|---|
| Seasonal path | +0.84 % max (Sep), −1.49 % min (Feb end) | % 2 dp signed | daily | mean of per-year cumulative returns from Jan 1, 3-day Gaussian smoothing |
| Window | Oct 23 2006 – Oct 23 2026 | `Mon D YYYY` | daily | N years back from today |
| Win rate Full | 55.0 % negative (11 y) / 45.0 % positive (9 y) | % 1 dp | daily | years with positive annual return |
| H1 | 57.9 % neg 11 y / 42.1 % pos 8 y, 19 years; period +0.06 %; best April +0.69 %; worst May −0.81 %; vol 0.89× "11% MOINS VOLATILE", σ Jan-Jun 0.63 % | | | |
| H2 | 45.0 % neg 9 y / 55.0 % pos 11 y, 20 years; +0.45 %; best December +0.86 %; worst October −0.11 %; 1.10× "10% PLUS VOLATILE", σ Jul-Dec 0.77 % | | | |
| Q1 | 47.4 % neg 9 / 52.6 % pos 10, 19 years; −0.41 %; best March +0.64 %; worst January −0.52 %; 0.98× "VOLATILITÉ NORMALE", σ Jan-Mar 0.69 % | | | |
| Q2 | 36.8 % neg 7 / 63.2 % pos 12, 19 years; +0.47 %; best April +0.69 %; worst May −0.81 %; 0.79× "21% MOINS VOLATILE", σ Apr-Jun 0.55 % | | | |
| Full | annual +0.51 %; best December +0.86 %; worst May −0.81 %; 1.00×; σ Full year 0.70 % ÷ σ global 0.70 % | | | σ = std of daily returns? (0.70 %) |
| Selection badge | H1 +0.15 %, H2 +0.70 %, Q1 −0.39 %, Q2 +0.55 % | % | | change of the seasonal path across the band |
| Weekday bars | Full: MON large −, TUE ~0, WED large +, THU small −, FRI small + | % | daily | mean daily return by weekday within period |
| Month bars | Full: JAN −, FEB −, MAR +, APR +, MAY large −, JUN +, JUL +, AUG/SEP/OCT/NOV small −, DEC large + | % | daily | mean monthly return |
| Period returns | 20 yearly bars (2007–2026) | % | daily | |
| Risk metrics | see 4.11 | % / ratio | daily | |
| Asset list | EUR/USD, GBP/USD, USD/JPY, USD/CHF, USD/CAD, AUD/USD, NZD/USD, EUR/GBP, EUR/JPY, … | | | category tag `FX` |

Source: daily price history (no source name visible on screen).

## 7. Key frames

| Frame | State |
|---|---|
| `t000.0.jpg` | default Full, hover tooltip Sep 13 |
| `t004.5.jpg` | tooltip flipped left near right edge (Dec 30) |
| `t008.5.jpg` | asset dropdown open |
| `t012.5.jpg` | years dropdown open |
| `t018.0.jpg` | filter menu open |
| `t025.5.jpg` | H1 selected (band, badge, pills, recomputed cards) |
| `t034.0.jpg` | Q1 selected (negative red badge, yellow vol card) |
| `t040.0.jpg` | Q2 selected + chart hover |
| `t044.5.jpg` | scrolled to bottom: period returns + risk metrics |

## 8. Open questions

- Compare, 2026 actual, Index (100), ‹ ›, ⟳ and the help `?` were never used — behaviour unknown (Compare probably
  overlays a second asset; 2026 actual probably overlays the current year path on the trend chart).
- Drag-to-select interaction details (snapping to days, minimum width) and `× Clear` behaviour.
- Thresholds for the volatility-ratio colour (yellow at 0.98 and 1.00, cyan at 0.89, red at 1.10).
- The volatility ratio text is in French while the rest is English — keep it translatable (the app has a French UI).
- Is σ global the std of daily returns over the whole window? (0.70 %, plausible for EUR/USD daily).
- Asset list content beyond the first 9 FX pairs (other categories likely: indices, commodities…).
- Q3 / Q4 never clicked (assumed same as others).
