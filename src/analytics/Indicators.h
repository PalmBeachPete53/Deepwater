#pragma once
#include "market/Candles.h"
#include <QVector>
#include <cmath>

// Plain indicator maths over market::Bar series (oldest first). Pure functions, no Qt event loop, unit-tested in
// src/analytics/tests. Every output vector has the input's length; values before the warm-up are NAN.
namespace analytics {
using market::Bar;

QVector<double> closes(const QVector<Bar>& b);
QVector<double> sma(const QVector<double>& v, int n);
// EMA seeded with the SMA of the first n values (the usual charting convention).
QVector<double> ema(const QVector<double>& v, int n);
// Wilder RSI: average gain / loss smoothed with alpha = 1/n, seeded with the simple mean of the first n changes.
QVector<double> rsi(const QVector<double>& v, int n = 14);
// Wilder ATR over true range (high-low, |high-prev close|, |low-prev close|).
QVector<double> atr(const QVector<Bar>& b, int n = 14);
// Rate of change in percent over n bars.
QVector<double> roc(const QVector<double>& v, int n);
// Standard deviation of log returns over the last n returns ending at each bar, annualised with periodsPerYear,
// in percent. (n returns need n+1 closes.)
QVector<double> realizedVol(const QVector<double>& c, int n, double periodsPerYear);
// Share (0..100) of the finite values of v[from..to) that are <= x. NAN if none.
double percentileRank(const QVector<double>& v, int from, int to, double x);

// 3-bar (order 1) or 5-bar (order 2) fractal swings. A bar i is a swing high when its high is strictly above the
// highs of the `order` bars on each side (equal neighbours do not make a swing, so a flat top is never counted
// twice); swing lows mirror it. A swing at i is only known once bar i+order has closed, so the last `order` bars
// never hold a swing.
struct Swing { int index; double price; bool high; };
QVector<Swing> swings(const QVector<Bar>& b, int order = 1);

inline double last(const QVector<double>& v) { return v.isEmpty() ? NAN : v.last(); }
inline bool finite(double x) { return std::isfinite(x); }
}
