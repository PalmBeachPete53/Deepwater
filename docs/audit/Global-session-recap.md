# Audit — Global › Session Recap (44 frames, t000.0–t021.5)

States reproduced: top (t000/t020.5), centre scroll steps (t001–t003, `DW_RECAP_SCROLL=150…746`), scrolled idle (t003–t011),
chart hover on the traced series (t011, `DW_RECAP_SCROLL=746 DW_RECAP_HOVER=546`), another market traced from its chip/card (t011.5–t014.5,
`DW_RECAP_TRACE=1`), hover on it (t014.5), scroll back (t019–t020). Blink frames (t004.5, t008.5…) are the cursor/dot blink (530 ms).

| Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|
| all | page strip | gradient `#100a03`→`#161005` | brighter `#120d04`→`#1a1305` | colours | fixed |
| all | registry | band `#221b04`, tab area `#151202`, date rows `#1a1506` | `#241c05`, page, `#0d0b08` | colours | fixed |
| all | session rows | flag centred in the 51 px row, name 14 px bold, values `#8ec4b4` / `#cc9a99` | flag on line 1, 15 px, dimmer | flag y, size, colours | fixed |
| all | centre bg | `#060503` | `#050402` | colour | fixed |
| t000 | read card | warm olive top fading to near-black (`#181808`→`#0b0b0b`) | flat `#0a0a0a` | vertical gradient | fixed |
| t000 | read label | very dim `#4c493c` | `#5c5848` | colour | fixed |
| t000 | section labels | yellow `#d8b445` | `#c99f2c` | colour | fixed |
| t000 | section text | ≈ 15 px (6.2 px/char) | 16 px | 15 px | fixed |
| t000 | lead | — | "2 releases fell: 0 above, 0 below, 0 in line" when no actuals | sentence only counts scored releases | fixed |
| t000 | right column | header bands `#140f04`; release rows on `#0d0906`; medium marker barely visible | `#0d0b08`, black, grey | colours | fixed |
| t003 | timeline frame | gold-tinted border all round | dim `#2a2412` | `#3a3522` | fixed |
| t003 | label spacing | `WINDOW TIMELINE` / `MOVES OF THE WINDOW` letter spacing ≈ 2.2 | 3.6 / 3.4 | 2.2 / 2.3; chips start after the label | fixed |
| t003 | plot | grid/fill 69–319 (panel y); values mapped 85–298 (max/min labels there) | values to the plot edges, fill to 298 | separate fill rect and value rect | fixed |
| t003 | lanes | horizontal rules under the plot, between lanes, under the wire lane; gridlines only in the plot | gridlines through lanes, no rules | rules at 325/353/383, gridlines plot-only | fixed |
| t003 | wire ticks | rounded pills, count only when ≥ 2, number above-right of the tick | square, every count centred | pills, labels ≥ 2, placement | fixed |
| t003 | moves title | baseline 18 px below panel + 9 | 4 px higher | +4 px | fixed |
| t003 | card change % | `#e39f9b` / `#93d6dc` (brighter) | `#d69997` / `#89cbcf` | colours | fixed |
| t011/t014.5 | crosshair | vertical line at the mouse x, dot snapped to the bar, tooltip right of the line at plot top + 5, bg `#0c0c0c` | line on the snapped point, box above the plot, `#151515` | line follows the mouse, box position, colour | fixed |
| t001–t003 | wheel step | ≈ 150 px per step, animated | 100 px | 150 px per notch | fixed |
| all | "Alfred" | ALFRED'S READ / POWERED BY ALFRED | AUTOMATED READ (rule-based) | — | deviation (documented) |
| all | values, releases, headlines | Oct 23 session | live sessions; wire archive only since the app ran; releases from the News calendar | — | data values |

Findings: 19 · fixed: 19 · deviations kept: 1 (Alfred → automated read).

## Full pass (all 44 frames opened, t000.0–t021.5, as 2x2 montages in order)
Checked every frame: top state, 5 scroll steps with motion blur (t001–t003), scrolled idle with blinks (t004.5, t008.5, t012.5, t017),
WTI hover tooltip (t011), Gold chip click → red series, gold card border, active chip (t011.5; the ring around the cursor is the recorder's click
marker, not app UI), Gold hover with the crosshair entered from the wire lane (t014.5), scroll back (t019–t020). No new discrepancy beyond the
table above.
