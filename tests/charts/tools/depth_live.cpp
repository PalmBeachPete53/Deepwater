// Live check of the depth stream against Binance (not a unit test: needs the network).
//   DW_PROFILE=<scratch> charts_depth_live BINANCE:BTCUSDT [seconds=90]
// Every 10 s: feed, synced, book levels, events/s, average apply time, columns, Missing columns; and the stream's best
// bid / ask next to Binance's own bookTicker fetched at the same moment (a few ticks apart at most: the two answers
// are not taken at the same microsecond).
#include "charts/data/DepthStream.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTimer>
#include <cstdio>
#include <sys/resource.h>

using namespace charts::data;

namespace {
double cpuSeconds() {
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
  return u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1e6 + u.ru_stime.tv_sec + u.ru_stime.tv_usec / 1e6;
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (qEnvironmentVariableIsEmpty("DW_PROFILE")) { std::fprintf(stderr, "set DW_PROFILE to a scratch folder\n"); return 2; }
  const QString sym = argc > 1 ? argv[1] : "BINANCE:BTCUSDT";
  const int seconds = argc > 2 ? atoi(argv[2]) : 90;
  const auto m = FlowMarket::fromChartSymbol(sym);
  if (!m) return 2;
  QElapsedTimer wall;
  wall.start();
  const double cpu0 = cpuSeconds();
  auto st = DepthStream::acquire(*m);
  qint64 syncedAt = -1, lastEvents = 0;
  int worstGap = 0, compared = 0, missingAfterSync = 0;
  size_t seenColumns = 0;
  QObject::connect(st.get(), &DepthStream::statusChanged, [&] { if (st->status().synced && syncedAt < 0) syncedAt = wall.elapsed(); });
  QObject::connect(st.get(), &DepthStream::appended, [&](int) {
    const auto& cols = st->live();
    seenColumns = cols.size();
    if (syncedAt >= 0 && !cols.empty() && (cols.back().flags & charts::flow::DepthColumn::Missing)) ++missingAfterSync;
  });
  QTimer tick;
  QObject::connect(&tick, &QTimer::timeout, [&] {
    const auto& s = st->status();
    std::printf("%4llds feed=%s synced=%d levels=%d events=%lld (+%.1f/s) apply=%.1f µs columns=%zu resyncs=%d snapshots=%d dropped=%lld err=%s\n",
                (long long)(wall.elapsed() / 1000), s.feed == FeedState::Open ? "open" : "not open", int(s.synced), s.bookLevels,
                (long long)s.events, (s.events - lastEvents) / 10.0, s.applyNs / 1000, st->live().size(), s.resyncs, s.snapshots,
                (long long)s.dropped, qPrintable(s.error));
    std::fflush(stdout);
    lastEvents = s.events;
    if (!s.synced || !st->ready()) return;
    const bool fut = m->venue == FlowMarket::BinanceUsdm;
    const QString url = QString(fut ? "https://fapi.binance.com/fapi/v1" : "https://api.binance.com/api/v3") + "/ticker/bookTicker?symbol=" + m->symbol;
    const qint64 ourBid = s.bestBid, ourAsk = s.bestAsk;
    getJson(url, {}, [&, ourBid, ourAsk](const Result<QJsonDocument>& r) {
      if (!r.ok()) return;
      const auto o = r.value.object();
      const qint64 bb = st->ticks().ticksOf(o.value("bidPrice").toString().toDouble());
      const qint64 ba = st->ticks().ticksOf(o.value("askPrice").toString().toDouble());
      const int gap = int(std::max(std::llabs(bb - ourBid), std::llabs(ba - ourAsk)));
      worstGap = std::max(worstGap, gap);
      ++compared;
      std::printf("      best bid/ask: ours %.2f / %.2f, Binance bookTicker %.2f / %.2f (%d ticks apart)\n", st->ticks().priceOf(ourBid),
                  st->ticks().priceOf(ourAsk), st->ticks().priceOf(bb), st->ticks().priceOf(ba), gap);
      std::fflush(stdout);
    }, GetOpts{0, false});
  });
  tick.start(10000);
  QTimer::singleShot(seconds * 1000, [&] {
    const double cpu = cpuSeconds() - cpu0;
    std::printf("\nrecorded %lld records into %s\n", (long long)st->recorded(), qPrintable(st->recordDir()));
    {
      // read the files back (what the writer had flushed by now) and measure the size per hour
      const auto recs = charts::flow::loadRange(st->recordDir(), 0, INT64_MAX / 2);
      qint64 bytes = 0;
      for (const QFileInfo& f : QDir(st->recordDir()).entryInfoList({"*.dwhm"}, QDir::Files)) bytes += f.size();
      int missing = 0, gapsInTime = 0;
      for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].flags & charts::flow::DepthColumn::Missing) ++missing;
        if (i && recs[i].timeMs - recs[i - 1].timeMs != 1000) ++gapsInTime;
      }
      std::printf("read back %zu records (%s … %s), %d Missing, %d gaps in time; %.1f KB on disk = %.2f MB an hour\n", recs.size(),
                  recs.empty() ? "-" : qPrintable(QDateTime::fromMSecsSinceEpoch(recs.front().timeMs).toString("HH:mm:ss")),
                  recs.empty() ? "-" : qPrintable(QDateTime::fromMSecsSinceEpoch(recs.back().timeMs).toString("HH:mm:ss")), missing, gapsInTime,
                  bytes / 1024.0, recs.empty() ? 0.0 : double(bytes) / double(recs.size()) * 3600 / 1e6);
    }
    std::printf("synced after %lld ms; CPU %.2f s over %.0f s (%.2f %%); best bid/ask compared %d times, worst %d ticks apart; "
                "Missing columns after the first sync: %d\n",
                (long long)syncedAt, cpu, wall.elapsed() / 1000.0, 100 * cpu / (wall.elapsed() / 1000.0), compared, worstGap, missingAfterSync);
    app.exit(0);
  });
  return app.exec();
}
