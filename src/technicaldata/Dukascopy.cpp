// Forex pairs from Dukascopy's historical data feed. See Dukascopy.h.
#include "technicaldata/Dukascopy.h"
#include <QDateTime>
#include <QTimeZone>
#include <QtEndian>
#include <cmath>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "LzmaDec.h"
}

namespace technicaldata::dukascopy {

namespace {
const QTimeZone& newYork() {
  static const QTimeZone z("America/New_York");
  return z;
}
QDateTime ny(qint64 t) { return QDateTime::fromSecsSinceEpoch(t, newYork()); }
qint64 at17(const QDate& d) { return QDateTime(d, QTime(17, 0), newYork()).toSecsSinceEpoch(); }

void* lzAlloc(ISzAllocPtr, size_t size) { return std::malloc(size); }
void lzFree(ISzAllocPtr, void* address) { std::free(address); }
const ISzAlloc kAlloc = {lzAlloc, lzFree};
}  // namespace

const QVector<Instrument>& majors() {
  static const QVector<Instrument> m = {
      {"EURUSD", "EUR", "USD", 1e-5, QDate(2003, 5, 4)}, {"GBPUSD", "GBP", "USD", 1e-5, QDate(2003, 5, 4)},
      {"USDJPY", "USD", "JPY", 1e-3, QDate(2003, 5, 4)}, {"USDCHF", "USD", "CHF", 1e-5, QDate(2003, 5, 4)},
      {"AUDUSD", "AUD", "USD", 1e-5, QDate(2003, 8, 3)}, {"USDCAD", "USD", "CAD", 1e-5, QDate(2003, 8, 3)},
      {"NZDUSD", "NZD", "USD", 1e-5, QDate(2003, 8, 3)},
  };
  return m;
}

const Instrument* instrument(const QString& symbol) {
  QString s = symbol.trimmed().toUpper();
  s.remove('/').remove('-').remove('_');
  if (s.contains(':')) s = s.section(':', 1);
  for (const Instrument& i : majors()) if (i.symbol == s) return &i;
  return nullptr;
}

bool isForex(const QString& market) { return market.startsWith(PREFIX); }
QString marketOf(const QString& symbol) {
  const Instrument* i = instrument(symbol);
  return i ? QString(PREFIX) + i->symbol : QString();
}

QUrl hoursUrl(const QString& symbol, int year, int month) {
  return QUrl(QString("https://datafeed.dukascopy.com/datafeed/%1/%2/%3/BID_candles_hour_1.bi5")
                  .arg(symbol).arg(year).arg(month - 1, 2, 10, QChar('0')));
}
QUrl minutesUrl(const QString& symbol, const QDate& day) {
  return QUrl(QString("https://datafeed.dukascopy.com/datafeed/%1/%2/%3/%4/BID_candles_min_1.bi5")
                  .arg(symbol).arg(day.year()).arg(day.month() - 1, 2, 10, QChar('0')).arg(day.day(), 2, 10, QChar('0')));
}

QByteArray lzmaDecode(const QByteArray& alone, QString* error) {
  constexpr int HEAD = LZMA_PROPS_SIZE + 8;
  if (alone.size() < HEAD) { if (error) *error = QStringLiteral("not an LZMA file"); return {}; }
  const quint64 size = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(alone.constData()) + LZMA_PROPS_SIZE);
  const bool known = size != ~quint64(0) && size <= 64u << 20;
  QByteArray out(known ? qsizetype(size) : qsizetype(4) << 20, Qt::Uninitialized);
  SizeT destLen = SizeT(out.size()), srcLen = SizeT(alone.size() - HEAD);
  ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
  const SRes r = LzmaDecode(reinterpret_cast<Byte*>(out.data()), &destLen, reinterpret_cast<const Byte*>(alone.constData()) + HEAD, &srcLen,
                            reinterpret_cast<const Byte*>(alone.constData()), LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &kAlloc);
  if (r != SZ_OK) { if (error) *error = QString("LZMA error %1").arg(r); return {}; }
  out.truncate(qsizetype(destLen));
  return out;
}

QVector<Kline> parse(const QByteArray& bi5, qint64 start, double point, QString* error) {
  QVector<Kline> out;
  if (bi5.isEmpty()) return out;   // Dukascopy's empty file: no row
  const QByteArray raw = lzmaDecode(bi5, error);
  if (raw.isEmpty()) return out;
  if (raw.size() % 24) { if (error) *error = QStringLiteral("unexpected file size"); return out; }
  const auto* p = reinterpret_cast<const uchar*>(raw.constData());
  out.reserve(raw.size() / 24);
  for (qsizetype i = 0; i + 24 <= raw.size(); i += 24) {
    const qint32 t = qFromBigEndian<qint32>(p + i);
    const qint32 o = qFromBigEndian<qint32>(p + i + 4), c = qFromBigEndian<qint32>(p + i + 8);
    const qint32 l = qFromBigEndian<qint32>(p + i + 12), h = qFromBigEndian<qint32>(p + i + 16);
    const quint32 vb = qFromBigEndian<quint32>(p + i + 20);
    float v;
    std::memcpy(&v, &vb, 4);
    if (!(v > 0)) continue;   // the market closed: the last price repeated, nothing traded
    Kline k;
    k.t = start + t;
    // integers of `point` → prices, rounded to its decimals (1.10366, not 1.1036600000000001)
    const double dec = std::pow(10.0, std::round(-std::log10(point)));
    k.o = std::round(o * point * dec) / dec; k.h = std::round(h * point * dec) / dec;
    k.l = std::round(l * point * dec) / dec; k.c = std::round(c * point * dec) / dec;
    k.v = double(v);
    out << k;
  }
  return out;
}

// ------------------------------------------------------------------------------------------------ the forex week
bool closed(qint64 t) {
  const QDateTime d = ny(t);
  const int dow = d.date().dayOfWeek();   // 1 Monday … 7 Sunday
  const int mins = d.time().hour() * 60 + d.time().minute();
  if (dow == 6) return true;                          // Saturday
  if (dow == 5) return mins >= 17 * 60;               // Friday from 17:00
  if (dow == 7) return mins < 17 * 60;                // Sunday until 17:00
  return false;
}

qint64 sessionStart(qint64 t) {
  const QDateTime d = ny(t);
  const qint64 today = at17(d.date());
  return today <= t ? today : at17(d.date().addDays(-1));
}

qint64 bucketStart(qint64 t, int tf) {
  if (tf == 14400) {
    const qint64 s = sessionStart(t);
    return s + (t - s) / 14400 * 14400;
  }
  if (tf == 86400) return sessionStart(t);
  if (tf == 604800) {
    const QDate d = ny(sessionStart(t)).date();
    return at17(d.addDays(-(d.dayOfWeek() % 7)));   // back to Sunday
  }
  return t - ((t % tf) + tf) % tf;
}

qint64 bucketEnd(qint64 start, int tf) {
  const QDate d = ny(start).date();
  if (tf == 86400) return at17(d.addDays(1));
  if (tf == 604800) return at17(d.addDays(5));   // Friday 17:00
  if (tf == 14400) {
    const qint64 next = at17(ny(sessionStart(start)).date().addDays(1));
    return std::min(start + 14400, next);
  }
  return start + tf;
}

qint64 nextExpected(qint64 t, int tf) {
  qint64 n;
  if (tf == 86400) n = at17(ny(t).date().addDays(1));
  else if (tf == 604800) n = at17(ny(t).date().addDays(7));
  else if (tf == 14400) n = bucketEnd(t, tf);
  else n = t + tf;
  // the weekend: the market opens again on Sunday at 17:00 New York
  if (closed(n)) {
    QDate d = ny(n).date();
    while (d.dayOfWeek() != 7) d = d.addDays(1);
    n = std::max(n, at17(d));
  }
  return n;
}

QVector<Kline> aggregate(const QVector<Kline>& bars, int tf, qint64 coveredUntil) {
  QVector<Kline> out;
  qint64 cur = LLONG_MIN;
  Kline k;
  auto flush = [&] {
    if (cur != LLONG_MIN && bucketEnd(cur, tf) <= coveredUntil) out << k;
  };
  for (const Kline& b : bars) {
    const qint64 s = bucketStart(b.t, tf);
    if (s != cur) {
      flush();
      cur = s;
      k = Kline{s, b.o, b.h, b.l, b.c, b.v};
      continue;
    }
    k.h = std::max(k.h, b.h);
    k.l = std::min(k.l, b.l);
    k.c = b.c;
    k.v += b.v;
  }
  flush();
  return out;
}

}  // namespace technicaldata::dukascopy
