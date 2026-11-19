# Global — shared visual language (widget kit)

Measured on `Global/overview` (the reference page for the whole app) and `Global/market-hours`, frames at
1600x966. Hex values come from `tools/pick` on flat areas; for thin text the brightest core pixel is given
(the true glyph colour may be a little more saturated because of JPEG chroma blur).

Note for all specs: the reference recordings are time-lapsed (Market Hours exactly 6x, Overview ≈6x).
Divide on-screen animation speeds by 6 to get real speeds.

## 1. Palette

| Token | Hex | Use |
|---|---|---|
| `bg.page` | `#060402` (`#030201`–`#0c0601`) | Page background, warm near-black. |
| `bg.grid` | `#0f0b05` lines on `bg.page` | Faint background grid (≈40 px cells), visible in empty areas and behind lists. |
| `bg.panel` | `#050403` → `#0d0802` | Panel body (very slightly warmer toward the centre). |
| `bg.panel.lifted` | `#1d1704` | Lifted olive zone inside a panel (Risk Sentiment body). |
| `bg.strip` | `#201804` → `#151602` | Top bars (market tape, page strips `#110b04`). |
| `bg.row` / `bg.row.hover` | `#010101` / `#111111` | List rows (Squawk feed). |
| `bg.row.selected` | `#2B2405` | Highlighted registry rows. |
| `border.panel` | `#433D2C` (`#3f3a28`–`#46422d`) | 1 px panel border. |
| `border.strip` | `#464230` | Tape bar border. |
| `border.divider` | `#101010` (black panels) / `#181714` / `#1e1a0f` (olive panels) | 1 px separators. |
| `bracket` | `#807752` (top-left), `#716B3D`–`#9A8C50` (bottom-right) | Corner brackets. |
| `accent` | `#FCD700` | Primary yellow: LIVE pill, selected pills, active timeframe, cursor block, now markers. |
| `accent.text` | `#EBD88A` (core `#FFE37F`) | Panel titles and yellow labels. |
| `accent.soft` | `#F4D95D` | Yellow in sentences (subtitle highlight, dropdown text). |
| `accent.bg` | `#332B03` | Active tab box, olive badge background. |
| `accent.bg.strong` | `#594C01` / `#4B3D0A` | Institution badges / crosshair x pill. |
| `accent.rule` | `#6B6045` over `#4E3E03` | 2 px vertical rule on the left of lists. |
| `text.primary` | `#F5F2EB` (pure white `#FFFFFA` for numbers) | Values, names. |
| `text.secondary` | `#A9A7A0` / `#98948A` | Symbols, tabs, axis-adjacent labels. |
| `text.tertiary` | `#8A877F` / `#8C8F8E` | Axis labels, times. |
| `text.dim` | `#605C55` / `#575B5C` | Section micro-labels, ages, column headers (`#66635B`). |
| `text.faint` | `#4C4E4E` | Footers (`Terminal Data · …`). |
| `neg` | `#E0666B` (coral red; big glyphs `#FE938A`, bars `#E87F78`) | Negative change, risk-off. |
| `neg.dim` | `#7E6261` | Low-importance negative (CUT probabilities). |
| `pos` (tape) | `#5FBF7F` | Positive change in the market tape. |
| `pos.wire` / `info` | `#4FD1DA` (`#35CDE4`) | Positive moves in the Squawk wire, risk-on, HIKE probabilities. |
| `pos.wire.dim` | `#4F7E81` | Low-importance cyan. |
| `neutral.zone` | `#F7D950` | Neutral zone (risk 40–60). |

Session colours (Market Hours, globe markers, tape clocks):

| Session | Bright | Dim (bars) | Card accent |
|---|---|---|---|
| Sydney | `#A78BFA` (`#B5A2FF`) | — | `#594F66` |
| Tokyo | `#E0645A` (est.) | `#884B43` / arc `#6E3434` | `#714031` |
| London | `#3FB6C4` (est.) | `#22707C` / arc `#326860` | `#385C66` |
| New York | `#E3C23A` (est.) | `#86710F` / arc `#70661F` | `#766006` |

Currency series (Currency Strength, reuse for any FX-major colouring):
CAD `#F05154`, NZD `#FCEB50`, AUD `#FBA800`, GBP `#00C6DA`, JPY `#F7F6F6`, CHF `#B864C6`, USD `#5DB963`,
EUR `#387AF8`.

Zone bands (0–100 scores): 80–100 `#12312C`, 60–80 `#182215`, 40–60 `#14130A`, 20–40 `#2B190F`, 0–20
`#3E2015`.

## 2. Fonts

- **Mono — JetBrains Mono** (dotted zero, visible in the big `08:18`): all numbers, tickers, table cells,
  titles, labels, pills, axis labels. Weights: Regular, SemiBold, Bold. Tabular figures everywhere.
- **Body — Inter-like proportional sans**: news text (14 px, line-height 21 px), descriptions (12 px),
  month labels and some axis labels (11 px), central-bank names (12 px), footers (10 px).
- **Display — bold squared grotesque** (e.g. Chakra Petch / Archivo Bold style), uppercase: country names
  in lists (13 px), session card names (28 px).
- Micro-labels: JetBrains Mono 9–10 px, uppercase, letter-spacing 2.5–3.5 px, `text.dim`.
- Panel titles: JetBrains Mono Bold 13 px, uppercase, letter-spacing ≈3.5 px, `accent.text`.

## 3. Panel

- Rectangle, 1 px `border.panel`, radius ≈2 px, body `bg.panel`. Panels are separated by 7–8 px gutters;
  page margins 7–8 px (Overview) or 14 px (Market Hours).
- **Corner brackets**: top-left L inside the panel, inset 4–5 px from the border, arms ≈10 px, 2 px
  thick, `bracket`; bottom-right mirrored L (arms ≈10 px) with the same inset. Every panel and card has
  both.
- **Title row**: title at panel x+11, baseline ≈16 px under the top (inside the bracket), then an ⓘ icon
  (10 px circle, 1 px stroke, `#5F5A50`, small "i") 8 px after the text. Optional controls right-aligned
  on the same row. A 1 px divider under the header row in some panels.
- **Variant "terminal title"** (lists, page strips): small yellow dot 5 px + micro-label in `text.dim`
  letter-spaced + a **blinking block cursor** 6×11–12 px `accent` + optional right-aligned count in
  `text.dim`. Examples: `• ASSET REGISTRY ▮ 23`, `YOUR CLOCK · AMERICA/BOGOTA ▮`,
  `⏱ • MARKET HOURS — THE FOUR FX SESSIONS ▮`.
- **Page strip** (Market Hours, other pages without the tape): full width, h 36–37, bg `#110b04`, bottom
  border 1 px `#4b3e21`; terminal title on the left, outlined badge on the right
  (`• 0/4 SESSIONS LIVE · YOUR CLOCK UTC-5`: bg `#1e1803`, border 1 px `#8A7E45`, text Bold 9 px
  `#FFF2B0`, letter-spacing 1 px).

## 4. Pills, badges, buttons

| Widget | Look |
|---|---|
| **LIVE pill (solid)** | bg `accent`, radius 3, `● LIVE` JetBrains Mono Bold 12 px `#0B0800`, h 21, padding 8 px. |
| **LIVE badge (outlined)** | bg `#342A03`, border 1 px `#968612`, radius 3, `● LIVE` Bold 11 px `#F8E169`, dot 4 px. Used in footers. |
| **LIVE tag (flat)** | bg `#36320F`, radius 3, `LIVE` Bold 10 px `#E6CB4A`. Next to section names (SQUAWK). |
| **Selected pill / segmented control** | bg `accent`, radius 2–3, text black Bold; unselected items plain text `text.secondary`, hover text `accent`. Used for timeframes (`5m 15m 1H 4H D W`) and region tabs. |
| **Tab with count** | `LABEL count`; active = box bg `accent.bg`, label `#FFE278`, count `#66614A`; inactive label `#83878A`, count `#646367`. |
| **Institution badge** | `• FED`: bg `#594C01`, radius 2, Bold 9 px `#FBEBAA`; dim variant bg `#1C1805`, text `#615E51`. |
| **State pill** | `● CLOSED`: bg `#0F0D09`, border 1 px `#3A372E`, radius 3, JetBrains Mono 10 px letter-spacing 2 px `#8E8B84`, dot 5 px. |
| **Toggle button** | ≈44×28, radius 3, icon + code (`⇄ FR`). On: bg `#2C2508`, border `#4A3F10`, text `#FFE477`. Off: bg `#060606`, border `#1B1B1B`, text `#9A9A9A`; hover border `#5C5C5C`. |
| **Dropdown** | bg `#231D07`, radius 2, h 20, text JetBrains Mono Bold 11 px `#F4D95D`, yellow chevron `⌄` right. |
| **Boxed chip** | bg `#0A0806`, border 1 px `#262219`, radius 2, h 22 (e.g. `1W ▼ 1 (WAS 33)`). |
| **Series end label** | solid series colour, h 17, radius 1, `CODE` + 1 px dark separator + value; white text on dark colours, `#3A3200` on light ones (yellow, white); stacked without gap when they collide. |

## 5. Tables and lists

- No zebra striping. Rows are separated by 1 px dividers (`#171717` on black, `#1E1A0F` on olive) or by
  spacing only (Central Banks, pitch 24.4 px).
- Column headers: JetBrains Mono 9 px, uppercase, letter-spacing 1.5 px, `#66635B`. Numbers
  right-aligned, text left-aligned.
- Emphasis rule: the dominant value of a group is Bold and bright, the others regular and dim (e.g. hold
  vs hike probabilities; cut always dim).
- Lists with an index: `01` … in `#615C45`, then flag, then name.
- Highlighted row: bg `#2B2405` + 2 px left bar `#FAE77C`.
- Hover row: bg `#111111` (black lists).
- Left accent rule: 2 px `#6B6045` along the whole list's left edge.
- Scrollbars: none visible in any frame (lists overflow and are clipped; use an overlay scrollbar that
  appears only while scrolling, thin, `#3a3528` thumb).

## 6. Charts

- Plot backgrounds are the panel bg (no frame). Axis labels on the right (values) and bottom (time),
  JetBrains Mono 11 px `#8D8A83`/`#928E88`; month names in body font 11 px.
- Zero line: 1 px solid `#8F8C86`. Gridlines (when present): 1 px `#2A2618`, dotted at mid-levels.
- Lines 1.5–2 px, no fill; risk-style lines can be coloured per segment by zone and get a soft glow.
- **Crosshair**: vertical dashed line (yellow at ~60 %), horizontal dashed line (olive `#6B5A1E`), a dot
  on each series at the hovered x, **x pill** at the bottom (bg `#4B3D0A`, radius 2, h 22, Bold 11 px
  `#F2EEDC`, `20 Oct '26   20:00`) and **y pill** on the value axis (bg `#4C3C04`, 1 px lighter border,
  Bold 11 px white). Pills are drawn above axis labels. The hovered date is also echoed in the chart
  subtitle (` · 20 OCT, 20:00`).
- **Live last point**: dot + pulsing ring (r≈10 px, series colour ~50 %).
- Mouse-wheel zoom anchored on the hovered bar.
- Date formats: crosshair `DD Mon 'YY   HH:MM`; subtitle `DD MON, HH:MM` (uppercase); axis day numbers,
  `Oct` at a month change, `HH:MM` when zoomed in; tables `DD Mon YYYY`; clocks `HH:MM:SS`; long date
  `FRIDAY, 23 OCTOBER 2026`.

## 7. Number formats

- Thousands separator `,`, decimal `.` (also in FR mode for numbers outside translated text).
- Prices: FX 4 dp (JPY pairs 2 dp), metals/energy 2 dp, indices 1 dp (tape) or 2 dp (wire).
- Changes: signed `+0.41%` / `-3.61%` with 2 dp; end labels omit the `+`.
- Probabilities: integer `%`, small ones 1 dp (`0.2%`).
- Countdowns: `1d 22h 41m`; relative ages `1h ago`.

## 8. Recurring elements

- **Footer** inside a feed panel: left `Terminal Data · HH:MM:SS` (body 10 px, `#656867`, time of the
  newest item), right the outlined LIVE badge; 1 px divider above.
- **Session clocks** in the market tape: dot + city name (12 px `#EBE7D8`) over `Closed`/`Open`
  (12 px `#5F5A49`).
- **Blinking cursor block** after terminal titles (yellow, ≈6×12 px); several cursors on one page blink
  in sync.
- **Flags**: flat rectangular flags, 18×12 (tables), 20×13 (session rows/cards), 22×15 (registry), no
  border, no rounding.
- **Info icon ⓘ** after every panel title (tooltip content not seen).
- **Animations** are slow and continuous (scrolling tape/wire, rotating globe, rotating clock sweep,
  pulsing rings, blinking cursor); state changes (filters, tabs, language) are instant, no transitions.
