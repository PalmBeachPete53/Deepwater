#pragma once
// Charts, chantier F: structural liquidity in the Smart Money Concepts / ICT sense, from OHLC bars alone (step F1:
// pivots and the level engine). Where stops probably gather: above notable highs (buy-side liquidity) and below
// notable lows (sell-side), stronger where several highs or lows sit at the same price (equal highs / equal lows).
// A level is swept when the price goes through it. Pure (QtCore types only); no allocation per bar beyond new levels.
// Separate from analytics::Liquidity (Market › Liquidity and the AI tools), whose rules differ and which stays as is.
//
// Pivots (SwingTracker, detectSwings):
//   Window   a high is a pivot when it is strictly higher than the `left` bars before it and at least as high as the
//            `right` bars after it (the leftmost of equal tops wins), mirrored for lows. It is CONFIRMED only `right`
//            bars later; until then it is provisional (detectSwings returns both, apart). The first `left` bars can't be
//            pivots (no full window before them).
//   Percent  / Atr   zigzag: the running extreme becomes a pivot when the price comes back from it by `percent` % of it
//            (or `atrMult` × ATR(atrLength), Wilder) — confirmed on that bar; the running extreme is the provisional one.
// Levels (LiquidityLevelEngine::step, one bar at a time):
//   1. the bar sweeps active levels it goes through — buy-side: high above the level (Wick) or close above it (Close);
//      sell-side mirrored. The level keeps the sweeping bar, its time and the depth (how far the wick went past). A bar
//      reaching into a level's zone without sweeping it is a test (counted once per bar).
//   2. a newly confirmed pivot joins an active level of its side within the tolerance (ATR fraction or % of price, at
//      the time of confirmation) — the group's price becomes its most extreme member (stops sit beyond the highest high)
//      and the level becomes Equal — or starts a new level with the next id. Ids never change. A level the pivot's own
//      bar went through by less than the tolerance was not swept: that bar was the equal high forming (it comes back).
//   Zones: from the level's price outwards (above highs, below lows) by `thickness` ticks, % of price or ATR fraction.
//   3. previous day / week and session highs and lows (SessionLevels.h), when on, join as levels of their own kind as
//      soon as their period ends — before this bar's sweep check, so a bar opening past yesterday's high sweeps it.
//   Score (score()): 1 per member, +0.5 per test survived, +up to 0.5 with age (full at 200 bars), +0.25 per doubling of
//   the detection timeframe over 1 minute. Only a ranking for the drawing (opacity), not a probability.
// Active levels are kept sorted by price per side: the levels a bar can touch are found by binary search on [low, high].
#include "charts/core/Types.h"
#include <QTime>
#include <QTimeZone>
#include <memory>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace charts::smc {

enum class PivotMethod { Window, Percent, Atr };
struct SwingParams {
  PivotMethod method = PivotMethod::Window;
  int left = 5, right = 5;     // Window
  double percent = 0.5;        // Percent: reversal in % of the extreme
  double atrMult = 1.5;        // Atr: reversal in ATRs
  int atrLength = 14;
};

struct Pivot {
  int index = -1;          // the bar of the high / low
  int64_t time = 0;        // its time (s)
  double price = 0;
  bool high = true;
  int confirmedAt = -1;    // the bar on which it became certain (-1: provisional)
};

// Wilder's ATR, one bar at a time (NaN until `length` bars).
class Atr {
public:
  explicit Atr(int length = 14) : len_(std::max(1, length)) {}
  double push(const Bar& b);
  double value() const { return n_ >= len_ ? atr_ : std::numeric_limits<double>::quiet_NaN(); }

private:
  int len_, n_ = 0;
  double atr_ = 0, sum_ = 0, prevClose_ = 0;
};

// Incremental pivot detection: push() each bar, get the pivots it confirms (at most one high and one low).
class SwingTracker {
public:
  explicit SwingTracker(const SwingParams& p);
  struct Confirmed { std::optional<Pivot> high, low; };
  Confirmed push(const Bar& b, double atr);   // atr: the bar's ATR (Atr method), NaN otherwise
  // Pivots not confirmed yet (Window: the candidates among the last `right` bars; zigzag: the running extreme).
  std::vector<Pivot> provisional() const;
  int bars() const { return n_; }

private:
  SwingParams p_;
  int n_ = 0;
  std::vector<Bar> ring_;          // Window: the last left + right + 1 bars
  // zigzag state
  int dir_ = 0;                    // 0 undecided, +1 tracking a high, -1 tracking a low
  double hi_ = 0, lo_ = 0;
  int hiIdx_ = -1, loIdx_ = -1;
  int64_t hiTime_ = 0, loTime_ = 0;
};

struct SwingResult { std::vector<Pivot> confirmed, provisional; };
SwingResult detectSwings(std::span<const Bar> bars, const SwingParams& p);

enum class LevelSide : uint8_t { BuySide, SellSide };   // above highs / below lows
enum class LevelKind : uint8_t { Swing, Equal, PrevDay, PrevWeek, Asia, London, NewYork };
enum class SizeUnit : uint8_t { Ticks, Percent, Atr };
enum class SweepRule : uint8_t { Wick, Close };

// Sessions and periods (SessionLevels.h): a session is its local hours in its own time zone.
struct SessionDef {
  LevelKind kind = LevelKind::Asia;
  QTimeZone tz;
  QTime start, end;                 // local hours; end ≤ start: runs past midnight
};
struct PeriodParams {
  bool day = true, week = true;
  QTimeZone tz = QTimeZone::UTC;    // the day and week calendar
  int dayStartMinutes = 0;
  std::vector<SessionDef> sessions; // the sessions turned on
};
class SessionLevelTracker;
struct PeriodLevel;

struct Level {
  uint32_t id = 0;
  LevelSide side = LevelSide::BuySide;
  LevelKind kind = LevelKind::Swing;
  double price = 0;                // the liquidity line (highest member high / lowest member low)
  double zoneLow = 0, zoneHigh = 0;
  int originIndex = -1;            // first member's bar
  int64_t originTime = 0;          // its time (s) — levels travel between timeframes by time
  int lastMemberIndex = -1;
  int members = 1;
  int tests = 0;
  int lastTestIndex = -1;
  int tfSeconds = 60;              // detection timeframe
  bool swept = false;
  int sweptIndex = -1;
  int64_t sweptTime = 0;
  double depth = 0;                // how far the sweeping bar went past the price
};

struct EngineParams {
  SwingParams swing;
  SizeUnit toleranceUnit = SizeUnit::Atr;
  double tolerance = 0.1;          // equal highs / lows: within this (ATRs, or % of price)
  SizeUnit thicknessUnit = SizeUnit::Atr;
  double thickness = 0.1;          // zone height (ticks, % of price, ATRs)
  double tick = 0.01;
  SweepRule sweep = SweepRule::Wick;
  int tfSeconds = 60;
  bool periodLevels = false;       // previous day / week and sessions as levels (F3)
  PeriodParams periods;
};

class LiquidityLevelEngine {
public:
  explicit LiquidityLevelEngine(const EngineParams& p);
  ~LiquidityLevelEngine();
  LiquidityLevelEngine(LiquidityLevelEngine&&) noexcept;
  void reserve(size_t levels);
  void step(const Bar& b);
  void run(std::span<const Bar> bars) { for (const Bar& b : bars) step(b); }

  int bars() const { return n_; }
  const std::vector<Level>& levels() const { return levels_; }   // by id (levels()[id]), swept ones included
  // Active (unswept) levels of a side, ascending by price (indices into levels()).
  const std::vector<uint32_t>& active(LevelSide s) const { return s == LevelSide::BuySide ? buy_ : sell_; }
  double score(const Level& l) const;
  const SwingTracker& swings() const { return tracker_; }
  double atr() const { return atr_.value(); }

private:
  double sizeOf(SizeUnit u, double v, double price) const;
  void addPivot(const Pivot& p);
  void addPeriod(const PeriodLevel& p);
  void sweepSide(std::vector<uint32_t>& act, const Bar& b, bool buySide);

  EngineParams p_;
  Atr atr_;
  SwingTracker tracker_;
  std::vector<Level> levels_;
  std::vector<uint32_t> buy_, sell_;
  double maxZone_[2] = {0, 0};     // widest zone ever made per side (buy, sell): the search margin
  std::unique_ptr<SessionLevelTracker> periods_;
  std::vector<PeriodLevel> finished_;
  int n_ = 0;
};

}  // namespace charts::smc
