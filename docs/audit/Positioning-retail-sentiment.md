# Audit — Positioning › Retail Sentiment (37 frames, list + EUR/USD modal)

App states: test profile + `DW_MYFXBOOK_FIXTURE` (recorded answer shaped like Myfxbook's, 150 hourly snapshots)
for the connected states; `DW_RETAIL_HOVER`, `DW_RETAIL_OPEN`, `DW_RETAIL_SCROLL`. Live values differ (fixture /
Yahoo), not counted. Frames t000.0–t001.0 = list; t001.5–t018.0 = modal (t002.5–t012.0 crosshair moves,
t012.5–t014.5 scroll down, t015.0–t016.0 bottom, t016.5–t018.0 scroll up).

| # | Frame | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000.0 | title bar | title ends x≈375, mono semi-bold, spacing ≈1.8 | spacing 4 → ended x≈460 | `pk::TitleBar` font 11 DemiBold, spacing 1.8 | fixed |
| 2 | t000.0/t001.0 | title dot | dot pulses (dim → bright) | static | `setPulse(true)`, 15 fps rect repaint | fixed |
| 3 | t000.0 | KPI labels / sub-labels | spacing ≈2.2 / 1.6, medium weight | spacing 3, too wide | fonts adjusted | fixed |
| 4 | t000.0 | segmented pills | text medium, active pill inset 1–2 px | regular, pill full width | font Medium + `setPillInset(1.5)` | fixed |
| 5 | t000.0 | table header | 11 px bold, spacing ≈1.2 | 10 px spacing 1.8 | font changed | fixed |
| 6 | t000.0 | symbol / % columns | bold, tracking ≈0.4 | demi-bold, no tracking | fonts | fixed |
| 7 | t000.0 | status badges | fills #071f22 / #3f200d, text #6fbcc8 / #c66a62, smaller caps | brighter, larger | `drawStatusBadge` colours + size | fixed |
| 8 | t000.0 | footer | `Terminal Data · HH:MM:SS` only | + long note | note only while history < 20 h | fixed |
| 9 | t000.0 | footer font | condensed | Inter | `pk::Footer` uses Rajdhani | fixed |
| 10 | t000.0 | footer badge | `● DELAYED` | `TEST FIXTURE` in fixture mode | real mode shows DELAYED; fixture badge kept (test only) | by design |
| 11 | t000.0 | title-bar right | nothing | Myfxbook connect / log-out badge | needed for the user's account (login rule) | deviation |
| 12 | t001.5 | chart skeleton | ≈25 warm-grey bars on #0d0b07, ≈0.5 s | none when the candles are cached | skeleton restyled + kept ≥ 0.5 s | fixed |
| 13 | t001.5 | history bars sweep in | yes | yes | — | ok |
| 14 | t002.0 | modal header | symbol cut off (outside recording) | `EUR/USD Euro / U.S. Dollar [badge] ×` | designed in the same style | deviation (open point) |
| 15 | t002.0 | chart toolbar | TradingView: style buttons, compare, Indicators, camera | own chart: candle button + source | own QPainter chart (decision) | deviation |
| 16 | t002.0 | legend | `Euro / U.S. Dollar · 1h · OANDA`, 16 px | `EUR/USD · 1h …`, 14 px | full name, 16 px; source Yahoo | fixed (source differs) |
| 17 | t002.0 | price/time axis | 13 px, time labels semibold | 12 px | fonts 13 | fixed |
| 18 | t004.5 | crosshair date tag | time labels under the tag hidden | overlapped | labels under the tag skipped | fixed |
| 19 | t002.0 | range buttons | `… M 6M YTD 1Y 5Y All` | `5D 2W 1M 3M 6M 1Y` | Yahoo 1 h history ≤ 730 d; ranges kept | deviation |
| 20 | t009.0 | price-scale hover | `USD ⌄`, `A`, `L` | none | TradingView-only controls | deviation |
| 21 | t002.0 | pie | top face y 137–285, wall ≈15 px | ry 68, wall 22, 7 px high | rx 118 ry 74 depth 15, centre +7 | fixed |
| 22 | t002.0 | stats labels | condensed 12 px #575957, computed #3b3b3a | Inter 11 brighter | condensed + colours | fixed |
| 23 | t002.0 | stats values | mono semi-bold | regular | DemiBold | fixed |
| 24 | t002.0 | panel titles / chips | mono 9 bold | normal | weights | fixed |
| 25 | t015.0 | panel notes, narrative, footnote | condensed (Rajdhani-like) | Inter | condensed 11/14/12 | fixed |
| 26 | t015.0 | modal bottom | narrative + footnote end ≈20 px above the frame | 23 px more space | content height 1645 | fixed |
| 27 | t013.5 | OI bars | 26 weekly bars, gradient | same | — | ok |
| 28 | t015.0 | volume/position bars | weekend gap = flat tiny bars | fixture has no gap | data-driven (fixture is hourly without gaps) | n/a |

28 findings: 21 fixed, 6 deliberate deviations (TradingView → own chart, login badge, modal left part), 1 n/a.
