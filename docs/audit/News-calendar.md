# Audit — News › Calendar (41 frames, t000.0–t020.0; all 41 opened)

App states reproduced with hooks: DW_CAL_HOVER=x,y (row/link hover), DW_CAL_PANEL=<title> (panel open),
DW_CAL_PANEL_CLOSE=1 (closed, focus outline), DW_CAL_EXPAND + DW_CAL_SCROLL + DW_CAL_CHART=bars,20y,<f> (expansion,
scrolled, chart tooltip). Screenshots: scratchpad/shots/news/audit/c_t*.png.

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000–t020 | row backgrounds | day groups alternate: Sun dark `#040302`, Mon lighter `#110f0b`, Tue dark | only today's rows lighter | alternate by day group | fixed |
| 2 | t014.5–t019 | expanded row | expanded row keeps the hover gold `#251f0b` | lighter grey | hover colour for the expanded row | fixed |
| 3 | t015 | expansion bg | `#140d08` | `#120c07` | colour | fixed |
| 4 | t014.5 | expand click | row stays where it is, expansion opens below | table jumped to put the row at the top | scroll only when the row is off screen | fixed |
| 5 | t003 | panel date line | `Mon 19 Oct, 07:30` only | date + currency + title | title removed | fixed |
| 6 | t003 | panel legend | y≈331 (18 px under boxes) | 8 px lower | spacing | fixed |
| 7 | t019 | history x labels | label every ≈11 months at 20Y (22 labels) | every ≈12 months | step 64 px | fixed |
| 8 | t013 | focus outline after close | 1 px gold outline stays on the clicked MIN cell | — (no hook to show it) | DW_CAL_PANEL_CLOSE hook; behaviour existed | fixed (hook) |
| 9 | t000–t001, t013.5 | hover | hover band on pointed row | not reachable in screenshots | DW_CAL_HOVER hook (pinned hover) | fixed (hook) |
| 10 | t010.5 | source link hover | link turns gold | same (checked with DW_CAL_HOVER on the actual-source line) | — | ok |
| 11 | t003 | economists' range bar, MIN/MAX values, bank bar | Econoday range 3.0→3.3, Danske 3.2 | "—", empty track, "No bank forecast" | none: no free source (Econoday terms forbid reuse; Danske API needs their embedded client secret) | deviation, documented |
| 12 | t000 | ACTUAL column | actuals for past events | mostly "—" this week | Myfxbook RSS + wire headlines + official series (phase 2); only 3 of 63 matched this week | data, documented |
| 13 | t000 | week / counts | Oct 18–24, 168 events | current ForexFactory week (106) | live data | not a discrepancy |
| 14 | t015 | description text | ForexFactory-style wording "A higher than expected reading should be taken as positive/bullish…" | own wording "A reading above the forecast is usually taken as positive (bullish)…" | kept own text (not copied) | deviation |
| 16 | t000–t020 (all 41) | second pass, every frame opened | t000.5–t001.0 hover CPI rows; t001.5 slide-in; t002.5–t012.5 pointer over panel (bar, boxes, legend, link); t013.5–t014 hover with outline kept; t015.5–t016.5 idle; t017–t019 hover + wheel; t019.5–t020 idle | all states reproduced; no new difference | — | ok |
| 15 | t000 | LIVE badge / footer | `Terminal Data · HH:MM:SS`, `● LIVE` | same | — | ok |

Checked and matching (no finding): search box, week navigator, COUNTRIES chips, IMPACT segments, column header,
sticky day header and pinned expanded row while scrolling (t017.5–t019), impact dots, period tags, arrows, surprise
colours, dotted MIN/MAX underline, panel slide-in 280 ms (t001.5→t002), BARS/LINE, 5Y–MAX, tooltip box and hover band.
