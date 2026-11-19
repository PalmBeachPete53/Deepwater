#pragma once
// Forex pairs of Data › Technical data (user's choices, 2026-11-18): the seven majors from Dukascopy's public historical
// data feed (no key; https://datafeed.dukascopy.com, the files its own JForex platform reads), bid prices, the trading
// day cut at 17:00 New York (the forex convention: five daily bars a week, the summer time of New York followed).
//
// Files (LZMA "alone" .bi5, rows of 24 bytes big-endian: seconds from the file's start, open, close, low, high as
// integers of `point`, volume as a float; months are 0-based in the paths):
//   <PAIR>/<YYYY>/<MM-1>/BID_candles_hour_1.bi5         one month of hour bars (a file per month: the quick way back)
//   <PAIR>/<YYYY>/<MM-1>/<DD>/BID_candles_min_1.bi5     one UTC day of minute bars (published once the day is over)
// Closed-market rows (weekend, holidays) come filled with the last price and no volume: left out.
// The 5m and 15m bars are the day's minutes put together; 4h, 1D and 1W the stored hours put together, cut at 17:00
// New York (4h: from that 17:00 on, the last one of a day shortened or lengthened by a summer-time change). A bar's time
// is its first second, UTC (a daily bar of Tuesday starts Monday 21:00 or 22:00 UTC).
#include "technicaldata/TechnicalDataStore.h"
#include <QDate>
#include <QString>
#include <QUrl>
#include <QVector>

namespace technicaldata::dukascopy {

struct Instrument {
  QString symbol;     // "EURUSD"
  QString base, quote;
  double point;       // a price unit of the files: 1e-5, 1e-3 for the yen
  QDate first;        // about when Dukascopy's history starts
};
const QVector<Instrument>& majors();                // EURUSD GBPUSD USDJPY USDCHF AUDUSD USDCAD NZDUSD
const Instrument* instrument(const QString& symbol);   // "EURUSD", "EUR/USD"; nullptr when not one
constexpr const char* PREFIX = "DUKASCOPY:";
bool isForex(const QString& market);                // "DUKASCOPY:…"
QString marketOf(const QString& symbol);            // "DUKASCOPY:EURUSD"

QUrl hoursUrl(const QString& symbol, int year, int month);   // month 1-12
QUrl minutesUrl(const QString& symbol, const QDate& day);

// A .bi5 file → bars (the closed market's rows left out); start = the file's first second, UTC. Empty body = no row.
QVector<Kline> parse(const QByteArray& bi5, qint64 start, double point, QString* error = nullptr);
QByteArray lzmaDecode(const QByteArray& alone, QString* error = nullptr);

// The forex week: open from Sunday 17:00 to Friday 17:00 New York.
bool closed(qint64 t);                              // inside the weekend
qint64 sessionStart(qint64 t);                      // the 17:00 New York at or before t
qint64 bucketStart(qint64 t, int tf);               // the bar t falls in: 14400 / 86400 / 604800 from 17:00 New York, else t - t % tf
qint64 bucketEnd(qint64 start, int tf);             // when that bar's trading ends (a week: Friday 17:00)
qint64 nextExpected(qint64 t, int tf);              // the bar after the one at t, the weekend skipped (holes: anything later)
// bars of a finer timeframe (sorted) → bars of tf; only those whose bucketEnd ≤ coveredUntil (complete)
QVector<Kline> aggregate(const QVector<Kline>& bars, int tf, qint64 coveredUntil);

}  // namespace technicaldata::dukascopy
