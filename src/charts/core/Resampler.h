#pragma once
// Bars to a larger timeframe, one bar at a time (chantier F, step F2) — reusable anywhere in Deepwater; the batch form
// (resampleBars) is the same loop, so incremental and complete give the same bars.
//
//   Buckets   below one day: multiples of the timeframe since the Unix epoch (UTC), like series::resample (crypto and
//             most data sources count that way). Days, weeks (Monday) and months: by the calendar of a time zone, a day
//             starting `dayStartMinutes` after local midnight (e.g. New York at 17:00 for FX, Europe/Paris at 00:00) —
//             through QTimeZone, so a day around a change of clock lasts 23 or 25 hours, never shifted by hand.
//   Missing   a bucket without any source bar gives no bar (no invented candle); a bucket with some source bars missing
//             gives the bar of those it has.
//   Live      a source bar pushed again with the same time (the forming bar updating) replaces the previous version of it
//             in its bucket — its volume is not counted twice.
// Source bars must come in ascending time (an older one is ignored and counted).
#include "charts/core/Types.h"
#include <QString>
#include <QTimeZone>
#include <functional>
#include <optional>
#include <vector>

namespace charts {

class Resampler {
public:
  // tf: "5m", "4h", "1D", "1W", "1M"… (series::parseTf). tz / dayStartMinutes: for D, W and M buckets.
  explicit Resampler(const QString& tf, const QTimeZone& tz = QTimeZone::UTC, int dayStartMinutes = 0);

  // A source bar. Returns the bucket closed by it (when it starts a new bucket), if any.
  std::optional<Bar> push(const Bar& b);
  // The bucket being built (nullopt before the first bar).
  std::optional<Bar> forming() const;
  qint64 bucketStart(qint64 t) const;   // start (s, UTC) of the bucket holding time t
  qint64 bucketEnd(qint64 start) const; // start of the next bucket
  int ignored() const { return ignored_; }

private:
  QString unit_;
  double n_ = 1, seconds_ = 60;
  QTimeZone tz_;
  int dayStart_ = 0;
  bool have_ = false;
  qint64 bucket_ = 0;
  Bar base_{};          // the bucket's source bars before the last one
  bool haveBase_ = false;
  Bar last_{};          // the last source bar (may still change)
  int ignored_ = 0;
  // cache of the last calendar bucket asked (bars come in order: most lookups hit it)
  mutable qint64 cacheFrom_ = 1, cacheTo_ = 0;
};

Bars resampleBars(const Bars& bars, const QString& tf, const QTimeZone& tz = QTimeZone::UTC, int dayStartMinutes = 0);

}  // namespace charts
