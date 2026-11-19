#pragma once
// Order flow: a small reader for Binance's aggregated-trade messages, without Qt JSON.
//
// Why not QJsonDocument: it allocates a tree for every message, and the trade stream of a busy perpetual sends
// hundreds a second. These messages are flat objects of known keys, so a scanner reads them in place (UTF-8 bytes from
// REST, UTF-16 text from QWebSocket) with no allocation. Anything it does not recognise makes it fail, never guess:
// tests/charts/core/flow_trades_tests.cpp checks it against Qt JSON on real captured messages.
//
// Message shapes (Binance docs, checked 2026-11-18):
//   spot WS     {"e":"aggTrade","E":..,"s":"BTCUSDT","a":..,"p":"63981.93000000","q":"0.01721000","f":..,"l":..,"T":..,"m":true,"M":true}
//   futures WS  {"e":"aggTrade","E":..,"a":..,"s":"BTCUSDT","p":"63951.20","q":"0.174","nq":"0.174","f":..,"l":..,"T":..,"m":true}
//   REST page   [{"a":..,"p":"..","q":"..","f":..,"l":..,"T":..,"m":true,"M":true}, ...]   (futures: + "nq", no "M")
// m = "is the buyer the market maker": true → the seller hit the bid (aggressive SELL), false → aggressive BUY.
#include "charts/core/OrderBook.h"
#include "charts/core/Ticks.h"
#include <QByteArrayView>
#include <QStringView>
#include <cstdint>
#include <vector>

namespace charts::flow {

struct AggTrade {
  int64_t id = -1;          // a: aggregate trade id (consecutive on one market)
  int64_t firstId = -1;     // f, l: the exchange's own trades it groups
  int64_t lastId = -1;
  int64_t timeMs = 0;       // T: trade time
  int64_t eventMs = 0;      // E: event time (WS only; 0 for REST)
  Decimal price;            // p, exact
  Decimal qty;              // q, exact
  bool buyerMaker = false;  // m
};

// One object "{...}" (leading spaces allowed). False if it is not an aggTrade ("e" present with another value), a
// required field (a, p, q, T, m) is missing or malformed, or the text is not a flat JSON object.
bool parseAggTrade(QStringView text, AggTrade& out);
bool parseAggTrade(QByteArrayView text, AggTrade& out);
// A REST page "[{...},{...}]" appended to out (out is not cleared). False on any malformed element (out then holds
// the elements read before it).
bool parseAggTradesPage(QByteArrayView body, std::vector<AggTrade>& out);

// Depth (order book) messages, prices turned into ticks of `scale`. The vectors of `out` are cleared, their capacity
// kept: once warm, reading a message allocates nothing.
//   WS   {"e":"depthUpdate","E":..,("T":..,)"s":"BTCUSDT",("ps":..,)"U":..,"u":..,("pu":..,)"b":[["p","q"],..],"a":[..]}
//   REST {"lastUpdateId":..,("E":..,"T":..,)"bids":[["p","q"],..],"asks":[..]}
bool parseDepthUpdate(QStringView text, const TickScale& scale, DepthDiff& out);
bool parseDepthUpdate(QByteArrayView text, const TickScale& scale, DepthDiff& out);
struct DepthSnapshot {
  int64_t lastUpdateId = -1;
  std::vector<BookLevel> bids, asks;
};
bool parseDepthSnapshot(QByteArrayView body, const TickScale& scale, DepthSnapshot& out);

}  // namespace charts::flow
