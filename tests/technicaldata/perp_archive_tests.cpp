// The perpetual's order book depth and positioning archives (src/technicaldata/PerpArchive): both against values
// computed apart in Python (fixtures/make_perp.py), the bands, the stored form, the imbalance.
#include "Check.h"
#include "technicaldata/FootprintArchive.h"
#include "technicaldata/PerpArchive.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <cmath>

using namespace technicaldata::perp;

namespace {
QByteArray unzipFixture(const QString& name) {
  QFile f(QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("fixtures/" + name));
  if (!f.open(QIODevice::ReadOnly)) return {};
  QByteArray csv;
  QString err;
  technicaldata::fp::inflateZip(f.readAll(), [&](const char* p, size_t n) { csv.append(p, qsizetype(n)); }, &err);
  return csv;
}
QJsonObject expected() {
  QFile f(QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("fixtures/perp-expected.json"));
  return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}
qint64 dayOf(const char* d) { return QDateTime(QDate::fromString(d, "yyyy-MM-dd"), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(); }
bool rel(double a, double b) { return std::fabs(a - b) <= 1e-6 * std::max(1.0, std::fabs(b)); }
}  // namespace

TEST("perp archive: the bands, in their order; a band read from either spelling") {
  CHECK(bandOf(-5) == 0 && bandOf(-0.2) == 5 && bandOf(0.2) == 6 && bandOf(5) == 11);
  CHECK(bandOf(0.5) == -1);
  CHECK(depthUrl("BTCUSDT", QDate(2026, 11, 18)).toString() ==
        "https://data.binance.vision/data/futures/um/daily/bookDepth/BTCUSDT/BTCUSDT-bookDepth-2026-11-18.zip");
  CHECK(metricsUrl("BTCUSDT", QDate(2020, 9, 1)).toString() ==
        "https://data.binance.vision/data/futures/um/daily/metrics/BTCUSDT/BTCUSDT-metrics-2020-09-01.zip");
}

TEST("perp archive: depth snapshots as Python reads them; 2023 without ±0.2 %; stored and read back") {
  const QJsonObject want = expected();
  for (const char* day : {"2023-01-01", "2026-11-18"}) {
    int bad = -1;
    const auto s = parseDepth(unzipFixture(QString("perp-bookDepth-%1-head.zip").arg(day)), dayOf(day), &bad);
    const QJsonArray w = want.value(QString("bookDepth-%1").arg(day)).toArray();
    CHECK(bad == 0);
    CHECK(int(s.size()) == w.size());
    for (int i = 0; i < std::min(int(s.size()), int(w.size())); ++i) {
      const QJsonArray r = w[i].toArray();
      CHECK(s[size_t(i)].t == qint64(r[0].toDouble()));
      for (int b = 0; b < kBands; ++b) {
        const QJsonValue v = r[b + 1];
        if (v.isNull()) CHECK(std::isnan(s[size_t(i)].depth[size_t(b)]));
        else CHECK(rel(s[size_t(i)].depth[size_t(b)], v.toDouble()));
      }
    }
    std::vector<DepthSample> back;
    CHECK(decodeDepth(encodeDepth(s), dayOf(day), back));
    CHECK(back.size() == s.size());
    for (size_t i = 0; i < std::min(back.size(), s.size()); ++i) {
      CHECK(back[i].t == s[i].t);
      for (int b = 0; b < kBands; ++b)
        CHECK(back[i].depth[size_t(b)] == s[i].depth[size_t(b)] || (std::isnan(back[i].depth[size_t(b)]) && std::isnan(s[i].depth[size_t(b)])));
    }
  }
  // the 2023 file has no ±0.2 %: its imbalance at 0.2 is unknown, at 1 % it is there
  const auto old = parseDepth(unzipFixture("perp-bookDepth-2023-01-01-head.zip"), dayOf("2023-01-01"));
  CHECK(!old.empty() && std::isnan(imbalance(old[0], 6)));
  CHECK(!old.empty() && imbalance(old[0], 7) > 0 && imbalance(old[0], 7) < 1);
  std::vector<DepthSample> none;
  CHECK(!decodeDepth(QByteArray("nope"), 0, none));
  // a day's own rows only
  int bad = 0;
  CHECK(parseDepth(unzipFixture("perp-bookDepth-2026-11-18-head.zip"), dayOf("2026-11-19"), &bad).empty());
  CHECK(bad == 40 * 12);
}

TEST("perp archive: open interest and ratios as Python reads them; rows listed twice kept once") {
  const QJsonObject want = expected();
  for (const char* day : {"2020-09-01", "2026-11-18"}) {
    int bad = -1;
    const auto rows = parseMetrics(unzipFixture(QString("perp-metrics-%1-head.zip").arg(day)), dayOf(day), &bad);
    const QJsonArray w = want.value(QString("metrics-%1").arg(day)).toArray();
    CHECK(bad == 0);
    CHECK(int(rows.size()) == w.size());
    for (int i = 0; i < std::min(int(rows.size()), int(w.size())); ++i) {
      const QJsonArray r = w[i].toArray();
      const OiRow& x = rows[size_t(i)];
      CHECK(x.t == qint64(r[0].toDouble()));
      CHECK(rel(x.oi, r[1].toDouble()) && rel(x.oiValue, r[2].toDouble()) && rel(x.topAccounts, r[3].toDouble()));
      CHECK(rel(x.topPositions, r[4].toDouble()) && rel(x.accounts, r[5].toDouble()) && rel(x.takerRatio, r[6].toDouble()));
    }
  }
}
