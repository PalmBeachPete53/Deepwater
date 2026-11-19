# Audit — News › Bank Research (47 frames, t000.0–t023.0)

States: globe idle (t000.0–t001.0), row hover (t001.5), PDF opened + row selected (t002.0–t010.0), PDF wheel
scroll down (t010.5–t017.0), idle (t017.5–t019.0), scroll back up (t019.5–t023.0). Hooks: `DW_GLOBE_LON`,
`DW_BANKRES_HOVER`, `DW_BANKRES_OPEN`, `DW_BANKRES_SCROLL`, `DW_BANKRES_TAB`. Screens: `scratchpad/shots/banks/audit/br-*`.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000.0 | globe projection | perspective view, NA large left, SF beacon at 0.93 R from centre | orthographic, 22° tilt, NA small | perspective camera 3 R, tilt 36°, lon −48 fitted on 6 beacon positions (≈10 px RMS) | fixed |
| 2 | t000.0 | globe lit land | gold = North America only; Greenland, S. America, Europe grey | gold on the upper-left limb incl. Arctic, S. America | light vector (−0.50, 0.30, 0.81), narrower ramp | fixed |
| 3 | t000.0 | dot pitch | ≈7 px at the centre | ≈10 px with perspective | grid step 1.5° → 1.1° | fixed |
| 4 | t000.0–1.5 | radar sweep | ≈60° wedge behind the globe, sharp bright edge #2c2605 on the trailing side, fades towards the leading side, ≈23°/s clockwise, edge at 221° at t0 | 75°, bright edge leading, 30°/s | 16 flat slices kept (lead's perf fix), width 60°, alpha 70→0, direction and speed measured | fixed |
| 5 | t000.0 | beacon labels | TD (not RY) in Toronto, JPM/GS in New York | RY shown | label priority list (JPM, TD, GS, BAC, WFC, NWG, ING, LYG, UBS, SAN) | fixed |
| 6 | t000.0 | arcs | few, low arcs | loops high above the pole | 5 arcs, height 0.03 + 0.035·angle | fixed |
| 7 | t000.0 | left tabs | tabs fill y 36–66, labels at x 17/125/215/292, inactive medium weight | y 41–65, x 17/118/206/285, all bold | tab rects + weights | fixed |
| 8 | t000.0 | dropdown border | almost invisible | visible #18170d | #131209 | fixed |
| 9 | t000.0 | note rows | logo +7, name baseline +21, chip y+7 h18 with black fill, title baseline +43 | +10 / +23 / +10 h17 / +45 | offsets | fixed |
| 10 | t000.0 | bank names (list, registry, PDF header) | squared bold display face (JPMORGAN 67 px wide) | JetBrains Mono bold (58 px) | theme::display Bold 12–14 | fixed |
| 11 | t000.0 | category chip | gold text #e6cf72, letter spacing ≈0, padding 6 | pale #d1c497, spacing 0.7 | colour/spacing/padding | fixed |
| 12 | t000.0 | registry header | grey #6e6958 text | gold #8a7e4a | colour | fixed |
| 13 | t001.5 | row hover | gold wash #282205 fading to the page colour at ≈70 % width, content −3 px | flat #1f1903 | horizontal gradient | fixed |
| 14 | t002.0 | selected row | brighter wash #3e3205 → #231b04 → #120802 across the row, no separate bar, chip box hidden | flat #2a2104 + 3 px bar | gradient, no bar, no chip box | fixed |
| 15 | t002.0 | PDF toolbar | bg #4d4d4d; ≡ · title · centred [page n / N · − 95 % + · fit · rotate · draw · undo/redo] · [add · download · print · ⋮] | bg #3c3c3c, no ≡, no print, no ⋮, unicode glyphs | toolbar #4a4a49, centred middle cluster, painted icons, ≡ opens a page-thumbnail sidebar, print (PDFKit print panel), ⋮ menu (open in viewer, copy link, source, document info) | fixed |
| 16 | t002.0 | PDF page | page fills the toolbar width, no scrollbar | 4 px side margins, visible scrollbar | margins 0, vertical bar hidden (wheel/trackpad scroll, smooth) | fixed |
| 17 | t002.0 | PDF header bank name | display bold | mono | display 14 | fixed |
| 18 | t002.0 | toolbar draw / undo / redo / add-to-Drive | Chromium annotation and Drive buttons | — | not built: browser features with no meaning in the app (a dead button would be fake) | deviation |
| 19 | t002.0 | zoom % | 95 % (Chromium's own scale) | ours = scale vs 96 dpi (e.g. 106 %) | honest value of our renderer | deviation |
| 20 | all | notes / PDF content | Danske, MUFG notes | ING notes only | terms: only ING allows reuse; others listed in DATA with reasons | deviation (data) |
| 21 | all | logos | real bank logos | monogram tiles | no trademark artwork bundled | deviation |
| 22 | t000.0 | registry values | $5.02T, UBS "—" | latest EDGAR values, UBS $1.62T | live data | data |
| 23 | t010.5–t023 | scroll | smooth wheel scroll ≈20 px / 0.5 s | animated wheel scroll (160 ms) | — | ok |

23 findings: 17 fixed, 1 already ok, 5 documented deviations or data differences.
