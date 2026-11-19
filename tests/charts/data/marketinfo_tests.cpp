// lib/market-info.js: the parsing of Nasdaq's and the RSS feeds' answers, checked against what the JS code makes
// of the same saved answers (fixtures/*.expected.json were written by node running the JS on these files).
// No network: the request path is checked through Fetch's test transport.
#include "Check.h"
#include "charts/data/MarketInfo.h"
#include "charts/data/Fetch.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QUrlQuery>
#include <cmath>

using namespace charts;
using namespace charts::data;

namespace {
QString fixtures() { return QFileInfo(QStringLiteral(__FILE__)).absolutePath() + "/fixtures"; }
QByteArray slurp(const QString& name) { QFile f(fixtures() + "/" + name); (void)f.open(QIODevice::ReadOnly); return f.readAll(); }
QJsonValue json(const QString& name) {
  const QJsonDocument d = QJsonDocument::fromJson("[" + slurp(name) + "]");
  return d.array().at(0);
}
// Compare item by item so a failure names the first difference.
void sameArray(const QJsonArray& got, const QJsonArray& want, const char* what) {
  CHECK_EQ(got.size(), want.size());
  for (int i = 0; i < std::min(got.size(), want.size()); ++i)
    if (got.at(i) != want.at(i)) {
      check::fail(__FILE__, __LINE__, QString("%1[%2]: %3 != %4").arg(what).arg(i)
                                          .arg(QString::fromUtf8(QJsonDocument(got.at(i).toObject()).toJson(QJsonDocument::Compact)),
                                               QString::fromUtf8(QJsonDocument(want.at(i).toObject()).toJson(QJsonDocument::Compact))));
      return;
    }
}
template <class F>
void waitFor(F done, int ms = 5000) {
  QElapsedTimer t;
  t.start();
  while (!done() && t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}
}  // namespace

TEST("parseRss: Cointelegraph and Decrypt items as the JS reads them") {
  const QJsonObject want = json("rss.expected.json").toObject();
  sameArray(parseRss(QString::fromUtf8(slurp("rss-cointelegraph.xml")), "Cointelegraph"), want.value("cointelegraph").toArray(), "cointelegraph");
  sameArray(parseRss(QString::fromUtf8(slurp("rss-decrypt.xml")), "Decrypt"), want.value("decrypt").toArray(), "decrypt");
  // entities, CDATA and tags; an item without a title is left out; no date -> null
  const QJsonArray a = parseRss("<rss><item><title><![CDATA[A &amp;amp; B &#039;x&#39; <b>bold</b>]]></title><link>u</link></item>"
                                "<item><title></title></item></rss>", "S");
  CHECK_EQ(a.size(), 1);
  CHECK_EQ(a.at(0).toObject().value("title").toString(), QString("A &amp; B 'x' bold"));   // one pass, as the JS: &amp;amp; -> &amp;
  CHECK(a.at(0).toObject().value("time").isNull());
  CHECK_EQ(a.at(0).toObject().value("summary").toString(), QString(""));
}

TEST("Date.parse forms used by the feeds") {
  CHECK_EQ(jsDateParse("Mon, 16 Nov 2026 13:31:49 +0000"), 1794835909000.0);
  CHECK_EQ(jsDateParse("Mon, 16 Nov 2026 08:31:49 -0500"), 1794835909000.0);
  CHECK_EQ(jsDateParse("Mon, 16 Nov 2026 13:31:49 GMT"), 1794835909000.0);
  CHECK_EQ(jsDateParse("2026-11-16T13:31:49Z"), 1794835909000.0);
  CHECK_EQ(jsDateParse("2026-11-16T13:31:49.000+00:00"), 1794835909000.0);
  CHECK_EQ(jsDateParse("2026-11-16"), 1794787200000.0);                 // date-only ISO: UTC
  CHECK_EQ(jsDateParse("Nov 15, 2026"), 1794697200000.0);               // local time (TZ=Europe/Paris in main)
  CHECK(std::isnan(jsDateParse("")));
  CHECK(std::isnan(jsDateParse("yesterday")));
  CHECK_EQ(jsParseFloat("1.2B"), 1.2);
  CHECK(std::isnan(jsParseFloat("--")));
  CHECK_EQ(jsParseFloat("  -3e2x"), -300.0);
}

TEST("earnings: one day of Nasdaq's calendar as the JS maps it") {
  const QJsonDocument day = QJsonDocument::fromJson(slurp("nasdaq-earnings-day.json"));
  sameArray(earningsRows(day, "2026-11-17"), json("nasdaq-earnings-day.expected.json").toArray(), "earnings");
  CHECK(earningsRows(QJsonDocument::fromJson("{\"data\":{\"rows\":null}}"), "x").isEmpty());
  const QJsonArray d = dividendRows(QJsonDocument::fromJson(
      R"({"data":{"calendar":{"rows":[{"companyName":"Apple","symbol":"AAPL","dividend_Ex_Date":"11/12/2026","payment_Date":"11/16/2026","record_Date":"11/12/2026","dividend_Rate":0.27,"indicated_Annual_Dividend":1.08}]}}})"),
      "2026-11-12");
  CHECK_EQ(d.size(), 1);
  CHECK_EQ(d.at(0).toObject(), (QJsonObject{{"date", "2026-11-12"}, {"symbol", "US:AAPL"}, {"ticker", "AAPL"}, {"name", "Apple"},
                                            {"exDate", "11/12/2026"}, {"payDate", "11/16/2026"}, {"recordDate", "11/12/2026"},
                                            {"amount", 0.27}, {"annual", 1.08}}));
}

TEST("weekdays: the next n weekdays from today, UTC") {
  CHECK_EQ(weekdays(5, QDate(2026, 11, 13)), (QStringList{"2026-11-13", "2026-11-16", "2026-11-17", "2026-11-18", "2026-11-19"}));
  CHECK_EQ(weekdays(1, QDate(2026, 11, 14)), (QStringList{"2026-11-16"}));
}

TEST("news: Nasdaq headlines by symbol, and crypto headlines picked by coin") {
  sameArray(nasdaqNewsRows(QJsonDocument::fromJson(slurp("nasdaq-news-aapl.json"))), json("nasdaq-news-aapl.expected.json").toArray(), "news");
  CHECK_EQ(coinBase("BTCUSDT"), QString("BTC"));
  CHECK_EQ(coinBase("ETH-USD"), QString("ETH"));
  CHECK_EQ(coinBase("SOLUSDT.P"), QString("SOL"));
  CHECK_EQ(coinBase("XBTUSD"), QString("BTC"));
  CHECK_EQ(coinBase("USDT"), QString("BTC"));
  const QJsonObject want = json("rss.expected.json").toObject();
  QJsonArray all = want.value("cointelegraph").toArray();
  for (const auto& x : want.value("decrypt").toArray()) all.append(x);
  const QJsonObject btc = cryptoHeadlines("BTC", all);
  int hits = 0;
  const QRegularExpression re("\\b(bitcoin|btc)\\b", QRegularExpression::CaseInsensitiveOption);
  for (const auto& x : all) hits += re.match(x.toObject().value("title").toString() + " " + x.toObject().value("summary").toString()).hasMatch();
  if (hits >= 3) {
    CHECK_EQ(btc.value("note").toString(), QString("Headlines mentioning BTC"));
    CHECK_EQ(btc.value("items").toArray().size(), std::min(hits, 30));
  }
  const QJsonObject zzz = cryptoHeadlines("ZZZQ", all);
  CHECK_EQ(zzz.value("note").toString(), QString("Few headlines mention ZZZQ right now, so these are the latest crypto headlines"));
  CHECK_EQ(zzz.value("items").toArray().size(), std::min<int>(30, all.size()));
}

TEST("financials: Nasdaq's statements and key stats in Nasdaq's order") {
  const Symbol aapl{"US:AAPL", "US", "AAPL"};
  const auto r = financialsFrom(aapl, "annual", slurp("nasdaq-financials-aapl.json"), slurp("nasdaq-summary-aapl.json"),
                                slurp("nasdaq-earnings-date-aapl.json"));
  CHECK(r.ok());
  const QJsonObject got = r.value.toObject(), want = json("nasdaq-financials-aapl.expected.json").toObject();
  for (const QString& k : want.keys())
    if (got.value(k) != want.value(k)) check::fail(__FILE__, __LINE__, "financials." + k + " differs");
  CHECK_EQ(got.keys(), want.keys());
  CHECK_EQ(got.value("stats").toArray().at(0).toObject().value("label").toString(), QString("Exchange"));   // not sorted
  // nothing at all: 404 with the JS text
  const auto none = financialsFrom(Symbol{"US:SPY", "US", "SPY"}, "annual", {}, {}, {});
  CHECK(!none.ok());
  if (!none.ok()) {
    CHECK_EQ(none.error->status, 404);
    CHECK_EQ(none.error->message, QString("Nasdaq has no financials for SPY (ETFs and funds don't file them)."));
  }
  CHECK_EQ(jsonKeysInOrder(R"({"b":1,"10":2,"a":{"z":1,"y":2},"2":3})", {}), (QStringList{"2", "10", "b", "a"}));
  CHECK_EQ(jsonKeysInOrder(R"({"b":1,"a":{"z":[1,{"q":2}],"y":"}\""}})", {"a"}), (QStringList{"z", "y"}));
}

TEST("financials, earnings and news go through the transport with Nasdaq's headers") {
  QList<net::Request> seen;
  testing::setTransport([&seen](const net::Request& r, net::Callback cb) {
    seen << r;
    net::Reply rep;
    rep.status = 200;
    const QString path = r.url.path();
    rep.body = path.endsWith("/financials") ? slurp("nasdaq-financials-aapl.json")
             : path.endsWith("/summary") ? slurp("nasdaq-summary-aapl.json")
             : path.endsWith("/earnings-date") ? slurp("nasdaq-earnings-date-aapl.json")
             : path.endsWith("/calendar/earnings") ? slurp("nasdaq-earnings-day.json")
             : path.endsWith("/articlebysymbol") && r.url.query().contains("stocks") ? QByteArray("{\"data\":{\"rows\":[]}}")
             : path.endsWith("/articlebysymbol") ? slurp("nasdaq-news-aapl.json")
             : QByteArray();
    QTimer::singleShot(0, [cb, rep] { cb(rep); });
  });
  bool done = false;
  Result<QJsonValue> fin;
  financials(Symbol{"US:AAPL", "US", "AAPL"}, "quarterly", nullptr, [&](const Result<QJsonValue>& r) { fin = r; done = true; });
  waitFor([&] { return done; });
  CHECK(done && fin.ok());
  CHECK_EQ(fin.value.toObject().value("frequency").toString(), QString("quarterly"));
  CHECK_EQ(seen.size(), 3);
  if (!seen.isEmpty()) {
    CHECK(seen[0].url.toString().contains("/api/company/AAPL/financials?frequency=2"));
    bool origin = false, referer = false;
    for (const auto& [k, v] : seen[0].headers) {
      origin = origin || (k == "Origin" && v == "https://www.nasdaq.com");
      referer = referer || (k == "Referer" && v == "https://www.nasdaq.com/");
    }
    CHECK(origin && referer);
  }
  done = false;
  Result<QJsonValue> notUs;
  financials(Symbol{"BINANCE:BTCUSDT", "BINANCE", "BTCUSDT"}, "annual", nullptr, [&](const Result<QJsonValue>& r) { notUs = r; done = true; });
  waitFor([&] { return done; });
  CHECK(!notUs.ok() && notUs.error->status == 422);
  QJsonArray earn;
  done = false;
  seen.clear();
  earnings(2, nullptr, [&](const QJsonArray& a) { earn = a; done = true; });
  waitFor([&] { return done; });
  CHECK_EQ(seen.size(), 2);   // two weekdays, each its own (cached) request
  CHECK_EQ(earn.size(), 2 * json("nasdaq-earnings-day.expected.json").toArray().size());
  QJsonObject nws;
  done = false;
  seen.clear();
  news(Symbol{"US:AAPL", "US", "AAPL"}, nullptr, [&](const QJsonObject& o) { nws = o; done = true; });
  waitFor([&] { return done; });
  CHECK_EQ(seen.size(), 2);   // stocks had nothing, etf had headlines
  CHECK_EQ(nws.value("note").toString(), QString("Headlines for AAPL from Nasdaq"));
  CHECK_EQ(nws.value("items").toArray().size(), 30);
  done = false;
  news(Symbol{"FX:EURUSD", "FX", "EURUSD"}, nullptr, [&](const QJsonObject& o) { nws = o; done = true; });
  waitFor([&] { return done; });
  CHECK_EQ(nws.value("note").toString(), QString("Headlines are available for crypto and US stocks."));
  testing::setTransport(nullptr);
}
