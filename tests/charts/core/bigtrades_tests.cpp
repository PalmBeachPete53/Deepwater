// Order flow, step C1: Big Trades (src/charts/core/BigTrades) — aggregation, thresholds, bubbles. Hand-made trades,
// expected values worked out by hand in the comments.
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/BigTrades.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

using namespace charts::flow;

namespace {
constexpr Side B = Side::Buy, S = Side::Sell;
Trade T(int64_t id, int64_t ms, int64_t ticks, Side side, double qty) {
  Trade t;
  t.id = id;
  t.timeMs = ms;
  t.ticks = ticks;
  t.side = side;
  t.qty = qty;
  return t;
}
// every aggregate of a list of trades, the open one flushed at the end
std::vector<AggregatedTrade> aggregate(const std::vector<Trade>& trades, Aggregator::Params p = {}) {
  Aggregator a(p);
  std::vector<AggregatedTrade> out;
  for (const Trade& t : trades)
    if (auto c = a.add(t)) out.push_back(*c);
  if (auto c = a.flush(INT64_MAX)) out.push_back(*c);
  return out;
}
AggregatedTrade agg(int64_t ms, double volume, Side side = B, int64_t ticks = 1000) {
  AggregatedTrade a;
  a.startMs = a.endMs = ms;
  a.side = side;
  a.volume = volume;
  a.vwapTicks = double(ticks);
  a.minTicks = a.maxTicks = ticks;
  a.executions = 1;
  return a;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ aggregation

TEST("bigtrades aggregation: same side, same price, inside the window → one order") {
  // 3 buys at 100 at 0, 10, 40 ms (window 50 ms from the first): one order of 6
  const auto v = aggregate({T(1, 0, 100, B, 1), T(2, 10, 100, B, 2), T(3, 40, 100, B, 3)});
  CHECK_EQ(int(v.size()), 1);
  CHECK_EQ(v[0].volume, 6.0);
  CHECK_EQ(v[0].executions, 3);
  CHECK_EQ(v[0].startMs, int64_t(0));
  CHECK_EQ(v[0].endMs, int64_t(40));
  CHECK_EQ(v[0].firstId, int64_t(1));
  CHECK_EQ(v[0].lastId, int64_t(3));
  CHECK(v[0].side == B);
  CHECK(!v[0].approximate);
}

TEST("bigtrades aggregation: a change of side, of price or a trade past the window starts a new order") {
  // side change
  auto v = aggregate({T(1, 0, 100, B, 1), T(2, 5, 100, S, 1), T(3, 6, 100, B, 1)});
  CHECK_EQ(int(v.size()), 3);
  // price change in same-price mode
  v = aggregate({T(1, 0, 100, B, 1), T(2, 5, 101, B, 1)});
  CHECK_EQ(int(v.size()), 2);
  // the window counts from the FIRST trade: 0, 30, 60 ms → [0, 30] and [60] although each gap is 30 ms
  v = aggregate({T(1, 0, 100, B, 1), T(2, 30, 100, B, 1), T(3, 60, 100, B, 1)});
  CHECK_EQ(int(v.size()), 2);
  CHECK_EQ(v[0].volume, 2.0);
  CHECK_EQ(v[1].startMs, int64_t(60));
  // exactly at the window's end still joins (≤ 50 ms)
  v = aggregate({T(1, 0, 100, B, 1), T(2, 50, 100, B, 1)});
  CHECK_EQ(int(v.size()), 1);
}

TEST("bigtrades aggregation: 'same timestamp only' (window 0)") {
  Aggregator::Params p;
  p.windowMs = 0;
  const auto v = aggregate({T(1, 7, 100, B, 1), T(2, 7, 100, B, 1), T(3, 8, 100, B, 1)}, p);
  CHECK_EQ(int(v.size()), 2);
  CHECK_EQ(v[0].volume, 2.0);
  CHECK_EQ(v[1].volume, 1.0);
}

TEST("bigtrades aggregation: sweep mode — VWAP and min/max over the levels taken") {
  Aggregator::Params p;
  p.sweep = true;
  // buys lifting 100 (1), 101 (1), 104 (2): VWAP = (100 + 101 + 208) / 4 = 102.25, min 100, max 104
  auto v = aggregate({T(1, 0, 100, B, 1), T(2, 1, 101, B, 1), T(3, 2, 104, B, 2)}, p);
  CHECK_EQ(int(v.size()), 1);
  CHECK_EQ(v[0].vwapTicks, 102.25);
  CHECK_EQ(v[0].minTicks, int64_t(100));
  CHECK_EQ(v[0].maxTicks, int64_t(104));
  CHECK_EQ(v[0].volume, 4.0);
  // a buy at a LOWER price is not the same sweep
  v = aggregate({T(1, 0, 101, B, 1), T(2, 1, 100, B, 1)}, p);
  CHECK_EQ(int(v.size()), 2);
  // sells sweep downwards: 100, 99, 97 → one order; VWAP with 1 @ 100 and 3 @ 96 = (100 + 288) / 4 = 97
  v = aggregate({T(1, 0, 100, S, 1), T(2, 1, 99, S, 1), T(3, 2, 97, S, 1)}, p);
  CHECK_EQ(int(v.size()), 1);
  CHECK_EQ(v[0].minTicks, int64_t(97));
  v = aggregate({T(1, 0, 100, S, 1), T(2, 1, 96, S, 3)}, p);
  CHECK_EQ(v[0].vwapTicks, 97.0);
  // a sell at a HIGHER price is not the same sweep
  CHECK_EQ(int(aggregate({T(1, 0, 100, S, 1), T(2, 1, 101, S, 1)}, p).size()), 2);
  // same price mode keeps those apart
  CHECK_EQ(int(aggregate({T(1, 0, 100, S, 1), T(2, 1, 99, S, 1)}).size()), 2);
}

TEST("bigtrades aggregation: flush closes the open order only once its window has passed") {
  Aggregator a;
  CHECK(!a.add(T(1, 1000, 100, B, 1)));
  CHECK(!a.flush(1050));       // 50 ms after the first trade: a trade could still join
  auto c = a.flush(1051);
  CHECK(c.has_value());
  CHECK_EQ(c->volume, 1.0);
  CHECK(!a.hasOpen());
  CHECK(!a.flush(5000));
  Aggregator::Params p;
  p.windowMs = 0;
  Aggregator z(p);
  z.add(T(1, 1000, 100, B, 1));
  CHECK(!z.flush(1000));
  CHECK(z.flush(1001).has_value());
}

TEST("bigtrades aggregation: sides from the tick rule (feed without sides) are flagged approximate") {
  SideClassifier cls;
  std::vector<Trade> trades;
  // 100, 101, 101, 101: unknown, buy, buy, buy → [unknown 100], [buy 101 × 3]
  const int64_t px[] = {100, 101, 101, 101};
  for (int i = 0; i < 4; ++i) {
    Trade t = T(i + 1, i, px[i], Side::Unknown, 1);
    t.side = cls.classify(t.ticks);
    t.from = cls.lastFrom();
    trades.push_back(t);
  }
  const auto v = aggregate(trades);
  CHECK_EQ(int(v.size()), 2);
  CHECK(v[0].side == Side::Unknown);
  CHECK(v[1].side == B);
  CHECK_EQ(v[1].volume, 3.0);
  CHECK(v[1].approximate);
}

// ------------------------------------------------------------------------------------------------ percentile

TEST("bigtrades percentile: linear interpolation on a known distribution (1 … 100)") {
  RollingPercentile r;
  for (int v = 100; v >= 1; --v) r.add(0, v);   // any order
  CHECK_EQ(int(r.size()), 100);
  CHECK_NEAR(r.quantile(99), 99.01, 1e-12);       // rank 0.99 · 99 = 98.01 → 99 + 0.01 · (100 − 99)
  CHECK_NEAR(r.quantile(50), 50.5, 1e-12);
  CHECK_EQ(r.quantile(0), 1.0);
  CHECK_EQ(r.quantile(100), 100.0);
  RollingPercentile one;
  one.add(0, 7);
  CHECK_EQ(one.quantile(99), 7.0);
  CHECK(std::isnan(RollingPercentile().quantile(50)));
}

TEST("bigtrades percentile: the sliding window drops old values; equals a full sort at every step (20 000 random ops)") {
  std::mt19937 rng(4);
  RollingPercentile r;
  std::deque<std::pair<int64_t, double>> ref;
  int64_t t = 0;
  bool same = true;
  for (int i = 0; i < 20000 && same; ++i) {
    t += int64_t(rng() % 100);
    const double v = double(rng() % 1000) / 8;   // many duplicates on purpose
    r.add(t, v);
    ref.emplace_back(t, v);
    const int64_t cut = t - 30000;
    r.expireBefore(cut);
    while (!ref.empty() && ref.front().first < cut) ref.pop_front();
    if (i % 37 == 0) {
      std::vector<double> s;
      for (const auto& x : ref) s.push_back(x.second);
      std::sort(s.begin(), s.end());
      for (double p : {0.0, 50.0, 90.0, 99.0, 99.9, 100.0}) {
        const double rank = p / 100 * double(s.size() - 1);
        const size_t lo = size_t(std::floor(rank));
        const double want = lo + 1 < s.size() ? s[lo] + (s[lo + 1] - s[lo]) * (rank - double(lo)) : s[lo];
        if (std::fabs(r.quantile(p) - want) > 1e-9) same = false;
      }
      if (r.size() != s.size()) same = false;
      if (r.size() > 0 && r.blocks() * 2 * RollingPercentile::kBlock < r.size()) same = false;   // blocks stay bounded
    }
  }
  CHECK(same);
}

TEST("bigtrades percentile: speed (printed) with a 30-minute window of a busy market") {
  std::mt19937 rng(8);
  std::lognormal_distribution<double> size(0, 1.5);
  RollingPercentile r;
  const int n = 300000;
  const auto t0 = std::chrono::steady_clock::now();
  double sink = 0;
  for (int i = 0; i < n; ++i) {
    const int64_t ms = int64_t(i) * 20;   // 50 aggregates a second → 90 000 in the window
    r.add(ms, size(rng));
    r.expireBefore(ms - 30 * 60 * 1000);
    sink += r.quantile(99);
  }
  const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / n;
  std::printf("  rolling p99: %.2f µs per aggregate (add + expire + quantile), window %zu values\n", us, r.size());
  CHECK(sink > 0);
  CHECK(r.blocks() >= r.size() / (2 * RollingPercentile::kBlock));   // split into blocks, not one long vector
  CHECK(us < 50);
}

// ------------------------------------------------------------------------------------------------ volatility

TEST("bigtrades volatility: ATR(14)/ATR(120) on 1-minute bars, clamped to [0.5, 2], 1 until 120 minutes closed") {
  // 134 calm minutes (low 1000, high 1010, close 1005: true range 10), then 14 minutes of another kind, then one
  // trade in the next minute to close the 14th
  auto run = [](int64_t lo, int64_t hi, int64_t last) {
    MinuteAtr a;
    int64_t m = 0;
    for (; m < 134; ++m) a.add(m * 60000 + 5, 1000, 1010, 1005);
    for (; m < 148; ++m) a.add(m * 60000 + 5, lo, hi, last);
    a.add(m * 60000 + 5, 1005, 1005, 1005);
    return a;
  };
  // wild: range 100 (true range 100): short 100, long (106·10 + 14·100) / 120 = 20.5 → 4.88 → clamped to 2
  MinuteAtr wild = run(1000, 1100, 1050);
  CHECK_EQ(wild.closedMinutes(), 120);   // only the last 120 are kept
  CHECK_EQ(wild.shortAtr(), 100.0);
  CHECK_NEAR(wild.longAtr(), 20.5, 1e-12);
  CHECK_EQ(wild.factor(), 2.0);
  // quiet: range 1 (true range 1 against the previous close 1005): short 1, long (1060 + 14) / 120 = 8.95 → 0.5
  MinuteAtr quiet = run(1004, 1005, 1005);
  CHECK_NEAR(quiet.longAtr(), 8.95, 1e-12);
  CHECK_EQ(quiet.factor(), 0.5);
  // in between: range 15 → short 15, long (1060 + 210) / 120 → factor 15 / 10.5833… = 1.4173…
  MinuteAtr mid = run(1000, 1015, 1005);
  CHECK_NEAR(mid.factor(), 15.0 / (1270.0 / 120.0), 1e-12);
  // the gap to the previous close counts in the true range: close 1005, next minute 1020..1030 → TR 25
  MinuteAtr gap;
  gap.add(5, 1000, 1010, 1005);
  gap.add(60005, 1020, 1030, 1025);
  gap.add(120005, 1025, 1025, 1025);
  CHECK_EQ(gap.closedMinutes(), 2);
  CHECK_EQ(gap.lastTrueRange(), 25.0);   // 1030 − 1005 (the previous close), not the bar's own 10
  // not enough minutes yet: factor 1
  MinuteAtr young;
  for (int64_t m = 0; m <= 100; ++m) young.add(m * 60000, 1000, 1000 + m, 1000);
  CHECK_EQ(young.factor(), 1.0);
}

// ------------------------------------------------------------------------------------------------ detection

TEST("bigtrades manual threshold: min, max, and max = 0 (no maximum)") {
  DetectorParams p;
  p.mode = DetectorParams::Mode::Manual;
  p.minVolume = 5;
  Detector d(p);
  CHECK(!d.judge(agg(0, 4.9)).big);
  CHECK(d.judge(agg(0, 5)).big);
  CHECK(d.judge(agg(0, 1000)).big);   // max 0: no upper limit
  p.maxVolume = 50;
  Detector t(p);
  CHECK(t.judge(agg(0, 50)).big);
  CHECK(!t.judge(agg(0, 50.5)).big);  // a tranche 5–50: bigger orders belong to another Big Trades
  CHECK_EQ(t.judge(agg(0, 7)).threshold, 5.0);
  p.minVolume = 0;   // no minimum: nothing, rather than every order
  Detector z(p);
  CHECK(!z.judge(agg(0, 1000)).big);
}

TEST("bigtrades auto threshold: the 99th percentile of the window, judged without the order itself") {
  DetectorParams p;
  p.minSamples = 100;
  p.volatility = false;
  Detector d(p);
  for (int v = 1; v <= 100; ++v) d.judge(agg(v, v));
  // window 1 … 100: p99 = 99.01
  auto v99 = d.judge(agg(200, 99));
  CHECK(v99.warm);
  CHECK_NEAR(v99.threshold, 99.01, 1e-12);
  CHECK(!v99.big);
  // the window is now 1 … 100 plus that 99 (101 sizes): rank 0.99 · 100 = 99 → sorted[99] = the second 99
  auto v100 = d.judge(agg(201, 100));
  CHECK_NEAR(v100.threshold, 99.0, 1e-12);
  CHECK(v100.big);
  // strictly above in auto mode: an order exactly at the percentile is not big (round lots cluster there)
  DetectorParams r = p;
  r.minSamples = 10;
  Detector lots(r);
  for (int i = 0; i < 200; ++i) lots.judge(agg(i, i % 4 ? 0.1 : 0.5));   // a quarter of the orders are 0.5 lots
  const auto atP = lots.judge(agg(300, 0.5));
  CHECK_EQ(atP.threshold, 0.5);
  CHECK(!atP.big);
  CHECK(lots.judge(agg(301, 0.51)).big);
  // judged before being added: 1 … 99 then 1000 → threshold from 1 … 99 (98.02), big; the next 99 sees the 1000
  DetectorParams q = p;
  q.minSamples = 99;
  Detector e(q);
  for (int v = 1; v <= 99; ++v) e.judge(agg(v, v));
  auto huge = e.judge(agg(100, 1000));
  CHECK_NEAR(huge.threshold, 98.02, 1e-12);
  CHECK(huge.big);
  auto after = e.judge(agg(101, 99));
  CHECK_NEAR(after.threshold, 99 + 0.01 * 901, 1e-9);   // rank 98.01 between 99 and 1000
  CHECK(!after.big);
}

TEST("bigtrades auto threshold: warm-up uses the fallback minimum (0 = show nothing)") {
  DetectorParams p;
  p.minSamples = 100;
  Detector none(p);
  for (int i = 0; i < 99; ++i) {
    const auto v = none.judge(agg(i, 1e9));
    if (v.big || v.warm || !std::isnan(v.threshold)) { CHECK(false); break; }
  }
  CHECK(none.judge(agg(99, 1e9)).warm == false);   // 99 in the window before this one
  CHECK(none.judge(agg(100, 1e9)).warm);           // now 100
  p.minVolume = 5;
  Detector fb(p);
  CHECK(fb.judge(agg(0, 5)).big);
  CHECK(!fb.judge(agg(1, 4)).big);
  CHECK_EQ(fb.judge(agg(2, 4)).threshold, 5.0);
}

TEST("bigtrades auto threshold: old sizes leave the window after 30 minutes") {
  DetectorParams p;
  p.minSamples = 3;
  p.volatility = false;
  Detector d(p);
  for (int i = 0; i < 3; ++i) d.judge(agg(1000 + i, 10));
  CHECK(d.current(2000).warm);
  CHECK_EQ(d.current(2000).threshold, 10.0);
  const auto later = d.judge(agg(1002 + 30 * 60 * 1000 + 1, 1));   // all three are older than 30 minutes
  CHECK(!later.warm);
  CHECK(!later.big);
}

TEST("bigtrades auto threshold: threshold = percentile × volatility factor, factor always within [0.5, 2]") {
  DetectorParams p;
  p.minSamples = 50;
  Detector d(p);
  std::mt19937 rng(2);
  std::lognormal_distribution<double> size(0, 1.2);
  int64_t px = 100000;
  bool ok = true, sawFactor = false;
  for (int i = 0; i < 40000; ++i) {
    const int64_t ms = int64_t(i) * 1000;   // one aggregate a second for 11 hours
    px += int64_t(rng() % 41) - 20 + (i > 30000 ? int64_t(rng() % 201) - 100 : 0);   // volatility jumps at the end
    AggregatedTrade a = agg(ms, size(rng), rng() % 2 ? B : S, px);
    const auto v = d.judge(a);
    if (v.warm) {
      ok = ok && v.factor >= 0.5 && v.factor <= 2.0 && std::fabs(v.threshold - v.percentileValue * v.factor) <= 1e-12 * v.threshold;
      if (v.factor > 1.2) sawFactor = true;
    }
  }
  CHECK(ok);
  CHECK(sawFactor);   // the jump in volatility raised the threshold
}

// ------------------------------------------------------------------------------------------------ bubbles

TEST("bigtrades bubbles: area proportional to volume (square root), clamped to min and max") {
  CHECK_EQ(bubbleDiameter(10, 10, 8, 40), 8.0);     // at the threshold: the smallest bubble
  CHECK_EQ(bubbleDiameter(40, 10, 8, 40), 16.0);    // 4 × the volume → 2 × the diameter, 4 × the area
  CHECK_EQ(bubbleDiameter(90, 10, 8, 40), 24.0);
  CHECK_EQ(bubbleDiameter(1e6, 10, 8, 40), 40.0);   // capped
  CHECK_EQ(bubbleDiameter(1, 10, 8, 40), 8.0);      // below the reference: still the minimum
  CHECK_EQ(bubbleDiameter(5, 0, 8, 40), 8.0);
  CHECK_EQ(bubbleOpacity(8, 8, 40, 0.35), 0.35);
  CHECK_EQ(bubbleOpacity(40, 8, 40, 0.35), 1.0);
  CHECK_NEAR(bubbleOpacity(24, 8, 40, 0.35), 0.35 + 0.65 * 0.5, 1e-12);
  CHECK_EQ(bubbleOpacity(20, 20, 20, 0.3), 1.0);
}

TEST("bigtrades alerts: a second, higher bar (factor × the threshold the order passed, factor at least 1)") {
  CHECK(!bigTradeAlert(2.9, 1, 3));
  CHECK(bigTradeAlert(3, 1, 3));
  CHECK(bigTradeAlert(1, 1, 0.5));    // a factor under 1 counts as 1: never below the display threshold
  CHECK(!bigTradeAlert(0.9, 1, 0.5));
  CHECK(!bigTradeAlert(100, 0, 3));   // no threshold (warming up, no fallback): no alert
  CHECK(!bigTradeAlert(100, std::nan(""), 3));
}

TEST("bigtrades hot path: aggregating trades allocates nothing") {
  Aggregator a;
  std::mt19937 rng(1);
  alloccount::Scope s;
  int closed = 0;
  for (int i = 0; i < 20000; ++i)
    if (a.add(T(i, int64_t(i) * 7, 1000 + int64_t(rng() % 3), rng() % 2 ? B : S, 1))) ++closed;
  const size_t n = s.count();
  CHECK_EQ(int(n), 0);
  CHECK(closed > 1000);
}
