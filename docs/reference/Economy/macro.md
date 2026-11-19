# Economy › Macro — reference spec

Source video: `Reference/Economy/macro.mp4` (17.5 s, 36 frames at 0.5 s, 1600x966).
Coordinates are pixels in the 1600x966 page area (app header not shown). Colours measured with the pixel picker
(JPEG; small anti-aliased text reads paler than the design colour, so a recommended token is given where useful).

## 1. Purpose

The macro globe. A rotating 3D Earth with a registry of economies (G20 by default, then by continent). Clicking an
economy opens its "Asset File": policy rate, sovereign yield curve, a macro dashboard (main indicators with trend,
latest, prior, release date), an "Advanced" grid of long-run World Bank / BIS / OECD figures, and, per indicator,
a full-history chart (back to 2001 with MAX) with its real release calendar. Values are real recorded releases,
"never estimated".

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–1.0 | Globe view. The globe turns west→east (surface moves right; about 25–30° between t0 and t1.5, so ≈ 18°/s, or a fly-to toward the hovered country — see open questions). Cursor over the Atlantic. |
| 1.5 | Cursor on registry row `01 UNITED STATES`: row hover state (brighter background, content nudged right). |
| 2.0 | Click → the globe and registry are replaced instantly by the **Asset File** of the United States (top bar stays). |
| 2.5–3.0 | Idle on the asset file. Block caret blinks after `ASSET FILE` / top-bar title. |
| 3.5 | Hover on yield-curve bar `3 Mo`: bar brightens, column band, tooltip `3 Mo / ● 4.220%`. |
| 4.0 | Hover on table row `Inflation Rate (Annual)`: row background lighter. |
| 4.5 | Click → **indicator detail view** (instant). |
| 5.0 | Detail view idle. |
| 5.5–13.0 | Cursor sweeps across the bar chart; each hovered bar turns pale yellow, a column band appears, the tooltip follows the cursor: 13 Jul 2022 9.1% → 10 Aug 2022 8.5% → 10 Nov 2022 7.7% → 13 Jun 2023 4% → 14 Nov 2023 3.2% → 12 Dec 2023 3.1% → 12 Mar 2024 3.2% → 13 Nov 2024 2.6% → 12 Mar 2025 2.8% → 10 Apr 2025 2.4% (held 11.5–12.5) → 12 Jun 2024 3.3% (13.0). |
| 13.5 | Wheel scroll ≈ 53 px (whole asset file scrolls, top bar fixed); hover now `15 May 2024 3.4%`. Release-calendar rows appear. |
| 14.0 | Scroll ≈ 140 px more. |
| 14.5 | Scroll to the end of the page (≈ 205 px more). Cursor over the x-axis labels: no tooltip, no band. |
| 15.0–17.5 | Idle. Caret and status dot keep blinking. |

## 3. Layout (1600x966)

### 3.1 Globe view
```
+------------------------------------------------------------------------------------------+ y=0
| ⊛ • GLOBAL ASSET NETWORK — LIVE SURVEILLANCE █        [• 82 ECONOMIES · 60 MONTHLY · 27 LIVE FEEDS] |
+------------------------------------------------------------------------------------------+ y=36 (1px #444124)
| • GLOBAL ASSET NETWORK                                          |• ASSET REGISTRY      23 |
|                                                                 |[★G20] AMERICA EUROPE … |
|                     ( rotating globe, centre ≈ (635,502),       |01 ▬ UNITED STATES [•FED]|
|                       radius ≈ 317, amber halo )                |02 ▬ EURO ZONE    [•ECB]|
|                                                                 | … 23 rows, 31 px each   |
|                                                                 |                         | y≈830
| [⟲]                                        [•LIVE FEED][•MONTHLY][•ANNUAL]               | y 935-957
+------------------------------------------------------------------------------------------+ y=966
```
- Top bar y 0–36, full width; background gradient `#150f03` (top) → `#0d0800`; 1 px bottom line `#444124`.
- Globe area x 0–1260, y 37–966. Background near-black warm (`#030200`–`#110b03`, slightly brighter at lower left) with an almost invisible vertical band texture.
- Registry panel x 1262–1600, y 38–≈ 830 (height follows the 23 rows).
- Bottom controls y 935–957: square icon button at x 8–30; segmented control at x 1022–1252.
- The globe stays centred in its area; the registry keeps a fixed width (≈ 338 px).

### 3.2 Asset File (dashboard)
- Centred content column x 141–1459 (1318 px wide; ≈ 141 px side margins at 1600). Page background `#080500`. Close `×` at (1573, 63).
- Vertical stack: `ASSET FILE` (y 65) → country title (y 88–132) → amber hairline (y 157) → `• SUMMARY` (y 184) →
  summary box (y 202–433) → `MACRO DASHBOARD` + tabs (y 450–468) → indicator table (y 483–689) → footnote (y 695–720) →
  `• ADVANCED` (y 746) → sources line + range buttons (y 772) → cards 5 per row (row 1 y 800–880, row 2 y 888–968, page continues).
- Whole column scrolls vertically under the fixed top bar.

### 3.3 Indicator detail
- Same column. `← BACK TO DASHBOARD` (x 141–306, y 168–196) → indicator header box (y 204–240) → chart box (y 250–852) →
  caption box (y 862–898) → release calendar box (y 906–943+ rows). Scroll reveals the calendar (page bottom at scroll ≈ 400 px with 7 rows).

## 4. Components

### 4.1 Top bar (all states)
- Left: round globe icon (amber `#cdc284`) at x 12–24, a status dot at x 38 (pulses between amber `#f5c518` and dim `#736f13`),
  title `GLOBAL ASSET NETWORK — LIVE SURVEILLANCE` mono ≈ 11 px, uppercase, letter-spacing ≈ 3.5 px, `#5b574e`, x 50–385, y ≈ 17.
- Blinking amber block caret `█` (≈ 6x11 px, `#f5c518`) right after the title; on/off roughly every 0.5–1 s.
- Right pill x 1305–1585, y 9–27: fill `#4f3f00`, 1 px border `#9b8720`, text `• 82 ECONOMIES · 60 MONTHLY · 27 LIVE FEEDS` mono bold ≈ 10 px, letter-spaced, `#e7d592`; its leading dot blinks with the status dot.

### 4.2 Globe
- Textured 3D Earth (satellite/relief colouring: olive-tan land, deep blue ocean with lighter shelves, white ice), thin dark country borders, darker night-side shading toward the rim.
- Amber glow halo outside the rim: `#402e00` at the edge fading to background over ≈ 40–60 px.
- Sub-label `• GLOBAL ASSET NETWORK` at x 8–185, y ≈ 50, mono ≈ 11 px letter-spaced `#565550`.
- No markers/pins visible in the video.

### 4.3 Asset registry (x 1262–1600)
- Header y 38–68, background `#1b1302`: dot + `ASSET REGISTRY` mono ≈ 11 px letter-spacing ≈ 3 px `#605a48`; count `23` right (`#5c5744`). 1 px line below.
- Region tabs y 79–93: `★ G20` selected = filled `#ffd900` with near-black bold text; `AMERICA` `EUROPE` `AFRICA` `ASIA` `OCEANIA` mono bold ≈ 11 px `#848178`, no fill.
- Rows (31 px each, starting y ≈ 112): background `#1e1801`, 3 px amber left bar `#f8e479` per row (x 1261–1263),
  index `01` mono ≈ 11 px `#535037` (x 1272), flag ≈ 21x14 (x 1295), name uppercase bold condensed ≈ 14 px `#efebdf` (x 1324),
  right badge `• FED` (x 1550–1592): fill `#544500`, 1 px border `#786504`, text `#eadf92` mono bold ≈ 10 px.
- G20 list: 01 UNITED STATES FED · 02 EURO ZONE ECB · 03 CHINA BIS · 04 JAPAN BOJ · 05 GERMANY ECB · 06 UNITED KINGDOM BOE · 07 FRANCE ECB ·
  08 INDIA BIS · 09 ITALY ECB · 10 BRAZIL BIS · 11 CANADA BOC · 12 SOUTH KOREA BIS · 13 RUSSIA BIS · 14 SPAIN ECB · 15 AUSTRALIA RBA ·
  16 MEXICO BIS · 17 INDONESIA BIS · 18 TURKEY BIS · 19 SWITZERLAND SNB · 20 SAUDI ARABIA BIS · 21 ARGENTINA BIS · 22 SOUTH AFRICA BIS · 23 SINGAPORE BIS.
  The badge is the policy-rate source: the central bank when the app has a live feed, else `BIS`.
- Hover: row background lighter amber tint (≈ `#2a2208`), left bar brighter, row content shifted ≈ 5 px right.

### 4.4 Bottom controls (globe view)
- Square icon button x 8–30, y 935–957: 1 px amber border, fill `#3f3200`, icon `#cdc284` (recentre / reset view).
- Segmented pill group x 1022–1252 inside a 1 px dim frame: `• LIVE FEED` (selected: fill `#403300`, border amber, text `#e9dca1`),
  `• MONTHLY` (border amber-dim, text `#b1a776`), `• ANNUAL` (dim, text `#58544b`). Mono bold ≈ 10 px, letter-spaced. Probably filters which economies/data cadence the globe shows.

### 4.5 Asset File header
- `ASSET FILE` mono ≈ 10 px, letter-spacing ≈ 4 px, `#58554d`, x 140, y 65 (with the blinking caret after it at times).
- Flag 44x28 at x 141 + `UNITED STATES OF AMERICA`: bold condensed sans (Rajdhani/Barlow Condensed-like) ≈ 46 px cap height ≈ 34, letter-spacing ≈ 2 px, `#f5f4ef`, x 196–825, y 88–132.
- Hairline at y 157 from x 140 to 1460: amber `#4e4b0c`-ish, fading to transparent toward the right.
- Section heading `• SUMMARY` / `• ADVANCED`: amber bold mono ≈ 11 px letter-spaced (`#cbab28`, highlights `#e2cd7f`) followed by a 1 px hairline to x 1460.

### 4.6 Summary box (x 141–1459, y 202–433)
- 1 px border `#282212`, small corner bracket at top-left. Background same as page.
- Left block (x 158–560): `FEDERAL RESERVE · POLICY RATE` mono ≈ 9 px letter-spaced `#5f5b54`; value `4.00` mono bold ≈ 56 px pale amber `#fde575`
  followed by `%` ≈ 20 px `#9b968e`; `PUBLIC FEED · 19:00:00` mono ≈ 9 px `#5e5b54` (time of the feed / last decision).
- Right block: `SOVEREIGN YIELD CURVE` mono ≈ 9 px letter-spaced `#5a574f` at x 597, `as of 2026-10-23` right-aligned at x 1441.
  Bar chart, plot x ≈ 650–1440, y ≈ 250–395: 14 bars (1 Mo, 1.5 Mo, 2 Mo, 3 Mo, 4 Mo, 6 Mo, 1 Yr, 2 Yr, 3 Yr, 5 Yr, 7 Yr, 10 Yr, 20 Yr, 30 Yr);
  x labels every other bar: `1 Mo 2 Mo 4 Mo 1 Yr 3 Yr 7 Yr 20 Yr` (proportional ≈ 10 px `#7b7870`);
  y labels at left `5.60% 5.40% 5.10% 4.80% 4.50% 4.20% 3.90%` (mono ≈ 10 px `#807d76`, x 612–643); faint horizontal grid `#181408`–`#211d12`.
  Bars: vertical gradient top `#fdd50d` → bottom `#88741a`, rounded 1 px top, ≈ 70% of slot width. Y axis does not start at zero.
  Heights read ≈ 4.15, 4.15, 4.18, 4.22, 4.25, 4.33, 4.33, 4.45, 4.70, 4.75, 4.80, 4.86, 4.96, 5.30 %.
- Bar hover: bar turns flat pale yellow `#ffe455`, a dim amber column band behind it (full plot height), tooltip box (x 923–1015, y 278–337):
  background `#0a0804`, 1 px border amber-dim (`#91804e` at ~50 %), radius 3 px; line 1 `3 Mo` white mono ≈ 11 px; line 2 amber dot + `4.220%` bold white (3 decimals).

### 4.7 Macro dashboard
- `MACRO DASHBOARD` mono ≈ 10 px letter-spacing ≈ 3 px `#5b5851` (x 140, y 458), then tabs (y 450–468):
  `MAIN INDICATOR` selected = filled `#ffda00`, near-black bold mono ≈ 11 px; `PRICES`, `GROWTH`, `LABOUR` = 1 px dim border, white bold text `#edece9`.
- Table x 141–1459, y 483–689, 1 px border `#282212`. Header row (h 26) background `#170d02`: `MAIN INDICATOR` amber bold (`#c8a921`) left;
  `TREND` (x 826) `LATEST` (right x 1063) `PRIOR` (right x 1221) `RELEASED` (right x 1451) mono ≈ 9 px `#87837a`.
- Rows h ≈ 37, 1 px separators. Name proportional ≈ 14 px `#e0dcd9`; TREND = sparkline ≈ 70x24 (x 787–858) cyan `#2cb1bf` when trending up, red `#e36062` when down;
  LATEST = small trend arrow (`↗` cyan / `↘` red) + value bold mono pale amber (`#f4edb9`); PRIOR mono `#dddad6`; RELEASED mono `#7a7770` (`YYYY-MM-DD`).
- Rows: Interest Rate ↗ 4.00% | 3.75% | 2026-09-16 · GDP (Quarter q/q) ↘ 2.2% | 2.1% | 2026-09-30 · Inflation Rate (Annual) ↗ 3.7% | 3.4% | 2026-10-14 ·
  Unemployment Rate ↗ 4.2% | 4.1% | 2026-10-02 · PMI Composite ↘ 57.6 | 58.4 | 2026-10-23.
  (Arrow direction/colour follows the sparkline's recent slope, not latest vs prior: GDP 2.2 vs 2.1 shows ↘ red.)
- Row hover: background lighter (`#1a1710`).
- Footnote (mono ≈ 9 px uppercase, `#77746d`, 2 lines): `LATEST REAL RECORDED RELEASE PER INDICATOR FROM THIS TERMINAL'S OWN ECONOMIC-CALENDAR HISTORY; GAPS FILLED FROM OFFICIAL AGENCIES (BIS · EUROSTAT · OECD · WORLD BANK — ANNUAL WHERE TAGGED). NEVER ESTIMATED. CLICK A ROW FOR ITS FULL HISTORY.`

### 4.8 Advanced
- `• ADVANCED` heading + hairline (y 746). Line `OFFICIAL SOURCES ONLY — WORLD BANK · BIS · OECD/FRED. USE THE RANGE BUTTONS TO CHANGE THE WINDOW.` mono ≈ 9 px `#534f47` (y 777).
- Range buttons right (x ≈ 1320–1455): `5Y` `10Y` (selected, filled `#ffda00`, dark text) `20Y` `MAX`, mono bold ≈ 10 px `#7d7a73`.
- Cards: 5 per row, ≈ 258x80, gap ≈ 7 px (x 141, 406, 671, 936, 1201), 1 px border `#282212`.
  Each card: label mono ≈ 9 px uppercase `#75726b` (top-left) + period right `2025` / `Sep 2026` (`#464239`);
  value bold ≈ 22 px `#faf9f6`; amber sparkline (≈ 90x26, right half, `#f2e6c2`→`#97892f`, end dot) for the chosen range;
  bottom line `▲ +5.0%` + ` vs prior · World Bank` (`#4d4942`).
  Triangle/number colour is semantic: good = cyan `#4edeea`, bad = red `#f27b7e`, neutral = grey.
- Cards seen: GDP 2025 $30.77T ▲ +5.0% (cyan) · World Bank | GDP PER CAPITA 2025 $90,027 ▲ +4.5% · World Bank | POPULATION 2025 341.8M ▲ +0.5% (grey) · World Bank |
  REAL GDP GROWTH 2025 2.2% ▼ -0.63pp (red) · World Bank | INFLATION Sep 2026 3.7% ▲ +0.30pp (red) · BIS | UNEMPLOYMENT 2025 4.2% ▲ +0.18pp (red) · World Bank |
  CURRENT ACCOUNT 2025 -3.8% ▲ +0.27pp (cyan) · World Bank | GOV. DEBT / GDP 2024 116% ▲ +1.05pp (red) · World Bank | FX RESERVES 2025 $1.39T ▲ +52.2% (cyan) · World Bank |
  10Y GOV. YIELD Sep 2026 4.99% ▲ +0.31pp (grey) · OECD via FRED.
  Units: level series use % change, rate series use `pp` (2 decimals). Money with `$` and `T`/`M` suffix.

### 4.9 Indicator detail
- `← BACK TO DASHBOARD` button: 1 px border `#3f3824`, fill `#1b1501`, mono ≈ 11 px letter-spaced `#898475`.
- Header box (x 141–1459, y 204–240, 1 px border `#45412f`): `Inflation Rate (Annual)` proportional semibold ≈ 15 px `#efece8`;
  `↗` cyan `#6ab8bb` + `3.7%` bold mono pale amber `#fef4d9`; `Prior 3.4%` mono `#7f7c75`; right `Released 2026-10-14` mono ≈ 10 px `#79766f`.
- Chart box x 141–1459, y 250–852 (1 px border `#474331`):
  - Toolbar y 260–286. Group 1 (one bordered container): `3Y` `5Y` `10Y` `MAX` (5Y selected: filled `#ffd900`, dark text).
    Separate bordered buttons `STANDARD DEVIATIONS`, `FORECAST`. Group `BAR` `AREA` (BAR selected, filled). Mono bold ≈ 11 px, inactive `#8a8781`.
    Right: search box x 1156–1448, y 260–284, background `#030200`, 1 px border, magnifier, placeholder `Compare with…` (`#55524d`), key chip `Ctrl K` (`#494844`, 1 px border).
  - Hint `DRAG ACROSS THE CHART TO ZOOM INTO A PERIOD · DOUBLE-CLICK TO RESET` mono ≈ 10 px letter-spaced `#56524b` (y 307).
  - Plot frame x 151–1448, y 325–845 (1 px `#1b170d`). Plot area x ≈ 165–1385, y 343–815.
    Y axis on the right (x 1392): `10%` … `1.5%` every 0.5 %, proportional ≈ 11 px `#959391`; horizontal grid lines very faint.
    X axis: `Nov 21 Feb 22 May 22 Aug 22 Nov 22 Feb 23 May 23 Aug 23 Nov 23 Feb 24 May 24 Aug 24 Nov 24 Feb 25 May 25 Aug 25 Jan 26 Apr 26 Jul 26 Oct 26` (proportional ≈ 11 px `#92908d`).
    One bar per release (≈ 60 bars in 5Y), bar ≈ 16 px wide on a ≈ 21 px pitch; gradient top `#fad615` → bottom `#88780d`. Y axis min is 1.5 %, not 0.
  - Bar hover: bar flat `#ffe455`; column band `#332609` (amber, low alpha) over the bar slot for the full plot height;
    tooltip ≈ 25 px right / 30 px below the cursor, follows it: background `#060101`, 1 px border amber-dim, radius 3 px, padding 10 px;
    line 1 date `13 Jun 2023` (`D MMM YYYY`, mono ≈ 11 px `#9b9b98`); line 2 amber square `■` 8 px `#ffd402` + `United States` bold mono `#e1dfd9` + value bold `4%` (trailing zeros dropped: `4%`, `3.2%`).
- Caption box (x 141–1459, y 862–898, 1 px border `#3f3824`): `Latest Inflation Rate (Annual) for United States: 3.7%, released October 14, 2026, up from 3.4% previously. Real range over the visible window: -2.1% to 9.1%, averaging 2.59.`
  proportional ≈ 12 px `#c6c4be`, then ` — SOURCE: THIS APP'S OWN RECORDED RELEASES` mono ≈ 9 px uppercase `#78756e`. (Generated sentence: latest, date, direction, prior, min/max/mean of the visible window.)
- Release calendar box: title `RELEASE CALENDAR (REAL RECORDED HISTORY)` mono ≈ 10 px letter-spacing ≈ 4 px `#59564e`; header `DATE` (left), `ACTUAL` (right x 1033), `FORECAST` (right x 1449) mono ≈ 9 px `#85837d` on `#0f0a03`.
  Rows ≈ 27 px, 1 px separators; date bold mono ≈ 12 px `#fffdf9`; actual bold mono pale amber `#f8f3d3` with `%`; forecast mono `#9a9790` without `%`.
  Rows: 2026-10-14 3.7% 3.6 · 2026-09-11 3.4% 3.4 · 2026-08-12 3.4% 3.4 · 2026-07-14 3.5% 3.8 · 2026-06-10 4.2% 4.2 · 2026-05-12 3.8% 3.7 · 2026-04-10 3.3% 3.4.

## 5. Interactions

- Globe: continuous rotation (or fly-to on hover/selection). Drag-to-rotate likely but not shown.
- Registry: tabs filter the list by group; hover row = highlight + nudge; click row → opens Asset File (instant swap, no animation captured at 0.5 s).
- Asset File: `×` closes back to the globe; yield bars and dashboard rows have hover states; clicking a dashboard row → indicator detail;
  dashboard tabs switch indicator groups; Advanced range buttons change the sparkline window.
- Detail: range `3Y/5Y/10Y/MAX`, `STANDARD DEVIATIONS` overlay, `FORECAST` overlay, `BAR/AREA` chart type, `Compare with…` (Ctrl K) to add another country/series,
  drag to zoom, double-click to reset (stated by the hint), hover tooltip with snapping to the nearest bar; `← BACK TO DASHBOARD`.
- Page-level wheel scroll of the whole asset file (smooth, top bar fixed).
- Ambient animations: blinking block caret(s) and status dot (≈ 1 Hz, irregular duty), pill dot blinking.

## 6. Data

| Field | Samples | Format | Update | Source shown |
|---|---|---|---|---|
| Coverage counters | 82 ECONOMIES · 60 MONTHLY · 27 LIVE FEEDS | integers | on load | — |
| Registry | 23 G20 economies + central-bank code | text | static | badge = rate source (FED, ECB, BOJ, BOE, BOC, RBA, SNB, BIS) |
| Policy rate | 4.00 % | 2 dp | on decision | `PUBLIC FEED · 19:00:00` |
| Sovereign yield curve | 3 Mo 4.220 % | 3 dp in tooltip, 2 dp axis | daily | `as of 2026-10-23` (US Treasury par curve) |
| Main indicators | Interest Rate 4.00 %, GDP q/q 2.2 %, Inflation 3.7 %, Unemployment 4.2 %, PMI 57.6 | 1–2 dp, release date | at release | app's economic-calendar history; gaps from BIS · Eurostat · OECD · World Bank |
| Sparkline | last ~12–24 releases | line | at release | same |
| Advanced cards | GDP $30.77T, GDP/capita $90,027, Pop 341.8M, Real growth 2.2 %, Inflation 3.7 %, Unemployment 4.2 %, CA -3.8 %, Debt/GDP 116 %, FX reserves $1.39T, 10Y 4.99 % | value + Δ vs prior (% or pp) | annual / monthly | World Bank, BIS, OECD via FRED |
| Indicator history | US CPI y/y monthly, 9.1 % Jul 2022 peak | release date + value | at release | "THIS APP'S OWN RECORDED RELEASES" |
| Release calendar | date, actual, forecast | `YYYY-MM-DD`, `3.7%`, `3.6` | at release | same |
| Caption stats | range -2.1 % to 9.1 %, average 2.59 | computed over visible window | on range change | computed |

Note: the caption's range (-2.1 % to 9.1 %) is wider than the 5Y bars shown (min ≈ 2.3 %), so it may be computed over the full loaded series, or the window differs — check when building.

## 7. Key frames

- `t000.0.jpg` — globe view, registry, bottom controls.
- `t001.5.jpg` — registry row hover.
- `t002.5.jpg` — Asset File, clean (summary, dashboard, advanced cards).
- `t003.5.jpg` — yield-curve bar tooltip.
- `t004.0.jpg` — dashboard row hover.
- `t005.0.jpg` — indicator detail, no hover.
- `t009.0.jpg` / `t011.5.jpg` — bar hover with tooltip and column band.
- `t014.5.jpg` — scrolled to bottom: release calendar.

## 8. Open questions

- Globe motion: constant auto-rotation, or a fly-to animation toward the hovered/selected country? Speed if constant (≈ 18°/s measured over 1.5 s).
- Is there a transition (fade/slide) between globe → asset file → detail? None visible at 0.5 s sampling.
- What `LIVE FEED / MONTHLY / ANNUAL` and the bottom-left icon do on the globe view.
- Yield-curve y ticks `5.60, 5.40, 5.10, …` are unevenly spaced (0.2 then 0.3) — tick algorithm unknown.
- Content of the `PRICES`, `GROWTH`, `LABOUR` dashboard tabs; second row of Advanced cards beyond what is visible; the rest of the page below the cards.
- `STANDARD DEVIATIONS`, `FORECAST`, `AREA`, `Compare with…` states never shown.
- Sparkline direction rule for the arrows (GDP 2.2 vs prior 2.1 is shown ↘).
