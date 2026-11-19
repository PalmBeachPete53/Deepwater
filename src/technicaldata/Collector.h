#pragma once
// Fills technical-data.sqlite (TechnicalDataStore.h) from Binance's public klines: for every pair and each of its
// timeframes (TechnicalDataSpec.h, chosen in Data › Technical data), the whole history from the pair's first day (the
// exchange's first bar, BTCUSDT 2017-08-17, or a later day the user chose), every timeframe alike (user's rule,
// 2026-11-18: "l'historique de 1m devrait être traité comme les autres timeframes"), then each new closed bar.
//   - A first day moved earlier: the bars before the first stored one are fetched (start_req says from when it was
//     asked); moved later, a timeframe removed, a pair removed: the store deletes, and the pages still coming are dropped.
//   - At launch (20 s after start) it catches up from the last stored bar; while the app runs it checks every minute
//     (a timeframe is asked again once a newer bar of it has closed). The user's rule: record minute by minute, and
//     catch up on what was missed when Deepwater opens again.
//   - Only closed bars are stored (a bar whose close time has passed): never the bar still forming.
//   - Pages of 1000 bars, forward from the last stored one, one request at a time, through net::Http (paced from the
//     sources registry: Binance 5 requests a second at most), no HTTP cache. api.binance.com first, its public mirror
//     data-api.binance.vision when it doesn't answer.
//   - Largest timeframe first, so the useful history comes in first; 1m last (about 4 900 pages for BTCUSDT).
//   - When a timeframe's history is in, its holes are listed and each is asked once more; what the exchange still
//     lacks (its own outages) stays recorded as a known gap.
// Test profiles (DW_PROFILE) never start it on their own (thousands of requests): DW_TECHNICALDATA_COLLECT=1 does.
#include "technicaldata/TechnicalDataStore.h"
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <functional>

namespace technicaldata {

struct Timeframe { QString name; int seconds; QString binance; };   // "1h", 3600, "1h"
const QVector<Timeframe>& timeframes();                             // 1m 5m 15m 1h 4h 1D 1W
QString binanceSymbol(const QString& market);                       // "BINANCE:BTCUSDT" → "BTCUSDT"

// Binance klines JSON → closed bars (close time before nowMs), and whether the answer had a bar still forming
QVector<Kline> parseKlines(const QByteArray& json, qint64 nowMs, bool* forming = nullptr, QString* error = nullptr);

class Collector : public QObject {
  Q_OBJECT
public:
  static Collector& instance();
  explicit Collector(Store* store, QObject* parent = nullptr);

  QStringList markets() const;   // the store's pairs (BINANCE:BTCUSDT first, the user's test market)
  void setMarkets(const QStringList& m) { m_markets = m; }   // tests

  // the network: tests give a fake one. reply(status, body, error)
  using Reply = std::function<void(int status, const QByteArray& body, const QString& error)>;
  using Fetch = std::function<void(const QUrl& url, QObject* ctx, Reply reply)>;
  void setFetch(Fetch f) { m_fetch = std::move(f); }
  void setClock(std::function<qint64()> nowMs) { m_now = std::move(nowMs); }
  void setPageSize(int n) { m_page = n; }
  void setTimeframes(const QVector<Timeframe>& t) { m_tfs = t; }   // default timeframes(); each pair keeps its own among them

  void start();          // catch up now, then every minute
  void stop();
  void syncNow();        // queue every timeframe that has a newer closed bar to fetch
  bool busy() const { return m_running || !m_queue.isEmpty(); }
  bool started() const { return m_started; }

  struct Progress {
    QString market, tf;
    int seconds = 0;
    SyncState state;
    bool queued = false, running = false;
    bool paused = false;           // the pair's collection, or everything's, is paused
    bool cancelled = false;        // its collection was cancelled (paused, emptied) and not started again
    qint64 pagesDone = 0;
    QString error;                 // the last failure, cleared by the next success
    int gaps = 0, gapsOpen = 0;    // holes found / not asked again yet
  };
  QVector<Progress> progress() const;
  QString status() const { return m_status; }

signals:
  void progressChanged();
  void idle();            // the queue is empty (after a catch-up or a minute's check)

private:
  struct Job { QString market; int tf = 0; bool gaps = false; };
  void enqueue(const QString& market, int tf, bool gaps = false);
  void next();
  void page(const Job& j, qint64 startMs, qint64 endMs, std::function<void(bool more, qint64 lastT)> done, bool mirror = false);
  void runSync(const Job& j);
  void runForward(const Job& j);
  void finish();
  QVector<Timeframe> timeframesOf(const QString& market) const;   // what to collect now: none while paused
  QVector<Timeframe> recordedOf(const QString& market) const;     // the pair's own, paused or not
  void pairChanged(const QString& market);
  qint64 startOf(const QString& market) const;
  bool wanted(const Job& j) const;
  void runGaps(const Job& j);
  QString key(const QString& market, int tf) const { return market + "|" + QString::number(tf); }
  qint64 now() const;

  Store* m_store;
  QStringList m_markets;
  QVector<Timeframe> m_tfs;
  Fetch m_fetch;
  std::function<qint64()> m_now;
  int m_page = 1000;
  QList<Job> m_queue;
  bool m_running = false, m_started = false;
  QTimer m_timer;
  QString m_status;
  QHash<QString, QString> m_errors;
  QHash<QString, qint64> m_pages;
  QHash<QString, QVector<Gap>> m_gaps;
  QString m_current;
};

}  // namespace technicaldata
