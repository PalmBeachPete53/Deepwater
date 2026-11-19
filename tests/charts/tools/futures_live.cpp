// Live check of the futures statistics against Binance (network; not run by the test suite):
//   DW_PROFILE=<scratch> ./build-tests-flow/charts_futures_live BTCUSDT 120
// Fills the last 30 days (open interest and accounts ratio every 5 min, funding), listens to liquidations for the given
// seconds, then prints counts, the time covered, holes in the 5 min grid, and the requests made. A second run in the same
// profile shows the start from disk (only the new points asked).
#include "charts/data/FuturesStats.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimeZone>
#include <QTimer>
#include <cstdio>

using namespace charts::data;

static QString iso(qint64 ms) { return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm"); }

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (qEnvironmentVariableIsEmpty("DW_PROFILE")) { std::fprintf(stderr, "set DW_PROFILE to a scratch folder\n"); return 2; }
  const QString sym = argc > 1 ? QString(argv[1]) : "BTCUSDT";
  const int seconds = argc > 2 ? atoi(argv[2]) : 60;
  QElapsedTimer t;
  t.start();
  auto s = FuturesStats::acquire(sym);
  std::printf("from disk: %zu OI, %zu ratio, %zu funding, %zu liquidations\n", s->openInterest().size(), s->ratios().size(),
              s->funding().size(), s->liquidations().size());
  qint64 caughtMs = -1;
  QObject::connect(s.get(), &FuturesStats::statusChanged, [&] {
    if (caughtMs < 0 && s->status().caughtUp) caughtMs = t.elapsed();
  });
  const size_t liq0 = s->liquidations().size();
  QTimer::singleShot(seconds * 1000, &app, [&] {
    const auto& oi = s->openInterest();
    int holes = 0;
    for (size_t i = 1; i < oi.size(); ++i) holes += int((oi[i].timeMs - oi[i - 1].timeMs) / 300000 - 1);
    std::printf("caught up in %lld ms after %d requests (%d errors%s%s)\n", (long long)caughtMs, s->status().requests, s->status().errors,
                s->status().error.isEmpty() ? "" : ", last: ", qPrintable(s->status().error));
    if (!oi.empty())
      std::printf("open interest: %zu points %s → %s UTC, %d missing 5 min periods, last %.3f (%s)\n", oi.size(), qPrintable(iso(oi.front().timeMs)),
                  qPrintable(iso(oi.back().timeMs)), holes, oi.back().oi, qPrintable(iso(oi.back().timeMs)));
    if (!s->ratios().empty()) std::printf("accounts ratio: %zu points, last %.4f\n", s->ratios().size(), s->ratios().back().ratio);
    if (!s->funding().empty())
      std::printf("funding: %zu rates %s → %s, last %.6f\n", s->funding().size(), qPrintable(iso(s->funding().front().timeMs)),
                  qPrintable(iso(s->funding().back().timeMs)), s->funding().back().rate);
    const auto& lq = s->liquidations();
    double longQ = 0, shortQ = 0;
    for (size_t i = liq0; i < lq.size(); ++i) (lq[i].longLiquidated ? longQ : shortQ) += lq[i].qty;
    std::printf("liquidations in %d s: %zu (longs %.3f, shorts %.3f), live %s\n", seconds, lq.size() - liq0, longQ, shortQ,
                s->status().live == FeedState::Open ? "open" : "not open");
    s.reset();
    QTimer::singleShot(200, &app, &QCoreApplication::quit);
  });
  return app.exec();
}
