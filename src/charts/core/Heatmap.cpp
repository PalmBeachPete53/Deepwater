#include "charts/core/Heatmap.h"
#include "charts/core/Ticks.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace charts::flow {

int64_t DepthSampler::autoRowTicks(int64_t midTicks) {
  const double want = std::max(1.0, double(midTicks) * 2e-5);
  // at most 1e9: DepthColumn::rowTicks is an int32
  for (int64_t mag = 1; mag <= 100000000; mag *= 10)
    for (int64_t s : {1, 2, 5})
      if (double(s * mag) >= want * 0.75) return s * mag;   // the first step not much smaller than wanted
  return 1000000000;
}

void DepthSampler::sample(const OrderBook& book, bool synced, int64_t timeMs, DepthColumn& out) {
  out.timeMs = timeMs;
  out.flags = 0;
  std::memset(out.qty, 0, sizeof(out.qty));
  const bool hasBid = book.has(OrderBook::Bid), hasAsk = book.has(OrderBook::Ask);
  out.bestBid = synced && hasBid ? book.best(OrderBook::Bid) : 0;
  out.bestAsk = synced && hasAsk ? book.best(OrderBook::Ask) : 0;
  if (!synced || (!hasBid && !hasAsk)) {
    out.flags |= DepthColumn::Missing;
    out.baseTicks = base_;
    out.rowTicks = int32_t(rowTicks_ ? rowTicks_ : 1);
    return;
  }
  const int64_t mid = hasBid && hasAsk ? book.best(OrderBook::Bid) + (book.best(OrderBook::Ask) - book.best(OrderBook::Bid)) / 2
                      : hasBid         ? book.best(OrderBook::Bid)
                                       : book.best(OrderBook::Ask);
  if (!rowTicks_) rowTicks_ = std::clamp<int64_t>(fixedRows_ > 0 ? fixedRows_ : autoRowTicks(mid), 1, 1000000000);
  const int64_t span = int64_t(DepthColumn::kRows) * rowTicks_;
  // the band moves only when the mid leaves its middle half; its base stays on the row grid
  if (!anchored_ || mid < base_ + span / 4 || mid >= base_ + 3 * span / 4) {
    base_ = groupFloor(mid, rowTicks_) - (DepthColumn::kRows / 2) * rowTicks_;
    if (anchored_) out.flags |= DepthColumn::Reanchored;
    anchored_ = true;
  }
  out.baseTicks = base_;
  out.rowTicks = int32_t(rowTicks_);
  const int64_t lo = base_, hi = base_ + span - 1, rt = rowTicks_;
  float* q = out.qty;
  auto add = [&](int64_t t, double v) { q[(t - lo) / rt] += float(v); };
  book.forEach(OrderBook::Bid, lo, hi, add);
  book.forEach(OrderBook::Ask, lo, hi, add);
}

}  // namespace charts::flow
