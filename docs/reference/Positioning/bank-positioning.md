# Positioning › Bank Positioning

Reference video: `Reference/Positioning/bank-positioning.mp4` (≈23 s, frames every 0.5 s at 1600x966).
Shared style (panel, corner brackets, title with block cursor, pills, fonts): see `_shared-notes.md`.

## 1. Purpose

"BANK POSITIONING — RESEARCH DESK CALLS & FORECASTS": what the banks' research desks are calling, asset class by
asset class. The **DESK** view (default) shows six market cards (FX, ASIA & EM FX, COMMODITIES, INDICES, RATES,
CRYPTO), each counting the banks' current LONG / SHORT / RANGE calls, a three-colour proportion bar and coverage
("43 positions · 11 banks · 11/12 markets covered"). A second view **ALL CALLS** (not opened in the video) lists every
call. The **FX BANK FORECASTS** button opens a large overlay: per currency pair, a table of each bank's quarterly
price forecasts (Q4 '26 … Q4 '28) and a daily candle chart with one dashed horizontal line per quarter at the
average of the banks' forecasts. A header badge says the archive holds 677 bank notes and is "UPDATED WITH EVERY NEW
BANK NOTE". Data = banks' research notes (Danske Bank, MUFG, Nordea, UBS …), parsed by the app.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | DESK view. Cursor at the top edge of the RATES card (x≈810, y≈297): RATES card is in hover state (raised 3 px, brighter top border, arrow `↗` turns pale yellow). Title-bar block cursor visible. |
| 0.5 | Same; block cursor off (blink). |
| 1.0 | Cursor moves left (x≈700, y≈297), RATES still hovered; block cursor off. |
| 1.5 | Cursor at the gap between INDICES and RATES (x≈520): no card hovered (RATES drops back); block cursor on. |
| 2.0 | Cursor over the INDICES card top (x≈470, y≈297): hovered state does not show yet (transition). Block cursor off. |
| 2.5 | INDICES card hovered (raised 3 px). Block cursor on. |
| 3.0 | Cursor on the RATES card (x≈860, y≈327): RATES hovered. |
| 3.5–4.0 | Cursor near the RATES arrow `↗` (x≈1030–1045): RATES hovered, arrow pale yellow. |
| 4.5 | Cursor on CRYPTO bar (x≈1170, y≈425): CRYPTO hovered. |
| 5.0–6.5 | Cursor in the empty area under the cards: nothing hovered. Block cursor blinks (on at 5.5, 6.0, 6.5; off at 5.0). |
| 7.0 | Cursor on ASIA & EM FX range bar: ASIA & EM FX hovered. |
| 7.5 | Cursor on the `FX BANK FORECASTS` button (hover: the button gets a 1 px brighter border). |
| 8.0 | Click → **FX BANK FORECASTS overlay** open on EUR/USD. Forecast lines are being drawn: the dashed lines only reach x≈1030 and the green last-price label is still mid-way (x≈1035–1080): lines animate in from left to right (≈0.5 s). |
| 8.5–23.0 | Overlay idle; lines fully drawn to the right axis, last price label `1.1204` on the axis. The cursor wanders over the header (t 15–17), then the title "FX BANK FORECASTS" (t 20–22), the empty table area (t 22.5) and the bottom-left corner of the chart panel (t 23.0). No visible hover reaction anywhere in the overlay (the title has no hover state). |

## 3. Layout

### 3.1 DESK view (1600x966)

```
y=0   [🏛 ● BANK POSITIONING — RESEARCH DESK CALLS & FORECASTS ▌] [⟋ FX BANK FORECASTS] [▦ DESK|▤ ALL CALLS]   [• 677 NOTES ARCHIVED · UPDATED WITH EVERY NEW BANK NOTE]
y=44  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
y=59  Markets
y=86  ┌⌜ FX ───────────────────┐ ┌⌜ ASIA & EM FX ─────────┐ ┌⌜ COMMODITIES ───────────┐
      │ 13 LONG 13 SHORT 17 RANGE│ │ …                      │ │ …                       │   cards 519 x 190
      │ ██████████▓▓▓▓▓▓▓▓░░░░░░ │ │                        │ │                         │
y=276 └────────────────────────⌟┘ └────────────────────────⌟┘ └─────────────────────────⌟┘
y=289 ┌ INDICES ┐ ┌ RATES ┐ ┌ CRYPTO ┐  (same size)
y=481 └─────────┘ └───────┘ └────────┘
      (empty page background below, faint hexagon/grid texture)
```

| Region | x | y | w | h |
|---|---|---|---|---|
| Header bar | 0 | 0 | 1600 | 44 (bottom border 1 px `#4b482c`) |
| `FX BANK FORECASTS` button | 505 | 10 | 175 | 24 |
| DESK / ALL CALLS segmented | 701 | 10 | 178 | 24 |
| Notes badge | 1228 | 13 | 358 | 20 |
| `Markets` label | 10 | 59 | — | 13 |
| Card row 1 | x = 10, 540, 1070 | 86 | 519 | 190 |
| Card row 2 | x = 10, 540, 1070 | 289 | 519 | 192 |
| Gaps | 11 px horizontal, 13 px vertical | | | |

Cards are a 3-column grid with equal widths (stretch with the window); the rest of the page is empty (no scroll
needed with 6 cards).

### 3.2 FX BANK FORECASTS overlay

```
                                                                                                  [× CLOSE]  y=13–33
y=41 ┌⌜ FX BANK FORECASTS  INSTITUTIONAL PRICE PROJECTIONS      [EUR/USD][EUR/SEK][USD/THB]…[USD/VND] ┐
y=86 ├──────────────────────────────────────────┬───────────────────────────────────────────────────────┤
     │INSTITUTION  LAST REVISED Q4'26 Q1'27 …Q4'28│ 🇪🇺 EUR/USD 1.1204          [Q4'26][Q1'27]…[Q4'28]  │
     │🇩🇰 Danske Bank   OCT 12   —   —  1.1300 …   │ candles + dashed average lines + labels         │
     │ …                                          │ x axis Oct … Oct                                │
     │                                            │ LINES = AVERAGE OF THE BANKS' FORECASTS …        │
y=537└──────────────────────────────────────────┴───────────────────────────────────────────────────⌟┘
     (page below the overlay is a flat near-black backdrop #040302)
```

| Region | x | y | w | h |
|---|---|---|---|---|
| Close button | 1517 | 12 | 68 | 22 |
| Overlay panel | 14 | 41 | 1572 | 496 |
| Header row | 14 | 41 | 1572 | 45 (separator y≈86) |
| Forecast table (left) | 15 | 87 | 784 | 450 |
| Chart panel (right) | 800 | 87 | 785 | 450 (left border 1 px at x≈799) |
| Pair pills | 819 → 1575 | 51 | ≈52–64 each | 26 |
| Quarter pills | 1238 → 1575 | 94 | ≈52 each | 23 |
| Price chart plot | 810–1505 | 130–484 | | |
| Price axis | 1510–1560 | | | |
| Footnote | 810 | 518 | | 12 |

The overlay height (496) fits its content (4 banks); the empty area below is the backdrop. The table column takes
50 %, the chart 50 %.

## 4. Components

### 4.1 Header bar (DESK view)
- Bank-building icon `🏛` (outline, 14 px, gold `#dfd28a`), small dim gold dot (`#827e3b`, 5 px; presumably a status
  dot), title `BANK POSITIONING — RESEARCH DESK CALLS & FORECASTS` monospace 11 px uppercase, letter-spacing ≈4 px,
  `#615d54`, then a yellow block cursor `▌` (7x13 px, `#f5e692`) that **blinks** (≈1 s period; on/off seen across
  frames).
- `FX BANK FORECASTS` button: line-chart icon + bold 12 px uppercase, letter-spacing ≈2 px, text `#f6df97`, bg
  `#383004` (dark gold), 1 px border slightly lighter on hover, radius 2.
- Segmented control: `▦ DESK` (active: yellow bg `#fcd808`, dark text, bold) and `▤ ALL CALLS` (inactive: text
  `#979289`, transparent, outer border `#2b2a1d`). Icons are small grid/table glyphs.
- Notes badge (right): `• 677 NOTES ARCHIVED · UPDATED WITH EVERY NEW BANK NOTE`, bold 9.5 px uppercase,
  letter-spacing ≈1.5 px, text `#fae8a1`, bg `#483c00`-ish dark gold, 1 px gold border `#7a6a1a`, small dot at the
  left.
- Header background: dark gradient (`#181004` → `#100800`), 1 px bottom border `#4b482c`.

### 4.2 `Markets` label
Monospace 12 px, letter-spacing ≈2 px, cream `#e2d59c`, x=10, y=65 (sentence case).

### 4.3 Market card (×6)
- Box: bg `#0c0800` → transparent warm gradient (top-left slightly lighter), 1 px border `#433c2b` (top) /
  `#312e0d` (sides), corner brackets: top-left `⌜` and bottom-right `⌟`, 8 px arms, 1.5 px, gold `#5f5a3a` / `#332b03`.
- Icon tile 26x26 at (+20, +24), bg `#302800`, rounded 3, gold line icon (`#e2d28e`): FX = two coins, ASIA & EM FX
  = globe, COMMODITIES = droplets, INDICES = line chart, RATES = `%`, CRYPTO = `₿`.
- Title: proportional condensed bold (Rajdhani-like) 22 px, uppercase, letter-spacing 1 px, near-white `#fffef4`;
  subtitle under it: monospace 10 px uppercase, letter-spacing ≈3 px, `#57544a`.
- Top-right `↗` arrow icon, 12 px, grey `#686761` (pale yellow `#ecdda5` when the card is hovered).
- Three counters at x = +18, +182, +345 (each column ≈163 px): big number monospace-ish 28 px bold (`13`), colours
  LONG cyan `#22d3ee`-ish (rendered `#75d0d9`), SHORT red/coral `#f87171` (`#ea9794`), RANGE yellow `#facc15`
  (`#f5de6c`); label under it monospace 9 px uppercase letter-spaced `#59554d`: `LONG`, `SHORT`, `RANGE`.
- Proportion bar: x +18 → card right −18, 5 px high (y≈221–226), rounded ends, three segments without gaps in
  order cyan `#36dffa` (long) → yellow `#ffdf2e` (range) → red `#ff8e87` (short), widths ∝ counts.
- Footer line (monospace 12 px): `43 positions · 11 banks · 11/12 markets covered` — numbers bold white `#fefdf9`,
  words grey `#87847e`, `·` separators.
- **Hover**: the card moves up 3 px (translateY −3), the top border gets brighter (`#353622`), the arrow turns pale
  yellow; transition ≈150–250 ms (the frame right after the cursor enters still shows the rest state).

Card contents in the video:

| Card | Subtitle | LONG | SHORT | RANGE | Footer |
|---|---|---|---|---|---|
| FX | `G10 MAJORS, CROSSES & THE DOLLAR` | 13 | 13 | 17 | 43 positions · 11 banks · 11/12 markets covered |
| ASIA & EM FX | `ASIA, SCANDIES, CEE, LATAM` | 13 | 5 | 11 | 29 positions · 10 banks · 10/10 markets covered |
| COMMODITIES | `METALS, ENERGY` | 14 | 4 | 6 | 24 positions · 14 banks · 5/6 markets covered |
| INDICES | `US, EUROPE, ASIA` | 6 | 0 | 0 | 6 positions · 4 banks · 3/8 markets covered |
| RATES | `GOVERNMENT BOND YIELDS` | 1 | 6 | 1 | 8 positions · 3 banks · 5/6 markets covered |
| CRYPTO | `MAJORS` | 1 | 0 | 0 | 1 positions · 1 banks · 1/3 markets covered |

(positions = LONG + SHORT + RANGE; the plural is not adapted: "1 positions", "1 banks".)

### 4.4 FX BANK FORECASTS overlay
- Backdrop: flat `#040302` (page hidden). Close button top-right `× CLOSE` (mono 11 px uppercase letter-spaced
  `#81817d`, 1 px border `#2b2a1d`, radius 2).
- Panel: bg `#0c0601`, 1 px border `#45412f`, top-left corner bracket `⌜` and bottom-right `⌟` (gold).
- Title `FX BANK FORECASTS` bold condensed 20 px uppercase white `#fffdf8` + subtitle `INSTITUTIONAL PRICE
  PROJECTIONS` mono 10 px letter-spaced ≈3 px `#5b5750`.
- Pair pills (right of the header): `EUR/USD` (active: yellow bg `#fcd808`, dark bold text), `EUR/SEK`, `USD/THB`,
  `GBP/USD`, `USD/CHF`, `USD/IDR`, `USD/INR`, `USD/JPY`, `USD/MYR`, `USD/PHP`, `USD/VND` (inactive: text `#98958d`,
  1 px border `#2b2a1d`, mono 11 px). Order seems to be the order of coverage, not alphabetical.

**Forecast table** (left):
- Header (mono 9.5 px uppercase letter-spaced): `INSTITUTION` (x 25), `LAST REVISED` (right edge x 300), then six
  quarter columns, right-aligned (right edges x 382, 464, 546, 628, 710, 792): `Q4 '26` yellow `#eada9a`, `Q1 '27`
  cyan `#82c4ca`, `Q2 '27` lavender `#b7add1`, `Q3 '27` green `#75b087`, `Q4 '27` orange `#d1a781`, `Q4 '28` pink
  `#cda2b2` — each quarter has a fixed colour shared with the chart.
- Rows (pitch ≈35 px, 1 px separators `#1e1a0f`): flag (18x12) + bank name (proportional 12 px `#e0ddd8`), date
  `OCT 12` (mono 12 px uppercase `#8c8881`), forecast values mono 13 px white `#fffdfa`, 4 decimals (`1.1300`), `—`
  (dim `#5e5b53`) when the bank gave no forecast for that quarter.
- Rows seen (EUR/USD): Danske Bank (🇩🇰) OCT 12: —, —, 1.1300, 1.1200, 1.1200, —; MUFG (🇯🇵) OCT 07: 1.1200,
  1.2000, 1.2000, —, —, —; Nordea (🇫🇮) OCT 16: 1.1000, —, 1.0800, —, 1.1400, 1.2000; UBS (🇨🇭) OCT 19: 1.1600, —, —,
  —, —, —. Sorted alphabetically.

**Chart panel** (right):
- Header: flag + `EUR/USD` (bold 15 px white) + last price `1.1204` (mono 14 px grey `#97938d`). Right: quarter
  pills `Q4 '26` `Q1 '27` `Q2 '27` `Q3 '27` `Q4 '27` `Q4 '28`, filled with their quarter colour (yellow `#fcd400`,
  cyan `#20d8f0`, lavender `#b4a0f8`, green `#24c870`, orange `#fc9c24`, pink `#f48cc4`), dark text, mono 11 px; all
  "on" (they are toggles for the lines).
- Candles: ≈1 year of daily OHLC (x axis `Oct Dec Feb Apr Jun Aug Oct`, proportional 11 px `#8e8b85`), thin
  candles (≈2 px bodies) cyan up (`#22d3ee`-ish, rendered `#74aab0`) / red down (`#ef5f5f`, `#b07c77`). Right axis
  4 decimals (`1.2078` top = max, `1.2000 1.1800 1.1600 1.1400 1.1200`, `1.1060` bottom = min), mono 11 px
  `#84807a`. Faint horizontal grid `#151006`; bottom axis line `#3e3b1d`.
- Forecast lines: one dashed horizontal line per quarter (2 px, dash 6 / gap 4) in its quarter colour, across the
  whole plot, at the **average** of the banks' forecasts. Each has a filled label at the left end: `Q4 '28 1.2000`
  (pink), `Q2 '27 1.1367 · 3 banks` (lavender), `Q4 '27 1.1300 · 2 banks` (orange), `Q4 '26 1.1267 · 3 banks`
  (yellow), `Q3 '27 1.1200` (green). ` · N banks` only when N > 1. Labels are sans 11 px dark text on the colour, 125
  px wide box (text clipped: `3 banks` is cut by the box edge). Labels overlap when values are close (Q4 '27 /
  Q4 '26). The `Q1 '27` line (1.2000, 1 bank) is hidden under the Q4 '28 line at the same price.
- Last price: green dashed line at 1.1204 with label `1.1204` (white on green `#54bc80`) at the right axis.
- Footnote (mono 9 px uppercase letter-spaced `#555149`): `LINES = AVERAGE OF THE BANKS' FORECASTS PER QUARTER
  (CLICK A BANK FOR ITS OWN) · FORECASTS AS WRITTEN IN EACH BANK'S NOTE`.

## 5. Interactions

- **Card hover**: lift −3 px, brighter top border, arrow pale yellow (see 4.3). The whole card is clickable (arrow
  = "open"): opens the calls of that asset class (not shown).
- **Title cursor**: the `▌` after the title blinks (≈1 s period).
- **FX BANK FORECASTS button**: hover = brighter border; click opens the overlay on the first pair (EUR/USD).
- **DESK / ALL CALLS**: switches between the cards view and the full list of calls (ALL CALLS not shown).
- **Overlay**:
  - Opening animation: dashed forecast lines draw from left to right (≈0.5 s), the last-price label slides to the
    axis.
  - Pair pills: switch the pair (table + chart).
  - Quarter pills: show/hide each quarter's line.
  - Clicking a bank row: per the footnote, shows that bank's own forecasts instead of the averages.
  - `× CLOSE` closes the overlay (Escape presumably too).
  - No hover state on table rows or on the title.

## 6. Data

| Field | Sample | Format | Update | Source |
|---|---|---|---|---|
| Calls per asset class | 13 long / 13 short / 17 range | integer counts | on each new bank note | banks' research notes, parsed |
| Positions / banks / coverage | 43 positions · 11 banks · 11/12 markets covered | integers, `n/m` | per note | computed (markets with ≥1 call / markets in the class) |
| Notes archived | 677 | integer | per note | app archive |
| Bank forecasts | 1.1300 | 4 dp (pair precision) per quarter | per note | each bank's note (Danske Bank, MUFG, Nordea, UBS) |
| Last revised | OCT 12 | `MMM DD` uppercase | per note | note date |
| Average per quarter | 1.1367 · 3 banks | 4 dp + count | computed | mean of available forecasts |
| Price chart | daily OHLC ≈1 year | 4 dp | daily (last bar live?) | price feed |
| Last price | 1.1204 | 4 dp | live / delayed | price feed |

## 7. Key frames

| Frame | Shows |
|---|---|
| `t000.0.jpg` | DESK view, RATES hovered, cursor on |
| `t001.5.jpg` | DESK view, no hover |
| `t002.5.jpg` | INDICES hovered |
| `t004.5.jpg` | CRYPTO hovered |
| `t007.0.jpg` | ASIA & EM FX hovered |
| `t007.5.jpg` | FX BANK FORECASTS button hover |
| `t008.0.jpg` | Overlay opening (lines drawing in) |
| `t008.5.jpg` | Overlay complete |

## 8. Open questions

- Content of the ALL CALLS view and of a market card's detail (click on a card / the arrow).
- What a call is exactly (LONG/SHORT/RANGE per market per bank, horizon, entry/target?).
- Behaviour when a bank row is clicked in the overlay (own-forecast lines — colours, labels).
- Whether the overlay grows with more banks (the table area is mostly empty with 4 rows) and how it scrolls.
- Exact list and order of pairs for other cards (only FX forecasts exist?).
- Which sources the app will really use for bank notes (the video names Danske Bank, MUFG, Nordea, UBS); legal use
  of the notes.
- Meaning of the dim dot before the header title (status / live indicator?).
