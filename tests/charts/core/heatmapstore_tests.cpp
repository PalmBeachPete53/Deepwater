// Order flow, step D3 (pure parts): 16-bit quantity codes, records, the column ring, the hourly files.
#include "Check.h"
#include "charts/core/HeatmapStore.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTemporaryDir>
#include <QTimeZone>
#include <random>

using namespace charts::flow;

namespace {
DepthRecord rec(int64_t t, std::mt19937& rng) {
  DepthRecord r;
  r.timeMs = t;
  r.baseTicks = 6300000 + int64_t(rng() % 1000);
  r.bestBid = r.baseTicks + 25600;
  r.bestAsk = r.bestBid + 1;
  r.rowTicks = 100;
  r.flags = rng() % 10 == 0 ? DepthColumn::Missing : 0;
  for (auto& c : r.codes) c = rng() % 3 ? uint16_t(rng() % 65536) : 0;
  return r;
}
HeatmapFileInfo info() { return HeatmapFileInfo{"BTCUSDT", 0, Decimal{1, -2, true}, 1000}; }
}  // namespace

TEST("heatmap codes: 0 stays 0, every quantity within 0.07 %, codes ascend with the quantity, re-encoding is exact") {
  CHECK_EQ(int(encodeQty(0)), 0);
  CHECK_EQ(int(encodeQty(-1)), 0);
  CHECK_EQ(decodeQty(0), 0.0);
  std::mt19937 rng(3);
  double worst = 0;
  bool exact = true, ascending = true;
  uint16_t prev = 0;
  for (int i = 0; i < 200000; ++i) {
    const double q = std::exp2(double(rng() % 60000) / 1000 - 26);   // 1.5e-8 … 7e9
    const uint16_t c = encodeQty(q);
    worst = std::max(worst, std::fabs(decodeQty(c) - q) / q);
    exact = exact && encodeQty(decodeQty(c)) == c;
  }
  for (double q = 1e-8; q < 1e9; q *= 1.01) {
    const uint16_t c = encodeQty(q);
    ascending = ascending && c >= prev;
    prev = c;
  }
  CHECK(worst <= 0.00068);
  CHECK(exact);
  CHECK(ascending);
  CHECK_EQ(int(encodeQty(1e-12)), 1);        // below the smallest step: the smallest non-zero code
  CHECK_EQ(int(encodeQty(1e60)), 65535);     // beyond the range: the largest
}

TEST("heatmap records: bytes round trip exactly; a column encodes row by row") {
  std::mt19937 rng(5);
  for (int k = 0; k < 100; ++k) {
    const DepthRecord r = rec(1795000000000 + k * 1000, rng);
    unsigned char b[DepthRecord::kBytes];
    r.toBytes(b);
    CHECK(DepthRecord::fromBytes(b) == r);
  }
  DepthColumn c;
  c.timeMs = 42;
  c.baseTicks = 1000;
  c.rowTicks = 5;
  c.qty[3] = 2.5f;
  const DepthRecord r = DepthRecord::from(c);
  CHECK_EQ(int(r.codes[3]), int(encodeQty(2.5)));
  CHECK_EQ(int(r.codes[4]), 0);
  CHECK_EQ(r.rowTicks, 5);
}

TEST("heatmap ring: fixed slots, absolute indices, the oldest overwritten, search by time") {
  ColumnRing<DepthRecord> ring(4);
  for (int i = 0; i < 6; ++i) ring.push().timeMs = 1000 * i;
  CHECK_EQ(int(ring.size()), 4);
  CHECK_EQ(int(ring.begin()), 2);
  CHECK_EQ(int(ring.end()), 6);
  CHECK_EQ(ring.at(2).timeMs, int64_t(2000));
  CHECK_EQ(ring.back().timeMs, int64_t(5000));
  CHECK_EQ(int(ring.lowerBoundTime(3500)), 4);
  CHECK_EQ(int(ring.lowerBoundTime(0)), 2);
  CHECK_EQ(int(ring.lowerBoundTime(9999)), 6);
}

TEST("heatmap files: write two hours, read back identical, the hour starts a new file, a cut record is ignored") {
  QTemporaryDir tmp;
  const QString dir = tmp.path() + "/spot-BTCUSDT";
  std::mt19937 rng(9);
  std::vector<DepthRecord> wrote;
  const int64_t t0 = 1795000000000 - 1795000000000 % 3600000 + 3600000 - 30000;   // 30 s before a whole hour
  {
    HeatmapWriter w(dir, info());
    for (int k = 0; k < 60; ++k) {
      wrote.push_back(rec(t0 + k * 1000, rng));
      CHECK(w.append(wrote.back()));
    }
  }
  const QStringList files = QDir(dir).entryList({"*.dwhm"}, QDir::Files, QDir::Name);
  CHECK_EQ(int(files.size()), 2);
  CHECK_EQ(files.value(1), HeatmapWriter::fileName(t0 + 30000));
  HeatmapReader rd;
  CHECK(rd.open(dir + "/" + files.value(0)));
  CHECK_EQ(int(rd.count()), 30);
  CHECK_EQ(rd.info().symbol, QString("BTCUSDT"));
  CHECK_EQ(rd.info().tick.mant, int64_t(1));
  CHECK_EQ(rd.info().tick.exp, -2);
  CHECK_EQ(int(rd.info().intervalMs), 1000);
  const auto all = loadRange(dir, 0, INT64_MAX / 2);
  CHECK_EQ(int(all.size()), 60);
  bool same = all.size() == wrote.size();
  for (size_t i = 0; same && i < all.size(); ++i) same = all[i] == wrote[i];
  CHECK(same);
  const auto part = loadRange(dir, t0 + 10000, t0 + 40000);   // across the hour
  CHECK_EQ(int(part.size()), 30);
  CHECK_EQ(part.front().timeMs, t0 + 10000);
  // a crash in the middle of a record: the reader counts whole records only; the writer cuts it before appending
  {
    QFile f(dir + "/" + files.value(1));
    CHECK(f.open(QIODevice::Append));
    f.write("partial", 7);
  }
  HeatmapReader cut;
  CHECK(cut.open(dir + "/" + files.value(1)));
  CHECK_EQ(int(cut.count()), 30);
  {
    HeatmapWriter w(dir, info());
    CHECK(w.append(rec(t0 + 60000, rng)));
  }
  HeatmapReader after;
  CHECK(after.open(dir + "/" + files.value(1)));
  CHECK_EQ(int(after.count()), 31);
  CHECK_EQ(after.record(30).timeMs, t0 + 60000);
}

TEST("heatmap files: other files are refused; old hours are deleted by name") {
  QTemporaryDir tmp;
  const QString p = tmp.path() + "/x.dwhm";
  {
    QFile f(p);
    CHECK(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(64, 'x'));
  }
  HeatmapReader rd;
  CHECK(!rd.open(p));
  CHECK(!rd.error().isEmpty());
  {
    QFile f(p);
    CHECK(f.open(QIODevice::WriteOnly));
    QByteArray h(64, 0);
    h.replace(0, 8, "DWHEATM1");
    h[8] = 9;   // version 9
    f.write(h);
  }
  HeatmapReader v;
  CHECK(!v.open(p));
  CHECK(v.error().contains("version 9"));
  const QString dir = tmp.path() + "/d";
  QDir().mkpath(dir);
  for (const char* n : {"2026-11-01T10.dwhm", "2026-11-17T23.dwhm", "2026-11-18T05.dwhm", "notes.txt"}) {
    QFile f(dir + "/" + n);
    CHECK(f.open(QIODevice::WriteOnly));
  }
  const int64_t now = QDateTime(QDate(2026, 11, 18), QTime(12, 0), QTimeZone::UTC).toMSecsSinceEpoch();
  CHECK_EQ(pruneOlderThan(dir, 7, now), 1);   // only 2026-11-01
  CHECK(QFile::exists(dir + "/2026-11-17T23.dwhm"));
  CHECK(QFile::exists(dir + "/notes.txt"));
}
