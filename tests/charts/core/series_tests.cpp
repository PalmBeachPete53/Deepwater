// Port of quant-terminal test/series.test.js (range bars, kagi, point & figure included; seasonals in
// seasonals_tests.cpp).
#include "Check.h"
#include "charts/core/Series.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>

using namespace charts;
using namespace charts::series;

namespace {
Bars loadBtc() {
  static Bars cached = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES "/btc-1d.json"));
    f.open(QIODevice::ReadOnly);
    Bars out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out.push_back({qint64(o["time"].toDouble()), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(),
                     o["close"].toDouble(), o["volume"].toDouble()});
    }
    return out;
  }();
  return cached;
}
bool throws(const std::function<void()>& f) {
  try { f(); } catch (const std::exception&) { return true; }
  return false;
}
}  // namespace

TEST("series: parses timeframes") {
  CHECK_EQ(parseTf("15m").seconds, 900.0);
  CHECK_EQ(parseTf("4h").seconds, 14400.0);
  CHECK_EQ(parseTf("1D").seconds, 86400.0);
  CHECK(throws([] { parseTf("banana"); }));
}

TEST("series: weekly buckets start on Monday 00:00 UTC") {
  const qint64 t = utcSeconds(2026, 9, 15) + 13 * 3600;   // Thursday 15 Oct 2026
  CHECK_EQ(isoString(bucketStart(t, "1W")), QStringLiteral("2026-10-12T00:00:00.000Z"));
  CHECK_EQ(isoString(bucketStart(t, "1M")), QStringLiteral("2026-10-01T00:00:00.000Z"));
}

TEST("series: resampling keeps OHLC semantics and sums volume") {
  const Bars bars = loadBtc();
  const Bars w = resample(bars, "1W");
  const Bar first = w[1];
  Bars members;
  for (const Bar& b : bars)
    if (bucketStart(b.time, "1W") == first.time) members.push_back(b);
  CHECK_EQ(first.open, members[0].open);
  CHECK_EQ(first.close, members.last().close);
  double hi = -1e300, vol = 0;
  for (const Bar& b : members) { hi = std::max(hi, b.high); vol += b.volume; }
  CHECK_EQ(first.high, hi);
  CHECK(std::fabs(first.volume - vol) < 1e-6);
}

TEST("series: picks the right native base timeframe") {
  const QStringList n = {"1m", "5m", "15m", "1h", "4h", "1D", "1W", "1M"};
  CHECK_EQ(nativeBase("7m", n), QStringLiteral("1m"));
  CHECK_EQ(nativeBase("2h", n), QStringLiteral("1h"));
  CHECK_EQ(nativeBase("3D", n), QStringLiteral("1D"));
  CHECK_EQ(nativeBase("1W", n), QStringLiteral("1W"));
  CHECK_EQ(nativeBase("3M", n), QStringLiteral("1M"));
}

TEST("series: chart-type transforms produce strictly increasing times") {
  const Bars bars = loadBtc();
  for (const Bars& out : {heikinAshi(bars), renko(bars).bars, lineBreak(bars)}) {
    CHECK(out.size() > 10);
    for (int i = 1; i < out.size(); i++) CHECK(out[i].time > out[i - 1].time);
  }
}

TEST("series: CSV export has a header and one row per bar") {
  const Bars bars = loadBtc().mid(0, 5);
  const QString csv = toCsv(bars, {{"rsi", {1, 2, 3, NaN, 5}}});
  const QStringList lines = csv.split('\n');
  CHECK_EQ(lines.size(), 6);
  CHECK(lines[0].endsWith("rsi"));
}

TEST("series: range bars each span the set range, except the last one still forming") {
  const Transformed r = rangeBars(loadBtc(), 2000);
  CHECK(r.bars.size() > 50);
  for (int i = 0; i + 1 < r.bars.size(); ++i) CHECK(std::fabs(r.bars[i].high - r.bars[i].low - 2000) < 1e-6);
  for (int i = 1; i < r.bars.size(); i++) CHECK(r.bars[i].time > r.bars[i - 1].time);
}

TEST("series: kagi turns alternate and only reverse by the reversal amount") {
  const Bars k = kagi(loadBtc(), 3000).bars;
  CHECK(k.size() > 10);
  for (int i = 1; i < k.size(); i++) CHECK(k[i].time > k[i - 1].time);
  for (int i = 2; i < k.size() - 1; i++) {
    const double a = k[i - 1].close - k[i - 2].close, b = k[i].close - k[i - 1].close;
    CHECK(a * b < 0);   // consecutive moves change direction
    CHECK(std::fabs(b) >= 3000 - 1e-6);
  }
}

TEST("series: point and figure columns alternate and reverse by at least three boxes") {
  const Transformed p = pointFigure(loadBtc(), 1000, 3);
  CHECK(p.bars.size() > 10);
  for (int i = 1; i < p.bars.size(); i++) {
    CHECK(p.dirs[i] != p.dirs[i - 1]);
    CHECK(p.bars[i].time > p.bars[i - 1].time);
  }
  for (int i = 1; i + 1 < p.bars.size(); ++i) CHECK(p.boxes[i] >= 3);
}

// ---- port-specific checks
TEST("series: price-only transforms carry no volume, range bars and heikin ashi do") {
  const Bars bars = loadBtc();
  CHECK(std::isnan(renko(bars).bars[0].volume));
  CHECK(std::isnan(lineBreak(bars)[0].volume));
  CHECK(std::isnan(kagi(bars).bars[0].volume));
  CHECK(std::isnan(pointFigure(bars).bars[0].volume));
  CHECK(!std::isnan(rangeBars(bars).bars[0].volume));
  CHECK_EQ(heikinAshi(bars)[5].volume, bars[5].volume);
  // a brick's CSV row has an empty volume column, like JS `b.volume ?? ""`
  CHECK(toCsv(renko(bars, 1000).bars).split('\n')[1].endsWith(","));
}

TEST("series: transforms that would loop for ever in the JS return nothing") {
  Bars flat;
  for (int i = 0; i < 5; ++i) flat.push_back({i * 86400LL, 0, 0, 0, 0, 1});
  CHECK(renko(flat).bars.isEmpty());       // ATR 0 and close 0: box 0
  CHECK(rangeBars(flat).bars.isEmpty());
  CHECK(renko(loadBtc(), -5).bars.isEmpty());
}
