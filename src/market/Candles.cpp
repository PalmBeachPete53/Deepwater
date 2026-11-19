#include "market/Candles.h"
#include "market/Instruments.h"
#include "net/Http.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimeZone>
#include <QHash>
#include <QPointer>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

namespace market {
static int defaultTtl(const QString& iv) {
  if (iv.endsWith('m') && iv != "1mo" && iv != "3mo") return 60;
  if (iv == "1h" || iv == "60m" || iv == "90m") return 300;
  if (iv == "1d") return 1800;
  return 6 * 3600;
}
static Series parse(const QString& sym, const QString& iv, const QString& range, const net::Reply& rep) {
  Series s; s.symbol = sym; s.interval = iv; s.range = range; s.stale = rep.stale;
  if (!rep.ok()) { s.error = rep.error; return s; }
  const auto res = QJsonDocument::fromJson(rep.body).object()["chart"].toObject()["result"].toArray();
  if (res.isEmpty()) { s.error = "no data"; return s; }
  const auto r = res[0].toObject();
  s.timezone = r["meta"].toObject()["exchangeTimezoneName"].toString();
  const auto ts = r["timestamp"].toArray();
  const auto q = r["indicators"].toObject()["quote"].toArray().first().toObject();
  const auto o = q["open"].toArray(), h = q["high"].toArray(), l = q["low"].toArray(), c = q["close"].toArray(), v = q["volume"].toArray();
  s.bars.reserve(ts.size());
  for (int i = 0; i < ts.size(); ++i) {
    if (i >= c.size() || c[i].isNull() || o[i].isNull()) continue;
    s.bars.push_back({qint64(ts[i].toDouble()), o[i].toDouble(), h[i].toDouble(), l[i].toDouble(), c[i].toDouble(),
                      i < v.size() ? v[i].toDouble() : 0.0});
  }
  if (s.bars.isEmpty()) s.error = "no data";
  return s;
}
QDate sessionDay(const QString& timezone, qint64 t) {
  // one QTimeZone per id (constructing one parses the zone database); its UTC offset looked up once per half hour of
  // UTC time (offset changes happen on the hour or half hour): a series' bars no longer convert one by one
  struct Zone { QTimeZone tz; QHash<qint64, int> offsets; };
  static thread_local QHash<QString, Zone> zones;
  static thread_local QString lastId; static thread_local Zone* last = nullptr;
  if (!timezone.isEmpty()) {
    if (!last || timezone != lastId) {
      auto it = zones.find(timezone);
      if (it == zones.end()) it = zones.insert(timezone, Zone{QTimeZone(timezone.toUtf8()), {}});
      lastId = timezone; last = &*it;
    }
    if (last->tz.isValid()) {
      const qint64 key = t >= 0 ? t / 1800 : (t - 1799) / 1800;
      auto o = last->offsets.constFind(key);
      if (o == last->offsets.cend()) {
        if (last->offsets.size() > 200000) last->offsets.clear();
        o = last->offsets.insert(key, last->tz.offsetFromUtc(QDateTime::fromSecsSinceEpoch(key * 1800, QTimeZone::UTC)));
      }
      return QDateTime::fromSecsSinceEpoch(t + *o, QTimeZone::UTC).date();
    }
  }
  return QDateTime::fromSecsSinceEpoch(t + 3 * 3600, QTimeZone::UTC).date();
}
static QString yahooOf(const QString& s) { const auto* i = find(s); return i ? i->yahoo : s; }

// The JSON of a series is parsed off the GUI thread (2 years of hourly bars is ~1.2 MB; an FX sheet asks for 13
// daily series at once). One parse thread: callbacks keep the order of the answers. The callback runs on the GUI
// thread, skipped when its context object was destroyed meanwhile (like net::get's).
static QThreadPool& parsePool() {
  static QThreadPool* p = [] { auto* t = new QThreadPool; t->setMaxThreadCount(1); t->setExpiryTimeout(30000); return t; }();
  return *p;
}
static void parseThen(QObject* ctx, SeriesCallback cb, QString sym, QString iv, QString range, const net::Reply& rep) {
  const QPointer<QObject> guard(ctx);
  const bool hasCtx = ctx != nullptr;
  auto* w = new QFutureWatcher<Series>;
  QObject::connect(w, &QFutureWatcher<Series>::finished, w, [w, guard, hasCtx, cb] {
    w->deleteLater();
    if (hasCtx && !guard) return;
    cb(w->result());
  });
  w->setFuture(QtConcurrent::run(&parsePool(), [sym, iv, range, rep] { return parse(sym, iv, range, rep); }));
}

void candles(const QString& symbol, const QString& interval, const QString& range, QObject* ctx, SeriesCallback cb, int ttl,
             bool priority) {
  QUrl u("https://query1.finance.yahoo.com/v8/finance/chart/" + QUrl::toPercentEncoding(yahooOf(symbol)));
  QUrlQuery q; q.addQueryItem("interval", interval); q.addQueryItem("range", range); q.addQueryItem("includePrePost", "false");
  u.setQuery(q);
  net::Request r; r.url = u; r.ttlSec = ttl >= 0 ? ttl : defaultTtl(interval); r.priority = priority;
  net::get(r, ctx, [ctx, symbol, interval, range, cb](const net::Reply& rep) { parseThen(ctx, cb, symbol, interval, range, rep); });
}
void candlesBetween(const QString& symbol, const QString& interval, qint64 from, qint64 to, QObject* ctx, SeriesCallback cb) {
  QUrl u("https://query1.finance.yahoo.com/v8/finance/chart/" + QUrl::toPercentEncoding(yahooOf(symbol)));
  QUrlQuery q; q.addQueryItem("interval", interval); q.addQueryItem("period1", QString::number(from));
  q.addQueryItem("period2", QString::number(to)); u.setQuery(q);
  net::Request r; r.url = u; r.ttlSec = defaultTtl(interval);
  // the cache key drops period2's seconds so a re-run the same day hits the cache
  r.cacheKey = QString("candles %1 %2 %3 %4").arg(symbol, interval).arg(from).arg(to / 86400);
  net::get(r, ctx, [ctx, symbol, interval, cb](const net::Reply& rep) { parseThen(ctx, cb, symbol, interval, "custom", rep); });
}
}
