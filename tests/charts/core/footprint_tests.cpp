// Order flow, step B1: footprint aggregation (src/charts/core/Footprint). Hand-made trades, results worked out by hand.
//
// The main candle (tick = 1, one minute), trades in id order:
//   id  price side qty          per level (bid x ask)   buy imb. ask(r) ≥ 3·bid(r−1)   sell imb. bid(r) ≥ 3·ask(r+1)
//    1  102   buy   4           105:  1 x  4            4 ≥ 3·1 yes                    highest row: none
//    2  102   sell  3           104:  1 x  9            9 ≥ 3·2 yes                    1 ≥ 3·4 no
//    3  101   sell  4           103:  2 x 12            12 ≥ 3·3 yes                   2 ≥ 3·9 no
//    4  101   buy   2           102:  3 x  9            9 ≥ 3·4 no                     3 ≥ 3·12 no
//    5  100   sell 12           101:  4 x  2            2 ≥ 3·12 no                    4 ≥ 3·9 no
//    6  100   buy   1           100: 12 x  1            lowest row: none               12 ≥ 3·2 yes
//    7  103   buy  12
//    8  103   sell  2           volume per level 13, 6, 12, 14, 10, 5 (100 → 105): POC 103 (14)
//    9  105   buy   4           delta per level −11, −2, +6, +10, +8, +3; bid 23, ask 37, volume 60, delta +14
//   10  105   sell  1           open 102 (id 1), close 104 (id 13), high 105, low 100
//   11  102   buy   5           stacked buy imbalances: 103, 104, 105 (3 rows ≥ stackMin 3)
//   12  104   sell  1
//   13  104   buy   9
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/Footprint.h"
#include "charts/core/FootprintSettings.h"
#include "charts/core/Ticks.h"
#include <chrono>
#include <cstring>
#include <random>

using namespace charts::flow;

namespace {
Trade T(int64_t id, int64_t ticks, Side side, double qty, int64_t ms = 1000) {
  Trade t;
  t.id = id;
  t.timeMs = ms;
  t.ticks = ticks;
  t.qty = qty;
  t.side = side;
  return t;
}
constexpr Side B = Side::Buy, S = Side::Sell;

std::vector<Trade> mainCandle() {
  return {T(1, 102, B, 4), T(2, 102, S, 3), T(3, 101, S, 4),  T(4, 101, B, 2),  T(5, 100, S, 12), T(6, 100, B, 1), T(7, 103, B, 12),
          T(8, 103, S, 2), T(9, 105, B, 4), T(10, 105, S, 1), T(11, 102, B, 5), T(12, 104, S, 1), T(13, 104, B, 9)};
}
FootprintBook book(const std::vector<Trade>& trades) {
  FootprintBook b(FootprintBook::fixedBuckets(60000));
  for (const Trade& t : trades) b.add(t);
  return b;
}
FootprintView view(const FootprintCandle& c, FootprintParams p = {}) {
  FootprintView v;
  analyze(c, p, v);
  return v;
}
// "bid x ask" of every row, lowest first
std::vector<std::pair<double, double>> cells(const FootprintView& v) {
  std::vector<std::pair<double, double>> out;
  for (const auto& r : v.rows) out.push_back({r.bid, r.ask});
  return out;
}
std::vector<int> flagged(const FootprintView& v, uint8_t flag) {
  std::vector<int> out;
  for (size_t i = 0; i < v.rows.size(); ++i)
    if (v.rows[i].flags & flag) out.push_back(int(v.rows[i].ticks));
  return out;
}
Decimal dec(const char* s) { return parseDecimal(s, s + std::strlen(s)); }
}  // namespace

TEST("footprint: bid x ask per level, delta, volume and POC (hand-made candle)") {
  const FootprintBook b = book(mainCandle());
  CHECK_EQ(int(b.candles().size()), 1);
  const FootprintCandle& c = b.candles().front();
  CHECK_EQ(c.startMs, int64_t(0));
  CHECK_EQ(int(c.trades), 13);
  CHECK_EQ(c.bidVolume, 23.0);
  CHECK_EQ(c.askVolume, 37.0);
  CHECK_EQ(c.volume(), 60.0);
  CHECK_EQ(c.delta(), 14.0);
  const FootprintView v = view(c);
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{12, 1}, {4, 2}, {3, 9}, {2, 12}, {1, 9}, {1, 4}}));
  CHECK_EQ(v.rows.front().ticks, int64_t(100));
  CHECK_EQ(v.rows.back().ticks, int64_t(105));
  std::vector<double> deltas;
  for (const auto& r : v.rows) deltas.push_back(r.delta());
  CHECK(deltas == (std::vector<double>{-11, -2, 6, 10, 8, 3}));
  CHECK_EQ(v.poc, 3);
  CHECK_EQ(v.rows[3].ticks, int64_t(103));
  CHECK(v.rows[3].flags & Poc);
  CHECK_EQ(flagged(v, Poc), std::vector<int>{103});
  CHECK_EQ(v.maxRowVolume, 14.0);
  CHECK_EQ(v.maxAbsRowDelta, 11.0);
  CHECK_EQ(v.volume, 60.0);
  CHECK_EQ(v.delta, 14.0);
}

TEST("footprint: open/high/low/close come from the trades (lowest and highest id)") {
  const FootprintBook bk1 = book(mainCandle());
  const FootprintCandle& c = bk1.candles().front();
  CHECK_EQ(c.open, int64_t(102));
  CHECK_EQ(c.close, int64_t(104));
  CHECK_EQ(c.high, int64_t(105));
  CHECK_EQ(c.low, int64_t(100));
  CHECK_EQ(c.openId, int64_t(1));
  CHECK_EQ(c.closeId, int64_t(13));
  // the same trades backwards (history arriving newest first): same candle
  auto rev = mainCandle();
  std::reverse(rev.begin(), rev.end());
  const FootprintBook bk2 = book(rev);
  const FootprintCandle& r = bk2.candles().front();
  CHECK_EQ(r.open, int64_t(102));
  CHECK_EQ(r.close, int64_t(104));
}

TEST("footprint: diagonal imbalances and the edge rule (no buy on the lowest row, no sell on the highest)") {
  const FootprintBook bk3 = book(mainCandle());
  const FootprintCandle& c = bk3.candles().front();
  const FootprintView v = view(c);
  CHECK_EQ(flagged(v, BuyImbalance), (std::vector<int>{103, 104, 105}));
  CHECK_EQ(flagged(v, SellImbalance), std::vector<int>{100});
  // even with a ratio that lets everything through, the edges never compare with a row outside the candle
  FootprintParams any;
  any.ratio = 0;
  const FootprintView w = view(c, any);
  CHECK_EQ(flagged(w, BuyImbalance), (std::vector<int>{101, 102, 103, 104, 105}));
  CHECK_EQ(flagged(w, SellImbalance), (std::vector<int>{100, 101, 102, 103, 104}));
}

TEST("footprint: an untraded row inside the candle counts 0; minVolume silences thin levels") {
  // 100: 5 x 0, 101: nothing, 102: 0 x 1
  const FootprintBook b = book({T(1, 100, S, 5), T(2, 102, B, 1)});
  const FootprintView v = view(b.candles().front());
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{5, 0}, {0, 0}, {0, 1}}));
  CHECK_EQ(flagged(v, BuyImbalance), std::vector<int>{102});    // 1 ≥ 3·0
  CHECK_EQ(flagged(v, SellImbalance), std::vector<int>{100});   // 5 ≥ 3·0
  FootprintParams p;
  p.minVolume = 2;
  const FootprintView m = view(b.candles().front(), p);
  CHECK(flagged(m, BuyImbalance).empty());                       // 1 < 2
  CHECK_EQ(flagged(m, SellImbalance), std::vector<int>{100});
  // two empty rows side by side: 0 ≥ 3·0 is not an imbalance (the compared volume must be > 0)
  const FootprintBook e = book({T(1, 100, S, 5), T(2, 103, B, 1)});
  const FootprintView ev = view(e.candles().front());
  CHECK_EQ(flagged(ev, BuyImbalance), std::vector<int>{103});
  CHECK_EQ(flagged(ev, SellImbalance), std::vector<int>{100});
}

TEST("footprint: the ratio is inclusive (ask = 3 × bid is an imbalance, just below is not)") {
  const FootprintView eq = view(book({T(1, 100, S, 3), T(2, 101, B, 9)}).candles().front());
  CHECK_EQ(flagged(eq, BuyImbalance), std::vector<int>{101});
  const FootprintView below = view(book({T(1, 100, S, 3), T(2, 101, B, 8.5)}).candles().front());
  CHECK(flagged(below, BuyImbalance).empty());
}

TEST("footprint: stacked imbalances (N consecutive rows of one side), N and minVolume configurable") {
  const FootprintBook bk4 = book(mainCandle());
  const FootprintCandle& c = bk4.candles().front();
  FootprintView v = view(c);
  CHECK_EQ(int(v.stacks.size()), 1);
  CHECK(v.stacks[0].side == Side::Buy);
  CHECK_EQ(v.rows[size_t(v.stacks[0].fromRow)].ticks, int64_t(103));
  CHECK_EQ(v.rows[size_t(v.stacks[0].toRow)].ticks, int64_t(105));
  FootprintParams p;
  p.stackMin = 4;
  CHECK(view(c, p).stacks.empty());
  p = {};
  p.minVolume = 5;               // 105's ask (4) drops out: 103-104 only
  CHECK(view(c, p).stacks.empty());
  p.stackMin = 2;
  v = view(c, p);
  CHECK_EQ(int(v.stacks.size()), 1);
  CHECK_EQ(v.rows[size_t(v.stacks[0].fromRow)].ticks, int64_t(103));
  CHECK_EQ(v.rows[size_t(v.stacks[0].toRow)].ticks, int64_t(104));
  // a sell stack: 100..102 each 9 x 1, 103 1 x 1 → sell at 100, 101, 102 (9 ≥ 3·1); 103 is the highest row
  const FootprintBook s = book({T(1, 100, S, 9), T(2, 100, B, 1), T(3, 101, S, 9), T(4, 101, B, 1), T(5, 102, S, 9), T(6, 102, B, 1),
                                T(7, 103, S, 1), T(8, 103, B, 1)});
  const FootprintView sv = view(s.candles().front());
  CHECK_EQ(flagged(sv, SellImbalance), (std::vector<int>{100, 101, 102}));
  CHECK(flagged(sv, BuyImbalance).empty());
  CHECK_EQ(int(sv.stacks.size()), 1);
  CHECK(sv.stacks[0].side == Side::Sell);
  CHECK_EQ(sv.stacks[0].fromRow, 0);
  CHECK_EQ(sv.stacks[0].toRow, 2);
}

TEST("footprint: grouping rows of 2 and 4 ticks") {
  const FootprintBook bk5 = book(mainCandle());
  const FootprintCandle& c = bk5.candles().front();
  FootprintParams p;
  p.groupTicks = 2;   // 100-101: 16 x 3, 102-103: 5 x 21, 104-105: 2 x 13
  FootprintView v = view(c, p);
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{16, 3}, {5, 21}, {2, 13}}));
  CHECK_EQ(v.rows[1].ticks, int64_t(102));
  CHECK_EQ(v.poc, 1);                                          // 26
  CHECK(flagged(v, BuyImbalance).empty());                     // 21 < 48, 13 < 15
  CHECK(flagged(v, SellImbalance).empty());                    // 16 < 63, 5 < 39
  p.groupTicks = 4;   // 100-103: 21 x 24, 104-107: 2 x 13
  v = view(c, p);
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{21, 24}, {2, 13}}));
  CHECK_EQ(v.rows[1].ticks, int64_t(104));
  CHECK_EQ(v.poc, 0);
  // a candle whose low is not on a row boundary: rows still start at a multiple of the grouping
  const FootprintBook odd = book({T(1, 101, B, 1), T(2, 102, S, 2), T(3, 103, B, 3)});
  p.groupTicks = 2;
  v = view(odd.candles().front(), p);
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{0, 1}, {2, 3}}));
  CHECK_EQ(v.rows[0].ticks, int64_t(100));
  CHECK_EQ(v.rows[1].ticks, int64_t(102));
}

TEST("footprint: POC ties go to the lowest row") {
  const FootprintView v = view(book({T(1, 100, B, 2), T(2, 101, S, 5), T(3, 102, B, 5)}).candles().front());
  CHECK_EQ(v.poc, 1);
  CHECK_EQ(flagged(v, Poc), std::vector<int>{101});
}

TEST("footprint: prices rounded to the tick before aggregation (tick 0.5)") {
  const TickScale sc(dec("0.5"));
  FootprintBook b(FootprintBook::fixedBuckets(60000));
  int64_t id = 0;
  for (const char* px : {"100.2", "100.0", "100.3", "100.25", "100.74", "100.76"}) b.add(T(++id, sc.ticksOf(dec(px)), B, 1));
  const FootprintView v = view(b.candles().front());
  // 100.2 and 100.0 → 100.0 (200); 100.3, 100.25 (half → away from 0), 100.74 → 100.5 (201); 100.76 → 101.0 (202)
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{0, 2}, {0, 3}, {0, 1}}));
  CHECK_EQ(v.rows[0].ticks, int64_t(200));
  CHECK(sc.priceOf(v.rows[1].ticks) == 100.5);
}

TEST("footprint: trades classified by the tick rule (no side in the feed)") {
  SideClassifier cls;
  FootprintBook b(FootprintBook::fixedBuckets(60000));
  const int64_t px[] = {100, 101, 101, 100};
  for (int i = 0; i < 4; ++i) {
    Trade t = T(i + 1, px[i], Side::Unknown, 1);
    t.side = cls.classify(t.ticks);
    t.from = cls.lastFrom();
    b.add(t);
  }
  const FootprintCandle& c = b.candles().front();
  // unknown (first), buy, buy, sell → the first trade counts in the volume only
  CHECK_EQ(c.unknownVolume, 1.0);
  CHECK_EQ(c.askVolume, 2.0);
  CHECK_EQ(c.bidVolume, 1.0);
  CHECK_EQ(c.volume(), 4.0);
  CHECK_EQ(c.delta(), 1.0);
  CHECK_EQ(int(c.approxTrades), 4);
  const FootprintView v = view(c);
  CHECK(cells(v) == (std::vector<std::pair<double, double>>{{1, 0}, {0, 2}}));
  CHECK_EQ(v.volume, 4.0);
}

TEST("footprint: trades go to their bar; older history makes earlier candles; partial first bar") {
  FootprintBook b(FootprintBook::fixedBuckets(60000));
  b.add(T(10, 100, B, 1, 120000));
  b.add(T(11, 101, B, 1, 179999));
  b.add(T(12, 102, S, 1, 180000));
  b.add(T(5, 99, S, 2, 60500));    // history: an earlier bar, inserted in front
  b.add(T(3, 98, S, 1, 59999));
  CHECK_EQ(int(b.candles().size()), 4);
  std::vector<int64_t> starts;
  for (const auto& c : b.candles()) starts.push_back(c.startMs);
  CHECK(starts == (std::vector<int64_t>{0, 60000, 120000, 180000}));
  CHECK(b.find(120000) && b.find(120000)->trades == 2);
  CHECK(!b.find(240000));
  b.setCoveredFrom(59999);         // the first trade we have is inside the bar starting at 0
  CHECK(b.partial(*b.find(0)));
  CHECK(!b.partial(*b.find(60000)));
  b.clear();
  CHECK(b.candles().empty());
  const auto bucket = FootprintBook::fixedBuckets(60000);
  CHECK_EQ(bucket(-1), int64_t(-60000));
  CHECK_EQ(bucket(-60000), int64_t(-60000));
  CHECK_EQ(bucket(59999), int64_t(0));
}

TEST("footprint: any arrival order gives the same candles (200 shuffles of 2000 trades)") {
  std::mt19937 rng(3);
  std::vector<Trade> trades;
  int64_t px = 1000;
  for (int i = 0; i < 2000; ++i) {
    px += int64_t(rng() % 5) - 2;
    // quantities in 1/64 steps: sums are exact in any order
    trades.push_back(T(i + 1, px, rng() % 2 ? B : S, double(1 + rng() % 256) / 64, int64_t(i) * 150));
  }
  const FootprintBook ref = book(trades);
  FootprintParams p;
  p.groupTicks = 2;
  bool same = true;
  for (int k = 0; k < 200 && same; ++k) {
    std::shuffle(trades.begin(), trades.end(), rng);
    const FootprintBook b = book(trades);
    same = b.candles().size() == ref.candles().size();
    for (size_t i = 0; same && i < b.candles().size(); ++i) {
      const auto &x = b.candles()[i], &y = ref.candles()[i];
      same = x.startMs == y.startMs && x.open == y.open && x.close == y.close && x.high == y.high && x.low == y.low &&
             x.bidVolume == y.bidVolume && x.askVolume == y.askVolume && x.cells.size() == y.cells.size();
      for (size_t j = 0; same && j < x.cells.size(); ++j)
        same = x.cells[j].ticks == y.cells[j].ticks && x.cells[j].bid == y.cells[j].bid && x.cells[j].ask == y.cells[j].ask;
      const FootprintView vx = view(x, p), vy = view(y, p);
      same = same && vx.poc == vy.poc && cells(vx) == cells(vy) && vx.stacks.size() == vy.stacks.size();
    }
  }
  CHECK(same);
}

TEST("footprint: adding to known levels and analyzing into a warm view allocate nothing") {
  FootprintBook b(FootprintBook::fixedBuckets(60000));
  for (int64_t px = 100; px <= 120; ++px) b.add(T(px, px, B, 1));   // every level exists
  FootprintView v;
  FootprintParams p;
  analyze(b.candles().front(), p, v);                               // warm the view
  std::mt19937 rng(5);
  alloccount::Scope s;
  for (int i = 0; i < 10000; ++i) b.add(T(1000 + i, 100 + int64_t(rng() % 21), rng() % 2 ? B : S, 1));
  for (int i = 0; i < 100; ++i) analyze(b.candles().front(), p, v);
  const size_t n = s.count();
  CHECK_EQ(int(n), 0);
  CHECK_EQ(int(b.candles().front().trades), 10021);
}

TEST("footprint: speed (printed): add per trade, analyze per candle") {
  // a BTCUSDT-like perpetual: random walk of 0.1 ticks, 1-minute bars, ~150 trades a second over 2 hours
  std::mt19937 rng(9);
  std::vector<Trade> trades;
  int64_t px = 639000;
  for (int i = 0; i < 1000000; ++i) {
    px += int64_t(rng() % 3) - 1;
    trades.push_back(T(i + 1, px, rng() % 2 ? B : S, 0.001 * double(1 + rng() % 50), int64_t(i) * 7));
  }
  FootprintBook b(FootprintBook::fixedBuckets(60000));
  const auto t0 = std::chrono::steady_clock::now();
  for (const Trade& t : trades) b.add(t);
  const auto t1 = std::chrono::steady_clock::now();
  FootprintView v;
  FootprintParams p;
  size_t rows = 0;
  for (const auto& c : b.candles()) { analyze(c, p, v); rows += v.rows.size(); }
  const auto t2 = std::chrono::steady_clock::now();
  const double addNs = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(trades.size());
  const double anaUs = std::chrono::duration<double, std::micro>(t2 - t1).count() / double(b.candles().size());
  std::printf("  footprint: %.0f ns per trade added, %.1f µs per candle analyzed (%zu candles, %.0f rows each)\n", addNs, anaUs,
              b.candles().size(), double(rows) / double(b.candles().size()));
  CHECK(addNs < 2000);
  CHECK(anaUs < 2000);
}

TEST("footprint settings: defaults, round trip through JSON, clamping of bad values") {
  const FootprintSettings d = FootprintSettings::fromJson({});
  CHECK(d.mode == FootprintSettings::Mode::BidAsk);
  CHECK_EQ(int(d.rows), 0);
  CHECK_EQ(d.ratio, 3.0);
  CHECK_EQ(d.stackMin, 3);
  CHECK(d.showPoc && d.showTotals);
  CHECK(d.toJson().isEmpty());   // all defaults: nothing saved
  FootprintSettings s;
  s.mode = FootprintSettings::Mode::Profile;
  s.rows = 4;
  s.ratio = 2.5;
  s.minVolume = 0.25;
  s.stackMin = 4;
  s.showPoc = false;
  s.buyColor = "#00ff88";
  const FootprintSettings r = FootprintSettings::fromJson(s.toJson());
  CHECK(r.mode == FootprintSettings::Mode::Profile);
  CHECK_EQ(int(r.rows), 4);
  CHECK_EQ(r.ratio, 2.5);
  CHECK_EQ(r.minVolume, 0.25);
  CHECK_EQ(r.stackMin, 4);
  CHECK(!r.showPoc && r.showTotals);
  CHECK_EQ(r.buyColor, QString("#00ff88"));
  CHECK(r.sellColor.isEmpty());
  CHECK_EQ(int(r.params().groupTicks), 4);
  CHECK_EQ(r.params().ratio, 2.5);
  // bad values: clamped or back to defaults, never an error
  const FootprintSettings b = FootprintSettings::fromJson(QJsonObject{{"mode", "nope"}, {"rows", -3}, {"ratio", 0.2}, {"minVolume", -1},
                                                                       {"stack", 99}, {"buyColor", "red"}, {"sellColor", "#ABCDEF"}});
  CHECK(b.mode == FootprintSettings::Mode::BidAsk);
  CHECK_EQ(int(b.rows), 0);
  CHECK_EQ(b.ratio, 1.0);
  CHECK_EQ(b.minVolume, 0.0);
  CHECK_EQ(b.stackMin, 20);
  CHECK(b.buyColor.isEmpty());
  CHECK_EQ(b.sellColor, QString("#abcdef"));
  CHECK_EQ(FootprintSettings::fromJson(QJsonObject{{"ratio", "4"}}).ratio, 4.0);   // numbers typed as text
  CHECK_EQ(int(FootprintSettings::fromJson({}).params().groupTicks), 1);
}
