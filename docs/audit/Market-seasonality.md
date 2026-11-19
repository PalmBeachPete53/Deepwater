# Audit — Market › Seasonality (97 frames, t000.0–t048.0)

States reproduced with `DW_SEASON_*` hooks (see `src/pages/market/seasonality/SeasonalityPage.h`); data values are
live EUR/USD (differ from the video). Screenshots: `SCRATCH/sea/audit/final/seasonality-*.png`.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000–t004.5, t040 | trend tooltip | box centred on the crosshair, just above the curve point; flipped left near the right edge | box right of the line at cursor height | placement rule rewritten | fixed |
| 2 | t040 | tooltip value | value cyan even when negative (`-0.33%`) | red when negative | always cyan | fixed |
| 3 | t000 | plot | 1 px hairline `#17150b` at every month start | none | drawn | fixed |
| 4 | t000 | TODAY marker | dashed teal `#4f706e` | grey `#57605d` | colour | fixed |
| 5 | t000 | toolbar | segment group 376–545, Compare at 552 | group 377–562, Compare 573 | segment width text+7, gap 3 | fixed |
| 6 | t000 | win-rate pie | `55.0%` in the mono face (slashed zero), word and years tight under it | condensed 34 px, loose | mono 28 ExtraBold, spacing | fixed |
| 7 | t000 | pie colours | red `#fc6d6d→#e85a5a`, cyan `#1ddff3→#15b4c8` | pale pink (QColor::lighter desaturates) | explicit gradients, darker red wall | fixed |
| 8 | t000 | pie position | top face from y 213 | 4 px lower | centre moved up | fixed |
| 9 | t000 | win caption | 12 px | 13 px | font | fixed |
| 10 | t008.5 | asset menu | fixed 258 px panel, 9th row cut by the edge, 23 px search, 12 px | panel grew to 9 full rows, 26 px search | fixed height + clip, search size | fixed |
| 11 | t012.5 | years button | border stays dim while open | gold border | removed | fixed |
| 12 | t012.5 | years menu | selected row `#352b05` | `#2a2010` | strong selection colour | fixed |
| 13 | t012.5 | years menu | 10 / 15 / 20 years | 5 / 10 / 15 / 20 | — | deviation (5 Y asked by the lead) |
| 14 | t017–t040 | filter menu | `Exclude outlier years (>1.5σ)` fits | text clipped | row font 12 px | fixed |
| 15 | t025.5 | max label | (peak elsewhere) | `+0.32%` overlapped the period badge | label moves under the peak when it would hit the badge | fixed |
| 16 | t034 | win-rate title pill | small condensed `Jan-Mar` | mono spaced pill | condensed pill | fixed |
| 17 | t034 | volatility state text | `#8e8b83`-ish, French | dimmer, English | brighter; English kept | fixed / deviation (English UI) |
| 18 | t044.5 | bar charts | solid faint lines `#222111` at ± | dashed | solid | fixed |
| 19 | t044.5 | risk tiles | label ≈ 8–9 px caps | 10 px | 9 px | fixed |
| 20 | t025.5–t040 | period band / badge | `Jan 1 ⟶ Jul 2`, `Mar 31 ⟶ Jul 2` (one day past) | exact `Jan 1 ⟶ Jun 30` | — | deviation (exact period ends, documented in the engine) |
| 21 | t018 | Filter button | gold border while hovered right after the click (t012.5 shows none for Years) | none | — | open: reference inconsistent |
| 22 | t038 | toolbar | ring circle around the cursor | — | — | not app UI (screen-recorder click effect) |
| 23 | all | numbers | video values | live values | — | data, not a discrepancy |

Verified again at 1600x1042 (all states above) and 1280x800. CPU idle 20 s: 0.7 %.

**Lead note (2026-11-14):** the "5 years" entry (added from the lead's first brief) is removed: the menu now offers 10, 15 and 20 years, as in the reference (t012.5).
