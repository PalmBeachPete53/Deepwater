// Port of quant-terminal test/server.test.js: the local HTTP API (src/charts/data/ApiServer) started inside the test on
// a free port, offline fixtures (the JS QT_OFFLINE=1), requests through QNetworkAccessManager or a raw socket where the
// request itself must be malformed. Nothing leaves the machine: any outside request gets a "host not found".
// Differences from the JS file, on purpose: no web page ("/" is a JSON note, no vendor script, no ad-blocker check),
// /api/shutdown is refused (Deepwater is quit from its window), plus tests the JS doesn't have (port retry, SSE).
#include "Check.h"
#include "FakeNet.h"
#include "charts/core/Indicators.h"
#include "charts/data/ApiServer.h"
#include "charts/data/Alerts.h"
#include "charts/data/Imports.h"
#include "charts/data/Providers.h"
#include "charts/data/Store.h"
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <functional>

using namespace charts;

namespace {
using Headers = QList<QPair<QByteArray, QByteArray>>;
const Headers H = {{"x-qt", "1"}, {"Content-Type", "application/json"}};

quint16& portRef() { static quint16 p = 0; return p; }
QString base() { return QStringLiteral("http://127.0.0.1:%1").arg(portRef()); }

void setUp() {
  data::testing::reset();
  data::testing::setOffline(QStringLiteral(CHARTS_FIXTURES));
  fakenet::calls().clear();
  fakenet::install([](const QUrl&) { return fakenet::Reply::dns(); });   // offline: no outside host answers
  auto& s = api::Server::instance();
  if (!s.isListening()) {
    CHECK(s.start(0));
    portRef() = s.port();
  }
}

struct Http {
  int status = 0;
  QByteArray body;
  QJsonValue json() const {
    const QJsonDocument d = QJsonDocument::fromJson(body);
    if (d.isObject()) return d.object();
    if (d.isArray()) return d.array();
    return QJsonValue(QJsonValue::Undefined);
  }
  QJsonObject obj() const { return json().toObject(); }
  QJsonArray arr() const { return json().toArray(); }
};

QNetworkAccessManager& nam() {
  static QNetworkAccessManager* m = [] {
    auto* n = new QNetworkAccessManager;
    n->setProxy(QNetworkProxy::NoProxy);
    return n;
  }();
  return *m;
}

QNetworkReply* start(const QByteArray& method, const QString& path, const QByteArray& body, const Headers& headers) {
  QNetworkRequest r(QUrl(base() + path));
  for (const auto& [k, v] : headers) r.setRawHeader(k, v);
  if (method == "GET") return nam().get(r);
  return nam().sendCustomRequest(r, method, body);
}
Http finish(QNetworkReply* rep, int maxMs = 30000) {
  if (!rep->isFinished()) {
    QEventLoop loop;
    QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(maxMs, &loop, &QEventLoop::quit);
    loop.exec();
  }
  Http h;
  h.status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  h.body = rep->readAll();
  rep->deleteLater();
  return h;
}
Http call(const QByteArray& method, const QString& path, const QByteArray& body = {}, const Headers& headers = H) {
  return finish(start(method, path, body, headers));
}
Http get(const QString& path) { return call("GET", path, {}, {}); }
Http post(const QString& path, const QJsonValue& body) {
  const QByteArray b = body.isObject() ? QJsonDocument(body.toObject()).toJson(QJsonDocument::Compact)
                     : body.isArray() ? QJsonDocument(body.toArray()).toJson(QJsonDocument::Compact)
                                      : store::stringify(body, 0).toUtf8();
  return call("POST", path, b);
}
QByteArray json(const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
int status(const QString& path) { return get(path).status; }

// A raw request (a malformed URL QNetworkAccessManager would fix, an HTTP/1.0 client, an event stream).
QByteArray raw(const QByteArray& request, int waitMs = 5000, const QByteArray& until = {}) {
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, portRef());
  if (!s.waitForConnected(3000)) return {};
  s.write(request);
  QByteArray out;
  QEventLoop loop;
  QObject::connect(&s, &QTcpSocket::readyRead, &loop, [&] {
    out += s.readAll();
    if (!until.isEmpty() && out.contains(until)) loop.quit();
  });
  QObject::connect(&s, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
  QTimer::singleShot(waitMs, &loop, &QEventLoop::quit);
  loop.exec();
  out += s.readAll();
  return out;
}
int rawStatus(const QByteArray& request) {
  const QByteArray r = raw(request);
  const auto m = QRegularExpression(R"(^HTTP/1\.\d (\d{3}))").match(QString::fromLatin1(r));
  return m.hasMatch() ? m.captured(1).toInt() : 0;
}
QByteArray rawGet(const QByteArray& target) {
  return "GET " + target + " HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\nConnection: close\r\n\r\n";
}
void pump(int ms) {
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
}
void deleteAllAlerts() {
  for (const QJsonValue& a : get("/api/alerts").arr()) call("DELETE", "/api/alerts/" + a.toObject().value("id").toString());
}
}  // namespace

TEST("server: listens on the next free port when the first one is taken") {
  setUp();
  auto& s = api::Server::instance();
  QTcpServer squatter;
  CHECK(squatter.listen(QHostAddress::LocalHost, 0));
  const quint16 taken = squatter.serverPort();
  s.stop();
  CHECK(!s.isListening());
  CHECK(s.start(taken, 20));
  CHECK_EQ(int(s.port()), int(taken) + 1);
  portRef() = s.port();
  CHECK_EQ(get("/api/health").obj().value("ok"), QJsonValue(true));   // base() follows the singleton below
  s.stop();
  CHECK(s.start(0));
  portRef() = s.port();
}

TEST("server: health and the app page load") {
  setUp();
  const QJsonObject h = get("/api/health").obj();
  CHECK_EQ(h.value("ok"), QJsonValue(true));
  CHECK_EQ(h.value("app").toString(), QString("quant-terminal"));
  CHECK(h.value("providers").isObject());
  CHECK(h.value("feeds").isArray());
  CHECK(h.value("alerts").isDouble());
  // No web page in Deepwater: "/" says where the API is served from; the JS vendor script is not there.
  const Http page = get("/");
  CHECK_EQ(page.status, 200);
  CHECK(page.body.contains("Charts tab"));
  CHECK_EQ(status("/vendor/lightweight-charts.standalone.production.js"), 404);
  CHECK_EQ(get("/api/nope").obj().value("error").toString(), QString("Unknown endpoint"));
  CHECK_EQ(get("/api/nope").status, 404);
}

TEST("server: candles come back sorted with OHLCV") {
  setUp();
  const QJsonObject r = get("/api/candles?symbol=BTCUSDT&tf=1W&limit=50").obj();
  const QJsonArray c = r.value("candles").toArray();
  CHECK_EQ(c.size(), 50);
  bool sorted = true;
  for (int i = 1; i < c.size(); ++i) sorted = sorted && c[i].toObject().value("time").toDouble() > c[i - 1].toObject().value("time").toDouble();
  CHECK(sorted);
  for (const char* k : {"open", "high", "low", "close", "volume"}) CHECK(c[0].toObject().contains(k));
}

TEST("server: state round-trips and the running app hears about it") {
  setUp();
  QJsonObject heard;
  auto conn = QObject::connect(&api::Server::instance(), &api::Server::stateChanged,
                               [&](const QJsonObject& s, const QJsonObject&) { heard = s; });
  const Http put = call("PUT", "/api/state", R"({"watchlists":{"Main":["BINANCE:BTCUSDT"]}})");
  CHECK_EQ(put.status, 200);
  CHECK_EQ(put.obj().value("ok"), QJsonValue(true));
  const QJsonObject s = get("/api/state").obj();
  CHECK_EQ(s.value("watchlists").toObject(), (QJsonObject{{"Main", QJsonArray{"BINANCE:BTCUSDT"}}}));
  CHECK_EQ(heard.value("watchlists").toObject(), s.value("watchlists").toObject());
  QObject::disconnect(conn);
}

TEST("server: alerts can be created, listed and deleted") {
  setUp();
  const QJsonObject a = post("/api/alerts", QJsonObject{{"symbol", "BINANCE:BTCUSDT"}, {"tf", "1D"}, {"left", "close"}, {"op", ">"}, {"right", 1}}).obj();
  CHECK(!a.value("id").toString().isEmpty());
  CHECK_EQ(call("POST", "/api/alerts/check").status, 200);
  bool fired = false;
  for (const QJsonValue& e : get("/api/alerts/log").arr()) fired = fired || e.toObject().value("alertId") == a.value("id");
  CHECK(fired);
  call("DELETE", "/api/alerts/" + a.value("id").toString());
  CHECK_EQ(get("/api/alerts").arr().size(), 0);
}

TEST("server: a malformed custom indicator is refused on save and never replaces a working one") {
  setUp();
  const auto save = [](const QString& code) {
    return call("POST", "/api/custom", json({{"name", "Test format check"}, {"code", code}}));
  };
  const QString broken = "export default { name: 'Broken', calc: (bars) => ({ value: bars.slice(5).map((b) => b.close) }) };";
  const Http r1 = save(broken);
  CHECK_EQ(r1.status, 400);
  CHECK(r1.obj().value("error").toString().contains(QRegularExpression(R"(calc\(\) returned 295 values, but there are 300 bars)")));
  const auto has = [](const QString& file) {
    for (const QJsonValue& c : get("/api/custom").arr()) if (c.toObject().value("file") == QJsonValue(file)) return true;
    return false;
  };
  CHECK(!has("test-format-check.js"));   // a refused new file is not kept
  const QString good = "export default { name: 'Fine', calc: (bars, p, ta) => ({ value: ta.source(bars) }) };";
  CHECK_EQ(save(good).status, 200);
  CHECK_EQ(save(broken).status, 400);
  CHECK_EQ(QString::fromUtf8(get("/custom/test-format-check.js").body), good);   // the working version stays
  QJsonValue err;
  for (const QJsonValue& c : get("/api/custom").arr())
    if (c.toObject().value("file") == QJsonValue("test-format-check.js")) err = c.toObject().value("error");
  CHECK(err.isNull());
  call("DELETE", "/api/custom/test-format-check.js");
  CHECK(!has("test-format-check.js"));
}

TEST("server: POST /api/custom saves Pine Script as .pine and accepts module.exports") {
  setUp();
  const auto save = [](const QString& code) {
    return call("POST", "/api/custom", json({{"name", "Test paste kinds"}, {"code", code}}));
  };
  const Http pine = save("//@version=5\nindicator(\"My RSI\")\nplot(ta.rsi(close, 14), \"RSI\")");
  CHECK_EQ(pine.status, 200);
  CHECK_EQ(pine.obj().value("file").toString(), QString("test-paste-kinds.pine"));
  CHECK_EQ(pine.obj().value("id").toString(), QString("pine:test-paste-kinds"));
  CHECK(charts::ind::byId("pine:test-paste-kinds"));
  CHECK(get("/custom/test-paste-kinds.pine").body.startsWith("//@version=5"));
  const Http badPine = save("//@version=5\nindicator(\"Bad\")\nplot(close +)");
  CHECK_EQ(badPine.status, 400);
  CHECK(get("/custom/test-paste-kinds.pine").body.contains("ta.rsi"));   // the refused save kept the working file
  call("DELETE", "/api/custom/test-paste-kinds.pine");
  charts::ind::unregisterIndicator("pine:test-paste-kinds");
  const Http none = save("const x = 1;");
  CHECK_EQ(none.status, 400);
  CHECK(none.obj().value("error").toString().startsWith("The file must `export default"));
  const Http cjs = save("module.exports = { name: 'CJS', calc: (bars, p, ta) => ({ value: ta.source(bars) }) };");
  CHECK_EQ(cjs.status, 200);
  CHECK(get("/custom/test-paste-kinds.js").body.startsWith("export default {"));
  call("DELETE", "/api/custom/test-paste-kinds.js");
}

TEST("server: POST /api/backtest runs the tester on the server, with quant checks on request") {
  setUp();
  const QJsonObject strategy{
      {"indicators", QJsonObject{{"fast", QJsonObject{{"id", "ema"}, {"params", QJsonObject{{"length", 9}}}}},
                                 {"slow", QJsonObject{{"id", "ema"}, {"params", QJsonObject{{"length", 21}}}}}}},
      {"entryLong", QJsonArray{QJsonObject{{"left", "fast"}, {"op", "crossesAbove"}, {"right", "slow"}}}},
      {"exitLong", QJsonArray{QJsonObject{{"left", "fast"}, {"op", "crossesBelow"}, {"right", "slow"}}}},
  };
  const QJsonObject r = post("/api/backtest", QJsonObject{{"symbol", "BTCUSDT"}, {"tf", "1D"}, {"limit", 500},
                                                          {"strategy", strategy}, {"quantChecks", true}}).obj();
  CHECK_EQ(r.value("symbol").toString(), QString("BINANCE:BTCUSDT"));
  CHECK_EQ(r.value("bars").toInt(), 500);
  const QJsonObject stats = r.value("stats").toObject();
  CHECK(stats.value("totalTrades").toInt() > 0 && stats.value("netProfitPct").isDouble());
  CHECK_EQ(r.value("trades").toArray().size(), stats.value("totalTrades").toInt());
  const QJsonObject qc = r.value("quantChecks").toObject();
  CHECK(qc.value("total").toInt() >= 3);
  bool bools = !qc.value("gates").toArray().isEmpty();
  for (const QJsonValue& g : qc.value("gates").toArray()) bools = bools && g.toObject().value("pass").isBool();
  CHECK(bools);
  CHECK(!r.contains("equity"));   // equity only when asked for
  const Http bad = post("/api/backtest", QJsonObject{{"symbol", "BTCUSDT"}, {"strategy", QJsonObject{{"entryLong",
      QJsonArray{QJsonObject{{"left", "nope"}, {"op", ">"}, {"right", 1}}}}}}});
  CHECK_EQ(bad.status, 400);
  CHECK(bad.obj().value("error").toString().contains("Unknown operand \"nope\""));
  CHECK_EQ(call("POST", "/api/backtest", "{}", {}).status, 403);
  CHECK_EQ(post("/api/backtest", QJsonObject{{"symbol", "BTCUSDT"}}).status, 400);
  const QJsonObject eq = post("/api/backtest", QJsonObject{{"symbol", "BTCUSDT"}, {"limit", 100}, {"strategy", strategy},
                                                           {"equity", true}}).obj();
  CHECK_EQ(eq.value("equity").toArray().size(), 100);
  bool ema = false;
  for (const QJsonValue& d : get("/api/indicators").arr()) {
    const QJsonObject o = d.toObject();
    if (o.value("id") != QJsonValue("ema")) continue;
    bool length = false;
    for (const QJsonValue& i : o.value("inputs").toArray()) length = length || i.toObject().value("key") == QJsonValue("length");
    ema = length && o.value("outputs").toArray().contains(QJsonValue("value"));
  }
  CHECK(ema);
}

TEST("server: writes without the app header are refused") {
  setUp();
  CHECK_EQ(call("PUT", "/api/state", "{}", {{"Content-Type", "application/json"}}).status, 403);
  // Reads need nothing; other hosts are refused even for reads.
  CHECK_EQ(rawStatus("GET /api/health HTTP/1.1\r\nHost: evil.example\r\nConnection: close\r\n\r\n"), 403);
  CHECK_EQ(rawStatus("GET /api/health HTTP/1.1\r\nHost: localhost:1\r\nConnection: close\r\n\r\n"), 200);
  CHECK_EQ(rawStatus("GET /api/health HTTP/1.1\r\nHost: [::1]:1\r\nConnection: close\r\n\r\n"), 200);
}

TEST("server: a custom indicator file can be saved and loaded") {
  setUp();
  const QString code = "export default { name: 'Double close', calc: (bars, p, ta) => ({ value: ta.source(bars).map((v) => v * 2) }) };";
  QString changed;
  auto conn = QObject::connect(&api::Server::instance(), &api::Server::customChanged, [&](const QString& f) { changed = f; });
  const QJsonObject r = call("POST", "/api/custom", json({{"name", "Test double close"}, {"code", code}})).obj();
  CHECK_EQ(r.value("ok"), QJsonValue(true));
  CHECK_EQ(r.value("id").toString(), QString("custom:test-double-close"));
  CHECK_EQ(changed, QString("test-double-close.js"));
  bool listed = false;
  for (const QJsonValue& c : get("/api/custom").arr()) listed = listed || c.toObject().value("file") == QJsonValue("test-double-close.js");
  CHECK(listed);
  bool indicator = false;
  for (const QJsonValue& d : get("/api/indicators").arr()) indicator = indicator || d.toObject().value("id") == QJsonValue("custom:test-double-close");
  CHECK(indicator);
  // A strategy with a custom indicator runs on the GUI thread (custom indicators can't leave it).
  const QJsonObject bt = post("/api/backtest", QJsonObject{{"symbol", "BTCUSDT"}, {"limit", 200}, {"strategy", QJsonObject{
      {"indicators", QJsonObject{{"dbl", QJsonObject{{"id", "custom:test-double-close"}}}}},
      {"entryLong", QJsonArray{QJsonObject{{"left", "dbl"}, {"op", ">"}, {"right", 0}}}}}}}).obj();
  CHECK_EQ(bt.value("bars").toInt(), 200);
  CHECK(bt.value("stats").toObject().value("totalTrades").toInt() >= 1);
  call("DELETE", "/api/custom/test-double-close.js");
  QObject::disconnect(conn);
}

TEST("server: screener and scan work") {
  setUp();
  CHECK(get("/api/screener?universe=crypto").arr().size() > 0);
  CHECK(get("/api/screener?universe=stocks").arr().size() > 0);
  const QJsonArray scan = post("/api/scan", QJsonObject{
      {"symbols", QJsonArray{"BTCUSDT"}}, {"tf", "1D"},
      {"indicators", QJsonObject{{"rsi", QJsonObject{{"id", "rsi"}, {"params", QJsonObject{{"length", 14}}}}}}},
      {"rules", QJsonArray{QJsonObject{{"left", "rsi"}, {"op", ">"}, {"right", 0}}}}}).arr();
  CHECK_EQ(scan.size(), 1);
  CHECK_EQ(scan[0].toObject().value("pass"), QJsonValue(true));
  CHECK_EQ(scan[0].toObject().value("passes").toArray(), (QJsonArray{true}));
  CHECK(scan[0].toObject().value("close").isDouble());
  // A bad rule is that symbol's error, not the whole scan's; symbols keep their order with 4 at a time.
  const QJsonArray many = post("/api/scan", QJsonObject{
      {"symbols", QJsonArray{"BTCUSDT", "ETHUSDT", "SOLUSDT", "XRPUSDT", "ADAUSDT", "BNBUSDT"}},
      {"rules", QJsonArray{QJsonObject{{"left", "nope"}, {"op", ">"}, {"right", 0}}}}}).arr();
  CHECK_EQ(many.size(), 6);
  CHECK_EQ(many[5].toObject().value("symbol").toString(), QString("BNBUSDT"));
  CHECK(many[0].toObject().value("error").toString().contains("Unknown operand"));
  CHECK_EQ(post("/api/scan", QJsonObject{{"symbols", QJsonArray()}}).arr().size(), 0);
}

// ---- regressions from the API fuzz (Oct 2026) ----
TEST("server: client mistakes get a 400, not a 500") {
  setUp();
  CHECK_EQ(status("/api/candles?symbol=&tf=1D"), 400);
  CHECK_EQ(status("/api/candles?symbol=BTCUSDT&tf=banana"), 400);
  CHECK_EQ(status("/api/resolve?symbol="), 400);
  CHECK_EQ(rawStatus(rawGet("/%E0%A4%A")), 400);
  CHECK_EQ(call("PUT", "/api/state", "not json").status, 400);
  CHECK_EQ(call("PUT", "/api/state", "not json").obj().value("error").toString(), QString("The request body isn't valid JSON"));
  CHECK_EQ(call("PUT", "/api/state", "[1,2]").status, 400);
  CHECK_EQ(call("PUT", "/api/state", "[1,2]").obj().value("error").toString(), QString("The request body must be a JSON object"));
  CHECK_EQ(call("POST", "/api/scan", R"({"symbols":5,"rules":"x"})").status, 400);
  CHECK_EQ(call("POST", "/api/webhook-test", R"({"url":"notaurl"})").status, 400);
  // Body limits: 2 MB for JSON (20 MB for the state).
  CHECK_EQ(call("POST", "/api/alerts", QByteArray(2000001, ' ')).status, 413);
  CHECK_EQ(call("POST", "/api/alerts", QByteArray(2000001, ' ')).obj().value("error").toString(), QString("Request too large"));
}

TEST("server: symbols that could reshape an upstream URL are refused") {
  setUp();
  for (const QString& s : QStringList{"BINANCE:BTC&interval=1m", "US:../../admin", ":::", "A B?C", QString(80, 'x')})
    CHECK_EQ(status("/api/candles?symbol=" + QUrl::toPercentEncoding(s) + "&tf=1D"), 400);
  for (const QString& s : QStringList{"BTCUSDT", "EUR/USD", "^FTSE", "YAHOO:GC=F", "VOD.L", "BRK-B"})
    CHECK_EQ(status("/api/resolve?symbol=" + QUrl::toPercentEncoding(s)), 200);
}

TEST("server: an opaque Origin is refused with 403, not a crash") {
  setUp();
  Headers h = H;
  h.append(qMakePair(QByteArray("Origin"), QByteArray("null")));
  CHECK_EQ(call("PUT", "/api/state", "{}", h).status, 403);
  Headers other = H;
  other.append(qMakePair(QByteArray("Origin"), QByteArray("https://evil.example")));
  CHECK_EQ(call("PUT", "/api/state", "{}", other).status, 403);
  Headers same = H;
  same.append(qMakePair(QByteArray("Origin"), base().toUtf8()));
  CHECK_EQ(call("PUT", "/api/state", "{}", same).status, 200);
}

TEST("server: GET requests sent by a page of another site are refused (audit S5)") {
  setUp();
  CHECK_EQ(call("GET", "/api/state", {}, {}).status, 200);   // curl, scripts: no browser headers
  CHECK_EQ(call("GET", "/api/state", {}, {{"Sec-Fetch-Site", "cross-site"}}).status, 403);
  CHECK_EQ(call("GET", "/api/state", {}, {{"Sec-Fetch-Site", "same-origin"}}).status, 200);
  CHECK_EQ(call("GET", "/api/state", {}, {{"Origin", "https://evil.example"}}).status, 403);
  CHECK_EQ(call("GET", "/api/state", {}, {{"Origin", "null"}}).status, 403);
  CHECK_EQ(call("GET", "/api/state", {}, {{"Origin", base().toUtf8()}}).status, 200);
  CHECK_EQ(call("HEAD", "/api/state", {}, {{"Sec-Fetch-Site", "cross-site"}}).status, 403);
}

TEST("server: alerts with a bad operator, operand or body are refused up front") {
  setUp();
  CHECK_EQ(post("/api/alerts", QJsonValue("x")).status, 400);
  CHECK_EQ(post("/api/alerts", QJsonObject{{"symbol", "BTCUSDT"}, {"right", 1}, {"op", "bogus"}}).status, 400);
  CHECK_EQ(post("/api/alerts", QJsonObject{{"symbol", "BTCUSDT"}, {"right", 1}, {"op", ">"}, {"left", "nope"}}).status, 400);
  CHECK_EQ(post("/api/alerts", QJsonObject{{"symbol", "BTCUSDT"}, {"op", ">"}, {"left", "rsi"}, {"right", 70},
                                           {"indicators", QJsonObject{{"rsi", QJsonObject{{"id", "rsi"}, {"params", QJsonObject{{"length", 14}}}}}}}}).status, 200);
  CHECK_EQ(call("PATCH", "/api/alerts/doesnotexist", "{}").status, 404);
  CHECK_EQ(call("PATCH", "/api/alerts/doesnotexist", "{}").obj().value("error").toString(), QString("No such alert"));
  deleteAllAlerts();
  CHECK_EQ(get("/api/alerts").arr().size(), 0);
}

TEST("server: a \"once\" alert fires once even when checks overlap") {
  setUp();
  const QJsonObject a = post("/api/alerts", QJsonObject{{"symbol", "BINANCE:BTCUSDT"}, {"tf", "1D"}, {"left", "close"}, {"op", ">"}, {"right", 1}}).obj();
  QList<QNetworkReply*> reps;
  for (int i = 0; i < 5; ++i) reps << start("POST", "/api/alerts/check", {}, H);
  for (QNetworkReply* r : reps) CHECK_EQ(finish(r).status, 200);
  int n = 0;
  for (const QJsonValue& e : get("/api/alerts/log").arr()) n += e.toObject().value("alertId") == a.value("id");
  CHECK_EQ(n, 1);
  call("DELETE", "/api/alerts/" + a.value("id").toString());
}

TEST("server: static files cannot escape their folder") {
  setUp();
  for (const char* u : {"/..%2F..%2Fserver.js", "/custom/..%2Fserver.js", "/custom/..%2F..%2F..%2Fetc%2Fpasswd", "/..%5cserver.js"}) {
    const int s = rawStatus(rawGet(u));
    CHECK(s == 403 || s == 404);
  }
}

TEST("server: POST /api/shutdown is refused: Deepwater is quit from its window") {
  setUp();
  CHECK_EQ(call("POST", "/api/shutdown", {}, {}).status, 403);   // needs the app header, as the JS
  const Http r = call("POST", "/api/shutdown", {}, {{"x-qt", "1"}});
  CHECK_EQ(r.status, 403);
  CHECK(r.obj().value("error").toString().contains("Quit Deepwater"));
  CHECK(api::Server::instance().isListening());
  CHECK_EQ(get("/api/health").status, 200);
}

TEST("server: slash notation: pairs stay pairs, two symbols make a ratio") {
  setUp();
  const auto r = [](const QString& s) { return get("/api/resolve?symbol=" + QUrl::toPercentEncoding(s)).obj().value("id").toString(); };
  CHECK_EQ(r("BTC/USDT"), QString("BINANCE:BTCUSDT"));
  CHECK_EQ(r("EUR/USD"), QString("FX:EURUSD"));
  CHECK_EQ(r("BTCUSDT/ETHUSDT"), QString("SPREAD:BINANCE:BTCUSDT / BINANCE:ETHUSDT"));
  const QJsonArray sp = get("/api/candles?symbol=" + QUrl::toPercentEncoding("BTCUSDT/ETHUSDT") + "&tf=1D&limit=20").obj().value("candles").toArray();
  CHECK_EQ(sp.size(), 20);
  CHECK(std::fabs(sp.last().toObject().value("close").toDouble() - 1) < 1e-9);   // same fixture both sides
}

TEST("server: a watchlist alert checks every symbol on the list, once each") {
  setUp();
  call("PUT", "/api/state", R"({"watchlists":{"Crypto":["BINANCE:BTCUSDT","BINANCE:ETHUSDT"]}})");
  const QJsonObject a = post("/api/alerts", QJsonObject{{"watchlist", "Crypto"}, {"tf", "1D"}, {"left", "close"}, {"op", ">"}, {"right", 0}}).obj();
  CHECK_EQ(a.value("watchlist").toString(), QString("Crypto"));
  call("POST", "/api/alerts/check");
  call("POST", "/api/alerts/check");
  QStringList symbols;
  bool named = true;
  for (const QJsonValue& v : get("/api/alerts/log").arr()) {
    const QJsonObject e = v.toObject();
    if (e.value("alertId") != a.value("id")) continue;
    symbols << e.value("symbol").toString();
    named = named && e.value("text").toString().contains(e.value("symbol").toString());
  }
  symbols.sort();
  CHECK_EQ(symbols, (QStringList{"BINANCE:BTCUSDT", "BINANCE:ETHUSDT"}));
  CHECK(named);   // each message names its symbol
  CHECK_EQ(post("/api/alerts", QJsonObject{{"tf", "1D"}, {"left", "close"}, {"op", ">"}, {"right", 0}}).status, 400);
  call("DELETE", "/api/alerts/" + a.value("id").toString());
}

TEST("server: multi-condition alerts: all must hold, or any, and five at most") {
  setUp();
  const auto mk = [](QJsonObject body) {
    body["symbol"] = "BTCUSDT";
    body["tf"] = "1D";
    body["trigger"] = "once";
    return post("/api/alerts", body);
  };
  const QJsonArray two{QJsonObject{{"left", "close"}, {"op", ">"}, {"right", 1}}, QJsonObject{{"left", "close"}, {"op", "<"}, {"right", 1}}};
  const QJsonObject all = mk({{"conditions", two}, {"match", "all"}}).obj();
  const QJsonObject any = mk({{"conditions", two}, {"match", "any"}}).obj();
  const QJsonObject line = mk({{"conditions", QJsonArray{QJsonObject{{"left", "close"}, {"op", ">"}, {"right",
      QJsonObject{{"line", QJsonObject{{"t1", 0}, {"p1", 1}, {"t2", 1}, {"p2", 1}, {"extend", "right"}}}}}}}}}).obj();
  call("POST", "/api/alerts/check");
  const QJsonArray log = get("/api/alerts/log").arr();
  const auto firedFor = [&](const QJsonObject& a) {
    for (const QJsonValue& e : log) if (e.toObject().value("alertId") == a.value("id")) return true;
    return false;
  };
  CHECK(!firedFor(all));   // all: one false condition blocks it
  CHECK(firedFor(any));    // any: one true condition fires it
  CHECK(firedFor(line));   // a drawn line works as a condition
  QJsonArray six;
  for (int i = 0; i < 6; ++i) six.append(QJsonObject{{"left", "close"}, {"op", ">"}, {"right", 1}});
  const Http s6 = mk({{"conditions", six}});
  CHECK_EQ(s6.status, 400);
  CHECK_EQ(s6.obj().value("error").toString(), QString("An alert can have up to 5 conditions"));
  for (const QJsonObject& a : {all, any, line}) call("DELETE", "/api/alerts/" + a.value("id").toString());
}

// ---- not in the JS file ----
TEST("server: /api/events streams alerts as server-sent events and lets go of closed clients") {
  setUp();
  auto& srv = api::Server::instance();
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, portRef());
  CHECK(s.waitForConnected(3000));
  s.write("GET /api/events HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\n\r\n");
  QByteArray got;
  const auto waitFor = [&](const QByteArray& what, int ms) {
    QEventLoop loop;
    auto c = QObject::connect(&s, &QTcpSocket::readyRead, &loop, [&] { got += s.readAll(); if (got.contains(what)) loop.quit(); });
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    got += s.readAll();   // what came while nobody listened
    if (!got.contains(what)) loop.exec();
    QObject::disconnect(c);
    return got.contains(what);
  };
  CHECK(waitFor("retry: 3000\n\n", 3000));
  CHECK(got.contains("text/event-stream"));
  CHECK_EQ(srv.eventClients(), 1);
  const QJsonObject a = post("/api/alerts", QJsonObject{{"symbol", "BINANCE:BTCUSDT"}, {"tf", "1D"}, {"op", ">"}, {"right", 1}, {"message", "SSE test"}}).obj();
  call("POST", "/api/alerts/check");
  CHECK(waitFor("event: alert\ndata: {", 3000));
  CHECK(got.contains("\"text\":\"Alert: SSE test (price "));
  CHECK(got.contains("\"alertId\":\"" + a.value("id").toString().toUtf8() + "\""));
  s.disconnectFromHost();
  if (s.state() != QAbstractSocket::UnconnectedState) s.waitForDisconnected(1000);
  for (int i = 0; i < 20 && srv.eventClients() > 0; ++i) pump(50);
  CHECK_EQ(srv.eventClients(), 0);
  call("DELETE", "/api/alerts/" + a.value("id").toString());
}

TEST("server: an HTTP/1.0 client (the Rust example) gets whole answers and the event stream") {
  setUp();
  const QByteArray host = "Host: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\n";
  QElapsedTimer t;
  t.start();
  const QByteArray r = raw("GET /api/health HTTP/1.0\r\n" + host + "Accept: application/json\r\n\r\n", 3000);
  CHECK(t.elapsed() < 2000);   // the connection closes after the answer (the Rust example reads to the end)
  CHECK(r.startsWith("HTTP/1."));
  const QByteArray body = r.mid(r.indexOf("\r\n\r\n") + 4);
  CHECK(QJsonDocument::fromJson(body).object().value("ok") == QJsonValue(true));
  const QByteArray ev = raw("GET /api/events HTTP/1.0\r\n" + host + "\r\n", 1500, "retry: 3000");
  CHECK(ev.contains("retry: 3000"));
  for (int i = 0; i < 20 && api::Server::instance().eventClients() > 0; ++i) pump(50);
  CHECK_EQ(api::Server::instance().eventClients(), 0);
}

TEST("server: webhook-test posts the test message and reports the status") {
  setUp();
  QByteArray sent;
  QString method;
  charts::data::testing::setTransport([&](const net::Request& r, net::Callback cb) {
    sent = r.body;
    method = QString::fromLatin1(r.method);
    net::Reply rep;
    rep.status = r.url.host() == "hooks.example" ? 204 : 500;
    if (rep.status >= 300) rep.error = "HTTP 500";
    QTimer::singleShot(0, [cb, rep] { cb(rep); });
  });
  const QJsonObject ok = post("/api/webhook-test", QJsonObject{{"url", "https://hooks.example/x"}}).obj();
  CHECK_EQ(ok, (QJsonObject{{"ok", true}, {"status", 204}}));
  CHECK_EQ(method, QString("POST"));
  CHECK_EQ(QJsonDocument::fromJson(sent).object(),
           (QJsonObject{{"text", "Deepwater test alert"}, {"content", "Deepwater test alert"}}));
  CHECK_EQ(post("/api/webhook-test", QJsonObject{{"url", "https://other.example/x"}}).obj(), (QJsonObject{{"ok", false}, {"status", 500}}));
  fakenet::install([](const QUrl&) { return fakenet::Reply::dns(); });
  const Http down = post("/api/webhook-test", QJsonObject{{"url", "https://down.example/x"}});
  CHECK_EQ(down.status, 500);
  CHECK_EQ(down.obj().value("error").toString(), QString("fetch failed"));
}

TEST("server: CSV imports: bars and a signal that becomes an indicator") {
  setUp();
  QString csv = "time,open,high,low,close,volume\n";
  for (int i = 0; i < 50; ++i)
    csv += QString("%1,%2,%3,%4,%5,10\n").arg(1700000000 + i * 60).arg(100 + i).arg(101 + i).arg(99 + i).arg(100.5 + i);
  const Http bars = call("POST", "/api/import/bars?name=test%20bars", csv.toUtf8(), {{"x-qt", "1"}});
  CHECK_EQ(bars.status, 200);
  CHECK_EQ(bars.obj().value("symbol").toString(), QString("CSV:TEST_BARS"));
  CHECK_EQ(bars.obj().value("bars").toInt(), 50);
  bool listed = false;
  for (const QJsonValue& i : get("/api/imports").arr()) listed = listed || i.toObject().value("name") == QJsonValue("TEST_BARS");
  CHECK(listed);
  CHECK_EQ(get("/api/candles?symbol=CSV:TEST_BARS&tf=1m").obj().value("candles").toArray().size(), 50);
  const Http sig = call("POST", "/api/import/signal?name=test%20sig", "time,position\n1700000000,1\n1700001200,0\n", {{"x-qt", "1"}});
  CHECK_EQ(sig.status, 200);
  CHECK_EQ(sig.obj().value("id").toString(), QString("custom:signal-test_sig"));
  CHECK(sig.obj().value("strategy").isObject());
  const Http bad = call("POST", "/api/import/bars?name=x", "nothing here", {{"x-qt", "1"}});
  CHECK_EQ(bad.status, 400);
  CHECK(!bad.obj().value("error").toString().isEmpty());
  call("DELETE", "/api/custom/signal-test_sig.js");
  QFile::remove(QDir(data::importsDir()).filePath("TEST_BARS.csv"));
  data::testing::setOffline({});
}

TEST("server: a refused or oversized body is answered from its head, before any of it is read (as the JS)") {
  setUp();
  const QByteArray host = "Host: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\n";
  QElapsedTimer t;
  t.start();
  // A web page posting 250 MB: refused 403 from the head; only 1 MB of the body is ever sent.
  QByteArray r = raw("POST /api/import/bars?name=x HTTP/1.1\r\n" + host + "Origin: http://evil.example\r\nContent-Type: text/plain\r\n"
                     "Content-Length: 250000000\r\n\r\n" + QByteArray(1000000, 'x'), 3000, "computer.");
  CHECK(r.startsWith("HTTP/1.1 403"));
  CHECK(r.endsWith("Forbidden: Deepwater only answers to this computer."));
  // The same without the app header, chunked: refused too.
  r = raw("POST /api/alerts HTTP/1.1\r\n" + host + "Transfer-Encoding: chunked\r\n\r\n10\r\n0123456789abcdef\r\n", 3000, "computer.");
  CHECK(r.startsWith("HTTP/1.1 403"));
  // The app's own header, a body over its route's limit: 413 with the JS error, before the body is sent.
  r = raw("POST /api/alerts HTTP/1.1\r\n" + host + "x-qt: 1\r\nContent-Length: 2000001\r\n\r\n", 3000, "too large\"}");
  CHECK(r.startsWith("HTTP/1.1 413"));
  CHECK(r.endsWith("{\"error\":\"Request too large\"}"));
  r = raw("POST /api/import/bars?name=x HTTP/1.1\r\n" + host + "x-qt: 1\r\nContent-Length: 300000001\r\n\r\n", 3000, "MB\"}");
  CHECK(r.endsWith("{\"error\":\"The file is over 300 MB\"}"));
  r = raw("PUT /api/state HTTP/1.1\r\n" + host + "x-qt: 1\r\nContent-Length: 20000001\r\n\r\n", 3000, "large\"}");
  CHECK(r.startsWith("HTTP/1.1 413"));
  CHECK(t.elapsed() < 3000);
  // Bigger than 2 MB where the route takes it: a 3 MB state patch, a 3 MB CSV.
  const QByteArray pad = QByteArray(3000000, 'a');
  CHECK_EQ(call("PUT", "/api/state", "{\"gateTest\":\"" + pad + "\"}").status, 200);
  CHECK_EQ(call("PUT", "/api/state", "{\"gateTest\":null}").status, 200);
  QByteArray csv = "time,open,high,low,close,volume\n";
  for (int i = 0; csv.size() < 3000000; ++i) csv += QByteArray::number(1700000000 + i * 60) + ",100,101,99,100.5,10\n";
  const Http big = call("POST", "/api/import/bars?name=gate%20big", csv, {{"x-qt", "1"}});
  CHECK_EQ(big.status, 200);
  CHECK(big.obj().value("bars").toInt() > 50000);
  QFile::remove(QDir(data::importsDir()).filePath("GATE_BIG.csv"));
  // Requests after a refused one still work (on a new connection), and one after another on the same connection.
  CHECK_EQ(status("/api/health"), 200);
  r = raw("GET /api/health HTTP/1.1\r\n" + host + "\r\nGET /api/imports HTTP/1.1\r\n" + host + "Connection: close\r\n\r\n");
  CHECK_EQ(r.count("HTTP/1.1 200"), 2);
}

TEST("server: HEAD answers carry no body; /custom/100%25.js is the file 100%.js, a bad escape is still a Bad URL") {
  setUp();
  const QByteArray host = "Host: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\n";
  // Two requests on one connection: the HEAD answer's headers, then straight away the GET answer.
  const QByteArray r = raw("HEAD /api/health HTTP/1.1\r\n" + host + "\r\nGET /api/health HTTP/1.1\r\n" + host + "Connection: close\r\n\r\n");
  const qsizetype second = r.indexOf("HTTP/1.1 200", 1);
  CHECK(r.startsWith("HTTP/1.1 200") && second > 0);
  CHECK(r.left(second).endsWith("\r\n\r\n"));   // nothing after the HEAD answer's headers
  CHECK(r.mid(second).contains("\"ok\":true"));
  const QString dir = data::customDir();
  QDir().mkpath(dir);
  QFile f(dir + "/100%.js");
  CHECK(f.open(QIODevice::WriteOnly));
  f.write("// percent\n");
  f.close();
  const QByteArray got = raw(rawGet("/custom/100%25.js"));
  CHECK(got.startsWith("HTTP/1.1 200"));
  CHECK(got.endsWith("// percent\n"));
  CHECK_EQ(rawStatus(rawGet("/custom/100%.js")), 400);   // decodeURIComponent throws
  CHECK_EQ(rawStatus(rawGet("/custom/%zz.js")), 400);
  CHECK_EQ(rawStatus(rawGet("/custom/..%2F..%2Fstate.json")), 403);
  QFile::remove(dir + "/100%.js");
}

TEST("server: PORT must be a port number (the JS listen() throws on others; here the default is used)") {
  CHECK_EQ(api::envPort(""), 3777);
  CHECK_EQ(api::envPort("abc"), 3777);
  CHECK_EQ(api::envPort("0"), 3777);
  CHECK_EQ(api::envPort(" 4000 "), 4000);
  CHECK_EQ(api::envPort("65535"), 65535);
  CHECK_EQ(api::envPort("70000"), -1);   // was 4464 (quint16 wrap)
  CHECK_EQ(api::envPort("-5"), -1);
  CHECK_EQ(api::envPort("3777.5"), -1);
  auto& s = api::Server::instance();
  const quint16 was = s.port();
  s.stop();
  CHECK(!s.start(70000, 0));
  CHECK(!s.isListening());
  CHECK(s.start(was, 20));
  portRef() = s.port();
}

TEST("server: a chunked body over its route's limit gets the JS 413 JSON; under it, it is read") {
  setUp();
  const QByteArray host = "Host: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\n";
  const QByteArray head = "HTTP/1.1\r\n" + host + "x-qt: 1\r\nTransfer-Encoding: chunked\r\n\r\n";
  // One chunk announced over the limit: refused at its size line, before its data.
  QByteArray r = raw("POST /api/alerts " + head + QByteArray::number(2000001, 16) + "\r\n" + QByteArray(1000, ' '), 3000, "large\"}");
  CHECK(r.startsWith("HTTP/1.1 413"));
  CHECK(r.contains("Content-Type: application/json"));
  CHECK(r.endsWith("{\"error\":\"Request too large\"}"));
  // Several chunks that go over together.
  QByteArray chunks;
  for (int i = 0; i < 3; ++i) chunks += QByteArray::number(900000, 16) + "\r\n" + QByteArray(900000, ' ') + "\r\n";
  r = raw("POST /api/alerts " + head + chunks, 3000, "large\"}");
  CHECK(r.startsWith("HTTP/1.1 413"));
  CHECK(r.endsWith("{\"error\":\"Request too large\"}"));
  // The routes with their own limits and texts.
  r = raw("POST /api/import/bars?name=x " + head + QByteArray::number(300000001, 16) + "\r\n", 3000, "MB\"}");
  CHECK(r.endsWith("{\"error\":\"The file is over 300 MB\"}"));
  r = raw("POST /api/import/signal?name=x " + head + QByteArray::number(100000001, 16) + "\r\n", 3000, "MB\"}");
  CHECK(r.endsWith("{\"error\":\"The file is over 100 MB\"}"));
  r = raw("PUT /api/state " + head + QByteArray::number(20000001, 16) + "\r\n", 3000, "large\"}");
  CHECK(r.endsWith("{\"error\":\"Request too large\"}"));
  // Under the limit: the chunks make the body.
  const QByteArray alert = R"({"symbol":"BINANCE:BTCUSDT","tf":"1D","left":"close","op":">","right":1,"message":"chunked"})";
  const QByteArray body = QByteArray::number(20, 16) + "\r\n" + alert.left(20) + "\r\n" + QByteArray::number(alert.size() - 20, 16) +
                          "\r\n" + alert.mid(20) + "\r\n0\r\n\r\n";
  r = raw("POST /api/alerts HTTP/1.1\r\n" + host + "x-qt: 1\r\nConnection: close\r\nTransfer-Encoding: chunked\r\n\r\n" + body);
  CHECK(r.startsWith("HTTP/1.1 200"));
  CHECK(r.contains("\"message\":\"chunked\""));
  for (const QJsonValue& a : get("/api/alerts").arr()) call("DELETE", "/api/alerts/" + a.toObject().value("id").toString());
  CHECK_EQ(status("/api/health"), 200);
}

TEST("server: a body on a route that doesn't read it is dropped as it comes, and the route answers (as the JS)") {
  setUp();
  CHECK(api::readsBody("POST", "/api/alerts"));
  CHECK(api::readsBody("PATCH", "/api/alerts/abc"));
  CHECK(api::readsBody("PUT", "/api/state"));
  CHECK(api::readsBody("POST", "/api/import/bars"));
  CHECK(!api::readsBody("GET", "/api/alerts"));
  CHECK(!api::readsBody("PATCH", "/api/alerts/log"));
  CHECK(!api::readsBody("POST", "/api/alerts/check"));
  CHECK(!api::readsBody("DELETE", "/api/alerts/abc"));
  CHECK(!api::readsBody("GET", "/api/state"));
  CHECK(!api::readsBody("POST", "/api/health"));
  const QByteArray host = "Host: 127.0.0.1:" + QByteArray::number(portRef()) + "\r\n";
  const QByteArray next = "GET /api/imports HTTP/1.1\r\n" + host + "Connection: close\r\n\r\n";
  // 3 MB on a GET (over the 2 MB a reading route takes), then a second request on the same connection.
  QByteArray r = raw("GET /api/health HTTP/1.1\r\n" + host + "Content-Length: 3000000\r\n\r\n" + QByteArray(3000000, 'x') + next);
  CHECK(r.startsWith("HTTP/1.1 200"));
  CHECK(r.contains("\"app\":\"quant-terminal\""));
  CHECK_EQ(r.count("HTTP/1.1 200"), 2);
  // The same chunked, on a DELETE with the app header.
  QByteArray chunks;
  for (int i = 0; i < 4; ++i) chunks += QByteArray::number(800000, 16) + ";ext=1\r\n" + QByteArray(800000, 'y') + "\r\n";
  chunks += "0\r\nX-Trailer: 1\r\n\r\n";
  r = raw("DELETE /api/alerts/nothing HTTP/1.1\r\n" + host + "x-qt: 1\r\nTransfer-Encoding: chunked\r\n\r\n" + chunks + next);
  CHECK(r.startsWith("HTTP/1.1 200"));
  CHECK(r.contains("{\"ok\":true}"));
  CHECK_EQ(r.count("HTTP/1.1 200"), 2);
  // The server runs on this thread: wait by running the event loop (waitForReadyRead would block it).
  const auto waitFor = [](QTcpSocket& s, QByteArray& got, const std::function<bool()>& done) {
    for (int i = 0; i < 100 && !done(); ++i) {
      pump(50);
      got += s.readAll();
    }
  };
  // A client that waits for "100 Continue" before its body (curl does over 1 MB) gets it, then the answer.
  {
    QTcpSocket s;
    s.connectToHost(QHostAddress::LocalHost, portRef());
    { QByteArray ignored; waitFor(s, ignored, [&] { return s.state() == QAbstractSocket::ConnectedState; }); }
    s.write("POST /api/alerts/check HTTP/1.1\r\n" + host + "x-qt: 1\r\nExpect: 100-continue\r\nContent-Length: 2500000\r\n\r\n");
    QByteArray got;
    waitFor(s, got, [&] { return got.contains("100 Continue\r\n\r\n"); });
    CHECK(got.startsWith("HTTP/1.1 100 Continue\r\n\r\n"));
    s.write(QByteArray(2500000, 'z'));
    s.write(next);
    waitFor(s, got, [&] { return got.count("HTTP/1.1 200") >= 2; });
    CHECK(got.contains("{\"ok\":true}"));
    CHECK_EQ(got.count("HTTP/1.1 200"), 2);
  }
  // The body arrives in pieces after the answer: dropped, then the next request is read.
  {
    QTcpSocket s;
    s.connectToHost(QHostAddress::LocalHost, portRef());
    QByteArray got;
    waitFor(s, got, [&] { return s.state() == QAbstractSocket::ConnectedState; });
    s.write("GET /api/imports HTTP/1.1\r\n" + host + "Transfer-Encoding: chunked\r\n\r\n5\r\nab");
    waitFor(s, got, [&] { return got.contains("\r\n\r\n["); });
    CHECK(got.startsWith("HTTP/1.1 200"));
    for (const QByteArray piece : {QByteArray("cde\r"), QByteArray("\n0\r\n"), QByteArray("\r\n") + next}) {
      s.write(piece);
      pump(50);
    }
    waitFor(s, got, [&] { return got.count("HTTP/1.1 200") >= 2; });
    CHECK_EQ(got.count("HTTP/1.1 200"), 2);
  }
  // Behind a request Qt is still answering (a webhook test whose host takes 600 ms): the head put back for Qt waits
  // its turn in front of the body bytes that keep coming.
  data::testing::setTransport([](const net::Request&, net::Callback cb) {
    net::Reply rep;
    rep.status = 200;
    QTimer::singleShot(600, [cb, rep] { cb(rep); });
  });
  for (const bool chunked : {false, true}) {
    QTcpSocket s;
    s.connectToHost(QHostAddress::LocalHost, portRef());
    QByteArray got;
    waitFor(s, got, [&] { return s.state() == QAbstractSocket::ConnectedState; });
    const QByteArray hook = R"({"url":"http://slow.example/hook"})";
    const QByteArray piece = chunked ? "3e8\r\n" + QByteArray(1000, 'x') + "\r\n" : QByteArray(1000, 'x');
    s.write("POST /api/webhook-test HTTP/1.1\r\n" + host + "x-qt: 1\r\nContent-Length: " + QByteArray::number(hook.size()) +
            "\r\n\r\n" + hook + "GET /api/health HTTP/1.1\r\n" + host +
            (chunked ? QByteArray("Transfer-Encoding: chunked\r\n\r\n") : QByteArray("Content-Length: 3000\r\n\r\n")) + piece);
    for (int i = 0; i < 2; ++i) {
      pump(100);
      s.write(piece);
    }
    if (chunked) s.write("0\r\n\r\n");
    pump(100);
    s.write(next);
    waitFor(s, got, [&] { return got.count("HTTP/1.1 200") >= 3; });
    CHECK_EQ(got.count("HTTP/1.1 200"), 3);
    CHECK(got.contains("{\"ok\":true,\"status\":200}"));
    CHECK(got.contains("\"app\":\"quant-terminal\""));
  }
  fakenet::install([](const QUrl&) { return fakenet::Reply::dns(); });
  // Refused requests with a body are still refused before it is read.
  r = raw("GET /api/health HTTP/1.1\r\nHost: evil.example\r\nContent-Length: 5\r\n\r\nhello", 3000, "computer.");
  CHECK(r.startsWith("HTTP/1.1 403"));
}
