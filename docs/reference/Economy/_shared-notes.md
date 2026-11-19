# Economy (+ News › Squawk) — shared widget notes

What the pages Economy › Macro, Economy › Rates and News › Squawk share with the rest of the app, so a common widget
kit can be built. Colours measured with the pixel picker on 1600x966 JPEG frames. Small text is anti-aliased: its
measured core is paler/duller than the design colour, so a recommended token is given (flat fills give true colours).

## Palette (tokens)

| Token | Value | Where measured |
|---|---|---|
| bg.page | `#030201` – `#080500` | squawk list, macro asset file, rates body `#040200` |
| bg.panel-header | `#0d0f05` / `#181205` | squawk header; rates panel title bar |
| bg.table-header | `#0b0a03` – `#170d02` | rates table header, macro dashboard header |
| line.hairline | `#211d12` – `#2d2a1e` | row separators, section rules |
| line.panel-border | `#241f12` – `#38311e` | panel/table frames |
| line.box-border (macro) | `#282212` – `#474331` | summary/dashboard/chart boxes |
| amber.accent | `#F5C518` (fills `#ffd900`/`#ffda00`) | selected tab/range buttons, bars top, carets, dots |
| amber.text | `#E5B112` (AA core `#c5a925`–`#e5b112`) | panel titles, section headings |
| amber.pale (values) | `#FDE575` / `#f4edb9` | big policy rate, latest values |
| amber.dim-fill | `#332802` – `#3d2901` | LIVE badge, active filter pill, selected chip |
| amber.row-select | `#352b04` → `#1d1704` gradient + 3 px bar `#f5d648` | rates selected row |
| hover.row | `#1a1002` (lists), `#201b05` (rates table), `#1a1710` (macro table) | |
| text.primary | `#e7e3d8` – `#f5f4ef` | names, titles |
| text.body | `#bbbab6` | squawk headlines |
| text.secondary | `#8a877e` – `#99948b` | buttons, axis labels |
| text.muted | `#5b574e` – `#75746e` | captions, letter-spaced subtitles |
| text.faint | `#3b3a36` – `#4e4d46` | footers, footnotes |
| cyan.positive | `#22D3E0` (flat bar `#19daf1`) | hike, positive %, implied bp, up-arrows |
| red.negative | `#F05A50` (flat bar `#f46a5d`) | cut, negative %, countdowns, down-arrows |
| teal.+25 | `#00c2bd` | +25bps bars/series, Hike pill fill `#052523` |
| green.+50 | `#00aa7c` | +50+bps |
| red.-25 / darkred.-50 | `#c84d48` / `#912922` | -25bps / -50+bps |
| grey.no-change | `#aab1ba` (bar), `#c4c6c7` (line) | No change |
| inferred.cyan | `#46c3d7` | `QUARTERLY`/`ANNUAL inferred` tags |
| impact.dot | `#ffd200` | calendar impact dots |

Semantic colouring rule seen everywhere: good/up/hawkish = cyan, bad/down/cut = red, neutral = grey. In macro cards
the colour follows economic meaning (inflation up = red, FX reserves up = cyan), not the sign.

## Typography

- Two families: a **monospace** (JetBrains Mono / IBM Plex Mono-like, slashed zero `0` with dot) for numbers, times,
  labels, buttons, badges; a **condensed geometric sans** (Rajdhani / Barlow Semi Condensed-like) for headlines,
  names, titles, tooltips and chart axis labels in Macro detail.
- Micro labels: mono ≈ 9–11 px, UPPERCASE, letter-spacing 2–4 px, muted colour (`NEXT-MEETING OVERVIEW · 8 CENTRAL BANKS`,
  `ASSET FILE`, `RELEASE CALENDAR (REAL RECORDED HISTORY)`).
- Panel/section titles: mono bold ≈ 11–14 px uppercase, letter-spacing 1.5–3 px, amber.
- Big numbers: mono bold (rate 56 px in Macro summary; 20 px stat cards; 14–15 px table values).
- Country/page titles: condensed sans bold ≈ 46 px, letter-spacing 2 px (`UNITED STATES OF AMERICA`).

## Shared widgets

### Section heading
`•` amber dot (5–6 px) + amber mono bold uppercase text (`• SUMMARY`, `• ADVANCED`, `• TERMINAL DATA`, `• POLYMARKET`,
`• COMMITTEES & VOTES`, `• SQUAWK`) followed by a 1 px hairline to the right edge (amber-tinted, may fade to transparent).

### Status dot / caret (ambient "live" animation)
- Amber dot before titles pulses: bright `#f3ce0b` ↔ dim `#6e6d22`, dim ≈ 0.5 s every ≈ 1.5 s (rates), ≈ 1 Hz (macro top bar).
- Macro: amber block caret `█` (≈ 6x11 px) after the top-bar title and after `ASSET FILE`, blinking.

### LIVE badge / footer
- `LIVE` badge: fill `#332802`, text amber bold mono ≈ 10 px, radius 2 (squawk header).
- Footer `Terminal Data · HH:MM:SS` mono ≈ 10 px `#4e4d46` left; `● LIVE` pill right (fill `#332600`, 1 px amber border, text `#ceaf49`).
  Each column/panel has its own footer (squawk: two footers).

### Panel chrome (Rates)
Framed panel, 1 px border `#241f12`; title bar 34 px (`#181205`, bottom line `#676150`) with `⠿` drag handle,
amber mono bold title, right icon row `⚙ 📌 🔒 ⌄ ⤢ ×` (`#777368`, ≈ 18 px pitch). Body scrolls under it.

### Tabs / segmented buttons
- Filled-amber selected (`#ffd900` fill, near-black bold mono text): macro registry `★ G20`, `MAIN INDICATOR`, `10Y`, `5Y`, `BAR`.
- Dim-fill selected (`#3d2901` fill, amber text, dim count): squawk `DATA 427`; rates chip `OCT 29 '26` (`#3a2e00` + amber border).
- Unselected: no fill or 1 px border `#2a2418`, text `#78756e`–`#8a8781`; hover brightens the text.
- Count suffix in a dimmer colour after the label (`ALL 465`, `High 35`).

### Pills / badges
- Action pills (fully rounded, h 16): Hold grey (`#34311b`/`#a4a29b`), Hike cyan (`#052523`/`#6fbac2`), with leading dot.
- Bias pills (radius 3): `NEUTRAL nn` grey, `HAWKISH nn` cyan.
- Countdown badge `in 5d`: cyan text on `#0f3324`.
- Registry badge `• FED`: fill `#544500`, border `#786504`, text `#eadf92`.
- Calendar chips: frequency `QUARTERLY inferred` (cyan outline) and range `1.0% – 1.3%` (amber outline, amber text).

### Tables
- Header row: darker band, mono ≈ 9 px uppercase muted labels; numeric columns right-aligned; column headers coloured by
  meaning where relevant (CUT red, HIKE cyan, MAIN INDICATOR amber).
- Rows: 1 px hairline separators, no zebra; hover = warm dark highlight; selected = amber left bar + amber gradient.
- Two-line cells: primary value (mono 13–15 px) + secondary line (mono 9–10 px muted) — `as of Oct 22`, `-1.7 1w`, `Federal Reserve`.

### Charts
- Background = panel background; grid = 1 px very faint horizontal lines only; axis labels mono ≈ 10 px muted (Macro detail uses the sans).
- Bars: amber vertical gradient (`#fdd50d` → `#88741a`) for single-series history; flat cyan/red for signed values; outcome colours for probabilities.
- Value labels above bars/points, mono ≈ 10 px muted.
- Hover patterns:
  1. Column band + bar recolour (Macro): hovered bar `#ffe455`, band `#332609` over the bar slot.
  2. Category crosshair (Rates): dashed white vertical line that slides to the category.
  3. Free crosshair (Rates odds): dashed vertical + horizontal lines, value badge on the y axis and date badge on the x axis (amber-dim fill `#3d3303`, light text), series markers.
- Tooltip: dark fill (`#060101`–`#0d0b07`), 1 px amber-dim border, radius 3–4, padding ≈ 10 px; first line date/category muted, then rows of
  colour swatch (square for bars, dot for series) + name + bold value; follows the cursor at ≈ (+25, +25) and flips left near the right edge.
- Number formatting: trailing zeros dropped in tooltips (`4%`, `3.2%`); `<1%` for tiny probabilities; signed values always carry `+`/`-`;
  bps with 1 decimal; rates with 2 (tables) or 3 (yield tooltip, axis `4.750%`) decimals.

### Search / inputs
Dark fill (`#030200`–`#140b03`), 1 px border, magnifier icon, muted placeholder; optional key chip (`Ctrl K`, 1 px border, mono 9 px).

### Scrolling
Smooth wheel scrolling (≈ 100 px per notch), free (no snap), headers/title bars fixed; no visible scrollbar in any frame
(overlay scrollbars that appear only while scrolling, or hidden).

### Flags
Rectangular flags ≈ 20x14 (tables, chips, registry), 44x28 in page titles; no rounding.
