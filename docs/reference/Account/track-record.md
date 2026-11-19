# Account › Track Record

Source video: `Reference/Account/track-record.mp4` (33 s, 66 frames at 0.5 s, 1600x966).
Frames: `SCRATCH/frames/Account/track-record/tNNN.N.jpg`.

## 1. Purpose

The trader's journal and performance report for one trading account. Closed trades (imported from MT4 / MT5 /
cTrader or logged by hand) are turned into statistics: net P&L, win rate, profit factor, drawdown, average
win/loss, a composite "performance score", cumulative P&L, P&L per month / quarter / trade, a full sortable trade
log, and an "Advanced details" view with drawdown curve, time-of-day and duration scatter plots, weekday and
symbol breakdowns, top / bottom trades. A sticky toolbar holds the three view tabs (DASHBOARD, TRADE LOG,
ADVANCED DETAILS), a FILTERS toggle (instrument, side, strategy), a year selector, the account selector, the last
import time, "+ LOG TRADE" and "IMPORT TRADES". The video shows a demo account ("Demo account · sample tra…",
10,000 USD, 60 closed trades between April and October 2026).

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | DASHBOARD view. Cursor on the "TRADE NET CUMULATIVE P&L" chart at 05/28: dashed vertical crosshair, dot on the line, tooltip `2026-05-28 / Day $91.50 / Cumulative $238.25`. |
| 0.5–1.5 | Cursor moves left; tooltip follows and snaps to points: t1.0 `2026-05-05 / Day $117.75 / Cumulative $423.25`. |
| 2.0–2.5 | Cursor over the empty area of the PERFORMANCE SCORE panel (x≈470,y≈297): no crosshair, no hover effect. |
| 3.0–3.5 | Back on the cumulative chart: t3.0 `2026-05-22 / Day -$111.00` (red) `/ Cumulative $246.75`. |
| 4.0 | `2026-07-10 / Day $192.50 / Cumulative $100.00`. |
| 4.5 | `2026-07-23 / Day $132.00 / Cumulative $457.00`. |
| 5.0–6.5 | Cursor goes low and right: t5.5 `2026-09-02 / Day -$42.75 / Cumulative $789.25` (tooltip drawn above the cursor because the cursor is low). |
| 7.0 | `2026-07-01 / Day -$100.00 / Cumulative -$173.00`. |
| 7.5 | Cursor over the NET P&L card: no hover effect on cards. |
| 8.0–9.5 | Cursor moves up to the DASHBOARD tab. |
| 10.0–11.0 | DASHBOARD tab clicked: a 1 px yellow focus outline appears around the active tab (x10..116, y27..58). |
| 11.5–12.0 | Cursor hovers TRADE LOG (label turns white). |
| 12.5 | Click TRADE LOG: view switches instantly (no transition) to the trade log (KPI strip + table). Focus outline now around TRADE LOG. |
| 13.0–15.5 | Trade log idle; cursor drifts right. |
| 16.0–16.5 | Cursor hovers ADVANCED DETAILS (label white). |
| 17.0 | Click ADVANCED DETAILS: 3x3 chart grid appears; horizontal bars are still growing (animation, ≈0.5 s). |
| 17.5–20.0 | Advanced details at rest. |
| 20.5 | Cursor hovers FILTERS (label turns white). |
| 21.0 | Click FILTERS: click ripple ring around the cursor; a filter bar slides in under the toolbar (all content moves down 38–39 px); FILTERS gets a yellow rounded outline (toggled on). |
| 21.5–23.0 | Idle; FILTERS label goes back to grey when the cursor leaves but keeps its outline. |
| 23.5–24.0 | Cursor moves to TOP 5 TRADES, hovers the USD/JPY bar: row band highlight + tooltip `USD/JPY · LONG 0.66 / Opened 16 Aug 2026, 20:34 / Held 4.6h / $258.00 · 2.58%`. |
| 24.0–28.5 | The page is scrolled down smoothly (≈20 px per 0.5 s) to its maximum (≈100 px); toolbar and filter bar stay fixed; the hovered row changes as content moves under the still cursor: t26.0 EUR/USD tooltip `EUR/USD · LONG 0.54 / Opened 21 Sept 2026, 21:26 / Held 1.7h / $238.50 · 2.38%` (placed left of the cursor), t29.5 `GBP/USD · SHORT 0.48 / Opened 29 Apr 2026, 04:20 / Held 1.2h / $234.00 · 2.34%`. |
| 29.0–32.5 | Scroll back up to the top; t32.5 hover on USD/JPY again with the same tooltip as t24. |

Measured scroll offsets (y of the row-2 panel top border at x=1300): 484 (t22.5) → 473 (t24.0) → 452 (t25.0) →
432 (t25.5) → 413 (t26.5) → 390 (t27.5) → 384 (t28.5–29.0, max) → 402 (t29.5) → 427 (t30.5) → 452 (t31.0) →
479 (t32.0) → 484 (t32.5). Smooth scrolling, about 20–23 px per 0.5 s.

## 3. Layout (1600x966)

The page has a dark warm-black background `#040302`–`#070304`. All three views share the same toolbar.

```
y=0   ┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
      │ [DASHBOARD|TRADE LOG|ADVANCED DETAILS] [▽FILTERS]     [All years ⌄]   [ACCOUNT ............ 10,000 USD ⌄] ⟳Last import… [+ LOG TRADE] │
      │                                                                                     [⇪ IMPORT TRADES]   │
y=85  ├────────────────────────── 1 px divider #2d2a15 ─────────────────────────────────────────────────────────┤
      │ (optional filter bar y=90..118: [All instruments⌄][Long & short⌄][All strategies⌄]      60 of 60 trades) │
      ├──────────────────────────────────────────────────────────────────────────────────────────────────────────┤
      │ view content (scrolls vertically, toolbar + filter bar fixed)                                             │
```

### Toolbar (fixed, y 0..85, full width, bottom divider ≈1 px `#2d2a15` at y≈85)
| Region | x, y, w, h |
|---|---|
| View tab group (one bordered box) | 10, 27, 377, 31 |
| — DASHBOARD | 10..116 |
| — TRADE LOG | 116..218 |
| — ADVANCED DETAILS | 218..387 |
| FILTERS button | 393, 27, 87, 31 |
| Year selector "All years ⌄" | 726, 25, 149, 36 (horizontally centred around x≈800) |
| Account selector box | 989, 8, 281, 36 |
| "⟳ Last import 21 Oct 2026, 18:00" | text from x 1278 to 1484, baseline y≈26 |
| "+ LOG TRADE" button | 1490, 11, 100, 30 |
| "⇪ IMPORT TRADES" button | 1456, 51, 134, 26 |

Right margin 10 px (buttons end at x 1590). Left margin 10 px.

### Filter bar (only when FILTERS is on), y 90..118
Three combos: x 8..113, 122..217, 227..333 (h 26, y 91..117). Right-aligned counter "60 of 60 trades" ending at
x≈1592, y≈104. Everything below moves down 38–39 px.

### DASHBOARD view (filters off)
| Region | x, y, w, h |
|---|---|
| KPI row: 5 cards, h 110 | y 96..206; x 10..318, 329..635, 647..953, 965..1270, 1282..1590 (w≈307, gap ≈11) |
| PERFORMANCE SCORE panel | 10, 217, 522, 386 (to y 603) |
| TRADE NET CUMULATIVE P&L panel | 544, 217, 1046, 415 (to y 632) |
| MONTH P&L | 10, 643, 519, 301 (to y 944) |
| QUARTER P&L | 541, 643, 517, 301 |
| LAST TRADES P&L | 1070, 643, 520, 301 |
| Next panel (cut off, yellow corner brackets visible at y≈958) | 10..1590, from y≈957 |

Gaps between panels ≈11–12 px. Panels share the outer 10 px margin. The cumulative chart stretches horizontally;
the score panel has a fixed-ish width (one third). When the window grows, the 3 bottom panels share the width
equally, KPI cards share the width equally.

### TRADE LOG view (filters off)
| Region | x, y, w, h |
|---|---|
| KPI strip: 6 cards, h 57 | y 96..153; x 10..267, 275..531, 540..796, 804..1060, 1069..1325, 1334..1590 (w≈257, gap ≈8) |
| CLOSED TRADES panel | 10, 165, 1580, to bottom (continues below the window; page scrolls) |
| — title row | y 174..197 (title left x 24; segmented pills x 1082..1322; search x 1330..1578) |
| — column header row | y≈210..238, text baseline y≈222 |
| — rows | first row centre y≈257, pitch ≈36.4 px (257, 293, 329, 366, 402, 439, …); 20 rows visible to y 966 |

### ADVANCED DETAILS view
| Region | filters off | filters on |
|---|---|---|
| Sub-header "ADVANCED DETAILS · 60 CLOSED TRADES" (x 10) | y≈108 | y≈147 |
| "$ / % BALANCE" toggle | x 1472..1590, y 97..121 | x 1472..1590, y 135..160 |
| Grid row 1 (h≈301) | y 133..435 | y 172..473 |
| Grid row 2 (h≈278) | y 446..725 | y 485..763 |
| Grid row 3 (h≈290) | y 736..≈1026 | y 775..≈1065 |
| Columns | x 10..530, 540..1059, 1069..1590 (w≈520, gap ≈10) | same |

Page scroll range with filters on: ≈100 px (document height ≈1066). No visible scrollbar in any frame.

## 4. Components

Fonts used everywhere (see `_shared-notes.md`):
- **Mono** (JetBrains-Mono-like, slashed/dotted zero): all numbers, tab labels, panel titles, table cells, tooltips.
- **Display sans, condensed** (Rajdhani/Saira-like): account name, filter combos, small footnotes.
- **Chart sans** (Arial/Helvetica-like, 11–12 px): chart axis labels and category labels in the Advanced and
  Month/Quarter/Last-trades charts.

### 4.1 View tabs
- One box, border 1 px `#2f291a`, bg `#140f05`, radius 0–2 px.
- Labels mono 12 px, bold, uppercase, letter-spacing ≈3 px: `DASHBOARD`, `TRADE LOG`, `ADVANCED DETAILS`.
- Inactive text `#8a8478` (dim grey); hover text `#e8e4da` (white).
- Active: text yellow `#fde695`; background fill dark olive `#2a2605`–`#393509`; 2 px yellow underline (`#e6c200`
  family) along the tab bottom (y≈55..57).
- After a click, a 1 px yellow (`#e6c200`) outline surrounds the active tab (keyboard-focus ring); it stays until
  another tab gets focus.

### 4.2 FILTERS toggle
- Funnel outline icon + `FILTERS`, mono 11 px, letter-spacing 2 px, colour `#6f6a5e`; hover `#e8e4da`.
- Off: no border. On: rounded rect outline 1 px yellow (`#d9b400`), radius 4, x 393..480, y 27..58; label stays grey
  when not hovered.
- Click shows a circular ripple ring (≈30 px diameter, grey) centred on the cursor for ≈0.3 s.

### 4.3 Year selector
- Box 149x36, bg `#211c02`–`#282108`, border 1 px `#3a331a`, radius 3.
- Text `All years` mono 20 px bold, colour `#f0d878` (yellow), chevron `⌄` 12 px same colour, right padding 14.

### 4.4 Account selector
- Box 281x36, bg `#271c03`, border 1 px `#3a321a`, radius 3.
- Line 1: `ACCOUNT` mono 9 px bold, letter-spacing 2 px, yellow `#e6b800` (x 998, y 17).
- Line 2: `Demo account · sample tra…` display sans 14 px bold white `#ece6d8`, elided with `…`.
- Right: `10,000 USD` mono 11 px `#a59f90` + chevron.

### 4.5 Last import, LOG TRADE, IMPORT TRADES
- `⟳ Last import 21 Oct 2026, 18:00`: refresh glyph + mono 11 px `#5d584c` (dim). Date format `D Mon YYYY, HH:MM`.
- `+ LOG TRADE`: outlined button, border 1 px `#3a3424`, bg transparent, text mono 12 px bold letter-spacing 2 px
  `#a09988`.
- `⇪ IMPORT TRADES` (upload icon): filled yellow `#fdd51f`, text black `#0a0800` mono 12 px bold, letter-spacing 2 px,
  radius 3.

### 4.6 Filter bar combos
- Each combo 95–106 px x 26 px, bg `#120d02`, border 1 px `#2b2817`, radius 3, text display-sans 12 px `#d6d0c0`,
  white chevron at right.
- Values: `All instruments`, `Long & short`, `All strategies`.
- Counter `60 of 60 trades` mono 10 px very dim `#3e3a31`.

### 4.7 DASHBOARD — KPI cards (5)
Common card: bg `#0c0602`–`#0d0802`, border 1 px `#3a351b` (top) / `#2c2512` (right), small L corner brackets top-left
(≈8 px arms, `#615942`) and bottom-right (≈10 px arms, `#776f59`). Title mono 10–11 px uppercase, letter-spacing
3 px, colour `#a29c8c`, followed by an ⓘ circle-i icon (10 px, `#6b665a`). Title at x+13, y+19.

1. **NET P&L ⓘ** — value `$1,805.00` mono 30 px bold cyan `#3adae5` (negative would be red). Below: pill
   `60 closed trades` mono 11 px `#a29c8c`, border 1 px `#383224`, radius 3, padding 2x6.
2. **TRADE WIN % ⓘ** — value `56.67%` mono 28 px bold white `#f4f1ea`. Right: semicircle gauge (180° arc, outer
   radius ≈38 px, stroke ≈8 px, x 542..618, y 132..170) — cyan `#2fe1f5` portion from the left = win share,
   salmon red `#fb867c` remainder. Under the gauge three small count chips: `34` cyan on `#0b2a30`, `0` grey
   `#8a8383` on `#1c1a18`, `26` red `#ff6f66` on `#2f0b08` (wins / breakeven / losses), mono 11 px bold, chip
   ≈20x16, radius 2.
3. **PROFIT FACTOR ⓘ** — value `1.68` mono 28 px bold cyan `#3ed2e5`. Right: donut ring (diameter ≈58 px, stroke
   ≈8 px, centre x≈907, y≈160): cyan arc = gross profit share, red `#ff897d` arc = gross loss share
   (≈37%, on the left/top-left side). No text inside.
4. **MAX DRAWDOWN ⓘ** — value `-$819` mono 28 px bold red `#f78a8b` (rounded to whole dollars). Sub-line
   `-7.78% · now $0` mono 11 px `#8a8478`. Right: mini drawdown sparkline (x 1150..1258, y 128..172): red line
   `#e0605a` 1.5 px hanging from a dashed top line (0), fill under-curve dark red `#56200d`.
5. **AVG WIN / LOSS TRADE ⓘ** — value `1.28` mono 28 px bold white. Right: horizontal split bar x 1376..1576,
   y 131..137 (h 6, radius 3): cyan `#21dae8` left part (avg win share ≈56%), red `#ff837b` right part. Under it
   `$132` cyan mono 11 px bold (left) and `-$103` red (right-aligned).

### 4.8 DASHBOARD — PERFORMANCE SCORE panel
- Highlighted panel: bright yellow corner brackets on all four corners (`#efd860`, arms ≈12 px, 2 px thick), panel
  border `#201805`, bg `#0d0802` with a faint vertical yellow glow at the edges.
- Title: pulsing dot (5 px, `#e0c566` ↔ `#9c7d35`, period 1 s) + `PERFORMANCE SCORE` mono 10 px bold yellow
  `#e9d258`, letter-spacing 2 px.
- Radar (hexagon) chart centred x≈270, y≈380, radius ≈95: 6 axes, labels display-sans 10 px bold `#d6d0c0`:
  `Win %` (top), `Profit factor` (upper right), `Avg win/loss` (lower right), `Recovery factor` (bottom),
  `Max drawdown` (lower left), `Consistency` (upper left). Grid: 4 concentric hexagons + spokes, 1 px `#2a2414`.
  Data polygon stroke 2 px gold `#dcb84a`, fill gold at ≈20% alpha (`#3f3105` on bg). Vertex dots 3 px: gold for
  most, green `#5d905a`/`#759f7a` on Consistency and Max drawdown vertices (green = good score?).
- Divider 1 px `#2a2414` at y≈522 (x 22..520).
- Score line, centred: `SCORE` display-sans 10 px `#4a463c`, `59.2` mono 26 px bold `#e8c66d`, `/ 100`
  display-sans 10 px `#4a463c`.
- Gradient bar x 147..397, y 572..577 (h 4–5, radius 3): red `#df5f61` → orange → yellow `#eebc52` → green
  `#4caf50`. (No marker visible; the score colour matches the gradient position.)
- Footnote centred, display-sans 9 px `#3d3a32`: `This app's own average of the six real metrics above — not an
  industry-standard score.`

### 4.9 DASHBOARD — TRADE NET CUMULATIVE P&L ⓘ
- Line chart, line 2 px cyan `#3fd0de`, area fill between line and the $0 baseline: teal gradient `#0f3131` (top)
  → `#091814` (near zero). Below zero the fill is the same teal tone (dark).
- Y axis (left, labels right-aligned at x≈620, chart sans 11 px `#9a958a`): `$2.0K, $1.5K, $1.0K, $500, $0,
  −$500` (unicode minus, K for thousands with one decimal). Gridlines 1 px `#1b170b` horizontal at each tick.
  Zero line: 1 px dashed `#4c4937`.
- X axis labels (chart sans 11 px `#9a958a`, y≈604): `04/23/26 05/05/26 05/26/26 06/15/26 06/29/26 07/09/26
  07/23/26 07/31/26 08/20/26 08/31/26 09/18/26 10/08/26` — MM/DD/YY of trading days; points are evenly spaced per
  trading day (category axis, not linear time).
- Plot area x≈628..1560, y≈262..588.
- Hover: vertical dashed crosshair 1 px `#8a8478` across the plot at the nearest point, marker on the line
  (white ring 2 px, light-blue fill, r 4), tooltip (see shared tooltip): `YYYY-MM-DD` (grey), `Day <signed $>`
  (value cyan if ≥0, red if <0), `Cumulative <$>` (bold white). Tooltip offset ≈12 px right of the crosshair,
  vertically near the cursor; flips above/left when near edges.

### 4.10 DASHBOARD — MONTH P&L ⓘ / QUARTER P&L ⓘ / LAST TRADES P&L ⓘ
- All: vertical bars, cyan `#1ddbf0` for ≥0, red `#ff817d` for <0, from a 1 px `#4c4937` zero line; horizontal
  gridlines `#1b170b`; axis labels chart sans 11 px `#9a958a`.
- MONTH: bar width ≈30 px; y ticks `$800 $600 $400 $200 $0 −$200 −$400`; x labels every second month `Apr 26`,
  `Jun 26`, `Aug 26`, `Oct 26`. Values ≈ Apr +300, May −60, Jun −310, Jul +725, Aug +70, Sep +335, Oct +740.
- QUARTER: bar width ≈30; y `$1.2K $1.0K $800 $600 $400 $200 $0 −$200`; x `Q2 2026` (≈ −60), `Q3 2026` (≈ +1,125),
  `Q4 2026` (≈ +740).
- LAST TRADES: one thin bar (≈8 px) per trade, 60 trades, gap ≈2 px; y `$300 $200 $100 $0 −$100 −$200`;
  x labels MM/DD: `06/29 07/09 07/23 07/31 08/19 08/31 09/18 10/08`. (Only the last ~45 trades fit? first label
  06/29 suggests it shows the latest N trades.)

### 4.11 TRADE LOG — KPI strip (6 cards)
- Card h 57, same card style (brackets top-left bright `#615942`, bottom-right). Label mono 9 px letter-spacing
  3 px `#8a8478` at y≈108; value mono 20 px bold at y≈133.
- `CLOSED TRADES` → `60` (white `#fbf9e5`); `WINS / LOSSES` → `34 / 26` (white); `WIN RATE` → `56.7%` (white, one
  decimal); `NET P&L` → `+$1,805.00` (cyan `#2ed8e5`, explicit +); `OPEN POSITIONS` → `0` (pale yellow `#f9f1b1`);
  `UNREALIZED` → `—` (cyan `#6fcad7`, em dash when no open positions).

### 4.12 TRADE LOG — CLOSED TRADES panel
- Title `CLOSED TRADES` mono 16 px bold white, letter-spacing 3 px, then `60 / 60` mono 10 px dim `#5d584c`
  (filtered / total).
- Segmented pills (x 1082..1322, h 23): `ALL` active (fill `#ffda44`, text black, radius 3), `WINS`, `LOSSES`,
  `BREAKEVEN` inactive (text `#73725d` mono 10 px bold letter-spacing 2 px, transparent). Group border 1 px
  `#2a2618`.
- Search box x 1330..1578, h 24, bg `#080602`, border 1 px `#2a2618`, magnifier icon + placeholder
  `Search instrument, strategy` mono 11 px `#5d584c`.
- Column header row: mono 10 px bold uppercase, letter-spacing 2 px, colour gold `#d9b54a`; the sorted column is
  white with an arrow: `CLOSED ↓` (descending by close time). Columns (left x or right edge):

| Column | Align | x | Cell format / colour |
|---|---|---|---|
| OPENED | left | 22 | `18 Oct 26, 22:37` mono 13 px dim `#8a8478` (`DD Mon YY, HH:MM`; September is `Sept`) |
| CLOSED ↓ | left | 162 | same format, white `#ece8de` |
| INSTRUMENT | left | 303 | display/mono 14 px bold white: `Gold`, `Nasdaq 100`, `AUD/USD`, `USD/CAD`, `EUR/USD`, `GBP/USD` |
| SIDE | left | 433 | badge 42x18 radius 3, mono 10 px bold letter-spacing 1: `LONG` text `#25a2ae`, border `#1d7f88`, bg `#052423`; `SHORT` text `#e0605a`, border `#862d26`, bg `#2f0a0a` |
| SIZE | right | ≈579 | `0.08`, `1.09`, `0.5` (trailing zeros trimmed) white |
| ENTRY | right | ≈682 | `4,148.69`, `29,837.2`, `0.70001`, `1.12171` white, thousands comma, trailing zeros trimmed |
| EXIT | right | ≈784 | same format white |
| STOP | right | ≈886 | muted red `#d0706a` (thin glyph peak measured `#a7867f`) |
| TARGET | right | ≈988 | muted green `#5fb877` (thin glyph peak measured `#689a72`) |
| HELD | right | ≈1067 | `6.5h`, `36m` dim `#a29c8c` (minutes under 1 h, else hours with 1 decimal) |
| R | right | ≈1141 | `+0.74R` cyan `#3fb8e8` / `-1.00R` red, 2 decimals, signed |
| STRATEGY | left | 1159 | `Mean revers…`, `Trend follo…`, `Breakout`, `News`, `Reversal` grey `#a29c8c`, elided at ≈90 px |
| SESSION | left | 1255 | `Asian`, `London`, `NY Overlap` dimmer `#5d584c` |
| P&L | right | ≈1487 | bold mono 15 px: `+$55.50` cyan `#3fc6ee`, `-$100.00` red |
| (actions) | | 1545, 1568 | eye icon (view) and trash icon (delete), 14 px, `#5d584c` |

- Row: height ≈36.4 px, bg same as panel `#0c0601`, separator 1 px `#32372d`–`#3c483c` (greenish grey). Left accent
  bar 3 px at x 11..14 full row height: cyan `#43d9ed` (win), red `#f88c83` (loss) (grey for breakeven, assumed).
- No zebra striping, no hover row highlight seen (the table was not hovered).

Rows visible at t13.5 (sorted by CLOSED desc):

| Opened | Closed | Instr | Side | Size | Entry | Exit | Stop | Target | Held | R | Strategy | Session | P&L |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 18 Oct 26, 22:37 | 19 Oct 26, 05:06 | Gold | LONG | 0.08 | 4,148.69 | 4,155.5 | 4,139.49 | 4,167.09 | 6.5h | +0.74R | Mean revers… | Asian | +$55.50 |
| 14 Oct 26, 20:51 | 14 Oct 26, 22:09 | Nasdaq 100 | SHORT | 1.09 | 29,837.2 | 29,726.2 | 29,974.2 | 29,563.2 | 1.3h | +0.81R | Mean revers… | Asian | +$121.50 |
| 14 Oct 26, 03:02 | 14 Oct 26, 10:20 | AUD/USD | LONG | 0.52 | 0.70001 | 0.70311 | 0.69761 | 0.70481 | 7.3h | +1.29R | Breakout | London | +$161.25 |
| 09 Oct 26, 08:14 | 09 Oct 26, 17:17 | Gold | LONG | 0.12 | 4,153.09 | 4,160.65 | 4,140.89 | 4,177.49 | 9.1h | +0.62R | News | NY Overlap | +$93.00 |
| 08 Oct 26, 03:19 | 08 Oct 26, 11:23 | Gold | SHORT | 0.07 | 4,149.05 | 4,135.47 | 4,163.65 | 4,119.85 | 8.1h | +0.93R | Trend follo… | London | +$93.00 |
| 07 Oct 26, 09:00 | 07 Oct 26, 10:11 | USD/CAD | SHORT | 0.73 | 1.40493 | 1.40683 | 1.40683 | 1.40113 | 1.2h | -1.00R | Reversal | NY Overlap | -$100.00 |
| 05 Oct 26, 20:40 | 06 Oct 26, 01:44 | EUR/USD | LONG | 0.5 | 1.12171 | 1.12485 | 1.11971 | 1.12571 | 5.1h | +1.57R | Mean revers… | Asian | +$157.00 |
| 01 Oct 26, 05:10 | 01 Oct 26, 10:21 | Nasdaq 100 | LONG | 0.78 | 29,763.6 | 29,968.4 | 29,635.6 | 30,019.6 | 5.2h | +1.60R | Trend follo… | London | +$160.00 |
| 21 Sept 26, 21:26 | 21 Sept 26, 23:08 | EUR/USD | LONG | 0.54 | 1.1234 | 1.12785 | 1.1206 | 1.129 | 1.7h | +1.59R | Mean revers… | Asian | +$238.50 |
| 17 Sept 26, 20:52 | 18 Sept 26, 04:40 | EUR/USD | SHORT | 0.32 | 1.12776 | 1.12571 | 1.13086 | 1.12156 | 7.8h | +0.66R | Mean revers… | Asian | +$66.00 |
| 11 Sept 26, 02:40 | 11 Sept 26, 06:12 | EUR/USD | LONG | 0.44 | 1.12766 | 1.12426 | 1.12426 | 1.13446 | 3.5h | -1.00R | Breakout | Asian | -$150.00 |
| 09 Sept 26, 22:58 | 10 Sept 26, 00:16 | AUD/USD | SHORT | 0.66 | 0.7013 | 0.69948 | 0.7032 | 0.6975 | 1.3h | +0.96R | Mean revers… | Asian | +$120.00 |
| 02 Sept 26, 09:54 | 02 Sept 26, 10:29 | Gold | SHORT | 0.07 | 4,169.32 | 4,175.25 | 4,179.72 | 4,148.52 | 36m | -0.57R | Reversal | NY Overlap | -$42.75 |
| 01 Sept 26, 03:43 | 01 Sept 26, 11:56 | EUR/USD | LONG | 0.5 | 1.11578 | 1.11786 | 1.11328 | 1.12078 | 8.2h | +0.83R | Breakout | London | +$103.75 |
| 31 Aug 26, 02:24 | 31 Aug 26, 10:08 | Nasdaq 100 | LONG | 1.61 | 29,751.9 | 29,872.8 | 29,658.9 | 29,937.9 | 7.7h | +1.30R | Breakout | Asian | +$195.00 |
| 27 Aug 26, 21:08 | 28 Aug 26, 04:06 | GBP/USD | LONG | 0.31 | 1.34491 | 1.34773 | 1.34171 | 1.35131 | 7.0h | +0.88R | Mean revers… | Asian | +$88.00 |
| 27 Aug 26, 03:26 | 27 Aug 26, 06:17 | EUR/USD | SHORT | 0.6 | 1.12778 | 1.13028 | 1.13028 | 1.12278 | 2.9h | -1.00R | Breakout | London | -$150.00 |
| 25 Aug 26, 09:18 | 25 Aug 26, 15:13 | USD/CAD | LONG | 0.42 | 1.39556 | 1.39363 | 1.39306 | 1.40056 | 5.9h | -0.77R | Reversal | NY Overlap | -$57.75 |
| 24 Aug 26, 08:16 | 24 Aug 26, 10:49 | Gold | SHORT | 0.06 | 4,149.08 | 4,164.68 | 4,164.68 | 4,117.88 | 2.6h | -1.00R | Reversal | NY Overlap | -$100.00 |
| 19 Aug 26, 20:03 | 19 Aug 26, 23:10 | EUR/USD | SHORT | 0.31 | 1.12066 | 1.12386 | 1.12386 | 1.11426 | 3.1h | -1.00R | Mean revers… | Asian | -$100.00 |

### 4.13 ADVANCED DETAILS — sub-header and unit toggle
- `ADVANCED DETAILS · 60 CLOSED TRADES` mono 10 px, letter-spacing 4 px, colour `#4a463c`.
- Toggle (x 1472..1590, h 25): segment `$` active (text `#eed882` on `#2a2408`–`#403808`), segment `% BALANCE`
  inactive (text `#8a8478`, mono 11 px, letter-spacing 2 px). Border 1 px `#2a2618`. Switching to `% BALANCE`
  would express every money value as % of account balance (not shown clicked).

### 4.14 ADVANCED DETAILS — 9 chart panels
Panel style = standard panel (corner brackets top-left `#6b6248` and bottom-right, border `#3a351b`). Titles mono
10 px uppercase, letter-spacing 4 px, `#807d6c`, followed by ⓘ. All charts use chart-sans 11–12 px labels
`#9a958a` for axes, `#e3e0db` for category labels, faint vertical gridlines `#1b170b`, left axis line 1 px
`#47433b` with small tick marks.

1. **DRAWDOWN · MAX -$818.75 ⓘ** (the max drawdown is in the title, 2 decimals). Area chart: line 1.5 px yellow
   `#e8c020`, fill from 0 down to the line: dark red/brown gradient `#6f3331` (deep) → `#34170f` (shallow).
   Y ticks `$0 −$200 −$400 −$600 −$800 −$1.0K`; x month labels `May Jun Jul Aug Sep Oct` (time axis, first-of-month).
   Plot x≈88..502, y≈212..432. Minimum ≈ −$818.75 in early July.
2. **TRADE TIME PERFORMANCE ⓘ** — scatter: x = hour of day of the trade (open time?) `02:00 … 22:00` every 2 h,
   y = P&L `$300 … −$200` step 100. Dots r≈4 px, cyan `#2bc3d3` (win) / red `#ef786e` (loss). Dashed zero line
   1 px `#605c55`. Clusters at 02–05 h, 08–10 h, 20–23 h.
3. **TRADE DURATION PERFORMANCE ⓘ** — scatter, x logarithmic duration with labels `6m`, `1.0h`, `10.0h`; same y and
   colours.
4. **P&L BY WEEKDAY ⓘ** — horizontal bars from a $0 axis at x≈244: Monday ≈+530, Tuesday ≈+170, Wednesday ≈+440,
   Thursday ≈+620, Friday ≈−210 (red, extends left), Sunday ≈+250. No Saturday row (days with no trades are
   omitted). X ticks `−$400 −$200 $0 $200 $400 $600 $800`. Bar height ≈20 px, row pitch 34 px.
5. **TOP 5 TRADES ⓘ** — horizontal cyan bars from $0; two-line labels right-aligned before the axis: instrument
   on line 1, close date `DD/MM/YYYY` on line 2: `USD/JPY 17/08/2026` 258.00, `EUR/USD 21/09/2026` 238.50,
   `GBP/USD 29/04/2026` 234.00, `Nasdaq 100 31/08/2026` 195.00, `GBP/USD 10/07/2026` ≈192. X ticks `$0 $50 … $300`.
6. **TOP 5 SYMBOLS P&L ⓘ** — cyan bars: `GBP/USD` ≈515, `AUD/USD` ≈455, `USD/JPY` ≈418, `Nasdaq 100` ≈360,
   `S&P 500` ≈325. X `$0 … $600` step 100.
7. **NUMBER OF TRADES BY WEEKDAY ⓘ** — taupe bars `#9c9788`: Monday 8, Tuesday 11, Wednesday 15, Thursday 12,
   Friday 9, Sunday 5. X `0 3 6 9 12 15`.
8. **BOTTOM 5 TRADES ⓘ** — red bars `#ff8378` anchored at the RIGHT ($0 at right edge x≈1029), growing left:
   `EUR/USD 11/09/2026` −150, `EUR/USD 27/08/2026` −150, `Gold 29/06/2026` −150, `USD/JPY 07/08/2026` −125,
   `Nasdaq 100 28/07/2026` −125. X `−$150 −$120 −$90 −$60 −$30 $0`.
9. **NUMBER OF TRADES · TOP 5 SYMBOLS ⓘ** — taupe bars: GBP/USD 10, AUD/USD 3, USD/JPY 8, Nasdaq 100 9, S&P 500 2.
   X `0 2 4 6 8 10`.

Bar hover (Top/Bottom trades, and presumably all bar charts): a full-width band behind the hovered category
(bg `#312c28`, ≈40 px tall), the bar becomes lighter (`#3edcec` → lighter cyan), and a tooltip:
```
USD/JPY · LONG 0.66            (bold white instrument, "·", side + size white)
Opened 16 Aug 2026, 20:34      (white)
Held 4.6h                      (white)
$258.00 · 2.58%                (cyan for profit; % = P&L / account balance)
```
Tooltip box ≈210x100, bg `#0f0602`, border 1 px `#544f2d`, radius 3, padding 12, mono 12 px, line height 19 px.
Placed ≈20 px right and below the cursor; flips to the left of the cursor when it would overflow the panel /
window (t26.0 EUR/USD tooltip at x 565..783 while the cursor is at x 810).

Bar entry animation: on view switch the horizontal bars grow from the axis to full length in ≈0.5 s (ease-out);
scatter dots and line charts appear without visible animation.

## 5. Interactions

- **View tabs**: click switches view instantly; hover = white label; focus ring after click.
- **FILTERS**: toggles the filter bar (content shifts by 38–39 px, no animation visible at 0.5 s sampling).
  Combos (not opened in the video) filter all views; the counter shows `<filtered> of <total> trades`.
- **Year selector**: `All years` or a single year (not opened in the video).
- **Account selector**: chooses the account; shows name and balance with currency.
- **LOG TRADE**: opens a manual trade entry form (not shown). **IMPORT TRADES**: imports a statement / connects
  MT4/MT5/cTrader (not shown). "Last import" timestamp updates after an import.
- **Cumulative P&L chart**: hover crosshair + snapping marker + tooltip (day P&L and cumulative).
- **Trade log**: segmented filter ALL/WINS/LOSSES/BREAKEVEN, text search over instrument and strategy, sortable
  columns (arrow on the sorted header; CLOSED desc by default), per-row view (eye) and delete (trash) actions.
- **Advanced details**: `$` / `% BALANCE` unit toggle; bar hover tooltips with row band; mouse-wheel smooth
  scrolling of the content under a fixed toolbar.
- **Cards**: no hover effects.
- **Performance score dot**: pulses (opacity ≈1 ↔ 0.55, 1 s period).

## 6. Data

| Field | Sample | Format | Update |
|---|---|---|---|
| Account name | `Demo account · sample tra…` | text, elided | on account change |
| Account balance | `10,000 USD` | integer, comma thousands + ISO currency | on import |
| Last import | `21 Oct 2026, 18:00` | `D Mon YYYY, HH:MM` | per import / sync |
| Net P&L | `$1,805.00` / `+$1,805.00` | $ 2 decimals, + sign in trade log | on new trade |
| Closed trades | 60 | int | on new trade |
| Wins / losses / breakeven | 34 / 26 / 0 | int | |
| Win rate | 56.67% (dashboard) / 56.7% (trade log) | 2 or 1 decimals | wins / closed |
| Profit factor | 1.68 | 2 decimals | gross profit / gross loss |
| Max drawdown | −$819 / −$818.75 / −7.78% · now $0 | $ rounded (card), 2 dec (title); % of peak equity; current DD | equity curve |
| Avg win / avg loss | 1.28 ($132 / −$103) | ratio 2 dec; $ rounded | |
| Performance score | 59.2 / 100 | 1 decimal | app's own average of 6 normalised metrics (Win %, Profit factor, Avg win/loss, Recovery factor, Max drawdown, Consistency) |
| Cumulative P&L per day | Day $91.50, Cumulative $238.25 | per trading day | |
| Month / Quarter P&L | ≈ +725 (Jul 26) | $ | |
| Open positions / unrealized | 0 / — | | live if a broker is connected |
| Trade fields | see table above | | |
| R multiple | +0.74R | (exit−entry)/(entry−stop) signed, 2 dec | per trade |
| Held | 6.5h / 36m | minutes < 1 h, else hours 1 dec | |
| Session | Asian / London / NY Overlap | derived from open time (UTC) | |
| Strategy | Mean reversion, Trend following, Breakout, News, Reversal | user tag | |
| Trade tooltip % | 2.58% | P&L / account balance | |

No external data source name is shown on this page; data comes from the user's imported statements or manual
entries ("sample trades" in the demo). The demo account is fake sample data.

## 7. Key frames

- `t000.0.jpg` — Dashboard with cumulative-chart tooltip.
- `t003.0.jpg` — Dashboard tooltip with a negative day (red).
- `t010.0.jpg` — Dashboard, focus ring on the DASHBOARD tab, full layout clean.
- `t013.5.jpg` — Trade log view (KPI strip + 20 rows).
- `t017.0.jpg` — Advanced details while bars animate.
- `t017.5.jpg` — Advanced details at rest, filters off.
- `t022.5.jpg` — Advanced details with filter bar open.
- `t026.0.jpg` — Scrolled + left-flipped tooltip (EUR/USD).
- `t029.5.jpg` — Max scroll, tooltip GBP/USD.
- `t032.5.jpg` — Back at top, USD/JPY tooltip, hover band.

## 8. Open questions

- What is below the dashboard's third row (a panel with yellow corner brackets starts at y≈957)? The dashboard
  was never scrolled.
- Content of the filter combos, the year list, the account list, the LOG TRADE form and the IMPORT TRADES flow
  (file import vs MT4/MT5/cTrader sync) are not shown.
- Exact formulas of the six radar metrics and their 0–100 normalisation; meaning of the green vertex dots.
- Whether "Last trades P&L" shows all trades or the last N (first label is 06/29 although trades start in April).
- `% BALANCE` mode rendering; breakeven row colour; table row hover; sort on other columns; pagination or
  infinite scroll in the trade log.
- Which time zone is used for Opened/Closed, sessions and the "Trade time performance" hour axis.
- Whether the time-of-day scatter uses open or close time.
