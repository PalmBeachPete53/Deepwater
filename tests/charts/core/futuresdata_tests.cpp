// Order flow, step E2 (pure part): reading Binance's futures statistics, record files, lining up with bars, brackets.
// The JSON below is what Binance answered on 2026-11-18 (curl, public endpoints).
#include "Check.h"
#include "charts/core/FuturesData.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>

using namespace charts::flow;

TEST("futures data: open interest, ratio and funding pages as Binance sends them") {
  std::vector<OiPoint> oi;
  CHECK(parseOpenInterestHist(R"([{"symbol":"BTCUSDT","sumOpenInterest":"95624.78900000","sumOpenInterestValue":"6006024899.79767600","CMCCirculatingSupply":"20113565.00000000","timestamp":1795029600000},{"symbol":"BTCUSDT","sumOpenInterest":"95690.91100000","sumOpenInterestValue":"6013008321.48988600","CMCCirculatingSupply":"20113565.00000000","timestamp":1795029900000}])", oi));
  CHECK_EQ(int(oi.size()), 2);
  CHECK_EQ(oi[0].timeMs, int64_t(1795029600000));
  CHECK_EQ(oi[1].oi, 95690.911);
  CHECK_EQ(oi[1].value, 6013008321.489886);
  std::vector<RatioPoint> r;
  CHECK(parseLongShortRatio(R"([{"symbol":"BTCUSDT","longAccount":"0.7727","longShortRatio":"3.3995","shortAccount":"0.2273","timestamp":1795029600000}])", r));
  CHECK_EQ(int(r.size()), 1);
  CHECK_EQ(r[0].ratio, 3.3995);
  CHECK_EQ(r[0].longShare, 0.7727);
  std::vector<FundingPoint> f;
  CHECK(parseFundingRates(R"([{"symbol":"BTCUSDT","fundingTime":1795017600000,"fundingRate":"0.00005963","markPrice":"63111.57293103","rateType":"Regular"},{"symbol":"BTCUSDT","fundingTime":1794988800001,"fundingRate":"0.00003132","markPrice":"64853.91142754","rateType":"Regular"}])", f));
  CHECK_EQ(int(f.size()), 2);
  CHECK_EQ(f[0].timeMs, int64_t(1794988800001));   // sorted by time
  CHECK_EQ(f[1].rate, 0.00005963);
  // not an array (an error object): refused, nothing kept
  CHECK(!parseOpenInterestHist(R"({"msg":"parameter 'startTime' is invalid.","code":-1130})", oi));
  CHECK(parseOpenInterestHist("[]", oi));
  CHECK(oi.empty());
}

TEST("futures data: a liquidation order from the forceOrder stream") {
  LiqEvent e;
  QString sym;
  CHECK(parseForceOrder(R"({"e":"forceOrder","E":1795030327233,"o":{"s":"BTCUSDT","S":"SELL","o":"LIMIT","f":"IOC","q":"0.367","p":"62520.00","ap":"62766.70","X":"FILLED","l":"0.367","z":"0.367","T":1795030327122,"ps":"BTCUSDT","st":1}})", e, &sym));
  CHECK_EQ(sym, QString("BTCUSDT"));
  CHECK_EQ(e.timeMs, int64_t(1795030327122));
  CHECK_EQ(e.price, 62766.70);   // the average fill, not the order's limit
  CHECK_EQ(e.qty, 0.367);
  CHECK_EQ(e.longLiquidated, 1);  // a SELL closes a long
  CHECK(parseForceOrder(R"({"e":"forceOrder","o":{"s":"ETHUSDT","S":"BUY","p":"1800","ap":"0","z":"2","T":5}})", e));
  CHECK_EQ(e.longLiquidated, 0);
  CHECK_EQ(e.price, 1800.0);      // no average yet: the order price
  CHECK(!parseForceOrder(R"({"e":"forceOrder","o":{"s":"ETHUSDT","S":"BUY","p":"1800","ap":"0","z":"0","T":5}})", e));   // nothing filled
  CHECK(!parseForceOrder(R"({"e":"aggTrade","o":{}})", e));
  CHECK(!parseForceOrder("not json", e));
}

TEST("futures data: merging pages by time (new after old, replaced, holes filled)") {
  auto pts = [](std::initializer_list<std::pair<int64_t, double>> l) {
    std::vector<OiPoint> v;
    for (auto [t, x] : l) v.push_back({t, x, 0});
    return v;
  };
  std::vector<OiPoint> a = pts({{10, 1}, {20, 2}});
  CHECK_EQ(int(mergeByTime(a, pts({{30, 3}, {40, 4}}))), 2);
  CHECK_EQ(int(a.size()), 4);
  CHECK_EQ(int(mergeByTime(a, pts({{20, 9}, {25, 5}}))), 1);   // 20 replaced, 25 new
  CHECK_EQ(int(a.size()), 5);
  CHECK_EQ(a[1].oi, 9.0);
  CHECK_EQ(a[2].timeMs, int64_t(25));
  CHECK_EQ(int(mergeByTime(a, pts({{5, 0}}))), 1);              // before everything
  CHECK_EQ(a.front().timeMs, int64_t(5));
  bool sorted = true;
  for (size_t i = 1; i < a.size(); ++i) sorted = sorted && a[i - 1].timeMs < a[i].timeMs;
  CHECK(sorted);
}

TEST("futures data: record files — append, reload, a cut record dropped, another kind ignored") {
  QTemporaryDir dir;
  const QString path = dir.path() + "/sub/oi.dat";
  std::vector<OiPoint> a = {{1, 10, 100}, {2, 20, 200}};
  CHECK(appendRecords<OiPoint>(path, a, 1));
  std::vector<OiPoint> b = {{3, 30, 300}};
  CHECK(appendRecords<OiPoint>(path, b, 1));
  auto back = loadRecords<OiPoint>(path, 1);
  CHECK_EQ(int(back.size()), 3);
  CHECK_EQ(back[2].oi, 30.0);
  CHECK(loadRecords<OiPoint>(path, 2).empty());   // another kind: not read as this one
  // a crash in the middle of a record: the partial record is dropped at the next append
  {
    QFile f(path);
    CHECK(f.open(QIODevice::Append));
    f.write("12345", 5);
  }
  CHECK_EQ(int(loadRecords<OiPoint>(path, 1).size()), 3);
  CHECK(appendRecords<OiPoint>(path, std::vector<OiPoint>{{4, 40, 400}}, 1));
  back = loadRecords<OiPoint>(path, 1);
  CHECK_EQ(int(back.size()), 4);
  CHECK_EQ(back[3].timeMs, int64_t(4));
  // a file of another kind is started again, not appended to
  CHECK(appendRecords<RatioPoint>(path, std::vector<RatioPoint>{{9, 1.5, 0.6}}, 2));
  CHECK_EQ(int(loadRecords<RatioPoint>(path, 2).size()), 1);
  CHECK(loadRecords<OiPoint>(path, 1).empty());
}

TEST("futures data: points lined up with bars (last at or before the close, not too old)") {
  const std::vector<OiPoint> pts = {{300, 1, 0}, {600, 2, 0}, {900, 3, 0}};
  const std::vector<int64_t> bars = {100, 300, 450, 600, 899, 1500, 2000};
  const auto v = alignOi(bars, pts, 600);
  CHECK(std::isnan(v[0]));   // before the first point
  CHECK_EQ(v[1], 1.0);       // exactly at a point
  CHECK_EQ(v[2], 1.0);
  CHECK_EQ(v[3], 2.0);
  CHECK_EQ(v[4], 2.0);
  CHECK_EQ(v[5], 3.0);       // 600 ms after the last point: still accepted
  CHECK(std::isnan(v[6]));   // 1100 ms after: too old (the series stopped)
  const std::vector<RatioPoint> rp = {{300, 2.5, 0}};
  CHECK_EQ(alignRatio(bars, rp, 1000)[2], 2.5);
}

TEST("futures data: brackets — maintenance amounts from caps and rates; unknown symbols assumed") {
  const BracketTable bt = binanceBrackets("BTCUSDT");
  CHECK(!bt.verified);                       // could not be re-checked (signed endpoint)
  CHECK_EQ(int(bt.brackets.size()), 12);
  CHECK_EQ(bt.brackets[0].mmr, 0.004);
  CHECK_EQ(bt.brackets[0].maxLeverage, 125);
  // the amounts Binance publishes follow from the caps and rates: 0, 50, 950, 11 450, 131 450, 481 450, 2 981 450 …
  const double want[] = {0, 50, 950, 11450, 131450, 481450, 2981450, 14481450, 26481450, 41481450, 121481450, 421481450};
  bool same = true;
  for (int i = 0; i < 12; ++i) same = same && std::fabs(bt.brackets[size_t(i)].cum - want[i]) < 1e-6;
  CHECK(same);
  const BracketTable other = binanceBrackets("DOGEUSDT");
  CHECK_EQ(int(other.brackets.size()), 1);
  CHECK(!other.verified);
  CHECK(other.source.startsWith("assumed"));
}
