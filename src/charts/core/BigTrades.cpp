#include "charts/core/BigTrades.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace charts::flow {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

// ------------------------------------------------------------------------------------------------ Aggregator

bool Aggregator::fits(const Trade& t) const {
  if (!open_ || t.side != cur_.side) return false;
  if (p_.windowMs <= 0 ? t.timeMs != cur_.startMs : t.timeMs - cur_.startMs > p_.windowMs) return false;
  if (!p_.sweep) return t.ticks == lastTicks_;
  // a sweep walks the book: buyers lift higher (or equal) asks, sellers hit lower (or equal) bids
  if (t.side == Side::Buy) return t.ticks >= lastTicks_;
  if (t.side == Side::Sell) return t.ticks <= lastTicks_;
  return t.ticks == lastTicks_;   // unknown side: same price only
}

std::optional<AggregatedTrade> Aggregator::add(const Trade& t) {
  if (fits(t)) {
    cur_.endMs = t.timeMs;
    cur_.volume += t.qty;
    sumPxVol_ += t.qty * double(t.ticks);
    cur_.vwapTicks = cur_.volume > 0 ? sumPxVol_ / cur_.volume : double(t.ticks);
    cur_.minTicks = std::min(cur_.minTicks, t.ticks);
    cur_.maxTicks = std::max(cur_.maxTicks, t.ticks);
    ++cur_.executions;
    cur_.lastId = t.id;
    cur_.approximate = cur_.approximate || t.from != SideFrom::Exact;
    lastTicks_ = t.ticks;
    return std::nullopt;
  }
  std::optional<AggregatedTrade> closed;
  if (open_) closed = cur_;
  cur_ = AggregatedTrade{};
  cur_.startMs = cur_.endMs = t.timeMs;
  cur_.side = t.side;
  cur_.volume = t.qty;
  cur_.vwapTicks = double(t.ticks);
  cur_.minTicks = cur_.maxTicks = t.ticks;
  cur_.executions = 1;
  cur_.firstId = cur_.lastId = t.id;
  cur_.approximate = t.from != SideFrom::Exact;
  sumPxVol_ = t.qty * double(t.ticks);
  lastTicks_ = t.ticks;
  open_ = true;
  return closed;
}

std::optional<AggregatedTrade> Aggregator::flush(int64_t nowMs) {
  if (!open_) return std::nullopt;
  // no later trade can join once the window from the first trade has passed (window 0: once time moved on)
  const bool done = p_.windowMs <= 0 ? nowMs > cur_.startMs : nowMs - cur_.startMs > p_.windowMs;
  if (!done) return std::nullopt;
  open_ = false;
  return cur_;
}

// ------------------------------------------------------------------------------------------------ RollingPercentile

void RollingPercentile::clear() {
  blocks_.clear();
  fifo_.clear();
  n_ = 0;
}

void RollingPercentile::insertSorted(double v) {
  if (blocks_.empty()) {
    blocks_.emplace_back();
    blocks_.back().reserve(kBlock);
    blocks_.back().push_back(v);
    return;
  }
  // first block whose last value is ≥ v (or the last block)
  auto it = std::lower_bound(blocks_.begin(), blocks_.end(), v, [](const std::vector<double>& b, double x) { return b.back() < x; });
  if (it == blocks_.end()) --it;
  it->insert(std::upper_bound(it->begin(), it->end(), v), v);
  if (it->size() > 2 * kBlock) {   // split a block that grew too big
    std::vector<double> tail(it->begin() + ptrdiff_t(kBlock), it->end());
    it->resize(kBlock);
    blocks_.insert(it + 1, std::move(tail));
  }
}

void RollingPercentile::eraseSorted(double v) {
  auto it = std::lower_bound(blocks_.begin(), blocks_.end(), v, [](const std::vector<double>& b, double x) { return b.back() < x; });
  for (; it != blocks_.end(); ++it) {
    auto pos = std::lower_bound(it->begin(), it->end(), v);
    if (pos != it->end() && *pos == v) {
      it->erase(pos);
      if (it->empty()) blocks_.erase(it);
      return;
    }
  }
}

void RollingPercentile::add(int64_t ms, double v) {
  fifo_.emplace_back(ms, v);
  insertSorted(v);
  ++n_;
}

void RollingPercentile::expireBefore(int64_t ms) {
  while (!fifo_.empty() && fifo_.front().first < ms) {
    eraseSorted(fifo_.front().second);
    fifo_.pop_front();
    --n_;
  }
}

double RollingPercentile::quantile(double pct) const {
  if (n_ == 0) return kNaN;
  const double rank = std::clamp(pct, 0.0, 100.0) / 100.0 * double(n_ - 1);
  const size_t lo = size_t(std::floor(rank));
  const double frac = rank - double(lo);
  auto at = [&](size_t k) {
    for (const auto& b : blocks_) {
      if (k < b.size()) return b[k];
      k -= b.size();
    }
    return blocks_.back().back();
  };
  const double a = at(lo);
  return frac > 0 && lo + 1 < n_ ? a + (at(lo + 1) - a) * frac : a;
}

// ------------------------------------------------------------------------------------------------ MinuteAtr

void MinuteAtr::close() {
  if (!have_) return;
  double tr = double(hi_ - lo_);
  if (havePrev_) tr = std::max({tr, std::fabs(double(hi_ - prevClose_)), std::fabs(double(lo_ - prevClose_))});
  trs_.push_back(tr);
  if (trs_.size() > 120) trs_.pop_front();
  prevClose_ = last_;
  havePrev_ = true;
  have_ = false;
}

void MinuteAtr::add(int64_t ms, int64_t lowTicks, int64_t highTicks, int64_t lastTicks) {
  const int64_t m = ms >= 0 ? ms / 60000 : -((-ms + 59999) / 60000);
  if (have_ && m != minute_) close();
  if (!have_) {
    have_ = true;
    minute_ = m;
    lo_ = lowTicks;
    hi_ = highTicks;
  } else {
    lo_ = std::min(lo_, lowTicks);
    hi_ = std::max(hi_, highTicks);
  }
  last_ = lastTicks;
}

double MinuteAtr::shortAtr() const {
  if (trs_.size() < 14) return kNaN;
  double s = 0;
  for (size_t i = trs_.size() - 14; i < trs_.size(); ++i) s += trs_[i];
  return s / 14;
}

double MinuteAtr::longAtr() const {
  if (trs_.size() < 120) return kNaN;
  double s = 0;
  for (double v : trs_) s += v;
  return s / 120;
}

double MinuteAtr::factor() const {
  const double l = longAtr(), s = shortAtr();
  if (!std::isfinite(l) || !std::isfinite(s) || l <= 0) return 1;
  return std::clamp(s / l, 0.5, 2.0);
}

// ------------------------------------------------------------------------------------------------ Detector

void Detector::reset() {
  window_.clear();
  atr_.clear();
}

Detector::Verdict Detector::threshold(int64_t nowMs) {
  Verdict v;
  if (p_.mode == DetectorParams::Mode::Manual) {
    v.threshold = p_.minVolume > 0 ? p_.minVolume : kNaN;   // no minimum set: nothing (every order would be "big")
    v.warm = true;
    return v;
  }
  window_.expireBefore(nowMs - p_.windowMs);
  v.factor = p_.volatility ? atr_.factor() : 1;
  if (window_.size() >= std::max<size_t>(1, p_.minSamples)) {
    v.warm = true;
    v.percentileValue = window_.quantile(p_.percentile);
    v.threshold = v.percentileValue * v.factor;
  } else {
    v.threshold = p_.minVolume > 0 ? p_.minVolume : kNaN;   // warm-up: the fallback, or nothing
  }
  return v;
}

Detector::Verdict Detector::judge(const AggregatedTrade& a) {
  Verdict v = threshold(a.endMs);
  const bool underMax = p_.maxVolume <= 0 || a.volume <= p_.maxVolume;
  // auto: strictly above the percentile (sizes cluster on round lots: with ≥, a run of 0.5 BTC orders at a p99 of
  // exactly 0.5 would all be "big"); manual and the warm-up fallback: at least the minimum
  const bool above = v.warm && p_.mode == DetectorParams::Mode::Auto ? a.volume > v.threshold : a.volume >= v.threshold;
  v.big = std::isfinite(v.threshold) && a.volume > 0 && above && underMax;
  if (p_.mode == DetectorParams::Mode::Auto) {
    window_.add(a.endMs, a.volume);
    atr_.add(a.endMs, a.minTicks, a.maxTicks, a.side == Side::Buy ? a.maxTicks : a.minTicks);
  }
  return v;
}

Detector::Verdict Detector::current(int64_t nowMs) { return threshold(nowMs); }

// ------------------------------------------------------------------------------------------------ bubbles

bool bigTradeAlert(double volume, double threshold, double factor) {
  if (!std::isfinite(threshold) || threshold <= 0 || !std::isfinite(factor)) return false;
  return volume >= std::max(1.0, factor) * threshold;
}

double bubbleDiameter(double volume, double reference, double minD, double maxD) {
  if (!(volume > 0) || !(reference > 0)) return minD;
  return std::clamp(minD * std::sqrt(volume / reference), minD, std::max(minD, maxD));
}

double bubbleOpacity(double diameter, double minD, double maxD, double minOpacity) {
  if (!(maxD > minD)) return 1;
  const double t = std::clamp((diameter - minD) / (maxD - minD), 0.0, 1.0);
  return std::clamp(minOpacity + (1 - minOpacity) * t, 0.0, 1.0);
}

}  // namespace charts::flow
