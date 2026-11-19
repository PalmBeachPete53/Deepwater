#pragma once
// Pine Script files (.pine in the custom folder) as Charts indicators. A file is checked when it loads: PineTS reads
// its input.* calls without running it (pine::describe), then runs it once on 400 sample bars to learn its plots.
// It is then registered as an ind::Def "pine:<name>" like any other indicator:
//   inputs   input.int / float / price → int / float; bool → select true|false; string with options → select;
//            source → source (the first one under the key "source", so it can take another indicator's output and
//            the chart's other timeframes like the built-ins); time → time. Colour, timeframe, symbol, session and
//            free text inputs keep the script's default (not editable yet).
//   outputs  plot() lines (step, circles, cross, area… drawn as lines; circles and cross as dots), histograms and
//            columns as histograms, with the script's colours, per bar when it changes them
//   levels   hline()
//   markers  plotshape / plotchar / plotarrow
//   bar colours  barcolor()
// Not drawn yet: bgcolor, fill, and the drawings (label, line, box, table, polyline).
//
// COMPUTING: the Def's calc goes through Cache. While a chart draws (inside a ChartScope, on the GUI thread) calc
// never waits, as a run takes from 0.1 s to a few seconds: it returns the finished result for those bars and params
// if there is one; otherwise it starts the run
// on a worker thread (pine::runAsync) and returns the last finished result of the same script, params and series
// (padded to the new bars), or a pending result ("loading…"). When a run ends, Cache::ready(id) is emitted and the
// charts draw again. One run at a time per script and params (a newer request waits for it, then runs on the newest
// bars), two scripts at once at most. Anywhere else (the strategy tester, alerts, the screener, a worker thread),
// calc runs the script itself and waits; on the GUI thread its result is kept for the chart, and the run has short
// limits (5 s, 256 MB: the window waits for it); a script these limits stopped answers its error at once on the GUI
// thread for the next 5 minutes instead of running again (audit P1).
#include "charts/core/Indicators.h"
#include "charts/core/Pine.h"
#include <QHash>
#include <QObject>
#include <QSet>
#include <memory>

namespace charts::pine {

// "pine:<file name without .pine>"
QString idFor(const QString& fileName);

struct Loaded {
  QString file, id;
  QString error;   // empty = registered
  bool ok() const { return error.isEmpty(); }
};

// Reads, checks and registers a .pine file (replacing an earlier version of it). GUI thread; it runs the script (up
// to 20 s for a slow one), so the start-up load uses loadFilesAsync.
Loaded loadFile(const QString& path);
// The same for several files, the checking runs on a Pine thread (pine::runTracked) one after the other, the
// registration on ctx's thread, then done(results in the order of paths). Nothing is called if ctx is gone. Audit P1:
// a .pine file with an endless loop froze every launch for 20 s.
void loadFilesAsync(const QStringList& paths, QObject* ctx, std::function<void(const QVector<Loaded>&)> done);
// The problems of a source without registering anything ("" = it would load).
QString check(const QString& source);

// The chart the indicators are being computed for, set by the chart around its indicator computing: Pine's
// syminfo.tickerid ("BINANCE:BTCUSDT"), and the right to answer "pending" (see COMPUTING). Thread-local.
class ChartScope {
public:
  explicit ChartScope(const QString& symbol);
  ~ChartScope();
  ChartScope(const ChartScope&) = delete;
  ChartScope& operator=(const ChartScope&) = delete;
private:
  QString previous_;
};
QString currentSymbol();

// What a loaded script needs to run and to turn PineTS's plots into an indicator result.
struct Info {
  QString id, file, source;
  QString sourceKey;       // a hash of source, part of the cache key (a reloaded script never gets the old results)
  struct In { QString key, title, type; };
  QVector<In> inputs;
  QSet<QString> lines;     // plot keys drawn as outputs
  QHash<QString, QString> outKey;   // plot key → output key (Charts keys are identifiers: "Fast EMA" → "Fast_EMA")
  QSet<QString> markers;   // plotshape / plotchar / plotarrow keys
  QString barcolor;        // the barcolor() key, if any
};

// PineTS's result on these bars → an indicator result (outputs, per bar colours, markers, bar colours, error).
ind::Result toIndicator(const Info& info, const Result& run, const Bars& bars);
// Params of the Charts settings → the script's inputs by title
QJsonObject inputsOf(const Info& info, const Params& params);

class Cache : public QObject {
  Q_OBJECT
public:
  static Cache& instance();
  ind::Result get(const std::shared_ptr<const Info>& info, const Bars& bars, const Params& params);
  void forget(const QString& id);   // a script changed or was deleted: its results go
  int running() const { return active_; }
  static void setGuiLimitsForTests(int timeoutMs, int memoryMB);   // 0 = defaults (5000 ms, 256 MB)
  int keptForTests() const { return int(done_.size()); }

signals:
  void ready(const QString& id);

private:
  Cache() = default;
  struct Done { quint64 barsKey = 0; qint64 firstTime = 0; int n = 0; ind::Result result; quint64 used = 0; };
  struct Want { std::shared_ptr<const Info> info; Bars bars; Params params; QString symbol; quint64 barsKey = 0; };
  void start(const QString& key, const Want& w);
  void startQueued();
  void keep(const QString& key, const Done& d);   // stores a result, the least recently used beyond MAX_DONE go
  QHash<QString, Done> done_;        // id|symbol|params → the last finished run
  QHash<QString, quint64> running_;  // id|source hash|symbol|params → bars key of the run under way
  QHash<QString, Want> waiting_;     // the newest request made while a run of that key was under way, or queued
  QStringList queue_;                // keys waiting for a free slot
  QHash<QString, QPair<qint64, QString>> slowUntil_;   // id|source hash → until when (ms) the GUI path answers the error
  int active_ = 0;
  quint64 clock_ = 0;
};

}  // namespace charts::pine
