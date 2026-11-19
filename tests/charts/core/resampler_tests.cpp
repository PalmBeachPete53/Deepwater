// Chantier F, step F2: the incremental resampler — same bars as series::resample in UTC, missing source bars, the live
// bar updating, and session days in a time zone around the 2026 changes of clock.
#include "Check.h"
#include "charts/core/Resampler.h"
#include "charts/core/Series.h"
#include <QDateTime>
#include <random>

using namespace charts;

namespace {
Bars minuteWalk(qint64 from, int n, unsigned seed, int skipEvery = 0) {
  std::mt19937 rng(seed);
  std::normal_distribution<double> ret(0, 0.001);
  std::uniform_real_distribution<double> u(0, 1);
  Bars v;
  double p = 100;
  for (int i = 0; i < n; ++i) {
    if (skipEvery && i % skipEvery == 3) continue;
    const double c = p * std::exp(ret(rng));
    v.push_back({from + i * 60, p, std::max(p, c) * (1 + 0.001 * u(rng)), std::min(p, c) * (1 - 0.001 * u(rng)), c, 1 + 10 * u(rng)});
    p = c;
  }
  return v;
}
bool same(const Bars& a, const Bars& b) {
  if (a.size() != b.size()) return false;
  for (int i = 0; i < a.size(); ++i)
    if (a[i].time != b[i].time || a[i].open != b[i].open || a[i].high != b[i].high || a[i].low != b[i].low || a[i].close != b[i].close ||
        std::fabs(a[i].volume - b[i].volume) > 1e-9 * std::max(1.0, a[i].volume))
      return false;
  return true;
}
qint64 utc(int y, int m, int d, int h = 0, int mi = 0) { return QDateTime(QDate(y, m, d), QTime(h, mi), QTimeZone::UTC).toSecsSinceEpoch(); }
}  // namespace

TEST("resampler: in UTC, the same bars as series::resample (5m … 1M), one bar at a time") {
  const Bars src = minuteWalk(utc(2026, 1, 28, 22, 7), 60 * 24 * 70, 4);   // 70 days of minutes, across month ends
  for (const QString tf : {"5m", "15m", "1h", "4h", "1D", "2D", "1W", "2W", "1M"}) {
    const Bars want = series::resample(src, tf);
    CHECK(same(resampleBars(src, tf), want));
    // incremental: closed bars as they close, then the forming one
    Resampler r(tf);
    Bars got;
    for (const Bar& b : src)
      if (auto c = r.push(b)) got.push_back(*c);
    got.push_back(*r.forming());
    CHECK(same(got, want));
  }
}

TEST("resampler: missing source bars — a bucket keeps the bars it has, an empty bucket gives no bar") {
  // 10:00 … 10:14 with 10:03, 10:08, 10:13 missing; then nothing until 10:30
  Bars src = minuteWalk(utc(2026, 5, 4, 10, 0), 15, 7, 5);
  const Bar late{utc(2026, 5, 4, 10, 31), 50, 51, 49, 50.5, 2};
  src.push_back(late);
  const Bars out = resampleBars(src, "5m");
  CHECK_EQ(int(out.size()), 4);                   // 10:00, 10:05, 10:10, 10:30 — no 10:15, 10:20, 10:25
  CHECK_EQ(out[3].time, utc(2026, 5, 4, 10, 30));
  CHECK_EQ(out[3].open, 50.0);
  // 10:00 bucket: bars 10:00, 10:01, 10:02, 10:04 (10:03 missing)
  double hi = -1, vol = 0;
  for (int i = 0; i < 4; ++i) { hi = std::max(hi, src[i].high); vol += src[i].volume; }
  CHECK_EQ(out[0].high, hi);
  CHECK(std::fabs(out[0].volume - vol) < 1e-9);
  CHECK_EQ(out[0].close, src[3].close);
}

TEST("resampler: the forming source bar updating replaces itself (volume not counted twice); older bars ignored") {
  Resampler r("5m");
  const qint64 t = utc(2026, 5, 4, 10, 0);
  r.push({t, 10, 11, 9, 10.5, 3});
  r.push({t + 60, 10.5, 10.6, 10.4, 10.5, 1});
  r.push({t + 60, 10.5, 12, 10.4, 11.8, 2});      // the same minute, later: higher high, more volume
  r.push({t + 60, 10.5, 12, 10.1, 11.0, 4});
  auto f = r.forming();
  CHECK(f.has_value());
  CHECK_EQ(f->time, t);
  CHECK_EQ(f->open, 10.0);
  CHECK_EQ(f->high, 12.0);
  CHECK_EQ(f->low, 9.0);
  CHECK_EQ(f->close, 11.0);
  CHECK_EQ(f->volume, 7.0);                         // 3 + the last version's 4
  r.push({t + 30, 1, 1, 1, 1, 100});                // older than the last: ignored
  CHECK_EQ(r.ignored(), 1);
  CHECK_EQ(r.forming()->volume, 7.0);
  auto closed = r.push({t + 300, 11, 11, 11, 11, 1});
  CHECK(closed.has_value());
  CHECK_EQ(closed->volume, 7.0);
  CHECK_EQ(closed->time, t);
}

TEST("resampler: session days in a time zone across the 2026 changes of clock (23 and 25 hour days)") {
  const QTimeZone ny("America/New_York"), paris("Europe/Paris");
  CHECK(ny.isValid());
  CHECK(paris.isValid());
  // New York, days from 17:00: US clocks go back on 1 November 2026 (EDT → EST) and forward on 8 March 2026
  Resampler fx("1D", ny, 17 * 60);
  const qint64 s1 = fx.bucketStart(utc(2026, 11, 1, 12));   // Sunday noon UTC: the day that began Saturday 17:00 EDT
  CHECK_EQ(s1, utc(2026, 10, 31, 21));                       // 17:00 EDT = 21:00 UTC
  CHECK_EQ(fx.bucketEnd(s1), utc(2026, 11, 1, 22));          // Sunday 17:00 EST = 22:00 UTC: 25 hours
  CHECK_EQ(fx.bucketStart(utc(2026, 11, 1, 21, 59)), s1);    // still that day an hour after 17:00 UTC-4 would be
  CHECK_EQ(fx.bucketStart(utc(2026, 11, 1, 22)), utc(2026, 11, 1, 22));
  const qint64 s2 = fx.bucketStart(utc(2026, 3, 8, 12));
  CHECK_EQ(s2, utc(2026, 3, 7, 22));                         // Saturday 17:00 EST
  CHECK_EQ(fx.bucketEnd(s2), utc(2026, 3, 8, 21));           // Sunday 17:00 EDT: 23 hours
  // Paris, days from midnight: Europe changes on 29 March and 25 October 2026
  Resampler eu("1D", paris);
  const qint64 m = eu.bucketStart(utc(2026, 3, 29, 12));
  CHECK_EQ(m, utc(2026, 3, 28, 23));                         // 00:00 CET
  CHECK_EQ(eu.bucketEnd(m), utc(2026, 3, 29, 22));           // next 00:00 CEST: 23 hours
  const qint64 o = eu.bucketStart(utc(2026, 10, 25, 12));
  CHECK_EQ(o, utc(2026, 10, 24, 22));
  CHECK_EQ(eu.bucketEnd(o), utc(2026, 10, 25, 23));          // 25 hours
  // weeks from Monday 00:00 Paris, months from the 1st
  CHECK_EQ(Resampler("1W", paris).bucketStart(utc(2026, 11, 18, 12)), utc(2026, 11, 15, 23));
  CHECK_EQ(Resampler("1M", paris).bucketStart(utc(2026, 11, 18, 12)), utc(2026, 10, 31, 23));
  // minutes across the US change, resampled to New York days: the 25-hour day holds 25 × 60 minutes
  const Bars mins = minuteWalk(utc(2026, 10, 30, 0), 60 * 24 * 4, 2);
  const Bars days = resampleBars(mins, "1D", ny, 17 * 60);
  int long25 = 0;
  for (const Bar& d : days) long25 += d.time == utc(2026, 10, 31, 21);
  CHECK_EQ(long25, 1);
  double vol = 0;
  for (const Bar& b : mins)
    if (b.time >= utc(2026, 10, 31, 21) && b.time < utc(2026, 11, 1, 22)) vol += b.volume;
  for (const Bar& d : days)
    if (d.time == utc(2026, 10, 31, 21)) CHECK(std::fabs(d.volume - vol) < 1e-6);
}
