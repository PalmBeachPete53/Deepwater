// Chantier F, step F3: previous day / week and session highs and lows, with the 2026 changes of clock (US 8 March and
// 1 November, Europe 29 March and 25 October: three weeks each year when London and New York are 4 hours apart).
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/SessionLevels.h"
#include <QDateTime>
#include <cmath>

using namespace charts;
using namespace charts::smc;

namespace {
qint64 utc(int y, int m, int d, int h = 0, int mi = 0) { return QDateTime(QDate(y, m, d), QTime(h, mi), QTimeZone::UTC).toSecsSinceEpoch(); }
SessionDef london() { return {LevelKind::London, QTimeZone("Europe/London"), QTime(8, 0), QTime(16, 30)}; }
SessionDef newYork() { return {LevelKind::NewYork, QTimeZone("America/New_York"), QTime(9, 30), QTime(16, 0)}; }
SessionDef asiaOvernight() { return {LevelKind::Asia, QTimeZone("America/New_York"), QTime(20, 0), QTime(0, 0)}; }   // 20:00–00:00 NY (ICT's Asia range)
// minute bars from `from`, price = minutes since `from` (so the high/low tell where in time they were made)
std::vector<Bar> minutes(qint64 from, int n) {
  std::vector<Bar> v;
  for (int i = 0; i < n; ++i) {
    const double p = 1000 + i;
    v.push_back({from + i * 60, p, p + 0.5, p - 0.5, p, 1});
  }
  return v;
}
}  // namespace

TEST("sessions: London and New York opens in UTC through both changes of clock (2026)") {
  SessionLevelTracker t(PeriodParams{});
  auto openOf = [&](const SessionDef& s, qint64 at) { return t.sessionAt(s, at)->first; };
  // winter: London 08:00 GMT, New York 09:30 EST = 14:30 UTC (6 h 30 apart)
  CHECK_EQ(openOf(london(), utc(2026, 3, 2, 12)), utc(2026, 3, 2, 8));
  CHECK_EQ(openOf(newYork(), utc(2026, 3, 2, 15)), utc(2026, 3, 2, 14, 30));
  // 9 March (US already on summer time, Europe not yet): New York 13:30 UTC, London still 08:00 — 5 h 30 apart
  CHECK_EQ(openOf(newYork(), utc(2026, 3, 9, 15)), utc(2026, 3, 9, 13, 30));
  CHECK_EQ(openOf(london(), utc(2026, 3, 9, 12)), utc(2026, 3, 9, 8));
  // 30 March (both on summer time): London 07:00 UTC
  CHECK_EQ(openOf(london(), utc(2026, 3, 30, 12)), utc(2026, 3, 30, 7));
  // 26 October (Europe back on winter time, US not yet): London 08:00, New York still 13:30 UTC
  CHECK_EQ(openOf(london(), utc(2026, 10, 26, 12)), utc(2026, 10, 26, 8));
  CHECK_EQ(openOf(newYork(), utc(2026, 10, 26, 15)), utc(2026, 10, 26, 13, 30));
  // 2 November: New York 14:30 UTC again
  CHECK_EQ(openOf(newYork(), utc(2026, 11, 2, 15)), utc(2026, 11, 2, 14, 30));
  // outside the hours: nothing; a session running past midnight belongs to the day it started
  CHECK(!t.sessionAt(london(), utc(2026, 3, 2, 17)).has_value());
  const auto asia = t.sessionAt(asiaOvernight(), utc(2026, 11, 3, 3, 30));   // 22:30 NY on 2 November
  CHECK(asia.has_value());
  CHECK_EQ(asia->first, utc(2026, 11, 3, 1));                                   // 20:00 EST
  CHECK_EQ(asia->second, utc(2026, 11, 3, 5));                                  // 00:00 EST
}

TEST("sessions: each finished session gives its high and low, with where they were made") {
  PeriodParams p;
  p.day = p.week = false;
  p.sessions = {london()};
  SessionLevelTracker t(p);
  std::vector<PeriodLevel> out, all;
  // two days of minutes from Monday 2 March 00:00 UTC
  const auto bars = minutes(utc(2026, 3, 2), 2 * 1440);
  for (int i = 0; i < int(bars.size()); ++i) {
    t.step(bars[size_t(i)], i, out);
    all.insert(all.end(), out.begin(), out.end());
  }
  CHECK_EQ(int(all.size()), 2);                      // Monday's and Tuesday's London sessions (Tuesday's ends 16:30)
  const PeriodLevel& mon = all[0];
  CHECK(mon.kind == LevelKind::London);
  CHECK_EQ(mon.start, utc(2026, 3, 2, 8));
  CHECK_EQ(mon.end, utc(2026, 3, 2, 16, 30));
  // prices rise one a minute: the low at the first minute of the session, the high at its last (16:29)
  CHECK_EQ(mon.lowTime, utc(2026, 3, 2, 8));
  CHECK_EQ(mon.highTime, utc(2026, 3, 2, 16, 29));
  CHECK_EQ(mon.high, 1000 + 16 * 60 + 29 + 0.5);
  CHECK_EQ(mon.low, 1000 + 8 * 60 - 0.5);
}

TEST("periods: previous day and week (UTC, and New York days from 17:00 across 1 November 2026)") {
  PeriodParams p;
  p.sessions.clear();
  SessionLevelTracker t(p);
  std::vector<PeriodLevel> out, all;
  // Monday 2 March 2026 … Tuesday 10 March 00:05 UTC
  const auto bars = minutes(utc(2026, 3, 2), 8 * 1440 + 6);
  for (int i = 0; i < int(bars.size()); ++i) {
    t.step(bars[size_t(i)], i, out);
    all.insert(all.end(), out.begin(), out.end());
  }
  int days = 0, weeks = 0;
  for (const auto& l : all) { days += l.kind == LevelKind::PrevDay; weeks += l.kind == LevelKind::PrevWeek; }
  CHECK_EQ(days, 8);                                  // 2 … 9 March, each ended by the next day's first bar
  CHECK_EQ(weeks, 1);                                 // the week of 2 March, ended by Monday 9 March
  for (const auto& l : all)
    if (l.kind == LevelKind::PrevWeek) {
      CHECK_EQ(l.start, utc(2026, 3, 2));
      CHECK_EQ(l.end, utc(2026, 3, 9));
    }
  // New York days from 17:00: the day that began Saturday 31 October 17:00 EDT lasts 25 hours
  PeriodParams ny;
  ny.week = false;
  ny.tz = QTimeZone("America/New_York");
  ny.dayStartMinutes = 17 * 60;
  SessionLevelTracker tn(ny);
  all.clear();
  const auto b2 = minutes(utc(2026, 10, 31, 20), 30 * 60);
  for (int i = 0; i < int(b2.size()); ++i) {
    tn.step(b2[size_t(i)], i, out);
    all.insert(all.end(), out.begin(), out.end());
  }
  bool long25 = false;
  for (const auto& l : all) long25 = long25 || (l.start == utc(2026, 10, 31, 21) && l.end - l.start == 25 * 3600);
  CHECK(long25);
}

TEST("engine with periods: yesterday's high is a level of its own kind, swept by today's move past it; same bits bar by bar") {
  EngineParams e;
  e.swing.left = e.swing.right = 50;   // few pivots: the period levels are what this checks
  e.periodLevels = true;
  e.periods.sessions = {london(), newYork()};
  // day 1 flat around 100 (high 101), day 2 opens at 100 then goes to 102
  std::vector<Bar> bars;
  const qint64 d1 = utc(2026, 5, 4);
  for (int i = 0; i < 1440; ++i) bars.push_back({d1 + i * 60, 100, i == 700 ? 101.0 : 100.2, 99.5, 100, 1});
  for (int i = 0; i < 600; ++i) bars.push_back({d1 + 86400 + i * 60, 100, 100.2 + (i > 500 ? 2 : 0), 99.8, 100, 1});
  LiquidityLevelEngine all(e);
  all.run(bars);
  const Level* prevHigh = nullptr;
  for (const Level& l : all.levels())
    if (l.kind == LevelKind::PrevDay && l.side == LevelSide::BuySide) prevHigh = &l;
  CHECK(prevHigh != nullptr);
  CHECK_EQ(prevHigh->price, 101.0);
  CHECK_EQ(prevHigh->originTime, d1 + 700 * 60);
  CHECK(prevHigh->swept);
  CHECK_EQ(prevHigh->sweptTime, d1 + 86400 + 501 * 60);
  int london = 0;
  for (const Level& l : all.levels()) london += l.kind == LevelKind::London;
  CHECK_EQ(london, 2);                                  // day 1's London session (high and low)
  LiquidityLevelEngine parts(e);
  parts.run(std::span<const Bar>(bars.data(), 777));
  for (size_t i = 777; i < bars.size(); ++i) parts.step(bars[i]);
  CHECK_EQ(parts.levels().size(), all.levels().size());
  bool same = true;
  for (size_t i = 0; i < all.levels().size(); ++i)
    same = same && all.levels()[i].price == parts.levels()[i].price && all.levels()[i].swept == parts.levels()[i].swept &&
           all.levels()[i].kind == parts.levels()[i].kind && all.levels()[i].sweptIndex == parts.levels()[i].sweptIndex;
  CHECK(same);
  // no allocation per bar inside a session or between sessions once the levels have room; the time zone work (Qt may
  // allocate) happens only at a session's edges, a few times a day
  LiquidityLevelEngine q(e);
  q.reserve(1000);
  q.run(std::span<const Bar>(bars.data(), 200));
  {
    alloccount::Scope s;
    q.run(std::span<const Bar>(bars.data() + 200, 210));   // 03:20 … 06:50 UTC: before London's 07:00 (BST) open
    CHECK_EQ(int(s.count()), 0);
  }
  {
    alloccount::Scope s;
    q.run(std::span<const Bar>(bars.data() + 410, 20));    // 06:50 … 07:10: the open
    CHECK(s.count() <= 6);
  }
  {
    alloccount::Scope s;
    q.run(std::span<const Bar>(bars.data() + 430, 300));   // 07:10 … 12:10: inside London
    CHECK_EQ(int(s.count()), 0);
  }
}
