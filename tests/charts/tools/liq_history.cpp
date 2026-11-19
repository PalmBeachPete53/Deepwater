// Calibrates the liquidation heatmap on Binance's archive (offline once the files are downloaded):
//   tools/binance_archive.py <dir> cm liquidationSnapshot BTCUSD_PERP 2023-06-25 2024-10-14   (and metrics, klines 5m)
//   ./build-tests-flow/charts_liq_history <dir> cm BTCUSD_PERP [row height=25] [dense share=0.05]
// Reads the daily CSVs, builds 5 min bars with the open interest at each close, runs the model with the default leverage
// mix and the 70 mixes of 5×…100× (in parallel), scores each on the real liquidations — chosen on the first two thirds,
// judged on the last third — next to the price-proximity baseline, and prints the ranking.
#include "charts/core/LiqCalibration.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTimeZone>
#include <QtConcurrent>
#include <cstdio>

using namespace charts;
using namespace charts::flow;

static QByteArray readAll(const QString& folder, const QString& filter, int* files) {
  QByteArray all;
  const QStringList names = QDir(folder).entryList({filter}, QDir::Files, QDir::Name);
  for (const QString& n : names) {
    QFile f(folder + "/" + n);
    if (f.open(QIODevice::ReadOnly)) all += f.readAll() + "\n";
  }
  *files = int(names.size());
  return all;
}
static QString day(qint64 ms) { return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString("yyyy-MM-dd"); }
static QString mixText(const std::vector<LiqParams::Lev>& m) {
  QStringList p;
  for (const auto& l : m) p << QString("%1× %2 %").arg(l.leverage).arg(qRound(l.weight * 100));
  return p.join(", ");
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (argc < 4) { std::fprintf(stderr, "usage: charts_liq_history <dir> <cm|um> <SYMBOL> [row height] [dense share]\n"); return 2; }
  const QString dir = argv[1], market = argv[2], sym = argv[3];
  const double rowH = argc > 4 ? atof(argv[4]) : 25, share = argc > 5 ? atof(argv[5]) : 0.05;
  QElapsedTimer t;
  t.start();
  int fk = 0, fm = 0, fl = 0;
  const auto bars = parseArchiveKlines(readAll(dir + "/" + market + "/klines/" + sym, "*.csv", &fk));
  std::vector<OiPoint> oi;
  std::vector<RatioPoint> ratios;
  parseArchiveMetrics(readAll(dir + "/" + market + "/metrics/" + sym, "*.csv", &fm), oi, ratios);
  const auto events = parseArchiveLiquidations(readAll(dir + "/" + market + "/liquidationSnapshot/" + sym, "*.csv", &fl));
  if (bars.empty() || oi.empty() || events.empty()) { std::fprintf(stderr, "missing data (%zu bars, %zu OI, %zu liquidations)\n", bars.size(), oi.size(), events.size()); return 1; }
  std::printf("read %d kline days (%zu bars), %d metric days (%zu OI points, %zu ratios), %d liquidation days (%zu liquidations) in %.1f s\n", fk,
              bars.size(), fm, oi.size(), ratios.size(), fl, events.size(), t.elapsed() / 1000.0);
  // 5 min bars, open interest at each close
  const int64_t barMs = 300000;
  LiqInput in;
  std::vector<int64_t> closeMs;
  double lo = 1e300, hi = 0;
  for (const Bar& b : bars) {
    in.open.push_back(b.open); in.high.push_back(b.high); in.low.push_back(b.low); in.close.push_back(b.close); in.volume.push_back(b.volume);
    in.openMs.push_back(b.time * 1000);
    closeMs.push_back(b.time * 1000 + barMs);
    lo = std::min(lo, b.low);
    hi = std::max(hi, b.high);
  }
  in.oi = alignOi(closeMs, oi, 15 * 60 * 1000);
  in.ratio = ratios.empty() ? std::vector<double>(bars.size(), NaN) : alignRatio(closeMs, ratios, 15 * 60 * 1000);
  int known = 0;
  for (double x : in.oi) known += std::isfinite(x);
  in.barMs = barMs;
  in.grid.priceLow = lo * 0.78;
  in.grid.rowHeight = rowH;
  in.grid.rows = int(std::ceil((hi * 1.22 - in.grid.priceLow) / rowH));
  // train: the first two thirds of the liquidations (in time), test: the last third
  const int64_t cut = events[events.size() * 2 / 3].timeMs;
  std::printf("bars %s … %s, open interest on %d of %zu bars; grid %d rows of %.0f from %.0f; train before %s (%zu), test after (%zu)\n",
              qPrintable(day(in.openMs.front())), qPrintable(day(in.openMs.back())), known, bars.size(), in.grid.rows, rowH, in.grid.priceLow,
              qPrintable(day(cut)), events.size() * 2 / 3, events.size() - events.size() * 2 / 3);
  LiqParams base;
  base.brackets = {{1e18, 125, 0.004, 0}};   // COIN-M: one bracket, 0.4 % (the first one), up to 125×
  base.inverse = market == "cm";             // COIN-M contracts are inverse
  std::vector<std::vector<LiqParams::Lev>> mixes = calibrationMixes();
  mixes.insert(mixes.begin(), calibrationLeverages());   // [0] = the default mix
  struct Row { std::vector<LiqParams::Lev> mix; StreamScores s; };
  QElapsedTimer tc;
  tc.start();
  const QList<Row> rows = QtConcurrent::blockingMapped(QList<std::vector<LiqParams::Lev>>(mixes.begin(), mixes.end()), [&](const std::vector<LiqParams::Lev>& m) {
    LiqParams p = base;
    p.leverages = m;
    return Row{m, scoreStream(in.series(), in.openMs, barMs, in.grid, p, events, cut, share)};
  });
  std::printf("71 runs in %.1f s\n\n", tc.elapsed() / 1000.0);
  const Row& def = rows.front();
  std::printf("share of liquidated quantity in the %.0f %% densest rows (± 1 row)        train   test\n", share * 100);
  std::printf("  price-proximity baseline (the rows next to the open)                 %5.1f %%  %5.1f %%\n", def.s.trainNear.hit * 100, def.s.testNear.hit * 100);
  std::printf("  default mix (%s)   %5.1f %%  %5.1f %%\n", qPrintable(mixText(def.mix)), def.s.trainModel.hit * 100, def.s.testModel.hit * 100);
  std::vector<const Row*> order;
  for (int i = 1; i < rows.size(); ++i) order.push_back(&rows[i]);
  std::sort(order.begin(), order.end(), [](const Row* a, const Row* b) { return a->s.trainModel.hit > b->s.trainModel.hit; });
  std::printf("  chosen on train: %s   %5.1f %%  %5.1f %%\n", qPrintable(mixText(order[0]->mix)), order[0]->s.trainModel.hit * 100, order[0]->s.testModel.hit * 100);
  // the fairer number: rank inside the rows each bar crossed (0.5 = no information on where)
  std::vector<const Row*> byRank = order;
  std::sort(byRank.begin(), byRank.end(), [](const Row* a, const Row* b) { return a->s.trainRank.rank > b->s.trainRank.rank; });
  std::printf("\nrank of the liquidation's row among the rows its bar crossed, by the model's density (0.5 = chance)   train   test\n");
  std::printf("  default mix                                                                                  %.3f   %.3f\n", def.s.trainRank.rank, def.s.testRank.rank);
  for (int i = 0; i < 6 && i < int(byRank.size()); ++i)
    std::printf("  %-90s %.3f   %.3f\n", qPrintable(mixText(byRank[i]->mix)), byRank[i]->s.trainRank.rank, byRank[i]->s.testRank.rank);
  std::printf("  worst: %-83s %.3f   %.3f\n", qPrintable(mixText(byRank.back()->mix)), byRank.back()->s.trainRank.rank, byRank.back()->s.testRank.rank);
  std::printf("  (ranked: train %d liquidations, test %d — the others fell in bars crossing a single row)\n", def.s.trainRank.events, def.s.testRank.events);
  std::printf("\nbest 8 on train (with their test):\n");
  for (int i = 0; i < 8 && i < int(order.size()); ++i)
    std::printf("  %-48s train %5.1f %%  test %5.1f %%\n", qPrintable(mixText(order[i]->mix)), order[i]->s.trainModel.hit * 100, order[i]->s.testModel.hit * 100);
  std::printf("worst 3 on train:\n");
  for (int i = int(order.size()) - 3; i < int(order.size()); ++i)
    std::printf("  %-48s train %5.1f %%  test %5.1f %%\n", qPrintable(mixText(order[i]->mix)), order[i]->s.trainModel.hit * 100, order[i]->s.testModel.hit * 100);
  // month by month (the default mix): liquidations come in cascades, so one bad month could carry the total
  std::printf("\nmonth by month, default mix (rank, liquidations ranked):\n");
  {
    QMap<QString, std::vector<LiqEvent>> byMonth;
    for (const LiqEvent& e : events) byMonth[QDateTime::fromMSecsSinceEpoch(e.timeMs, QTimeZone::UTC).toString("yyyy-MM")].push_back(e);
    const QStringList months = byMonth.keys();
    const QList<QPair<QString, StreamScores>> per = QtConcurrent::blockingMapped(months, [&](const QString& m) {
      LiqParams p = base;
      p.leverages = calibrationLeverages();
      return qMakePair(m, scoreStream(in.series(), in.openMs, barMs, in.grid, p, byMonth[m], INT64_MAX, share));
    });
    int above = 0, n = 0;
    for (const auto& [m, sc] : per) {
      if (sc.trainRank.events < 50) continue;
      std::printf("  %s  %.3f  (%d)\n", qPrintable(m), sc.trainRank.rank, sc.trainRank.events);
      above += sc.trainRank.rank > 0.5;
      ++n;
    }
    std::printf("  above 0.5 in %d of %d months\n", above, n);
  }
  std::printf("\nscored: train %d liquidations (%.0f contracts), test %d (%.0f)\n", def.s.trainModel.events, def.s.trainModel.qty, def.s.testModel.events,
              def.s.testModel.qty);
  return 0;
}
