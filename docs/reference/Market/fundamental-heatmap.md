# Market › Fundamental Heatmap

Reference video: `Reference/Market/fundamental-heatmap.mp4` (34 s, 68 frames at 0.5 s, 1600x966).
Frames: `frames/Market/fundamental-heatmap/tNNN.N.jpg`. Colours measured with `tools/pick` (JPEG noise ±4).
User description: *"Currencies scored on their fundamentals: growth, inflation, rates, employment."*

## 1. Purpose

A "macro file" per major currency (AUD, EUR, USD, NZD, GBP, CHF, JPY, CAD). A strip of 8 score cards ranks the
currencies (bullish / neutral / bearish). Selecting one shows its central bank policy (rate, last move, next meeting,
inflation vs the 2 % target gauge, 10-year history charts of the policy rate and CPI), an AI-written reading of the
latest releases ("Lecture fondamentale d'Alfred"), a pair comparator against another currency with a verdict, and a
heatmap table of all 8 currencies across Rates / Inflation / Growth / Labour / Activity / Sentiment with a Bias.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–4.5 | Default: AUD selected. Cursor over the inflation gauge (no tooltip). Gold block cursors after `FUNDAMENTALS — MACRO FILE BY CURRENCY` and after `MACRO FILE · AUSTRALIA` blink (≈1 s period, each on its own phase). |
| 5.0–8.0 | Cursor over Alfred's bullets, then the pair comparator tabs (no hover style visible), the column headers, the 5Y/10Y buttons, the MACRO FILE label. |
| 8.5–10.0 | Cursor on the AUD card (selected: gold border). |
| 10.5–12.0 | AUD card border turns light grey `#6a6b66` (the gold "just selected" border fades to the regular selected/hover border ~10 s after selection or on hover). |
| 12.0–13.0 | Hover EUR, then USD card. |
| 13.5 | Click **USD** → USD card gold border; whole page swaps instantly to the United States file (policy, gauge, charts, Alfred text, comparator now `USD ⇆ EUR` tab EUR active, heatmap USD row highlighted). |
| 15.0 | USD card border fades to grey (≈1.5 s after the click). |
| 16.0–17.0 | Cursor moves to GBP. |
| 17.5 | Click **GBP** → transition frame: charts **draw left-to-right** (line + fill visible only up to ≈2023.8); Alfred card shows loading text `Alfred lit les publications…` with no date, so the card is short and the PAIR COMPARATOR jumps up to y 236. |
| 18.0 | GBP fully drawn; Alfred text arrives, comparator moves back down. |
| 19.0 | GBP border fades to grey. |
| 20.5–21.5 | Hover CHF, JPY. |
| 22.0 | Click **JPY** → charts drawing (policy line to ≈2022, inflation to ≈2022.3). Alfred text present immediately (cached). |
| 22.5–23.5 | JPY drawn. |
| 24.0 | Hover the inflation chart at Apr 2021: dashed vertical crosshair, dot on the line, tooltip `Apr 2021 / Inflation -1.1% / -3.1 pt vs 2% target`. |
| 24.5–29.0 | Smooth scroll down (top strip sticky). Tooltip `Jun 2020 / Inflation 0.1% / -1.9 pt vs 2% target` stays while the cursor is on the chart. As the page scrolls under the stationary cursor, heatmap cells under it get the hover lift (CONTRACTION AUD at 27.5, EXPANSION EUR 28.5, EXPANSION USD 29.0). At the bottom: full heatmap (8 rows), `▸ JPY RELEASE DETAIL` button, footer legend. |
| 29.5–33.5 | Scroll back up to the top; tooltip reappears when the chart passes under the cursor (32.5–33.5). |

## 3. Layout (1600x966, page bg `#040302`)

```
y  0 ┌ 🏛 • FUNDAMENTALS — MACRO FILE BY CURRENCY ▌                  [• 8 ECONOMIES · LATEST PUBLISHED RELEASES] ┐ h36 sticky
  52 │ [01 AUD +18][02 EUR +11][03 USD +7][04 NZD +4][05 GBP -4][06 CHF -12][07 JPY -25][08 CAD -39]          │ h93
 157 │ MACRO FILE · AUSTRALIA ▌                                     ┌ LECTURE FONDAMENTALE D'ALFRED (1157–1586)┐│
 175 │ 🇦🇺 AUD                                                       │ headline, paragraph, 3 bullets           ││
 222 │ ─────────── gold rule ───────────────────────────            └──────────────────────────────────────────┘│
 236 │┌ MONETARY POLICY · RBA (x14–1145, h476) ─────────────────┐  ┌ PAIR COMPARATOR (y463–875) ──────────────┐│
     ││ POLICY RATE 4.60% | LAST MOVE · HIKE | gauge             │  │ AUD ⇆ [EUR][USD][NZD][GBP][CHF][JPY][CAD]││
     ││ HISTORY [5Y][10Y][20Y][MAX]                              │  │ table 7 rows                              ││
     ││ ┌POLICY RATE chart─────┐┌INFLATION VS 2% chart───────┐  │  │ FUNDAMENTAL VERDICT box                   ││
 712 │└──────────────────────────────────────────────────────────┘  └───────────────────────────────────────────┘│
 727 │┌ FUNDAMENTAL HEATMAP · 8 CURRENCIES (x14–1145) ───────────┐                                             │
     ││ CURRENCY RATES INFLATION GROWTH LABOUR ACTIVITY SENTIMENT BIAS                                          │
     ││ 8 rows × 64 px                                            │                                             │
     │└──────────────────────────────────────────────────────────┘                                             │
     │ [▸ JPY RELEASE DETAIL]                                                                                  │
     │ footer legend                                                                                            │
     └──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

| Region | x | y | w | h |
|---|---|---|---|---|
| Top strip (sticky) | 0 | 0 | 1600 | 36 (bottom line y 36 `#443b24`) |
| Currency cards | 14 + i·197.3 | 52 | 189 | 93 (gap 8) |
| Macro file title | 14 | 157 | ~1130 | 65 |
| Left column | 14 | 236 | 1131 | — |
| Right column | 1157 | 157 | 429 | — |
| Monetary policy card | 14 | 236 | 1131 | 476 |
| Heatmap card | 14 | 727 | 1131 | ≈572 (8 rows) |

Total scroll height ≈1340 px (max scroll ≈380 px). Left column stretches; right column fixed ≈430 px; cards strip
divides the full width in 8.

## 4. Components

### 4.1 Top strip (y 0–36, bg `#120d04`, bottom line `#443b24`)
- Bank icon (gold outline, 12 px) at x 19; gold dot `●` `#c9bf38` at x 38 (pulsing glow).
- `FUNDAMENTALS — MACRO FILE BY CURRENCY` monospace 10 px, letter-spacing 3 px, `#5a574d`, then a gold block cursor
  `▌` (7x12, `#f2d900`) that blinks.
- Right badge x 1318–1585, y 9–27: 1 px border gold-dim `#6c6229`, bg `#1a1503`, text `• 8 ECONOMIES · LATEST
  PUBLISHED RELEASES` monospace bold 9 px spaced 1.5 px, gold `#e8c84a`.

### 4.2 Currency score cards (y 52–145)
Each card: bg `#0c0601`, 1 px border `#3e381d`; **2 px top accent line** coloured by bias (cyan `#73cad1`/`#00838b`,
neutral light grey `#dfdfdb`, red `#e39a8d`/`#a74026`) with a short glow below.
- Row 1 (y 73): index `01` monospace 9 px `#5a564e` (x+12); flag 20x14; code bold sans 16 px white; score right
  `+18` monospace bold 13 px coloured like the bias (cyan `#329ca1`, light grey, red `#f08784`).
- Row 2 (y 95): score gauge: 2 px dark track across the card (`#1b1b0f`), centre = 0; a 3 px bar from the centre to
  the right for positive (cyan `#40d0e3`) or to the left for negative (red `#ea837e`), length ∝ score; small white dot
  for ≈0 (NZD +4, GBP −4).
- Row 3 (y 104–136): bias pill full width minus 12 px padding, monospace bold 12 px letter-spacing 2 px:
  `BULLISH` bg `#144e54` border `#1d5557` text `#4fd0dc`; `NEUTRAL` bg `#292825` text `#cdcdcd`; `BEARISH` bg
  `#552924` text `#c16762`.
- **Selected** card: 1 px gold border `#bbad7c`/`#d4b030` (t0 AUD, after click); after ~1.5 s it becomes 1 px light
  grey `#625e57`/`#6a6b66` and stays so (selected-idle). Code text gold when selected? (USD/GBP/JPY code turn
  gold-ish `#f0d070` in the clicked frame).
- Order: sorted by score descending (AUD +18, EUR +11, USD +7, NZD +4, GBP −4, CHF −12, JPY −25, CAD −39).

### 4.3 Macro file title
- `MACRO FILE · AUSTRALIA` monospace 10 px spacing 3 px `#5a564e` + blinking gold block.
- Flag 20x14 + code `AUD` heavy sans 32 px `#f3f2ef` letter-spacing 2 px (y 175–210).
- Gold rule y 222, x 14 → 1145, same gradient fade as other pages (`#aca48f` → transparent).

### 4.4 MONETARY POLICY card (x 14–1145, y 236–712)
bg `#0c0601`, 1 px border `#413b27`, gold corner brackets (top-left with a small gold diamond `◆` `#d4b020` before
the title; bottom-right L at 1136,703). Title `MONETARY POLICY · RBA` monospace 10 px spacing 3 px `#7d7f6a`
(central bank short name: `RBA`, `FEDERAL RESERVE`, `BANK OF ENGLAND`, `BANK OF JAPAN`).

- `POLICY RATE` label (monospace 9 px spaced `#5a564e`, y 295); value `4.60%` monospace 44 px weight 500
  `#fffdf1` (y 305–345); `prior 4.35% · 2026-09-29` monospace 10 px `#5a564e` (y 360).
- **LAST MOVE pill** x 205–506, y 309–337: `LAST MOVE · HIKE` monospace 11 px spaced 2 px; HIKE → bg gradient teal
  `#148996`→`#26c4d5`, text `#c8f4f8`; HOLD → bg `#2a2826`, text `#e0ddd6`; (CUT presumably red).
- **NEXT MEETING box** x 205–506, y 343–366, 1 px border `#1a150a`: `NEXT MEETING · 2026-11-03 · IN 11D` monospace
  11 px spaced 2 px `#8a877e`, the `IN 11D` part bold gold `#bfb683`/`#f0d040`.
- **Inflation gauge** (x 520–1133): label `INFLATION VS THE 2% TARGET` (y 283, monospace 9 px spaced 3 px
  `#5a564e`); track y 314, 4 px, horizontal gradient `#0e4b4b` (teal, 0 %) → `#4f4a0b` (olive, 2 %) → `#663b2d`
  (brown-red) → `#6c3c36` (6 %), range 0–6 %; white vertical tick 2x30 at 2 % (x 723); white dot 7 px = core CPI;
  gold diamond 14 px `#fcda0b` with dark outline = headline CPI. When both overlap (JPY 2.1/2.1) the diamond is drawn
  with the white dot inside it plus a soft glow ring. Scale labels 9 px `#4a463e`: `0%` (left), `2% target`
  (centre under the track, x≈800), `6%` (right). Legend y 360: `◆ headline 4%` gold `#eed75d`, `● core 3.6%` white,
  monospace 11 px.
- `HISTORY` label (x 25, y 400) + range segmented `5Y 10Y 20Y MAX` (x 995–1130, y 392–410), active `10Y` solid
  gold `#ffe611` bg black text, inactive `#8a877e` monospace 10 px.
- **Two history charts** (sub-panels bg `#060502`, 1 px border `#14120b`): left x 25–573, right x 585–1133, y
  422–703.
  - Header: title monospace 9 px spaced `#5a564e` (`POLICY RATE · RBA`, `INFLATION VS THE 2% TARGET`), sub-line 9 px
    `#3f3d36` (`BIS monthly · last point = latest decision`, `CPI y/y · BIS monthly · last point = latest release`);
    last value right-aligned monospace 13 px: policy `4.60%` cyan `#83cedb`; inflation `4.0%` red `#e06050` when
    above target, gold `#f0c840` when near target (JPY 2.1 %).
  - Axes: y labels left monospace 10 px `#6f6c63` (`0% … 5%` / `-1% … 8%`, auto range with nice steps: 1 %, 0.3 %,
    2 %), x labels `2018 2020 2022 2024 2026` 11 px `#6f6c63`; faint horizontal grid `#110f08`; a slightly brighter
    zero line.
  - Policy chart: **step line** cyan `#3cc3d4` 1.5 px with glow, fill below to zero with a teal gradient
    (`#0b2120` → transparent), end point dot cyan 4 px.
  - Inflation chart: line gold `#efd66e` 1.5 px; dashed horizontal 2 % target line `#d0d0c8` with label `2% target`
    11 px white-grey at the left; fill **between the line and the target**: above target dark red `#3f1e14`, below
    target dark teal `#0b2120`.
  - Hover (inflation chart): dashed vertical line `#c8c4bc`, white dot with gold ring on the line, tooltip bg
    `#0d0601` 1 px border `#5d4b2a` radius 3, padding 10: `Apr 2021` 11 px `#615b56`; `Inflation -1.1%` 12 px
    white (value bold); `-3.1 pt vs 2% target` monospace 11 px cyan `#2ec8d0` (red presumably when above). Tooltip
    to the right of the line, ~40 px from it, at the target-line height.
  - Draw animation on currency change: line + fill revealed left→right in ≈0.5–0.7 s.

### 4.5 LECTURE FONDAMENTALE D'ALFRED (x 1157–1586, y 157–452)
bg `#151004` (slightly warmer than other cards), 1 px border `#2a2010`; sparkle icon gold; title monospace 10 px
spacing 3 px `#5a564e`; date `2026-10-23` 10 px `#5a564e`; `FR` badge (gold `#533c00` bg, gold text) top-right
(language toggle / indicator).
- Headline bold sans 14 px white, e.g. `RBA tightens amid inflation surge, activity slows sharply`.
- Paragraph proportional 12 px, line-height 21 px, `#c8c4bc`.
- 3 bullets: 12 px, indent 18 px, 1 px left rule `#3a3020`, line-height 17 px.
- Loading state: `Alfred lit les publications…` monospace 12 px `#8a877e` (card collapses to ~65 px).
- The text is AI-generated from the latest releases (name "Alfred" = the app's assistant).

### 4.6 PAIR COMPARATOR (x 1157–1586, y 463–875; moves with Alfred's height)
- Title `◆ PAIR COMPARATOR`. Row: base code `AUD` bold monospace 16 px gold `#f2d060` + `⇆` icon grey + tabs of the
  7 other currencies monospace 11 px `#8a877e`; active tab (`USD`) gold text `#f2e6b3` on bg `#332901`, radius 2.
  Default comparison: USD (EUR when the base is USD).
- Table header (right-aligned) `AUD   USD   DIFF` monospace 9 px spaced `#4a463e`.
- 7 rows, 31 px pitch, 1 px rules `#1d180d`: label proportional 12 px `#c9c6c1`; two values monospace 13 px
  (the **better** value cyan `#20c8dc`, the other white `#e8e5e0`); DIFF monospace 12 px `#8a877e` signed 2 dp
  (base − other).
  Better = higher for Policy rate, Headline CPI, Core CPI, GDP q/q, PMI manufacturing, Retail sales; lower for
  Unemployment. `—` when missing (no colour).
- **FUNDAMENTAL VERDICT** box (x 1167–1575, y 777–867): bg `#0a1512`, 1 px border teal `#364d44`, small radius;
  `FUNDAMENTAL VERDICT · LATEST RELEASES COMPARED` monospace 9 px spaced `#4a5a52`; `Fundamentals favour AUD`
  bold monospace 15 px cyan `#3ad0e0`; `scores AUD +52 · USD +42` and `levels: AUD leads on 3, USD on 3 of 7`
  monospace 11 px `#8a877e`. Bottom-right gold corner bracket on the comparator card.

### 4.7 FUNDAMENTAL HEATMAP · 8 CURRENCIES (x 14–1145, from y 727)
- Title `◆ FUNDAMENTAL HEATMAP · 8 CURRENCIES` + corner brackets.
- Column headers (y 780) monospace 10 px spaced 3 px `#4a463e`: `CURRENCY` (left), `RATES`, `INFLATION`, `GROWTH`,
  `LABOUR`, `ACTIVITY`, `SENTIMENT` (centred), `BIAS` gold bold `#f0c840`.
- Column x ranges: currency 27–171, rates 177–300, inflation 306–456, growth 461–576, labour 582–739, activity
  745–883, sentiment 889–1038, bias 1044–1132; gaps 5–6 px.
- Rows: 64 px pitch, cells 58 px high, radius 2, 1 px border lighter than fill.
  - Currency cell: bg `#0f0a02` (selected: `#362d05` with 1 px gold border `#938243`), flag 20x14, code bold sans
    15 px white (selected: gold), score right monospace 11 px `#4e4428` (`+52`).
  - Signal cells: word monospace bold 12 px spaced 1.5 px + sub-line monospace 10 px (value + unit + trend arrow
    `↑`/`↓`). Fill by meaning: **bright teal** bg `#168089`–`#1f939f` (border `#74b7a9`), text `#9cecf4` / sub
    `#5ccad6` = supports the currency, strongly; **dim teal** `#0f4f58`/`#105e65` = supports weakly (e.g. arrow
    down); **grey** `#282724` text `#e8e6e0` = neutral; **red** `#814239`/`#9a4d48` text `#f08070` = hurts;
    `—` grey empty cell for missing data.
  - Bias cell: `BULLISH` dim teal bg `#005658` text cyan; `NEUTRAL` grey; `BEARISH` dark red `#5a2420` text
    `#f07060`.
  - Selected row: every cell of the row gets a 1 px gold border `#c8a840`.
  - Hover cell: scales up ≈3 % (2 px each side), brighter fill, drop shadow; cursor pointer.
- Below the card: collapsible button `▸ JPY RELEASE DETAIL` (x 14–195, y 906–928, 1 px border `#2a2518`, monospace
  10 px spaced 3 px `#8a877e`) — expands the selected currency's release list (not opened in the video).
- Footer (y 944): monospace 9 px spaced 1.5 px `#4a463e`: `LATEST PUBLISHED RELEASES (ECONOMIC CALENDAR, OFFICIAL
  AGENCIES) · ▲▼ COLOUR = WHETHER THE MOVE SUPPORTS THE CURRENCY (LOWER UNEMPLOYMENT AND JOBLESS CLAIMS SUPPORT IT) ·
  SCORE = MEAN OF SIX ORIENTED SIGNALS`.

## 5. Interactions

| Element | Action | Result |
|---|---|---|
| Currency card | hover | (no strong change; selected card border shows) |
| Currency card | click | selects currency: card gold border (fades to grey after ~1.5 s), macro title, policy card, gauge, charts (animated draw), Alfred (loading then text), comparator base, heatmap row highlight — all instant except charts and Alfred |
| Heatmap row | click | presumably selects the currency too (not shown) |
| Heatmap cell | hover | lift/scale + brighter (seen while scrolling) |
| History range 5Y/10Y/20Y/MAX | click | changes both charts' x range (not shown; 10Y default) |
| Inflation chart | hover | crosshair + dot + tooltip (month, inflation, gap to target) |
| Policy chart | hover | presumably same with rate (not shown) |
| Comparator tab | click | changes the other currency (not shown) |
| `FR` badge | click | language of Alfred's text (not shown) |
| `▸ JPY RELEASE DETAIL` | click | expands release details (not shown) |
| Page | wheel | smooth scroll, top strip sticky |
| Block cursors | — | blink ~1 Hz |

## 6. Data

| Field | AUD | EUR | USD | NZD | GBP | CHF | JPY | CAD |
|---|---|---|---|---|---|---|---|---|
| Card score | +18 | +11 | +7 | +4 | −4 | −12 | −25 | −39 |
| Card bias | BULLISH | BULLISH | BULLISH | NEUTRAL | NEUTRAL | BEARISH | BEARISH | BEARISH |
| Heatmap score | +52 | +45 | +42 | +38 | +31 | +23 | +10 | −4 |
| Rates | HIKE 4.60% (+25bp) | HIKE 2.65% (+25bp) | HIKE 4.00% (+25bp) | HIKE 2.75% (+25bp) | HOLD 3.75% | HOLD 0.00% | HIKE 1.25% (+25bp) | HOLD 2.25% |
| Inflation | ABOVE TARGET 4.0% y/y ↑ | ABOVE TARGET 3.8% ↑ | ABOVE TARGET 3.7% ↑ | ABOVE TARGET 3.7% ↓ (dim) | ABOVE TARGET 3.5% ↑ | BELOW TARGET 1.0% ↑ (grey) | NEAR TARGET 2.1% ↑ (dim) | ABOVE TARGET 3.1% ↑ |
| Growth | EXPANDING 0.4% q/q ↑ | EXPANDING 0.5% ↑ | EXPANDING 2.2% ↑ | EXPANDING 0.2% ↓ (dim) | EXPANDING 0.1% ↓ (dim) | EXPANDING 1.9% ↑ | EXPANDING 0.3% ↓ (dim) | FLAT 0.0% ↓ (red) |
| Labour | STABLE 4.7% unemployment (grey) | STABLE 6.4% | SOFTENING 4.2% (red) | SOFTENING 5.6% | IMPROVING 4.7% (teal) | STABLE 3.1% | SOFTENING 2.5% | SOFTENING 6.5% |
| Activity | CONTRACTION PMI 47.7 (red) | EXPANSION PMI 51.9 | EXPANSION 57.6 | EXPANSION 56.5 | EXPANSION 53.1 | EXPANSION 55.3 | EXPANSION 52.3 | EXPANSION 51.5 (dim) |
| Sentiment | IMPROVING confidence -4.7 | WEAKENING -17.9 (red) | WEAKENING 48.0 | IMPROVING 89.5 | WEAKENING -24.0 | WEAKENING -36.0 | WEAKENING 35.4 | — |
| Bias | BULLISH | BULLISH | BULLISH | NEUTRAL | NEUTRAL | BEARISH | BEARISH | BEARISH |

Policy files seen:
| | AUD | USD | GBP | JPY |
|---|---|---|---|---|
| Bank | RBA | FEDERAL RESERVE | BANK OF ENGLAND | BANK OF JAPAN |
| Rate / prior / date | 4.60% / 4.35% / 2026-09-29 | 4.00% / 3.75% / 2026-09-16 | 3.75% / 3.75% / 2026-09-17 | 1.25% / 1.00% / 2026-09-17 |
| Last move | HIKE | HIKE | HOLD | HIKE |
| Next meeting | 2026-11-03 · IN 11D | 2026-10-28 · IN 5D | 2026-11-05 · IN 13D | 2026-10-30 · IN 7D |
| Headline / core CPI | 4% / 3.6% | 3.7% / 2.6% | 3.5% / 2.8% | 2.1% / 2.1% |
| Chart last values | 4.60% / 4.0% | 3.88% / 3.7% | 3.75% / 3.5% | 1.25% / 2.1% |

Comparator rows (base vs other): Policy rate, Headline CPI y/y, Core CPI y/y, GDP q/q, Unemployment,
PMI manufacturing, Retail sales y/y. Samples: AUD/USD `4.60% 4.00% +0.60; 4.00% 3.7% +0.30; 3.6% 2.6% +1.00;
0.4% 2.2% -1.80; 4.7% 4.2% +0.50; 49.7 56.0 -6.30; — 6.29% —` → verdict AUD, levels 3 vs 3; USD/EUR verdict USD
(scores +42 · +45, levels 6 vs 1 — the verdict follows the level count, not the score); GBP/USD verdict USD (+31 ·
+42, 1 vs 6); JPY/USD verdict USD (+10 · +42, 1 vs 6).

Computed values:
- Heatmap score = mean of six oriented signals (footer), shown ×100-ish (+52…−4). Bias from the score (≥ ~+40
  BULLISH, ~+30…+39 NEUTRAL, ≤ ~+23 BEARISH in this sample).
- Card score (+18…−39) differs from the heatmap score — probably the heatmap score centred on the cross-section mean
  (mean of heatmap scores ≈ +30; AUD 52−30≈+22, CAD −4−30≈−34: close but not exact). Needs a decision.
- Gap to target in the tooltip = inflation − 2.0.
- Days until next meeting.
Sources visible: `BIS monthly` (policy rate, CPI history), "economic calendar, official agencies" (latest releases).
Update: on release (daily check); charts monthly.

## 7. Key frames

| Frame | State |
|---|---|
| `t000.0.jpg` | default AUD file, gold selected card, both block cursors visible |
| `t010.5.jpg` | selected card border faded to grey |
| `t013.5.jpg` | USD selected |
| `t017.5.jpg` | GBP mid-transition: chart draw animation, Alfred loading text, comparator moved up |
| `t018.0.jpg` | GBP done, LAST MOVE · HOLD grey pill |
| `t022.0.jpg` | JPY mid-transition (draw animation) |
| `t024.0.jpg` | inflation chart tooltip |
| `t029.0.jpg` | bottom: full heatmap, release-detail button, footer |

## 8. Open questions

- How card scores (+18…) relate to heatmap scores (+52…).
- Exact mapping rules from values to words (HIKE/HOLD/CUT, ABOVE/NEAR/BELOW TARGET, EXPANDING/FLAT/CONTRACTING,
  STABLE/SOFTENING/IMPROVING, EXPANSION/CONTRACTION, IMPROVING/WEAKENING) and to the bright vs dim teal shades.
- Content of `▸ JPY RELEASE DETAIL` when expanded.
- `FR` badge behaviour (language switch of Alfred's text, or source language indicator).
- 5Y / 20Y / MAX ranges, policy chart tooltip, comparator tab clicks — not exercised.
- Alfred text generation: which model, when refreshed (dated 2026-10-23 = data date).
- Heatmap row click behaviour.
