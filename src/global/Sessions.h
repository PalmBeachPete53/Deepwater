#pragma once
#include <QColor>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimeZone>

// The four FX sessions (Sydney, Tokyo, London, New York): conventional business hours of each centre,
// Monday to Friday in the centre's own calendar, daylight-saving aware through the tz database.
// Everything here is computed locally from the clock; no network.
namespace global {

// The user's clock. Normally the system clock and zone. Test hooks (documented, read once):
//   DW_TZ=America/Bogota            use this IANA zone as the user's zone
//   DW_NOW=2026-10-23T16:18:33      start the clock at this local time (in the user's zone); it then runs normally
QDateTime now();                 // current time in the user's zone
QDateTime nowUtc();
const QTimeZone& userZone();
QString userZoneId();            // "America/Bogota"
QString utcOffsetLabel(const QDateTime& t);   // "UTC-5", "UTC+5:30", "UTC"

struct Session {
  QString id;          // "sydney"
  QString name;        // "Sydney"
  QByteArray tz;       // IANA zone of the centre
  QTime open, close;   // local business hours
  QString flag;        // iso2 for ui::flag
  QColor bright;       // open state (bars, arcs, dots, markers)
  QColor bar;          // timeline bar, closed
  QColor barText;      // label inside a timeline bar
  QColor arc;          // dial arc, dim
  QColor accent;       // card top accent, closed
  double lat, lon;     // centre position (globe markers)
  QString shortName;   // "SYD", "TKY", "LDN", "NY"
};
const QList<Session>& sessions();
QString sessionName(const Session& s);
// Session colours in the current theme. Deepwater: the measured colours above, unchanged. Other themes: four
// hue-distinct bases (purple, the theme's down red, its up green, gold), fitted to dark or light backgrounds,
// and the dim variants derived from them against the theme's panel colour.
enum class Tone { Bright, Bar, BarText, Arc, Accent };
QColor sessionColour(const Session& s, Tone t);   // translated city name ("Londres" in French)    // Sydney, Tokyo, London, New York (Market Hours order)

struct Span {
  QDateTime open, close;   // UTC
  bool valid() const { return open.isValid(); }
  bool contains(const QDateTime& utc) const { return valid() && open <= utc && utc < close; }
};

bool isOpen(const Session& s, const QDateTime& utc);
Span currentSpan(const Session& s, const QDateTime& utc);          // invalid when closed
Span nextSpan(const Session& s, const QDateTime& utc);             // next session that opens after utc
Span currentOrNext(const Session& s, const QDateTime& utc);
int liveCount(const QDateTime& utc);

// Parts of the sessions that fall on the user's calendar day `day` (weekend sessions skipped).
struct DaySegment {
  int startMin = 0, endMin = 0;   // minutes from the user's midnight, 0..1440
  Span span;                      // the whole session
};
// Position of t on its own day's 24 h axis, in minutes 0..1440 (scaled on 23 h / 25 h DST days).
double dayMinutes(const QDateTime& t, const QTimeZone& zone);
QList<DaySegment> segmentsOnDay(const Session& s, const QDate& day, const QTimeZone& zone);

// Windows of the user's day where two or more sessions are open together.
struct Overlap {
  int startMin = 0, endMin = 0;
  QList<int> sessions;            // indexes in sessions()
};
QList<Overlap> overlapsOnDay(const QDate& day, const QTimeZone& zone);

// "1d 22h 41m" (floor); under one hour "41m 12s".
QString countdown(qint64 secs);
// Hours of a span in a zone: "15:00-00:00" (midnight at the end of a span reads 00:00).
QString spanHours(const Span& sp, const QTimeZone& zone);
// "FX BETWEEN SESSIONS", "LONDON SESSION LIVE", "LONDON · NEW YORK OVERLAP", "WEEKEND · FX MARKETS CLOSED"
QString statusLine(const QDateTime& utc);
}
