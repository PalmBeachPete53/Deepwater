# Audit — Market › Technical Heatmap (84 frames, t000.0–t041.5)

States reproduced (DW_PROFILE, 1600x1042): ALL idle (t000–t012) `DW_TH_*` none · hover chips (t012.5, t016.5, t021, t025.5,
t029.5, t038) `DW_TH_HOVER=x,y` · FX (t013.5–t016) `DW_TH_CATEGORY=1` · COMMODITIES transition (t017.5)
`DW_TH_CATEGORY_AT=7000:2` · COMMODITIES/INDICES/CRYPTO (t018–t029) `DW_TH_CATEGORY=2/3/5` · empty state (t030.5–t033)
`DW_TH_SEARCH=zzz` (our YIELDS has 4 rows) · YIELDS+BULLISH (t039–t041.5) `DW_TH_CATEGORY=6 DW_TH_TREND=1`.
Idle frames between transitions only differ by the title dot pulse / block-cursor blink (diff 0.1–0.5).

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | all | KPI values | regular-medium weight, soft glow, digits y78–103 | bold-looking, strong glow, 4 px low | weight 500, glow alpha 22→13, baseline −3 | fixed |
| 2 | all | KPI labels | letter-spacing ≈2.6 | 3.0 (wider) | 2.6 | fixed |
| 3 | all | chips | label spacing ≈1.9 px | 2.5 px (COMMODITIES 13 px wider) | 1.9 | fixed |
| 4 | t012.5–t038.5 | chip hover | gold text **and** gold count, bolder | gold text, count dimmed | bold gold label + gold count | fixed |
| 5 | all | active chip | gold fill r≈2 with soft gold glow | flat fill r=1 | glow rings + r=2 | fixed |
| 6 | t017.5 | click | gold ring around the cursor ≈0.3 s | none | — | recorder artefact, not reproduced (same ring in other videos) |
| 7 | t017.5 | rows fade | row 1 full, row 12 ≈40 % while radar still tiny | 30 ms stagger / 300 ms (too slow) | 12 ms stagger / 110 ms | fixed |
| 8 | t017.5 | radar grow | tiny polygon when rows are in | ease-out 500 ms (grows too fast) | ease-in-out 600 ms | fixed |
| 9 | all | header cells | dark boxes visible (#0c0a07) | nearly invisible when rows present | always #0c0a07 | fixed |
| 10 | all | sort header | `TREND ▾` gold #f3dc7d | cream #f6eccb | gold | fixed |
| 11 | all | cells | inset glow: darker centre, brighter edges | weak glow | 7 inset strokes, edge +16 % | fixed |
| 12 | all | selected row | instrument tile dim-gold border, name gold #f7dc72 | bright gold border, pale name | dim border, gold name | fixed |
| 13 | t012 | column widths | instrument column ≈275 px under ALL | ≈225 px | instrument weight 1.7→2.0 | fixed |
| 14 | all | radar labels | Volatility/Structure at y≈343 | 10 px higher | +10 px | fixed |
| 15 | all | signal name | ≈32 px bold, wide | 30 px | 32 px, spacing 1 | fixed |
| 16 | all | search | placeholder spacing ≈1.5, not focused | 2.0, caret at startup at 1280 | 1.5, ClickFocus | fixed |
| 17 | all | counts | 108 instruments, no STOCKS chip, YIELDS 0 | 127 catalogue (126 with data), STOCKS 28, YIELDS 4 | — | deliberate (catalogue) |
| 18 | t030.5 | empty state | YIELDS 0 → message + legend | YIELDS has 4 rows; empty state shown by filters/search | — | deliberate (data) |
| 19 | all | footnote | 3 lines | reference sentence + full rules (short form + tooltip when it does not fit) | — | deliberate (rules disclosed) |
| 20 | all | title cursor | blink aliased every ≈4 s in 0.5 s frames | 1 s steps | — | kept (frames cannot resolve the period) |

Values (counts, %, prices) differ: live data, not a discrepancy.
