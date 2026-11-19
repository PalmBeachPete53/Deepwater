# Account tab — shared widget notes

What the two Account pages (Track Record, Risk Calculator) share with the rest of the app, measured on the
reference frames (1600x966). Colours are JPEG-sampled; when two values are given, the first is the flat-area
value, the second the brighter/darker edge. Use these as the source for a common widget kit.

## Palette

| Token | Hex | Where |
|---|---|---|
| page bg | `#040302` (range `#030201`–`#070304`) | behind panels, gaps |
| toolbar / title bar bg | `#0f0a02` → `#140f03` | top bar of each page |
| panel bg | `#0c0601` / `#0d0802` | all panels and cards |
| input bg | `#050401` | text fields (darker than panels) |
| panel border | `#3a351b` (top), `#2c2512`–`#2d2914` (sides) | 1 px |
| divider | `#2d2a15` / `#463d17` | under toolbar, inside panels |
| corner bracket (normal) | `#615942` top-left, `#776f59` bottom-right | L shapes, 2 px |
| corner bracket (highlight) | `#efd860` | featured panel (Performance score), all 4 corners |
| accent yellow | `#fdd51f` / `#fed71c` / `#ffda44` | filled buttons, active pills, toggles |
| accent yellow text | `#fde695` (tab), `#f0d878` (selector), `#e6c200` (labels/verdict), `#ffd709` (big numbers) | |
| positive / cyan | `#1ddbf0` (bars), `#2ed8e5`–`#3adae5` (big values), `#2bc3d3` (dots), `#3fd0de` (lines) | profit, long, reward |
| negative / red | `#ff817d` (bars), `#f78a8b` (big values), `#ef786e` (dots), `#e0605a` (text) | loss, short, stop |
| neutral bar | `#9c9788` | counts (number of trades) |
| text white | `#ece8de` / `#f4f1ea` | values |
| text grey | `#a29c8c` | card titles, secondary |
| text dim | `#8a8478` | inactive tabs, units |
| text faint | `#5d584c` | field labels, captions |
| text ghost | `#3d3a32` | footnotes, counters |
| grid line | `#1b170b` | chart gridlines |
| zero line | `#4c4937` (dashed in line charts) | |
| hover band | `#312c28` | bar chart row hover |
| tooltip bg / border | `#0f0602` / `#544f2d` | all chart tooltips |

## Fonts

- **Mono** (JetBrains Mono-like; zero with a dot/slash): every number, panel titles, tab labels, table cells,
  tooltips. Weights 400 and 700.
  - Panel / card title: 9–11 px, uppercase, letter-spacing 3–4 px, colour `#807d6c`–`#a29c8c`.
  - Big KPI value: 28–30 px bold; hero numbers 44 px (lot size) and ≈72 px (R:R ratio, with glow).
  - Table cell 13 px; tooltip 12 px.
- **Condensed display sans** (Rajdhani / Saira-like): account names, combo values, small footnotes
  (`1 lot = 100,000 units`, `1.00% of balance`), radar axis labels.
- **Chart sans** (Arial/Helvetica-like) 11–12 px: axis tick labels and category labels in charts.

## Panel

- Rectangle, radius 0, 1 px border, bg panel colour.
- Corner brackets: top-left L inset ≈6 px, arms ≈8 px, 2 px thick; bottom-right L the same (slightly brighter).
  Featured panels: 4 brackets, bright yellow, ≈12 px arms.
- Title row at (x+13, y+19): uppercase mono title + optional `ⓘ` (10 px outline circle-i, `#6b665a`) which
  presumably shows an explanation tooltip.
- Gaps between panels 10–12 px; page margins 10 px (Track Record) / 14 px (Risk Calculator).

## Toolbar / title bar

- Track Record: tab group box (tabs mono 12 px bold, letter-spacing 3, inactive `#8a8478`, hover white, active
  yellow `#fde695` on `#2a2605` with 2 px yellow underline; 1 px yellow focus ring after click), a ghost toggle
  (`FILTERS` with funnel icon; on = rounded yellow outline), centred big selector (`All years ⌄`, mono 20 px bold
  yellow on `#211c02`, border `#3a331a`), account box, outlined button (`+ LOG TRADE`) and filled yellow button
  (`IMPORT TRADES`, black text).
- Risk Calculator: thin title bar (37 px): icon + pulsing dot + uppercase spaced title + blinking yellow block
  cursor `█`, and a right-aligned yellow badge (`• EUR/USD · DETERMINISTIC MATH`, border `#544b03`, bg `#1a1503`).
- Pulsing status dot: 5–6 px, yellow, opacity ≈0.5 ↔ 1, period ≈1 s.
- Terminal block cursor: 7x12 `#fbd911`, blink ≈530 ms on / 530 ms off.

## Buttons, pills, toggles, badges

- Filled primary: bg `#fdd51f`, text `#0a0800` mono bold uppercase letter-spacing 2, radius 3, h 26.
- Outlined secondary: border 1 px `#3a3424`, text `#a09988`, h 30.
- Segmented pills: group border 1 px `#2a2618`; active segment fill `#ffda44` with black text; inactive text
  `#73725d`. Variant with dark active fill (`$ | % BALANCE`): active text `#eed882` on `#2a2408`.
- Two-button unit toggle (`% | $`): active 22x33 yellow fill, black glyph; inactive glyph `#55514b`.
- Side badges: `LONG` cyan text `#25a2ae` / border `#1d7f88` / bg `#052423`; `SHORT` red text `#e0605a` /
  border `#862d26` / bg `#2f0a0a`; mono 10 px bold, 42x18, radius 3. Direction badge with arrow: `↗ LONG` cyan on
  `#052931`.
- Count chips: 20x16, radius 2, coloured digit on a tinted bg (cyan on `#0b2a30`, grey on `#1c1a18`, red on
  `#2f0b08`).
- Info pill: 1 px `#383224` border, radius 3, grey mono 11 px (`60 closed trades`).

## Inputs and dropdowns

- Text input: h 38, bg `#050401`, border 1 px `#1b170d`, radius 3, mono 16 px white value, label above in mono 9 px
  uppercase letter-spacing 4 `#5d584c`.
- Native-style combo (filter bar): h 26, bg `#120d02`, border `#2b2817`, display-sans 12 px `#d6d0c0`, white chevron.
- Searchable dropdown (instrument): opens inline (pushes content), bracketed list box, search field auto-focused
  with magnifier and `Search...` placeholder, rows 25 px, current row `#29251d` + yellow text, hover row olive
  `#433603` + yellow text, right-aligned category tag (`FX`) in tiny mono `#3a3630`.
- Search box (tables): bg `#080602`, border `#2a2618`, magnifier + placeholder mono 11 px `#5d584c`.

## Tables

- Header: mono 10 px bold uppercase letter-spacing 2, gold `#d9b54a`; sorted column white with `↓`/`↑`.
- Rows ≈36 px, separator 1 px `#32372d`–`#3c483c`, no zebra. Left 3 px accent bar coloured by outcome
  (cyan / red).
- Numbers right-aligned, thousands comma, trailing zeros trimmed for prices/sizes; money `+$55.50` / `-$100.00`
  bold coloured; dates `DD Mon YY, HH:MM` (September = `Sept`); dim colour for secondary columns.
- Row actions: eye + trash icons 14 px `#5d584c` at the right.

## Charts

- Bars: cyan positive / red negative / taupe neutral; horizontal bar charts with category labels right-aligned
  in chart-sans `#e3e0db`; two-line labels (name + `DD/MM/YYYY`). Bars grow from the axis on first show (≈0.5 s,
  ease-out).
- Hover on a bar row: band `#312c28` across the plot, bar lightens, tooltip.
- Line/area: 2 px line, gradient fill to the zero line (teal `#0f3131`→`#091814` for equity, dark red
  `#6f3331`→`#34170f` for drawdown with a yellow `#e8c020` line).
- Crosshair: 1 px dashed vertical line, marker = white ring + light fill r 4, snapping to nearest point.
- Axis ticks: `$2.0K`, `$500`, `$0`, `−$200` (unicode minus, K with one decimal), month labels `May`/`Apr 26`,
  dates `MM/DD/YY` on category axes.
- Gauges: semicircle (180°) two-colour arc 8 px; donut ring 8 px; split bar 6 px radius 3; radar hexagon with
  gold polygon and 20% fill; gradient score bar red→yellow→green.

## Tooltip

- bg `#0f0602`, border 1 px `#544f2d`, radius 3, padding 12, mono 12 px, line height ≈19 px, no shadow visible.
- Line 1 bold white (title or date in grey), following lines white, the value line coloured cyan/red.
- Placed ≈12–20 px right/below the cursor, flipped to the left or above when near the edge. Follows the
  pointer while moving; also updates when content scrolls under a still pointer.

## Scrolling

- Toolbar (and optional filter bar) are fixed; content scrolls smoothly with the wheel. No scrollbar visible in
  the frames (hidden or overlay scrollbar).

## Not seen on these pages

- No "Terminal Data · HH:MM:SS" footer and no "LIVE" badge on the Account pages. The Risk Calculator's footer is a
  plain explanatory line (display-sans 9 px `#3d3a32`).
