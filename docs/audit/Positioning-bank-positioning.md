# Audit — Positioning › Bank Positioning (47 frames, t000.0–t023.0)

States: RATES hover (t000.0–1.0, t003.0–4.0), no hover (t001.5, t005.0–6.5), INDICES hover (t002.5), CRYPTO hover
(t004.5), ASIA & EM FX hover (t007.0), FX BANK FORECASTS button hover (t007.5), overlay opening (t008.0), overlay idle
(t008.5–t023.0, cursor over header/title/table: no hover reaction). Hooks: `DW_BANKPOS_HOVER=<card>`,
`DW_BANKPOS_BTNHOVER=1`, `DW_BANKPOS_OVERLAY=1`, `DW_BANKPOS_PROGRESS=<0..1>` (new), `DW_BANKPOS_VIEW=1`.
Screens: `scratchpad/shots/banks/audit/bp-*`.

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000.0 | card hover hook | RATES lifted 3 px, arrow pale yellow | hook state lost (leave event on show) | forced hover survives leave events | fixed |
| 2 | t000.0 | background | hexagon texture barely visible | hexagons clearly visible | line alpha 7 → 4 | fixed |
| 3 | t008.0 | opening animation | dashed lines half drawn, price tag mid-way | animation existed, no way to capture it | `DW_BANKPOS_PROGRESS` hook freezes it | fixed |
| 4 | t008.5 | quarter headers | bold mono, quarter colours | regular weight | mono 10 bold | fixed |
| 5 | t008.5 | month axis | `Oct Dec Feb …` starting at the first bar | first month skipped (`Jan Mar …`) | first bar's month at the left edge, then every second month | fixed |
| 6 | t008.5 | footnote | one line, spacing ≈0.8 | longer text, elided | reference wording (table instead of note); "computed by Deepwater" moved to the note under the table | fixed |
| 7 | t008.5 | equal quarter values | labels overlap (Q1 '27 hidden under Q4 '28) | — | one shared label "Q4 '26 / Q1 '27 1.1300" | deliberate improvement |
| 8 | t008.5 | pair pills | 11 pairs | 14 visible + "+N" menu (ING covers ~30 pairs) | — | data |
| 9 | t008.5 | chart header flag | US flag | EU flag (base currency) | — | deviation (ambiguous in the frame) |
| 10 | all | counts / banks | 11 banks, 4 banks per pair | 1 bank (ING) | only ING allows reuse; banks not read listed on the page | deviation (data) |
| 11 | t000.0–7.5 | cards, header, button, segmented control, badge | — | matched in the previous pass (positions, fonts, colours re-checked) | — | ok |

11 findings: 6 fixed, 1 ok, 4 deliberate/data differences.
