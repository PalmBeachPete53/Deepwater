#pragma once
#include <QDate>
#include <QList>
#include <QString>
#include <QVector>
#include <QObject>
#include <cmath>
#include <functional>

// Seasonality statistics engine (no UI, no network except loadHistory). Used by Market › Seasonality and meant to be
// reused by the Markets Watch sheet (seasonality by month: monthlyAverages(); year path: yearPath()).
//
// Model
//  - Input: daily closes of one instrument with exchange-local dates, ascending (loadHistory() gives them).
//  - Calendar slots: a date maps to a slot 0..365 of a leap-year template (Jan 1 = 0, Feb 29 = 59, Mar 1 = 60,
//    Dec 31 = 365), so a given month/day sits at the same x in every year. Days without a close (weekends,
//    holidays, Feb 29 of common years) carry the last close.
//  - Years: the N calendar years ending with the year of `today` (N = 5/10/15/20). A year is used when a close
//    exists in the 10 days before its Jan 1 (its base) or its first close falls in its first 10 days (that close is
//    the base); years without enough history are dropped (Dataset::years can be shorter than N).
//  - Year path: cumulative return in % at each slot = close(slot) / base − 1. The current year stops at its last
//    close ("reach"); finished years reach slot 365.
//  - Average path: mean of the year paths of the finished years at each slot (the unfinished current year is left
//    out: it is shown as its own overlay with yearPath(); including it would put a step at today's date and bias
//    pathReturn / maxDrawdown of any period after today). NaN everywhere when no year is finished. smoothPath() is the 3-day
//    Gaussian-weighted moving average shown on screen (σ = 1 day, weights over ±1 day, renormalised at the edges).
//  - Period [a, b] (slots, inclusive): per-year return = close(b) / close(a − 1) − 1 (close(−1) = base). A year
//    "counts" for a period when its data covers the whole period: finished years always, the current year only
//    once the period is over. Win rate, mean / median / std of period returns, best / worst year use counted years.
//  - Month return (year y, month m) = the period return of [first slot of m, last slot of m]. Month averages use
//    counted years; with excludeOutliers, for each month the years whose return is more than 1.5 standard
//    deviations from that month's mean are left out and the mean is recomputed (affects the month averages only).
//  - Daily returns: close / previous close − 1 between consecutive trading days, dated by the later day. Weekday
//    averages and σ(period) use the daily returns whose date falls in the period (all used years, current year
//    included); σ(global) uses every daily return of the used years. Volatility ratio = σ(period) / σ(global).
//  - pathReturn = change of the raw average path over the period, compounded: (1 + p(b)) / (1 + p(a − 1)) − 1.
//  - maxDrawdown = largest peak-to-trough drop of the raw average path inside the period (as wealth 1 + p).
//  - compounded = Π(1 + r_y) − 1 over counted years (holding the period every year); returnRisk = mean / std.
// All returns are in percent.
namespace seasonality {

struct DailyClose { QDate date; double close = NAN; };

constexpr int kSlots = 366;
int slotOf(const QDate& d);                    // 0..365
int slotOf(int month, int day);
QDate dateOfSlot(int slot, int year);          // Feb 29 of a common year gives Feb 28
QString slotLabel(int slot);                   // "Sep 13"
int monthFirstSlot(int month);                 // month 1..12
int monthLastSlot(int month);
int monthOfSlot(int slot);
QString monthShort(int month);                 // "Jan" / "janv." (interface language)
QString monthAxis(int month);                  // "JAN" / "JANV" (upper case, no abbreviation dot)
QString weekdayAxis(int dayOfWeek);           // 1 = Monday: "MON" / "LUN"
QString dateLabel(const QDate& d);             // "Oct 23 2006" / "23 oct. 2006"
QString monthLong(int month);                  // "January"

struct Period {
  enum Kind { Full, H1, H2, Q1, Q2, Q3, Q4, Custom };
  Kind kind = Full;
  int a = 0, b = kSlots - 1;                   // inclusive slots
  static Period preset(Kind k);
  static Period custom(int a, int b);          // ordered and clamped; a whole year gives Full
  bool isFull() const { return a == 0 && b == kSlots - 1; }
  QList<int> months() const;                   // months overlapping the period, in order
  QString pill() const;                        // "Full year", "Jan-Jun", "Mar 5-Jun 20"
  QString rangeLabel() const;                  // "Jan 1 → Jun 30" (with a real arrow)
  bool operator==(const Period& o) const { return a == o.a && b == o.b; }
};

struct YearData {
  int year = 0;
  double base = NAN;                           // close before the year (or its first close)
  QVector<double> close;                       // kSlots closes, carried forward; base before the first close
  int reach = kSlots - 1;                      // last slot covered (current year: slot of its last close)
  bool finished = true;                        // the year is over in the data
};

struct DayReturn { QDate date; int slot; int year; double r; };   // r in %

struct Dataset {
  QString symbol;
  int yearsRequested = 0;
  QDate today, first, last;                    // first: base date of the oldest year; last: last close
  QVector<YearData> years;                     // ascending
  QVector<DayReturn> daily;
  bool ok() const { return !years.isEmpty(); }
  int index(int year) const;                   // into years, -1 if absent
};

Dataset build(const QVector<DailyClose>& closes, int years, const QDate& today, const QString& symbol = {});

QVector<double> averagePath(const Dataset& ds);                // kSlots values in %, finished years only, NaN where none
QVector<double> smoothPath(const QVector<double>& path, double sigmaDays = 1.0, int halfWidth = 1);
QVector<double> yearPath(const Dataset& ds, int year);          // NaN after the year's reach
double pathChange(const QVector<double>& path, int a, int b);   // compounded % change over [a, b]

// Return of one year over [a, b] in %, NaN when not covered. counted = covers the whole period.
double periodReturn(const YearData& y, int a, int b, bool* counted = nullptr);

struct MonthStat { int month = 0; double mean = NAN; int n = 0; };
QVector<MonthStat> monthlyAverages(const Dataset& ds, bool excludeOutliers = false);   // 12 months

struct YearReturn { int year = 0; double ret = NAN; bool counted = false; };

struct Stats {
  Period period;
  int yearsCounted = 0, positive = 0, negative = 0;
  double winRate = NAN;                        // share of counted years with a positive period return (0..1)
  double pathReturn = NAN;                     // average path change over the period
  double meanReturn = NAN, medianReturn = NAN, stdReturn = NAN, compounded = NAN, returnRisk = NAN;
  MonthStat best, worst;                       // best / worst average month inside the period
  QVector<MonthStat> months;                   // months of the period (outlier filter applied)
  double weekday[5] = {NAN, NAN, NAN, NAN, NAN};   // Mon..Fri mean daily return %
  int weekdayN[5] = {0, 0, 0, 0, 0};
  QVector<YearReturn> perYear;                 // every used year (current year may be uncounted)
  double sigmaPeriod = NAN, sigmaGlobal = NAN, volRatio = NAN;
  double maxDrawdown = NAN;
  YearReturn bestYear, worstYear;
};
Stats compute(const Dataset& ds, const QVector<double>& rawPath, const Period& p, bool excludeOutliers);

// Volatility-ratio state: -1 calmer than normal (< 0.95), 0 normal, +1 more volatile (> 1.05).
int volState(double ratio);

// Daily closes of `symbol` (market::instruments() id) for the last `years` calendar years plus the month before
// (base of the first year), from Yahoo Finance daily bars (explicit period1/period2: range=max would thin to
// monthly bars). Dates are exchange-local session days (market::sessionDay: the bar's date in the exchange time
// zone; Yahoo stamps a finished daily bar at the session open or local midnight and today's open bar with the time
// of its last trade). Callback on the GUI thread; `error` empty when ok; `stale` when served from an old cache.
using HistoryCallback = std::function<void(const QVector<DailyClose>& closes, const QString& error, bool stale)>;
void loadHistory(const QString& symbol, int years, QObject* ctx, HistoryCallback cb);
}
