// Order flow, step E4: scoring the liquidation estimate against real liquidations, and choosing the leverage mix.
// The "real" liquidations here come from a known market: the model itself run with every position at 25× — the
// calibrator, which never sees that, must find a mix leaning to 25× and score better than the default on later data.
#include "Check.h"
#include "charts/core/LiqCalibration.h"
#include <cmath>
#include <random>

using namespace charts;
using namespace charts::flow;


TEST("calibration score: a liquidation in a dense row counts, one elsewhere doesn't, the bar before is what counts") {
  LiqParams p;
  p.leverages = {{10, 1}};
  LiquidationModel m({49.5, 1, 100}, p);   // rows of 1 around whole prices
  m.step({100, 100, 100, 100, 1e9, 1000, NaN});
  m.step({100, 100, 100, 100, 1e9, 1100, NaN});   // longs at 90 (row 40), shorts at 110 (row 60)
  m.step({100, 100, 100, 100, 1e9, 1100, NaN});
  const std::vector<int64_t> open = {0, 60000, 120000};
  // bar 2 (the column before: bar 1, which has the levels)
  const std::vector<LiqEvent> ev = {{130000, 90.2, 2, 1, 0},    // a long at 90: dense → hit
                                    {130000, 91.1, 1, 1, 0},    // a neighbour row: hit
                                    {130000, 75.0, 3, 1, 0},    // far: miss
                                    {130000, 110.0, 4, 0, 0},   // a short at 110: hit (shorts' side)
                                    {130000, 90.0, 5, 0, 0},    // a short at 90: the shorts have nothing there → miss
                                    {10000, 90.0, 7, 1, 0}};    // in bar 0: no column before it → not scored
  const CalibScore s = scoreModel(m, open, 60000, ev, 0.05);
  CHECK_EQ(s.events, 5);
  CHECK_EQ(s.qty, 15.0);
  CHECK(std::fabs(s.hit - (2 + 1 + 4) / 15.0) < 1e-12);
}

namespace {
// a market where everyone uses 25×: bars of a random walk, open interest that only grows, and the liquidations the
// model of that market produces (what each bar's range took from each row, bar by bar)
struct Market {
  std::vector<double> o, h, l, c, v, oi, ratio;
  std::vector<int64_t> openMs;
  std::vector<LiqEvent> events;
  LiqGrid grid{40000, 50, 400};
  Market(int n, const std::vector<LiqParams::Lev>& truth) {
    std::mt19937 rng(3);
    std::normal_distribution<double> ret(0, 0.006);
    std::uniform_real_distribution<double> u(0, 1);
    double p = 60000, openInterest = 50000;
    LiqParams tp;
    tp.leverages = truth;
    LiquidationModel m(grid, tp);
    for (int i = 0; i < n; ++i) {
      const double close = p * std::exp(ret(rng));
      o.push_back(p);
      c.push_back(close);
      h.push_back(std::max(p, close) * (1 + 0.003 * u(rng)));
      l.push_back(std::min(p, close) * (1 - 0.003 * u(rng)));
      v.push_back(1e9);
      openInterest += 50 + 100 * u(rng);
      oi.push_back(openInterest);
      ratio.push_back(NaN);
      openMs.push_back(int64_t(i) * 3600000);
      // what this bar liquidates: the same model, this bar's range, no new positions (open interest unchanged)
      LiquidationModel probe = m;
      probe.step({o.back(), h.back(), l.back(), c.back(), 0, i ? oi[size_t(i) - 1] : NaN, NaN});
      if (i) {
        for (int r = 0; r < grid.rows; ++r) {
          const double gotL = m.longs()[size_t(r)] - probe.longs()[size_t(r)], gotS = m.shorts()[size_t(r)] - probe.shorts()[size_t(r)];
          const double price = grid.priceLow + (r + 0.5) * grid.rowHeight;
          if (gotL > 1e-9) events.push_back({openMs.back() + 1800000, price, gotL, 1, 0});
          if (gotS > 1e-9) events.push_back({openMs.back() + 1800000, price, gotS, 0, 0});
        }
      }
      m.step({o.back(), h.back(), l.back(), c.back(), v.back(), oi.back(), NaN});
      p = close;
    }
  }
  LiqInput input() const {
    LiqInput in;
    in.open = o; in.high = h; in.low = l; in.close = c; in.volume = v; in.oi = oi; in.ratio = ratio;
    in.openMs = openMs;
    in.barMs = 3600000;
    in.grid = grid;
    return in;
  }
};
}  // namespace

TEST("calibration: finds the leverage of a market where everyone uses 25×, and beats the default on unseen data") {
  const Market mk(500, {{25, 1}});
  CHECK(mk.events.size() > 60);
  const Calibration c = calibrate(mk.input(), LiqParams{}, mk.events);
  CHECK(c.ok);
  CHECK_EQ(c.candidates, 70);
  double w25 = 0;
  for (const auto& l : c.best)
    if (l.leverage == 25) w25 = l.weight;
  std::printf("  calibration: best mix 25× weight %.2f; rank train %.3f (default %.3f), test %.3f (default %.3f); %d events, %.0f ms\n", w25,
              c.rankTrainBest, c.rankTrainDefault, c.rankTestBest, c.rankTestDefault, c.eventsInRange, c.ms);
  CHECK(w25 >= 0.5);
  CHECK(c.rankTestBest > c.rankTestDefault);
  CHECK(c.rankTestBest > 0.75);  // the true mix ranks the liquidations high (ties among dense rows keep it below 1)
}

TEST("calibration: too few real liquidations → nothing chosen, the default mix kept") {
  const Market mk(60, {{25, 1}});
  std::vector<LiqEvent> few(mk.events.begin(), mk.events.begin() + std::min<size_t>(10, mk.events.size()));
  const Calibration c = calibrate(mk.input(), LiqParams{}, few);
  CHECK(!c.ok);
  CHECK_EQ(int(c.best.size()), int(calibrationLeverages().size()));
  CHECK_EQ(c.candidates, 0);
}

TEST("archive files: klines (with or without header), metrics out of order, liquidations with repeated lines") {
  const QByteArray kl = "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,taker_buy_quote_volume,ignore\n"
                        "1722816300000,57900.1,58000,57700.5,57800,1234,1722816599999,2.1,10,600,1.0,0\n"
                        "1722816000000,58000,58100,57850,57900.1,999,1722816299999,1.7,9,500,0.8,0\n";
  const auto bars = parseArchiveKlines(kl);
  CHECK_EQ(int(bars.size()), 2);
  CHECK_EQ(bars[0].time, qint64(1722816000));   // sorted, seconds
  CHECK_EQ(bars[1].low, 57700.5);
  CHECK_EQ(bars[1].volume, 1234.0);
  CHECK_EQ(int(parseArchiveKlines("1722816000000,1,2,0.5,1.5,7,1722816299999,0,0,0,0,0").size()), 1);   // older files: no header
  // the rows of a real metrics file (BTCUSDT 2026-11-10: not in time order)
  const QByteArray me = "create_time,symbol,sum_open_interest,sum_open_interest_value,count_toptrader_long_short_ratio,sum_toptrader_long_short_ratio,"
                        "count_long_short_ratio,sum_taker_long_short_vol_ratio\n"
                        "2026-11-10 05:40:00,BTCUSDT,98691.6930000000000000,7389698420.0838000000000000,3.15166337,2.34416100,2.80764210,1.57560700\n"
                        "2026-11-10 00:05:00,BTCUSDT,98000.5,7300000000,3.1,2.3,2.5,1.2\n"
                        "2024-08-05 00:00:00,BTCUSD_PERP,16437041.00000000,28279.89924763,,,,1.27260143\n";
  std::vector<OiPoint> oi;
  std::vector<RatioPoint> ratio;
  parseArchiveMetrics(me, oi, ratio);
  CHECK_EQ(int(oi.size()), 3);
  CHECK_EQ(oi[0].oi, 16437041.0);                   // the oldest first
  CHECK_EQ(oi[2].timeMs, qint64(1794289200000));    // 2026-11-10 05:40 UTC
  CHECK_EQ(oi[2].oi, 98691.693);
  CHECK_EQ(int(ratio.size()), 2);                   // the COIN-M row has no accounts ratio
  CHECK_EQ(ratio[1].ratio, 2.8076421);
  // a real liquidation file's first lines (BTCUSD_PERP 2024-08-05: the first fill twice)
  const QByteArray lq = "time,side,order_type,time_in_force,original_quantity,price,average_price,order_status,last_fill_quantity,accumulated_fill_quantity\n"
                        "1722816530063,SELL,LIMIT,IOC,2452,57704.5,57908.9,FILLED,180,2452\n"
                        "1722816530063,SELL,LIMIT,IOC,2452,57704.5,57908.9,FILLED,180,2452\n"
                        "1722816539029,SELL,LIMIT,IOC,1,57676.2,57902.0,FILLED,1,1\n"
                        "1722816540000,BUY,LIMIT,IOC,5,58100,0,FILLED,5,5\n";
  const auto ev = parseArchiveLiquidations(lq);
  CHECK_EQ(int(ev.size()), 3);
  CHECK_EQ(ev[0].price, 57908.9);                   // the average fill
  CHECK_EQ(ev[0].qty, 2452.0);
  CHECK_EQ(ev[0].longLiquidated, 1);
  CHECK_EQ(ev[2].longLiquidated, 0);
  CHECK_EQ(ev[2].price, 58100.0);                   // no average: the order price
}

TEST("streaming score: the same as scoring the stored columns; the price-proximity baseline counted apart") {
  const Market mk(400, {{25, 1}});
  const LiqInput in = mk.input();
  LiqParams p;
  p.leverages = {{25, 1}};
  LiquidationModel m(in.grid, p);
  m.run(in.series(), 0, in.close.size());
  const int64_t cut = mk.events[mk.events.size() / 2].timeMs;
  std::vector<LiqEvent> train, test;
  for (const auto& e : mk.events) (e.timeMs < cut ? train : test).push_back(e);
  const CalibScore a = scoreModel(m, in.openMs, in.barMs, train), b = scoreModel(m, in.openMs, in.barMs, test);
  const StreamScores s = scoreStream(in.series(), in.openMs, in.barMs, in.grid, p, mk.events, cut);
  CHECK_EQ(s.trainModel.events, a.events);
  CHECK(std::fabs(s.trainModel.hit - a.hit) < 1e-12);
  CHECK(std::fabs(s.testModel.hit - b.hit) < 1e-12);
  CHECK(s.trainNear.events == s.trainModel.events);
  CHECK(s.trainNear.hit >= 0 && s.trainNear.hit <= 1);
  // the baseline on a hand case: open at 100 in rows of 1 from 49.5, 5 % of 100 rows = the 5 rows from 100 down (96 … 100)
  LiqParams one;
  one.leverages = {{10, 1}};
  std::vector<double> o = {100, 100}, h = {100, 100}, l = {100, 100}, c = {100, 100}, v = {1e9, 1e9}, oi = {1000, 1100}, r = {NaN, NaN};
  const std::vector<int64_t> open = {0, 60000};
  const LiqSeries s2{o, h, l, c, v, oi, r};
  const std::vector<LiqEvent> ev = {{70000, 95.0, 1, 1, 0},    // a long at 95: next to the 96 … 100 rows (± 1) → baseline hit
                                    {70000, 90.0, 1, 1, 0},    // at 90: baseline miss
                                    {70000, 104.0, 1, 0, 0}};  // a short at 104: rows 100 … 104 → baseline hit
  const StreamScores s3 = scoreStream(s2, open, 60000, {49.5, 1, 100}, one, ev, 1LL << 62);
  CHECK_EQ(s3.trainNear.events, 3);
  CHECK(std::fabs(s3.trainNear.hit - 2.0 / 3) < 1e-12);
  // rank: bar 0 sets the open interest, bar 1 opens longs at 100 (levels at 90, row 40), bar 2 goes from 85 to 100
  // (rows 35 … 50, 16 rows)
  std::vector<double> o3 = {100, 100, 100}, h3 = {100, 100, 100}, l3 = {100, 100, 85}, c3 = {100, 100, 100}, v3 = {1e9, 1e9, 1e9},
                      oi3 = {1000, 1100, 1100}, r3 = {NaN, NaN, NaN};
  const std::vector<int64_t> open3 = {0, 60000, 120000};
  const LiqSeries s4{o3, h3, l3, c3, v3, oi3, r3};
  const std::vector<LiqEvent> ev2 = {{130000, 90.0, 3, 1, 0},    // on the dense row: rank 1 (above the 15 others)
                                     {130000, 95.0, 1, 1, 0}};   // on an empty row: ties with 14 others → 7 / 15
  const StreamScores s5 = scoreStream(s4, open3, 60000, {49.5, 1, 100}, one, ev2, 1LL << 62);
  CHECK_EQ(s5.trainRank.events, 2);
  CHECK(std::fabs(s5.trainRank.rank - (3 * 1.0 + 1 * (7.0 / 15)) / 4) < 1e-12);
}
