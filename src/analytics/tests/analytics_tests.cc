// Unit tests of src/analytics (see CMakeLists.txt beside this file). Plain asserts, exit code = failures.
#include "analytics/Indicators.h"
#include "analytics/Liquidity.h"
#include "analytics/Technical.h"
#include "market/Instruments.h"
#include <QDateTime>
#include <QTimeZone>
#include <cstdio>

using namespace analytics;
static int g_fail = 0, g_run = 0;
#define CHECK(c) do { ++g_run; if (!(c)) { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

static Bar bar(qint64 t, double o, double h, double l, double c, double v = 0) { return Bar{t, o, h, l, c, v}; }
static QVector<Bar> fromCloses(const QVector<double>& c, qint64 t0 = 1700000000, qint64 step = 86400) {
  QVector<Bar> b;
  for (int i = 0; i < c.size(); ++i) {
    const double o = i ? c[i - 1] : c[i];
    b << bar(t0 + i * step, o, std::max(o, c[i]) + 0.1, std::min(o, c[i]) - 0.1, c[i], 1000);
  }
  return b;
}

static void testAverages() {
  const QVector<double> v{1, 2, 3, 4, 5, 6};
  const auto s = sma(v, 3);
  CHECK(!finite(s[1])); CHECK(near(s[2], 2)); CHECK(near(s[5], 5));
  const auto e = ema(v, 3);
  CHECK(near(e[2], 2)); CHECK(near(e[3], 3)); CHECK(near(e[5], 5));     // linear input: EMA lags by (n-1)/2 = 1
  const auto r = roc(v, 2);
  CHECK(near(r[2], 200)); CHECK(near(r[5], 50));
}

static void testRsi() {
  QVector<double> up; for (int i = 0; i < 30; ++i) up << 100 + i;
  CHECK(near(last(rsi(up, 14)), 100));
  QVector<double> dn; for (int i = 0; i < 30; ++i) dn << 100 - i;
  CHECK(near(last(rsi(dn, 14)), 0));
  QVector<double> zz; for (int i = 0; i < 40; ++i) zz << 100 + (i % 2);
  const double x = last(rsi(zz, 14));
  CHECK(x > 40 && x < 60);
  // Wilder's classic example (first RSI of his table ≈ 70.53)
  const QVector<double> w{44.34, 44.09, 44.15, 43.61, 44.33, 44.83, 45.10, 45.42, 45.84, 46.08, 45.89, 46.03, 45.61, 46.28, 46.28};
  CHECK(near(rsi(w, 14)[14], 70.5, 0.1));   // 70.46 exact; the published 70.53 rounds the averages
}

static void testAtrVol() {
  QVector<Bar> b;
  for (int i = 0; i < 20; ++i) b << bar(i, 10, 11, 9, 10);
  CHECK(near(last(atr(b, 14)), 2));
  QVector<double> c; for (int i = 0; i < 30; ++i) c << 100;
  CHECK(near(last(realizedVol(c, 20, 252)), 0));
  QVector<double> p{1, 2, 3, 4, 5};
  CHECK(near(percentileRank(p, 0, 5, 3), 60));
  CHECK(near(percentileRank(p, -10, 50, 5), 100));
}

static void testSwings() {
  // lows/highs: peak at 2, trough at 4
  QVector<Bar> b{bar(0, 1, 2, 0.5, 1), bar(1, 1, 3, 0.6, 1), bar(2, 1, 5, 0.7, 1), bar(3, 1, 4, 0.4, 1),
                 bar(4, 1, 3.5, 0.1, 1), bar(5, 1, 3.6, 0.3, 1), bar(6, 1, 3.7, 0.35, 1)};
  const auto s = swings(b, 1);
  // highs 2 3 5 4 3.5 3.6 3.7 → swing high at 2 only; lows .5 .6 .7 .4 .1 .3 .35 → swing low at 4 only
  CHECK(s.size() == 2);
  bool hi2 = false, lo4 = false;
  for (const auto& x : s) { if (x.high && x.index == 2) hi2 = true; if (!x.high && x.index == 4) lo4 = true; }
  CHECK(hi2); CHECK(lo4);
  // flat top is not a swing
  QVector<Bar> f{bar(0, 1, 1, 0, 1), bar(1, 1, 2, 0, 1), bar(2, 1, 2, 0, 1), bar(3, 1, 1, 0, 1)};
  for (const auto& x : swings(f, 1)) CHECK(!x.high);
}

// Two equal highs at 1.1000 / 1.1005 (within 0.06 %), then price stays below → untested BSL ×2.
static QVector<Bar> twoTops(double after) {
  QVector<Bar> b; qint64 t = 0;
  auto add = [&](double h, double l, double c) { b << bar(t, (h + l) / 2, h, l, c); t += 3600; };
  add(1.0950, 1.0900, 1.0940); add(1.1000, 1.0930, 1.0960); add(1.0970, 1.0920, 1.0930);
  add(1.0960, 1.0880, 1.0900); add(1.1005, 1.0940, 1.0950); add(1.0980, 1.0910, 1.0920);
  add(1.0950, 1.0890, 1.0900);
  if (std::isfinite(after)) add(after, 1.0900, 1.0960);   // pierces 1.1005 and closes back below → sweep
  for (int i = 0; i < 8; ++i) add(1.0940 - i * 0.002, 1.0880 - i * 0.002, 1.0890 - i * 0.002);
  return b;
}

static void testPools() {
  auto pools = findPools(twoTops(NAN));
  int bsl = 0;
  for (const auto& p : pools) if (p.side == PoolSide::BuySide) {
    ++bsl; CHECK(p.members == 2); CHECK(near(p.level, 1.1005)); CHECK(p.state == PoolState::Untested);
    CHECK(p.touches == 1);
  }
  CHECK(bsl == 1);
  pools = findPools(twoTops(1.1010));
  for (const auto& p : pools) if (p.side == PoolSide::BuySide) {
    CHECK(p.state == PoolState::Swept); CHECK(p.eventIndex == 7);
    CHECK(p.reversal != Reversal::None);
  }
  // a close above the level is a break, not a sweep
  auto b = twoTops(NAN);
  b.insert(7, bar(7 * 3600, 1.0950, 1.1030, 1.0940, 1.1020));
  for (const auto& p : findPools(b)) if (p.side == PoolSide::BuySide) CHECK(p.state == PoolState::Broken);
  // tolerance: 0.2 % apart → no pool
  PoolParams tight; tight.tolPct = 0.01;
  int n = 0; for (const auto& p : findPools(twoTops(NAN), tight)) if (p.side == PoolSide::BuySide) ++n;
  CHECK(n == 0);
  // stop-run on the untested pool
  pools = findPools(twoTops(NAN));
  const auto s = stopRun(pools, 1.1000, 0.0005);
  CHECK(s.poolIndex >= 0);
  CHECK(s.level == StopRunLevel::High);                          // 0.0005 away = 1 ATR
  CHECK(stopRun(pools, 1.0700, 0.0005).level == StopRunLevel::Low);
}

static void testSessions() {
  // two UTC days of hourly bars; day 2 has bars 00:00..13:00
  const qint64 d1 = QDateTime(QDate(2026, 10, 21), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  QVector<Bar> b;
  for (int h = 0; h < 24; ++h) b << bar(d1 + h * 3600, 1, 1.0 + h * 0.001, 0.9 - h * 0.001, 1);
  for (int h = 0; h < 14; ++h) b << bar(d1 + 86400 + h * 3600, 1, 2.0 + h, 0.5 - h * 0.01, 1);
  const auto t = sessionExtremes(b, QDate(2026, 10, 22));
  CHECK(t.rows.size() == 6);
  CHECK(!finite(t.rows[0].high));                                // Sydney (21:00 →) not started
  CHECK(near(t.rows[1].high, 2.0 + 8));                          // Tokyo 00..08
  CHECK(near(t.rows[2].high, 2.0 + 13));                         // London 07..13 so far
  CHECK(near(t.rows[3].low, 0.5 - 0.13));                        // New York 12..13
  CHECK(near(t.pdh, 1.023)); CHECK(near(t.pdl, 0.877));
  CHECK(t.rows[1].sub == "today · UTC");
  CHECK(t.rows[4].sub == "2026-10-21");
}

static void testVolume() {
  QVector<Bar> b;
  for (int i = 0; i < 30; ++i) b << bar(qint64(i) * 86400, 1, 1, 1, 1, 100);
  b.last().v = 200;
  CHECK(near(volumeRatio(b, 1000), 2.0));
  CHECK(near(volumeRatio(b, 29), 1.0));                          // last bar is today → previous complete day
  CHECK(volumeSources("EURUSD") == QStringList{"FXE"});
  CHECK(volumeSources("USDJPY") == QStringList{"FXY"});
  CHECK((volumeSources("GBPCHF") == QStringList{"FXB", "FXF"}));
  CHECK(volumeSources("GBPNZD") == QStringList{"FXB"});
  CHECK(volumeSources("BRENT") == QStringList{"BNO"});
  CHECK(volumeSources("MSFT").isEmpty());
  CHECK(volumeSources("US10Y") == QStringList{"-"});
  for (const auto& t : proxyTickers()) CHECK(market::find(t) == nullptr);   // no proxy collides with an app symbol
}

static void testTechnical() {
  QVector<double> c; for (int i = 0; i < 300; ++i) c << 100 * std::pow(1.003, i) + std::sin(i / 3.0);
  const auto b = fromCloses(c);
  const auto r = technicalRead(b, {{"X", b}}, false, 1 << 30);
  CHECK(r.ok);
  CHECK(r.trend == Lean::Bull); CHECK(r.trendPct > 0.1);
  CHECK(r.rsi > 50);
  CHECK(r.volume == VolumeRead::Average); CHECK(near(r.volRatio, 1.0)); CHECK(r.volTicker == "X");
  for (double x : r.radar) CHECK(x >= 0 && x <= 100);
  QVector<double> d; for (int i = 0; i < 300; ++i) d << 100 * std::pow(0.997, i) + std::sin(i / 3.0);
  const auto q = technicalRead(fromCloses(d), {}, false, 1 << 30);
  CHECK(q.trend == Lean::Bear); CHECK(q.volume == VolumeRead::NA); CHECK(near(q.radar[3], 50));
  CHECK(!technicalRead(fromCloses({1, 2, 3}), {}, false, 0).ok);
}

int main() {
  testAverages(); testRsi(); testAtrVol(); testSwings(); testPools(); testSessions(); testVolume(); testTechnical();
  std::printf("%d checks, %d failed\n", g_run, g_fail);
  return g_fail;
}
