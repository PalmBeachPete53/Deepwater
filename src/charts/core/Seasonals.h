#pragma once
// Port of quant-terminal public/lib/seasonals.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Seasonals: how a symbol moved through each calendar year, from daily bars (UTC dates).
// Each year's path is measured from the previous year's last close, so 1 January's move counts.
#include "charts/core/Types.h"
#include <QJsonObject>
#include <array>

namespace charts::seasonals {

struct Point { int doy = 0; double pct = NaN; };          // day of year (1 = 1 January), % from the base

struct Year {
  int year = 0;
  QVector<Point> points;
  double total = NaN;      // last point's pct
  bool complete = false;   // the whole year is held: chained to the year before, ends after day 350, not this year
};

struct MonthRow {
  int year = 0;
  std::array<double, 12> months;   // % move of each month, NaN = no bar that month (JS null)
  double total = NaN;
};

struct MonthAvg {
  bool valid = false;      // false = JS null (no year has that month)
  double avg = NaN, upPct = NaN;
  int n = 0;
};

struct Result {
  QVector<Year> years;
  QVector<MonthRow> monthly;
  QVector<Point> average;            // 366 days (doy 1..366) when completeYears > 0, else empty
  std::array<MonthAvg, 12> monthAvg;
  int completeYears = 0;
  QJsonObject toJson() const;        // the JS object: { years, monthly, average, monthAvg, completeYears }
};

// thisYear: the current UTC year (JS new Date().getUTCFullYear()); 0 = read the clock. A year equal to it is never
// complete. Tests pass a fixed year.
Result seasonals(const Bars& bars, int thisYear = 0);

}  // namespace charts::seasonals
