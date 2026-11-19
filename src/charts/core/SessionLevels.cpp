#include "charts/core/SessionLevels.h"
#include <QDateTime>
#include <algorithm>

namespace charts::smc {

std::vector<SessionDef> defaultSessions() {
  return {{LevelKind::Asia, QTimeZone("Asia/Tokyo"), QTime(9, 0), QTime(15, 0)},
          {LevelKind::London, QTimeZone("Europe/London"), QTime(8, 0), QTime(16, 30)},
          {LevelKind::NewYork, QTimeZone("America/New_York"), QTime(9, 30), QTime(16, 0)}};
}

void SessionLevelTracker::Running::add(const Bar& b, int index) {
  if (hi < 0 || b.high > high) { high = b.high; hi = index; hiT = b.time; }
  if (lo < 0 || b.low < low) { low = b.low; lo = index; loT = b.time; }
}

SessionLevelTracker::SessionLevelTracker(const PeriodParams& p)
    : p_(p), day_("1D", p.tz, p.dayStartMinutes), week_("1W", p.tz, p.dayStartMinutes) {
  for (const SessionDef& s : p_.sessions) sessRun_.push_back(Running{s.kind});
}

std::optional<std::pair<qint64, qint64>> SessionLevelTracker::sessionAt(const SessionDef& s, qint64 t) const {
  if (!s.tz.isValid()) return std::nullopt;
  const QDate d = QDateTime::fromSecsSinceEpoch(t, s.tz).date();
  const bool overnight = s.end <= s.start;
  // the instance starting on the local day of t, or (overnight) on the day before
  for (const QDate& day : {d, d.addDays(-1)}) {
    const qint64 a = QDateTime(day, s.start, s.tz).toSecsSinceEpoch();
    const qint64 b = QDateTime(overnight ? day.addDays(1) : day, s.end, s.tz).toSecsSinceEpoch();
    if (t >= a && t < b) return std::make_pair(a, b);
    if (!overnight) break;
  }
  return std::nullopt;
}

void SessionLevelTracker::step(const Bar& b, int index, std::vector<PeriodLevel>& out) {
  out.clear();
  auto period = [&](bool on, Resampler& cal, Running& run) {
    if (!on) return;
    const qint64 start = cal.bucketStart(b.time);
    if (run.open && start != run.start) out.push_back(run.finish());   // the previous day / week is over
    if (!run.open || start != run.start) {
      run = Running{run.kind};
      run.open = true;
      run.start = start;
      run.end = cal.bucketEnd(start);
    }
    run.add(b, index);
  };
  period(p_.day, day_, dayRun_);
  period(p_.week, week_, weekRun_);
  for (size_t k = 0; k < p_.sessions.size(); ++k) {
    Running& run = sessRun_[k];
    if (run.open && b.time >= run.end) {   // past its end: finished
      out.push_back(run.finish());
      run.open = false;
    }
    if (run.open && b.time < run.end) {    // inside the running instance: no time zone work
      run.add(b, index);
      continue;
    }
    if (b.time < run.skipUntil) continue;  // known to be outside until the next opening
    const SessionDef& s = p_.sessions[k];
    const auto in = sessionAt(s, b.time);
    if (!in) {
      // the next opening after this bar (today's, or tomorrow's)
      const QDate d = QDateTime::fromSecsSinceEpoch(b.time, s.tz).date();
      qint64 next = QDateTime(d, s.start, s.tz).toSecsSinceEpoch();
      if (next <= b.time) next = QDateTime(d.addDays(1), s.start, s.tz).toSecsSinceEpoch();
      run.skipUntil = next;
      continue;
    }
    if (!run.open || run.start != in->first) {
      run = Running{run.kind};
      run.open = true;
      run.start = in->first;
      run.end = in->second;
    }
    run.add(b, index);
  }
}

}  // namespace charts::smc
