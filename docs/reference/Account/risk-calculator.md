# Account › Risk Calculator

Source video: `Reference/Account/risk-calculator.mp4` (32.5 s, 65 frames at 0.5 s, 1600x966).
Frames: `SCRATCH/frames/Account/risk-calculator/tNNN.N.jpg`.

## 1. Purpose

A position-sizing desk used before entering a trade. The user picks an instrument, enters the account balance,
the risk per trade (as % of balance or as a fixed amount in the account currency), the entry price, the stop
loss and an optional take profit. The page computes, deterministically and instantly, the lot size, position
size in units, stop distance, money at risk, reward at take profit, the risk/reward ratio with a quality verdict,
and the breakeven win rate; it draws the trade as a price map (stop – entry – target). Entry/stop/target are
prefilled from the live price and remain editable. Direction (LONG/SHORT) is inferred from stop vs entry.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–7.0 | Idle default state (EUR/USD, balance 10000, risk 1 %, entry 1.12000, SL 1.11500, TP 1.13000). Cursor wanders over RISK, REWARD, TRADE MAP (on the bar at t4.5) and the R/R panel: no hover effect anywhere. Title dot pulses; a yellow block cursor after the title blinks. |
| 7.5 | Cursor over the INSTRUMENT combo (no visible hover change). |
| 8.0 | Click: the combo opens INLINE as an expanded list (pushes the rest of the form down by 234 px). Chevron flips to `^`. Search field gets focus (blinking caret, t8.5–10.0). |
| 8.0–10.5 | List shown: EUR/USD (current, highlighted), GBP/USD, USD/JPY, USD/CHF, USD/CAD, AUD/USD, NZD/USD, EUR/GBP (cut, list scrolls). |
| 11.0 | Hover USD/CHF: row gets an olive highlight and yellow text. |
| 11.5–15.0 | Cursor goes to the `%` toggle (already active) and rests on it (clicked around t15.0 — no change since already active). The list stays open. |
| 15.5–16.0 | Cursor moves to `$`. |
| 16.5 | Click `$`: `$` becomes active (yellow), `%` dim. All outputs recompute instantly with risk = 1 USD: LOT SIZE `0,00`, POSITION SIZE `200 units`, RISK `1,00` `0.01% of balance`, REWARD `2,00`, bottom RISK `1`, REWARD `2`. R/R stays `1 : 2.00`. |
| 17.0–18.0 | Cursor on `$`. |
| 18.5–32.0 | Cursor parked inside the R/R panel (x≈815, y≈545). Nothing changes except the title-dot pulse and the block-cursor blink. Dropdown remains open until the end. |

## 3. Layout (1600x966)

```
y=0    ┌────────────────────────────────────────────────────────────────────────────────────────────────┐
       │ ▦ ● RISK CALCULATOR — POSITION SIZING DESK █                      [• EUR/USD · DETERMINISTIC MATH]│ title bar h 37
y=37   ├─────────────── 1 px #463d17 ───────────────────────────────────────────────────────────────────────┤
y=51   │┌TRADE PARAMETERS──────┐ ┌POSITION──────────────────────────────────────────────────────── [↗LONG]┐│
       ││INSTRUMENT [EUR/USD ⌄]│ │LOT SIZE 0,20 lots      POSITION SIZE 20 000 units   STOP DISTANCE 0.00500││
       ││ACCOUNT BALANCE [10000]│ └───────────────────────────────────────────────────────────────── y=192 ┘│
       ││RISK PER TRADE [1][%][$]│ ┌RISK──────────────────────────┐ ┌REWARD─────────────────────────────┐ │
       ││ENTRY PRICE [1.12000]  │ │100,00   1.00% of balance      │ │200,00   at take profit            │ │ y 214..322
       ││STOP LOSS [1.11500]    │ └──────────────────────────────┘ └───────────────────────────────────┘ │
       ││TAKE PROFIT (OPTIONAL) │ ┌TRADE MAP  STOP        ENTRY                                  TARGET─┐ │ y 344..484
       ││[1.13000]              │ │  |═════red═════|══════════════cyan════════════════════════|        │ │
       ││                       │ └───────────────────────────────────────────────────────────────────┘ │
       ││                       │ ┌──────────────────────────────────────────────────────────────────┐ │ y 506..920
       ││                       │ │                RISK / REWARD RATIO   1 : 2.00   ACCEPTABLE        │ │
       ││                       │ │                [====|=====       ]  BREAKEVEN 33%  RISK  REWARD   │ │
       │└───────────────────────┘ └──────────────────────────────────────────────────────────────────┘ │
y=944  │                           footnote (tiny)                                                      │
       └────────────────────────────────────────────────────────────────────────────────────────────────┘
```

| Region | x, y, w, h |
|---|---|
| Title bar | 0, 0, 1600, 37 (divider 1 px `#463d17` at y≈36–37) |
| Left panel TRADE PARAMETERS | 14, 51, 407, 900 (to y 951) |
| POSITION panel | 432, 51, 1154, 141 (to y 192) |
| RISK panel | 432, 214, 571, 108 (to x 1003, y 322) |
| REWARD panel | 1015, 214, 571, 108 (to x 1586) |
| TRADE MAP panel | 432, 344, 1154, 140 (to y 484) |
| R/R panel | 432, 506, 1154, 414 (to y 920) |
| Footnote | x 432, baseline y≈944 |

Margins: 14 px left, 14 px right (panels end at x 1586), 14 px top under the title bar; gaps between right
panels 22 px vertically, 12 px horizontally (RISK/REWARD); 11 px between left panel and right column.
Left column fixed width (≈407); right column stretches. The R/R panel absorbs extra height; the left panel
spans the full height. No scrolling in the video.

Left form internal layout (closed dropdown): labels at x 27, rows: INSTRUMENT label y 95 / combo y 107..138;
ACCOUNT BALANCE y 156 / input 169..207; RISK PER TRADE y 225 / input 237..275 (x 27..355) + toggle x 362..402;
ENTRY PRICE y 293 / input 306..344; STOP LOSS y 362 / input 375..413; TAKE PROFIT (OPTIONAL) y 430 / input
443..482. Field pitch ≈68–69 px. With the instrument list open everything below moves down by 234 px
(ACCOUNT BALANCE label at y 390).

## 4. Components

### 4.1 Title bar
- Left: calculator glyph 10x12 (`#4d4201`, dim yellow) at x 18, then a status dot 6 px at x 38 (pulses between
  `#7a7819` and `#efd632`, ≈1 s period), then `RISK CALCULATOR — POSITION SIZING DESK` mono 10 px, letter-spacing
  ≈4 px, colour `#5d584c` (dim grey), then a solid yellow block cursor `█` 7x12 `#fbd911` at x 373..380 that
  blinks (≈530 ms on / 530 ms off; visible in about half the frames).
- Right badge: `• EUR/USD · DETERMINISTIC MATH` mono 9 px bold, letter-spacing 2 px, yellow `#e8d27a`; box
  x 1387..1585, y 9..27, bg `#1a1503`, border 1 px `#544b03`, radius 2. The pair follows the instrument.
- Bar bg `#0f0a02` with a subtle vertical gradient to `#040302` below the divider.

### 4.2 Panel style (all 6 panels)
- bg `#0c0601` (left panel) / `#0d0802`, border 1 px `#3f3a29` (top) – `#2d2914`, radius 0.
- Corner brackets: top-left L (≈8 px arms, 2 px, `#6b6248`/`#4d4c30`) inset 6 px; bottom-right L (≈8 px arms,
  `#7a6f50`) inset 6 px.
- Panel title: mono 9–10 px uppercase, letter-spacing 4 px, colour `#6b665a` (`TRADE PARAMETERS`, `POSITION`,
  `RISK`, `REWARD`, `TRADE MAP`) at x+13, y+20. The R/R panel has no title in the corner.

### 4.3 Form fields (left panel)
- Labels: mono 9 px uppercase, letter-spacing 4 px, `#5d584c`: `INSTRUMENT`, `ACCOUNT BALANCE`, `RISK PER TRADE`,
  `ENTRY PRICE`, `STOP LOSS`, `TAKE PROFIT (OPTIONAL)`.
- Text inputs: x 27..408, h 38, bg `#050401` (darker than panel), border 1 px `#1b170d`, radius 3; value mono
  16 px white `#fdfcf9`, left padding 9. Raw numbers without formatting: `10000`, `1`, `1.12000`, `1.11500`,
  `1.13000` (prices keep the instrument's 5 decimals).
- Instrument combo: x 27..408, h 31, bg `#060502`, border 1 px `#1b170d`; text display-sans 13 px `#ece8de`
  `EUR/USD`; chevron `⌄` 10 px at x 393 (`^` when open).
- Risk unit toggle (two small buttons right of the risk input, inside a 1 px `#2a2618` box x 359..406):
  active button filled `#fed71c` (22x33, radius 3) with black glyph; inactive glyph `#55514b` on transparent.
  Glyphs `%` and `$`.

### 4.4 Instrument list (inline dropdown)
- Opens directly under the combo, inside the left panel: x 27..408, y 140..372, border 1 px `#4a4224`, bg
  `#060502`, corner brackets TL (x 33, y 147) and BR (x 401, y 365) yellow-dim `#857f67`.
- Search field x 33..401, y 147..172, bg `#040300`, border 1 px `#2a2618`: magnifier icon 9 px + placeholder
  `Search...` display-sans 12 px `#5d584c`; focused on open (thin caret `#ece8de`, blinking).
- Rows h ≈25.5 (y centres 188, 215, 240, 265, 290, 317, 342, 367…): symbol display-sans 12 px `#d6d0c0` at x 40;
  category tag `FX` mono 8 px `#3a3630` right-aligned at x≈396.
- Current item: bg `#29251d`, text yellow `#e6c200`. Hover item: bg olive `#433603`–`#4a3c08`, text yellow
  `#f0d040`.
- Visible items: `EUR/USD, GBP/USD, USD/JPY, USD/CHF, USD/CAD, AUD/USD, NZD/USD, EUR/GBP…` (list scrolls; height
  shows ≈7.5 rows).
- The list does not close when the pointer leaves it or when other controls (`%`/`$`) are clicked.

### 4.5 POSITION panel
- Three columns at x 446, 825, 1203 (thirds of the panel).
- `LOT SIZE` label (mono 9 px, letter-spacing 4, `#5d584c`) y 100; value `0,20` mono 44 px bold yellow `#f5cf1a`
  (subtle glow) + unit `lots` display-sans 18 px `#8a8478`; footnote `1 lot = 100,000 units` display-sans 9 px
  `#5d584c` at y 170.
- `POSITION SIZE` value `20 000` mono 24 px bold white `#ece8de` + `units` display-sans 14 px `#8a8478`.
- `STOP DISTANCE` value `0.00500` mono 24 px bold white (price distance with instrument decimals).
- Direction badge top-right x 1521..1571, y 67..85: `↗ LONG` mono 9 px bold cyan `#3fd0e8` on `#052931`, radius 3.
  (A short trade would show a red `↘ SHORT` badge — assumed.)

### 4.6 RISK / REWARD cards
- RISK: value `100,00` mono 28 px bold red `#f07070` at x 446, y 268; sub `1.00% of balance` display-sans 9 px
  `#5d584c`.
- REWARD: value `200,00` mono 28 px bold cyan `#41cee9`; sub `at take profit`.

### 4.7 TRADE MAP panel
- Three markers placed linearly by price between min(stop, target) and max(stop, target), inset ≈38 px from the
  panel edges: STOP x 470, ENTRY x 829, TARGET x 1547.
- Marker label above (y 383): mono 9 px bold, letter-spacing 2: `STOP` red `#e0605a`, `ENTRY` white `#ece8de`,
  `TARGET` cyan `#3fc6d8`.
- Vertical marker line 2 px from y 396 to 452: red `#f79590`, white `#e4fbfd`, cyan `#60d2d9`.
- Horizontal bar y 419..430 (h 11, no radius): red segment stop→entry `#a15857`/`#994f4b`, cyan segment
  entry→target `#2c8b96`/`#1d8288`.
- Prices under the markers (y 465, centred): mono 12 px bold white `1.11500`, `1.12000`, `1.13000`.
- No hover behaviour.

### 4.8 RISK / REWARD RATIO panel
- Background: radial yellow glow centred on the ratio (centre ≈`#120d04`, edges `#060502`).
- Centred column (centre x ≈1009):
  - `RISK / REWARD RATIO` mono 10 px, letter-spacing 4 px, `#5d584c` at y 617.
  - `1 : 2.00` mono ≈72 px bold yellow `#ffd709` with a soft yellow glow (text-shadow ≈12 px, 40% alpha); glyph
    tops y≈637, baseline y≈690; spans x 838..1180.
  - Verdict `ACCEPTABLE` display-sans 10 px bold yellow `#e6c200` at y 712.
  - Scale bar x 772..1246, y 738..744 (h 6, radius 3): track `#151006`, fill yellow `#f8d61a` from 1:0 to the
    current ratio (ends at x 1008 for 2.0 on a 0–4 scale), a thin dark tick at 1:1 (x 890). Tick labels under it
    (y 754) display-sans 8 px `#3a3630`: `1:0`, `1:1`, `1:2`, `1:3`, `1:4` at x 776, 892, 1008, 1125, 1242.
  - Three stats (labels y 787 mono 9 px letter-spacing 4 `#5d584c`; values y 806 mono 15 px):
    `BREAKEVEN WIN RATE` → `33%` white (x≈848); `RISK` → `100` red `#f07070` (x 1009); `REWARD` → `200` cyan
    `#41cee9` (x 1170).

### 4.9 Footnote
Display-sans 9 px `#3d3a32`, single line at y 944 from x 432:
`Account in USD · position size = risk ÷ stop distance in EUR/USD, converted from USD into USD at the live rate (no conversion needed) · 1 lot = 100,000 units · levels prefilled from the live price, yours to change.`
The pair and the conversion phrase change with instrument / account currency.

## 5. Interactions

- **Instrument combo**: click opens an inline, searchable list (search auto-focused); hover highlights a row;
  click selects (not shown) → title badge, footnote, pip/lot rules and prefilled levels update from the live price.
- **Inputs**: plain numeric line edits; every keystroke recomputes all outputs (no Calculate button).
- **% / $ toggle**: switches the meaning of "risk per trade" between % of balance and an absolute amount in the
  account currency; recomputation is immediate (t16.5).
- **Direction**: LONG when stop < entry, SHORT when stop > entry (badge).
- **Take profit optional**: when empty, REWARD / R:R / breakeven would show placeholders (not shown).
- No hover effects on cards or the trade map; no tooltips.
- Animations: title dot pulse (1 s), block cursor blink (~1.06 s period), search caret blink.

## 6. Data and formulas

| Field | Sample (risk 1 %) | Sample (risk $1) | Format | Computation |
|---|---|---|---|---|
| Instrument | EUR/USD | | text | user choice; category tag FX |
| Account balance | 10000 | | raw input | user (could be prefilled from the selected account) |
| Risk per trade | 1 (%) | 1 ($) | raw input | user |
| Entry / Stop / Take profit | 1.12000 / 1.11500 / 1.13000 | | 5 decimals | prefilled from live price (offsets ≈ −50 / +100 pips), editable |
| Risk amount | 100,00 | 1,00 | 2 decimals, comma decimal separator | balance × % or the $ amount |
| Risk % of balance | 1.00% of balance | 0.01% of balance | 2 decimals, dot | risk / balance |
| Stop distance | 0.00500 | 0.00500 | instrument decimals | |entry − stop| |
| Position size | 20 000 units | 200 units | integer, space thousands | risk / (stop distance × quote→account rate) |
| Lot size | 0,20 lots | 0,00 lots | 2 decimals, comma | units / 100,000 (FX contract size) |
| Reward | 200,00 | 2,00 | 2 decimals, comma | units × |target − entry| × rate |
| R:R | 1 : 2.00 | 1 : 2.00 | `1 : x.xx` | |target − entry| / |entry − stop| |
| Verdict | ACCEPTABLE | ACCEPTABLE | uppercase | thresholds unknown (e.g. <1 poor, 1–2 marginal, ≥2 acceptable, ≥3 good?) |
| Breakeven win rate | 33% | 33% | integer % | 1 / (1 + R:R) |
| Direction | LONG | | badge | sign of entry − stop |
| Live rate | — | | | FX conversion quote → account currency; "live price" for prefill (source not named on screen) |

Badge `DETERMINISTIC MATH` stresses that no AI/estimate is involved. No data source name is visible; live prices
must come from the app's market data feed.

## 7. Key frames

- `t000.0.jpg` — default state, block cursor visible.
- `t004.5.jpg` — cursor on the trade-map bar (no hover effect).
- `t008.0.jpg` — instrument list just opened.
- `t011.0.jpg` — list row hover (USD/CHF).
- `t016.5.jpg` — `$` clicked, values recomputed.
- `t018.5.jpg` — final state ($ mode, list open), cursor out of the way.
- `t032.0.jpg` — end frame.

## 8. Open questions

- Mixed number formatting: LOT SIZE and RISK/REWARD use a comma decimal (`0,20`, `100,00`), POSITION SIZE uses a
  space thousands separator (`20 000`), but STOP DISTANCE, R:R, % and the footnote use a dot. Decide one locale
  rule (probably the app locale for money/lots, fixed dot for prices).
- Verdict thresholds and wording for other ratios; colour of the verdict when poor.
- Behaviour with no take profit, invalid inputs (stop = entry, stop on the wrong side of TP), or a SHORT trade
  (badge, map order).
- Full instrument list (only FX majors visible; are indices/metals/crypto included with other tags?) and the
  pip / contract size per instrument class.
- How the list closes (selection, Escape, click outside) — it never closed in the video.
- Whether ACCOUNT BALANCE is prefilled from the Track Record account selector, and which currency is used.
- Rounding: `0,00 lots` for 200 units hides the real 0.002 — should lots show more decimals or a minimum lot step?
