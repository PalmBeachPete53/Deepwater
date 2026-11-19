#pragma once
// Order flow: the local order book of one market (heatmap, step D1). Pure: no Qt, no network, no threads.
//
//   OrderBook  quantities per price in ticks, bids and asks. Levels near the price live in a dense window of
//              windowTicks ticks (plain arrays indexed by tick: an update is one store, no allocation); levels outside
//              it (the far end of a 5000-level snapshot) live in a sorted vector per side, found by binary search. When
//              the mid price leaves the middle half of the window, recenter() moves the window (rare, O(window)).
//              Best bid / ask are kept up to date; removing the best scans to the next level (a few ticks).
//   BookSync   Binance's procedure to build a correct local book from a REST snapshot and the WebSocket diffs, as
//              documented (checked 2026-11-18):
//              spot     open the stream, buffer events, note the first event's U; get the snapshot; if its
//                       lastUpdateId < that U, get another one; drop buffered events with u <= lastUpdateId; then for
//                       each event: u <= L → ignore; U > L + 1 → gap (resync); else apply, L = u.
//              futures  drop events with u < lastUpdateId; the first one applied must have U <= lastUpdateId <= u; then
//                       each event's pu must equal the previous event's u, otherwise resync.
//              A quantity is absolute (0 = remove the level); removing a level we don't know is normal. A book that
//              ends up crossed (best bid ≥ best ask) is wrong too: resync.
// Quantities are doubles, as everywhere in Deepwater (Binance sizes have ≤ 8 decimals).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace charts::flow {

struct BookLevel {
  int64_t ticks = 0;
  double qty = 0;
};

class OrderBook {
public:
  enum Side { Bid = 0, Ask = 1 };
  explicit OrderBook(int64_t windowTicks = int64_t(1) << 17, size_t farReserve = 16384);

  void clear();                                   // empty; the window stays where it is
  void set(Side s, int64_t ticks, double qty);    // qty ≤ 0 removes the level
  double qty(Side s, int64_t ticks) const;
  bool has(Side s) const { return side_[s].count > 0; }
  int64_t best(Side s) const { return side_[s].best; }   // valid when has(s)
  size_t levels(Side s) const { return side_[s].count; }
  bool crossed() const { return has(Bid) && has(Ask) && best(Bid) >= best(Ask); }

  int64_t windowBase() const { return base_; }
  int64_t windowTicks() const { return n_; }
  // Moves the window so the mid price sits in its middle when it has left the middle half. True if it moved.
  bool recenterIfNeeded();
  void recenter(int64_t center);                  // window = [center − n/2, center + n/2)

  // Every level of a side with ticks in [lo, hi], ascending: f(ticks, qty).
  template <class F>
  void forEach(Side s, int64_t lo, int64_t hi, F&& f) const {
    const SideBook& b = side_[s];
    auto farFrom = [&](int64_t a, int64_t z) {
      for (auto it = lowerFar(b, a); it != b.far.end() && it->ticks <= z; ++it) f(it->ticks, it->qty);
    };
    farFrom(lo, std::min(hi, base_ - 1));                         // below the window
    const int64_t a = std::max(lo, base_), z = std::min(hi, base_ + n_ - 1);
    for (int64_t t = a; t <= z; ++t)
      if (const double q = b.dense[size_t(t - base_)]; q > 0) f(t, q);
    farFrom(std::max(lo, base_ + n_), hi);                        // above it
  }

private:
  struct SideBook {
    std::vector<double> dense;
    std::vector<BookLevel> far;   // ascending ticks, outside the window
    size_t count = 0;
    int64_t best = 0;
  };
  static std::vector<BookLevel>::const_iterator lowerFar(const SideBook& b, int64_t t);
  bool inWindow(int64_t t) const { return t >= base_ && t < base_ + n_; }
  void findBest(Side s, int64_t from);            // the old best was removed: look further from there

  int64_t n_, base_ = 0;
  SideBook side_[2];
};

// One diff event of the depth stream (prices already in ticks).
struct DepthDiff {
  int64_t U = 0, u = 0;          // first and last update id in the event
  int64_t pu = -1;               // futures: last update id of the previous event (-1 on spot)
  int64_t eventMs = 0;
  std::vector<BookLevel> bids, asks;
};

class BookSync {
public:
  enum class Venue { Spot, Futures };
  enum class State { Buffering, Synced };
  enum class Result {
    Applied,        // the event changed the book
    Buffered,       // waiting for the snapshot
    Ignored,        // older than the book (normal)
    Gap,            // a hole in the ids or a crossed book: the book is cleared, get a new snapshot (state Buffering)
    Synced,         // snapshot taken and the buffered events applied
    SnapshotTooOld  // spot: the snapshot is older than the first buffered event: get another one
  };

  BookSync(Venue v, OrderBook& book) : venue_(v), book_(book) {}
  void reset();                                   // clear the book, start buffering (on (re)connect)
  Result onEvent(const DepthDiff& e);
  Result onSnapshot(int64_t lastUpdateId, const std::vector<BookLevel>& bids, const std::vector<BookLevel>& asks);
  State state() const { return state_; }
  int64_t lastUpdateId() const { return last_; }
  size_t buffered() const { return buffer_.size(); }
  int gaps() const { return gaps_; }

private:
  Result apply(const DepthDiff& e);               // after the checks
  Result gap();
  Venue venue_;
  OrderBook& book_;
  State state_ = State::Buffering;
  int64_t last_ = -1;          // L: id of the book (spot) / u of the last applied event (futures)
  bool first_ = true;          // futures: the first event after the snapshot has its own rule
  std::vector<DepthDiff> buffer_;
  int gaps_ = 0;
};

}  // namespace charts::flow
