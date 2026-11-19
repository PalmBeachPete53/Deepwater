# Audit — Market › Fundamental Heatmap (68 frames, t000.0–t033.5)

Real data (Economy services). States reproduced with `DW_FUND_*` hooks (`FundamentalPage.h`): CCY, FRESH (gold
"just selected" border), REVEAL (frozen draw-in), HOVER, SCROLL, DETAIL. Screenshots:
`SCRATCH/sea/audit/final/fundamental-*.png`.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000 | right column | read card 295 px, comparator from y 463 | card shorter → comparator ~25 px higher | min height 295 | fixed |
| 2 | t000–t033 | comparator | core CPI and retail sales values | "—" (no source then) | connected to MacroStore CoreInflation / RetailSalesYoy | fixed |
| 3 | t017.5, t022 | inflation chart | dashed target line revealed with the draw-in | full width during the animation | clipped to the reveal | fixed |
| 4 | t024 | history charts | axis labels 10–11 px mono | smaller | 11 px | fixed |
| 5 | t029 | footer | `▲▼ COLOUR = …` | no glyph | added | fixed |
| 6 | all | strip | pulsing dot + blinking cursors | 10 fps repaint = 2.4 % CPU | 4 fps tick | fixed (perf) |
| 7 | t017.5 | read card | "loading" text for ~0.5 s, card collapses | rule-based text is instant | — | deviation (AUTOMATED READ, no model latency) |
| 8 | all | read card | "LECTURE FONDAMENTALE D'ALFRED", `FR` badge, French loading text | AUTOMATED READ, `RULES` badge | — | deviation (decided) |
| 9 | all | activity column / comparator | PMI | OECD business confidence (BCI, 100 = avg) | — | deviation (no free PMI) |
| 10 | t000 | gauge | `2% target` under the track middle; 2 % for every bank | label under the tick; bank's own band (RBA 2–3 %, SNB 0–2 %) | — | deviation (honest target) |
| 11 | t000 | cards vs heatmap | two different score scales | one relative score | — | deviation (decided) |
| 12 | t000 | verdict on a tie | names the base | "Fundamentals balanced" + score Δ | — | deviation (decided) |
| 13 | t013.5 | policy card | pill / gauge x shift with content width | fixed columns | — | open: reference inconsistency, not copied |
| 14 | t029 | page bottom | ends at the footer | method footnote below the footer (scoring rules) | — | deviation (rules must be printed) |
| 15 | all | numbers | video values | live values | — | data |

Verified again at 1600x1042 and 1280x800. CPU idle 20 s: 0.8 %.
