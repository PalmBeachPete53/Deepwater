#pragma once
// Data › Technical data, the perpetual's order book and positioning history (user's choice 2026-11-20, after asking
// which of Charts' three liquidity views gives an informational edge: the order book is the only one that is not a
// function of the bars; the liquidation map is a model whose real input is the open interest; the SMC levels come
// from the bars). From Binance's public daily archives of the USD-M perpetual (data.binance.vision, futures/um):
//
//   bookDepth  every ~30 s, the cumulative order book depth (base asset) from the price out to ±0.2 %, ±1 … ±5 %
//              (bids below, asks above). From 2023-01-01; ±0.2 % only on later days (the first ones have ±1 … ±5:
//              the missing bands are NaN). CSV "timestamp,percentage,depth,notional", 12 rows a snapshot.
//   metrics    every 5 minutes: open interest (base and USD), top traders' long/short ratio by accounts and by
//              positions, all accounts' long/short ratio, takers' buy/sell volume ratio. From 2020-09-01 (the first
//              months list each row twice: kept once).
// Pure (no network, no database): tests/technicaldata/perp_archive_tests.cpp.
#include <QByteArrayView>
#include <QDate>
#include <QString>
#include <QUrl>
#include <QVector>
#include <array>
#include <vector>

namespace technicaldata::perp {

// The bands, in this order everywhere (storage, tables, machine learning): bids −5 … −0.2 %, asks +0.2 … +5 %.
constexpr int kBands = 12;
const std::array<double, kBands>& bandPercents();   // −5 −4 −3 −2 −1 −0.2 0.2 1 2 3 4 5
int bandOf(double percent);                         // −1 if not one of them

struct DepthSample {
  qint64 t = 0;                    // Unix seconds UTC
  std::array<float, kBands> depth; // cumulative base quantity per band (NaN: the file has no such band then)
};

struct OiRow {
  qint64 t = 0;                    // Unix seconds UTC (on the 5-minute grid)
  double oi = 0, oiValue = 0;      // open interest, base asset and USD
  double topAccounts = 0;          // top traders' long/short ratio, by accounts
  double topPositions = 0;         // top traders' long/short ratio, by positions
  double accounts = 0;             // all accounts' long/short ratio
  double takerRatio = 0;           // takers' buy / sell volume
};

QUrl depthUrl(const QString& symbol, const QDate& day);     // …/futures/um/daily/bookDepth/BTCUSDT/BTCUSDT-bookDepth-<day>.zip
QUrl metricsUrl(const QString& symbol, const QDate& day);
QDate firstDepthDay();     // 2023-01-01
QDate firstMetricsDay();   // 2020-09-01

// CSV → rows of that day (UTC, [dayStart, dayStart + 86400)); bad: lines that could not be read.
std::vector<DepthSample> parseDepth(QByteArrayView csv, qint64 dayStart, int* bad = nullptr);
std::vector<OiRow> parseMetrics(QByteArrayView csv, qint64 dayStart, int* bad = nullptr);

// A day of depth, compressed (TechnicalDataStore's depth_days): "PDD1", n, seconds of the day, then each band.
QByteArray encodeDepth(const std::vector<DepthSample>& samples);
bool decodeDepth(QByteArrayView blob, qint64 dayStart, std::vector<DepthSample>& out);

// Values read from depth bands: bid / (bid + ask) out to a band (0.5 = balanced), from the pair of bands ±p.
double imbalance(const DepthSample& s, int askBand);   // askBand 6 … 11 (±0.2 … ±5 %); NaN without both sides

}  // namespace technicaldata::perp
