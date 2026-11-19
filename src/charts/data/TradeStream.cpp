#include "charts/data/TradeStream.h"
#include <QDateTime>
#include <QHash>
#include <algorithm>

namespace charts::data {

namespace {
int g_backoffBase = 1000;
int g_renewAfter = (23 * 60 + 50) * 60 * 1000;   // Binance cuts every socket at 24 h
int g_rateLimit = 60000;

QHash<QString, std::weak_ptr<TradeStream>>& registry() {
  static QHash<QString, std::weak_ptr<TradeStream>> r;
  return r;
}
size_t capacityFor(int budget) { return std::max<size_t>(size_t(budget) * 2, 4096); }
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
}  // namespace

void TradeStream::setTimingForTests(int backoffBaseMs, int renewAfterMs, int rateLimitMs) {
  g_backoffBase = backoffBaseMs > 0 ? backoffBaseMs : 1000;
  g_renewAfter = renewAfterMs > 0 ? renewAfterMs : (23 * 60 + 50) * 60 * 1000;
  g_rateLimit = rateLimitMs > 0 ? rateLimitMs : 60000;
}

std::shared_ptr<TradeStream> TradeStream::acquire(const FlowMarket& m, int budget) {
  budget = std::max(budget, 1000);
  auto& reg = registry();
  if (auto s = reg.value(m.key()).lock()) {
    s->raiseBudget(budget);
    return s;
  }
  std::shared_ptr<TradeStream> s(new TradeStream(m, budget), &TradeStream::dispose);
  reg.insert(m.key(), s);
  s->start();
  return s;
}

void TradeStream::dispose(TradeStream* s) {
  *s->alive_ = false;
  s->pumpTimer_.stop();
  s->reopenTimer_.stop();
  s->renewTimer_.stop();
  s->feed_.reset();   // closes the socket now; pending REST answers find alive_ false
  auto& reg = registry();
  if (auto it = reg.find(s->market_.key()); it != reg.end() && it->expired()) reg.erase(it);
  s->deleteLater();
}

TradeStream::TradeStream(const FlowMarket& m, int budget) : market_(m), budget_(budget), feed_(makeExchangeFeed(m)) {
  status_.budget = budget;
  for (QTimer* t : {&pumpTimer_, &reopenTimer_, &renewTimer_}) t->setSingleShot(true);
  connect(&pumpTimer_, &QTimer::timeout, this, &TradeStream::pump);
  connect(&reopenTimer_, &QTimer::timeout, this, &TradeStream::openLive);
  // the planned renewal before Binance's 24 h cut: not a lost connection; the REST fill covers the switch
  connect(&renewTimer_, &QTimer::timeout, this, [this] {
    if (!feed_) return;
    feed_->closeTrades();
    openLive();
  });
  scratch_.reserve(1000);
}

TradeStream::~TradeStream() = default;

void TradeStream::setError(const QString& e) {
  if (status_.error == e) return;
  status_.error = e;
  emit statusChanged();
}

void TradeStream::start() {
  auto alive = alive_;
  feed_->fetchTickSize(market_, [this, alive](const Result<flow::Decimal>& r) {
    if (!*alive) return;
    if (!r.ok()) {
      // no tick size, no price grid: try again later (10 s, 20 s … 5 min)
      setError(r.error->message);
      const int delay = std::min(300000, 10000 << std::min(fails_++, 5));
      QTimer::singleShot(delay, this, [this, alive] { if (*alive) start(); });
      return;
    }
    fails_ = 0;
    scale_ = flow::TickScale(r.value);
    tape_ = std::make_unique<flow::TradeTape>(capacityFor(budget_));
    setError({});
    emit becameReady();
    openLive();
    schedulePump(0);
  });
}

void TradeStream::openLive() {
  if (!feed_) return;
  status_.feed = FeedState::Connecting;
  emit statusChanged();
  feed_->openTrades(market_, this);
}

void TradeStream::reopenLater() {
  const int delay = std::min(60000, g_backoffBase << std::min(fails_, 6));
  ++fails_;
  reopenTimer_.start(delay);
}

void TradeStream::onFeedState(FeedState s, const QString& why) {
  status_.feed = s;
  if (s == FeedState::Open) {
    openedAt_ = nowMs();
    renewTimer_.start(g_renewAfter);
    if (status_.error.startsWith("live:")) status_.error.clear();
  } else if (s == FeedState::Closed) {
    renewTimer_.stop();
    if (openedAt_ && nowMs() - openedAt_ >= 60000) fails_ = 0;   // it had been up for a minute: start the backoff over
    if (openedAt_) ++status_.reconnects;
    openedAt_ = 0;
    status_.error = "live: " + why;
    reopenLater();
  }
  emit statusChanged();
}

flow::Trade TradeStream::toTrade(const flow::AggTrade& a) {
  flow::Trade t;
  t.id = a.id;
  t.timeMs = a.timeMs;
  t.ticks = scale_.ticksOf(a.price);
  t.qty = a.qty.toDouble();
  // Binance: m = the buyer was the maker → the seller was the aggressor
  t.side = a.buyerMaker ? flow::Side::Sell : flow::Side::Buy;
  t.from = flow::SideFrom::Exact;
  return t;
}

void TradeStream::onTrade(const flow::AggTrade& a) {
  if (!tape_) return;
  flow::Trade t = toTrade(a);
  t.side = classifier_.classify(t.ticks, t.side);
  t.from = classifier_.lastFrom();
  ++status_.liveTrades;
  const size_t gapsBefore = tape_->gaps().size();
  if (!tape_->append(t)) return;
  if (classifier_.approximate() && !status_.approximate) {
    status_.approximate = true;
    emit statusChanged();
  }
  emit appended();
  if (tape_->gaps().size() > gapsBefore) schedulePump(0);   // a jump in ids: fill it
}

void TradeStream::schedulePump(int delayMs) {
  if (jobRunning_) return;
  if (pumpTimer_.isActive() && pumpTimer_.remainingTime() <= delayMs) return;
  pumpTimer_.start(delayMs);
}

void TradeStream::raiseBudget(int budget) {
  if (budget <= budget_) return;
  budget_ = budget;
  status_.budget = budget;
  status_.historyDone = false;
  if (tape_ && tape_->capacity() < capacityFor(budget)) {
    // a bigger ring: copy the trades over (the gaps are the id jumps, so they come back the same)
    auto bigger = std::make_unique<flow::TradeTape>(capacityFor(budget));
    std::vector<flow::Trade> all(tape_->size());
    for (size_t i = 0; i < tape_->size(); ++i) all[i] = (*tape_)[i];
    bigger->merge(all.data(), all.size());
    tape_ = std::move(bigger);
    emit historyChanged();
  }
  emit statusChanged();
  schedulePump(0);
}

void TradeStream::pump() {
  if (jobRunning_ || !tape_ || !feed_) return;
  enum { Latest, Gap, History } kind;
  qint64 fromId = -1, gapFrom = -1;
  int limit = 1000;
  const auto& gaps = tape_->gaps();
  if (!latestDone_) {
    kind = Latest;
  } else if (!gaps.empty()) {
    kind = Gap;
    const auto g = gaps.back();   // the newest gap first: recent trades matter most
    // Within the gap, the newest page first, and only as far back as the budget reaches counting the trades already
    // newer than it (audit D9): a laptop asleep 8 h leaves millions of ids; filled oldest-first they were dropped again
    // by the tape while the recent trades came last. What the budget can't hold is accepted as a hole.
    const int64_t newer = int64_t(tape_->size() - tape_->lowerBound(g.to + 1));
    const int64_t keep = int64_t(budget_) - newer;
    if (keep <= 0) {
      tape_->forgetGap(g.from);
      gapNoProgress_ = 0;
      schedulePump(0);
      return;
    }
    fromId = std::max<int64_t>(g.from, g.to - std::min<int64_t>(1000, keep) + 1);
    limit = int(g.to - fromId + 1);
    gapFrom = g.from;
  } else if (!historyEnd_ && int(tape_->size()) < budget_ && tape_->firstId() > 0) {
    kind = History;
    limit = std::min(1000, budget_ - int(tape_->size()));
    fromId = std::max<qint64>(0, tape_->firstId() - limit);
    limit = int(tape_->firstId() - fromId);
  } else {
    if (!status_.historyDone) {
      status_.historyDone = true;
      emit statusChanged();
    }
    return;
  }
  jobRunning_ = true;
  auto alive = alive_;
  const qint64 firstBefore = tape_->firstId();
  feed_->fetchTrades(market_, fromId, limit, [this, alive, kind, fromId, gapFrom, firstBefore](const Result<std::vector<flow::AggTrade>>& r) {
    if (!*alive) return;
    jobRunning_ = false;
    if (!r.ok()) {
      const int st = r.error->status;
      setError("history: " + r.error->message);
      if (st == 400 && kind == History) { historyEnd_ = true; schedulePump(0); return; }   // e.g. older than the exchange keeps
      // a gap the exchange refuses to serve (older than it keeps): asked again forever it blocked the history
      if (st == 400 && kind == Gap) { tape_->forgetGap(gapFrom); gapNoProgress_ = 0; schedulePump(0); return; }
      if (st == 429) { schedulePump(g_rateLimit); return; }
      if (st == 418) { schedulePump(g_rateLimit * 5); return; }
      schedulePump(std::min(60000, 2000 << std::min(restFails_++, 5)));
      return;
    }
    restFails_ = 0;
    if (status_.error.startsWith("history:")) setError({});
    scratch_.clear();
    for (const flow::AggTrade& a : r.value) scratch_.push_back(toTrade(a));
    const size_t room = size_t(std::max(0, budget_ - int(tape_->size())));
    const size_t stored = tape_->merge(scratch_.data(), scratch_.size(), kind == History ? room : size_t(budget_));
    if (kind == Latest) latestDone_ = true;
    if (kind == History && (scratch_.empty() || scratch_.front().id >= firstBefore || stored == 0)) historyEnd_ = true;
    if (kind == History && fromId == 0) historyEnd_ = true;
    if (kind == Gap) {
      // ids the exchange doesn't return (never seen on Binance, whose ids are consecutive): after 3 tries, accept the hole
      gapNoProgress_ = stored ? 0 : gapNoProgress_ + 1;
      if (gapNoProgress_ >= 3) { tape_->forgetGap(gapFrom); gapNoProgress_ = 0; }
    }
    // the pace is read first: a slot of historyChanged that released the stream would leave feed_ null
    const int pace = feed_->historyPaceMs(market_);
    if (stored) emit historyChanged();
    if (!*alive || !feed_) return;
    schedulePump(pace);
  });
}

}  // namespace charts::data
