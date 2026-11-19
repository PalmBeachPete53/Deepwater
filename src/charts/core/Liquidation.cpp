#include "charts/core/Liquidation.h"
#include <algorithm>
#include <cmath>

namespace charts::flow {

const LeverageBracket* bracketFor(std::span<const LeverageBracket> brackets, double notional) {
  if (brackets.empty()) return nullptr;
  for (const LeverageBracket& b : brackets)
    if (notional <= b.notionalCap) return &b;
  return &brackets.back();
}

double liquidationPrice(double entry, double leverage, bool isLong, double mmr, double cum, double notional) {
  const double perUnit = notional > 0 ? cum * entry / notional : 0;   // cum / Q, with Q = notional / entry
  return isLong ? (entry * (1 - 1 / leverage) - perUnit) / (1 - mmr) : (entry * (1 + 1 / leverage) + perUnit) / (1 + mmr);
}

double liquidationPriceInverse(double entry, double leverage, bool isLong, double mmr) {
  return isLong ? entry * (1 + mmr) / (1 + 1 / leverage) : entry * (1 - mmr) / (1 - 1 / leverage);
}

int LiqGrid::rowOf(double price) const {
  if (!(rowHeight > 0) || !std::isfinite(price)) return -1;
  const double r = std::floor((price - priceLow) / rowHeight);
  return r >= 0 && r < rows ? int(r) : -1;
}

LiquidationModel::LiquidationModel(const LiqGrid& grid, const LiqParams& params)
    : grid_(grid), params_(params), longs_(size_t(std::max(grid.rows, 0)), 0.0), shorts_(size_t(std::max(grid.rows, 0)), 0.0) {
  // the liquidation price is linear in the entry for a fixed notional: entry × factor, once per leverage
  const LeverageBracket* br = bracketFor(params_.brackets, params_.positionNotional);
  const double mmr = br ? br->mmr : 0, cum = br ? br->cum : 0;
  double total = 0;
  for (const auto& l : params_.leverages) total += l.weight > 0 ? l.weight : 0;
  for (const auto& l : params_.leverages) {
    if (!(l.weight > 0) || !(l.leverage >= 1)) continue;
    const double lev = br ? std::min(l.leverage, double(br->maxLeverage)) : l.leverage;   // the bracket's maximum caps it
    if (params_.inverse)
      levels_.push_back({liquidationPriceInverse(1, lev, true, mmr), liquidationPriceInverse(1, lev, false, mmr), l.weight / total});
    else
      levels_.push_back({liquidationPrice(1, lev, true, mmr, cum, params_.positionNotional),
                         liquidationPrice(1, lev, false, mmr, cum, params_.positionNotional), l.weight / total});
  }
  if (params_.decayHalfLifeBars > 0) decay_ = std::exp2(-1 / params_.decayHalfLifeBars);
}

void LiquidationModel::reserve(size_t bars) {
  longCols_.reserve(bars * size_t(grid_.rows));
  shortCols_.reserve(bars * size_t(grid_.rows));
  liqLong_.reserve(bars);
  liqShort_.reserve(bars);
}

void LiquidationModel::deposit(std::vector<double>& side, double price, double qty) {
  const int r = grid_.rowOf(price);
  if (r < 0) outOfGrid_ += qty;
  else side[size_t(r)] += qty;
}

// The bar's range reaches the levels: longs at or above its low, shorts at or below its high. The row holding the low
// (high) loses the part of its height the bar covered.
void LiquidationModel::consume(double low, double high, double& liqLong, double& liqShort) {
  const double h = grid_.rowHeight, k = std::clamp(params_.consume, 0.0, 1.0);
  const int R = grid_.rows;
  // a zero row height gave rl = 0/0 = NaN, which passed the range tests and became the row index (INT_MIN on x86-64)
  if (!(h > 0) || R <= 0) return;
  if (std::isfinite(low)) {
    const double rl = (low - grid_.priceLow) / h;   // low in rows
    const int first = rl < 0 ? 0 : rl >= R ? R : int(std::floor(rl));
    for (int r = first; r < R; ++r) {
      const double covered = std::clamp(double(r + 1) - rl, 0.0, 1.0);   // part of the row at or above the low
      const double take = longs_[size_t(r)] * covered * k;
      longs_[size_t(r)] -= take;
      liqLong += take;
    }
  }
  if (std::isfinite(high)) {
    const double rh = (high - grid_.priceLow) / h;
    const int last = rh < 0 ? -1 : rh >= R ? R - 1 : int(std::floor(rh));
    for (int r = 0; r <= last; ++r) {
      const double covered = std::clamp(rh - double(r), 0.0, 1.0);   // part of the row at or below the high
      const double take = shorts_[size_t(r)] * covered * k;
      shorts_[size_t(r)] -= take;
      liqShort += take;
    }
  }
}

void LiquidationModel::step(const LiqBar& b) {
  // 1. levels fade (positions closed for reasons the model doesn't see)
  if (decay_ != 1)
    for (int r = 0; r < grid_.rows; ++r) {
      longs_[size_t(r)] *= decay_;
      shorts_[size_t(r)] *= decay_;
    }
  // 2. the bar's range liquidates what it reaches (positions from earlier bars only)
  double liqL = 0, liqS = 0;
  consume(b.low, b.high, liqL, liqS);
  // 3. open interest: new positions, or closed ones
  if (std::isfinite(b.oi)) {
    if (haveOi_) {
      const double d = b.oi - prevOi_;
      if (d < 0 && prevOi_ > 0) {
        const double keep = std::max(0.0, b.oi / prevOi_);
        for (int r = 0; r < grid_.rows; ++r) {
          longs_[size_t(r)] *= keep;
          shorts_[size_t(r)] *= keep;
        }
      } else if (d > 0) {
        const double opened = std::isfinite(b.volume) ? std::min(d, std::max(0.0, b.volume)) : d;
        double L = std::isfinite(b.ratio) && b.ratio > 0 ? b.ratio / (1 + b.ratio) : 0.5;
        if (params_.candleBias != 0 && b.high > b.low) L += params_.candleBias * (b.close - b.open) / (b.high - b.low);
        L = std::clamp(L, 0.05, 0.95);
        const double entry = (b.high + b.low + b.close) / 3;
        const double longQty = opened * 2 * L, shortQty = opened * 2 * (1 - L);
        for (const Level& lv : levels_) {
          deposit(longs_, entry * lv.longFactor, longQty * lv.weight);
          deposit(shorts_, entry * lv.shortFactor, shortQty * lv.weight);
        }
      }
    }
    prevOi_ = b.oi;
    haveOi_ = true;
  }
  // 4. the bar's column
  if (keepColumns_)
    for (int r = 0; r < grid_.rows; ++r) {
      longCols_.push_back(float(longs_[size_t(r)]));
      shortCols_.push_back(float(shorts_[size_t(r)]));
    }
  liqLong_.push_back(liqL);
  liqShort_.push_back(liqS);
  ++bars_;
}

void LiquidationModel::run(const LiqSeries& s, size_t from, size_t to) {
  to = std::min(to, s.size());
  for (size_t i = from; i < to; ++i) step(s.at(i));
}

}  // namespace charts::flow
