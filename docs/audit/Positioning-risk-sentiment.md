# Audit — Positioning › Risk Sentiment (29 frames)

App states: scroll 0 / 64 / 212 (`DW_RISK_SCROLL`), row hover and history crosshair (`DW_RISK_HOVER`). Live Yahoo
values differ (composite 33 vs 32), not counted. Frames: t000.0–t003.0 idle (pulse/blink), t003.5–t006.0 row
hovers, t006.5–t010.5 scroll down with tooltip, t011.0–t014.0 scroll up.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000.0 | title bar | ends x≈405, spacing ≈1.8 | spacing 4 | shared `pk::TitleBar` fix | fixed |
| 2 | t000.0 | gauge ring | bars r≈199–240, ≈10 px wide, soft glow | r 188–217, 7 px | geometry + glow | fixed |
| 3 | t000.0 | zone labels | 10–11 px on the ring's inner edge (r≈186) | 9 px at r 172 | size/position | fixed |
| 4 | t000.0 | 1W / 1M markers | r 162–180, label 10 px | r 150–166, 9 px | moved | fixed |
| 5 | t000.0 | needle | to r≈192 | r 170 | longer | fixed |
| 6 | t000.0 | value | 64–66 px extra-bold | 64 bold, tracking −2 | 66 ExtraBold | fixed |
| 7 | t000.0 | change pill | two pills `1W …` and `1M …` | one pill | two pills | fixed |
| 8 | t000.0 | history bands | #371b15 #1f1108 #1b1503 #0b1915 #0f2d2c | theme bands (brighter red/olive) | measured colours | fixed |
| 9 | t000.0 | descriptions, VIX labels | condensed | Inter | condensed 12/13 | fixed |
| 10 | t003.5 | row hover gradient | warm, left → 60 % | same | — | ok |
| 11 | t007.5 | history tooltip | box left of and below the cursor, keeps its place while scrolling | below the dot | placed from the cursor | fixed |
| 12 | t010.5 | max scroll | 212 px | 204 | content height 1142 | fixed |
| 13 | t010.5 | VIX axis | step 3 (`33 30 … 12`) | step 5 | 1/2/3/5 nice steps | fixed |
| 14 | t010.5 | footnote | one line ending `UPDATED 16:06` | elided before the time | shorter text, wraps instead of cutting | fixed |
| 15 | t000.5–t002.5 | dots pulse / cursors blink | yes | yes | — | ok |

15 findings: 13 fixed, 2 ok-checked states.
