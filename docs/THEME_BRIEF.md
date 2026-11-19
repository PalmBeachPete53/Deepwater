# Theme brief — the ten themes of preview.html

User's request: implement the themes of `preview.html` (project root) in Deepwater. The Options dialog now lists
Deepwater (default) + Ember, Abyss, Jade, Oxide, Nova, Bloomberg (dark) and Glacier, Dune, Sakura (light); the
theme applies live (pages are re-created) and is saved. The header, the Options dialog, the ui:: kit, the style sheet
(tooltips, scrollbars, menus, line edits), `ui::DotGlobe` (Overview and Macro globes) and the logo already follow the
theme. Your job: make YOUR pages follow it.

SCRATCH = /private/tmp/claude-501/-Users-raphael-dev-deepwater/dc71afae-325d-4318-bf2b-f513b5ef5c63/scratchpad

## 0. Fresh copy first
Delete your work copy, re-copy `/Users/raphael/dev/deepwater` (without `build/` and `Reference/`), configure, build.
Same ownership and rules as before. Read `src/ui/Theme.h` (top to bottom), `preview.html`.

## 1. How colours work now
- Your pages keep their measured **Deepwater** colours in code; pass every colour you paint through
  `theme::map(...)` at paint time: `theme::map("#433d2c")`, `theme::map(QColor(…))`, `theme::map(r, g, b, a)`.
  For Deepwater `map()` is the identity, so the default look must stay **pixel-identical** to today.
  `map()` projects Deepwater roles onto the theme: neutrals by lightness (background → panel → hover → grid → lines
  → faint/section/dim text → text), ambers → the theme's accent, cyan/greens → up, coral/reds → down, blues and
  purples (chart series) keep their hue with a lightness fitted to light themes. Alpha is kept.
- `theme::pal()` tokens are already mapped. `theme::roles()` gives the preview's roles directly when a role is the
  right meaning (e.g. `roles().halo`, `roles().tooltip`, `roles().clusterFill`, category colours `energy, precious,
  industrial, agri, softs, hub`). `theme::onAccent()` = text/icon drawn on a filled accent area (was near-black on
  yellow). `theme::isDefault()`, `theme::isDark()`.
- Gradients: map each stop. Pens, brushes, fills, text, QSS strings, QPalette, chart series, tooltips, badges,
  selection washes, glows — everything.
- **No mapped colour may outlive a theme change**: never store `theme::map(...)` results in `static` variables or in
  members of objects that survive page re-creation (services, singletons). Static *Deepwater* constants are fine if
  mapped where used. Cached pixmaps/images (backgrounds, layers, glyph caches): include `theme::revision()` in the
  cache key or rebuild them in the page's constructor. A raster drawn in Deepwater colours can be converted once with
  `theme::mapImage(img)` (keep it out of per-frame paths).
- Do NOT recolour: flags, bank/coin logos and other real images; currency colours already come from
  `theme::currencyColor()`.
- Globes (banks package: `GlobeView`): keep the Deepwater rendering exactly when `theme::isDefault()`; otherwise use
  the roles like `ui::DotGlobe` does (ocean inner→outer for the sphere, land border for unlit dots, accent for lit
  dots, graticule, halo).

## 2. Judgement, not just mechanics
The mapping is automatic but some places need care, especially in the **light** themes:
- Text on filled accents/badges/pills: use `theme::onAccent()` (or check contrast).
- Large "lifted" dark islands that should be light in light themes, glows that look dirty on light backgrounds
  (reduce or drop glow alpha when `!theme::isDark()`), heat-map scales that must stay readable, selected-row washes,
  chart grids, crosshair pills. Up/down must stay clearly distinguishable in every theme.
- Bloomberg: text is amber, accent is white, section text light grey — check titles, KPI values and chips read well.

## 3. Verify
`DW_THEME=<id> tools/shot.sh <page> out.png 9000 1600x1042 <build dir>` (also `DW_THEME=… tools/sizes.sh`). For each of
your pages: **glacier**, **sakura** (light), **bloomberg**, **abyss**, **nova** at 1600x1042 plus open states (modals,
panels, tooltips, menus, hover); and **deepwater** to confirm nothing changed. Look at every screenshot: no
unreadable text, no leftover Deepwater-amber or black blocks in other themes, no invisible lines. Run your pages in
French once too (`DW_LANG=fr`). CPU unchanged (map() is cached; don't call mapImage per frame).

## 4. Report (≤ 25 lines)
What you mapped (counts or files), special cases handled per theme, anything left in Deepwater colours on purpose
(images…), shared-file changes needed (exact), screenshot folder.
