# Market — shared widget notes (part 1: Markets Watch, Seasonality, Fundamental Heatmap)

Companion to `_shared-notes-2.md` (Technical Heatmap, Liquidity). Same token names are reused where the values
agree; differences are listed. Colours measured with `tools/pick` on the reference frames (JPEG noise ±4).

## Palette (measured → suggested token)

| Token | Measured | Suggested | Use |
|---|---|---|---|
| `bg.page` | `#040302`, `#050402`, `#030302` | `#050402` | page background (warm near-black) on all three pages |
| `bg.strip` | `#0f0a03`, `#0f0a02`, `#120d04` | `#100b03` | page title strip / sticky header / sheet header |
| `bg.panel` | `#0c0601`, `#0d0802`, `#080603` | `#0b0802` | cards, list panels; seasonality & sheet cards slightly darker `#080603` |
| `bg.row.alt` | `#140f05` | `#140f05` | zebra row / row hover in market lists |
| `bg.tooltip` | `#0f0a03`, `#0d0601` | `#0e0802` | chart tooltips |
| `bg.menu` | `#110b03` | `#110b03` | dropdown menus |
| `bg.menu.sel` | `#352b05`, `#2a2010` | `#352b05` | selected / highlighted dropdown row (dark gold) |
| `border.panel` | `#433d22`, `#454225`, `#413b27`, `#3e381d` | `#423c24` | list panels, stat cards, macro cards (dim gold) |
| `border.card.dim` | `#211d12`, `#232217`, `#24221a` | `#232017` | detail-sheet cards, seasonality cards |
| `border.subtle` | `#1a1b14`, `#202212`, `#251f12`, `#1a150a` | `#1d1b12` | search box, segmented groups, toolbar buttons, NEXT MEETING box |
| `rule.strip` | `#2d2a1a`, `#2b2814`, `#443b24`, `#242418` | `#2d2a1a` | 1 px bottom line under title strips / toolbars |
| `gold` | `#fdda11`, `#fcda0b`, `#ffe611`, `#f2d900` | `#facc15` | active pill/segment fill, block cursor, diamond marker |
| `gold.text` | `#e6d35f`, `#f2d632`, `#eed75d`, `#f2e6b3` | `#f0d24a` | panel titles (FOREX), section labels (SUMMARY), legend |
| `gold.ring` | `#c2b78a`, `#bbad7c`, `#d4b030` | `#c9b070` | selected card border, clicked segment ring, selected heatmap row |
| `gold.bracket` | `#877c50`, `#8a7d60` | `#8a7d60` | corner brackets |
| `cyan` | `#2dd5ee`, `#1acde1`, `#1ed7ee`, `#3bc8de`, `#00c4df` | `#22d3ee` | up / positive / long / bullish |
| `cyan.dim` | `#168c98`, `#144e54`, `#0f4f58`, `#005658` | `#14606a` | weak-positive cells, bullish pill bg |
| `red` | `#e05a52`, `#e4665c`, `#f06563`, `#eb7063`, `#ff6972` | `#ef6b62` | down / negative / short / bearish |
| `red.dim` | `#a2544d`, `#552924`, `#471b18`, `#814239` | `#5a2a24` | bearish pill/tile bg, past-month bars |
| `violet.pill` | bg `#1f1741`, text `#9490a9` | | period pills in Seasonality (`Jan-Jun`) |
| `text.primary` | `#f1f1ed`, `#edeae3`, `#fffdf1`, `#f3f2ef` | `#f2f0ea` | titles, symbols, big values |
| `text.body` | `#cbcab4`, `#c8c4bc`, `#c9c6c1` | `#c8c5bc` | buttons, prose, table labels |
| `text.muted` | `#87847e`, `#8a877e`, `#8a857c` | `#88857d` | inactive pills, axis labels, sub values |
| `text.label` | `#5e5956`, `#5a564e`, `#57534c`, `#5a574d` | `#5a564f` | uppercase spaced labels, column headers |
| `text.dim` | `#463c30`, `#3b372f`, `#4a463e`, `#474745` | `#45413a` | placeholders, card labels, footers |
| `grid` | `#1a1a17`, `#14120a`, `#110f08`, `#27261b` (dashed) | `#16140d` | chart grid |
| `zero.line` | `#656345` (dashed), `#3e3a20` | `#5a5838` | zero / target baseline in charts |
| `crosshair` | `#8b8b83` (seasonality), `#c8c4bc` dashed (fundamentals) | `#8b8881` | chart crosshair lines |

Status colours are always cyan (good/up) vs red (bad/down); grey for neutral; gold for "the selected thing" and for
the near-target state.

## Fonts
- **Monospace** (JetBrains Mono / IBM Plex Mono look, slashed zero): all numbers, prices, percentages, uppercase
  labels, pills, tabs, segment labels, dates in data, footers. Labels: 9–11 px uppercase, letter-spacing 2–3 px.
- **Display sans** (heavy, Inter Black / Archivo look): page big titles `MARKETS` (≈44 px, weight 900, spacing 2 px),
  currency code `AUD` (32 px), leader/laggard ticker (22 px), symbols in sheet headers (13 px bold).
- **Proportional sans** (Inter-like): prose (Alfred text 12 px), table labels (`Policy rate`, `Interest Rate`),
  sub-names (`Adobe`), footnotes in Seasonality (9–10 px), toolbar buttons in Seasonality (12 px).
- Big KPI values: monospace 24–44 px, weight 500–700.

## Page header patterns
Three variants are seen:
1. **Hero header** (Markets Watch): eyebrow (monospace 10 px spaced, `text.label`), huge display title, inline
   subtitle with a bold count + spaced monospace description, then a **gold rule** fading to the right
   (1 px, `#d6d0b7` → transparent over ~1500 px, faint gold halo above/below). Faint warm radial glow top-right.
2. **Title strip** (Fundamental Heatmap, also Technical Heatmap / Liquidity): 36 px strip `bg.strip`, glyph icon +
   pulsing gold dot + `NAME — SUBTITLE` monospace spaced + **blinking gold block cursor** `▌`; right badge with 1 px
   gold border and `• N ECONOMIES · …` gold bold monospace. Sticky on scroll.
3. **App-bar header** (Seasonality): 31 px strip, `Seasonality Lab` sans semibold 13 px + small caps subtitle; under
   it a 34–39 px toolbar row of bordered buttons. Both sticky.
- The gold rule is reused under section titles (`MACRO FILE · AUSTRALIA / AUD`).
- Blinking block cursor is also appended to sub-titles (`MACRO FILE · AUSTRALIA ▌`), each cursor blinking ≈1 Hz.

## Panels / cards
- Square corners (0–2 px radius), 1 px border, flat fill, no shadow.
- **Corner brackets** (gold, L-shaped ~8–10 px, 1–2 px thick) inside the top-left and bottom-right corners on
  list panels (Markets Watch) and macro cards (Fundamental Heatmap). Top-left bracket often paired with a small gold
  diamond `◆` before the card title.
- Card title styles:
  - list panels: `• FOREX ²⁸` gold bold monospace 13 px + right `avg -0.14%`;
  - macro cards: `◆ MONETARY POLICY · RBA` monospace 10 px spaced 3 px `text.label`;
  - Seasonality cards: white monospace bold 15 px spaced 3 px (`SEASONAL TREND OF …`);
  - detail-sheet cards: bold sans 10 px uppercase `#c8c4bc` with a 28 px title row and a bottom rule `#272618`.
- **Section labels** (Seasonality): `• SUMMARY` gold bold monospace 11 px spaced 2 px + 1 px rule to the right edge.
- **Stat cards** (Markets Watch): label 10 px spaced; value 22–24 px; sub-line 10 px dim; mini 3 px breadth bar
  cyan/red.
- **Score cards** (Fundamental Heatmap): 2 px top accent bar in the bias colour, index `01`, flag, code, signed
  score, centred ± gauge bar, full-width bias pill.

## Pills, segments, buttons
- **Filter pill** (category): inactive bg `#060303`, 1 px border `#1e1a14`, label monospace 12 px spaced `#87847e`,
  count in a dark inner badge; active = solid gold fill, black text, darker gold count badge.
- **Segmented control**: 1 px group border `border.subtle`, segments without separators; active = solid gold fill
  black text (Markets Watch, `10Y` in Fundamentals) **or** teal bg + cyan text (Seasonality default) **or** cyan text
  with a gold rounded ring (Seasonality clicked / hovered segment).
- **Toolbar button** (Seasonality): h 26, 1 px `#251f12`, bg `#0f0a03`, text 12 px `#cbcab4`, trailing `⌄`;
  open state → cyan text and `⌃`, gold border on some.
- **Bias pill**: monospace bold 12 px spaced 2 px; bullish bg `#144e54` / text `#4fd0dc`; neutral `#292825` /
  `#cdcdcd`; bearish `#552924` / `#c16762`.
- **Status tile** (technical heatmap strip on the sheet): label 8 px caps + value 12 px; red tile `#471b18` border
  `#451f1e`; grey tile `#272523`.
- **Period pill**: violet bg `#1f1741`, text `#9490a9`, monospace 10 px spaced.
- **Volatility-state pill**: tinted bg of the state colour (yellow `#3a2d00`, cyan `#013130`, red `#1a0805`).
- **Info box with gold emphasis**: `NEXT MEETING · 2026-11-03 · IN 11D` (1 px subtle border, spaced monospace, last
  part bold gold).
- **Keyboard hint**: small bordered box with `/` at the right of the search field.

## Dropdowns
bg `bg.menu`, 1 px border `#28281a`, square; rows 25 px, monospace 12–13 px; selected/highlighted row `bg.menu.sel`
with white bold text; optional search field on top (magnifier, gold focus border); optional right tag (`FX`
monospace 9 px dim); optional grey note paragraph at the bottom. Only one dropdown open at a time; it stays open
while the page scrolls.

## Tables / rows
- Market lists: rows 33 px (49 px with a sub-name), zebra `#0c0601` / `#140f05`, no separators; icon 16 px +
  bold monospace symbol; numbers right-aligned monospace; change signed 2 dp % cyan/red.
- Comparator / data-compare tables: 26–31 px rows, 1 px rules `#1d180d`–`#252117`, proportional labels, monospace
  values right-aligned, the better value in cyan.
- Heatmap tables: tile cells 58 px high, 5–6 px gaps, radius 2, colour = meaning, intensity = strength; selected row
  = 1 px gold border on every cell; hover = scale/lift.
- Column headers: monospace 9–10 px uppercase spaced `text.label`, special column (`BIAS`) in gold bold.

## Charts (common rules)
- Transparent/dark plot bg, faint horizontal grid only, axis labels monospace 10 px `#8a877e`/`#6f6c63`, no axis
  lines; zero/target line dashed olive.
- Line series: cyan `#3bc8de` (primary), gold `#efd56e` (inflation / average), blue `#5a86d0` (USD index); 1.5 px,
  soft glow; area fill gradient from ~35 % alpha to 0 under the line, or between the line and a target (red above,
  teal below).
- Step lines for policy rates; end point dot 4 px; last value printed top-right of the chart card.
- Bars: flat fill, cyan positive / red negative, no labels, near-full category width; highlighted category (current
  month) in salmon `#ff827e`.
- 3D pies/donuts (win rate, retail position, COT): tilted ellipse with extruded side walls (darker shade of the slice
  colour), labels white bold on the slices. Needs a custom painter (QPainter paths).
- Crosshair + tooltip: vertical line (solid grey or dashed light), dot on the series with a white core, small
  tooltip box (`bg.tooltip`, 1 px warm border `#2a2518`/`#5d4b2a`) with date (dim), value (bold), optional delta line
  in cyan/red; tooltip flips side near the right edge. Y-axis marker dot at the hovered value (Seasonality).
- Max/min annotations (value label + dot) and a dashed `TODAY` vertical marker with a small cyan label.
- Draw-in animation (series revealed left→right in ≈0.5 s) when the data set changes (Fundamental Heatmap).
- Loading states: skeleton bars, `Loading...` text, spinner + `Computing …` message, `Not connected` /
  `DATA SOURCE NOT CONNECTED.` with a plug icon; cards load independently and may change height (reflow).

## Scrolling
- Pages scroll as a whole with smooth (eased ≈0.5 s) wheel scrolling; title strips, toolbars and the detail-sheet
  header are sticky. No visible scroll bar in any of the three videos (overlay / hidden scroll bars).

## Other shared elements
- Flags: 20x14 rectangular (cards), 16x11 in rows; FX pairs show two overlapping flags.
- Instrument icons: commodity glyphs, company logos (circular 16 px), coin logos, 2-letter grey badges fallback.
- French strings appear inside an English UI (`VOLATILITÉ NORMALE`, `LECTURE FONDAMENTALE D'ALFRED`, `Alfred lit les
  publications…`, `FR` badge): the build should route every string through the translation layer.
- No "Terminal Data · HH:MM:SS" footer and no LIVE badge are visible on these three pages (Markets Watch states
  "QUOTES REFRESH EVERY 20S" in its subtitle instead).
- TradingView Advanced Chart is embedded as-is in the instrument sheet (its own dark theme, not the app palette).
