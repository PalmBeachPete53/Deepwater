#pragma once
// Order flow: the live order book reduced to Binance's bookDepth bands (user's request 2026-11-20: the live book kept
// in memory for live trading, never on disk). The cumulative depth (base asset) from the mid price out to ±0.2 %,
// ±1 … ±5 %: bids below, asks above, in the order of technicaldata::perp::bandPercents() (src/technicaldata/
// PerpArchive.h), so a model trained on the archived bands reads the live ones the same way.
//
// What the local book knows: Binance's REST snapshot holds 1000 levels on the perpetual, 5000 on spot; past the
// deepest of them a level is only known once the stream updates it. cover = how far from the mid the snapshot
// reached on each side, in %: bands beyond it may be short of what Binance's own bookDepth counts.
// Pure, header-only: tests/charts/core/bookbands_tests.cpp.
#include "charts/core/OrderBook.h"
#include "charts/core/Ticks.h"
#include <array>
#include <cmath>
#include <limits>

namespace charts::flow {

struct BookBands {
  static constexpr int kBands = 12;
  static constexpr std::array<double, 6> kPercents{0.2, 1, 2, 3, 4, 5};   // each side, inward to outward
  int64_t timeMs = 0;                 // exchange clock
  double bestBid = 0, bestAsk = 0, mid = 0;
  std::array<float, kBands> depth{};  // −5 −4 −3 −2 −1 −0.2 (bids) +0.2 +1 +2 +3 +4 +5 (asks); NaN: no book
  float coverBid = 0, coverAsk = 0;   // % from the mid the snapshot reached (0 = unknown)
  bool ok() const { return mid > 0; }
};

inline void bookBands(const OrderBook& b, const TickScale& sc, int64_t coverBidTicks, int64_t coverAskTicks, int64_t timeMs, BookBands& o) {
  o = BookBands{};
  o.timeMs = timeMs;
  o.depth.fill(std::numeric_limits<float>::quiet_NaN());
  if (!sc.valid() || !b.has(OrderBook::Bid) || !b.has(OrderBook::Ask)) return;
  o.bestBid = sc.priceOf(b.best(OrderBook::Bid));
  o.bestAsk = sc.priceOf(b.best(OrderBook::Ask));
  o.mid = (o.bestBid + o.bestAsk) / 2;
  if (!(o.mid > 0)) return;
  const double outer = BookBands::kPercents.back();
  std::array<double, 6> bid{}, ask{};
  b.forEach(OrderBook::Bid, sc.ticksOf(o.mid * (1 - outer / 100)) - 1, b.best(OrderBook::Bid), [&](int64_t t, double q) {
    const double pct = (o.mid - sc.priceOf(t)) / o.mid * 100;
    for (size_t k = 0; k < 6; ++k) if (pct <= BookBands::kPercents[k] + 1e-12) bid[k] += q;
  });
  b.forEach(OrderBook::Ask, b.best(OrderBook::Ask), sc.ticksOf(o.mid * (1 + outer / 100)) + 1, [&](int64_t t, double q) {
    const double pct = (sc.priceOf(t) - o.mid) / o.mid * 100;
    for (size_t k = 0; k < 6; ++k) if (pct <= BookBands::kPercents[k] + 1e-12) ask[k] += q;
  });
  for (size_t k = 0; k < 6; ++k) {
    o.depth[5 - k] = float(bid[k]);   // −0.2 at index 5 … −5 at index 0
    o.depth[6 + k] = float(ask[k]);   // +0.2 at index 6 … +5 at index 11
  }
  if (coverBidTicks > 0) o.coverBid = float(std::max(0.0, (o.mid - sc.priceOf(coverBidTicks)) / o.mid * 100));
  if (coverAskTicks > 0) o.coverAsk = float(std::max(0.0, (sc.priceOf(coverAskTicks) - o.mid) / o.mid * 100));
}

}  // namespace charts::flow
