#pragma once
// Order flow: what an exchange must provide to the trade stream (and, in a later step, the order book), behind
// interfaces so other exchanges can be added. Only Binance exists (spot + USD-M perpetuals): BinanceFeed.cpp.
//
// Threading: everything here runs on the GUI thread (QWebSocket and net::get answer there); callbacks are never
// called synchronously from the call that started them.
#include "charts/core/BinanceMsg.h"
#include "charts/core/FuturesData.h"
#include "charts/data/Fetch.h"
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace charts::data {

// A market whose trades we can stream. Built from a chart symbol: "BINANCE:BTCUSDT" (spot), "BINANCE:BTCUSDT.P"
// (USD-M perpetual, TradingView's ".P", as the chart's symbol search writes it).
struct FlowMarket {
  enum Venue { BinanceSpot, BinanceUsdm };
  Venue venue = BinanceSpot;
  QString symbol;   // the exchange's name, upper case: "BTCUSDT"
  static std::optional<FlowMarket> fromChartSymbol(const QString& chartSymbol);
  QString key() const;   // "binance-spot:BTCUSDT" / "binance-usdm:BTCUSDT"
  bool operator==(const FlowMarket& o) const { return venue == o.venue && symbol == o.symbol; }
};

enum class FeedState { Closed, Connecting, Open };

class ITradeSink {
public:
  virtual ~ITradeSink() = default;
  virtual void onTrade(const flow::AggTrade& t) = 0;                    // live, in arrival order
  virtual void onFeedState(FeedState s, const QString& why) = 0;         // why: for Closed, the reason (log text)
};

// Depth (order book) — the stream side lives in the depth thread (DepthStream's worker), not the GUI thread.
class IOrderBookSink {
public:
  virtual ~IOrderBookSink() = default;
  virtual void onDepthDiff(const flow::DepthDiff& d) = 0;              // a parsed diff event, in arrival order
  virtual void onDepthState(FeedState s, const QString& why) = 0;
};
class IDepthSocket {
public:
  virtual ~IDepthSocket() = default;
  // Created, opened and closed in the depth thread. urls: in the order to try (hosts switched off already removed).
  virtual void open(const QStringList& urls, const flow::TickScale& scale, IOrderBookSink* sink) = 0;
  virtual void close() = 0;
};

// Liquidation orders of one futures symbol (chantier E). Same states as the trades: Connecting, Open, Closed once.
class ILiquidationSink {
public:
  virtual ~ILiquidationSink() = default;
  virtual void onLiquidation(const flow::LiqEvent& e) = 0;
  virtual void onLiquidationState(FeedState s, const QString& why) = 0;
};

class IExchangeFeed {
public:
  virtual ~IExchangeFeed() = default;
  // Live trades into sink until closeTrades(). Reports Connecting, then Open, then Closed once (never reopens by
  // itself: the stream decides when to try again).
  virtual void openTrades(const FlowMarket& m, ITradeSink* sink) = 0;
  virtual void closeTrades() = 0;
  // One REST page of trades, ascending: fromId >= 0 → from that id; fromId < 0 → the latest ones. An Error with
  // status 429 / 418 means "slow down" / "banned for a while" (the stream waits).
  using TradesCb = std::function<void(const Result<std::vector<flow::AggTrade>>&)>;
  virtual void fetchTrades(const FlowMarket& m, qint64 fromId, int limit, TradesCb cb) = 0;
  virtual void fetchTickSize(const FlowMarket& m, std::function<void(const Result<flow::Decimal>&)> cb) = 0;
  // Pause between two history pages, from the endpoint's weight (Binance: spot 4 of 6000/min, futures 20 of 2400/min).
  virtual int historyPaceMs(const FlowMarket& m) const = 0;
  // The REST order book snapshot (raw JSON body; parsed in the depth thread). GUI thread.
  virtual void fetchDepthSnapshot(const FlowMarket&, std::function<void(const Result<QByteArray>&)> cb) {
    later([cb] { cb(Result<QByteArray>::failure(Error{"no order book for this exchange", 501, false, {}})); });
  }
  // Futures statistics (open interest, ratios, funding): one REST answer as is, path and query from the host's root
  // ("/futures/data/openInterestHist?…"). GUI thread.
  virtual void fetchFuturesData(const QString& pathAndQuery, std::function<void(const Result<QByteArray>&)> cb) {
    Q_UNUSED(pathAndQuery);
    later([cb] { cb(Result<QByteArray>::failure(Error{"no futures statistics for this exchange", 501, false, {}})); });
  }
  // Live liquidation orders of a futures symbol ("BTCUSDT") until closeLiquidations().
  virtual void openLiquidations(const QString& symbol, ILiquidationSink* sink) {
    Q_UNUSED(symbol);
    later([sink] { sink->onLiquidationState(FeedState::Closed, "no liquidation stream for this exchange"); });
  }
  virtual void closeLiquidations() {}
};

std::unique_ptr<IExchangeFeed> makeExchangeFeed(const FlowMarket& m);

// Binance endpoints (pure, tested). Futures market streams moved to /market/ws/ (the legacy /ws/ path stopped
// sending aggTrade and kline data after 2026-04-23; measured 2026-11-18: 0 messages in 5 s vs 40 on /market/ws/).
QStringList binanceTradeSocketUrls(const FlowMarket& m);   // in the order to try
QStringList binanceRestBases(const FlowMarket& m);         // "https://api.binance.com/api/v3", ...
QString binanceAggTradesPath(const FlowMarket& m, qint64 fromId, int limit);   // "/aggTrades?symbol=..&limit=..[&fromId=..]"
// Depth: diff stream every 100 ms (futures: /public/ws/, the high-frequency path), snapshot limit (spot 5000 = weight
// 250, futures 1000 = weight 20).
QStringList binanceDepthSocketUrls(const FlowMarket& m);
int binanceDepthLimit(const FlowMarket& m);
QString binanceLiquidationSocketUrl(const QString& symbol);   // wss://fstream.binance.com/market/ws/<symbol>@forceOrder
std::unique_ptr<IDepthSocket> makeDepthSocket();   // call in the depth thread

namespace testing {
// Tests: every stream made from now on uses this factory (nullptr = back to Binance).
void setFeedFactory(std::function<std::unique_ptr<IExchangeFeed>(const FlowMarket&)> f);
void setDepthSocketFactory(std::function<std::unique_ptr<IDepthSocket>()> f);   // called in the depth thread
}

}  // namespace charts::data
