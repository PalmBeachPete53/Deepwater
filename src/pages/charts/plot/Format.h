#pragma once
// Engine-internal formatting helpers (labels on the axes) and small numeric utilities.
#include "pages/charts/plot/Plot.h"

namespace charts::plot::fmt {

QString fixed(double v, int precision, double minMove = 0);   // "1234.50", "−0.25" (U+2212 like the JS library)
QString volume(double v, int precision);                       // "33.27K", "1.2M", "950"
QString percent(double v);                                     // "12.34%"
QString monthShort(int month1to12);                            // "Jan"
// dateFormat tokens: yyyy, yy, MMMM, MMM, MM, M, dd, d; everything else literal (as Lightweight Charts).
QString date(Time utcSeconds, const QString& dateFormat);
QString clock(Time utcSeconds, bool seconds);                  // "14:05" / "14:05:09"

// Time weights of a point relative to the previous one (bigger = coarser boundary crossed).
enum Weight : int {
  WNone = 0, WSecond = 10, WMinute1 = 20, WMinute5 = 21, WMinute30 = 22,
  WHour1 = 30, WHour3 = 31, WHour6 = 32, WHour12 = 33, WDay = 50, WMonth = 60, WYear = 70
};
int timeWeight(Time t, Time prev);

// Contrast text colour for a label background (black on light, white on dark).
QColor textOn(const QColor& bg);

inline int makeEven(int v) { return v + (v & 1); }

}  // namespace charts::plot::fmt
