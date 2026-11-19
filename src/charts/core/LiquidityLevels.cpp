#include "charts/core/LiquidityLevels.h"
#include "charts/core/SessionLevels.h"
#include <algorithm>
#include <cmath>

namespace charts::smc {

double Atr::push(const Bar& b) {
  const double tr = n_ == 0 ? b.high - b.low : std::max({b.high - b.low, std::fabs(b.high - prevClose_), std::fabs(b.low - prevClose_)});
  prevClose_ = b.close;
  ++n_;
  if (n_ < len_) sum_ += tr;
  else if (n_ == len_) atr_ = (sum_ + tr) / len_;          // the first: a plain mean
  else atr_ = (atr_ * (len_ - 1) + tr) / len_;            // then Wilder's smoothing
  return value();
}

SwingTracker::SwingTracker(const SwingParams& p) : p_(p) {
  p_.left = std::max(1, p_.left);
  p_.right = std::max(1, p_.right);
  if (p_.method == PivotMethod::Window) ring_.reserve(size_t(p_.left + p_.right + 1));
}

SwingTracker::Confirmed SwingTracker::push(const Bar& b, double atr) {
  Confirmed out;
  const int i = n_++;
  if (p_.method == PivotMethod::Window) {
    // the ring holds bars i - L - R … i (oldest first once full); the candidate is the bar R before this one
    const size_t cap = size_t(p_.left + p_.right + 1);
    if (ring_.size() < cap) ring_.push_back(b);
    else {
      std::rotate(ring_.begin(), ring_.begin() + 1, ring_.end());
      ring_.back() = b;
    }
    if (ring_.size() < cap) return out;
    const size_t c = size_t(p_.left);
    const Bar& cand = ring_[c];
    bool isHigh = true, isLow = true;
    for (size_t k = 0; k < c; ++k) {
      isHigh = isHigh && cand.high > ring_[k].high;   // strictly above the left side
      isLow = isLow && cand.low < ring_[k].low;
    }
    for (size_t k = c + 1; k < cap; ++k) {
      isHigh = isHigh && cand.high >= ring_[k].high;  // at least as high as the right side (leftmost tie wins)
      isLow = isLow && cand.low <= ring_[k].low;
    }
    const int idx = i - p_.right;
    if (isHigh) out.high = Pivot{idx, cand.time, cand.high, true, i};
    if (isLow) out.low = Pivot{idx, cand.time, cand.low, false, i};
    return out;
  }
  // zigzag
  auto threshold = [&](double ref) {
    return p_.method == PivotMethod::Percent ? ref * p_.percent / 100 : (std::isfinite(atr) ? p_.atrMult * atr : INFINITY);
  };
  if (i == 0 || dir_ == 0) {
    if (i == 0 || b.high > hi_) { hi_ = b.high; hiIdx_ = i; hiTime_ = b.time; }
    if (i == 0 || b.low < lo_) { lo_ = b.low; loIdx_ = i; loTime_ = b.time; }
    if (i == 0) return out;
    // the first move large enough decides: a fall from the high confirms it, a rise from the low confirms that
    if (hi_ - b.low >= threshold(hi_) && hiIdx_ < i) {
      out.high = Pivot{hiIdx_, hiTime_, hi_, true, i};
      dir_ = -1;
      lo_ = b.low; loIdx_ = i; loTime_ = b.time;
    } else if (b.high - lo_ >= threshold(lo_) && loIdx_ < i) {
      out.low = Pivot{loIdx_, loTime_, lo_, false, i};
      dir_ = +1;
      hi_ = b.high; hiIdx_ = i; hiTime_ = b.time;
    }
    return out;
  }
  if (dir_ > 0) {   // tracking a high
    if (b.high > hi_) { hi_ = b.high; hiIdx_ = i; hiTime_ = b.time; }
    else if (hi_ - b.low >= threshold(hi_)) {
      out.high = Pivot{hiIdx_, hiTime_, hi_, true, i};
      dir_ = -1;
      lo_ = b.low; loIdx_ = i; loTime_ = b.time;
    }
  } else {          // tracking a low
    if (b.low < lo_) { lo_ = b.low; loIdx_ = i; loTime_ = b.time; }
    else if (b.high - lo_ >= threshold(lo_)) {
      out.low = Pivot{loIdx_, loTime_, lo_, false, i};
      dir_ = +1;
      hi_ = b.high; hiIdx_ = i; hiTime_ = b.time;
    }
  }
  return out;
}

std::vector<Pivot> SwingTracker::provisional() const {
  std::vector<Pivot> out;
  if (p_.method != PivotMethod::Window) {
    if (dir_ >= 0 && hiIdx_ >= 0 && n_ > 0) out.push_back({hiIdx_, hiTime_, hi_, true, -1});
    if (dir_ <= 0 && loIdx_ >= 0 && n_ > 0) out.push_back({loIdx_, loTime_, lo_, false, -1});
    return out;
  }
  // the last `right` bars: pivots so far (a full left window, and at least as extreme as the bars after them)
  const int n = int(ring_.size());
  for (int k = std::max(p_.left, n - p_.right); k < n; ++k) {
    const Bar& cand = ring_[size_t(k)];
    bool isHigh = true, isLow = true;
    for (int j = k - p_.left; j < k; ++j) {
      isHigh = isHigh && cand.high > ring_[size_t(j)].high;
      isLow = isLow && cand.low < ring_[size_t(j)].low;
    }
    for (int j = k + 1; j < n; ++j) {
      isHigh = isHigh && cand.high >= ring_[size_t(j)].high;
      isLow = isLow && cand.low <= ring_[size_t(j)].low;
    }
    const int idx = n_ - n + k;
    if (isHigh) out.push_back({idx, cand.time, cand.high, true, -1});
    if (isLow) out.push_back({idx, cand.time, cand.low, false, -1});
  }
  return out;
}

SwingResult detectSwings(std::span<const Bar> bars, const SwingParams& p) {
  SwingResult r;
  SwingTracker t(p);
  Atr atr(p.atrLength);
  for (const Bar& b : bars) {
    const double a = atr.push(b);
    const auto c = t.push(b, a);
    if (c.high) r.confirmed.push_back(*c.high);
    if (c.low) r.confirmed.push_back(*c.low);
  }
  std::stable_sort(r.confirmed.begin(), r.confirmed.end(), [](const Pivot& a, const Pivot& b) { return a.index < b.index; });
  r.provisional = t.provisional();
  return r;
}

// ------------------------------------------------------------------------------------------------ engine

LiquidityLevelEngine::LiquidityLevelEngine(const EngineParams& p) : p_(p), atr_(p.swing.atrLength), tracker_(p.swing) {
  if (p_.periodLevels) {
    periods_ = std::make_unique<SessionLevelTracker>(p_.periods);
    finished_.reserve(8);
  }
}
LiquidityLevelEngine::~LiquidityLevelEngine() = default;
LiquidityLevelEngine::LiquidityLevelEngine(LiquidityLevelEngine&&) noexcept = default;

void LiquidityLevelEngine::addPeriod(const PeriodLevel& pl) {
  for (bool buy : {true, false}) {
    const double price = buy ? pl.high : pl.low;
    const double thick = sizeOf(p_.thicknessUnit, p_.thickness, price);
    maxZone_[buy ? 0 : 1] = std::max(maxZone_[buy ? 0 : 1], thick);
    Level l;
    l.id = uint32_t(levels_.size());
    l.side = buy ? LevelSide::BuySide : LevelSide::SellSide;
    l.kind = pl.kind;
    l.price = price;
    l.zoneLow = buy ? price : price - thick;
    l.zoneHigh = buy ? price + thick : price;
    l.originIndex = l.lastMemberIndex = buy ? pl.highIndex : pl.lowIndex;
    l.originTime = buy ? pl.highTime : pl.lowTime;
    l.tfSeconds = p_.tfSeconds;
    levels_.push_back(l);
    std::vector<uint32_t>& act = buy ? buy_ : sell_;
    act.insert(std::lower_bound(act.begin(), act.end(), price, [&](uint32_t id, double x) { return levels_[id].price < x; }), l.id);
  }
}

void LiquidityLevelEngine::reserve(size_t levels) {
  levels_.reserve(levels);
  buy_.reserve(levels);
  sell_.reserve(levels);
}

double LiquidityLevelEngine::sizeOf(SizeUnit u, double v, double price) const {
  switch (u) {
    case SizeUnit::Ticks: return v * p_.tick;
    case SizeUnit::Percent: return price * v / 100;
    default: {
      const double a = atr_.value();
      return std::isfinite(a) ? v * a : price * 0.001 * v;   // before the ATR exists: 0.1 % of price per ATR asked
    }
  }
}

void LiquidityLevelEngine::sweepSide(std::vector<uint32_t>& act, const Bar& b, bool buySide) {
  // buy-side levels (above highs, ascending): the bar touches those with zoneLow ≤ high, i.e. price ≤ high + zone;
  // sell-side mirrored with low. Binary search on price, then the zone check per level.
  if (act.empty()) return;
  auto priceOf = [&](uint32_t id) { return levels_[id].price; };
  // buy-side (ascending): every level up to high + the widest zone (one below the low was gapped over: swept too);
  // sell-side: every level from low − the widest zone up
  size_t lo = 0, hi = act.size();
  if (buySide)
    hi = size_t(std::upper_bound(act.begin(), act.end(), b.high + maxZone_[0], [&](double x, uint32_t id) { return x < priceOf(id); }) - act.begin());
  else
    lo = size_t(std::lower_bound(act.begin(), act.end(), b.low - maxZone_[1], [&](uint32_t id, double x) { return priceOf(id) < x; }) - act.begin());
  size_t w = lo;   // compaction of the kept ones within [lo, hi)
  for (size_t k = lo; k < hi; ++k) {
    Level& l = levels_[act[k]];
    bool swept, touched;
    if (buySide) {
      swept = p_.sweep == SweepRule::Wick ? b.high > l.price : b.close > l.price;
      touched = b.high >= l.zoneLow;
    } else {
      swept = p_.sweep == SweepRule::Wick ? b.low < l.price : b.close < l.price;
      touched = b.low <= l.zoneHigh;
    }
    if (swept) {
      l.swept = true;
      l.sweptIndex = n_;
      l.sweptTime = b.time;
      l.depth = buySide ? b.high - l.price : l.price - b.low;
      continue;
    }
    if (touched && l.lastTestIndex != n_) {
      ++l.tests;
      l.lastTestIndex = n_;
    }
    act[w++] = act[k];
  }
  if (w != hi) act.erase(act.begin() + std::ptrdiff_t(w), act.begin() + std::ptrdiff_t(hi));
}

void LiquidityLevelEngine::addPivot(const Pivot& pv) {
  const bool buy = pv.high;
  std::vector<uint32_t>& act = buy ? buy_ : sell_;
  const double tol = sizeOf(p_.toleranceUnit, p_.tolerance, pv.price);
  // a level the pivot's own bar went through by less than the tolerance: that was an equal high (low) forming, not a
  // sweep — the level comes back and the pivot joins it
  for (Level& l : levels_)
    if (l.swept && l.sweptIndex == pv.index && (l.side == LevelSide::BuySide) == buy && std::fabs(l.price - pv.price) <= tol) {
      l.swept = false;
      l.sweptIndex = -1;
      l.sweptTime = 0;
      l.depth = 0;
      act.insert(std::lower_bound(act.begin(), act.end(), l.price, [&](uint32_t id, double x) { return levels_[id].price < x; }), l.id);
    }
  // the nearest active level of this side within the tolerance
  auto it = std::lower_bound(act.begin(), act.end(), pv.price, [&](uint32_t id, double x) { return levels_[id].price < x; });
  int best = -1;
  double bestD = tol;
  for (auto j : {it, it == act.begin() ? act.end() : it - 1}) {
    if (j == act.end()) continue;
    const double d = std::fabs(levels_[*j].price - pv.price);
    if (d <= bestD) { bestD = d; best = int(*j); }
  }
  const double thick = sizeOf(p_.thicknessUnit, p_.thickness, pv.price);
  maxZone_[buy ? 0 : 1] = std::max(maxZone_[buy ? 0 : 1], thick);
  if (best >= 0) {
    Level& l = levels_[size_t(best)];
    ++l.members;
    l.kind = LevelKind::Equal;
    l.lastMemberIndex = pv.index;
    const double np = buy ? std::max(l.price, pv.price) : std::min(l.price, pv.price);
    if (np != l.price) {
      l.price = np;
      // keep the side sorted after the move
      act.erase(std::find(act.begin(), act.end(), uint32_t(best)));
      act.insert(std::lower_bound(act.begin(), act.end(), np, [&](uint32_t id, double x) { return levels_[id].price < x; }), uint32_t(best));
    }
    if (buy) { l.zoneLow = l.price; l.zoneHigh = l.price + thick; }
    else { l.zoneHigh = l.price; l.zoneLow = l.price - thick; }
    return;
  }
  Level l;
  l.id = uint32_t(levels_.size());
  l.side = buy ? LevelSide::BuySide : LevelSide::SellSide;
  l.price = pv.price;
  l.zoneLow = buy ? pv.price : pv.price - thick;
  l.zoneHigh = buy ? pv.price + thick : pv.price;
  l.originIndex = l.lastMemberIndex = pv.index;
  l.originTime = pv.time;
  l.tfSeconds = p_.tfSeconds;
  levels_.push_back(l);
  act.insert(std::lower_bound(act.begin(), act.end(), pv.price, [&](uint32_t id, double x) { return levels_[id].price < x; }), l.id);
}

void LiquidityLevelEngine::step(const Bar& b) {
  const double a = atr_.push(b);
  // 0. periods that ended before this bar: their highs and lows are levels from now on
  if (periods_) {
    periods_->step(b, n_, finished_);
    for (const PeriodLevel& pl : finished_) addPeriod(pl);
  }
  // 1. what this bar sweeps or tests (levels from earlier pivots only)
  sweepSide(buy_, b, true);
  sweepSide(sell_, b, false);
  // 2. pivots this bar confirms
  const auto c = tracker_.push(b, a);
  if (c.high) addPivot(*c.high);
  if (c.low) addPivot(*c.low);
  ++n_;
}

double LiquidityLevelEngine::score(const Level& l) const {
  const int end = l.swept ? l.sweptIndex : n_;
  const double age = std::min(1.0, double(std::max(0, end - l.originIndex)) / 200.0) * 0.5;
  const double tf = std::log2(std::max(1.0, l.tfSeconds / 60.0)) * 0.25;
  return l.members + 0.5 * l.tests + age + tf;
}

}  // namespace charts::smc
