#pragma once
// Charts tab — chart pane helpers: the formatters chart.js imports from public/js/api.js (thin aliases of
// charts::ui::fmt, src/pages/charts/Format.h) and uid generation (chart.js newUid). Port of quant-terminal (MIT,
// Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). Private to the chart pane files (namespace pane).
#include "pages/charts/Format.h"
#include <QString>

namespace charts::ui::pane {

inline QString fmtPrice(double v) { return fmt::price(v); }
inline QString fmtNum(double v, int d = 2) { return fmt::num(v, d); }
inline QString fmtPct(double v) { return fmt::pct(v); }
inline QString ticker(const QString& id) { return fmt::ticker(id); }
inline QString exchange(const QString& id) { return fmt::exchange(id); }
QString newUid();                      // `i${Date.now().toString(36)}${(uidSeq++).toString(36)}`

}  // namespace charts::ui::pane
