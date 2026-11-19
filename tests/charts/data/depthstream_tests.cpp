// Order flow, step D2: the depth stream on its own thread (src/charts/data/DepthStream) against a fake exchange.
// The fake keeps a true order book, sends diff events to the socket (in the depth thread) and answers snapshots (GUI
// thread) — a mutex here is the TEST's, the stream itself shares nothing but its lock-free ring. After the exchange
// pauses, the newest column must equal the exchange's book row by row, whatever happened before (gaps, a dropped
// socket, futures rules).
#include "Check.h"
#include "charts/data/DepthStream.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QMutex>
#include <QTimer>
#include <map>
#include <random>

using namespace charts::data;
using charts::flow::BookLevel;
using charts::flow::DepthColumn;
using charts::flow::DepthDiff;

namespace {

struct FakeExchange {
  QMutex mu;
  std::map<int64_t, double> book[2];
  int64_t id = 5000;
  std::mt19937 rng{7};
  bool paused = false;
  int skipNext = 0;        // events generated but never delivered (a hole in the ids)
  bool dropSocket = false; // the socket closes at its next tick
  int delivered = 0, snapshots = 0, opens = 0;
  int64_t clockOffsetMs = 0;   // the exchange's clock minus the PC's
  bool staleSnapshots = false; // snapshots older than every buffered event (Binance's REST lagging its stream)
  QList<qint64> snapshotTimes; // when each snapshot was asked (ms, monotonic)

  // one event: a few level changes, applied to the true book; empty when paused or skipped
  bool next(DepthDiff& e) {
    QMutexLocker l(&mu);
    if (paused) return false;
    e.bids.clear();
    e.asks.clear();
    e.pu = id;
    e.U = id + 1;
    const int n = 1 + int(rng() % 5);
    for (int k = 0; k < n; ++k) {
      const int s = int(rng() % 2);
      const int64_t t = s == 0 ? 990 - int64_t(rng() % 30) : 1010 + int64_t(rng() % 30);
      const double q = rng() % 4 ? double(1 + rng() % 50) / 4 : 0;
      ++id;
      if (q > 0) book[s][t] = q;
      else book[s].erase(t);
      (s == 0 ? e.bids : e.asks).push_back(BookLevel{t, q});
    }
    e.u = id;
    e.eventMs = QDateTime::currentMSecsSinceEpoch() + clockOffsetMs;   // the exchange clock is "now", like Binance's
    if (skipNext > 0) {
      --skipNext;
      return false;
    }
    ++delivered;
    return true;
  }
  QByteArray snapshot() {
    QMutexLocker l(&mu);
    ++snapshots;
    static QElapsedTimer mono;
    if (!mono.isValid()) mono.start();
    snapshotTimes << mono.elapsed();
    QByteArray j = "{\"lastUpdateId\":" + QByteArray::number(staleSnapshots ? 1 : id) + ",\"bids\":[";
    bool first = true;
    for (auto it = book[0].rbegin(); it != book[0].rend(); ++it) {
      j += (first ? "" : ",") + QByteArray("[\"") + QByteArray::number(it->first) + "\",\"" + QByteArray::number(it->second) + "\"]";
      first = false;
    }
    j += "],\"asks\":[";
    first = true;
    for (const auto& [t, q] : book[1]) {
      j += (first ? "" : ",") + QByteArray("[\"") + QByteArray::number(t) + "\",\"" + QByteArray::number(q) + "\"]";
      first = false;
    }
    return j + "]}";
  }
};
FakeExchange* g_ex = nullptr;

// Lives in the depth thread: a timer pulls events from the exchange.
class FakeDepthSocket : public IDepthSocket {
public:
  void open(const QStringList&, const charts::flow::TickScale&, IOrderBookSink* sink) override {
    close();
    sink_ = sink;
    { QMutexLocker l(&g_ex->mu); ++g_ex->opens; }
    sink_->onDepthState(FeedState::Connecting, {});
    timer_ = std::make_unique<QTimer>();
    QObject::connect(timer_.get(), &QTimer::timeout, [this] {
      bool drop;
      { QMutexLocker l(&g_ex->mu); drop = g_ex->dropSocket; g_ex->dropSocket = false; }
      if (drop) {
        timer_->stop();
        sink_->onDepthState(FeedState::Closed, "test drop");
        return;
      }
      for (int k = 0; k < 3; ++k)
        if (g_ex->next(e_)) sink_->onDepthDiff(e_);
    });
    timer_->start(2);
    sink_->onDepthState(FeedState::Open, {});
  }
  void close() override { timer_.reset(); }

private:
  IOrderBookSink* sink_ = nullptr;
  std::unique_ptr<QTimer> timer_;
  DepthDiff e_;
};

class FakeFeed : public IExchangeFeed {
public:
  void openTrades(const FlowMarket&, ITradeSink*) override {}
  void closeTrades() override {}
  void fetchTrades(const FlowMarket&, qint64, int, TradesCb) override {}
  void fetchTickSize(const FlowMarket&, std::function<void(const Result<charts::flow::Decimal>&)> cb) override {
    QTimer::singleShot(0, [cb] { cb(Result<charts::flow::Decimal>::success(charts::flow::Decimal{1, 0, true})); });
  }
  int historyPaceMs(const FlowMarket&) const override { return 0; }
  void fetchDepthSnapshot(const FlowMarket&, std::function<void(const Result<QByteArray>&)> cb) override {
    const QByteArray body = g_ex->snapshot();
    QTimer::singleShot(5, [cb, body] { cb(Result<QByteArray>::success(body)); });
  }
};

bool spin(const std::function<bool()>& pred, int maxMs = 5000) {
  QElapsedTimer t;
  t.start();
  while (!pred()) {
    if (t.elapsed() > maxMs) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return true;
}

struct Setup {
  FakeExchange ex;
  Setup() {
    g_ex = &ex;
    testing::setFeedFactory([](const FlowMarket&) { return std::unique_ptr<IExchangeFeed>(new FakeFeed); });
    testing::setDepthSocketFactory([] { return std::unique_ptr<IDepthSocket>(new FakeDepthSocket); });
    DepthStream::setTimingForTests(20, 150, 30);
  }
  ~Setup() {
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    testing::setFeedFactory(nullptr);
    testing::setDepthSocketFactory(nullptr);
    DepthStream::setTimingForTests(0, 0, 0);
    g_ex = nullptr;
  }
};

// After the exchange pauses: the newest column, row by row, against the exchange's true book.
bool lastColumnEqualsExchange(DepthStream& s) {
  { QMutexLocker l(&g_ex->mu); g_ex->paused = true; }
  // a few sampling periods for the depth thread to apply the last events and sample a synced column
  const bool ok = spin([&] { return s.status().synced && !s.live().empty() && !(s.live().back().flags & DepthColumn::Missing); });
  if (!ok) return false;
  const int64_t n0 = s.live().end();
  if (!spin([&] { return s.live().end() >= n0 + 3; })) return false;
  const charts::flow::DepthRecord& c = s.live().back();
  float want[DepthColumn::kRows] = {};
  { QMutexLocker l(&g_ex->mu);
    for (int side = 0; side < 2; ++side)
      for (const auto& [t, q] : g_ex->book[side]) {
        const int64_t r = (t - c.baseTicks) / c.rowTicks;
        if (t >= c.baseTicks && r < DepthColumn::kRows) want[r] += float(q);
      }
  }
  // stored as 16-bit codes: within 0.07 % of the true quantity
  for (int r = 0; r < DepthColumn::kRows; ++r)
    if (std::fabs(charts::flow::decodeQty(c.codes[r]) - want[r]) > 7e-4 * want[r] + 1e-6) return false;
  return true;
}

}  // namespace

TEST("depthstream: Binance depth URLs and snapshot limits") {
  const auto spot = *FlowMarket::fromChartSymbol("BINANCE:BTCUSDT");
  const auto fut = *FlowMarket::fromChartSymbol("BINANCE:BTCUSDT.P");
  CHECK_EQ(binanceDepthSocketUrls(spot).value(0), QString("wss://stream.binance.com:9443/ws/btcusdt@depth@100ms"));
  CHECK_EQ(binanceDepthSocketUrls(fut), QStringList{"wss://fstream.binance.com/public/ws/btcusdt@depth@100ms"});
  CHECK_EQ(binanceDepthLimit(spot), 5000);
  CHECK_EQ(binanceDepthLimit(fut), 1000);
}

TEST("depthstream: syncs on its own thread, columns reach the GUI thread, the last one equals the exchange's book") {
  Setup s;
  auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"));
  CHECK(spin([&] { return st->status().synced; }));
  CHECK(spin([&] { return st->live().size() > 20; }));
  CHECK(lastColumnEqualsExchange(*st));
  CHECK_EQ(st->status().resyncs, 0);
  CHECK(spin([&] { return st->status().events > 50; }));   // the status comes once a second
  CHECK_EQ(int(st->status().dropped), 0);
  // columns are on the sampling grid, in order
  bool ordered = true;
  for (int64_t i = st->live().begin() + 1; i < st->live().end(); ++i)
    ordered = ordered && st->live().at(i).timeMs > st->live().at(i - 1).timeMs && st->live().at(i).timeMs % 20 == 0;
  CHECK(ordered);
}

TEST("depthstream: a hole in the update ids → Missing columns, a new snapshot, then a correct book again") {
  Setup s;
  auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"));
  CHECK(spin([&] { return st->status().synced; }));
  const int snaps = s.ex.snapshots;
  { QMutexLocker l(&s.ex.mu); s.ex.skipNext = 1; }
  CHECK(spin([&] { return st->status().resyncs >= 1; }));
  CHECK(spin([&] { return s.ex.snapshots > snaps && st->status().synced; }));
  bool sawMissing = false;
  for (int64_t i = st->live().begin(); i < st->live().end(); ++i) sawMissing = sawMissing || (st->live().at(i).flags & DepthColumn::Missing);
  CHECK(sawMissing);
  CHECK(lastColumnEqualsExchange(*st));
}

TEST("depthstream: a dropped socket reconnects (backoff), resyncs and is correct again") {
  Setup s;
  auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"));
  CHECK(spin([&] { return st->status().synced; }));
  { QMutexLocker l(&s.ex.mu); s.ex.dropSocket = true; }
  CHECK(spin([&] { QMutexLocker l(&s.ex.mu); return s.ex.opens >= 2; }));
  CHECK(spin([&] { return st->status().synced && st->status().feed == FeedState::Open; }));
  CHECK(lastColumnEqualsExchange(*st));
}

TEST("depthstream: recording — a writer thread fills the hourly file; read back byte for byte; the next stream preloads it") {
  Setup s;
  charts::data::DepthStream::setRecording(true, 7, 100);   // a record every 100 ms (5 samples) to keep the test short
  QString dir;
  std::vector<charts::flow::DepthRecord> liveCopy;
  {
    auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:ETHUSDT"));
    dir = st->recordDir();
    CHECK(spin([&] { return st->status().synced; }));
    CHECK(spin([&] { return st->recorded() >= 15; }));
    for (int64_t i = st->live().begin(); i < st->live().end(); ++i)
      if (st->live().at(i).timeMs % 100 == 0) liveCopy.push_back(st->live().at(i));
    CHECK(!liveCopy.empty());
  }   // released: the depth thread stops, then the writer writes what is left and flushes
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  const auto onDisk = charts::flow::loadRange(dir, 0, INT64_MAX / 2);
  CHECK(onDisk.size() >= liveCopy.size());
  // every record the GUI kept on a 100 ms mark is on disk, identical
  size_t found = 0;
  for (const auto& r : liveCopy)
    for (const auto& d : onDisk)
      if (d.timeMs == r.timeMs) { found += d == r; break; }
  CHECK_EQ(int(found), int(liveCopy.size()));
  // a new stream of the same market starts with that history
  auto again = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:ETHUSDT"));
  CHECK(spin([&] { return again->ready(); }));
  CHECK(again->history().size() >= onDisk.size());
  CHECK(again->history().at(again->history().begin()) == onDisk.front());
  again.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  charts::data::DepthStream::setRecording(true, 7, 1000);
}

TEST("depthstream: recording switched on and off while the stream runs") {
  Setup s;
  charts::data::DepthStream::setRecording(false, 7, 100);
  {
    auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:SOLUSDT"));
    QDir(st->recordDir()).removeRecursively();
    CHECK(spin([&] { return st->status().synced && st->live().size() > 20; }));
    CHECK(!st->recording());
    CHECK_EQ(int(st->recorded()), 0);                 // off: nothing reaches the writer (there is none)
    st->record(true, 3);
    CHECK(st->recording());
    CHECK(spin([&] { return st->recorded() >= 5; }));   // on: the writer starts and records come
    st->record(false, 3);
    QElapsedTimer t;
    t.start();
    spin([&] { return t.elapsed() > 400; }, 1000);    // what was already queued drains
    const qint64 n = st->recorded();
    spin([&] { return t.elapsed() > 1200; }, 2000);
    CHECK_EQ(int(st->recorded()), int(n));             // off again: no more
    QDir(st->recordDir()).removeRecursively();
  }
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  charts::data::DepthStream::setRecording(true, 7, 1000);
}

TEST("depthstream: futures (pu chain), and one stream per market however many users") {
  Setup s;
  auto a = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT.P"));
  auto b = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT.P"));
  CHECK(a.get() == b.get());
  CHECK(spin([&] { return a->status().synced; }));
  { QMutexLocker l(&s.ex.mu); s.ex.skipNext = 1; }   // pu ≠ previous u → resync
  CHECK(spin([&] { return a->status().resyncs >= 1 && a->status().synced; }));
  CHECK(lastColumnEqualsExchange(*a));
  a.reset();
  b.reset();   // last user gone: the thread stops (dispose waits for it)
}

// Audit D9: with the PC's clock ahead of the exchange, the columns are on the exchange's clock from the first one (the
// first sample used the local clock and the grid then never went back).
TEST("depthstream: columns follow the exchange's clock even when the PC's is ahead") {
  Setup s;
  s.ex.clockOffsetMs = -5000;
  auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:ADAUSDT"));
  CHECK(spin([&] { return st->status().synced && st->live().size() > 20; }));
  const int64_t lag = QDateTime::currentMSecsSinceEpoch() - st->live().back().timeMs;
  CHECK(lag > 4000 && lag < 6000);
  st.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

// Audit D3: every heatmap of a market shares its stream; each one's wish counts, the longest keep wins, and a heatmap
// with recording off never deletes files.
TEST("depthstream: one heatmap's settings don't delete another's recordings") {
  Setup s;
  charts::data::DepthStream::setRecording(false, 7, 100);
  {
    auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:DOTUSDT"));
    const QString dir = st->recordDir();
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    auto oldFile = [&](int days) { return dir + "/" + charts::flow::HeatmapWriter::fileName(now - int64_t(days) * 86400000); };
    for (int d : {10, 40}) { QFile f(oldFile(d)); (void)f.open(QIODevice::WriteOnly); }
    CHECK(spin([&] { return st->ready(); }));
    QObject paneA, paneB;
    st->record(true, 30, &paneA);   // starts the writer, which prunes to 30 days
    CHECK(!QFile::exists(oldFile(40)));
    CHECK(QFile::exists(oldFile(10)));
    st->record(true, 7, &paneB);    // the other pane keeps less: A's 30 days still count
    CHECK_EQ(st->keepDays(), 30);
    CHECK(QFile::exists(oldFile(10)));
    st->record(false, 1, &paneA);   // recording off: never a prune
    CHECK(st->recording());         // B still records
    CHECK(QFile::exists(oldFile(10)));
    st->record(false, 1, &paneB);
    CHECK(!st->recording());
    CHECK(QFile::exists(oldFile(10)));
    QDir(dir).removeRecursively();
  }
  {
    // a wish goes with its owner
    auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:DOTUSDT"));
    CHECK(spin([&] { return st->ready(); }));
    auto paneA = std::make_unique<QObject>();
    QObject paneB;
    st->record(false, 30, paneA.get());
    st->record(false, 7, &paneB);
    CHECK_EQ(st->keepDays(), 30);
    paneA.reset();
    CHECK_EQ(st->keepDays(), 7);
    QDir(st->recordDir()).removeRecursively();
  }
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  charts::data::DepthStream::setRecording(true, 7, 1000);
}

// Audit G3: a snapshot older than the stream ("too old") was asked again every second, outside the backoff (spot:
// weight 250 each). Now ≥ 3 s doubling (the tests' 150 ms gap scales the base down to 150 ms): the gaps grow.
TEST("depthstream: snapshots older than the stream are asked again with a growing wait") {
  Setup s;
  { QMutexLocker l(&s.ex.mu); s.ex.staleSnapshots = true; }
  auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"));
  CHECK(spin([&] { QMutexLocker l(&s.ex.mu); return s.ex.snapshotTimes.size() >= 4; }, 4000));
  QList<qint64> t;
  { QMutexLocker l(&s.ex.mu); t = s.ex.snapshotTimes; }
  CHECK(!st->status().synced);
  // 150, 300, 600 ms …: each gap well above the one before (a flat 1 s before the fix)
  for (int i = 2; i < std::min<qsizetype>(t.size(), 4); ++i) CHECK(t[i] - t[i - 1] > (t[i - 1] - t[i - 2]) * 3 / 2);
  { QMutexLocker l(&s.ex.mu); s.ex.staleSnapshots = false; }
  CHECK(spin([&] { return st->status().synced; }, 5000));
}

// Audit G2: the recordings of a market whose heatmap is not opened again were never deleted. A recording stream saves
// its days kept in its folder; the start-up pass prunes every folder with them (7 days where none was saved).
TEST("depthstream: start-up pruning of every market's folder with its saved retention") {
  Setup s;
  charts::data::DepthStream::setRecording(false, 7, 100);
  const QString root = QDir::tempPath() + "/dw-depth-prune-test";
  QDir(root).removeRecursively();
  const int64_t now = QDateTime::currentMSecsSinceEpoch();
  auto touch = [&](const QString& market, int days) {
    QDir().mkpath(root + "/" + market);
    const QString f = root + "/" + market + "/" + charts::flow::HeatmapWriter::fileName(now - int64_t(days) * 86400000);
    QFile file(f);
    (void)file.open(QIODevice::WriteOnly);
    return f;
  };
  auto retention = [&](const QString& market, const QByteArray& days) {
    QFile f(root + "/" + market + "/retention-days");
    (void)f.open(QIODevice::WriteOnly);
    f.write(days);
  };
  const QString a10 = touch("spot-AAAUSDT", 10), a40 = touch("spot-AAAUSDT", 40);
  retention("spot-AAAUSDT", "30\n");
  const QString b3 = touch("usdm-BBBUSDT", 3), b10 = touch("usdm-BBBUSDT", 10);   // no retention saved: 7 days
  const QString c0 = touch("spot-CCCUSDT", 0), c3 = touch("spot-CCCUSDT", 3);
  retention("spot-CCCUSDT", "1");
  const QString d10 = touch("spot-DDDUSDT", 10);
  retention("spot-DDDUSDT", "garbage");                                          // unreadable: the default
  CHECK_EQ(DepthStream::pruneAllRecordings(root, now), 4);
  CHECK(QFile::exists(a10));
  CHECK(!QFile::exists(a40));
  CHECK(QFile::exists(b3));
  CHECK(!QFile::exists(b10));
  CHECK(QFile::exists(c0));
  CHECK(!QFile::exists(c3));
  CHECK(!QFile::exists(d10));
  QDir(root).removeRecursively();

  // a recording stream writes its retention (the longest wish) into its folder
  {
    auto st = DepthStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:EOSUSDT"));
    QDir(st->recordDir()).removeRecursively();
    CHECK(spin([&] { return st->ready(); }));
    QObject paneA, paneB;
    st->record(true, 30, &paneA);
    CHECK_EQ(DepthStream::retentionOf(st->recordDir()), 30);
    st->record(true, 12, &paneB);
    CHECK_EQ(DepthStream::retentionOf(st->recordDir()), 30);
    st->record(false, 12, &paneA);   // A stops recording: B's 12 days are what is kept now
    CHECK_EQ(DepthStream::retentionOf(st->recordDir()), 12);
    st->record(false, 12, &paneB);
    QDir(st->recordDir()).removeRecursively();
  }
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  charts::data::DepthStream::setRecording(true, 7, 1000);
}
