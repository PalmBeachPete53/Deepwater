#pragma once
// Order flow: the order book of one market, kept live on its own thread, sampled into heatmap columns (step D2).
// One stream per market however many charts use it (DepthStream::acquire, like TradeStream).
//
// Threads and what crosses between them (nothing else is shared):
//   depth thread   the socket (IDepthSocket), the JSON reader, the book (OrderBook + BookSync), the sampler, timers.
//                  Every 100 ms it writes one DepthColumn into the SPSC ring (in place, no allocation, no lock).
//   GUI thread     owns the DepthStream; drains the ring every 100 ms into its own column list; fetches the REST
//                  snapshot when the depth thread asks for one (net::get keeps its pacing and source health) and hands
//                  the body back; shows the status.
//   messages       depth → GUI: an atomic flag (snapshot wanted) and a status ring (fixed-size values);
//                  GUI → depth: a snapshot ring (the REST body). Columns: the column ring. All three rings are the
//                  lock-free SPSC ring (charts/core/Spsc.h: release on publish, acquire on read); the thread starts and
//                  stops through QThread's started / finished signals (thread creation and join order everything else).
// No Qt queued call carries data between the threads: ThreadSanitizer cannot see through Qt's uninstrumented event
// queue, and with only these exchanges it checks every one of them (TSan run of tests/charts/data/depthstream_tests).
// The depth thread never takes a lock and never waits: a full ring makes it drop the column (counted).
//
// Recording (step D3, user's choice 2026-11-18: 1 s on disk, 100 ms on screen): the depth thread hands every column
// on a whole second to a writer thread (a fourth SPSC ring); the writer encodes it (16-bit codes) and appends it to the
// hourly file <data>/charts/depth/<spot|usdm>-<SYMBOL>/<yyyy-MM-ddTHH>.dwhm (~3.8 MB an hour), flushing every 5 s.
// Files older than keepDays (7) are deleted when a stream starts, and every market's folder at start-up with the days
// its recording last asked for (pruneAllRecordings). The GUI keeps two rings of encoded columns:
// live (100 ms, the last 10 minutes) and history (1 s, the last 6 hours: read from the files at start, then fed live).
//
// Bands (user's request 2026-11-20, the live book for live trading, in memory only): on every whole second of a synced
// book the depth thread also reduces the whole local book to Binance's bookDepth bands (charts/core/BookBands.h:
// cumulative depth out to ±0.2 … ±5 % of the mid, and how far the REST snapshot reached) and hands them to the GUI
// through a fifth SPSC ring; the GUI keeps the last 24 hours (bands()). Never written anywhere.
//
// Snapshots (Binance's weights: spot 5000 levels = 250, futures 1000 = 20 of the per-minute budget): at most one every
// 10 s, 20 s, 40 s … 5 min while they keep failing; one older than the stream's first buffered event (Binance's
// REST lagging its stream) is asked again after 3 s, 6 s, 12 s … 5 min. A book that is not synced gives Missing
// columns (holes in the heatmap). Reconnects back off 1 s … 60 s; the socket is renewed at 23 h 50 (Binance cuts at 24 h; a resync follows).
#include "charts/core/BookBands.h"
#include "charts/core/Heatmap.h"
#include "charts/core/HeatmapStore.h"
#include "charts/core/Spsc.h"
#include "charts/data/ExchangeFeed.h"
#include <QHash>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <deque>
#include <memory>
#include <thread>

namespace charts::data {

class DepthWorker;

class DepthStream : public QObject {
  Q_OBJECT
public:
  using Ring = flow::SpscRing<flow::DepthColumn, 64>;
  struct Status {
    FeedState feed = FeedState::Closed;
    bool synced = false;
    int resyncs = 0;           // gaps in the ids or crossed books found (each one asks for a new snapshot)
    int snapshots = 0;         // snapshots taken
    qint64 events = 0;         // diff events received
    qint64 dropped = 0;        // columns the ring had no room for
    int bookLevels = 0;        // bid + ask levels in the local book
    qint64 bestBid = 0, bestAsk = 0;   // ticks
    double applyNs = 0;        // average time to read and apply one diff event (ns), over the last status period
    QString error;
  };

  static std::shared_ptr<DepthStream> acquire(const FlowMarket& m);
  ~DepthStream() override;

  const FlowMarket& market() const { return market_; }
  bool ready() const { return scale_.valid(); }
  const flow::TickScale& ticks() const { return scale_; }
  const Status& status() const { return status_; }
  // Columns (GUI side), encoded: live = every sample (100 ms) of the last 10 minutes; history = one a second over the last
  // 6 hours (the recorded files at start, then the live columns on whole seconds).
  const flow::ColumnRing<flow::DepthRecord>& live() const { return *live_; }
  const flow::ColumnRing<flow::DepthRecord>& history() const { return *history_; }
  // The book reduced to bookDepth's bands, one a second while synced, the last 24 hours, oldest first (memory only).
  const std::deque<flow::BookBands>& bands() const { return bands_; }
  static constexpr size_t kBandsKept = 24 * 3600;
  QString recordDir() const;   // where this market's files go
  qint64 recorded() const { return recorded_.load(std::memory_order_relaxed); }   // records written since start

  // Tests: sampling interval, the shortest pause between snapshots, the reconnect backoff base (ms; 0 = defaults).
  static void setTimingForTests(int sampleMs, int snapshotGapMs, int backoffMs);
  // Recording defaults for new streams: on (default) or off, days kept on disk (7), and the disk interval (1000 ms; a
  // multiple of the sampling).
  static void setRecording(bool on, int keepDays = 7, int intervalMs = 1000);
  // This stream, while it runs (the heatmap's settings): recording on/off and days kept. The stream is shared by every
  // heatmap of the market, so each one registers its own wish under `owner` (dropped when the owner is destroyed); the
  // stream records while any wish says on and keeps the files of the longest wish (audit D3: pane A "30 days" and
  // pane B "7 days" must not delete days 8-30). Turning it on starts the writer if it isn't running; a shorter keep
  // prunes the old files now, but only from a call that records (a heatmap with recording off never deletes files).
  // A call with no owner is one shared anonymous wish: its on/off is the last call's, and its days only grow during
  // the session (two ownerless heatmaps would otherwise take turns lowering it), so a lower value prunes at the next
  // start. The depth thread reads the flag at every sample.
  void record(bool on, int keepDays, QObject* owner = nullptr);
  // Audit G2: pruning ran only for the market whose stream starts, so the folders of markets no longer opened kept
  // every hour recorded (~91 MB a day each). A recording stream saves its days kept in its folder (file
  // "retention-days"); this pass prunes every folder under root with its saved days, or defaultDays for a folder
  // without one (recorded before the file existed: the heatmap's default, 7). Blocking file work: call it off the
  // GUI thread (charts::api::launch runs it on the thread pool at start-up). Returns the files deleted.
  static int pruneAllRecordings(const QString& root, qint64 nowMs, int defaultDays = 7);
  static QString recordingsRoot();                 // <data>/charts/depth
  static int retentionOf(const QString& marketDir);   // the saved days kept, 0 = none saved
  bool recording() const { return recordOn_.load(std::memory_order_relaxed); }
  int keepDays() const { return keepDays_.load(std::memory_order_relaxed); }

signals:
  void becameReady();
  void appended(int n);
  void statusChanged();

public:
  // what crosses between the threads besides the columns (fixed-size values)
  struct StatusMsg {
    FeedState feed = FeedState::Closed;
    bool synced = false;
    int resyncs = 0, snapshots = 0, bookLevels = 0;
    qint64 events = 0, dropped = 0, bestBid = 0, bestAsk = 0;
    double applyNs = 0;
    char error[200] = {};
  };
  struct SnapshotMsg {
    bool ok = false;
    QByteArray body;
    char error[200] = {};
  };

private:
  explicit DepthStream(const FlowMarket& m);
  static void dispose(DepthStream* s);
  void start();
  void drain();
  void fetchSnapshot();          // asked by the depth thread


  FlowMarket market_;
  std::unique_ptr<IExchangeFeed> feed_;
  flow::TickScale scale_;
  Status status_;
  std::unique_ptr<Ring> ring_;                                         // depth → GUI: columns
  std::unique_ptr<flow::SpscRing<StatusMsg, 16>> statusRing_;          // depth → GUI: status, once a second
  std::unique_ptr<flow::SpscRing<SnapshotMsg, 4>> snapshotRing_;       // GUI → depth: snapshot bodies
  std::atomic<bool> wantSnapshot_{false};                              // depth → GUI
  std::unique_ptr<Ring> recRing_;                                      // depth → writer: whole-second columns
  std::unique_ptr<flow::SpscRing<flow::BookBands, 16>> bandRing_;      // depth → GUI: the bands, once a second
  std::deque<flow::BookBands> bands_;                                  // GUI: the last kBandsKept
  std::thread writer_;
  std::atomic<bool> stopWriter_{false};
  std::atomic<qint64> recorded_{0};
  std::atomic<bool> recordOn_;    // GUI → depth thread (read at every sample)
  std::atomic<int> keepDays_;     // GUI → writer (hourly pruning)
  std::unique_ptr<flow::ColumnRing<flow::DepthRecord>> live_, history_;
  struct RecordWish {
    bool on = true;
    int keepDays = 7;
  };
  QHash<QObject*, RecordWish> wishes_;   // GUI thread only; nullptr = the ownerless callers
  void applyWishes(bool mayPrune);
  void saveRetention();
  int savedKeep_ = 0;   // the days last written to the folder's retention file (GUI thread)
  void startWriter();
  void preloadHistory();
  QThread thread_;
  DepthWorker* worker_ = nullptr;
  QTimer drainTimer_;
  bool snapshotInFlight_ = false;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
  friend class DepthWorker;
};

}  // namespace charts::data
