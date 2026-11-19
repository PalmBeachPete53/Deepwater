# Shared look — Global › Session Recap, News › Calendar, News Feed, Bank Research

Measured with the colour picker on the 1600x966 frames (JPEG: values ±3). Use these as the common widget kit.

## Palette

| Token | Hex | Where |
|---|---|---|
| bg.page | `#050402` / `#040302` | page background (near-black, warm) |
| bg.page.alt | `#0f0a02` | Calendar page background, reader top bar |
| bg.panel | `#0a0905` / `#0a0a0a` | side panels, cards (Calendar detail, Alfred's read) |
| bg.panel.warm | `#0d0802` | chart panels, mini cards |
| bg.strip | `#120d04` → `#1a1305` | page strip at the top (title + badge) |
| bg.band | `#241c05` / `#231b04` | olive header bands (registry headers) |
| bg.row.hover | `#251f0b` | table row hover (Calendar) |
| bg.row.selected | `#433805` (Session list) / `#2a2104` (Bank list) | selected list row |
| bg.card.hover | `#1a1302` | story card hover |
| bg.group | `#110f08` | day group header rows |
| bg.expanded | `#140d08` | inline expanded row |
| border.subtle | `#241e1a` / `#272215` / `#2d281b` | panel/card borders, filter separators |
| border.row | `#20210a` / `#14120c` | row separators |
| border.gold | `#46412f` / `#4d4a39` | card borders with a gold tint |
| border.gold.strong | `#d2c38d` | selected card |
| gold.accent | `#fdd90b` (`#fed41c`, `#fdd913`) | solid yellow pills/buttons: EN, BARS, 20Y, BACK TO FEED |
| gold.text | `#f2da93` / `#e4d080` / `#dbc677` | titles (MARKET WIRE, LATEST HEADLINES), active tab text + underline |
| gold.label | `#d1c497` / `#cebb96` | category / topic tags |
| gold.soft | `#e1d6a5` | section labels (ECONOMISTS' RANGE) |
| gold.bracket | `#f3eaaa` (bright) / `#918a73` (subtle) | corner brackets |
| cursor.block | `#f5e392` | blinking block cursor |
| text.white | `#fefdf8` | names, titles, actual values |
| text.body | `#b4afa6` / `#a6a59f` | paragraphs |
| text.mid | `#84837f` / `#807e77` | meta lines |
| text.dim | `#504e46` / `#5b5750` | labels, ages, axis captions |
| text.faint | `#45443f` | ages on cards |
| pos (text) | `#7eb8ab` / `#8bc6cd` | positive values, ↑ arrows |
| pos (line) | `#1bdbeb` core, `#3ed3e0` | positive series lines, dots |
| pos (fill) | `#0f3131` → transparent | area under positive series |
| neg (text) | `#d69997` / `#bb7373` | negative values, ↓ arrows |
| neg (line) | `#fd7f83` / `#e08c8b` | negative series lines, end dots |
| high impact | `#9e6a6a` text | "HIGH IMPACT" label |
| important | `#c7b691` text | "IMPORTANT" label |
| medium impact marker | `#52565d` (grey diamond) / `#5d5c56` (grey dot) | |
| high/med impact dot (calendar) | `#f4da29` | yellow impact dots |

Range-bar gradient (Calendar): `#c46757` → `#ce9239` → `#d8af1c` → `#89ae6c` → `#22a5b2`.

## Typography

- **Monospace** (JetBrains Mono / IBM Plex Mono–like) for every label, number, time, ticker and badge.
  Labels are UPPERCASE, 9–11 px, letter-spacing 2–4 px (section titles up to 6 px for hints), colour text.dim.
  Numbers 12–14 px, bold for the "actual"/current value.
- **Condensed sans** (Rajdhani / Barlow Semi Condensed–like) for headlines, event names, bank and instrument names; semibold 13–20 px,
  bold up to 46 px (reader title). Big display titles (`NEW YORK SESSION`, `MARKET WIRE`) bold, uppercase, letter-spaced, 26–32 px.
- Body text ≈14–15 px, line-height ≈1.7.

## Recurring widgets

1. **Page strip** (37–40 px): left `icon · gold dot · TITLE — SUBTITLE ▮` (monospace dim, blinking block cursor 7–9x14 px), right a **badge**
   (`• A · B · C`: monospace bold 10 px spaced, text `#efdd99`, bg dark gold `#554600`, 1 px border `#736935`, radius 2). Bottom border `#3e381d`.
2. **Blinking**: gold dot and block cursors blink (≈1 s period, hard on/off). Several cursors can blink on one page (strip, column headers, panel headers).
3. **Column header band**: olive band `#241c05`, gold dot + spaced monospace title + block cursor; 1 px gold-brown bottom border.
4. **Tabs**: text-only monospace bold spaced; active = gold text + 3 px gold underline (`#dbc677`/`#f7e069`), optional dark-gold bg; inactive `#5a5749`.
5. **Corner brackets**: L-shaped arms 10 px, 2–3 px thick at panel corners (bright `#f3eaaa` on hero panels, subtle `#918a73` on chart panels).
6. **Chips / pills**: h 18–31, 1 px border, radius 2, monospace bold 10–11 px uppercase.
   Inactive: bg `#0e0705`, border `#282317`, text `#c0b9ad`. Active: bg dark gold `#39310f`, text `#f9e494`, (filter bar) 2 px bright-gold bottom border.
   Solid yellow pill for binary toggles (EN/FR, BARS/LINE, range buttons): bg `#fdd90b`, dark text.
7. **Ticker chip** (registry): `• JPM` bg `#483e01`, border `#6b5d1a`, text `#f3e4a5`; disabled variant grey.
8. **Tables / lists**: no zebra; rows separated by 1 px lines; hover = warm dark gold bg; selected = olive bg (+ optional 3 px gold left bar).
   Sticky column header and sticky group headers. Numeric columns right-aligned monospace. Empty value `—`.
9. **Signed numbers**: always with sign (`+2.34%`, `-0.18%`), 2 dp for %, coloured pos/neg; arrows ↑ cyan / ↓ red after values.
   Thousands separator `,`; K/M/T/B suffixes for big numbers; currency prefix (`$`, `C$`, `£`, `€`, `¥`).
10. **Footer** (31 px): `Terminal Data · HH:MM:SS` monospace 11 px `#51504a` left; `● LIVE` badge right (bg `#382e08`, text `#e8d491`, dot `#a79711`).
11. **Tooltips** (charts): small dark box (bg ≈`#0d0802`/`#151515`, 1 px border gold-brown `#5f5123` or grey), monospace 11 px, time dim + value white bold + change coloured;
    a vertical crosshair/hover band and a dot on the series.
12. **Charts**: area line charts with gradient fill to transparent, last-point dot, dashed "open"/"previous" reference line, right-side axis labels,
    vertical hourly gridlines only; bar charts in a yellow vertical gradient (`#ead16e`→`#8d7d31`) with grey negatives.
13. **Language toggle** EN/FR (Alfred's read, news reader) and `AUTO-TRANSLATED` caption.
14. **Alfred** (in-app AI) appears as "ALFRED'S READ" (Session Recap) and the Alfred side panel with glowing amber orb (News reader).
15. **Flags** 20x13 px rectangular before currency codes/session names; **bank logos** 18x18 square tiles.
16. **Scrollbars**: not visible in any frame (overlay/hidden scrollbars); scrolling is smooth (wheel steps animated over ≈150 ms).
17. **Time zone caption** `UTC-5` shown next to time-based widgets.

## Layout conventions

- Three-column pages: fixed left column 317–362 px, fixed right column 326–373 px, centre stretches. Column borders 1 px very dark (`#110f0b`–`#1f1e1b`).
- Outer page margin ≈9–14 px; inner panel padding ≈17–22 px; vertical gap between stacked blocks ≈9–12 px.
- Side columns stay fixed while the centre scrolls.
