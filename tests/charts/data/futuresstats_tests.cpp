// Order flow, step E2: one symbol's futures statistics (src/charts/data/FuturesStats) against a fake Binance — no
// network. The fake serves open interest and the accounts ratio every 5 minutes over its last 30 days (refusing an older
// start with HTTP 400, as Binance does), funding every 8 hours, and lets the test push liquidations or fail requests.
#include "Check.h"
#include "charts/data/FuturesStats.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTimer>
#include <QUrlQuery>

using namespace charts::data;

namespace {

constexpr qint64 kFive = 5 * 60 * 1000;
struct FakeBinance {
  qint64 now = 1795029600000;        // the fake clock (ms), on a 5 min boundary
  qint64 keepMs = 30LL * 86400 * 1000;
  QStringList paths;                 // every request
  QList<int> failNext;
  ILiquidationSink* liqSink = nullptr;
  int liqOpens = 0;
  qint64 stepPerRequest = 0;         // the clock moves on by this at every request (a real clock does)

  QByteArray answer(const QString& pathAndQuery, int& status) {
    paths << pathAndQuery;
    now += stepPerRequest;
    status = 200;
    if (!failNext.isEmpty()) { status = failNext.takeFirst(); return "{}"; }
    const QUrl u("https://x" + pathAndQuery);
    const QUrlQuery q(u);
    const int limit = q.queryItemValue("limit").toInt();
    const qint64 start = q.queryItemValue("startTime").toLongLong();
    const qint64 end = q.hasQueryItem("endTime") ? q.queryItemValue("endTime").toLongLong() : 0;
    QJsonArray out;
    if (u.path() == "/fapi/v1/fundingRate") {
      // as measured on Binance: with a start, up to 1000 forward; without, the latest 500
      const qint64 eight = 8 * 3600 * 1000;
      const int n = start > 0 ? std::min(limit, 1000) : std::min(limit, 500);
      qint64 first = start > 0 ? (start + eight - 1) / eight * eight : (now / eight - n + 1) * eight;
      for (qint64 t = first; t <= now && out.size() < n; t += eight)
        out.append(QJsonObject{{"fundingTime", double(t)}, {"fundingRate", QString::number(double(t % 7) * 1e-5)}, {"markPrice", "60000"}});
    } else {
      if (start && start < now - keepMs) { status = 400; return R"({"msg":"parameter 'startTime' is invalid.","code":-1130})"; }
      const bool isOi = u.path() == "/futures/data/openInterestHist";
      // as measured on Binance: start and end → that window; a start alone → the latest `limit` points
      qint64 from = start, to = now;
      if (end) to = std::min(end, now);
      else from = std::max(start, (now / kFive - limit + 1) * kFive);
      // periods without trades have no point (Binance leaves holes): every 97th period is missing here
      for (qint64 t = (from + kFive - 1) / kFive * kFive; t <= to && out.size() < limit; t += kFive) {
        if (t / kFive % 97 == 0) continue;
        if (isOi) out.append(QJsonObject{{"symbol", "X"}, {"sumOpenInterest", QString::number(50000 + double(t / kFive % 1000))}, {"sumOpenInterestValue", "1"}, {"timestamp", double(t)}});
        else out.append(QJsonObject{{"symbol", "X"}, {"longShortRatio", "1.5"}, {"longAccount", "0.6"}, {"timestamp", double(t)}});
      }
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
  }
};
FakeBinance* g_b = nullptr;

class FakeFeed : public IExchangeFeed {
public:
  void openTrades(const FlowMarket&, ITradeSink*) override {}
  void closeTrades() override {}
  void fetchTrades(const FlowMarket&, qint64, int, TradesCb) override {}
  void fetchTickSize(const FlowMarket&, std::function<void(const Result<charts::flow::Decimal>&)>) override {}
  int historyPaceMs(const FlowMarket&) const override { return 0; }
  void fetchFuturesData(const QString& path, std::function<void(const Result<QByteArray>&)> cb) override {
    QTimer::singleShot(1, [path, cb, alive = alive_] {
      if (!*alive) return;
      int st = 0;
      const QByteArray body = g_b->answer(path, st);
      if (st == 200) cb(Result<QByteArray>::success(body));
      else cb(Result<QByteArray>::failure(Error{QString("fake HTTP %1").arg(st), st, true, {}}));
    });
  }
  void openLiquidations(const QString&, ILiquidationSink* sink) override {
    ++g_b->liqOpens;
    sink->onLiquidationState(FeedState::Connecting, {});
    QTimer::singleShot(0, [sink, alive = alive_] {
      if (!*alive) return;
      g_b->liqSink = sink;
      sink->onLiquidationState(FeedState::Open, {});
    });
  }
  void closeLiquidations() override { g_b->liqSink = nullptr; }
  ~FakeFeed() override { *alive_ = false; }
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

bool spin(const std::function<bool()>& pred, int maxMs = 5000) {
  QElapsedTimer t;
  t.start();
  while (!pred()) {
    if (t.elapsed() > maxMs) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return true;
}

struct Setup {
  FakeBinance b;
  Setup() {
    g_b = &b;
    testing::setFeedFactory([](const FlowMarket&) { return std::unique_ptr<IExchangeFeed>(new FakeFeed); });
    FuturesStats::setTimingForTests(40, 1, 10);
    FuturesStats::setClockForTests([] { return g_b->now; });
  }
  ~Setup() {
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    testing::setFeedFactory(nullptr);
    FuturesStats::setTimingForTests(0, 0, 0);
    FuturesStats::setClockForTests(nullptr);
    g_b = nullptr;
  }
};
int countOf(const QStringList& l, const QString& part) { return int(std::count_if(l.begin(), l.end(), [&](const QString& s) { return s.contains(part); })); }
void release(std::shared_ptr<FuturesStats>& s) {
  s.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

}  // namespace

namespace {
// what the fake has in [from, to]: every 5 min point but each 97th (Binance's holes)
int pointsIn(qint64 from, qint64 to) {
  int n = 0;
  for (qint64 t = (from + kFive - 1) / kFive * kFive; t <= to; t += kFive) n += t / kFive % 97 != 0;
  return n;
}
}  // namespace

TEST("futuresstats: first run fills the last 29.5 days in windows of 500 periods, then stores them") {
  Setup su;
  auto s = FuturesStats::acquire("AAAUSDT");
  QDir(s->dir()).removeRecursively();
  release(s);
  s = FuturesStats::acquire("AAAUSDT");
  CHECK(spin([&] { return s->status().caughtUp; }));
  // 29.5 days of 5 min points up to now, holes included (a short page is not the end)
  const qint64 start = su.b.now - qint64(29.5 * 86400 * 1000);
  const int want = pointsIn(start, su.b.now);
  CHECK_EQ(int(s->openInterest().size()), want);
  CHECK_EQ(int(s->ratios().size()), want);
  CHECK_EQ(s->openInterest().back().timeMs, su.b.now);
  const qint64 window = 500 * kFive;
  CHECK_EQ(countOf(su.b.paths, "openInterestHist"), int((su.b.now - start + window) / window));   // one request per window
  CHECK(su.b.paths.filter("openInterestHist").first().contains("&endTime="));                   // never a start alone
  // funding: a year back, 1000 a page forward (3 a day: 1095 → two pages)
  CHECK(s->funding().size() >= 1094 && s->funding().size() <= 1096);
  CHECK_EQ(countOf(su.b.paths, "fundingRate"), 2);
  CHECK(QFile::exists(s->dir() + "/oi5m.dat"));
  CHECK_EQ(int(charts::flow::loadRecords<charts::flow::OiPoint>(s->dir() + "/oi5m.dat", 1).size()), want);
  CHECK(s->status().live == FeedState::Open);
  release(s);
}

TEST("futuresstats: the next run starts from the disk and asks only what is new; polling adds the next points") {
  Setup su;
  auto s = FuturesStats::acquire("BBBUSDT");
  QDir(s->dir()).removeRecursively();
  release(s);
  s = FuturesStats::acquire("BBBUSDT");
  CHECK(spin([&] { return s->status().caughtUp; }));
  const size_t n0 = s->openInterest().size();
  const qint64 lastStored = s->openInterest().back().timeMs;
  release(s);
  su.b.now += 3 * kFive;   // Deepwater closed for 15 minutes
  su.b.paths.clear();
  s = FuturesStats::acquire("BBBUSDT");
  CHECK_EQ(s->openInterest().size(), n0);                     // straight from the file
  const size_t gained = size_t(pointsIn(lastStored + 1, su.b.now));
  CHECK(gained >= 2);
  CHECK(spin([&] { return s->openInterest().size() == n0 + gained; }));
  CHECK_EQ(countOf(su.b.paths, "openInterestHist"), 1);       // one window from after the last stored point
  const QString q = su.b.paths.filter("openInterestHist").first();
  CHECK(q.contains("startTime=" + QString::number(lastStored + 1)));
  // while it runs: the poll asks again and the new points arrive
  su.b.now += 2 * kFive;
  const size_t more = size_t(pointsIn(su.b.now - 2 * kFive + 1, su.b.now));
  CHECK(spin([&] { return s->openInterest().size() == n0 + gained + more; }));
  CHECK_EQ(int(charts::flow::loadRecords<charts::flow::OiPoint>(s->dir() + "/oi5m.dat", 1).size()), int(n0 + gained + more));
  release(s);
}

TEST("futuresstats: Binance refusing the start (400) → 29 days back; errors wait and go on from the same place") {
  Setup su;
  su.b.keepMs = qint64(29.2 * 86400 * 1000);   // stricter than our 29.5 days
  auto s = FuturesStats::acquire("CCCUSDT");
  QDir(s->dir()).removeRecursively();
  release(s);
  su.b.failNext = {503, 503};
  s = FuturesStats::acquire("CCCUSDT");
  CHECK(spin([&] { return s->status().caughtUp; }));
  CHECK(s->status().errors >= 3);                  // two 503 and the 400
  CHECK(s->status().error.isEmpty());              // cleared by the next success
  CHECK(!s->openInterest().empty());
  CHECK(s->openInterest().front().timeMs >= su.b.now - 29LL * 86400 * 1000);
  CHECK_EQ(s->openInterest().back().timeMs, su.b.now);
  release(s);
}

TEST("futuresstats: live liquidations are kept, written to disk and read back") {
  Setup su;
  auto s = FuturesStats::acquire("DDDUSDT");
  QDir(s->dir()).removeRecursively();
  release(s);
  s = FuturesStats::acquire("DDDUSDT");
  CHECK(spin([&] { return su.b.liqSink != nullptr; }));
  int changes = 0;
  QObject::connect(s.get(), &FuturesStats::changed, [&] { ++changes; });
  // times near the fake clock: events older than a year are dropped at the flush (audit G3)
  su.b.liqSink->onLiquidation({su.b.now - 2000, 60000, 0.5, 1, 0});
  su.b.liqSink->onLiquidation({su.b.now - 1000, 61000, 1.5, 0, 0});
  CHECK_EQ(int(s->liquidations().size()), 2);
  CHECK(changes >= 2);
  // the socket drops: reopened after the backoff
  const int opens = su.b.liqOpens;
  su.b.liqSink->onLiquidationState(FeedState::Closed, "test drop");
  CHECK(spin([&] { return su.b.liqOpens > opens && s->status().live == FeedState::Open; }));
  release(s);   // pending events are written
  const auto onDisk = charts::flow::loadRecords<charts::flow::LiqEvent>(FuturesStats::acquire("DDDUSDT")->dir() + "/liquidations.dat", 4);
  CHECK_EQ(int(onDisk.size()), 2);
  CHECK_EQ(onDisk[1].price, 61000.0);
  CHECK_EQ(onDisk[1].longLiquidated, 0);
  auto again = FuturesStats::acquire("DDDUSDT");
  CHECK_EQ(int(again->liquidations().size()), 2);
  release(again);
}

TEST("futuresstats: one object per symbol however many users") {
  Setup su;
  auto a = FuturesStats::acquire("EEEUSDT");
  auto b = FuturesStats::acquire("eeeusdt");
  CHECK(a.get() == b.get());
  CHECK(a.get() != FuturesStats::acquire("FFFUSDT").get());
  release(a);
  release(b);
}

// Audit G3: a symbol Binance always refuses (HTTP 400 for every start) looped back to back: the correction moved the
// start to now − 29 days, the clock moved on, the start was "too old" again, and so on until Binance's 429 / 418.
TEST("futuresstats: a start always refused (400) is moved once, then the requests back off") {
  Setup su;
  su.b.keepMs = 1;                 // every start is refused
  su.b.stepPerRequest = 1000;      // the clock advances between the requests, as in real life
  auto s = FuturesStats::acquire("GGGUSDT");
  QDir(s->dir()).removeRecursively();
  release(s);
  su.b.paths.clear();
  s = FuturesStats::acquire("GGGUSDT");
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < 300) QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  // the first try, the one correction, then retries at 10, 20, 40, 80, 160 ms (backoff base 10 ms in the tests)
  const int n = countOf(su.b.paths, "openInterestHist");
  CHECK(n >= 3);
  CHECK(n <= 9);
  CHECK(s->status().errors >= 3);
  release(s);
}

TEST("futuresstats: 418 (banned) waits minutes, not the usual backoff") {
  Setup su;
  auto s = FuturesStats::acquire("HHHUSDT");
  QDir(s->dir()).removeRecursively();
  release(s);
  su.b.paths.clear();
  su.b.failNext = {418};
  s = FuturesStats::acquire("HHHUSDT");
  CHECK(spin([&] { return s->status().errors >= 1; }));
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < 400) QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  // 418 waits 600 × the backoff base (6 s in the tests, 10 min in the app): nothing more was asked
  CHECK_EQ(countOf(su.b.paths, "openInterestHist"), 1);
  CHECK(!s->status().caughtUp);
  release(s);
}

TEST("futuresstats: liquidations older than a year are dropped from memory and from the file") {
  Setup su;
  auto s = FuturesStats::acquire("IIIUSDT");
  const QString dir = s->dir();
  QDir(dir).removeRecursively();
  release(s);
  const qint64 day = 86400LL * 1000;
  const std::vector<charts::flow::LiqEvent> old = {{su.b.now - 400 * day, 50000, 1, 1, 0}, {su.b.now - 367 * day, 51000, 1, 0, 0},
                                                    {su.b.now - 10 * day, 60000, 2, 1, 0}};
  CHECK(charts::flow::appendRecords<charts::flow::LiqEvent>(dir + "/liquidations.dat", std::span<const charts::flow::LiqEvent>(old), 4));
  s = FuturesStats::acquire("IIIUSDT");
  CHECK_EQ(int(s->liquidations().size()), 1);
  CHECK_EQ(s->liquidations().front().price, 60000.0);
  release(s);   // the flush at release rewrites the file
  const auto onDisk = charts::flow::loadRecords<charts::flow::LiqEvent>(dir + "/liquidations.dat", 4);
  CHECK_EQ(int(onDisk.size()), 1);
  // within the year (plus a day of slack): nothing dropped, the file is not rewritten
  const std::vector<charts::flow::LiqEvent> recent = {{su.b.now - 365 * day + 3600000, 1, 1, 1, 0}};
  QFile::remove(dir + "/liquidations.dat");
  CHECK(charts::flow::appendRecords<charts::flow::LiqEvent>(dir + "/liquidations.dat", std::span<const charts::flow::LiqEvent>(recent), 4));
  s = FuturesStats::acquire("IIIUSDT");
  CHECK_EQ(int(s->liquidations().size()), 1);
  release(s);
}

TEST("futuresstats: a second Deepwater on the same folder keeps its points in memory, then takes the files over") {
  Setup su;
  auto s = FuturesStats::acquire("LCKUSDT");
  const QString dir = s->dir();
  release(s);
  QDir(dir).removeRecursively();
  QDir().mkpath(dir);
  // the other Deepwater: holds the folder's lock (a QLockFile of this process counts as held for another one)
  auto other = std::make_unique<QLockFile>(dir + "/.lock");
  other->setStaleLockTime(0);
  CHECK(other->tryLock(0));
  s = FuturesStats::acquire("LCKUSDT");
  CHECK(spin([&] { return s->status().caughtUp; }));
  const size_t n = s->openInterest().size();
  CHECK(n > 1000);
  if (g_b->liqSink) g_b->liqSink->onLiquidation(charts::flow::LiqEvent{su.b.now - 1000, 60000, 0.5, 1, 0});
  spin([] { return false; }, 150);   // a few flushes go by (40 ms each)
  CHECK(!QFile::exists(dir + "/oi5m.dat"));            // nothing written while the other holds the folder
  CHECK(!QFile::exists(dir + "/liquidations.dat"));
  // what the other one wrote meanwhile: an older point we never fetched, and one liquidation
  const qint64 old = s->openInterest().front().timeMs - 10 * kFive;
  const std::vector<charts::flow::OiPoint> theirs{charts::flow::OiPoint{old, 1, 1}};
  CHECK(charts::flow::appendRecords<charts::flow::OiPoint>(dir + "/oi5m.dat", std::span<const charts::flow::OiPoint>(theirs), 1));
  const std::vector<charts::flow::LiqEvent> theirLiq{charts::flow::LiqEvent{su.b.now - 1000, 60000, 0.5, 1, 0},
                                                     charts::flow::LiqEvent{su.b.now - 2000, 61000, 0.2, 0, 0}};
  CHECK(charts::flow::appendRecords<charts::flow::LiqEvent>(dir + "/liquidations.dat", std::span<const charts::flow::LiqEvent>(theirLiq), 4));
  other.reset();   // it quits
  CHECK(spin([&] { return QFile::exists(dir + "/oi5m.dat") && s->openInterest().size() == n + 1; }));
  const auto disk = charts::flow::loadRecords<charts::flow::OiPoint>(dir + "/oi5m.dat", 1);
  CHECK_EQ(disk.size(), n + 1);                                    // theirs and ours, each once
  if (!disk.empty()) CHECK_EQ(disk.front().timeMs, old);
  CHECK(spin([&] { return charts::flow::loadRecords<charts::flow::LiqEvent>(dir + "/liquidations.dat", 4).size() == 2; }));
  CHECK_EQ(s->liquidations().size(), size_t(2));                   // the shared one deduplicated
  release(s);
}
