#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// AI › ML data sets: Deepwater's data written as CSV tables (one row per date, oldest first, a "date" column), or a CSV
// file of the user's, copied in. Saved under <data>/ml/datasets/<id>.csv; <id>.meta.json holds its title, source and
// description (columns, rows, period), so listing the data sets does not parse every CSV. The folder is kept under
// DATASETS_MAX_BYTES: past it the oldest data sets are deleted after a new one is written (never the one in training).
//
//  Prices  one market (app symbol), daily / weekly / hourly bars from Yahoo, with ready-made features computed from past
//          bars only (returns, volatility, RSI, distance to moving averages, range).
//  Macro   one economy's monthly table (policy rate, inflation, unemployment, growth, confidence, yields…); quarterly
//          values carried forward over their months. Values are dated by reference period, not by release date.
//  COT     one CFTC market's weekly nets per trader category, open interest, and the continuous future's close.
//  Csv     any CSV with a header row; a date column is found by name (date, time, datetime, timestamp).
//  Market  Data › Technical data (technical-data.sqlite): one market and timeframe as stored, the newest `rows` closed bars (200 000 by
//          default, 1 000 000 at most): OHLC, volumes, trades, taker-buy share, the gap before each bar (bars the
//          exchange lacks: flagged, never filled in), the 23 indicators of Charts' engine as stored, and their distance
//          to the close (indicator levels follow the price, which rose 15-fold since 2017). Warm-up bars left out unless
//          asked.
namespace ml {

enum class SourceKind { Prices, Macro, Cot, Csv, TechnicalData };

struct DatasetSpec {
  SourceKind kind = SourceKind::Prices;
  QString key;         // Prices: symbol; Macro: iso2; Cot: market id; Csv: file path; Market: "BINANCE:BTCUSDT"
  QString interval;    // Prices: "1d" "1wk" "1h"; Market: "1m" "5m" "15m" "1h" "4h" "1D" "1W"
  int rows = 0;        // Market: newest bars to take (0 = 200 000; at most 1 000 000)
  bool keepWarm = false;   // Market: keep the warm-up bars (long averages not settled)
  QString id() const;  // file name without .csv
};
constexpr qint64 DATASETS_MAX_BYTES = 5LL << 30;
constexpr int TECHNICAL_DATA_ROWS_DEFAULT = 200000, TECHNICAL_DATA_ROWS_MAX = 1000000;
// The page's choices of rows (50 000, 200 000, 500 000, 1 000 000): any other number goes up to the next one, so a model
// asking for every row count in turn does not write one large file per count. 0 = the default.
int snapTechnicalDataRows(int rows);

// Columns that are price or volume levels: left out of the default features (a model that sees the level mostly learns
// the period). Shared by the ML page and the cubes' ML tools.
const QStringList& levelColumns();
bool isLevelColumn(const QString& column);   // levelColumns(), or a Data › Technical data pair's column that follows the scale

struct Dataset {
  QString path, title, source, error;
  QStringList columns;         // header, "date" first when present
  QStringList numeric;         // columns that hold numbers
  QString dateColumn;
  int rows = 0;
  QString first, last;         // dates of the first and last rows
  QStringList pruned;          // build() only: titles of the old data sets it deleted to stay under DATASETS_MAX_BYTES
  bool ok() const { return error.isEmpty() && rows > 0; }
};

using DatasetCallback = std::function<void(const Dataset&)>;
void build(const DatasetSpec& spec, QObject* ctx, DatasetCallback cb);   // fetches if needed, writes the CSV, describes it
Dataset describe(const QString& csvPath, const QString& title = {}, const QString& source = {});
QList<Dataset> listDatasets();          // every data set on disk, newest first (title and source from <id>.meta.json)
Dataset datasetById(const QString& id); // error set when there is none
QString idOf(const Dataset& d);         // file name without .csv
QString datasetsDir();
QStringList csvCells(const QString& line);   // one CSV line's cells, quotes handled, trimmed (as describe() reads them)
// The last n non-blank lines of a CSV file, its header never among them, read from the end (a million-row file is not
// read whole for its last 20 rows). Empty when it cannot be read.
QStringList lastLines(const QString& csvPath, int n);
// The data sets on disk (the CSV files), without reading them
int datasetCount();
}  // namespace ml
