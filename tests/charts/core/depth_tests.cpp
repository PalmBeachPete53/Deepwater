// Order flow, step D2 (pure parts): depth messages, the column sampler, the lock-free ring.
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/BinanceMsg.h"
#include "charts/core/Heatmap.h"
#include "charts/core/Spsc.h"
#include <QString>
#include <cstring>
#include <thread>

using namespace charts::flow;

namespace {
TickScale scale(const char* t) { return TickScale(parseDecimal(t, t + std::strlen(t))); }
const char* kSpotDiff = R"({"e":"depthUpdate","E":1795024026414,"s":"BTCUSDT","U":103144393324,"u":103144393349,"b":[["63235.20000000","0.37223000"],["63234.22000000","0.00000000"]],"a":[["63235.21000000","1.81343000"]]})";
const char* kFutDiff = R"({"e":"depthUpdate","E":1795024026426,"T":1795024026425,"s":"BTCUSDT","ps":"BTCUSDT","U":12157820814231,"u":12157820824216,"pu":12157820814226,"b":[["6320.70","0.017"],["12711.10","0.000"]],"a":[]})";
const char* kSpotSnap = R"({"lastUpdateId":103144396240,"bids":[["63239.32000000","8.57221000"],["63239.01000000","0.00018000"]],"asks":[["63239.33000000","0.28683000"]]})";
const char* kFutSnap = R"({"lastUpdateId":12157821418681,"E":1795024030489,"T":1795024030486,"bids":[["63211.30","17.167"]],"asks":[["63211.40","0.072"],["63211.50","0.001"]]})";
}  // namespace

TEST("depth messages: spot and futures diffs (real ones), prices in ticks, quantity 0 kept as a removal") {
  DepthDiff d;
  CHECK(parseDepthUpdate(QByteArrayView(kSpotDiff), scale("0.01"), d));
  CHECK_EQ(d.U, int64_t(103144393324));
  CHECK_EQ(d.u, int64_t(103144393349));
  CHECK_EQ(d.pu, int64_t(-1));
  CHECK_EQ(d.eventMs, int64_t(1795024026414));
  CHECK_EQ(int(d.bids.size()), 2);
  CHECK_EQ(d.bids[0].ticks, int64_t(6323520));
  CHECK(d.bids[0].qty == 0.37223);
  CHECK_EQ(d.bids[1].qty, 0.0);
  CHECK_EQ(int(d.asks.size()), 1);
  CHECK(parseDepthUpdate(QStringView(QString::fromLatin1(kFutDiff)), scale("0.10"), d));   // as QWebSocket hands it over
  CHECK_EQ(d.pu, int64_t(12157820814226));
  CHECK_EQ(d.bids[0].ticks, int64_t(63207));
  CHECK_EQ(int(d.asks.size()), 0);
}

TEST("depth messages: REST snapshots, and what is refused") {
  DepthSnapshot s;
  CHECK(parseDepthSnapshot(QByteArrayView(kSpotSnap), scale("0.01"), s));
  CHECK_EQ(s.lastUpdateId, int64_t(103144396240));
  CHECK_EQ(int(s.bids.size()), 2);
  CHECK_EQ(s.asks[0].ticks, int64_t(6323933));
  CHECK(parseDepthSnapshot(QByteArrayView(kFutSnap), scale("0.10"), s));
  CHECK_EQ(int(s.asks.size()), 2);
  CHECK(s.bids[0].qty == 17.167);
  DepthDiff d;
  CHECK(!parseDepthUpdate(QByteArrayView(R"({"e":"aggTrade","a":1})"), scale("0.01"), d));
  CHECK(!parseDepthUpdate(QByteArrayView(R"({"e":"depthUpdate","U":1,"u":2,"b":[["1"]],"a":[]})"), scale("0.01"), d));   // half a level
  CHECK(!parseDepthUpdate(QByteArrayView(R"({"e":"depthUpdate","U":1,"u":2,"b":[]})"), scale("0.01"), d));               // no asks
  CHECK(!parseDepthSnapshot(QByteArrayView(R"({"code":-1121,"msg":"Invalid symbol."})"), scale("0.01"), s));
}

TEST("depth messages: reading into a warm DepthDiff allocates nothing") {
  const QString msg = QString::fromLatin1(kSpotDiff);
  const TickScale sc = scale("0.01");
  DepthDiff d;
  d.bids.reserve(64);
  d.asks.reserve(64);
  alloccount::Scope s;
  int ok = 0;
  for (int i = 0; i < 10000; ++i) ok += parseDepthUpdate(QStringView(msg), sc, d);
  const size_t n = s.count();
  CHECK_EQ(int(n), 0);
  CHECK_EQ(ok, 10000);
}

TEST("depth sampler: rows of rowTicks around the mid, bids and asks summed per row, band kept until the price leaves it") {
  OrderBook b(1 << 14);
  b.recenter(100000);
  b.set(OrderBook::Bid, 99995, 1);
  b.set(OrderBook::Bid, 99990, 2);    // same row as 99995 with 10-tick rows
  b.set(OrderBook::Ask, 100005, 4);
  b.set(OrderBook::Ask, 100100, 5);
  b.set(OrderBook::Ask, 200000, 7);   // outside the band: not in the column
  DepthSampler s(10);
  DepthColumn c;
  s.sample(b, true, 5000, c);
  // mid 100000 → base = 100000 − 256 · 10 = 97440; 99995 and 99990 → row 255; 100005 → 256; 100100 → 266
  CHECK_EQ(c.baseTicks, int64_t(97440));
  CHECK_EQ(c.rowTicks, 10);
  CHECK_EQ(c.timeMs, int64_t(5000));
  CHECK_EQ(c.qty[255], 3.0f);
  CHECK_EQ(c.qty[256], 4.0f);
  CHECK_EQ(c.qty[266], 5.0f);
  float total = 0;
  for (float q : c.qty) total += q;
  CHECK_EQ(total, 12.0f);
  CHECK_EQ(c.bestBid, int64_t(99995));
  CHECK_EQ(c.bestAsk, int64_t(100005));
  CHECK_EQ(int(c.flags), 0);
  // the price moves inside the middle half (97440 + 1280 … 97440 + 3840): same band
  b.set(OrderBook::Bid, 99995, 0);
  b.set(OrderBook::Bid, 99990, 0);
  b.set(OrderBook::Ask, 100005, 0);
  b.set(OrderBook::Bid, 101000, 1);
  b.set(OrderBook::Ask, 101010, 1);
  s.sample(b, true, 5100, c);
  CHECK_EQ(c.baseTicks, int64_t(97440));
  // …and out of it (mid 102005 ≥ 97440 + 3840): the band re-anchors around it
  b.set(OrderBook::Bid, 102000, 1);
  b.set(OrderBook::Ask, 102010, 1);
  b.set(OrderBook::Bid, 101000, 0);
  b.set(OrderBook::Ask, 101010, 0);
  b.set(OrderBook::Ask, 100100, 0);
  s.sample(b, true, 5200, c);
  CHECK_EQ(c.baseTicks, int64_t(102000 - 2560));
  CHECK(c.flags & DepthColumn::Reanchored);
  // not synced: a Missing column, no quantities
  s.sample(b, false, 5300, c);
  CHECK(c.flags & DepthColumn::Missing);
  CHECK_EQ(c.qty[256], 0.0f);
  CHECK_EQ(c.bestBid, int64_t(0));
}

TEST("depth sampler: Auto rows ≈ 2e-5 of the price, rounded to 1-2-5 ticks") {
  CHECK_EQ(DepthSampler::autoRowTicks(6300000), int64_t(100));   // BTCUSDT spot, 0.01 tick: $1 rows
  CHECK_EQ(DepthSampler::autoRowTicks(630000), int64_t(10));     // BTCUSDT perpetual, 0.1 tick: $1 rows
  CHECK_EQ(DepthSampler::autoRowTicks(1000), int64_t(1));
  CHECK_EQ(DepthSampler::autoRowTicks(175000), int64_t(5));      // ETHUSDT 1750.00: 3.5 ticks → 5
}

TEST("depth sampler: sampling a warm column allocates nothing") {
  OrderBook b(1 << 14);
  b.recenter(100000);
  for (int i = 1; i < 500; ++i) {
    b.set(OrderBook::Bid, 100000 - i, i);
    b.set(OrderBook::Ask, 100000 + i, i);
  }
  DepthSampler s(4);
  DepthColumn c;
  s.sample(b, true, 0, c);
  alloccount::Scope a;
  for (int i = 0; i < 1000; ++i) s.sample(b, true, i, c);
  const size_t n = a.count();
  CHECK_EQ(int(n), 0);
}

TEST("spsc ring: one producer thread, one consumer thread, 2 million items in order, none lost") {
  struct Item { uint64_t seq; uint64_t check; };
  SpscRing<Item, 64> ring;
  constexpr uint64_t N = 2000000;
  std::thread producer([&] {
    for (uint64_t i = 0; i < N; ++i) {
      Item* s;
      while (!(s = ring.beginPush())) std::this_thread::yield();
      s->seq = i;
      s->check = i * 2654435761u;
      ring.commitPush();
    }
  });
  uint64_t next = 0, bad = 0;
  while (next < N) {
    const Item* s = ring.front();
    if (!s) { std::this_thread::yield(); continue; }
    if (s->seq != next || s->check != next * 2654435761u) ++bad;
    ring.popFront();
    ++next;
  }
  producer.join();
  CHECK_EQ(int(bad), 0);
  CHECK_EQ(int(ring.size()), 0);
  Item x{};
  CHECK(!ring.pop(x));
  for (int i = 0; i < 64; ++i) CHECK(ring.push(Item{uint64_t(i), 0}));
  CHECK(!ring.push(Item{99, 0}));   // full: refused, never overwritten
}
