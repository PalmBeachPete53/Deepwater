#pragma once
#include <QDateTime>
#include <QList>
#include <QString>

// wire::Sessions — the three trading sessions recapped by Global › Session Recap, DST-aware.
// Windows in each city's own clock (so daylight saving moves them in UTC automatically):
//   Tokyo     09:00–15:00 Asia/Tokyo
//   London    08:00–16:30 Europe/London
//   New York  08:30–16:00 America/New_York   (from the US data releases to the cash close)
// A session belongs to its city's calendar date; weekends have no session (holidays are not removed:
// a session without bars shows "—").
namespace wire {
enum class Market { Tokyo, London, NewYork };

struct Session {
  Market market = Market::NewYork;
  QDate date;              // the city's own date
  QDateTime start, end;    // UTC
  bool valid() const { return start.isValid(); }
  QString name() const;    // "NEW YORK"
  QString code() const;    // "NYC" / "LDN" / "TKY"
  QString flag() const;    // "us" / "gb" / "jp"
  bool contains(const QDateTime& t) const { return t >= start && t < end; }
  QString key() const;     // "2026-10-23/NYC" (settings, caches)
};

Session session(Market m, const QDate& cityDate);
// Sessions whose start lies in [from, to] (UTC), newest first.
QList<Session> sessionsBetween(const QDateTime& from, const QDateTime& to);
QList<Market> markets();   // Tokyo, London, New York
// "UTC-5", "UTC+1", "UTC+5:30" for the local clock at that moment.
QString utcLabel(const QDateTime& at = QDateTime::currentDateTime());
}
