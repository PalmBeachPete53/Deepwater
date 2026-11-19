# News › Bank Research

Source video: `Reference/News/bank-research.mp4` (23 s, frames `t000.0`–`t023.0`, 1600x966).
Coordinates at 1600x966, origin top-left of the page area.

## 1. Purpose

A library of the research PDFs that the major banks publish themselves (Danske Bank, MUFG, ING, KBC, Deutsche
Bank Research Institute, J.P. Morgan AM…), listed newest first in the left column with bank, category and age.
The centre shows a rotating 3D globe with beacons on the banks' head offices ("select a beacon or a bank to open
its file"); opening a note replaces the globe by an embedded PDF viewer. The right column is a registry of the
21 systemically important banks with total assets and ticker.

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–1.0 | Idle. Globe rotates slowly (west→east: land moves to the right, ≈65 px/s near the centre); a gold radar-like sweep wedge rotates around the globe; the cursor sits near the TD/JPM beacons. |
| 1.5 | Cursor on the first list row (Danske · Financials): row in hover/pressed state (gold bg, content shifted ≈3 px left). |
| 2.0 | Click: the row becomes selected (gold bg + 3 px left gold bar). The centre is replaced instantly by the PDF viewer panel showing the note (page 1/10 at 95 %). |
| 2.5–10.0 | Idle; cursor moves over the PDF. Only blinking changes (gold dot and block cursors). |
| 10.5–17.0 | Mouse-wheel scroll inside the PDF (≈20 px per 0.5 s, smooth): banner scrolls away, body, "Earnings and efficiency" chart, "Key figures" table, "Capitalisation" appear. |
| 17.5–19.0 | Idle. |
| 19.5–23.0 | Scroll back up to the top of page 1. |

## 3. Layout (1600x966)

```
y=0   +------------------------------------------------------------------------------------------+
      | 🏛 ● GLOBAL BANKING NETWORK — LIVE SURVEILLANCE ▮           [• 21 INSTITUTIONS · 3 REGIONS · SEC EDGAR] |
y=37  +----------------+--------------------------------------------------+----------------------+
      |RESEARCH NEWS DATA FILINGS|                                     | ● BANK REGISTRY —     |
      | 📄 BANK RESEARCH — DAILY  |         rotating globe              |   SYSTEMIC INSTITUTIONS|
      |    PDFS  35 today · 412   |     (or PDF viewer panel)           | 01 JPM ... $5.02T [JPM]|
      | [ALL BANKS ⌄][ALL PUBL. ⌄]|                                     | ...                   |
      | TODAY                     |                                     | 21 NOMURA ¥62.6T [NMR] |
      | DANSKE FINANCIALS     12h |                                     |                       |
      | title 2 lines             |                                     |                       |
      | ⎘ PDF · OPEN IN TERMINAL  |  SELECT A BEACON OR A BANK TO OPEN ITS FILE                 |
      | footer: sources list      |                                     |                       |
y=966 +----------------+--------------------------------------------------+----------------------+
      x=0          x=362                                            x=1260                x=1600
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Page strip | 0 | 0 | 1600 | 37 | bg dark warm `#120d04`, bottom border gold-brown. |
| Left column | 0 | 37 | 362 | 929 | bg `#050402`, right border 1 px `#1c1a17`. List scrolls. Fixed width. |
| Left tabs | 0 | 42 | 362 | 25 | |
| Left header band | 0 | 67 | 362 | 52 | bg `#231b04` (olive). |
| Dropdowns | 9 / 185 | 126 | 169 each | 28 | |
| List | 0 | 160 | 362 | 760 | |
| Left footer | 0 | 920 | 362 | 46 | sources text. |
| Centre | 362 | 37 | 898 | 929 | Globe canvas; stretches. |
| PDF viewer panel | 377 | 52 | 869 | 900 | Floating panel over the globe area (15 px margin). |
| Right column | 1262 | 37 | 338 | 929 | bg `#080603`, left border 1 px `#1f1e1b`. Fixed. |

## 4. Components

### 4.1 Page strip
- Left: bank/building icon (gold, x≈12), gold dot (`#948954`, blinks), `GLOBAL BANKING NETWORK — LIVE SURVEILLANCE` monospace 11 px
  uppercase spaced ≈4 px `#5f5b51`, blinking block cursor 6x14 `#f1daa0` at x≈410.
- Right badge x=1318–1586, y=8–28: `• 21 INSTITUTIONS · 3 REGIONS · SEC EDGAR` monospace bold 10 px spaced, text `#eddc98`, bg `#524604`→darker, 1 px gold border.

### 4.2 Left tabs
- `RESEARCH` (active: icon + text monospace bold 12 px spaced 3 px `#f9e8ab`, bg `#4c2d04`→dark gold, underline 3 px `#f7e069` x=0–95),
  `NEWS`, `DATA`, `FILINGS` (inactive `#625e49`, each with a small icon). Only RESEARCH is shown in the video.

### 4.3 Header band
- Doc icon (gold) + `BANK RESEARCH — DAILY PDFS` monospace 10 px spaced 3 px `#67624f` + blinking block cursor on the next line;
  right `35 today · 412` monospace 10 px dim (35 notes published today, 412 in the library).

### 4.4 Dropdowns
- `ALL BANKS ⌄` and `ALL PUBLICATIONS ⌄`: monospace 12 px spaced 2 px white `#f8f8f6`, chevron white, bg `#040501`, 1 px border `#18170d`, h 28.

### 4.5 Note list
- Group label `TODAY` monospace bold 10 px spaced 2 px `#e1d5a6` at y≈171 (other groups presumably `YESTERDAY`, dates).
- Row (h≈93, separators 1 px `#171511`), padding-left 9:
  - Bank logo 18x18 (white square with bank mark; MUFG red circle; ING orange lion) at x=9.
  - Bank short name monospace bold 12 px spaced 1 px white `#fefefa` (`DANSKE`, `MUFG`, `ING`).
  - Category monospace 10 px spaced 1 px gold `#d1c497` (`FINANCIALS`, `MATERIALS`, `FX WEEKLY`, `UTILITIES`, `REAL ESTATE`, `RTM NORWAY`, `ARTICLE`, `EUROPEAN MACRO WEEKLY`).
  - Age right-aligned at x≈351, monospace 10 px `#54534c` (`12h`, `13h` …).
  - Title semibold ≈13 px (condensed sans) white, line-height 18, max 2 lines, x=38.
  - Footer `⎘ PDF · OPEN IN TERMINAL` monospace 9 px spaced `#4f4d49`.
  - Hover/pressed: bg `#2a2104`, content shifts ≈3 px. Selected: bg `#2a2104` + left bar 3 px `#f5e093`.

| Bank | Category | Age | Title |
|---|---|---|---|
| DANSKE | FINANCIALS | 12h | Länsförsäkringar Bank AB (MW) - A EUR senior deal appears more likely in early 2027 than late 2026 |
| DANSKE | MATERIALS | 12h | Kemira - (MW) - Q3 26: Cost pressure and soft demand continue to burden profitability |
| MUFG | FX WEEKLY | 13h | FX Weekly |
| DANSKE | UTILITIES | 13h | TVO (OW) - Steady Q3 26 performance leads to record-low leverage |
| DANSKE | REAL ESTATE | 14h | Akelius Residential AB (OW) - Q3 26: Strong balance sheet despite valuation headwinds |
| DANSKE | RTM NORWAY | 14h | RtM Norway - Norway the new "safe haven" market |
| ING | ARTICLE | 15h | Federal Reserve preview: A pause before a December hike |
| DANSKE | FINANCIALS | 16h | SBAB Bank AB - (UW) - Rock solid asset quality, and NII affected by higher deposit costs |
| MUFG | EUROPEAN MACRO WEEKLY | 16h | (cut off) |

- Left footer (y≈922–960, monospace 8 px spaced, `#4b4a45`, 3 lines):
  `ING THINK · MUFG RESEARCH · KBC ECONOMICS · DANSKE BANK RESEARCH · DEUTSCHE BANK RESEARCH INSTITUTE · J.P. MORGAN ASSET MANAGEMENT — THE BANKS' OWN PUBLISHED PDFS`.

### 4.6 Globe (centre, default state)
- Background `#050402` with a sparse starfield of 2–3 px grey dots (`#2b2927`), static.
- Globe: centre ≈(812,505), radius ≈270. Sphere body dark navy-grey `#12151d` with soft shading (lit from upper-left, rim darker),
  graticule lines every ≈15° in 1 px `#2c2f35` (≈40 % alpha).
- Land drawn as a dot grid (≈3 px dots, ≈7 px pitch). Lit region (North America in the frames) dots gold (`#988f69` measured, core ≈`#d8c060`);
  other land dots grey `#46484e`. Back-side land is hidden.
- Bank beacons: at each head-office lat/lon a 2 px gold vertical bar (16 px tall, `#eee394`) with a label chip to its right
  (ticker in monospace bold 10 px gold `#d1c587` on a near-black chip): `WFC`, `TD`, `JPM`, `GS`, `BAC`, `NWG`, `ING`, `LYG`, `UBS`, `SAN`…
  Beacons hide when on the far side.
- Thin gold arcs (1 px, `#6d6a51`, ~50 % alpha) connecting regions / leaving the globe, rotating with it.
- Orbit ring: thin gold circle (1 px, `#6d6a51`) radius ≈400, drawn partially (arc fading), plus an outer dotted grey ring (dots every ≈15 px, `#272623`) radius ≈430.
- Sweep: a translucent gold wedge (radar beam, `#3a3010` ~25 % alpha) outside/behind the globe rotating clockwise (lower-left at t=0 → left at t=1.5).
- Hint at the bottom centre (y≈943): `SELECT A BEACON OR A BANK TO OPEN ITS FILE` monospace 11 px spaced 6 px `#52514f`.
- Globe rotation: continuous, ≈14°/s in the video.

### 4.7 PDF viewer panel (x=377–1246, y=52–952)
- Panel bg `#070503`, 1 px border `#3e3b2c`, drop shadow; slight gap (15 px) to the columns.
- Header (y≈55–110):
  - `FINANCIALS · 23 OCT 2026, 11:11` monospace 10 px spaced 3 px `#585750` + blinking block cursor `#f7e8a6`.
  - Bank logo 22x22 (white tile) + `DANSKE` monospace bold 13 px white + title 15 px semibold white, single line with ellipsis
    (`Länsförsäkringar Bank AB (MW) - A EUR senior deal appears more likely in early 2...`).
  - Right buttons (h≈22): `⤓ PDF` (x 1076–1124), `⧉ SOURCE` (x 1130–1204), `×` (x≈1220) — monospace 10 px spaced `#85827b`, 1 px border `#2a2820`.
- PDF area x=385–1240, y=117–950: the standard Chromium PDF viewer (toolbar bg `#3c3c3c`/`#4d4d4d`: `≡`, `Company Note`, page `1 / 10`, `−`, `95%`, `+`,
  fit-page, rotate, annotate, undo, redo, add, download, print, `⋮`) over a grey gutter `#2d3130`; the page is rendered at 95 %.
  In Qt this is a QPdfView (or QWebEngineView with the built-in PDF viewer) with its own toolbar.

### 4.8 Right column: BANK REGISTRY
- Header (y≈47–75): gold dot `#978f57` + `BANK REGISTRY — SYSTEMIC INSTITUTIONS` monospace 10 px spaced 4 px `#8a7e4a` (2 lines) + blinking block cursor; bottom border gold-brown.
- Rows h≈35 (separators 1 px `#1b170c`):
  index `01` monospace 12 px `#535249` (x≈1273), logo 18x18 (x≈1296), name monospace bold 12 px spaced 1 px white `#fefdf7` (x=1322),
  total assets right-aligned at x≈1545 monospace 12 px `#828073`, ticker chip x=1554–1593 (h 20, bg `#483e01`, 1 px border `#6b5d1a`, text `• JPM` monospace bold 10 px `#f3e4a5`).
  No US listing (UBS): value `—` and chip dimmed (bg `#110f0b`, text `#54514c`).

| # | Name | Assets | Ticker |
|---|---|---|---|
| 01 | JPMORGAN | $5.02T | JPM |
| 02 | BANK OF AMERICA | $3.50T | BAC |
| 03 | CITI | $2.66T | C |
| 04 | WELLS FARGO | $2.28T | WFC |
| 05 | GOLDMAN SACHS | $2.13T | GS |
| 06 | MORGAN STANLEY | $1.68T | MS |
| 07 | RBC | C$2.34T | RY |
| 08 | TD | C$2.10T | TD |
| 09 | HSBC | $3.23T | HSBC |
| 10 | BARCLAYS | £1.54T | BCS |
| 11 | LLOYDS | £944B | LYG |
| 12 | NATWEST | £715B | NWG |
| 13 | DEUTSCHE BANK | €1.44T | DB |
| 14 | UBS | — | UBS (dim) |
| 15 | SANTANDER | €1.87T | SAN |
| 16 | BBVA | €860B | BBVA |
| 17 | ING | €1.06T | ING |
| 18 | MUFG | ¥426T | MUFG |
| 19 | SMBC GROUP | ¥292T | SMFG |
| 20 | MIZUHO | ¥295T | MFG |
| 21 | NOMURA | ¥62.6T | NMR |

Order: US (6), Canada (2), UK (4), Europe (5), Japan (4) — 3 regions (Americas, Europe, Asia). Assets in local currency with T/B suffix, 2–3 significant decimals.

## 5. Interactions

- Click a list row → opens that note in the PDF viewer panel (instant), row selected.
- Click a beacon on the globe or a registry row → opens that bank's file (per hint; not shown). Probably filters the list to the bank.
- Dropdowns ALL BANKS / ALL PUBLICATIONS filter the list (not shown).
- `⎘ PDF · OPEN IN TERMINAL` = opens the PDF in the app (same as row click); `⤓ PDF` downloads, `⧉ SOURCE` opens the bank's web page, `×` closes the viewer (back to the globe).
- PDF viewer: wheel scroll, zoom, page navigation, print, download (native viewer controls).
- Animations: globe rotation, sweep wedge rotation, blinking cursors and gold dots.
- Tabs NEWS / DATA / FILINGS (SEC EDGAR filings) exist but not shown.

## 6. Data

| Field | Sample | Format | Update | Source on screen |
|---|---|---|---|---|
| Research notes (bank, category, title, age, PDF URL, published time) | DANSKE · FINANCIALS · 12h | age in h; header `23 OCT 2026, 11:11` | polled daily/hourly | ING THINK, MUFG RESEARCH, KBC ECONOMICS, DANSKE BANK RESEARCH, DEUTSCHE BANK RESEARCH INSTITUTE, J.P. MORGAN ASSET MANAGEMENT |
| Counters | 35 today · 412 | int | on poll | — |
| Bank registry: total assets | $5.02T | currency + T/B | quarterly | SEC EDGAR (badge) / filings |
| Tickers | JPM | US ticker / ADR | static | — |
| HQ coordinates for beacons | — | lat/lon | static | — |
| PDF content | Danske "Company Note" 10 pages | PDF | on open | bank site |

## 7. Key frames

- `t000.0.jpg` — globe view, list, registry.
- `t001.5.jpg` — row hover/press, globe rotated.
- `t002.0.jpg` — PDF viewer opened, row selected.
- `t007.5.jpg` — PDF viewer idle (top of page 1).
- `t013.0.jpg` / `t017.0.jpg` — PDF scrolled (charts, key figures table).
- `t023.0.jpg` — back near the top.

## 8. Open questions

- What makes land dots gold vs grey (sun-lit side, region with many banks, or the radar sweep's trail).
- Beacon hover / click behaviour and the "bank file" view (list filtered? bank profile?).
- NEWS / DATA / FILINGS tabs content.
- Two static yellow pointer arrows at x≈380 (y≈128 and y≈322) appear in every PDF frame: most likely recording artefacts (ghost cursors), not UI.
- Older groups after TODAY (dates?) and list paging.
- Whether the PDF viewer should be the native Chromium one (as in the video) or QPdfView with an equivalent toolbar.
