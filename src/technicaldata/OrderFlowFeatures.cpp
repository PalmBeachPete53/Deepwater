// A bar's order flow values for machine learning. See OrderFlowFeatures.h.
#include "technicaldata/OrderFlowFeatures.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "technicaldata/TechnicalDataStore.h"
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace technicaldata {

namespace {
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
qint64 dayOf(qint64 t) { return t - ((t % 86400) + 86400) % 86400; }
const QStringList kFpCols{"delta_pct", "poc_dist", "va_width", "va_pos", "imb_buy", "imb_sell", "stack_buy", "stack_sell",
                          "low_delta_pct", "high_delta_pct", "open_delta_pct"};
}  // namespace

bool OrderFlowFeatures::isColumn(const QString& name) {
  return name.startsWith("fp_") || name.startsWith("depth_") || name.startsWith("oi_");
}

OrderFlowFeatures::OrderFlowFeatures(const Store& store, const QString& pair, int tfSeconds) : m_store(store), m_pair(pair), m_tf(tfSeconds) {
  if (!pair.startsWith("BINANCE:") || tfSeconds <= 0 || tfSeconds > 86400 || 86400 % tfSeconds != 0) return;
  const PairSpec spec = specFromJson(store.pairSpec(pair));
  for (const QString& v : spec.footprint) {
    if (v != "spot" && v != "perp") continue;
    m_venues << v;
    for (const QString& c : kFpCols) m_cols << "fp_" + v + "_" + c;
    for (const FpDay& d : store.footprintDays(pair, v)) m_fpDays[v == "spot" ? 0 : 1].insert(d.day);
  }
  if (m_venues.contains("perp")) m_cols << "fp_perp_vol_ratio";
  m_depth = spec.depth;
  if (m_depth) {
    m_cols << "depth_imb_02" << "depth_imb_1" << "depth_imb_5" << "depth_bid1_chg" << "depth_ask1_chg";
    for (const ArchDay& d : store.archiveDays(pair, "depth")) m_depthDays.insert(d.day);
  }
  m_oi = spec.openInterest;
  if (m_oi) {
    m_cols << "oi_chg_pct" << "oi_top_pos_ls" << "oi_accounts_ls" << "oi_taker_ratio";
    for (const ArchDay& d : store.archiveDays(pair, "oi")) m_oiDays.insert(d.day);
  }
}

const std::vector<fp::Minute>* OrderFlowFeatures::fpDay(const QString& venue, qint64 day) {
  if (!m_fpDays[venue == "spot" ? 0 : 1].contains(day)) return nullptr;
  const QString key = venue + QString::number(day);
  auto it = m_fpCache.find(key);
  if (it == m_fpCache.end()) {
    if (m_fpCache.size() > 6) m_fpCache.clear();   // a table goes forward: a few days are enough
    std::vector<fp::Minute> ms;
    fp::decodeDay(m_store.footprintData(m_pair, venue, day), day, ms);
    it = m_fpCache.insert(key, std::move(ms));
  }
  return &*it;
}

const std::vector<perp::DepthSample>* OrderFlowFeatures::depthDay(qint64 day) {
  if (!m_depthDays.contains(day)) return nullptr;
  auto it = m_depthCache.find(day);
  if (it == m_depthCache.end()) {
    if (m_depthCache.size() > 4) m_depthCache.clear();
    std::vector<perp::DepthSample> s;
    perp::decodeDepth(m_store.depthData(m_pair, day), day, s);
    it = m_depthCache.insert(day, std::move(s));
  }
  return &*it;
}

const std::vector<perp::OiRow>* OrderFlowFeatures::oiDay(qint64 day) {
  if (!m_oiDays.contains(day)) return nullptr;
  auto it = m_oiCache.find(day);
  if (it == m_oiCache.end()) {
    if (m_oiCache.size() > 4) m_oiCache.clear();
    it = m_oiCache.insert(day, m_store.openInterest(m_pair, day, day + 86400));
  }
  return &*it;
}

// the last element with time < t (or ≤ t), looking in t's day then the day before
template <class V, class F>
static const typename V::value_type* lastBefore(qint64 t, bool inclusive, F&& dayOfData) {
  for (qint64 day : {dayOf(inclusive ? t : t - 1), dayOf(inclusive ? t : t - 1) - 86400}) {
    const V* v = dayOfData(day);
    if (!v || v->empty()) continue;
    auto it = std::partition_point(v->begin(), v->end(), [&](const auto& x) { return inclusive ? x.t <= t : x.t < t; });
    if (it != v->begin()) return &*std::prev(it);
  }
  return nullptr;
}

const perp::DepthSample* OrderFlowFeatures::depthAt(qint64 t, bool inclusive) {
  return lastBefore<std::vector<perp::DepthSample>>(t, inclusive, [this](qint64 d) { return depthDay(d); });
}
const perp::OiRow* OrderFlowFeatures::oiAt(qint64 t, bool inclusive) {
  return lastBefore<std::vector<perp::OiRow>>(t, inclusive, [this](qint64 d) { return oiDay(d); });
}

bool OrderFlowFeatures::row(qint64 barTime, double close, double volume, std::vector<double>& out) {
  if (!available() || !(close > 0)) return false;
  std::vector<double> v;
  v.reserve(size_t(m_cols.size()));
  const qint64 end = barTime + m_tf;
  double perpVolume = NaN;
  for (const QString& venue : m_venues) {
    const auto* day = fpDay(venue, dayOf(barTime));
    if (!day) return false;
    fp::Minute bar;
    auto it = std::lower_bound(day->begin(), day->end(), barTime, [](const fp::Minute& m, qint64 t) { return m.t < t; });
    // the bar's opening minute: its delta / its volume (arXiv 2607.09426: the imbalance at the open of each quarter hour
    // on Binance perpetuals predicts the next 4 to 12 hours); 0 when no trade in that minute
    double openDelta = 0;
    if (it != day->end() && it->t == barTime && it->volume() > 0) openDelta = it->delta() / it->volume() * 100;
    for (; it != day->end() && it->t < end; ++it) fp::merge(bar, *it);
    const double vol = bar.volume();
    if (bar.trades == 0 || !(vol > 0)) return false;
    if (venue == "perp") perpVolume = vol;
    const fp::Stats s = fp::statsOf(bar);
    const double width = s.vaHigh - s.vaLow;
    // the bar's lowest and highest quarter of levels
    const size_t n = bar.levels.size(), q = std::max<size_t>(1, (n + 3) / 4);
    double lowDelta = 0, highDelta = 0;
    for (size_t k = 0; k < n; ++k) {
      const double d = double(bar.levels[k].ask) - double(bar.levels[k].bid);
      if (k < q) lowDelta += d;
      if (k + q >= n) highDelta += d;
    }
    v.push_back(bar.delta() / vol * 100);
    v.push_back((close / s.poc - 1) * 100);
    v.push_back(width / close * 100);
    v.push_back(width > 0 ? std::clamp((close - s.vaLow) / width, 0.0, 1.0) : 0.5);
    v.push_back(s.buyImbalances);
    v.push_back(s.sellImbalances);
    v.push_back(s.buyStacks);
    v.push_back(s.sellStacks);
    v.push_back(lowDelta / vol * 100);
    v.push_back(highDelta / vol * 100);
    v.push_back(openDelta);
  }
  if (m_venues.contains("perp")) {
    if (!(volume > 0)) return false;
    v.push_back(perpVolume / volume);
  }
  if (m_depth) {
    const perp::DepthSample* now = depthAt(end, false);
    const perp::DepthSample* then = depthAt(barTime, true);
    if (!now || end - now->t > 300 || !then || barTime - then->t > 300) return false;
    for (int band : {6, 7, 11}) {
      const double x = perp::imbalance(*now, band);
      if (!std::isfinite(x)) return false;
      v.push_back(x * 100);
    }
    auto chg = [](float a, float b) { return std::isfinite(a) && std::isfinite(b) && b > 0 ? (double(a) / double(b) - 1) * 100 : NaN; };
    const double bid = chg(now->depth[4], then->depth[4]), ask = chg(now->depth[7], then->depth[7]);   // −1 % and +1 %
    if (!std::isfinite(bid) || !std::isfinite(ask)) return false;
    v.push_back(bid);
    v.push_back(ask);
  }
  if (m_oi) {
    const perp::OiRow* now = oiAt(end, false);
    const perp::OiRow* then = oiAt(barTime, true);
    if (!now || end - now->t > 600 || !then || barTime - then->t > 600 || !(then->oi > 0)) return false;
    v.push_back((now->oi / then->oi - 1) * 100);
    v.push_back(now->topPositions);
    v.push_back(now->accounts);
    v.push_back(now->takerRatio);
  }
  for (double x : v) if (!std::isfinite(x)) return false;
  out = std::move(v);
  return true;
}

}  // namespace technicaldata
