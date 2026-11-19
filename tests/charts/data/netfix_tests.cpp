// Regression tests for the 2026-11-16 audit (docs/charts/AUDIT.md): C2 (SPREAD: crash), S1 (keys in network error
// texts), W3 (a cut-off 200 kept in the disk cache), W12 (requests queued to a down host each waiting a timeout).
// The last two use the real net::Http against a local QTcpServer (127.0.0.1, no outside traffic).
#include "Check.h"
#include "FakeNet.h"
#include "charts/data/Providers.h"
#include "charts/data/Fetch.h"
#include "net/Http.h"
#include "net/Sources.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QTimeZone>
#include <memory>
#include <optional>

using namespace charts;
using namespace charts::data;

TEST("audit S1: network error texts keep only scheme://host of every URL") {
  CHECK_EQ(sources::redactUrls("Error transferring https://api.telegram.org/bot123:SECRET/sendMessage - server replied: Forbidden"),
           QString("Error transferring https://api.telegram.org - server replied: Forbidden"));
  CHECK_EQ(sources::redactUrls("Error transferring https://api.stlouisfed.org/fred/series?api_key=SECRET&id=GDP - server replied: Bad Request"),
           QString("Error transferring https://api.stlouisfed.org - server replied: Bad Request"));
  CHECK_EQ(sources::redactUrls("https://user:pw@example.com:8443/x?y=1#z"), QString("https://example.com:8443"));
  CHECK_EQ(sources::redactUrls("Host api.binance.com not found"), QString("Host api.binance.com not found"));
  CHECK_EQ(sources::redactUrls("HTTP 403"), QString("HTTP 403"));
}

TEST("audit C2: a SPREAD: symbol that isn't a spread answers an error instead of crashing") {
  data::testing::reset();
  fakenet::install([](const QUrl&) { return fakenet::Reply::text("not mocked", 404); });
  for (const QString s : {"SPREAD:FOO", "spread:a/b"}) {
    const auto r = fakenet::await<CandlesResult>([s](Callback<CandlesResult> cb) { getCandles(s, "1D", 100, nullptr, cb); });
    CHECK(!r.ok());
    if (!r.ok()) CHECK_EQ(r.error->status, 500);
  }
  data::testing::setTransport({});
}

namespace {
// A local server: "cut" answers 200 with a Content-Length longer than the body then closes; "hang" never answers.
struct LocalServer {
  QTcpServer srv;
  QString mode;
  QList<QTcpSocket*> held;
  explicit LocalServer(const QString& m) : mode(m) {
    srv.listen(QHostAddress::LocalHost, 0);
    QObject::connect(&srv, &QTcpServer::newConnection, [this] {
      while (QTcpSocket* s = srv.nextPendingConnection()) {
        if (mode == "hang") { held << s; continue; }
        QObject::connect(s, &QTcpSocket::readyRead, s, [s] {
          s->readAll();
          s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 1000\r\n\r\n{\"cut\":");
          s->flush();
          s->disconnectFromHost();
        });
      }
    });
  }
  QString url(const QString& path) const { return QString("http://127.0.0.1:%1%2").arg(srv.serverPort()).arg(path); }
};
void spin(int ms) { QEventLoop l; QTimer::singleShot(ms, &l, &QEventLoop::quit); l.exec(); }
}  // namespace

TEST("audit W3: a 200 whose body is cut off is a failure and isn't cached") {
  LocalServer srv("cut");
  net::Request r;
  r.url = QUrl(srv.url("/cut"));
  r.ttlSec = 3600;
  r.timeoutMs = 3000;
  std::optional<net::Reply> got;
  net::get(r, nullptr, [&](const net::Reply& rep) { got = rep; });
  for (int i = 0; i < 100 && !got; ++i) spin(30);
  CHECK(got.has_value());
  if (got) CHECK(!got->ok());
  CHECK(net::cached(r).body.isEmpty());
}

TEST("audit W12: requests queued to a host that never answers fail after one timeout, not one each") {
  data::testing::reset();
  data::testing::setTransport({});   // the real net::Http and its one-request-per-host queue
  data::testing::setTimeoutMs(1000);
  LocalServer srv("hang");
  QElapsedTimer t;
  t.start();
  auto doneAt = std::make_shared<QList<qint64>>();
  for (int i = 0; i < 3; ++i)
    getJson(srv.url(QString("/q%1").arg(i)), {}, [doneAt, &t](const Result<QJsonDocument>& r) {
      CHECK(!r.ok());
      doneAt->append(t.elapsed());
    });
  for (int i = 0; i < 200 && doneAt->size() < 3; ++i) spin(25);
  CHECK_EQ(doneAt->size(), 3);
  if (doneAt->size() == 3) CHECK(doneAt->last() < 1900);   // before: ~1 s, 2 s, 3 s
  data::testing::setTimeoutMs(0);
  data::testing::reset();
}

// ---- Retry-After (2026-11-19: net::Reply carried no headers, a 429's wait was a guess) ----

TEST("Retry-After: seconds and HTTP-date are read, capped, garbage ignored") {
  const QDateTime now = QDateTime(QDate(2026, 11, 19), QTime(7, 28, 0), QTimeZone::UTC);
  CHECK_EQ(net::retryAfterSeconds("120", now), 120);
  CHECK_EQ(net::retryAfterSeconds(" 5 ", now), 5);
  CHECK_EQ(net::retryAfterSeconds("0", now), 0);
  CHECK_EQ(net::retryAfterSeconds("999999", now), net::kRetryAfterCapSec);
  CHECK_EQ(net::retryAfterSeconds("99999999999999999999", now), net::kRetryAfterCapSec);
  CHECK_EQ(net::retryAfterSeconds("Thu, 19 Nov 2026 07:29:30 GMT", now), 90);
  CHECK_EQ(net::retryAfterSeconds("Thu, 19 Nov 2026 07:00:00 GMT", now), 0);   // already past
  CHECK_EQ(net::retryAfterSeconds("", now), -1);
  CHECK_EQ(net::retryAfterSeconds("-3", now), -1);
  CHECK_EQ(net::retryAfterSeconds("soon", now), -1);
}

namespace {
// Answers every request 429 with the given Retry-After; counts the requests that reached it.
struct RateLimitServer {
  QTcpServer srv;
  QByteArray retryAfter;
  int hits = 0;
  explicit RateLimitServer(const QByteArray& ra, const QHostAddress& on = QHostAddress::LocalHost) : retryAfter(ra) {
    srv.listen(on, 0);
    QObject::connect(&srv, &QTcpServer::newConnection, [this] {
      while (QTcpSocket* s = srv.nextPendingConnection()) {
        QObject::connect(s, &QTcpSocket::readyRead, s, [this, s] {
          s->readAll();
          ++hits;
          s->write("HTTP/1.1 429 Too Many Requests\r\nRetry-After: " + retryAfter +
                   "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
          s->flush();
          s->disconnectFromHost();
        });
      }
    });
  }
  QString url(const QString& host, const QString& path) const {
    return QString("http://%1:%2%3").arg(host).arg(srv.serverPort()).arg(path);
  }
};
std::optional<net::Reply> fetchOnce(const QString& url, int maxMs = 5000) {
  net::Request r;
  r.url = QUrl(url);
  r.timeoutMs = 3000;
  r.staleOnError = false;
  std::optional<net::Reply> got;
  net::get(r, nullptr, [&](const net::Reply& rep) { got = rep; });
  for (int i = 0; i < maxMs / 20 && !got; ++i) spin(20);
  return got;
}
}  // namespace

TEST("Retry-After: a short wait holds the host's next request until it has passed") {
  RateLimitServer srv("1");
  const auto a = fetchOnce(srv.url("127.0.0.1", "/a"));
  CHECK(a.has_value());
  if (!a) return;
  CHECK_EQ(a->status, 429);
  CHECK_EQ(a->retryAfterSec, 1);
  QElapsedTimer t;
  t.start();
  const auto b = fetchOnce(srv.url("127.0.0.1", "/b"));
  CHECK(b.has_value());
  CHECK_EQ(srv.hits, 2);           // it was sent, once the wait was over
  CHECK(t.elapsed() >= 800);       // not before (the second's first ms may already be gone)
}

TEST("Retry-After: a long wait answers the host's next requests 429 at once, without sending them") {
  // "localhost", not 127.0.0.1: the hold is per host name and would otherwise last into the other tests
  RateLimitServer srv("30", QHostAddress::Any);
  const auto a = fetchOnce(srv.url("localhost", "/a"));
  CHECK(a.has_value());
  if (!a) return;
  CHECK_EQ(a->retryAfterSec, 30);
  QElapsedTimer t;
  t.start();
  const auto b = fetchOnce(srv.url("localhost", "/b"));
  CHECK(b.has_value());
  if (!b) return;
  CHECK_EQ(b->status, 429);
  CHECK(b->retryAfterSec >= 29 && b->retryAfterSec <= 30);
  CHECK(!b->ok());
  CHECK_EQ(srv.hits, 1);
  CHECK(t.elapsed() < 500);
}

TEST("Retry-After: Charts marks the host down for the server's wait, not the one-minute guess") {
  data::testing::reset();
  net::Reply rep;
  rep.status = 429;
  rep.retryAfterSec = 2;
  noteHostResult("ra.example", rep);
  CHECK(hostDown("ra.example"));
  spin(2200);
  CHECK(!hostDown("ra.example"));
  data::testing::reset();
}

TEST("priority requests go ahead of the queued ones but keep their own order") {
  spin(300);   // the host's pace since an earlier test: "/a" must leave at once
  QTcpServer srv;
  srv.listen(QHostAddress::LocalHost, 0);
  QStringList seen;
  QObject::connect(&srv, &QTcpServer::newConnection, [&] {
    while (QTcpSocket* s = srv.nextPendingConnection())
      QObject::connect(s, &QTcpSocket::readyRead, s, [&seen, s] {
        const QByteArray req = s->readAll();
        seen << QString::fromLatin1(req.split(' ').value(1));
        s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
        s->flush();
        s->disconnectFromHost();
      });
  });
  int done = 0;
  auto send = [&](const QString& path, bool priority) {
    net::Request r;
    r.url = QUrl(QString("http://127.0.0.1:%1%2").arg(srv.serverPort()).arg(path));
    r.priority = priority;
    r.staleOnError = false;
    net::get(r, nullptr, [&done](const net::Reply&) { ++done; });
  };
  send("/a", false);   // leaves at once
  send("/b", false);   // waits for the host's pace
  send("/p1", true);
  send("/p2", true);
  for (int i = 0; i < 200 && done < 4; ++i) spin(20);
  CHECK_EQ(seen.join(' '), QString("/a /p1 /p2 /b"));
}

namespace {
// Records the path of every request it receives, answers 200 "ok".
struct OrderServer {
  QTcpServer srv;
  QStringList seen;
  OrderServer() {
    spin(300);   // the host's pace (250 ms, undeclared host) since an earlier test: the first request leaves at once
    srv.listen(QHostAddress::LocalHost, 0);
    QObject::connect(&srv, &QTcpServer::newConnection, [this] {
      while (QTcpSocket* s = srv.nextPendingConnection())
        QObject::connect(s, &QTcpSocket::readyRead, s, [this, s] {
          seen << QString::fromLatin1(s->readAll().split(' ').value(1));
          s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
          s->flush();
          s->disconnectFromHost();
        });
    });
  }
  void send(const QString& path, QObject* ctx, bool priority, int& done) {
    net::Request r;
    r.url = QUrl(QString("http://127.0.0.1:%1%2").arg(srv.serverPort()).arg(path));
    r.priority = priority;
    r.staleOnError = false;
    net::get(r, ctx, [&done](const net::Reply&) { ++done; });
  }
};
}  // namespace

TEST("background requests wait behind every other request, even ones asked later") {
  OrderServer s;
  int done = 0;
  s.send("/a", nullptr, false, done);   // leaves at once
  { net::BackgroundScope bg("t"); s.send("/bg", nullptr, false, done); }
  s.send("/b", nullptr, false, done);
  s.send("/p", nullptr, true, done);
  for (int i = 0; i < 200 && done < 4; ++i) spin(20);
  CHECK_EQ(s.seen.join(' '), QString("/a /p /b /bg"));
}

TEST("a page asking for what is being preloaded brings it back into line") {
  OrderServer s;
  int done = 0;
  s.send("/a", nullptr, false, done);
  { net::BackgroundScope bg("t"); s.send("/x", nullptr, false, done); s.send("/y", nullptr, false, done); }
  s.send("/b", nullptr, false, done);
  s.send("/y", nullptr, false, done);   // the same URL in the foreground: joins /y, which leaves before /x
  for (int i = 0; i < 200 && done < 5; ++i) spin(20);
  CHECK_EQ(s.seen.join(' '), QString("/a /b /y /x"));
}

TEST("promote() lifts the background requests of one page only") {
  OrderServer s;
  int done = 0;
  s.send("/a", nullptr, false, done);
  { net::BackgroundScope bg("other/page"); s.send("/o1", nullptr, false, done); }
  {
    net::BackgroundScope bg("market/page");
    s.send("/p1", nullptr, false, done);
    { net::BackgroundScope inner("other/page"); s.send("/o2", nullptr, false, done); }   // nested: its own tag, then back
    s.send("/p2", nullptr, false, done);
  }
  s.send("/b", nullptr, false, done);
  CHECK_EQ(net::promote("market/page"), 2);
  for (int i = 0; i < 300 && done < 6; ++i) spin(20);
  CHECK_EQ(s.seen.join(' '), QString("/a /b /p1 /p2 /o1 /o2"));
}
