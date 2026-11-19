# Audit — News › News Feed (30 frames, t000.0–t014.5; all 30 opened)

Hooks: DW_FEED_TOPIC, DW_FEED_HOVER=<card>, DW_FEED_OPEN=<index|lead|src:id>, DW_FEED_READER_SCROLL, DW_FEED_LANG.
Screenshots: scratchpad/shots/news/audit/f_t*.png.

| # | Frames | Region | Reference | App (before) | Fix | Status |
|---|---|---|---|---|---|---|
| 1 | t000 | cards / lead | market news only | MarketWatch personal-finance pieces (Medicare, 401(k), caregiving) in cards | lifestyle filter extended (Medicare, 401(k)/IRA/Roth, caregiving, tips, "your money"…) | fixed |
| 2 | t005–t014.5 | reader meta | `INVESTINGLIVE · ADAM BUTTON` (no age) | author + age | age only when no author | fixed |
| 3 | t008–t011 | reader sub-heads | "Monday, Oct 26" same size/weight as body, white | bold, larger | regular weight 16 px white | fixed |
| 4 | t006.5 | reader body | ≈15–16 px light condensed | 17 px | 16 px | fixed |
| 5 | t006.5 | orb | dark red-brown mottled sphere, even amber rim, soft halo, faint specks | crescent highlight, bright rim on one side | centred gradient, mottling, even rim, specks in the halo (cached layers, 12 fps) | fixed |
| 6 | t006.5 | panel header text | condensed (Rajdhani-like) `ALFRED`, `ARTICLE LOADED`, `MUTED` | monospace | condensed 11–12 px | fixed |
| 7 | t006.5 | panel content | "Ask Alfred…" + chat input | automated read (key points, mentions, quotes, tone) + find box | AUTOMATED READ (user decision: no LLM) | deviation, documented |
| 8 | t000 | images | every card has a photo | CNBC / Yahoo / investingLive feeds carry no image → generated source tile | none without scraping articles (decision: feed content only) | deviation |
| 9 | t000 | header sources | 5 chips | 8 chips (+ CENTRAL BANKS, FINANCIALJUICE, NASDAQ), Investing.com off by default | keep (lead decision: Investing.com robots 403) | deviation |
| 10 | t000 | lead | photo lead | lead may have no image when no market story with image in 6 h | ranking prefers images | data |
| 11 | 1280x800 | automated read | — | quotes and tone overflowed under the find box | fewer key points on short windows, sections drawn only when they fit | fixed |
| 15 | t008–t011, t013–t014.5 | reader wheel scroll | smooth, ≈160 px per wheel step | jumped 120 px per step, no animation | animated wheel scroll (160 px, 160 ms ease-out; trackpads stay native) | fixed |
| 16 | t000–t014.5 (all 30) | second pass, every frame opened | t000.5–t001.0 pointer to chip; t002–t004 idle with ticker; t005–t006.5 reader FR…→EN; t007–t007.5 idle; t011.5–t012.5 idle scrolled | no other new difference | — | ok |
| 12 | t001.5 | CENTRAL BANK filter | tag line, 32 stories | same behaviour | — | ok |
| 13 | t004.5 | card hover | gold bg, gold title, `+` button | same | — | ok |
| 14 | t005 / t006.5 | FR… / EN chip | `FR...` while loading then `EN` + AUTO-TRANSLATED | same | — | ok |

Checked and matching: MARKET WIRE title, ticker (real speed 40 px/s, reference is 6× sped up), LEAD STORY brackets,
filter bar chips, LATEST HEADLINES ranking and labels, footer, BACK TO FEED, queue chip with ×, title underline,
hero image frame, smooth scroll of the article only (t008–t013.5).
