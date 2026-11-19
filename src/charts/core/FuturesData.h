#pragma once
// Order flow, chantier E, step E2 (pure part): Binance USD-M statistics for the liquidation model — reading the REST
// answers and the liquidation stream, keeping them in fixed-size record files, and lining them up with chart bars.
//
// Endpoints (checked live 2026-11-18, all keyless, weight 0, 1000 requests / 5 min per IP for /futures/data):
//   /futures/data/openInterestHist?symbol=&period=5m&limit≤500&startTime=   sumOpenInterest (base), …Value (quote),
//        timestamp = end of the period. Only the last 30 days: an older startTime is refused (HTTP 400, code −1130).
//   /futures/data/globalLongShortAccountRatio  (same paging and 30 days)   longShortRatio, longAccount, shortAccount
//   /fapi/v1/fundingRate?symbol=&limit≤1000                                 fundingTime, fundingRate, markPrice
//   wss://fstream.binance.com/market/ws/<symbol>@forceOrder                 one liquidation order per message (the
//        legacy /ws/ and /public/ws/ paths sent nothing in 90 s while /market/ws/ sent 128 for all symbols):
//        {"e":"forceOrder","E":…,"o":{"s":"BTCUSDT","S":"SELL","q":"0.367","p":"62520.00","ap":"62766.70","X":"FILLED",
//         "l":…,"z":"0.367","T":…}} — S SELL = a long position was liquidated (sold), BUY = a short; ap = average fill
//        price, z = filled quantity. Binance sends at most one liquidation per symbol per second (a snapshot), so
//        these are a sample of the real liquidations, not all of them.
//   /fapi/v1/leverageBracket is signed (HTTP 401 without a key): the brackets come from the table below.
#include "charts/core/Liquidation.h"
#include <QByteArray>
#include <QString>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace charts::flow {

// Fixed-size records (also the on-disk layout: plain little-endian structs).
struct OiPoint { int64_t timeMs = 0; double oi = 0, value = 0; };              // end of the 5 min period
struct RatioPoint { int64_t timeMs = 0; double ratio = 0, longShare = 0; };    // long / short accounts
struct FundingPoint { int64_t timeMs = 0; double rate = 0, markPrice = 0; };
struct LiqEvent {
  int64_t timeMs = 0;
  double price = 0, qty = 0;   // average fill price, filled quantity (base)
  int32_t longLiquidated = 0;  // 1: a long was liquidated (SELL order), 0: a short (BUY)
  int32_t pad = 0;
};

// The REST pages (JSON arrays) → points, ascending. False on an answer that isn't the expected array.
bool parseOpenInterestHist(const QByteArray& json, std::vector<OiPoint>& out);
bool parseLongShortRatio(const QByteArray& json, std::vector<RatioPoint>& out);
bool parseFundingRates(const QByteArray& json, std::vector<FundingPoint>& out);
// One forceOrder message → an event (and the symbol). False for anything else, or an order not filled at all.
bool parseForceOrder(const QByteArray& json, LiqEvent& out, QString* symbol = nullptr);

// Sorted merge by time: points of `add` replace those of `into` at the same time. Returns how many were new.
template <class T>
size_t mergeByTime(std::vector<T>& into, const std::vector<T>& add);

// Record files: a 16-byte header ("DWFS", version, record size, kind) then records. Appending writes only new records;
// a file whose header doesn't match is ignored (load) or started again (append).
template <class T>
bool appendRecords(const QString& path, std::span<const T> recs, uint32_t kind);
template <class T>
std::vector<T> loadRecords(const QString& path, uint32_t kind);

// For each bar (its close time, ms): the last point at or before it, if not older than maxAgeMs; NaN otherwise.
std::vector<double> alignOi(std::span<const int64_t> barCloseMs, std::span<const OiPoint> pts, int64_t maxAgeMs);
std::vector<double> alignRatio(std::span<const int64_t> barCloseMs, std::span<const RatioPoint> pts, int64_t maxAgeMs);

// Maintenance margin brackets used by the model. BTCUSDT: Binance's published USD-M table as known in 2024 — the
// current one could not be re-read on 2026-11-18 (the endpoint needs a key, the web page builds the table by script),
// so it is marked to check. Other symbols: one assumed bracket (0.5 %, up to 50×). `verified` says which.
struct BracketTable { std::vector<LeverageBracket> brackets; bool verified = false; QString source; };
BracketTable binanceBrackets(const QString& symbol);
// The maintenance amounts from the caps and rates (Binance: cum_i = cum_{i-1} + floor_i · (mmr_i − mmr_{i-1})).
void fillMaintenanceAmounts(std::vector<LeverageBracket>& b);

}  // namespace charts::flow
