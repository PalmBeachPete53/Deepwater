#include "charts/core/FuturesData.h"
#include "charts/core/Types.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cstring>
#include <limits>

namespace charts::flow {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
// Binance sends numbers as strings ("95624.789"), times as numbers
double num(const QJsonValue& v) {
  if (v.isString()) {
    bool ok = false;
    const double d = v.toString().toDouble(&ok);
    return ok ? d : kNaN;
  }
  return v.isDouble() ? v.toDouble() : kNaN;
}
int64_t ms(const QJsonValue& v) { return v.isDouble() ? clampToInt64(v.toDouble()) : v.isString() ? v.toString().toLongLong() : 0; }

template <class T, class F>
bool parseArray(const QByteArray& json, std::vector<T>& out, F each) {
  QJsonParseError e;
  const QJsonDocument d = QJsonDocument::fromJson(json, &e);
  if (e.error != QJsonParseError::NoError || !d.isArray()) return false;
  out.clear();
  for (const QJsonValue& v : d.array()) {
    T p;
    if (each(v.toObject(), p)) out.push_back(p);
  }
  std::stable_sort(out.begin(), out.end(), [](const T& a, const T& b) { return a.timeMs < b.timeMs; });
  return true;
}

struct Header {
  char magic[4] = {'D', 'W', 'F', 'S'};
  uint32_t version = 1, recordSize = 0, kind = 0;
};
static_assert(sizeof(Header) == 16);
}  // namespace

bool parseOpenInterestHist(const QByteArray& json, std::vector<OiPoint>& out) {
  return parseArray(json, out, [](const QJsonObject& o, OiPoint& p) {
    p = {ms(o.value("timestamp")), num(o.value("sumOpenInterest")), num(o.value("sumOpenInterestValue"))};
    return p.timeMs > 0 && std::isfinite(p.oi);
  });
}

bool parseLongShortRatio(const QByteArray& json, std::vector<RatioPoint>& out) {
  return parseArray(json, out, [](const QJsonObject& o, RatioPoint& p) {
    p = {ms(o.value("timestamp")), num(o.value("longShortRatio")), num(o.value("longAccount"))};
    return p.timeMs > 0 && std::isfinite(p.ratio);
  });
}

bool parseFundingRates(const QByteArray& json, std::vector<FundingPoint>& out) {
  return parseArray(json, out, [](const QJsonObject& o, FundingPoint& p) {
    p = {ms(o.value("fundingTime")), num(o.value("fundingRate")), num(o.value("markPrice"))};
    return p.timeMs > 0 && std::isfinite(p.rate);
  });
}

bool parseForceOrder(const QByteArray& json, LiqEvent& out, QString* symbol) {
  const QJsonObject root = QJsonDocument::fromJson(json).object();
  if (root.value("e").toString() != QLatin1String("forceOrder")) return false;
  const QJsonObject o = root.value("o").toObject();
  const QString side = o.value("S").toString();
  if (side != QLatin1String("SELL") && side != QLatin1String("BUY")) return false;
  LiqEvent e;
  e.timeMs = ms(o.value("T"));
  e.price = num(o.value("ap"));
  if (!(e.price > 0)) e.price = num(o.value("p"));
  e.qty = num(o.value("z"));
  if (!(e.qty > 0)) return false;   // nothing filled
  e.longLiquidated = side == QLatin1String("SELL") ? 1 : 0;
  if (e.timeMs <= 0 || !(e.price > 0)) return false;
  if (symbol) *symbol = o.value("s").toString();
  out = e;
  return true;
}

template <class T>
size_t mergeByTime(std::vector<T>& into, const std::vector<T>& add) {
  if (add.empty()) return 0;
  size_t added = 0;
  // the usual case: everything new is after what we have
  if (into.empty() || add.front().timeMs > into.back().timeMs) {
    for (const T& p : add)
      if (into.empty() || p.timeMs > into.back().timeMs) { into.push_back(p); ++added; }
    return added;
  }
  std::vector<T> out;
  out.reserve(into.size() + add.size());
  size_t i = 0, j = 0;
  while (i < into.size() || j < add.size()) {
    if (j == add.size() || (i < into.size() && into[i].timeMs < add[j].timeMs)) out.push_back(into[i++]);
    else {
      if (i < into.size() && into[i].timeMs == add[j].timeMs) ++i;   // replaced
      else ++added;
      // equal times inside `add`: the last one wins
      if (!out.empty() && out.back().timeMs == add[j].timeMs) out.back() = add[j];
      else out.push_back(add[j]);
      ++j;
    }
  }
  into.swap(out);
  return added;
}

template <class T>
bool appendRecords(const QString& path, std::span<const T> recs, uint32_t kind) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  Header want;
  want.recordSize = uint32_t(sizeof(T));
  want.kind = kind;
  bool fresh = true;
  if (f.exists() && f.size() >= qint64(sizeof(Header))) {
    if (!f.open(QIODevice::ReadOnly)) return false;
    Header h;
    fresh = f.read(reinterpret_cast<char*>(&h), sizeof h) != qint64(sizeof h) || std::memcmp(&h, &want, sizeof h) != 0;
    f.close();
  }
  if (!f.open(fresh ? QIODevice::WriteOnly | QIODevice::Truncate : QIODevice::ReadWrite)) return false;
  if (fresh) {
    if (f.write(reinterpret_cast<const char*>(&want), sizeof want) != qint64(sizeof want)) return false;
  } else {
    // a record cut by a crash at the end: dropped, the next write starts on a record boundary
    const qint64 body = f.size() - qint64(sizeof(Header));
    f.resize(qint64(sizeof(Header)) + body - body % qint64(sizeof(T)));
    f.seek(f.size());
  }
  const qint64 bytes = qint64(recs.size() * sizeof(T));
  return !bytes || f.write(reinterpret_cast<const char*>(recs.data()), bytes) == bytes;
}

template <class T>
std::vector<T> loadRecords(const QString& path, uint32_t kind) {
  std::vector<T> out;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return out;
  Header h, want;
  want.recordSize = uint32_t(sizeof(T));
  want.kind = kind;
  if (f.read(reinterpret_cast<char*>(&h), sizeof h) != qint64(sizeof h) || std::memcmp(&h, &want, sizeof h) != 0) return out;
  const qint64 n = (f.size() - qint64(sizeof h)) / qint64(sizeof(T));
  out.resize(size_t(n));
  const qint64 bytes = n * qint64(sizeof(T));
  if (f.read(reinterpret_cast<char*>(out.data()), bytes) != bytes) out.clear();
  return out;
}

template size_t mergeByTime<OiPoint>(std::vector<OiPoint>&, const std::vector<OiPoint>&);
template size_t mergeByTime<RatioPoint>(std::vector<RatioPoint>&, const std::vector<RatioPoint>&);
template size_t mergeByTime<FundingPoint>(std::vector<FundingPoint>&, const std::vector<FundingPoint>&);
template size_t mergeByTime<LiqEvent>(std::vector<LiqEvent>&, const std::vector<LiqEvent>&);
template bool appendRecords<OiPoint>(const QString&, std::span<const OiPoint>, uint32_t);
template bool appendRecords<RatioPoint>(const QString&, std::span<const RatioPoint>, uint32_t);
template bool appendRecords<FundingPoint>(const QString&, std::span<const FundingPoint>, uint32_t);
template bool appendRecords<LiqEvent>(const QString&, std::span<const LiqEvent>, uint32_t);
template std::vector<OiPoint> loadRecords<OiPoint>(const QString&, uint32_t);
template std::vector<RatioPoint> loadRecords<RatioPoint>(const QString&, uint32_t);
template std::vector<FundingPoint> loadRecords<FundingPoint>(const QString&, uint32_t);
template std::vector<LiqEvent> loadRecords<LiqEvent>(const QString&, uint32_t);

namespace {
template <class P, class V>
std::vector<double> align(std::span<const int64_t> bars, std::span<const P> pts, int64_t maxAge, V value) {
  std::vector<double> out(bars.size(), kNaN);
  size_t j = 0;
  for (size_t i = 0; i < bars.size(); ++i) {
    // pts ascending, bars ascending: the pointer only moves forward
    while (j < pts.size() && pts[j].timeMs <= bars[i]) ++j;
    if (j == 0) continue;
    const P& p = pts[j - 1];
    if (bars[i] - p.timeMs <= maxAge) out[i] = value(p);
  }
  return out;
}
}  // namespace

std::vector<double> alignOi(std::span<const int64_t> barCloseMs, std::span<const OiPoint> pts, int64_t maxAgeMs) {
  return align(barCloseMs, pts, maxAgeMs, [](const OiPoint& p) { return p.oi; });
}
std::vector<double> alignRatio(std::span<const int64_t> barCloseMs, std::span<const RatioPoint> pts, int64_t maxAgeMs) {
  return align(barCloseMs, pts, maxAgeMs, [](const RatioPoint& p) { return p.ratio; });
}

void fillMaintenanceAmounts(std::vector<LeverageBracket>& b) {
  double cum = 0, floor = 0, prevMmr = 0;
  for (size_t i = 0; i < b.size(); ++i) {
    if (i) cum += floor * (b[i].mmr - prevMmr);
    b[i].cum = cum;
    floor = b[i].notionalCap;
    prevMmr = b[i].mmr;
  }
}

BracketTable binanceBrackets(const QString& symbol) {
  BracketTable t;
  if (symbol == QLatin1String("BTCUSDT")) {
    t.brackets = {{50000, 125, 0.004, 0},          {600000, 100, 0.005, 0},     {3000000, 75, 0.0065, 0},
                  {12000000, 50, 0.01, 0},         {70000000, 25, 0.02, 0},     {100000000, 20, 0.025, 0},
                  {230000000, 10, 0.05, 0},        {480000000, 5, 0.1, 0},      {600000000, 4, 0.125, 0},
                  {800000000, 3, 0.15, 0},         {1200000000, 2, 0.25, 0},    {1800000000, 1, 0.5, 0}};
    fillMaintenanceAmounts(t.brackets);
    t.source = "Binance USD-M BTCUSDT brackets as published in 2024 (not re-checked: the endpoint needs an API key)";
    return t;
  }
  t.brackets = {{1e18, 50, 0.005, 0}};
  t.source = "assumed: one bracket, 0.5 % maintenance, up to 50× (no published table for this symbol here)";
  return t;
}

}  // namespace charts::flow
