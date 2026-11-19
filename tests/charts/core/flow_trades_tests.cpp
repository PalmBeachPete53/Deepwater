// Order flow, step A: exact prices in ticks, the aggTrade reader, the aggressor side and the trade tape.
// Expected values are worked out by hand in the comments; Binance messages are real ones captured 2026-11-18.
#include "AllocCount.h"
#include "Check.h"
#include "charts/core/BinanceMsg.h"
#include "charts/core/Ticks.h"
#include "charts/core/Trades.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <cstdlib>
#include <map>
#include <random>

using namespace charts::flow;

namespace {
Decimal dec(const char* s) {
  const char* e = s + std::strlen(s);
  return parseDecimal(s, e);
}
TickScale scale(const char* tick) { return TickScale(dec(tick)); }
Trade tr(int64_t id, int64_t ms = 0) {
  Trade t;
  t.id = id;
  t.timeMs = ms ? ms : id * 10;
  t.ticks = 100 + id;
  t.qty = 1;
  return t;
}
std::vector<int64_t> ids(const TradeTape& tp) {
  std::vector<int64_t> v;
  for (size_t i = 0; i < tp.size(); ++i) v.push_back(tp[i].id);
  return v;
}
bool sameGaps(const TradeTape& tp, std::vector<std::pair<int64_t, int64_t>> want) {
  if (tp.gaps().size() != want.size()) return false;
  for (size_t i = 0; i < want.size(); ++i)
    if (tp.gaps()[i].from != want[i].first || tp.gaps()[i].to != want[i].second) return false;
  return true;
}

const char* kSpotWs = R"({"e":"aggTrade","E":1795011889809,"s":"BTCUSDT","a":4122255954,"p":"63972.09000000","q":"0.00050000","f":6876815826,"l":6876815826,"T":1795011889808,"m":true,"M":true})";
const char* kFutWs = R"({"e":"aggTrade","E":1795011889422,"a":3530974501,"s":"BTCUSDT","p":"63942.30","q":"0.001","nq":"0.001","f":8293135965,"l":8293135965,"T":1795011889272,"m":true,"st":1})";
const char* kSpotRest = R"([{"a":4122255946,"p":"63972.10000000","q":"0.00011000","f":6876815816,"l":6876815817,"T":1795011884438,"m":false,"M":true},{"a":4122255947,"p":"63972.10000000","q":"0.00418000","f":6876815818,"l":6876815819,"T":1795011884567,"m":false,"M":true},{"a":4122255948,"p":"63972.10000000","q":"0.00050000","f":6876815820,"l":6876815820,"T":1795011886115,"m":false,"M":true}])";
const char* kFutRest = R"([{"a":3530974495,"p":"63942.40","q":"0.008","nq":"0.008","f":8293135943,"l":8293135943,"T":1795011886146,"m":false},{"a":3530974496,"p":"63942.30","q":"0.084","nq":"0.084","f":8293135944,"l":8293135947,"T":1795011886652,"m":true},{"a":3530974497,"p":"63942.40","q":"0.016","nq":"0.016","f":8293135948,"l":8293135948,"T":1795011886893,"m":false}])";
}  // namespace

// ------------------------------------------------------------------------------------------------ decimals, ticks

TEST("flow decimal: exact mantissa and exponent, trailing fraction zeros dropped") {
  Decimal d = dec("63951.20");
  CHECK(d.ok); CHECK_EQ(d.mant, int64_t(639512)); CHECK_EQ(d.exp, -1);
  d = dec("63981.93000000");
  CHECK_EQ(d.mant, int64_t(6398193)); CHECK_EQ(d.exp, -2);
  d = dec("0.00000100");                   // 1 × 10^-6
  CHECK_EQ(d.mant, int64_t(1)); CHECK_EQ(d.exp, -6);
  d = dec("100");                          // integer zeros are significant
  CHECK_EQ(d.mant, int64_t(100)); CHECK_EQ(d.exp, 0);
  d = dec("-1.50");
  CHECK_EQ(d.mant, int64_t(-15)); CHECK_EQ(d.exp, -1);
  d = dec("0.0");
  CHECK(d.ok); CHECK_EQ(d.mant, int64_t(0));
  CHECK(!dec("").ok);
  CHECK(!dec("-").ok);
  CHECK(!dec("abc").ok);
  CHECK(!dec("99999999999999999999").ok);  // 20 digits: more than an int64
  const char* s = "1.2.3";
  const char* stop = nullptr;
  d = parseDecimal(s, s + 5, &stop);
  CHECK_EQ(d.mant, int64_t(12)); CHECK_EQ(d.exp, -1); CHECK(stop == s + 3);
}

TEST("flow decimal: toDouble is the double strtod gives (2000 random prices and sizes)") {
  std::mt19937_64 rng(7);
  int bad = 0;
  for (int i = 0; i < 2000; ++i) {
    const int64_t ip = int64_t(rng() % 2000000), fp = int64_t(rng() % 100000000);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%lld.%08lld", (long long)ip, (long long)fp);
    if (dec(buf).toDouble() != std::strtod(buf, nullptr)) ++bad;
  }
  CHECK_EQ(bad, 0);
}

TEST("flow ticks: exchange prices to ticks with integers only") {
  const TickScale t01 = scale("0.01000000");   // BTCUSDT spot
  CHECK(t01.valid());
  CHECK_EQ(t01.ticksOf(dec("63972.09000000")), int64_t(6397209));
  CHECK_EQ(t01.ticksOf(dec("63972.1")), int64_t(6397210));
  const TickScale t1 = scale("0.10");          // BTCUSDT perpetual
  CHECK_EQ(t1.ticksOf(dec("63951.20")), int64_t(639512));
  CHECK_EQ(t1.ticksOf(dec("63951.25")), int64_t(639513));   // between two ticks: nearest, halves away from zero
  CHECK_EQ(t1.ticksOf(dec("63951.24")), int64_t(639512));
  CHECK_EQ(t1.ticksOf(dec("-0.05")), int64_t(-1));
  CHECK_EQ(t1.ticksOf(dec("100")), int64_t(1000));
  const TickScale tiny = scale("0.00000001");
  CHECK_EQ(tiny.ticksOf(dec("0.00001234")), int64_t(1234));
  const TickScale t5 = scale("5");             // a tick above 1
  CHECK_EQ(t5.ticksOf(dec("1235")), int64_t(247));
  CHECK_EQ(t5.ticksOf(dec("1237.5")), int64_t(248));
  // back to prices: the same double as parsing the text
  CHECK(t01.priceOf(6397209) == 63972.09);
  CHECK(t1.priceOf(639512) == 63951.2);
  CHECK_EQ(t01.ticksOf(63972.09), int64_t(6397209));
  CHECK(!scale("0").valid());
}

TEST("flow ticks: groupFloor for level grouping, negatives included") {
  CHECK_EQ(groupFloor(7, 4), int64_t(4));
  CHECK_EQ(groupFloor(8, 4), int64_t(8));
  CHECK_EQ(groupFloor(-1, 4), int64_t(-4));
  CHECK_EQ(groupFloor(-4, 4), int64_t(-4));
  CHECK_EQ(groupFloor(-5, 4), int64_t(-8));
  CHECK_EQ(groupFloor(5, 1), int64_t(5));
}

// ------------------------------------------------------------------------------------------------ aggTrade reader

TEST("flow aggTrade: spot and futures WebSocket messages") {
  AggTrade a;
  CHECK(parseAggTrade(QByteArrayView(kSpotWs), a));
  CHECK_EQ(a.id, int64_t(4122255954));
  CHECK_EQ(a.firstId, int64_t(6876815826));
  CHECK_EQ(a.lastId, int64_t(6876815826));
  CHECK_EQ(a.timeMs, int64_t(1795011889808));
  CHECK_EQ(a.eventMs, int64_t(1795011889809));
  CHECK_EQ(scale("0.01").ticksOf(a.price), int64_t(6397209));
  CHECK(a.qty.toDouble() == 0.0005);
  CHECK(a.buyerMaker);
  CHECK(parseAggTrade(QByteArrayView(kFutWs), a));   // + "nq" and a numeric "st"
  CHECK_EQ(a.id, int64_t(3530974501));
  CHECK_EQ(scale("0.10").ticksOf(a.price), int64_t(639423));
  CHECK(a.qty.toDouble() == 0.001);
  CHECK(a.buyerMaker);
  // the UTF-16 text QWebSocket hands over reads the same
  AggTrade b;
  CHECK(parseAggTrade(QStringView(QString::fromLatin1(kSpotWs)), b));
  CHECK_EQ(b.id, int64_t(4122255954));
  CHECK_EQ(b.price.mant, dec("63972.09").mant);
}

TEST("flow aggTrade: equals Qt JSON field by field (spot and futures REST pages)") {
  for (const char* body : {kSpotRest, kFutRest}) {
    std::vector<AggTrade> v;
    CHECK(parseAggTradesPage(QByteArrayView(body), v));
    const QJsonArray ref = QJsonDocument::fromJson(QByteArray(body)).array();
    CHECK_EQ(int(v.size()), int(ref.size()));
    for (int i = 0; i < ref.size() && i < int(v.size()); ++i) {
      const QJsonObject o = ref[i].toObject();
      CHECK_EQ(v[i].id, int64_t(o["a"].toDouble()));
      CHECK_EQ(v[i].firstId, int64_t(o["f"].toDouble()));
      CHECK_EQ(v[i].lastId, int64_t(o["l"].toDouble()));
      CHECK_EQ(v[i].timeMs, int64_t(o["T"].toDouble()));
      CHECK(v[i].price.toDouble() == o["p"].toString().toDouble());
      CHECK(v[i].qty.toDouble() == o["q"].toString().toDouble());
      CHECK_EQ(v[i].buyerMaker, o["m"].toBool());
      CHECK_EQ(v[i].eventMs, int64_t(0));
    }
  }
  std::vector<AggTrade> v;
  CHECK(parseAggTradesPage(QByteArrayView(" [ ] "), v));
  CHECK(v.empty());
}

TEST("flow aggTrade: refuses what it does not know instead of guessing") {
  AggTrade a;
  CHECK(!parseAggTrade(QByteArrayView(R"({"e":"depthUpdate","E":1,"s":"BTCUSDT","U":1,"u":2,"b":[],"a":[]})"), a));
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1","q":"1","T":1})"), a));                       // no m
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1","q":"1","T":1,"m":true} x)"), a));            // trailing text
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1\"0","q":"1","T":1,"m":true})"), a));      // escaped quote: refused
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1x","q":"1","T":1,"m":true})"), a));             // bad price
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1.5,"p":"1","q":"1","T":1,"m":true})"), a));            // id not whole
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1","q":"1","T":1,"m":true,"x":[1]})"), a));      // nested value
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1","q":"1","T":1,"m":maybe})"), a));
  CHECK(!parseAggTrade(QByteArrayView(R"({"a":1,"p":"1","q":"1","T":1,"m":true)"), a));               // cut off
  CHECK(!parseAggTrade(QByteArrayView(""), a));
  CHECK(parseAggTrade(QByteArrayView(R"( {"a":1,"p":"1","q":"1","T":1,"m":false} )"), a));
  std::vector<AggTrade> v;
  CHECK(!parseAggTradesPage(QByteArrayView(R"([{"a":1,"p":"1","q":"1","T":1,"m":false},{"a":2}])"), v));
  CHECK_EQ(int(v.size()), 1);   // what was read before the bad element stays
  CHECK(!parseAggTradesPage(QByteArrayView(R"({"code":-1121,"msg":"Invalid symbol."})"), v));
}

// ------------------------------------------------------------------------------------------------ side

TEST("flow side: exact sides pass through and are not approximate") {
  SideClassifier c;
  CHECK(c.classify(100, Side::Sell) == Side::Sell);
  CHECK(c.classify(101, Side::Buy) == Side::Buy);
  CHECK(c.lastFrom() == SideFrom::Exact);
  CHECK(!c.approximate());
}

TEST("flow side: tick rule (up = buy, down = sell, equal = previous side, first = unknown)") {
  SideClassifier c;
  const int64_t px[] = {100, 101, 101, 100, 100, 102, 102};
  const Side want[] = {Side::Unknown, Side::Buy, Side::Buy, Side::Sell, Side::Sell, Side::Buy, Side::Buy};
  for (int i = 0; i < 7; ++i) CHECK(c.classify(px[i]) == want[i]);
  CHECK(c.lastFrom() == SideFrom::TickRule);
  CHECK(c.approximate());
  // an exact trade is "the previous trade" of the next one
  SideClassifier m;
  m.classify(100, Side::Sell);
  CHECK(m.classify(100) == Side::Sell);
  CHECK(m.classify(99) == Side::Sell);
  CHECK(m.classify(99) == Side::Sell);
  CHECK(m.classify(100) == Side::Buy);
}

TEST("flow side: quote rule, mid of the spread falls back to the tick rule") {
  SideClassifier c;
  SideClassifier::Quote q{99, 101};          // mid 100
  CHECK(c.classify(101, Side::Unknown, &q) == Side::Buy);    // at the ask
  CHECK(c.classify(102, Side::Unknown, &q) == Side::Buy);    // above it
  CHECK(c.classify(99, Side::Unknown, &q) == Side::Sell);    // at the bid
  CHECK(c.classify(98, Side::Unknown, &q) == Side::Sell);
  CHECK(c.lastFrom() == SideFrom::Quote);
  CHECK(c.classify(100, Side::Unknown, &q) == Side::Buy);    // at the mid: tick rule, up from 98
  CHECK(c.lastFrom() == SideFrom::TickRule);
  SideClassifier::Quote w{99, 102};          // mid 100.5
  CHECK(c.classify(101, Side::Unknown, &w) == Side::Buy);
  CHECK(c.classify(100, Side::Unknown, &w) == Side::Sell);
  SideClassifier::Quote crossed{101, 101};   // no real spread: refused, tick rule
  CHECK(c.classify(101, Side::Unknown, &crossed) == Side::Buy);
  CHECK(c.lastFrom() == SideFrom::TickRule);
  CHECK(c.approximate());
}

// ------------------------------------------------------------------------------------------------ tape

TEST("flow tape: live appends, id jumps leave gaps, duplicates ignored") {
  TradeTape tp(10);
  for (int64_t i = 1; i <= 5; ++i) CHECK(tp.append(tr(i)));
  CHECK(tp.append(tr(8)));
  CHECK(sameGaps(tp, {{6, 7}}));
  CHECK_EQ(tp.missing(), int64_t(2));
  CHECK(!tp.append(tr(7)));   // older than the last: not live
  CHECK(!tp.append(tr(8)));
  const uint64_t g = tp.generation();
  CHECK_EQ(int(tp.merge(std::vector<Trade>{tr(6), tr(7)}.data(), 2)), 2);
  CHECK(ids(tp) == (std::vector<int64_t>{1, 2, 3, 4, 5, 6, 7, 8}));
  CHECK(tp.gaps().empty());
  CHECK(tp.generation() != g);
}

TEST("flow tape: older history goes in front, newest first, within the budget") {
  TradeTape tp(20);
  for (int64_t i = 10; i <= 12; ++i) tp.append(tr(i));
  std::vector<Trade> page{tr(5), tr(6), tr(7), tr(8), tr(9)};
  CHECK_EQ(int(tp.merge(page.data(), page.size(), 3)), 3);   // budget 3: 7, 8, 9
  CHECK(ids(tp) == (std::vector<int64_t>{7, 8, 9, 10, 11, 12}));
  std::vector<Trade> older{tr(1), tr(2)};
  CHECK_EQ(int(tp.merge(older.data(), older.size())), 2);
  CHECK(ids(tp) == (std::vector<int64_t>{1, 2, 7, 8, 9, 10, 11, 12}));
  CHECK(sameGaps(tp, {{3, 6}}));
  // one page mixing older, gap and newer trades
  std::vector<Trade> mix{tr(0), tr(3), tr(4), tr(5), tr(6), tr(13)};
  CHECK_EQ(int(tp.merge(mix.data(), mix.size())), 6);
  CHECK(ids(tp) == (std::vector<int64_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}));
  CHECK(tp.gaps().empty());
}

TEST("flow tape: gap filling by runs, partial fills split or shrink gaps") {
  TradeTape tp(20);
  for (int64_t i : {1, 2, 5, 6, 9, 10}) tp.append(tr(i));
  CHECK(sameGaps(tp, {{3, 4}, {7, 8}}));
  std::vector<Trade> p1{tr(4)};
  tp.merge(p1.data(), 1);
  CHECK(sameGaps(tp, {{3, 3}, {7, 8}}));
  std::vector<Trade> p2{tr(3), tr(3), tr(7), tr(8), tr(9)};   // duplicate 3, known 9
  CHECK_EQ(int(tp.merge(p2.data(), p2.size())), 3);
  CHECK(ids(tp) == (std::vector<int64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
  CHECK(tp.gaps().empty());
  TradeTape sp(20);
  sp.append(tr(1));
  sp.append(tr(10));
  std::vector<Trade> mid{tr(5)};
  sp.merge(mid.data(), 1);
  CHECK(sameGaps(sp, {{2, 4}, {6, 9}}));
  CHECK(sp.forgetGap(2));
  CHECK(sameGaps(sp, {{6, 9}}));
}

TEST("flow tape: a full tape drops its oldest trades, gaps before the front go with them") {
  TradeTape tp(4);
  for (int64_t i : {1, 2, 5, 6}) tp.append(tr(i));
  CHECK(sameGaps(tp, {{3, 4}}));
  tp.append(tr(7));
  CHECK(ids(tp) == (std::vector<int64_t>{2, 5, 6, 7}));
  tp.append(tr(8));
  CHECK(ids(tp) == (std::vector<int64_t>{5, 6, 7, 8}));
  CHECK(tp.gaps().empty());
  CHECK_EQ(int(tp.dropped()), 2);
  // no room for history when full
  std::vector<Trade> old{tr(4)};
  CHECK_EQ(int(tp.merge(old.data(), 1)), 0);
  // a gap filled in a full tape: the oldest trade makes room
  TradeTape f(5);
  for (int64_t i : {1, 2, 4, 5, 6}) f.append(tr(i));
  std::vector<Trade> three{tr(3)};
  CHECK_EQ(int(f.merge(three.data(), 1)), 1);
  CHECK(ids(f) == (std::vector<int64_t>{2, 3, 4, 5, 6}));
  // an empty tape keeps the newest of a page within the budget
  TradeTape e(10);
  std::vector<Trade> page{tr(1), tr(2), tr(3), tr(4), tr(5)};
  CHECK_EQ(int(e.merge(page.data(), page.size(), 2)), 2);
  CHECK(ids(e) == (std::vector<int64_t>{4, 5}));
}

TEST("flow tape: lowerBound by id and by time") {
  TradeTape tp(10);
  for (int64_t i : {3, 4, 7, 9}) tp.append(tr(i, i * 100));
  CHECK_EQ(int(tp.lowerBound(0)), 0);
  CHECK_EQ(int(tp.lowerBound(4)), 1);
  CHECK_EQ(int(tp.lowerBound(5)), 2);
  CHECK_EQ(int(tp.lowerBound(10)), 4);
  CHECK_EQ(int(tp.lowerBoundTime(700)), 2);
  CHECK_EQ(int(tp.lowerBoundTime(701)), 3);
}

TEST("flow tape: random order of pages and live jumps always ends complete and sorted") {
  std::mt19937 rng(11);
  for (int round = 0; round < 200; ++round) {
    const int N = 50 + int(rng() % 300);
    TradeTape tp(size_t(N) + 10);
    std::vector<int64_t> all(N);
    for (int i = 0; i < N; ++i) all[i] = i + 1;
    // live: an ascending subset ending with N
    std::vector<int64_t> missing;
    for (int64_t id : all) {
      if (id != N && rng() % 3 == 0) missing.push_back(id);
      else tp.append(tr(id));
    }
    // the rest in random pages of ascending ids, in random page order
    std::shuffle(missing.begin(), missing.end(), rng);
    while (!missing.empty()) {
      const size_t k = std::min<size_t>(missing.size(), 1 + rng() % 8);
      std::vector<int64_t> part(missing.end() - ptrdiff_t(k), missing.end());
      missing.resize(missing.size() - k);
      std::sort(part.begin(), part.end());
      std::vector<Trade> page;
      for (int64_t id : part) page.push_back(tr(id));
      if (rng() % 4 == 0) page.push_back(tr(part.front()));   // a duplicate now and then
      std::sort(page.begin(), page.end(), [](const Trade& a, const Trade& b) { return a.id < b.id; });
      tp.merge(page.data(), page.size());
    }
    CHECK(ids(tp) == all);
    CHECK(tp.gaps().empty());
  }
}

TEST("flow hot path allocates nothing: parse + classify + append, 10000 live trades") {
  const QString msg = QString::fromLatin1(kFutWs);   // made before counting
  const TickScale sc = scale("0.10");
  TradeTape tp(20000);
  SideClassifier c;
  int64_t sumTicks = 0;
  alloccount::Scope s;
  for (int i = 0; i < 10000; ++i) {
    AggTrade a;
    if (!parseAggTrade(QStringView(msg), a)) break;
    Trade t;
    t.id = a.id + i;
    t.timeMs = a.timeMs;
    t.ticks = sc.ticksOf(a.price);
    t.qty = a.qty.toDouble();
    t.side = c.classify(t.ticks, a.buyerMaker ? Side::Sell : Side::Buy);
    tp.append(t);
    sumTicks += t.ticks;
  }
  const size_t n = s.count();
  CHECK_EQ(int(n), 0);
  CHECK_EQ(int(tp.size()), 10000);
  CHECK_EQ(sumTicks, int64_t(639423) * 10000);
}
