// Order flow, step A: the shared trade stream (src/charts/data/TradeStream) against a fake exchange — no network.
// The fake holds trades with consecutive ids, serves REST pages like Binance (fromId, or the latest), and lets the
// test push live trades, drop the socket or answer 429 / 400.
#include "Check.h"
#include "charts/data/ExchangeFeed.h"
#include "charts/data/TradeStream.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

using namespace charts::data;
using charts::flow::AggTrade;

namespace {

struct FakeExchange {
  int64_t lastId = 0;                 // trades 1..lastId exist
  int64_t oldestServed = 1;           // REST refuses (400) a fromId below this (Binance futures: last 48 h only)
  ITradeSink* sink = nullptr;
  int opens = 0, made = 0;
  std::vector<int64_t> fetches;       // fromId of every REST call (-1 = latest)
  QList<int> failNext;                // statuses to answer the next REST calls with
  QString tick = "0.10";

  AggTrade trade(int64_t id) const {
    AggTrade a;
    a.id = a.firstId = a.lastId = id;
    a.timeMs = 1795000000000 + id * 10;
    const QByteArray p = QByteArray::number(60000.0 + double(id % 50) / 10, 'f', 1);
    a.price = charts::flow::parseDecimal(p.constData(), p.constData() + p.size());
    a.qty = charts::flow::Decimal{int64_t(1 + id % 7), -3, true};
    a.buyerMaker = id % 2 == 0;
    return a;
  }
  void live(int64_t id) {             // the exchange trades id (and everything before it); the socket sends only id
    lastId = std::max(lastId, id);
    if (sink) sink->onTrade(trade(id));
  }
  void drop() {
    if (!sink) return;
    ITradeSink* s = sink;
    sink = nullptr;
    s->onFeedState(FeedState::Closed, "test drop");
  }
};
FakeExchange* g_ex = nullptr;

class FakeFeed : public IExchangeFeed {
public:
  void openTrades(const FlowMarket&, ITradeSink* sink) override {
    ++g_ex->opens;
    g_ex->sink = nullptr;
    sink->onFeedState(FeedState::Connecting, {});
    QTimer::singleShot(0, [sink, alive = alive_] {
      if (!*alive) return;
      g_ex->sink = sink;
      sink->onFeedState(FeedState::Open, {});
    });
  }
  void closeTrades() override { g_ex->sink = nullptr; }
  void fetchTrades(const FlowMarket&, qint64 fromId, int limit, TradesCb cb) override {
    g_ex->fetches.push_back(fromId);
    QTimer::singleShot(1, [fromId, limit, cb, alive = alive_] {
      if (!*alive) return;
      if (!g_ex->failNext.isEmpty()) {
        const int st = g_ex->failNext.takeFirst();
        cb(Result<std::vector<AggTrade>>::failure(Error{QString("fake HTTP %1").arg(st), st, true, {}}));
        return;
      }
      if (fromId >= 0 && fromId < g_ex->oldestServed) {
        cb(Result<std::vector<AggTrade>>::failure(Error{"fake HTTP 400", 400, true, {}}));
        return;
      }
      std::vector<AggTrade> page;
      const int64_t from = fromId >= 0 ? fromId : std::max<int64_t>(1, g_ex->lastId - limit + 1);
      for (int64_t id = from; id < from + limit && id <= g_ex->lastId; ++id) page.push_back(g_ex->trade(id));
      cb(Result<std::vector<AggTrade>>::success(page));
    });
  }
  void fetchTickSize(const FlowMarket&, std::function<void(const Result<charts::flow::Decimal>&)> cb) override {
    QTimer::singleShot(0, [cb, alive = alive_] {
      if (!*alive) return;
      const QByteArray t = g_ex->tick.toLatin1();
      cb(Result<charts::flow::Decimal>::success(charts::flow::parseDecimal(t.constData(), t.constData() + t.size())));
    });
  }
  int historyPaceMs(const FlowMarket&) const override { return 0; }
  ~FakeFeed() override { *alive_ = false; }

private:
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

// Runs the event loop until pred() or maxMs.
bool spin(const std::function<bool()>& pred, int maxMs = 3000) {
  QElapsedTimer t;
  t.start();
  while (!pred()) {
    if (t.elapsed() > maxMs) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return true;
}

struct Setup {
  FakeExchange ex;
  Setup(int64_t lastId) {
    ex.lastId = lastId;
    g_ex = &ex;
    testing::setFeedFactory([](const FlowMarket&) {
      ++g_ex->made;
      return std::unique_ptr<IExchangeFeed>(new FakeFeed);
    });
    TradeStream::setTimingForTests(20, 0, 50);
  }
  ~Setup() {
    QCoreApplication::processEvents();   // let disposed streams go
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    testing::setFeedFactory(nullptr);
    TradeStream::setTimingForTests(0, 0, 0);
    g_ex = nullptr;
  }
};

FlowMarket perp() { return *FlowMarket::fromChartSymbol("BINANCE:BTCUSDT.P"); }

bool sameGapCount(const charts::flow::TradeTape& tp, size_t n) { return tp.gaps().size() == n; }

bool contiguous(const charts::flow::TradeTape& tp, int64_t from, int64_t to) {
  if (int64_t(tp.size()) != to - from + 1) return false;
  for (size_t i = 0; i < tp.size(); ++i)
    if (tp[i].id != from + int64_t(i)) return false;
  return tp.gaps().empty();
}

}  // namespace

TEST("tradestream: chart symbols, Binance URLs and paths") {
  const auto spot = FlowMarket::fromChartSymbol("BINANCE:BTCUSDT");
  CHECK(spot && spot->venue == FlowMarket::BinanceSpot && spot->symbol == "BTCUSDT");
  const auto fut = FlowMarket::fromChartSymbol("binance:ethusdt.p");
  CHECK(fut && fut->venue == FlowMarket::BinanceUsdm && fut->symbol == "ETHUSDT");
  CHECK(FlowMarket::fromChartSymbol("BTCUSDT"));
  CHECK(!FlowMarket::fromChartSymbol("NASDAQ:AAPL"));
  CHECK(!FlowMarket::fromChartSymbol("BINANCE:BTC/USDT"));
  CHECK_EQ(spot->key(), QString("binance-spot:BTCUSDT"));
  CHECK_EQ(fut->key(), QString("binance-usdm:ETHUSDT"));
  CHECK_EQ(binanceTradeSocketUrls(*fut), QStringList{"wss://fstream.binance.com/market/ws/ethusdt@aggTrade"});
  CHECK_EQ(binanceTradeSocketUrls(*spot).value(0), QString("wss://stream.binance.com:9443/ws/btcusdt@aggTrade"));
  CHECK_EQ(binanceTradeSocketUrls(*spot).value(1), QString("wss://data-stream.binance.vision/ws/btcusdt@aggTrade"));
  CHECK_EQ(binanceRestBases(*fut), QStringList{"https://fapi.binance.com/fapi/v1"});
  CHECK_EQ(binanceAggTradesPath(*spot, -1, 1000), QString("/aggTrades?symbol=BTCUSDT&limit=1000"));
  CHECK_EQ(binanceAggTradesPath(*spot, 42, 5000), QString("/aggTrades?symbol=BTCUSDT&limit=1000&fromId=42"));
}

TEST("tradestream: tick size, socket, then history newest first up to the budget") {
  Setup s(5000);
  auto st = TradeStream::acquire(perp(), 3000);
  CHECK(spin([&] { return st->status().historyDone; }));
  CHECK(st->ready());
  CHECK_EQ(st->ticks().ticksOf(charts::flow::Decimal{600001, -1, true}), int64_t(600001));
  CHECK_EQ(s.ex.opens, 1);
  CHECK(st->status().feed == FeedState::Open);
  // latest page (4001..5000), then 3001.., then 2001..: 3000 trades
  CHECK(s.ex.fetches == (std::vector<int64_t>{-1, 3001, 2001}));
  CHECK(contiguous(st->tape(), 2001, 5000));
  // sides from Binance's m: even ids are buyer-maker = aggressive sells
  CHECK(st->tape()[0].side == charts::flow::Side::Buy);    // 2001: odd
  CHECK(st->tape()[1].side == charts::flow::Side::Sell);   // 2002
  CHECK(!st->status().approximate);
  CHECK(st->status().error.isEmpty());
}

TEST("tradestream: live trades append; a jump in ids is filled from REST") {
  Setup s(1000);
  auto st = TradeStream::acquire(perp(), 1000);
  CHECK(spin([&] { return st->status().historyDone && s.ex.sink; }));
  int appended = 0, history = 0;
  QObject::connect(st.get(), &TradeStream::appended, [&] { ++appended; });
  QObject::connect(st.get(), &TradeStream::historyChanged, [&] { ++history; });
  s.ex.live(1001);
  s.ex.live(1002);
  s.ex.live(1010);   // 1003..1009 never reached the socket
  CHECK_EQ(appended, 3);
  CHECK_EQ(int(st->tape().gaps().size()), 1);
  CHECK(spin([&] { return st->tape().gaps().empty(); }));
  CHECK_EQ(s.ex.fetches.back(), int64_t(1003));
  CHECK(contiguous(st->tape(), 1, 1010));
  CHECK(history >= 1);
  CHECK_EQ(int(st->status().liveTrades), 3);
  // two holes at once: the newest is filled first (recent trades matter most)
  const size_t before = s.ex.fetches.size();
  s.ex.live(1015);
  s.ex.live(1020);
  CHECK(sameGapCount(st->tape(), 2));
  CHECK(spin([&] { return st->tape().gaps().empty(); }));
  CHECK(s.ex.fetches.size() >= before + 2);
  CHECK_EQ(s.ex.fetches[before], int64_t(1016));
  CHECK_EQ(s.ex.fetches[before + 1], int64_t(1011));
  CHECK(contiguous(st->tape(), 1, 1020));
}

TEST("tradestream: a dropped socket reconnects after the backoff and the missed trades are filled") {
  Setup s(1000);
  auto st = TradeStream::acquire(perp(), 1000);
  CHECK(spin([&] { return st->status().historyDone && s.ex.sink; }));
  s.ex.drop();
  CHECK(st->status().feed == FeedState::Closed);
  CHECK(st->status().error.startsWith("live:"));
  s.ex.lastId = 1100;   // the market went on meanwhile
  CHECK(spin([&] { return s.ex.sink != nullptr; }));
  CHECK_EQ(s.ex.opens, 2);
  CHECK_EQ(st->status().reconnects, 1);
  CHECK(st->status().error.isEmpty());
  s.ex.live(1101);
  CHECK(spin([&] { return st->tape().gaps().empty() && st->tape().size() == 1101; }));
  CHECK(contiguous(st->tape(), 1, 1101));
  // backoff grows while connections keep failing: 20 ms, 40 ms, ...
  QElapsedTimer t;
  t.start();
  s.ex.drop();
  CHECK(spin([&] { return s.ex.sink != nullptr; }));
  CHECK(t.elapsed() >= 35);
}

TEST("tradestream: the planned renewal before Binance's 24 h cut is not a lost connection") {
  Setup s(10);
  TradeStream::setTimingForTests(20, 80, 50);
  auto st = TradeStream::acquire(perp(), 1000);
  CHECK(spin([&] { return s.ex.opens >= 3; }, 2000));
  CHECK_EQ(st->status().reconnects, 0);
}

TEST("tradestream: HTTP 429 pauses the history, then it goes on") {
  Setup s(3000);
  s.ex.failNext = {429};
  auto st = TradeStream::acquire(perp(), 2000);
  CHECK(spin([&] { return st->status().error.contains("429"); }));
  CHECK(spin([&] { return st->status().historyDone; }));
  CHECK(contiguous(st->tape(), 1001, 3000));
  CHECK(st->status().error.isEmpty());
}

TEST("tradestream: history ends where the exchange stops serving (futures: 48 h)") {
  Setup s(5000);
  s.ex.oldestServed = 3500;
  auto st = TradeStream::acquire(perp(), 4000);
  CHECK(spin([&] { return st->status().historyDone; }));
  CHECK(contiguous(st->tape(), 4001, 5000));   // fromId 3001 refused: no older history
}

TEST("tradestream: one stream per market, shared; budget raised by a bigger user; gone with its last user") {
  Setup s(5000);
  auto a = TradeStream::acquire(perp(), 1000);
  auto b = TradeStream::acquire(perp(), 1000);
  CHECK(a.get() == b.get());
  CHECK_EQ(s.ex.made, 1);
  auto spot = TradeStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT"), 1000);
  CHECK(spot.get() != a.get());
  CHECK_EQ(s.ex.made, 2);
  CHECK(spin([&] { return a->status().historyDone; }));
  CHECK_EQ(int(a->tape().size()), 1000);
  auto big = TradeStream::acquire(perp(), 3000);
  CHECK(big.get() == a.get());
  CHECK_EQ(a->status().budget, 3000);
  CHECK(spin([&] { return a->status().historyDone && a->tape().size() == 3000; }));
  CHECK(contiguous(a->tape(), 2001, 5000));
  a.reset();
  b.reset();
  big.reset();
  spot.reset();
  CHECK(s.ex.sink == nullptr || s.ex.opens >= 2);
  auto again = TradeStream::acquire(perp(), 1000);
  CHECK_EQ(s.ex.made, 3);   // a new stream: the old one was released
}

// Audit D9: a long gap (laptop asleep) is filled from its newest end, only as far as the budget, then let go.
TEST("tradestream: a gap larger than the budget is filled newest-first, the rest accepted as a hole") {
  Setup s(1000);
  auto st = TradeStream::acquire(perp(), 1000);
  CHECK(spin([&] { return st->status().historyDone && s.ex.sink; }));
  const size_t before = s.ex.fetches.size();
  s.ex.live(50000);   // 1001..49999 missed
  CHECK(spin([&] { return st->tape().gaps().empty(); }));
  CHECK(s.ex.fetches.size() > before);
  CHECK_EQ(s.ex.fetches[before], int64_t(49001));   // the newest page of the gap first
  CHECK(s.ex.fetches.size() - before <= 2);          // the budget is reached after one page: no walk over 49 000 ids
  CHECK_EQ(st->tape().back().id, int64_t(50000));
  // the 1000 trades before the hole stay, the newest 1000 are contiguous
  CHECK_EQ(int(st->tape().size()), 2000);
  CHECK_EQ(st->tape()[1000].id, int64_t(49001));
}

TEST("tradestream: a gap the exchange refuses (HTTP 400) is forgotten, not asked forever") {
  Setup s(1000);
  auto st = TradeStream::acquire(perp(), 1000);
  CHECK(spin([&] { return st->status().historyDone && s.ex.sink; }));
  s.ex.oldestServed = 1500;   // the exchange no longer serves 1001..1499
  s.ex.live(1010);
  CHECK(spin([&] { return st->tape().gaps().empty(); }));
  const size_t n = s.ex.fetches.size();
  spin([] { return false; }, 200);
  CHECK_EQ(s.ex.fetches.size(), n);   // no retry loop
  s.ex.live(1011);
  CHECK_EQ(st->tape().back().id, int64_t(1011));
}
