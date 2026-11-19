// Regression tests for the fixes of the audit of 2026-11-18 (docs/AUDIT-2026-11-18-FIX-PROMPT.md), batch 1: memory
// errors, crashes and endless loops in the pure computing layer. Each test failed (crash, hang, 8 GB allocation or a
// sanitizer report) before its fix.
#include "Check.h"
#include "charts/core/CustomJs.h"
#include "charts/core/Indicators.h"
#include "charts/core/Liquidation.h"
#include "charts/core/Series.h"
#include "charts/core/Ticks.h"
#include <QElapsedTimer>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

using namespace charts;
using charts::series::parseTf;
using charts::series::rangeBars;
using charts::series::renko;
using charts::series::Transformed;

namespace {
Bars walk(int n, double start = 100, double step = 0.5) {
  Bars b;
  double p = start;
  for (int i = 0; i < n; ++i) {
    const double o = p;
    p += (i % 7 < 4 ? step : -step);
    b << Bar{1700000000 + qint64(i) * 60, o, std::max(o, p) + step / 2, std::min(o, p) - step / 2, p, 10};
  }
  return b;
}
flow::Decimal dec(const std::string& s) { return flow::parseDecimal(s.data(), s.data() + s.size()); }
}  // namespace

TEST("audit M3: a Def held through byId() stays valid while another thread re-registers and removes its id") {
  ind::Def d;
  d.id = "custom:audit-m3";
  d.name = "Audit M3";
  d.calc = [](const Bars& b, const Params&) {
    ind::Result r;
    r.put("value", ind::sma(ind::source(b), 3));
    return r;
  };
  CHECK(ind::registerIndicator(d, false).isEmpty());
  const ind::DefPtr held = ind::byId("custom:audit-m3");
  CHECK(held != nullptr);
  std::atomic<bool> stop{false};
  std::thread writer([&] {
    ind::Def x = d;
    for (int k = 0; k < 300 && !stop; ++k) {
      x.name = QString("Audit M3 v%1").arg(k);
      ind::registerIndicator(x, false);
      (void)ind::all();
    }
    ind::unregisterIndicator("custom:audit-m3");
  });
  const Bars b = walk(50);
  int ok = 0;
  for (int k = 0; k < 300; ++k) {
    // the held Def is the first registration, whatever the writer does (it was freed under the reader before)
    if (held->name == "Audit M3" && held->calc(b, {}).value("value").size() == 50) ++ok;
    // both ways the app reads it: a kept snapshot, and a loop on the returned temporary (which detaches it)
    const QVector<ind::Def> snapshot = ind::all();
    for (const ind::Def& x : snapshot) (void)x.id.size();
    for (const ind::Def& x : ind::all()) (void)x.name.size();
  }
  stop = true;
  writer.join();
  CHECK_EQ(ok, 300);
  CHECK(ind::byId("custom:audit-m3") == nullptr);
  CHECK_EQ(held->name, QString("Audit M3"));
}

TEST("audit M4: absurd lengths and offsets finish at once with no huge allocation") {
  const Bars b = walk(300);
  QElapsedTimer t;
  t.start();
  for (double len : {1e9, -1e9, std::nan(""), 1e300}) {
    const Params p{{"length", len}};
    const Series rci = ind::compute("rci", b, p).value("value");
    CHECK_EQ(rci.size(), b.size());
    const Series ts = ind::compute("trendstrength", b, p).value("value");
    CHECK_EQ(ts.size(), b.size());
    if (len > 0) CHECK(std::all_of(rci.begin(), rci.end(), [](double v) { return std::isnan(v); }));   // never fills
  }
  CHECK_EQ(ind::compute("fisher", b, {{"length", -1e9}}).value("fisher").size(), b.size());
  // shifts: capped 100,000 bars ahead (1e9 allocated 8 GB per output)
  const ind::Result ichi = ind::compute("ichimoku", b, {{"displacement", 1e9}});
  CHECK(ichi.value("spanA").size() <= b.size() + 100000);
  CHECK(ind::compute("ichimoku", b, {{"displacement", -1e9}}).value("lagging").size() <= b.size() + 100000);
  CHECK(ind::compute("alligator", b, {{"jawOffset", 1e9}}).value("jaw").size() <= b.size() + 100000);
  CHECK_EQ(ind::compute("dpo", b, {{"length", -1e9}}).value("value").size() <= b.size() + 100000, true);
  CHECK(t.elapsed() < 5000);
  // sane parameters are unchanged: the window of the default rci fills from bar 8
  const Series rci9 = ind::compute("rci", b, {}).value("value");
  CHECK(std::isnan(rci9[7]) && std::isfinite(rci9[8]));
}

TEST("audit M7: decimals with absurd exponents are refused, tick counts that don't fit give nothing") {
  CHECK(!dec("0." + std::string(130, '0') + "1").ok);
  CHECK(dec("0." + std::string(20, '0') + "1").ok);   // 21 fraction digits: still a decimal
  const flow::TickScale cent(dec("0.01"));
  CHECK_EQ(cent.ticksOf(dec("63972.09")), int64_t(6397209));
  // an exponent gap of 19 digits would overflow the __int128 product
  CHECK(!cent.tryTicksOf(dec("1" + std::string(19, '0'))).has_value());
  const flow::TickScale tiny(dec("0." + std::string(25, '0') + "1"));
  CHECK(!tiny.tryTicksOf(dec("100")).has_value());
  CHECK_EQ(cent.ticksOf(std::nan("")), int64_t(0));
  CHECK_EQ(cent.ticksOf(1e300), int64_t(0));
}

TEST("audit M9: timeframes with absurd counts are refused") {
  bool threw = false;
  try { (void)parseTf("99999999m"); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
  threw = false;
  try { (void)parseTf("1" + QString(400, '0') + "m"); } catch (const std::exception&) { threw = true; }
  CHECK(threw);
  CHECK_EQ(parseTf("1000000m").n, 1000000.0);
}

TEST("audit M11: renko and range bars end on boxes below the price resolution and keep a brick budget") {
  // flat history except float noise: the default box (ATR 14) is ~1e-14 on a price of 1e6
  Bars flat;
  for (int i = 0; i < 200; ++i) {
    const double p = 1e6 + (i % 2 ? 1e-9 : 0);
    flat << Bar{1700000000 + qint64(i) * 60, p, p, p, p, 1};
  }
  flat << Bar{1700000000 + 200 * 60, 1e6, 2e6, 1e6, 2e6, 1};   // one big move after it
  QElapsedTimer t;
  t.start();
  CHECK(renko(flat).bars.size() <= 200000);
  CHECK(rangeBars(flat).bars.size() <= 200000);
  // only the noise: any box that fits the budget is below the resolution of 1e6, so nothing to draw
  const Bars noise = flat.mid(0, 200);
  CHECK(renko(noise, 1e-14).bars.isEmpty());
  CHECK(rangeBars(noise, 1e-14).bars.isEmpty());
  // a tiny but representable box on a long path: raised to fit the budget
  const Bars b = walk(20000, 100, 50);
  CHECK(renko(b, 1e-3).bars.size() <= 200001);
  CHECK(rangeBars(b, 1e-3).bars.size() <= 200001);
  CHECK(t.elapsed() < 10000);
  // sane boxes are unchanged
  const Transformed r = renko(walk(200), 1);
  CHECK(!r.bars.isEmpty() && r.box == 1);
}

TEST("audit M13: the liquidation model ignores a grid with no row height") {
  flow::LiquidationModel m(flow::LiqGrid{100, 0, 10}, flow::LiqParams{});
  for (int i = 0; i < 5; ++i) m.step(flow::LiqBar{100, 101, 99, 100, 1000, 5000.0 + i * 100, 1});
  CHECK_EQ(m.bars(), 5);
}

TEST("audit S11: ranks by sorting equal the pair count, ties and NaN included") {
  // the former n² definition, kept here as the reference
  auto pairRanks = [](const Series& v) {
    Series out(v.size());
    for (qsizetype i = 0; i < v.size(); ++i) {
      int less = 0, eq = 0;
      for (double b : v) {
        if (b < v[i]) less++;
        else if (b == v[i]) eq++;
      }
      out[i] = less + (eq + 1) / 2.0;
    }
    return out;
  };
  const Series cases[] = {{3, 1, 2}, {5, 5, 5, 1}, {2, std::nan(""), 2, 7, -1, 7, 7}, {}, {std::nan("")}};
  for (const Series& v : cases) {
    const Series a = ind::avgRanks(v), b = pairRanks(v);
    CHECK_EQ(a.size(), b.size());
    for (qsizetype i = 0; i < a.size(); ++i) CHECK(a[i] == b[i]);
  }
  // rci on a long window finishes quickly now (n log n per bar, window at most 1000)
  const Bars b = walk(20000);
  QElapsedTimer t;
  t.start();
  const Series r = ind::compute("rci", b, {{"length", 5000}}).value("value");
  CHECK_EQ(r.size(), b.size());
  CHECK(std::isfinite(r.last()));
  CHECK(t.elapsed() < 20000);
}

TEST("audit S6: a custom indicator cannot import a file outside its own folder") {
  const QStringList abs = custom::check("import x from '/etc/hosts';\nexport default { name: 'x', calc() { return {}; } };\n");
  CHECK(!abs.isEmpty() && abs.join(' ').contains("only files of the custom indicators folder"));
  const QStringList up = custom::check("import x from '../../outside.js';\nexport default { name: 'x', calc() { return {}; } };\n");
  CHECK(!up.isEmpty() && up.join(' ').contains("only files of the custom indicators folder"));
}
