# Audit — News › Squawk (83 frames, t000.0–t041.0)

States reproduced with hooks: idle (t000/t031), list scrolled 457 px + row hover (t005.5, `DW_SQUAWK_SCROLL=457 DW_SQUAWK_HOVER=500,280`),
2-line row hover (t008, `DW_SQUAWK_HOVER=500,190`), tab hover (t014.5/t015, `DW_SQUAWK_HOVER=300,100`), minute rollover of ages (t019,
timer aligned on the minute), calendar scrolled + row hover with pinned day (t034.5, `DW_SQUAWK_SCROLL=0,-110 DW_SQUAWK_HOVER=1200,700`).
Frames between these are idle/hover repeats (diff < 1.5) and were checked against the same screenshots.

| Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|
| all | header band | 0–43 `#0f0a04`, 1 px rule `#2c2517` at y 44 | 40 px `#0c0904`, no rule | height 45 + rule, colour | fixed |
| all | ticker | strip 45–77 `#080603`, rule `#29251b` at 78, text centre y 61 | 40 px `#0a0604`, rule 78–79 | height 34, colours, baseline | fixed |
| all | ticker items | time → dot 8 px → name 16 px, items 34 px apart | " • " text, 40 px gap | painted 2 px dot, 34 px gap | fixed |
| all | filter tabs | band 79–119 `#080603`, pills 88–110, rule `#2d2a1e` at 120 | 40 px header colour | height 42, colour, pill y | fixed |
| t014.5/t015 | tab hover | label brightens + dark pill `#17110a` | label only | hover pill | fixed |
| all | subtitle | ≈ 12 px, light weight, dim | 13 px medium | 12 px regular | fixed |
| all | `文A EN` | icon and EN separated | glued | +2 px | fixed |
| all | TV title | `TERMINAL TV` heavy, `#d5d3cd` | bold | extra-bold | fixed |
| all | TV caption band | y 352–385 `#0c0805`, rule `#241d0d` at 386 | page black | band colour + rule | fixed |
| all | calendar filters | whole filter block `#0f0a04`; rules at 430/462/496/530, strong `#3b351a` at 562 | page black, rules 429/462/495/528 | block colour, rule positions | fixed |
| all | calendar search | 393–422, fill `#120d06`, border `#2c2218`, 13 px dim placeholder | 28 px, `#140b03`, 14 px | 30 px, colours, 13 px | fixed |
| all | day header | band 564–591 `#120d06`, rule `#332a1d` at 592, 7 px black gap, row rule at 600 | 34 px `#080502` | 37 px block as measured | fixed |
| all | event rows | 59 px pitch, rules `#212208`, rows on `#000000` | `#16120a`, page black | colours | fixed |
| t034.5 | calendar scroll | day header stays pinned at the top while rows scroll under it | scrolled away | sticky header (pushed by the next day) | fixed |
| t031 | 12:00 countdown | next event amber, `in` / `HH:MM:SS` red | same | — | ok |
| all | TV video | channel live stream | WKWebView (Qt WebEngine lacks H.264/AAC); app screenshots can't show native views, checked with window captures | — | deviation (documented) |
| all | counts / items | 465 items, AUD CPI week | live wire and this week's calendar | — | data values |
| all | LIVE pills, footers, row geometry, age column, coin, hover colour | match | match | — | ok |

Findings: 16 · fixed: 15 · deviation kept: 1 (TV engine; the stream itself plays in the app).

## Full pass (all 83 frames opened, t000.0–t041.0, as 2x2 montages in order)
Checked every frame: idle ticker drift, hovered rows (t003–t013, t019–t041), the tab hovers on ALL / DATA / MOVERS / RELEASES (t014–t018, t024–t027),
list scrolls (t005.5, t008, t020, t029.5), the minute rollover (t019), the calendar hover on 5-Year Note Auction (t033.5) and the AUD CPI cluster
with the pinned day header (t034.5–t037.5), countdowns ticking 1/s. No new discrepancy beyond the table above (TV picture and values are live data).
