// Chantier F, step F1: pivots (window and zigzag), equal highs / lows, sweeps (wick vs close), tests, incremental ==
// complete — on hand-made bars.
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/LiquidityLevels.h"
#include <cmath>
#include <random>

using namespace charts;
using namespace charts::smc;

namespace {
Bar hl(double h, double l, int64_t t = 0) { return {t, (h + l) / 2, h, l, (h + l) / 2, 1}; }
Bar ohlc(double o, double h, double l, double c, int64_t t = 0) { return {t, o, h, l, c, 1}; }
std::vector<Bar> fromHighs(std::initializer_list<double> highs) {
  std::vector<Bar> v;
  int64_t t = 1000;
  for (double h : highs) v.push_back(hl(h, h - 1, t += 60));
  return v;
}
SwingParams window(int l, int r) {
  SwingParams p;
  p.left = l;
  p.right = r;
  return p;
}
}  // namespace

TEST("swings (window): a high is a pivot over N bars each side, confirmed N bars later; edges; equal tops") {
  //                               0  1  2  3  4  5  6  7  8  9
  const auto bars = fromHighs({1, 2, 5, 3, 2, 4, 4, 1, 2, 6});
  const SwingResult r = detectSwings(bars, window(2, 2));
  std::vector<int> highs;
  for (const Pivot& p : r.confirmed)
    if (p.high) highs.push_back(p.index);
  CHECK_EQ(int(highs.size()), 2);
  CHECK_EQ(highs[0], 2);                    // 5 > 1, 2 and ≥ 3, 2
  CHECK_EQ(highs[1], 5);                    // 4 and 4: the left one (strictly above its left, at least as high as its right)
  for (const Pivot& p : r.confirmed)
    if (p.high && p.index == 2) {
      CHECK_EQ(p.confirmedAt, 4);           // two bars later
      CHECK_EQ(p.time, bars[2].time);
    }
  // the last bar (6) is a high so far, but only provisional: no bars after it yet
  bool provisional9 = false;
  for (const Pivot& p : r.provisional) provisional9 = provisional9 || (p.high && p.index == 9 && p.confirmedAt == -1);
  CHECK(provisional9);
  // the first bars can't be pivots (no full window before them), even the highest
  const SwingResult first = detectSwings(fromHighs({9, 1, 2, 1, 1, 1}), window(2, 2));
  for (const Pivot& p : first.confirmed) CHECK(!(p.high && p.index < 2));
  // lows mirrored: the lowest low of bars 3..7 → index 4 (lows = high − 1)
  bool low7 = false;
  for (const Pivot& p : r.confirmed) low7 = low7 || (!p.high && p.index == 7 && p.price == 0);
  CHECK(low7);
}

TEST("swings (zigzag): a high confirmed when the price falls back by the percentage (or ATRs) from it") {
  SwingParams p;
  p.method = PivotMethod::Percent;
  p.percent = 5;
  // up to 110 (bar 3), down: 104.6 isn't 5 % below 110, 104.4 is → confirmed on bar 5; then up 5 % from the low → low
  std::vector<Bar> b = {hl(100, 99, 1), hl(105, 101, 2), hl(108, 104, 3), hl(110, 106, 4), hl(108, 104.6, 5), hl(106, 104.4, 6),
                        hl(105, 103, 7), hl(109, 104, 8)};
  const SwingResult r = detectSwings(b, p);
  // first: the low of bar 0 (99), confirmed by bar 1 rising 5 % above it (105 ≥ 103.95)
  CHECK(!r.confirmed[0].high);
  CHECK_EQ(r.confirmed[0].index, 0);
  CHECK_EQ(r.confirmed[0].confirmedAt, 1);
  CHECK(r.confirmed.size() >= 3);
  CHECK(r.confirmed[1].high);
  CHECK_EQ(r.confirmed[1].index, 3);
  CHECK_EQ(r.confirmed[1].price, 110.0);
  CHECK_EQ(r.confirmed[1].confirmedAt, 5);
  // the low: 103 at bar 6 (index 6), confirmed when 103 × 1.05 = 108.15 is reached (bar 7: 109)
  bool low = false;
  for (const Pivot& q : r.confirmed) low = low || (!q.high && q.index == 6 && q.confirmedAt == 7);
  CHECK(low);
  // ATR: nothing confirmed before the ATR exists (14 bars), then reversals of 1.5 ATR
  SwingParams a;
  a.method = PivotMethod::Atr;
  std::vector<Bar> walk;
  for (int i = 0; i < 40; ++i) walk.push_back(hl(100 + 10 * std::sin(i / 4.0) + 1, 100 + 10 * std::sin(i / 4.0) - 1, i));
  const SwingResult ra = detectSwings(walk, a);
  CHECK(!ra.confirmed.empty());
  CHECK(ra.confirmed.front().confirmedAt >= 13);
}

namespace {
EngineParams pct(double tolPct, double thickPct = 0.1, SweepRule rule = SweepRule::Wick) {
  EngineParams e;
  e.swing = window(1, 1);
  e.toleranceUnit = SizeUnit::Percent;
  e.tolerance = tolPct;
  e.thicknessUnit = SizeUnit::Percent;
  e.thickness = thickPct;
  e.sweep = rule;
  return e;
}
// two tops of 100 and 100.05 apart from each other, then quiet bars under them
std::vector<Bar> twoTops() { return {hl(98, 97), hl(100, 98), hl(98, 96), hl(100.05, 98), hl(98, 96.5), hl(98.5, 97)}; }
}  // namespace

TEST("levels: equal highs grouped within the tolerance (most extreme price, members, stable id) — or not") {
  LiquidityLevelEngine wide(pct(0.1));
  wide.run(twoTops());
  CHECK_EQ(int(wide.active(LevelSide::BuySide).size()), 1);
  const Level& l = wide.levels()[wide.active(LevelSide::BuySide)[0]];
  CHECK_EQ(l.members, 2);
  CHECK(l.kind == LevelKind::Equal);
  CHECK_EQ(l.price, 100.05);                     // the stops sit above the higher top
  CHECK_EQ(l.id, 0u);                            // the first top's id, kept
  CHECK_EQ(l.originIndex, 1);
  CHECK(std::fabs(l.zoneHigh - (100.05 + 0.10005)) < 1e-9);   // zone above it, 0.1 % thick
  // the second top a little lower: the level stays at the higher first one
  LiquidityLevelEngine lower(pct(0.1));
  lower.run(std::vector<Bar>{hl(98, 97), hl(100.05, 98), hl(98, 96), hl(100, 98), hl(98, 96.5), hl(98.5, 97)});
  CHECK_EQ(lower.levels()[lower.active(LevelSide::BuySide)[0]].price, 100.05);
  CHECK_EQ(lower.levels()[lower.active(LevelSide::BuySide)[0]].members, 2);
  // 0.01 %: 100 and 100.05 (0.05 % apart) are not equal — the second top went through the first: two levels, the
  // first swept by the second's bar
  LiquidityLevelEngine tight(pct(0.01));
  tight.run(twoTops());
  std::vector<Level> buy;
  for (const Level& x : tight.levels())
    if (x.side == LevelSide::BuySide) buy.push_back(x);
  CHECK_EQ(int(buy.size()), 2);
  CHECK_EQ(int(tight.active(LevelSide::BuySide).size()), 1);
  CHECK(buy[0].swept);
  CHECK_EQ(buy[0].sweptIndex, 3);
  CHECK_EQ(buy[1].price, 100.05);
  CHECK_EQ(buy[1].members, 1);
  CHECK(wide.score(l) > tight.score(buy[1]));
}

TEST("levels: swept by a wick or only by a close; tests counted once per bar; depth and sweeping bar kept; gaps") {
  // one top at 100 (zone 100 … 100.1)
  std::vector<Bar> base = {hl(98, 97, 1), hl(100, 98, 2), hl(98, 96, 3)};
  const Bar poke = ohlc(98, 100.5, 97.5, 99.8, 4);   // wick above, close below
  const Bar close = ohlc(99, 100.3, 98.5, 100.2, 5); // closes above
  LiquidityLevelEngine w(pct(0.01));
  w.run(base);
  w.step(poke);
  CHECK(w.levels()[0].swept);
  CHECK_EQ(w.levels()[0].sweptIndex, 3);
  CHECK_EQ(w.levels()[0].sweptTime, int64_t(4));
  CHECK(std::fabs(w.levels()[0].depth - 0.5) < 1e-12);
  CHECK(w.active(LevelSide::BuySide).empty());
  LiquidityLevelEngine c(pct(0.01, 0.1, SweepRule::Close));
  c.run(base);
  c.step(poke);
  CHECK(!c.levels()[0].swept);                   // a wick through is a test, not a sweep
  CHECK_EQ(c.levels()[0].tests, 1);
  c.step(close);
  CHECK(c.levels()[0].swept);
  CHECK_EQ(c.levels()[0].sweptIndex, 4);
  CHECK(std::fabs(c.levels()[0].depth - 0.3) < 1e-12);
  // reaching the zone from below without going through: a test (wick rule), once for the bar
  LiquidityLevelEngine t(pct(0.01, 1.0));        // zone 1 % thick above 100
  t.run(base);
  t.step(ohlc(98, 99.99, 97, 98, 6));            // under the price: not even in the zone (zone is above the price)
  CHECK_EQ(t.levels()[0].tests, 0);
  // sell-side mirror: a low at 96 (bar 2), zone below it; a bar to 95.9 sweeps it
  CHECK_EQ(int(t.active(LevelSide::SellSide).size()), 1);
  t.step(ohlc(97, 97.5, 95.9, 97, 7));
  CHECK(t.levels()[t.levels().size() - 1].swept || t.active(LevelSide::SellSide).empty());
  // a gap over a buy-side level (the whole bar above it) sweeps it too
  LiquidityLevelEngine g(pct(0.01));
  g.run(base);
  g.step(ohlc(103, 104, 102, 103.5, 8));
  CHECK(g.levels()[0].swept);
  CHECK(std::fabs(g.levels()[0].depth - 4) < 1e-12);
}

TEST("levels: a test is a bar entering the zone without sweeping (close rule), counted once per bar, and raises the score") {
  std::vector<Bar> base = {hl(98, 97, 1), hl(100, 98, 2), hl(98, 96, 3)};
  LiquidityLevelEngine c(pct(0.01, 0.5, SweepRule::Close));
  c.run(base);
  const double s0 = c.score(c.levels()[0]);
  c.step(ohlc(99, 100.2, 98, 99.5, 4));
  c.step(ohlc(99, 100.4, 98, 99.9, 5));
  CHECK_EQ(c.levels()[0].tests, 2);
  CHECK(c.score(c.levels()[0]) > s0 + 0.99);
  CHECK(!c.levels()[0].swept);
}

namespace {
std::vector<Bar> walk(int n, unsigned seed) {
  std::mt19937 rng(seed);
  std::normal_distribution<double> ret(0, 0.004);
  std::uniform_real_distribution<double> u(0, 1);
  std::vector<Bar> v;
  double p = 60000;
  for (int i = 0; i < n; ++i) {
    const double c = p * std::exp(ret(rng));
    v.push_back({int64_t(i) * 60, p, std::max(p, c) * (1 + 0.002 * u(rng)), std::min(p, c) * (1 - 0.002 * u(rng)), c, 1});
    p = c;
  }
  return v;
}
bool sameLevels(const LiquidityLevelEngine& a, const LiquidityLevelEngine& b) {
  if (a.levels().size() != b.levels().size()) return false;
  for (size_t i = 0; i < a.levels().size(); ++i) {
    const Level &x = a.levels()[i], &y = b.levels()[i];
    if (x.id != y.id || x.side != y.side || x.kind != y.kind || x.price != y.price || x.zoneLow != y.zoneLow || x.zoneHigh != y.zoneHigh ||
        x.originIndex != y.originIndex || x.originTime != y.originTime || x.members != y.members || x.tests != y.tests ||
        x.swept != y.swept || x.sweptIndex != y.sweptIndex || x.sweptTime != y.sweptTime || x.depth != y.depth)
      return false;
  }
  return a.active(LevelSide::BuySide) == b.active(LevelSide::BuySide) && a.active(LevelSide::SellSide) == b.active(LevelSide::SellSide);
}
}  // namespace

TEST("levels: bar by bar or all at once — the same levels; the engine's pivots are detectSwings' ones") {
  const auto bars = walk(3000, 5);
  for (SweepRule rule : {SweepRule::Wick, SweepRule::Close}) {
    EngineParams p;
    p.swing = window(5, 5);
    p.sweep = rule;
    LiquidityLevelEngine all(p), parts(p);
    all.run(bars);
    parts.run(std::span<const Bar>(bars.data(), 1234));
    for (size_t i = 1234; i < bars.size(); ++i) parts.step(bars[i]);
    CHECK(sameLevels(all, parts));
    CHECK(all.levels().size() > 50);
    int swept = 0, equal = 0;
    for (const Level& l : all.levels()) { swept += l.swept; equal += l.kind == LevelKind::Equal; }
    CHECK(swept > 0);
    CHECK(equal > 0);
    // sorted by price per side
    for (LevelSide s : {LevelSide::BuySide, LevelSide::SellSide}) {
      bool sorted = true;
      const auto& act = all.active(s);
      for (size_t k = 1; k < act.size(); ++k) sorted = sorted && all.levels()[act[k - 1]].price <= all.levels()[act[k]].price;
      CHECK(sorted);
    }
  }
  // every confirmed pivot reached the engine: members over all levels = confirmed pivots
  EngineParams p;
  p.swing = window(5, 5);
  LiquidityLevelEngine e(p);
  e.run(bars);
  int members = 0;
  for (const Level& l : e.levels()) members += l.members;
  CHECK_EQ(members, int(detectSwings(bars, p.swing).confirmed.size()));
}

TEST("levels: no allocation per bar once room is reserved") {
  const auto bars = walk(5000, 9);
  EngineParams p;
  p.swing = window(3, 3);
  LiquidityLevelEngine e(p);
  e.reserve(5000);
  e.run(std::span<const Bar>(bars.data(), 100));   // the pivot ring fills
  alloccount::Scope s;
  e.run(std::span<const Bar>(bars.data() + 100, bars.size() - 100));
  CHECK_EQ(int(s.count()), 0);
}
