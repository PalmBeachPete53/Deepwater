// Footprint history from Binance's archives (src/technicaldata/FootprintArchive): the basis-point grid, both CSV
// formats, the zip inflated as it is read, the minutes of a day against values computed apart in Python
// (fixtures/make_footprint.py), the stored form and the merge into longer bars.
#include "Check.h"
#include "technicaldata/FootprintArchive.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QTimeZone>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

using namespace technicaldata::fp;

namespace {
QByteArray fixture(const QString& name) {
  QFile f(QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("fixtures/" + name));
  if (!f.open(QIODevice::ReadOnly)) return {};
  return f.readAll();
}
const QDate kDay(2026, 11, 18);
bool rel(double a, double b, double tol) { return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b)); }
}  // namespace

TEST("footprint archive: the basis-point grid is geometric, 1 bp wide, the same everywhere") {
  for (double p : {3200.5, 19000.0, 64947.36, 124659.0}) {
    const int32_t k = levelOf(p);
    CHECK(levelLow(k) <= p * (1 + 1e-12));
    CHECK(levelLow(k + 1) > p * (1 - 1e-12));
    CHECK_NEAR(levelLow(k + 1) / levelLow(k), 1.0001, 1e-12);   // one basis point at any price
    CHECK(levelMid(k) > levelLow(k) && levelMid(k) < levelLow(k + 1));
  }
  CHECK(levelOf(64947.36) < levelOf(64957.77));
  CHECK_NEAR(levelLow(levelOf(64947.36) + 1) - levelLow(levelOf(64947.36)), 6.49, 0.01);   // ≈ $6.5 at 65 000
}

TEST("footprint archive: both CSV formats, header skipped, microseconds, a line split anywhere") {
  const QByteArray spot = "4121859520,64947.36000000,0.00518000,6875073437,6875073437,1794960000024505,False,True\n"
                          "4121859521,64947.35000000,0.05793000,6875073438,6875073442,1794960000024603,True,True\n";
  const QByteArray perp = "agg_trade_id,price,quantity,first_trade_id,last_trade_id,transact_time,is_buyer_maker\r\n"
                          "3530319136,64921.0,3.584,8291300673,8291300720,1794960000005,true\r\n"
                          "3530319137,64920.9,0.001,8291300721,8291300721,1794960000010,false";
  for (int cut = 0; cut <= spot.size(); ++cut) {
    std::vector<AggTrade> got;
    CsvReader r([&](const AggTrade& t) { got.push_back(t); });
    r.feed(spot.constData(), size_t(cut));
    r.feed(spot.constData() + cut, size_t(spot.size() - cut));
    r.finish();
    CHECK(got.size() == 2);
    CHECK(r.bad() == 0);
    if (got.size() != 2) break;
    CHECK(got[0].id == 4121859520LL);
    CHECK_NEAR(got[0].price, 64947.36, 1e-9);
    CHECK_NEAR(got[0].qty, 0.00518, 1e-12);
    CHECK(got[0].timeMs == 1794960000024LL);   // microseconds → milliseconds
    CHECK(!got[0].buyerMaker);
    CHECK(got[1].buyerMaker);
  }
  std::vector<AggTrade> got;
  CsvReader r([&](const AggTrade& t) { got.push_back(t); });
  r.feed(perp.constData(), size_t(perp.size()));
  r.finish();   // no newline at the end: the last line still counts
  CHECK(got.size() == 2);
  CHECK(r.bad() == 0);
  CHECK(got.size() == 2 && got[0].timeMs == 1794960000005LL && got[0].buyerMaker && !got[1].buyerMaker);
  CHECK(got.size() == 2 && std::fabs(got[0].qty - 3.584) < 1e-12);
  // a damaged line is counted and skipped
  std::vector<AggTrade> few;
  CsvReader b([&](const AggTrade& t) { few.push_back(t); });
  const QByteArray broken = "12,64920.9,0.001,1,1,1794960000010,maybe\n13,64920.9,x,1,1,1794960000010,true\n14,1,1,1,1,1794960000010,true\n";
  b.feed(broken.constData(), size_t(broken.size()));
  CHECK(few.size() == 1);
  CHECK(b.bad() == 2);
}

TEST("footprint archive: a zip inflated as it is read, CRC checked, with or without a data descriptor") {
  for (const QString& name : {QString("spot-2026-11-18-head.zip"), QString("spot-2026-11-18-head-descriptor.zip"),
                              QString("perp-2026-11-18-head.zip"), QString("perp-2026-11-18-head-descriptor.zip")}) {
    const QByteArray zip = fixture(name);
    CHECK(!zip.isEmpty());
    QByteArray all;
    QString err;
    CHECK(inflateZip(zip, [&](const char* p, size_t n) { all.append(p, qsizetype(n)); }, &err));
    CHECK(err.isEmpty());
    CHECK(all.count('\n') >= 300);
    // one byte changed in the compressed data: refused (inflate error or CRC), never silently wrong
    QByteArray bad = zip;
    bad[bad.size() / 2] = char(bad[bad.size() / 2] ^ 0x5a);
    QString why;
    CHECK(!inflateZip(bad, [](const char*, size_t) {}, &why));
    CHECK(!why.isEmpty());
    // cut short
    CHECK(!inflateZip(QByteArrayView(zip).first(zip.size() / 3), [](const char*, size_t) {}, &why));
  }
  QString why;
  CHECK(!inflateZip(QByteArray("not a zip at all, longer than thirty bytes."), [](const char*, size_t) {}, &why));
}

TEST("footprint archive: the minutes of a day match the values computed apart (Python), both markets") {
  const QJsonObject expected = QJsonDocument::fromJson(fixture("footprint-expected.json")).object();
  CHECK(!expected.isEmpty());
  for (const QString venue : {QString("spot"), QString("perp")}) {
    const DayResult r = buildDay(fixture(venue + "-2026-11-18-head.zip"), kDay);
    CHECK(r.error.isEmpty());
    CHECK(r.trades == 300);
    CHECK(r.bad == 0);
    CHECK(r.outside == 0);
    CHECK(r.idGaps == 299 * 19);   // one trade in twenty kept
    const QJsonArray want = expected.value(venue).toArray();
    CHECK(int(r.minutes.size()) == want.size());
    if (int(r.minutes.size()) != want.size()) continue;
    for (int i = 0; i < want.size(); ++i) {
      const Minute& m = r.minutes[size_t(i)];
      const QJsonObject w = want[i].toObject();
      CHECK(m.t == qint64(w.value("t").toDouble()));
      CHECK(m.open == w.value("open").toDouble());
      CHECK(m.high == w.value("high").toDouble());
      CHECK(m.low == w.value("low").toDouble());
      CHECK(m.close == w.value("close").toDouble());
      CHECK(rel(m.buy, w.value("buy").toDouble(), 1e-12));
      CHECK(rel(m.sell, w.value("sell").toDouble(), 1e-12));
      CHECK(m.trades == quint32(w.value("trades").toInt()));
      CHECK(m.firstId == qint64(w.value("first").toDouble()));
      CHECK(m.lastId == qint64(w.value("last").toDouble()));
      // levels: the traded ones as Python found them, the others inside 0 × 0
      const QJsonArray lv = w.value("levels").toArray();
      CHECK(m.base == lv.first().toArray()[0].toInt());
      CHECK(m.base + int(m.levels.size()) - 1 == lv.last().toArray()[0].toInt());
      double sb = 0, sa = 0;
      for (const auto& x : lv) {
        const QJsonArray a = x.toArray();
        const Level& l = m.levels[size_t(a[0].toInt() - m.base)];
        CHECK(rel(l.bid, a[1].toDouble(), 1e-6));
        CHECK(rel(l.ask, a[2].toDouble(), 1e-6));
      }
      for (const Level& l : m.levels) { sb += l.bid; sa += l.ask; }
      CHECK(rel(sb, m.sell, 1e-6));   // the levels add up to the minute's totals
      CHECK(rel(sa, m.buy, 1e-6));
    }
  }
  // the zip with a data descriptor gives the same minutes
  const DayResult a = buildDay(fixture("perp-2026-11-18-head.zip"), kDay), b = buildDay(fixture("perp-2026-11-18-head-descriptor.zip"), kDay);
  CHECK(a.minutes.size() == b.minutes.size());
  CHECK(!a.minutes.empty() && encodeDay(a.minutes) == encodeDay(b.minutes));
  // trades of another day are not counted in this one
  const DayResult other = buildDay(fixture("spot-2026-11-18-head.zip"), kDay.addDays(1));
  CHECK(other.outside == 300);
  CHECK(!other.error.isEmpty());
}

TEST("footprint archive: stored and read back unchanged, compact") {
  const DayResult r = buildDay(fixture("spot-2026-11-18-head.zip"), kDay);
  const QByteArray blob = encodeDay(r.minutes);
  std::vector<Minute> back;
  const qint64 dayStart = QDateTime(kDay, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  CHECK(decodeDay(blob, dayStart, back));
  CHECK(back.size() == r.minutes.size());
  for (size_t i = 0; i < std::min(back.size(), r.minutes.size()); ++i) {
    const Minute &x = back[i], &y = r.minutes[i];
    CHECK(x.t == y.t && x.open == y.open && x.high == y.high && x.low == y.low && x.close == y.close);
    CHECK(x.buy == y.buy && x.sell == y.sell && x.trades == y.trades && x.firstId == y.firstId && x.lastId == y.lastId);
    CHECK(x.base == y.base && x.levels.size() == y.levels.size());
    for (size_t k = 0; k < std::min(x.levels.size(), y.levels.size()); ++k)
      CHECK(x.levels[k].bid == y.levels[k].bid && x.levels[k].ask == y.levels[k].ask);
  }
  CHECK(!decodeDay(QByteArray("garbage"), dayStart, back));
  CHECK(back.empty());
  QByteArray cut = qUncompress(blob);
  cut.chop(3);
  CHECK(!decodeDay(qCompress(cut), dayStart, back));
}

TEST("footprint archive: minutes merge into a longer bar level by level; analyze reads the levels") {
  const DayResult r = buildDay(fixture("perp-2026-11-18-head.zip"), kDay);
  CHECK(r.minutes.size() >= 3);
  if (r.minutes.size() < 3) return;
  Minute five;
  for (const Minute& m : r.minutes) merge(five, m);
  double buy = 0, sell = 0;
  quint32 trades = 0;
  double hi = 0, lo = 1e18;
  for (const Minute& m : r.minutes) { buy += m.buy; sell += m.sell; trades += m.trades; hi = std::max(hi, m.high); lo = std::min(lo, m.low); }
  CHECK(rel(five.buy, buy, 1e-12) && rel(five.sell, sell, 1e-12) && five.trades == trades);
  CHECK(five.open == r.minutes.front().open && five.close == r.minutes.back().close && five.high == hi && five.low == lo);
  CHECK(five.t == r.minutes.front().t && five.firstId == r.minutes.front().firstId && five.lastId == r.minutes.back().lastId);
  double sb = 0, sa = 0;
  for (const Level& l : five.levels) { sb += l.bid; sa += l.ask; }
  CHECK(rel(sb, sell, 1e-5) && rel(sa, buy, 1e-5));
  // merged in the other order: the same bar
  Minute back;
  for (auto it = r.minutes.rbegin(); it != r.minutes.rend(); ++it) merge(back, *it);
  CHECK(back.t == five.t && back.open == five.open && back.close == five.close && back.base == five.base && back.levels.size() == five.levels.size());
  // the footprint analysis on the basis-point levels: POC = the level with the most volume
  charts::flow::FootprintView v;
  charts::flow::analyze(levelCandle(five), charts::flow::FootprintParams{}, v);
  CHECK(v.poc >= 0);
  if (v.poc >= 0) {
    size_t best = 0;
    for (size_t k = 0; k < five.levels.size(); ++k)
      if (five.levels[k].bid + five.levels[k].ask > five.levels[best].bid + five.levels[best].ask) best = k;
    CHECK(v.rows[size_t(v.poc)].ticks == five.base + int64_t(best));
  }
  CHECK(rel(v.delta, five.buy - five.sell, 1e-9));
  // on the market's tick grid (USD-M BTCUSDT: 0.1): each level at the tick nearest its middle, totals unchanged
  const charts::flow::TickScale ticks(charts::flow::Decimal{1, -1, true});
  const charts::flow::FootprintCandle c = tickCandle(five, ticks);
  double cb = 0, ca = 0;
  for (const auto& cell : c.cells) { cb += cell.bid; ca += cell.ask; }
  CHECK(rel(cb, sb, 1e-12) && rel(ca, sa, 1e-12));
  CHECK(c.high == ticks.ticksOf(five.high) && c.low == ticks.ticksOf(five.low));
  for (size_t k = 1; k < c.cells.size(); ++k) CHECK(c.cells[k].ticks > c.cells[k - 1].ticks);
  // every cell inside the bar's range, every minute (analyze() indexes its rows by it), at any grouping
  const charts::flow::TickScale cent(charts::flow::Decimal{1, -2, true});
  for (const Minute& m : r.minutes)
    for (const auto* sc : {&ticks, &cent}) {
      const charts::flow::FootprintCandle tc = tickCandle(m, *sc);
      for (const auto& cell : tc.cells) CHECK(cell.ticks >= tc.low && cell.ticks <= tc.high);
      for (int64_t g : {1, 7, 100, 1000}) {
        charts::flow::FootprintParams p;
        p.groupTicks = g;
        charts::flow::FootprintView v;
        charts::flow::analyze(tc, p, v);
        CHECK(!v.rows.empty());
      }
    }
}

TEST("footprint archive: checksum files and archive addresses") {
  CHECK(checksumOf("217bf4e4e3b228bffa16b8b0cf161a23a550838c5e68947754a904e27b281cdb  BTCUSDT-aggTrades-2026-11-18.zip\n") ==
        "217bf4e4e3b228bffa16b8b0cf161a23a550838c5e68947754a904e27b281cdb");
  CHECK(checksumOf("<html>Not found</html>").isEmpty());
  CHECK(dailyUrl(Venue::Spot, "BTCUSDT", kDay).toString() ==
        "https://data.binance.vision/data/spot/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2026-11-18.zip");
  CHECK(checksumUrl(Venue::Perp, "BTCUSDT", kDay).toString() ==
        "https://data.binance.vision/data/futures/um/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2026-11-18.zip.CHECKSUM");
  CHECK(venueFromId("perp") == Venue::Perp && !venueFromId("swap"));
}

// A measure on whole days, not run by default: DW_FP_DAYS=<folder> holding spot_BTCUSDT-aggTrades-<day>.zip and
// futures_BTCUSDT-aggTrades-<day>.zip (data.binance.vision), DW_FP_DAY=<yyyy-MM-dd>.
TEST("footprint archive: whole days (measure, with DW_FP_DAYS)") {
  const QString dir = qEnvironmentVariable("DW_FP_DAYS");
  if (dir.isEmpty()) return;
  const QDate day = QDate::fromString(qEnvironmentVariable("DW_FP_DAY", "2026-11-18"), "yyyy-MM-dd");
  for (const QString& file : {QString("spot_BTCUSDT-aggTrades-%1.zip"), QString("futures_BTCUSDT-aggTrades-%1.zip")}) {
    QFile f(dir + "/" + file.arg(day.toString("yyyy-MM-dd")));
    CHECK(f.open(QIODevice::ReadOnly));
    const QByteArray zip = f.readAll();
    QElapsedTimer t;
    t.start();
    const DayResult r = buildDay(zip, day);
    const qint64 buildMs = t.elapsed();
    t.restart();
    const QByteArray blob = encodeDay(r.minutes);
    const qint64 encMs = t.elapsed();
    t.restart();
    std::vector<Minute> back;
    CHECK(decodeDay(blob, QDateTime(day, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(), back));
    const qint64 decMs = t.elapsed();
    size_t levels = 0, maxLevels = 0;
    for (const Minute& m : r.minutes) { levels += m.levels.size(); maxLevels = std::max(maxLevels, m.levels.size()); }
    CHECK(r.error.isEmpty());
    qInfo("%s: zip %.1f MB, %lld trades, %zu minutes, %zu levels (avg %.1f, max %zu), bad %lld, outside %lld, id gaps %lld; "
          "built in %lld ms, stored %.1f KB (encode %lld ms, decode %lld ms)",
          qPrintable(file.arg(day.toString("yyyy-MM-dd"))), zip.size() / 1e6, r.trades, r.minutes.size(), levels,
          r.minutes.empty() ? 0.0 : double(levels) / double(r.minutes.size()), maxLevels, r.bad, r.outside, r.idGaps, buildMs,
          blob.size() / 1e3, encMs, decMs);
  }
}

// ---- the collector, with a fake data.binance.vision serving the fixtures (nothing leaves the machine)
#include "technicaldata/ArchiveCollector.h"
#include "technicaldata/PerpArchive.h"
#include "technicaldata/TechnicalDataSpec.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

namespace {
using namespace technicaldata;
qint64 utcDay(int y, int m, int d) { return QDateTime(QDate(y, m, d), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(); }
struct FakeArchive {
  QHash<QString, QByteArray> files;   // path after /data/ → body
  QSet<QString> badSum;               // their checksum file gives another sha
  QStringList asked;
  void put(const QString& path, const QByteArray& zip) { files.insert(path, zip); }
  void serve(const QUrl& url, Collector::Reply reply) {
    const QString rel = url.path().section("/data/", 1);
    asked << rel;
    QByteArray body;
    if (rel.endsWith(".CHECKSUM")) {
      const QString zip = rel.chopped(9);
      if (files.contains(zip))
        body = (badSum.contains(zip) ? QByteArray(64, 'a') : QCryptographicHash::hash(files.value(zip), QCryptographicHash::Sha256).toHex()) +
               "  " + zip.section('/', -1).toUtf8() + "\n";
    } else {
      body = files.value(rel);
    }
    QTimer::singleShot(0, [reply, body] { body.isEmpty() ? reply(404, "<Error>NoSuchKey</Error>", "HTTP 404") : reply(200, body, {}); });
  }
};
struct FpRig {
  QTemporaryDir dir;
  std::unique_ptr<Store> store;
  std::unique_ptr<ArchiveCollector> col;
  std::shared_ptr<FakeArchive> fake = std::make_shared<FakeArchive>();
  const QString m = "BINANCE:BTCUSDT";
  explicit FpRig(const PairSpec& spec) {
    store = std::make_unique<Store>(dir.filePath("technical-data.sqlite"));
    store->setPairSpec(m, specToJson(spec));
    store->wait();
    QCoreApplication::processEvents();
    col = std::make_unique<ArchiveCollector>(store.get());
    col->setMarkets({m});
    auto f = fake;
    col->setFetch([f](const QUrl& u, QObject*, Collector::Reply r) { f->serve(u, std::move(r)); });
    col->setClock([] { return (utcDay(2026, 11, 19) + 10 * 3600) * 1000; });   // yesterday = 2026-11-18
  }
  bool runUntilIdle(int ms = 20000) {
    QEventLoop loop;
    bool idle = false;
    auto c = QObject::connect(col.get(), &ArchiveCollector::idle, &loop, [&] { idle = true; loop.quit(); });
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(c);
    store->wait();
    QCoreApplication::processEvents();
    store->wait();
    return idle;
  }
};
PairSpec fpSpec(QStringList venues, qint64 start) {
  PairSpec s;
  s.timeframes = {86400};
  s.footprint = venues;
  s.footprintStart = start;
  return s;
}
const char* const kSpot18 = "spot/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2026-11-18.zip";
const char* const kPerp18 = "futures/um/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2026-11-18.zip";
const char* const kPerp17 = "futures/um/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2026-11-17.zip";
}  // namespace

TEST("footprint collector: newest day first, checksum checked, missing days recorded, recent ones asked again later") {
  FpRig r(fpSpec({"spot", "perp"}, utcDay(2026, 11, 14)));
  r.fake->put(kSpot18, fixture("spot-2026-11-18-head.zip"));
  r.fake->put(kPerp18, fixture("perp-2026-11-18-head.zip"));
  r.fake->put(kPerp17, fixture("perp-2026-11-18-head.zip"));
  r.fake->badSum.insert(kPerp17);                                     // a damaged download: refused, not stored
  r.col->start();
  CHECK(r.runUntilIdle());
  CHECK(!r.fake->asked.isEmpty() && r.fake->asked.first().contains("2026-11-18"));   // the newest day first
  const auto spot = r.store->footprintDays(r.m, "spot"), perp = r.store->footprintDays(r.m, "perp");
  CHECK(spot.size() == 1 && perp.size() == 1);
  if (spot.size() == 1 && perp.size() == 1) {
    CHECK(spot[0].day == utcDay(2026, 11, 18) && spot[0].minutes == 13 && spot[0].trades == 300);
    CHECK(perp[0].minutes == 9 && perp[0].zipBytes == fixture("perp-2026-11-18-head.zip").size());
    // the stored day reads back as the archive builds it
    std::vector<fp::Minute> back;
    CHECK(fp::decodeDay(r.store->footprintData(r.m, "spot", spot[0].day), spot[0].day, back));
    CHECK(fp::encodeDay(back) == fp::encodeDay(fp::buildDay(fixture("spot-2026-11-18-head.zip"), QDate(2026, 11, 18)).minutes));
  }
  // older than three days and absent: missing for good (11-14, 11-15); 11-16 and 11-17 may still come out
  const auto missing = r.store->footprintMissing(r.m, "spot");
  CHECK(missing.contains(utcDay(2026, 11, 14)) && missing.contains(utcDay(2026, 11, 15)));
  CHECK(!missing.contains(utcDay(2026, 11, 16)) && !missing.contains(utcDay(2026, 11, 17)));
  CHECK(!r.store->footprintMissing(r.m, "perp").contains(utcDay(2026, 11, 17)));   // a bad checksum is no missing day
  bool errorShown = false;
  for (const auto& p : r.col->progress())
    if (p.kind == "perp") { errorShown = p.error.contains("checksum"); CHECK(p.stored == 1 && p.wanted == 5 && p.missing == 2); }
  CHECK(errorShown);
  // a second turn asks nothing it already has, nor what it was told to ask later
  const int before = int(r.fake->asked.size());
  QTimer::singleShot(0, r.col.get(), [&r] { r.col->syncNow(); });
  CHECK(r.runUntilIdle());
  CHECK(int(r.fake->asked.size()) == before);
}

TEST("footprint collector: a market removed or a later first day deletes its days; a paused pair writes nothing") {
  FpRig r(fpSpec({"spot", "perp"}, utcDay(2026, 11, 18)));
  r.fake->put(kSpot18, fixture("spot-2026-11-18-head.zip"));
  r.fake->put(kPerp18, fixture("perp-2026-11-18-head.zip"));
  r.store->setPaused(r.m, true);
  r.col->start();
  r.runUntilIdle(3000);
  CHECK(r.fake->asked.isEmpty());
  CHECK(r.store->footprintDays(r.m, "spot").isEmpty());
  r.store->setPaused(r.m, false);
  CHECK(r.runUntilIdle());
  CHECK(r.store->footprintDays(r.m, "spot").size() == 1 && r.store->footprintDays(r.m, "perp").size() == 1);
  // perp no longer recorded: its day goes, spot stays
  r.store->setPairSpec(r.m, specToJson(fpSpec({"spot"}, utcDay(2026, 11, 18))));
  r.runUntilIdle(3000);
  CHECK(r.store->footprintDays(r.m, "perp").isEmpty());
  CHECK(r.store->footprintDays(r.m, "spot").size() == 1);
  // a later first day: the days before go
  r.store->setPairSpec(r.m, specToJson(fpSpec({"spot"}, utcDay(2026, 11, 19))));
  r.runUntilIdle(3000);
  CHECK(r.store->footprintDays(r.m, "spot").isEmpty());
  // spec round trip
  const PairSpec s = specFromJson(specToJson(fpSpec({"perp", "spot"}, 1786060800)));
  CHECK(s.footprint == QStringList({"spot", "perp"}) && s.footprintStart == 1786060800);
  CHECK(specFromJson(specToJson(fpSpec({}, 0))).footprint.isEmpty());
  CHECK(!specToJson(fpSpec({}, 0)).contains("footprint"));
}

TEST("footprint archive: a minute's values — POC, value area, imbalances") {
  Minute m;
  m.t = utcDay(2026, 11, 18);
  m.base = levelOf(65000);
  //            level:   0         1          2          3         4
  m.levels = {{1, 0}, {2, 0.5f}, {1, 9}, {0, 4}, {0.5f, 0.5f}};   // bid, ask
  m.sell = 4.5; m.buy = 14; m.trades = 12;
  m.low = m.open = levelMid(m.base);
  m.high = m.close = levelMid(m.base + 4);
  const Stats s = statsOf(m);
  CHECK(s.levels == 5);
  CHECK_NEAR(s.poc, levelMid(m.base + 2), 1e-9);    // 10 at level 2
  CHECK_NEAR(s.pocVolume, 10, 1e-9);
  // 70 % of 18.5 = 12.95: POC 10, then the larger neighbour (level 3: 4) → 14
  CHECK_NEAR(s.vaLow, levelLow(m.base + 2), 1e-9);
  CHECK_NEAR(s.vaHigh, levelLow(m.base + 4), 1e-9);
  // ask(2) 9 ≥ 3 × bid(1) 2 → buy imbalance at 2; ask(3) 4 ≥ 3 × bid(2) 1 → at 3; ask(4) 0.5 against bid(3) 0 → at 4
  // (a level inside the bar where nobody sold counts as 0: Footprint.h); bid(0) 1 vs ask(1) 0.5: 1 ≥ 1.5 no
  CHECK(s.buyImbalances == 3);
  CHECK(s.sellImbalances == 0);
  CHECK(s.buyStacks == 1);   // three in a row
  CHECK(s.sellStacks == 0);
  CHECK(statsOf(Minute{}).levels == 0);
}

TEST("archive collector: the perpetual's depth and open interest stored, read back, deleted when unticked") {
  PairSpec s = fpSpec({}, 0);
  s.depth = true;
  s.openInterest = true;
  FpRig r(s);
  // the 2026-11-18 heads as that day's files (their rows are of that day), nothing for the days before
  r.fake->put("futures/um/daily/bookDepth/BTCUSDT/BTCUSDT-bookDepth-2026-11-18.zip", fixture("perp-bookDepth-2026-11-18-head.zip"));
  r.fake->put("futures/um/daily/metrics/BTCUSDT/BTCUSDT-metrics-2026-11-18.zip", fixture("perp-metrics-2026-11-18-head.zip"));
  r.col->start();
  CHECK(r.runUntilIdle(60000));
  const auto depth = r.store->archiveDays(r.m, "depth"), oi = r.store->archiveDays(r.m, "oi");
  CHECK(depth.size() == 1 && oi.size() == 1);
  if (depth.size() == 1) {
    CHECK(depth[0].day == utcDay(2026, 11, 18) && depth[0].rows == 40);
    std::vector<technicaldata::perp::DepthSample> back;
    CHECK(technicaldata::perp::decodeDepth(r.store->depthData(r.m, depth[0].day), depth[0].day, back));
    CHECK(back.size() == 40 && !back.empty() && back[0].t == utcDay(2026, 11, 18) + 7);   // 00:00:07
  }
  const auto rows = r.store->openInterest(r.m, utcDay(2026, 11, 18), utcDay(2026, 11, 19));
  CHECK(rows.size() == 40);
  CHECK(!rows.empty() && std::fabs(rows[0].oi - 92007.439) < 1e-6 && rows[1].t - rows[0].t == 300);
  // every other day back to the first archive is missing (older than three days) or not out yet (the last ones)
  CHECK(r.store->archiveMissing(r.m, "depth").size() == int(QDate(2023, 1, 1).daysTo(QDate(2026, 11, 15))) + 1);
  CHECK(r.store->archiveMissing(r.m, "oi").contains(utcDay(2020, 9, 1)));
  for (const auto& p : r.col->progress())
    if (p.kind == "depth") CHECK(p.stored == 1 && p.rows == 40 && p.from == QDate(2023, 1, 1));
  // unticked: its rows go, the other stays
  s.depth = false;
  r.store->setPairSpec(r.m, specToJson(s));
  r.runUntilIdle(3000);
  CHECK(r.store->archiveDays(r.m, "depth").isEmpty() && r.store->archiveMissing(r.m, "depth").isEmpty());
  CHECK(r.store->archiveDays(r.m, "oi").size() == 1 && r.store->openInterest(r.m, 0, utcDay(2027, 1, 1)).size() == 40);
  // the spec round trip
  const PairSpec back = specFromJson(specToJson(s));
  CHECK(!back.depth && back.openInterest && !back.liveBook);
  PairSpec live;
  live.timeframes = {86400};
  live.liveBook = true;
  CHECK(specFromJson(specToJson(live)).liveBook && !specToJson(live).contains("perp"));
}

// ---- the order flow's values of a bar for machine learning (OrderFlowFeatures)
#include "technicaldata/OrderFlowFeatures.h"
TEST("order flow features: a bar's values from its own minutes, the depth and open interest before its close only") {
  QTemporaryDir dir;
  Store st(dir.filePath("technical-data.sqlite"));
  const QString m = "BINANCE:BTCUSDT";
  PairSpec s = fpSpec({"perp"}, utcDay(2026, 11, 18));
  s.depth = true;
  s.openInterest = true;
  st.setPairSpec(m, specToJson(s));
  st.wait();
  QCoreApplication::processEvents();
  const qint64 day = utcDay(2026, 11, 18);
  const DayResult r = buildDay(fixture("perp-2026-11-18-head.zip"), QDate(2026, 11, 18));
  FpDay info;
  info.day = day;
  info.minutes = int(r.minutes.size());
  info.trades = r.trades;
  st.addFootprintDay(m, "perp", info, encodeDay(r.minutes));
  // depth: a snapshot at 00:00:30, one at 00:01:00 exactly (the first bar's close: must not be used for it)
  std::vector<technicaldata::perp::DepthSample> depth(3);
  for (int i = 0; i < 3; ++i) {
    depth[size_t(i)].t = day + (i == 0 ? -10 : i == 1 ? 30 : 60);
    for (int b = 0; b < 12; ++b) depth[size_t(i)].depth[size_t(b)] = float(10 + b + i * 100);
  }
  ArchDay dd;
  dd.day = day;
  dd.rows = 2;
  std::vector<technicaldata::perp::DepthSample> today(depth.begin() + 1, depth.end()), before(depth.begin(), depth.begin() + 1);
  st.addDepthDay(m, dd, technicaldata::perp::encodeDepth(today));
  ArchDay prev = dd;
  prev.day = day - 86400;
  st.addDepthDay(m, prev, technicaldata::perp::encodeDepth(before));   // the snapshot before the day's first bar
  std::vector<technicaldata::perp::OiRow> oi{{day - 300, 100, 0, 1, 1.5, 2, 0.9}, {day, 110, 0, 1, 1.6, 2.1, 1.1}};
  ArchDay od;
  od.day = day;
  st.addOiDay(m, od, {oi[1]});
  od.day = day - 86400;
  st.addOiDay(m, od, {oi[0]});
  st.wait();
  QCoreApplication::processEvents();
  st.wait();

  OrderFlowFeatures f(st, m, 60);
  CHECK(f.available());
  CHECK(f.columns().size() == 11 + 1 + 5 + 4);
  CHECK(f.columns().at(10) == "fp_perp_open_delta_pct");
  CHECK(OrderFlowFeatures::isColumn("fp_perp_delta_pct") && OrderFlowFeatures::isColumn("oi_chg_pct") && !OrderFlowFeatures::isColumn("rsi14"));
  const Minute& m0 = r.minutes.front();
  std::vector<double> v;
  CHECK(f.row(m0.t, m0.close, m0.volume() * 2, v));   // the pair's own volume: twice the perpetual's
  CHECK(v.size() == size_t(f.columns().size()));
  if (v.size() == size_t(f.columns().size())) {
    CHECK_NEAR(v[0], m0.delta() / m0.volume() * 100, 1e-9);                  // fp_perp_delta_pct
    const Stats ms = statsOf(m0);
    CHECK_NEAR(v[1], (m0.close / ms.poc - 1) * 100, 1e-9);                   // fp_perp_poc_dist
    CHECK(v[3] >= 0 && v[3] <= 1);                                           // va_pos
    CHECK_NEAR(v[10], m0.delta() / m0.volume() * 100, 1e-9);                 // fp_perp_open_delta_pct: a 1-minute bar is its opening minute
    CHECK_NEAR(v[11], 0.5, 1e-12);                                           // fp_perp_vol_ratio
    // depth: the snapshot before 00:01:00 is 00:00:30 (index 1), not the one at 00:01:00; bids' share at ±0.2 %
    const double bid02 = 10 + 5 + 100, ask02 = 10 + 6 + 100;
    CHECK_NEAR(v[12], bid02 / (bid02 + ask02) * 100, 1e-6);
    // change of −1 % depth since the open: the snapshot at or before 00:00:00 is yesterday's (index 0)
    CHECK_NEAR(v[15], ((10 + 4 + 100) / double(10 + 4) - 1) * 100, 1e-6);
    // open interest: the row before 00:01:00 is 00:00:00 (110), at the open also 00:00:00 → no change in this bar
    CHECK_NEAR(v[17], 0, 1e-12);
    CHECK_NEAR(v[18], 1.6, 1e-12);
  }
  // the second minute: its close 00:02:00, the depth at 00:01:00 now counts, and the open-interest change is 0 too
  if (r.minutes.size() > 1 && r.minutes[1].t == m0.t + 60) {
    CHECK(f.row(r.minutes[1].t, r.minutes[1].close, r.minutes[1].volume(), v));
    CHECK_NEAR(v[12], double(10 + 5 + 200) / (10 + 5 + 200 + 10 + 6 + 200) * 100, 1e-6);
  }
  // not covered: a minute with no trade stored, a day without footprint, a weekly timeframe
  CHECK(!f.row(day - 3600, 60000, 1, v));
  CHECK(!f.row(day + 20 * 3600, 60000, 1, v));
  CHECK(!OrderFlowFeatures(st, m, 604800).available());
  CHECK(!OrderFlowFeatures(st, "DUKASCOPY:EURUSD", 60).available());
}

// ------------------------------------------------------------------------------------------------ live footprint
#include "technicaldata/LiveFootprint.h"
#include <cstring>
TEST("live footprint: the tape's whole minutes, the same as the archive's, holes and partial minutes left out") {
  const char* tk = "0.1";
  const charts::flow::TickScale sc(charts::flow::parseDecimal(tk, tk + std::strlen(tk)));
  const qint64 day = utcDay(2026, 11, 19), t0 = day + 3600;   // 01:00
  // trades from 01:00:30 to 01:05:10, one every 2 s, prices around 90 000, sides alternating in runs
  std::vector<charts::flow::Trade> all;
  std::vector<AggTrade> agg;
  for (int i = 0; i < 140; ++i) {
    charts::flow::Trade t;
    t.id = 1000 + i;
    t.timeMs = (t0 + 30 + 2 * i) * 1000;
    t.ticks = 900000 + (i * 37 % 200) - 100;
    t.qty = 0.01 * (1 + i % 5);
    t.side = (i / 3) % 2 ? charts::flow::Side::Sell : charts::flow::Side::Buy;
    all.push_back(t);
    agg.push_back(AggTrade{t.id, sc.priceOf(t.ticks), t.qty, t.timeMs, t.side == charts::flow::Side::Sell});
  }
  charts::flow::TradeTape tape(1000);
  for (const auto& t : all) tape.append(t);
  LiveFootprint f;
  LiveFold st;
  foldTrades(tape, sc, f, st, t0 + 400);
  // 01:00 is partial (first trade at :30), 01:05 still open: 01:01 … 01:04 whole
  CHECK(f.covered.size() == 1);
  CHECK(!f.covered.empty() && f.covered.front().first == t0 + 60 && f.covered.front().second == t0 + 300);
  CHECK(f.covers(t0 + 60, t0 + 300) && !f.covers(t0, t0 + 120) && !f.covers(t0 + 240, t0 + 360));
  CHECK(f.minutes.size() == 4);
  // the same minutes as the archive builder on those trades
  DayBuilder b(day);
  for (const auto& a : agg) b.add(a);
  const std::vector<Minute> want = b.finish();
  for (const Minute& w : want) {
    if (w.t < t0 + 60 || w.t >= t0 + 300) continue;
    const auto it = f.minutes.find(w.t);
    CHECK(it != f.minutes.end());
    if (it == f.minutes.end()) continue;
    const Minute& h = it->second;
    CHECK(h.trades == w.trades && h.base == w.base && h.levels.size() == w.levels.size());
    CHECK_NEAR(h.buy, w.buy, 1e-12);
    CHECK_NEAR(h.sell, w.sell, 1e-12);
    CHECK_NEAR(h.open, w.open, 1e-9);
    CHECK_NEAR(h.close, w.close, 1e-9);
    for (size_t k = 0; k < std::min(h.levels.size(), w.levels.size()); ++k) {
      CHECK_NEAR(h.levels[k].bid, w.levels[k].bid, 1e-6);
      CHECK_NEAR(h.levels[k].ask, w.levels[k].ask, 1e-6);
    }
  }
  // more trades, with a hole (ids 1145 … 1149 missing) inside 01:05: that minute is left out
  for (int i = 140; i < 260; ++i) {
    if (i >= 145 && i < 150) continue;
    charts::flow::Trade t = all.back();
    t.id = 1000 + i;
    t.timeMs = (t0 + 30 + 2 * i) * 1000;
    tape.append(t);
  }
  CHECK(tape.missing() == 5);
  foldTrades(tape, sc, f, st, t0 + 700);
  // the hole lies between 01:05:18 (i = 144) and 01:05:30 (i = 150): 01:05 is cut out, 01:06 … 01:09 whole
  CHECK(f.covers(t0 + 60, t0 + 300));
  CHECK(!f.covers(t0 + 300, t0 + 360));
  CHECK(f.covers(t0 + 360, t0 + 540));
  CHECK(f.minutes.count(t0 + 300) == 0);
  // nothing older than 48 h stays
  foldTrades(tape, sc, f, st, t0 + LiveFootprint::kKept + 400);
  CHECK(f.minutes.count(t0 + 60) == 0 && !f.covers(t0 + 60, t0 + 120));
  CHECK(f.covers(t0 + 420, t0 + 540));
}
