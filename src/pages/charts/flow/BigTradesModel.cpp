#include "pages/charts/flow/BigTradesModel.h"
#include <QDateTime>
#include <QtConcurrent>
#include <algorithm>

namespace charts::ui::flow {

using ::charts::flow::AggregatedTrade;
using ::charts::flow::Trade;

bool BigTradesModel::Config::operator==(const Config& o) const {
  return agg.windowMs == o.agg.windowMs && agg.sweep == o.agg.sweep && det.mode == o.det.mode && det.minVolume == o.det.minVolume &&
         det.maxVolume == o.det.maxVolume && det.percentile == o.det.percentile && det.windowMs == o.det.windowMs &&
         det.volatility == o.det.volatility && det.minSamples == o.det.minSamples;
}

BigTradesModel::BigTradesModel(const data::FlowMarket& market, QObject* parent)
    : QObject(parent), market_(market), stream_(data::TradeStream::acquire(market)) {
  rebuildTimer_.setSingleShot(true);
  notifyTimer_.setSingleShot(true);
  connect(&rebuildTimer_, &QTimer::timeout, this, &BigTradesModel::startRebuild);
  connect(&notifyTimer_, &QTimer::timeout, this, &BigTradesModel::changed);
  connect(&flushTimer_, &QTimer::timeout, this, &BigTradesModel::tick);
  flushTimer_.start(100);
  connect(&watcher_, &QFutureWatcher<Snapshot>::finished, this, [this] {
    const int gen = watcher_.property("gen").toInt();
    // result() rethrows what the worker threw (only bad_alloc is possible): on the GUI thread that would end the app
    bool ok = true;
    Snapshot s;
    try { s = watcher_.result(); } catch (const std::exception&) { ok = false; }
    if (ok && gen == gen_) adopt(std::move(s));
    if (rebuildAgain_) {
      rebuildAgain_ = false;
      startRebuild();
    }
  });
  connect(stream_.get(), &data::TradeStream::appended, this, &BigTradesModel::addNew);
  // history pages come every 0.3–1 s while the history loads: recompute at most every 2 s
  connect(stream_.get(), &data::TradeStream::historyChanged, this, [this] {
    if (!rebuildTimer_.isActive()) rebuildTimer_.start(2000);
  });
  connect(stream_.get(), &data::TradeStream::becameReady, this, [this] { rebuildTimer_.start(0); });
  connect(stream_.get(), &data::TradeStream::statusChanged, this, &BigTradesModel::notify);
}

BigTradesModel::~BigTradesModel() {
  // no waitForFinished(): removing or hiding the indicator would block the GUI for the whole history run. compute() is
  // static and works on its own copies (tape, settings), so the run may finish after this object is gone: QFuture's
  // shared state holds its result and the watcher (disconnected first) is destroyed without waiting.
  ++gen_;
  watcher_.disconnect(this);
}

void BigTradesModel::setConfig(const Config& c) {
  if (configured_ && c == cfg_) return;
  cfg_ = c;
  configured_ = true;
  rebuildTimer_.start(0);
}

qint64 BigTradesModel::coveredFromMs() const {
  return stream_->ready() && !stream_->tape().empty() ? stream_->tape().front().timeMs : 0;
}

std::pair<size_t, size_t> BigTradesModel::range(int64_t fromMs, int64_t toMs) const {
  auto lo = std::lower_bound(bubbles_.begin(), bubbles_.end(), fromMs, [](const Bubble& b, int64_t t) { return b.a.startMs < t; });
  auto hi = std::lower_bound(lo, bubbles_.end(), toMs, [](const Bubble& b, int64_t t) { return b.a.startMs < t; });
  return {size_t(lo - bubbles_.begin()), size_t(hi - bubbles_.begin())};
}

BigTradesModel::Snapshot BigTradesModel::compute(std::vector<Trade> trades, Config cfg) {
  Snapshot s{::charts::flow::Aggregator(cfg.agg), ::charts::flow::Detector(cfg.det), {}, -1, 0};
  for (const Trade& t : trades)
    if (auto c = s.agg.add(t)) {
      const auto v = s.det.judge(*c);
      if (v.big) s.bubbles.push_back({*c, v.threshold});
    }
  if (!trades.empty()) {
    s.lastId = trades.back().id;
    s.lastTradeMs = trades.back().timeMs;
  }
  return s;
}

void BigTradesModel::startRebuild() {
  if (!configured_ || !stream_->ready()) return;
  if (watcher_.isRunning()) {   // one at a time; the latest settings and tape win
    rebuildAgain_ = true;
    return;
  }
  const auto& tape = stream_->tape();
  std::vector<Trade> copy(tape.size());
  for (size_t i = 0; i < tape.size(); ++i) copy[i] = tape[i];
  watcher_.setProperty("gen", ++gen_);
  watcher_.setFuture(QtConcurrent::run(&BigTradesModel::compute, std::move(copy), cfg_));
}

void BigTradesModel::adopt(Snapshot s) {
  agg_ = std::move(s.agg);
  det_ = std::move(s.det);
  bubbles_ = std::move(s.bubbles);
  lastId_ = s.lastId;
  if (s.lastTradeMs) {
    lastTradeMs_ = s.lastTradeMs;
    lastTradeWall_ = QDateTime::currentMSecsSinceEpoch();
  }
  addNew();   // trades that arrived while the worker ran
  emit changed();
}

void BigTradesModel::judge(const AggregatedTrade& a) {
  const auto v = det_.judge(a);
  if (!v.big) return;
  // live orders arrive in time order; keep the vector sorted anyway
  Bubble b{a, v.threshold};
  // live only (judge() runs for new trades; the history is computed elsewhere), and only orders from the last minute
  if (alertFactor_ > 0 && ::charts::flow::bigTradeAlert(a.volume, v.threshold, alertFactor_) && lastTradeMs_ - a.endMs < 60000) {
    ++alertsFired_;
    emit alert(a, v.threshold);
  }
  if (bubbles_.empty() || bubbles_.back().a.startMs <= a.startMs) bubbles_.push_back(b);
  else bubbles_.insert(std::upper_bound(bubbles_.begin(), bubbles_.end(), a.startMs,
                                        [](int64_t t, const Bubble& x) { return t < x.a.startMs; }), b);
  notify();
}

void BigTradesModel::addNew() {
  if (!configured_ || !stream_->ready() || watcher_.isRunning()) return;
  const auto& tape = stream_->tape();
  if (lastId_ < 0) {
    // the last rebuild saw an empty tape (the REST history never landed): live trades alone must start the history,
    // or Big Trades stay empty for good (as FootprintFeed::addNew)
    if (!tape.empty() && !rebuildTimer_.isActive()) rebuildTimer_.start(0);
    return;
  }
  for (size_t i = tape.lowerBound(lastId_ + 1); i < tape.size(); ++i) {
    const Trade& t = tape[i];
    if (auto c = agg_.add(t)) judge(*c);
    lastId_ = t.id;
    lastTradeMs_ = t.timeMs;
    lastTradeWall_ = QDateTime::currentMSecsSinceEpoch();
  }
  // The tape drops its oldest trades past its budget; bubbles from before its first trade would stay for good (a rebuild
  // from the tape would not make them). Dropped in batches (an eighth of them, or 64) so erasing the front stays rare.
  if (!bubbles_.empty() && !tape.empty() && bubbles_.front().a.startMs < tape.front().timeMs) {
    const auto keep = std::lower_bound(bubbles_.begin(), bubbles_.end(), tape.front().timeMs,
                                       [](const Bubble& b, int64_t t) { return b.a.startMs < t; });
    const size_t stale = size_t(keep - bubbles_.begin());
    if (stale >= 64 || stale * 8 >= bubbles_.size()) {
      bubbles_.erase(bubbles_.begin(), keep);
      notify();
    }
  }
}

void BigTradesModel::tick() {
  if (!configured_ || watcher_.isRunning() || !agg_.hasOpen() || !lastTradeWall_) return;
  // the exchange's clock now: the last trade's time plus what elapsed since it came (no local clock skew)
  const int64_t now = lastTradeMs_ + (QDateTime::currentMSecsSinceEpoch() - lastTradeWall_);
  if (auto c = agg_.flush(now)) judge(*c);
}

::charts::flow::Detector::Verdict BigTradesModel::current() {
  const int64_t now = lastTradeWall_ ? lastTradeMs_ + (QDateTime::currentMSecsSinceEpoch() - lastTradeWall_) : 0;
  return det_.current(now);
}

void BigTradesModel::notify() {
  if (!notifyTimer_.isActive()) notifyTimer_.start(100);
}

}  // namespace charts::ui::flow
