# Translation brief — English / French interface

User's request: an Options dialog (gear at the right of the header, Cmd+,) where the interface language is set to
English or Français. The dialog, the language switch and the header are done (`src/core/I18n.h`,
`src/app/OptionsDialog.*`, `translations/app_fr.ts`). Your job: make YOUR pages fully French when French is chosen.

SCRATCH = /private/tmp/claude-501/-Users-raphael-dev-deepwater/dc71afae-325d-4318-bf2b-f513b5ef5c63/scratchpad

## 0. Fresh copy first
Delete your work copy, re-copy `/Users/raphael/dev/deepwater` (without `build/` and `Reference/`), configure, build.
Same ownership and rules as before. Read `src/core/I18n.h` (how strings are translated), `docs/ARCHITECTURE.md`.

## 1. Mark every user-visible string
- QObject subclasses: `tr("Text")`. Anything else (painted text, helpers, static tables):
  `QCoreApplication::translate("<package>", "Text")`; in static tables `QT_TRANSLATE_NOOP("<package>", "Text")` and
  translate where it is shown. Use your package name as the context (see the table) — never a context another
  package uses. lupdate only sees literal strings in these calls: no wrappers of your own around them (or tell
  lupdate with `-tr-function-alias`), no string building before translation. Use `%1` arguments:
  `tr("%1 of %2 trades").arg(n).arg(total)`; plurals with `translate("<package>", "%n trade(s)", nullptr, n)` — English resolves "(s)" from the count
  (i18n::EnglishPlurals), French uses the .ts numerus forms.
- Everything a user reads: titles, labels, column headers, pills, buttons, tooltips, ⓘ texts, footnotes, method
  notes, legends, axis captions, empty / loading / error / "not connected" states, dialogs, menus, badges.
- Rule-based AUTOMATED READ texts: write them in the interface language (those that already have EN/FR rules: the
  default language becomes the interface language; keep an in-page EN/FR toggle only where it exists today).
- Content translation toggles (News Feed and Squawk headlines EN/FR): default to the interface language.
- Do NOT translate: data coming from sources (headlines, event names from the calendar feeds, bank note titles,
  instrument names from Yahoo), tickers and symbols, currency codes, source and publisher names, units (%, bp,
  pips, lots). Usual trading words stay as French traders say them (long, short, risk-on / risk-off, drawdown,
  spread, breakout, carry, pips, swing…). Country and region names in your own static tables: translate.
- Dates: day and month names via `i18n::locale()` (never a hard-coded `QLocale(QLocale::English)` for display).
  Numbers keep the app's English format (`fmt::`), in both languages.
- Never build translated strings once at startup (static caches of labels): the pages are re-created on a language
  change, but statics survive. Never save translated text in settings.

## 2. French translations
Create `translations/<package>_fr.ts` (your package only):
`~/Qt/6.12.0/macos/bin/lupdate <your source folders/files> -ts translations/<package>_fr.ts -no-obsolete -target-language fr_FR`
then fill every `<translation>` (edit the XML with a script; remove `type="unfinished"`). Natural, concise French as
a French market terminal would say it (match the length of the English where you can; uppercase labels stay
uppercase; keep the same `%1`). `~/Qt/6.12.0/macos/bin/lrelease translations/<package>_fr.ts -qm /dev/null` must report
0 unfinished. The build embeds every `translations/*_fr.ts` automatically (reconfigure after adding the file).

| Package | Context | Source folders |
|---|---|---|
| global | `global` (or class names) | src/global, src/pages/global/overview, src/pages/global/market-hours |
| wire | `wire` | src/wire, src/pages/news/squawk, src/pages/global/session-recap |
| news | `news` | src/news, src/pages/news/calendar, src/pages/news/feed |
| banks | `banks` | src/banks, src/pages/news/bank-research, src/pages/positioning/bank-positioning |
| economy | `economy` | src/macro, src/pages/economy |
| positioning | `positioning` | src/positioning, src/pages/positioning/*.{h,cpp} (not bank-positioning/) |
| watch | `watch` | src/pages/market/watch |
| seasonal | `seasonal` | src/seasonality, src/fundamental, src/pages/market/seasonality, src/pages/market/fundamental |
| technical | `technical` | src/analytics, src/pages/market/technical, src/pages/market/liquidity |
| account | `account` | src/account, src/pages/account |

## 3. Verify
- Grep your files for remaining string literals that a user can see and are not translated; list any you leave
  on purpose (data, symbols…) in the report.
- Screenshots in French: `DW_LANG=fr tools/shot.sh <page> out.png 9000 1600x1042 <build dir>` and
  `DW_LANG=fr tools/sizes.sh <page> <dir> 9000 <build dir> 1024x600 1280x680 1920x1000`, plus the open states
  (modals, panels, tooltips, menus) with your DW_* hooks. Look at every one: no English left, no clipped or
  overlapping French text (French is ~20–30 % longer: elide with a tooltip, wrap, or adjust widths). Then check
  English still looks exactly as before at 1600x1042.
- Live switch: launch in English, switch to French with the Options dialog (or `--lang fr` on a second run) and back;
  the page must follow.

## 4. Report (≤ 25 lines)
Strings translated (count from lrelease), files changed, deliberate English left, layout fixes for French, shared-file
changes needed (exact), screenshot folder.
