# Positioning — shared notes (common widget kit)

What the four Positioning pages (Retail Sentiment, COT, Bank Positioning, Risk Sentiment) share with each other and,
most likely, with the rest of the app. All colours measured with the picker on the 1600x966 frames (JPEG noise
±4 per channel; text colours are the brightest ink pixels, so the real glyph colour is the value given or slightly
brighter).

## 1. Base palette

| Token | Hex (measured) | Use |
|---|---|---|
| `bg.page` | `#050402` (`#040302` – `#060500`) | page background, near-black with a warm tint |
| `bg.panel` | `#0c0601` (`#080603` on COT) | panels, KPI cards, overlays |
| `bg.header` | gradient `#100800` → `#181004` / `#211a05` | page title bar |
| `bg.row.hover` | `#1b1503` | table row hover (Retail) |
| `bg.row.group` | `#140f03` | group header rows ("FX · 28") |
| `bg.input` | `#140f05` | filter input, segmented controls |
| `bg.modal` | `#020506` (cool) | Retail detail modal; backdrop `#020200` |
| `border.panel` | `#3a3715` – `#45412f` (1 px) | panel and card borders |
| `border.subtle` | `#2b2a1d` | inputs, inactive pills, segmented outlines |
| `border.separator` | `#1a150a` – `#2b261a` (1 px) | table row separators |
| `border.header` | `#3b321d` – `#4b482c` | bottom line of the page title bar |
| `text.primary` | `#fffefb` / `#f8f5ec` | names, big numbers, symbols |
| `text.secondary` | `#9e9990` – `#a09d94` | table labels, inactive buttons |
| `text.muted` | `#87847e` / `#7e7e76` | numbers like positions count, raw readings |
| `text.dim` | `#5e5c57` – `#635f55` | panel titles, page titles |
| `text.faint` | `#3c3a33` – `#48443d` | descriptions, subtitles, footnotes |
| `accent.yellow` | `#facc15`-ish; pills `#fcd808` / `#ffd706` | active pill fill, section headings, cursors |
| `accent.gold` | `#f2dc99` / `#eadb9f` / `#e2d59c` | table headers, group labels, "Markets" label |
| `gold.dark` | `#302404` – `#383004` | active-but-secondary buttons (dark gold fill) |
| `long / up / risk-on` | `#22d3ee` (rendered `#1fdbf2`, `#2ad4ea`, `#36dffa`) | cyan: long, up candles, positive % |
| `short / down / risk-off` | `#ef5f5f` – `#f87171` (rendered `#ea5d60`, `#e8605c`, `#ff8e87`) | red/coral: short, down |
| `neutral / range` | `#facc15` (`#ffdf2e`, `#f5de6c`) | range calls, neutral zone |
| `positive.alt` | `#4ade80` muted (`#6f9276` as text) | COT positive changes / net |
| `purple` | `#a78bfa` (`#a589c1`) | third chart series |
| quarter palette | yellow `#fcd400`, cyan `#20d8f0`, lavender `#b4a0f8`, green `#24c870`, orange `#fc9c24`, pink `#f48cc4` | Bank forecasts Q4'26 … Q4'28 |

Cyan and red are always the long/short pair across the four pages; yellow is the neutral/third state.

## 2. Fonts

- **Monospace** (JetBrains Mono / IBM Plex Mono look-alike; slashed zero `0` with a dot — e.g. `40`, `0` in
  "1/3 markets") for: titles in caps, all numbers, table cells, pills, badges, axis labels, footers.
- **Condensed proportional** (Rajdhani / Barlow Condensed look-alike, bold) for: big card titles (`FX`, `ASIA & EM
  FX`), page names (`COT Advanced`), modal titles (`FX BANK FORECASTS`), status badges (`CROWDED LONG`).
- **Proportional sans** (Inter-like) for: descriptions, narrative text, chart axes from TradingView-style charts,
  bank names, tooltips.
- Caps titles: 10–11 px, letter-spacing 3–4 px (very wide), dim grey `#5e5c57`–`#635f55`.
- Big numbers: 24–28 px bold mono (KPI cards, market cards), 64 px for the gauge value.

## 3. Page title bar

Common to Retail Sentiment, Bank Positioning and Risk Sentiment (COT uses a lighter variant):

```
[icon] ● TITLE IN CAPS — SUBTITLE IN CAPS ▌                              [right badge / controls]
```

- Height 34–44 px, warm dark gradient bg, 1 px bottom border.
- Icon: 14 px outline glyph in pale gold `#cbbf96` (users icon, bank building, gauge).
- Status dot: 5–6 px round, olive `#7e763c`; on Risk Sentiment it **pulses** (≈2 s period) up to `#f2dd64`; the
  dot in a panel header and in the right badge pulse in sync.
- Title: mono 11 px uppercase, letter-spacing ≈4 px, `#5f5a52`–`#635f55`, em dash with spaces between title and
  subtitle.
- Block cursor right after the title: solid yellow `▌` ≈7x13 px `#fde78f`, **blinks** (≈3 s period, roughly half
  on). The same cursor appears after some panel titles (Risk Sentiment `COMPOSITE RISK APPETITE ▌`) and blinks in
  sync.
- Right badge (status pill): bold mono 9.5 px uppercase letter-spaced ≈1–1.5 px, gold text `#f6e29a`/`#fae8a1` on dark
  gold `#3a3004`–`#483c00`, 1 px gold border `#7a6a1a`, radius 2, leading small dot. Texts seen: `• 677 NOTES
  ARCHIVED · UPDATED WITH EVERY NEW BANK NOTE`, `• 7 MARKETS · DAILY CLOSES · PERCENTILE VS 1 YEAR`.
- COT variant: `COT Advanced` (condensed 13 px near-white) + dim subtitle `CFTC · FREE, WEEKLY, OFFICIAL`.

## 4. Panels and cards

- Panel: bg `#0c0601`, 1 px border `#403a28`/`#3a3715`, no or tiny radius (≤2 px).
- **Corner brackets**: an L-shaped 1.5 px gold mark at the inside top-left (`⌜`, arms 8–10 px, `#8c885b` bright on
  KPI cards, `#5f5a3a` elsewhere) and at the inside bottom-right (`⌟`, dimmer `#464324`). Used on KPI cards, market
  cards, the Risk Sentiment panels and the Bank forecasts overlay. Not on COT panels.
- Panel title: inside the panel at top-left, mono 10 px uppercase, letter-spacing ≈3 px, `#5e5c57`; optional
  ` · 1 YEAR`-style suffix in the same style; optional dim subtitle; right side for legends or values.
- KPI card (Retail): 2 px coloured top accent line (cyan / red / yellow, lighter inner row), label 10 px caps
  letter-spaced dim, big value 24 px coloured with glow, sub-label 10 px caps dim.
- Hover on clickable cards (Bank market cards): translateY −3 px, brighter top border, arrow icon `↗` turns pale
  yellow `#ecdda5`; ≈200 ms transition.

## 5. Section headings (COT)

`● SUMMARY`, `● ADVANCED`: yellow dot 5 px + bold 11 px caps letter-spaced ≈2.5 px yellow `#f8e4a9`, then a 1 px
rule `#25241b` to the right edge.

## 6. Pills, segmented controls, buttons

- Segmented control: outer 1 px border `#2b2a1d`, bg `#140f05`, items mono 11–12 px `#807d74`; **active item =
  solid yellow pill** (`#fcd808`/`#ffd706`), dark text (`#0c0601`), bold, radius 2–3 px. Seen: `All | FX |
  Commodities | Crypto`, `Majors first | Most extreme | Most active | A-Z`, `DEVISES | COMMODITIES | INDICES`,
  `DESK | ALL CALLS`, `EN | FR`, pair pills `EUR/USD …`.
- Secondary active (dark gold): bg `#302404`–`#383004`, border `#43381e`/`#3e3109`, gold text `#d5c788`–`#dfcc80`
  (COT currency buttons, `Unweighted`, TFF category, `FX BANK FORECASTS` button).
- Neutral active (grey): bg `#242020`, text `#e8e3da` (COT range `1Y`).
- Outline button: 1 px `#272215`, text `#7d7a74` (`☰ Custom`, `× CLOSE`).
- Filter input: bg `#140f05`, border `#2b2a1d`, magnifier icon + placeholder `Filter…` mono 12 px `#535047`.

## 7. Status badges (table)

Condensed bold 10 px caps with a small trend glyph, h≈16, radius 2, 1 px border:
- Long: text `#7ec5d0`, bg `#062c2f`, border `#266d7e` — `↗ CROWDED LONG`, `↗ LEANING LONG`.
- Short: text `#bf7169`, bg `#3e2008`, border `#51200e` — `↘ CROWDED SHORT`, `↘ LEANING SHORT`.
- Neutral: text `#7a766f`, transparent, thin border — `— BALANCED`.

## 8. Tables

- Header: mono 9–11 px bold caps letter-spaced 1–2 px, gold `#eadb9f` (Retail) or grey `#848076` (COT), coloured
  group headers (`LONG` cyan, `SHORT` red); a 1 px rule under the header.
- Rows: 25–36 px pitch, 1 px separators `#1e1a0f`–`#242415`, label proportional 11–12 px, numbers mono 12.5–13 px
  right-aligned, comma thousands separators, `—` (dim) for empty. Hover = whole-row bg `#1b1503`.
- Signed numbers: `-` for negatives, no `+` in COT tables; `+`/`-` on percentages and deltas elsewhere
  (`+0.81%`, `-1.65%`, `+5.64 pts`, `↗ 2pt`).

## 9. Charts

- Background = panel bg; no chart border; grid lines dotted or 1 px very dim (`#151006`–`#201f14`).
- Axes: right-side price axis, mono 11 px `#7c7972`–`#878680`; time axis proportional 11 px, month names (`NOV
  DEC JAN …` caps on Risk; `Oct Dec Feb` on Bank; year in place of January on COT).
- Up/down candles: cyan / red, thin wicks, no hollow bodies. Last price: dotted line + filled label on the axis.
- Crosshair: dashed lines (`#5c5b45` olive on COT, cream `#f9f4b5` on Risk); axis labels in filled boxes (cream
  text on `#483c04`).
- Tooltips: dark box `#0c0800`/`#0c0804`, 1 px border `#62553f`, radius 3, white mono 12 px, coloured dot for the
  series.
- Lines glow (same-colour blur ≈4 px) on Risk Sentiment; area fills fade to transparent.
- 3D pies/donuts (Retail, COT): tilted ellipse with extruded side walls, cyan top `#14c0d0`→`#23edf9`, coral
  `#f46868`, darker side walls, soft shadow, % labels in white bold.
- Embedded TradingView chart (Retail modal) uses TradingView's own dark theme (bg `#0a0a0a`), candles `#00c2e4` /
  `#ff656b`.

## 10. Footer

`Terminal Data · HH:MM:SS` bottom-left, proportional 10 px `#53514b`–`#57554e`, on page bg; right-aligned state
badge: `● DELAYED` (yellow text `#f3dc8a`, bg `#352a00`, border `#3f3706`) or `● OFFICIAL` (cyan text `#78ccd4`, bg
`#10241c`, border `#273b38`), bold condensed 10 px caps, h≈20, radius 2. Retail and COT have it; Bank Positioning
and Risk Sentiment do not (Risk has a full-width footnote ending with `UPDATED 16:06` instead).

## 11. Scrolling

- Page header and (on COT) the selector row stay fixed; the content below scrolls with smooth wheel scrolling.
- No scrollbar is visible in any recording (hidden / overlay scrollbars).

## 12. Modals / overlays

- Large dialogs over a near-black flat backdrop (`#020200`–`#040302`), page hidden.
- Retail modal: cool blue-black bg, 2 px cyan top accent line fading left→right, `×` close at the top-right.
- Bank overlay: same panel style as the page (gold border, corner brackets), `× CLOSE` outline button above the
  top-right corner.
- Opening animations: chart skeletons while loading; bar/line charts draw in from the left (≈0.5 s).
