#include "charts/core/OrderBook.h"
#include <algorithm>

namespace charts::flow {

// ------------------------------------------------------------------------------------------------ OrderBook

OrderBook::OrderBook(int64_t windowTicks, size_t farReserve) : n_(std::max<int64_t>(windowTicks, 64)) {
  for (SideBook& b : side_) {
    b.dense.assign(size_t(n_), 0.0);
    b.far.reserve(farReserve);
  }
}

void OrderBook::clear() {
  for (SideBook& b : side_) {
    std::fill(b.dense.begin(), b.dense.end(), 0.0);
    b.far.clear();
    b.count = 0;
    b.best = 0;
  }
}

std::vector<BookLevel>::const_iterator OrderBook::lowerFar(const SideBook& b, int64_t t) {
  return std::lower_bound(b.far.begin(), b.far.end(), t, [](const BookLevel& l, int64_t v) { return l.ticks < v; });
}

double OrderBook::qty(Side s, int64_t t) const {
  const SideBook& b = side_[s];
  if (inWindow(t)) return b.dense[size_t(t - base_)];
  const auto it = lowerFar(b, t);
  return it != b.far.end() && it->ticks == t ? it->qty : 0.0;
}

void OrderBook::set(Side s, int64_t t, double q) {
  SideBook& b = side_[s];
  const bool wasEmpty = b.count == 0;
  if (inWindow(t)) {
    double& cell = b.dense[size_t(t - base_)];
    const bool had = cell > 0;
    if (q > 0) {
      cell = q;
      if (!had) ++b.count;
    } else {
      if (!had) return;   // removing a level we never had: normal on Binance
      cell = 0;
      --b.count;
    }
  } else {
    auto it = std::lower_bound(b.far.begin(), b.far.end(), t, [](const BookLevel& l, int64_t v) { return l.ticks < v; });
    const bool had = it != b.far.end() && it->ticks == t;
    if (q > 0) {
      if (had) it->qty = q;
      else {
        b.far.insert(it, BookLevel{t, q});
        ++b.count;
      }
    } else {
      if (!had) return;
      b.far.erase(it);
      --b.count;
    }
  }
  if (q > 0) {
    if (wasEmpty || (s == Bid ? t > b.best : t < b.best)) b.best = t;
  } else if (b.count > 0 && t == b.best) {
    findBest(s, t);
  }
}

void OrderBook::findBest(Side s, int64_t from) {
  SideBook& b = side_[s];
  const int64_t top = base_ + n_;
  if (s == Bid) {
    // far bids above the window beat every dense one; then the window downwards from the old best; then far below it
    if (!b.far.empty() && b.far.back().ticks >= top) { b.best = b.far.back().ticks; return; }
    for (int64_t t = std::min(from, top - 1); t >= base_; --t)
      if (b.dense[size_t(t - base_)] > 0) { b.best = t; return; }
    auto it = lowerFar(b, base_);
    if (it != b.far.begin()) { b.best = std::prev(it)->ticks; return; }
  } else {
    if (!b.far.empty() && b.far.front().ticks < base_) { b.best = b.far.front().ticks; return; }
    for (int64_t t = std::max(from, base_); t < top; ++t)
      if (b.dense[size_t(t - base_)] > 0) { b.best = t; return; }
    auto it = lowerFar(b, top);
    if (it != b.far.end()) { b.best = it->ticks; return; }
  }
}

void OrderBook::recenter(int64_t center) {
  const int64_t nb = center - n_ / 2;
  if (nb == base_) return;
  for (SideBook& b : side_) {
    // window → far (merged in order), then the far levels inside the new window → dense
    std::vector<BookLevel> moved;
    for (int64_t i = 0; i < n_; ++i)
      if (double& q = b.dense[size_t(i)]; q > 0) {
        moved.push_back({base_ + i, q});
        q = 0;
      }
    if (!moved.empty()) {
      std::vector<BookLevel> merged;
      merged.reserve(std::max(b.far.capacity(), b.far.size() + moved.size()));
      std::merge(b.far.begin(), b.far.end(), moved.begin(), moved.end(), std::back_inserter(merged),
                 [](const BookLevel& x, const BookLevel& y) { return x.ticks < y.ticks; });
      b.far.swap(merged);
    }
    auto lo = std::lower_bound(b.far.begin(), b.far.end(), nb, [](const BookLevel& l, int64_t v) { return l.ticks < v; });
    auto hi = std::lower_bound(lo, b.far.end(), nb + n_, [](const BookLevel& l, int64_t v) { return l.ticks < v; });
    for (auto it = lo; it != hi; ++it) b.dense[size_t(it->ticks - nb)] = it->qty;
    b.far.erase(lo, hi);
  }
  base_ = nb;
}

bool OrderBook::recenterIfNeeded() {
  int64_t mid;
  if (has(Bid) && has(Ask)) mid = best(Bid) + (best(Ask) - best(Bid)) / 2;
  else if (has(Bid)) mid = best(Bid);
  else if (has(Ask)) mid = best(Ask);
  else return false;
  if (mid >= base_ + n_ / 4 && mid < base_ + 3 * n_ / 4) return false;
  recenter(mid);
  return true;
}

// ------------------------------------------------------------------------------------------------ BookSync

void BookSync::reset() {
  book_.clear();
  state_ = State::Buffering;
  buffer_.clear();
  last_ = -1;
  first_ = true;
}

BookSync::Result BookSync::gap() {
  ++gaps_;
  reset();
  return Result::Gap;
}

BookSync::Result BookSync::apply(const DepthDiff& e) {
  for (const BookLevel& l : e.bids) book_.set(OrderBook::Bid, l.ticks, l.qty);
  for (const BookLevel& l : e.asks) book_.set(OrderBook::Ask, l.ticks, l.qty);
  last_ = e.u;
  first_ = false;
  if (book_.crossed()) return gap();
  book_.recenterIfNeeded();
  return Result::Applied;
}

BookSync::Result BookSync::onEvent(const DepthDiff& e) {
  if (state_ == State::Buffering) {
    if (buffer_.size() >= 5000) buffer_.erase(buffer_.begin());   // a snapshot that never comes: keep the newest
    buffer_.push_back(e);
    return Result::Buffered;
  }
  if (venue_ == Venue::Spot) {
    if (e.u <= last_) return Result::Ignored;
    if (e.U > last_ + 1) return gap();
    return apply(e);
  }
  // futures
  if (first_) {
    if (e.u < last_) return Result::Ignored;
    if (!(e.U <= last_ && last_ <= e.u)) return gap();
    return apply(e);
  }
  if (e.u <= last_) return Result::Ignored;
  if (e.pu != last_) return gap();
  return apply(e);
}

BookSync::Result BookSync::onSnapshot(int64_t lastUpdateId, const std::vector<BookLevel>& bids, const std::vector<BookLevel>& asks) {
  // older than the first event we buffered: the events between are lost, ask for a newer snapshot
  if (!buffer_.empty() && lastUpdateId < buffer_.front().U) return Result::SnapshotTooOld;
  book_.clear();
  for (const BookLevel& l : bids) book_.set(OrderBook::Bid, l.ticks, l.qty);
  for (const BookLevel& l : asks) book_.set(OrderBook::Ask, l.ticks, l.qty);
  book_.recenterIfNeeded();
  last_ = lastUpdateId;
  first_ = true;
  state_ = State::Synced;
  std::vector<DepthDiff> pending;
  pending.swap(buffer_);
  for (const DepthDiff& e : pending)
    if (onEvent(e) == Result::Gap) return Result::Gap;
  if (book_.crossed()) return gap();
  return Result::Synced;
}

}  // namespace charts::flow
