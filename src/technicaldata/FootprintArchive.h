#pragma once
// Data › Technical data, footprint history (user's request 2026-11-20): years of 1-minute footprint (buyer / seller
// volume per price level) for machine learning and the agents, built from Binance's public trade archives
// (data.binance.vision, one zip of CSV per day and market), never from the REST API (1000 trades a call: weeks).
// Pure: no network, no database (ArchiveCollector downloads, TechnicalDataStore keeps). Tests:
// tests/technicaldata/footprint_archive_tests.cpp.
//
// User's choices (2026-11-20):
//   - price levels are fractions of the price, not dollars: BTC went from 3 000 to 100 000+, and a $10 level means
//     something else in 2019 and 2025. Level k covers [g^k, g^(k+1)) with g = 1.0001 (1 basis point), the same grid
//     for every minute, day and year, so minutes add up level by level (a 5-minute or hourly footprint is exact);
//   - both markets: spot and the USD-M perpetual, to see which one informs best.
//
// A minute (Minute): open / high / low / close of its trades, aggressive buy and sell volume (exact sums, doubles),
// aggregate trades, the first and last aggregate trade ids, and its levels from the lowest traded one to the highest
// (untraded ones inside are 0 × 0): bid = volume of aggressive SELLS there (they hit the bid), ask = aggressive BUYS
// (they lifted the ask), as the live footprint (charts/core/Footprint.h) defines them. Level volumes are floats
// (7 digits: 0.00003 BTC on 500); the minute's totals stay exact.
//
// The archives (checked 2026-11-20 on 2026-11-18): spot "id,price,qty,first,last,time,isBuyerMaker,isBestMatch", no
// header, time in microseconds since 2025 (milliseconds before), True / False; USD-M a header line, time in
// milliseconds, true / false. Every file is one CSV in a zip (deflate), with a .CHECKSUM (sha256) beside it.
#include "charts/core/Footprint.h"
#include "charts/core/Ticks.h"
#include <QByteArray>
#include <QByteArrayView>
#include <QDate>
#include <QString>
#include <QUrl>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace technicaldata::fp {

enum class Venue { Spot, Perp };
QString venueId(Venue v);                          // "spot", "perp"
std::optional<Venue> venueFromId(const QString& id);
const std::vector<Venue>& venues();                // spot, perp
// The day's archive and its checksum: data/spot/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2026-11-18.zip,
// data/futures/um/daily/aggTrades/…
QUrl dailyUrl(Venue v, const QString& symbol, const QDate& day);
QUrl checksumUrl(Venue v, const QString& symbol, const QDate& day);
// The first day each market has an archive (BTCUSDT: spot 2017-08-17, USD-M 2019-12-31).
QDate firstDay(Venue v, const QString& symbol);

// ---- the grid: 1 basis point, geometric
constexpr double kGrowth = 1.0001;
int32_t levelOf(double price);                     // floor(ln p / ln g); price > 0
double levelLow(int32_t k);                        // g^k, the level's lowest price
double levelMid(int32_t k);                        // the level's middle price

struct Level { float bid = 0, ask = 0; };

struct Minute {
  qint64 t = 0;                    // open time, Unix seconds UTC (a multiple of 60)
  double open = 0, high = 0, low = 0, close = 0;
  double buy = 0, sell = 0;        // aggressive buy (ask) / sell (bid) volume, base asset
  quint32 trades = 0;              // aggregate trades
  qint64 firstId = 0, lastId = 0;  // aggregate trade ids
  qint32 base = 0;                 // level of levels[0]
  std::vector<Level> levels;       // dense, ascending price
  double volume() const { return buy + sell; }
  double delta() const { return buy - sell; }
};

// Adds b to a (same minute or not: the later one's close, the earlier one's open): a longer bar, level by level.
void merge(Minute& a, const Minute& b);

// ---- reading an archive
struct AggTrade {
  qint64 id = 0;
  double price = 0, qty = 0;
  qint64 timeMs = 0;
  bool buyerMaker = false;         // true: the seller was the aggressor
};

// The CSV of either market, fed in pieces of any size (a line may span two). Header and blank lines are skipped; a
// line that can't be read counts in bad() and is skipped.
class CsvReader {
public:
  using Sink = std::function<void(const AggTrade&)>;
  explicit CsvReader(Sink sink) : sink_(std::move(sink)) {}
  void feed(const char* p, size_t n);
  void finish();                   // the last line, when the file does not end with a newline
  qint64 lines() const { return lines_; }
  qint64 bad() const { return bad_; }
private:
  void line(const char* b, const char* e);
  Sink sink_;
  std::string carry_;
  qint64 lines_ = 0, bad_ = 0;
};

// The one file of a zip (Binance's archives), inflated as it is read: out() gets the bytes in pieces. Checks the
// entry's CRC-32 (from its header, or the data descriptor after it). False and an error text when the zip can't be
// read.
bool inflateZip(QByteArrayView zip, const std::function<void(const char*, size_t)>& out, QString* error);

// The minutes of one UTC day, from its trades in file order (ascending ids).
class DayBuilder {
public:
  explicit DayBuilder(qint64 dayStart) : day_(dayStart) {}
  void add(const AggTrade& t);
  std::vector<Minute> finish();    // ascending time; only minutes with trades
  qint64 trades() const { return trades_; }
  qint64 outside() const { return outside_; }   // trades dated outside the day (none expected)
  qint64 idGaps() const { return idGaps_; }     // aggregate ids missing between two trades (none expected)
private:
  void flushLevels(Minute& m);
  qint64 day_;
  std::vector<Minute> minutes_;
  struct Acc { int32_t k; double bid, ask; };
  std::vector<Acc> cur_;           // levels of the minute being built, unsorted, summed in double
  qint64 trades_ = 0, outside_ = 0, idGaps_ = 0, lastId_ = -1;
};

struct DayResult {
  std::vector<Minute> minutes;
  qint64 trades = 0, bad = 0, outside = 0, idGaps = 0;
  QString error;                   // empty when the day was read
};
// A downloaded archive → its minutes (the zip inflated as it is parsed: the CSV never sits in memory whole).
DayResult buildDay(QByteArrayView zip, const QDate& day);
// The sha256 a .CHECKSUM file gives ("<hex>  <name>"), lower case; empty if none.
QByteArray checksumOf(QByteArrayView checksumFile);

// ---- storage: one blob per day and market (TechnicalDataStore's fp_days), columns then levels, compressed
QByteArray encodeDay(const std::vector<Minute>& minutes);
bool decodeDay(QByteArrayView blob, qint64 dayStart, std::vector<Minute>& out);

// ---- reading a minute
// The minute as a footprint candle whose "ticks" are its levels (1 bp each): charts::flow::analyze then gives its
// rows, POC, diagonal imbalances and stacks on the basis-point grid.
charts::flow::FootprintCandle levelCandle(const Minute& m);
// The minute on a market's own tick grid, for the Charts footprint: each level at the tick nearest its middle price,
// kept inside the bar's low / high (the end levels' middles can lie outside it).
charts::flow::FootprintCandle tickCandle(const Minute& m, const charts::flow::TickScale& ticks);

// What a minute (or a merged bar) shows, on the basis-point levels: the values of the footprint table (Data ›
// Technical data) and of machine learning. Prices are the levels' middles.
struct Stats {
  int levels = 0;                    // from the lowest traded level to the highest
  double poc = 0, pocVolume = 0;     // the level with the most volume (ties: the lowest)
  double vaLow = 0, vaHigh = 0;      // value area: 70 % of the volume around the POC (the larger neighbour first)
  int buyImbalances = 0, sellImbalances = 0;   // diagonal, ratio 3 (charts::flow::analyze, as the Charts footprint)
  int buyStacks = 0, sellStacks = 0;           // 3 or more in a row
};
Stats statsOf(const Minute& m, double valueArea = 0.70, const charts::flow::FootprintParams& p = {});

}  // namespace technicaldata::fp
