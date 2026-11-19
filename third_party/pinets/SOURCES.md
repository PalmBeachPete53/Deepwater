# PineTS — origin

Pine Script® runtime of the Charts tab (src/charts/core/Pine.cpp): transpiles a Pine Script v5/v6 source to
JavaScript and runs it bar by bar, as TradingView does. It runs inside QuickJS-ng (third_party/quickjs-ng), embedded
in the app as the resource `:/pinets/pinets.js` (CMakeLists.txt and tests/charts/CMakeLists.txt).

- Project: PineTS by LuxAlgo / QuantForge, https://github.com/QuantForgeOrg/PineTS (npm package `pinets`)
- Licence: AGPL-3.0-only, see LICENSE in this folder. Deepwater is distributed under the same licence (LICENSE at
  the root) because it ships this file.
- Release: `0.13.1` (published on npm 2026-10-27)
- Downloaded: 2026-11-18 from https://registry.npmjs.org/pinets/-/pinets-0.13.1.tgz
  (npm integrity `sha512-iBIWiuapykqPHHg0EFByRFpuwHX17etDoFN8HNsG7Idqd5fwjawkl7Fxb3Y2dAJSiXniluJHrD+NFLrAdDj2Ow==`)

Kept: `package/dist/pinets.min.browser.js`, renamed `pinets.js`, unchanged byte for byte
(SHA-256 `dbc3e05a33a127e784737e90991cbb0bad85204b7e2cae167dd0f1e0678e7379`), and `package/LICENSE`. It is the
self-contained build (its dependencies acorn, acorn-walk and astring, MIT, are bundled inside) that defines the global
`PineTSLib`. Left out: the source maps, the Node/ES builds, the README.

What QuickJS lacks and Deepwater supplies (Pine.cpp): `Intl.DateTimeFormat` (the only Intl use: a time zone's wall
clock and whether summer time applies), backed by QTimeZone; `setTimeout`; `console`. Not supplied on purpose:
`fetch`, `WebSocket`, files — a script reaches no network and no disk; its bars come from Deepwater.

Update: download the new tarball into an empty scratch folder, check its integrity, copy the same two files, update
this file, run the Charts core tests (pine_tests.cpp) and the Pine-vs-engine comparison.

## Updating PineTS

Before keeping a new version, run both engine checks on it:

- `./build-tests-flow/charts_core_tests "pine reference"` — Charts' C++ indicators must still equal their Pine definitions (runs everywhere).
- `./build-tests-flow/charts_talib_tests` — the PineTS engine against TA-Lib (39 shared functions and engine constructions on real BTCUSDT bars). Built only where TA-Lib is installed (`brew install ta-lib`); the known differences (variance/correlation precision, CMO and SAR definitions) are written in `tests/charts/talib/pine_talib_tests.cpp`.
