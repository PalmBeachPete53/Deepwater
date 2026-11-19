#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/providers.js. Market data from free, keyless public sources, each with fallbacks. Keys you add
// yourself (<data>/charts/feeds.json, see Feeds.h) go in front of them.
//
//   Crypto  BINANCE:BTCUSDT   Binance -> Binance market-data mirror -> Binance.US -> Coinbase -> Kraken
//   Perps   BINANCE:BTCUSDT.P Binance USD-M futures -> Bybit linear perpetuals
//   Stocks  US:AAPL           Your Alpaca / Polygon key -> Yahoo (when it isn't rate limiting you) -> Nasdaq
//   Forex   FX:EURUSD         Your OANDA key -> Yahoo -> Kraken
//   OANDA   OANDA:XAU_USD     Your OANDA key only (any instrument your account lists)
//   Futures CME:NQ1!          Your Polygon key (feeds.json) -> Yahoo, about 10 minutes delayed
//   Global  YAHOO:^FTSE       Yahoo only (indices, futures, non-US stocks)
//   Files   CSV:NAME          Your own imported bars (Imports.h)
//
// Symbols are "EXCHANGE:TICKER" strings, like TradingView's. resolve() turns what a person types
// ("btc", "aapl", "eurusd", "VOD.L") into one.
//
// Every call is asynchronous: the callback runs later on the GUI thread (never inside the call), and is skipped
// when `ctx` (optional) was destroyed meanwhile. toJson() gives the JS JSON shapes the local HTTP API returns.
#include "charts/data/Fetch.h"
#include "charts/data/Feeds.h"
#include <QMap>
#include <QHash>
#include <QSet>

namespace charts::data {

// ---------- symbols ----------
struct Symbol {
  QString id, exchange, ticker;
  QJsonObject toJson() const;   // {id, exchange, ticker}
};
struct SpreadParts { QString a, op, b; };
// Spreads and ratios, like TradingView's BTCUSDT/ETHUSDT or AAPL - MSFT: one operator between two
// symbols (or a symbol and a number). Canonical id: "SPREAD:<id> <op> <id>", spaces around op.
std::optional<SpreadParts> parseSpread(const QString& raw);
void resolve(const QString& raw, QObject* ctx, Callback<Symbol> cb);
QString kindOf(const QString& id);   // "crypto" | "fx" | "stock" | "global"

// CME equity index futures as continuous front-month charts (NQ1!). JS FUTURES, in its order.
struct FuturesInfo { QString root, name, yahoo; };
const QList<FuturesInfo>& futures();

// Binance spot symbols that are TRADING: ticker -> base / quote, in Binance's own order (search ranks by it).
struct BinancePair { QString base, quote; };
struct BinanceSymbols {
  QStringList order;
  QHash<QString, BinancePair> map;
  bool has(const QString& t) const { return map.contains(t); }
  int size() const { return map.size(); }
};
void binanceSymbolMap(QObject* ctx, Callback<BinanceSymbols> cb);   // never fails: empty when Binance is unreachable
void perpSymbols(QObject* ctx, Callback<QSet<QString>> cb);         // Binance USD-M perpetuals; never fails
// "BTCUSD" -> "XBTUSD": Kraken's own name for the pair, keyed by the names everyone else uses.
QHash<QString, QString> krakenPairMap(const QJsonObject& result);

// ---------- candles ----------
struct CandlesResult {
  QString symbol, tf, provider;
  Bars candles;
  QStringList notes;
  QJsonObject toJson() const;   // {symbol, tf, provider, candles:[{time,open,high,low,close,volume}], notes}
};
// Same provider chains, order, notes and in-memory cache TTLs as the JS. tf like "1D", "15m"; limit 10..20000.
void getCandles(const QString& rawSymbol, const QString& tf, int limit, QObject* ctx, Callback<CandlesResult> cb);
// Yahoo's chart JSON -> bars (daily bars at the exchange's local date, UTC midnight). Error: Yahoo's message.
Result<Bars> parseYahoo(const QJsonObject& j, const QString& tf);
// Nasdaq stamps intraday points with New York wall-clock time written as if it were UTC.
qint64 nyWallToUtc(qint64 ms);
bool yahooAvailable();

// ---------- quotes, spreads ----------
struct Quote {
  double price = 0, changePct = 0, volume = 0;
  QString status;        // Nasdaq's marketStatus (only from the Nasdaq watchlist); empty = absent
  bool hasStatus = false;
  QString error;         // non-empty: {error}
  QJsonObject toJson() const;
};
using Quotes = QMap<QString, Quote>;   // symbol id -> quote
QJsonObject quotesToJson(const Quotes& q);
void getQuotes(const QStringList& symbols, QObject* ctx, Callback<Quotes> cb);

struct SpreadQuote {
  QString symbol;
  double bid = 0, ask = 0, spreadPct = 0;
  QString source;
  QJsonObject toJson() const;   // {symbol, bid, ask, spreadPct, source}
};
// Live bid and ask for the tester's spread setting. Only the crypto exchanges publish one for free.
void getSpread(const QString& rawSymbol, QObject* ctx, Callback<SpreadQuote> cb);

// ---------- search ----------
struct SearchHit {
  QString symbol, name, type, exchange;
  QJsonObject toJson() const;
};
QJsonArray hitsToJson(const QList<SearchHit>& hits);
void search(const QString& q, QObject* ctx, Callback<QList<SearchHit>> cb);

// ---------- screeners ----------
// Stocks: {ticker, symbol, name, price, changePct, volume, marketCap, sector, industry, country, live}
// Crypto (Binance): {ticker, symbol, name, price, changePct, volume, high, low, sector}; Kraken fallback has no high/low.
struct ScreenerRow {
  QString ticker, symbol, name, sector, industry, country;
  double price = 0, changePct = 0, volume = 0, marketCap = 0, high = 0, low = 0;
  bool live = false;
  enum Kind { Stock, Crypto, CryptoNoRange, Raw } kind = Stock;
  QJsonObject raw;   // kind Raw: a fixture row, returned as is (OFFLINE)
  QJsonObject toJson() const;
};
QJsonArray rowsToJson(const QList<ScreenerRow>& rows);
void nasdaqScreener(const QString& kind, QObject* ctx, Callback<QList<ScreenerRow>> cb);   // kind "stocks" | "etf"
void cryptoScreener(QObject* ctx, Callback<QList<ScreenerRow>> cb);

// ---------- calendar, health ----------
struct CalendarEvent {
  QJsonValue title, country, date, impact, forecast, previous;   // as Forex Factory sends them
  QJsonObject toJson() const;
};
QJsonArray eventsToJson(const QList<CalendarEvent>& e);
void economicCalendar(QObject* ctx, Callback<QList<CalendarEvent>> cb);   // Forex Factory's weekly export

struct Probe { bool ok = false; qint64 ms = 0; QString error; QJsonObject toJson() const; };
using Probes = QMap<QString, Probe>;
QJsonObject probesToJson(const Probes& p);
void probeAll(QObject* ctx, Callback<Probes> cb);

// The JS `health` map: last outcome per provider ("binance", "binance-futures", "coinbase", "kraken", "yahoo",
// "nasdaq", "calendar").
struct HealthEntry { bool ok = false; QString at; QString error; };   // error empty = absent
const QMap<QString, HealthEntry>& health();
QJsonObject healthJson();   // {name: {ok, at, error?}}
void mark(const QString& name, bool ok, const Error* err = nullptr);

// ---------- for the other data modules (market-info.js imports getJson, cached, NQ, resolve) ----------
Headers nqHeaders();   // Origin / Referer of www.nasdaq.com, which api.nasdaq.com requires

// ---------- tests ----------
namespace testing {
void reset();                                   // every module state: caches, hosts down, Yahoo backoff, health…
void setOffline(const QString& fixturesDir);    // the JS QT_OFFLINE=1 fixture mode; empty = off
}
}  // namespace charts::data
