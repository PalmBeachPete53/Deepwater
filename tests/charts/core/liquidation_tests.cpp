// Order flow, step E1: the liquidation model, on hand-made bars.
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/Liquidation.h"
#include <QElapsedTimer>
#include <cmath>
#include <cstring>
#include <random>

using namespace charts::flow;

namespace {
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
LiqBar flat(double p, double oi, double vol = 1e9, double ratio = NaN) { return {p, p, p, p, vol, oi, ratio}; }
LiqBar bar(double o, double h, double l, double c, double vol, double oi, double ratio = NaN) { return {o, h, l, c, vol, oi, ratio}; }
LiqParams only(double leverage) {
  LiqParams p;
  p.leverages = {{leverage, 1}};
  return p;
}
// rows of height 1 centred on whole prices: price 90 is in row 40 whatever the last bit of 100 × 0.9
LiqGrid unitGrid() { return {49.5, 1, 100}; }
bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps * std::max(1.0, std::fabs(b)); }
}  // namespace

TEST("liquidation price: Binance's isolated formula, with the bracket's rate and amount") {
  // Binance: LP = (WB + cum − side·Q·EP) / (Q·mmr − side·Q), WB = Q·EP / lev (one isolated position)
  auto binance = [](double ep, double lev, int side, double mmr, double cum, double notional) {
    const double q = notional / ep, wb = q * ep / lev;
    return (wb + cum - side * q * ep) / (q * mmr - side * q);
  };
  CHECK(near(liquidationPrice(60000, 10, true, 0.004, 0, 10000), binance(60000, 10, 1, 0.004, 0, 10000)));
  CHECK(near(liquidationPrice(60000, 10, false, 0.004, 0, 10000), binance(60000, 10, -1, 0.004, 0, 10000)));
  CHECK(near(liquidationPrice(60000, 10, true, 0.005, 50, 100000), binance(60000, 10, 1, 0.005, 50, 100000)));
  CHECK(near(liquidationPrice(60000, 25, false, 0.01, 1300, 600000), binance(60000, 25, -1, 0.01, 1300, 600000)));
  CHECK(near(liquidationPrice(60000, 10, true, 0.004, 0, 10000), 60000 * 0.9 / 0.996));   // 54 216.87
  CHECK(near(liquidationPrice(100, 10, true, 0, 0, 1), 90));
  CHECK(near(liquidationPrice(100, 10, false, 0, 0, 1), 110));
  // the bracket of a notional: the first whose cap holds it, the last beyond every cap
  const LeverageBracket br[] = {{50000, 125, 0.004, 0}, {600000, 100, 0.005, 50}, {3000000, 75, 0.0065, 950}};
  CHECK(bracketFor(br, 10000) == &br[0]);
  CHECK(bracketFor(br, 50000) == &br[0]);
  CHECK(bracketFor(br, 50001) == &br[1]);
  CHECK(bracketFor(br, 1e9) == &br[2]);
  CHECK(bracketFor({}, 1) == nullptr);
}

TEST("liquidation price, inverse contracts (COIN-M): margin + profit = maintenance, in the coin") {
  // Q contracts of C dollars: margin Q·C/(E·lev) coins, long profit Q·C·(1/E − 1/P), maintenance mmr·Q·C/P
  for (double lev : {5.0, 10.0, 25.0, 100.0})
    for (double mmr : {0.004, 0.01}) {
      const double E = 60000, L = liquidationPriceInverse(E, lev, true, mmr), S = liquidationPriceInverse(E, lev, false, mmr);
      CHECK(std::fabs(1 / (E * lev) + (1 / E - 1 / L) - mmr / L) < 1e-15);   // per dollar of contract
      CHECK(std::fabs(1 / (E * lev) + (1 / S - 1 / E) - mmr / S) < 1e-15);
      if (mmr < 1 / lev) {   // (at 100× with 1 % maintenance a position dies at its entry)
        CHECK(L < E);
        CHECK(S > E);
      }
    }
  // 10×: the inverse long dies about 1 % higher than the linear one (≈ $570 at 60 000)
  CHECK(liquidationPriceInverse(60000, 10, true, 0.004) - liquidationPrice(60000, 10, true, 0.004, 0, 1) > 500);
  LiqParams p;
  p.leverages = {{10, 1}};
  p.inverse = true;
  LiquidationModel m({40000, 10, 4000}, p);
  m.step({60000, 60000, 60000, 60000, 1e9, 1000, NaN});
  m.step({60000, 60000, 60000, 60000, 1e9, 1100, NaN});
  CHECK(std::fabs(m.longs()[size_t(LiqGrid{40000, 10, 4000}.rowOf(liquidationPriceInverse(60000, 10, true, 0)))] - 100) < 1e-9);
}

TEST("liquidation model: open interest rises then the price falls through the longs' level") {
  LiquidationModel m(unitGrid(), only(10));
  m.step(flat(100, 1000));                                   // first known OI: a baseline, nothing opened
  CHECK_EQ(int(m.longs()[40]), 0);
  m.step(bar(100, 101, 99, 100, 500, 1100));                 // +100 contracts at ~100: 100 longs, 100 shorts at 10×
  CHECK(near(m.longs()[40], 100));                           // long liquidation at 90 (row 40)
  CHECK(near(m.shorts()[60], 100));                          // short liquidation at 110 (row 60)
  CHECK(near(m.longColumn(1)[40], 100));
  CHECK_EQ(m.liquidatedLongs(1), 0.0);                       // opened in that bar: not liquidated by it
  // even when that bar's own range spans both levels (we don't know the order of prices inside it)
  LiquidationModel wide(unitGrid(), only(10));
  wide.step(flat(100, 1000));
  wide.step(bar(100, 111, 89, 100, 1e9, 1100));             // typical price (111 + 89 + 100) / 3 = 100
  CHECK(near(wide.longs()[40], 100));
  CHECK(near(wide.shorts()[60], 100));
  CHECK_EQ(wide.liquidatedLongs(1) + wide.liquidatedShorts(1), 0.0);
  m.step(bar(100, 100, 89, 90, 0, 1100));                    // down to 89: the longs at 90 are gone
  CHECK_EQ(m.longs()[40], 0.0);
  CHECK(near(m.liquidatedLongs(2), 100));
  CHECK(near(m.shorts()[60], 100));                          // the shorts stay
  CHECK_EQ(m.liquidatedShorts(2), 0.0);
  CHECK(near(m.longColumn(1)[40], 100));                     // earlier columns unchanged
  CHECK_EQ(m.longColumn(2)[40], 0.0f);
  m.step(bar(90, 111, 90, 111, 0, 1100));                    // up through 110: the shorts too
  CHECK(near(m.liquidatedShorts(3), 100));
  CHECK_EQ(m.bars(), 4);
}

TEST("liquidation model: a bar reaching into a price row takes that part of it") {
  LiquidationModel m({49.5, 2, 50}, only(10));               // row 20 = 89.5 … 91.5 holds the level at 90
  m.step(flat(100, 1000));
  m.step(bar(100, 100, 100, 100, 1e9, 1100));
  CHECK(near(m.longs()[20], 100));
  m.step(bar(100, 100, 90.5, 95, 0, 1100));                  // the low covers the top half of the row
  CHECK(near(m.longs()[20], 50));
  CHECK(near(m.liquidatedLongs(2), 50));
  // shorts: the high covers the bottom quarter of row 30 (109.5 … 111.5, holding 110)
  m.step(bar(95, 110, 95, 100, 0, 1100));
  CHECK(near(m.shorts()[30], 75));
  CHECK(near(m.liquidatedShorts(3), 25));
  // the consume share: only that much of a reached level goes
  LiqParams p = only(10);
  p.consume = 0.5;
  LiquidationModel h({49.5, 1, 100}, p);
  h.step(flat(100, 1000));
  h.step(flat(100, 1100));
  h.step(bar(100, 100, 80, 85, 0, 1100));
  CHECK(near(h.longs()[40], 50));
}

TEST("liquidation model: falling open interest shrinks every level; the volume caps what opened") {
  LiquidationModel m(unitGrid(), only(10));
  m.step(flat(100, 1000));
  m.step(flat(100, 1100));
  m.step(flat(100, 1045));                                   // 5 % of positions closed
  CHECK(near(m.longs()[40], 95));
  CHECK(near(m.shorts()[60], 95));
  LiquidationModel v(unitGrid(), only(10));
  v.step(flat(100, 1000));
  v.step(flat(100, 1100, 30));                               // +100 OI but only 30 traded
  CHECK(near(v.longs()[40], 30));
  // unknown OI: nothing changes, and the next known value is compared with the last known one
  v.step(flat(100, NaN));
  v.step(flat(100, 1110, 1e9));
  CHECK(near(v.longs()[40], 40));
}

TEST("liquidation model: the accounts ratio and the candle tilt longs vs shorts") {
  LiquidationModel m(unitGrid(), only(10));
  m.step(flat(100, 1000));
  m.step(flat(100, 1100, 1e9, 3.0));                         // 3 long accounts per short: L = 0.75
  CHECK(near(m.longs()[40], 150));
  CHECK(near(m.shorts()[60], 50));
  LiqParams p = only(10);
  p.candleBias = 0.5;
  LiquidationModel c(unitGrid(), p);
  c.step(flat(100, 1000));
  c.step(bar(99, 101, 99, 101, 1e9, 1100));                  // a full green bar: L = 0.5 + 0.5 → kept at 0.95
  const double entry = (101 + 99 + 101) / 3.0;
  CHECK(near(c.longs()[size_t(unitGrid().rowOf(entry * 0.9))], 190));
  CHECK(near(c.shorts()[size_t(unitGrid().rowOf(entry * 1.1))], 10));
}

TEST("liquidation model: leverage spread, bracket cap, fading, levels outside the grid") {
  // two leverages weighted 3:1
  LiqParams two;
  two.leverages = {{10, 3}, {25, 1}};
  LiquidationModel m(unitGrid(), two);
  m.step(flat(100, 1000));
  m.step(flat(100, 1100));
  CHECK(near(m.longs()[40], 75));                            // 10×: 90
  CHECK(near(m.longs()[46], 25));                            // 25×: 96
  CHECK(near(m.shorts()[54], 25));                           // 25×: 104
  // a crash through the bottom of the grid takes every long level; a spike over its top every short one
  m.step(bar(100, 100, 10, 20, 0, 1100));
  CHECK(near(m.liquidatedLongs(2), 100));
  m.step(bar(20, 500, 20, 400, 0, 1100));
  CHECK(near(m.liquidatedShorts(3), 100));
  // 125× asked, the bracket allows 50×: liquidation 1/50 away, moved by the maintenance rate
  LiqParams capped = only(125);
  capped.brackets = {{50000, 50, 0.01, 0}};
  LiquidationModel b(unitGrid(), capped);
  b.step(flat(100, 1000));
  b.step(flat(100, 1100));
  CHECK(near(b.longs()[size_t(unitGrid().rowOf(100 * 0.98 / 0.99))], 100));
  // half-life of 2 bars: two bars later, half is left
  LiqParams fade = only(10);
  fade.decayHalfLifeBars = 2;
  LiquidationModel f(unitGrid(), fade);
  f.step(flat(100, 1000));
  f.step(flat(100, 1100));
  f.step(flat(100, 1100));
  f.step(flat(100, 1100));
  CHECK(near(f.longs()[40], 50, 1e-12));
  // 5× from 100: 80 for the longs, under a grid starting at 85 → counted, not kept
  LiquidationModel o({85, 1, 50}, only(5));
  o.step(flat(100, 1000));
  o.step(flat(100, 1100));
  CHECK(near(o.outOfGrid(), 100));                           // the longs' level; the shorts' (120) is inside
  CHECK(near(o.shorts()[35], 100));
}

namespace {
struct Walk {
  std::vector<double> o, h, l, c, v, oi, ratio;
  explicit Walk(size_t n) {
    std::mt19937 rng(7);
    std::normal_distribution<double> step(0, 0.004), doi(0, 15);
    std::uniform_real_distribution<double> u(0, 1);
    double p = 60000, open = 50000;
    for (size_t i = 0; i < n; ++i) {
      const double close = p * std::exp(step(rng));
      o.push_back(p);
      c.push_back(close);
      h.push_back(std::max(p, close) * (1 + 0.002 * u(rng)));
      l.push_back(std::min(p, close) * (1 - 0.002 * u(rng)));
      v.push_back(200 + 400 * u(rng));
      open = std::max(1000.0, open + doi(rng));
      oi.push_back(i % 37 == 5 ? std::numeric_limits<double>::quiet_NaN() : open);
      ratio.push_back(0.6 + 1.6 * u(rng));
      p = close;
    }
  }
  LiqSeries series() const { return {o, h, l, c, v, oi, ratio}; }
};
LiqParams everything() {
  LiqParams p;
  p.brackets = {{50000, 125, 0.004, 0}, {600000, 100, 0.005, 50}};
  p.candleBias = 0.3;
  p.decayHalfLifeBars = 500;
  p.consume = 0.9;
  return p;
}
}  // namespace

TEST("liquidation model: bar by bar or all at once — the same bits") {
  const Walk w(600);
  const LiqGrid g{40000, 25, 1600};
  LiquidationModel all(g, everything()), parts(g, everything());
  all.run(w.series(), 0, 600);
  parts.run(w.series(), 0, 250);
  for (size_t i = 250; i < 600; ++i) parts.step(w.series().at(i));   // the live path: one bar at a time
  CHECK_EQ(all.bars(), parts.bars());
  CHECK(std::memcmp(all.longColumn(0), parts.longColumn(0), sizeof(float) * 600 * 1600) == 0);
  CHECK(std::memcmp(all.shortColumn(0), parts.shortColumn(0), sizeof(float) * 600 * 1600) == 0);
  CHECK(std::memcmp(all.longs().data(), parts.longs().data(), sizeof(double) * 1600) == 0);
  bool sameLiq = true;
  for (int i = 0; i < 600; ++i) sameLiq = sameLiq && all.liquidatedLongs(i) == parts.liquidatedLongs(i) && all.liquidatedShorts(i) == parts.liquidatedShorts(i);
  CHECK(sameLiq);
  // something actually happened: levels built up and some were liquidated
  double rest = 0, gone = 0;
  for (double x : all.longs()) rest += x;
  for (int i = 0; i < 600; ++i) gone += all.liquidatedLongs(i) + all.liquidatedShorts(i);
  CHECK(rest > 0);
  CHECK(gone > 0);
}

TEST("liquidation model: no allocation per bar once reserved; 7 days of 5 min bars well under 100 ms") {
  const Walk w(2016);
  const LiqGrid g{40000, 25, 1600};   // $25 rows over $40 000: wider than any 5× level of the walk
  LiquidationModel m(g, everything());
  m.reserve(2016);
  {
    alloccount::Scope s;
    m.run(w.series(), 0, 1000);
    CHECK_EQ(int(s.count()), 0);
  }
  LiquidationModel t(g, everything());
  t.reserve(2016);
  QElapsedTimer timer;
  timer.start();
  t.run(w.series(), 0, 2016);
  const double ms = double(timer.nsecsElapsed()) / 1e6;
  std::printf("  liquidation model: 2016 bars × 1600 rows in %.1f ms\n", ms);
  // the time limit holds for an optimised build; sanitizer builds (unoptimised, instrumented) only print it
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define DW_SANITIZED 1
#endif
#endif
#ifndef DW_SANITIZED
  CHECK(ms < 100);
#endif
}
