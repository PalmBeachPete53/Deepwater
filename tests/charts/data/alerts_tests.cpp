// Tests of the alerts service (src/charts/data/Alerts, port of lib/alerts.js): validation messages, descriptions, the
// stored shape, the fire rules (once / oncePerBar / oncePerBarClose / watchlists), the log cap, webhooks (payload per
// service, through the Fetch test transport) and the checker. Candles come from the offline fixture (btc-1d.json,
// the JS QT_OFFLINE=1), so nothing leaves the machine.
#include "Check.h"
#include "FakeNet.h"
#include "charts/data/Alerts.h"
#include "charts/data/Providers.h"
#include "charts/data/Store.h"
#include <QDir>
#include <QEventLoop>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimer>

using namespace charts;
using alerts::Alerts;

namespace {
struct Posted { QString url, method; QByteArray body, contentType; };
QList<Posted>& posted() { static QList<Posted> p; return p; }
int& hookStatus() { static int s = 200; return s; }

void setUp() {
  data::testing::reset();
  data::testing::setOffline(QStringLiteral(CHARTS_FIXTURES));
  posted().clear();
  hookStatus() = 200;
  data::testing::setTransport([](const net::Request& r, net::Callback cb) {
    QByteArray type;
    for (const auto& [k, v] : r.headers) if (k.compare("content-type", Qt::CaseInsensitive) == 0) type = v;
    posted().append({r.url.toString(), QString::fromLatin1(r.method), r.body, type});
    net::Reply rep;
    if (r.url.host() == "down.example") rep.error = "Host down.example not found";
    else {
      rep.status = hookStatus();
      if (rep.status >= 300) rep.error = QString("HTTP %1").arg(rep.status);
    }
    QTimer::singleShot(0, [cb, rep] { cb(rep); });
  });
  auto& al = Alerts::instance();
  al.stop();
  for (const QJsonValue& a : al.list()) al.remove(a.toObject().value("id").toString());
}
void tearDown() {
  auto& al = Alerts::instance();
  for (const QJsonValue& a : al.list()) al.remove(a.toObject().value("id").toString());
  data::testing::setOffline({});
}
QString err(const QJsonObject& a) {
  const auto e = Alerts::validate(a);
  return e ? e->message : QString();
}
void evaluateNow() {
  bool done = false;
  QEventLoop loop;
  Alerts::instance().evaluate([&] { done = true; loop.quit(); });
  QTimer::singleShot(15000, &loop, &QEventLoop::quit);
  if (!done) loop.exec();
  CHECK(done);
}
void pump(int ms) {
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
}
QJsonObject addOk(const QJsonObject& in) {
  const auto r = Alerts::instance().add(in);
  CHECK(r.ok());
  if (!r.ok()) std::fprintf(stderr, "    add: %s\n", qPrintable(r.error->message));
  return r.value;
}
QJsonArray eventsFor(const QJsonObject& a) {
  QJsonArray out;
  for (const QJsonValue& e : Alerts::instance().history())
    if (e.toObject().value("alertId") == a.value("id")) out.append(e);
  return out;
}
Bars fixtureBars() {
  auto r = fakenet::await<data::CandlesResult>([](data::Callback<data::CandlesResult> cb) {
    data::getCandles("BINANCE:BTCUSDT", "1D", 400, nullptr, cb);
  });
  CHECK(r.ok());
  return r.value.candles;
}
const QJsonObject kBase{{"symbol", "BINANCE:BTCUSDT"}, {"tf", "1D"}, {"left", "close"}, {"op", ">"}, {"right", 1}};
QJsonObject with(QJsonObject o, const QJsonObject& more) {
  for (auto it = more.begin(); it != more.end(); ++it) o.insert(it.key(), it.value());
  return o;
}
}  // namespace

TEST("alerts: validate refuses bad alerts with the JS messages") {
  const QJsonObject ok{{"symbol", "BTCUSDT"}, {"left", "close"}, {"op", ">"}, {"right", 1}};
  CHECK_EQ(err(ok), QString());
  CHECK(!Alerts::validate(ok).has_value());
  CHECK_EQ(err(with(ok, {{"watchlist", ""}})), QString("watchlist must be the name of one of your watchlists"));
  CHECK_EQ(err(with(ok, {{"watchlist", 5}})), QString("watchlist must be the name of one of your watchlists"));
  CHECK_EQ(err(with(ok, {{"watchlist", QJsonValue::Null}})), QString("watchlist must be the name of one of your watchlists"));
  CHECK_EQ(err(QJsonObject{{"left", "close"}, {"op", ">"}, {"right", 1}}), QString("An alert needs a symbol or a watchlist"));
  CHECK_EQ(err(with(ok, {{"symbol", 5}})), QString("An alert needs a symbol or a watchlist"));
  CHECK_EQ(err(QJsonObject{{"watchlist", "Main"}, {"left", "close"}, {"op", ">"}, {"right", 1}}), QString());
  QJsonArray six;
  for (int i = 0; i < 6; ++i) six.append(QJsonObject{{"left", "close"}, {"op", ">"}, {"right", 1}});
  CHECK_EQ(err(with(ok, {{"conditions", six}})), QString("An alert can have up to 5 conditions"));
  CHECK_EQ(err(with(ok, {{"match", "some"}})), QString("match must be \"all\" or \"any\""));
  CHECK_EQ(err(with(ok, {{"match", ""}})), QString());   // falsy match means "all"
  CHECK_EQ(err(with(ok, {{"webhook", "ftp://x"}})), QString("Webhook must be an http(s) URL"));
  CHECK_EQ(err(with(ok, {{"webhook", "https://a b"}})), QString("Webhook must be an http(s) URL"));
  CHECK_EQ(err(with(ok, {{"webhook", "HTTPS://hooks.example/x"}})), QString());
  CHECK_EQ(err(QJsonObject{{"symbol", "X"}, {"op", ">"}}), QString("An alert needs something to compare against"));
  CHECK_EQ(err(with(ok, {{"right", ""}})), QString("An alert needs something to compare against"));
  CHECK_EQ(err(with(ok, {{"right", QJsonValue::Null}})), QString("An alert needs something to compare against"));
  CHECK_EQ(err(with(ok, {{"right", 0}})), QString());
  const QString use = "Use one of >, <, >=, <=, crossesAbove, crossesBelow, crosses";
  CHECK_EQ(err(with(ok, {{"op", "bogus"}})), "Unknown operator \"bogus\". " + use);
  CHECK_EQ(err(QJsonObject{{"symbol", "X"}, {"right", 1}}), "Unknown operator \"undefined\". " + use);
  CHECK_EQ(err(with(ok, {{"left", "nope"}})),
           QString("Unknown operand \"nope\". Define it under indicators or use a price field or number."));
  CHECK_EQ(err(with(ok, {{"left", "rsi"}, {"indicators", QJsonObject{{"rsi", QJsonObject{{"id", "rsi"}}}}}})), QString());
  CHECK(err(with(ok, {{"left", "x"}, {"indicators", QJsonObject{{"x", QJsonObject{{"id", "nosuch"}}}}}})).contains("nosuch"));
  // conditions replace left/op/right; each one is checked
  CHECK_EQ(err(with(ok, {{"conditions", QJsonArray{QJsonObject{{"left", "close"}, {"op", "<"}, {"right", "open"}},
                                                   QJsonObject{{"left", "close"}, {"op", "?"}, {"right", 2}}}}})),
           "Unknown operator \"?\". " + use);
  CHECK_EQ(err(with(ok, {{"conditions", QJsonArray{QJsonObject{{"left", "close"}, {"op", ">"}, {"right",
      QJsonObject{{"line", QJsonObject{{"t1", 0}, {"p1", 1}, {"t2", 60}, {"p2", 2}}}}}}}}})), QString());
}

TEST("alerts: conditionsOf reads the conditions, or the single left/op/right") {
  CHECK_EQ(Alerts::conditionsOf(QJsonObject{{"left", "close"}, {"op", ">"}, {"right", 1}}),
           (QJsonArray{QJsonObject{{"left", "close"}, {"op", ">"}, {"right", 1}}}));
  CHECK_EQ(Alerts::conditionsOf(QJsonObject{{"left", "close"}, {"conditions", QJsonArray()}}),
           (QJsonArray{QJsonObject{{"left", "close"}}}));
  const QJsonArray two{QJsonObject{{"left", "a"}}, QJsonObject{{"left", "b"}}};
  CHECK_EQ(Alerts::conditionsOf(QJsonObject{{"left", "close"}, {"conditions", two}}), two);
}

TEST("alerts: describe says what an alert waits for") {
  const QJsonObject a{{"symbol", "BINANCE:BTCUSDT"}, {"tf", "1h"}, {"left", "close"}, {"op", "crossesAbove"}, {"right", 100000}};
  CHECK_EQ(Alerts::describe(a), QString("BINANCE:BTCUSDT 1h: close crosses above 100000"));
  CHECK_EQ(Alerts::describe(a, "BINANCE:ETHUSDT"), QString("BINANCE:ETHUSDT 1h: close crosses above 100000"));
  const QStringList ops = {">", "<", ">=", "<=", "crossesBelow", "crosses", "weird"};
  const QStringList words = {"is above", "is below", "is at or above", "is at or below", "crosses below", "crosses", "weird"};
  for (int i = 0; i < ops.size(); ++i)
    CHECK_EQ(Alerts::describe(with(a, {{"op", ops[i]}, {"right", 0.1}})), "BINANCE:BTCUSDT 1h: close " + words[i] + " 0.1");
  CHECK_EQ(Alerts::describe(with(a, {{"right", 1e21}})), QString("BINANCE:BTCUSDT 1h: close crosses above 1e+21"));
  const QJsonArray two{QJsonObject{{"left", "rsi"}, {"op", ">"}, {"right", 70}},
                       QJsonObject{{"left", "close"}, {"op", "<"}, {"right", QJsonObject{{"line", QJsonObject{{"t1", 1}}}}}}};
  CHECK_EQ(Alerts::describe(with(a, {{"conditions", two}})),
           QString("BINANCE:BTCUSDT 1h: rsi is above 70 and close is below your drawn line"));
  CHECK_EQ(Alerts::describe(with(a, {{"conditions", two}, {"match", "any"}})),
           QString("BINANCE:BTCUSDT 1h: rsi is above 70 or close is below your drawn line"));
  CHECK_EQ(Alerts::describe(with(a, {{"message", "BTC broke 100k"}})), QString("BTC broke 100k"));
  CHECK_EQ(Alerts::describe(with(a, {{"message", "Up"}, {"watchlist", "Crypto"}}), "BINANCE:SOLUSDT"), QString("BINANCE:SOLUSDT: Up"));
}

TEST("alerts: add fills the defaults and stores the JS shape in alerts.json") {
  setUp();
  auto& al = Alerts::instance();
  const QJsonObject a = addOk(QJsonObject{{"symbol", "BINANCE:BTCUSDT"}, {"right", 1}});
  CHECK(QRegularExpression("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$").match(a.value("id").toString()).hasMatch());
  CHECK_EQ(a.value("symbol").toString(), QString("BINANCE:BTCUSDT"));
  CHECK_EQ(a.value("tf").toString(), QString("1m"));
  CHECK_EQ(a.value("left").toString(), QString("close"));
  CHECK_EQ(a.value("op").toString(), QString("crossesAbove"));
  CHECK_EQ(a.value("right"), QJsonValue(1));
  CHECK_EQ(a.value("match").toString(), QString("all"));
  CHECK_EQ(a.value("indicators"), QJsonValue(QJsonObject()));
  CHECK_EQ(a.value("trigger").toString(), QString("once"));
  CHECK_EQ(a.value("webhook").toString(), QString(""));
  CHECK_EQ(a.value("message").toString(), QString(""));
  CHECK_EQ(a.value("active"), QJsonValue(true));
  CHECK(a.value("lastBar").isNull());
  CHECK_EQ(a.value("fired"), QJsonValue(0));
  CHECK(QRegularExpression(R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$)").match(a.value("createdAt").toString()).hasMatch());
  CHECK(!a.contains("conditions"));
  CHECK(!a.contains("watchlist"));
  CHECK_EQ(data::Error{}.status, 0);
  const QJsonArray stored = store::read("alerts", QJsonArray()).toArray();
  CHECK_EQ(stored.size(), 1);
  CHECK_EQ(stored[0].toObject(), a);
  // left / op / right come from the first condition when not given
  const QJsonObject c = addOk(QJsonObject{{"symbol", "X"}, {"conditions", QJsonArray{QJsonObject{{"left", "open"}, {"op", "<"}, {"right", 5}}}}});
  CHECK_EQ(c.value("left").toString(), QString("open"));
  CHECK_EQ(c.value("op").toString(), QString("<"));
  CHECK_EQ(c.value("right"), QJsonValue(5));
  CHECK(c.value("conditions").isArray());
  // a watchlist alert has an empty symbol
  const QJsonObject w = addOk(QJsonObject{{"watchlist", "Main"}, {"symbol", "IGNORED"}, {"right", 1}});
  CHECK_EQ(w.value("symbol").toString(), QString(""));
  CHECK_EQ(w.value("watchlist").toString(), QString("Main"));
  // refused
  for (const QJsonValue& bad : {QJsonValue("x"), QJsonValue(5), QJsonValue(QJsonValue::Null)}) {
    const auto r = al.add(bad);
    CHECK(!r.ok() && r.error->message == "Send the alert as a JSON object" && r.error->status == 400);
  }
  const auto arr = al.add(QJsonArray{1});
  CHECK(!arr.ok() && arr.error->message == "An alert needs a symbol or a watchlist");
  CHECK_EQ(al.list().size(), 3);
  tearDown();
}

TEST("alerts: update merges and keeps the id; remove deletes") {
  setUp();
  auto& al = Alerts::instance();
  int changes = 0;
  auto conn = QObject::connect(&al, &Alerts::changed, [&] { ++changes; });
  const QJsonObject a = addOk(kBase);
  const auto u = al.update(a.value("id").toString(), QJsonObject{{"active", false}, {"id", "other"}, {"message", "hi"}});
  CHECK(u.ok());
  CHECK_EQ(u.value.value("id"), a.value("id"));
  CHECK_EQ(u.value.value("active"), QJsonValue(false));
  CHECK_EQ(u.value.value("message").toString(), QString("hi"));
  CHECK_EQ(al.find(a.value("id").toString())->value("message").toString(), QString("hi"));
  const auto bad = al.update(a.value("id").toString(), QJsonObject{{"op", "nope"}});
  CHECK(!bad.ok() && bad.error->status == 400);
  CHECK_EQ(al.find(a.value("id").toString())->value("op").toString(), QString(">"));   // unchanged
  const auto none = al.update("doesnotexist", {});
  CHECK(!none.ok() && none.error->status == 404 && none.error->message == "No such alert");
  al.remove(a.value("id").toString());
  CHECK_EQ(al.list().size(), 0);
  CHECK_EQ(store::read("alerts", QJsonArray()).toArray().size(), 0);
  CHECK(changes >= 3);
  QObject::disconnect(conn);
  tearDown();
}

TEST("alerts: webhook bodies: Discord content, Slack and Telegram text, anything else the whole event") {
  const QJsonObject payload{{"alert", QJsonObject{{"id", "a"}}}, {"event", QJsonObject{{"id", "e"}}}};
  CHECK_EQ(Alerts::webhookBody("https://discord.com/api/webhooks/1/x", "hi", payload), (QJsonObject{{"content", "hi"}}));
  CHECK_EQ(Alerts::webhookBody("https://discordapp.com/api/webhooks/1/x", "hi", payload), (QJsonObject{{"content", "hi"}}));
  CHECK_EQ(Alerts::webhookBody("https://hooks.slack.com/services/T/B/x", "hi", payload), (QJsonObject{{"text", "hi"}}));
  CHECK_EQ(Alerts::webhookBody("https://api.telegram.org/bot1:x/sendMessage?chat_id=5", "hi", payload), (QJsonObject{{"text", "hi"}}));
  CHECK_EQ(Alerts::webhookBody("http://127.0.0.1:9000/hook", "hi", payload),
           (QJsonObject{{"text", "hi"}, {"content", "hi"}, {"alert", payload["alert"]}, {"event", payload["event"]}}));
}

TEST("alerts: sendWebhook posts JSON and reports failures like the JS") {
  setUp();
  const auto send = [](const QString& url) {
    QString out = "pending";
    QEventLoop loop;
    Alerts::sendWebhook(url, "hello", QJsonObject{{"event", QJsonObject{{"id", "e"}}}}, [&](const QString& e) { out = e; loop.quit(); });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (out == "pending") loop.exec();
    return out;
  };
  CHECK_EQ(send("https://hooks.slack.com/services/x"), QString());
  CHECK_EQ(posted().size(), 1);
  CHECK_EQ(posted()[0].method, QString("POST"));
  CHECK_EQ(posted()[0].contentType, QByteArray("application/json"));
  CHECK_EQ(posted()[0].body, QByteArray(R"({"text":"hello"})"));
  CHECK_EQ(send("https://my.example/hook"), QString());
  CHECK_EQ(QJsonDocument::fromJson(posted()[1].body).object(),
           (QJsonObject{{"text", "hello"}, {"content", "hello"}, {"event", QJsonObject{{"id", "e"}}}}));
  hookStatus() = 500;
  CHECK_EQ(send("https://my.example/hook"), QString("Webhook returned HTTP 500"));
  CHECK_EQ(send("https://down.example/hook"), QString("fetch failed"));
  CHECK_EQ(send("http://[::1"), QString("Invalid URL"));
  tearDown();
}

TEST("alerts: \"once\" fires once and switches off; the event has the JS shape") {
  setUp();
  auto& al = Alerts::instance();
  QList<QJsonObject> heard;
  auto conn = QObject::connect(&al, &Alerts::fired, [&](const QJsonObject& e) { heard << e; });
  const QJsonObject a = addOk(kBase);
  evaluateNow();
  evaluateNow();
  const QJsonArray ev = eventsFor(a);
  CHECK_EQ(ev.size(), 1);
  CHECK_EQ(heard.size(), 1);
  const QJsonObject e = ev[0].toObject();
  CHECK_EQ(heard.value(0), e);
  QStringList keys = e.keys();
  CHECK_EQ(keys, (QStringList{"alertId", "at", "barTime", "id", "price", "symbol", "text", "tf"}));
  const Bars bars = fixtureBars();
  CHECK_EQ(e.value("barTime").toDouble(), double(bars.last().time));
  CHECK_EQ(e.value("price").toDouble(), bars.last().close);
  CHECK_EQ(e.value("symbol").toString(), QString("BINANCE:BTCUSDT"));
  CHECK_EQ(e.value("text").toString(),
           "Alert: BINANCE:BTCUSDT 1D: close is above 1 (price " + store::numberToString(bars.last().close) + ")");
  const QJsonObject after = *al.find(a.value("id").toString());
  CHECK_EQ(after.value("active"), QJsonValue(false));
  CHECK_EQ(after.value("fired"), QJsonValue(1));
  CHECK_EQ(after.value("lastFiredAt"), e.value("at"));
  CHECK_EQ(after.value("lastBar").toDouble(), double(bars.last().time));
  CHECK_EQ(store::read("alert-log", QJsonArray()).toArray().first().toObject(), e);
  QObject::disconnect(conn);
  tearDown();
}

TEST("alerts: oncePerBar fires once per bar; oncePerBarClose reads the last closed bar") {
  setUp();
  auto& al = Alerts::instance();
  const Bars bars = fixtureBars();
  const QJsonObject a = addOk(with(kBase, {{"trigger", "oncePerBar"}}));
  const QJsonObject c = addOk(with(kBase, {{"trigger", "oncePerBarClose"}}));
  evaluateNow();
  evaluateNow();
  CHECK_EQ(eventsFor(a).size(), 1);
  CHECK_EQ(eventsFor(c).size(), 1);
  CHECK_EQ(eventsFor(c)[0].toObject().value("barTime").toDouble(), double(bars[bars.size() - 2].time));
  CHECK_EQ(eventsFor(c)[0].toObject().value("price").toDouble(), bars[bars.size() - 2].close);
  CHECK_EQ(al.find(a.value("id").toString())->value("active"), QJsonValue(true));   // stays on
  // a new bar (here: an older lastBar) fires again
  CHECK(al.update(a.value("id").toString(), QJsonObject{{"lastBar", 1}}).ok());
  evaluateNow();
  CHECK_EQ(eventsFor(a).size(), 2);
  // switched off: not checked
  CHECK(al.update(a.value("id").toString(), QJsonObject{{"active", false}, {"lastBar", 1}}).ok());
  evaluateNow();
  CHECK_EQ(eventsFor(a).size(), 2);
  // a condition that does not hold never fires
  const QJsonObject never = addOk(with(kBase, {{"op", "<"}, {"trigger", "oncePerBar"}}));
  evaluateNow();
  CHECK_EQ(eventsFor(never).size(), 0);
  tearDown();
}

TEST("alerts: a watchlist alert fires once per symbol (once) or once per bar per symbol") {
  setUp();
  auto& al = Alerts::instance();
  store::mergeState(QJsonObject{{"watchlists", QJsonObject{{"Crypto", QJsonArray{"BINANCE:BTCUSDT", "BINANCE:ETHUSDT", "BINANCE:BTCUSDT"}}}}});
  const QJsonObject a = addOk(QJsonObject{{"watchlist", "Crypto"}, {"tf", "1D"}, {"op", ">"}, {"right", 0}, {"message", "Up"}});
  evaluateNow();
  evaluateNow();
  const QJsonArray ev = eventsFor(a);
  CHECK_EQ(ev.size(), 2);   // the duplicate symbol counts once
  QStringList texts;
  for (const QJsonValue& e : ev) texts << e.toObject().value("text").toString();
  texts.sort();
  CHECK(texts.value(0).startsWith("Alert: BINANCE:BTCUSDT: Up (price "));
  CHECK(texts.value(1).startsWith("Alert: BINANCE:ETHUSDT: Up (price "));
  const QJsonObject after = *al.find(a.value("id").toString());
  CHECK_EQ(after.value("active"), QJsonValue(true));   // "once" means once per symbol: it stays on
  CHECK_EQ(after.value("fired"), QJsonValue(2));
  CHECK_EQ(after.value("lastBars").toObject().keys(), (QStringList{"BINANCE:BTCUSDT", "BINANCE:ETHUSDT"}));
  // a symbol added to the list is checked on the next run
  store::mergeState(QJsonObject{{"watchlists", QJsonObject{{"Crypto", QJsonArray{"BINANCE:BTCUSDT", "BINANCE:SOLUSDT"}}}}});
  evaluateNow();
  CHECK_EQ(eventsFor(a).size(), 3);
  // oncePerBar on a watchlist: same bar, no second fire
  const QJsonObject b = addOk(QJsonObject{{"watchlist", "Crypto"}, {"tf", "1D"}, {"op", ">"}, {"right", 0}, {"trigger", "oncePerBar"}});
  evaluateNow();
  evaluateNow();
  CHECK_EQ(eventsFor(b).size(), 2);
  // an unknown watchlist checks nothing
  const QJsonObject none = addOk(QJsonObject{{"watchlist", "Nope"}, {"tf", "1D"}, {"op", ">"}, {"right", 0}});
  evaluateNow();
  CHECK_EQ(eventsFor(none).size(), 0);
  tearDown();
}

TEST("alerts: a webhook is posted when an alert fires, and the event says how it went") {
  setUp();
  auto& al = Alerts::instance();
  const QJsonObject a = addOk(with(kBase, {{"webhook", "https://my.example/hook"}, {"trigger", "oncePerBar"}}));
  evaluateNow();
  CHECK_EQ(posted().size(), 1);
  const QJsonObject body = QJsonDocument::fromJson(posted()[0].body).object();
  CHECK_EQ(body.keys(), (QStringList{"alert", "content", "event", "text"}));
  CHECK_EQ(body.value("alert").toObject().value("fired"), QJsonValue(1));   // the alert as it is after firing
  CHECK(!body.value("event").toObject().contains("webhook"));
  CHECK_EQ(body.value("text"), body.value("event").toObject().value("text"));
  CHECK_EQ(eventsFor(a)[0].toObject().value("webhook").toString(), QString("sent"));
  hookStatus() = 404;
  CHECK(al.update(a.value("id").toString(), QJsonObject{{"lastBar", 1}}).ok());
  evaluateNow();
  CHECK_EQ(eventsFor(a)[0].toObject().value("webhook").toString(), QString("failed: Webhook returned HTTP 404"));
  const QJsonObject d = addOk(with(kBase, {{"webhook", "https://discord.com/api/webhooks/1/x"}}));
  evaluateNow();
  CHECK_EQ(QJsonDocument::fromJson(posted().last().body).object().keys(), (QStringList{"content"}));
  CHECK_EQ(eventsFor(d).size(), 1);
  tearDown();
}

TEST("alerts: the log keeps the last 500 events, newest first") {
  setUp();
  auto& al = Alerts::instance();
  QJsonArray old;
  for (int i = 0; i < 500; ++i) old.append(QJsonObject{{"id", QString("old-%1").arg(i)}, {"alertId", "gone"}});
  store::write("alert-log", old);
  al.reload();
  CHECK_EQ(al.history().size(), 500);
  const QJsonObject a = addOk(kBase);
  evaluateNow();
  CHECK_EQ(al.history().size(), 500);
  CHECK_EQ(al.history()[0].toObject().value("alertId"), a.value("id"));
  CHECK_EQ(al.history()[1].toObject().value("id").toString(), QString("old-0"));
  CHECK_EQ(al.history()[499].toObject().value("id").toString(), QString("old-498"));
  CHECK_EQ(store::read("alert-log", QJsonArray()).toArray().size(), 500);
  store::write("alert-log", QJsonArray());
  al.reload();
  tearDown();
}

TEST("alerts: one evaluation at a time; a rule that breaks at check time records its error") {
  setUp();
  auto& al = Alerts::instance();
  const QJsonObject a = addOk(kBase);
  int done = 0;
  QEventLoop loop;
  for (int i = 0; i < 3; ++i) al.evaluate([&] { if (++done == 3) loop.quit(); });
  CHECK(al.evaluating());
  QTimer::singleShot(10000, &loop, &QEventLoop::quit);
  loop.exec();
  CHECK_EQ(done, 3);
  CHECK_EQ(eventsFor(a).size(), 1);
  // an operator the file was edited to: validate never saw it, the check stores the JS error on the alert
  QJsonArray list = al.list();
  QJsonObject bad = with(kBase, {{"id", "edited"}, {"op", "bogus"}, {"active", true}, {"trigger", "oncePerBar"}});
  list.append(bad);
  store::write("alerts", list);
  al.reload();
  evaluateNow();
  CHECK_EQ(al.find("edited")->value("error").toString(), QString("Unknown operator \"bogus\""));
  tearDown();
}

TEST("alerts: the checker runs on its own once started, and stops") {
  setUp();
  auto& al = Alerts::instance();
  const QJsonObject a = addOk(with(kBase, {{"trigger", "oncePerBar"}}));
  al.start(100);
  CHECK(al.started());
  for (int i = 0; i < 60 && eventsFor(a).isEmpty(); ++i) pump(100);   // first check 2 s after start
  CHECK_EQ(eventsFor(a).size(), 1);
  al.stop();
  CHECK(!al.started());
  CHECK(al.update(a.value("id").toString(), QJsonObject{{"lastBar", 1}}).ok());
  pump(500);
  CHECK_EQ(eventsFor(a).size(), 1);
  tearDown();
}

// Two Deepwaters on one data folder: the other one is simulated by writes to alerts.json / alert-log.json made
// straight to the files (what its Alerts does), between this instance's operations.
namespace {
QJsonArray onDisk() { return store::read("alerts", QJsonArray()).toArray(); }
QJsonObject onDiskById(const QString& id) {
  for (const QJsonValue& v : onDisk()) if (v.toObject().value("id").toString() == id) return v.toObject();
  return {};
}
// The other instance changes one alert (or adds it when `id` is new).
void elsewhere(const QString& id, const QJsonObject& fields) {
  QJsonArray list = onDisk();
  bool found = false;
  for (int i = 0; i < list.size(); ++i) {
    QJsonObject a = list[i].toObject();
    if (a.value("id").toString() != id) continue;
    for (auto it = fields.begin(); it != fields.end(); ++it) a.insert(it.key(), it.value());
    list[i] = a;
    found = true;
  }
  if (!found) list.append(with(with(kBase, {{"id", id}, {"active", true}, {"fired", 0}, {"trigger", "once"}}), fields));
  store::write("alerts", list);
}
}  // namespace

TEST("alerts: two instances on one alerts.json keep each other's changes, and changes on disk are announced") {
  setUp();
  auto& al = Alerts::instance();
  int changes = 0;
  auto conn = QObject::connect(&al, &Alerts::changed, [&] { ++changes; });
  const QJsonObject a = addOk(kBase);
  const QString aid = a.value("id").toString();

  // the other instance adds B; our update of A keeps B (it used to write its own list over it)
  elsewhere("b", {{"message", "from B"}});
  CHECK(al.update(aid, QJsonObject{{"message", "edited here"}}).ok());
  CHECK_EQ(onDisk().size(), 2);
  CHECK_EQ(onDiskById("b").value("message").toString(), QString("from B"));
  CHECK_EQ(onDiskById(aid).value("message").toString(), QString("edited here"));

  // it edits A; our update of B is made on top of that edit, and a PATCH of its alert we never listed works
  elsewhere(aid, {{"message", "edited there"}});
  elsewhere("c", {{"message", "from C"}});
  CHECK(al.update("c", QJsonObject{{"active", false}}).ok());
  CHECK_EQ(onDiskById(aid).value("message").toString(), QString("edited there"));
  CHECK_EQ(onDiskById("c").value("active"), QJsonValue(false));
  CHECK_EQ(onDisk().size(), 3);

  // remove keeps what the other one did since
  elsewhere("b", {{"message", "B again"}});
  al.remove("c");
  CHECK_EQ(onDisk().size(), 2);
  CHECK_EQ(onDiskById("b").value("message").toString(), QString("B again"));

  // its delete is not undone by our next change; a change on disk shows in list()/find() and emits changed()
  {
    QJsonArray list;
    for (const QJsonValue& v : onDisk()) if (v.toObject().value("id").toString() != "b") list.append(v);
    store::write("alerts", list);
  }
  changes = 0;
  CHECK_EQ(al.list().size(), 1);
  CHECK(!al.find("b").has_value());
  for (int i = 0; i < 50 && changes == 0; ++i) pump(100);
  CHECK(changes >= 1);
  const auto gone = al.update("b", QJsonObject{{"message", "x"}});
  CHECK(!gone.ok() && gone.error->status == 404);
  CHECK(addOk(with(kBase, {{"message", "D"}})).value("message") == QJsonValue("D"));
  CHECK_EQ(onDisk().size(), 2);

  // and one with no API call at all: the folder watcher sees it
  changes = 0;
  elsewhere("e", {{"message", "from E"}});
  for (int i = 0; i < 50 && changes == 0; ++i) pump(100);
  CHECK(changes >= 1);
  CHECK(al.find("e").has_value());
  QObject::disconnect(conn);
  tearDown();
}

TEST("alerts: the checker's writes (lastBar, fired, active, log) don't undo the other instance's edits") {
  setUp();
  auto& al = Alerts::instance();
  const QJsonObject a = addOk(with(kBase, {{"trigger", "oncePerBar"}}));
  const QString aid = a.value("id").toString();
  // edited elsewhere before the run: the run checks and fires the edited alert and keeps the edit
  elsewhere(aid, {{"message", "edited there"}});
  elsewhere("other", {{"op", "<"}, {"message", "never fires"}});
  evaluateNow();
  const QJsonObject after = onDiskById(aid);
  CHECK_EQ(after.value("message").toString(), QString("edited there"));
  CHECK_EQ(after.value("fired"), QJsonValue(1));
  CHECK(after.value("lastBar").isDouble());
  CHECK_EQ(onDiskById("other").value("message").toString(), QString("never fires"));
  CHECK_EQ(eventsFor(a).at(0).toObject().value("text").toString().left(19), QString("Alert: edited there"));

  // edited elsewhere during the run (while the candles load): the edit stays, the fire is recorded too
  const QJsonObject o = addOk(kBase);   // "once": switched off when it fires
  const QString oid = o.value("id").toString();
  bool done = false;
  QEventLoop loop;
  al.evaluate([&] { done = true; loop.quit(); });
  elsewhere(oid, {{"message", "mid-run edit"}});
  elsewhere("late", {{"op", "<"}});
  QTimer::singleShot(15000, &loop, &QEventLoop::quit);
  if (!done) loop.exec();
  CHECK(done);
  const QJsonObject o2 = onDiskById(oid);
  CHECK_EQ(o2.value("message").toString(), QString("mid-run edit"));
  CHECK_EQ(o2.value("active"), QJsonValue(false));
  CHECK_EQ(o2.value("fired"), QJsonValue(1));
  CHECK(!onDiskById("late").isEmpty());

  // the log: an event the other instance logged (it checked before this one took over) is kept
  QJsonArray log = store::read("alert-log", QJsonArray()).toArray();
  log.prepend(QJsonObject{{"id", "theirs"}, {"alertId", "gone"}, {"text", "from the other instance"}});
  store::write("alert-log", log);
  CHECK_EQ(al.history().at(0).toObject().value("id").toString(), QString("theirs"));
  CHECK(al.update(aid, QJsonObject{{"lastBar", 1}}).ok());
  evaluateNow();
  const QJsonArray log2 = store::read("alert-log", QJsonArray()).toArray();
  CHECK_EQ(log2.at(0).toObject().value("alertId"), a.value("id"));
  CHECK_EQ(log2.at(1).toObject().value("id").toString(), QString("theirs"));
  CHECK_EQ(log2.size(), log.size() + 1);
  store::write("alert-log", QJsonArray());
  al.reload();
  tearDown();
}

// Audit D19: tf is validated; an alert whose candles can't load says so on the alert (it failed silently forever).
TEST("alerts: a bad timeframe is refused; candles that can't load are recorded on the alert, cleared when they do") {
  setUp();
  auto& al = Alerts::instance();
  CHECK(err(with(kBase, {{"tf", "2x"}})).startsWith("Bad timeframe"));
  CHECK(err(with(kBase, {{"tf", 5}})).startsWith("tf must be"));
  CHECK_EQ(err(with(kBase, {{"tf", "4h"}})), QString());
  CHECK(!al.add(with(kBase, {{"tf", "nope"}})).ok());
  // one stored before tf was checked
  QJsonArray list = al.list();
  list.append(with(kBase, {{"id", "badtf"}, {"tf", "2x"}, {"active", true}, {"trigger", "oncePerBar"}}));
  store::write("alerts", list);
  al.reload();
  evaluateNow();
  CHECK(!al.find("badtf")->value("loadError").toString().isEmpty());
  // fixed by hand: the next check loads the candles and the note goes
  list = al.list();
  for (int i = 0; i < list.size(); ++i)
    if (list[i].toObject().value("id") == QJsonValue("badtf")) list[i] = with(list[i].toObject(), {{"tf", "1D"}});
  store::write("alerts", list);
  al.reload();
  evaluateNow();
  CHECK(!al.find("badtf")->contains("loadError"));
  tearDown();
}

// Audit D22: an alerts.json that isn't JSON is copied aside before a change writes over it.
TEST("alerts: an unparsable alerts.json is copied aside before it is replaced") {
  setUp();
  auto& al = Alerts::instance();
  const QString dir = store::dataDir();
  for (const QString& f : QDir(dir).entryList({"alerts.json.bad-*"})) QFile::remove(dir + "/" + f);
  {
    QFile f(dir + "/alerts.json");
    (void)f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write("[{\"id\": \"mine\", \"symbol\": \"X\"");   // cut
  }
  al.reload();
  addOk(kBase);   // writes alerts.json
  const QStringList bad = QDir(dir).entryList({"alerts.json.bad-*"});
  CHECK_EQ(int(bad.size()), 1);
  QFile b(dir + "/" + bad.value(0));
  CHECK(b.open(QIODevice::ReadOnly) && b.readAll().contains("\"mine\""));
  b.close();
  for (const QString& f : bad) QFile::remove(dir + "/" + f);
  tearDown();
}

// Audit G11: the checker rewrote alerts.json at the end of every check (every 15 s) with nothing changed: a disk
// write, the watcher's callback and the Alerts view rebuilt each time.
TEST("alerts: a check that changes nothing does not rewrite alerts.json") {
  setUp();
  auto& al = Alerts::instance();
  const QJsonObject a = addOk(with(kBase, {{"trigger", "oncePerBar"}}));
  evaluateNow();   // fires: lastBar and the log are written
  CHECK_EQ(eventsFor(a).size(), 1);
  const QString file = store::dataDir() + "/alerts.json";
  const QDateTime before = QFileInfo(file).lastModified();
  QFile f(file);
  CHECK(f.open(QIODevice::ReadOnly));
  const QByteArray bytes = f.readAll();
  f.close();
  pump(30);
  int changes = 0;
  const auto c = QObject::connect(&al, &Alerts::changed, [&] { ++changes; });
  evaluateNow();   // the same bar: nothing fires, nothing changes
  pump(10);
  QObject::disconnect(c);
  CHECK_EQ(changes, 0);
  CHECK(QFileInfo(file).lastModified() == before);
  QFile g(file);
  CHECK(g.open(QIODevice::ReadOnly));
  CHECK(g.readAll() == bytes);
  tearDown();
}
