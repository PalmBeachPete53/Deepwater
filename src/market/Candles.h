#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QDate>
#include <functional>

// Price history (Yahoo Finance chart endpoint), cached on disk.
//   market::candles("EURUSD", "1h", "1mo", this, [](const market::Series& s) { ... });
// interval: 1m 2m 5m 15m 30m 60m 1h 90m 1d 5d 1wk 1mo 3mo; range: 1d 5d 1mo 3mo 6mo 1y 2y 5y 10y ytd max
// Yahoo limits: 1m ≤ 7 days, intraday < 1h ≤ 60 days, 1h ≤ 730 days.
namespace market {
struct Bar { qint64 t; double o, h, l, c, v; };   // t: UTC seconds, bar open
struct Series {
  QString symbol, interval, range;
  QVector<Bar> bars;
  QString error;
  bool stale = false;
  QString timezone;                                  // exchange time zone (Yahoo meta.exchangeTimezoneName), IANA id
  bool ok() const { return error.isEmpty() && !bars.isEmpty(); }
};
// Session (trading) day of a daily/weekly/monthly bar stamped t: its date in the exchange's time zone. Yahoo stamps a
// finished daily bar at the local session open or local midnight, and the still-open bar of today with the time of
// its last trade, so the exchange-local date is right for both (a fixed "+N h of UTC" dates the open bar tomorrow in
// the evening). Without a known time zone: the UTC date of t + 3 h (the old rule).
QDate sessionDay(const QString& timezone, qint64 t);
inline QDate sessionDay(const Series& s, qint64 t) { return sessionDay(s.timezone, t); }
using SeriesCallback = std::function<void(const Series&)>;
// priority: ahead of the other requests waiting for Yahoo (net::Request::priority) — for a few small series a page
// shows at the top, not for bulk loads (they would all jump the queue together).
void candles(const QString& symbol, const QString& interval, const QString& range, QObject* ctx, SeriesCallback cb,
             int ttlSec = -1 /* default from interval */, bool priority = false);
// Daily closes between two dates (period1/period2 UTC seconds), for long seasonal history.
void candlesBetween(const QString& symbol, const QString& interval, qint64 from, qint64 to, QObject* ctx, SeriesCallback cb);
}
