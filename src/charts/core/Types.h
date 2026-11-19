#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Shared value types of the pure computing layer (src/charts/core): no widgets, no network, QtCore only.
#include <QVector>
#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <cmath>
#include <limits>

namespace charts {

// double → int for indexes, pixel rows and counts: clamped to ±1e9 first, since converting an out-of-range or NaN
// double is undefined behaviour (NaN gives `nanValue`).
inline int clampToInt(double v, int nanValue = 0) { return std::isnan(v) ? nanValue : int(std::clamp(v, -1e9, 1e9)); }
// The same for times in ms or s: ±9e18.
inline qint64 clampToInt64(double v, qint64 nanValue = 0) { return std::isnan(v) ? nanValue : qint64(std::clamp(v, -9e18, 9e18)); }

// One OHLCV bar. time = Unix seconds, UTC, start of the bar (as quant-terminal's bars).
struct Bar {
  qint64 time = 0;
  double open = 0, high = 0, low = 0, close = 0, volume = 0;
};
using Bars = QVector<Bar>;

// A numeric series aligned with Bars; NaN = no value (JS NaN / undefined / null).
using Series = QVector<double>;

inline constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool isNum(double v) { return std::isfinite(v); }   // JS Number.isFinite

// Indicator / strategy parameters: the same JSON objects quant-terminal passes around ({length: 14, source: "close"}).
using Params = QJsonObject;
inline double num(const Params& p, const QString& k, double def = NaN) {
  const QJsonValue v = p.value(k);
  if (v.isDouble()) return v.toDouble();
  if (v.isBool()) return v.toBool() ? 1 : 0;
  if (v.isString()) { bool ok = false; double d = v.toString().toDouble(&ok); return ok ? d : def; }
  return def;
}
inline int inum(const Params& p, const QString& k, int def = 0) {
  double d = num(p, k, NaN); return isNum(d) ? int(d) : def;
}
inline QString str(const Params& p, const QString& k, const QString& def = {}) {
  const QJsonValue v = p.value(k);
  if (v.isString()) return v.toString();
  if (v.isDouble()) return QString::number(v.toDouble());
  return def;
}

}  // namespace charts
