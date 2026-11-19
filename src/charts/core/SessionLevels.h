#pragma once
// Chantier F, step F3: the high and low of the previous day and week, and of the Asia / London / New York sessions, as
// liquidity levels of their own kind. Each period ends, its high becomes a buy-side level and its low a sell-side one
// (the engine then sweeps them like the others). All calendar work goes through QTimeZone: a session is its local hours
// in its own zone, so London and New York keep their local opens when their clocks change on different dates (in 2026:
// US 8 March and 1 November, Europe 29 March and 25 October — weeks when London–New York is 4 hours apart, not 5).
//   - Day and week: the Resampler's calendar (a time zone and the minute the day starts, e.g. New York 17:00).
//   - A bar belongs to a session when its START is inside the session's local hours [start, end) (a session crossing
//     midnight runs into the next local day). Bars longer than the session make no session level.
//   - A period is complete when a bar at or after its end arrives (bars come in order; a gap closes it as well).
//   - Time zone work (QDateTime, which may allocate) happens only at a session's or day's edges — each session keeps its
//     current instance and its next opening — so bars inside or between sessions cost no allocation.
#include "charts/core/LiquidityLevels.h"
#include "charts/core/Resampler.h"
#include <QString>
#include <QTime>
#include <QTimeZone>
#include <optional>
#include <vector>

namespace charts::smc {

// Tokyo 09:00–15:00, London 08:00–16:30, New York 09:30–16:00 (their exchanges' cash hours).
std::vector<SessionDef> defaultSessions();

// A finished period: its high and low, and where they were made.
struct PeriodLevel {
  LevelKind kind = LevelKind::PrevDay;
  qint64 start = 0, end = 0;        // the period (s, UTC)
  double high = 0, low = 0;
  int highIndex = -1, lowIndex = -1;
  qint64 highTime = 0, lowTime = 0;
};

class SessionLevelTracker {
public:
  explicit SessionLevelTracker(const PeriodParams& p);
  // One bar (index = its position in the series). Finished periods go to `out` (cleared first).
  void step(const Bar& b, int index, std::vector<PeriodLevel>& out);
  // The session instance holding time t: its [start, end) in UTC seconds, or nothing.
  std::optional<std::pair<qint64, qint64>> sessionAt(const SessionDef& s, qint64 t) const;

private:
  struct Running {
    LevelKind kind;
    bool open = false;
    qint64 start = 0, end = 0;
    double high = 0, low = 0;
    int hi = -1, lo = -1;
    qint64 hiT = 0, loT = 0;
    void add(const Bar& b, int index);
    PeriodLevel finish() const { return {kind, start, end, high, low, hi, lo, hiT, loT}; }
    qint64 skipUntil = INT64_MIN;   // sessions: times before this are known to be outside (no time zone work for them)
  };
  PeriodParams p_;
  Resampler day_, week_;
  Running dayRun_{LevelKind::PrevDay}, weekRun_{LevelKind::PrevWeek};
  std::vector<Running> sessRun_;
};

}  // namespace charts::smc
