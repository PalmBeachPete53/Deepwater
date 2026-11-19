#include "charts/core/Trades.h"
#include <algorithm>

namespace charts::flow {

// ------------------------------------------------------------------------------------------------ SideClassifier

Side SideClassifier::classify(int64_t ticks, Side exact, const Quote* quote) {
  Side s = Side::Unknown;
  if (exact != Side::Unknown) {
    s = exact;
    lastFrom_ = SideFrom::Exact;
  } else {
    if (quote && quote->ask > quote->bid) {
      lastFrom_ = SideFrom::Quote;
      if (ticks >= quote->ask) s = Side::Buy;
      else if (ticks <= quote->bid) s = Side::Sell;
      else {
        // inside the spread: compare with the mid without dividing (2·price vs bid + ask)
        const int64_t twice = 2 * ticks, sum = quote->bid + quote->ask;
        if (twice > sum) s = Side::Buy;
        else if (twice < sum) s = Side::Sell;
      }
    }
    if (s == Side::Unknown) {
      lastFrom_ = SideFrom::TickRule;
      if (have_) s = ticks > lastTicks_ ? Side::Buy : ticks < lastTicks_ ? Side::Sell : lastSide_;
    }
    approximate_ = true;
  }
  have_ = true;
  lastTicks_ = ticks;
  lastSide_ = s;
  return s;
}

// ------------------------------------------------------------------------------------------------ TradeTape

TradeTape::TradeTape(size_t capacity) : buf_(std::max<size_t>(capacity, 1)) { gaps_.reserve(64); }

int64_t TradeTape::missing() const {
  int64_t n = 0;
  for (const Gap& g : gaps_) n += g.to - g.from + 1;
  return n;
}

bool TradeTape::forgetGap(int64_t from) {
  for (auto it = gaps_.begin(); it != gaps_.end(); ++it)
    if (it->from == from) { gaps_.erase(it); return true; }
  return false;
}

size_t TradeTape::lowerBound(int64_t id) const {
  size_t lo = 0, hi = size_;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if ((*this)[mid].id < id) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

size_t TradeTape::lowerBoundTime(int64_t ms) const {
  size_t lo = 0, hi = size_;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if ((*this)[mid].timeMs < ms) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

void TradeTape::dropFront() {
  head_ = (head_ + 1) % buf_.size();
  --size_;
  ++dropped_;
  ++generation_;
  const int64_t first = firstId();
  // gaps now before the first trade are not missing any more: they are outside the tape
  size_t k = 0;
  while (k < gaps_.size() && (size_ == 0 || gaps_[k].to < first)) ++k;
  if (k) gaps_.erase(gaps_.begin(), gaps_.begin() + ptrdiff_t(k));
}

void TradeTape::addGap(int64_t from, int64_t to) {
  if (from > to) return;
  auto it = std::lower_bound(gaps_.begin(), gaps_.end(), from, [](const Gap& g, int64_t v) { return g.from < v; });
  gaps_.insert(it, Gap{from, to});
}

void TradeTape::removeId(int64_t id) {
  auto it = std::upper_bound(gaps_.begin(), gaps_.end(), id, [](int64_t v, const Gap& g) { return v < g.from; });
  if (it == gaps_.begin()) return;
  --it;
  if (id > it->to) return;
  if (it->from == it->to) gaps_.erase(it);
  else if (id == it->from) ++it->from;
  else if (id == it->to) --it->to;
  else {
    const Gap tail{id + 1, it->to};
    it->to = id - 1;
    gaps_.insert(it + 1, tail);
  }
}

bool TradeTape::append(const Trade& t) {
  if (size_ && t.id <= lastId()) return false;
  if (full()) dropFront();
  if (size_ && t.id > lastId() + 1) addGap(lastId() + 1, t.id - 1);
  buf_[(head_ + size_) % buf_.size()] = t;
  ++size_;
  return true;
}

size_t TradeTape::merge(const Trade* t, size_t n, size_t maxOlder) {
  if (!n) return 0;
  if (!size_) {
    // empty tape: the newest trades that fit and that the budget allows
    const size_t take = std::min({n, buf_.size(), maxOlder});
    size_t stored = 0;
    for (size_t k = n - take; k < n; ++k)
      if (append(t[k])) ++stored;
    return stored;
  }
  size_t stored = 0;
  // 1. older than the first trade: from the newest of them backwards, while there is room and budget
  size_t older = 0;
  const int64_t first0 = firstId();
  while (older < n && t[older].id < first0) ++older;
  if (older) {
    size_t keep = std::min({older, buf_.size() - size_, maxOlder});
    if (keep) {
      int64_t next = first0;   // id of the trade that will follow the inserted one
      for (size_t k = older; k-- > older - keep;) {
        if (t[k].id >= next) continue;   // duplicate inside the page
        if (t[k].id < next - 1) addGap(t[k].id + 1, next - 1);
        head_ = (head_ + buf_.size() - 1) % buf_.size();
        buf_[head_] = t[k];
        ++size_;
        ++stored;
        next = t[k].id;
      }
      if (stored) ++generation_;
    }
  }
  // 2. inside the tape: only ids of a gap, inserted by runs that go to the same place
  size_t k = older;
  const int64_t last0 = lastId();
  bool inserted = false;
  while (k < n && t[k].id <= last0) {
    const int64_t id = t[k].id;
    const auto g = std::upper_bound(gaps_.begin(), gaps_.end(), id, [](int64_t v, const Gap& x) { return v < x.from; });
    const bool unknown = g != gaps_.begin() && id <= (g - 1)->to;
    if (!unknown) { ++k; continue; }
    size_t pos = lowerBound(id);
    size_t runEnd = k + 1;
    while (runEnd < n && t[runEnd].id <= last0 && t[runEnd].id > t[runEnd - 1].id && lowerBound(t[runEnd].id) == pos) {
      const int64_t rid = t[runEnd].id;
      const auto rg = std::upper_bound(gaps_.begin(), gaps_.end(), rid, [](int64_t v, const Gap& x) { return v < x.from; });
      if (rg == gaps_.begin() || rid > (rg - 1)->to) break;
      ++runEnd;
    }
    // a run longer than the whole tape keeps its newest trades: otherwise size_ grew past the capacity
    if (runEnd - k > buf_.size()) k = runEnd - buf_.size();
    const size_t m = runEnd - k;
    // room: the oldest trades make way (a full tape keeps its newest trades)
    while (size_ + m > buf_.size() && size_ > 0) dropFront();
    if (firstId() > t[k].id) { k = runEnd; continue; }   // the gap itself fell off the front
    pos = lowerBound(id);
    // shift [pos, size) right by m, from the end
    for (size_t j = size_; j-- > pos;) at(j + m) = at(j);
    for (size_t j = 0; j < m; ++j) {
      at(pos + j) = t[k + j];
      removeId(t[k + j].id);
    }
    size_ += m;
    stored += m;
    inserted = true;
    k = runEnd;
  }
  if (inserted) ++generation_;
  // 3. newer than the last trade
  for (; k < n; ++k)
    if (append(t[k])) ++stored;
  return stored;
}

}  // namespace charts::flow
