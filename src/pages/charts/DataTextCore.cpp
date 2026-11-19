// Charts tab — English templates of runtime texts built in src/charts/core (indicators, backtest, quant checks,
// custom indicators), translated at display time by fmt::tx (DataText.cpp). Same rules as DataText.cpp's list.
#include <QCoreApplication>
#include <QVector>

namespace charts::ui::fmt {

namespace {
// Pieces of runtime texts that are only ever shown inside a template's part (no template of their own): marked here
// so lupdate collects them, translated as whole parts by fmt::tx. IndicatorsValidate.cpp builds them as
// `input "key": <piece>` / `output 2: <piece>`.
[[maybe_unused]] const char* const kParts[] = {
    QT_TRANSLATE_NOOP("charts", "must be an object like { key, type, default }"),
    QT_TRANSLATE_NOOP("charts", "`key` must be letters, digits or _ and start with a letter"),
    QT_TRANSLATE_NOOP("charts", "the key is used twice"),
    QT_TRANSLATE_NOOP("charts", "an int input needs a whole-number default"),
    QT_TRANSLATE_NOOP("charts", "a float input needs a number default"),
    QT_TRANSLATE_NOOP("charts", "a time input's default is a bar time in seconds, or 0"),
    QT_TRANSLATE_NOOP("charts", "a select input needs `options`, a list of text choices"),
    QT_TRANSLATE_NOOP("charts", "its default must be one of its options"),
    QT_TRANSLATE_NOOP("charts", "a source input's key must be \"source\""),
    QT_TRANSLATE_NOOP("charts", "must be an object like { key, kind, color }"),
    QT_TRANSLATE_NOOP("charts", "`kind` must be \"line\" or \"histogram\""),
    QT_TRANSLATE_NOOP("charts", "`color` must be a colour like #2962ff"),
    QT_TRANSLATE_NOOP("charts", "`lineWidth` must be 1, 2, 3 or 4"),
};
}  // namespace

QVector<const char*> coreTemplates() {
  return {
      // Series.cpp (parseTf)
      QT_TRANSLATE_NOOP("charts", "Bad timeframe \"%1\". Use forms like 1s, 5m, 4h, 1D, 1W, 1M."),
      QT_TRANSLATE_NOOP("charts", "Bad timeframe \"%1\""),
      // Backtest.cpp: strategy errors, fills and costs (Stats::fills, Stats::feesAndSlippage)
      QT_TRANSLATE_NOOP("charts", "Unknown operand \"%1\". Define it under indicators or use a price field or number."),
      QT_TRANSLATE_NOOP("charts", "Indicator \"%1\" (%2) has no output \"%3\""),
      QT_TRANSLATE_NOOP("charts", "Unknown operator \"%1\""),
      QT_TRANSLATE_NOOP("charts", "Bar magnifier: lower-timeframe bars inside %1 of %2 bars%3"),
      QT_TRANSLATE_NOOP("charts", "%1% fee + %2% slippage per side%3"),
      QT_TRANSLATE_NOOP("charts", " + %1% spread"),
      // Quant.cpp: the gates' details
      QT_TRANSLATE_NOOP("charts", "%1 Sharpe on data the settings were never picked on (needs above 0)"),
      QT_TRANSLATE_NOOP("charts", "%1% chance the best-looking settings are below average on unseen data (needs under 50%)"),
      QT_TRANSLATE_NOOP("charts", "%1 probability the edge is real after %2 version%3 tried (needs above 0.95)"),
      QT_TRANSLATE_NOOP("charts", "%1% with double fees and %2% slippage a side%3 (needs above 0)"),
      QT_TRANSLATE_NOOP("charts", "%1% of nearby settings keep at least half the Sharpe (needs 60%+)"),
      // CustomJs.cpp: limits of a custom indicator's run
      QT_TRANSLATE_NOOP("charts", "Custom indicator %1 took too long (stopped after %2 s)"),
      QT_TRANSLATE_NOOP("charts", "%1 is too long for Deepwater: at most %2 (%3 per bar, one value per bar is expected)"),
      QT_TRANSLATE_NOOP("charts", "an object with %1 keys"),
      QT_TRANSLATE_NOOP("charts", "a list of %1 values"),
      // IndicatorsValidate.cpp: the format check of a custom indicator (formatProblems joins them with "\n- ")
      QT_TRANSLATE_NOOP("charts", "This indicator doesn't follow the format (docs/custom-indicators.md):\n- %1"),
      QT_TRANSLATE_NOOP("charts", "calc() stopped with an error on test bars: %1"),
      QT_TRANSLATE_NOOP("charts", "output \"%1\": calc() returned no list for it. Return { %1: [...] }"),
      QT_TRANSLATE_NOOP("charts", "output \"%1\": calc() returned %2 values, but there are %3 bars. Return one value per bar (NaN where there isn't one)"),
      QT_TRANSLATE_NOOP("charts", "output \"%1\": every value must be a number (or NaN/null)"),
      QT_TRANSLATE_NOOP("charts", "barColors: return one colour per bar (%1), like \"#26a69a\", or null to keep a bar's normal colour"),
      QT_TRANSLATE_NOOP("charts", "markers: each needs a time in seconds, a position (%1) and a shape (%2)"),
      QT_TRANSLATE_NOOP("charts", "input %1: %2"),
      QT_TRANSLATE_NOOP("charts", "output %1: %2"),
      QT_TRANSLATE_NOOP("charts", "`type` must be one of %1"),
      QT_TRANSLATE_NOOP("charts", "a source input's default must be one of %1"),
      QT_TRANSLATE_NOOP("charts", "`%1` must be a number"),
      // a list of problems, one per "\n- " (after the header above): first one, then the rest in turn
      QT_TRANSLATE_NOOP("charts", "%1\n- %2"),
  };
}

}  // namespace charts::ui::fmt
