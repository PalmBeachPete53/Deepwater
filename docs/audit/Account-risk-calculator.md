# Audit — Account › Risk Calculator (Reference/Account/risk-calculator.mp4, 65 frames)

States reproduced with `DW_RC_LEVELS=1.12000,1.11500,1.13000`, `DW_RC_OPEN`, `DW_RC_UNIT=pct|cash`, `DW_HOVER`, `DW_CLICK`.
Script: `scratchpad/acct/rcstates.sh`; captures `scratchpad/shots/audit/rc-*.png`.

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000–t032 | instrument combo | condensed sans (Rajdhani-like) medium 14 | Inter 12 | condensed medium 14 | fixed |
| 2 | t008–t032 | list rows | condensed medium 13 | Inter 12 | condensed medium 13 | fixed |
| 3 | t008–t032 | search field | "Search…" condensed 13, dim `#4e4a40` | mono bold bright (global QLineEdit style won) | own style sheet font + placeholder colour | fixed |
| 4 | t008–t011 | current / hover row | rounded (r≈3) | square | rounded rects | fixed |
| 5 | t012–t032 | % / $ toggle | active button glows after it was clicked | no glow | glow while the toggle has focus | fixed |
| 6 | t015.5–t016 | toggle | hovering the inactive glyph turns it yellow | no hover | hover colour | fixed |
| 7 | t016.5 | search caret | gone once focus moves to the toggle | caret kept (test hook gave no focus) | DW_CLICK gives focus like a real click | fixed |
| 8 | t000 | R/R panel | label y 616, ratio baseline ≈690 | 5–10 px lower | centre without +3, label −3 | fixed |
| 9 | t000 | R/R panel bg | faint glow | visible brown ring | glow alpha 150→90 | fixed |
| 10 | t000–t007 | title dot / block cursor | pulse 1 s, blink ≈1.06 s | same (10 fps timer) | — | ok |
| 11 | t008.0 | list opening | inline, pushes form 234 px, chevron flips | same | — | ok |
| 12 | t016.5 | $ mode values | 0,00 lots, 200 units, 1,00, 0.01% of balance, 2,00 | 0.00 / 200 / 1.00 / 0.01% / 2.00 | English number format (decision) | deviation |
| 13 | t016.5 | lot footnote | `1 lot = 100,000 units` | + "under the usual 0.01 lot minimum" | honest warning, kept | deviation |
| 14 | t000 | levels | 1.12000 / 1.11500 / 1.13000 | live price (e.g. 1.10280 / −50 / +100 pips) | data | data |

All 65 frames opened (contact sheets of 16 + key-state side-by-sides); no new finding.
Totals: 14 findings, 9 fixed, 2 ok, 2 deviations, 1 data.
