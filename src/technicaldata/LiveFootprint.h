#pragma once
// Data › Technical data, the live footprint (2026-11-20, for the agents' get_footprint): a market's executed trades
// in memory (Charts' TradeTape, src/charts/core/Trades.h) folded into 1-minute footprints on the basis-point levels of
// the stored history (FootprintArchive.h's DayBuilder: the same minutes Binance's archive of that day will give), for
// the bars of today the daily archives publish only tomorrow. Memory only (LiveBook.h), never written.
//
// Whole minutes only: the tape's first trade's minute (history starts inside it) and its last trade's minute (still
// open) are left out, and so are the minutes around a hole of the tape (aggregate ids missing). Minutes folded once
// stay when their trades leave the tape, for kKept. Pure: tests/technicaldata/footprint_archive_tests.cpp.
#include "charts/core/Ticks.h"
#include "charts/core/Trades.h"
#include "technicaldata/FootprintArchive.h"
#include <map>
#include <utility>
#include <vector>

namespace technicaldata {

struct LiveFootprint {
  static constexpr qint64 kKept = 48 * 3600;
  std::map<qint64, fp::Minute> minutes;            // by open time (Unix s); minutes with trades
  std::vector<std::pair<qint64, qint64>> covered;  // whole minutes known, [from, to), ascending, disjoint
  bool covers(qint64 from, qint64 to) const;       // every minute of [from, to)
};

struct LiveFold { qint64 firstId = -1, missing = 0, lastTo = 0; };   // what the last fold saw

// Folds the tape's whole minutes into f: only those after the last fold, unless older trades came in since (history
// pages, a hole filled); then drops what is older than nowSec − kKept.
void foldTrades(const charts::flow::TradeTape& tape, const charts::flow::TickScale& ticks, LiveFootprint& f, LiveFold& st, qint64 nowSec);

}  // namespace technicaldata
