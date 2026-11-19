# News › Calendar

Source video: `Reference/News/calendar.mp4` (20 s, frames `t000.0`–`t020.0`, 1600x966).
Coordinates at 1600x966, origin top-left of the page area.

## 1. Purpose

The economic calendar for one week: every release with time, currency, impact, actual, forecast (consensus),
previous, surprise (actual − forecast) and the economists' range (min/max). Clicking a value opens a right-hand
detail panel with the economists' range bar, individual bank forecasts (e.g. Danske Bank), and the sources.
Clicking an event name expands the row inline with a description of the indicator and a long release-history
bar chart (since 2006).

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–0.5 | Idle, full-width table. Cursor on the `Core CPI` (ANNUAL) row → row hover bg. |
| 1.0 | Cursor moves right over the `CPI` MONTHLY row (hover follows). |
| 1.5 | Click on the MIN cell (`3.0%`) of the `CPI` ANNUAL row (Mon 07:30 CAD). The detail panel slides in from the right (at x=1158 in this frame). Table reflows to x=0–1147, event names get truncated with `...`. |
| 2.0 | Panel settled at x=1148 (slide ≈ 250–300 ms). |
| 2.5–12.5 | Cursor wanders over the panel: bank bar (4.0–5.0), RANGE box, legend; at 10.5 the source link `Danske Bank — Market Movers - 19-23 October 2026` is hovered and turns gold. No tooltip appears. |
| 13.0 | Panel closed (closing action not visible; probably click on the same cell again / outside / Esc). Table back to full width. The MIN cell of the CPI ANNUAL row keeps a 1 px gold focus outline (x=1363–1476, y=542–567). |
| 13.5–14.0 | Hover on `Core CPI` MONTHLY row. |
| 14.5 | Click on the event name/tag of the `CPI` ANNUAL row → row expands inline: description + `CANADA · RELEASE HISTORY` chart; chevron flips to ⌃. |
| 15.0–16.5 | Idle on expanded row. |
| 17.0–19.0 | Hover over the chart, then mouse-wheel scroll of the table body by ≈340 px in 3 steps. Column header row and the current day group header stay pinned (sticky). The chart tooltip shows `01 Jan 2014 ■ CPI 1.484%` with a highlighted bar and a vertical hover band. |
| 19.5–20.0 | Idle, scrolled. |

## 3. Layout (1600x966)

```
y=0    +------------------------------------------------------------------------------------------+
y=7    | 🔍 Search event, country, or currency...                                                  |  search h=29
y=50   | ‹  Oct 18 - Oct 24, 2026  ›                                                               |
y=82   | COUNTRIES [🇺🇸USD][🇪🇺EUR][🇬🇧GBP][🇯🇵JPY][🇨🇭CHF][🇨🇦CAD][🇦🇺AUD][🇳🇿NZD] [⊘ More... ⌄]         |
y=117  | IMPACT  (All 168) High 20  Medium 42  Low 106                                             |
y=147  | TIME  CURRENCY  EVENT                ACTUAL  FORECAST  PREVIOUS  SURPRISE   MIN    MAX     |  sticky header
y=177  | Sunday, October 18                                                                        |  sticky group header
       | 18:05 🇦🇺 AUD ● RBA Assist Gov Kent Speaks ⌄      —       —        —        —       —     —  |  rows h=37
       | ...                                                                                       |
y=935  | Terminal Data · 23:24:20                                                          [● LIVE] |  footer
y=966  +------------------------------------------------------------------------------------------+
```

With the detail panel open: table x=0–1147, panel x=1148–1600 (452 px wide, full page height y=0–966).

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Search box | 9 | 7 | 1581 | 29 | bg `#140f05`, 1 px border `#342e22`. |
| Week navigator | 9 | 50 | ≈210 | 18 | |
| Countries row | 9 | 82 | — | 20 | |
| Impact row | 9 | 117 | — | 19 | |
| Separators | 0 | 43 / 75 / 108 / 143 / 175 | full | 1–2 | `#2d281b` (filter blocks are separated by full-width lines). |
| Column header | 0 | 147 | full | 26 | bg `#100a04`. Sticky. |
| Table body | 0 | 177 | full | to 934 | Scrolls; day group headers sticky under the column header. |
| Footer | 0 | 935 | 1600 | 31 | top border 1 px `#1e1f06`. |
| Detail panel | 1148 | 0 | 452 | 966 | Overlays the right part; table width shrinks (reflow, not overlay). |

Column right edges (numeric columns are right-aligned):

| Column | Full width (x) | Panel open (x) |
|---|---|---|
| TIME | left 10 | left 10 |
| CURRENCY | left 103 (flag), code at 130 | same |
| EVENT | left 228 | left 228 (names truncated with `...`) |
| ACTUAL | right 997 | right 543 |
| FORECAST | right 1123 | right 670 |
| PREVIOUS | right 1247 | right 795 |
| SURPRISE | right 1358 | right 906 |
| MIN | right 1474 | right 1021 |
| MAX | right 1590 | right 1137 |

## 4. Components

### 4.1 Search box
- Magnifier icon (dim) at x≈20, placeholder `Search event, country, or currency...` 13 px `#444038`. Full width.

### 4.2 Week navigator
- `‹` and `›` chevrons (`#76766e`, 8x9 px) around `Oct 18 - Oct 24, 2026` monospace 13 px semibold `#b8b3aa`. Week = Sunday→Saturday.

### 4.3 COUNTRIES chips
- Label `COUNTRIES` monospace 9 px spaced 2 px `#534f47`.
- Chips (h≈20, gap ≈6): flag 16x11 + code monospace bold 11 px `#e2decb`; bg `#120705`, 1 px border gold-brown `#4f4024`, radius 2.
  Order: USD, EUR, GBP, JPY, CHF, CAD, AUD, NZD. All shown in the selected (bordered) state.
- `⊘ More... ⌄` chip: grey text `#6d6a60`, no gold border — opens a dropdown with more countries (not shown).

### 4.4 IMPACT segmented filter
- Label `IMPACT` monospace 9 px spaced `#535048`.
- Options: `All 168` (active: bg `#343027`, label white `#e1e0d5`, count dim `#5a574e`), `High 20`, `Medium 42`, `Low 106`
  (inactive: label `#6b685f`, count dimmer). Count = number of events of that impact in the week.

### 4.5 Column header
- `TIME`, `CURRENCY`, `EVENT`, `ACTUAL`, `FORECAST`, `PREVIOUS`, `SURPRISE`, `MIN`, `MAX` — monospace 9 px uppercase, spaced 1.5 px, `#7c786f`.

### 4.6 Day group header
- `Sunday, October 18`, `Monday, October 19`, `Tuesday, October 20` — bold 11 px `#b4afa6`, bg `#110f08`, h≈26, followed by a 4 px
  darker gap band (`#070301`, top line `#2f2821`).

### 4.7 Event row (h≈37, separators 1 px `#20210a`)
- Row bg `#040302`; hover bg `#251f0b` (full width).
- TIME: `18:05` monospace 12 px `#7c7971`.
- CURRENCY: flag 20x13 at x=103 + code monospace bold 12 px `#b2ada0`.
- EVENT: impact dots at x=228 — one dot (6 px) per impact level: low = 1 grey dot `#5d5c56`; medium = 2 dots; high = 3 dots?
  (the CAD CPI rows show 2 yellow dots `#f4da29`; speeches/auctions show 1 grey dot). Then the event name 13 px semibold `#d3d2cb`.
  Optional period tag after the name: `ANNUAL` / `MONTHLY` in monospace 9 px spaced, cyan `#87c3ca`, followed by `inferred` in dim `#474131`
  (tag box x≈307–397, h 14, bg dark teal `#003029`-ish). Trailing chevron `⌄` `#555451` (rotates to `⌃` when expanded).
- ACTUAL: monospace 13 px bold white `#fbfaf3`, plus an arrow just right of the value: `↑` cyan `#73acb0` if actual > previous, `↓` red `#a07978` if actual < previous.
  Empty values = `—` (white for ACTUAL, `#4c4b46` for other columns).
- FORECAST: monospace 13 px `#b4afa4`. PREVIOUS: `#7f7c75`.
- SURPRISE = actual − forecast, signed, same unit: negative red `#bb7373` (`-0.2%`), positive cyan `#8abfc5` (`+10.9K`), `—` when no forecast.
- MIN / MAX: monospace 13 px yellow `#f5dea3` with a 1–2 px dotted underline `#bab093` (clickable → opens the detail panel). `—` when no survey range.

Rows visible (week Oct 18–24 2026; times in the user's zone):

| Day | Time | Ccy | Impact | Event | Tag | Actual | Fcst | Prev | Surprise | Min | Max |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Sun 18 | 18:05 | AUD | low | RBA Assist Gov Kent Speaks | | — | — | — | — | — | — |
| Sun 18 | 20:30 | EUR | low | ECB's Lane Speaks | | — | — | — | — | — | — |
| Sun 18 | 23:30 | JPY | low | Tertiary Industry Activity Index | | -4.10↓ | — | 0.70 | — | — | — |
| Mon 19 | 01:00 | EUR | low | ECB's Elderson Speaks | | — | — | — | — | — | — |
| Mon 19 | 07:30 | CAD | med | Common CPI | | 2.4%↓ | 2.6% | 2.6% | -0.2% | — | — |
| Mon 19 | 07:30 | CAD | med | Core CPI | ANNUAL inferred | 2.2%↓ | — | 2.4% | — | — | — |
| Mon 19 | 07:30 | CAD | med | Core CPI | MONTHLY inferred | 0.0%↓ | — | 0.1% | — | — | — |
| Mon 19 | 07:30 | CAD | med | CPI | MONTHLY inferred | 0.1%↑ | 0.2% | -0.1% | -0.1% | 0.0% | 0.4% |
| Mon 19 | 07:30 | CAD | med | CPI | ANNUAL inferred | 3.1%↑ | 3.2% | 3.0% | -0.1% | 3.0% | 3.3% |
| Mon 19 | 07:30 | CAD | med | Median CPI | | 1.9%↓ | 2.0% | 2.0% | -0.1% | — | — |
| Mon 19 | 07:30 | CAD | med | Trimmed CPI | | 1.8%↓ | 1.9% | 1.9% | -0.1% | — | — |
| Mon 19 | 09:00 | USD | low | NAHB Housing Market Index | | 29↓ | 31 | 32 | -2 | 30 | 32 |
| Mon 19 | 10:30 | USD | low | 3-Month Bill Auction | | 4.095%↑ | — | 4.085% | — | — | — |
| Mon 19 | 10:30 | USD | low | 6-Month Bill Auction | | 4.220%↑ | — | 4.200% | — | — | — |
| Mon 19 | 10:30 | CAD | low | BoC Business Outlook Survey | | — | — | — | — | — | — |
| Tue 20 | 01:00 | GBP | low | Average Earnings ex Bonus | | 3.7%↑ | 3.6% | 3.6% | +0.1% | 3.3% | 3.7% |
| Tue 20 | 01:00 | GBP | low | Average Earnings Index +Bonus | | 3.7%↓ | 3.8% | 3.9% | -0.1% | 3.7% | 4.0% |
| Tue 20 | 01:00 | GBP | low | Claimant Count Change | | 30.3K↑ | 19.4K | 7.0K | +10.9K | — | — |

(The "inferred" tag marks rows whose period — annual vs monthly — the app inferred because the feed gives two events with the same name.)

### 4.8 Footer (y=935–966)
- Left `Terminal Data · 23:24:20` monospace 11 px `#51504a` (last data refresh time, HH:MM:SS).
- Right `● LIVE` badge (x=1544–1594, y=942–962): bg `#382e08`, text monospace bold 10 px `#e8d491`, dot `#a79711`.

### 4.9 Detail panel (x=1148–1600, opened from MIN/MAX cell)
- Panel bg `#0a0905` (slightly lighter than the table), left border 1 px `#242017`. Padding 17 px.
- `Mon 19 Oct, 07:30` monospace 12 px `#817f77` at y≈9–21 (x≈1188, after a small icon).
- **ECONOMISTS' RANGE** (y≈56): monospace bold 11 px spaced 2 px, yellow `#e1d6a5`; right-aligned `3.0% → 3.3%` monospace bold 14 px white.
- Range bar (y≈90–95, x≈1324–1494, h≈4): gradient red `#c46757` → orange `#ce9239` → yellow `#d8af1c` → green `#89ae6c` → teal `#22a5b2`
  (low→high), on a track `#21201a` that spans the full panel width. Markers (vertical, ≈30 px tall, y≈78–108):
  dashed grey line = previous (`#a3a19b`, at 3.0%), white solid = actual (`#fdfcf9`, 3.1%), yellow solid = consensus (`#eedd9b`, 3.2%).
  Labels under the bar (y≈115, monospace 10 px): `low 3.0%` (`#52514a`) at the bar start, `actual 3.1%` (white) under the actual marker, `high 3.3%` at the end.
- **BANKS' OWN FORECASTS** (y≈142), same label style (`#e5d9a3`). One row per bank (h≈24): bank name 12 px `#d7d6d2` (`Danske Bank`), a horizontal bar
  x≈1300–1530 (track `#15140d`) filled with a yellow gradient (`#484010` → `#cfb210`) from the left up to the bank's forecast on the same scale,
  plus dashed previous and white actual markers; value right `3.2%` monospace bold yellow `#eedf9f`.
- 2x2 stat boxes (y≈199–313, each ≈204x53, gap 8): bg `#080605`, 1 px border `#1a1a17`:
  `CONSENSUS` → `3.2%` (yellow `#eee0a9`); `RANGE` → `3.0% - 3.3%` (white); `PREVIOUS` → `3.0%` (`#a2a29c`); `ACTUAL` → `3.1%` (white).
  Labels monospace 9 px spaced `#50514a`; values monospace bold 13 px.
- Legend (y≈326, 11 px `#807e77`): `▎consensus` (yellow bar `#fbeaaa`), `¦previous` (grey dashed `#91908d`), `▎actual` (white),
  `● below · ● above consensus` (red dot `#f69a96`, cyan dot `#8fccd6`) — dot colours used for bank forecasts vs consensus.
- Rule 1 px `#191711` at y≈352.
- **SOURCES** (y≈367, monospace 10 px spaced `#5b5a54`):
  - `Econoday consensus survey · CA: CPI — CPI - Y/Y` (12 px `#807e77`).
  - `⧉ Danske Bank — Market Movers - 19-23 October 2026` with date `16 Oct` right-aligned (`#54534d`). This is a link (hover: text gold `#cfc392`) to the bank's PDF.

### 4.10 Inline expansion (click on the event name/tag)
- Expanded area bg `#140d08`, full table width, below the row.
- Description, 13 px `#8b8681`, line-height ≈20, two paragraphs, x=14:
  "The Consumer Price Index (CPI) measures the change in the price of goods and services from the perspective of the consumer. It is a key way to measure changes in purchasing trends and inflation."
  "A higher than expected reading should be taken as positive/bullish for the CAD, while a lower than expected reading should be taken as negative/bearish for the CAD."
- History chart card x=14–1586 (h≈473, seen at y=321–794 after scroll), bg `#050402`, 1 px border `#272218`:
  - Header row (y+13..+47): `CANADA · RELEASE HISTORY` monospace bold 10 px spaced 2 px gold `#cfc496`; under it `CPI` bold 13 px white.
    Then `LATEST · OCT 2026` (monospace 9 px spaced `#4f4d49`) over `3.1%` (monospace bold 20 px white); then
    `avg 2.2%` (white), `low -0.9%` (red `#c89c9c`) `Jul 2009` (dim `#53514d`), `high 8.1%` (cyan `#8bc6cd`) `Jul 2022` (dim).
  - Right controls: segmented `BARS | LINE` (active BARS: solid yellow `#fdd913` bg, dark text; inactive `#8f8b7a` on `#1d1803`),
    then range buttons `5Y 10Y 20Y MAX` (active 20Y yellow bg; others `#898574`). Each button h≈17, monospace bold 10 px.
  - Plot x≈25–1574, y≈380–735: inner border 1 px `#242415`; horizontal gridlines `#0f0d0a`; zero line `#827b64`.
    Y axis labels on the right (x≈1517), monospace 11 px `#94938f`: `10% 8% 6% 4% 2% 0% -2%`.
    X labels (y≈744) monospace 11 px `#969692`, every ≈11 months: `Nov 06, Oct 07, Sep 08, Aug 09, Jul 10, Jun 11, May 12, Apr 13, Mar 14, Feb 15, Jan 16, Dec 16, Nov 17, Oct 18, Sep 19, Aug 20, Aug 21, Jul 22, Jun 23, May 24, Apr 25, Mar 26`.
  - Bars: one per monthly release (239 bars), ≈5 px wide with 1 px gap; positive bars yellow vertical gradient (top `#ead16e` → bottom `#8d7d31`),
    negative bars grey `#a5a9aa` (2009, 2020–21 dips).
  - Hover: vertical highlight band ≈8 px `#25200d` over the hovered month, the hovered bar brighter (`#ffe585`), tooltip box
    (≈105x55, bg `#0d0802`, 1 px border `#5f5123`) with `01 Jan 2014` (11 px `#a3a3a1`) and `■ CPI 1.484%` (yellow square + white value, 3 dp).
  - Footer (y+449): left `239 RELEASES · NOV 2006 ⟶ OCT 2026 · RECORDED CALENDAR RELEASES`, right `DRAG ON THE CHART TO ZOOM` — monospace 9 px spaced `#524f44`.

## 5. Interactions

- Row hover → row bg `#251f0b`.
- Click MIN or MAX cell (dotted-underlined yellow) → detail panel slides in from the right (≈250–300 ms, ease-out); table reflows to the left.
  The clicked cell gets a 1 px gold focus outline that stays after closing.
- Source link in panel: hover gold; click opens the bank PDF (likely in News › Bank research).
- Close panel: not visible (assume click on the same cell, Esc, or click elsewhere).
- Click event name / tag / chevron → inline expansion toggles; chevron ⌄ ↔ ⌃.
- History chart: hover tooltip snaps to the month; BARS/LINE toggle; 5Y/10Y/20Y/MAX ranges; drag to zoom (per footer text).
- Mouse wheel scrolls the table body; column header and current day header stay pinned.
- Filters: search (live filter by event, country, currency), week navigator ‹ ›, country chips toggle, impact segment.
- Footer clock updates on each data refresh; LIVE badge static.

## 6. Data

| Field | Sample | Format | Update | Source on screen |
|---|---|---|---|---|
| Event time | 07:30 | HH:MM local | weekly calendar | — |
| Currency + flag | CAD | ISO code | — | — |
| Impact | low / medium (high exists: "High 20") | dots | — | — |
| Event name + period tag | CPI · ANNUAL inferred | text | — | — |
| Actual | 3.1% / 30.3K / 4.095% / -4.10 | unit kept from source (%, K, 3 dp for auctions) | at release time (live) | — |
| Forecast (consensus) | 3.2% | same unit | before release | Econoday consensus survey |
| Previous | 3.0% | same | — | — |
| Surprise | -0.1% / +10.9K | actual − forecast, signed | computed | — |
| Arrow | ↑ / ↓ | actual vs previous | computed | — |
| Min / Max (economists' range) | 3.0% / 3.3% | same unit | before release | Econoday survey |
| Bank forecasts | Danske Bank 3.2% | per bank | weekly | Danske Bank "Market Movers - 19-23 October 2026" (16 Oct) |
| Counts per impact | 168 / 20 / 42 / 106 | int | computed | — |
| Indicator description | CPI text | prose | static | — |
| Release history | 239 monthly values Nov 2006 → Oct 2026, avg 2.2%, low -0.9% (Jul 2009), high 8.1% (Jul 2022) | % 3 dp in tooltip | grows with each release ("RECORDED CALENDAR RELEASES") | recorded by the app |
| Footer time | 23:24:20 | HH:MM:SS | each refresh | "Terminal Data" |

## 7. Key frames

- `t000.0.jpg` — full-width table, filters, footer.
- `t002.0.jpg` / `t003.0.jpg` — detail panel open (economists' range, Danske Bank bar, boxes, sources).
- `t010.5.jpg` — hovered source link (gold).
- `t013.0.jpg` — panel closed, focus outline on MIN cell.
- `t014.5.jpg` / `t015.0.jpg` — inline expansion with description and release history chart.
- `t017.0.jpg` – `t019.0.jpg` — chart hover tooltip and scrolling with sticky headers.
- `t020.0.jpg` — scrolled final state.

## 8. Open questions

- How the detail panel is closed; whether clicking ACTUAL/FORECAST cells also opens it.
- Exact impact dot rules (1/2/3 dots, colours for high); no high-impact row was visible.
- `More...` dropdown content; whether country chips are toggles (all appear selected).
- The `inferred` tag semantics (assumed: period deduced by the app) and its colour for MONTHLY vs ANNUAL (both cyan in frames).
- Time zone of the TIME column (likely local, UTC-5 like Session Recap).
- LINE mode and drag-to-zoom of the history chart were not shown.
- Which banks besides Danske Bank can appear in "BANKS' OWN FORECASTS".
