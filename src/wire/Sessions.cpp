#include "wire/Sessions.h"
#include <QCoreApplication>
#include <QTimeZone>
#include <algorithm>

namespace wire {
namespace {
struct Def { Market m; const char* tz; QTime open, close; const char* name; const char* code; const char* flag; };
const Def& def(Market m) {
  static const Def d[3] = {
    {Market::Tokyo, "Asia/Tokyo", QTime(9, 0), QTime(15, 0), QT_TRANSLATE_NOOP("wire", "TOKYO"), "TKY", "jp"},
    {Market::London, "Europe/London", QTime(8, 0), QTime(16, 30), QT_TRANSLATE_NOOP("wire", "LONDON"), "LDN", "gb"},
    {Market::NewYork, "America/New_York", QTime(8, 30), QTime(16, 0), QT_TRANSLATE_NOOP("wire", "NEW YORK"), "NYC", "us"},
  };
  return d[int(m)];
}
}

QString Session::name() const { return QCoreApplication::translate("wire", def(market).name); }
QString Session::code() const { return def(market).code; }
QString Session::flag() const { return def(market).flag; }
QString Session::key() const { return date.toString("yyyy-MM-dd") + "/" + code(); }

QList<Market> markets() { return {Market::Tokyo, Market::London, Market::NewYork}; }

Session session(Market m, const QDate& d) {
  Session s;
  s.market = m; s.date = d;
  if (!d.isValid() || d.dayOfWeek() > 5) return s;
  const Def& x = def(m);
  const QTimeZone tz(x.tz);
  s.start = QDateTime(d, x.open, tz).toUTC();
  s.end = QDateTime(d, x.close, tz).toUTC();
  return s;
}

QList<Session> sessionsBetween(const QDateTime& from, const QDateTime& to) {
  QList<Session> out;
  for (QDate d = from.toUTC().date().addDays(-1); d <= to.toUTC().date().addDays(1); d = d.addDays(1))
    for (Market m : markets()) {
      const Session s = session(m, d);
      if (s.valid() && s.start >= from && s.start <= to) out << s;
    }
  std::sort(out.begin(), out.end(), [](const Session& a, const Session& b) { return a.start > b.start; });
  return out;
}

QString utcLabel(const QDateTime& at) {
  const int off = at.toLocalTime().offsetFromUtc();
  const int h = std::abs(off) / 3600, m = (std::abs(off) % 3600) / 60;
  QString s = QString("UTC%1%2").arg(off < 0 ? "-" : "+").arg(h);
  if (m) s += QString(":%1").arg(m, 2, 10, QChar('0'));
  return off == 0 ? QString("UTC") : s;
}
}
