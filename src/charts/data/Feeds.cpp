// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/feeds.js: your own keys (Alpaca, Polygon/Massive, OANDA) in front of the public sources.
#include "charts/data/Feeds.h"
#include "core/Paths.h"
#include "net/Sources.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimeZone>
#include <QHash>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace charts::data::feeds {

// Key-based sources: each key goes only to its own host, in a header (never the URL, never a log).
static const sources::Registrar regAlpaca({"alpaca", "Alpaca Markets (market data API)", {"data.alpaca.markets"},
    sources::Access::Key, 350, "US stock bars (IEX feed on a free account, SIP on a paid one), with the user's own key",
    "Used only with the user's own Alpaca key from <data>/charts/feeds.json, under the user's Alpaca account terms "
    "(free plan: 200 requests per minute; we stay under 3 per second)", {"charts/charts"}});
static const sources::Registrar regPolygon({"polygon", "Massive (ex-Polygon.io)", {"api.massive.com"},
    sources::Access::Key, 250, "US stock aggregates and CME futures aggregates (NQ, MNQ, ES, MES), with the user's own key",
    "Used only with the user's own Polygon/Massive key from <data>/charts/feeds.json, under the user's plan "
    "(the free plan allows 5 requests per minute and is end-of-day only; paid plans are unlimited)", {"charts/charts"}});
static const sources::Registrar regOanda({"oanda", "OANDA v20 REST API", {"api-fxpractice.oanda.com", "api-fxtrade.oanda.com"},
    sources::Access::Key, 100, "Forex, metals and CFD candles (mid prices) for the instruments of the user's account",
    "Used only with the user's own OANDA token from <data>/charts/feeds.json (practice or live server), under the "
    "user's OANDA account terms; OANDA allows 120 requests per second per connection, we stay far below", {"charts/charts"}});

namespace {
const QString ALPACA = "https://data.alpaca.markets";
const QString POLYGON = "https://api.massive.com";
const QString OANDA_PRACTICE = "https://api-fxpractice.oanda.com";
const QString OANDA_LIVE = "https://api-fxtrade.oanda.com";

struct Memo { qint64 mtime = -1; QJsonObject value; };
Memo& memo() { static Memo m; return m; }

QJsonObject alpacaCfg() {
  const QJsonObject a = feedConfig().value("alpaca").toObject();
  return !a.value("keyId").toString().isEmpty() && !a.value("secretKey").toString().isEmpty() ? a : QJsonObject();
}
QJsonObject polygonCfg() {
  const QJsonObject c = feedConfig();
  const QJsonObject p = c.contains("polygon") && !c.value("polygon").isNull() ? c.value("polygon").toObject() : c.value("massive").toObject();
  return !p.value("apiKey").toString().isEmpty() ? p : QJsonObject();
}
QJsonObject oandaCfg() {
  const QJsonObject o = feedConfig().value("oanda").toObject();
  return !o.value("token").toString().isEmpty() ? o : QJsonObject();
}
QString oandaHost() { return oandaCfg().value("account").toString() == "live" ? OANDA_LIVE : OANDA_PRACTICE; }
QString jsStr(const QJsonValue& v) { return v.isString() ? v.toString() : v.isDouble() ? QString::number(v.toDouble()) : QString(); }

// ---------- requests ----------
Error feedError(const QString& message, int status) { return Error{message, status, true, {}}; }
// A provider that rate limits or refuses the key is skipped for a minute, so every chart on screen
// doesn't hit it again; the public sources carry the charts meanwhile.
struct Rest { qint64 until = 0; QString why; int status = 0; };
QHash<QString, Rest>& resting() { static QHash<QString, Rest> r; return r; }

void getFeed(const QString& name, const QString& url, const Headers& headers, Callback<QJsonDocument> cb) {
  const auto rest = resting().constFind(name);
  if (rest != resting().constEnd() && rest->until > QDateTime::currentMSecsSinceEpoch()) {
    const Error e = feedError(rest->why, rest->status);
    later([cb, e] { cb(Result<QJsonDocument>::failure(e)); });
    return;
  }
  net::Request r;
  r.url = QUrl(url);
  r.headers = {{"Accept", "application/json"}};
  r.headers << headers;
  r.timeoutMs = timeoutMs();
  r.ttlSec = 0;
  r.staleOnError = false;   // nothing from a key provider is written to the disk cache
  send(r, [cb, name](const net::Reply& rep) {
    if (rep.status == 0 && !rep.error.isEmpty()) {
      cb(Result<QJsonDocument>::failure(feedError(name + " couldn't be reached", 0)));
      return;
    }
    if (rep.status < 200 || rep.status >= 300) {
      // Plain words for the chart; the provider's own message stays on the error for logs.
      QString detail;
      const QJsonObject j = QJsonDocument::fromJson(rep.body).object();
      for (const char* k : {"message", "errorMessage", "error"})
        if (!jsStr(j.value(k)).isEmpty()) { detail = jsStr(j.value(k)).left(200); break; }
      const int st = rep.status;
      const QString why = st == 401 ? name + " refused your key"
                        : st == 403 ? "your " + name + " plan doesn't include this"
                        : st == 429 ? name + " is rate limiting your key"
                                    : name + " returned an error";
      if (st == 401 || st == 429) resting().insert(name, Rest{QDateTime::currentMSecsSinceEpoch() + 60000, why, st});
      Error e = feedError(why, st);
      e.detail = detail;
      cb(Result<QJsonDocument>::failure(e));
      return;
    }
    parseJson(rep.body, [cb, name](const QJsonDocument& d, const QString& err) {
      if (!err.isEmpty()) cb(Result<QJsonDocument>::failure(Error{name + " sent an answer that isn't JSON (" + err + ")", 0, false, {}}));
      else cb(Result<QJsonDocument>::success(d));
    });
  });
}

// ---------- shared ----------
QHash<qint64, int>& nyOffset() { static QHash<qint64, int> m; return m; }

// How far back to ask for `want` bars of `base`, with room for weekends, holidays and a stock's
// 6.5-hour day.
QString startFor(const QString& base, double want, qint64 earliestMs) {
  const double sec = tfSeconds(base);
  const double tradingDays = intraday(base) ? want / (23400 / sec) : want;
  const double days = std::ceil(tradingDays * 1.5) + 5;
  const qint64 t = std::max(qint64(double(QDateTime::currentMSecsSinceEpoch()) - days * 86400e3), earliestMs);
  return isoDate(t);
}

// Daily bars carry the trading date as UTC midnight, like every other source in the app.
qint64 dayOf(qint64 sec) { return qint64(std::floor(double(sec) / 86400)) * 86400; }

Bars finish(Bars bars, const QString& base, const QString& tf, int limit) {
  std::stable_sort(bars.begin(), bars.end(), [](const Bar& a, const Bar& b) { return a.time < b.time; });
  Bars out;
  for (const Bar& b : bars) if (out.isEmpty() || b.time > out.last().time) out.push_back(b);
  return base == tf ? sliceLast(out, limit) : sliceLast(resampleTo(out, tf), limit);
}

qint64 utcMs(int y, int m0, int d, int h = 0, int mi = 0) {
  return QDateTime(QDate(y, 1, 1).addMonths(m0).addDays(d - 1), QTime(h, mi), QTimeZone::UTC).toMSecsSinceEpoch();
}

const QList<QPair<QString, QString>> ALPACA_TF = {{"1m", "1Min"}, {"5m", "5Min"}, {"15m", "15Min"}, {"30m", "30Min"}, {"1D", "1Day"}};
const QList<QPair<QString, QPair<int, QString>>> POLYGON_TF = {
    {"1m", {1, "minute"}}, {"5m", {5, "minute"}}, {"15m", {15, "minute"}}, {"30m", {30, "minute"}}, {"1D", {1, "day"}}};
const QList<QPair<QString, QString>> FUT_RES = {{"1m", "1min"}, {"5m", "5min"}, {"15m", "15min"}, {"30m", "30min"}, {"1h", "1hour"}, {"1D", "1session"}};
const QList<QPair<QString, QString>> OANDA_TF = {{"1m", "M1"}, {"5m", "M5"}, {"15m", "M15"}, {"30m", "M30"}, {"1h", "H1"}, {"4h", "H4"}, {"1D", "D"}};
template <class V> QStringList keysOf(const QList<QPair<QString, V>>& l) { QStringList k; for (const auto& p : l) k << p.first; return k; }
template <class V> V valueOf(const QList<QPair<QString, V>>& l, const QString& key) { for (const auto& p : l) if (p.first == key) return p.second; return V{}; }

double numOr0(const QJsonValue& v) { return v.isDouble() ? v.toDouble() : v.isString() ? v.toString().toDouble() : 0; }

// Loop over Polygon's pages (at most 6, until `want` rows were taken). eachRow returns true for a row it kept.
void polygonPages(const QString& firstUrl, int want, std::function<bool(const QJsonObject&)> eachRow, std::function<void(const std::optional<Error>&)> done) {
  struct St { QString url; int page = 0; int n = 0; };
  auto st = std::make_shared<St>(St{firstUrl, 0, 0});
  auto step = std::make_shared<std::function<void()>>();
  const Headers headers = {{"Authorization", "Bearer " + polygonCfg().value("apiKey").toString().toUtf8()}};
  *step = [st, step, want, eachRow, done, headers]() {
    if (st->url.isEmpty() || st->page >= 6 || st->n >= want) { done(std::nullopt); *step = nullptr; return; }
    ++st->page;
    getFeed("Polygon", st->url, headers, [st, step, eachRow, done](const Result<QJsonDocument>& r) {
      if (!r.ok()) { done(r.error); *step = nullptr; return; }
      const QJsonObject j = r.value.object();
      for (const auto& v : j.value("results").toArray()) if (eachRow(v.toObject())) st->n++;
      // next_url is Polygon's own link to the next page; the key goes in the header, never the URL.
      const QString next = j.value("next_url").toString();
      // https too: the key goes with it in the Authorization header
      const QUrl nu(next);
      st->url = !next.isEmpty() && nu.scheme() == "https" && nu.host() == QUrl(POLYGON).host() ? next : QString();
      (*step)();
    });
  };
  (*step)();
}
}  // namespace

QString feedsFile() { return paths::data("charts") + "/feeds.json"; }

QJsonObject feedConfig() {
  const QFileInfo fi(feedsFile());
  if (!fi.exists()) return {};
  const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
  if (mtime != memo().mtime) {
    QJsonObject value;
    QFile f(fi.filePath());
    if (f.open(QIODevice::ReadOnly)) {
      QJsonParseError pe;
      const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
      if (pe.error != QJsonParseError::NoError)
        qWarning().noquote() << "data/feeds.json isn't valid JSON, so your own keys are off:" << pe.errorString();
      else value = d.object();
    }
    memo() = Memo{mtime, value};
  }
  return memo().value;
}

bool hasOanda() { return !oandaCfg().isEmpty(); }
QStringList configured() {
  QStringList out;
  if (!alpacaCfg().isEmpty()) out << "alpaca";
  if (!polygonCfg().isEmpty()) out << "polygon";
  if (!oandaCfg().isEmpty()) out << "oanda";
  return out;
}
QString alpacaName() {
  const QString feed = jsStr(alpacaCfg().value("feed"));
  return "Alpaca (" + (feed.isEmpty() ? QString("iex") : feed).toUpper() + ")";
}
bool isKeyProvider(const QString& name) {
  static const QRegularExpression re(R"(^(Alpaca|Polygon|OANDA)\b)");
  return re.match(name).hasMatch();
}

int nyOffsetHours(qint64 sec) {
  const qint64 day = qint64(std::floor(double(sec) / 86400));
  auto it = nyOffset().constFind(day);
  if (it != nyOffset().constEnd()) return *it;
  static const QTimeZone ny("America/New_York");
  const int h = QDateTime::fromSecsSinceEpoch(day * 86400 + 12 * 3600, ny).time().hour();
  nyOffset().insert(day, 12 - h);
  return 12 - h;
}

bool inUsSession(qint64 sec) {
  const qint64 local = (((sec - qint64(nyOffsetHours(sec)) * 3600) % 86400) + 86400) % 86400;
  return local >= 34200 && local < 57600;
}

// ---------- Alpaca (US stocks) ----------
void alpacaCandles(const QString& ticker, const QString& tf, int limit, Callback<Bars> cb) {
  const QJsonObject k = alpacaCfg();
  if (k.isEmpty()) { later([cb] { cb(Result<Bars>::failure(Error{QT_TRANSLATE_NOOP("charts", "No Alpaca key in data/feeds.json")})); }); return; }
  const QString base = nativeBaseOf(tf, keysOf(ALPACA_TF));
  if (base.isEmpty()) { later([cb, tf] { cb(Result<Bars>::failure(Error{"Alpaca can't build a " + tf + " chart"})); }); return; }
  const bool isIntra = intraday(base);
  const int ratio = std::max(1, int(std::lround(tfSeconds(tf) / tfSeconds(base))));
  const int want = std::min(limit * ratio + ratio, 60000);
  const Headers headers = {{"APCA-API-KEY-ID", k.value("keyId").toString().toUtf8()},
                           {"APCA-API-SECRET-KEY", k.value("secretKey").toString().toUtf8()}};
  const QString feed = jsStr(k.value("feed")).isEmpty() ? QString("iex") : jsStr(k.value("feed"));
  const QString start = startFor(base, want, utcMs(2016, 0, 1));
  struct St { Bars out; QString token; int page = 0; };
  auto st = std::make_shared<St>();
  auto step = std::make_shared<std::function<void()>>();
  *step = [=]() {
    if (st->page >= 12 || st->out.size() >= want) { cb(Result<Bars>::success(finish(st->out, base, tf, limit))); *step = nullptr; return; }
    QList<QPair<QString, QString>> q = {{"symbols", ticker}, {"timeframe", valueOf(ALPACA_TF, base)}, {"start", start},
                                        {"limit", "10000"}, {"feed", feed}, {"adjustment", "split"}, {"sort", "desc"}};
    if (!st->token.isEmpty()) q << qMakePair(QString("page_token"), st->token);
    ++st->page;
    getFeed("Alpaca", ALPACA + "/v2/stocks/bars?" + formEncode(q), headers, [=](const Result<QJsonDocument>& r) {
      if (!r.ok()) { cb(Result<Bars>::failure(*r.error)); *step = nullptr; return; }
      const QJsonObject j = r.value.object();
      for (const auto& v : j.value("bars").toObject().value(ticker).toArray()) {
        const QJsonObject b = v.toObject();
        const double ms = parseIsoMs(b.value("t").toString());
        if (std::isnan(ms)) continue;
        const qint64 t = qint64(std::floor(ms / 1000));
        if (isIntra && !inUsSession(t)) continue;
        st->out.push_back(Bar{isIntra ? t : dayOf(t), b.value("o").toDouble(), b.value("h").toDouble(), b.value("l").toDouble(),
                              b.value("c").toDouble(), numOr0(b.value("v"))});
      }
      st->token = j.value("next_page_token").toString();
      if (st->token.isEmpty()) { cb(Result<Bars>::success(finish(st->out, base, tf, limit))); *step = nullptr; return; }
      (*step)();
    });
  };
  (*step)();
}

// ---------- Polygon / Massive (US stocks) ----------
void polygonCandles(const QString& ticker, const QString& tf, int limit, Callback<Bars> cb) {
  if (polygonCfg().isEmpty()) { later([cb] { cb(Result<Bars>::failure(Error{QT_TRANSLATE_NOOP("charts", "No Polygon key in data/feeds.json")})); }); return; }
  const QString base = nativeBaseOf(tf, keysOf(POLYGON_TF));
  if (base.isEmpty()) { later([cb, tf] { cb(Result<Bars>::failure(Error{"Polygon can't build a " + tf + " chart"})); }); return; }
  const bool isIntra = intraday(base);
  const int ratio = std::max(1, int(std::lround(tfSeconds(tf) / tfSeconds(base))));
  const int want = std::min(limit * ratio + ratio, 60000);
  const auto [mult, span] = valueOf(POLYGON_TF, base);
  const QString from = startFor(base, want, utcMs(2003, 0, 1));
  const QString to = isoDate(QDateTime::currentMSecsSinceEpoch() + 86400000);
  auto out = std::make_shared<Bars>();
  polygonPages(
      POLYGON + "/v2/aggs/ticker/" + encodeUriComponent(ticker) + "/range/" + QString::number(mult) + "/" + span + "/" + from +
          "/" + to + "?adjusted=true&sort=desc&limit=50000",
      want,
      [out, isIntra](const QJsonObject& r) {
        const qint64 t = qint64(std::floor(r.value("t").toDouble() / 1000));
        if (isIntra && !inUsSession(t)) return false;
        out->push_back(Bar{isIntra ? t : dayOf(t), r.value("o").toDouble(), r.value("h").toDouble(), r.value("l").toDouble(),
                           r.value("c").toDouble(), numOr0(r.value("v"))});
        return true;
      },
      [out, base, tf, limit, cb](const std::optional<Error>& e) {
        if (e) cb(Result<Bars>::failure(*e));
        else cb(Result<Bars>::success(finish(*out, base, tf, limit)));
      });
}

Chain stockChain(const QString& ticker, const QString& tf, int limit) {
  Chain chain;
  if (!alpacaCfg().isEmpty())
    chain << ChainStep{alpacaName(), [=](Callback<Bars> cb) { alpacaCandles(ticker, tf, limit, cb); }};
  if (!polygonCfg().isEmpty())
    chain << ChainStep{"Polygon", [=](Callback<Bars> cb) { polygonCandles(ticker, tf, limit, cb); }};
  return chain;
}

// ---------- Polygon / Massive (CME futures) ----------
// Equity index futures expire on the third Friday of March, June, September and December. Trading
// moves to the next contract 8 days before expiry (CME's roll date), so that's when the front contract changes.
static qint64 thirdFriday(int y, int m) {
  const int first = QDate(y, m + 1, 1).dayOfWeek() % 7;   // JS getUTCDay: Sunday = 0
  return utcMs(y, m, 1 + ((12 - first) % 7) + 14, 13, 30);
}

QString frontContract(const QString& root, qint64 nowMs) {
  static const QString MONTH_CODES = "FGHJKMNQUVXZ";
  const qint64 now = nowMs < 0 ? QDateTime::currentMSecsSinceEpoch() : nowMs;
  int y = QDateTime::fromMSecsSinceEpoch(now, QTimeZone::UTC).date().year();
  for (int k = 0; k < 12; k++) {
    const int m = 2 + 3 * (k % 4);
    if (k && k % 4 == 0) y++;
    if (now < thirdFriday(y, m) - 8 * qint64(86400000)) return root + MONTH_CODES[m] + QString::number(((y % 10) + 10) % 10);
  }
  return {};
}

void polygonFuturesCandles(const QString& contract, const QString& tf, int limit, Callback<Bars> cb) {
  if (polygonCfg().isEmpty()) { later([cb] { cb(Result<Bars>::failure(Error{QT_TRANSLATE_NOOP("charts", "No Polygon key in data/feeds.json")})); }); return; }
  const QString base = nativeBaseOf(tf, keysOf(FUT_RES));
  if (base.isEmpty()) { later([cb, tf] { cb(Result<Bars>::failure(Error{"Polygon can't build a " + tf + " futures chart"})); }); return; }
  const bool isIntra = intraday(base);
  const double sec = tfSeconds(base);
  const int ratio = std::max(1, int(std::lround(tfSeconds(tf) / sec)));
  const int want = std::min(limit * ratio + ratio, 60000);
  // Futures trade about 23 hours a day, 5 days a week.
  const double days = isIntra ? std::ceil(((want * sec) / 82800) * 1.5) + 4 : std::ceil(want * 1.5) + 5;
  const QString since = isoDate(qint64(double(QDateTime::currentMSecsSinceEpoch()) - days * 86400e3));
  auto out = std::make_shared<Bars>();
  polygonPages(
      POLYGON + "/futures/v1/aggs/" + encodeUriComponent(contract) + "?resolution=" + valueOf(FUT_RES, base) +
          "&window_start.gte=" + since + "&sort=window_start.desc&limit=50000",
      want,
      [out, isIntra](const QJsonObject& r) {
        double t;
        if (isIntra) {
          const QJsonValue ws = r.value("window_start");
          t = std::floor((ws.isString() ? ws.toString().toDouble() : ws.toDouble(NaN)) / 1e9);
        } else {
          const double ms = parseIsoMs(r.value("session_end_date").toString() + "T00:00:00Z");
          t = ms / 1000;
        }
        if (!std::isfinite(t) || r.value("close").isNull() || r.value("close").isUndefined()) return false;
        out->push_back(Bar{qint64(t), r.value("open").toDouble(), r.value("high").toDouble(), r.value("low").toDouble(),
                           r.value("close").toDouble(), numOr0(r.value("volume"))});
        return true;
      },
      [out, base, tf, limit, cb](const std::optional<Error>& e) {
        if (e) cb(Result<Bars>::failure(*e));
        else cb(Result<Bars>::success(finish(*out, base, tf, limit)));
      });
}

Chain futuresChain(const QString& root, const QString& tf, int limit) {
  if (polygonCfg().isEmpty()) return {};
  const QString contract = frontContract(root);
  return {ChainStep{"Polygon " + contract, [=](Callback<Bars> cb) { polygonFuturesCandles(contract, tf, limit, cb); }}};
}

// ---------- OANDA (forex, metals, your account's other instruments) ----------
void oandaCandles(const QString& instrument, const QString& tf, int limit, Callback<Bars> cb) {
  const QJsonObject k = oandaCfg();
  if (k.isEmpty()) { later([cb] { cb(Result<Bars>::failure(Error{QT_TRANSLATE_NOOP("charts", "No OANDA token in data/feeds.json")})); }); return; }
  const QString base = nativeBaseOf(tf, keysOf(OANDA_TF));
  if (base.isEmpty()) { later([cb, tf] { cb(Result<Bars>::failure(Error{"OANDA can't build a " + tf + " chart"})); }); return; }
  const int ratio = std::max(1, int(std::lround(tfSeconds(tf) / tfSeconds(base))));
  QList<QPair<QString, QString>> q = {{"price", "M"}, {"granularity", valueOf(OANDA_TF, base)},
                                      {"count", QString::number(std::min(limit * ratio + ratio, 5000))}};   // OANDA's most per request
  // Daily bars start at UTC midnight, like every other source, rather than OANDA's 5pm New York.
  if (base == "1D") q << qMakePair(QString("dailyAlignment"), QString("0")) << qMakePair(QString("alignmentTimezone"), QString("UTC"));
  getFeed("OANDA", oandaHost() + "/v3/instruments/" + encodeUriComponent(instrument) + "/candles?" + formEncode(q),
          {{"Authorization", "Bearer " + k.value("token").toString().toUtf8()}},
          [=](const Result<QJsonDocument>& r) {
            if (!r.ok()) { cb(Result<Bars>::failure(*r.error)); return; }
            Bars out;
            for (const auto& v : r.value.object().value("candles").toArray()) {
              const QJsonObject c = v.toObject();
              const double ms = parseIsoMs(c.value("time").toString());
              if (!std::isfinite(ms) || !c.value("mid").isObject()) continue;
              const QJsonObject mid = c.value("mid").toObject();
              out.push_back(Bar{qint64(std::floor(ms / 1000)), numOr0(mid.value("o")), numOr0(mid.value("h")), numOr0(mid.value("l")),
                                numOr0(mid.value("c")), numOr0(c.value("volume"))});
            }
            cb(Result<Bars>::success(finish(out, base, tf, limit)));
          });
}

Chain fxChain(const QString& pair, const QString& tf, int limit) {
  if (oandaCfg().isEmpty()) return {};
  const QString instrument = pair.left(3) + "_" + pair.mid(3);
  return {ChainStep{"OANDA", [=](Callback<Bars> cb) { oandaCandles(instrument, tf, limit, cb); }}};
}

QList<QPair<QString, std::function<void(Callback<QJsonDocument>)>>> probes() {
  QList<QPair<QString, std::function<void(Callback<QJsonDocument>)>>> out;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const QString since = isoDate(now - 10 * qint64(86400000));
  const QJsonObject k = alpacaCfg();
  if (!k.isEmpty()) {
    const QString feed = jsStr(k.value("feed")).isEmpty() ? QString("iex") : jsStr(k.value("feed"));
    const Headers h = {{"APCA-API-KEY-ID", k.value("keyId").toString().toUtf8()}, {"APCA-API-SECRET-KEY", k.value("secretKey").toString().toUtf8()}};
    out << qMakePair(QString("Alpaca (your key)"), std::function<void(Callback<QJsonDocument>)>([=](Callback<QJsonDocument> cb) {
      getFeed("Alpaca", ALPACA + "/v2/stocks/bars?symbols=SPY&timeframe=1Day&limit=1&start=" + since + "&feed=" + encodeUriComponent(feed), h, cb);
    }));
  }
  if (!polygonCfg().isEmpty()) {
    const Headers h = {{"Authorization", "Bearer " + polygonCfg().value("apiKey").toString().toUtf8()}};
    out << qMakePair(QString("Polygon (your key)"), std::function<void(Callback<QJsonDocument>)>([=](Callback<QJsonDocument> cb) {
      getFeed("Polygon", POLYGON + "/v2/aggs/ticker/SPY/range/1/day/" + since + "/" + isoDate(now) + "?limit=1", h, cb);
    }));
  }
  if (!oandaCfg().isEmpty()) {
    const Headers h = {{"Authorization", "Bearer " + oandaCfg().value("token").toString().toUtf8()}};
    const QString host = oandaHost();
    out << qMakePair(QString("OANDA (your key)"), std::function<void(Callback<QJsonDocument>)>([=](Callback<QJsonDocument> cb) {
      getFeed("OANDA", host + "/v3/instruments/EUR_USD/candles?granularity=D&count=1", h, cb);
    }));
  }
  return out;
}

namespace testing {
void reset() {
  memo() = Memo{};
  resting().clear();
  nyOffset().clear();
}
}
}  // namespace charts::data::feeds
