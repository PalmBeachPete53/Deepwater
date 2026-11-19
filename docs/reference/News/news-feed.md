# News › News Feed ("MARKET WIRE")

Source video: `Reference/News/news-feed.mp4` (15 s, frames `t000.0`–`t014.5`, 1600x966).
Coordinates at 1600x966, origin top-left of the page area.

## 1. Purpose

A visual newswire. A source bar and live clock, a scrolling headline ticker, a large lead story with image,
topic filter chips (Top impact, Central bank, Inflation, Employment, GDP, PMI, Retail sales, Earnings) with a
search box, a list of story cards with thumbnails, and a right column of the latest headlines ranked by impact.
All text is auto-translated to the user's language (French in the video). Clicking a story opens an in-app
article reader with the full text, a reading queue and an "Alfred" AI panel to ask questions about the article.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–0.5 | Idle, unfiltered feed (`400 STORIES`). Ticker scrolls right→left continuously. Clock ticks. |
| 1.0 | Cursor moves to the `CENTRAL BANK` chip. |
| 1.5 | Click `CENTRAL BANK`: chip becomes active, lead story and card list are replaced instantly; story count becomes `32 STORIES`. Right headline column and ticker unchanged. |
| 2.0–4.0 | Idle (ticker moving). |
| 4.5 | Hover the first card: card bg turns dark gold, title turns gold, a `+` (add to queue) button appears at the right of the card. |
| 5.0 | Click the card: the whole page is replaced by the article reader (feed header/ticker hidden). Translate chip shows `FR...` (loading). |
| 5.5–6.0 | Reader loaded, original English text. |
| 6.5 | Translate chip shows `EN` + `AUTO-TRANSLATED` label; body re-layouts slightly (paragraphs shift ≈16 px). |
| 7.0–7.5 | Idle. |
| 8.0–11.0 | Mouse-wheel scroll down the article (smooth, ≈160 px per step), revealing paragraphs and day sub-heads. Right Alfred panel stays fixed. |
| 11.5–12.5 | Idle. |
| 13.0–14.5 | Scroll back up to the top. |

The header clock advanced 24 s in 4 s of video (23:22:08 → 23:22:32): the video is probably sped up ≈6x (cursor motion looks natural though — see open questions).

## 3. Layout (1600x966)

Feed view:
```
y=0   +------------------------------------------------------------------------------------------+
      | MARKET WIRE  SOURCES [INVESTING.COM][INVESTINGLIVE][YAHOO FINANCE][CNBC][MARKETWATCH] [●LIVE] 23:22:17   AUTO-TRANSLATED [文A FR] 32 STORIES |
y=41  | 14h ago Headline ◆ 14h ago Headline ◆ …  (marquee)                                        |
y=86  +-------------------------------------------------------------+ +------------------------+
      | ┌● LEAD STORY                                              ┐| | LATEST HEADLINES       |
      | [image 421x260]  CENTRAL BANK                               || | HIGH IMPACT · SRC · 6H |
      |                  Big title 2 lines                          || | headline (≤3 lines)    |
      |                  INVESTING.COM · 6H AGO                     || | IMPORTANT · …          |
y=386 | └                                                           ┘| | …                      |
y=398 | [🔍 Search headlines...] [⚡TOP IMPACT][CENTRAL BANK][INFLATION]…[EARNINGS]           | |                        |
y=461 | [thumb] TAG / title / SOURCE · 1H AGO                            | |                        |
      | [thumb] …                                                        | |                        |
y=935 | Terminal Data · 22:52:35                                [● LIVE] | |                        |
      +------------------------------------------------------------------+ +------------------------+
      x=0 x=10                                                   x=1249 x=1267               x=1593
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Header bar | 0 | 0 | 1600 | 40 | bg `#040302`. |
| Ticker | 0 | 41 | 1600 | 36 | bg `#060302`, top/bottom 1 px `#0a0504`/`#0f0602`. |
| Main column | 10 | 86 | 1239 | 880 | Stretches with window. Scrolls (lead + filter + cards); not scrolled in the video. |
| Lead story panel | 10 | 86 | 1239 | 300 | |
| Filter bar | 10 | 398 | 1239 | 51 | |
| Story cards | 10 | 461→ | 1239 | 86 / 107 | gap 9 px. |
| Footer (main column only) | 0 | 935 | 1258 | 31 | |
| Right column | 1267 | 86 | 326 | 880 | Fixed width; independent scroll. |

Reader view:
| Region | x | y | w | h |
|---|---|---|---|---|
| Reader top bar | 0 | 0 | 1600 | 46 (bottom line 1 px `#201f14`) |
| Article column | 199 | 55 | 861 | scrolls |
| Divider | 1259 | 46 | 1 | 920 |
| Alfred panel | 1268 | 55 | 324 | 903 |

## 4. Components

### 4.1 Header bar
- `MARKET WIRE`: condensed bold display font (Oswald/Bebas-like) ≈26 px, uppercase, yellow `#f2da93` (core ≈`#fbe8c6`), x=14.
- `SOURCES` monospace 9 px spaced 2 px `#b7b1a8` at x=178.
- Source chips (h≈22, x from 236, gap 6): `INVESTING.COM`, `INVESTINGLIVE`, `YAHOO FINANCE`, `CNBC`, `MARKETWATCH`; monospace bold 11 px white `#fdfaf0`,
  bg `#231c01`, 1 px border `#464127` (gold-brown), radius 2. All active in the video (toggle filters).
- `● LIVE` chip (x≈750–802): dot gold `#cfab46`, text `#7f7d73` monospace 10 px, 1 px border dim.
- Clock `23:22:17` monospace 13 px, amber `#d6bb9a`, ticking every second.
- Right: `AUTO-TRANSLATED` monospace 9 px spaced `#4a4742`; language chip (x≈1468–1510, h 21) with a translate icon and `FR`, text `#e5c99a`,
  bg `#241806`, gold border; `400 STORIES` / `32 STORIES` monospace 10 px spaced `#4d4b47` (count of stories matching the current filter).

### 4.2 Ticker (y=41–77)
- Continuous marquee scrolling right→left at a constant speed (≈118 px per 0.5 s of video).
- Item: `14h ago` (age, monospace 12 px, `#bbaa82` beige-gold; "ago" slightly darker) + headline (≈13 px semibold `#f9f8f7`), separated by a
  grey diamond `◆` 9 px `#605c54` with ≈16 px padding each side. Items in reverse chronological order, translated text.

### 4.3 Lead story panel (x=10–1249, y=86–386)
- bg `#040300` with a subtle radial glow of dark gold at the top centre (`#0e0800`). 1 px border `#302912` top, `#1a1602` sides, `#190b01` bottom.
- Corner brackets at all four corners: L-shaped, arms 10 px, 3 px thick, bright gold `#f3eaaa`.
- `● LEAD STORY` at (21,101): dot `#d8ba63`, text monospace bold 12 px spaced 2 px `#eedfa3`.
- Image x=11–432, y=125–385 (421x260), cover-cropped, no border.
- Text block x=446:
  - Optional topic tag `CENTRAL BANK` monospace 9 px spaced 2 px `#d3c194` (shown only when the story is classified; absent on the unfiltered lead).
  - Title: bold sans ≈32 px, line-height 40, white `#fefefc`, max 2 lines, width ≈735 px.
  - Meta: `INVESTING.COM` (monospace bold 10 px white) `·` `6H AGO` (monospace 10 px `#4c4b46`).
- Lead = most recent / highest-impact story of the current filter.

### 4.4 Filter bar (x=10–1249, y=398–449)
- bg `#090703`, 1 px border `#231e14` (top `#181a12`).
- Search box x=22–207, y=408–440: magnifier + placeholder `Search headlines...` 12 px `#403e37`, 1 px border `#252114`, bg transparent.
- Chips (h≈31, gap 8, monospace bold 11 px spaced 1.5 px uppercase): `⚡ TOP IMPACT`, `CENTRAL BANK`, `INFLATION`, `EMPLOYMENT`, `GDP`, `PMI`, `RETAIL SALES`, `EARNINGS`.
  - Inactive: text `#c0b9ad`, bg `#0e0705`, 1 px border `#282317`.
  - Active: bg `#39310f`, text yellow `#f9e494`, top border `#4e4527`, bottom border 2 px bright gold `#e0d07c`.
  - Single-select (one topic at a time) in the video.

### 4.5 Story card
- x=10–1249, h≈86 (one-line title) or ≈107 (two-line), gap 9 px; bg `#040302`, 1 px border `#241e1a`, square corners.
- Thumbnail 96x68 at x=20, vertically centred, radius ≈2, cover crop.
- Text x=125: optional tag (monospace 9 px spaced, `#cebb96`), title semibold ≈20 px white `#fefefc` (max 2 lines), meta `YAHOO FINANCE · 1H AGO`
  (source monospace bold 10 px white, age `#45443f`).
- Hover: bg `#1a1302`, border `#312d12`/`#342e20`, title colour gold `#f2df93`, and a 31x31 `+` button at x=1210 (1 px gold border `#7f733a`, `+` `#dbca8e`) = add to the reading queue.

Unfiltered stories (FR): lead `L'Arabie saoudite intercepte un missile Houthi alors que Trump menace de nouvelles frappes` (INVESTING.COM · 29M AGO);
cards: `La volatilité du pétrole remet l'action Virtu Financial et les plateformes de trading sous les projecteurs` (YAHOO FINANCE · 1H AGO),
`Ce que les sceptiques d'Orezone Gold ont négligé` (YAHOO FINANCE · 1H AGO), `Alerion Clean Power et 2 autres actions d'énergie propre à surveiller` (YAHOO FINANCE · 1H AGO),
`Marco Rubio appelle ceux qui cherchent un changement à Cuba à s'associer à la CIA` (INVESTING.COM · 1H AGO),
`La capacité cloud AWS d'Amazon largement épuisée jusqu'en 2027, selon le PDG` (INVESTING.COM · 1H AGO).

CENTRAL BANK filter: lead `Flot de résultats, la réunion de la Fed testera le rallye fragile du marché de l'IA` (INVESTING.COM · 6H AGO);
cards (all tagged CENTRAL BANK): `La semaine macro des résultats qui s'annonce : La Fed et les grandes technologies publient le même jour` (INVESTINGLIVE · 9H AGO),
`Citi s'attend à ce que l'USD/JPY reste limité à une fourchette au milieu de la décision de taux de la BoJ` (INVESTING.COM · 18H AGO),
`L'indice PMI d'octobre de la zone euro manque les estimations alors que la croissance ralentit, …` (INVESTINGLIVE · 20H AGO),
`Les ventes au détail britanniques chutent plus que prévu en septembre, compliquant les perspectives de hausse des taux de la BOE` (INVESTINGLIVE · 21H AGO), …

### 4.6 Right column: LATEST HEADLINES (x=1267–1593)
- Panel bg `#040300`, 1 px border `#392f24` (sides), corner brackets on top corners (gold).
- Title `LATEST HEADLINES` condensed bold ≈16 px spaced 1 px, gold `#f1e096`, at y≈98.
- Row (separators 1 px `#141208`/`#231a0a`, padding 12): meta line monospace 9–10 px:
  impact label (`HIGH IMPACT` red `#9e6a6a`; `IMPORTANT` gold `#c7b691`) · source (bold white `#fefdf9`) · age (`6H AGO`, `#4f4b43`);
  headline semibold ≈16 px white, max 3 lines then `...`.
- Ranking: high impact first then important, each newest first. Unaffected by the topic filter.
- Rows seen: HIGH IMPACT INVESTING.COM 6H — `Flot de résultats, la réunion de la Fed testera le rallye fragile du marché de l'IA`;
  HIGH IMPACT INVESTINGLIVE 9H — `La semaine macro des résultats qui s'annonce : …`;
  IMPORTANT INVESTING.COM 14H — `L'indice PMI manufacturier américain dépasse les attentes, signalant une expansion du secteur`;
  IMPORTANT INVESTING.COM 14H — `L'indice PMI des services américains correspond aux prévisions, signalant une croissance régulière`;
  IMPORTANT INVESTINGLIVE 14H — `L'indice PMI flash des services S&P Global américain d'octobre est de 57,9 contre 58,0 attendu`;
  IMPORTANT INVESTINGLIVE 15H — `Ventes au détail canadiennes août +1,2% vs +1,3% attendu`;
  IMPORTANT INVESTING.COM 18H — `Les ventes au détail britanniques chutent de 0,4 % en septembre …`; …

### 4.7 Footer (main column)
- `Terminal Data · 22:52:35` monospace 11 px `#53514c`; `● LIVE` badge at x=1204–1254 (bg `#2d2818`, text `#e8d294`).

### 4.8 Article reader
- Top bar (y=0–46, bg `#0f0a02`):
  - `← BACK TO FEED` button x=10–129, y=8–38: solid yellow `#fed90b` bg, dark text monospace bold 11 px spaced.
  - `QUEUE` monospace 9 px spaced `#878279`; queue chip(s) x=181–466: `1. Earnings macro week ahead: The Fed and... ×` monospace 11 px gold `#c9b67a`, bg `#3f3706`, 1 px border `#534c25`.
- Article column x=199–1060:
  - Meta (y≈72): `INVESTINGLIVE` monospace bold 11 px gold `#e8c998` + `·` + `ADAM BUTTON` (`#727169`).
  - Translate chip (x≈907–966, y 61–94): icon + `EN` (or `FR...` while loading) gold on `#382f01`, and `AUTO-TRANSLATED` monospace 9 px `#494742`.
  - Title: bold sans ≈46 px, line-height ≈52, white `#f5f3ee`, 2 lines (in original language EN after translation toggle state).
  - Gold underline 55x3 px `#f1d699` under the title (y≈212).
  - Hero image x=200–1059, y=230–681 (860x451), 1 px border.
  - Body: ≈15 px light sans (Rajdhani-like), colour `#e4e3e0`, line-height ≈26, paragraph spacing ≈26; sub-heads bold white
    (`Monday, Oct 26`, `Tuesday, Oct 27 – FOMC day one`, `Wednesday, Oct 28 – Fed decision at 2 pm ET`, `Thursday, Oct 29 – September PCE`).
- Alfred panel (x=1268–1592, y=55–958): bg `#080603`, 1 px border `#272215`.
  - Header: `● ALFRED` (dot `#eedda2`, text monospace bold 10 px `#aca89e`), status `ARTICLE LOADED` (gold `#ad9e7a`), chip `🔇 MUTED` (red `#b76e6c`, bg `#2f110f`).
  - Orb ≈150 px centred at (1430,185): glowing amber sphere (rim `#835828`, core `#502f09`), text `ALFRED` (`#f6e7cb`) and `TRADE SIDER 1.0` (`#af9b80`) inside; soft pulse animation.
  - Prompt `Ask Alfred anything about this article.` 12 px `#b7b5b0` at y≈298.
  - Input at bottom (x=1275–1560, y=925–952): placeholder `Ask about this article...` `#413c33`, 1 px border; send button 24x24 gold paper-plane `#d9c884`.

## 5. Interactions

- Topic chip click → filter list + lead (instant, no animation), updates `N STORIES`.
- Search box filters headlines as you type (not shown).
- Source chips toggle sources (not shown).
- Language chip toggles auto-translation (FR shown).
- Ticker: endless marquee; probably pauses on hover and opens the story on click (not shown).
- Card hover → gold state + `+` button (adds to QUEUE). Card click → article reader.
- Reader: translation toggle (EN ↔ FR); BACK TO FEED; queue chip `×` removes; wheel scroll of the article column only; Alfred input (chat about the article).

## 6. Data

| Field | Sample | Format | Update | Source on screen |
|---|---|---|---|---|
| Stories (title, source, age, image, topic tag) | see 4.5 | age `29M AGO`, `1H AGO`, `6H AGO`, ticker `14h ago` | live (RSS/API polling) | INVESTING.COM, INVESTINGLIVE, YAHOO FINANCE, CNBC, MARKETWATCH |
| Topic classification | CENTRAL BANK, INFLATION, … | tag | per story (keyword/AI) | computed |
| Impact level | HIGH IMPACT / IMPORTANT | label | per story | computed |
| Story count | 400 / 32 | int | live | — |
| Clock | 23:22:17 | HH:MM:SS | 1 s | — |
| Translation | FR | — | on demand | "AUTO-TRANSLATED" |
| Article full text, author, hero image | INVESTINGLIVE · ADAM BUTTON | — | on open | source site |
| Footer time | 22:52:35 | HH:MM:SS | each refresh | "Terminal Data" |

## 7. Key frames

- `t000.0.jpg` — unfiltered feed, 400 stories.
- `t001.5.jpg` — CENTRAL BANK filter active, 32 stories.
- `t004.5.jpg` — card hover with `+` button.
- `t005.0.jpg` — reader loading (`FR...`).
- `t006.5.jpg` — reader loaded, `EN · AUTO-TRANSLATED`, Alfred panel.
- `t010.0.jpg` — reader scrolled (body and sub-heads).
- `t014.5.jpg` — reader back at top.

## 8. Open questions

- Video speed: clock runs ≈6x; ticker speed in real time is probably ≈40 px/s.
- What TOP IMPACT means exactly (impact ranking rule) and how impact levels are computed.
- Whether multiple topic chips can be combined; whether the right column follows filters (it did not here).
- Behaviour of the reader with paywalled or failed article fetches.
- The reader title stays in English while the toggle says EN — confirm what the toggle shows (current language vs target).
- Alfred "MUTED" refers to voice output; voice features not shown.
- Main column scroll behaviour (lead panel scrolls away or stays) not shown.
