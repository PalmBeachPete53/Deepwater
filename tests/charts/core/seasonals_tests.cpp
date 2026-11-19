// Port of the seasonals test of quant-terminal test/series.test.js.
#include "Check.h"
#include "charts/core/Seasonals.h"
#include "charts/core/Series.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

using namespace charts;

namespace {
Bars loadBtc() {
  QFile f(QStringLiteral(CHARTS_FIXTURES "/btc-1d.json"));
  f.open(QIODevice::ReadOnly);
  Bars out;
  for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
    const QJsonObject o = v.toObject();
    out.push_back({qint64(o["time"].toDouble()), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(),
                   o["close"].toDouble(), o["volume"].toDouble()});
  }
  return out;
}
int yearOf(qint64 t) { int y, m, d; series::utcDate(t, y, m, d); return y; }
}  // namespace

TEST("seasonals: each year chains from the last close of the year before") {
  const Bars bars = loadBtc();
  const seasonals::Result s = seasonals::seasonals(bars);
  CHECK(s.years.size() >= 3);
  const seasonals::Year* y = nullptr;
  for (const auto& x : s.years)
    if (x.complete) { y = &x; break; }
  CHECK(y);   // at least one complete year in the fixture
  if (!y) return;
  Bars rows, prev;
  for (const Bar& b : bars) {
    if (yearOf(b.time) == y->year) rows.push_back(b);
    if (yearOf(b.time) == y->year - 1) prev.push_back(b);
  }
  CHECK(std::fabs(y->total - (rows.last().close / prev.last().close - 1) * 100) < 1e-9);
  // compounding the twelve monthly moves gives the year's move
  for (const auto& m : s.monthly) {
    if (m.year != y->year) continue;
    double acc = 1;
    for (double v : m.months) acc *= 1 + (std::isnan(v) ? 0 : v) / 100;
    CHECK(std::fabs((acc - 1) * 100 - y->total) < 1e-6);
  }
  CHECK_EQ(s.average.size(), 366);
}

TEST("seasonals: the current year is never complete") {
  const Bars bars = loadBtc();
  const auto a = seasonals::seasonals(bars, 2025);
  for (const auto& y : a.years)
    if (y.year == 2025) CHECK(!y.complete);
  const auto b = seasonals::seasonals(bars, 2030);
  for (const auto& y : b.years)
    if (y.year == 2025) CHECK(y.complete);
  CHECK(seasonals::seasonals({}).years.isEmpty());
  CHECK(seasonals::seasonals({}).average.isEmpty());
}
