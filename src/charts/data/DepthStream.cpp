#include "charts/data/DepthStream.h"
#include "core/Paths.h"
#include "net/Sources.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QHash>
#include <QUrl>
#include <algorithm>
#include <cstring>

namespace charts::data {

namespace {
int g_sampleMs = 100;
int g_snapshotGapMs = 10000;
int g_backoffMs = 1000;
bool g_record = true;
int g_keepDays = 7;
int g_recordMs = 1000;
constexpr size_t kLiveColumns = 10 * 60 * 10;       // 10 min at 100 ms
constexpr size_t kHistoryColumns = 6 * 3600;        // 6 h at 1 s
constexpr int kRenewMs = (23 * 60 + 50) * 60 * 1000;   // Binance cuts every socket at 24 h
constexpr const char* kRetentionFile = "retention-days";   // in each market's folder (see saveRetention)

QHash<QString, std::weak_ptr<DepthStream>>& registry() {
  static QHash<QString, std::weak_ptr<DepthStream>> r;
  return r;
}
}  // namespace

void DepthStream::setRecording(bool on, int keepDays, int intervalMs) {
  g_record = on;
  g_keepDays = std::max(1, keepDays);
  g_recordMs = intervalMs > 0 ? intervalMs : 1000;
}

void DepthStream::record(bool on, int keepDays, QObject* owner) {
  keepDays = std::clamp(keepDays, 1, 3650);
  auto it = wishes_.find(owner);
  if (it == wishes_.end()) {
    it = wishes_.insert(owner, RecordWish{on, keepDays});
    // the owner's wish goes with it (a heatmap removed, or switched to another market)
    if (owner) connect(owner, &QObject::destroyed, this, [this, owner] {
      if (wishes_.remove(owner)) applyWishes(false);
    });
  } else {
    it->on = on;
    // ownerless: several heatmaps may share this wish; letting one lower the days would delete what another keeps
    it->keepDays = owner ? keepDays : std::max(it->keepDays, keepDays);
  }
  applyWishes(on);
}

// any(on) and max(keepDays) over the wishes; with none, the defaults the stream started with stay
void DepthStream::applyWishes(bool mayPrune) {
  if (wishes_.isEmpty()) return;
  bool on = false;
  int keep = 1;
  for (const RecordWish& w : std::as_const(wishes_)) {
    on = on || w.on;
    keep = std::max(keep, w.keepDays);
  }
  const int before = keepDays_.exchange(keep, std::memory_order_relaxed);
  recordOn_.store(on, std::memory_order_relaxed);
  if (!ready()) return;   // start() starts the writer once the tick size is known
  if (on && !writer_.joinable()) startWriter();
  if (on) saveRetention();
  if (mayPrune && on && keep < before) flow::pruneOlderThan(recordDir(), keep, QDateTime::currentMSecsSinceEpoch());
}

// The days kept, next to the files: the start-up pass (pruneAllRecordings) reads it for markets whose stream does not
// run this session. Written only while recording, so it is what the recording heatmaps last asked for.
void DepthStream::saveRetention() {
  const int keep = keepDays_.load(std::memory_order_relaxed);
  if (keep == savedKeep_) return;
  QDir().mkpath(recordDir());
  QSaveFile f(recordDir() + "/" + QLatin1String(kRetentionFile));
  if (f.open(QIODevice::WriteOnly) && f.write(QByteArray::number(keep) + "\n") > 0 && f.commit()) savedKeep_ = keep;
}

int DepthStream::retentionOf(const QString& dir) {
  QFile f(dir + "/" + QLatin1String(kRetentionFile));
  if (!f.open(QIODevice::ReadOnly)) return 0;
  bool ok = false;
  const int d = f.read(16).trimmed().toInt(&ok);
  return ok && d >= 1 ? std::min(d, 3650) : 0;
}

int DepthStream::pruneAllRecordings(const QString& root, qint64 nowMs, int defaultDays) {
  int n = 0;
  for (const QFileInfo& fi : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    const int saved = retentionOf(fi.filePath());
    n += flow::pruneOlderThan(fi.filePath(), saved > 0 ? saved : std::max(1, defaultDays), nowMs);
  }
  return n;
}

QString DepthStream::recordingsRoot() { return paths::data("charts/depth"); }

void DepthStream::setTimingForTests(int sampleMs, int snapshotGapMs, int backoffMs) {
  g_sampleMs = sampleMs > 0 ? sampleMs : 100;
  g_snapshotGapMs = snapshotGapMs > 0 ? snapshotGapMs : 10000;
  g_backoffMs = backoffMs > 0 ? backoffMs : 1000;
}

// ================================================================================================ depth thread

class DepthWorker : public QObject, public IOrderBookSink {
public:
  // Built on the GUI thread before the depth thread starts (thread creation orders these reads before the thread).
  DepthWorker(DepthStream* gui, const QStringList& urls, const flow::TickScale& scale, bool futures)
      : urls_(urls), scale_(scale), ring_(gui->ring_.get()), statusRing_(gui->statusRing_.get()), snapshotRing_(gui->snapshotRing_.get()),
        wantSnapshot_(&gui->wantSnapshot_), recRing_(gui->recRing_.get()), recordOn_(&gui->recordOn_), recordMs_(g_recordMs), sync_(futures ? flow::BookSync::Venue::Futures : flow::BookSync::Venue::Spot, book_) {
    snap_.bids.reserve(5000);
    snap_.asks.reserve(5000);
  }

  // ---- in the depth thread
  void start() {
    socket_ = makeDepthSocket();
    sampleTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&sampleTimer_, &QTimer::timeout, this, [this] { sample(); });
    QObject::connect(&statusTimer_, &QTimer::timeout, this, [this] { pushStatus(); });
    QObject::connect(&reopenTimer_, &QTimer::timeout, this, [this] { openSocket(); });
    QObject::connect(&renewTimer_, &QTimer::timeout, this, [this] { socket_->close(); openSocket(); });
    QObject::connect(&snapshotTimer_, &QTimer::timeout, this, [this] { maybeRequestSnapshot(); });
    reopenTimer_.setSingleShot(true);
    renewTimer_.setSingleShot(true);
    snapshotTimer_.setSingleShot(true);
    sampleTimer_.start(g_sampleMs);
    statusTimer_.start(1000);
    mono_.start();
    openSocket();
  }

  void shutdown() {
    for (QTimer* t : {&sampleTimer_, &statusTimer_, &reopenTimer_, &renewTimer_, &snapshotTimer_}) t->stop();
    if (socket_) socket_->close();
    socket_.reset();
  }

  void onSnapshotBody(const QByteArray& body) {
    snapshotPending_ = false;
    if (!flow::parseDepthSnapshot(QByteArrayView(body), scale_, snap_)) {
      ++snapFails_;
      error_ = "order book snapshot not readable";
      maybeRequestSnapshot();
      return;
    }
    ++snapshots_;
    const auto r = sync_.onSnapshot(snap_.lastUpdateId, snap_.bids, snap_.asks);
    if (r == flow::BookSync::Result::Synced) {
      snapFails_ = 0;
      tooOld_ = 0;
      error_.clear();
    } else if (r == flow::BookSync::Result::SnapshotTooOld) {
      // the stream ran ahead of it: a newer one soon, but backed off too (≥ 3 s, doubling up to 5 min). A flat
      // 1 s re-asked a 5000-level spot snapshot (weight 250) every second for as long as Binance's REST answers
      // lagged its stream (audit G3).
      const qint64 base = std::min(3000, g_snapshotGapMs);   // the tests' shorter gap scales it down
      nextSnapshotAt_ = mono_.elapsed() + std::min<qint64>(300000, base << std::min(tooOld_++, 7));
    } else {
      ++resyncs_;
      ++snapFails_;
    }
    maybeRequestSnapshot();
    pushStatus();
  }

  void onSnapshotFailed(const QString& why) {
    snapshotPending_ = false;
    ++snapFails_;
    error_ = "snapshot: " + why;
    maybeRequestSnapshot();
    pushStatus();
  }

  // ---- IOrderBookSink (the socket calls these in the depth thread)
  void onDepthDiff(const flow::DepthDiff& d) override {
    QElapsedTimer t;
    t.start();
    ++events_;
    if (d.eventMs) {
      lastEventMs_ = d.eventMs;
      lastEventMono_ = mono_.elapsed();
    }
    if (sync_.onEvent(d) == flow::BookSync::Result::Gap) ++resyncs_;
    if (sync_.state() == flow::BookSync::State::Buffering) maybeRequestSnapshot();
    applyNs_ += double(t.nsecsElapsed());
    ++applyCount_;
  }

  void onDepthState(FeedState s, const QString& why) override {
    feed_ = s;
    if (s == FeedState::Open) {
      openedAt_ = mono_.elapsed();
      renewTimer_.start(kRenewMs);
    } else if (s == FeedState::Closed) {
      renewTimer_.stop();
      sync_.reset();   // the buffered events are gone with the socket: a new snapshot after reconnecting
      if (openedAt_ >= 0 && mono_.elapsed() - openedAt_ > 60000) fails_ = 0;
      openedAt_ = -1;
      error_ = "live: " + why;
      reopenTimer_.start(std::min(60000, g_backoffMs << std::min(fails_++, 6)));
    }
    pushStatus();
  }

private:
  void openSocket() {
    sync_.reset();
    tooOld_ = 0;   // a new socket: a new race between the stream and the snapshot
    socket_->open(urls_, scale_, this);
  }

  void maybeRequestSnapshot() {
    if (snapshotPending_ || sync_.state() != flow::BookSync::State::Buffering || sync_.buffered() == 0) return;
    const qint64 now = mono_.elapsed();
    if (now < nextSnapshotAt_) {
      if (!snapshotTimer_.isActive()) snapshotTimer_.start(int(nextSnapshotAt_ - now));
      return;
    }
    snapshotPending_ = true;
    nextSnapshotAt_ = now + std::min<qint64>(300000, qint64(g_snapshotGapMs) << std::min(snapFails_, 5));
    wantSnapshot_->store(true, std::memory_order_release);   // the GUI thread polls it with the column ring
  }

  // exchange clock: the last event time plus the time since it came, on the sampling grid, never going back. Only
  // called once an event time is known: starting from the local clock would stamp every column of the session late
  // by however far the PC's clock is ahead of the exchange (the grid then only moves forward from there; audit D9).
  qint64 exchangeNow() {
    qint64 t = lastEventMs_ + (mono_.elapsed() - lastEventMono_);
    t -= t % g_sampleMs;
    if (t <= lastSample_) t = lastSample_ + g_sampleMs;
    lastSample_ = t;
    return t;
  }

  void sample() {
    // snapshot answers from the GUI thread (polled here: at most one sampling period of delay)
    while (const DepthStream::SnapshotMsg* m = snapshotRing_->front()) {
      if (m->ok) onSnapshotBody(m->body);
      else onSnapshotFailed(QString::fromUtf8(m->error));
      snapshotRing_->popFront();
    }
    if (!lastEventMs_) return;   // no column until the exchange's clock is known (see exchangeNow)
    flow::DepthColumn* c = ring_->beginPush();
    if (!c) {
      ++dropped_;
      return;
    }
    sampler_.sample(book_, sync_.state() == flow::BookSync::State::Synced, exchangeNow(), *c);
    // the disk gets the columns on whole seconds (the sampling grid divides the recording interval)
    if (recordOn_->load(std::memory_order_relaxed) && c->timeMs % recordMs_ == 0 && !recRing_->push(*c)) ++dropped_;
    ring_->commitPush();
  }

  void pushStatus() {
    DepthStream::StatusMsg* m = statusRing_->beginPush();
    if (!m) return;   // the GUI has not read the last ones: this one is dropped, the next comes in a second
    DepthStream::StatusMsg& st = *m;
    st.feed = feed_;
    st.synced = sync_.state() == flow::BookSync::State::Synced;
    st.resyncs = resyncs_;
    st.snapshots = snapshots_;
    st.events = events_;
    st.dropped = dropped_;
    st.bookLevels = int(book_.levels(flow::OrderBook::Bid) + book_.levels(flow::OrderBook::Ask));
    st.bestBid = book_.has(flow::OrderBook::Bid) ? book_.best(flow::OrderBook::Bid) : 0;
    st.bestAsk = book_.has(flow::OrderBook::Ask) ? book_.best(flow::OrderBook::Ask) : 0;
    st.applyNs = applyCount_ ? applyNs_ / double(applyCount_) : 0;
    const QByteArray e = error_.toUtf8().left(int(sizeof(st.error)) - 1);
    std::memcpy(st.error, e.constData(), size_t(e.size()));
    st.error[e.size()] = 0;
    statusRing_->commitPush();
    applyNs_ = 0;
    applyCount_ = 0;
  }

  QStringList urls_;
  flow::TickScale scale_;
  DepthStream::Ring* ring_;                                   // the rings and the flag belong to the DepthStream, which
  flow::SpscRing<DepthStream::StatusMsg, 16>* statusRing_;    // outlives this worker (it joins the thread first)
  flow::SpscRing<DepthStream::SnapshotMsg, 4>* snapshotRing_;
  std::atomic<bool>* wantSnapshot_;
  DepthStream::Ring* recRing_;
  const std::atomic<bool>* recordOn_;                         // whether whole-second columns go to the writer
  int recordMs_;
  flow::OrderBook book_{int64_t(1) << 17};
  flow::BookSync sync_;
  flow::DepthSampler sampler_;
  flow::DepthSnapshot snap_;
  std::unique_ptr<IDepthSocket> socket_;
  // children of the worker, so moveToThread takes them along (a member QTimer without a parent would stay on the GUI
  // thread and refuse to start from the depth thread)
  QTimer sampleTimer_{this}, statusTimer_{this}, reopenTimer_{this}, renewTimer_{this}, snapshotTimer_{this};
  QElapsedTimer mono_;
  FeedState feed_ = FeedState::Closed;
  QString error_;
  bool snapshotPending_ = false;
  qint64 nextSnapshotAt_ = 0, lastEventMs_ = 0, lastEventMono_ = 0, lastSample_ = 0, openedAt_ = -1;
  int snapFails_ = 0, fails_ = 0, resyncs_ = 0, snapshots_ = 0, tooOld_ = 0;   // tooOld_: snapshots older than the stream in a row
  qint64 events_ = 0, dropped_ = 0;
  double applyNs_ = 0;
  qint64 applyCount_ = 0;
};

// ================================================================================================ GUI thread

std::shared_ptr<DepthStream> DepthStream::acquire(const FlowMarket& m) {
  auto& reg = registry();
  if (auto s = reg.value(m.key()).lock()) return s;
  std::shared_ptr<DepthStream> s(new DepthStream(m), &DepthStream::dispose);
  reg.insert(m.key(), s);
  s->start();
  return s;
}

DepthStream::DepthStream(const FlowMarket& m)
    : market_(m), feed_(makeExchangeFeed(m)), ring_(std::make_unique<Ring>()),
      statusRing_(std::make_unique<flow::SpscRing<StatusMsg, 16>>()), snapshotRing_(std::make_unique<flow::SpscRing<SnapshotMsg, 4>>()),
      recRing_(std::make_unique<Ring>()), recordOn_(g_record), keepDays_(g_keepDays),
      live_(std::make_unique<flow::ColumnRing<flow::DepthRecord>>(kLiveColumns)),
      history_(std::make_unique<flow::ColumnRing<flow::DepthRecord>>(kHistoryColumns)) {
  connect(&drainTimer_, &QTimer::timeout, this, &DepthStream::drain);
}

DepthStream::~DepthStream() = default;

void DepthStream::dispose(DepthStream* s) {
  *s->alive_ = false;
  s->drainTimer_.stop();
  if (s->worker_) {
    s->thread_.quit();   // the worker closes its socket in the depth thread (finished, direct connection), then the join
    s->thread_.wait();
    delete s->worker_;   // its thread has stopped: nothing runs it any more
    s->worker_ = nullptr;
  }
  if (s->writer_.joinable()) {   // after the depth thread: nothing more comes into its ring; it writes what is left
    s->stopWriter_.store(true, std::memory_order_release);
    s->writer_.join();
  }
  auto& reg = registry();
  if (auto it = reg.find(s->market_.key()); it != reg.end() && it->expired()) reg.erase(it);
  s->deleteLater();
}

void DepthStream::start() {
  auto alive = alive_;
  feed_->fetchTickSize(market_, [this, alive](const Result<flow::Decimal>& r) {
    if (!*alive) return;
    if (!r.ok()) {
      status_.error = r.error->message;
      emit statusChanged();
      QTimer::singleShot(10000, this, [this, alive] { if (*alive) start(); });
      return;
    }
    scale_ = flow::TickScale(r.value);
    preloadHistory();
    if (recordOn_.load(std::memory_order_relaxed)) startWriter();
    // hosts switched off in the Sources tab are left out here (the settings are read on this thread only)
    QStringList urls;
    for (const QString& u : binanceDepthSocketUrls(market_))
      if (!sources::isHostDisabled(QUrl(u).host())) urls << u;
    worker_ = new DepthWorker(this, urls, scale_, market_.venue == FlowMarket::BinanceUsdm);
    worker_->moveToThread(&thread_);
    thread_.setObjectName("depth " + market_.key());
    // both run in the depth thread: started first thing, finished last thing before it ends
    DepthWorker* w = worker_;
    connect(&thread_, &QThread::started, w, [w] { w->start(); }, Qt::DirectConnection);
    connect(&thread_, &QThread::finished, w, [w] { w->shutdown(); }, Qt::DirectConnection);
    thread_.start();
    drainTimer_.start(g_sampleMs);
    emit becameReady();
  });
}

void DepthStream::drain() {
  while (const StatusMsg* m = statusRing_->front()) {
    status_.feed = m->feed;
    status_.synced = m->synced;
    status_.resyncs = m->resyncs;
    status_.snapshots = m->snapshots;
    status_.events = m->events;
    status_.dropped = m->dropped;
    status_.bookLevels = m->bookLevels;
    status_.bestBid = m->bestBid;
    status_.bestAsk = m->bestAsk;
    status_.applyNs = m->applyNs;
    status_.error = QString::fromUtf8(m->error);
    statusRing_->popFront();
    emit statusChanged();
  }
  if (wantSnapshot_.exchange(false, std::memory_order_acq_rel)) fetchSnapshot();
  int n = 0;
  while (const flow::DepthColumn* c = ring_->front()) {
    flow::DepthRecord& r = live_->push();
    r = flow::DepthRecord::from(*c);
    if (c->timeMs % g_recordMs == 0 && (history_->empty() || history_->back().timeMs < c->timeMs)) history_->push() = r;
    ring_->popFront();
    ++n;
  }
  if (n) emit appended(n);
}

QString DepthStream::recordDir() const {
  return paths::data("charts/depth/" + QString(market_.venue == FlowMarket::BinanceUsdm ? "usdm-" : "spot-") + market_.symbol);
}

// The last 6 hours of this market's files (what earlier sessions recorded), before the live columns come.
void DepthStream::preloadHistory() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (const flow::DepthRecord& r : flow::loadRange(recordDir(), now - qint64(kHistoryColumns) * 1000, now + 3600000, scale_.tick)) history_->push() = r;
}

// The writer thread: whole-second columns from the depth thread → encoded records → the hourly files. It sleeps 200 ms
// between rounds (no lock, no condition variable: the ring is the only thing it shares) and flushes every 5 s.
void DepthStream::startWriter() {
  const QString dir = recordDir();
  saveRetention();
  flow::pruneOlderThan(dir, keepDays_.load(std::memory_order_relaxed), QDateTime::currentMSecsSinceEpoch());
  flow::HeatmapFileInfo info{market_.symbol, uint8_t(market_.venue == FlowMarket::BinanceUsdm ? 1 : 0), scale_.tick, uint32_t(g_recordMs)};
  Ring* ring = recRing_.get();
  std::atomic<bool>* stop = &stopWriter_;
  std::atomic<qint64>* count = &recorded_;
  const std::atomic<int>* keep = &keepDays_;
  writer_ = std::thread([ring, stop, count, keep, dir, info] {
    flow::HeatmapWriter w(dir, info);
    auto lastFlush = std::chrono::steady_clock::now(), lastPrune = lastFlush;
    for (;;) {
      const bool stopping = stop->load(std::memory_order_acquire);
      while (const flow::DepthColumn* c = ring->front()) {
        if (w.append(flow::DepthRecord::from(*c))) count->fetch_add(1, std::memory_order_relaxed);
        ring->popFront();
      }
      const auto now = std::chrono::steady_clock::now();
      if (stopping || now - lastFlush > std::chrono::seconds(5)) {
        w.flush();
        lastFlush = now;
      }
      if (stopping) return;
      if (now - lastPrune > std::chrono::hours(1)) {   // a session left running for days keeps only what it should
        flow::pruneOlderThan(dir, keep->load(std::memory_order_relaxed), QDateTime::currentMSecsSinceEpoch());
        lastPrune = now;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });
}

void DepthStream::fetchSnapshot() {
  if (snapshotInFlight_ || !worker_) return;
  snapshotInFlight_ = true;
  auto alive = alive_;
  feed_->fetchDepthSnapshot(market_, [this, alive](const Result<QByteArray>& r) {
    if (!*alive) return;
    snapshotInFlight_ = false;
    SnapshotMsg* m = snapshotRing_->beginPush();
    if (!m) return;   // never with one request at a time
    m->ok = r.ok();
    m->body = r.ok() ? r.value : QByteArray();
    const QByteArray e = r.ok() ? QByteArray() : r.error->message.toUtf8().left(int(sizeof(m->error)) - 1);
    std::memcpy(m->error, e.constData(), size_t(e.size()));
    m->error[e.size()] = 0;
    snapshotRing_->commitPush();
  });
}

}  // namespace charts::data
