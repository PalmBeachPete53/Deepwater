#pragma once
// Charts tab — the gold "Bonus: quant checks" box under a strategy tester result. Port of quant-terminal
// public/js/quantui.js (mountQuantChecks, render) (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
//
// The checks (charts::quant::quantChecks, about 25 backtests) run on a worker thread when the strategy uses
// built-in indicators only. Custom indicators run on the GUI thread only (charts::custom), so a strategy with one
// runs there, 30 ms after the click like the JS (the page waits for it, as the browser does).
#include "pages/charts/panels/bottom/Tester.h"

class QWidget;

namespace charts::ui {
class ChartsApp;
namespace bottom {

QWidget* mountQuantChecks(const TestResult& result, ChartsApp* app);

// true when strat.indicators uses a custom indicator ("custom:…"), which must run on the GUI thread.
bool usesCustom(const QJsonObject& strat);

}  // namespace bottom
}  // namespace charts::ui
