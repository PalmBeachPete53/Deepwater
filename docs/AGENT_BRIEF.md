# Builder brief — Deepwater pages

You build part of Deepwater, a Qt 6 / C++20 desktop trading terminal, from specs written frame by frame from the
reference videos. The base (window, header, theme, kit, HTTP, quotes, candles) exists. Read first:

1. `docs/ARCHITECTURE.md` — layout, rules, traps (mandatory).
2. The specs of your pages: `docs/reference/<Tab>/<sub>.md` and the `_shared-notes*.md` files of every tab
   (common visual language, measured colours).
3. The base code you will use: `src/ui/Theme.h`, `src/ui/Kit.h`, `src/ui/Chart.h`, `src/ui/Format.h`,
   `src/net/Http.h`, `src/net/Sources.h`, `src/market/*.h`, `src/app/Pages.h`.

Reference frames (1600x966, one per 0.5 s) are in
`/private/tmp/claude-501/-Users-raphael-dev-deepwater/dc71afae-325d-4318-bf2b-f513b5ef5c63/scratchpad/frames/<Tab>/<sub>/`.
Open the key frames listed in your specs and compare them with your screenshots side by side, again and again.
Colour picker: `<scratchpad>/tools/pick frame.jpg x y …`.

## Workspace (important: other agents work at the same time)
- Work in your own copy: `cp -R /Users/raphael/dev/deepwater <scratchpad>/work/<your package>` (exclude `build/`,
  `Reference/`), build there with its own build dir:
  `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos && cmake --build build`.
- You OWN only the directories listed in your package. Create files only there. Do not edit shared files
  (`src/app`, `src/core`, `src/net`, `src/ui`, `src/market`, `CMakeLists.txt`, other packages' dirs). If you need a
  change in a shared file, make it in your copy so you can work, and list it precisely (file, diff, why) in your
  final report; it will be applied by hand. Prefer adding a helper inside your own dirs.
- Assets (logos, images) go in `resources/<your domain>/` with a `SOURCES.txt` beside them (origin + licence). Only
  public-domain / CC0 / open-licence assets.
- Never touch `/Users/raphael/dev/deepwater` itself; the merge is done by the lead.

## Testing
- `DW_PROFILE=<scratchpad>/work/<pkg>-profile DW_CACHE_DIR=<scratchpad>/http-cache tools/shot.sh <page> out.png 9000 1600x1042 <your build dir>`
  → the page area is exactly 1600x966 like the reference (screenshot is 2x: 3200x2084). Also check 1280x800 and
  2200x1300 (no overlap, no clipped text, sensible stretch). Never run the app without `DW_PROFILE`.
- Look at every screenshot you take, compare with the reference frames, fix, repeat until it matches closely:
  layout proportions, fonts, colours, spacing, every label, every interaction state the video shows (hover,
  tooltips, selection). For hover/click states add a temporary env hook (e.g. `DW_HOVER=x,y`) and remove it before
  you finish, or keep it only if it is a documented `DW_*` page hook useful for later tests.
- CPU: a visible idle page must stay low (repaint only what changes; animations ≤ 30 fps; no timers when hidden).
  Measure with `ps -o %cpu` on a 20 s run and report it.

## Data
- Real data only (see ARCHITECTURE rules). Use the sources suggested in your package; you may find better free ones.
  Test every URL with curl first (user agent `Deepwater/1.0 (desktop market terminal)`), read the terms, declare it
  in a `sources::Registrar` with pace and terms. Respect rate limits; a 403/challenge means stop, not bypass.
- What the reference shows but no free source provides: keep the component, fill it with the closest honest real
  data and label it, or show "—" with a tooltip explaining. Never fake or randomise.
- Keys the user already saved (read with `settings::apiKey(name)`): `e-stat`, `dbnomics`. Ask nothing; if a key would
  unlock something, say so in the report.
- The reference's "Alfred"/AI text blocks become rule-based "AUTOMATED READ" texts computed from the data (no LLM).

## Final report (your last message, ≤ 60 lines)
Files created; shared-file changes needed (exact); sources used (host, terms, pace); what matches the reference and
what doesn't (and why); screenshots paths of each page at 1600x1042; CPU numbers; env hooks kept; open points.
