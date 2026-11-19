#include "global/Sessions.h"
#include <QCoreApplication>
#include "ui/Theme.h"
#include <QtGlobal>
#include <QHash>
#include <algorithm>

namespace global {

// ---------------------------------------------------------------- clock
namespace {
struct ClockState {
  QTimeZone zone;
  qint64 offsetMs = 0;   // DW_NOW - system time at start
  ClockState() {
    const QByteArray tz = qgetenv("DW_TZ");
    zone = !tz.isEmpty() && QTimeZone(tz).isValid() ? QTimeZone(tz) : QTimeZone::systemTimeZone();
    const QByteArray n = qgetenv("DW_NOW");
    if (!n.isEmpty()) {
      QDateTime t = QDateTime::fromString(QString::fromLatin1(n), Qt::ISODate);
      if (t.isValid()) {
        t = QDateTime(t.date(), t.time(), zone);
        offsetMs = QDateTime::currentDateTimeUtc().msecsTo(t);
      }
    }
  }
};
const ClockState& clock() { static const ClockState c; return c; }
}

QDateTime nowUtc() { return QDateTime::currentDateTimeUtc().addMSecs(clock().offsetMs); }
QDateTime now() { return nowUtc().toTimeZone(clock().zone); }
const QTimeZone& userZone() { return clock().zone; }
QString userZoneId() {
  QString id = QString::fromLatin1(clock().zone.id());
  return id.isEmpty() ? QStringLiteral("LOCAL") : id;
}
QString utcOffsetLabel(const QDateTime& t) {
  const int off = t.offsetFromUtc();
  if (off == 0) return "UTC";
  const int a = std::abs(off), h = a / 3600, m = (a % 3600) / 60;
  return QString("UTC%1%2%3").arg(off < 0 ? "-" : "+").arg(h).arg(m ? QString(":%1").arg(m, 2, 10, QChar('0')) : QString());
}

// ---------------------------------------------------------------- sessions
const QList<Session>& sessions() {
  static const QList<Session> s{
    {"sydney", QT_TRANSLATE_NOOP("global", "Sydney"), "Australia/Sydney", QTime(7, 0), QTime(16, 0), "au", QColor("#a78bfa"), QColor("#5f4f8c"),
     QColor("#2a2242"), QColor("#4b3f72"), QColor("#594f66"), -33.87, 151.21, "SYD"},
    {"tokyo", QT_TRANSLATE_NOOP("global", "Tokyo"), "Asia/Tokyo", QTime(9, 0), QTime(18, 0), "jp", QColor("#e0645a"), QColor("#884b43"),
     QColor("#4a201c"), QColor("#6e3434"), QColor("#714031"), 35.68, 139.69, "TKY"},
    {"london", QT_TRANSLATE_NOOP("global", "London"), "Europe/London", QTime(8, 0), QTime(17, 0), "gb", QColor("#3fb6c4"), QColor("#22707c"),
     QColor("#0f3a40"), QColor("#326860"), QColor("#385c66"), 51.51, -0.13, "LDN"},
    {"newyork", QT_TRANSLATE_NOOP("global", "New York"), "America/New_York", QTime(8, 0), QTime(17, 0), "us", QColor("#e3c23a"), QColor("#86710f"),
     QColor("#3e3300"), QColor("#70661f"), QColor("#766006"), 40.71, -74.01, "NY"},
  };
  return s;
}

QString sessionName(const Session& s) { return QCoreApplication::translate("global", s.name.toUtf8().constData()); }

QColor sessionColour(const Session& s, Tone t) {
  if (theme::isDefault()) {
    switch (t) {
      case Tone::Bright: return s.bright; case Tone::Bar: return s.bar; case Tone::BarText: return s.barText;
      case Tone::Arc: return s.arc; case Tone::Accent: return s.accent;
    }
  }
  const bool dark = theme::isDark();
  const auto& r = theme::roles();
  QColor base;
  if (s.id == "sydney") base = dark ? QColor("#a78bfa") : QColor("#7a5bd0");
  else if (s.id == "tokyo") base = r.down;
  else if (s.id == "london") base = r.up;
  else base = dark ? QColor("#e3c23a") : QColor("#b07d10");
  switch (t) {
    case Tone::Bright: return base;
    case Tone::Bar: return theme::mix(base, r.panel, dark ? 0.35 : 0.15);
    case Tone::Arc: return theme::mix(base, r.panel, dark ? 0.5 : 0.45);
    case Tone::Accent: return theme::mix(base, r.panel, 0.4);
    case Tone::BarText: return dark ? theme::mix(base, QColor("#000000"), 0.7) : QColor("#ffffff");
  }
  return base;
}

static bool weekday(const QDate& d) { return d.dayOfWeek() <= 5; }
// A session's zone, built once: QTimeZone(id) looks the IANA id up each time, and the spans are asked for every
// second (Market Hours) and every globe frame (Overview).
static QTimeZone zoneOf(const Session& s) {
  thread_local QHash<QByteArray, QTimeZone> zones;
  auto it = zones.find(s.tz);
  if (it == zones.end()) it = zones.insert(s.tz, QTimeZone(s.tz));
  return *it;
}
static Span spanOn(const Session& s, const QDate& localDay) {
  const QTimeZone tz = zoneOf(s);
  return {QDateTime(localDay, s.open, tz).toUTC(), QDateTime(localDay, s.close, tz).toUTC()};
}

Span currentSpan(const Session& s, const QDateTime& utc) {
  const QDate d = utc.toTimeZone(zoneOf(s)).date();
  if (!weekday(d)) return {};
  const Span sp = spanOn(s, d);
  return sp.contains(utc) ? sp : Span{};
}
bool isOpen(const Session& s, const QDateTime& utc) { return currentSpan(s, utc).valid(); }
Span nextSpan(const Session& s, const QDateTime& utc) {
  QDate d = utc.toTimeZone(zoneOf(s)).date();
  for (int i = 0; i < 10; ++i, d = d.addDays(1)) {
    if (!weekday(d)) continue;
    const Span sp = spanOn(s, d);
    if (sp.open > utc) return sp;
  }
  return {};
}
Span currentOrNext(const Session& s, const QDateTime& utc) {
  const Span c = currentSpan(s, utc);
  return c.valid() ? c : nextSpan(s, utc);
}
int liveCount(const QDateTime& utc) {
  int n = 0;
  for (const auto& s : sessions()) n += isOpen(s, utc);
  return n;
}

double dayMinutes(const QDateTime& t, const QTimeZone& zone) {
  const QDateTime l = t.toTimeZone(zone);
  const QDateTime d0(l.date(), QTime(0, 0), zone), d1(l.date().addDays(1), QTime(0, 0), zone);
  const double len = d0.secsTo(d1);
  return len > 0 ? d0.secsTo(l) / len * 1440.0 : 0;
}

QList<DaySegment> segmentsOnDay(const Session& s, const QDate& day, const QTimeZone& zone) {
  QList<DaySegment> out;
  const QDateTime d0(day, QTime(0, 0), zone), d1(day.addDays(1), QTime(0, 0), zone);
  const qint64 dayLenMin = d0.secsTo(d1) / 60;   // 1380/1440/1500 on DST days
  for (int k = -2; k <= 2; ++k) {
    const QDate ld = day.addDays(k);
    if (!weekday(ld)) continue;
    const Span sp = spanOn(s, ld);
    if (sp.close <= d0.toUTC() || sp.open >= d1.toUTC()) continue;
    DaySegment seg;
    seg.span = sp;
    seg.startMin = int(qMax<qint64>(0, d0.toUTC().secsTo(sp.open) / 60));
    seg.endMin = int(qMin<qint64>(dayLenMin, d0.toUTC().secsTo(sp.close) / 60));
    // scale to a 24 h axis on DST days so 24:00 stays at the right edge
    if (dayLenMin != 1440) {
      seg.startMin = int(seg.startMin * 1440 / dayLenMin);
      seg.endMin = int(seg.endMin * 1440 / dayLenMin);
    }
    if (seg.endMin > seg.startMin) out << seg;
  }
  std::sort(out.begin(), out.end(), [](const DaySegment& a, const DaySegment& b) { return a.startMin < b.startMin; });
  return out;
}

QList<Overlap> overlapsOnDay(const QDate& day, const QTimeZone& zone) {
  // sweep over the minutes where the number of open sessions changes
  struct Ev { int m; int idx; bool open; };
  QList<Ev> ev;
  const auto& ss = sessions();
  for (int i = 0; i < ss.size(); ++i)
    for (const auto& seg : segmentsOnDay(ss[i], day, zone)) { ev << Ev{seg.startMin, i, true} << Ev{seg.endMin, i, false}; }
  std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.m != b.m ? a.m < b.m : (!a.open && b.open); });
  QList<Overlap> out;
  QList<int> open;
  int last = 0;
  for (const auto& e : ev) {
    if (open.size() >= 2 && e.m > last) {
      QList<int> sorted = open; std::sort(sorted.begin(), sorted.end());
      if (!out.isEmpty() && out.last().endMin == last && out.last().sessions == sorted) out.last().endMin = e.m;
      else out << Overlap{last, e.m, sorted};
    }
    if (e.open) open << e.idx; else open.removeOne(e.idx);
    last = e.m;
  }
  return out;
}

QString countdown(qint64 secs) {
  secs = qMax<qint64>(0, secs);
  const qint64 d = secs / 86400, h = (secs % 86400) / 3600, m = (secs % 3600) / 60, s = secs % 60;
  if (d > 0) return QCoreApplication::translate("global", "%1d %2h %3m").arg(d).arg(h).arg(m);
  if (h > 0) return QCoreApplication::translate("global", "%1h %2m").arg(h).arg(m);
  return QCoreApplication::translate("global", "%1m %2s").arg(m).arg(s, 2, 10, QChar('0'));
}

QString spanHours(const Span& sp, const QTimeZone& zone) {
  if (!sp.valid()) return "—";
  return sp.open.toTimeZone(zone).toString("HH:mm") + "-" + sp.close.toTimeZone(zone).toString("HH:mm");
}

QString statusLine(const QDateTime& utc) {
  QStringList open;
  for (const auto& s : sessions()) if (isOpen(s, utc)) open << sessionName(s).toUpper();
  if (open.isEmpty()) {
    // the FX week closes at 17:00 New York on Friday and reopens with Sydney on Monday morning (Sunday in New York)
    static const QTimeZone nyZone("America/New_York");
    const QDateTime ny = utc.toTimeZone(nyZone);
    const int dow = ny.date().dayOfWeek();
    if ((dow == 5 && ny.time() >= QTime(17, 0)) || dow >= 6)
      return QCoreApplication::translate("global", "WEEKEND · FX MARKETS CLOSED");
    return QCoreApplication::translate("global", "FX BETWEEN SESSIONS");
  }
  if (open.size() == 1) return QCoreApplication::translate("global", "%1 SESSION LIVE").arg(open.first());
  return QCoreApplication::translate("global", "%1 OVERLAP").arg(open.join(" · "));
}
}
