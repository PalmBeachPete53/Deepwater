// Port of quant-terminal test/feeds.test.js: your own data keys (feeds.json), with a fake network: no live calls
// and no real keys. Viewers asked to plug in their own feed instead of the free public sources (YouTube, Oct 2026).
#include "Check.h"
#include "FakeNet.h"
#include "charts/data/Providers.h"
#include "charts/data/Feeds.h"
#include <QFile>
#include <QDateTime>
#include <QTimeZone>

using namespace charts;
using namespace charts::data;

namespace {
const QString KEY_ID = "PKTESTKEYID", SECRET = "TESTSECRET", API_KEY = "POLYTESTKEY", TOKEN = "OANDATESTTOKEN";
const QStringList ALL_KEYS = {KEY_ID, SECRET, API_KEY, TOKEN};

qint64 stamp = QDateTime::currentSecsSinceEpoch();
void touch(const QString& file) {
  stamp += 5;   // a clearly newer mtime, whatever the file system's clock resolution
  QFile f(file);
  if (f.open(QIODevice::ReadWrite)) f.setFileTime(QDateTime::fromSecsSinceEpoch(stamp, QTimeZone::UTC), QFileDevice::FileModificationTime);
}
void writeFeeds(const std::optional<QJsonObject>& obj) {
  const QString file = feeds::feedsFile();
  if (!obj) { QFile::remove(file); return; }
  QFile f(file);
  f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write(QJsonDocument(*obj).toJson(QJsonDocument::Compact));
  f.close();
  touch(file);
}

// 5-minute bars for one day, 08:00 to 19:55 New York (EDT, UTC-4): pre-market, the session, after hours.
const qint64 DAY = qint64(parseIsoMs("2026-10-14T00:00:00Z") / 1000);
QVector<qint64> fiveMin() {
  QVector<qint64> t;
  for (int i = 0; i < 144; i++) t << DAY + 12 * 3600 + i * 300;
  return t;
}
QJsonObject bar(int k) { return {{"o", 100 + k}, {"h", 101 + k}, {"l", 99 + k}, {"c", 100.5 + k}, {"v", 1000 + k}}; }
QString iso(qint64 sec) { return isoMs(sec * 1000); }

int alpacaStatus = 200, polygonFuturesStatus = 200, oandaStatus = 200;

fakenet::Reply handler(const QUrl& u) {
  const QString host = u.host();
  if (host == "data.alpaca.markets") {
    if (alpacaStatus != 200) return fakenet::Reply::json(QJsonObject{{"message", "forbidden."}}, alpacaStatus);
    const QString sym = fakenet::param(u, "symbols");
    QJsonArray rows;
    if (fakenet::param(u, "timeframe") == "1Day")
      for (int i = 0; i < 30; i++) {
        QJsonObject b = bar(i);
        b["t"] = iso(DAY - i * 86400 + 4 * 3600);
        rows.append(b);
      }
    else {
      const auto ts = fiveMin();
      for (int i = ts.size() - 1; i >= 0; --i) {
        QJsonObject b = bar(i);
        b["t"] = iso(ts[i]);
        rows.append(b);
      }
    }
    CHECK_EQ(fakenet::param(u, "sort"), QString("desc"));
    const bool page2 = fakenet::param(u, "page_token") == "p2";
    const int half = (rows.size() + 1) / 2;
    QJsonArray part;
    for (int i = page2 ? half : 0; i < (page2 ? rows.size() : half); i++) part.append(rows[i]);
    return fakenet::Reply::json(QJsonObject{{"bars", QJsonObject{{sym, part}}}, {"next_page_token", page2 ? QJsonValue() : QJsonValue("p2")}});
  }
  if (host == "api.massive.com" && u.path().startsWith("/futures/")) {
    if (polygonFuturesStatus != 200) return fakenet::Reply::json(QJsonObject{{"status", "NOT_AUTHORIZED"}}, polygonFuturesStatus);
    CHECK_EQ(fakenet::param(u, "sort"), QString("window_start.desc"));
    const auto ts = fiveMin();
    QJsonArray rows;
    for (int i = ts.size() - 1; i >= 0; --i)
      rows.append(QJsonObject{{"window_start", double(ts[i]) * 1e9}, {"open", 20000 + i}, {"high", 20001 + i}, {"low", 19999 + i},
                              {"close", 20000.5 + i}, {"volume", 50}});
    return fakenet::Reply::json(QJsonObject{{"results", rows}, {"status", "OK"}});
  }
  if (host == "api-fxpractice.oanda.com" || host == "api-fxtrade.oanda.com") {
    if (oandaStatus != 200) return fakenet::Reply::json(QJsonObject{{"errorMessage", "Insufficient authorization"}}, oandaStatus);
    const bool daily = fakenet::param(u, "granularity") == "D";
    QVector<qint64> times;
    if (daily) for (int i = 0; i < 5; i++) times << DAY + i * 86400;
    else times = fiveMin();
    QJsonArray candles;
    for (int i = 0; i < times.size(); i++)
      candles.append(QJsonObject{{"time", iso(times[i]).replace(".000Z", ".000000000Z")}, {"complete", true}, {"volume", 12},
                                 {"mid", QJsonObject{{"o", QString::number(2000 + i)}, {"h", QString::number(2001 + i)},
                                                     {"l", QString::number(1999 + i)}, {"c", QString::number(2000.5 + i)}}}});
    return fakenet::Reply::json(QJsonObject{{"candles", candles}});
  }
  if (host == "query1.finance.yahoo.com") {
    const auto ts = fiveMin();
    QJsonArray t, o, h, l, c, v;
    for (qint64 x : ts) { t.append(double(x)); o.append(25000); h.append(25010); l.append(24990); c.append(25005); v.append(7); }
    return fakenet::Reply::json(QJsonObject{{"chart", QJsonObject{{"result", QJsonArray{QJsonObject{
        {"meta", QJsonObject{{"gmtoffset", -14400}}}, {"timestamp", t},
        {"indicators", QJsonObject{{"quote", QJsonArray{QJsonObject{{"open", o}, {"high", h}, {"low", l}, {"close", c}, {"volume", v}}}}}}}}}}}});
  }
  if (host == "api.massive.com") {
    const auto ts = fiveMin();
    QJsonArray rows;
    for (int i = ts.size() - 1; i >= 0; --i) {
      QJsonObject b = bar(i);
      b["t"] = double(ts[i]) * 1000;
      rows.append(b);
    }
    return fakenet::Reply::json(QJsonObject{{"results", rows}, {"status", "OK"}});
  }
  return fakenet::Reply::text("not mocked", 404);
}

// Calls that carry a key to a host other than its own.
QStringList leaks() {
  const QHash<QString, QStringList> own = {{"data.alpaca.markets", {KEY_ID, SECRET}}, {"api.massive.com", {API_KEY}},
                                           {"api-fxpractice.oanda.com", {TOKEN}}, {"api-fxtrade.oanda.com", {TOKEN}}};
  QStringList out;
  for (const auto& c : fakenet::calls()) {
    QString carries = c.url;
    for (const auto& h : c.headers) carries += QString::fromUtf8(h.first + ":" + h.second);
    for (const QString& k : ALL_KEYS)
      if (carries.contains(k) && !own.value(c.host).contains(k)) out << c.host + " got " + k;
  }
  return out;
}

// Every test can run alone (charts_data_tests oanda): without this, a filtered run skipped the first test's setup and
// sent the fake keys to the real hosts. Idempotent, so it doesn't disturb a full run.
void fakeNetOn() {
  data::testing::setOffline({});
  data::testing::setTimeoutMs(300);
  fakenet::install(handler);
}

Result<CandlesResult> candlesNow(const QString& s, const QString& tf, int limit) {
  fakeNetOn();
  return fakenet::await<CandlesResult>([=](Callback<CandlesResult> cb) { getCandles(s, tf, limit, nullptr, cb); });
}
bool anyNote(const CandlesResult& r, const QString& re) {
  for (const auto& n : r.notes) if (n.contains(QRegularExpression(re))) return true;
  return false;
}
QList<fakenet::Call> callsTo(const QString& host) {
  QList<fakenet::Call> out;
  for (const auto& c : fakenet::calls()) if (c.host == host) out << c;
  return out;
}
}  // namespace

TEST("feeds: with no feeds.json, stock charts use only the public sources") {
  data::testing::reset();
  data::testing::setOffline({});
  data::testing::setTimeoutMs(300);
  fakenet::calls().clear();
  fakenet::install(handler);
  writeFeeds(std::nullopt);
  CHECK_EQ(feeds::configured(), QStringList());
  CHECK_EQ(feeds::stockChain("AAPL", "1D", 100).size(), 0);
}

TEST("feeds: an Alpaca key takes over US stock charts: session bars only, keys in headers only") {
  writeFeeds(QJsonObject{{"alpaca", QJsonObject{{"keyId", KEY_ID}, {"secretKey", SECRET}}}});
  fakenet::calls().clear();
  const auto r = candlesNow("US:AAPL", "5m", 50);
  CHECK_EQ(r.value.provider, QString("Alpaca (IEX)"));
  CHECK_EQ(r.value.candles.size(), 50);
  bool session = true;
  for (const Bar& b : r.value.candles) session = session && feeds::inUsSession(b.time);
  CHECK(session);   // pre-market and after-hours bars are left out
  CHECK_EQ(r.value.candles.last().time, DAY + 19 * 3600 + 55 * 60);   // last bar is 15:55 New York
  for (int i = 1; i < r.value.candles.size(); i++) CHECK(r.value.candles[i].time > r.value.candles[i - 1].time);
  CHECK(anyNote(r.value, "IEX"));   // the chart says it's the IEX feed
  const auto alpaca = callsTo("data.alpaca.markets");
  CHECK_EQ(alpaca.size(), 2);       // follows Alpaca's next_page_token
  CHECK_EQ(QString::fromUtf8(alpaca.value(0).header("APCA-API-KEY-ID")), KEY_ID);
  for (const auto& c : alpaca) CHECK(!c.url.contains(SECRET) && !c.url.contains(KEY_ID));   // keys never go in the URL
  CHECK_EQ(leaks(), QStringList());
  CHECK_EQ(feeds::configured(), QStringList{"alpaca"});   // health lists names, never keys
}

TEST("feeds: daily bars from a key land on the trading date at UTC midnight, like the public sources") {
  const auto r = candlesNow("US:MSFT", "1D", 20);
  CHECK_EQ(r.value.provider, QString("Alpaca (IEX)"));
  for (const Bar& b : r.value.candles) CHECK(b.time % 86400 == 0);
  CHECK_EQ(r.value.candles.last().time, DAY);
}

TEST("feeds: weekly charts are built from the key's daily bars") {
  const auto r = candlesNow("US:NVDA", "1W", 3);
  CHECK_EQ(r.value.provider, QString("Alpaca (IEX)"));
  CHECK(r.value.candles.size() >= 3);
}

TEST("feeds: a refused Alpaca key falls back to Polygon, and the chart says why in plain words") {
  writeFeeds(QJsonObject{{"alpaca", QJsonObject{{"keyId", KEY_ID}, {"secretKey", SECRET}}}, {"polygon", QJsonObject{{"apiKey", API_KEY}}}});
  alpacaStatus = 401;
  fakenet::calls().clear();
  const auto r = candlesNow("US:TSLA", "15m", 10);
  CHECK_EQ(r.value.provider, QString("Polygon"));
  CHECK(r.value.candles.size() > 0);
  for (const Bar& b : r.value.candles) CHECK(feeds::inUsSession(b.time));
  CHECK(r.value.notes.contains("Alpaca refused your key, so this chart uses Polygon."));
  CHECK(!r.value.notes.join(" ").contains(QRegularExpression(R"(HTTP|401|\.com)")));
  const auto poly = callsTo("api.massive.com");
  CHECK(!poly.isEmpty());
  CHECK_EQ(QString::fromUtf8(poly.value(0).header("Authorization")), "Bearer " + API_KEY);
  CHECK(!poly.value(0).url.contains(API_KEY));   // the Polygon key goes in a header, not the URL
  CHECK_EQ(leaks(), QStringList());
  // A refused key rests for a minute instead of being asked again on every chart.
  fakenet::calls().clear();
  candlesNow("US:AMD", "15m", 10);
  CHECK_EQ(callsTo("data.alpaca.markets").size(), 0);
}

static QStringList* g_warnings = nullptr;
TEST("feeds: a broken feeds.json leaves the public sources working") {
  QFile f(feeds::feedsFile());
  f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write("{ not json");
  f.close();
  touch(feeds::feedsFile());
  QStringList warnings;
  g_warnings = &warnings;
  const auto old = qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& m) { if (g_warnings) *g_warnings << m; });
  CHECK_EQ(feeds::configured(), QStringList());
  qInstallMessageHandler(old);
  g_warnings = nullptr;
  bool mentioned = false;
  for (const auto& w : warnings) mentioned = mentioned || w.contains("feeds.json");
  CHECK(mentioned);
}

// ---------- CME futures (NQ, MNQ, ES, MES) ----------
TEST("feeds: the front contract rolls 8 days before the third Friday of each quarter month") {
  auto at = [](const char* d) { return feeds::frontContract("NQ", qint64(parseIsoMs(d))); };
  CHECK_EQ(at("2026-10-15T12:00:00Z"), QString("NQZ6"));
  CHECK_EQ(at("2026-12-09T12:00:00Z"), QString("NQZ6"));
  CHECK_EQ(at("2026-12-10T14:00:00Z"), QString("NQH7"));   // Dec 2026 expires Fri 18th, so it rolls Thu 10th
  CHECK_EQ(at("2027-03-12T12:00:00Z"), QString("NQM7"));
  CHECK_EQ(feeds::frontContract("MNQ", qint64(parseIsoMs("2026-01-02T12:00:00Z"))), QString("MNQH6"));
}

TEST("feeds: NQ, MNQ, ES and MES resolve and search as CME futures") {
  fakeNetOn();
  for (const auto& [typed, id] : QList<QPair<QString, QString>>{{"NQ", "CME:NQ1!"}, {"nq1!", "CME:NQ1!"}, {"MNQ", "CME:MNQ1!"},
                                                               {"CME_MINI:NQ1!", "CME:NQ1!"}, {"CME:ES1!", "CME:ES1!"}, {"MES1!", "CME:MES1!"}}) {
    const auto r = fakenet::await<Symbol>([typed = typed](Callback<Symbol> cb) { resolve(typed, nullptr, cb); });
    CHECK_EQ(r.value.id, id);
  }
  const auto mnq = fakenet::await<QList<SearchHit>>([](Callback<QList<SearchHit>> cb) { search("MNQ", nullptr, cb); });
  CHECK_EQ(mnq.value.value(0).symbol, QString("CME:MNQ1!"));
  const auto es = fakenet::await<QList<SearchHit>>([](Callback<QList<SearchHit>> cb) { search("ES", nullptr, cb); });
  CHECK_EQ(es.value.value(0).symbol, QString("CME:ES1!"));
  CHECK_EQ(es.value.value(0).type, QString("futures"));
}

TEST("feeds: without a Polygon key, futures chart Yahoo's delayed feed and say so") {
  writeFeeds(std::nullopt);
  const auto r = candlesNow("NQ1!", "5m", 30);
  CHECK_EQ(r.value.provider, QString("Yahoo (delayed)"));
  CHECK_EQ(r.value.candles.size(), 30);
  CHECK(anyNote(r.value, "^Delayed about 10 minutes"));
}

TEST("feeds: with a Polygon key, futures chart the live front contract, key in a header") {
  writeFeeds(QJsonObject{{"polygon", QJsonObject{{"apiKey", API_KEY}}}});
  fakenet::calls().clear();
  const auto r = candlesNow("CME:MNQ1!", "15m", 20);
  const QString contract = feeds::frontContract("MNQ");
  CHECK_EQ(r.value.provider, "Polygon " + contract);
  CHECK(r.value.candles.size() > 0 && r.value.candles.size() <= 20);
  CHECK(anyNote(r.value, "Front contract " + contract));
  fakenet::Call c;
  for (const auto& x : fakenet::calls()) if (x.url.contains("/futures/v1/aggs/")) { c = x; break; }
  CHECK(c.url.contains("/futures/v1/aggs/" + contract + "?"));
  CHECK_EQ(QString::fromUtf8(c.header("Authorization")), "Bearer " + API_KEY);
  CHECK_EQ(leaks(), QStringList());
}

TEST("feeds: a Polygon plan without futures falls back to the delayed chart, in plain words") {
  polygonFuturesStatus = 403;
  const auto r = candlesNow("CME:ES1!", "1h", 20);
  CHECK_EQ(r.value.provider, QString("Yahoo (delayed)"));
  CHECK(r.value.notes.contains("Your Polygon plan doesn't include this, so this chart uses Yahoo (delayed)."));
  polygonFuturesStatus = 200;
}

TEST("feeds: an OANDA token charts OANDA: symbols with mid prices, token in a header, practice server by default") {
  writeFeeds(QJsonObject{{"oanda", QJsonObject{{"token", TOKEN}}}});
  fakenet::calls().clear();
  const auto r = candlesNow("OANDA:NAS100_USD", "5m", 30);
  CHECK_EQ(r.value.provider, QString("OANDA"));
  CHECK_EQ(r.value.candles.size(), 30);
  CHECK(std::isfinite(r.value.candles.value(0).close) && r.value.candles.value(0).close > 0);
  CHECK(anyNote(r.value, "not exchange data"));
  const auto c = callsTo("api-fxpractice.oanda.com");
  CHECK(!c.isEmpty() && c[0].url.contains("/v3/instruments/NAS100_USD/candles?"));
  CHECK_EQ(QString::fromUtf8(c.value(0).header("Authorization")), "Bearer " + TOKEN);
  CHECK_EQ(leaks(), QStringList());
}

TEST("feeds: a live OANDA account uses the live server, and daily bars land on UTC midnight") {
  writeFeeds(QJsonObject{{"oanda", QJsonObject{{"token", TOKEN}, {"account", "live"}}}});
  fakenet::calls().clear();
  const auto r = candlesNow("OANDA:XAU_USD", "1D", 5);
  const auto c = callsTo("api-fxtrade.oanda.com");
  CHECK(!c.isEmpty() && c[0].url.contains("dailyAlignment=0") && c[0].url.contains("alignmentTimezone=UTC"));
  CHECK_EQ(r.value.candles.value(0).time % 86400, 0);
}

TEST("feeds: an OANDA token goes first for forex and metals; gold isn't called a token chart") {
  writeFeeds(QJsonObject{{"oanda", QJsonObject{{"token", TOKEN}}}});
  fakenet::calls().clear();
  const auto fx = candlesNow("EURUSD", "15m", 10);
  CHECK_EQ(fx.value.provider, QString("OANDA"));
  bool asked = false;
  for (const auto& x : fakenet::calls()) asked = asked || x.url.contains("/v3/instruments/EUR_USD/candles?");
  CHECK(asked);
  const auto gold = candlesNow("XAUUSD", "15m", 10);
  CHECK_EQ(gold.value.provider, QString("OANDA"));
  CHECK(!anyNote(gold.value, "backed by real gold"));
}

TEST("feeds: a refused OANDA token falls back to the public sources for forex and says why") {
  writeFeeds(QJsonObject{{"oanda", QJsonObject{{"token", TOKEN}}}});
  oandaStatus = 401;
  const auto r = candlesNow("GBPUSD", "1h", 10);
  CHECK(r.ok());
  CHECK(r.value.provider != "OANDA");
  CHECK(anyNote(r.value, "^OANDA refused your key"));
  oandaStatus = 200;
}

TEST("feeds: OANDA: symbols without a token say what to do") {
  writeFeeds(std::nullopt);
  const auto r = candlesNow("OANDA:SPX500_USD", "1h", 10);
  CHECK(!r.ok() && r.error->message.contains("need your own OANDA token"));
}
