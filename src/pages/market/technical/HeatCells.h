#pragma once
#include "analytics/Technical.h"
#include <QColor>
#include <QString>

// What one heatmap cell shows: state word, detail line, fill (hue = meaning, intensity = magnitude), text colour,
// plus the sentence-case value and colour used in the signal file. Colours measured on the reference frames.
namespace heat {
enum Column { Trend, Momentum, Volatility, Volume, Structure, Liquidity, ColumnCount };
struct Cell {
  QString word, detail;      // detail empty → single centred word
  QColor fill;               // invalid → no fill (cell background only)
  QColor text;
  QString fileValue;         // "Bullish", "Recently swept"
  QColor fileColor;
  QString fileDetail;        // right column of the signal file
  QString tip;               // tooltip: the numbers behind the read
};
Cell cell(const analytics::TechRead& r, Column c);
QString columnTitle(Column c);
double sortKey(const analytics::TechRead& r, Column c);
}
