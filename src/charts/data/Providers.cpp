// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/providers.js: market data from free, keyless public sources, each with fallbacks.
#include "charts/data/Providers.h"
#include "charts/core/Series.h"
#include "net/Sources.h"
#if __has_include("charts/data/Imports.h")
#include "charts/data/Imports.h"
#define CHARTS_HAS_IMPORTS 1
#endif
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <QTimeZone>
#include <QLocale>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <array>
#include <numeric>
#include <cmath>

namespace charts::data {

// ------------------------------------------------------------------------------------------------ sources
using sources::Access;
static const QStringList kPage = {"charts/charts"};
static const sources::Registrar regBinance({"binance", "Binance (spot market data API)", {"api.binance.com"}, Access::Keyless, 200,
    "Crypto spot candles (klines), 24h tickers, best bid/ask, symbol list; live klines over WebSocket",
    "Public market-data endpoints without key; Binance allows 6000 request weight per minute per IP (klines cost 2, "
    "the symbol list 20); we send at most 5 requests per second. Blocked in some countries (HTTP 451), then the "
    "mirror and the other exchanges take over", kPage});
static const sources::Registrar regBinanceVision({"binance-vision", "Binance (public market-data mirror)", {"data-api.binance.vision"},
    Access::Keyless, 200, "The same spot market data as api.binance.com, from Binance's public mirror",
    "Binance's mirror published for market data only, same limits as the main API; tried when api.binance.com "
    "doesn't answer", kPage});
static const sources::Registrar regBinanceUs({"binance-us", "Binance.US (market data API)", {"api.binance.us"}, Access::Keyless, 200,
    "Crypto spot candles and tickers from Binance.US (a separate exchange with its own prices): last resort",
    "Public market-data endpoints without key; 1200 request weight per minute per IP; we send at most 5 per second", kPage});
static const sources::Registrar regBinanceFut({"binance-futures", "Binance USD-M futures (market data API)", {"fapi.binance.com"},
    Access::Keyless, 200, "Crypto perpetual futures candles (BTCUSDT.P), best bid/ask, perpetual symbol list; live klines over WebSocket; "
    "open interest and long/short account ratio every 5 minutes, funding rates, live liquidation orders (liquidation heatmap)",
    "Public market-data endpoints without key; 2400 request weight per minute per IP; we send at most 5 per second. "
    "The statistics keep only 30 days (1000 requests per 5 minutes, weight 0): Deepwater stores them as they come", kPage});
static const sources::Registrar regBybit({"bybit", "Bybit (v5 market API)", {"api.bybit.com"}, Access::Keyless, 200,
    "Linear perpetual candles, the fallback for crypto perpetuals where Binance futures is blocked",
    "Public market endpoints without key; Bybit allows 600 requests per 5 s per IP; we send at most 5 per second", kPage});
static const sources::Registrar regCoinbase({"coinbase", "Coinbase Exchange (public API)", {"api.exchange.coinbase.com"},
    Access::Keyless, 350, "Crypto candles (300 per request), ticker bid/ask, product list",
    "Public endpoints without key; Coinbase allows 10 requests per second per IP; we send about 3", kPage});
static const sources::Registrar regKraken({"kraken", "Kraken (public REST API)", {"api.kraken.com"}, Access::Keyless, 1000,
    "Crypto and forex candles (last 720 bars), tickers, asset pairs",
    "Public endpoints without key; Kraken's public call limit is about one per second, which we keep", kPage});
static const sources::Registrar regNasdaq({"nasdaq-api", "Nasdaq (api.nasdaq.com, the data behind nasdaq.com)", {"api.nasdaq.com"},
    Access::Keyless, 1000,
    "US stocks, ETFs and indices: daily history (10 years), today's intraday points, live watchlist quotes, "
    "screener lists, symbol search",
    "Undocumented JSON endpoints that nasdaq.com's own pages call, without key; asked on the user's request only "
    "(no crawling), one request per second, with the Origin/Referer headers the API requires. Its robots.txt "
    "disallows all crawlers (to keep the endpoints out of search engines)", kPage});
static const sources::Registrar regYahoo({"yahoo", "Yahoo Finance", {"query1.finance.yahoo.com"}, Access::Keyless, 350,
    "Candles for global indices, futures, non-US stocks, forex, metals; symbol search",
    "Public endpoints without key, personal non-commercial display; data delayed for some exchanges", kPage});
static const sources::Registrar regFF({"forexfactory", "Forex Factory (Fair Economy)", {"nfs.faireconomy.media"}, Access::Keyless, 300000,
    "Weekly economic calendar: time, currency, impact, forecast, previous",
    "Public weekly calendar export offered by Forex Factory for third-party use; asks for low request rates "
    "(one download per 5 minutes at most; cached 30 min)", kPage});

// ------------------------------------------------------------------------------------------------ plumbing
namespace {
QString& fixturesDir() { static QString d; return d; }
bool offline() { return !fixturesDir().isEmpty(); }

const QStringList FIAT = {"USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD"};
// Spot metals, quoted like forex (XAUUSD). Yahoo's futures first; gold falls back to PAXG, a token
// backed one-to-one by gold, so the chart still loads when Yahoo is rate limiting.
struct Metal { QString ticker, name, word, yahoo, token; };
const QList<Metal>& metals() {
  static const QList<Metal> m = {{"XAUUSD", QT_TRANSLATE_NOOP("charts", "Gold / USD"), "GOLD", "GC=F", "PAXG"}, {"XAGUSD", QT_TRANSLATE_NOOP("charts", "Silver / USD"), "SILVER", "SI=F", {}}};
  return m;
}
const Metal* metal(const QString& t) {
  for (const auto& m : metals()) if (m.ticker == t) return &m;
  return nullptr;
}
const FuturesInfo* futuresOf(const QString& root) {
  for (const auto& f : futures()) if (f.root == root) return &f;
  return nullptr;
}
const QStringList MAJOR_CRYPTO = {"BTC", "ETH", "SOL", "XRP", "BNB", "DOGE", "ADA", "AVAX", "LINK", "DOT", "LTC",
                                  "BCH", "TRX", "XLM", "ATOM", "UNI", "NEAR", "APT", "ARB", "OP", "SUI", "PEPE",
                                  "SHIB", "AAVE", "ETC", "FIL", "HBAR", "ICP", "INJ", "TON", "XMR", "ZEC", "HYPE"};

// Compiled once per pattern (GUI thread only): the screeners test thousands of tickers.
const QRegularExpression& rx(const QString& p) {
  static QHash<QString, QRegularExpression> c;
  auto it = c.find(p);
  if (it == c.end()) it = c.insert(p, QRegularExpression(p, QRegularExpression::UseUnicodePropertiesOption));
  return *it;
}
bool test(const QString& pattern, const QString& s) { return rx(pattern).match(s).hasMatch(); }

template <class T> void failLater(const Callback<T>& cb, const Error& e) { later([cb, e] { cb(Result<T>::failure(e)); }); }

// The public API's promise semantics: the callback never runs inside the call, and is skipped once ctx is gone.
template <class T, class F>
void runAsync(QObject* ctx, Callback<T> cb, F&& body) {
  auto inCall = std::make_shared<bool>(true);
  Callback<T> g = guarded(ctx, std::move(cb));
  Callback<T> wrapped = [inCall, g](const Result<T>& r) {
    if (*inCall) later([g, r] { g(r); });
    else g(r);
  };
  body(wrapped);
  *inCall = false;
}

// JS unary plus on a JSON value.
double plus(const QJsonValue& v) {
  if (v.isDouble()) return v.toDouble();
  if (v.isNull()) return 0;
  if (v.isBool()) return v.toBool() ? 1 : 0;
  if (v.isString()) {
    const QString s = v.toString().trimmed();
    if (s.isEmpty()) return 0;
    bool ok = false;
    const double d = s.toDouble(&ok);
    return ok ? d : NaN;
  }
  return NaN;
}
// JS truthiness of a JSON value.
bool truthy(const QJsonValue& v) {
  if (v.isObject() || v.isArray()) return true;
  if (v.isString()) return !v.toString().isEmpty();
  if (v.isDouble()) return v.toDouble() != 0 && !std::isnan(v.toDouble());
  if (v.isBool()) return v.toBool();
  return false;
}
QString jsString(const QJsonValue& v) {
  if (v.isString()) return v.toString();
  if (v.isDouble()) return series::numStr(v.toDouble());
  return {};
}
// `a || b`: the first truthy string.
QString orStr(std::initializer_list<QJsonValue> vs, const QString& def = {}) {
  for (const auto& v : vs) if (truthy(v)) return jsString(v);
  return def;
}

QMap<QString, HealthEntry>& healthMap() { static QMap<QString, HealthEntry> h; return h; }

QJsonDocument fixture(const QString& name) {
  QFile f(fixturesDir() + "/" + name);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(f.readAll());
}
Bars barsOf(const QJsonArray& a) {
  Bars out;
  out.reserve(a.size());
  for (const auto& v : a) {
    const QJsonObject o = v.toObject();
    out.push_back(Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(NaN), o.value("high").toDouble(NaN),
                      o.value("low").toDouble(NaN), o.value("close").toDouble(NaN), o.value("volume").toDouble(0)});
  }
  return out;
}
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
int ratioOf(const QString& tf, const QString& base) { return std::max(1, int(std::lround(tfSeconds(tf) / tfSeconds(base)))); }
Bars finishTf(const Bars& out, const QString& base, const QString& tf, int limit) {
  return base == tf ? sliceLast(out, limit) : sliceLast(resampleTo(out, tf), limit);
}
}  // namespace

const QList<FuturesInfo>& futures() {
  // The public chart is Yahoo's, about 10 minutes behind; your own Polygon key (feeds.json) charts the live contract.
  static const QList<FuturesInfo> f = {{"NQ", QT_TRANSLATE_NOOP("charts", "E-mini Nasdaq-100 futures"), "NQ=F"},
                                       {"MNQ", QT_TRANSLATE_NOOP("charts", "Micro E-mini Nasdaq-100 futures"), "MNQ=F"},
                                       {"ES", QT_TRANSLATE_NOOP("charts", "E-mini S&P 500 futures"), "ES=F"},
                                       {"MES", QT_TRANSLATE_NOOP("charts", "Micro E-mini S&P 500 futures"), "MES=F"}};
  return f;
}

const QMap<QString, HealthEntry>& health() { return healthMap(); }
QJsonObject healthJson() {
  QJsonObject o;
  for (auto it = healthMap().cbegin(); it != healthMap().cend(); ++it) {
    QJsonObject e{{"ok", it->ok}, {"at", it->at}};
    if (!it->error.isEmpty()) e["error"] = it->error;
    o[it.key()] = e;
  }
  return o;
}
void mark(const QString& name, bool ok, const Error* err) {
  healthMap()[name] = HealthEntry{ok, isoNow(), err ? err->message.left(200) : QString()};
}

Headers nqHeaders() { return {{"Origin", "https://www.nasdaq.com"}, {"Referer", "https://www.nasdaq.com/"}}; }

// ------------------------------------------------------------------------------------------------ JSON shapes
QJsonObject Symbol::toJson() const { return {{"id", id}, {"exchange", exchange}, {"ticker", ticker}}; }

static QJsonValue numJson(double v) { return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null); }
static QJsonArray candlesJson(const Bars& bars) {
  QJsonArray a;
  for (const Bar& b : bars)
    a.append(QJsonObject{{"time", double(b.time)}, {"open", numJson(b.open)}, {"high", numJson(b.high)}, {"low", numJson(b.low)},
                         {"close", numJson(b.close)}, {"volume", numJson(b.volume)}});
  return a;
}
QJsonObject CandlesResult::toJson() const {
  return {{"symbol", symbol}, {"tf", tf}, {"provider", provider}, {"candles", candlesJson(candles)}, {"notes", QJsonArray::fromStringList(notes)}};
}
QJsonObject Quote::toJson() const {
  if (!error.isEmpty()) return {{"error", error}};
  QJsonObject o{{"price", numJson(price)}, {"changePct", numJson(changePct)}, {"volume", numJson(volume)}};
  if (hasStatus) o["status"] = status;
  return o;
}
QJsonObject quotesToJson(const Quotes& q) {
  QJsonObject o;
  for (auto it = q.cbegin(); it != q.cend(); ++it) o[it.key()] = it->toJson();
  return o;
}
QJsonObject SpreadQuote::toJson() const {
  return {{"symbol", symbol}, {"bid", bid}, {"ask", ask}, {"spreadPct", spreadPct}, {"source", source}};
}
QJsonObject SearchHit::toJson() const { return {{"symbol", symbol}, {"name", name}, {"type", type}, {"exchange", exchange}}; }
QJsonArray hitsToJson(const QList<SearchHit>& hits) {
  QJsonArray a;
  for (const auto& h : hits) a.append(h.toJson());
  return a;
}
QJsonObject ScreenerRow::toJson() const {
  switch (kind) {
    case Raw: return raw;
    case Stock:
      return {{"ticker", ticker}, {"symbol", symbol}, {"name", name}, {"price", numJson(price)}, {"changePct", numJson(changePct)},
              {"volume", numJson(volume)}, {"marketCap", numJson(marketCap)}, {"sector", sector}, {"industry", industry},
              {"country", country}, {"live", live}};
    case Crypto:
      return {{"ticker", ticker}, {"symbol", symbol}, {"name", name}, {"price", numJson(price)}, {"changePct", numJson(changePct)},
              {"volume", numJson(volume)}, {"high", numJson(high)}, {"low", numJson(low)}, {"sector", sector}};
    case CryptoNoRange:
      return {{"ticker", ticker}, {"symbol", symbol}, {"name", name}, {"price", numJson(price)}, {"changePct", numJson(changePct)},
              {"volume", numJson(volume)}, {"sector", sector}};
  }
  return {};
}
QJsonArray rowsToJson(const QList<ScreenerRow>& rows) {
  QJsonArray a;
  for (const auto& r : rows) a.append(r.toJson());
  return a;
}
QJsonObject CalendarEvent::toJson() const {
  QJsonObject o;
  // JSON.stringify leaves out undefined fields
  auto put = [&o](const char* k, const QJsonValue& v) { if (!v.isUndefined()) o[k] = v; };
  put("title", title); put("country", country); put("date", date); put("impact", impact); put("forecast", forecast); put("previous", previous);
  return o;
}
QJsonArray eventsToJson(const QList<CalendarEvent>& e) {
  QJsonArray a;
  for (const auto& x : e) a.append(x.toJson());
  return a;
}
QJsonObject Probe::toJson() const {
  if (ok) return {{"ok", true}, {"ms", double(ms)}};
  return {{"ok", false}, {"error", error}};
}
QJsonObject probesToJson(const Probes& p) {
  QJsonObject o;
  for (auto it = p.cbegin(); it != p.cend(); ++it) o[it.key()] = it->toJson();
  return o;
}

// ------------------------------------------------------------------------------------------------ Binance
namespace {
const QStringList BINANCE_HOSTS = {"https://api.binance.com", "https://data-api.binance.vision", "https://api.binance.us"};
constexpr int kBinanceUs = 2;
int binanceHostIdx = 0;          // the global host (0 or 1) that answered last: never Binance.US
bool binanceKnownGood = false;

// Which hosts a request may use. Binance.US is a separate exchange (its own prices, fewer pairs): it stays a named last
// resort, never the preferred host (audit D19: after one failure of the two global hosts it became sticky, and charts
// showed its prices under the name "Binance", merged with global live klines).
enum class BinanceHosts { All, Global, Us };

// Audit G11: while no host was known good (every launch), each request went to the three hosts at once, so the ~10 MB
// symbol list was downloaded, parsed and cached three times. Now a small request (/api/v3/ping, weight 1) goes to the
// three first; the requests made meanwhile wait for it, then go to one host at a time in order, skipping those the
// probe found unreachable. A probe that finds none still lets them race as before (its answers are errors, small).
struct BinanceProbe {
  bool running = false;
  qint64 at = 0;          // when the last probe ended (ms since 1970), 0 = never
  bool ok[3] = {};
  std::vector<std::function<void()>> waiting;
};
BinanceProbe& binanceProbe() { static BinanceProbe p; return p; }
constexpr qint64 kProbeValidMs = 60000;

void releaseProbeWaiters() {
  auto w = std::move(binanceProbe().waiting);
  binanceProbe().waiting.clear();
  for (auto& f : w) f();
}

void probeBinance(std::function<void()> then) {
  BinanceProbe& pr = binanceProbe();
  pr.waiting.push_back(std::move(then));
  if (pr.running) return;
  pr.running = true;
  for (bool& b : pr.ok) b = false;
  auto left = std::make_shared<int>(int(BINANCE_HOSTS.size()));
  for (int i = 0; i < BINANCE_HOSTS.size(); ++i)
    getJson(BINANCE_HOSTS[i] + "/api/v3/ping", {}, [i, left](const Result<QJsonDocument>& r) {
      BinanceProbe& p = binanceProbe();
      p.ok[i] = r.ok();
      // the first global host to answer is the preferred one: the waiting requests go now, without the slow ones
      if (r.ok() && i != kBinanceUs && !binanceKnownGood) {
        binanceHostIdx = i;
        binanceKnownGood = true;
        releaseProbeWaiters();
      }
      if (--*left == 0) {
        p.running = false;
        p.at = QDateTime::currentMSecsSinceEpoch();
        releaseProbeWaiters();
      }
    }, GetOpts{0, false});
}

void binanceSend(const QString& p, Callback<QJsonDocument> cb, const GetOpts& o, BinanceHosts which);

void binanceGet(const QString& p, Callback<QJsonDocument> cb, const GetOpts& o = {}, BinanceHosts which = BinanceHosts::All) {
  const BinanceProbe& pr = binanceProbe();
  if (!binanceKnownGood && which != BinanceHosts::Us &&
      (pr.running || !pr.at || QDateTime::currentMSecsSinceEpoch() - pr.at >= kProbeValidMs)) {
    probeBinance([p, cb, o, which] { binanceSend(p, cb, o, which); });
    return;
  }
  binanceSend(p, cb, o, which);
}

void binanceSend(const QString& p, Callback<QJsonDocument> cb, const GetOpts& o, BinanceHosts which) {
  QList<int> order;
  if (which != BinanceHosts::Us) order << binanceHostIdx << 1 - binanceHostIdx;
  if (which != BinanceHosts::Global) order << kBinanceUs;
  // no host known good: the hosts the last probe reached, in the same order (all of them, racing, if it reached none)
  bool race = false;
  if (!binanceKnownGood && order.size() > 1) {
    QList<int> reached;
    for (int i : order) if (binanceProbe().ok[i]) reached << i;
    if (reached.isEmpty()) race = true;
    else order = reached;
  }
  const int n = order.size();
  struct St {
    QList<int> order;
    QVector<std::optional<Result<QJsonDocument>>> early;
    int k = 0;
    bool done = false;
    Error lastErr;
    Callback<QJsonDocument> cb;
    GetOpts o;
    QString p;
  };
  auto st = std::make_shared<St>();
  st->order = order;
  st->cb = std::move(cb);
  st->o = o;
  st->p = p;
  auto finishOk = [](const std::shared_ptr<St>& st, int idx, const Result<QJsonDocument>& r) {
    st->done = true;
    if (idx != kBinanceUs) {   // only a global host becomes the preferred one
      binanceHostIdx = idx;
      binanceKnownGood = true;
    }
    mark("binance", true);
    st->cb(r);
  };
  auto finishFail = [which](const std::shared_ptr<St>& st) {
    st->done = true;
    if (which != BinanceHosts::Us) binanceKnownGood = false;
    mark("binance", false, &st->lastErr);
    st->cb(Result<QJsonDocument>::failure(st->lastErr));
  };
  // 400 = bad symbol on the global hosts (don't hop hosts for that); from Binance.US it only means the pair isn't
  // listed there, which is the end of the list anyway. 451/403 = blocked in your country: the next host.
  auto badRequest = [](int idx, const Result<QJsonDocument>& r) { return idx != kBinanceUs && r.error->status == 400; };
  // The probe reached no host: ask all of them at once but still prefer them in order (Binance.US last). A firewall
  // that silently drops Binance then costs one timeout, not three (the dropped hosts are skipped for a minute).
  if (race) {
    st->early.resize(n);
    auto advance = std::make_shared<std::function<void()>>();
    *advance = [st, finishOk, finishFail, badRequest, n]() {
      while (!st->done && st->k < n) {
        if (!st->early[st->k]) return;   // wait for this host's answer
        const Result<QJsonDocument>& r = *st->early[st->k];
        if (r.ok()) { finishOk(st, st->order[st->k], r); return; }
        st->lastErr = *r.error;
        if (badRequest(st->order[st->k], r)) { st->done = true; st->cb(r); return; }
        st->k++;
      }
      if (!st->done) finishFail(st);
    };
    for (int k = 0; k < n; k++)
      getJson(BINANCE_HOSTS[order[k]] + p, {}, [st, k, advance](const Result<QJsonDocument>& r) {
        st->early[k] = r;
        if (!st->done) (*advance)();
      }, o);
    return;
  }
  auto next = std::make_shared<std::function<void()>>();
  *next = [st, next, finishOk, finishFail, badRequest, n]() {
    if (st->k >= n) { finishFail(st); *next = nullptr; return; }
    const int idx = st->order[st->k];
    getJson(BINANCE_HOSTS[idx] + st->p, {}, [st, next, idx, finishOk, badRequest](const Result<QJsonDocument>& r) {
      if (r.ok()) { finishOk(st, idx, r); *next = nullptr; return; }
      st->lastErr = *r.error;
      if (badRequest(idx, r)) { st->done = true; st->cb(r); *next = nullptr; return; }
      st->k++;
      (*next)();
    }, st->o);
  };
  (*next)();
}

const QStringList BINANCE_NATIVE = {"1s", "1m", "3m", "5m", "15m", "30m", "1h", "2h", "4h", "6h", "8h", "12h", "1D", "3D", "1W", "1M"};
QString binanceInterval(const QString& tf) {
  if (tf == "1D") return "1d";
  if (tf == "3D") return "3d";
  if (tf == "1W") return "1w";
  return tf;
}

void binanceFuturesGet(const QString& p, Callback<QJsonDocument> cb, const GetOpts& o = {}) {
  getJson("https://fapi.binance.com" + p, {}, [cb](const Result<QJsonDocument>& r) {
    if (r.ok()) mark("binance-futures", true);
    else mark("binance-futures", false, &*r.error);
    cb(r);
  }, o);
}

void binanceCandles(const QString& ticker, const QString& tf, int limit, bool futures, Callback<Bars> cb,
                    BinanceHosts hosts = BinanceHosts::Global) {
  const QString base = nativeBaseOf(tf, futures ? BINANCE_NATIVE.mid(1) : BINANCE_NATIVE);
  if (base.isEmpty()) { failLater(cb, Error{"Binance can't build a " + tf + " chart"}); return; }
  const int ratio = ratioOf(tf, base);
  const int want = std::min(limit * ratio + ratio, 20000);
  struct St { Bars out; qint64 endTime = -1; };
  auto st = std::make_shared<St>();
  auto step = std::make_shared<std::function<void()>>();
  *step = [=]() {
    auto finish = [=] { cb(Result<Bars>::success(finishTf(st->out, base, tf, limit))); *step = nullptr; };
    if (st->out.size() >= want) { finish(); return; }
    const int n = std::min(1000, int(want - st->out.size()));
    const QString q = "/klines?symbol=" + encodeUriComponent(ticker) + "&interval=" + binanceInterval(base) + "&limit=" +
                      QString::number(n) + (st->endTime >= 0 ? "&endTime=" + QString::number(st->endTime) : QString());
    auto got = [=](const Result<QJsonDocument>& r) {
      if (!r.ok()) { cb(Result<Bars>::failure(*r.error)); *step = nullptr; return; }
      const QJsonArray rows = r.value.array();
      if (rows.isEmpty()) { finish(); return; }
      Bars page;
      page.reserve(rows.size());
      for (const auto& v : rows) {
        const QJsonArray k = v.toArray();
        if (!std::isfinite(plus(k.at(0)))) continue;   // a row without a usable time (qint64 of NaN is undefined)
        page.push_back(Bar{clampToInt64(plus(k.at(0)) / 1000), plus(k.at(1)), plus(k.at(2)), plus(k.at(3)), plus(k.at(4)), plus(k.at(5))});
      }
      st->out = page + st->out;
      st->endTime = clampToInt64(plus(rows.at(0).toArray().at(0))) - 1;
      if (rows.size() < n) { finish(); return; }
      (*step)();
    };
    if (futures) binanceFuturesGet("/fapi/v1" + q, got);
    else binanceGet("/api/v3" + q, got, {}, hosts);
  };
  (*step)();
}

std::optional<BinanceSymbols>& lastBinanceSymbols() { static std::optional<BinanceSymbols> s; return s; }

void binanceSymbolMapI(Callback<BinanceSymbols> cb) {
  if (offline()) {
    BinanceSymbols s;
    s.order = {"BTCUSDT", "ETHUSDT"};
    s.map = {{"BTCUSDT", {"BTC", "USDT"}}, {"ETHUSDT", {"ETH", "USDT"}}};
    cb(Result<BinanceSymbols>::success(s));
    return;
  }
  cached<BinanceSymbols>(
      "binance:symbols", 6 * 3600000LL,
      [](Callback<BinanceSymbols> done) {
        // The symbol list is large (~10 MB); the disk cache keeps it across restarts for the same 6 hours.
        binanceGet("/api/v3/exchangeInfo", [done](const Result<QJsonDocument>& r) {
          if (!r.ok()) { done(Result<BinanceSymbols>::failure(*r.error)); return; }
          BinanceSymbols s;
          for (const auto& v : r.value.object().value("symbols").toArray()) {
            const QJsonObject o = v.toObject();
            if (o.value("status").toString() != "TRADING") continue;
            const QString t = o.value("symbol").toString();
            if (!s.map.contains(t)) s.order << t;
            s.map.insert(t, BinancePair{o.value("baseAsset").toString(), o.value("quoteAsset").toString()});
          }
          lastBinanceSymbols() = s;
          done(Result<BinanceSymbols>::success(s));
        }, GetOpts{6 * 3600, false});
      },
      [cb](const Result<BinanceSymbols>& r) {
        cb(Result<BinanceSymbols>::success(r.ok() ? r.value : lastBinanceSymbols().value_or(BinanceSymbols{})));
      });
}

void perpSymbolsI(Callback<QSet<QString>> cb) {
  if (offline()) { cb(Result<QSet<QString>>::success({"BTCUSDT", "ETHUSDT"})); return; }
  cached<QSet<QString>>(
      "binance:perps", 6 * 3600000LL,
      [](Callback<QSet<QString>> done) {
        binanceFuturesGet("/fapi/v1/exchangeInfo", [done](const Result<QJsonDocument>& r) {
          if (!r.ok()) { done(Result<QSet<QString>>::failure(*r.error)); return; }
          QSet<QString> s;
          for (const auto& v : r.value.object().value("symbols").toArray()) {
            const QJsonObject o = v.toObject();
            if (o.value("contractType").toString() == "PERPETUAL" && o.value("status").toString() == "TRADING")
              s.insert(o.value("symbol").toString());
          }
          done(Result<QSet<QString>>::success(s));
        }, GetOpts{6 * 3600, false});
      },
      [cb](const Result<QSet<QString>>& r) { cb(Result<QSet<QString>>::success(r.ok() ? r.value : QSet<QString>())); });
}

// ------------------------------------------------------------------------------------------------ Bybit
const QList<QPair<QString, QString>> BYBIT_NATIVE = {{"1m", "1"}, {"3m", "3"}, {"5m", "5"}, {"15m", "15"}, {"30m", "30"}, {"1h", "60"},
                                                     {"2h", "120"}, {"4h", "240"}, {"6h", "360"}, {"12h", "720"}, {"1D", "D"}, {"1W", "W"}};
template <class V> QStringList keysOf(const QList<QPair<QString, V>>& l) { QStringList k; for (const auto& p : l) k << p.first; return k; }
template <class V> V valueOf(const QList<QPair<QString, V>>& l, const QString& key) { for (const auto& p : l) if (p.first == key) return p.second; return V{}; }

void bybitCandles(const QString& ticker, const QString& tf, int limit, Callback<Bars> cb) {
  const QString base = nativeBaseOf(tf, keysOf(BYBIT_NATIVE));
  if (base.isEmpty()) { failLater(cb, Error{"Bybit can't build a " + tf + " chart"}); return; }
  const int ratio = ratioOf(tf, base);
  const int want = std::min(limit * ratio + ratio, 20000);
  struct St { Bars out; qint64 end = -1; };
  auto st = std::make_shared<St>();
  auto step = std::make_shared<std::function<void()>>();
  *step = [=]() {
    auto finish = [=] { cb(Result<Bars>::success(finishTf(dedupe(st->out), base, tf, limit))); *step = nullptr; };
    if (st->out.size() >= want) { finish(); return; }
    const int n = std::min(1000, int(want - st->out.size()));
    getJson("https://api.bybit.com/v5/market/kline?category=linear&symbol=" + encodeUriComponent(ticker) + "&interval=" +
                valueOf(BYBIT_NATIVE, base) + "&limit=" + QString::number(n) + (st->end >= 0 ? "&end=" + QString::number(st->end) : QString()),
            {}, [=](const Result<QJsonDocument>& r) {
              if (!r.ok()) { cb(Result<Bars>::failure(*r.error)); *step = nullptr; return; }
              const QJsonObject j = r.value.object();
              if (truthy(j.value("retCode"))) { cb(Result<Bars>::failure(Error{"Bybit: " + jsString(j.value("retMsg"))})); *step = nullptr; return; }
              const QJsonArray rows = j.value("result").toObject().value("list").toArray();
              if (rows.isEmpty()) { finish(); return; }
              Bars page;
              for (int i = rows.size() - 1; i >= 0; --i) {   // newest first: reversed
                const QJsonArray k = rows.at(i).toArray();
                if (!std::isfinite(plus(k.at(0)))) continue;   // a row without a usable time (qint64 of NaN is undefined)
                page.push_back(Bar{clampToInt64(plus(k.at(0)) / 1000), plus(k.at(1)), plus(k.at(2)), plus(k.at(3)), plus(k.at(4)), plus(k.at(5))});
              }
              st->out = page + st->out;
              st->end = clampToInt64(plus(rows.last().toArray().at(0))) - 1;
              if (rows.size() < n) { finish(); return; }
              (*step)();
            });
  };
  (*step)();
}

// ------------------------------------------------------------------------------------------------ Coinbase
const QList<QPair<QString, int>> COINBASE_NATIVE = {{"1m", 60}, {"5m", 300}, {"15m", 900}, {"1h", 3600}, {"6h", 21600}, {"1D", 86400}};

void coinbaseCandles(const QString& product, const QString& tf, int limit, Callback<Bars> cb) {
  const QString base = nativeBaseOf(tf, keysOf(COINBASE_NATIVE));
  if (base.isEmpty()) { failLater(cb, Error{"Coinbase can't build a " + tf + " chart"}); return; }
  const int g = valueOf(COINBASE_NATIVE, base);
  const int ratio = std::max(1, int(std::lround(tfSeconds(tf) / g)));
  const int want = std::min(limit * ratio, 1500);
  // Pages of 300 periods: a full history needs want / 300 of them. A thinly traded product fills few periods a page
  // and walked back page after page to its first trade (hundreds of requests, audit G11): twice the full count, then
  // what was found is the chart.
  const int maxPages = 2 * ((want + 299) / 300) + 1;
  struct St { Bars out; qint64 end; int pages = 0; };
  auto st = std::make_shared<St>(St{{}, nowMs() / 1000});
  auto step = std::make_shared<std::function<void()>>();
  *step = [=]() {
    auto finish = [=] {
      mark("coinbase", true);
      cb(Result<Bars>::success(finishTf(dedupe(st->out), base, tf, limit)));
      *step = nullptr;
    };
    if (st->out.size() >= want || st->pages >= maxPages) { finish(); return; }
    ++st->pages;
    const qint64 start = st->end - 300LL * g;
    getJson("https://api.exchange.coinbase.com/products/" + encodeUriComponent(product) + "/candles?granularity=" + QString::number(g) +
                "&start=" + isoMs(start * 1000) + "&end=" + isoMs(st->end * 1000),
            {}, [=](const Result<QJsonDocument>& r) {
              if (!r.ok()) { mark("coinbase", false, &*r.error); cb(Result<Bars>::failure(*r.error)); *step = nullptr; return; }
              const QJsonArray rows = r.value.array();
              if (rows.isEmpty()) { finish(); return; }
              Bars page;
              for (int i = rows.size() - 1; i >= 0; --i) {
                const QJsonArray x = rows.at(i).toArray();
                page.push_back(Bar{qint64(x.at(0).toDouble()), x.at(3).toDouble(NaN), x.at(2).toDouble(NaN), x.at(1).toDouble(NaN),
                                   x.at(4).toDouble(NaN), x.at(5).toDouble(NaN)});
              }
              st->out = page + st->out;
              st->end = start;
              (*step)();
            });
  };
  (*step)();
}

// Product lists for search. An exchange that doesn't answer just drops out of the results.
void coinbaseProducts(Callback<QSet<QString>> cb) {
  cached<QSet<QString>>(
      "coinbase:products", 6 * 3600000LL,
      [](Callback<QSet<QString>> done) {
        getJson("https://api.exchange.coinbase.com/products", {}, [done](const Result<QJsonDocument>& r) {
          if (!r.ok()) { done(Result<QSet<QString>>::failure(*r.error)); return; }
          QSet<QString> s;
          for (const auto& v : r.value.array())
            if (v.toObject().value("status").toString() == "online") s.insert(v.toObject().value("id").toString());
          done(Result<QSet<QString>>::success(s));
        });
      },
      [cb](const Result<QSet<QString>>& r) { cb(Result<QSet<QString>>::success(r.ok() ? r.value : QSet<QString>())); });
}

// ------------------------------------------------------------------------------------------------ Kraken
const QList<QPair<QString, int>> KRAKEN_NATIVE = {{"1m", 1}, {"5m", 5}, {"15m", 15}, {"30m", 30}, {"1h", 60}, {"4h", 240}, {"1D", 1440}};
QString krakenAsset(const QString& a) { return a == "BTC" ? "XBT" : a; }
QString krakenErrors(const QJsonObject& j) {
  QStringList e;
  for (const auto& v : j.value("error").toArray()) e << jsString(v);
  return e.join(", ");
}

void krakenPairs(Callback<QHash<QString, QString>> cb) {
  cached<QHash<QString, QString>>(
      "kraken:pairs", 6 * 3600000LL,
      [](Callback<QHash<QString, QString>> done) {
        getJson("https://api.kraken.com/0/public/AssetPairs", {}, [done](const Result<QJsonDocument>& r) {
          if (!r.ok()) { done(Result<QHash<QString, QString>>::failure(*r.error)); return; }
          const QJsonObject j = r.value.object();
          if (!j.value("error").toArray().isEmpty()) { done(Result<QHash<QString, QString>>::failure(Error{krakenErrors(j)})); return; }
          done(Result<QHash<QString, QString>>::success(krakenPairMap(j.value("result").toObject())));
        });
      },
      [cb](const Result<QHash<QString, QString>>& r) {
        cb(Result<QHash<QString, QString>>::success(r.ok() ? r.value : QHash<QString, QString>()));
      });
}

void krakenCandles(const QString& pair, const QString& tf, int limit, Callback<Bars> cb) {
  const QString base = nativeBaseOf(tf, keysOf(KRAKEN_NATIVE));
  if (base.isEmpty()) { failLater(cb, Error{"Kraken can't build a " + tf + " chart"}); return; }
  getJson("https://api.kraken.com/0/public/OHLC?pair=" + encodeUriComponent(pair) + "&interval=" + QString::number(valueOf(KRAKEN_NATIVE, base)),
          {}, [=](const Result<QJsonDocument>& r) {
            auto fail = [&](const Error& e) { mark("kraken", false, &e); cb(Result<Bars>::failure(e)); };
            if (!r.ok()) { fail(*r.error); return; }
            const QJsonObject j = r.value.object();
            if (!j.value("error").toArray().isEmpty()) { fail(Error{"Kraken: " + krakenErrors(j)}); return; }
            const QJsonObject res = j.value("result").toObject();
            QString key;
            for (const QString& k : res.keys()) if (k != "last") { key = k; break; }
            if (key.isEmpty()) { fail(Error{"Cannot read properties of undefined (reading 'map')"}); return; }
            Bars out;
            for (const auto& v : res.value(key).toArray()) {
              const QJsonArray x = v.toArray();
              out.push_back(Bar{qint64(x.at(0).toDouble()), plus(x.at(1)), plus(x.at(2)), plus(x.at(3)), plus(x.at(4)), plus(x.at(6))});
            }
            mark("kraken", true);
            cb(Result<Bars>::success(finishTf(out, base, tf, limit)));
          });
}

// ------------------------------------------------------------------------------------------------ Yahoo
qint64 yahooDownUntil = 0;
const QList<QPair<QString, QPair<QString, QString>>> YAHOO_NATIVE = {
    {"1m", {"1m", "7d"}},   {"2m", {"2m", "60d"}},     {"5m", {"5m", "60d"}}, {"15m", {"15m", "60d"}}, {"30m", {"30m", "60d"}},
    {"1h", {"60m", "730d"}}, {"1D", {"1d", "20y"}},     {"1W", {"1wk", "max"}}, {"1M", {"1mo", "max"}}};

void yahooCandles(const QString& symbol, const QString& tf, int limit, Callback<Bars> cb) {
  if (!yahooAvailable()) { failLater(cb, Error{QT_TRANSLATE_NOOP("charts", "Yahoo is rate limiting this connection (retrying in a few minutes)")}); return; }
  const QString base = nativeBaseOf(tf, keysOf(YAHOO_NATIVE));
  if (base.isEmpty()) { failLater(cb, Error{"Yahoo can't build a " + tf + " chart"}); return; }
  const auto [interval, range] = valueOf(YAHOO_NATIVE, base);
  // The JS sends "User-Agent: Mozilla/5.0" (Yahoo answers 429 to full browser agents); our own agent works too
  // (market/QuoteHub uses it), so no special header.
  getJson("https://query1.finance.yahoo.com/v8/finance/chart/" + encodeUriComponent(symbol) + "?interval=" + interval + "&range=" + range +
              "&includePrePost=false",
          {}, [=](const Result<QJsonDocument>& r) {
            if (!r.ok()) {
              if (r.error->status == 429) yahooDownUntil = nowMs() + 10 * 60000;
              mark("yahoo", false, &*r.error);
              cb(Result<Bars>::failure(*r.error));
              return;
            }
            const Result<Bars> out = parseYahoo(r.value.object(), base);
            if (!out.ok()) { cb(out); return; }
            mark("yahoo", true);
            cb(Result<Bars>::success(finishTf(out.value, base, tf, limit)));
          });
}

// ------------------------------------------------------------------------------------------------ Nasdaq
QHash<QString, QString>& nqClass() { static QHash<QString, QString> m; return m; }
// Nasdaq's numbers come as "$1,234.50", "12.3%"; anything unreadable is 0.
double nqNum(const QJsonValue& s) {
  if (s.isNull() || s.isUndefined()) return 0;
  QString str = jsString(s);
  if (s.isBool()) str = s.toBool() ? "true" : "false";
  static const QRegularExpression strip(R"([$,%\s])");
  str.remove(strip);
  static const QRegularExpression lead(R"(^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?)");
  const auto m = lead.match(str);
  if (!m.hasMatch()) return 0;
  const double v = m.captured(0).toDouble();
  return std::isfinite(v) ? v : 0;
}

struct NqAnswer { QJsonObject j; QString cls; };
void nasdaqQuery(const QString& ticker, std::function<QString(const QString&)> pathFn, Callback<NqAnswer> cb) {
  QStringList classes;
  for (const QString& c : {nqClass().value(ticker), QString("stocks"), QString("etf"), QString("index")})
    if (!c.isEmpty() && !classes.contains(c)) classes << c;
  struct St { QStringList classes; int i = 0; std::optional<Error> lastErr; };
  auto st = std::make_shared<St>(St{classes, 0, std::nullopt});
  auto step = std::make_shared<std::function<void()>>();
  *step = [=]() {
    if (st->i >= st->classes.size()) {
      const bool hasStatus = st->lastErr && st->lastErr->status;
      mark("nasdaq", !hasStatus, hasStatus ? &*st->lastErr : nullptr);
      cb(Result<NqAnswer>::failure(st->lastErr.value_or(Error{"Nasdaq has no data for " + ticker})));
      *step = nullptr;
      return;
    }
    const QString cls = st->classes[st->i++];
    getJson(pathFn(cls), nqHeaders(), [=](const Result<QJsonDocument>& r) {
      if (r.ok()) {
        const QJsonObject j = r.value.object();
        if (truthy(j.value("data"))) {
          nqClass().insert(ticker, cls);
          mark("nasdaq", true);
          cb(Result<NqAnswer>::success(NqAnswer{j, cls}));
          *step = nullptr;
          return;
        }
        const QJsonValue msg = j.value("status").toObject().value("bCodeMessage").toArray().at(0).toObject().value("errorMessage");
        st->lastErr = Error{truthy(msg) ? jsString(msg) : "Nasdaq has no data for " + ticker};
      } else {
        st->lastErr = *r.error;
      }
      (*step)();
    });
  };
  (*step)();
}

struct NqToday { QVector<QPair<double, double>> pts; double volume = 0; };   // (x ms, y)
void nasdaqToday(const QString& ticker, Callback<NqToday> cb) {
  nasdaqQuery(ticker, [ticker](const QString& cls) {
    return "https://api.nasdaq.com/api/quote/" + encodeUriComponent(ticker) + "/chart?assetclass=" + cls;
  }, [cb](const Result<NqAnswer>& r) {
    if (!r.ok()) { cb(Result<NqToday>::failure(*r.error)); return; }
    const QJsonObject data = r.value.j.value("data").toObject();
    NqToday t;
    for (const auto& v : data.value("chart").toArray()) {
      const QJsonObject p = v.toObject();
      const double x = p.value("x").toDouble(NaN);
      const QDateTime d = QDateTime::fromMSecsSinceEpoch(clampToInt64(x), QTimeZone::UTC);
      const int mins = d.time().hour() * 60 + d.time().minute();
      if (std::isfinite(x) && mins >= 570 && mins < 960) t.pts.push_back({x, p.value("y").toDouble(NaN)});   // 09:30-16:00 New York
    }
    t.volume = nqNum(data.value("volume"));
    cb(Result<NqToday>::success(t));
  });
}

void nasdaqCandles(const QString& ticker, const QString& tf, int limit, Callback<Bars> cb) {
  if (intraday(tf)) {
    nasdaqToday(ticker, [=](const Result<NqToday>& r) {
      if (!r.ok()) { cb(Result<Bars>::failure(*r.error)); return; }
      Bars minute;
      for (const auto& [x, y] : r.value.pts) {
        const qint64 t = qint64(std::floor(double(nyWallToUtc(qint64(x))) / 1000 / 60)) * 60;
        minute.push_back(Bar{t, y, y, y, y, 0});
      }
      cb(Result<Bars>::success(sliceLast(resampleTo(dedupe(minute), tf), limit)));
    });
    return;
  }
  const qint64 to = nowMs();
  const qint64 from = to - 3653LL * 86400000;   // Nasdaq serves up to ~10 years
  nasdaqQuery(ticker, [=](const QString& cls) {
    return "https://api.nasdaq.com/api/quote/" + encodeUriComponent(ticker) + "/historical?assetclass=" + cls + "&fromdate=" + isoDate(from) +
           "&limit=9999&todate=" + isoDate(to);
  }, [=](const Result<NqAnswer>& r) {
    if (!r.ok()) { cb(Result<Bars>::failure(*r.error)); return; }
    const QJsonArray rows = r.value.j.value("data").toObject().value("tradesTable").toObject().value("rows").toArray();
    Bars daily;
    for (int i = rows.size() - 1; i >= 0; --i) {
      const QJsonObject row = rows.at(i).toObject();
      const QStringList mdy = row.value("date").toString().split('/');
      if (mdy.size() < 3) continue;
      const qint64 time = series::utcSeconds(mdy[2].toInt(), mdy[0].toInt() - 1, mdy[1].toInt());
      Bar b{time, nqNum(row.value("open")), nqNum(row.value("high")), nqNum(row.value("low")), nqNum(row.value("close")), nqNum(row.value("volume"))};
      if (!(b.close > 0)) continue;
      if (!b.open) { b.open = b.close; b.high = b.high ? b.high : b.close; b.low = b.low ? b.low : b.close; }
      daily.push_back(b);
    }
    auto done = [=](Bars daily) { cb(Result<Bars>::success(tf == "1D" ? sliceLast(daily, limit) : sliceLast(resampleTo(daily, tf), limit))); };
    // Today's session isn't in the history table until the close, so build it from the intraday feed.
    nasdaqToday(ticker, [daily, done](const Result<NqToday>& t) mutable {
      if (t.ok() && !t.value.pts.isEmpty()) {
        const auto& pts = t.value.pts;
        const qint64 day = qint64(std::floor(pts[0].first / 86400e3)) * 86400;
        if (daily.isEmpty() || daily.last().time < day) {
          double hi = -INFINITY, lo = INFINITY;
          for (const auto& p : pts) { hi = std::max(hi, p.second); lo = std::min(lo, p.second); }
          daily.push_back(Bar{day, pts.first().second, hi, lo, pts.last().second, t.value.volume});
        }
      }
      done(daily);   // history alone is fine
    });
  });
}

// The full screener list (last-close data, with sectors), without the live overlay.
void fullScreener(const QString& kind, Callback<QList<ScreenerRow>> cb) {
  if (offline()) {
    QList<ScreenerRow> rows;
    for (const auto& v : fixture("screener-us.json").array()) {
      ScreenerRow r;
      r.kind = ScreenerRow::Raw;
      r.raw = v.toObject();
      r.ticker = r.raw.value("ticker").toString();
      r.symbol = r.raw.value("symbol").toString();
      rows << r;
    }
    cb(Result<QList<ScreenerRow>>::success(rows));
    return;
  }
  // The mapped rows are what is kept (6 h), so the ETF list behind every getQuotes costs no re-mapping.
  cached<QList<ScreenerRow>>(
      "nq:screener-full:" + kind, 6 * 3600000LL,
      [kind](Callback<QList<ScreenerRow>> done) {
        getJson("https://api.nasdaq.com/api/screener/" + kind + "?tableonly=true&limit=25&offset=0&download=true", nqHeaders(),
                [kind, done](const Result<QJsonDocument>& r) {
          if (!r.ok()) { done(Result<QList<ScreenerRow>>::failure(*r.error)); return; }
          const QJsonObject data = r.value.object().value("data").toObject();
          const QJsonArray raw = truthy(data.value("rows")) ? data.value("rows").toArray() : data.value("data").toObject().value("rows").toArray();
          QList<ScreenerRow> rows;
          rows.reserve(raw.size());
          static const QRegularExpression bad(R"([\^/])");
          for (const auto& v : raw) {
            const QJsonObject o = v.toObject();
            ScreenerRow s;
            s.kind = ScreenerRow::Stock;
            s.ticker = o.value("symbol").toString().trimmed();
            if (s.ticker.isEmpty() || s.ticker.contains(bad)) continue;
            s.symbol = "US:" + s.ticker;
            s.name = orStr({o.value("name"), o.value("companyName")});
            s.price = nqNum(o.contains("lastsale") && !o.value("lastsale").isNull() ? o.value("lastsale") : o.value("lastSalePrice"));
            s.changePct = nqNum(o.contains("pctchange") && !o.value("pctchange").isNull() ? o.value("pctchange") : o.value("percentageChange"));
            s.volume = nqNum(o.value("volume"));
            s.marketCap = nqNum(o.value("marketCap"));
            s.sector = orStr({o.value("sector")}, kind == "etf" ? "ETF" : "");
            s.industry = orStr({o.value("industry")});
            s.country = orStr({o.value("country")});
            rows << s;
          }
          done(Result<QList<ScreenerRow>>::success(rows));
        });
      },
      [cb](const Result<QList<ScreenerRow>>& r) {
        if (r.ok()) mark("nasdaq", true);
        cb(r);
      });
}

std::optional<QSet<QString>>& usTickers() { static std::optional<QSet<QString>> s; return s; }
// Only the tickers are needed here, so the list without the live overlay (same tickers, 10 fewer requests).
void usTickerSet(Callback<QSet<QString>> cb) {
  if (usTickers()) { cb(Result<QSet<QString>>::success(*usTickers())); return; }
  fullScreener("stocks", [cb](const Result<QList<ScreenerRow>>& r) {
    // Only a list that came is kept for the session: storing the empty set of a failed call made every US ticker
    // resolve to crypto until a restart (audit D10). A failure answers empty for this call; the next one asks again.
    if (!r.ok()) {
      cb(Result<QSet<QString>>::success(usTickers() ? *usTickers() : QSet<QString>()));
      return;
    }
    if (!usTickers()) {
      QSet<QString> s;
      for (const auto& row : r.value) s.insert(row.ticker);
      usTickers() = s;
    }
    cb(Result<QSet<QString>>::success(*usTickers()));
  });
}

// Live quotes for US symbols, 20 per request.
void nasdaqLiveQuotes(const QStringList& tickers, const QString& defaultClass, Callback<Quotes> cb) {
  auto withEtfs = [=](const QSet<QString>& etfs) {
    QList<QStringList> chunks;
    for (int i = 0; i < tickers.size(); i += 20) chunks << tickers.mid(i, 20);
    auto out = std::make_shared<Quotes>();
    auto err = std::make_shared<std::optional<Error>>();
    auto join = Join::make(chunks.size(), [out, err, cb] {
      if (*err) cb(Result<Quotes>::failure(**err));
      else cb(Result<Quotes>::success(*out));
    });
    for (const QStringList& chunk : chunks) {
      QStringList qs;
      for (const QString& t : chunk) {
        const QString cls = !nqClass().value(t).isEmpty() ? nqClass().value(t)
                          : !defaultClass.isEmpty()      ? defaultClass
                          : etfs.contains(t)             ? QString("etf")
                          : QStringList{"NDX", "COMP", "RUT", "DJI"}.contains(t) ? QString("index")
                                                                                   : QString("stocks");
        qs << "symbol=" + encodeUriComponent(t.toLower() + "|" + cls);
      }
      const QString q = qs.join('&');
      cached<QJsonDocument>(
          "nq:wl:" + q, 5000,
          [q](Callback<QJsonDocument> done) { getJson("https://api.nasdaq.com/api/quote/watchlist?" + q, nqHeaders(), done); },
          [out, err, join](const Result<QJsonDocument>& r) {
            if (!r.ok()) { if (!*err) *err = *r.error; join->one(); return; }
            for (const auto& v : r.value.object().value("data").toArray()) {
              const QJsonObject o = v.toObject();
              Quote q;
              q.price = nqNum(o.value("lastSalePrice"));
              q.changePct = nqNum(o.value("percentageChange"));
              q.volume = nqNum(o.value("volume"));
              q.hasStatus = !o.value("marketStatus").isUndefined();
              q.status = jsString(o.value("marketStatus"));
              out->insert(jsString(o.value("symbol")), q);
            }
            join->one();
          });
    }
  };
  if (!defaultClass.isEmpty()) { withEtfs({}); return; }
  fullScreener("etf", [withEtfs](const Result<QList<ScreenerRow>>& r) {
    QSet<QString> etfs;
    if (r.ok()) for (const auto& row : r.value) etfs.insert(row.ticker);
    withEtfs(etfs);
  });
}

// ------------------------------------------------------------------------------------------------ spreads
bool isNumStr(const QString& x) { return test(R"(^-?\d+(\.\d+)?$)", x.trimmed()); }
double jsNumberOf(const QString& x) { return x.trimmed().toDouble(); }

void resolveI(const QString& raw, Callback<Symbol> cb);
void getCandlesI(const QString& rawSymbol, const QString& tf, int limit, Callback<CandlesResult> cb);

struct CryptoParts { QString base, quote; };
CryptoParts cryptoParts(const QString& ticker) {
  const auto m = rx(R"(^(.+?)(USDT|USDC|FDUSD|USD|BTC|ETH|EUR|BNB)$)").match(ticker);
  return m.hasMatch() ? CryptoParts{m.captured(1), m.captured(2)} : CryptoParts{ticker, "USDT"};
}

void spreadCandles(const Symbol& sym, const QString& tf, int limit, Callback<CandlesResult> cb) {
  // "SPREAD:FOO" resolves to exchange SPREAD without a parsable spread: the JS throws a TypeError here (500), the port
  // answers the same status with a readable message instead of dereferencing nothing.
  const auto parsed = parseSpread(sym.id);
  if (!parsed) { cb(Result<CandlesResult>::failure(Error{"\"" + sym.id.left(40) + "\" isn't a spread", 500, false, {}})); return; }
  const SpreadParts sp = *parsed;
  struct Side { bool isConst = false; double constant = 0; CandlesResult r; std::optional<Error> err; };
  auto sides = std::make_shared<std::array<Side, 2>>();
  auto join = Join::make(2, [=]() {
    const Side& ra = (*sides)[0];
    const Side& rb = (*sides)[1];
    if (ra.err) { cb(Result<CandlesResult>::failure(*ra.err)); return; }
    if (rb.err) { cb(Result<CandlesResult>::failure(*rb.err)); return; }
    const QString op = sp.op;
    auto f = [op](double x, double y) { return op == "/" ? x / y : op == "*" ? x * y : op == "+" ? x + y : x - y; };
    auto combine = [f](const Bar& ba, const Bar& bb, qint64 time) {
      const double o = f(ba.open, bb.open), c = f(ba.close, bb.close);
      double hi = -INFINITY, lo = INFINITY;
      for (double p : {o, c, f(ba.high, bb.high), f(ba.low, bb.low)})
        if (std::isfinite(p)) { hi = std::max(hi, p); lo = std::min(lo, p); }
      return Bar{time, o, hi, lo, c, 0};
    };
    auto k = [](double v) { return Bar{0, v, v, v, v, 0}; };
    Bars candles;
    if (ra.isConst) for (const Bar& x : rb.r.candles) candles << combine(k(ra.constant), x, x.time);
    else if (rb.isConst) for (const Bar& x : ra.r.candles) candles << combine(x, k(rb.constant), x.time);
    else {
      QHash<qint64, Bar> byTime;
      for (const Bar& x : rb.r.candles) byTime.insert(x.time, x);
      for (const Bar& x : ra.r.candles) if (byTime.contains(x.time)) candles << combine(x, byTime.value(x.time), x.time);
    }
    Bars kept;
    for (const Bar& b : candles) if (std::isfinite(b.open) && std::isfinite(b.close)) kept << b;
    if (kept.isEmpty()) { cb(Result<CandlesResult>::failure(Error{QT_TRANSLATE_NOOP("charts", "The two symbols have no bars at the same times"), 422})); return; }
    QStringList from;
    for (const QString& p : {ra.r.provider, rb.r.provider}) if (!p.isEmpty()) from << p;
    CandlesResult out{sym.id, tf, from.isEmpty() ? QString("constant") : from.join(" + "), sliceLast(kept, limit),
                      {QT_TRANSLATE_NOOP("charts", "Spread built from both symbols' bars at matching times.")}};
    cb(Result<CandlesResult>::success(out));
  });
  const QString parts[2] = {sp.a, sp.b};
  for (int i = 0; i < 2; i++) {
    if (isNumStr(parts[i])) {
      (*sides)[i].isConst = true;
      (*sides)[i].constant = jsNumberOf(parts[i]);
      later([join] { join->one(); });
      continue;
    }
    getCandlesI(parts[i], tf, limit, [sides, i, join](const Result<CandlesResult>& r) {
      if (r.ok()) (*sides)[i].r = r.value;
      else (*sides)[i].err = r.error;
      join->one();
    });
  }
}

qint64 ttlFor(const QString& tf) {
  const double s = tfSeconds(tf);
  return s < 60 ? 1500 : s < 3600 ? 4000 : s < 86400 ? 15000 : 60000;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ public helpers
QHash<QString, QString> krakenPairMap(const QJsonObject& result) {
  QHash<QString, QString> m;
  for (auto it = result.constBegin(); it != result.constEnd(); ++it) {
    const QJsonObject p = it->toObject();
    if (!truthy(p.value("wsname")) || p.value("status").toString() == "delisted") continue;
    const QStringList bq = p.value("wsname").toString().split('/');
    QString b = bq.value(0);
    if (b == "XBT") b = "BTC";
    else if (b == "XDG") b = "DOGE";
    m.insert(b + bq.value(1), jsString(p.value("altname")));
  }
  return m;
}

bool yahooAvailable() { return nowMs() > yahooDownUntil; }

Result<Bars> parseYahoo(const QJsonObject& j, const QString& tf) {
  const QJsonObject chart = j.value("chart").toObject();
  const QJsonValue r0 = chart.value("result").toArray().at(0);
  if (!r0.isObject()) {
    const QJsonValue d = chart.value("error").toObject().value("description");
    return Result<Bars>::failure(Error{truthy(d) ? jsString(d) : QT_TRANSLATE_NOOP("charts", "Yahoo returned no data for that symbol")});
  }
  const QJsonObject r = r0.toObject();
  const QJsonObject q = r.value("indicators").toObject().value("quote").toArray().at(0).toObject();
  const QJsonObject meta = r.value("meta").toObject();
  const double off = meta.value("gmtoffset").toDouble(0);
  // gmtoffset is the exchange's offset NOW: weekly and monthly bars are stamped at local midnight, so a bar from the
  // other side of a DST change, shifted by today's offset, landed on the previous day (audit D19: 1 Oct 00:00 EDT =
  // 04:00 UTC, minus EST's 5 h = 30 Sep). The exchange's zone gives the offset of each bar's own date.
  const QTimeZone zone(meta.value("exchangeTimezoneName").toString().toLatin1());
  auto offsetAt = [&](double t) {
    return zone.isValid() ? double(zone.offsetFromUtc(QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::UTC))) : off;
  };
  const bool daily = !intraday(tf);
  const QJsonArray ts = r.value("timestamp").toArray();
  const QJsonArray O = q.value("open").toArray(), H = q.value("high").toArray(), L = q.value("low").toArray(), C = q.value("close").toArray(),
                   V = q.value("volume").toArray();
  auto isNull = [](const QJsonValue& v) { return v.isNull() || v.isUndefined(); };
  Bars out;
  out.reserve(ts.size());
  for (int i = 0; i < ts.size(); i++) {
    if (isNull(C.at(i)) || isNull(O.at(i))) continue;
    const double t = ts.at(i).toDouble();
    const qint64 time = daily ? qint64(std::floor((t + offsetAt(t)) / 86400)) * 86400 : qint64(t);
    const QJsonValue v = V.at(i);
    out.push_back(Bar{time, O.at(i).toDouble(), H.at(i).toDouble(NaN), L.at(i).toDouble(NaN), C.at(i).toDouble(),
                      truthy(v) ? v.toDouble() : 0});
  }
  return Result<Bars>::success(dedupe(out));
}

qint64 nyWallToUtc(qint64 ms) {
  static const QTimeZone ny("America/New_York");
  const int wallHour = QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).time().hour();
  for (int h : {4, 5}) {
    const qint64 utc = ms + h * 3600000LL;
    if (QDateTime::fromMSecsSinceEpoch(utc, ny).time().hour() == wallHour) return utc;
  }
  return ms + 5 * 3600000LL;
}

std::optional<SpreadParts> parseSpread(const QString& raw) {
  const QString s = raw.trimmed().toUpper();
  if (s.startsWith("SPREAD:")) {
    const auto m = rx(R"(^SPREAD:(.+?) ([/*+-]) (.+)$)").match(s);
    if (!m.hasMatch()) return std::nullopt;
    return SpreadParts{m.captured(1), m.captured(2), m.captured(3)};
  }
  const auto fx = rx(R"(^([A-Z]{3})\/([A-Z]{3})$)").match(s);
  if (fx.hasMatch() && FIAT.contains(fx.captured(1)) && FIAT.contains(fx.captured(2))) return std::nullopt;   // EUR/USD is a pair, not a ratio
  // SOL/USDT, BTC/USD: a quote currency on the right means a trading pair, not a ratio.
  if (test(R"(^[A-Z0-9]{2,12}\/(USDT|USDC|FDUSD|BUSD|USD)$)", s)) return std::nullopt;
  for (const char* re : {R"(^(.+?)\s*([/*])\s*(.+)$)", R"(^(.+?)\s+([+-])\s+(.+)$)"}) {
    const auto m = rx(re).match(s);
    if (m.hasMatch() && !m.captured(1).trimmed().isEmpty() && !m.captured(3).trimmed().isEmpty())
      return SpreadParts{m.captured(1).trimmed(), m.captured(2), m.captured(3).trimmed()};
  }
  return std::nullopt;
}

QString kindOf(const QString& id) {
  const QString ex = id.section(':', 0, 0);
  return QStringList{"BINANCE", "COINBASE", "KRAKEN"}.contains(ex) ? "crypto" : ex == "FX" ? "fx" : ex == "US" ? "stock" : "global";
}

// ------------------------------------------------------------------------------------------------ resolve
namespace {
Symbol sym3(const QString& ex, const QString& t) { return Symbol{ex + ":" + t, ex, t}; }

void resolveSymbol(const QString& raw, Callback<Symbol> cb) {
  QString input = raw.trimmed().toUpper();
  input.remove(rx(R"(\s+)"));
  if (input.isEmpty()) { cb(Result<Symbol>::failure(badInput(QT_TRANSLATE_NOOP("charts", "Empty symbol")))); return; }
  auto rest = [raw, input, cb]() {
    // BTCUSDT.P, BTC.P, BTCPERP, BINANCE:ETHUSDT.P: crypto perpetual futures, TradingView's ".P".
    const auto perp = rx(R"(^(?:BINANCE:)?([A-Z0-9]{2,15}?)(USDT|USDC)?[-/]?(?:\.P|PERP)$)").match(input);
    if (perp.hasMatch()) {
      const QString t = perp.captured(1) + (perp.captured(2).isEmpty() ? QString("USDT") : perp.captured(2)) + ".P";
      cb(Result<Symbol>::success(sym3("BINANCE", t)));
      return;
    }
    // Tickers go into upstream URLs, so only the characters real tickers use are allowed.
    QString check = input;   // JS replace without /g: only the first match
    if (const auto m = rx(R"((^|:)\^)").match(input); m.hasMatch())
      check.replace(m.capturedStart(0), m.capturedLength(0), m.captured(1) + "A");
    if (!test(R"(^([A-Z0-9_]{1,20}:)?[A-Z0-9][A-Z0-9.\-_^=/]{0,29}$)", check)) {
      cb(Result<Symbol>::failure(badInput("\"" + raw.left(40) + "\" isn't a symbol")));
      return;
    }
    if (input.contains(':')) {
      const QStringList parts = input.split(':');
      cb(Result<Symbol>::success(Symbol{parts[0] + ":" + parts.value(1), parts[0], parts.value(1)}));
      return;
    }
    QString fx = input;
    if (const int s = fx.indexOf('/'); s >= 0) fx.remove(s, 1);
    if (test("^[A-Z]{6}$", fx) && FIAT.contains(fx.left(3)) && FIAT.contains(fx.mid(3))) { cb(Result<Symbol>::success(sym3("FX", fx))); return; }
    if (metal(fx)) { cb(Result<Symbol>::success(sym3("FX", fx))); return; }
    if (test("[=^.]", input)) { cb(Result<Symbol>::success(sym3("YAHOO", input))); return; }
    binanceSymbolMapI([input, cb](const Result<BinanceSymbols>& br) {
      const BinanceSymbols& bin = br.value;
      if (!bin.size()) {
        // Binance unreachable (blocked country, firewall): recognise crypto without its symbol list,
        // so the Coinbase and Kraken fallbacks still get the request.
        const auto pair = rx(R"(^([A-Z0-9]{2,12})[-/]?(USDT|USDC|FDUSD)$)").match(input);
        if (pair.hasMatch()) { cb(Result<Symbol>::success(sym3("BINANCE", pair.captured(1) + pair.captured(2)))); return; }
        QString base = input;
        base.remove(rx(R"([-/]?USD$)"));
        if (MAJOR_CRYPTO.contains(base)) { cb(Result<Symbol>::success(sym3("BINANCE", base + "USDT"))); return; }
      }
      const auto slashPair = rx(R"(^([A-Z0-9]{2,12})\/([A-Z0-9]{2,6})$)").match(input);
      if (slashPair.hasMatch() && bin.has(slashPair.captured(1) + slashPair.captured(2))) {
        cb(Result<Symbol>::success(sym3("BINANCE", slashPair.captured(1) + slashPair.captured(2))));
        return;
      }
      if (bin.has(input)) { cb(Result<Symbol>::success(sym3("BINANCE", input))); return; }
      QString crypto = input;
      crypto.remove(rx("[-/]"));
      auto tail = [input, cb]() {
        if (test(R"(^[A-Z]{2,10}-USD$)", input)) { cb(Result<Symbol>::success(sym3("COINBASE", input))); return; }
        cb(Result<Symbol>::success(sym3("US", input)));
      };
      if (bin.has(crypto + "USDT")) {
        usTickerSet([input, crypto, cb, tail](const Result<QSet<QString>>& us) {
          if (!us.value.contains(input)) cb(Result<Symbol>::success(sym3("BINANCE", crypto + "USDT")));
          else tail();
        });
        return;
      }
      tail();
    });
  };
  // NQ1!, CME_MINI:NQ1!, MNQ: futures. A bare root that is also a US stock ticker (ES) stays the stock.
  const auto fut = rx(R"(^(CME(?:_MINI)?:)?([A-Z]{2,3})(1!)?$)").match(input);
  if (fut.hasMatch() && futuresOf(fut.captured(2))) {
    const QString root = fut.captured(2);
    const Symbol s = sym3("CME", root + "1!");
    if (!fut.captured(1).isEmpty() || !fut.captured(3).isEmpty()) { cb(Result<Symbol>::success(s)); return; }
    usTickerSet([s, root, cb, rest](const Result<QSet<QString>>& us) {
      if (!us.value.contains(root)) cb(Result<Symbol>::success(s));
      else rest();
    });
    return;
  }
  rest();
}

void resolveSpread(const SpreadParts& sp, Callback<Symbol> cb) {
  if (parseSpread(sp.a) || parseSpread(sp.b)) { cb(Result<Symbol>::failure(badInput(QT_TRANSLATE_NOOP("charts", "Use one operator per spread, like BTCUSDT/ETHUSDT")))); return; }
  if (isNumStr(sp.a) && isNumStr(sp.b)) { cb(Result<Symbol>::failure(badInput(QT_TRANSLATE_NOOP("charts", "A spread needs at least one symbol")))); return; }
  struct Side { QString id; std::optional<Error> err; };
  auto sides = std::make_shared<std::array<Side, 2>>();
  auto join = Join::make(2, [sides, sp, cb] {
    for (const auto& s : *sides) if (s.err) { cb(Result<Symbol>::failure(*s.err)); return; }
    const QString a = (*sides)[0].id, b = (*sides)[1].id;
    cb(Result<Symbol>::success(Symbol{"SPREAD:" + a + " " + sp.op + " " + b, "SPREAD", a + " " + sp.op + " " + b}));
  });
  const QString parts[2] = {sp.a, sp.b};
  for (int i = 0; i < 2; i++) {
    if (isNumStr(parts[i])) {
      (*sides)[i].id = series::numStr(jsNumberOf(parts[i]));
      later([join] { join->one(); });
      continue;
    }
    resolveI(parts[i], [sides, i, join](const Result<Symbol>& r) {
      if (r.ok()) (*sides)[i].id = r.value.id;
      else (*sides)[i].err = r.error;
      join->one();
    });
  }
}

void resolveI(const QString& raw, Callback<Symbol> cb) {
  const auto sp = parseSpread(raw);
  // ETH/BTC is Binance's ETHBTC pair when Binance lists it; otherwise it's a ratio.
  if (sp && sp->op == "/" && test("^[A-Z0-9]{2,12}$", sp->a) && test("^[A-Z0-9]{2,6}$", sp->b)) {
    binanceSymbolMapI([raw, sp, cb](const Result<BinanceSymbols>& r) {
      if (r.value.has(sp->a + sp->b)) resolveSymbol(raw, cb);
      else resolveSpread(*sp, cb);
    });
    return;
  }
  if (sp) resolveSpread(*sp, cb);
  else resolveSymbol(raw, cb);
}

// ------------------------------------------------------------------------------------------------ candles
void getCandlesI(const QString& rawSymbol, const QString& tf, int limitIn, Callback<CandlesResult> cb) {
  try {
    series::parseTf(tf);
  } catch (const std::exception& e) {
    cb(Result<CandlesResult>::failure(badInput(QString::fromUtf8(e.what()))));
    return;
  }
  const int limit = std::max(10, std::min(limitIn ? limitIn : 1500, 20000));
  resolveI(rawSymbol, [tf, limit, cb](const Result<Symbol>& sr) {
    if (!sr.ok()) { cb(Result<CandlesResult>::failure(*sr.error)); return; }
    const Symbol sym = sr.value;
    if (sym.exchange == "SPREAD") { spreadCandles(sym, tf, limit, cb); return; }
    // Your own imported bars (<data>/charts/imports/<NAME>.csv).
    if (sym.exchange == "CSV") {
#ifdef CHARTS_HAS_IMPORTS
      // on the pool: the first read of a big file takes seconds (audit P8); the answer comes back on the GUI thread
      (void)QtConcurrent::run([ticker = sym.ticker, tf, limit, cb] {
        Result<CandlesResult> r = Result<CandlesResult>::failure(Error{});
        try {
          const CsvCandles c = csvCandles(ticker, tf, limit);
          r = Result<CandlesResult>::success(CandlesResult{c.symbol, c.tf, c.provider, c.candles, c.notes});
        } catch (const ImportError& e) {
          r = Result<CandlesResult>::failure(Error{e.message, e.status});
        } catch (const std::exception& e) {
          r = Result<CandlesResult>::failure(Error{QString::fromUtf8(e.what())});
        }
        // the application object is on the GUI thread whatever thread runs this
        QMetaObject::invokeMethod(QCoreApplication::instance(), [cb, r] { cb(r); }, Qt::QueuedConnection);
      });
#else
      // TODO(charts): Imports.h not written yet; CSV symbols chart once charts::data::csvCandles exists.
      cb(Result<CandlesResult>::failure(Error{QT_TRANSLATE_NOOP("charts", "Imported files aren't available in this build"), 404}));
#endif
      return;
    }
    if (offline()) {
      const Bars bars = barsOf(fixture("btc-1d.json").array());
      cb(Result<CandlesResult>::success(
          CandlesResult{sym.id, tf, "fixture", tf == "1D" ? sliceLast(bars, limit) : sliceLast(resampleTo(bars, tf), limit), {}}));
      return;
    }
    cached<CandlesResult>(
        "candles:" + sym.id + ":" + tf + ":" + QString::number(limit), ttlFor(tf),
        [sym, tf, limit](Callback<CandlesResult> done) {
          const QString t = sym.ticker;
          Chain chain;
          auto step = [](const QString& name, std::function<void(Callback<Bars>)> fn) { return ChainStep{name, std::move(fn)}; };
          if (sym.exchange == "BINANCE" && t.endsWith(".P")) {
            const QString pair = t.left(t.size() - 2);
            chain << step("Binance futures", [=](Callback<Bars> c) { binanceCandles(pair, tf, limit, true, c); })
                  << step("Bybit", [=](Callback<Bars> c) { bybitCandles(pair, tf, limit, c); });
          } else if (sym.exchange == "BINANCE") {
            const auto [base, quote] = cryptoParts(t);
            const bool usdQuote = QStringList{"USDT", "USDC", "FDUSD", "USD"}.contains(quote);
            chain << step("Binance", [=](Callback<Bars> c) { binanceCandles(t, tf, limit, false, c); });
            if (usdQuote) {
              chain << step("Coinbase", [=](Callback<Bars> c) { coinbaseCandles(base + "-USD", tf, limit, c); });
              chain << step("Kraken", [=](Callback<Bars> c) { krakenCandles(krakenAsset(base) + "USD", tf, limit, c); });
            }
            // Binance.US last, under its own name: its prices are its own, and the chart must not open the global
            // kline socket on top of them (LiveFeed opens one only for "Binance")
            chain << step("Binance.US", [=](Callback<Bars> c) { binanceCandles(t, tf, limit, false, c, BinanceHosts::Us); });
          } else if (sym.exchange == "COINBASE") {
            chain << step("Coinbase", [=](Callback<Bars> c) { coinbaseCandles(t, tf, limit, c); });
          } else if (sym.exchange == "KRAKEN") {
            chain << step("Kraken", [=](Callback<Bars> c) { krakenCandles(t, tf, limit, c); });
          } else if (sym.exchange == "FX" && metal(t)) {
            const Metal m = *metal(t);
            chain = feeds::fxChain(t, tf, limit);
            chain << step("Yahoo", [=](Callback<Bars> c) { yahooCandles(m.yahoo, tf, limit, c); });
            if (!m.token.isEmpty())
              chain << step("Binance", [=](Callback<Bars> c) { binanceCandles(m.token + "USDT", tf, limit, false, c); })
                    << step("Coinbase", [=](Callback<Bars> c) { coinbaseCandles(m.token + "-USD", tf, limit, c); });
          } else if (sym.exchange == "FX") {
            chain = feeds::fxChain(t, tf, limit);
            chain << step("Yahoo", [=](Callback<Bars> c) { yahooCandles(t + "=X", tf, limit, c); })
                  << step("Kraken", [=](Callback<Bars> c) { krakenCandles(t, tf, limit, c); });
          } else if (sym.exchange == "OANDA") {
            if (!feeds::hasOanda()) {
              done(Result<CandlesResult>::failure(badInput(
                  QT_TRANSLATE_NOOP("charts", "OANDA symbols need your own OANDA token in data/feeds.json. See the README's own data feed section."))));
              return;
            }
            chain << step("OANDA", [=](Callback<Bars> c) { feeds::oandaCandles(t, tf, limit, c); });
          } else if (sym.exchange == "CME" && futuresOf(QString(t).remove(rx("1!$")))) {
            const QString root = QString(t).remove(rx("1!$"));
            const QString yahoo = futuresOf(root)->yahoo;
            chain = feeds::futuresChain(root, tf, limit);
            chain << step(QT_TRANSLATE_NOOP("charts", "Yahoo (delayed)"), [=](Callback<Bars> c) { yahooCandles(yahoo, tf, limit, c); });
          } else if (sym.exchange == "US") {
            // Your own keys (feeds.json) go first; the public sources stay behind them.
            chain = feeds::stockChain(t, tf, limit);
            if (intraday(tf))
              chain << step("Yahoo", [=](Callback<Bars> c) { yahooCandles(t, tf, limit, c); })
                    << step("Nasdaq", [=](Callback<Bars> c) { nasdaqCandles(t, tf, limit, c); });
            else
              chain << step("Nasdaq", [=](Callback<Bars> c) { nasdaqCandles(t, tf, limit, c); })
                    << step("Yahoo", [=](Callback<Bars> c) { yahooCandles(t, tf, limit, c); });
          } else {
            chain << step("Yahoo", [=](Callback<Bars> c) { yahooCandles(t, tf, limit, c); });
          }
          tryChain(chain, [sym, tf, t, done](const Result<ChainResult>& rr) {
            if (!rr.ok()) { done(Result<CandlesResult>::failure(*rr.error)); return; }
            const ChainResult& res = rr.value;
            QStringList notes;
            // Plain words on the chart; the technical reason stays in the log and the health map.
            QStringList publicFails;
            for (const QString& f : res.fallbacks) {
              if (!feeds::isKeyProvider(f)) { publicFails << f; continue; }
              QString why = f;
              why.remove(rx(R"(^[^:]*:\s*)"));
              notes << (why.isEmpty() ? QString() : why.left(1).toUpper() + why.mid(1)) + ", so this chart uses " + res.provider + ".";
            }
            if (!publicFails.isEmpty()) {
              QStringList busy;
              for (const QString& f : publicFails) {
                const QString name = f.section(':', 0, 0).trimmed();
                if (!busy.contains(name)) busy << name;
              }
              notes << busy.join(" and ") + " isn't answering right now, so this chart uses " + res.provider + ".";
            }
            if (res.provider == "Yahoo (delayed)")
              notes << QT_TRANSLATE_NOOP("charts", "Delayed about 10 minutes, because CME charges for live futures data. Your own Polygon futures key in data/feeds.json makes it live.");
            if (sym.exchange == "CME" && res.provider.startsWith("Polygon "))
              notes << "Front contract " + res.provider.mid(8) +
                           " from your Polygon key, not joined to older contracts. Whether it's live depends on your Polygon futures plan.";
            if (res.provider == "Alpaca (IEX)")
              notes << QT_TRANSLATE_NOOP("charts", "Live from your Alpaca key on the free IEX feed: prices are live, volume counts IEX trades only.");
            if (res.provider == "OANDA")
              notes << QT_TRANSLATE_NOOP("charts", "OANDA's own prices from your token, not exchange data: volume counts price changes, not contracts or shares.");
            if (const Metal* m = metal(t); m && res.provider != "Yahoo" && res.provider != "OANDA")
              notes << "This chart follows " + (m->token.isEmpty() ? QString("undefined") : m->token) +
                           ", a token backed by real gold, until Yahoo's gold feed is back.";
            if (sym.exchange == "US" && res.provider == "Nasdaq" && intraday(tf))
              notes << QT_TRANSLATE_NOOP("charts", "Free intraday stock data covers today only and has no volume per bar. Older bars and volume appear when Yahoo is back.");
            if (res.provider == "Kraken") notes << QT_TRANSLATE_NOOP("charts", "Kraken's free feed shows the last 720 bars.");
            done(Result<CandlesResult>::success(CandlesResult{sym.id, tf, res.provider, res.candles, notes}));
          });
        },
        cb);
  });
}
}  // namespace

// ------------------------------------------------------------------------------------------------ public API
void resolve(const QString& raw, QObject* ctx, Callback<Symbol> cb) {
  runAsync<Symbol>(ctx, std::move(cb), [&](Callback<Symbol> c) { resolveI(raw, c); });
}
void binanceSymbolMap(QObject* ctx, Callback<BinanceSymbols> cb) {
  runAsync<BinanceSymbols>(ctx, std::move(cb), [&](Callback<BinanceSymbols> c) { binanceSymbolMapI(c); });
}
void perpSymbols(QObject* ctx, Callback<QSet<QString>> cb) {
  runAsync<QSet<QString>>(ctx, std::move(cb), [&](Callback<QSet<QString>> c) { perpSymbolsI(c); });
}
void getCandles(const QString& rawSymbol, const QString& tf, int limit, QObject* ctx, Callback<CandlesResult> cb) {
  runAsync<CandlesResult>(ctx, std::move(cb), [&](Callback<CandlesResult> c) { getCandlesI(rawSymbol, tf, limit, c); });
}

// Latest price and day change for a list of symbols (watchlists, paper trading, heatmaps).
void getQuotes(const QStringList& symbols, QObject* ctx, Callback<Quotes> cbIn) {
  runAsync<Quotes>(ctx, std::move(cbIn), [&](Callback<Quotes> cb) {
    auto resolved = std::make_shared<QVector<std::optional<Symbol>>>(symbols.size());
    auto out = std::make_shared<Quotes>();
    auto afterResolve = [resolved, out, cb]() {
      QStringList binance;
      for (const auto& r : *resolved) if (r && r->exchange == "BINANCE" && !r->ticker.endsWith(".P")) binance << r->ticker;
      QList<Symbol> us;
      for (const auto& r : *resolved) if (r && r->exchange == "US") us << *r;
      auto candlesStep = [resolved, out, cb]() {
        QList<Symbol> rest;
        for (const auto& r : *resolved) if (r && !out->contains(r->id)) rest << *r;
        auto join = Join::make(rest.size(), [out, cb] { cb(Result<Quotes>::success(*out)); });
        for (const Symbol& r : rest)
          getCandlesI(r.id, "1D", 3, [out, r, join](const Result<CandlesResult>& c) {
            Quote q;
            if (!c.ok()) q.error = c.error->message;
            else if (c.value.candles.isEmpty()) q.error = "Cannot read properties of undefined (reading 'close')";
            else {
              const Bar last = c.value.candles.last();
              q.price = last.close;
              q.changePct = c.value.candles.size() > 1 ? (last.close - c.value.candles[c.value.candles.size() - 2].close) /
                                                             c.value.candles[c.value.candles.size() - 2].close * 100
                                                       : 0;
              q.volume = last.volume;
            }
            if (!out->contains(r.id)) out->insert(r.id, q);
            join->one();
          });
      };
      auto usStep = [us, out, candlesStep]() {
        if (us.isEmpty() || offline()) { candlesStep(); return; }
        QStringList tickers;
        for (const auto& s : us) tickers << s.ticker;
        nasdaqLiveQuotes(tickers, {}, [us, out, candlesStep](const Result<Quotes>& live) {
          if (live.ok())
            for (const auto& s : us) if (live.value.contains(s.ticker)) out->insert(s.id, live.value.value(s.ticker));
          candlesStep();   // fall through to candles for the rest
        });
      };
      if (binance.isEmpty() || offline()) { usStep(); return; }
      std::sort(binance.begin(), binance.end());
      QStringList quoted;
      for (const QString& b : binance) quoted << "\"" + b + "\"";
      const QString json = "[" + quoted.join(',') + "]";
      cached<QJsonDocument>(
          "binance:24h:" + binance.join(','), 3000,
          [json](Callback<QJsonDocument> done) { binanceGet("/api/v3/ticker/24hr?symbols=" + encodeUriComponent(json), done); },
          [out, usStep](const Result<QJsonDocument>& r) {
            if (r.ok())
              for (const auto& v : r.value.array()) {
                const QJsonObject o = v.toObject();
                Quote q;
                q.price = plus(o.value("lastPrice"));
                q.changePct = plus(o.value("priceChangePercent"));
                q.volume = plus(o.value("quoteVolume"));
                out->insert("BINANCE:" + o.value("symbol").toString(), q);
              }
            usStep();
          });
    };
    auto join = Join::make(symbols.size(), afterResolve);
    for (int i = 0; i < symbols.size(); i++)
      resolveI(symbols[i], [resolved, i, join](const Result<Symbol>& r) {
        if (r.ok()) (*resolved)[i] = r.value;
        join->one();
      });
  });
}

void getSpread(const QString& rawSymbol, QObject* ctx, Callback<SpreadQuote> cbIn) {
  runAsync<SpreadQuote>(ctx, std::move(cbIn), [&](Callback<SpreadQuote> cb) {
    resolveI(rawSymbol, [cb](const Result<Symbol>& sr) {
      if (!sr.ok()) { cb(Result<SpreadQuote>::failure(*sr.error)); return; }
      const Symbol sym = sr.value;
      auto finish = [sym, cb](double bid, double ask, const QString& source) {
        if (!(bid > 0 && ask >= bid)) { cb(Result<SpreadQuote>::failure(Error{source + " sent no usable bid and ask"})); return; }
        cb(Result<SpreadQuote>::success(SpreadQuote{sym.id, bid, ask, (200 * (ask - bid)) / (ask + bid), source}));
      };
      auto onJson = [cb](std::function<void(const QJsonDocument&)> f) {
        return [cb, f](const Result<QJsonDocument>& r) {
          if (!r.ok()) { cb(Result<SpreadQuote>::failure(*r.error)); return; }
          f(r.value);
        };
      };
      if (sym.exchange == "BINANCE" && sym.ticker.endsWith(".P")) {
        binanceFuturesGet("/fapi/v1/ticker/bookTicker?symbol=" + encodeUriComponent(sym.ticker.left(sym.ticker.size() - 2)), onJson([finish](const QJsonDocument& d) {
          finish(plus(d.object().value("bidPrice")), plus(d.object().value("askPrice")), "Binance futures");
        }));
      } else if (sym.exchange == "BINANCE") {
        binanceGet("/api/v3/ticker/bookTicker?symbol=" + encodeUriComponent(sym.ticker), onJson([finish](const QJsonDocument& d) {
          finish(plus(d.object().value("bidPrice")), plus(d.object().value("askPrice")), "Binance");
        }));
      } else if (sym.exchange == "COINBASE") {
        getJson("https://api.exchange.coinbase.com/products/" + encodeUriComponent(sym.ticker) + "/ticker", {}, onJson([finish](const QJsonDocument& d) {
          finish(plus(d.object().value("bid")), plus(d.object().value("ask")), "Coinbase");
        }));
      } else if (sym.exchange == "KRAKEN") {
        getJson("https://api.kraken.com/0/public/Ticker?pair=" + encodeUriComponent(sym.ticker), {}, onJson([finish, cb](const QJsonDocument& d) {
          const QJsonObject j = d.object();
          if (!j.value("error").toArray().isEmpty()) { cb(Result<SpreadQuote>::failure(Error{"Kraken: " + krakenErrors(j)})); return; }
          const QJsonObject res = j.value("result").toObject();
          const QJsonObject r = res.isEmpty() ? QJsonObject() : res.begin()->toObject();
          finish(plus(r.value("b").toArray().at(0)), plus(r.value("a").toArray().at(0)), "Kraken");
        }));
      } else {
        cb(Result<SpreadQuote>::failure(
            badInput(QT_TRANSLATE_NOOP("charts", "A live spread is only published for crypto (Binance, Coinbase and Kraken). Enter one by hand for this market."))));
      }
    });
  });
}

void search(const QString& q, QObject* ctx, Callback<QList<SearchHit>> cbIn) {
  runAsync<QList<SearchHit>>(ctx, std::move(cbIn), [&](Callback<QList<SearchHit>> cb) {
    const QString query = q.trimmed().toUpper();
    if (query.isEmpty()) { cb(Result<QList<SearchHit>>::success({})); return; }
    auto results = std::make_shared<QList<SearchHit>>();
#ifdef CHARTS_HAS_IMPORTS
    QString csvQuery = query;
    csvQuery.remove(rx("^CSV:"));
    for (const ImportSummary& f : listImportsReady())   // never parses here: a keystroke must not wait for a file
      if (f.error.isEmpty() && (f.name.contains(csvQuery) || query == "CSV"))
        *results << SearchHit{f.symbol, "Your file, " + QLocale(QLocale::English).toString(qlonglong(f.bars)) + " " + f.tf + " bars", QT_TRANSLATE_NOOP("charts", "your data"), "CSV"};
#endif
    if (query.startsWith("OANDA:") && query.size() > 6) *results << SearchHit{query, QT_TRANSLATE_NOOP("charts", "From your OANDA token"), QT_TRANSLATE_NOOP("charts", "your feed"), "OANDA"};
    QString root = query;
    root.remove(rx("^CME(_MINI)?:"));
    root.remove(rx("1!?$"));
    for (const auto& f : futures())
      if ((root.size() >= 2 && f.root.startsWith(root)) || (query.size() >= 4 && f.name.toUpper().contains(query)))
        *results << SearchHit{"CME:" + f.root + "1!", f.name, QT_TRANSLATE_NOOP("charts", "futures"), "CME"};
    QString qNoSlash = query;
    if (const int s = qNoSlash.indexOf('/'); s >= 0) qNoSlash.remove(s, 1);
    for (const auto& m : metals())
      if (query.size() >= 2 && (m.ticker.startsWith(qNoSlash) || m.word.startsWith(query)))
        *results << SearchHit{"FX:" + m.ticker, m.name, QT_TRANSLATE_NOOP("charts", "commodity"), "FX"};
    binanceSymbolMapI([=](const Result<BinanceSymbols>& br) {
      const BinanceSymbols& bin = br.value;
      QString coin = query;
      coin.remove(rx("^BINANCE:"));
      coin.remove(rx(R"([-/]?(\.P|PERP)$)"));
      QStringList hitsT;
      for (const QString& t : bin.order) if (t.startsWith(coin) || bin.map.value(t).base == coin) hitsT << t;
      std::stable_sort(hitsT.begin(), hitsT.end(), [&bin](const QString& a, const QString& b) {
        const int ua = bin.map.value(a).quote == "USDT" ? -1 : 0, ub = bin.map.value(b).quote == "USDT" ? -1 : 0;
        if (ua != ub) return ua < ub;
        return a.size() < b.size();
      });
      QList<SearchHit> cryptoHits;
      for (const QString& t : hitsT.mid(0, 8))
        cryptoHits << SearchHit{"BINANCE:" + t, bin.map.value(t).base + " / " + bin.map.value(t).quote, "crypto", "Binance"};
      // The same coin on Coinbase's and Kraken's own feeds, the way TradingView lists one row per exchange,
      // straight after the main Binance pair.
      QStringList bases;
      for (const auto& h : cryptoHits) {
        const QString b = h.name.section(" / ", 0, 0);
        if (!bases.contains(b)) bases << b;
      }
      bases = bases.mid(0, 3);
      struct Lists { QSet<QString> cb; QHash<QString, QString> kr; QSet<QString> perps; };
      auto lists = std::make_shared<Lists>();
      auto afterLists = [=]() {
        QList<SearchHit> feedHits;
        for (const QString& b : bases) {
          if (lists->cb.contains(b + "-USD")) feedHits << SearchHit{"COINBASE:" + b + "-USD", b + " / USD", "crypto", "Coinbase"};
          if (lists->kr.contains(b + "USD")) feedHits << SearchHit{"KRAKEN:" + lists->kr.value(b + "USD"), b + " / USD", "crypto", "Kraken"};
        }
        // The perpetual future for the same coin, as TradingView lists BTCUSDT.P under BTCUSDT.
        for (const QString& b : bases)
          if (lists->perps.contains(b + "USDT")) feedHits << SearchHit{"BINANCE:" + b + "USDT.P", b + " / USDT perpetual", "futures", "Binance"};
        QString qNoEx = query;
        qNoEx.remove(rx("^BINANCE:"));
        const bool perpFirst = coin != qNoEx;
        auto isPerp = [](const SearchHit& f) { return f.symbol.endsWith(".P"); };
        if (perpFirst) for (const auto& f : feedHits) if (isPerp(f)) *results << f;
        *results << cryptoHits.mid(0, 1);
        for (const auto& f : feedHits) if (!perpFirst || !isPerp(f)) *results << f;
        *results << cryptoHits.mid(1);
        for (const QString& a : FIAT)
          for (const QString& b : FIAT)
            if (a != b && (a + b).startsWith(qNoSlash)) *results << SearchHit{"FX:" + a + b, a + " / " + b, "forex", "FX"};
        auto finish = [results, cb]() {
          QSet<QString> seen;
          QList<SearchHit> out;
          for (const auto& r : *results) {
            if (seen.contains(r.symbol)) continue;
            seen.insert(r.symbol);
            out << r;
            if (out.size() == 30) break;
          }
          cb(Result<QList<SearchHit>>::success(out));
        };
        if (offline()) { finish(); return; }
        auto nq = std::make_shared<QList<SearchHit>>();
        auto yh = std::make_shared<QList<SearchHit>>();
        auto join = Join::make(2, [results, nq, yh, finish] { *results << *nq << *yh; finish(); });
        getJson("https://api.nasdaq.com/api/autocomplete/slookup/10?search=" + encodeUriComponent(q), nqHeaders(), [nq, join](const Result<QJsonDocument>& r) {
          if (r.ok())
            for (const auto& v : r.value.object().value("data").toArray()) {
              const QJsonObject o = v.toObject();
              *nq << SearchHit{"US:" + jsString(o.value("symbol")), jsString(o.value("name")),
                               orStr({o.value("asset"), o.value("subCategory")}, "stock").toLower(), orStr({o.value("exchange")}, "US")};
            }
          join->one();
        });
        if (!yahooAvailable()) { join->one(); return; }
        getJson("https://query1.finance.yahoo.com/v1/finance/search?q=" + encodeUriComponent(q) + "&quotesCount=8&newsCount=0", {},
                [yh, join](const Result<QJsonDocument>& r) {
                  if (!r.ok()) {
                    if (r.error->status == 429) yahooDownUntil = nowMs() + 10 * 60000;
                  } else {
                    for (const auto& v : r.value.object().value("quotes").toArray()) {
                      const QJsonObject o = v.toObject();
                      const QString s = jsString(o.value("symbol"));
                      if (s.isEmpty() || test("^[A-Z]+$", s) || o.value("quoteType").toString() == "CRYPTOCURRENCY") continue;
                      *yh << SearchHit{"YAHOO:" + s, orStr({o.value("shortname"), o.value("longname")}, s),
                                       jsString(o.value("quoteType")).toLower(), orStr({o.value("exchDisp")}, "Yahoo")};
                    }
                  }
                  join->one();
                });
      };
      if (bases.isEmpty()) { afterLists(); return; }
      auto join = Join::make(offline() ? 1 : 3, afterLists);
      if (!offline()) {
        coinbaseProducts([lists, join](const Result<QSet<QString>>& r) { lists->cb = r.value; join->one(); });
        krakenPairs([lists, join](const Result<QHash<QString, QString>>& r) { lists->kr = r.value; join->one(); });
      }
      perpSymbolsI([lists, join](const Result<QSet<QString>>& r) { lists->perps = r.value; join->one(); });
    });
  });
}

void nasdaqScreener(const QString& kind, QObject* ctx, Callback<QList<ScreenerRow>> cbIn) {
  runAsync<QList<ScreenerRow>>(ctx, std::move(cbIn), [&](Callback<QList<ScreenerRow>> cb) {
    if (offline()) { fullScreener(kind, cb); return; }
    cached<QList<ScreenerRow>>(
        "nq:screener:" + kind, 60000,
        [kind](Callback<QList<ScreenerRow>> done) {
          fullScreener(kind, [kind, done](const Result<QList<ScreenerRow>>& r) {
            if (!r.ok()) { done(r); return; }
            // The full list (with sectors) is last-close data, so overlay live quotes on the 200 biggest.
            auto rows = std::make_shared<QList<ScreenerRow>>(r.value);
            QVector<int> idx(rows->size());
            std::iota(idx.begin(), idx.end(), 0);
            std::stable_sort(idx.begin(), idx.end(), [&rows](int x, int y) { return (*rows)[y].marketCap < (*rows)[x].marketCap; });
            idx.resize(std::min<int>(200, idx.size()));
            QStringList top;
            for (int i : idx) top << (*rows)[i].ticker;
            nasdaqLiveQuotes(top, kind == "etf" ? "etf" : "stocks", [rows, idx, done](const Result<Quotes>& live) {
              if (live.ok())   // otherwise last-close data is still useful
                for (int i : idx) {
                  ScreenerRow& row = (*rows)[i];
                  const auto it = live.value.constFind(row.ticker);
                  if (it == live.value.constEnd() || !it->price) continue;
                  row.price = it->price;
                  row.changePct = it->changePct;
                  row.volume = it->volume ? it->volume : row.volume;
                  row.live = true;
                }
              done(Result<QList<ScreenerRow>>::success(*rows));
            });
          });
        },
        cb);
  });
}

void cryptoScreener(QObject* ctx, Callback<QList<ScreenerRow>> cbIn) {
  runAsync<QList<ScreenerRow>>(ctx, std::move(cbIn), [&](Callback<QList<ScreenerRow>> cb) {
    if (offline()) {
      QList<ScreenerRow> rows;
      for (const auto& v : fixture("screener-crypto.json").array()) {
        ScreenerRow r;
        r.kind = ScreenerRow::Raw;
        r.raw = v.toObject();
        r.ticker = r.raw.value("ticker").toString();
        r.symbol = r.raw.value("symbol").toString();
        rows << r;
      }
      cb(Result<QList<ScreenerRow>>::success(rows));
      return;
    }
    cached<QList<ScreenerRow>>(
        "screener:crypto", 20000,
        [](Callback<QList<ScreenerRow>> done) {
          auto kraken = [done]() {
            // Binance blocked: Kraken's ticker (change measured from today's UTC open)
            getJson("https://api.kraken.com/0/public/Ticker", {}, [done](const Result<QJsonDocument>& r) {
              if (!r.ok()) { done(Result<QList<ScreenerRow>>::failure(*r.error)); return; }
              const QJsonObject res = r.value.object().value("result").toObject();
              QList<ScreenerRow> rows;
              for (auto it = res.constBegin(); it != res.constEnd(); ++it) {
                const QString k = it.key();
                if (!test("USD$", k) || test("USDT?USD$|^Z|EURUSD|GBPUSD", k)) continue;
                const QJsonObject v = it->toObject();
                ScreenerRow s;
                s.kind = ScreenerRow::CryptoNoRange;
                s.ticker = k;
                s.symbol = "KRAKEN:" + k;
                s.name = QString(k).remove(rx("USD$")).remove(rx("^X(?=[A-Z]{3}$)"));
                s.price = plus(v.value("c").toArray().at(0));
                const double open = plus(v.value("o"));
                s.changePct = (s.price - open) / open * 100;
                s.volume = plus(v.value("v").toArray().at(1)) * plus(v.value("p").toArray().at(1));
                s.sector = "Crypto";
                rows << s;
              }
              done(Result<QList<ScreenerRow>>::success(rows));
            });
          };
          binanceSymbolMapI([done, kraken](const Result<BinanceSymbols>& br) {
            const BinanceSymbols bin = br.value;
            binanceGet("/api/v3/ticker/24hr", [bin, done, kraken](const Result<QJsonDocument>& r) {
              if (!r.ok()) { kraken(); return; }
              QList<ScreenerRow> rows;
              for (const auto& v : r.value.array()) {
                const QJsonObject o = v.toObject();
                const QString t = o.value("symbol").toString();
                if (bin.map.value(t).quote != "USDT" || !bin.has(t) || test("(UP|DOWN|BULL|BEAR)USDT$", t) || !(plus(o.value("quoteVolume")) > 0)) continue;
                ScreenerRow s;
                s.kind = ScreenerRow::Crypto;
                s.ticker = t;
                s.symbol = "BINANCE:" + t;
                s.name = bin.map.value(t).base;
                s.price = plus(o.value("lastPrice"));
                s.changePct = plus(o.value("priceChangePercent"));
                s.volume = plus(o.value("quoteVolume"));
                s.high = plus(o.value("highPrice"));
                s.low = plus(o.value("lowPrice"));
                s.sector = "Crypto";
                rows << s;
              }
              done(Result<QList<ScreenerRow>>::success(rows));
            });
          });
        },
        cb);
  });
}

void economicCalendar(QObject* ctx, Callback<QList<CalendarEvent>> cbIn) {
  runAsync<QList<CalendarEvent>>(ctx, std::move(cbIn), [&](Callback<QList<CalendarEvent>> cb) {
    if (offline()) { cb(Result<QList<CalendarEvent>>::success({})); return; }
    cached<QList<CalendarEvent>>(
        "calendar", 30 * 60000,
        [](Callback<QList<CalendarEvent>> done) {
          // Same URL as news/Calendar: the disk cache answers both within 30 minutes (Forex Factory asks for few requests).
          getJson("https://nfs.faireconomy.media/ff_calendar_thisweek.json", {}, [done](const Result<QJsonDocument>& r) {
            if (!r.ok()) { done(Result<QList<CalendarEvent>>::failure(*r.error)); return; }
            mark("calendar", true);
            QList<CalendarEvent> rows;
            for (const auto& v : r.value.array()) {
              const QJsonObject o = v.toObject();
              rows << CalendarEvent{o.value("title"), o.value("country"), o.value("date"), o.value("impact"), o.value("forecast"), o.value("previous")};
            }
            done(Result<QList<CalendarEvent>>::success(rows));
          }, GetOpts{30 * 60, false});
        },
        cb);
  });
}

void probeAll(QObject* ctx, Callback<Probes> cbIn) {
  runAsync<Probes>(ctx, std::move(cbIn), [&](Callback<Probes> cb) {
    QList<QPair<QString, std::function<void(Callback<QJsonDocument>)>>> checks = {
        {"Binance (crypto)", [](Callback<QJsonDocument> c) { binanceGet("/api/v3/ping", c); }},
        {"Coinbase (crypto fallback)", [](Callback<QJsonDocument> c) { getJson("https://api.exchange.coinbase.com/products/BTC-USD/ticker", {}, c); }},
        {"Kraken (crypto + forex)", [](Callback<QJsonDocument> c) { getJson("https://api.kraken.com/0/public/Time", {}, c); }},
        {"Nasdaq (US stocks)", [](Callback<QJsonDocument> c) { getJson("https://api.nasdaq.com/api/quote/AAPL/info?assetclass=stocks", nqHeaders(), c); }},
        {"Yahoo (global, intraday stocks)",
         [](Callback<QJsonDocument> c) { getJson("https://query1.finance.yahoo.com/v8/finance/chart/SPY?interval=1d&range=5d", {}, c); }},
        // Same options as the calendar itself: a copy fetched within 30 minutes answers at once instead of waiting
        // up to 5 minutes behind Forex Factory's pacing (one request per 300 s).
        {"Economic calendar", [](Callback<QJsonDocument> c) {
           getJson("https://nfs.faireconomy.media/ff_calendar_thisweek.json", {}, c, GetOpts{30 * 60, false});
         }},
    };
    checks << feeds::probes();
    auto out = std::make_shared<Probes>();
    auto join = Join::make(checks.size(), [out, cb] { cb(Result<Probes>::success(*out)); });
    for (const auto& [name, fn] : checks) {
      auto t = std::make_shared<QElapsedTimer>();
      t->start();
      fn([out, join, t, name = name](const Result<QJsonDocument>& r) {
        out->insert(name, r.ok() ? Probe{true, t->elapsed(), {}} : Probe{false, 0, r.error->message});
        join->one();
      });
    }
  });
}

namespace testing {
void reset() {
  data::testing::clearFetchState();
  feeds::testing::reset();
  healthMap().clear();
  binanceHostIdx = 0;
  binanceKnownGood = false;
  binanceProbe() = BinanceProbe{};
  lastBinanceSymbols().reset();
  usTickers().reset();
  nqClass().clear();
  yahooDownUntil = 0;
}
void setOffline(const QString& dir) { fixturesDir() = dir; }
}
}  // namespace charts::data
