# Audit — Account › Track Record (Reference/Account/track-record.mp4, 66 frames)

App states reproduced with `DW_TR_VIEW`, `DW_TR_FILTERS`, `DW_TR_SCROLL`, `DW_HOVER`, `DW_CLICK`, `DW_ACCOUNT_DEMO`
and `TZ=America/New_York` (the video's clock). Script: `scratchpad/acct/trstates.sh`; captures `scratchpad/shots/audit/tr-*.png`.

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t010–t020.5 | view tabs | 1 px yellow focus ring appears 1.0–1.5 s after the click | ring drawn at once | ring shown 1.3 s after the click | fixed |
| 2 | t021–t023 | FILTERS | rounded outline = focus ring, appears ≈1.5 s after the click, tab ring disappears | outline tied to "filters on", shown at once | outline is the focus ring (same delay), moves to the clicked tab | fixed |
| 3 | t022.5–t032.5 | FILTERS ring | `#cdb558`-ish, radius 5 | `#d9b400`, radius 4 | colour/radius | fixed |
| 4 | all | focus ring | kept while the window is inactive | lost on window deactivation | ignore ActiveWindow/Popup focus-out | fixed |
| 5 | t026.0 | Top 5 tooltip | flips left of the cursor before reaching the panel edge (≈10 px) | stayed on the right | flip margin 10 px, offset 26 px | fixed |
| 6 | t024–t032.5 | tooltips | mono 12 with ≈7.5 px pitch (box 218 px for EUR/USD) | 7.2 px pitch (205 px) | letter spacing 0.3 | fixed |
| 7 | t013.5 | trade log, last row | 19 Aug EUR/USD is row 20 | a generated USD/JPY −68 trade drifted to 20 Aug | demo generator: dates capped before 19 Aug | fixed |
| 8 | t010.0 | Month P&L | Apr ≈+300, May ≈−60 | Apr +140, May +98 (generated dates) | demo: 30 Apr trade pinned | fixed |
| 9 | t000–t007 | cumulative chart | crosshair, ring marker, tooltip Day/Cumulative, flip near bottom | same | — | ok |
| 10 | t017.0 | Advanced bars | bars grow from the axis | same (0.5 s ease-out) | — | ok |
| 11 | t024–t032.5 | scroll | smooth wheel, toolbar + filter bar fixed, hover follows content | same | — | ok |
| 12 | t010.0 | score | 59.2 | 60.6 | own formula (documented in tooltip) | deviation |
| 13 | t010.0 | cumulative x labels | 08/20/26 (UTC date) while Last trades shows 08/19 (local) | local date everywhere (08/19) | consistent local time on purpose | deviation |
| 14 | t013.5 | HELD | 36m / 2.6h (seconds in source) | 35m / 2.5h (sample times in minutes) | data | data |
| 15 | t017.5+ | weekday / symbol bars | Fri ≈−$210, GBP/USD ≈$515 | −$125, $492 (40 of 60 sample trades generated) | data | data |
| 16 | — | Alfred | not visible on this page (cut panel at y 957) | AUTOMATED READ panel (rule-based) | — | deviation |
| 17 | t024.0–t031.5 | bar tooltips | near the plot bottom the tooltip goes above the pointer (GBP/USD at t028.5–t030.0) | stayed below and covered the axis | flip bounds = plot area | fixed |

All 66 frames opened (contact sheets of 16 + key-state side-by-sides).
Totals: 17 findings, 9 fixed, 3 ok, 3 documented deviations, 2 data differences.

**Lead note (2026-11-14):** the grey ring around the cursor when FILTERS is clicked (t021) is the screen recorder's click indicator (the same ring appears on clicks in other videos); it is not reproduced. The FILTERS focus ring is kept.
