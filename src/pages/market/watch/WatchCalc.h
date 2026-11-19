#pragma once
#include "market/Candles.h"
#include <QVector>
#include <QString>
#include <QHash>
#include <cmath>

// Display data and small computations of the Markets Watch instrument sheet. Seasonality comes from the
// seasonality engine (src/seasonality) and the technical tiles from src/analytics; the structs below only carry
// what the cards draw.
namespace watch {

// What the two seasonality cards draw (filled from seasonality::Dataset by the sheet).
struct Seasonality {
  int firstYear = 0, lastYear = 0;          // years covered by the month statistics
  int pathFirstYear = 0, pathLastYear = 0;  // complete years averaged in the year path
  int currentYear = 0;
  double month[12];                         // average monthly return, % (NaN if no data)
  int monthN[12];                           // number of years per month
  QVector<double> avgPath;                  // 366 calendar slots (leap-year template): smoothed average path, %
  QVector<double> curPath;                  // current year cumulative % (NaN after the last close)
  bool ok = false;
};

// One technical status tile (filled from analytics::TechRead by the sheet).
struct Tile {
  QString key;     // TREND MOMENTUM VOLATILITY VOLUME STRUCTURE LIQUIDITY
  QString value;   // Bullish, Neutral, Normal, Below avg, Pool above…
  int bias = 0;    // +1 bullish (cyan), -1 bearish (red), 0 neutral (grey)
  QString tip;     // how it was computed, with the numbers
};

// Rebased performance series (percent since the first common day).
struct PerfLine { QString name; QVector<qint64> t; QVector<double> pct; };
// Currency index: mean log change of `ccy` against the other majors (pairs: app symbol → daily bars).
PerfLine currencyIndex(const QString& ccy, const QHash<QString, QVector<market::Bar>>& pairs, qint64 from);
// Pairs needed for a currency index of ccy (app symbols present in the catalogue).
QStringList indexPairs(const QString& ccy);
// Instrument rebased since `from`.
PerfLine rebased(const QString& name, const QVector<market::Bar>& daily, qint64 from);
// Keep only the dates both lines share (so the two start at 0 on the same day), rebased again.
void alignPair(PerfLine& a, PerfLine& b);

// 4-hour bars from 1-hour bars (UTC buckets 00/04/08/12/16/20).
QVector<market::Bar> aggregate(const QVector<market::Bar>& in, qint64 bucketSec);

// Simple moving averages and Bollinger for chart overlays.
QVector<double> sma(const QVector<market::Bar>& b, int n);
QVector<double> ema(const QVector<market::Bar>& b, int n);
}
