// Forex pairs from Dukascopy into technical-data.sqlite. See ForexCollector.h.
#include "technicaldata/ForexCollector.h"
#include "net/Http.h"
#include "net/Sources.h"
#include "technicaldata/Dukascopy.h"
#include "technicaldata/TechnicalDataSpec.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>
#include <memory>

namespace technicaldata {

namespace {
// one file took ~15 s in 2026-11 (the server is slow, not the line), and it often answered 503 "No server is available"
// (19 of 22 in a first run): two on the wire at once, a second apart, and patient retries
const sources::Registrar regDukascopy({"dukascopy", "Dukascopy Bank (historical data feed)", {"datafeed.dukascopy.com"},
                                       sources::Access::Keyless, 1000,
                                       "Forex bars (bid) since 2003: monthly hour files and daily minute files (Data › Technical data)",
                                       "Public historical data feed, no key (what its own JForex platform downloads); personal use",
                                       {"data/technical-data"}, {}, 2});
using namespace dukascopy;

qint64 dayStart(const QDate& d) { return QDateTime(d, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(); }
QDate dayOf(qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date(); }
QString key(const char* what, const QString& market) { return QString("fx.%1:%2").arg(what, market); }
}  // namespace

struct ForexCollector::Turn {
  QString market;
  PairSpec spec;
  const Instrument* inst = nullptr;
  QDate startDay, yesterday;
  bool needHour = false;
  QVector<int> minuteTfs, derivedTfs;
  int budget = 0;
  bool hoursDone = false, recentDone = false, unpublished = false;   // unpublished: a recent day's file is not out yet
  QDate hours, recent, minutes;   // where the downloads stand (the store's meta, read once: its writes are queued)
  QVector<Kline> tail;            // the last stored hours: a week a 4h / 1D / 1W bar may begin before the new ones
  bool fileFailed = false;        // a file could not be read (not a refusal): the turn ends as failed
  std::unique_ptr<QObject> ctx = std::make_unique<QObject>();   // gone with the turn: answers still coming are dropped
};

ForexCollector& ForexCollector::instance() {
  static ForexCollector* c = new ForexCollector(&Store::instance());
  return *c;
}

ForexCollector::ForexCollector(Store* store, QObject* parent) : QObject(parent), m_store(store) {
  m_fetch = [](const QUrl& url, QObject* ctx, Collector::Reply reply) {
    net::Request r;
    r.url = url;
    r.ttlSec = 0;
    r.staleOnError = false;   // no disk cache: tens of thousands of files, each read once
    r.timeoutMs = 60000;
    net::get(r, ctx, [reply](const net::Reply& rep) { reply(rep.status, rep.body, rep.ok() ? QString() : rep.error); });
  };
  m_timer.setInterval(30 * 60 * 1000);
  connect(&m_timer, &QTimer::timeout, this, &ForexCollector::syncNow);
  connect(store, &Store::pairsChanged, this, [this](const QString& market) {
    if (!isForex(market)) return;
    m_scanned.remove(market);
    m_queue.removeAll(market);
    // its settings changed: its turn starts again (queued first, so ending the turn never looks idle in between)
    if (m_started && markets().contains(market)) enqueue(market);
    if (m_running && m_current == market) abortTurn();
    emit progressChanged();
  });
  // paused: out of the queue, its turn ended now (files on the wire are dropped); resumed: a turn
  connect(store, &Store::pausedChanged, this, [this](const QString& market) {
    if (!market.isEmpty() && !isForex(market)) return;
    for (const QString& m : QStringList(m_queue)) if (!active(m)) m_queue.removeAll(m);
    if (m_started) for (const QString& m : markets()) if (market.isEmpty() || m == market) enqueue(m);
    if (m_running && !active(m_current)) abortTurn();
    emit progressChanged();
  });
}

qint64 ForexCollector::now() const { return m_now ? m_now() : QDateTime::currentMSecsSinceEpoch(); }
QDate ForexCollector::yesterday() const { return dayOf(now() / 1000).addDays(-1); }

QStringList ForexCollector::recorded() const {
  if (!m_markets.isEmpty()) return m_markets;
  QStringList out;
  for (const QString& m : m_store->pairs()) if (isForex(m)) out << m;
  return out;
}

QStringList ForexCollector::markets() const {
  QStringList out;
  for (const QString& m : recorded()) if (!m_store->isPaused(m)) out << m;
  return out;
}

bool ForexCollector::active(const QString& market) const {
  return m_started && !m_store->isPaused(market) && recorded().contains(market);
}

void ForexCollector::start() {
  if (m_started) return;
  m_started = true;
  syncNow();
  m_timer.start();
}

void ForexCollector::stop() {
  m_started = false;
  m_timer.stop();
  m_queue.clear();
  abortTurn();
}

// through a copy: ending the turn clears m_abort, which must not destroy the function running
void ForexCollector::abortTurn() {
  const std::function<void()> a = m_abort;
  if (a) a();
}

void ForexCollector::syncNow() {
  for (const QString& m : markets()) enqueue(m);
}

void ForexCollector::enqueue(const QString& market) {
  if (m_queue.contains(market)) return;
  m_queue << market;
  emit progressChanged();
  if (!m_running) QTimer::singleShot(0, this, &ForexCollector::next);
}

void ForexCollector::next() {
  if (m_running) return;
  if (m_queue.isEmpty()) { m_status.clear(); emit progressChanged(); emit idle(); return; }
  const QString m = m_queue.takeFirst();
  m_running = true;
  m_current = m;
  emit progressChanged();
  run(m);
}

// ------------------------------------------------------------------------------------------------ a turn
void ForexCollector::run(const QString& market) {
  auto t = std::make_shared<Turn>();
  t->market = market;
  t->spec = specFromJson(m_store->pairSpec(market));
  t->inst = instrument(market);
  if (!t->spec.valid() || !t->inst || !active(market)) { finishTurn(t, false); return; }
  t->startDay = t->spec.start ? dayOf(t->spec.start) : t->inst->first;
  t->yesterday = yesterday();
  for (int tf : t->spec.timeframes) {
    if (tf < 3600) t->minuteTfs << tf;
    else if (tf > 3600) t->derivedTfs << tf;
  }
  t->needHour = t->spec.timeframes.contains(3600) || !t->derivedTfs.isEmpty();
  t->budget = m_chunk;
  // once what is queued is written (a change just saved, the last turn's pages), without blocking the window
  m_store->whenWritten(this, [this, t, market] {
  if (!active(market)) { finishTurn(t, false); return; }
  t->hours = QDate::fromString(m_store->meta(key("hours", market)), Qt::ISODate);
  t->recent = QDate::fromString(m_store->meta(key("recent", market)), Qt::ISODate);
  t->minutes = QDate::fromString(m_store->meta(key("minutes", market)), Qt::ISODate);
  if (!t->hours.isValid()) t->hours = t->startDay.addDays(-1);
  if (!t->minutes.isValid()) t->minutes = t->startDay.addDays(-1);
  // a timeframe made from the stored hours but still empty (just added): made from them now, nothing to download
  if (t->needHour && m_store->state(market, 3600).count > 0)
    for (int tf : t->derivedTfs)
      if (m_store->state(market, tf).count == 0) {
        const QVector<Kline> hours = m_store->range(market, 3600, 0, LLONG_MAX);
        const QVector<Kline> agg = aggregate(hours, tf, hours.isEmpty() ? 0 : hours.last().t + 3600);
        if (!agg.isEmpty()) m_store->add(market, tf, agg);
      }
  t->tail = m_store->last(market, 3600, 8 * 24);
  phaseHours(t);
  });
}

void ForexCollector::phaseHours(std::shared_ptr<Turn> t) {
  m_phaseTfs.clear();
  for (int tf : t->spec.timeframes) if (tf >= 3600) m_phaseTfs << tf;
  if (!t->needHour) { t->hoursDone = t->recentDone = true; phaseMinutes(t); return; }
  const QString m = t->market;
  // the hours deleted (1h added back, an earlier first day): from the first day again
  if (m_store->state(m, 3600).count == 0 && t->hours >= t->startDay) {
    t->hours = t->startDay.addDays(-1);
    t->recent = QDate();
    m_store->setMeta(key("hours", m), t->hours.toString(Qt::ISODate));
    m_store->setMeta(key("recent", m), QString());
  }
  // an earlier first day: the monthly files are read again from it, each one before the stored hours (far behind their
  // last one); their indicators wait until the hours have caught up instead of computing the whole later history
  // again after every month. Kept across turns (120 files each) while it lasts.
  bool behind = false;
  for (int tf : t->spec.timeframes)
    if (tf >= 3600) {
      const SyncState s = m_store->state(m, tf);
      behind |= s.count > 0 && dayOf(s.last) > t->hours.addMonths(3);
    }
  holdHours(t, behind);
  QDate& hours = t->hours;
  const QDate recent = t->recent;
  const QDate today = dayOf(now() / 1000);
  const QDate lastMonthEnd = QDate(today.year(), today.month(), 1).addDays(-1);
  QVector<File> files;
  for (QDate ms(hours.addDays(1).year(), hours.addDays(1).month(), 1); ms <= lastMonthEnd && files.size() < t->budget; ms = ms.addMonths(1)) {
    const QDate me = ms.addMonths(1).addDays(-1);
    if (recent.isValid() && me <= recent) {   // its days were read one by one already
      if (me > hours) { hours = me; m_store->setMeta(key("hours", m), me.toString(Qt::ISODate)); }
      continue;
    }
    files << File{hoursUrl(t->inst->symbol, ms.year(), ms.month()), dayStart(ms), me, true};
  }
  if (files.isEmpty()) { t->hoursDone = true; holdHours(t, false); phaseRecent(t); return; }
  const bool reachesEnd = files.last().day == lastMonthEnd;
  t->budget -= int(files.size());
  fetchOrdered(t, files, [this, t, lastMonthEnd](const File& f, int status, const QByteArray& body) {
    const QString m = t->market;
    if (status == 404) {
      // a month Dukascopy has no file for: before its history, or not published yet (the days cover it meanwhile)
      if (f.day < lastMonthEnd.addMonths(-2)) { t->hours = f.day; m_store->setMeta(key("hours", m), f.day.toString(Qt::ISODate)); return true; }
      t->hoursDone = true;
      return false;
    }
    QString err;
    const QVector<Kline> bars = parse(body, f.start, t->inst->point, &err);
    if (!err.isEmpty()) { m_errors[m] = err; t->fileFailed = true; return false; }
    m_status = QCoreApplication::translate("data", "%1: hours of %2 (Dukascopy's monthly files)").arg(t->inst->symbol, f.day.toString("yyyy-MM"));
    emit progressChanged();
    writeHours(t, bars, dayStart(f.day.addDays(1)));
    t->hours = f.day;
    m_store->setMeta(key("hours", m), f.day.toString(Qt::ISODate));
    ++m_files[m];
    return true;
  }, [this, t, reachesEnd](bool failed) {
    if (failed) { finishTurn(t, false); return; }
    if (!t->hoursDone && !reachesEnd) { finishTurn(t, true); return; }   // more months: the next pair's turn first
    t->hoursDone = true;
    holdHours(t, false);
    phaseRecent(t);
  });
}

void ForexCollector::phaseRecent(std::shared_ptr<Turn> t) {
  QDate from = t->hours;
  if (t->recent.isValid() && t->recent > from) from = t->recent;
  QVector<File> files;
  for (QDate d = std::max(from.addDays(1), t->startDay); d <= t->yesterday && files.size() < std::max(t->budget, 40); d = d.addDays(1))
    if (d.dayOfWeek() != 6) files << File{minutesUrl(t->inst->symbol, d), dayStart(d), d, false};
  if (files.isEmpty()) { t->recentDone = true; phaseMinutes(t); return; }
  const bool reachesEnd = files.last().day >= (t->yesterday.dayOfWeek() == 6 ? t->yesterday.addDays(-1) : t->yesterday);
  t->budget -= int(files.size());
  fetchOrdered(t, files, [this, t](const File& f, int status, const QByteArray& body) {
    const QString m = t->market;
    if (status == 404) {
      if (f.day < t->yesterday.addDays(-3)) { t->recent = f.day; m_store->setMeta(key("recent", m), f.day.toString(Qt::ISODate)); return true; }
      t->unpublished = true;   // not published yet: the next check
      return false;
    }
    QString err;
    const QVector<Kline> minutes = parse(body, f.start, t->inst->point, &err);
    if (!err.isEmpty()) { m_errors[m] = err; t->fileFailed = true; return false; }
    m_status = QCoreApplication::translate("data", "%1: hours of %2 (Dukascopy's minute files)").arg(t->inst->symbol, f.day.toString("yyyy-MM-dd"));
    emit progressChanged();
    const qint64 end = dayStart(f.day.addDays(1));
    writeHours(t, aggregate(minutes, 3600, end), end);
    t->recent = f.day;
    m_store->setMeta(key("recent", m), f.day.toString(Qt::ISODate));
    ++m_files[m];
    return true;
  }, [this, t, reachesEnd](bool failed) {
    if (failed) { finishTurn(t, false); return; }
    t->recentDone = reachesEnd && !t->unpublished;
    t->unpublished = false;
    phaseMinutes(t);
  });
}

void ForexCollector::phaseMinutes(std::shared_ptr<Turn> t) {
  m_phaseTfs = t->minuteTfs;
  const QString m = t->market;
  // the hours are up to date: their history is whole, and their holes (holidays) are known
  if (t->needHour && t->hoursDone && t->recentDone && !m_scanned.contains(m + "|h")) {
    m_scanned.insert(m + "|h");
    for (int tf : t->spec.timeframes)
      if (tf >= 3600) {
        m_store->setHistoryDone(m, tf);
        m_store->scanGaps(m, tf, this, [this](const QVector<Gap>&) { emit progressChanged(); });
      }
  }
  if (t->minuteTfs.isEmpty() || t->budget <= 0) { finishTurn(t, !t->minuteTfs.isEmpty() && t->budget <= 0); return; }
  QDate& until = t->minutes;
  bool emptied = false;
  for (int tf : t->minuteTfs) emptied |= m_store->state(m, tf).count == 0;
  if (emptied && until >= t->startDay) {   // a minute timeframe added (or deleted): its history from the first day
    until = t->startDay.addDays(-1);
    m_store->setMeta(key("minutes", m), until.toString(Qt::ISODate));
    m_scanned.remove(m + "|m");
  }
  QVector<File> files;
  for (QDate d = until.addDays(1); d <= t->yesterday && files.size() < t->budget; d = d.addDays(1))
    if (d.dayOfWeek() != 6) files << File{minutesUrl(t->inst->symbol, d), dayStart(d), d, false};
  if (files.isEmpty()) {
    if (!m_scanned.contains(m + "|m")) {
      m_scanned.insert(m + "|m");
      for (int tf : t->minuteTfs) {
        m_store->setHistoryDone(m, tf);
        m_store->scanGaps(m, tf, this, [this](const QVector<Gap>&) { emit progressChanged(); });
      }
    }
    finishTurn(t, false);
    return;
  }
  const bool reachesEnd = files.last().day >= (t->yesterday.dayOfWeek() == 6 ? t->yesterday.addDays(-1) : t->yesterday);
  fetchOrdered(t, files, [this, t](const File& f, int status, const QByteArray& body) {
    const QString m = t->market;
    if (status == 404) {
      if (f.day < t->yesterday.addDays(-3)) { t->minutes = f.day; m_store->setMeta(key("minutes", m), f.day.toString(Qt::ISODate)); return true; }
      t->unpublished = true;
      return false;
    }
    QString err;
    const QVector<Kline> minutes = parse(body, f.start, t->inst->point, &err);
    if (!err.isEmpty()) { m_errors[m] = err; t->fileFailed = true; return false; }
    m_status = QCoreApplication::translate("data", "%1: minutes of %2 (Dukascopy's minute files)").arg(t->inst->symbol, f.day.toString("yyyy-MM-dd"));
    emit progressChanged();
    const qint64 end = dayStart(f.day.addDays(1));
    for (int tf : t->minuteTfs) {
      const QVector<Kline> bars = tf == 60 ? minutes : aggregate(minutes, tf, end);
      if (!bars.isEmpty()) m_store->add(m, tf, bars);
    }
    t->minutes = f.day;
    m_store->setMeta(key("minutes", m), f.day.toString(Qt::ISODate));
    ++m_files[m];
    return true;
  }, [this, t, reachesEnd](bool failed) {
    if (failed || t->unpublished) { finishTurn(t, false); return; }
    if (!reachesEnd) { finishTurn(t, true); return; }
    phaseMinutes(t);   // up to date: the history is whole, its holes are looked for
  });
}

void ForexCollector::finishTurn(std::shared_ptr<Turn> t, bool more) {
  QTimer::singleShot(0, this, [t] { t->ctx.reset(); });   // never inside one of its own answers
  // Dukascopy kept refusing a file: that pair again in 10 minutes (the others meanwhile)
  if (m_errors.contains(t->market) && m_started) {
    const QString m = t->market;
    QTimer::singleShot(m_retryTurnMs, this, [this, m] { if (m_started && markets().contains(m)) enqueue(m); });
  }
  m_running = false;
  m_current.clear();
  m_phaseTfs.clear();
  // not coming back for more months (failed, paused, settings changed, closing): what was written is computed now
  if (!more || !m_started) holdHours(t, false);
  if (more && m_started) m_queue << t->market;
  emit progressChanged();
  next();
}

void ForexCollector::holdHours(const std::shared_ptr<Turn>& t, bool on) {
  QVector<int> tfs;
  for (int tf : t->spec.timeframes) if (tf >= 3600) tfs << tf;
  const QString m = t->market;
  if (on) { for (int tf : tfs) m_store->setBackfilling(m, tf, true); return; }
  // once the hours queued so far are written (they have no callback): the computation then sees them all
  m_store->whenWritten(this, [this, m, tfs] { for (int tf : tfs) m_store->setBackfilling(m, tf, false); });
}

// hours → the store, and the 4h, 1D, 1W bars they complete (from the stored hours, which the weeks need)
// hours → the store, and the 4h, 1D, 1W bars they complete: from the turn's last stored hours and these (a week starts
// up to five days before its last hours), so nothing waits for the writer
void ForexCollector::writeHours(std::shared_ptr<Turn> t, const QVector<Kline>& hours, qint64 coveredUntil) {
  if (hours.isEmpty()) return;
  const QString market = t->market;
  if (t->spec.timeframes.contains(3600)) m_store->add(market, 3600, hours);
  // the tail must be the week right before these hours: after a jump (the turn's first file, months skipped, a first
  // day moved earlier) it is read from the store, where everything before them is written by then
  const qint64 first = hours.first().t;
  if (!t->derivedTfs.isEmpty() && (t->tail.isEmpty() || t->tail.last().t >= first || t->tail.last().t < first - 8 * 86400))
    t->tail = m_store->range(market, 3600, first - 8 * 86400, first - 1);
  QVector<Kline> all = t->tail;
  for (const Kline& k : hours) if (all.isEmpty() || k.t > all.last().t) all << k;
  for (int tf : t->derivedTfs) {
    const qint64 first = bucketStart(hours.first().t, tf);
    QVector<Kline> bars;
    for (const Kline& k : aggregate(all, tf, coveredUntil)) if (k.t >= first) bars << k;
    if (!bars.isEmpty()) m_store->add(market, tf, bars);
  }
  t->tail = all.mid(std::max<qsizetype>(0, all.size() - 8 * 24));
}

// ------------------------------------------------------------------------------------------------ files in order
void ForexCollector::fetchOrdered(std::shared_ptr<Turn> t, QVector<File> files,
                                  std::function<bool(const File&, int, const QByteArray&)> use, std::function<void(bool)> done) {
  struct S {
    QVector<File> files;
    QHash<int, QPair<int, QByteArray>> got;
    QHash<int, int> tries;
    int issued = 0, used = 0;
    bool over = false;
    std::function<void(int)> issue;
    std::function<void()> pump;
  };
  auto s = std::make_shared<S>();
  s->files = std::move(files);
  QPointer<QObject> ctx(t->ctx.get());
  const QString m = t->market;
  auto end = [this, s, done](bool failed) {
    if (s->over) return;
    s->over = true;
    m_abort = nullptr;
    QTimer::singleShot(0, this, [s] { s->issue = nullptr; s->pump = nullptr; });   // the cycle s → issue → s
    done(failed);
  };
  // paused, settings changed, Deepwater closing: the turn ends here, nothing more is written (no error: not retried)
  m_abort = [end] { end(true); };
  s->pump = [this, s, use, end, m, t] {
    if (!s->over && !active(m)) { end(true); return; }
    while (!s->over && s->got.contains(s->used)) {
      const auto r = s->got.take(s->used);
      const int before = int(m_errors.count(m));
      if (!use(s->files[s->used], r.first, r.second)) { end(t->fileFailed); return; }   // stopped, or a file unreadable
      if (!before || r.first == 200) m_errors.remove(m);   // a file came: the last failure is over
      ++s->used;
    }
    if (s->over) return;
    if (s->used == s->files.size()) { m_errors.remove(m); end(false); return; }
    while (s->issued < s->files.size() && s->issued - s->used < 6) s->issue(s->issued++);
  };
  s->issue = [this, s, ctx, end, m](int i) {
    if (!ctx || s->over) return;
    m_fetch(s->files[i].url, ctx.data(), [this, s, ctx, end, m, i](int status, const QByteArray& body, const QString& err) {
      if (!ctx || s->over) return;
      if (status != 0 && status != 200 && status != 404 && status != 429 && status < 500) {   // 403 …: not a file, not retried
        m_errors[m] = QString("HTTP %1").arg(status);
        end(true);
        return;
      }
      const bool retry = status == 0 || status == 429 || status >= 500;
      if (retry) {
        const int n = ++s->tries[i];
        if (n <= m_retry.size()) {
          QTimer::singleShot(m_retry[n - 1], ctx.data(), [s, i] { if (!s->over && s->issue) s->issue(i); });
          return;
        }
        m_errors[m] = status ? QString("HTTP %1").arg(status) : err.isEmpty() ? QCoreApplication::translate("data", "no answer") : err;
        end(true);
        return;
      }
      s->got.insert(i, {status == 200 ? 200 : status, body});
      if (s->pump) s->pump();
    });
  };
  s->pump();
}

QVector<Collector::Progress> ForexCollector::progress() const {
  QVector<Collector::Progress> out;
  for (const QString& m : recorded()) {
    const PairSpec spec = specFromJson(m_store->pairSpec(m));
    for (const auto& tf : timeframes()) {
      if (!spec.timeframes.contains(tf.seconds)) continue;
      Collector::Progress p;
      p.market = m;
      p.tf = tf.name;
      p.seconds = tf.seconds;
      p.state = m_store->state(m, tf.seconds);
      p.running = m_running && m_current == m && m_phaseTfs.contains(tf.seconds);   // the timeframes being fetched
      p.paused = m_store->isPaused(m);
      p.cancelled = m_store->cancelled(m);
      p.queued = m_queue.contains(m);
      p.pagesDone = m_files.value(m);
      p.error = m_errors.value(m);
      out << p;
    }
  }
  return out;
}

}  // namespace technicaldata
