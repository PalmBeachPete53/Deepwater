#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/imports.js. Your own data files, kept in <data>/charts/imports/ on this computer:
//   - Bars: an OHLCV CSV (1-minute bars exported from Alpaca, a broker, anywhere). It charts as
//     CSV:<NAME> on its own timeframe and any longer one (built from its bars), and backtests like
//     any other symbol.
//   - Signals: a CSV of time and position (+1 long, 0 flat, -1 short). It becomes an indicator
//     (<data>/charts/custom/signal-<name>.js), so the strategy tester and the quant checks run on your
//     strategy exactly as your own code decided it.
// Everything here is synchronous (but listImportsReady) and touches only local files (no network). Thread-safe (the
// parsed-file cache is guarded, and no parse holds its lock), so a provider calls csvCandles from a worker thread.
// Files that can't be read are cached with their error until they change.
#include "charts/core/Types.h"
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QPair>
#include <stdexcept>

namespace charts::data {

// The JS `bad(message, status)`: an Error carrying the HTTP status the local API answers with.
struct ImportError : std::runtime_error {
  int status;
  QString message;
  explicit ImportError(const QString& msg, int st = 400)
      : std::runtime_error(msg.toStdString()), status(st), message(msg) {}
};

QString importsDir();   // paths::data("charts/imports")  (JS IMPORTS_DIR)
QString customDir();    // paths::data("charts/custom")   (JS CUSTOM)

// "btc 1m (Alpaca).csv" -> "BTC_1M_ALPACA": safe as a file name and as a ticker. Throws ImportError(400).
QString cleanName(const QString& raw);

// ---------- CSV ----------
// A row: its cells. A missing cell (row shorter than the header) reads as JS `undefined`.
using Row = QStringList;
QVector<Row> rowsOf(const QString& text);                // throws "The file is empty"

// Column indexes found from a header row (-1 = not found). timeOfDay: MetaTrader's separate time column.
struct Columns {
  int time = -1, open = -1, high = -1, low = -1, close = -1, volume = -1, position = -1, symbol = -1, timeOfDay = -1;
};
Columns columns(const Row& header);

// Seconds since 1970 (UTC), NaN when unreadable. Unix seconds, milliseconds, microseconds or nanoseconds;
// ISO 8601 with or without a zone (no zone means UTC); dates with -, / or . between year, month and day.
// timeOfDay empty = not given (as the JS falsy check).
double parseTime(const QString& cell, const QString& timeOfDay = {});

QString tfLabel(qint64 sec);   // 60 -> "1m", 86400 -> "1D", 2592000 -> "1M"

// Bars from CSV text. Throws ImportError(400) with the row number and what to change when a row can't be read.
struct ParsedBars {
  Bars bars;
  qint64 baseSeconds = 0;   // the file's own timeframe: the most common gap between bars
  int skipped = 0;
};
ParsedBars parseBars(const QString& text);

// ---------- bars: store, list, chart ----------
// One imported file as /api/imports lists it. error non-empty: the file can't be read (then only name,
// symbol and error are in toJson(), as the JS).
struct ImportSummary {
  QString name, symbol, tf, error;
  int bars = 0, skipped = 0;
  qint64 from = 0, to = 0;
  QJsonObject toJson() const;   // {name, symbol, bars, tf, from, to, skipped} or {name, symbol, error}
};
ImportSummary importBars(const QString& rawName, const QString& text);   // writes imports/<NAME>.csv; throws
QVector<ImportSummary> listImports();
QJsonArray listImportsJson();
// The imports already read (current version, readable), without reading any file here: those not read yet are read
// on the thread pool and listed by a later call. For the symbol search, which lists them at every keystroke on the
// GUI thread (audit P8: a 100 MB file took seconds at the first search, a broken one at every keystroke).
QVector<ImportSummary> listImportsReady();

// Candles of an imported file for a timeframe at or above its own; the last `limit` bars
// (limit <= 0 behaves as JS slice(-limit): 0 = all). Throws ImportError: 404 no such file, 400 tf shorter than
// the file's bars; a malformed tf throws ImportError(500: the JS Error has no status) with series' "Bad timeframe ..." message.
// name = the part after "CSV:" (already clean). Providers call this for symbols "CSV:NAME".
struct CsvCandles {
  QString symbol;     // "CSV:NAME"
  QString tf;
  QString provider;   // "Your file NAME.csv"
  Bars candles;
  QStringList notes;
  QJsonObject toJson() const;   // {symbol, tf, provider, candles:[{time,open,high,low,close,volume}], notes}
};
CsvCandles csvCandles(const QString& name, const QString& tf, int limit = 0);
QJsonArray barsToJson(const Bars& bars);

// ---------- signals ----------
struct Signals {
  int rows = 0;
  QVector<QPair<qint64, double>> changes;   // [time, position], only the changes (the position holds until the next)
  qint64 from = 0, to = 0;
  QJsonObject toJson() const;               // {rows, changes:[[t,p],...], from, to}
};
Signals parseSignals(const QString& text);   // throws ImportError(400)

// The indicator file a signal becomes (JS source text, an ES module like the JS custom indicators).
// Each bar takes the last position set before the next bar starts, so on a longer timeframe it's the position
// at the bar's close. fileName empty = name.
QString signalIndicatorSource(const QString& name, const Signals& s, const QString& fileName = {});

// What server.js /api/import/signal does with the file, split so the caller can load and validate it between:
// writeSignalIndicator writes custom/signal-<name lowercased>.js and remembers the previous content;
// if loading it fails the caller calls restoreSignalIndicator (deletes the new file or puts the old one back).
struct SignalFile {
  QString file;        // "signal-<name>.js"
  QString full;        // absolute path
  QString id;          // "custom:signal-<name>"
  bool existed = false;
  QByteArray before;   // previous content when existed
};
SignalFile writeSignalIndicator(const QString& name, const Signals& s, const QString& fileName);
void restoreSignalIndicator(const SignalFile& f);
// The /api/import/signal answer: {ok, name, id, file, rows, changes, from, to, strategy}.
QJsonObject signalImportResult(const QString& name, const SignalFile& f, const Signals& s);

// The tester rules that trade a signal exactly: long while it's above 0, short while it's below.
QJsonObject signalStrategy(const QString& id);

// JS helpers used by the port (exposed for the other data modules and tests).
double jsNumber(const QString& s);               // Number(s) on a string: "" -> 0, hex, Infinity, else NaN
void clearCache();                               // drop parsed files (tests)
int cachedResampledForTests(const QString& name);   // resampled copies kept for a file (-1: not in the cache)

}  // namespace charts::data
