// The live book reduced to bookDepth's bands (src/charts/core/BookBands.h): sums against a hand count, the edges, the
// snapshot's reach, an empty book.
#include "charts/core/BookBands.h"
#include "Check.h"

using namespace charts::flow;

TEST("book bands: cumulative depth out to ±0.2 … ±5 % of the mid, bids below and asks above") {
  const TickScale sc(Decimal{1, -1, true});   // 0.1, the BTCUSDT perpetual
  OrderBook b;
  b.recenter(sc.ticksOf(100000.0));
  // mid 100 000: bid 99 999.9 / ask 100 000.1
  b.set(OrderBook::Bid, sc.ticksOf(99999.9), 1);     // 0.0001 % below
  b.set(OrderBook::Bid, sc.ticksOf(99850.0), 2);     // 0.15 %
  b.set(OrderBook::Bid, sc.ticksOf(99500.0), 4);     // 0.5 %
  b.set(OrderBook::Bid, sc.ticksOf(99000.0), 8);     // exactly 1 %
  b.set(OrderBook::Bid, sc.ticksOf(96000.0), 16);    // 4 %
  b.set(OrderBook::Bid, sc.ticksOf(94000.0), 32);    // 6 %: outside
  b.set(OrderBook::Ask, sc.ticksOf(100000.1), 1);
  b.set(OrderBook::Ask, sc.ticksOf(101500.0), 3);    // 1.5 %
  b.set(OrderBook::Ask, sc.ticksOf(105000.0), 5);    // exactly 5 %
  BookBands o;
  bookBands(b, sc, sc.ticksOf(99000.0), sc.ticksOf(102000.0), 1234, o);
  CHECK(o.ok());
  CHECK_NEAR(o.mid, 100000.0, 1e-6);
  CHECK(o.timeMs == 1234);
  // bids: −0.2 = 1 + 2; −1 = + 4 + 8; −2, −3 the same; −4, −5 + 16
  CHECK_NEAR(o.depth[5], 3, 1e-9);
  CHECK_NEAR(o.depth[4], 15, 1e-9);
  CHECK_NEAR(o.depth[3], 15, 1e-9);
  CHECK_NEAR(o.depth[2], 15, 1e-9);
  CHECK_NEAR(o.depth[1], 31, 1e-9);
  CHECK_NEAR(o.depth[0], 31, 1e-9);
  // asks: +0.2 = 1, +1 = 1, +2 … +4 = 4, +5 = 9
  CHECK_NEAR(o.depth[6], 1, 1e-9);
  CHECK_NEAR(o.depth[7], 1, 1e-9);
  CHECK_NEAR(o.depth[8], 4, 1e-9);
  CHECK_NEAR(o.depth[10], 4, 1e-9);
  CHECK_NEAR(o.depth[11], 9, 1e-9);
  CHECK_NEAR(o.coverBid, 1, 1e-6);   // the snapshot reached 1 % below, 2 % above
  CHECK_NEAR(o.coverAsk, 2, 1e-6);
  // cumulative: never smaller further out
  for (int k = 0; k < 5; ++k) CHECK(o.depth[size_t(k)] >= o.depth[size_t(k + 1)] && o.depth[size_t(11 - k)] >= o.depth[size_t(10 - k)]);
}

TEST("book bands: no book, no bands; unknown reach is 0") {
  const TickScale sc(Decimal{1, -2, true});
  OrderBook b;
  BookBands o;
  bookBands(b, sc, 0, 0, 5, o);
  CHECK(!o.ok());
  CHECK(std::isnan(o.depth[0]) && std::isnan(o.depth[11]));
  b.recenter(sc.ticksOf(50.0));
  b.set(OrderBook::Bid, sc.ticksOf(49.99), 2);
  b.set(OrderBook::Ask, sc.ticksOf(50.01), 3);
  bookBands(b, sc, 0, 0, 6, o);
  CHECK(o.ok() && o.coverBid == 0 && o.coverAsk == 0);
  CHECK_NEAR(o.depth[5], 2, 1e-9);
  CHECK_NEAR(o.depth[6], 3, 1e-9);
}
