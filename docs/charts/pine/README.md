# Pine definitions of Charts' indicators

The user's rule (2026-11-18): **Pine is the definition of an indicator, Charts' C++ engine executes it.**

Each `<id>.pine` here defines the Charts indicator `<id>` (src/charts/core/IndicatorsDefs.cpp) in Pine Script v5, as
TradingView would compute it. Its inputs are titled with the indicator's Charts keys (`length`, `source`, `mult`…) and
its plots with the indicator's output keys (`value`, `upper`, `basis`…), so the two can be compared one to one.

`tests/charts/core/pine_reference_tests.cpp` runs, on every build of the Charts tests, each definition in PineTS
(src/charts/core/Pine) and the C++ engine on real BTCUSDT bars from Binance (fixtures `binance-btcusdt-*.csv`: the
daily history since 2017-08-17, and 5000 bars each of 1h, 5m and 1m), with the defaults and other settings. The
same bars must have no value, and every value must agree within 1e-9 relative. A definition file without a test case
fails the test.

Covered (2026-11-18): sma, ema, bb, bbpb, bbw, rsi (with its MA), macd, atr, dmi, stoch, obv, vwap (day, week,
month anchors). Largest differences seen: 1.3e-10 (MACD), most below 1e-12.

What changed in the engine for it: `stdev` is now Pine's two-pass formula (mean, then the squared deviations from it).
quant-terminal's one-pass E[x²] − E[x]² lost digits (Bollinger %B off by up to 4e-7 on quiet 1-minute bars, a stdev
of 4.9e-4 instead of 0 over one bar).

To add an indicator: write `<id>.pine` the same way, add its cases to `cases()` in the test, run
`./build-tests/charts_core_tests "pine reference"`.
