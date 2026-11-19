#pragma once
// Order flow, chantier E, step E4: checking the liquidation estimate against the real liquidations Binance published, and
// choosing the leverage mix that matches them best. Pure.
//
//   scoreModel   for every real liquidation, the model's column of the bar BEFORE it (what the heatmap showed before it
//                happened), on the liquidated side (a long liquidated → the longs' levels): is its price row, or a
//                neighbour, among the column's densest rows (the top `topShare` of all rows, 5 % by default)? The score
//                is the share of liquidated QUANTITY that fell in those dense rows. Dense rows are a fixed share of the
//                grid, so every leverage mix is judged with the same room.
//   calibrate    the 70 mixes of 5×, 10×, 25×, 50×, 100× in steps of 25 %: each one is run on the bars and judged on
//                the first two thirds of the real liquidations (in time) by RANK (below: where, among the rows the bar
//                crossed, the liquidation's row stands in the model's density — 0.5 is chance); the best one is then
//                scored on the last third, which it never saw (the honest number), next to the default mix. Below 30
//                real liquidations in the bars' time, nothing is chosen. (A first version judged by the share in the
//                densest rows: on 16 months of Binance's archive that favoured whatever puts dense rows next to the
//                price — liquidations happen where the price is — so it measured closeness, not knowledge.)
// Binance publishes at most one liquidation per symbol per second, so these are a sample; large cascades are
// under-counted, the score stays a comparison between mixes, not a measure of the real share.
#include "charts/core/FuturesData.h"
#include "charts/core/LiqRender.h"
#include <span>
#include <vector>

namespace charts::flow {

struct CalibScore {
  double hit = 0;      // share of liquidated quantity in the dense rows (0 … 1)
  double qty = 0;      // liquidated quantity scored
  int events = 0;      // liquidations scored
};

CalibScore scoreModel(const LiquidationModel& m, std::span<const int64_t> openMs, int64_t barMs, std::span<const LiqEvent> events,
                      double topShare = 0.05);

struct RankScore;
struct Calibration {
  bool ok = false;                         // enough liquidations to choose
  std::vector<LiqParams::Lev> best;        // the chosen mix (the default one when not ok)
  CalibScore trainDefault, trainBest, testDefault, testBest;   // share in the dense rows (kept for reference)
  // the criterion: rank among the rows each bar crossed (0.5 = chance; see RankScore below)
  double rankTrainDefault = 0, rankTrainBest = 0, rankTestDefault = 0, rankTestBest = 0;
  int candidates = 0;
  int eventsInRange = 0;
  double ms = 0;
};

// The leverages tried and their default weights.
const std::vector<LiqParams::Lev>& calibrationLeverages();
Calibration calibrate(const LiqInput& in, const LiqParams& base, std::span<const LiqEvent> events, double topShare = 0.05);

}  // namespace charts::flow

namespace charts::flow {

// ---- long histories (Binance's public archive, data.binance.vision), scored as the model runs ----
//
// Archive files (CSV inside daily zips), as published:
//   klines         open_time,open,high,low,close,volume,close_time,…   (older files have no header line)
//   metrics        create_time (UTC "YYYY-MM-DD HH:MM:SS", end of the 5 min period),symbol,sum_open_interest,
//                  sum_open_interest_value,count_toptrader_long_short_ratio,sum_toptrader_long_short_ratio,
//                  count_long_short_ratio,sum_taker_long_short_vol_ratio — rows not always in time order; the
//                  accounts ratio is empty for COIN-M
//   liquidationSnapshot (COIN-M only, 2023-06-25 … 2024-10-14)  time,side,order_type,time_in_force,original_quantity,
//                  price,average_price,order_status,last_fill_quantity,accumulated_fill_quantity — some lines repeated
//                  exactly (one fill reported twice): kept once
std::vector<Bar> parseArchiveKlines(const QByteArray& csv);
void parseArchiveMetrics(const QByteArray& csv, std::vector<OiPoint>& oi, std::vector<RatioPoint>& ratios);
std::vector<LiqEvent> parseArchiveLiquidations(const QByteArray& csv);

// One mix over a long history without keeping columns: each bar's real liquidations are scored against the levels
// as they stood before that bar, by the model (its densest rows) AND by a baseline that knows nothing but the price —
// the same number of rows right next to the bar's open on the liquidated side (below it for longs, above for shorts).
// Liquidations happen where the price is, so the baseline already catches many: the model is only worth something
// where it beats it. Events before cutMs are "train", the others "test".
// The fairer number, rank: among the price rows the bar actually went through (its low to its high), where does the row
// of the liquidation stand in the model's density (before the bar)? 0 = the emptiest of them, 1 = the densest; ties
// share (half). Weighted by quantity. 0.5 = the model says nothing about where, inside what the price crossed, the
// liquidations would be; above 0.5 it does. Bars crossing a single row are left out (nothing to rank).
struct RankScore { double rank = 0; double qty = 0; int events = 0; };
struct StreamScores { CalibScore trainModel, testModel, trainNear, testNear; RankScore trainRank, testRank; };
StreamScores scoreStream(const LiqSeries& s, std::span<const int64_t> openMs, int64_t barMs, const LiqGrid& grid, const LiqParams& p,
                         std::span<const LiqEvent> events, int64_t cutMs, double topShare = 0.05);
// Every mix tried by calibrate() (70: 5×, 10×, 25×, 50×, 100× in quarters).
std::vector<std::vector<LiqParams::Lev>> calibrationMixes();

}  // namespace charts::flow
