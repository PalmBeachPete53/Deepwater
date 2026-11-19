// The user's rule (2026-11-18): Pine is the definition of an indicator, Charts' C++ engine executes it. Every indicator
// that has a definition in docs/charts/pine/<id>.pine is computed both ways on real BTCUSDT bars from Binance
// (tests/charts/core/fixtures/binance-btcusdt-{1d,1h,5m,1m}.csv: the whole daily history since 2017-08-17, the last
// 5000 bars of each intraday timeframe) with its defaults and some other settings, and every output must agree:
// the same bars without a value (na), and every value within 1e-9 relative (the two engines add in different
// orders; PineTS also rounds its ta results to `precision` digits). The definitions' inputs carry Charts' keys as
// titles and their plots Charts' output keys.
#include "Check.h"
#include "charts/core/Indicators.h"
#include "charts/core/Pine.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QThread>
#include <QMutex>
#include <QRunnable>
#include <QThreadPool>
#include <cmath>

using namespace charts;

namespace {
QString root() { return QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("../../.."); }

Bars csvBars(const QString& name) {
  Bars out;
  QFile f(root() + "/tests/charts/core/fixtures/" + name);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { check::fail(__FILE__, __LINE__, "cannot open " + name); return out; }
  while (!f.atEnd()) {
    const QByteArray line = f.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#') || line.startsWith("time")) continue;
    const QList<QByteArray> c = line.split(',');
    if (c.size() < 6) continue;
    out << Bar{c[0].toLongLong(), c[1].toDouble(), c[2].toDouble(), c[3].toDouble(), c[4].toDouble(), c[5].toDouble()};
  }
  return out;
}

struct Set { QString name; Bars bars; };
const QVector<Set>& sets() {
  static const QVector<Set> s = {
      {"1D", csvBars("binance-btcusdt-1d.csv")},
      {"1h", csvBars("binance-btcusdt-1h.csv")},
      {"5m", csvBars("binance-btcusdt-5m.csv")},
      {"1m", csvBars("binance-btcusdt-1m.csv")},
  };
  return s;
}

QString definition(const QString& id) {
  QFile f(root() + "/docs/charts/pine/" + id + ".pine");
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

struct Case { QString id; QJsonObject params; };
const QVector<Case>& cases() {
  static const QVector<Case> c = {
      {"sma", {}}, {"sma", {{"length", 50}}}, {"sma", {{"length", 200}}}, {"sma", {{"length", 20}, {"source", "hl2"}}},
      {"ema", {}}, {"ema", {{"length", 20}}}, {"ema", {{"length", 50}}}, {"ema", {{"length", 200}}},
      {"bb", {}}, {"bb", {{"length", 50}, {"mult", 2.5}}}, {"bb", {{"source", "hlc3"}}},
      {"bbpb", {}}, {"bbw", {}},
      {"rsi", {}}, {"rsi", {{"length", 7}, {"maLength", 0}}},
      {"macd", {}}, {"macd", {{"fast", 5}, {"slow", 35}, {"signal", 5}}},
      {"atr", {}}, {"atr", {{"length", 7}}},
      {"dmi", {}}, {"dmi", {{"diLength", 10}, {"adxSmoothing", 20}}},
      {"stoch", {}}, {"stoch", {{"smoothK", 3}}},
      {"obv", {}},
      {"vwap", {}}, {"vwap", {{"anchor", "week"}}}, {"vwap", {{"anchor", "month"}}},
  };
  return c;
}

struct Outcome {
  QString where;           // "bb 1h {"mult":2.5}"
  QString error;           // a run failed or the outputs differ
  double worst = 0;        // largest |c - pine| / max(1, |pine|)
  qint64 values = 0;
};

Outcome compare(const Case& c, const Set& s, const QString& source) {
  Outcome o;
  o.where = QString("%1 %2 %3").arg(c.id, s.name, QString::fromUtf8(QJsonDocument(c.params).toJson(QJsonDocument::Compact)));
  ind::Result mine;
  try {
    mine = ind::compute(c.id, s.bars, c.params);
  } catch (const std::exception& e) {
    o.error = QString("Charts threw: %1").arg(e.what());
    return o;
  }
  pine::Options opt;
  opt.symbol = "BINANCE:BTCUSDT";
  opt.inputs = c.params;
  opt.timeoutMs = 120000;
  const pine::Result ref = pine::run(source, s.bars, opt);
  if (!ref.ok()) { o.error = "Pine: " + ref.error; return o; }
  const ind::DefPtr def = ind::byId(c.id);
  for (const ind::Output& out : def->outputs) {
    const pine::Plot* p = ref.plot(out.key);
    if (!p) { o.error = "no Pine plot \"" + out.key + "\""; return o; }
    const Series mineV = mine.value(out.key);
    if (mineV.size() != p->values.size()) { o.error = QString("%1: %2 values vs %3").arg(out.key).arg(mineV.size()).arg(p->values.size()); return o; }
    for (qsizetype i = 0; i < mineV.size(); ++i) {
      const double a = mineV[i], b = p->values[i];
      ++o.values;
      if (std::isnan(a) != std::isnan(b)) {
        o.error = QString("%1[%2]: Charts %3, Pine %4 (one has no value)").arg(out.key).arg(i).arg(a, 0, 'g', 17).arg(b, 0, 'g', 17);
        return o;
      }
      if (std::isnan(a)) continue;
      const double rel = std::fabs(a - b) / std::max(1.0, std::fabs(b));
      o.worst = std::max(o.worst, rel);
      if (rel > 1e-9) {
        o.error = QString("%1[%2]: Charts %3, Pine %4").arg(out.key).arg(i).arg(a, 0, 'g', 17).arg(b, 0, 'g', 17);
        return o;
      }
    }
  }
  return o;
}
}  // namespace

TEST("pine reference: the fixtures are real BTCUSDT bars, the definitions are there") {
  CHECK_EQ(sets()[0].bars.size(), 3379);
  CHECK_EQ(sets()[0].bars.first().time, qint64(1502928000));   // 2017-08-17, Binance's first BTCUSDT day
  for (int i = 1; i < 4; ++i) CHECK_EQ(sets()[i].bars.size(), 5000);
  QSet<QString> ids;
  for (const Case& c : cases()) ids.insert(c.id);
  for (const QString& id : ids) {
    CHECK(!definition(id).isEmpty());
    CHECK(ind::byId(id));
  }
  // every definition file is tested
  for (const QFileInfo& fi : QDir(root() + "/docs/charts/pine").entryInfoList({"*.pine"}, QDir::Files))
    CHECK(ids.contains(fi.completeBaseName()));
}

TEST("pine reference: every Charts indicator with a Pine definition equals it, on 1D, 1h, 5m and 1m BTCUSDT") {
  QHash<QString, QString> src;
  for (const Case& c : cases()) src.insert(c.id, definition(c.id));
  QVector<Outcome> results(cases().size() * sets().size());
  QThreadPool pool;
  pool.setStackSize(16u << 20);
  pool.setMaxThreadCount(std::max(2, QThread::idealThreadCount()));
  QElapsedTimer t;
  t.start();
  int k = 0;
  for (const Case& c : cases())
    for (const Set& s : sets()) {
      Outcome* slot = &results[k++];
      const QString code = src.value(c.id);
      pool.start(QRunnable::create([slot, c, &s, code] { *slot = compare(c, s, code); }));
    }
  pool.waitForDone();
  QHash<QString, double> worstById;
  qint64 values = 0;
  for (const Outcome& o : results) {
    if (!o.error.isEmpty()) check::fail(__FILE__, __LINE__, o.where + ": " + o.error);
    const QString id = o.where.section(' ', 0, 0);
    worstById[id] = std::max(worstById.value(id), o.worst);
    values += o.values;
  }
  QStringList ids = worstById.keys();
  ids.sort();
  QStringList line;
  for (const QString& id : ids) line << QString("%1 %2").arg(id).arg(worstById[id], 0, 'g', 2);
  std::printf("    %d runs, %lld values, %.1f s; largest relative difference per indicator: %s\n", int(results.size()),
              static_cast<long long>(values), t.elapsed() / 1000.0, line.join(", ").toUtf8().constData());
}
