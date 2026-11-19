# Economy › Rates — reference spec

Source video: `Reference/Economy/rates.mp4` (22 s, 45 frames at 0.5 s, 1600x966).
Coordinates are pixels in the 1600x966 page area (app header not shown). "Unscrolled y" = position at scroll 0.
Colours measured with the pixel picker (JPEG). Small anti-aliased text reads paler than its design colour; flat fills
(bars, pills) give the true colours, which are used as tokens.

## 1. Purpose

Central-bank rate probabilities, meeting by meeting. One panel lists 8 central banks (Fed, ECB, BoE, BoJ, SNB, RBA, RBNZ, BoC)
with current rate, next meeting, implied basis-point move, cut/hold/hike probabilities and a bias label. Below, for the selected
bank (Fed by default): the implied policy-rate path across the next 10 meetings, the implied move per meeting, the next-meeting
probability distribution and the odds history — once from the terminal's own data and once from Polymarket — then the
policy committees with chairs, decisions, votes and dissents.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0 | Top of panel. Fed row selected (amber). Cursor on RBA row → hover. |
| 0.5 | Hover SNB row. Title dot dim (blink). |
| 1.0 | Hover BoE row. |
| 1.5–2.5 | Hover ECB row (cursor moves over its `Hold` pill and `in 6d` badge; no tooltip). |
| 3.0–4.5 | Hover BoE row (cursor moves across IMPL BP and HOLD cells). |
| 5.0 | Hover ECB row. |
| 5.5 | Hover RBA row. |
| 6.0–11.5 | Smooth wheel scroll down, ≈ 100 px per 0.5 s (56, 127, 109, 91, 103, 100, 104, 91, 99, 101, 119, 9 px). Panel title bar stays fixed. Sections revealed: TERMINAL DATA charts, NEXT MEETING PROBABILITY + ODDS EVOLUTION (TERMINAL), POLYMARKET pair, COMMITTEES & VOTES. Final scroll offset ≈ 1112 px. |
| 12.0 | Idle at bottom of the video's scroll. |
| 12.5 | Hover the Polymarket next-meeting bar chart: dashed vertical crosshair slides in, tooltip `+25bps ● 6.5%`. |
| 13.0–14.0 | Crosshair snaps to `-25bps` (`<1%`), tooltip follows cursor. |
| 14.5 | Crosshair on `No change` (`92.5%`), dashed line drawn over the bar. |
| 15.0–15.5 | `+25bps 6.5%`. |
| 16.0 | `+50+bps <1%` (crosshair still sliding: drawn between categories). |
| 16.5 | Cursor in the gap between panels: no crosshair. |
| 17.0–21.0 | Hover across ODDS EVOLUTION (POLYMARKET): full crosshair (vertical + horizontal dashed), y value badge at left, date badge at bottom, series markers, multi-series tooltip. Times shown: 25 Sept 21:00, 26 Sept 07:00, 29 Sept 23:00, 07 Oct 06:00, 10 Oct 21:00, 11 Oct 04:00, 13 Oct 17:00, 17 Oct 06:00, 18 Oct 05:00. Tooltip flips to the left of the crosshair near the right edge (t 20.5+). |
| 21.5–22.0 | Static. |

No clicks in the video.

## 3. Layout (1600x966)

The page is one framed, dockable panel:
```
x=10                                                                                   1590
+--------------------------------------------------------------------------------------+ y=10
| ⠿ INTEREST RATE PROBABILITY                                   ⚙ 📌 🔒 ⌄ ⤢ ×          | (fixed, h 34)
+--------------------------------------------------------------------------------------+ y=44
| • Interest Rate Probability                                                          | y≈66
| NEXT-MEETING OVERVIEW · 8 CENTRAL BANKS                                              | y≈88
| [4 / 8 PRICED TO HIKE] [4 / 8 PRICED TO HOLD] [0 / 8 PRICED TO CUT] [5 / 8 MEETING THIS WEEK] y 99-154
| • $5.4M in real Polymarket liquidity backing these odds across 8 banks               | y≈166
| +-- table: BANK | RATE | NEXT MEETING | IMPL BP | CUT | HOLD | HIKE | BIAS ----------+ y 190-583
| • TERMINAL DATA ─────────────────────────────────────────────────────────────────    | y≈606
| [IMPLIED INTEREST RATE CURVE ⓘ]            [IMPLIED POINTS ⓘ]                        | y 630-1014
| [NEXT MEETING PROBABILITY ⓘ]               [ODDS EVOLUTION (TERMINAL) ⓘ]             | y 1026-1410
| • POLYMARKET ──────────────────────────────────────────────────────────────────      | y≈1443
| [NEXT MEETING PROBABILITY ⓘ]               [ODDS EVOLUTION (POLYMARKET) ⓘ]           | y 1468-1852
| • COMMITTEES & VOTES ──────────                                                      | y≈1885
| POLICY COMMITTEES — CHAIRS, DECISIONS & VOTES     From each bank's own official …   |
| table BANK · COMMITTEE | HEAD | MEETING | DECISION | VOTE | DISSENT | SOURCE          | y≈1925 …
+--------------------------------------------------------------------------------------+
```
- Panel frame x 10–1590, 1 px border `#241f12`; title bar y 10–44 (background `#181205`, 1 px bottom line `#676150`).
  The body (y 44–966) scrolls vertically; the title bar does not.
- Content inset: x 21–1579 (11 px from the frame).
- Stat cards: 4 equal cards, w ≈ 386, gap 6 (x 21, 412, 803, 1194), h 55.
- Two-column chart rows: left panel x 21–795, right x 806–1579 (gap 11), each ≈ 384 px tall, 12 px vertical gap between rows.
- On a wider window every block stretches horizontally; the two chart columns stay 50/50.

## 4. Components

### 4.1 Panel title bar
- `⠿` drag handle (amber) at x 20, then `INTEREST RATE PROBABILITY`, mono bold ≈ 12 px, uppercase, letter-spacing ≈ 1.5 px, amber (`#c5a925` core, highlights `#ffeca1`; token `#E8C547`).
- Right icons x 1476–1580 (≈ 18 px apart): settings ⚙, pin, lock, chevron ⌄ (collapse), expand ⤢, close ×. Thin line icons `#777368`.

### 4.2 Heading block
- Dot (≈ 6 px, amber `#f3ce0b`) that blinks: dim (`#6e6d22`) for ≈ 0.5 s every ≈ 1.5 s. Then `Interest Rate Probability` proportional semibold ≈ 15 px `#eceae7` (x 34, y 59–73).
- `NEXT-MEETING OVERVIEW · 8 CENTRAL BANKS` mono ≈ 9 px uppercase, letter-spacing ≈ 2 px, `#75746e`.

### 4.3 Stat cards (y 99–154)
- 1 px border `#302c22`, background `#0c0702`, radius ≈ 2 px. Padding 11 px.
- Big number mono ≈ 20 px: `4` cyan (`#22D3E0` token; measured `#37cad8`) for HIKE, `4` light grey `#afb0b1` for HOLD, `0` red (`#F05A50` token; measured `#da8e8e` AA) for CUT, `5` light grey for MEETING THIS WEEK; then `/ 8` mono ≈ 12 px `#6c6761`.
- Label below, mono ≈ 9 px uppercase `#555149`: `PRICED TO HIKE`, `PRICED TO HOLD`, `PRICED TO CUT`, `MEETING THIS WEEK`.
- Footnote under the cards (y 166): cyan dot `#4cbcc0` + `$5.4M in real Polymarket liquidity backing these odds across 8 banks` proportional ≈ 9 px `#3b3a36`.

### 4.4 Bank table (unscrolled y 190–583)
- Box 1 px border `#38311e`. Header row y 190–220, background `#0b0a03`; labels mono ≈ 9 px uppercase:
  `BANK` (x 32), `RATE` (right x 580), `NEXT MEETING` (x 591), `IMPL BP` (right x 960), `CUT` (right x 1090, red `#c0504a`), `HOLD` (right x 1219, `#6a6861`), `HIKE` (right x 1349, cyan), `BIAS` (right x 1568). Others `#6d6b62`.
- 8 rows, 45.4 px each, separators 1 px `#292518`.
- Cells:
  - Flag 20x14 at x 32; code `Fed` proportional bold ≈ 14 px `#e7e1cd` (x 59, first line); full name below `Federal Reserve` mono ≈ 10 px `#736b4e`.
  - Action pill at x 370–410 (h ≈ 16, fully rounded): `• Hold` = fill `#34311b`, text `#a4a29b`; `• Hike` = fill `#052523`, text and dot cyan (`#6fbac2` / `#25a8b2`). (A `• Cut` pill would be red; not seen.)
  - RATE: `4.00%` mono bold ≈ 15 px cyan, right-aligned at x 581; second line `as of Oct 22` mono ≈ 9 px `#4b4633` (SNB shows `Sep '26 avg`).
  - NEXT MEETING: `Oct 28 '26` mono ≈ 14 px `#c9c5b5` (x 591); below, badge `in 5d` (mono bold ≈ 9 px, cyan text on fill `#0f3324`, h 12, radius 2).
  - IMPL BP: `+1.7` mono bold ≈ 14 px cyan, right x 960; below `-1.7 1w` mono ≈ 9 px, red when negative, cyan-green when positive.
  - CUT `0.2%` mono ≈ 14 px red; HOLD `92.5%` mono ≈ 14 px `#c9c7c0`; HIKE `6.8%` mono bold ≈ 14 px cyan. One decimal.
  - BIAS pill right-aligned at x 1568 (h 16, radius 3): `NEUTRAL 93` = fill `#171712`, text `#9e9f9e`; `HAWKISH 97` = fill `#072a28`, text cyan `#63b3ba`. Mono bold ≈ 9 px uppercase. (`DOVISH nn` in red implied, not seen.)
- Selected row (Fed): 3 px amber left bar `#f5d648`, background gradient from `#352b04` at the left to `#1d1704` at the right.
- Hovered row: background `#201b05`.
- Rows (code / full name / pill / rate / as of / meeting / in / impl / 1w / cut / hold / hike / bias):
  - Fed / Federal Reserve / Hold / 4.00% / as of Oct 22 / Oct 28 '26 / in 5d / +1.7 / -1.7 1w / 0.2% / 92.5% / 6.8% / NEUTRAL 93
  - ECB / ECB / Hold / 2.50% / as of Oct 23 / Oct 29 '26 / in 6d / +0.5 / -0.3 1w / 0.1% / 97.9% / 1.9% / NEUTRAL 98
  - BoE / Bank of England / Hike / 3.75% / as of Oct 22 / Nov 05 '26 / in 13d / +24.4 / +0.8 1w / 0.3% / 2.6% / 97.4% / HAWKISH 97
  - BoJ / Bank of Japan / Hold / 1.23% / as of Oct 21 / Oct 30 '26 / in 7d / +0.9 / -0.3 1w / 0.1% / 96.5% / 3.3% / NEUTRAL 97
  - SNB / SNB / Hold / 0.00% / Sep '26 avg / Dec 10 '26 / in 48d / +4.6 / +1.2 1w / 0.3% / 79.7% / 18.7% / HAWKISH 19
  - RBA / RBA / Hike / 4.60% / as of Oct 15 / Nov 03 '26 / in 11d / +16.4 / +13.1 1w / 0.2% / 32.0% / 66.0% / HAWKISH 66
  - RBNZ / RBNZ / Hike / 2.75% / as of Oct 16 / Oct 28 '26 / in 5d / +21.8 / +5.3 1w / 0.1% / 11.5% / 87.2% / HAWKISH 87
  - BoC / Bank of Canada / Hike / 2.25% / as of Oct 22 / Oct 28 '26 / in 5d / +19.9 / +15.3 1w / 0.1% / 18.9% / 79.6% / HAWKISH 80

### 4.5 Section headings
- `• TERMINAL DATA`, `• POLYMARKET`, `• COMMITTEES & VOTES`: amber dot + mono bold ≈ 11 px uppercase, letter-spacing ≈ 2 px, amber (`#c3a518` core, `#ffef83` highlight), followed by a 1 px hairline `#211f17` to the right edge.

### 4.6 Chart panels (common)
- 1 px border `#2a2a14`, background `#080603`. Title bar h ≈ 32 with 1 px bottom line; title mono bold ≈ 10 px uppercase `#a39f98` + `ⓘ` circle icon (≈ 9 px, `#4f4c45`).
- Axis labels mono ≈ 10 px `#6a6961`. Horizontal grid lines 1 px `#1f2012`–`#282914`.

### 4.7 IMPLIED INTEREST RATE CURVE (left, unscrolled y 630–1014)
- Y axis left (x 42–80): `4.750%` `4.500%` `4.250%` `4.000%` `3.750%` (3 decimals), spacing 71 px (4.750% at y 695 … 3.750% at y 979).
- X axis (y ≈ 995): `Now`, `Dec 10 '26`, `Mar 17 '27`, `Jun 09 '27`, `Sep 15 '27`, `Dec 08 '27` (every other point).
- 11 points, pitch 62 px from x 119 to 739: Now 4.00%, Oct 29 '26 4.02%, Dec 10 4.21%, Jan 28 '27 4.29%, Mar 17 4.39%, Apr 28 4.44%, Jun 09 4.53%, Jul 28 4.57%, Sep 15 4.58%, Oct 27 4.57%, Dec 08 4.56%.
- Line: smooth (monotone cubic) cyan `#2cb1bf`, ≈ 1.5 px; markers ≈ 5 px dots (`#6fd5dc` with darker ring). Value label above each point mono ≈ 10 px `#8f8d87` (`4.21%`, 2 decimals).
- Area fill under the line from x 119 to 739 (hard vertical edge at the last point): teal gradient `#0d3131` (top) → `#080d0b` (bottom).
- Dashed horizontal baseline at the current rate (4.000%), `#4a5042`, dash ≈ 4/3, across the full plot width; label `Current` proportional ≈ 11 px `#686962` under the first point.

### 4.8 IMPLIED POINTS (right)
- Y axis left (x 820–857): `20 bps` `15 bps` `10 bps` `5 bps` `0 bps` `-5 bps`, 57 px per 5 bps (20 at y 695, 0 at y 923).
- 10 bars, width 29, pitch 69.5, centres x 900 … 1525: Oct 29 '26 +1.7, Dec 10 +19.2, Jan 28 +8.3, Mar 17 +10.0, Apr 28 +5.3, Jun 09 +8.2, Jul 28 +4.0, Sep 15 +1.1, Oct 27 -0.8, Dec 08 -0.6.
- Positive bars flat cyan `#19daf1`; negative bars flat red `#f46a5d` hanging below the zero line. No rounding.
- Value label above each bar (also above the zero line for negatives), mono ≈ 10 px `#97948c`, signed 1 decimal.
- Zero line 1 px `#31291e`. X labels every other bar: `Oct 29 '26` `Jan 28 '27` `Apr 28 '27` `Jul 28 '27` `Oct 27 '27`.

### 4.9 NEXT MEETING PROBABILITY (left; Terminal and Polymarket versions)
- Y axis `100%` … `0%` step 20 % (56 px per step). Five categories, pitch 139 px, bar width 47: `-50+bps` `-25bps` `No change` `+25bps` `+50+bps` (x labels mono ≈ 11 px `#7a7770`).
- Colours by outcome: -50+bps dark red `#912922`, -25bps red `#c84d48`, No change blue-grey `#aab1ba`, +25bps teal `#00c2bd`, +50+bps green `#00aa7c`.
- Bars below 1 % are drawn as a 2 px line on the baseline in their colour; label `<1%`.
- Value label above bar mono ≈ 11 px `#b0aea8`: Terminal `<1% <1% 91.1% 7.4% <1%`; Polymarket `<1% <1% 92.5% 6.5% <1%`.
- Baseline 1 px dark reddish-brown.
- Hover: vertical dashed white line (1 px, dash 4/3) through the hovered category centre, full plot height, drawn over the bar; it animates (slides) from the previous category (seen mid-way at t 12.5 and 16.0).
  Tooltip at cursor + (25, 20): background `#0d0b07`, 1 px border amber-dim `#3a3010`, radius 3, padding 10; line 1 category name proportional ≈ 12 px white; line 2 coloured dot (8 px) + value bold ≈ 12 px white (`6.5%`, `<1%`, `92.5%`).

### 4.10 ODDS EVOLUTION (TERMINAL) / (POLYMARKET) (right)
- Chip row (y ≈ title + 40): `📊 KEY DATA OFF` toggle button (1 px border, orange-amber text `#e0a050`, chart icon), then one chip per meeting:
  Terminal `OCT 29 '26` `DEC 10 '26` `JAN 28 '27` `MAR 17 '27` `APR 28 '27` `JUN 09 '27` `JUL 28 '27` `SEP 15 '27` `OCT 27 '27` `DEC 08 '27` (wraps to a 2nd line);
  Polymarket only `OCT 29 '26` `DEC 10 '26` `JAN 28 '27`. Chips: h 20, radius 10, 1 px border `#2a2418`, mono bold ≈ 9 px `#6b6962`; selected `OCT 29 '26` = fill `#3a2e00`, amber border, text `#d1bf7b`.
- Legend right-aligned above the plot: ring markers (white centre, coloured ring) + labels mono ≈ 10 px `#928d85`: `-50+bps` `-25bps` `No change` `+25bps` `+50+bps`.
- Plot: y `0%`…`100%` step 20 %, x day ticks `25 29 Oct 3 5 9 13 17 21` (month name replaces day 1). Hourly step-like series ≈ late Sep → Oct 23.
- Series: No change `#c4c6c7` and +25bps teal `#1ab4ac` (≈ 1.5 px) each with a translucent fill to the bottom (grey ≈ `#23211b`, teal ≈ `#0e1f17` composite);
  -50/-25 red and +50 green lines hugging 0 %. A dashed reddish horizontal line at 50 %. Grid lines `#1f2012`.
- Hover (Polymarket version shown): vertical + horizontal dashed white crosshair; y badge at the plot's left edge (`75.2%`, fill `#3d3303`, text `#ede6d4` proportional ≈ 13 px);
  x badge on the axis (`25 Sept 2026, 21:00`, same style); small ring markers on each series at the crosshair x;
  tooltip box (background `#0a0600`, 1 px border amber-dim, radius 4): date line grey `#67645c` proportional ≈ 12 px, then 5 rows: coloured dot, name `#c5c2bd`, value bold right-aligned `#dcdad4` (1 decimal):
  e.g. `25 Sept 2026, 21:00 | -50+bps 0.3% | -25bps 0.5% | No change 35.5% | +25bps 64.5% | +50+bps 0.8%`.
  Placed right of the crosshair; flips to the left when it would overflow the panel.

### 4.11 COMMITTEES & VOTES (unscrolled y ≈ 1885+)
- Sub-title `POLICY COMMITTEES — CHAIRS, DECISIONS & VOTES` mono ≈ 9 px letter-spacing ≈ 3 px `#726f6a`; right note `From each bank's own official statement · updated after each meeting` proportional ≈ 9 px `#3c3b36`.
- Table, header mono ≈ 9 px uppercase `#6d6b62`: `BANK · COMMITTEE` (x 30) `HEAD` (x 311) `MEETING` (x 424) `DECISION` (x 511) `VOTE` (x 841) `DISSENT` (x 1046) `SOURCE` (right x 1570).
- One group per bank, one sub-row per recent meeting (≈ 38 px):
  - Federal Reserve (selected group: background `#332901`): name amber `#e0ca76` proportional semibold ≈ 12 px; `FOMC` `#645c3b`; note `Each statement lists every member's vote and names any dissent.` ≈ 9 px dim.
    HEAD: `CHAIR` mono ≈ 8 px `#918a6f` + `Kevin Warsh` semibold `#e5dfcd`.
    `Sep 16, 2026` | `–` | `12–0` (mono bold white) `unanimous` (`#797255`) | `–` | `Official ↗` (`#4e4524` link).
    `Jul 29, 2026` | `–` | `9–3` | `Beth Hammack (+25bp hike), Neel Kashkari (+25bp hike), Lorie Logan (+25bp hike)` (`#888165`) | `Official ↗`.
  - ECB / `Governing Council` / `Individual votes are not published.`; `PRESIDENT` `Christine Lagarde`; `Sep 10, 2026` | `Raised the three key rates by 25bp — deposit facility 2.50%` (`#807d77`) | `Unanimous (per President Lagarde)` | `–` | `Official ↗`.
  - Further banks below the visible area.

## 5. Interactions

- Bank rows: hover highlight; click selects the bank (amber bar + gradient) and drives all charts below (Fed selected by default; inferred, not clicked in video).
- Title-bar icons: settings, pin, lock layout, collapse, maximise, close (panel chrome).
- Body: smooth wheel scroll; title bar fixed.
- Next-meeting bar chart: category crosshair that slides between categories + tooltip.
- Odds evolution: free crosshair with axis badges, series markers and tooltip that flips sides near the right edge; meeting chips select which meeting's odds are drawn; `KEY DATA OFF` toggles key-data event markers.
- `ⓘ` icons: explanatory tooltip (not shown).
- `Official ↗`: opens the bank's statement.
- Live: countdown badges (`in 5d`) and probabilities refresh with the feed; title dot blinks.

## 6. Data

| Field | Samples | Format | Update | Source on screen |
|---|---|---|---|---|
| Policy rate | 4.00%, as of Oct 22; SNB 0.00% Sep '26 avg | 2 dp % + date | daily | — |
| Next meeting | Oct 28 '26, in 5d | `MMM DD 'YY`, days | daily | — |
| Cut/hold/hike probabilities | 0.2 / 92.5 / 6.8 % | 1 dp | live (minutes) | Polymarket (footnote: $5.4M liquidity) |
| IMPL BP | +1.7 (−1.7 1w) | signed 1 dp bp; weekly change | live | computed: ≈ 25 × (P(hike) − P(cut)) (+50 bp tails): Fed 25×(6.8−0.2)%=1.65 |
| Action pill | Hold / Hike | argmax outcome | live | computed |
| Bias | NEUTRAL 93, HAWKISH 19 | label + rounded % | live | computed: HAWKISH = round(P(hike)) when P(hike) is material (≥ ~10 %), DOVISH = round(P(cut)), else NEUTRAL = round(P(hold)) |
| Stat cards | 4/8 hike, 4/8 hold, 0/8 cut, 5/8 this week | counts | live | computed from pills and meeting dates (≤ 7 days) |
| Implied curve | 4.00 → 4.56 % over 10 meetings | 2 dp | live | "TERMINAL DATA"; computed: current rate + cumulative implied points |
| Implied points | +19.2 bps for Dec 10 | signed 1 dp | live | "TERMINAL DATA" |
| Next meeting distribution | 91.1 % no change (terminal) / 92.5 % (Polymarket) | 1 dp, `<1%` | live | TERMINAL DATA / POLYMARKET |
| Odds history | hourly points, ~1 month | % per outcome | live, hourly | TERMINAL / POLYMARKET |
| Committees | chair, meeting date, decision, vote, dissenters | text | after each meeting | "each bank's own official statement", `Official ↗` |

Date conventions: the table shows the first day of a two-day meeting (`Oct 28 '26`) while chips/charts use the decision day (`OCT 29 '26`). Current date in the video ≈ Oct 23, 2026.

## 7. Key frames

- `t000.0.jpg` — top of panel, table, first charts (Fed selected, RBA hover).
- `t002.0.jpg` — hover over a Hold pill row.
- `t006.0.jpg` — full TERMINAL DATA charts with axes.
- `t008.0.jpg` — next-meeting and odds-evolution (terminal) panels.
- `t011.0.jpg` — Polymarket pair + committees table.
- `t014.5.jpg` — category crosshair over the `No change` bar.
- `t017.0.jpg` — odds-evolution crosshair, badges and tooltip.
- `t020.5.jpg` — tooltip flipped to the left near the right edge.

## 8. Open questions

- Is the panel title bar (drag handle, pin, lock, collapse, expand, close) the app's generic dockable-panel chrome? Other pages may not have it.
- What clicking a bank row changes (charts for that bank are inferred).
- Exact bias threshold for HAWKISH vs NEUTRAL (between 6.8 % and 18.7 % in the data) and the DOVISH style.
- Source of "TERMINAL DATA" probabilities (OIS/futures model?) vs table probabilities (Polymarket?).
- `KEY DATA OFF` behaviour; `ⓘ` texts; rest of the committees table below ECB.
- Whether the odds-evolution x range is fixed (≈ 1 month) or selectable.
