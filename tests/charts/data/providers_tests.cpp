// Port of quant-terminal test/providers.test.js: provider fallbacks with a fake network, no live calls. Each case
// is a real bug found by blocking hosts: Binance blocked made "BTCUSDT" resolve to a US stock, and a firewall that
// silently drops Binance cost a full timeout on every chart.
// Plus CHARTS_LIVE=1: a short live check of the chains from this machine (`CHARTS_LIVE=1 charts_data_tests live`).
#include "Check.h"
#include "FakeNet.h"
#include "charts/data/Providers.h"
#include "charts/data/ExchangeFeed.h"
#include "charts/data/LiveFeed.h"
#include "charts/core/Series.h"
#include "core/Paths.h"
#include <QFile>
#include <QHash>
#include <QVariant>
#include <cmath>
#include <cstdio>

using namespace charts;
using namespace charts::data;

namespace {
Bars& btc() {
  static Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    f.open(QIODevice::ReadOnly);
    Bars out;
    for (const auto& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out << Bar{qint64(o["time"].toDouble()), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(), o["close"].toDouble(), o["volume"].toDouble()};
    }
    return out;
  }();
  return b;
}
QHash<QString, QVariant> mode;   // host -> "drop" | "dns" | status code; "*" = every host
std::function<std::optional<fakenet::Reply>(const QUrl&)> extra;   // a test's own fetch override

fakenet::Reply handler(const QUrl& u) {
  if (extra)
    if (auto r = extra(u)) return *r;
  const QVariant m = mode.contains(u.host()) ? mode.value(u.host()) : mode.value("*");
  if (m.toString() == "drop") return fakenet::Reply::drop();
  if (m.toString() == "dns") return fakenet::Reply::dns();
  if (m.isValid() && m.toInt()) return fakenet::Reply::text("blocked", m.toInt());
  if (u.host() == "api.exchange.coinbase.com") {
    const double end = parseIsoMs(fakenet::param(u, "end")) / 1000;
    Bars rows;
    for (const Bar& b : btc()) if (b.time < end) rows << b;
    rows = sliceLast(rows, 300);
    QJsonArray a;
    for (int i = rows.size() - 1; i >= 0; --i)
      a.append(QJsonArray{double(rows[i].time), rows[i].low, rows[i].high, rows[i].open, rows[i].close, rows[i].volume});
    return fakenet::Reply::json(a);
  }
  return fakenet::Reply::text("not mocked", 404);
}

Result<Symbol> resolveNow(const QString& s) { return fakenet::await<Symbol>([s](Callback<Symbol> cb) { resolve(s, nullptr, cb); }); }
Result<CandlesResult> candlesNow(const QString& s, const QString& tf, int limit) {
  return fakenet::await<CandlesResult>([=](Callback<CandlesResult> cb) { getCandles(s, tf, limit, nullptr, cb); });
}
bool anyCall(const QString& part) {
  for (const auto& c : fakenet::calls()) if (c.host.contains(part)) return true;
  return false;
}
QString exchangeOf(const QString& s) { auto r = resolveNow(s); return r.ok() ? r.value.exchange : "error: " + r.error->message; }
QString idOf(const QString& s) { auto r = resolveNow(s); return r.ok() ? r.value.id : "error: " + r.error->message; }

void setUp() {
  data::testing::reset();
  data::testing::setOffline({});
  data::testing::setTimeoutMs(300);
  QFile::remove(feeds::feedsFile());   // the public sources only
  extra = nullptr;
  mode.clear();
  fakenet::calls().clear();
  fakenet::install(handler);
}
}  // namespace

TEST("providers: with Binance blocked, crypto still resolves as crypto and loads from Coinbase") {
  setUp();
  mode = {{"api.binance.com", 451}, {"data-api.binance.vision", 451}, {"api.binance.us", 451}};
  for (const QString s : {"BTCUSDT", "btc", "ETH-USD", "SOL/USDT"}) CHECK_EQ(exchangeOf(s), QString("BINANCE"));
  CHECK_EQ(exchangeOf("AAPL"), QString("US"));
  const auto r = candlesNow("BTCUSDT", "1D", 100);
  CHECK(r.ok());
  CHECK_EQ(r.value.provider, QString("Coinbase"));
  CHECK_EQ(r.value.candles.size(), 100);
}

TEST("providers: a silently dropped host costs one timeout, then is skipped") {
  mode = {{"api.binance.com", "drop"}, {"data-api.binance.vision", "drop"}, {"api.binance.us", "drop"}};
  QElapsedTimer t;
  t.start();
  const auto a = candlesNow("BINANCE:ETHUSDT", "1D", 50);
  CHECK_EQ(a.value.provider, QString("Coinbase"));
  CHECK(t.elapsed() < 1500);   // three hosts should time out together
  t.restart();
  fakenet::calls().clear();
  const auto b = candlesNow("BINANCE:SOLUSDT", "1D", 50);
  CHECK_EQ(b.value.provider, QString("Coinbase"));
  CHECK(t.elapsed() < 200);    // second chart skips the dead hosts
  CHECK(!anyCall("binance"));  // no new Binance calls while it is marked down
}

TEST("providers: network failures are upstream errors (the server answers 502)") {
  mode = {{"*", "dns"}};
  const auto r = fakenet::await<QList<CalendarEvent>>([](Callback<QList<CalendarEvent>> cb) { economicCalendar(nullptr, cb); });
  CHECK(!r.ok());
  CHECK(r.error && r.error->upstream);
}

TEST("providers: the chart note for a fallback is plain words: no hostnames or HTTP codes") {
  // A Skool member saw "Using Kraken (Yahoo: query1.finance.yahoo.com returned HTTP 429)" on a chart and had to ask what it meant.
  mode = {{"api.binance.com", 451}, {"data-api.binance.vision", 451}, {"api.binance.us", 451}};
  const auto r = candlesNow("BTCUSDT", "1D", 100);
  const QString text = r.value.notes.join(" ");
  CHECK(text.contains("Binance isn't answering right now, so this chart uses Coinbase."));
  CHECK(!text.contains(QRegularExpression(R"(HTTP|\.com|returned)")));
}

TEST("providers: Kraken pairs are keyed by the names everyone else uses (BTC, DOGE), valued by Kraken's own") {
  const auto m = krakenPairMap(QJsonObject{
      {"XXBTZUSD", QJsonObject{{"altname", "XBTUSD"}, {"wsname", "XBT/USD"}, {"status", "online"}}},
      {"XDGUSD", QJsonObject{{"altname", "XDGUSD"}, {"wsname", "XDG/USD"}, {"status", "online"}}},
      {"SOLUSD", QJsonObject{{"altname", "SOLUSD"}, {"wsname", "SOL/USD"}, {"status", "online"}}},
      {"OLDUSD", QJsonObject{{"altname", "OLDUSD"}, {"wsname", "OLD/USD"}, {"status", "delisted"}}},
  });
  CHECK_EQ(m.value("BTCUSD"), QString("XBTUSD"));
  CHECK_EQ(m.value("DOGEUSD"), QString("XDGUSD"));
  CHECK_EQ(m.value("SOLUSD"), QString("SOLUSD"));
  CHECK(!m.contains("OLDUSD"));
}

TEST("providers: XAUUSD charts gold, and falls back to PAXG when Yahoo is down") {
  mode = {{"query1.finance.yahoo.com", 429}, {"query2.finance.yahoo.com", 429}};
  for (const QString s : {"XAUUSD", "xau/usd", "FX:XAUUSD"}) CHECK_EQ(idOf(s), QString("FX:XAUUSD"));
  CHECK_EQ(idOf("XAGUSD"), QString("FX:XAGUSD"));
  CHECK_EQ(exchangeOf("GOLD"), QString("US"));   // Barrick's ticker stays a stock
  for (const QString s : {"^FTSE", "YAHOO:^FTSE"}) CHECK_EQ(idOf(s), QString("YAHOO:^FTSE"));
  fakenet::calls().clear();
  const auto r = candlesNow("XAUUSD", "1D", 50);
  CHECK(anyCall("binance") || anyCall("coinbase"));   // gold tries the PAXG token feeds after Yahoo
  if (r.ok()) {
    bool paxg = false;
    for (const auto& n : r.value.notes) paxg = paxg || n.contains("PAXG");
    CHECK(paxg);
  }
}

TEST("providers: crypto perpetual futures: BTCUSDT.P charts Binance futures, then Bybit where Binance futures is blocked") {
  // A YouTube comment: "only spot trading is available in the list, and futures are not there".
  for (const auto& [typed, id] : QList<QPair<QString, QString>>{{"BTCUSDT.P", "BINANCE:BTCUSDT.P"}, {"btc.p", "BINANCE:BTCUSDT.P"},
                                                               {"ETHPERP", "BINANCE:ETHUSDT.P"}, {"BINANCE:SOLUSDT.P", "BINANCE:SOLUSDT.P"}})
    CHECK_EQ(idOf(typed), id);
  auto kline = [](const Bar& b) {
    return QJsonArray{double(b.time * 1000), series::numStr(b.open), series::numStr(b.high), series::numStr(b.low), series::numStr(b.close),
                      series::numStr(b.volume)};
  };
  extra = [kline](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (u.host() == "fapi.binance.com" && mode.value("fapi.binance.com").toInt() != 451) {
      CHECK_EQ(fakenet::param(u, "symbol"), QString("BTCUSDT"));
      QJsonArray a;
      for (const Bar& b : sliceLast(btc(), fakenet::param(u, "limit").toInt())) a.append(kline(b));
      return fakenet::Reply::json(a);
    }
    if (u.host() == "api.bybit.com") {
      CHECK_EQ(fakenet::param(u, "category"), QString("linear"));
      const Bars last = sliceLast(btc(), 60);
      QJsonArray list;
      for (int i = last.size() - 1; i >= 0; --i) list.append(kline(last[i]));
      return fakenet::Reply::json(QJsonObject{{"retCode", 0}, {"result", QJsonObject{{"list", list}}}});
    }
    return std::nullopt;
  };
  mode.clear();
  const auto a = candlesNow("BTCUSDT.P", "1D", 50);
  CHECK_EQ(a.value.provider, QString("Binance futures"));
  CHECK_EQ(a.value.candles.size(), 50);
  mode = {{"fapi.binance.com", 451}};
  const auto b = candlesNow("BINANCE:BTCUSDT.P", "1W", 5);
  CHECK_EQ(b.value.provider, QString("Bybit"));
  CHECK(b.value.notes.join(" ").contains("Binance futures isn't answering right now, so this chart uses Bybit."));
  extra = nullptr;
}

// ---------- beyond the JS file: pure helpers and the OFFLINE fixture mode ----------
TEST("providers: parseSpread, kindOf and nyWallToUtc") {
  CHECK(!parseSpread("EUR/USD"));
  CHECK(!parseSpread("SOL/USDT"));
  const auto s = parseSpread("btcusdt/ethusdt");
  CHECK(s && s->a == "BTCUSDT" && s->op == "/" && s->b == "ETHUSDT");
  const auto m = parseSpread("AAPL - MSFT");
  CHECK(m && m->op == "-");
  CHECK(!parseSpread("BRK-B"));   // + and - need spaces around them
  const auto c = parseSpread("SPREAD:US:SPY * 2");
  CHECK(c && c->a == "US:SPY" && c->b == "2");
  CHECK_EQ(kindOf("BINANCE:BTCUSDT"), QString("crypto"));
  CHECK_EQ(kindOf("FX:EURUSD"), QString("fx"));
  CHECK_EQ(kindOf("US:AAPL"), QString("stock"));
  CHECK_EQ(kindOf("CME:NQ1!"), QString("global"));
  CHECK_EQ(isoMs(0), QString("1970-01-01T00:00:00.000Z"));
  CHECK_EQ(encodeUriComponent("btc|stocks ^(x)!"), QString("btc%7Cstocks%20%5E(x)!"));
  CHECK_EQ(formEncode({{"a", "x y*~"}, {"b", "1/2"}}), QString("a=x+y*%7E&b=1%2F2"));
  CHECK_NEAR(parseIsoMs("2026-10-14T12:00:00.000000000Z"), 1791979200000.0, 0);
  CHECK_NEAR(parseIsoMs("2026-10-14T08:00:00-04:00"), 1791979200000.0, 0);
  // 09:30 wall time on a summer day (EDT, UTC-4) and a winter day (EST, UTC-5)
  CHECK_EQ(nyWallToUtc(qint64(parseIsoMs("2026-07-01T09:30:00Z"))), qint64(parseIsoMs("2026-07-01T13:30:00Z")));
  CHECK_EQ(nyWallToUtc(qint64(parseIsoMs("2026-01-05T09:30:00Z"))), qint64(parseIsoMs("2026-01-05T14:30:00Z")));
}

TEST("providers: OFFLINE fixture mode charts the fixture, spreads join two symbols") {
  setUp();
  data::testing::setOffline(QStringLiteral(CHARTS_FIXTURES));
  const auto r = candlesNow("BTCUSDT", "1D", 50);
  CHECK_EQ(r.value.provider, QString("fixture"));
  CHECK_EQ(r.value.symbol, QString("BINANCE:BTCUSDT"));
  CHECK_EQ(r.value.candles.size(), 50);
  const auto sp = candlesNow("BTCUSDT/ETHUSDT", "1D", 20);
  CHECK(sp.ok());
  CHECK_EQ(sp.value.symbol, QString("SPREAD:BINANCE:BTCUSDT / BINANCE:ETHUSDT"));
  CHECK_EQ(sp.value.provider, QString("fixture + fixture"));
  CHECK_EQ(sp.value.candles.size(), 20);
  CHECK_NEAR(sp.value.candles.last().close, 1.0, 1e-12);
  const auto k = candlesNow("BTCUSDT*2", "1D", 10);
  CHECK_EQ(k.value.symbol, QString("SPREAD:BINANCE:BTCUSDT * 2"));
  CHECK_NEAR(k.value.candles.last().close, 2 * btc().last().close, 1e-9);
  CHECK_EQ(k.value.notes, QStringList{"Spread built from both symbols' bars at matching times."});
  const auto bad = candlesNow("2/3", "1D", 10);
  CHECK(bad.error && bad.error->message == "A spread needs at least one symbol" && bad.error->status == 400);
  const auto tf = candlesNow("BTCUSDT", "7x", 10);
  CHECK(tf.error && tf.error->status == 400 && tf.error->message.startsWith("Bad timeframe"));
  const auto csv = candlesNow("CSV:NOPE", "1D", 10);   // Imports.h: no such file
  CHECK(csv.error && csv.error->status == 404);
  const auto empty = resolveNow("   ");
  CHECK(empty.error && empty.error->message == "Empty symbol");
  const auto weird = resolveNow("<script>");
  CHECK(weird.error && weird.error->message == "\"<script>\" isn't a symbol");
  const auto json = r.value.toJson();
  CHECK_EQ(json.keys(), (QStringList{"candles", "notes", "provider", "symbol", "tf"}));
  CHECK_EQ(json["candles"].toArray().at(0).toObject().keys(), (QStringList{"close", "high", "low", "open", "time", "volume"}));
  data::testing::setOffline({});
}

TEST("providers: LiveFeed picks the Binance stream or polling like chart.js") {
  CHECK_EQ(LiveFeed::binanceWsTf("1D"), QString("1d"));
  CHECK_EQ(LiveFeed::binanceWsTf("1M"), QString("1M"));
  CHECK_EQ(LiveFeed::binanceWsTf("2D"), QString());
  CHECK_EQ(LiveFeed::pollMs("KRAKEN:XBTUSD"), 3000);
  CHECK_EQ(LiveFeed::pollMs("US:AAPL"), 10000);
  LiveFeed f;
  f.start("US:AAPL", "1D", "Nasdaq");
  CHECK(f.isPolling() && !f.isSocket());
  f.stop();
  CHECK(!f.isPolling());
}

namespace {
fakenet::Reply klines(int n) {
  QJsonArray rows;
  const double t0 = 1700000000000.0;
  for (int i = 0; i < n; ++i) rows.append(QJsonArray{t0 + i * 86400000.0, "100", "110", "90", "105", "12"});
  return fakenet::Reply::json(rows);
}
}  // namespace

// Audit D19: Binance.US is a separate exchange, the last resort under its own name, never the preferred host.
TEST("providers: Binance.US is a named last resort, never sticky") {
  setUp();
  mode = {{"*", 451}};
  bool globalUp = false;
  extra = [&](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (!u.path().endsWith("/klines")) return std::nullopt;
    if (u.host() == "api.binance.us") return klines(60);
    if (globalUp && u.host() == "api.binance.com") return klines(60);
    return std::nullopt;
  };
  const auto a = candlesNow("BINANCE:BTCUSDT", "1D", 50);
  CHECK(a.ok());
  CHECK_EQ(a.value.provider, QString("Binance.US"));   // not "Binance": the chart opens no global kline socket on it
  globalUp = true;
  fakenet::calls().clear();
  const auto b = candlesNow("BINANCE:BTCUSDT", "1D", 40);
  CHECK(b.ok());
  CHECK_EQ(b.value.provider, QString("Binance"));
  CHECK(anyCall("api.binance.com"));
  CHECK(!anyCall("api.binance.us"));   // the global host came back: Binance.US is not asked
  extra = nullptr;
}

TEST("providers: HTTP 429 marks the host down for a while: the next call sends nothing") {
  setUp();
  extra = [](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (u.host() == "ratelimited.example") return fakenet::Reply::text("slow down", 429);
    return std::nullopt;
  };
  const auto a = fakenet::await<QJsonDocument>([](Callback<QJsonDocument> cb) { getJson("https://ratelimited.example/x", {}, cb); });
  CHECK(!a.ok() && a.error->status == 429);
  fakenet::calls().clear();
  const auto b = fakenet::await<QJsonDocument>([](Callback<QJsonDocument> cb) { getJson("https://ratelimited.example/y", {}, cb); });
  CHECK(!b.ok());
  CHECK(b.error->message.contains("slow down"));
  CHECK(!anyCall("ratelimited.example"));
  extra = nullptr;
}

TEST("providers: Yahoo weekly and monthly bars keep their date across a DST change") {
  // 1 Oct 2023 00:00 New York (EDT, 04:00 UTC) and 1 Dec 2023 00:00 (EST, 05:00 UTC), read with today's offset EST
  const QJsonObject j{{"chart", QJsonObject{{"result", QJsonArray{QJsonObject{
      {"meta", QJsonObject{{"gmtoffset", -18000}, {"exchangeTimezoneName", "America/New_York"}}},
      {"timestamp", QJsonArray{1696132800, 1701406800}},
      {"indicators", QJsonObject{{"quote", QJsonArray{QJsonObject{{"open", QJsonArray{1, 2}}, {"high", QJsonArray{1, 2}}, {"low", QJsonArray{1, 2}},
                                                                    {"close", QJsonArray{1, 2}}, {"volume", QJsonArray{10, 20}}}}}}}}}}}}};
  const auto r = parseYahoo(j, "1M");
  CHECK(r.ok());
  CHECK_EQ(r.value.size(), 2);
  CHECK_EQ(r.value.value(0).time, qint64(1696118400));   // 2023-10-01, not 09-30
  CHECK_EQ(r.value.value(1).time, qint64(1701388800));   // 2023-12-01
}

// Audit D10: one failed Nasdaq screener call made every US ticker resolve to crypto for the session.
TEST("providers: a failed US ticker list is not kept; the next resolve asks again") {
  setUp();
  bool nasdaqUp = false;
  extra = [&](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (u.path().endsWith("/exchangeInfo"))
      return fakenet::Reply::json(QJsonObject{{"symbols", QJsonArray{
          QJsonObject{{"symbol", "LINKUSDT"}, {"status", "TRADING"}, {"baseAsset", "LINK"}, {"quoteAsset", "USDT"}},
          QJsonObject{{"symbol", "UNIUSDT"}, {"status", "TRADING"}, {"baseAsset", "UNI"}, {"quoteAsset", "USDT"}}}}});
    if (u.host() == "api.nasdaq.com" && u.path().contains("/screener/")) {
      if (!nasdaqUp) return fakenet::Reply::text("down", 503);
      return fakenet::Reply::json(QJsonObject{{"data", QJsonObject{{"rows", QJsonArray{
          QJsonObject{{"symbol", "LINK"}, {"name", "Link Inc"}}, QJsonObject{{"symbol", "UNI"}, {"name", "Uni Corp"}}}}}}});
    }
    return std::nullopt;
  };
  CHECK_EQ(idOf("LINK"), QString("BINANCE:LINKUSDT"));   // no US list: crypto
  nasdaqUp = true;
  CHECK_EQ(idOf("UNI"), QString("US:UNI"));              // the list came this time: the US stock
  extra = nullptr;
}

// Audit D9: where api.binance.com is blocked, the tick size comes from the mirror (no order flow started at all).
TEST("providers: the order-flow tick size falls back to the other Binance host") {
  setUp();
  mode = {{"api.binance.com", 451}};
  extra = [](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (u.host() == "data-api.binance.vision" && u.path().endsWith("/exchangeInfo"))
      return fakenet::Reply::json(QJsonObject{{"symbols", QJsonArray{QJsonObject{
          {"symbol", "BTCUSDT"}, {"filters", QJsonArray{QJsonObject{{"filterType", "PRICE_FILTER"}, {"tickSize", "0.01000000"}}}}}}}});
    return std::nullopt;
  };
  data::testing::setFeedFactory(nullptr);
  auto feed = makeExchangeFeed(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"));
  const auto r = fakenet::await<flow::Decimal>([&](Callback<flow::Decimal> cb) { feed->fetchTickSize(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"), cb); });
  CHECK(r.ok());
  CHECK(r.ok() && r.value.mant == 1 && r.value.exp == -2);
  CHECK(anyCall("api.binance.com") && anyCall("data-api.binance.vision"));
  extra = nullptr;
}

// ---------- live check (CHARTS_LIVE=1): a handful of real requests ----------
TEST("live: provider chains from this machine") {
  if (qEnvironmentVariable("CHARTS_LIVE") != "1") return;
  data::testing::reset();
  data::testing::setOffline({});
  data::testing::setTransport(nullptr);
  data::testing::setTimeoutMs(0);
  QFile::remove(feeds::feedsFile());
  const QList<QPair<QString, QString>> cases = {{"BTCUSDT", "1D"}, {"AAPL", "1D"}, {"EURUSD", "1h"}, {"NQ1!", "1h"}, {"BTCUSDT.P", "1h"}};
  for (const auto& [sym, tf] : cases) {
    QElapsedTimer t;
    t.start();
    const auto r = fakenet::await<CandlesResult>([&](Callback<CandlesResult> cb) { getCandles(sym, tf, 300, nullptr, cb); }, 60000);
    if (!r.ok()) {
      std::printf("LIVE %-10s %-3s ERROR %s\n", qPrintable(sym), qPrintable(tf), qPrintable(r.error->message));
      CHECK(r.ok());
      continue;
    }
    const Bar& b = r.value.candles.last();
    std::printf("LIVE %-10s %-3s %-22s %-18s bars=%-4lld last=%s o=%g h=%g l=%g c=%g v=%g (%lld ms)\n", qPrintable(sym), qPrintable(tf),
                qPrintable(r.value.symbol), qPrintable(r.value.provider), qlonglong(r.value.candles.size()),
                qPrintable(series::isoString(b.time)), b.open, b.high, b.low, b.close, b.volume, qlonglong(t.elapsed()));
    for (const auto& n : r.value.notes) std::printf("     note: %s\n", qPrintable(n));
    CHECK(!r.value.candles.isEmpty());
  }
  // The kline WebSocket of a Binance chart: the first message
  LiveFeed f;
  QEventLoop loop;
  Bars got;
  QObject::connect(&f, &LiveFeed::barsUpdated, &loop, [&](const Bars& b) { got = b; loop.quit(); });
  QTimer::singleShot(20000, &loop, &QEventLoop::quit);
  f.start("BINANCE:BTCUSDT", "1m", "Binance");
  loop.exec();
  std::printf("LIVE socket %s %s -> %s\n", qPrintable(f.socketUrl()), f.isSocket() ? "open" : "closed",
              got.isEmpty() ? "no message" : qPrintable(QString("bar %1 close %2").arg(series::isoString(got[0].time)).arg(got[0].close)));
  CHECK(!got.isEmpty());
  f.stop();
}

// Audit G11: with no host known good (every launch), each Binance request went to the three hosts at once, so the
// ~10 MB symbol list was downloaded three times. A ping goes to the three first; the request then goes to one host.
TEST("providers: no Binance host known yet: a ping to each, then the symbol list from one host only") {
  setUp();
  extra = [](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (u.host().contains("binance") && u.path().endsWith("/ping")) return fakenet::Reply::json(QJsonObject{});
    if (u.host().contains("binance") && u.path().endsWith("/exchangeInfo"))
      return fakenet::Reply::json(QJsonObject{{"symbols", QJsonArray{
          QJsonObject{{"symbol", "BTCUSDT"}, {"status", "TRADING"}, {"baseAsset", "BTC"}, {"quoteAsset", "USDT"}}}}});
    return std::nullopt;
  };
  const auto r = fakenet::await<BinanceSymbols>([](Callback<BinanceSymbols> cb) { binanceSymbolMap(nullptr, cb); });
  CHECK(r.ok() && r.value.map.contains("BTCUSDT"));
  int pings = 0, lists = 0;
  for (const auto& c : fakenet::calls()) {
    if (c.url.contains("/ping")) ++pings;
    if (c.url.contains("/exchangeInfo")) ++lists;
  }
  CHECK_EQ(pings, 3);
  CHECK_EQ(lists, 1);
  CHECK(anyCall("api.binance.com"));
  // known good now: the next request asks no ping
  fakenet::calls().clear();
  (void)candlesNow("BINANCE:BTCUSDT", "1D", 10);
  for (const auto& c : fakenet::calls()) CHECK(!c.url.contains("/ping"));
  extra = nullptr;
}

// Audit G11: Coinbase pages of 300 periods were asked until 1500 bars were found; a thin product (a few trades a page)
// walked back hundreds of pages.
TEST("providers: Coinbase pagination stops after a bounded number of pages") {
  setUp();
  extra = [](const QUrl& u) -> std::optional<fakenet::Reply> {
    if (u.host() == "api.exchange.coinbase.com" && u.path().endsWith("/candles")) {
      const double end = parseIsoMs(fakenet::param(u, "end")) / 1000;
      // one trade a page: [time, low, high, open, close, volume]
      return fakenet::Reply::json(QJsonArray{QJsonArray{std::floor(end / 60) * 60 - 60, 1, 2, 1.5, 1.5, 3}});
    }
    return std::nullopt;
  };
  (void)candlesNow("COINBASE:THIN-USD", "1m", 1500);
  int pages = 0;
  for (const auto& c : fakenet::calls()) if (c.url.contains("/candles")) ++pages;
  CHECK(pages > 0);
  CHECK(pages <= 11);   // 2 × the 5 full pages 1500 bars need, + 1
  extra = nullptr;
}
