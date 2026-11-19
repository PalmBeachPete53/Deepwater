# Audit — Economy › Macro (36 frames, 0.0–17.5 s)

App states: `DW_MACRO_HOVER`, `DW_MACRO_COUNTRY`, `DW_MACRO_DETAIL`, `DW_MACRO_SCROLL`, `DW_MACRO_TAB`, `DW_MACRO_GROUP`
(see MacroPage.h). Screens: `SCRATCH/shots/audit-eco/m_*_p.png` (page area, 1600x966).

| Frame(s) | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|
| 0.0–1.5 | Globe | no tint on countries, land olive-tan, white ice | cadence tint yellowed the land, land too dark/green | tint removed; land grade brighter olive-tan, ice kept white | fixed |
| 0.0–1.5 | Bottom controls | LIVE FEED filled, MONTHLY outlined, ANNUAL dim | ANNUAL off-style and toggles tinted the globe | toggles now filter the registry by data cadence (all on by default, ANNUAL dim style) | fixed |
| 0.0–1.5 | Registry header | blinking block caret after `ASSET REGISTRY` | no caret | caret added (shared blink clock) | fixed |
| 1.5 | Registry hover | row lighter + nudged right; globe not highlighted | also outlines + fills the country and turns the globe to it | fill removed; outline + fly-to kept (lead's motion decision) | deviation (documented) |
| 0.0–2.0 | Registry | Singapore badge `BIS` | `WB` | BIS publishes no policy rate for Singapore (MAS steers the exchange rate) | data, documented |
| 2.0–4.0 | Title | condensed bold (≈ 629 px wide), baseline ≈ 132 | Chakra Petch, wider, 3 px low | Rajdhani Bold 52 px, spacing 2.4, baseline −3 px | fixed |
| 2.5 | Dashboard rows | names in condensed medium | Inter | Rajdhani Medium 16 px | fixed |
| 2.5 | Advanced cards | values in condensed bold | Chakra Petch | Rajdhani Bold 27 px | fixed |
| 2.5 | Row 5 | `PMI Composite` | `Business Confidence (OECD)` | S&P Global PMIs have no free source; closest official series, labelled | deviation |
| 2.5 | Column | `RELEASED` (release date) | `PERIOD` | official sources give reference periods only (no free release dates) | deviation |
| 2.5 | 10Y card | `OECD via FRED` | `OECD` | no FRED key; same OECD series direct | deviation |
| 3.5 | Yield tooltip | box above-right of the cursor | below-right | offset (+55, −87) | fixed |
| 3.5 | Yield y axis | ≈ 0.3 steps (3.90 … 5.40, 5.60) | 0.5 steps | step picked from 0.05…2 for ≤ 5 intervals, one step margin | fixed |
| 4.0 | Row hover | lighter row | same | — | ok |
| 4.5–5.0 | Detail header | `Inflation Rate (Annual)` condensed bold; `Released 2026-10-14` | Inter; `Period … · Next release —` | condensed bold 17 px; period shown (no release dates) | fixed / deviation |
| 5.0–13.0 | Detail y axis | 1.5 % … 10 % (one step beyond the data both ways) | 2 % … 9.5 % | ylo = floor − step, yhi = ceil + step | fixed |
| 5.0–13.0 | Detail x axis | labels every 3 bars from the first bar (`Nov 21`, `Feb 22` …) | calendar-aligned (`Jan 22`, `Apr 22` …) | labels counted in bars from the first one | fixed |
| 5.5–13.0 | Bar tooltip | `13 Jul 2022` (release date) | `Jul 2022` (period) | no free release dates | deviation |
| 5.0 | Caption | condensed text; `released October 14, 2026` | Inter; `for September 2026` | condensed medium 14 px; period wording | fixed / deviation |
| 13.5–14.5 | Release calendar | `DATE · ACTUAL · FORECAST` | `PERIOD · ACTUAL · CHANGE` | no free consensus forecasts | deviation |
| 13.5–17.5 | Scroll | whole file scrolls under the fixed top bar, ~100 px per notch | same | — | ok |
| all | Carets / dots | blink ≈ 1 Hz | same | — | ok |
| all | Live data | 82 · 60 · 27 counters, values | 82 · 60 · 37 (computed from real feeds) | — | data |
