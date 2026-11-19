#pragma once
// Order flow, chantier E: where leveraged positions would be liquidated (step E1 — the model, pure).
//
// Nobody publishes where positions sit, so this is an ESTIMATE built from public series, the way the known liquidation
// heatmaps do it. Every assumption is written where it is used; the ones that matter most:
//   1. New positions: when the open interest rises over a bar, that many contracts were opened — one long and one short
//      each (open interest counts each contract once). The rise is capped by the bar's volume (a contract is opened by a
//      trade). When it falls, positions were closed: every resting level shrinks by the same fraction (we can't know
//      which ones closed).
//   2. Who: the long / short accounts ratio (Binance's) tilts the split: longs get 2·L of the new contracts and shorts
//      2·(1 − L), with L = ratio / (1 + ratio) — 50/50 when unknown. Optionally the candle tilts it further (a green bar
//      opened more longs): L + bias · (close − open) / (high − low), kept within 5 %…95 %.
//   3. At what price: the bar's typical price (high + low + close) / 3, a stand-in for its VWAP.
//   4. With what leverage: a fixed spread of leverages with weights (5×, 10×, 25×, 50×, 100× by default), each capped by
//      the margin bracket's maximum.
//   5. Where it dies: Binance's isolated-margin liquidation price for one position of a typical size (the bracket of
//      that notional gives the maintenance margin rate and amount):
//        long  LP = (E · (1 − 1/lev) − cum/Q) / (1 − mmr)        short  LP = (E · (1 + 1/lev) + cum/Q) / (1 + mmr)
//      with Q = notional / E. Cross margin, fees and funding are ignored.
//   6. Afterwards: a level stays until the price reaches it — a bar whose low is at or under a long level (high at or
//      over a short level) liquidates it; a price row only partly inside the bar loses that part. Positions opened in a
//      bar are not liquidated by that same bar (we don't know the order of prices inside it). Optionally levels fade
//      (half-life in bars: positions closed for other reasons).
//
// The grid is fixed (price rows of equal height); levels outside it are counted, not kept. One step per bar; running
// all the bars at once is the same loop, so incremental and complete results are identical bit for bit. The state is in
// double, the per-bar columns in float (rows × bars, one contiguous column per bar).
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace charts::flow {

// One of Binance's maintenance margin brackets (USD-M): positions of notional ≤ cap use this rate and amount.
struct LeverageBracket {
  double notionalCap = 0;   // upper bound of the bracket (quote currency)
  int maxLeverage = 125;
  double mmr = 0;           // maintenance margin rate
  double cum = 0;           // maintenance amount (quote currency)
};
// The bracket of a notional (brackets sorted by cap; the last one beyond the last cap; null when empty).
const LeverageBracket* bracketFor(std::span<const LeverageBracket> brackets, double notional);
// Isolated liquidation price of one position (entry price, leverage, maintenance rate and amount, notional).
double liquidationPrice(double entry, double leverage, bool isLong, double mmr, double cum, double notional);
// The same for an inverse contract (COIN-M: margin and profit in the coin). From the margin equation — margin + profit =
// maintenance, all in coin, maintenance on the value at the liquidation price:
//   long  LP = E · (1 + mmr) / (1 + 1/lev)        short  LP = E · (1 − mmr) / (1 − 1/lev)   (maintenance amount ignored)
double liquidationPriceInverse(double entry, double leverage, bool isLong, double mmr);

struct LiqParams {
  struct Lev { double leverage, weight; };
  std::vector<Lev> leverages = {{5, 0.30}, {10, 0.30}, {25, 0.20}, {50, 0.12}, {100, 0.08}};
  double positionNotional = 10000;          // typical position size (quote), picks the bracket
  std::vector<LeverageBracket> brackets;    // empty: no maintenance margin
  double candleBias = 0;                    // 0: off
  double decayHalfLifeBars = 0;             // 0: levels never fade
  double consume = 1;                       // share of a level removed when the price reaches it
  bool inverse = false;                     // COIN-M contracts (liquidationPriceInverse)
};

struct LiqGrid {
  double priceLow = 0;    // bottom of row 0
  double rowHeight = 1;
  int rows = 0;
  int rowOf(double price) const;   // −1 outside
};

// One bar of input. oi: open interest at the bar's close (base units, like volume); NaN when unknown (no change).
// ratio: long / short accounts; NaN or ≤ 0 when unknown (50/50).
struct LiqBar { double open, high, low, close, volume, oi, ratio; };

// The same, as columns (structure of arrays), for running many bars.
struct LiqSeries {
  std::span<const double> open, high, low, close, volume, oi, ratio;
  size_t size() const { return close.size(); }
  LiqBar at(size_t i) const { return {open[i], high[i], low[i], close[i], volume[i], oi[i], ratio.empty() ? 0.0 : ratio[i]}; }
};

class LiquidationModel {
public:
  LiquidationModel(const LiqGrid& grid, const LiqParams& params);
  // Room for this many bars of columns (so step() never allocates).
  void reserve(size_t bars);
  // Off: no per-bar columns kept (long histories scored on the fly; longColumn() / shortColumn() unusable).
  void setKeepColumns(bool on) { keepColumns_ = on; }
  void step(const LiqBar& b);
  void run(const LiqSeries& s, size_t from, size_t to);   // step() over [from, to)

  int bars() const { return bars_; }
  const LiqGrid& grid() const { return grid_; }
  // Resting quantity per price row at the end of a bar (rows floats each).
  const float* longColumn(int bar) const { return longCols_.data() + size_t(bar) * size_t(grid_.rows); }
  const float* shortColumn(int bar) const { return shortCols_.data() + size_t(bar) * size_t(grid_.rows); }
  // The current state (after the last bar), and what each bar liquidated.
  std::span<const double> longs() const { return longs_; }
  std::span<const double> shorts() const { return shorts_; }
  double liquidatedLongs(int bar) const { return liqLong_[size_t(bar)]; }
  double liquidatedShorts(int bar) const { return liqShort_[size_t(bar)]; }
  double outOfGrid() const { return outOfGrid_; }   // quantity whose level fell outside the price rows

private:
  void consume(double low, double high, double& liqLong, double& liqShort);
  void deposit(std::vector<double>& side, double price, double qty);

  LiqGrid grid_;
  LiqParams params_;
  struct Level { double longFactor, shortFactor, weight; };   // liquidation price = entry × factor (per leverage)
  std::vector<Level> levels_;
  double decay_ = 1;
  std::vector<double> longs_, shorts_;
  std::vector<float> longCols_, shortCols_;
  std::vector<double> liqLong_, liqShort_;
  double prevOi_ = 0;
  bool haveOi_ = false;
  double outOfGrid_ = 0;
  int bars_ = 0;
  bool keepColumns_ = true;
};

}  // namespace charts::flow
