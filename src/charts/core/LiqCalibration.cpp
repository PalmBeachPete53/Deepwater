#include "charts/core/LiqCalibration.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QTimeZone>
#include <algorithm>
#include <cmath>

namespace charts::flow {

const std::vector<LiqParams::Lev>& calibrationLeverages() {
  static const std::vector<LiqParams::Lev> l = {{5, 0.30}, {10, 0.30}, {25, 0.20}, {50, 0.12}, {100, 0.08}};
  return l;
}

CalibScore scoreModel(const LiquidationModel& m, std::span<const int64_t> openMs, int64_t barMs, std::span<const LiqEvent> events,
                      double topShare) {
  CalibScore s;
  const LiqGrid& g = m.grid();
  if (g.rows <= 0 || openMs.empty() || barMs <= 0) return s;
  const int k = std::max(1, int(std::ceil(topShare * g.rows)));
  std::vector<float> scratch(size_t(g.rows));
  double hitQty = 0;
  for (const LiqEvent& e : events) {
    // the bar holding the event, and the column before it
    const auto it = std::upper_bound(openMs.begin(), openMs.end(), e.timeMs);
    const int bar = int(it - openMs.begin()) - 1;
    if (bar < 1 || bar >= m.bars() || e.timeMs >= openMs[size_t(bar)] + barMs) continue;
    const float* col = e.longLiquidated ? m.longColumn(bar - 1) : m.shortColumn(bar - 1);
    const int row = g.rowOf(e.price);
    s.events += 1;
    s.qty += e.qty;
    if (row < 0) continue;
    // the k-th largest value of the column: rows at or above it are the dense ones (zero never counts)
    std::copy(col, col + g.rows, scratch.begin());
    std::nth_element(scratch.begin(), scratch.begin() + (k - 1), scratch.end(), std::greater<float>());
    const float threshold = std::max(scratch[size_t(k - 1)], std::numeric_limits<float>::min());
    bool hit = false;
    for (int r = std::max(0, row - 1); r <= std::min(g.rows - 1, row + 1); ++r) hit = hit || col[r] >= threshold;
    if (hit) hitQty += e.qty;
  }
  s.hit = s.qty > 0 ? hitQty / s.qty : 0;
  return s;
}

namespace {
std::vector<std::vector<LiqParams::Lev>> mixes();
}
std::vector<std::vector<LiqParams::Lev>> calibrationMixes() { return mixes(); }

namespace {
// every split of 4 quarters among 5 leverages (70 mixes)
std::vector<std::vector<LiqParams::Lev>> mixes() {
  std::vector<std::vector<LiqParams::Lev>> out;
  const auto& lev = calibrationLeverages();
  for (int a = 0; a <= 4; ++a)
    for (int b = 0; a + b <= 4; ++b)
      for (int c = 0; a + b + c <= 4; ++c)
        for (int d = 0; a + b + c + d <= 4; ++d) {
          const int e = 4 - a - b - c - d;
          const int q[5] = {a, b, c, d, e};
          std::vector<LiqParams::Lev> m;
          for (int i = 0; i < 5; ++i)
            if (q[i]) m.push_back({lev[size_t(i)].leverage, q[i] * 0.25});
          out.push_back(std::move(m));
        }
  return out;
}

}  // namespace

Calibration calibrate(const LiqInput& in, const LiqParams& base, std::span<const LiqEvent> events, double topShare) {
  QElapsedTimer t;
  t.start();
  Calibration c;
  c.best = calibrationLeverages();
  if (in.close.empty() || in.grid.rows <= 0) return c;
  // the liquidations within the bars (after the first one: they need a column before them)
  const int64_t from = in.openMs.size() > 1 ? in.openMs[1] : INT64_MAX, to = in.openMs.back() + in.barMs;
  std::vector<LiqEvent> ev;
  for (const LiqEvent& e : events)
    if (e.timeMs >= from && e.timeMs < to) ev.push_back(e);
  std::sort(ev.begin(), ev.end(), [](const LiqEvent& a, const LiqEvent& b) { return a.timeMs < b.timeMs; });
  c.eventsInRange = int(ev.size());
  if (ev.size() < 30) {
    c.ms = double(t.nsecsElapsed()) / 1e6;
    return c;
  }
  const int64_t cut = ev[ev.size() * 2 / 3].timeMs;
  auto score = [&](const std::vector<LiqParams::Lev>& mix) {
    LiqParams p = base;
    p.leverages = mix;
    return scoreStream(in.series(), in.openMs, in.barMs, in.grid, p, ev, cut, topShare);
  };
  const StreamScores def = score(calibrationLeverages());
  c.trainDefault = def.trainModel;
  c.testDefault = def.testModel;
  c.rankTrainDefault = def.trainRank.rank;
  c.rankTestDefault = def.testRank.rank;
  const auto all = mixes();
  c.candidates = int(all.size());
  double bestRank = -1;
  StreamScores bestS;
  for (const auto& m : all) {
    const StreamScores s = score(m);
    if (s.trainRank.rank > bestRank + 1e-12) {   // ties: the first one tried keeps it
      bestRank = s.trainRank.rank;
      c.best = m;
      bestS = s;
    }
  }
  c.trainBest = bestS.trainModel;
  c.testBest = bestS.testModel;
  c.rankTrainBest = bestS.trainRank.rank;
  c.rankTestBest = bestS.testRank.rank;
  c.ok = true;
  c.ms = double(t.nsecsElapsed()) / 1e6;
  return c;
}

// ------------------------------------------------------------------------------------------------ archive

namespace {
// the CSV's lines, split on commas
template <class F>
void eachRow(const QByteArray& csv, F f) {
  qsizetype at = 0;
  QList<QByteArrayView> cols;
  while (at < csv.size()) {
    qsizetype end = csv.indexOf('\n', at);
    if (end < 0) end = csv.size();
    QByteArrayView line(csv.constData() + at, end - at);
    if (line.endsWith('\r')) line.chop(1);
    at = end + 1;
    if (line.isEmpty()) continue;
    cols.clear();
    qsizetype s = 0;
    for (qsizetype i = 0; i <= line.size(); ++i)
      if (i == line.size() || line[i] == ',') {
        cols.push_back(line.sliced(s, i - s));
        s = i + 1;
      }
    f(cols);
  }
}
}  // namespace

std::vector<Bar> parseArchiveKlines(const QByteArray& csv) {
  std::vector<Bar> out;
  eachRow(csv, [&](const QList<QByteArrayView>& c) {
    if (c.size() < 6) return;
    bool ok = false;
    const qint64 t = c[0].toLongLong(&ok);
    if (!ok) return;   // the header line
    out.push_back({t / 1000, c[1].toDouble(), c[2].toDouble(), c[3].toDouble(), c[4].toDouble(), c[5].toDouble()});
  });
  std::sort(out.begin(), out.end(), [](const Bar& a, const Bar& b) { return a.time < b.time; });
  return out;
}

void parseArchiveMetrics(const QByteArray& csv, std::vector<OiPoint>& oi, std::vector<RatioPoint>& ratios) {
  eachRow(csv, [&](const QList<QByteArrayView>& c) {
    if (c.size() < 4) return;
    const QDateTime t = QDateTime::fromString(QString::fromLatin1(c[0]), "yyyy-MM-dd HH:mm:ss");
    if (!t.isValid()) return;   // the header line
    const qint64 ms = QDateTime(t.date(), t.time(), QTimeZone::UTC).toMSecsSinceEpoch();
    bool ok = false;
    const double o = c[2].toDouble(&ok);
    if (ok) oi.push_back({ms, o, c[3].toDouble()});
    if (c.size() > 6) {
      const double r = c[6].toDouble(&ok);
      if (ok && r > 0) ratios.push_back({ms, r, r / (1 + r)});
    }
  });
  auto byTime = [](const auto& a, const auto& b) { return a.timeMs < b.timeMs; };
  std::sort(oi.begin(), oi.end(), byTime);
  std::sort(ratios.begin(), ratios.end(), byTime);
  oi.erase(std::unique(oi.begin(), oi.end(), [](const OiPoint& a, const OiPoint& b) { return a.timeMs == b.timeMs; }), oi.end());
  ratios.erase(std::unique(ratios.begin(), ratios.end(), [](const RatioPoint& a, const RatioPoint& b) { return a.timeMs == b.timeMs; }),
               ratios.end());
}

std::vector<LiqEvent> parseArchiveLiquidations(const QByteArray& csv) {
  std::vector<LiqEvent> out;
  eachRow(csv, [&](const QList<QByteArrayView>& c) {
    if (c.size() < 10) return;
    bool ok = false;
    const qint64 t = c[0].toLongLong(&ok);
    if (!ok) return;
    LiqEvent e;
    e.timeMs = t;
    e.longLiquidated = c[1] == "SELL" ? 1 : 0;
    e.price = c[6].toDouble();
    if (!(e.price > 0)) e.price = c[5].toDouble();
    e.qty = c[9].toDouble();
    if (e.qty > 0 && e.price > 0) out.push_back(e);
  });
  auto key = [](const LiqEvent& a) { return std::make_tuple(a.timeMs, a.longLiquidated, a.price, a.qty); };
  std::sort(out.begin(), out.end(), [&](const LiqEvent& a, const LiqEvent& b) { return key(a) < key(b); });
  out.erase(std::unique(out.begin(), out.end(), [&](const LiqEvent& a, const LiqEvent& b) { return key(a) == key(b); }), out.end());
  return out;
}

StreamScores scoreStream(const LiqSeries& s, std::span<const int64_t> openMs, int64_t barMs, const LiqGrid& g, const LiqParams& p,
                         std::span<const LiqEvent> events, int64_t cutMs, double topShare) {
  StreamScores out;
  LiquidationModel m(g, p);
  m.setKeepColumns(false);
  const int k = std::max(1, int(std::ceil(topShare * g.rows)));
  std::vector<double> scratch(size_t(std::max(g.rows, 0)));
  double hitTrainM = 0, hitTestM = 0, hitTrainN = 0, hitTestN = 0, rankTrain = 0, rankTest = 0;
  size_t e = 0;
  const size_t n = std::min(s.size(), openMs.size());
  for (size_t i = 0; i < n; ++i) {
    while (e < events.size() && events[e].timeMs < openMs[i]) ++e;   // before this bar (a hole in the bars): skipped
    const int64_t end = openMs[i] + barMs;
    if (i > 0)
      for (; e < events.size() && events[e].timeMs < end; ++e) {
        const LiqEvent& ev = events[e];
        const bool train = ev.timeMs < cutMs;
        CalibScore& sm = train ? out.trainModel : out.testModel;
        CalibScore& sn = train ? out.trainNear : out.testNear;
        sm.events += 1;
        sm.qty += ev.qty;
        sn.events += 1;
        sn.qty += ev.qty;
        const int row = g.rowOf(ev.price);
        if (row < 0) continue;
        // the model: the k densest rows of the liquidated side, as they stood before this bar
        const std::span<const double> col = ev.longLiquidated ? m.longs() : m.shorts();
        std::copy(col.begin(), col.end(), scratch.begin());
        std::nth_element(scratch.begin(), scratch.begin() + (k - 1), scratch.end(), std::greater<double>());
        const double threshold = std::max(scratch[size_t(k - 1)], std::numeric_limits<double>::min());
        bool hit = false;
        for (int r = std::max(0, row - 1); r <= std::min(g.rows - 1, row + 1); ++r) hit = hit || col[size_t(r)] >= threshold;
        if (hit) (train ? hitTrainM : hitTestM) += ev.qty;
        // rank among the rows the bar crossed
        const int rl = std::max(0, g.rowOf(s.low[i]) < 0 ? 0 : g.rowOf(s.low[i])), rh = g.rowOf(s.high[i]) < 0 ? g.rows - 1 : g.rowOf(s.high[i]);
        if (rh > rl && row >= rl && row <= rh) {
          const double v = col[size_t(row)];
          int less = 0, equal = 0;
          for (int r = rl; r <= rh; ++r) {
            less += col[size_t(r)] < v;
            equal += col[size_t(r)] == v;
          }
          const double pct = (less + 0.5 * (equal - 1)) / double(rh - rl);
          RankScore& rs = train ? out.trainRank : out.testRank;
          rs.events += 1;
          rs.qty += ev.qty;
          (train ? rankTrain : rankTest) += pct * ev.qty;
        }
        // the baseline: the k rows next to the bar's open on the liquidated side (± 1 row, as for the model)
        const int openRow = g.rowOf(s.open[i]);
        if (openRow >= 0) {
          const int lo = ev.longLiquidated ? openRow - k + 1 : openRow, hi = ev.longLiquidated ? openRow : openRow + k - 1;
          if (row >= lo - 1 && row <= hi + 1) (train ? hitTrainN : hitTestN) += ev.qty;
        }
      }
    m.step(s.at(i));
  }
  auto fin = [](CalibScore& c, double hit) { c.hit = c.qty > 0 ? hit / c.qty : 0; };
  fin(out.trainModel, hitTrainM);
  fin(out.testModel, hitTestM);
  fin(out.trainNear, hitTrainN);
  fin(out.testNear, hitTestN);
  out.trainRank.rank = out.trainRank.qty > 0 ? rankTrain / out.trainRank.qty : 0;
  out.testRank.rank = out.testRank.qty > 0 ? rankTest / out.testRank.qty : 0;
  return out;
}

}  // namespace charts::flow
