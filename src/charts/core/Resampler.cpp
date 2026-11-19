#include "charts/core/Resampler.h"
#include "charts/core/Series.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>

namespace charts {

namespace {
Bar merged(const Bar& a, const Bar& b) {   // a then b, same bucket
  Bar c = a;
  c.high = std::max(a.high, b.high);
  c.low = std::min(a.low, b.low);
  c.close = b.close;
  c.volume = (std::isnan(a.volume) ? 0 : a.volume) + (std::isnan(b.volume) ? 0 : b.volume);
  return c;
}
}  // namespace

Resampler::Resampler(const QString& tf, const QTimeZone& tz, int dayStartMinutes) : tz_(tz), dayStart_(dayStartMinutes) {
  const series::Tf p = series::parseTf(tf);
  unit_ = p.unit;
  n_ = p.n;
  seconds_ = p.seconds;
  if (!tz_.isValid()) tz_ = QTimeZone::UTC;
}

qint64 Resampler::bucketStart(qint64 t) const {
  if (unit_ != "D" && unit_ != "W" && unit_ != "M") {
    const qint64 size = qint64(seconds_);
    return (t >= 0 ? t / size : -((-t + size - 1) / size)) * size;
  }
  if (t >= cacheFrom_ && t < cacheTo_) return cacheFrom_;
  // the session day holding t: its local date, or the day before when t is before that day's start
  const QTime start = QTime(0, 0).addSecs(dayStart_ * 60);
  QDate d = QDateTime::fromSecsSinceEpoch(t, tz_).date();
  auto dayStartOf = [&](const QDate& day) { return QDateTime(day, start, tz_).toSecsSinceEpoch(); };
  if (dayStartOf(d) > t) d = d.addDays(-1);
  QDate first = d;
  if (unit_ == "D") {
    // n days: counted from the epoch's day, as the UTC version does (1D: every day)
    const qint64 k = d.toJulianDay() - QDate(1970, 1, 1).toJulianDay();
    const qint64 n = qint64(n_);
    first = d.addDays(-(((k % n) + n) % n));
  } else if (unit_ == "W") {
    first = d.addDays(-(d.dayOfWeek() - 1));   // Monday
    const qint64 n = qint64(n_);
    if (n > 1) {
      const qint64 w = (first.toJulianDay() - QDate(1970, 1, 5).toJulianDay()) / 7;   // 1970-01-05 was a Monday
      first = first.addDays(-7 * (((w % n) + n) % n));
    }
  } else {
    const qint64 months = qint64(d.year()) * 12 + (d.month() - 1), n = qint64(n_);
    const qint64 m0 = months - (((months % n) + n) % n);
    first = QDate(int(m0 / 12), int(m0 % 12) + 1, 1);
  }
  const qint64 from = dayStartOf(first);
  cacheFrom_ = from;
  cacheTo_ = bucketEnd(from);
  return from;
}

qint64 Resampler::bucketEnd(qint64 start) const {
  if (unit_ != "D" && unit_ != "W" && unit_ != "M") return start + qint64(seconds_);
  const QDateTime s = QDateTime::fromSecsSinceEpoch(start, tz_);
  // the same local start time on the next calendar day / week / month (23 or 25 hours across a change of clock)
  const QTime at = QTime(0, 0).addSecs(dayStart_ * 60);
  QDate d = s.date();
  if (s.time() < at && at != QTime(0, 0)) d = d.addDays(-1);   // a start just after midnight belongs to the day before
  QDate next = unit_ == "D" ? d.addDays(qint64(n_)) : unit_ == "W" ? d.addDays(7 * qint64(n_)) : d.addMonths(int(n_));
  return QDateTime(next, at, tz_).toSecsSinceEpoch();
}

std::optional<Bar> Resampler::push(const Bar& b) {
  if (have_ && b.time < last_.time) {   // older than what we have: out of order
    ++ignored_;
    return std::nullopt;
  }
  const qint64 start = bucketStart(b.time);
  std::optional<Bar> closed;
  if (have_ && start != bucket_) {
    closed = forming();
    closed->time = bucket_;
    haveBase_ = false;
  } else if (have_ && b.time != last_.time) {
    base_ = haveBase_ ? merged(base_, last_) : last_;   // the previous source bar is final: into the base
    haveBase_ = true;
  }
  // (same time as the last source bar: it is replaced, the base unchanged)
  last_ = b;
  if (std::isnan(last_.volume)) last_.volume = 0;
  bucket_ = start;
  have_ = true;
  return closed;
}

std::optional<Bar> Resampler::forming() const {
  if (!have_) return std::nullopt;
  Bar c = haveBase_ ? merged(base_, last_) : last_;
  c.open = haveBase_ ? base_.open : last_.open;
  c.time = bucket_;
  return c;
}

Bars resampleBars(const Bars& bars, const QString& tf, const QTimeZone& tz, int dayStartMinutes) {
  Resampler r(tf, tz, dayStartMinutes);
  Bars out;
  for (const Bar& b : bars)
    if (auto c = r.push(b)) out.push_back(*c);
  if (auto f = r.forming()) out.push_back(*f);
  return out;
}

}  // namespace charts
