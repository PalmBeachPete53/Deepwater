# Audit — Global › Overview (129 frames, t000.0–t064.0) and Global › Market Hours (14 frames)

Method: every reference frame viewed (2x2 contact sheets of consecutive frames + crops of each changing region);
app states reproduced with hooks and screenshotted at 1600x1042, compared side by side.
Hooks: `DW_OVERVIEW_CLICK=x,y[;x,y]` (one click 2.5 s after show), `DW_OVERVIEW_HOVER=x,y` (mouse kept there),
`DW_STRENGTH_HOVER=x,y`, `DW_STRENGTH_ZOOM=±n` (panel coords), Market Hours `DW_TZ`, `DW_NOW`;
Squawk FR via the profile key `global.overview.squawkFr=true`.
Live data (values, headlines, dates) differ by nature and are not findings.

## Overview

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | all | Globe | Blue Marble without clouds, blue bathymetry oceans, darker night side | cloudy texture, washed-out | texture → NASA BlueMarbleNG topo+bathymetry (PD); shading/limb retuned | fixed |
| 2 | all | Globe | soft wide amber haze, no bright rim | bright narrow ring | atmosphere gradient widened (30 px) and dimmed | fixed |
| 3 | t000–t016 | Squawk FR header | description column 6 lines (`DATA / headlines + / … / releases`) | 6 lines but wrapping differently, 13 px | column width = "price moves +", 12 px condensed | fixed |
| 4 | all | Squawk LIVE tag | flat tag, no border | yellow border (pen leak) | pen reset | fixed |
| 5 | all | Squawk footer | `Terminal Data · <newest headline time>` (16:37:43 while 17:34 movers scroll) | newest item of any kind | newest headline | fixed |
| 6 | t000, t049 | Risk gauge | fine graduations every 2 pts, small 0–100 labels, thin bars, curved RISK-OFF/NEUTRAL/RISK-ON words, 40/60 marks, soft halo behind value | no minor ticks, thicker bars, only NEUTRAL, outlined value | all added / retuned | fixed |
| 7 | t001.0, t004.0 | Currency crosshair | crosshair stays with the cursor over the subtitle row; y pill reads above the plot (`1.28%`) | crosshair hidden above the plot | hover zone extended to the subtitle row, y pill unclamped | fixed |
| 8 | t005.0 | Timeframe buttons | hovered `4H` / `D` white bold | no hover state | hover state added | fixed |
| 9 | t000–t064 | Currency lines | ≈1.8 px lines | 1.5 px | 1.8 px | fixed |
| 10 | t000, t028 | Currency x axis | day numbers at default zoom; `HH:MM` only when zoomed in (t064) | hour labels mixed in at default zoom | hours only above 200 px/day | fixed |
| 11 | t024.5, t025.0 | Registry tab hover | hovered tab yellow **bold** | yellow regular | bold | fixed |
| 12 | t026, t049 | Registry rows | highlighted rows olive gradient (#201901→#2f2604); WB rows dark band; badges with 1 px border (#8a7a2a / #2a2614), 18 px tall | flat fills, borderless 15 px badges | gradient fills + bordered badges | fixed |
| 13 | t049 | Central Banks | 1 px divider under the title, faint separators between rows, names 12 px | none, 11 px | added | fixed |
| 14 | t000–t016 | Squawk FR texts | French headlines | French via news::Translate (on-device) | — | matches (first pass translates a few/s, then cached) |
| 15 | t026 | Registry OCEANIA | 7 rows, 2 highlighted, others WB | same (macro::registry) | — | matches |
| 16 | t028–t064 | Zoom out/in | anchored on the cursor bar, re-based, future area, rings at line ends | same | — | matches |
| 17 | all | Risk history | legend overlaps `HISTORY · 1 YEAR` in the reference | kept apart (spec asks to avoid the overlap) | — | deliberate |
| 18 | all | Central Banks odds | 8 banks priced | 5 of 8 priced → "—" + pricingNote tooltip (no free source for RBA, RBNZ, …) | — | deliberate (no source) |
| 19 | all | Squawk RELEASES count | 39 | 0 until a calendar registers `wire::setReleaseSource` | — | other package |
| 20 | t000 | Squawk item count | 581 items | wire keeps ≈150 live items (24 h) | — | live data |

## Market Hours

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | all | Disclaimer | ends at x≈790 | ends at x≈826 (spacing 1.5) | spacing 1.2 | fixed |
| 2 | all | Timeline | no OVERLAPS row | OVERLAPS row (sessions trading together) | — | deliberate addition (spec open question "overlaps") |
| 3 | t000–t006.5 | Clock, cards, timeline, badge, cursors | — | matches (layout ±2 px, colours, 6x→real speeds; sweep 20 fps per lead) | — | matches |
