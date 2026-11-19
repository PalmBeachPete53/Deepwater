#include "charts/core/Footprint.h"
#include "charts/core/Ticks.h"
#include <algorithm>
#include <cmath>

namespace charts::flow {

FootprintBook::FootprintBook(BucketFn bucketOf) : bucketOf_(std::move(bucketOf)) {}

FootprintBook::BucketFn FootprintBook::fixedBuckets(int64_t barMs) {
  return [barMs](int64_t ms) {
    int64_t q = ms / barMs;
    if (ms % barMs != 0 && ms < 0) --q;
    return q * barMs;
  };
}

void FootprintBook::clear() {
  candles_.clear();
  coveredFrom_ = INT64_MIN / 2;
}

const FootprintCandle* FootprintBook::find(int64_t startMs) const {
  auto it = std::lower_bound(candles_.begin(), candles_.end(), startMs,
                             [](const FootprintCandle& c, int64_t s) { return c.startMs < s; });
  return it != candles_.end() && it->startMs == startMs ? &*it : nullptr;
}

FootprintCandle& FootprintBook::candleFor(int64_t startMs) {
  // live trades go to the last candle; history to earlier ones
  if (!candles_.empty() && candles_.back().startMs == startMs) return candles_.back();
  if (candles_.empty() || candles_.back().startMs < startMs) {
    candles_.emplace_back();
    candles_.back().startMs = startMs;
    return candles_.back();
  }
  auto it = std::lower_bound(candles_.begin(), candles_.end(), startMs,
                             [](const FootprintCandle& c, int64_t s) { return c.startMs < s; });
  if (it != candles_.end() && it->startMs == startMs) return *it;
  it = candles_.insert(it, FootprintCandle{});
  it->startMs = startMs;
  return *it;
}

void FootprintBook::add(const Trade& t) {
  FootprintCandle& c = candleFor(bucketOf_(t.timeMs));
  if (c.trades == 0) {
    c.open = c.high = c.low = c.close = t.ticks;
    c.openId = c.closeId = t.id;
  } else {
    if (t.id < c.openId) { c.openId = t.id; c.open = t.ticks; }
    if (t.id > c.closeId) { c.closeId = t.id; c.close = t.ticks; }
    c.high = std::max(c.high, t.ticks);
    c.low = std::min(c.low, t.ticks);
  }
  ++c.trades;
  if (t.from != SideFrom::Exact) ++c.approxTrades;
  if (t.side == Side::Unknown) {
    c.unknownVolume += t.qty;
    return;
  }
  // the cell of this price: most trades land near the last price, so a binary search over a short vector
  auto it = std::lower_bound(c.cells.begin(), c.cells.end(), t.ticks, [](const FootprintCell& x, int64_t v) { return x.ticks < v; });
  if (it == c.cells.end() || it->ticks != t.ticks) it = c.cells.insert(it, FootprintCell{t.ticks, 0, 0});
  if (t.side == Side::Buy) { it->ask += t.qty; c.askVolume += t.qty; }
  else { it->bid += t.qty; c.bidVolume += t.qty; }
}

void analyze(const FootprintCandle& c, const FootprintParams& p, FootprintView& out) {
  out.rows.clear();
  out.stacks.clear();
  out.poc = -1;
  out.maxRowVolume = out.maxAbsRowDelta = 0;
  out.volume = c.volume();
  out.delta = c.delta();
  out.open = c.open;
  out.high = c.high;
  out.low = c.low;
  out.close = c.close;
  if (c.trades == 0) return;
  const int64_t g = std::max<int64_t>(1, p.groupTicks);
  // rows from the candle's low to its high (the range comes from every trade, unknown sides included)
  const int64_t lowRow = groupFloor(c.low, g), highRow = groupFloor(c.high, g);
  const size_t n = size_t((highRow - lowRow) / g + 1);
  out.rows.resize(n);
  for (size_t i = 0; i < n; ++i) out.rows[i] = FootprintRow{lowRow + int64_t(i) * g, 0, 0, 0};
  for (const FootprintCell& cell : c.cells) {
    FootprintRow& r = out.rows[size_t((groupFloor(cell.ticks, g) - lowRow) / g)];
    r.bid += cell.bid;
    r.ask += cell.ask;
  }
  // POC: largest volume, ties → lowest row (the first one met going up)
  for (size_t i = 0; i < n; ++i) {
    const double v = out.rows[i].volume();
    if (v > 0 && (out.poc < 0 || v > out.rows[size_t(out.poc)].volume())) out.poc = int(i);
    out.maxRowVolume = std::max(out.maxRowVolume, v);
    out.maxAbsRowDelta = std::max(out.maxAbsRowDelta, std::fabs(out.rows[i].delta()));
  }
  if (out.poc >= 0) out.rows[size_t(out.poc)].flags |= Poc;
  // diagonal imbalances (edge rule in Footprint.h)
  const auto strong = [&](double v) { return v > 0 && v >= p.minVolume; };
  for (size_t i = 0; i < n; ++i) {
    FootprintRow& r = out.rows[i];
    if (i > 0 && strong(r.ask) && r.ask >= p.ratio * out.rows[i - 1].bid) r.flags |= BuyImbalance;
    if (i + 1 < n && strong(r.bid) && r.bid >= p.ratio * out.rows[i + 1].ask) r.flags |= SellImbalance;
  }
  // stacked zones: runs of the same flag
  const int need = std::max(1, p.stackMin);
  for (const auto& [flag, side] : {std::pair{BuyImbalance, Side::Buy}, std::pair{SellImbalance, Side::Sell}}) {
    size_t i = 0;
    while (i < n) {
      if (!(out.rows[i].flags & flag)) { ++i; continue; }
      size_t j = i;
      while (j + 1 < n && (out.rows[j + 1].flags & flag)) ++j;
      if (int(j - i + 1) >= need) out.stacks.push_back(FootprintStack{int(i), int(j), side});
      i = j + 1;
    }
  }
}

}  // namespace charts::flow
