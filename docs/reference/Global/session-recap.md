# Global › Session Recap

Source video: `Reference/Global/session-recap.mp4` (22 s, frames `t000.0`–`t021.5`, 1600x966).
All coordinates are at 1600x966, origin top-left of the page area (the app header is not in the video).

## 1. Purpose

A post-mortem of each trading session (Tokyo, London, New York). The left column is a registry of past
sessions grouped by day, each with its top mover. Selecting a session opens its "session file" in the
centre: an AI-written read of the session ("Alfred's read", EN/FR), a timeline chart of the session window
for the biggest movers with economic releases and newswire counts underneath, and a 4x3 grid of mini
charts ("Moves of the window"). The right column lists the economic releases and the wire headlines that
fell inside the session window.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–0.5 | Idle. New York session of Fri 23 Oct selected. Centre column scrolled to top: session title + Alfred's read. |
| 1.0–3.0 | Mouse-wheel scroll of the centre column only (5 steps, ~150 px each). Alfred's read scrolls away; Window timeline panel then Moves of the window come into view. Left and right columns do not move. |
| 3.0–11.0 | Idle on the scrolled view: WINDOW TIMELINE panel at y=64–477, MOVES OF THE WINDOW grid below (y=495–945). WTI Crude chip active, WTI card has the gold border. Small diffs at 4.5 and 8.5 = blinking header cursor/dot. |
| 11.0 | Hover in the chart at x≈864 (12:30): vertical crosshair line, dot on the line, tooltip `12:30 UTC-5  103.00  +1.68%`. |
| 11.5 | Click on chip "Gold -0.71%": the chart re-plots Gold (red line + red fill, max 4,047.1 / min 3,991.4, OPEN 4,042.3); the GOLD mini card gets the gold border, WTI card loses it. Chip "Gold" becomes the active chip. |
| 12.0–14.0 | Idle on Gold. |
| 14.5 | Hover at 12:00: tooltip `12:00 UTC-5  4,014.4  -0.69%` (red pct). |
| 15.0–18.5 | Idle (blinks). |
| 19.0–20.0 | Scroll the centre column back up to the top (3 steps). |
| 20.5–21.5 | Idle at top (blink at 21.0). |

## 3. Layout (1600x966)

```
y=0   +-----------------------------------------------------------------------------------------+
      | ✣ ● SESSION RECAP — POWERED BY ALFRED ▮                 [• TOKYO · LONDON · NEW YORK · UTC-5] |  page strip h=37
y=37  +-------------------+------------------------------------------------+--------------------+
      | ● RECAP REGISTRY ▮| SESSION FILE · NYC                             | RELEASES IN THE     |
      | SESSIONS | WEEKS  | 🇺🇸 NEW YORK SESSION                            | WINDOW        UTC-5 |
      | FRI 23 OCT        | Fri 23 Oct · 07:30-15:00 UTC-5  Breadth 4↑ 8↓ …| 07:30 🇨🇦 ◇ Core …   |
      | 🇺🇸 NEW YORK +2.31%| ════ gold gradient rule ═════════════          |   A 1.2% F 1.1% P …  |
      | 🇬🇧 LONDON  +11.83%| ┌ ALFRED'S READ ───────────── EN FR ┐          | … (6 rows)          |
      | 🇯🇵 TOKYO   -1.80% | │ headline / lead / 5 sections      │          | WIRE IN THE WINDOW  |
      | THU 22 OCT …      | └───────────────────────────────────┘          | 14:52 USD CFTC …    |
      |  (scroll list)    | ┌ WINDOW TIMELINE (chips, chart, lanes) ┐      | … (scroll list)     |
      |                   | MOVES OF THE WINDOW  4 x 3 mini cards           |                     |
y=966 +-------------------+------------------------------------------------+--------------------+
      x=0            x=317 x=338                                     x=1205 x=1227          x=1600
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Page strip | 0 | 0 | 1600 | 37 | bg `#120d04`→`#1a1305`, bottom border 1–2 px `#3e381d`. |
| Left column (registry) | 0 | 37 | 317 | 929 | bg `#050402`; right border 1 px `#110f0b`. Fixed width. List scrolls vertically. |
| Registry header | 0 | 40 | 317 | 30 | bg `#241c05` (dark olive band). |
| Tabs SESSIONS / WEEKS | 0 | 75 | 317 | 30 | Active underline x=12–148, y=100–102. |
| Session list | 0 | 108 | 317 | to bottom | Date headers h≈22, session rows h≈51. |
| Centre column | 338 | 37 | 867 | 929 | Scrolls vertically (only this column scrolls in the video). 21 px left gutter after the left border, 21 px right gutter before the right column. Stretches with the window. |
| Session title block | 338 | 55 | 867 | 105 | Ends with gold rule at y=156–158. |
| Alfred's read card | 338 | 173 | 867 | 624 | 1 px border `#4d4a39` (lighter top/left). |
| Window timeline panel | 338 | ≈810 doc (64 after scroll) | 867 | 413 | Corner brackets. |
| Moves of the window | 338 | ≈1241 doc (495 after scroll) | 867 | ≈450 | 4 columns x 3 rows. |
| Right column | 1227 | 37 | 373 | 929 | Left border 1 px `#181714`. Fixed width. |
| Releases list | 1227 | 40 | 373 | ≈345 | Header h≈32, 6 rows h≈52. |
| Wire list | 1227 | ≈385 | 373 | to bottom | Header h≈35, rows h≈33. Scrolls. |

Scroll offset seen: centre scrolled by ≈746 px between t=1.0 and t=3.0.

## 4. Components

### 4.1 Page strip (y=0–37)
- Left, from x=12: a small 4-point star/compass icon "✣" (dim `#5e5a50`), a gold dot (8 px, `#9d9457`, blinks: dims every ~2 s), then the
  label `SESSION RECAP — POWERED BY ALFRED` in monospace 11 px uppercase, letter-spacing ≈3 px, colour `#5e5a50`.
  Followed by a blinking block cursor 9x14 px `#f5e392` at x≈330.
- Right, x=1355–1585, y=9–28: badge `• TOKYO · LONDON · NEW YORK · UTC-5` — monospace bold 10 px, spaced ≈2 px,
  text `#efdd99`, bg dark gold `#554600`→`#342100`, 1 px border `#736935`, radius 2.

### 4.2 Left column: RECAP REGISTRY
- Header band y=40–70, bg `#241c05`: gold dot + `RECAP REGISTRY` monospace 11 px spaced 3 px `#645e4d`, plus blinking block cursor.
- Tabs at y≈88 (text centre): `SESSIONS` (active: monospace bold 12 px, spaced 3 px, `#e6dfbc`, underline 3 px `#dbc677` from x=12 to 148)
  and `WEEKS` (inactive `#5a5749`, x≈214). No visible tab background.
- Date group header: e.g. `FRI 23 OCT`, `THU 22 OCT`, `WED 21 OCT`, `TUE 20 OCT`, `MON 19 OCT` — monospace 10 px uppercase,
  spaced 2 px, `#504e46`, x=8, row h≈22, bg `#0d0b08`.
- Session row (h≈51, x=0–316):
  - Flag 20x13 px at x=10, vertically centred on line 1 (US, UK, JP flags).
  - Line 1 (y+12): session name bold, condensed sans 13 px uppercase, white `#fefae8`, x=37: `NEW YORK`, `LONDON`, `TOKYO`.
  - Line 2 (y+30): monospace 10 px `#5f5631` (olive dim): `07:30-15:00 UTC-5 · WTI Crude` = session window + `·` + top mover name.
  - Right, x≈262–308, vertically centred: top mover change, monospace 13 px semibold with sign and 2 decimals + `%`:
    positive cyan `#7eb8ab`, negative red `#b28888`.
  - Selected row: full-width bg `#433805` (olive gold). Hover state not shown.
  - Separator lines 1 px `#110f06` between rows.
- Sessions visible (all with UTC-5 windows; NY 07:30-15:00, LDN 02:00-10:30, TKY 19:00-01:00):

| Day | Session | Top mover | Change |
|---|---|---|---|
| Fri 23 Oct | New York | WTI Crude | +2.31% |
| Fri 23 Oct | London | Natural Gas | +11.83% |
| Fri 23 Oct | Tokyo | Natural Gas | -1.80% |
| Thu 22 Oct | New York / London / Tokyo | Silver | -2.23% / -1.65% / -2.14% |
| Wed 21 Oct | New York / London / Tokyo | Natural Gas / Natural Gas / Silver | -2.90% / -4.57% / -0.80% |
| Tue 20 Oct | New York / London / Tokyo | Ethereum / Ethereum / WTI Crude | +1.91% / +1.42% / +1.33% |
| Mon 19 Oct | New York / London / … | Natural Gas / Gold | -3.64% / -0.86% |

Note: the list value for NY Fri is +2.31% while the session file shows WTI +2.34% (list value probably computed on a slightly different bar/close).

### 4.3 Centre: session title block (scroll position 0)
- `SESSION FILE · NYC` at x=337, y≈60–70: monospace 10 px spaced 3 px `#585855`.
- Flag (US, ≈26x17) at x≈345, then `NEW YORK SESSION`: bold geometric/condensed sans ≈32 px, uppercase, letter-spacing ≈2 px, white `#fbfbf8`, y≈82–112.
- Meta line y≈126–139, monospace 12 px:
  `Fri 23 Oct · 07:30-15:00 UTC-5` (`#84837f`) · `Breadth` (`#848480`) `4↑` (cyan `#7bb9be`) `8↓` (red `#bf8d8f`) ·
  `6 releases` · `98 headlines` (`#84837f`) and a pill `CLOSED` (x=892–944, y=123–141, bg `#181818`, 1 px border `#3d3d3a`,
  text monospace 10 px spaced `#595a56`). Breadth = count of the 12 tracked instruments up vs down over the window (4 up, 8 down matches the 12 cards).
- Gold rule y=156–158, x=338→1205: horizontal gradient from `#cdc392` (left) fading to transparent by x≈1050.

### 4.4 Alfred's read card (y=173–797)
- Card: bg `#0a0a0a`, 1 px border `#4d4a39`, square corners. Padding ≈22 px.
- Header row y≈201: `✣ ALFRED'S READ` monospace 11 px spaced 3 px `#4f4c3f` (the ✣ in gold-brown `#4a2f00`).
  Right side: `WRITTEN AT THE CLOSE · FROM THE DATA BELOW` monospace 9 px spaced, dim, then language toggle:
  `EN` active pill (x≈1126–1155, bg solid yellow `#fed41c`, black bold text) and `FR` inactive (text `#79776b`, no bg).
- Headline y≈228–250: `US Equities Mixed; Oil Surges on Geopolitical Remarks` — semibold sans ≈24 px, white.
- Lead paragraph ≈15 px, line-height ≈26 px, colour `#a6a59f`, max width ≈820 px:
  "The S&P 500 and Dow Jones declined while the Nasdaq 100 edged higher, with mixed signals from US manufacturing and services PMI releases. Oil jumped 2.34%, natural gas fell 2.15%, gold retreated 0.71%, and a headline reported speculative traders most bearish on the euro since 2020."
- Five sections, each a 2-column row separated by 1 px rules `#141410`:
  - Left column (x=360–530): section label in monospace bold 11 px spaced 3 px, gold-brown (`INDICES`, `METALS`, `FOREX`, `CRYPTO`, `ENERGY`),
    then one line per instrument in monospace 12 px: name `#828278`, change coloured (cyan positive / red negative), e.g. `S&P 500 -0.18%`.
  - Right column (x=542–1180): explanatory text ≈14 px `#b4afa6`, line-height ≈22.
  - INDICES: S&P 500 -0.18%, Nasdaq 100 +0.11%, Dow Jones -0.43% — "The S&P 500 fell 0.18%, the Dow Jones dropped 0.43%, and the Nasdaq 100 rose 0.11%. US S&P Global Manufacturing PMI came in at 56.0, above the 55.4 forecast, while Composite PMI matched 57.6 and Services PMI matched 57.9, both lower than prior week."
  - METALS: Gold -0.71%, Silver +0.28% — "Gold declined 0.71% and silver rose 0.28%."
  - FOREX: EUR/USD -0.01%, USD/JPY +0.08%, EUR/GBP -0.02% — "EUR/USD fell 0.01%, EUR/GBP eased 0.02%, and USD/JPY gained 0.08%."
  - CRYPTO: Bitcoin -0.54%, Ethereum -0.24% — "Bitcoin fell 0.54% and Ethereum declined 0.24%."
  - ENERGY: WTI Crude +2.34%, Natural Gas -2.15% — "WTI crude oil rose 2.34% and natural gas fell 2.15%."

### 4.5 Window timeline panel (viewport y=64–477 after scroll; x=338–1205)
- Panel bg `#0d0802` (very dark warm), 1 px border `#45412f` (left) / `#201a08` (top) — i.e. a faint border; L-shaped corner brackets
  10 px arms, 2 px thick, `#918a73` at top-left; matching brackets at the other corners (bottom-right is fainter `#190f00`).
- Row 1 (y≈80–98): `WINDOW TIMELINE` monospace 11 px spaced 3 px `#5b5750` at x=351; then 6 chips (h≈18, gap ≈6, 1 px border `#2a2a22`,
  monospace 11 px): `WTI Crude +2.34%`, `Natural Gas -2.15%`, `Gold -0.71%`, `Bitcoin -0.54%`, `Dow Jones -0.43%`, `Silver +0.28%`
  — the six biggest absolute movers, sorted by |change|. Name `#807d76`, change cyan/red. Active chip: bg `#39340b`, name text cream `#e3d8a5`.
- Row 2 (y≈108, right-aligned): legend `◆ high impact` (yellow diamond `#e8da9b`), `◆ medium` (grey diamond `#585451`),
  `▮ headline` (white tick `#dfdcda`), `UTC-5` — monospace 10 px `#535048`.
- Price chart (plot x≈353–1136, y≈135–362):
  - Series = 15-minute bars of the selected instrument across the session window (07:30→15:00 = 30 bars), plotted as a line of closes.
  - Line 1.5 px: positive instrument cyan `#1bdbeb` (core) / `#75c2cb`; negative instrument red `#fd7f83`.
    Colour follows the sign of the window change (WTI +2.34% cyan, Gold -0.71% red).
  - Area fill under the line to the plot bottom: vertical gradient from `#0f3131` (cyan case; red case dark maroon) to transparent.
  - Last point: filled dot r≈5 in the line colour.
  - Vertical gridlines at each full hour, 1 px `#1e1912`; no horizontal gridlines.
  - Right axis labels (x≈1142, monospace 11 px `#7b7972`): window max at the top (`103.86`, Gold `4,047.1`) and window min at the bottom (`101.20`, Gold `3,991.4`). Thousands separator `,`; decimals follow the instrument.
  - Open line: dashed horizontal line 1 px `#414441` at the window open price, with label `OPEN 101.30` (Gold: `OPEN 4,042.3`) at the left, monospace 10 px spaced `#525a53`.
- Lanes under the chart (full plot width):
  - `RELEASES` lane (y≈403, label right-aligned at x≈1143, monospace 10 px spaced `#4e4a42`): one diamond per release time
    (12 px, medium = grey `#52565d`, high = yellow), with a small count label (`3`) when several releases share the time (07:30 has 3, 08:45 has 3).
  - `WIRE` lane (y≈420–445): one white vertical tick per 15-min bucket that contains headlines (`#a2a1a1`, 3 px wide, height ≈10–18 px
    scaling with count) with the count above in monospace 9 px `#726f68`: 2, 2, 6, 5, 7, 5, 20, 3, 2, 3, 4, 3, 2, 10, 17.
  - Time axis labels (y≈458): `08:00 09:00 10:00 11:00 12:00 13:00 14:00 15:00`, monospace 10 px `#504c45`, centred under hour gridlines (≈104.5 px per hour).
- Hover: vertical crosshair 1 px light grey through chart and lanes, a small dot on the series at the hovered bar, and a tooltip
  box anchored at the top of the plot just right of the crosshair (≈185x23 px, bg `#151515` ~90 % opacity, 1 px border `#2a2a2a`):
  `12:30 UTC-5` (monospace 11 px `#7b7972`), value (`103.00`, white bold), change since window open (`+1.68%` cyan / `-0.69%` red).

### 4.6 MOVES OF THE WINDOW (viewport y=495–945 after scroll)
- Title row y≈495: `MOVES OF THE WINDOW` monospace 11 px spaced 3 px `#595953` + subtitle `15-min bars · open ⟶ close · click to trace`
  monospace 10 px `#51514e`.
- Grid 4 columns x 3 rows: card 210x137 px, horizontal gap 8 px (x = 338, 556, 774, 992), vertical gap ≈9 px (rows at ≈515, 661, 808).
- Card: bg `#0d0802`, 1 px border `#3f392a`; selected (traced) card border 1 px gold `#d2c38d` (WTI at start, Gold after click).
  - Top-left (x+10, y+17): instrument name uppercase, condensed bold 12 px, white `#fefdf8` (`S&P 500`, `GOLD`, `EUR/USD`, `BITCOIN`, …).
  - Top-right: change, monospace bold 14 px, sign + 2 dp + `%`, cyan `#89cbcf` / red `#d69997`.
  - Sparkline area (≈190x70): line in cyan `#3ed3e0` or red `#e08c8b` (colour by sign), area fill to bottom (dark teal / dark red gradient),
    a dotted horizontal line at the open price (`#3a3a3a`), end dot r≈3 (`#fe8d8f` red / cyan).
  - Bottom row (y+113): `O 7,676.3` left (label `O` and value `#545049`) and `C 7,662.5` right (`#7d7a74`, slightly brighter), monospace 11 px.
- Cards (order: row by row):

| Row | Col 1 | Col 2 | Col 3 | Col 4 |
|---|---|---|---|---|
| 1 | S&P 500 -0.18% (O 7,676.3 C 7,662.5) | GOLD -0.71% (O 4,042.3 C 4,013.8) | EUR/USD -0.01% (O 1.1199 C 1.1198) | BITCOIN -0.54% (O 81,610.8 C 81,166.5) |
| 2 | NASDAQ 100 +0.11% (O 29,084.0 C 29,116.5) | SILVER +0.28% (O 53.88 C 54.03) | USD/JPY +0.08% (O 156.71 C 156.83) | ETHEREUM -0.24% (O 2,397.6 C 2,391.7) |
| 3 | DOW JONES -0.43% (O 52,649.0 C 52,423.0) | WTI CRUDE +2.34% (O 101.30 C 103.67) | EUR/GBP -0.02% (O 0.8372 C 0.8371) | NATURAL GAS -2.15% (O 3.119 C 3.052) |

Column order is by asset family: indices / metals+energy / FX / crypto.

### 4.7 Right column: RELEASES IN THE WINDOW
- Header (y≈45–70): `RELEASES IN THE WINDOW` monospace 11 px spaced 4 px `#5b5750`; `UTC-5` right-aligned. Bottom border 1 px gold-brown `#3a3214`.
- Row (h≈52, separators 1 px `#14120c`):
  - Line 1: time `07:30` monospace 12 px `#5f5d57` at x≈1238; flag 20x13 at x≈1277; impact marker (small hollow circle/diamond, grey for medium) at x≈1305;
    event name ≈15 px regular white-ish `#e9e6d6` at x≈1316.
  - Line 2 (x≈1306): `A 1.2%  F 1.1%  P -0.7%` monospace 12 px: letters `A`/`F`/`P` dim `#4e4a42`, actual bold white, forecast and previous `#6a675f`. Missing values shown as `—`.
- Rows: 07:30 CA Core Retail Sales A 1.2% F 1.1% P -0.7% · 07:30 CA Retail Sales A 1.2% F 1.3% P -0.7% · 07:31 CA Retail Sales A 0.7% F — P 1.2% · 08:45 US S&P Global Manufacturing PMI A 56.0 F 55.4 P 55.9 · 08:45 US S&P Global Composite PMI A 57.6 F 57.6 P 58.4 · 08:45 US S&P Global Services PMI A 57.9 F 57.9 P 58.8.

### 4.8 Right column: WIRE IN THE WINDOW
- Header like 4.7: `WIRE IN THE WINDOW`, band bg `#0d0b08`, bottom border gold-brown.
- Row (h≈33): time monospace 12 px `#5f5d57` + headline ≈14 px `#e9e6d6` single line (ellipsis when too long). Newest first.
  Optional tag chip after the time (seen: `WTI` chip, dark bg with border) when the headline maps to a tracked instrument.
- Rows seen: `14:52 USD CFTC Positions Week of October 20th - FJElite`, same for EUR, AUD (14:52), JPY, GBP, CHF, NZD, CAD, ZN, ZT, VIX, NQ, ES, WTI, Copper, Gold (14:51).

## 5. Interactions

- Select a session row (left) → loads that session file in the centre (only the NY Fri row was selected in the video).
- SESSIONS / WEEKS tabs: WEEKS presumably shows week recaps (not shown).
- EN / FR toggle on Alfred's read: switches the language of the written read (only EN shown).
- Mouse wheel over the centre column scrolls it; side columns stay put.
- Chips in the timeline: click selects the traced instrument (re-plots chart, axis labels, OPEN line, colour). Clicking a mini card does the same
  ("click to trace"); the traced card has the gold border. Change is instant (no animation seen).
- Hover on the chart: crosshair + dot + tooltip, snapped to the 15-min bar under the cursor.
- Blinking: header gold dot and block cursors blink (period ≈1–2 s).

## 6. Data

| Field | Sample values | Format | Update | Source on screen |
|---|---|---|---|---|
| Session list (day, session, window, top mover, change) | NEW YORK 07:30-15:00 UTC-5 · WTI Crude +2.31% | `HH:MM-HH:MM UTC±N`, % 2 dp signed | per session close | none |
| Session meta: breadth, releases count, headlines count, status | Breadth 4↑ 8↓, 6 releases, 98 headlines, CLOSED | ints | at close / live while open | none |
| Alfred's read (headline, lead, per-section text) | see 4.4 | prose EN/FR | written once at close ("WRITTEN AT THE CLOSE · FROM THE DATA BELOW") | "ALFRED" (in-app AI) |
| 12 tracked instruments, 15-min bars over the window | WTI 101.30→103.67 | price, instrument decimals, `,` thousands | at close (live during session probably) | none |
| Window change per instrument | +2.34% | (close/open-1)·100, 2 dp | computed | — |
| Timeline max/min/open | 103.86 / 101.20 / OPEN 101.30 | instrument decimals | computed | — |
| Tooltip change | +1.68% at 12:30 | vs window open | computed | — |
| Releases in window | CA Core Retail Sales A 1.2% F 1.1% P -0.7% | A/F/P | as released | (calendar feed) |
| Release impact | medium (grey) / high (yellow) | diamond | — | — |
| Wire headlines in window + 15-min counts | 98 headlines; counts 2…20 | time + text | live | wire source (FJElite appears as author in headlines) |

Instruments: S&P 500, Nasdaq 100, Dow Jones, Gold, Silver, WTI Crude, Natural Gas, EUR/USD, USD/JPY, EUR/GBP, Bitcoin, Ethereum.

## 7. Key frames

- `t000.0.jpg` — top of the session file: title, meta, Alfred's read, right column releases + wire.
- `t003.0.jpg` — scrolled: Window timeline (WTI traced) + Moves of the window grid.
- `t011.0.jpg` — chart hover tooltip on WTI (12:30, 103.00, +1.68%).
- `t011.5.jpg` / `t012.0.jpg` — after clicking the Gold chip (red series, GOLD card selected).
- `t014.5.jpg` — Gold hover tooltip (12:00, 4,014.4, -0.69%).
- `t020.5.jpg` — back to top.

## 8. Open questions

- WEEKS tab content and the exact look of a week recap.
- What the session file looks like for an open (live) session (status pill other than `CLOSED`, live chart updates).
- High-impact release diamonds (yellow) are in the legend but none appear in this window.
- Rule for the top mover in the left list (+2.31% vs +2.34% in the file — different bar or rounding?).
- Whether the right column wire list scrolls independently and how many items it holds; what the `WTI` tag chip means exactly (instrument mapping?).
- Alfred's read in FR; how the text is generated (local model, prompt) is outside the video.
- The timeline chips show only six movers; whether the other six can be traced only via the cards (yes per "click to trace", not shown).
