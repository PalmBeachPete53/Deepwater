#pragma once
// Port of quant-terminal public/lib/series.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Timeframes, resampling and chart-type transforms. Shared by the chart pane, the data services and the HTTP API.
//
// Conventions of the port:
//  - Errors are thrown as std::runtime_error with the JS message (parseTf, isoString).
//  - A Bar whose volume is NaN has NO volume (the JS bricks of renko / lineBreak / kagi / pointFigure carry no
//    `volume` field): toCsv writes "" for it like JS `b.volume ?? ""`; resample treats it as 0 like `b.volume || 0`.
//  - The extra per-bar fields of the JS transforms (box, range, reversal, color, boxes, dir) live in Transformed.
#include "charts/core/Types.h"
#include <QStringList>
#include <QPair>
#include <stdexcept>

namespace charts::series {

// "15m" -> { n: 15, unit: 'm', seconds: 900 }. Accepts 1s..59s, minutes, hours, D, W, M.
// n and seconds are doubles as in JS (a 20-digit count stays a number). Throws for a bad timeframe.
struct Tf {
  double n = 1;
  QString unit;            // "s" | "m" | "h" | "D" | "W" | "M"
  double seconds = 0;      // n x seconds of the unit (M = 30 days, as the JS UNIT_SEC)
};
Tf parseTf(const QString& tf);
bool isIntraday(const QString& tf);

// Start of the bucket a bar at time t (UTC seconds) belongs to.
qint64 bucketStart(qint64 t, const QString& tf);
qint64 bucketStart(qint64 t, const Tf& tf);       // same, timeframe already parsed

// Merge bars into a larger timeframe. Bars must be sorted ascending.
Bars resample(const Bars& bars, const QString& tf);

// Pick the largest native timeframe that divides the requested one. Empty string = none (JS null).
QString nativeBase(const QString& tf, const QStringList& natives);

Bars heikinAshi(const Bars& bars);

// What the price-only transforms return: the bars plus the extra fields the JS puts on every bar.
struct Transformed {
  Bars bars;               // volume NaN (= absent) except for rangeBars
  double box = NaN;        // renko, pointFigure: b.box (same on every bar)
  double range = NaN;      // rangeBars: b.range
  double reversal = NaN;   // kagi: b.reversal
  QStringList colors;      // kagi: b.color per point, "#089981" yang / "#f23645" yin
  QVector<int> boxes;      // pointFigure: b.boxes per column
  QVector<int> dirs;       // pointFigure: b.dir per column, 1 = X (up), -1 = O (down)
};

// Renko from closes. boxSize 0 = ATR(14) of the last bar. Bricks get unique, increasing times.
Transformed renko(const Bars& bars, double boxSize = 0);
// Three-line break. Bricks have no volume (NaN).
Bars lineBreak(const Bars& bars, int lines = 3);

// One CSV column after the OHLCV ones (JS extraCols object, in insertion order).
using CsvColumns = QVector<QPair<QString, Series>>;
QString toCsv(const Bars& bars, const CsvColumns& extraCols = {});

// Average true range of the last 14 bars, the default box/range size for the price-only charts.
double recentAtr(const Bars& bars, int n = 14);

// Range bars: a new bar starts each time price has covered `range` (default: ATR 14). Each source
// bar is walked open -> low -> high -> close (or open -> high -> low -> close on down bars).
Transformed rangeBars(const Bars& bars, double range = 0);
// Kagi: turning points of the close, reversing after `reversal` (default ATR 14). Yang (green)
// once the line breaks the previous high, yin (red) once it breaks the previous low.
Transformed kagi(const Bars& bars, double reversal = 0);
// Point & figure from closes: X columns up, O columns down, a box of `box` (default ATR 14) and a
// `reversal`-box turn. Each column comes back as one candle spanning the column.
Transformed pointFigure(const Bars& bars, double box = 0, double reversal = 3);

// ---- JS formatting helpers (byte-identical to the JS), used by the CSV exports and the strategy texts
QString numStr(double v);                 // String(number): 0.1, 1e+21, 1e-7, NaN, Infinity, -0 -> "0"
QString toFixed(double v, int digits);    // Number.prototype.toFixed (exact decimal value, ties away from zero)
QString isoString(qint64 t);              // new Date(t * 1000).toISOString(); throws "Invalid time value"
// UTC calendar of a Unix time in seconds (proleptic Gregorian, as JS Date): month 0..11, day 1..31.
void utcDate(qint64 t, int& year, int& month, int& day);
// Date.UTC(year, month0, day) / 1000 for whole days (month0 may be outside 0..11, as in JS).
qint64 utcSeconds(qint64 year, qint64 month0, qint64 day);

}  // namespace charts::series
