# Market — shared widget notes (part 2: Technical Heatmap, Liquidity)

Companion to `_shared-notes.md` (written for the other Market pages). This file lists what the Technical Heatmap and
Liquidity pages share with the rest of the app, with measured colours (`tools/pick`, JPEG ±4), so a common widget
kit can be built. Where both pages differ, both values are given.

## Palette (measured → suggested token)

| Token | Measured | Suggested hex | Use |
|---|---|---|---|
| `bg.page` | `#050402`, `#060503`, `#040302` | `#050402` | page background (warm near-black) |
| `bg.panel` | `#0a0802`, `#0c0601`, `#0d0802`, `#0a0600` | `#0b0802` | cards, chart, bottom panel |
| `bg.cell` | `#060503` | `#060503` | table cells without fill |
| `border.panel` | `#393322`, `#3f3b1a`, `#443522`, `#433b2f`, `#3d3820`, `#453d31` | `#3f3826` | 1 px panel/card border (dim gold) |
| `border.subtle` | `#181714`, `#1a1815` | `#191714` | dividers, cell borders, chip group borders, search box |
| `border.strip` | `#453d1b` (heatmap), `#393617` (liquidity) | `#3f3a1a` | bottom line of the title strip |
| `gold` | `#fed80f`, `#fcd515`, `#fad717`, `#efd427`, `#ffdb00` | `#facc15` | accent: active chip fill, block cursor, dot, PDH/PDL |
| `gold.text` | `#fce56b`, `#f3e197`, `#e7ddae`, `#fff189` | `#fde68a` | gold text (hover, active tab, badges) |
| `gold.bracket` | `#868064`, `#978567`, `#7f7d66` | `#8a7d60` | corner brackets |
| `cyan` | `#00d8ed`, `#03ddec`, `#04dcf5`, `#2cd5ea` | `#22d3ee` | bullish / sell-side (SSL) / low / up candle |
| `red` | `#f49a9b`, `#ff7f75`, `#fe817a`, `#f48284` | `#f87171` | bearish / buy-side (BSL) / high / down candle |
| `violet` | `#c4b9e4`, `#afa3e5`, `#ada1cc` | `#a78bfa` | liquidity pools in the heatmap, NEAR LIQUIDITY |
| `text.primary` | `#fffffa`, `#fffffd` | `#fffffa` | names, prices |
| `text.body` | `#dddad6`, `#e3e0db`, `#f2f0eb` | `#e5e2dc` | normal text |
| `text.muted` | `#6b6a5e`, `#676558`, `#666357`, `#62665d`, `#6b6a66` | `#67655a` | titles, labels, column headers |
| `text.dim` | `#51504e`, `#4c4b42`, `#48453c`, `#5c5951` | `#4f4d46` | footnotes, sub-lines, `+N` |
| `crosshair` | `#8b8881` | `#8b8881` | chart crosshair |
| `crosshair.label` | bg `#172230`/`#1a1d2c`, text `#e9edf2` | `#1a2130` | crosshair labels (bluish, not warm) |

## Fonts
- Monospace (JetBrains Mono / IBM Plex Mono look): every number, every uppercase label, chips, tags, badges,
  column headers, table values, KPI values.
- Proportional sans (Inter-like) for prose: footnotes, sub-lines (`Sell-side closer`, `today · UTC`), radar axis
  labels (`Trend`, `Momentum`…), sweep sentences (`Sell-side liquidity taken at`), dropdown search placeholder.
- Uppercase labels use wide letter-spacing (2–3.5 px at ~10 px size). Values use normal spacing.

## Title strip (page header inside the sub-tab)
- Height 37 (heatmap) / 46 (liquidity, because it holds a button); bg `#0f0a02`→`#171004` (slight warm gradient);
  1 px bottom line `border.strip`.
- Left: glyph (`∿` pulse on the heatmap, `≡` on liquidity) gold, then a **pulsing gold dot** (●, 8 px, brightness
  cycles ≈2 s), then the page title in muted monospace uppercase letter-spaced, formatted
  `NAME — SUBTITLE` (em dash), then a **blinking gold block cursor** `▌` (≈8x12 px, steps on/off every ~1–2 s).
- Right side holds page-specific content: an info badge (`• 108 INSTRUMENTS · DAILY CANDLES · COMPUTED`) or a big
  white monospace last price (`1.12045`).
- Optional inline control after the title: instrument button (1 px border `#47412d`, text + chevron, dropdown).

## Panel / card
- 1 px `border.panel`, bg `bg.panel`, no radius (square corners) or ≤2 px.
- **Corner brackets**: L-shaped marks ~10 px long, 2 px thick, in `gold.bracket`, at the top-left (inside, offset
  ~3 px) and bottom-right corners. Present on KPI cards and on the chart panel (top-left) and its bottom-right
  resize corner.
- **KPI card**: label (monospace ~10 px uppercase, spacing 3–3.5 px, muted) at top-left, value under it
  (monospace, 22–30 px, coloured with a soft glow), optional proportional sub-line (`of 62 detected`); 2 px **top
  accent bar** in the card's semantic colour (cyan / red / violet / gold) with a faint glow below it. Cards in a
  row share the width equally with 7 px gaps.

## Chips (filters)
- Chip group: 1 px `border.subtle` box, chips inside without separators.
- Chip text monospace ~10 px uppercase, spacing ~2.5 px, `#95928d`; optional count after a space in `#62605a`.
- Semantic chips keep their colour when inactive (`BULLISH` cyan, `BEARISH` red).
- Hover: text → `gold` (`#fce56b`). Active: solid `gold` fill (`#fed80f`), text near-black.

## Tabs (panel tabs)
- Monospace ~9.5 px uppercase semibold. Inactive `#68655b`, hover gold text, active gold text + 2 px gold underline
  (`#f3e197`), a rounded 1 px focus box fades in around the focused tab.

## Tables / rows
- Heatmap: every cell is a separate rounded (2 px) tile, 58 px high, gaps ~6 px; fill colour = meaning, intensity =
  magnitude (see the heatmap spec). Selected row: gold-tinted first cell + 1 px gold border on all cells.
- List rows (liquidity): full width, 36 px pitch, hairline separators `#0f0a02`, no zebra; a gold-tinted
  separator band marks the current price (`LAST 1.12045`).
- Column headers: monospace ~10 px uppercase spaced, muted; active sort header bold cream/gold + `▾`.
- Numbers right-aligned in monospace; signed % with 2 dp; prices at source precision.

## Pills / tags / badges
- Status pill (`TREND BULLISH`): monospace bold ~11 px spaced, coloured text on a ~15 % tint of the same colour,
  1 px border in the colour at ~50 %.
- Price-axis level tags: solid colour bg (cyan / red / gold), dark text on cyan & gold, white on red, h≈14.
- Header badge: 1 px gold border, dark fill, gold bold monospace text with a leading `•`.
- `● DELAYED` footer badge: gold dot + gold monospace bold text in a 1 px gold-dim bordered box (bottom-right).
  (A `LIVE` variant presumably uses the same widget.)

## Legend line
- `⊙ LEGEND` then items `■ LABEL` in their colour (6 px squares), monospace ~9.5 px uppercase spaced, ends with a
  muted explanation (`INTENSITY = MAGNITUDE OF THE READ`).
- Chart legend variant: line swatch (`——` solid/dashed or `···`) + coloured uppercase label.

## Dropdown
- Panel bg `#0d0802`, 1 px `#413e1e` border, shadow; search field on top (magnifier + placeholder `Search...`);
  items ~25 px high, current item bg `#29251d` with gold text; right-aligned tiny asset-class tag (`FX`) in dim.

## Footer
- `Terminal Data · HH:MM:SS` bottom-left, proportional ~9 px `#6d6d65`; status badge (`● DELAYED`) bottom-right.
  Footer and title strip stay fixed; the content between them scrolls (no visible scrollbar in the videos).
- The Technical Heatmap video shows no footer (its table fills to the bottom) — check against other pages.

## Footnotes
- Every computed page ends with a short proportional footnote in `text.dim` (~9–10.5 px) disclosing the method
  (e.g. "Radar: each read on a 0–100 scale where 50 is neutral — …", "Real 3-bar fractal swings clustered within
  0.06% … — never a backtested statistical probability.").

## Motion
- Pulsing dot (≈2 s sine), blinking block cursor (step), row fade-in staggered top→bottom on filter change,
  radar polygon growing from its centre (~0.5 s ease-out), focus box fade-in on tabs (~0.5–1 s).

## Chart kit (liquidity page; likely reused by other chart pages)
- Lightweight-Charts look, French locale for dates (`oct.`, `09 oct. '26  18:00:00`).
- Candles up `cyan`, down `red`, wick same colour; grid almost invisible (`#140f03`); axis text `#8e8d86` ~9 px
  monospace; crosshair dashed `#8b8881` with bluish labels; wheel zoom around the cursor; price auto-scale.
