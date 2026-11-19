#pragma once
// Pine Script® in Deepwater: runs a Pine Script v5/v6 source on Deepwater's bars with PineTS (third_party/pinets,
// AGPL-3.0, see its SOURCES.md) inside QuickJS-ng (third_party/quickjs-ng), as TradingView runs it: bar by bar, with
// the series history ([1]), na, var/varip, input.*, plot*, hline, bgcolor, the ta.* / math.* / str.* / array.* /
// matrix.* / map.* libraries and the drawings (label, line, box, table, polyline).
//
// The user's rule (2026-11-18): Pine is the definition, the C++ engine executes. The built-in indicators stay computed
// by charts::ind (Indicators.h); tests compare them with their Pine definitions run here. Pine scripts the user adds
// run here, for what Charts does not have.
//
// SANDBOX: every run gets a fresh QuickJS runtime (about 50 ms to load PineTS), so nothing is shared between scripts
// or runs. A script reaches no network (no fetch, no WebSocket), no files and no other process: QuickJS's core has no
// such API and none is added. Its bars are the ones given here. Limits per run: Options::timeoutMs (the interrupt
// handler stops the JS), Options::memoryMB (QuickJS's allocator refuses more), and the JS stack left on the calling
// thread (charts/core/JsStack.h: run() on a thread with a big stack, as runAsync() does, for deep scripts).
//
// WHAT QUICKJS LACKS, SUPPLIED HERE: Intl.DateTimeFormat (PineTS's only Intl use: a time zone's wall clock and whether
// summer time applies) backed by QTimeZone; setTimeout (queued, run when the promise jobs are done, no delay);
// console.* (collected in Result::log).
//
// THREADS: run() works on any thread (its runtime lives and dies inside the call); runs on different threads are
// independent. The threads of runAsync / runTracked are counted and stopped at the app's quit (stopRunsForQuit).
#include "charts/core/Types.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace charts::pine {

struct Options {
  QString symbol = QStringLiteral("DEEPWATER:SYMBOL");   // syminfo.tickerid, e.g. "BINANCE:BTCUSDT"
  // Pine timeframe ("1", "5", "60", "240", "D", "W", "M", "1S"…); empty = read from the bars' spacing (timeframeOf)
  QString timeframe;
  QString timezone = QStringLiteral("UTC");   // the chart's time zone (IANA id) for time functions and sessions
  QJsonObject inputs;                          // input title → value, over the script's defaults
  QJsonObject props;                           // strategy() / indicator() arguments replaced, e.g. commission_value
  int timeoutMs = 60000;
  int memoryMB = 1024;
  // Set to true to stop the run (the interrupt handler reads it, as it reads the time limit): a superseded request
  // stops instead of running to its end. The app's quit stops every run the same way (stopRunsForQuit).
  std::shared_ptr<const std::atomic<bool>> cancel;
};

struct Plot {
  QString key;            // PineTS's plot key: the title, or "bgcolor#0", "plotshape#1"… for untitled calls
  QString title;
  QJsonObject options;    // the call's options: style ("line", "histogram", "shape", "hline", "background", …),
                          // color, linewidth, shape, location, overlay…
  Series values;          // one per bar, aligned with the bars given; NaN = na; true/false = 1/0
  QStringList colors;     // per bar colour when the script changes it bar by bar ("" = the plot's colour); else empty
  // plotshape / plotchar / plotarrow: the bars where it shows, with that bar's options:
  // [{ i, value, shape, location, color, text, char }]
  QJsonArray points;
};

struct Result {
  QString error;          // empty = the script ran; else the PineTS / JS error ("line 4: …" when PineTS knows it)
  QVector<Plot> plots;
  QJsonObject indicator;  // indicator() / strategy() arguments: title, shorttitle, overlay, precision, format…
  QJsonObject drawings;   // __labels__, __lines__, __boxes__, __linefills__, __polylines__, __tables__ → their items
  // strategy() scripts: PineTS's broker emulator (TradingView's fill rules) — { config, closedtrades, opentrades,
  // initial_capital, equity, netprofit, openprofit, grossprofit, grossloss, max_drawdown, wintrades, losstrades,
  // eventrades, position_size, account_currency }; a trade: { entry_id, entry_comment, entry_price,
  // entry_bar_index, entry_time (ms), size (< 0 short), commission, entry_commission, exit_price, exit_bar_index,
  // exit_time, exit_id, exit_comment, profit (net of commission), status }. Empty for an indicator().
  QJsonObject strategy;
  QStringList log;        // console.* and log.* lines, at most 1000
  double ms = 0;          // time of the run itself (without loading PineTS)
  bool stopped = false;   // a limit (time, memory), Options::cancel or the app's quit stopped it (error says which)
  bool ok() const { return error.isEmpty(); }
  const Plot* plot(const QString& key) const;
};

// What a source declares, read without running it (PineTS's static scan): "indicator", "strategy" or "library", and
// its input.* calls in order: [{ id, name, title, type (int float bool string source color timeframe time symbol
// session price text_area), defval, minval, maxval, step, options, tooltip, group, inline, display }].
struct Script {
  QString error;
  QString declaration;
  QJsonArray inputs;
  bool ok() const { return error.isEmpty(); }
};
Script describe(const QString& source);

// Runs a source on bars (oldest first, Unix seconds, as Types.h). Any thread.
Result run(const QString& source, const Bars& bars, const Options& options = {});

// The same on a new thread with a 16 MB stack; done(result) is called on ctx's thread (dropped if ctx is gone).
void runAsync(const QString& source, const Bars& bars, const Options& options, QObject* ctx, std::function<void(const Result&)> done);
// Any Pine work on such a thread (runAsync uses it; so do the script loads of PineIndicators): counted, so the app's
// quit can wait for it. done() on ctx's thread (none if ctx is null or gone).
void runTracked(std::function<void()> work, QObject* ctx, std::function<void()> done);
// At quit (connected to QCoreApplication::aboutToQuit by the first runTracked): stops every run (the interrupt handler
// sees it within milliseconds of JS) and waits up to waitMs for the threads of runTracked to end. False if some are
// still going (a native step longer than that). Audit G7. Tests: runningTracked(), resetQuitForTests() after a stop.
bool stopRunsForQuit(int waitMs);
int runningTracked();
void resetQuitForTests();

// The Pine timeframe of bars from their spacing (the smallest gap between two bars): "1S"…, "1"…"720", "D", "W",
// "M" ("" for fewer than 2 bars). A Charts timeframe ("1m", "4h", "1D", "1W", "1M"…) to the Pine one ("" if unknown).
QString timeframeOf(const Bars& bars);
QString timeframeFromCharts(const QString& tf);

QString pinetsVersion();   // the PineTS release embedded ("0.13.1")

// The wall clock of a time zone at a UTC instant, what the Intl shim gives PineTS. weekday 0 = Sunday. dst: the
// offset is above the lower of the year's January and July offsets (summer time, whatever the zone calls it, so
// Europe/Dublin's "Irish Standard Time" in summer counts as in Intl's names). nullopt for an unknown zone.
struct WallClock { int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, weekday = 0; bool dst = false; };
std::optional<WallClock> wallClock(const QString& timezone, qint64 msUtc);

}  // namespace charts::pine
