#pragma once
// Data › Technical data → machine learning (step 3 of the user's plan, 2026-11-20: does the order flow add anything to a
// model on bars?): the values of one bar of a pair from what the pair records besides its bars, all scale-free (no
// price level), each known at the bar's close and never after it:
//
//   footprint, per market recorded (fp_spot_…, fp_perp_…), from the bar's own minutes (FootprintArchive.h) merged:
//     delta_pct        (bought − sold) / volume × 100
//     poc_dist         close vs the point of control, % (the level with the most volume)
//     va_width         value area (70 % of the volume around the POC) high − low, % of the close
//     va_pos           where the close sits in the value area: 0 = its low, 1 = its high
//     imb_buy/_sell    diagonal imbalances (ratio 3), counted
//     stack_buy/_sell  runs of 3 or more of them
//     low_delta_pct    delta of the lowest quarter of the bar's levels, % of the volume (sellers hitting the lows, or
//     high_delta_pct   absorbed there; the highest quarter likewise)
//     open_delta_pct   delta of the bar's opening minute, % of that minute's volume (2026-11-20, step 5: the imbalance
//                      at each quarter hour's open, said to predict the next 4 to 12 hours on perpetuals)
//     fp_perp_vol_ratio  the perpetual's volume / the pair's own (spot) volume
//   book depth (the USD-M perpetual, PerpArchive.h), the last snapshot before the close (at most 5 minutes old):
//     depth_imb_02 / _1 / _5   bids' share of the depth out to ±0.2 / ±1 / ±5 %, in %
//     depth_bid1_chg / _ask1_chg   change of the depth out to −1 % / +1 % since the last snapshot at the open, %
//   open interest (the perpetual's metrics), the last row before the close (at most 10 minutes old):
//     oi_chg_pct       open interest change since the last row at the open, %
//     oi_top_pos_ls, oi_accounts_ls, oi_taker_ratio   top traders' long/short by positions, all accounts', takers'
//
// A bar is covered when every family the pair records has its values there; the ML table keeps only covered bars so
// that a model on the bars alone and one with these columns learn from the very same rows.
// Timeframes of fixed length up to a day (1m … 1D: a bar never crosses a UTC day boundary of the archives).
#include "technicaldata/FootprintArchive.h"
#include "technicaldata/PerpArchive.h"
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <vector>

namespace technicaldata {

class Store;

class OrderFlowFeatures {
public:
  OrderFlowFeatures(const Store& store, const QString& pair, int tfSeconds);
  bool available() const { return !m_cols.isEmpty(); }   // the pair records something and the timeframe suits
  const QStringList& columns() const { return m_cols; }
  // The bar opening at barTime (Unix s); close and volume from the pair's own bar. False: not covered (out untouched).
  bool row(qint64 barTime, double close, double volume, std::vector<double>& out);
  static bool isColumn(const QString& name);   // fp_…, depth_…, oi_…: the order flow's columns

private:
  const std::vector<fp::Minute>* fpDay(const QString& venue, qint64 day);
  const std::vector<perp::DepthSample>* depthDay(qint64 day);
  const std::vector<perp::OiRow>* oiDay(qint64 day);
  const perp::DepthSample* depthAt(qint64 t, bool inclusive);   // the last snapshot before t (or at t)
  const perp::OiRow* oiAt(qint64 t, bool inclusive);
  const Store& m_store;
  QString m_pair;
  int m_tf = 0;
  QStringList m_venues;   // footprint markets recorded
  bool m_depth = false, m_oi = false;
  QStringList m_cols;
  QSet<qint64> m_fpDays[2], m_depthDays, m_oiDays;   // stored days
  // decoded days, the last few (a table goes forward in time)
  QHash<QString, std::vector<fp::Minute>> m_fpCache;
  QHash<qint64, std::vector<perp::DepthSample>> m_depthCache;
  QHash<qint64, std::vector<perp::OiRow>> m_oiCache;
};

}  // namespace technicaldata
