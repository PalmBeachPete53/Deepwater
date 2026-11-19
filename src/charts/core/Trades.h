#pragma once
// Order flow: executed trades shared by the footprint and Big Trades.
//   Side / SideClassifier  who was the aggressor (exact from the feed, else quote rule, else tick rule)
//   Trade                  one aggregated trade, price in ticks
//   TradeTape              the trades of one market in id order, fixed capacity, with the id ranges still missing
// Pure (no Qt, no allocation after construction): tests/charts/core/flow_trades_tests.cpp.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace charts::flow {

enum class Side : int8_t { Sell = -1, Unknown = 0, Buy = 1 };   // the aggressor: Buy = lifted the ask
enum class SideFrom : uint8_t { Exact, Quote, TickRule };        // how the side was known

struct Trade {
  int64_t id = 0;          // the feed's trade id (Binance: aggregate id, consecutive)
  int64_t timeMs = 0;      // trade time, Unix ms UTC
  int64_t ticks = 0;       // price in ticks of the market's TickScale
  double qty = 0;          // base-asset quantity
  Side side = Side::Unknown;
  SideFrom from = SideFrom::Exact;
};

// The aggressor side of each trade, in arrival order.
//  1. exact: the feed says it (Binance's `m`), taken as is;
//  2. quote rule, when the best bid/ask at the time of the trade is known: at or above the ask = Buy, at or below the
//     bid = Sell; strictly inside the spread, above the mid = Buy, below = Sell, at the mid = tick rule (Lee–Ready);
//  3. tick rule: higher than the previous trade = Buy, lower = Sell, equal = the previous trade's side. The first trade
//     has no previous one: Unknown.
// Every trade (whatever rule) becomes "the previous trade" of the next one. approximate() is true once any trade was
// classified by rule 2 or 3: the UI says so.
class SideClassifier {
public:
  struct Quote { int64_t bid = 0, ask = 0; };   // ticks; a side with no price = no quote (ask <= bid is refused)
  Side classify(int64_t ticks, Side exact = Side::Unknown, const Quote* quote = nullptr);
  SideFrom lastFrom() const { return lastFrom_; }
  bool approximate() const { return approximate_; }
  void reset() { *this = SideClassifier(); }

private:
  bool have_ = false;
  int64_t lastTicks_ = 0;
  Side lastSide_ = Side::Unknown;
  SideFrom lastFrom_ = SideFrom::Exact;
  bool approximate_ = false;
};

// The trades of one market ordered by id, at most capacity() of them (a ring: storage allocated once).
//  - append(): live trades. An id jump leaves a gap; an id at or below the last is ignored (duplicate). When full,
//    the oldest trade is dropped.
//  - merge(): a page of history or of a gap (ascending ids): older ids go before the first trade while there is
//    room, ids inside a gap are inserted in place, newer ids are appended; known ids are skipped.
// gaps() lists the missing id ranges between firstId() and lastId(); the stream fills them from REST.
// generation() changes whenever trades are inserted before the end or dropped at the front, so a reader that keeps
// an index (not an id) knows it must seek again.
class TradeTape {
public:
  explicit TradeTape(size_t capacity);

  size_t size() const { return size_; }
  size_t capacity() const { return buf_.size(); }
  bool empty() const { return size_ == 0; }
  bool full() const { return size_ == buf_.size(); }
  const Trade& operator[](size_t i) const { return buf_[(head_ + i) % buf_.size()]; }   // 0 = oldest
  const Trade& front() const { return (*this)[0]; }
  const Trade& back() const { return (*this)[size_ - 1]; }
  int64_t firstId() const { return size_ ? front().id : -1; }
  int64_t lastId() const { return size_ ? back().id : -1; }

  bool append(const Trade& t);
  // Takes trades ascending by id; returns how many were stored. maxOlder: at most this many trades older than the
  // first one are kept (the history budget), the newest of them first.
  size_t merge(const Trade* t, size_t n, size_t maxOlder = SIZE_MAX);

  struct Gap { int64_t from = 0, to = 0; };   // missing ids, inclusive
  const std::vector<Gap>& gaps() const { return gaps_; }
  int64_t missing() const;                    // ids in all gaps
  bool forgetGap(int64_t from);               // accept the gap starting at `from` as a permanent hole

  size_t lowerBound(int64_t id) const;        // first index with trade id >= id (size() if none)
  size_t lowerBoundTime(int64_t ms) const;    // first index with time >= ms (times never decrease along ids)
  uint64_t generation() const { return generation_; }
  uint64_t dropped() const { return dropped_; }   // trades dropped at the front since construction

private:
  Trade& at(size_t i) { return buf_[(head_ + i) % buf_.size()]; }
  void dropFront();
  void insertAt(size_t i, const Trade& t);    // shifts the trades after i one place (room needed)
  void addGap(int64_t from, int64_t to);
  void removeId(int64_t id);                  // id now known: shrink or split its gap

  std::vector<Trade> buf_;
  size_t head_ = 0, size_ = 0;
  std::vector<Gap> gaps_;
  uint64_t generation_ = 0, dropped_ = 0;
};

}  // namespace charts::flow
