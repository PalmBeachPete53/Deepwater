#pragma once
// Data › Technical data (user's design, 2026-11-17/18): market bars on several timeframes in their own database,
// <data>/technical-data/technical-data.sqlite, apart from the chains of facts (a bar is many values of one market at one time, and
// indicators are computed from bars, not observed). For machine learning later.
//
//   markets(id, symbol "BINANCE:BTCUSDT", source)
//   bars(market, tf, t, o, h, l, c, v, qv, n, tbv, tbqv)   one closed bar; tf in seconds (60 … 604800), t = open time
//        (Unix seconds, UTC); v base volume, qv quote volume, n trades, tbv / tbqv taker-buy base / quote volume
//        (Binance gives them all: who hit the ask, i.e. aggressive buying). WITHOUT ROWID, key (market, tf, t).
//   pairs(market, spec, added)   what is recorded for a pair: timeframes, first day, indicators (TechnicalDataSpec.h);
//        only its bars are kept (a page arriving for a pair or timeframe removed meanwhile is dropped)
//   sync(market, tf, first, last, count, history_done, updated, start_req)   what is stored, whether the whole history
//        is in, and from which first day it was asked (a pair's start moved earlier: the bars before are fetched)
//   gaps(market, tf, from_t, to_t, missing, checked)              holes between two stored bars, and whether the
//        exchange was asked again for them (what it still lacks is the exchange's own outage)
//   ind(market, tf, t, warm, c0 … cN)  indicator values of one bar: one column per indicator output any pair records
//        (a pair's own columns filled, the others NULL, which costs a byte); a new one is added, never rebuilt
//   ind_sync(market, tf, last, count, version)   up to which bar they are computed, with which definitions
//   meta(k, v)   pairs_seeded, fx_seeded; the forex collector's cursors "fx.<what>:<market>" (deleted with the pair)
// Writes go to one worker thread in order (SQLite WAL lets reads go on meanwhile); reads are synchronous and use a
// connection of their own per thread.
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class QThread;

namespace technicaldata {

struct Kline {
  qint64 t = 0;                          // open time, Unix seconds UTC
  double o = 0, h = 0, l = 0, c = 0, v = 0, qv = 0;
  qint64 n = 0;
  double tbv = 0, tbqv = 0;
};

struct SyncState {
  qint64 first = 0, last = 0;            // open times of the first and last stored bars (0 = none)
  qint64 count = 0;
  bool historyDone = false;              // everything from the exchange's first bar to the last closed one is in
  qint64 updated = 0;                    // last write, ms since epoch
  qint64 startReq = 0;                   // the first day the history was asked from (0 = the exchange's first bar)
};

struct Gap { qint64 from = 0, to = 0; qint64 missing = 0; bool checked = false; };   // bars `from` and `to` stored, between them missing

// Indicator values of one bar (TechnicalDataIndicators.h: one value per column of indicatorColumns(), NaN = none)
struct IndRow {
  qint64 t = 0;
  bool warm = false;                     // inside the warm-up: long averages not settled yet
  QVector<double> v;
};
struct IndState { qint64 last = 0; qint64 count = 0; QString version; };

class Store : public QObject {
  Q_OBJECT
public:
  static Store& instance();                       // <data>/technical-data/technical-data.sqlite
  explicit Store(const QString& path, QObject* parent = nullptr);   // tests: another file
  ~Store() override;
  QString path() const { return m_path; }

  // writes, in order on the worker; done(new rows) on the caller's thread (dropped if ctx is gone)
  void add(const QString& market, int tf, const QVector<Kline>& rows, QObject* ctx = nullptr, std::function<void(int)> done = {});
  void setHistoryDone(const QString& market, int tf);
  // finds the holes of a series (on the worker) and records them; done(gaps) on ctx's thread
  void scanGaps(const QString& market, int tf, QObject* ctx, std::function<void(const QVector<Gap>&)> done);
  void setGapChecked(const QString& market, int tf, qint64 from);
  void wait();                                    // until the writes queued so far are done (tests, quit)
  // pairs: spec as TechnicalDataSpec.h's specToJson. Saving deletes what is no longer wanted (a timeframe removed,
  // the bars before a later first day); removing a pair deletes all it holds. pairsChanged(market) when done.
  void setPairSpec(const QString& market, const QJsonObject& spec, QObject* ctx = nullptr, std::function<void()> done = {});
  void removePair(const QString& market, QObject* ctx = nullptr, std::function<void()> done = {});
  // Pause (user's request 2026-11-18): a pair's ("paused" in its spec) or everything's (meta collection_paused), known
  // here at once, before the write, so a collector asking isPaused() never writes after the click; the writer also
  // drops the bars of a paused pair that were still on their way. Indicators of what is stored go on.
  bool isPaused(const QString& market) const;     // the pair, or everything
  bool pairPaused(const QString& market) const;
  bool allPaused() const;
  bool cancelled(const QString& market) const;    // cancelled and not started again since: paused, empty
  // A cancel, a removal or a settings save is queued for the pair and not written yet (a large delete takes seconds):
  // collectors leave it alone meanwhile (isPaused is true), so nothing read before the change is acted on after it.
  bool busy(const QString& market) const;
  // Grows with each of those changes: indicator rows computed from what was there before are dropped by the writer
  int generation(const QString& market) const;
  // fn on ctx's thread once the writes queued so far are done, without blocking (wait() blocks)
  void whenWritten(QObject* ctx, std::function<void()> fn);
  void setPaused(const QString& market, bool paused);
  void setAllPaused(bool paused);
  // Cancel: the pair's collection stops (paused) and all it downloaded is deleted; the pair stays, empty
  void cancelPair(const QString& market, QObject* ctx = nullptr, std::function<void()> done = {});
  void setStartRequested(const QString& market, int tf, qint64 start);
  // A collector fetching the bars before the first stored one (a first day moved earlier) marks the series while it
  // does: each of its pages lands in the past, and the indicators would compute the whole later history again after
  // every page. They wait instead and compute once from the earliest bar added, when backfillDone comes. GUI thread.
  void setBackfilling(const QString& market, int tf, bool on);
  bool backfilling(const QString& market, int tf) const { return m_backfill.contains(market + "|" + QString::number(tf)); }
  void setMeta(const QString& key, const QString& value);   // a collector's notes, e.g. "fx.hours:DUKASCOPY:EURUSD"
  QString meta(const QString& key) const;
  void clearIndicators(const QString& market, int tf);
  // indicators: columns = the value names, in IndRow::v order (the table follows them: other columns → the table is
  // made again, every series to compute again); replace = drop that series' rows first (new definitions)
  // generation: generation(market) when the computation started (-1: no check); older than a change since → dropped
  void addIndicators(const QString& market, int tf, const QStringList& columns, const QVector<IndRow>& rows,
                     const QString& version, bool replace = false, QObject* ctx = nullptr, std::function<void()> done = {},
                     int generation = -1);

  // reads
  SyncState state(const QString& market, int tf) const;
  QVector<Kline> range(const QString& market, int tf, qint64 from, qint64 to, int limit = -1) const;   // t in [from, to]
  QVector<Kline> last(const QString& market, int tf, int n) const;   // the n newest, oldest first
  QVector<Kline> before(const QString& market, int tf, qint64 t, int n) const;   // the n bars before t, oldest first
  QVector<Gap> gaps(const QString& market, int tf) const;
  QStringList markets() const;                    // every market with a row (the pairs, in the order added)
  QStringList pairs() const;                      // the pairs recorded, in the order added
  QJsonObject pairSpec(const QString& market) const;   // empty when not a pair
  IndState indState(const QString& market, int tf) const;
  QStringList indicatorColumns() const;           // the value columns of the table, in order (empty before any)
  QStringList indicatorColumns(const QString& market) const;   // the pair's own, in IndRow::v order
  // the pair's columns (IndRow::v in indicatorColumns(market) order; NaN where none)
  QVector<IndRow> indicators(const QString& market, int tf, qint64 from, qint64 to, int limit = -1) const;
  qint64 fileBytes() const;                       // the database and its WAL

signals:
  // after a write that added or changed bars; from = the earliest bar added (a hole filled is in the past)
  void changed(const QString& market, int tf, qint64 from);
  void indicatorsChanged(const QString& market, int tf);
  void pairsChanged(const QString& market);       // added, changed or removed (its data deleted when it had to be)
  void pausedChanged(const QString& market);      // paused or resumed; "" = everything
  void backfillDone(const QString& market, int tf);   // setBackfilling(…, false) on a series that was marked

private:
  mutable QMutex m_pauseLock;
  QSet<QString> m_paused, m_cancelled;
  QHash<QString, int> m_busy, m_gen;
  QSet<QString> m_backfill;   // market|tf being filled backwards (setBackfilling)
  int beginChange(const QString& market);   // busy + next generation; pausedChanged so the collectors let go now
  void endChange(const QString& market);
  bool m_allPaused = false;
  QString m_path;
  QThread* m_thread = nullptr;
  class Writer* m_writer = nullptr;
};

}  // namespace technicaldata
