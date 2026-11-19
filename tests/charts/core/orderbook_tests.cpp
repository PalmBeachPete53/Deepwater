// Order flow, step D1: the local order book and Binance's synchronisation (src/charts/core/OrderBook).
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/OrderBook.h"
#include <chrono>
#include <map>
#include <random>

using namespace charts::flow;
using B = OrderBook;

namespace {
std::vector<std::pair<int64_t, double>> all(const OrderBook& b, B::Side s) {
  std::vector<std::pair<int64_t, double>> v;
  b.forEach(s, INT64_MIN / 4, INT64_MAX / 4, [&](int64_t t, double q) { v.push_back({t, q}); });
  return v;
}
DepthDiff ev(int64_t U, int64_t u, std::vector<BookLevel> bids = {}, std::vector<BookLevel> asks = {}, int64_t pu = -1) {
  DepthDiff e;
  e.U = U;
  e.u = u;
  e.pu = pu;
  e.bids = std::move(bids);
  e.asks = std::move(asks);
  return e;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ book

TEST("orderbook: add, change, remove at 0, remove an unknown level, best bid and ask") {
  OrderBook b(1024);
  b.recenter(1000);
  b.set(B::Bid, 998, 2);
  b.set(B::Bid, 999, 1);
  b.set(B::Ask, 1001, 3);
  b.set(B::Ask, 1003, 4);
  CHECK_EQ(b.best(B::Bid), int64_t(999));
  CHECK_EQ(b.best(B::Ask), int64_t(1001));
  CHECK_EQ(b.qty(B::Bid, 998), 2.0);
  b.set(B::Bid, 998, 5);                 // a quantity is absolute
  CHECK_EQ(b.qty(B::Bid, 998), 5.0);
  CHECK_EQ(int(b.levels(B::Bid)), 2);
  b.set(B::Bid, 999, 0);                 // removing the best: the next one becomes best
  CHECK_EQ(b.best(B::Bid), int64_t(998));
  CHECK_EQ(int(b.levels(B::Bid)), 1);
  b.set(B::Ask, 1500, 0);                // unknown level: nothing happens
  CHECK_EQ(int(b.levels(B::Ask)), 2);
  b.set(B::Ask, 1001, 0);
  CHECK_EQ(b.best(B::Ask), int64_t(1003));
  CHECK(!b.crossed());
  b.set(B::Bid, 1003, 1);
  CHECK(b.crossed());
  b.set(B::Bid, 1003, 0);
  b.set(B::Bid, 998, 0);
  CHECK(!b.has(B::Bid));
}

TEST("orderbook: levels beyond the window live aside; recentring moves them in and out without losing any") {
  OrderBook b(64);   // a tiny window to exercise the far levels
  b.recenter(1000);  // window 968 … 1031
  CHECK_EQ(b.windowBase(), int64_t(968));
  b.set(B::Bid, 999, 1);
  b.set(B::Bid, 900, 2);                 // far below
  b.set(B::Bid, 1100, 3);                // far above (a bid above the window: it is the best)
  b.set(B::Ask, 1001, 4);
  b.set(B::Ask, 5000, 5);
  CHECK_EQ(b.best(B::Bid), int64_t(1100));
  b.set(B::Bid, 1100, 0);
  CHECK_EQ(b.best(B::Bid), int64_t(999));
  b.set(B::Bid, 999, 0);
  CHECK_EQ(b.best(B::Bid), int64_t(900));   // from the far levels below
  b.set(B::Bid, 999, 1);
  b.set(B::Ask, 1001, 0);
  CHECK_EQ(b.best(B::Ask), int64_t(5000));
  b.set(B::Ask, 1001, 4);
  CHECK(all(b, B::Bid) == (std::vector<std::pair<int64_t, double>>{{900, 2}, {999, 1}}));
  b.recenter(4990);                      // window 4958 … 5021: 999 and 1001 go aside, 5000 comes in
  CHECK(all(b, B::Bid) == (std::vector<std::pair<int64_t, double>>{{900, 2}, {999, 1}}));
  CHECK(all(b, B::Ask) == (std::vector<std::pair<int64_t, double>>{{1001, 4}, {5000, 5}}));
  CHECK_EQ(b.qty(B::Ask, 5000), 5.0);
  CHECK_EQ(b.best(B::Ask), int64_t(1001));
  // the mid (1000) is outside the middle half: recenterIfNeeded brings the window back around it
  CHECK(b.recenterIfNeeded());
  CHECK_EQ(b.windowBase(), int64_t(1000 - 32));
  CHECK(!b.recenterIfNeeded());
  // forEach on a range crossing far / window / far
  std::vector<int64_t> seen;
  b.forEach(B::Ask, 1000, 6000, [&](int64_t t, double) { seen.push_back(t); });
  CHECK(seen == (std::vector<int64_t>{1001, 5000}));
}

TEST("orderbook: equals a std::map reference over 200 000 random updates and recentrings") {
  std::mt19937 rng(21);
  OrderBook b(256);
  std::map<int64_t, double> ref[2];
  int64_t mid = 100000;
  b.recenter(mid);
  bool same = true;
  for (int i = 0; i < 200000 && same; ++i) {
    if (i % 5000 == 0) mid += int64_t(rng() % 2001) - 1000;   // the price moves far now and then
    const int s = int(rng() % 2);
    // bids below mid, asks above, mostly near it, sometimes far
    const int64_t dist = rng() % 10 ? 1 + int64_t(rng() % 100) : 1 + int64_t(rng() % 5000);
    const int64_t t = s == 0 ? mid - dist : mid + dist;
    const double q = rng() % 3 ? double(1 + rng() % 1000) / 8 : 0;
    b.set(B::Side(s), t, q);
    if (q > 0) ref[s][t] = q;
    else ref[s].erase(t);
    if (i % 997 == 0) b.recenterIfNeeded();
    if (i % 101 == 0) {
      for (int k = 0; k < 2 && same; ++k) {
        const auto got = all(b, B::Side(k));
        std::vector<std::pair<int64_t, double>> want(ref[k].begin(), ref[k].end());
        same = got == want && b.levels(B::Side(k)) == ref[k].size();
        if (same && !ref[k].empty()) same = b.best(B::Side(k)) == (k == 0 ? ref[k].rbegin()->first : ref[k].begin()->first);
      }
    }
  }
  CHECK(same);
}

TEST("orderbook: updates inside the window and in the reserved far levels allocate nothing") {
  OrderBook b(1 << 12, 4096);
  b.recenter(1000000);
  std::mt19937 rng(3);
  alloccount::Scope s;
  for (int i = 0; i < 100000; ++i) {
    const int64_t t = 1000000 + int64_t(rng() % 3000) - 1500;
    b.set(t < 1000000 ? B::Bid : B::Ask, t == 1000000 ? 1000001 : t, rng() % 4 ? 1.5 : 0);
  }
  for (int i = 0; i < 2000; ++i) b.set(B::Ask, 2000000 + i, 1);   // far, within the reserve
  const size_t n = s.count();
  CHECK_EQ(int(n), 0);
}

// ------------------------------------------------------------------------------------------------ sync

TEST("booksync spot: buffer, snapshot too old, drop u <= lastUpdateId, then continuity; a hole asks for a resync") {
  OrderBook book(1024);
  BookSync sync(BookSync::Venue::Spot, book);
  CHECK(sync.onEvent(ev(101, 105, {{990, 1}})) == BookSync::Result::Buffered);
  CHECK(sync.onEvent(ev(106, 110, {{991, 2}}, {{1010, 5}})) == BookSync::Result::Buffered);
  CHECK(sync.onEvent(ev(111, 115, {}, {{1011, 6}})) == BookSync::Result::Buffered);
  // lastUpdateId 100 < the first buffered U (101): another snapshot (Binance's rule, even if 101 follows 100)
  CHECK(sync.onSnapshot(100, {{980, 9}}, {{1020, 9}}) == BookSync::Result::SnapshotTooOld);
  CHECK(sync.state() == BookSync::State::Buffering);
  // lastUpdateId 107: event 101-105 is already in it (dropped), 106-110 contains 108 = L + 1 (applied), then 111-115
  CHECK(sync.onSnapshot(107, {{980, 9}, {990, 7}}, {{1020, 9}}) == BookSync::Result::Synced);
  CHECK_EQ(sync.lastUpdateId(), int64_t(115));
  CHECK_EQ(book.qty(B::Bid, 990), 7.0);    // not 1: event 101-105 was older than the snapshot
  CHECK_EQ(book.qty(B::Bid, 991), 2.0);
  CHECK_EQ(book.best(B::Ask), int64_t(1010));
  CHECK(sync.onEvent(ev(116, 120, {{990, 0}})) == BookSync::Result::Applied);
  CHECK_EQ(book.qty(B::Bid, 990), 0.0);
  CHECK(sync.onEvent(ev(110, 118, {{995, 1}})) == BookSync::Result::Ignored);   // already in the book
  CHECK(sync.onEvent(ev(122, 130, {{995, 1}})) == BookSync::Result::Gap);       // exactly one id (121) missing
  CHECK(sync.state() == BookSync::State::Buffering);
  CHECK(!book.has(B::Bid));
  CHECK_EQ(sync.gaps(), 1);
}

TEST("booksync futures: drop u < lastUpdateId, first event U <= L <= u, then pu = previous u") {
  OrderBook book(1024);
  BookSync sync(BookSync::Venue::Futures, book);
  sync.onEvent(ev(190, 195, {{990, 1}}, {}, 189));
  sync.onEvent(ev(196, 205, {{991, 2}}, {}, 195));
  sync.onEvent(ev(206, 210, {}, {{1010, 3}}, 205));
  CHECK(sync.onSnapshot(200, {{980, 9}}, {{1020, 9}}) == BookSync::Result::Synced);
  CHECK_EQ(sync.lastUpdateId(), int64_t(210));
  CHECK_EQ(book.qty(B::Bid, 990), 0.0);   // 190-195 dropped (u < 200)
  CHECK_EQ(book.qty(B::Bid, 991), 2.0);
  CHECK(sync.onEvent(ev(211, 215, {}, {}, 210)) == BookSync::Result::Applied);
  CHECK(sync.onEvent(ev(220, 225, {}, {}, 218)) == BookSync::Result::Gap);   // pu 218 ≠ 215
  // a snapshot older than the first buffered event: ask for another one
  sync.onEvent(ev(301, 305, {}, {}, 299));
  CHECK(sync.onSnapshot(300, {{980, 1}}, {{1020, 1}}) == BookSync::Result::SnapshotTooOld);
  // the first event applied must contain lastUpdateId: an event ending exactly at it (296-300) does, and the stream
  // goes on from there
  OrderBook b2(1024);
  BookSync s2(BookSync::Venue::Futures, b2);
  s2.onEvent(ev(290, 295, {}, {}, 289));
  s2.onEvent(ev(296, 300, {{985, 4}}, {}, 295));
  CHECK(s2.onSnapshot(300, {{980, 1}}, {{1020, 1}}) == BookSync::Result::Synced);
  CHECK_EQ(b2.qty(B::Bid, 985), 4.0);
  CHECK(s2.onEvent(ev(301, 305, {}, {}, 300)) == BookSync::Result::Applied);
  // …but when that event never arrived, the next one (301-305) does not contain 300: resync
  OrderBook b3(1024);
  BookSync s3(BookSync::Venue::Futures, b3);
  s3.onEvent(ev(290, 295, {}, {}, 289));
  CHECK(s3.onSnapshot(300, {{980, 1}}, {{1020, 1}}) == BookSync::Result::Synced);   // 290-295 dropped
  CHECK(s3.onEvent(ev(301, 305, {}, {}, 300)) == BookSync::Result::Gap);
}

TEST("booksync: a crossed book is wrong — resync") {
  OrderBook book(1024);
  BookSync sync(BookSync::Venue::Spot, book);
  CHECK(sync.onSnapshot(10, {{999, 1}}, {{1001, 1}}) == BookSync::Result::Synced);
  CHECK(sync.onEvent(ev(11, 11, {{1005, 1}})) == BookSync::Result::Gap);   // a bid above the best ask, ask not removed
  CHECK_EQ(sync.gaps(), 1);
}

// The exchange as the documentation describes it: single updates with consecutive ids, grouped into stream events
// (each lists the FINAL quantity of every level it touched); a snapshot is the book at some update id, usually in
// the middle of an event. Whatever the moment of the snapshot, the synced book must equal the exchange's.
TEST("booksync: simulated exchange, snapshot at any moment, both venues — the local book equals the exchange's") {
  for (const auto venue : {BookSync::Venue::Spot, BookSync::Venue::Futures}) {
    int bad = 0, synced = 0;
    for (int round = 0; round < 300; ++round) {
      std::mt19937 rng(1000 + round);
      std::map<int64_t, double> ex[2];   // the exchange's book
      int64_t id = 1000;
      // updates: (id, side, ticks, qty); events: ranges of them
      struct Up { int64_t id; int side; int64_t t; double q; };
      std::vector<Up> ups;
      std::vector<std::pair<size_t, size_t>> events;   // [first, last] index into ups
      for (int k = 0; k < 40; ++k) {
        const size_t first = ups.size();
        const int n = 1 + int(rng() % 6);
        for (int j = 0; j < n; ++j) {
          const int s = int(rng() % 2);
          const int64_t t = s == 0 ? 990 - int64_t(rng() % 20) : 1010 + int64_t(rng() % 20);
          ups.push_back({++id, s, t, rng() % 3 ? double(1 + rng() % 9) : 0});
        }
        events.push_back({first, ups.size() - 1});
      }
      // snapshot at a random update (inside some event), taken after the stream opened
      const size_t cut = 3 + rng() % (ups.size() - 6);
      std::map<int64_t, double> snap[2];
      for (size_t i = 0; i <= cut; ++i) {
        if (ups[i].q > 0) snap[ups[i].side][ups[i].t] = ups[i].q;
        else snap[ups[i].side].erase(ups[i].t);
      }
      for (const Up& u : ups) {   // the exchange's final book
        if (u.q > 0) ex[u.side][u.t] = u.q;
        else ex[u.side].erase(u.t);
      }
      auto eventOf = [&](size_t k) {
        DepthDiff e;
        e.U = ups[events[k].first].id;
        e.u = ups[events[k].second].id;
        e.pu = k ? ups[events[k - 1].second].id : e.U - 1;
        std::map<int64_t, double> fin[2];
        for (size_t i = events[k].first; i <= events[k].second; ++i) fin[ups[i].side][ups[i].t] = ups[i].q;
        for (auto& [t, q] : fin[0]) e.bids.push_back({t, q});
        for (auto& [t, q] : fin[1]) e.asks.push_back({t, q});
        return e;
      };
      OrderBook book(64);
      BookSync sync(venue, book);
      // the snapshot answer arrives while some events are already buffered (the first ones before it)
      const size_t arrivedBefore = std::min(events.size(), size_t(1 + rng() % 10));
      size_t k = 0;
      for (; k < arrivedBefore; ++k) sync.onEvent(eventOf(k));
      std::vector<BookLevel> sb, sa;
      for (auto& [t, q] : snap[0]) sb.push_back({t, q});
      for (auto& [t, q] : snap[1]) sa.push_back({t, q});
      auto r = sync.onSnapshot(ups[cut].id, sb, sa);
      if (r == BookSync::Result::SnapshotTooOld) {
        // as the client would: a newer snapshot (the exchange's book after everything buffered so far)
        std::map<int64_t, double> s2[2];
        const size_t upto = events[k - 1].second;
        for (size_t i = 0; i <= upto; ++i) {
          if (ups[i].q > 0) s2[ups[i].side][ups[i].t] = ups[i].q;
          else s2[ups[i].side].erase(ups[i].t);
        }
        sb.clear();
        sa.clear();
        for (auto& [t, q] : s2[0]) sb.push_back({t, q});
        for (auto& [t, q] : s2[1]) sa.push_back({t, q});
        r = sync.onSnapshot(ups[upto].id, sb, sa);
      }
      for (; k < events.size(); ++k) sync.onEvent(eventOf(k));
      if (sync.state() == BookSync::State::Synced) {
        ++synced;
        for (int s = 0; s < 2; ++s) {
          std::vector<std::pair<int64_t, double>> want(ex[s].begin(), ex[s].end());
          if (all(book, B::Side(s)) != want) ++bad;
        }
      } else if (venue == BookSync::Venue::Spot) {
        ++bad;   // spot never needs a resync here
      }
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(synced, 300);   // every round ends with a book (the snapshot always lands inside a buffered or later event)
  }
}

TEST("orderbook: speed (printed) of applying diffs") {
  OrderBook b(1 << 17);
  b.recenter(6000000);
  std::mt19937 rng(5);
  std::vector<BookLevel> lv(1000000);
  for (auto& l : lv) l = {6000000 + int64_t(rng() % 4001) - 2000, rng() % 4 ? double(rng() % 100) / 10 : 0};
  const auto t0 = std::chrono::steady_clock::now();
  for (const auto& l : lv) b.set(l.ticks < 6000000 ? B::Bid : B::Ask, l.ticks == 6000000 ? 6000001 : l.ticks, l.qty);
  const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / double(lv.size());
  std::printf("  order book: %.1f ns per level update (window %lld ticks)\n", ns, (long long)b.windowTicks());
  CHECK(ns < 1000);
}
