#pragma once
// Order flow: Big Trades — large aggressive orders, found in the trade tape. Pure (no Qt widgets, no network).
//
//   Aggregator         exchanges split one market order into many executions; consecutive trades of the same side
//                      within a time window (counted from the FIRST trade of the group, so a busy stream cannot chain a
//                      group forever) and at the same price — or, in sweep mode, walking the book in the aggressive
//                      direction (buys at a price ≥ the previous one, sells ≤) — make one aggregated trade.
//   RollingPercentile  a percentile of the last N minutes of aggregated sizes without sorting everything at each trade:
//                      values kept in sorted blocks of ≤ 512, insert/erase/quantile in O(√n)-ish.
//   MinuteAtr          ATR(14) / ATR(120) on 1-minute bars built from the trades (independent of the chart's timeframe):
//                      the volatility factor, clamped to [0.5, 2]; 1 until 120 minutes have closed.
//   Detector           manual mode: min ≤ volume ≤ max (max 0 = no max, so several Big Trades can split sizes into
//                      tranches; min 0 = nothing, since every order would pass); auto mode: big = strictly above the
//                      percentile of the window × the volatility factor (sizes cluster on round lots). Each aggregate
//                      is judged with the threshold computed WITHOUT it, then added to the window (no look-ahead, so
//                      history and live give the same answers). While the window holds fewer than minSamples sizes, the
//                      fallback minimum is used (0 = nothing is shown during the warm-up).
//   bubbleDiameter     area ∝ volume: d = minD · √(volume / reference), clamped to [minD, maxD].
// Decisions taken with the user's leave (2026-11-02, kept 2026-11-18): window from the first trade, 50 ms default,
// same price default, ATR ratio on 1-minute bars, fallback 0, judge-before-add.
#include "charts/core/Trades.h"
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <vector>

namespace charts::flow {

struct AggregatedTrade {
  int64_t startMs = 0, endMs = 0;   // first and last execution
  Side side = Side::Unknown;
  double volume = 0;
  double vwapTicks = 0;             // volume-weighted average price, in ticks (not a whole number in a sweep)
  int64_t minTicks = 0, maxTicks = 0;
  int executions = 0;               // tape trades grouped (Binance's own aggTrades already merge same-price fills)
  int64_t firstId = 0, lastId = 0;
  bool approximate = false;         // some side came from the quote or tick rule
};

struct AggregatorParams {
  int64_t windowMs = 50;    // 0 = only trades with exactly the same timestamp
  bool sweep = false;       // false: same price only; true: several levels in the aggressive direction
};

class Aggregator {
public:
  using Params = AggregatorParams;
  explicit Aggregator(Params p = Params()) : p_(p) {}
  // Adds a trade (in id order). Returns the aggregate it closed, if the trade does not fit the open one.
  std::optional<AggregatedTrade> add(const Trade& t);
  // Closes the open aggregate if no trade can join it any more at time nowMs (live: called by a timer; history: at the
  // end with INT64_MAX).
  std::optional<AggregatedTrade> flush(int64_t nowMs);
  bool hasOpen() const { return open_; }
  void reset() { open_ = false; }

private:
  bool fits(const Trade& t) const;
  Params p_;
  bool open_ = false;
  AggregatedTrade cur_;
  int64_t lastTicks_ = 0;
  double sumPxVol_ = 0;
};

class RollingPercentile {
public:
  static constexpr size_t kBlock = 512;
  void add(int64_t ms, double v);
  void expireBefore(int64_t ms);    // drops values added at a time < ms (times must not decrease)
  size_t size() const { return n_; }
  size_t blocks() const { return blocks_.size(); }   // sorted blocks (each ≤ 2 × kBlock values)
  // Linear interpolation between closest ranks (numpy's default): rank = p/100 · (n − 1). NaN when empty.
  double quantile(double pct) const;
  void clear();

private:
  void insertSorted(double v);
  void eraseSorted(double v);
  std::vector<std::vector<double>> blocks_;   // each sorted, blocks in order, none empty
  std::deque<std::pair<int64_t, double>> fifo_;
  size_t n_ = 0;
};

class MinuteAtr {
public:
  // A price seen at a time (any order within the current minute; minutes must not go back).
  void add(int64_t ms, int64_t lowTicks, int64_t highTicks, int64_t lastTicks);
  double shortAtr() const;          // SMA of the last 14 true ranges (NaN until 14 minutes closed)
  double longAtr() const;           // SMA of the last 120
  int closedMinutes() const { return int(trs_.size()); }
  double lastTrueRange() const { return trs_.empty() ? std::numeric_limits<double>::quiet_NaN() : trs_.back(); }
  // clamp(short / long, 0.5, 2); 1 until 120 minutes have closed or when long is 0
  double factor() const;
  void clear() { *this = MinuteAtr(); }

private:
  void close();
  bool have_ = false, havePrev_ = false;
  int64_t minute_ = 0, lo_ = 0, hi_ = 0, last_ = 0, prevClose_ = 0;
  std::deque<double> trs_;          // last 120 true ranges
};

struct DetectorParams {
  enum class Mode { Manual, Auto } mode = Mode::Auto;
  double minVolume = 0;             // manual threshold; also the fallback during the auto warm-up (0 = nothing shown)
  double maxVolume = 0;             // 0 = no maximum
  double percentile = 99;
  int64_t windowMs = 30 * 60 * 1000;
  bool volatility = true;
  size_t minSamples = 100;
};

class Detector {
public:
  explicit Detector(DetectorParams p = DetectorParams()) : p_(p) {}
  struct Verdict {
    bool big = false;
    double threshold = 0;           // the minimum used (NaN while warming up with no fallback)
    double percentileValue = 0;     // auto: the window's percentile before the volatility factor
    double factor = 1;              // auto: the volatility factor used
    bool warm = false;              // auto: the window held enough sizes
  };
  // Judges a closed aggregate with what came before it, then adds it to the window and the ATR.
  Verdict judge(const AggregatedTrade& a);
  // The threshold the next aggregate would be judged with (for the legend), at time nowMs.
  Verdict current(int64_t nowMs);
  const DetectorParams& params() const { return p_; }
  size_t samples() const { return window_.size(); }   // sizes in the window (auto mode)
  void reset();

private:
  Verdict threshold(int64_t nowMs);
  DetectorParams p_;
  RollingPercentile window_;
  MinuteAtr atr_;
};

// The alert of a big order: its volume is at least `factor` (≥ 1) times the threshold it passed, so alerts are a
// second, higher bar than the display (user's spec: "un second seuil, plus élevé que le seuil d'affichage").
bool bigTradeAlert(double volume, double threshold, double factor);

// Bubble diameter (px): area proportional to volume, from minD at the reference volume (the threshold) up to maxD.
double bubbleDiameter(double volume, double reference, double minD, double maxD);
// Opacity from minOpacity (smallest bubble) to 1 (largest), following the diameter.
double bubbleOpacity(double diameter, double minD, double maxD, double minOpacity);

}  // namespace charts::flow
