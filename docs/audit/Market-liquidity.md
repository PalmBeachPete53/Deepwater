# Audit — Market › Liquidity (85 frames, t000.0–t042.0)

States reproduced: crosshair t000–t006.5 `DW_LIQ_HOVER=810,295 / 1149,399 / 1333,511` · button t007.5 `DW_LIQ_HOVER=450,22` ·
dropdown t008–t009.5 `DW_LIQ_DROPDOWN=1` · scrolled t010–t013.5 `+DW_LIQ_SCROLL=214`, crosshair t011 `DW_LIQ_HOVER=323,180` ·
tab hover t014–t015.5 `DW_LIQ_HOVER=60,490 / 160,490` · sweeps t016.5–t020.5 `DW_LIQ_TAB=1` · sessions t021–t035
`DW_LIQ_TAB=2` · zoom t035.5–t042 `DW_LIQ_SCROLL=231 DW_LIQ_ZOOM=n`.

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | all | title glyph | three small waves (≋) | three straight lines | waves path | fixed |
| 2 | all | KPI labels | spacing ≈2.5 | 3.5 (≈15 px wider) | 2.5 | fixed |
| 3 | all | KPI `High` | pale salmon core #f0a6a5 | saturated #f87171 | #f29896 (medium gold #f5d565) | fixed |
| 4 | all | instrument button / list | proportional sans 11 px | mono 11 px | Inter 11 (medium on the button) | fixed |
| 5 | t008–t042 | dropdown search | dim placeholder #4d4741 | bright (QSS color tinted it) | `placeholder-text-color` | fixed |
| 6 | all | tabs | mono ≈9 px, gap ≈22 px (x 27/131/227) | 10 px, wider | 9 px, spacing 0.2, gap 22 | fixed |
| 7 | t014–t015.5 | tab focus box | appears when the active tab is hovered and stays | only after a click | hover on active tab starts the fade-in | fixed |
| 8 | all | pool rows | dots ≈4.5 px, pitch ≈8; `BSL` spaced; no ×N column | 4 px dots pitch 7, extra ×N text | dots 2.2 r / pitch 8, spacing 1, ×N removed (SWEPT/BROKEN marker kept) | fixed |
| 9 | t017 | sweep cards | sentence ≈12 px, dot ≈5 px | 13 px, 4.6 px dot | 12 px, 5.6 px dot | fixed |
| 10 | all | bottom panel bg | faint hexagon grid | plain | cached hex tile, 1 px very dim | fixed |
| 11 | t000–t011 | chart levels | every drawn line has its tag; lines inside price action | many untagged dim lines | 4 nearest untested each side + 3 latest sweeps on screen, all tagged | fixed |
| 12 | t000 | candles | bodies ≈3 px at the default zoom | 2 px | ceil(0.8 × spacing) | fixed |
| 13 | t011 | crosshair hook | crosshair in the scrolled state | lost (Leave after popup) | hover hook pinned, dropdown opened first | fixed (hook) |
| 14 | t000 | price/time format | French `09 oct. '26`, `oct.` | English `09 Oct '26`, `Oct` | — | deliberate (English UI) |
| 15 | all | level tag values | 2 dp | instrument precision (5 dp) | — | deliberate (2 dp hides the level) |
| 16 | all | pool counts | 62 pools / 13 sweeps | 357 / 90 on 2 years of real hourly bars; list = untested + taken in 30 d | — | deliberate (documented rules) |
| 17 | t021 | scroll clamp | content shorter → chart moves down ≈59 px | same mechanism; our footnote is 2 lines so the clamp differs by a few px | — | ok (content length) |
| 18 | t035.5–t042 | zoom | around the cursor x≈810 | around the cursor (hook zooms at the chart centre) | — | ok |
| 19 | all | chart engine | TradingView Lightweight Charts | own QPainter chart | — | deliberate |
| 20 | all | footer time | 16:29:27 static | time of the last data arrival | — | ok |
