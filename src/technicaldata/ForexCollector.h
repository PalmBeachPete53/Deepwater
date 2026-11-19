#pragma once
// Fills technical-data.sqlite with the forex pairs (Dukascopy.h): next to the Binance collector, on its own (Dukascopy
// answers slowly, ~15 s a file: it must not hold the minute updates of the crypto pairs back).
//
// A pair's turn, up to `chunk` files (then the next pair: they all move forward together):
//   1. hours: the monthly hour files, from the pair's first day to the last whole month → 1h, and from them 4h, 1D, 1W
//      (cut at 17:00 New York). A month Dukascopy does not have yet stops here; the days below cover it meanwhile.
//   2. recent days: the minute files of each day after those months, up to yesterday → their hours (1h, 4h, 1D, 1W).
//   3. minute history, when the pair records 1m, 5m or 15m: the minute files from its first day on, in order, up to
//      yesterday (written in order, so their indicators are computed once). About 6 000 files a pair from 2003: days.
// Where it stands is kept in the store's meta table (fx.hours / fx.recent / fx.minutes:<market>, last day done), so
// a run goes on where the last one stopped. A file Dukascopy refuses (503, no answer) is asked again five times over
// about nine minutes; then the turn ends and that pair is tried again 10 minutes later (every pair every 30 minutes). Saturdays are skipped (closed); a day or month Dukascopy has no file for
// is empty when old, not published yet when recent. Days are UTC: Dukascopy publishes a day once it is over.
// Test profiles never start it on their own: DW_TECHNICALDATA_COLLECT=1 does.
#include "technicaldata/Collector.h"
#include <QDate>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace technicaldata {

class ForexCollector : public QObject {
  Q_OBJECT
public:
  static ForexCollector& instance();
  explicit ForexCollector(Store* store, QObject* parent = nullptr);

  QStringList markets() const;   // the store's forex pairs
  void setMarkets(const QStringList& m) { m_markets = m; }   // tests, DW_TECHNICALDATA_ONLY
  void setFetch(Collector::Fetch f) { m_fetch = std::move(f); }
  void setClock(std::function<qint64()> nowMs) { m_now = std::move(nowMs); }
  void setChunk(int files) { m_chunk = files; }
  void setRetryDelays(const QVector<int>& ms, int turnMs = 10 * 60 * 1000) { m_retry = ms; m_retryTurnMs = turnMs; }

  void start();
  void stop();
  void syncNow();   // every pair gets a turn
  bool busy() const { return m_running || !m_queue.isEmpty(); }
  bool started() const { return m_started; }
  QVector<Collector::Progress> progress() const;
  QString status() const { return m_status; }

signals:
  void progressChanged();
  void idle();

private:
  struct File { QUrl url; qint64 start = 0; QDate day; bool month = false; };
  struct Turn;
  void enqueue(const QString& market);
  void next();
  void run(const QString& market);
  void phaseHours(std::shared_ptr<Turn> t);
  void phaseRecent(std::shared_ptr<Turn> t);
  void phaseMinutes(std::shared_ptr<Turn> t);
  void finishTurn(std::shared_ptr<Turn> t, bool more);
  void holdHours(const std::shared_ptr<Turn>& t, bool on);   // Store::setBackfilling on the pair's hour timeframes
  // fetches files in order (net::Http keeps Dukascopy's few at once), hands each answer in order to use() (false: stop
  // here); done(failed) — failed: a file still did not come after the retries
  void fetchOrdered(std::shared_ptr<Turn> t, QVector<File> files, std::function<bool(const File&, int status, const QByteArray&)> use,
                    std::function<void(bool failed)> done);
  void writeHours(std::shared_ptr<Turn> t, const QVector<Kline>& hours, qint64 coveredUntil);
  bool active(const QString& market) const;   // started, a pair, not paused
  void abortTurn();
  QStringList recorded() const;               // the forex pairs, paused or not
  qint64 now() const;
  QDate yesterday() const;

  Store* m_store;
  QStringList m_markets;
  Collector::Fetch m_fetch;
  std::function<qint64()> m_now;
  int m_chunk = 120;
  QVector<int> m_retry{10000, 30000, 60000, 120000, 300000};
  int m_retryTurnMs = 10 * 60 * 1000;
  QStringList m_queue;
  bool m_running = false, m_started = false;
  QString m_current, m_status;
  QHash<QString, QString> m_errors;
  QHash<QString, qint64> m_files;
  QSet<QString> m_scanned;   // markets whose holes were looked for in this run
  QTimer m_timer;
  std::function<void()> m_abort;
  QVector<int> m_phaseTfs;         // the timeframes the running phase writes (its rows show it running)   // ends the files being fetched now (a pause, Deepwater closing)
};

}  // namespace technicaldata
