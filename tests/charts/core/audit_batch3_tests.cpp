// Audit 2026-11-18, batch 3 (wrong or lost data), the pure parts: look-ahead outputs refused in rules (D12), SMI
// Ergodic against Pine's ta.tsi (D19), Pine registration and check agreeing (D19), the Pine result cache after a
// reload (D19), Pine indicators on a small-stack pool thread (D19), the heatmap files: one writer per folder, a file
// of another header moved aside, records of another tick left out (D13).
#include "Check.h"
#include "charts/core/Backtest.h"
#include "charts/core/HeatmapStore.h"
#include "charts/core/Indicators.h"
#include "charts/core/Pine.h"
#include "charts/core/PineIndicators.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRunnable>
#include <QTemporaryDir>
#include <QThreadPool>
#include <cmath>

using namespace charts;

namespace {
const Bars& btc() {
  static const Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    Bars out;
    if (!f.open(QIODevice::ReadOnly)) return out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out << Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(), o.value("high").toDouble(),
                 o.value("low").toDouble(), o.value("close").toDouble(), o.value("volume").toDouble()};
    }
    return out;
  }();
  return b;
}

// the message an operand is refused with ("" = accepted)
QString refusal(const QString& operand, const QJsonObject& indicators) {
  try {
    bt::resolveSeries(btc(), indicators)(operand);
  } catch (const std::exception& e) {
    return QString::fromUtf8(e.what());
  }
  return {};
}

QString writeTemp(QTemporaryDir& dir, const QString& name, const QString& text) {
  const QString path = dir.filePath(name);
  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(text.toUtf8());
  return path;
}

struct Unregister {
  QStringList ids;
  ~Unregister() { for (const QString& id : ids) { ind::unregisterIndicator(id); pine::Cache::instance().forget(id); } }
};

flow::DepthRecord rec(int64_t t) {
  flow::DepthRecord r;
  r.timeMs = t;
  r.baseTicks = 1000;
  r.bestBid = 1005;
  r.bestAsk = 1006;
  r.codes[3] = 700;
  return r;
}
}  // namespace

TEST("audit D12: outputs computed from later bars are refused in rules, with the reason") {
  const QJsonObject ind{{"ichi", QJsonObject{{"id", "ichimoku"}}},
                        {"zz", QJsonObject{{"id", "zigzag"}}},
                        {"dpoC", QJsonObject{{"id", "dpo"}, {"params", QJsonObject{{"centered", "On"}}}}},
                        {"dpo", QJsonObject{{"id", "dpo"}}}};
  CHECK(refusal("ichi.lagging", ind).contains("later bars"));
  CHECK(refusal("zz", ind).contains("later bars"));
  CHECK(refusal("dpoC", ind).contains("later bars"));
  // the same indicators' other outputs and settings stay usable
  CHECK_EQ(refusal("ichi.conversion", ind), QString());
  CHECK_EQ(refusal("ichi.spanA", ind), QString());   // shifted forward: drawn from past bars
  CHECK_EQ(refusal("dpo", ind), QString());
  // a lagging span of 1 bar is not shifted at all
  CHECK_EQ(refusal("i1.lagging", QJsonObject{{"i1", QJsonObject{{"id", "ichimoku"}, {"params", QJsonObject{{"displacement", 1}}}}}}), QString());
}

TEST("audit D19: SMI Ergodic is Pine's ta.tsi(close, short, long): long EMA first, then short") {
  const char* src = R"(//@version=5
indicator("smi check")
lng = input.int(20, "long")
sht = input.int(5, "short")
plot(ta.tsi(close, sht, lng), "smi")
)";
  for (const QJsonObject& params : {QJsonObject{}, QJsonObject{{"long", 30}, {"short", 8}}}) {
    const ind::Result mine = ind::compute("smiergodic", btc(), params);
    pine::Options o;
    o.inputs = params;
    o.timeoutMs = 60000;
    const pine::Result ref = pine::run(src, btc(), o);
    CHECK(ref.ok());
    const pine::Plot* p = ref.plot("smi");
    CHECK(p != nullptr);
    if (!p) return;
    const Series a = mine.value("smi");
    CHECK_EQ(a.size(), p->values.size());
    int compared = 0;
    double worst = 0;
    for (qsizetype i = 0; i < std::min(a.size(), p->values.size()); ++i) {
      if (!std::isfinite(a[i]) || !std::isfinite(p->values[i])) continue;
      ++compared;
      worst = std::max(worst, std::fabs(a[i] - p->values[i]));
    }
    CHECK(compared > int(btc().size()) / 2);
    CHECK(worst < 1e-6);   // PineTS rounds to its precision
  }
}

TEST("audit D19: a Pine script with linewidth 5 or a long title registers, and check() says the same") {
  QTemporaryDir dir;
  const QString title = QString("A very long indicator title ").repeated(4);   // > 80 characters
  const QString src = "//@version=5\nindicator(\"" + title + "\")\nplot(ta.sma(close, 10), \"ma\", linewidth=5)\n";
  CHECK_EQ(pine::check(src), QString());
  const auto l = pine::loadFile(writeTemp(dir, "wide-line.pine", src));
  Unregister guard{{l.id}};
  CHECK(l.ok());
  const ind::DefPtr d = ind::byId(l.id);
  CHECK(d != nullptr);
  if (!d) return;
  CHECK(d->name.size() <= 80);
  CHECK_EQ(d->outputs.value(0).lineWidth, 4);
  // a script registration refuses is refused by check() too (same rules): no outputs to draw
  CHECK(!pine::check("//@version=5\nindicator(\"x\")\nbgcolor(color.red)\n").isEmpty());
}

TEST("audit D19: after a reload the Pine cache never serves the old script's result") {
  QTemporaryDir dir;
  const QString path = dir.filePath("reload.pine");
  auto put = [&](int k) { writeTemp(dir, "reload.pine", "//@version=5\nindicator(\"r\")\nplot(close * 0 + " + QString::number(k) + ", \"v\")\n"); };
  put(1);
  auto l = pine::loadFile(path);
  Unregister guard{{l.id}};
  CHECK(l.ok());
  ind::Result r = ind::compute(l.id, btc());   // GUI thread, no chart: computed now and kept
  CHECK_EQ(r.value("v").value(5), 1.0);
  put(2);
  l = pine::loadFile(path);
  CHECK(l.ok());
  r = ind::compute(l.id, btc());
  CHECK_EQ(r.value("v").value(5), 2.0);
}

TEST("audit D19: a Pine indicator computed on a pool thread (512 KB stack) runs on a big stack") {
  QTemporaryDir dir;
  const auto l = pine::loadFile(writeTemp(dir, "pool.pine", "//@version=5\nindicator(\"p\")\nplot(ta.rsi(close, 14), \"rsi\")\n"));
  Unregister guard{{l.id}};
  CHECK(l.ok());
  ind::Result r;
  QThreadPool pool;   // the default stack of pool threads, as the tester's and /api/backtest's
  pool.start(QRunnable::create([&] { r = ind::compute(l.id, btc()); }));
  pool.waitForDone();
  CHECK_EQ(r.error, QString());
  CHECK_EQ(r.value("rsi").size(), btc().size());
}

TEST("audit D13: one heatmap writer per folder; a file of another header is moved aside; another tick's records are left out") {
  QTemporaryDir tmp;
  const QString dir = tmp.filePath("spot-TEST");
  const int64_t t0 = 1795000000000 - 1795000000000 % 3600000;   // an hour's start
  flow::HeatmapFileInfo info{"TEST", 0, flow::Decimal{1, -2, true}, 1000};
  {
    flow::HeatmapWriter a(dir, info);
    CHECK(a.append(rec(t0)));
    CHECK(a.flush());
    flow::HeatmapWriter b(dir, info);   // a second Deepwater on the same data folder
    CHECK(!b.append(rec(t0 + 1000)));
    CHECK(b.error().contains("another Deepwater"));
  }
  {
    flow::HeatmapWriter b(dir, info);   // the first one is gone: the lock is free
    CHECK(b.append(rec(t0 + 1000)));
  }
  CHECK_EQ(int(flow::loadRange(dir, t0, t0 + 3600000).size()), 2);
  // the exchange changed the tick: the same hour's file is not appended to; it stays, under another name
  flow::HeatmapFileInfo info2 = info;
  info2.tick = flow::Decimal{1, -1, true};
  {
    flow::HeatmapWriter w(dir, info2);
    CHECK(w.append(rec(t0 + 2000)));
  }
  const QStringList files = QDir(dir).entryList({"*.dwhm"}, QDir::Files, QDir::Name);
  CHECK_EQ(int(files.size()), 2);
  CHECK(files.value(0).endsWith("-1.dwhm"));
  // read with today's tick, only today's tick's records; without one, everything
  CHECK_EQ(int(flow::loadRange(dir, t0, t0 + 3600000, info2.tick).size()), 1);
  CHECK_EQ(int(flow::loadRange(dir, t0, t0 + 3600000, info.tick).size()), 2);
  CHECK_EQ(int(flow::loadRange(dir, t0, t0 + 3600000).size()), 3);
  // a file that isn't a heatmap file at all is moved aside too, never appended to
  QFile junk(dir + "/" + flow::HeatmapWriter::fileName(t0 + 3600000));
  CHECK(junk.open(QIODevice::WriteOnly));
  junk.write(QByteArray(200, 'x'));
  junk.close();
  {
    flow::HeatmapWriter w(dir, info2);
    CHECK(w.append(rec(t0 + 3600000)));
  }
  CHECK_EQ(int(flow::loadRange(dir, t0 + 3600000, t0 + 7200000, info2.tick).size()), 1);
}
