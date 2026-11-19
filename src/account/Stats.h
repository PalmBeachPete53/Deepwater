#pragma once
#include "account/Journal.h"
#include <QVector>
#include <QDate>
#include <QStringList>

// Statistics of a list of closed trades (sorted by close time). Every figure is computed from the trades'
// net P&L (profit + commission + swap) in the account currency. Dates use the local time zone.
namespace account {

struct Bucket { QString label; double pnl = 0; int count = 0; };

struct Stats {
  int n = 0, wins = 0, losses = 0, breakeven = 0;
  double net = 0, grossProfit = 0, grossLoss = 0;       // grossLoss positive
  double winRate = NAN;                                  // 0..1
  double profitFactor = NAN;
  double avgWin = NAN, avgLoss = NAN;                    // avgLoss negative
  double winLossRatio = NAN;                             // avgWin / |avgLoss|
  double maxDD = 0;                                      // most negative equity drop (<= 0), money
  double maxDDPct = 0;                                   // largest drop in % of the peak equity (balance + cumulative),
                                                         // <= 0; NAN when the starting balance is unknown (0)
  double currentDD = 0;                                  // equity now vs its peak (<= 0)
  double recoveryFactor = NAN;                           // net / |maxDD|
  double consistency = NAN;                              // share of profitable months, 0..1
  int profitableMonths = 0, months = 0;

  QVector<double> cumulative;                            // after each trade
  QVector<double> drawdown;                              // after each trade (<= 0)
  struct Day { QDate date; double pnl = 0, cum = 0; };
  QVector<Day> days;                                     // per close day (local)
  QVector<Bucket> byMonth, byQuarter;                    // chronological, empty periods between filled ones included
  QVector<Bucket> byWeekday;                             // Monday..Sunday, days without trades omitted
  QVector<Bucket> bySymbol;                              // P&L descending

  // Performance score: this app's own 0..100 scale per metric, and their plain average.
  struct Axis { QString name; double score = NAN; QString detail; };
  QVector<Axis> axes;                                    // Win %, Profit factor, Avg win/loss, Recovery factor, Max drawdown, Consistency
  double score = NAN;
};

Stats compute(const QList<Trade>& trades, double balance);

// Session of the open time (UTC hours): Asian 21–07, London 07–12, NY Overlap 12–16, New York 16–21.
QString session(const QDateTime& openUtc);
}
