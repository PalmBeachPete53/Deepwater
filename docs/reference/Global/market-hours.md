# Global › Market Hours

Source video: `Reference/Global/market-hours.mp4` (7 s, frames `t000.0`–`t006.5`, 1600x966; extra frames every
0.1 s in `frames-fine/market-hours/` were used for the clock sweep).
All coordinates are at 1600x966, origin top-left of the page area (the app header is not in the video).
Colours measured with `tools/pick`. Shared widget kit: `_shared-notes.md`.

**Playback speed.** The recording is time-lapsed exactly **6x**: the clock seconds advance 3 s per 0.5 s of
video (16:18:33 at t0.0 → 16:19:12 at t6.5). Speeds below are converted to real time.

## 1. Purpose

Shows the four FX sessions (Sydney, Tokyo, London, New York) on the user's own clock: an analogue clock with
the sessions drawn as coloured 24-hour arcs and a "now" marker, a 24-hour timeline with one bar per session
for the user's current day and a vertical "now" line, and four session cards with each centre's local time,
open/closed state, a countdown to the next open and the session hours in local and user time. Everything is
daylight-saving aware and Monday-to-Friday only (weekend sessions are not drawn).

## 2. Timeline

| t (s) | What happens |
|---|---|
| 0.0–6.5 | Idle page, all sessions **CLOSED** (Friday 23 Oct 2026, 16:18:33 → 16:19:12 in America/Bogota, UTC−5). The mouse wanders over the timeline and the clock without any hover effect. |
| every frame | Digital seconds and all four card clocks tick (3 s per 0.5 s video); the second hand moves; the radar sweep on the clock face rotates. |
| 0.0, 2.5, 3.0, 5.5, 6.0, 6.5 | Blinking block cursors after `MARKET HOURS — THE FOUR FX SESSIONS` and after `YOUR CLOCK · AMERICA/BOGOTA` are visible (hidden in the other frames). |
| 4.5 (16:19:00) | Minute rolls over: `16:18` → `16:19` everywhere, countdowns `41m` → `40m`. |

## 3. Layout (1600x966)

```
y=0   +--------------------------------------------------------------------------------------------+
      | ⏱ • MARKET HOURS — THE FOUR FX SESSIONS ▮                [• 0/4 SESSIONS LIVE · YOUR CLOCK UTC-5] |  strip h=36
y=37  +--------------------------------------------------------------------------------------------+
y=51  +--------------------+ +---------------------------------------------------------------------+
      | YOUR CLOCK · AMERICA/BOGOTA ▮ | 24-HOUR TIMELINE · YOUR TIME (UTC-5)                       |
      |   (analogue clock  | |            00:00 03:00 06:00 … 24:00                                |
      |    with 4 session  | |  🇦🇺 SYDNEY   [                                    |               ]  |
      |    arcs + now dot) | |  🇯🇵 TOKYO    [■■■■]                               |                  |
      |                    | |  🇬🇧 LONDON   [  ■■■■■■■■■■]                       |                  |
      |     16:18:33       | |  🇺🇸 NEW YORK [        ■■■■■■■■■■]                 | now line         |
      | FRIDAY, 23 OCTOBER 2026 · UTC-5 |                                                          |
      | FX BETWEEN SESSIONS|                                                                      |
y=616 +--------------------+ +---------------------------------------------------------------------+
y=627 +---------+ +---------+ +---------+ +---------+
      | SYDNEY  | | TOKYO   | | LONDON  | | NEW YORK|   4 session cards
y=802 +---------+ +---------+ +---------+ +---------+
y=817   FX SESSIONS ARE THE CONVENTIONAL BUSINESS HOURS … SHOWN ON YOUR OWN CLOCK
        (empty page background below, faint grid texture)
      x=14          x=466 x=477                                                             x=1586
```

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Page strip | 0 | 0 | 1600 | 37 | bg `#110b04`, bottom border 1 px `#4b3e21` at y 36. |
| Clock panel | 14 | 51 | 453 | 566 | Panel style (border `#413c2f`, brackets). Fixed width (recommended). |
| Timeline panel | 477 | 51 | 1110 | 566 | Stretches horizontally. Content (rows) sits in the middle third; top 180 px empty. |
| Session cards | 14 / 409 / 806 / 1201 | 627 | ≈385 each | 176 | Gap ≈11 px; the 4 cards share the full width equally. |
| Disclaimer | 14 | 812 | 776 | 10 | One line. |
| Empty area | 0 | 830 | 1600 | 136 | Page bg with faint grid. |

Page bg `#0c0601`–`#030201` (darker toward the bottom), faint grid of 1 px lines (`#0f0b05` on `#080502`,
≈40 px cells) visible in the empty bottom area.

## 4. Components

### 4.1 Page strip (y 0–37)

- x 13: clock icon (outlined circle with hands, 10 px, `#8a7a3a`), x 38: yellow dot 5 px `#957E1F`→
  `#FCD700`, x 50: `MARKET HOURS — THE FOUR FX SESSIONS` JetBrains Mono 10 px, letter-spacing 3 px,
  uppercase, `#6d6a61`; then a block cursor 6×11 px `#FCD700` that blinks (visible ≈half the frames).
- Right badge x 1337–1586, y 9–27: bg `#1e1803`, border 1 px `#8a7e45`, radius 2, text
  `• 0/4 SESSIONS LIVE · YOUR CLOCK UTC-5` JetBrains Mono Bold 9 px, letter-spacing 1 px, `#FFF2B0`
  (dot yellow). `0/4` = number of sessions open now.

### 4.2 Clock panel (x 14–467, y 51–617)

- Title `YOUR CLOCK · AMERICA/BOGOTA` at (28,72), JetBrains Mono 10 px, letter-spacing 3 px, `#656460`,
  then a blinking block cursor 6×11 px `#FCD700` (same blink as the strip cursor). Time-zone id uppercase
  (IANA name of the user's zone).
- **Analogue clock**, centre (239,292):
  - Outer dotted circle r≈200: 1 px dotted (`#3a3528`, dot every 4 px); around it a sparse ring of tiny
    dots (r 205–215, 1–2 px, `#4a4434`, ~60 dots, slight random jitter) — "star dust".
  - **Session arcs on a 24-hour dial** (00:00 at top, clockwise, 15° per hour), each the session's span
    in the user's time, round caps, concentric:
    - Sydney 15:00→24:00, r≈183, purple `#A78BFA` (core `#B5A2FF`), 5 px, **bright with glow**;
    - Tokyo 19:00→04:00, r≈175, dim red `#6e3434`, 3 px;
    - London 03:00→12:00, r≈165, dim teal `#326860`, 3 px;
    - New York 07:00→16:00, r≈155, dim olive `#70661F`, 3 px.
    One arc is drawn bright: the one whose 24-hour span contains the current time-of-day (16:18 lies in
    Sydney's 15:00–24:00), the others dim. (Rule inferred from one frame state — see open questions.)
  - **Now dot** on the 24-hour dial: filled circle 10 px `#FFDC16` at r≈198, angle = time-of-day × 15°
    (16:18 → 244.5°, at (60,378)).
  - Dial: thin ring r≈144 (1 px `#6a5a1a`); minute ticks (60, 1 px × 4 px, `#6a5c2a`) and hour ticks
    (12, 2 px × 10 px, `#DEC558`; the four quarter ticks 12/3/6/9 slightly longer); numerals `12`, `3`,
    `6`, `9` only, proportional 18 px `#E7DEC1`, at r≈103.
  - Face: radial gradient `#5F5005` (centre) → `#2a2203` (r 120) → `#0c0901` at the ring — a warm
    olive glow.
  - **Radar sweep**: a pale wedge (≈35°, from the centre to r≈190, `#FCD700` at ~10 % alpha with the
    leading edge brightest and a fading trail), rotating clockwise ≈300° per video second → one turn per
    ≈1.2 s video ≈ **7 s real**, independent of the second hand.
  - Hands: hour hand white `#FBF6EB`, 5 px, round caps, length ≈62; minute hand white, 3 px, length
    ≈105; both with soft white glow. Second hand yellow `#E9D36C`, 1.5 px, length ≈125 with a 15 px tail
    on the opposite side; it jumps once per second (no sweep).
  - Hub: black disc r 6 with 2 px yellow ring `#FCD700`.
- **Digital time** centred at x 240, y 531: `16:18` JetBrains Mono SemiBold 48 px white `#FFFEF9`, then
  `:33` 26 px yellow `#F8DD55` (baseline aligned).
- Date line y 569: `FRIDAY, 23 OCTOBER 2026 · UTC-5` JetBrains Mono 12 px, letter-spacing 3.5 px,
  `#999991`.
- Status line y 593: `FX BETWEEN SESSIONS` JetBrains Mono 10 px, letter-spacing 3 px, `#64635c`
  (state text when no session is open; when sessions are open it would name them — not seen).

### 4.3 24-hour timeline panel (x 477–1587, y 51–617)

- Title `24-HOUR TIMELINE · YOUR TIME (UTC-5)` at (488,67), JetBrains Mono 10 px, letter-spacing 3 px,
  `#68655d`.
- Time axis: x0 = 636 (00:00) to x24 = 1575 (24:00), **39.1 px per hour**. Labels every 3 h `00:00 03:00
  … 24:00` at y 236, JetBrains Mono 10 px `#69665e`, left-aligned on their gridline (24:00 right-aligned).
- Vertical gridlines from y 245 to 470: every 3 h 1 px `#272218`, every hour 1 px `#1e1a0f`.
- Rows: label column x 488–600 — flag 20×13 at x 488 + name `SYDNEY` / `TOKYO` / `LONDON` / `NEW YORK`
  at x 516, JetBrains Mono 12 px, letter-spacing 2.5 px, `#A4A199`. Row centres y 274, 330, 385, 441
  (pitch 55.6 px).
- Row track: full axis width, y ±10 around the row centre (h 20), bg `#181408`–`#1a150a`.
- Session bars: same height as the track, radius 2, fill = dimmed session colour: Tokyo `#884B43`,
  London `#22707C`, New York `#86710F`, Sydney (purple, not drawn today). Label inside at the left
  (x+7): `19:00-04:00` JetBrains Mono 9 px in a darker shade of the bar colour (`#4a201c` on Tokyo,
  `#0f3a40` on London, `#3e3300` on NY). The label is the full session span even when the bar is
  clipped at 00:00 (Tokyo shows only 00:00–04:00 but reads `19:00-04:00`).
- Bars drawn = the parts of each session that fall on the **user's current calendar day**, skipping
  weekend sessions. Friday in Bogota: Tokyo 00:00–04:00 (Friday's Tokyo session that started Thursday
  19:00), London 02:00–11:00, New York 07:00–16:00, Sydney none (Friday's Sydney session ended at 00:00;
  Saturday's would be a weekend).
- **Now line**: vertical 2 px `#F0D356` with glow (≈6 px, 30 %), x = 636 + 16.31 × 39.1 = 1274, from
  y 245 to 470. Moves with time (≈0.65 px per minute).
- No hover effect seen on bars or rows.

### 4.4 Session cards (y 627–802)

Four cards, x 14–398, 409–794, 806–1190, 1201–1586. Card bg `#0c0601`, border 1 px `#413a2d`, radius 2,
top-left and bottom-right brackets; **top accent** 4 px at y 627–631 in the dimmed session colour:
Sydney `#594F66`, Tokyo `#714031`, London `#385C66`, New York `#766006` (would brighten when open).

Contents (Sydney card offsets; the others are identical):
- Flag 20×13 at (32,652); name `SYDNEY` bold display 28 px (squared grotesque, uppercase, slight
  letter-spacing 1 px) white `#FAF8F3` at x 62, centre y 658.
- State pill right-aligned at x 300–380, y 648–669: border 1 px `#3a372e`, bg `#0f0d09`, radius 3,
  `● CLOSED` JetBrains Mono 10 px, letter-spacing 2 px, `#8E8B84`, dot 5 px `#5f5c55`. (Open state not
  seen; expected session colour + `OPEN`.)
- Label `SYDNEY TIME` at (32,697) JetBrains Mono 9 px, letter-spacing 2.5 px, `#615d56`.
- Local time `08:18` JetBrains Mono Bold 40 px white `#FFFEFB` at x 32, centre y 727; seconds `:33`
  JetBrains Mono 20 px `#716E67` baseline-aligned after it.
- Right block, right-aligned at x 380: `OPENS IN` (9 px, letter-spacing 2.5 px, `#635f58`, y 712) and the
  countdown `1d 22h 41m` JetBrains Mono Bold 18 px white (y 734). Format `Nd Nh Nm`, floor of the remaining
  time (flips at the exact minute: 16:19:00 → `40m`); days omitted when 0 (assumed).
- Progress track y 758–762 (x 32–380): 4 px, radius 2, `#231E14` (empty; would show the elapsed part of an
  open session in the session colour).
- Footer y 779: left `07:00-16:00 local` (centre's local session hours), right `15:00-00:00 your time`
  (same session in the user's zone); JetBrains Mono 11 px `#9B9690`. Note `00:00` is used for midnight at
  the end of a span.
- Values in the video (16:18:33 Bogota, Fri 23 Oct 2026):

| Card | Local time | State | Opens in | Local hours | Your time |
|---|---|---|---|---|---|
| SYDNEY | 08:18:33 (Sat, AEDT UTC+11) | CLOSED | 1d 22h 41m | 07:00-16:00 local | 15:00-00:00 your time |
| TOKYO | 06:18:33 (Sat, JST UTC+9) | CLOSED | 2d 2h 41m | 09:00-18:00 local | 19:00-04:00 your time |
| LONDON | 22:18:33 (BST UTC+1) | CLOSED | 2d 10h 41m | 08:00-17:00 local | 03:00-12:00 your time |
| NEW YORK | 17:18:33 (EDT UTC−4) | CLOSED | 2d 14h 41m | 08:00-17:00 local | 07:00-16:00 your time |

- DST detail visible in the data: London's footer says `03:00-12:00 your time` (next session, Monday
  26 Oct, after the UK switches to GMT on 25 Oct) while its timeline bar for today reads `02:00-11:00`
  (BST). The card footer therefore shows the hours of the **next** session; the timeline shows today's.

### 4.5 Disclaimer (y 817)

`FX SESSIONS ARE THE CONVENTIONAL BUSINESS HOURS OF EACH CENTRE, MONDAY TO FRIDAY, DAYLIGHT-SAVING AWARE ·
SHOWN ON YOUR OWN CLOCK` — JetBrains Mono 8 px, letter-spacing 1.5 px, `#65655c`, left at x 14.

## 5. Interactions

| Element | Action | Result |
|---|---|---|
| Whole page | timer 1 s | Digital clock, card clocks and second hand update; minute rollover updates countdowns and the now line/dot. |
| Clock sweep | continuous | Wedge rotates clockwise, ≈7 s per turn (real). |
| Block cursors | blink | Strip and clock-title cursors blink together (~1 s period real is a sensible value; aliased in the video). |
| Mouse over timeline / clock | hover | Nothing (no tooltip, no highlight) in the video. |
| Session open/close | time | Badge `n/4 SESSIONS LIVE`, card pill, card accent, arcs, tape clocks on Overview and the status line change (not seen). |

## 6. Data

| Field | Sample | Format | Update | Notes |
|---|---|---|---|---|
| User zone | America/Bogota, UTC-5 | IANA id, `UTC±H` | on start / OS change | from the system |
| User time | 16:18:33, FRIDAY, 23 OCTOBER 2026 | `HH:MM` + `:SS`, `WEEKDAY, D MONTH YYYY` | 1 s | system clock |
| Centre local time | Sydney 08:18:33 | `HH:MM:SS` | 1 s | tz database (Australia/Sydney, Asia/Tokyo, Europe/London, America/New_York) |
| Session hours (local) | Sydney 07–16, Tokyo 09–18, London 08–17, New York 08–17 | `HH:MM-HH:MM local` | static | conventional business hours, Mon–Fri |
| Session hours (your time) | 15:00-00:00 … | `HH:MM-HH:MM your time` | on DST changes | next session's conversion |
| State | CLOSED | OPEN / CLOSED | 1 s | weekday + hours |
| Opens in | 1d 22h 41m | `Nd Nh Nm` (floor) | 1 min | to the next weekday open |
| Sessions live | 0/4 | `n/4` | 1 s | count of open sessions |
| Status line | FX BETWEEN SESSIONS | text | 1 s | derived |

No data source label on this page; everything is computed locally from the clock and the time-zone
database.

## 7. Key frames

| Frame | State |
|---|---|
| `t000.0.jpg` | Reference state (cursors visible, sweep at 3 o'clock). |
| `t001.0.jpg` | Sweep at 12 o'clock, cursors hidden. |
| `t004.5.jpg` | Minute rollover 16:19:00, countdowns `40m`. |
| `t006.5.jpg` | Last frame, cursors visible. |
| `frames-fine/market-hours/t000.0`–`t001.1` | Sweep rotation every 0.1 s. |

## 8. Open questions

- Open-session look: colours of an open bar/arc/card accent/pill, progress bar fill, the status line text
  (e.g. `LONDON · NEW YORK OVERLAP`?) and the badge — all sessions were closed in the video.
- Overlaps: the user description mentions "their overlaps"; no overlap highlight was visible (London/New
  York overlap 07:00–11:00 shows only as stacked bars). Is there an overlap band on the timeline or the
  clock when sessions overlap live?
- Bright-arc rule on the clock (current time-of-day within the arc vs "next session to open" — both
  match Sydney here).
- Exact blink period of the cursors and the real sweep speed (the video is 6x).
- Whether the timeline shows today only or a rolling 24 h window around now; whether rows/bars have
  tooltips.
- Behaviour at the Sunday open (Sydney opens Sunday 15:00 your time) and countdown format under 1 day
  (`22h 41m`?) and under 1 hour.
