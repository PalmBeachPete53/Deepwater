#pragma once
// Data › Technical data, Binance's public daily archives (data.binance.vision) as part of the collection (user's
// requests 2026-11-20: "ces données de footprint sont des données techniques, elles devront être incorporées dans la
// base technical data et faire partie de la collecte"; then the same for the perpetual's order book and positioning).
// For every Binance pair whose settings ask for them (TechnicalDataSpec.h), from each archive's first day to yesterday:
//   footprint spot / perp   aggTrades → 1-minute footprint (FootprintArchive.h) → fp_days        (PairSpec::footprint)
//   depth                   USD-M bookDepth, ±0.2 … ±5 % every ~30 s (PerpArchive.h) → depth_days (PairSpec::depth)
//   oi                      USD-M metrics: open interest and ratios every 5 min → oi, oi_days    (PairSpec::openInterest)
//   - Newest day first (the recent past is what one looks at first), every kind of a day together, back to the first
//     day; then each new day once Binance publishes it (the morning after, UTC: an archive not out yet is asked again
//     an hour later).
//   - Up to four days on the way at once, at most one of them a footprint day (its zip is 13-100 MB): each its
//     .CHECKSUM, then the zip, checked against the sha256, read by a worker thread and stored. The small depth and
//     metrics files are mostly waiting on the network (≈ 0.7 s a request), hence several at once. Through net::Http
//     (the source allows four requests on the wire, 250 ms apart), no cache.
//   - A day the archives lack (404 on a day older than three days) is recorded as missing (fp_missing / arch_missing)
//     and not asked again; a refusal or a broken file is tried again on the next turn.
//   - Paused, cancelled, settings changed: as the kline collector (Collector.h), the store's pause and pairsChanged.
// Test profiles never start it on their own (DW_TECHNICALDATA_COLLECT=1 does, as for the other collectors).
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataStore.h"
#include <QDate>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace technicaldata {

class ArchiveCollector : public QObject {
  Q_OBJECT
public:
  enum class Kind { FootprintSpot, FootprintPerp, Depth, OpenInterest };
  static QString kindId(Kind k);   // "spot", "perp", "depth", "oi" (the footprint's venues keep their names)

  static ArchiveCollector& instance();
  explicit ArchiveCollector(Store* store, QObject* parent = nullptr);

  QStringList markets() const;   // the store's Binance pairs that ask for an archive
  void setMarkets(const QStringList& m) { m_markets = m; }   // tests, DW_TECHNICALDATA_ONLY
  void setFetch(Collector::Fetch f) { m_fetch = std::move(f); }
  void setClock(std::function<qint64()> nowMs) { m_now = std::move(nowMs); }

  void start();
  void stop();
  void syncNow();
  bool busy() const { return !m_inFlight.isEmpty() || !m_queue.isEmpty(); }
  static constexpr int kInFlight = 4;   // days on the way at once (one footprint day at most)
  bool started() const { return m_started; }

  struct Progress {
    QString market, kind;        // "BINANCE:BTCUSDT", "spot" / "perp" / "depth" / "oi"
    QDate from, to;              // the days wanted (to = the last day published)
    int wanted = 0, stored = 0, missing = 0;
    int unpublished = 0;         // of the last three days, not stored yet: Binance publishes a day the morning after
    QDate first, last;           // stored
    qint64 bytes = 0;
    qint64 rows = 0;             // trades (footprint), snapshots (depth), rows (oi)
    bool running = false, queued = false, paused = false;
    QDate current;               // the day on its way
    QString error;               // the last failure, cleared once a day is stored
  };
  QVector<Progress> progress() const;

signals:
  void progressChanged();
  void idle();

private:
  struct Job { QString market; Kind kind; QDate day; };
  struct Want { Kind kind; QDate from; QSet<qint64> have; };
  QVector<Want> wants(const QString& market) const;   // what the pair asks for, with the days it has (or known missing)
  QDate lastPublished() const;   // yesterday UTC
  QVector<Job> todo(const QString& market) const;
  void next();
  void run(const Job& j);
  // error "" = done (stored, or known missing); stopTurn: the network or the server failed, the next turn goes on;
  // stored: the day was written (the last failure shown is cleared only then)
  void finish(const Job& j, const QString& error, bool stopTurn = false, bool stored = false);
  bool active(const QString& market) const;
  qint64 now() const;

  Store* m_store;
  QStringList m_markets;
  Collector::Fetch m_fetch;
  std::function<qint64()> m_now;
  QList<Job> m_queue;
  QList<Job> m_inFlight;         // on the way now
  bool m_started = false;
  bool inFlight(const QString& market, Kind kind, const QDate& day) const;
  int m_gen = 0;                 // stop() and settings changes: answers and builds of an older generation are dropped
  QTimer m_timer;
  QHash<QString, QString> m_errors;   // market|kind → last failure
  QHash<QString, qint64> m_retryAt;   // market|kind|day → not before (ms): an archive not out yet, a failure
};

}  // namespace technicaldata
