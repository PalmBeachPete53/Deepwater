// Live check of the trade stream against Binance itself (not a unit test: needs the network).
//   DW_PROFILE=<scratch> charts_trades_live BINANCE:BTCUSDT.P [seconds=120] [budget=300000]
// Prints the stream's state every 10 s, then compares every closed 1-minute candle the tape fully covers with
// Binance's own 1m kline: volume, taker-buy volume (= our aggressive buys), high and low, and the id continuity.
#include "charts/core/BigTrades.h"
#include "charts/core/Footprint.h"
#include "charts/data/TradeStream.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>
#include <sys/resource.h>

using namespace charts::data;

namespace {
double cpuSeconds() {
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
  return u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1e6 + u.ru_stime.tv_sec + u.ru_stime.tv_usec / 1e6;
}
const char* feedName(FeedState s) { return s == FeedState::Open ? "open" : s == FeedState::Connecting ? "connecting" : "closed"; }
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (qEnvironmentVariableIsEmpty("DW_PROFILE")) { std::fprintf(stderr, "set DW_PROFILE to a scratch folder\n"); return 2; }
  const QString sym = argc > 1 ? argv[1] : "BINANCE:BTCUSDT.P";
  const int seconds = argc > 2 ? atoi(argv[2]) : 120;
  const int budget = argc > 3 ? atoi(argv[3]) : TradeStream::kDefaultBudget;
  const auto m = FlowMarket::fromChartSymbol(sym);
  if (!m) { std::fprintf(stderr, "not a Binance symbol: %s\n", qPrintable(sym)); return 2; }

  QElapsedTimer wall;
  wall.start();
  const double cpu0 = cpuSeconds();
  auto st = TradeStream::acquire(*m, budget);
  qint64 readyAt = -1, historyAt = -1;
  QObject::connect(st.get(), &TradeStream::becameReady, [&] { readyAt = wall.elapsed(); });
  QObject::connect(st.get(), &TradeStream::statusChanged, [&] {
    if (st->status().historyDone && historyAt < 0) historyAt = wall.elapsed();
  });
  qint64 lastLive = 0;
  QTimer tick;
  QObject::connect(&tick, &QTimer::timeout, [&] {
    const auto& s = st->status();
    const auto& tp = st->tape();
    std::printf("%5llds feed=%s tape=%zu ids=%lld..%lld gaps=%zu missing=%lld live=%lld (+%.1f/s) history=%s reconnects=%d err=%s\n",
                (long long)(wall.elapsed() / 1000), feedName(s.feed), tp.size(), (long long)tp.firstId(), (long long)tp.lastId(),
                tp.gaps().size(), (long long)tp.missing(), (long long)s.liveTrades, (s.liveTrades - lastLive) / 10.0,
                s.historyDone ? "done" : "loading", s.reconnects, qPrintable(s.error));
    std::fflush(stdout);
    lastLive = s.liveTrades;
  });
  tick.start(10000);
  QTimer::singleShot(seconds * 1000, [&] {
    tick.stop();
    const double cpu = cpuSeconds() - cpu0;
    const auto& tp = st->tape();
    std::printf("\nready after %lld ms, history %s after %lld ms, CPU %.2f s over %.0f s wall (%.2f %%)\n", (long long)readyAt,
                st->status().historyDone ? "done" : "NOT done", (long long)historyAt, cpu, wall.elapsed() / 1000.0,
                100 * cpu / (wall.elapsed() / 1000.0));
    // id continuity
    qint64 jumps = 0;
    for (size_t i = 1; i < tp.size(); ++i)
      if (tp[i].id != tp[i - 1].id + 1) ++jumps;
    std::printf("tape %zu trades, %lld id jumps, %zu gaps listed, %llu dropped\n", tp.size(), (long long)jumps, tp.gaps().size(),
                (unsigned long long)tp.dropped());
    if (tp.size() < 2) { app.exit(1); return; }
    // closed minutes fully inside the tape, built by the footprint (src/charts/core/Footprint): OHLC from the trades
    struct Min { double vol = 0, buy = 0; int64_t hi = INT64_MIN, lo = INT64_MAX; };
    std::map<qint64, Min> mins;
    const qint64 firstMin = (tp.front().timeMs / 60000 + 1) * 60000, lastMin = (tp.back().timeMs / 60000) * 60000;   // excl. forming
    charts::flow::FootprintBook fp(charts::flow::FootprintBook::fixedBuckets(60000));
    for (size_t i = 0; i < tp.size(); ++i) fp.add(tp[i]);
    charts::flow::FootprintView view;
    for (const auto& c : fp.candles()) {
      if (c.startMs < firstMin || c.startMs >= lastMin) continue;
      charts::flow::analyze(c, {}, view);
      double rowVol = 0;
      for (const auto& r : view.rows) rowVol += r.volume();
      if (std::fabs(rowVol - c.volume()) > 1e-9 * c.volume()) std::printf("  rows of %lld don't add up to the candle\n", (long long)c.startMs);
      mins[c.startMs] = Min{c.volume(), c.askVolume, c.high, c.low};
    }
    // Big Trades on the whole tape (step C1): aggregates, auto threshold, the biggest orders found
    {
      charts::flow::Aggregator ag;
      charts::flow::Detector det;
      std::vector<charts::flow::AggregatedTrade> big;
      size_t aggs = 0, merged = 0;
      charts::flow::Detector::Verdict last;
      auto judge = [&](const charts::flow::AggregatedTrade& a) {
        ++aggs;
        if (a.executions > 1) ++merged;
        last = det.judge(a);
        if (last.big) big.push_back(a);
      };
      for (size_t i = 0; i < tp.size(); ++i)
        if (auto c = ag.add(tp[i])) judge(*c);
      if (auto c = ag.flush(INT64_MAX)) judge(*c);
      std::printf("big trades: %zu trades → %zu orders (%zu of several executions); last threshold %.4f "
                  "(p99 %.4f × volatility %.2f, %s); %zu big orders (%.2f %%)\n",
                  tp.size(), aggs, merged, last.threshold, last.percentileValue, last.factor, last.warm ? "warm" : "warming up",
                  big.size(), 100.0 * double(big.size()) / double(std::max<size_t>(aggs, 1)));
      std::sort(big.begin(), big.end(), [](const auto& a, const auto& b) { return a.volume > b.volume; });
      for (size_t k = 0; k < std::min<size_t>(5, big.size()); ++k) {
        const auto& a = big[k];
        std::printf("  %s %s %.4f @ %.2f (%.2f – %.2f), %d executions\n",
                    qPrintable(QDateTime::fromMSecsSinceEpoch(a.startMs).toString("HH:mm:ss.zzz")), a.side == charts::flow::Side::Buy ? "BUY " : "SELL",
                    a.volume, a.vwapTicks * st->ticks().step, st->ticks().priceOf(a.minTicks), st->ticks().priceOf(a.maxTicks), a.executions);
      }
    }
    if (mins.empty()) { std::printf("no closed minute inside the tape yet\n"); app.exit(0); return; }
    const bool fut = m->venue == FlowMarket::BinanceUsdm;
    const QString url = (fut ? QString("https://fapi.binance.com/fapi/v1/klines") : QString("https://api.binance.com/api/v3/klines")) +
                        "?symbol=" + m->symbol + "&interval=1m&startTime=" + QString::number(mins.begin()->first) +
                        "&limit=" + QString::number(std::min<size_t>(mins.size(), 1000));
    getJson(url, {}, [&, mins](const Result<QJsonDocument>& r) {
      if (!r.ok()) { std::printf("klines: %s\n", qPrintable(r.error->message)); app.exit(1); return; }
      int n = 0, okVol = 0, okBuy = 0, okHl = 0;
      double worstVol = 0, worstBuy = 0, sumOurs = 0, sumKl = 0;
      std::vector<double> diffs, vols;   // per compared minute, in time order
      for (const QJsonValue& v : r.value.array()) {
        const QJsonArray k = v.toArray();
        const qint64 t = qint64(k.at(0).toDouble());
        const auto it = mins.find(t);
        if (it == mins.end()) continue;
        ++n;
        const double vol = k.at(5).toString().toDouble(), buy = k.at(9).toString().toDouble();
        const int64_t hi = st->ticks().ticksOf(k.at(2).toString().toDouble()), lo = st->ticks().ticksOf(k.at(3).toString().toDouble());
        const double dv = std::fabs(it->second.vol - vol) / std::max(vol, 1e-12), db = std::fabs(it->second.buy - buy) / std::max(buy, 1e-12);
        diffs.push_back(it->second.vol - vol);
        vols.push_back(vol);
        sumOurs += it->second.vol;
        sumKl += vol;
        worstVol = std::max(worstVol, dv);
        worstBuy = std::max(worstBuy, db);
        if (dv < 1e-9) ++okVol;
        if (db < 1e-9) ++okBuy;
        if (hi == it->second.hi && lo == it->second.lo) ++okHl;
      }
      std::printf("closed 1m candles compared with Binance klines: %d\n  volume equal %d/%d (worst rel. diff %.2e)\n"
                  "  taker-buy volume equal %d/%d (worst %.2e)\n  high/low equal %d/%d\n",
                  n, okVol, n, worstVol, okBuy, n, worstBuy, okHl, n);
      // a trade Binance's aggTrade puts in one minute and its kline in the next: the two differences cancel
      int shifted = 0;
      for (size_t i = 0; i < diffs.size(); ++i) {
        if (std::fabs(diffs[i]) <= 1e-9 * vols[i]) continue;
        const bool prev = i > 0 && std::fabs(diffs[i] + diffs[i - 1]) <= 1e-9 * vols[i];
        const bool next = i + 1 < diffs.size() && std::fabs(diffs[i] + diffs[i + 1]) <= 1e-9 * vols[i];
        if (prev || next) ++shifted;
      }
      if (qEnvironmentVariableIsSet("TRADES_LIVE_VERBOSE")) {
        double cum = 0;
        for (size_t i = 0; i < diffs.size(); ++i) {
          cum += diffs[i];
          if (std::fabs(diffs[i]) > 1e-9 * vols[i]) std::printf("    minute %2zu diff %+.4f cumulative %+.4f (kline vol %.3f)\n", i, diffs[i], cum, vols[i]);
        }
      }
      std::printf("  volume over all compared minutes: ours %.6f, klines %.6f (rel. diff %.2e)\n"
                  "  unequal minutes cancelled by a neighbour minute: %d of %d\n",
                  sumOurs, sumKl, std::fabs(sumOurs - sumKl) / std::max(sumKl, 1e-12), shifted, n - okVol);
      app.exit(okVol == n && okBuy == n && okHl == n ? 0 : 1);
    }, GetOpts{0, false});
  });
  return app.exec();
}
