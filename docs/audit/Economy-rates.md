# Audit — Economy › Rates (45 frames, 0.0–22.0 s)

App states: `DW_RATES_HOVER`, `DW_RATES_SCROLL`, `DW_RATES_BANK`, `DW_RATES_HOOK_DELAY` (see RatesPage.h).
Screens: `SCRATCH/shots/audit-eco/r*_p.png`.

| Frame(s) | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|
| 0.0 | Stat cards | `PRICED TO HIKE`, `/ 8` | label + `· 6 OF 8 BANKS PRICED`, `/ 6` | suffix moved to a tooltip; denominator stays the number of priced banks (honest) | fixed / deviation |
| 0.0 | Footnote | `$5.4M in real Polymarket liquidity…` | instrument list per bank | Polymarket unreachable (TLS); odds computed from futures/OIS | deviation |
| 0.0–5.5 | Table | 8 banks, all priced | 6 priced; RBA, RBNZ `NO FEED` (+ reason tooltip) | no legal feed until a FinancialJuice picture validates (ASX/RBA/RBNZ forbid programs) | deviation |
| 0.0–5.5 | Row hover / selection | warm hover, amber bar + gradient | same | — | ok |
| 6.0 | Implied curve / points | layout, labels, dashed baseline, `Current` | same | — | ok |
| 8.0 | Next-meeting bars, odds evolution chips, legend | same layout | same; extra source line under the bars | source line kept (labels the instrument) | deviation |
| 8.0 | Odds x axis | day ticks with month name | month label was dropped near month end | ticks skip the last 2 days of a month | fixed (phase 2) |
| 11.0 | Polymarket pair | live Polymarket charts | panels say why Polymarket is unreachable | not bypassed | deviation |
| 11.0 | Committees | Fed rows decision `–` | decision text read from the statement | more information, same layout | ok |
| 12.5–16.0 | Category crosshair | dashed line slides between categories, tooltip at cursor + (25, 20) | same (on the TERMINAL chart; Polymarket has no data) | — | ok |
| 17.0–21.0 | Odds crosshair | dashed cross, y badge left, date badge bottom, markers, tooltip flips near right edge | same | — | ok |
| all | Title bar | handle, title, ⚙ 📌 🔒 ⌄ ⤢ × | same (collapse works; others tooltip) | — | ok |
