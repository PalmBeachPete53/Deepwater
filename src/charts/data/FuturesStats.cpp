#include "charts/data/FuturesStats.h"
#include "core/Paths.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QLockFile>
#include <algorithm>
#include <filesystem>

namespace charts::data {

namespace {
int g_pollMs = 5 * 60 * 1000;
int g_pageGapMs = 250;
int g_backoffMs = 1000;
int g_flushMs = 5000;   // liquidations to disk, and a lock held elsewhere tried again
std::function<qint64()> g_now;
qint64 now() { return g_now ? g_now() : QDateTime::currentMSecsSinceEpoch(); }
constexpr qint64 kFiveMin = 5 * 60 * 1000;
constexpr qint64 kKeptByBinance = qint64(29.5 * 86400 * 1000);   // under Binance's 30 days, so the first page is accepted
constexpr qint64 kSafeStart = qint64(29 * 86400) * 1000;          // after a refusal (HTTP 400)
constexpr int kPage = 500;
// Real liquidations kept (memory and liquidations.dat): a year is far more than the calibration uses, and BTCUSDT's
// stream adds thousands a day (32 bytes each) for as long as the collector runs. Old ones are dropped once they are a
// day past the limit, so the file is rewritten about once a day, not at every flush.
constexpr qint64 kLiqKeepMs = 365LL * 86400 * 1000;
constexpr qint64 kLiqSlackMs = 86400LL * 1000;

QHash<QString, std::weak_ptr<FuturesStats>>& registry() {
  static QHash<QString, std::weak_ptr<FuturesStats>> r;
  return r;
}
}  // namespace

void FuturesStats::setTimingForTests(int pollMs, int pageGapMs, int backoffMs) {
  g_pollMs = pollMs > 0 ? pollMs : 5 * 60 * 1000;
  g_pageGapMs = pageGapMs > 0 ? pageGapMs : 250;
  g_backoffMs = backoffMs > 0 ? backoffMs : 1000;
  g_flushMs = pollMs > 0 ? pollMs : 5000;
}
void FuturesStats::setClockForTests(std::function<qint64()> nowMs) { g_now = std::move(nowMs); }

std::shared_ptr<FuturesStats> FuturesStats::acquire(const QString& symbol) {
  const QString sym = symbol.toUpper();
  auto& reg = registry();
  if (auto s = reg.value(sym).lock()) return s;
  std::shared_ptr<FuturesStats> s(new FuturesStats(sym), [](FuturesStats* p) {
    *p->alive_ = false;
    p->flushLiquidations();
    if (p->feed_) p->feed_->closeLiquidations();
    auto& r = registry();
    if (auto it = r.find(p->symbol_); it != r.end() && it->expired()) r.erase(it);
    p->deleteLater();
  });
  reg.insert(sym, s);
  s->start();
  return s;
}

FuturesStats::FuturesStats(const QString& symbol)
    : symbol_(symbol), feed_(makeExchangeFeed(FlowMarket{FlowMarket::BinanceUsdm, symbol})) {
  pollTimer_.setSingleShot(true);
  retryTimer_.setSingleShot(true);
  liveTimer_.setSingleShot(true);
  connect(&pollTimer_, &QTimer::timeout, this, [this] { queueCatchUp(); });
  connect(&retryTimer_, &QTimer::timeout, this, [this] { next(); });
  connect(&liveTimer_, &QTimer::timeout, this, [this] { openLive(); });
  connect(&flushTimer_, &QTimer::timeout, this, [this] { flushLiquidations(); });
}

FuturesStats::~FuturesStats() = default;

QString FuturesStats::dir() const { return paths::data("charts/futures/" + symbol_); }

QString FuturesStats::file(int kind) const {
  switch (kind) {
    case Oi: return dir() + "/oi5m.dat";
    case Ratio: return dir() + "/ratio5m.dat";
    case Funding: return dir() + "/funding.dat";
    default: return dir() + "/liquidations.dat";
  }
}

void FuturesStats::start() {
  loadDisk();
  acquireLock();   // just loaded: nothing to merge if it is ours
  queueCatchUp();
  openLive();
  flushTimer_.start(g_flushMs);
}

bool FuturesStats::acquireLock() {
  if (lock_) return true;
  QDir().mkpath(dir());
  auto l = std::make_unique<QLockFile>(dir() + "/.lock");
  l->setStaleLockTime(0);   // stale only when its process is gone (a crash), not after 30 s of a long session
  if (!l->tryLock(0)) return false;
  lock_ = std::move(l);
  return true;
}

void FuturesStats::loadDisk() {
  oi_ = flow::loadRecords<flow::OiPoint>(file(Oi), Oi);
  ratio_ = flow::loadRecords<flow::RatioPoint>(file(Ratio), Ratio);
  funding_ = flow::loadRecords<flow::FundingPoint>(file(Funding), Funding);
  liq_ = flow::loadRecords<flow::LiqEvent>(file(Liquidations), Liquidations);
  liqSaved_ = liq_.size();
  dropOldLiquidations();   // a file left by a version that kept everything (rewritten at the first flush)
  if (!oi_.empty() || !liq_.empty()) emit changed();
}

// The first missing time of a series: after the last stored point, but never before what Binance still keeps.
qint64 FuturesStats::startFor(int kind) const {
  const qint64 floor = now() - kKeptByBinance;
  qint64 last = 0;
  if (kind == Oi && !oi_.empty()) last = oi_.back().timeMs;
  if (kind == Ratio && !ratio_.empty()) last = ratio_.back().timeMs;
  if (kind == Funding && !funding_.empty()) last = funding_.back().timeMs;
  if (kind == Funding) return last ? last + 1 : now() - 365LL * 86400 * 1000;   // funding: a year the first time
  return std::max(last + 1, floor);
}

void FuturesStats::queueCatchUp() {
  for (int k : {Oi, Ratio, Funding})
    if (std::none_of(jobs_.begin(), jobs_.end(), [k](const Job& j) { return j.kind == k; })) jobs_.push_back({k, startFor(k)});
  next();
}

void FuturesStats::next() {
  if (busy_ || retryTimer_.isActive()) return;
  if (jobs_.empty()) {
    if (!status_.caughtUp) {
      status_.caughtUp = true;
      emit statusChanged();
    }
    // the next 5 min point is published a little after the period ends: ask 20 s after it
    const qint64 t = now();
    const qint64 wait = g_pollMs < kFiveMin ? g_pollMs : (kFiveMin - t % kFiveMin) + 20000;
    pollTimer_.start(int(std::clamp<qint64>(wait, 1000, kFiveMin + 20000)));
    return;
  }
  const Job j = jobs_.front();
  QString path;
  if (j.kind == Funding) {
    // with a start, up to 1000 rates forward from it (measured 2026-11-18; without one, only the latest 500)
    path = "/fapi/v1/fundingRate?symbol=" + symbol_ + "&limit=1000&startTime=" + QString::number(j.start);
  } else {
    // a window of 500 periods: with a start alone Binance answers the LATEST 500 (measured 2026-11-18), not those after it
    path = QString(j.kind == Oi ? "/futures/data/openInterestHist" : "/futures/data/globalLongShortAccountRatio") + "?symbol=" + symbol_ +
           "&period=5m&limit=" + QString::number(kPage) + "&startTime=" + QString::number(j.start) + "&endTime=" +
           QString::number(j.start + kPage * kFiveMin - 1);
  }
  busy_ = true;
  ++status_.requests;
  auto alive = alive_;
  const int kind = j.kind;
  const qint64 start = j.start;
  feed_->fetchFuturesData(path, [this, alive, kind, start](const Result<QByteArray>& r) {
    if (!*alive) return;
    busy_ = false;
    onPage(kind, start, r);
  });
}

namespace {
// The whole series into a temporary file, then renamed over the old one: removing the file first and writing after
// (as before) lost the history kept past Binance's 30 days on a crash in between (audit D13). std::filesystem::rename
// replaces the target in one step (rename(2); MoveFileEx with replace on Windows).
template <class T>
bool rewrite(const QString& path, uint32_t kind, std::span<const T> all) {
  const QString tmp = path + ".tmp";
  QFile::remove(tmp);   // a leftover of an earlier crash
  if (!flow::appendRecords<T>(tmp, all, kind)) {
    QFile::remove(tmp);
    return false;
  }
  std::error_code ec;
  std::filesystem::rename(std::filesystem::path(tmp.toStdU16String()), std::filesystem::path(path.toStdU16String()), ec);
  if (ec) QFile::remove(tmp);
  return !ec;
}

// appends the new tail, or rewrites the file when points landed among the stored ones
template <class T>
void save(const QString& path, uint32_t kind, const std::vector<T>& all, size_t before, size_t added, bool tailOnly) {
  if (!added) return;
  if (tailOnly) flow::appendRecords<T>(path, std::span<const T>(all.data() + before, all.size() - before), kind);
  else rewrite<T>(path, kind, std::span<const T>(all));
}
}  // namespace

void FuturesStats::onPage(int kind, qint64 start, const Result<QByteArray>& r) {
  if (!r.ok()) {
    ++status_.errors;
    status_.error = r.error->message;
    Job& j = jobs_.front();
    if (r.error->status == 400 && kind != Funding && !j.moved && start < now() - kSafeStart) {
      // Binance's 30 days counted differently from ours: start later, once. Without the flag a symbol Binance always
      // refuses looped here back to back (now() moves on, so the test stayed true) until a 429, then a 418 ban.
      j.start = now() - kSafeStart;
      j.moved = true;
      next();
    } else {
      qint64 wait = std::min<qint64>(300000, qint64(g_backoffMs) << std::min(fails_++, 8));
      // 429 = slow down, 418 = this IP is banned (2 minutes to days): asking again within seconds only lengthens it
      if (r.error->status == 429) wait = std::max<qint64>(wait, qint64(g_backoffMs) * 60);
      if (r.error->status == 418) wait = std::max<qint64>(wait, qint64(g_backoffMs) * 600);
      retryTimer_.start(int(wait));
    }
    emit statusChanged();
    return;
  }
  fails_ = 0;
  if (!status_.error.isEmpty()) {
    status_.error.clear();
    emit statusChanged();
  }
  size_t got = 0, added = 0;
  qint64 last = 0;
  bool ok = false;
  auto take = [&](auto& series, auto& page, const QString& path) {
    const size_t before = series.size();
    const bool tail = page.empty() || series.empty() || page.front().timeMs > series.back().timeMs;
    added = flow::mergeByTime(series, page);
    // without the lock the points stay in memory; takeOverFiles writes them if the other Deepwater quits
    if (lock_) save(path, uint32_t(kind), series, before, added, tail);
    got = page.size();
    if (!page.empty()) last = page.back().timeMs;
  };
  if (kind == Oi) {
    std::vector<flow::OiPoint> page;
    if ((ok = flow::parseOpenInterestHist(r.value, page))) take(oi_, page, file(Oi));
  } else if (kind == Ratio) {
    std::vector<flow::RatioPoint> page;
    if ((ok = flow::parseLongShortRatio(r.value, page))) take(ratio_, page, file(Ratio));
  } else {
    std::vector<flow::FundingPoint> page;
    if ((ok = flow::parseFundingRates(r.value, page))) take(funding_, page, file(Funding));
  }
  if (!ok) {
    ++status_.errors;
    status_.error = "Binance sent statistics we can't read";
    jobs_.pop_front();   // not retried in a loop: the next poll asks again
    emit statusChanged();
    QTimer::singleShot(g_pageGapMs, this, [this, alive = alive_] { if (*alive) next(); });
    return;
  }
  if (added) emit changed();
  // the next window while this one ended before now (periods without trades have no point, so a short page isn't the
  // end); funding: the next page after a full one
  if (kind == Funding) {
    if (int(got) >= 1000 && last + 1 > start) jobs_.front().start = last + 1;
    else jobs_.pop_front();
  } else {
    const qint64 windowEnd = start + kPage * kFiveMin - 1;
    if (windowEnd < now()) jobs_.front().start = windowEnd + 1;
    else jobs_.pop_front();
  }
  Q_UNUSED(last);
  QTimer::singleShot(g_pageGapMs, this, [this, alive = alive_] { if (*alive) next(); });
}

// ---- live liquidations

void FuturesStats::openLive() { feed_->openLiquidations(symbol_, this); }

void FuturesStats::onLiquidation(const flow::LiqEvent& e) {
  if (!liq_.empty() && e.timeMs < liq_.back().timeMs) {
    // out of order (never seen; kept sorted anyway)
    auto it = std::upper_bound(liq_.begin(), liq_.end(), e.timeMs, [](qint64 t, const flow::LiqEvent& x) { return t < x.timeMs; });
    liq_.insert(it, e);
    liqSaved_ = 0;   // rewrite on the next flush
  } else {
    liq_.push_back(e);
  }
  emit changed();
}

void FuturesStats::onLiquidationState(FeedState s, const QString& why) {
  status_.live = s;
  if (s == FeedState::Open) liveFails_ = 0;
  if (s == FeedState::Closed && *alive_) {
    if (!why.isEmpty()) status_.error = "liquidations: " + why;
    liveTimer_.start(int(std::min<qint64>(60000, qint64(g_backoffMs) << std::min(liveFails_++, 6))));
  }
  emit statusChanged();
}

// Drops the liquidations older than kLiqKeepMs once the oldest is a day past it; the next flush rewrites the file.
void FuturesStats::dropOldLiquidations() {
  const qint64 cut = now() - kLiqKeepMs;
  if (liq_.empty() || liq_.front().timeMs >= cut - kLiqSlackMs) return;
  const auto keep = std::lower_bound(liq_.begin(), liq_.end(), cut, [](const flow::LiqEvent& x, qint64 t) { return x.timeMs < t; });
  liq_.erase(liq_.begin(), keep);
  liqSaved_ = 0;
  liqRewrite_ = true;   // the file is rewritten even when nothing is left (an append would keep the old ones)
}

void FuturesStats::flushLiquidations() {
  dropOldLiquidations();
  if (!lock_) {
    if (!acquireLock()) return;   // still written by another Deepwater
    takeOverFiles();
  }
  if (liqSaved_ == liq_.size() && !liqRewrite_) return;
  const bool ok = (liqSaved_ == 0 || liqRewrite_) && QFile::exists(file(Liquidations))
                      ? rewrite<flow::LiqEvent>(file(Liquidations), Liquidations, std::span<const flow::LiqEvent>(liq_))
                      : flow::appendRecords<flow::LiqEvent>(file(Liquidations),
                                                            std::span<const flow::LiqEvent>(liq_.data() + liqSaved_, liq_.size() - liqSaved_),
                                                            Liquidations);
  if (ok) {   // a failed write is tried again at the next flush
    liqSaved_ = liq_.size();
    liqRewrite_ = false;
  }
}

// The other Deepwater wrote this folder until it quit: what it stored after this one loaded the files is merged with
// what this one gathered meanwhile (the same points from Binance, ours win at equal times; liquidations deduplicated),
// and every file is rewritten whole, so nothing is appended twice.
void FuturesStats::takeOverFiles() {
  size_t gained = 0;
  auto mergeSeries = [&](auto& series, int kind) {
    using T = typename std::decay_t<decltype(series)>::value_type;
    std::vector<T> merged = flow::loadRecords<T>(file(kind), uint32_t(kind));
    const size_t onDisk = merged.size();
    flow::mergeByTime(merged, series);
    if (merged.size() > series.size()) gained += merged.size() - series.size();
    series = std::move(merged);
    if (!series.empty() || onDisk) rewrite<T>(file(kind), uint32_t(kind), std::span<const T>(series));
  };
  mergeSeries(oi_, Oi);
  mergeSeries(ratio_, Ratio);
  mergeSeries(funding_, Funding);
  std::vector<flow::LiqEvent> liq = flow::loadRecords<flow::LiqEvent>(file(Liquidations), Liquidations);
  const size_t before = liq_.size();
  liq.insert(liq.end(), liq_.begin(), liq_.end());
  std::stable_sort(liq.begin(), liq.end(), [](const flow::LiqEvent& a, const flow::LiqEvent& b) { return a.timeMs < b.timeMs; });
  auto same = [](const flow::LiqEvent& a, const flow::LiqEvent& b) {
    return a.timeMs == b.timeMs && a.price == b.price && a.qty == b.qty && a.longLiquidated == b.longLiquidated;
  };
  liq.erase(std::unique(liq.begin(), liq.end(), same), liq.end());
  if (liq.size() > before) gained += liq.size() - before;
  liq_ = std::move(liq);
  liqSaved_ = 0;
  liqRewrite_ = true;   // flushLiquidations, right after, writes them whole
  dropOldLiquidations();
  if (gained) emit changed();
}

}  // namespace charts::data
