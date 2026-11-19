#pragma once
// What Data › Technical data records for one pair (user's design, 2026-11-18): which timeframes, from which day, and
// which indicators. Kept as JSON in technical-data.sqlite's pairs table (TechnicalDataStore.h), one row a pair:
//   {"timeframes": [60, 300, …], "start": 0, "indicators": [{"id": "rsi", "params": {"length": 14}}, …]}
//   - start: Unix seconds UTC of the first bar wanted, 0 = the pair's first day on the exchange.
//   - indicators: any Charts indicator (charts::ind, the one engine) with its settings; each gives one column per
//     output kept ("outputs", all when absent). "running": true marks a running sum (OBV) carried over from the bar
//     before a piece; "tfs" lists the timeframes it is stored on when not all (its window is longer than what is
//     recomputed on the others).
// Column names: the indicator, its first number, its other settings when not the default, then the output unless
// it is "value": rsi7, rsi7_ma, ema100_hl2, bb20_mult2p5_upper. The same settings on two pairs: the same column.
// The first pair, BINANCE:BTCUSDT, gets defaultSpec(): the seven timeframes, the whole history and the 23 columns
// it had before pairs could be chosen, with their names and version, so nothing is computed again.
//
// An indicator is only accepted when checkIndicator() finds, by computing it, that it can be stored as a series:
//   - it never looks ahead: computed on fewer bars, its earlier values stay the same (a zigzag, or Ichimoku's lagging
//     span, changes them once later bars come). Checked output by output: the others of an indicator can stay;
//   - computed from OVERLAP bars before a piece it gives the values of a computation on the whole series, on each
//     timeframe (or the same values plus a constant: a running sum, carried over).
#include "technicaldata/TechnicalDataIndicators.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

namespace technicaldata {

struct IndSpec {
  QString id;            // Charts indicator, e.g. "rsi"
  QJsonObject params;    // its settings (missing ones: the indicator's defaults)
  bool running = false;  // a running sum from the first bar (OBV): carried over between pieces
  QVector<int> tfs;      // stored on these timeframes only; empty = every timeframe of the pair
  QStringList outputs;   // the outputs stored (keys, e.g. "value", "ma"); empty = all of them
};

struct PairSpec {
  QVector<int> timeframes;     // seconds, in timeframes() order
  qint64 start = 0;            // Unix seconds UTC; 0 = from the exchange's first bar
  QVector<IndSpec> indicators;
  bool paused = false;         // collection paused (Store::setPaused; kept as the store has it when the spec is saved)
  bool valid() const { return !timeframes.isEmpty(); }
};

PairSpec defaultSpec();
// a forex pair (Dukascopy.h, user's choice 2026-11-18): 5m to 1W from Dukascopy's first day, the same 23 columns
PairSpec defaultForexSpec();
// forex: 4h, 1D and 1W are made from the stored hours, so 1h comes with any of them
PairSpec withForexHours(PairSpec s);
PairSpec specFromJson(const QJsonObject& o);
QJsonObject specToJson(const PairSpec& s);
QJsonObject indToJson(const IndSpec& i);
IndSpec indFromJson(const QJsonObject& o);

// the indicator's settings with its defaults filled in (what it is computed with)
QJsonObject fullParams(const QString& id, const QJsonObject& params);
// the columns of one indicator / of a pair, named as the table stores them ("rsi14", "ema100", "bb_20_2p5_upper")
QVector<IndColumn> columnsOf(const IndSpec& i);
QVector<IndColumn> columnsOf(const PairSpec& s);
QStringList columnNames(const PairSpec& s);
QString versionOf(const PairSpec& s);   // the indicators' definitions: another version = computed again
int warmupOf(const PairSpec& s);        // bars: 3 × the longest whole-number setting, at least WARMUP
QString describe(const IndSpec& i);     // "RSI · length 14 · close", for lists

// The check of an indicator before it is stored, on generated bars (deterministic, nothing leaves the machine).
struct IndCheck {
  bool ok = false;
  bool running = false;
  QVector<int> tfs;      // timeframes it can be stored on (of those asked)
  QStringList outputs;   // the outputs that can be stored
  QStringList ahead;     // the outputs refused because they look ahead
  QString reason;        // why not (translated), or what is left out when ok
};
IndCheck checkIndicator(const QString& id, const QJsonObject& params, const QVector<int>& timeframes);
bool storable(const QString& id);   // offered at all: a built-in Charts indicator with lines and no anchor time

// How a column's values follow the market's scale, found by computing it on bars whose prices are ten times larger
// and on bars whose volumes are: Free (RSI, %B: the same), Price (an average, the ATR: ten times larger), Other
// (the OBV: volume; anything else). Machine learning leaves levels out by default, and relates Price ones to the close.
enum class Scale { Free, Price, Other };
Scale scaleOf(const IndColumn& c);
bool isOverlay(const QString& id);   // drawn on the price (an average, bands): a level to compare with the close

// Size on disk, from technical-data.sqlite as measured (2026-11-18, BTCUSDT: 6.26 M bars in 596 MB, the indicator
// rows of 23 columns in 1.6 GB): about 95 bytes a bar, 26 an indicator row, 10 a value.
qint64 estimateBars(int tfSeconds, qint64 from, qint64 to);
qint64 estimateBytes(qint64 bars, int columns);

}  // namespace technicaldata
