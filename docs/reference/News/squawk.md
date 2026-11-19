# News › Squawk — reference spec

Source video: `Reference/News/squawk.mp4` (41 s, 83 frames at 0.5 s, 1600x966).
All coordinates are pixels in the 1600x966 page area (the app header is not in the video).
Colours were measured with the pixel picker on JPEG frames. Small text is anti-aliased, so text
colours are given as "measured core" plus a recommended token where the core is washed out.

## 1. Purpose

The real-time wire. One screen shows three live things side by side: on the left a scrolling squawk
list of timestamped headlines, price moves and fresh data releases, with a scrolling price-move ticker
on top; on the right a live TV player (Bloomberg / Yahoo Finance / CNBC YouTube streams) above a compact
economic calendar with countdowns to the next releases. Everything is timestamped to the second and
updates without user action.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–5.0 | Idle. List at top (newest `11:50:00 just now`). Ticker scrolls leftwards continuously. TV plays (Bloomberg, "Long-Term Pressure Eases in Crude" chart). Cursor rests on the 11:00:33 row. |
| 5.5 | Mouse-wheel scroll of the squawk list by about 457 px (no snapping: a row is cut at the top). Rows down to `09:41:13 2h ago` visible. Row `10:36:44` under the cursor is highlighted. |
| 6.0–7.5 | Idle in scrolled state. |
| 8.0 | List scrolled back to the top. Row `11:04:18` (2-line row) hovered and highlighted. |
| 8.5–14.0 | Cursor moves over rows; each hovered row highlights. Countdowns in the calendar tick every second. |
| 14.5 | Cursor over the `⚡ MOVERS` tab: tab text brightens (hover). No click. |
| 15.0–18.0 | Cursor over `RELEASES` tab (hover only, the list does not change; `DATA` stays selected). |
| 19.0 | Minute rollover: every relative age updates at once (`just now` → `1m ago`, `4m ago` → `5m ago` …). Hover row `11:05:54`; 19.5 hover `11:01:09`. |
| 20.0 | Wheel scroll: list now starts at `10:49:36`. Hover `10:33:28`. |
| 20.5–29.0 | Hover moves over rows (`10:39:01` at 22.5 …). Ticker keeps moving. |
| 29.5 | Small wheel scroll; list top at `10:49:36` stays, lower rows shift. |
| 31.5 | Hover row `10:32:15`. |
| 33.5 | Cursor enters the calendar; row `5-Year Note Auction` highlighted. |
| 34.5 | Calendar list wheel-scrolled to the AUD 19:30 CPI cluster; hovered row highlighted. |
| 35.0–37.5 | Hover on calendar rows, countdowns tick (`07:38:37` → `07:38:36` …). |
| 38.0–41.0 | Cursor back on the squawk list, hover `10:31:45` then `10:32:15`. Video ends. |

There is no click in the whole video; only hover, wheel scroll, live ticking and the TV stream.

## 3. Layout (1600x966)

```
x=0                                   933|935                                       1600
+---------------------------------------+-------------------------------------------+ y=0
| ((•)) • SQUAWK [LIVE] subtitle   文A EN  465 ITEMS | ▭ TERMINAL TV [B Bloomberg][Y Yahoo][C CNBC] [⧉ Open on YouTube]
+---------------------------------------+                                            | y=40
| ticker (marquee)                      |      +------------------------------+     |
+---------------------------------------+ y=80 |   16:9 video  (1005..1532)    |     |
| ALL 465  [DATA 427]  ⚡MOVERS 25  RELEASES 13 |   y 40..350                  |     |
+---------------------------------------+ y=120+------------------------------+     |
| HH:MM:SS  age  ●  headline …          |  caption line (y≈372)                      |
| HH:MM:SS  age  ●  headline … (2 lines)|  [🔍 Search event, country, or currency…] y393-421
| …  (scroll area)                      |  ‹ Oct 25 – Oct 31, 2026 ›             y≈445 |
|                                       |  COUNTRIES [flag USD]…[More… ⌄]        y≈478 |
|                                       |  IMPACT [All 198] High 35 Medium 22 Low 141 y≈511
|                                       |  TIME CURRENCY EVENT  ACTUAL FORECAST PREVIOUS y≈545
|                                       |  ● Tuesday, October 27 [TODAY]         y≈576 |
|                                       |  calendar rows 59 px (scroll area)          |
+---------------------------------------+-------------------------------------------+ y≈938
| Terminal Data · 11:45:37      [● LIVE]| Terminal Data · 11:50:04          [● LIVE]| y≈940-966
+---------------------------------------+-------------------------------------------+
```

- Two columns. Left column x 0–933 (squawk). A 1–2 px vertical divider at x 933–934 (`#1b1806`).
  Right column x 935–1600.
- Left column: header row y 0–40, ticker y 40–80, filter-tab row y 80–120, list y 120–938 (scrolls),
  footer y ≈ 938–966.
  A thin amber line runs along the left edge of the left panel (x = 1, `#6f5b14`).
- Right column: TV block y 0–383 (title row y 0–35, video y 40–350, caption y ≈ 366–378),
  calendar block y 385–938 (search, week navigation, filters, header row, then scrolling rows), footer y ≈ 938–966.
- Stretching: the squawk list and the calendar row area take all extra height. Extra width is most likely
  split between the two columns (≈ 58 % / 42 % here); the video keeps 16:9 and is centred in its column.
  Headline text wraps at the column width.

## 4. Components

### 4.1 Squawk header (x 0–933, y 0–40)
- Icon `((•))` (broadcast symbol) at x 12–40, amber `#d7c27c` core / token `#E5B112`.
- `• SQUAWK`: monospace, bold, ≈ 14 px, uppercase, letter-spacing ≈ 3 px, amber (core `#e5b112`, highlights `#fbde80`). x 45–110, baseline y ≈ 26.
- `LIVE` badge: x 120–150, y 12–28, background `#332802`, text amber `#d4ac14` bold mono ≈ 10 px, rounded 2 px.
- Subtitle: `DATA headlines + real-time price moves + fresh data releases`, proportional ≈ 12 px, grey `#5c584f`, x 158–470.
- Right: button `文A EN` (translate icon + `EN`) x 795–840, y 9–31, 1 px border dim (`#292410`), text `#767266`;
  `465 ITEMS` mono ≈ 11 px, letter-spaced, `#4f4c43`, x 846–920.
- Header background `#0d0f05`/`#0c0602` (very dark warm), bottom separator `#272615` at y ≈ 78–79 below the ticker.

### 4.2 Price-move ticker (x 0–933, y 42–78)
- Endless marquee scrolling right-to-left at a constant speed (≈ 45–50 px/s, estimate), background `#0a0604`.
- Item format: `HH:MM:SS • Name ±X.XX% — now P` then ≈ 40 px gap, e.g.
  `11:50:09 • Coffee -5.73% — now 294.55`, `11:50:09 • AMD +4.34% — now 557.95`, `11:50:09 • Corn -2.12% — now 450.75`.
- Time and name: mono bold ≈ 13 px, near-white `#dbd9d8`. The `•` dot is coloured by sign.
- Percentage: red for negative (`#e0605a`, measured core `#7e2d21`…`#e08e89`), cyan for positive (`#3ec0d6`, measured `#1d97ab`…`#5abdcf`). Always signed, 2 decimals.
- `— now 294.55`: near-white `#d2d0d0`, price with thousands separators (`1,028.50`) and the instrument's natural decimals.
- Items seen: UnitedHealth -2.47% now 408.92; Boeing -10.51% now 170.46; Goldman Sachs +2.86% now 910.09; Coffee -5.73% now 294.55; AMD +4.34% now 557.95; Corn -2.12% now 450.75; Johnson & Johnson +3.12% now 267.44; Palladium -2.28% now 1,028.50; Adobe -2.x%.

### 4.3 Filter tabs (y 88–110)
- `ALL 465` | `DATA 427` | `⚡ MOVERS 25` | `RELEASES 13`. Label mono ≈ 11 px uppercase, then count in a dimmer colour.
- Inactive label `#78756e`, inactive count `#4c4940`; hover brightens the label (≈ `#a09d95`).
- Active (`DATA`): filled pill background `#3d2901`, label amber `#dfbf4d` (core `#d5b307`), count `#594f2e`. No border.
- x ranges: ALL 18–68, DATA 86–148, MOVERS 168–248 (with a small lightning glyph), RELEASES 266–348.
- Counts add up: 427 + 25 + 13 = 465 = ALL = header "465 ITEMS". So DATA = headline wire, MOVERS = price moves, RELEASES = economic releases.
- Thin separator under the tab row at y ≈ 120 (`#2d2a1e`).

### 4.4 Squawk list (x 0–933, y 120–938, vertical scroll)
- One row per item, newest on top. Background `#030201`.
- Row height: 39 px for a 1-line headline, 59–60 px for 2 lines. 1 px separator `#211d12` between rows.
- Columns:
  - Time `HH:MM:SS` at x 15–76: mono bold ≈ 13 px, `#dededc`.
  - Age at x 88–135: mono ≈ 11 px, `#413f39`: `just now`, `1m ago`, `4m ago`, `1h ago`, `2h ago` (minutes under 1 h, then hours).
  - Source icon at x 155–170: small round amber coin with a glyph (`#c6aa3c`, core `#bb9300`), vertically aligned with the first text line.
  - Headline at x 178 → ≈ 880: proportional (condensed geometric sans, Rajdhani/Barlow-like) ≈ 14 px, `#bbbab6`. Wraps to 2 lines; continuation is indented to x 178.
- Hover: whole row background `#1a1002`. No other change.
- Examples (verbatim): `10:49:36 1h ago Iran and Belarus sign memorandum on energy cooperation - IRNA`;
  `10:48:25 1h ago Central Election Committee claims that they have not received a request from Likud to extend the voting hours - Israel's Channel 12 News`;
  `10:32:15 Constellation $CEG …`; `10:31:45 US 52-Week Bill High Yield Actual 4.29% (Forecast -, Previous 4.400%)`;
  `10:29:32 EIB and Canada … - Politico`; `10:15:57 Microsoft AI launches MAI image 2.7 in preview. $MSFT`;
  `10:15:47 Fed bids for 52-Week bills total $4.8 bln`; `10:02:40 Meta AI director …`; `10:02:13 Russian Foreign Minister Lavrov …`;
  `10:00:51 US to sell $115 bln 4-Week bills …`; `09:55:20 Gazprom CEO …`; `09:55:07 Maersk …`;
  `09:41:20 2h ago Russia's Deputy PM Novak …`; `09:41:13 Meta's $25 billion stock sale …`.
  Headline suffix `- SOURCE` (IRNA, Politico, Israel's Channel 12 News) and cashtags (`$MSFT`, `$CEG`) are part of the text.
- Scroll: free wheel scroll (no snap, rows may be cut at the top), no visible scrollbar in the frames (overlay scrollbar that only appears while scrolling, or hidden).

### 4.5 Left footer (y ≈ 940–966)
- `Terminal Data · 11:45:37` mono ≈ 10 px, `#4e4d46`, x 6–115.
- `● LIVE` pill at x 878–925: background `#332600`, 1 px amber border, text `#ceaf49` bold mono ≈ 10 px, amber dot.

### 4.6 Terminal TV (x 935–1600, y 0–383)
- Title `▭ TERMINAL TV` at x 940–1060, y 10–28: TV icon amber (`#83691f`), text mono bold ≈ 13 px, letter-spaced, `#d5d3cd`.
- Channel tabs (y 6–30), each a bordered button with a square letter badge:
  `[B] Bloomberg Television` (x 1070–1210, selected), `[Y] Yahoo Finance` (x 1218–1323), `[C] CNBC Television` (x 1332–1445).
  Selected: background `#3e2e02`, 1 px amber border, badge filled amber, text `#c6aa4d`. Unselected: 1 px border `#2a2418`, text `#bfbeb7`, badge dim.
- Right: `⧉ Open on YouTube` button (x 1483–1592), 1 px dim border, text `#7b776f`.
- Video: x ≈ 1005–1532, y ≈ 40–350 (≈ 527x296, 16:9), black letterbox `#000000` to the column edges. Plays a live YouTube stream with its own captions.
- Caption under the video (y ≈ 372): `Live when the network is broadcasting · powered by each channel's own YouTube stream, not hosted by this app.` proportional ≈ 10 px, `#342f27`.

### 4.7 Calendar filters (x 945–1590)
- Search field y 393–421: background `#140b03`, 1 px border `#2a2217`, magnifier icon, placeholder `Search event, country, or currency...` (`#383329`, proportional ≈ 13 px).
- Week navigator y ≈ 445: `‹  Oct 25 – Oct 31, 2026  ›` mono bold ≈ 13 px `#99948b`, chevrons are buttons.
- `COUNTRIES` label (mono ≈ 9 px, letter-spaced, `#413f35`) then chips `[flag USD] [flag EUR] [flag GBP] [flag JPY] [flag CHF] [flag CAD] [flag AUD] [flag NZD] [🌐 More... ⌄]`.
  Chips ≈ 52x22, 1 px border `#3d3012`, fill `#302503`, flag 18x12 + code mono `#b9b19e`. All eight look selected; `More...` is a dropdown with dim text.
- `IMPACT` label then chips `All 198` (selected: fill `#302c24`, text `#bab8b2`, count dim) `High 35` `Medium 22` `Low 141` (text `#555046`). 35 + 22 + 141 = 198.
- Header row y ≈ 535–556, background `#0f0a06`: `TIME` `CURRENCY` `EVENT` `ACTUAL` `FORECAST` `PREVIOUS`, mono ≈ 9 px uppercase letter-spaced, `#625e56`. ACTUAL/FORECAST/PREVIOUS right-aligned.

### 4.8 Calendar rows (scroll area y 562–938)
- Day header `● Tuesday, October 27` amber bold proportional ≈ 13 px (`#cd9f0c`) + `TODAY` badge (amber text `#e1b11d` on `#3a2c03`, 2 px radius) at y ≈ 576.
- Event rows 59 px high, separator `#212208`/`#110b03`. Columns:
  - Time `12:00` mono ≈ 13 px. The next upcoming event's time is amber bold (`#dab31b`); others grey `#63625c`.
  - Flag 20x13 + currency code `USD` (`#8b8a81`).
  - Impact: a grey `•` for low impact; two yellow dots `●●` (`#ffd200`) for medium/high (AUD CPI rows).
  - Event name proportional ≈ 13 px `#b1b0ad` followed by a small `⌄` (expander). Long names are truncated with `…` when chips follow (`C…`, `T…`, `Monthly CPI Indi…`).
  - Optional chips after the name: frequency tag `QUARTERLY` / `ANNUAL` (cyan text `#46c3d7`, 1 px cyan-dim border, mono ≈ 9 px bold) with the word `inferred` in dim grey inside the same chip; then a range chip `1.0% – 1.3%` (amber text `#f0d070`, 1 px amber border `#bd9c19`, mono bold ≈ 10 px).
  - ACTUAL column: before release it shows a countdown on two lines, `in` / `00:09:02`, mono ≈ 13 px red (core `#e0574a`, AA `#d8786e`), right-aligned at x ≈ 1425. It decreases every second. After release it would show the actual value.
  - FORECAST and PREVIOUS: mono ≈ 13 px, right-aligned (x ≈ 1508 and 1590), FORECAST `#898882`, PREVIOUS `#6c6b64`; missing value `—`. Units kept as published (`5.033%`, `23.34T`, `0.800M`, `-4.930M`, `1.3%`, `4.20%`, `102.31`).
- Hover: row background `#1a1000`–`#1b1300`.
- Right footer: `Terminal Data · 11:50:04` (`#484840`) and `● LIVE` pill at x ≈ 1545–1595, same style as the left footer.

## 5. Interactions

- Hover squawk row → background `#1a1002` (instant, no fade visible at 0.5 s).
- Hover filter tab → label brightens; click (not shown) presumably filters the list; selected tab gets the amber pill.
- Mouse wheel on squawk list → smooth free scroll (≈ 450 px per gesture in the video).
- Hover calendar row → background `#1a1000`.
- Mouse wheel on calendar rows → independent scroll of the calendar list (filters and header stay fixed).
- Live behaviour: new items prepend to the list; relative ages refresh every minute (all rows at once, t = 19.0);
  ticker marquee scrolls continuously; countdowns tick each second; the TV stream plays.
- Not shown but implied by the controls: TV channel switch, `Open on YouTube` (opens the channel's live URL in the browser),
  `EN` translate toggle for headlines, calendar search, week ‹ ›, country chips, impact chips, `⌄` expander on an event (probably shows details/history).

## 6. Data

| Field | Samples | Format | Update | Source on screen |
|---|---|---|---|---|
| Squawk headline | see 4.4 | time `HH:MM:SS`, age, text with `- Source` suffix | real time (push) | suffix names (IRNA, Politico, Israel's Channel 12 News); footer "Terminal Data" |
| Release item in list | `US 52-Week Bill High Yield Actual 4.29% (Forecast -, Previous 4.400%)` | text with Actual/Forecast/Previous | at release | — |
| Item counts | ALL 465, DATA 427, MOVERS 25, RELEASES 13 | integer | live | — |
| Price move (ticker / MOVERS) | Boeing -10.51% now 170.46 | signed %, 2 dp; last price | live, when a move threshold is crossed | — (computed: % change, likely vs previous close) |
| TV stream | Bloomberg TV, Yahoo Finance, CNBC TV | YouTube live embed | live video | "powered by each channel's own YouTube stream" |
| Calendar event | 12:00 USD 5-Year Note Auction, prev 5.033% | time, ccy, impact, name, actual/forecast/previous | live countdown 1 s; values at release | footer "Terminal Data" |
| Inferred forecast | `QUARTERLY inferred [1.0% – 1.3%]`, fc 1.3% prev 0.6% | range chip + frequency tag | at calendar refresh | computed by the app ("inferred") |
| Calendar counts | All 198, High 35, Medium 22, Low 141 | integer | per week | — |
| Footer stamps | `Terminal Data · 11:45:37` (left), `· 11:50:04` (right) | HH:MM:SS | static during the 41 s → last refresh/receipt time | "Terminal Data" |

Calendar rows seen (Tuesday Oct 27): 12:00 USD 5-Year Note Auction (— / 5.033%); 12:00 USD M2 Money Supply (— / 23.34T);
12:00 EUR ECB President Lagarde Speaks; 15:30 USD API Weekly Crude Oil Stock (0.800M / -4.930M); 19:15 AUD RBA Assist Gov Bullock Speaks;
19:30 AUD (QUARTERLY inferred, 1.0%–1.3%) 1.3% / 0.6%; 19:30 AUD C… (ANNUAL inferred, 3.7%–4.0%) 3.9% / 4.0%; 19:30 AUD CPI Index Number — / 102.31;
19:30 AUD (QUARTERLY inferred, 0.9%–1.1%) 1.0% / 0.8%; 19:30 AUD T… (ANNUAL inferred, 3.5%–3.7%) 3.6% / 3.6%; 19:30 AUD Monthly CPI Indi… (3.8%–4.3%) 4.20% / 4.00%;
19:30 AUD Weighted me… (QUARTERLY inferred) … 0.7%. Countdowns: `00:09:02` (12:00) and `07:38:37` (19:30) at the same moment.

## 7. Key frames

- `t000.0.jpg` — idle, list at top, all three blocks.
- `t005.5.jpg` — list scrolled, hovered row.
- `t008.0.jpg` — hover on a 2-line row.
- `t015.0.jpg` — hover on RELEASES tab.
- `t019.0.jpg` — minute rollover of ages.
- `t020.0.jpg` — list scrolled to 10:49:36.
- `t031.0.jpg` — clean reference for colours (header, tabs, calendar).
- `t034.5.jpg` / `t036.0.jpg` — calendar scrolled to CPI cluster with inferred chips, hovered row.

## 8. Open questions

- Exact meaning of the subtitle's leading `DATA` (literal text, or the selected tab's name echoed?).
- Whether `ALL/DATA/MOVERS/RELEASES` are exclusive filters or toggles (no click in the video).
- The amber source icon in each row: one generic icon or per-source logos? Only one design seen.
- MOVERS threshold (≥ 2 %?) and reference price (previous close vs intraday open).
- Ticker scroll speed (estimated 45–50 px/s) and whether it pauses on hover.
- What the left footer time (11:45:37) refers to vs the right one (11:50:04).
- Impact dot mapping (one grey dot = Low; two yellow dots = Medium or High?). No three-dot row seen.
- Content of the `⌄` expander on calendar events and of `More...` countries.
- `EN` button behaviour (translation of headlines?).
