#include "technicaldata/PerpArchive.h"
#include "charts/core/Ticks.h"
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>

namespace technicaldata::perp {

using charts::flow::parseDecimal;

const std::array<double, kBands>& bandPercents() {
  static const std::array<double, kBands> p{-5, -4, -3, -2, -1, -0.2, 0.2, 1, 2, 3, 4, 5};
  return p;
}
int bandOf(double percent) {
  for (int i = 0; i < kBands; ++i)
    if (std::fabs(bandPercents()[size_t(i)] - percent) < 1e-9) return i;
  return -1;
}

namespace {
QString path(const char* kind, const QString& symbol, const QDate& day) {
  return QString("https://data.binance.vision/data/futures/um/daily/%1/%2/%2-%1-%3.zip").arg(kind, symbol, day.toString("yyyy-MM-dd"));
}

// days since 1970-01-01 of a civil date (Howard Hinnant's algorithm)
qint64 daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = unsigned((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return qint64(era) * 146097 + qint64(doe) - 719468;
}

// "2026-11-18 00:00:07" → Unix seconds; -1 if it isn't that
qint64 timeOf(const char* b, const char* e) {
  if (e - b != 19 || b[4] != '-' || b[7] != '-' || b[10] != ' ' || b[13] != ':' || b[16] != ':') return -1;
  auto num = [&](int at, int n) {
    int v = 0;
    for (int i = 0; i < n; ++i) {
      const char c = b[at + i];
      if (c < '0' || c > '9') return -1;
      v = v * 10 + (c - '0');
    }
    return v;
  };
  const int y = num(0, 4), mo = num(5, 2), d = num(8, 2), h = num(11, 2), mi = num(14, 2), s = num(17, 2);
  if (y < 0 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 60) return -1;
  return daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

// The lines of a CSV and their comma-separated fields (no quoting in Binance's files).
template <class F>
void eachLine(QByteArrayView csv, F&& f) {
  const char* p = csv.data();
  const char* end = p + csv.size();
  while (p < end) {
    const char* nl = static_cast<const char*>(std::memchr(p, '\n', size_t(end - p)));
    const char* e = nl ? nl : end;
    const char* le = e;
    if (le > p && le[-1] == '\r') --le;
    if (le > p) {
      std::array<const char*, 9> fb{}, fe{};
      int n = 0;
      const char* s = p;
      for (const char* q = p; q <= le && n < 9; ++q)
        if (q == le || *q == ',') { fb[size_t(n)] = s; fe[size_t(n)] = q; ++n; s = q + 1; }
      f(fb, fe, n);
    }
    p = nl ? nl + 1 : end;
  }
}

bool real(const char* b, const char* e, double& v) {
  const char* stop = nullptr;
  const auto d = parseDecimal(b, e, &stop);
  if (!d.ok || stop != e) return false;
  v = d.toDouble();
  return true;
}
}  // namespace

QUrl depthUrl(const QString& symbol, const QDate& day) { return QUrl(path("bookDepth", symbol, day)); }
QUrl metricsUrl(const QString& symbol, const QDate& day) { return QUrl(path("metrics", symbol, day)); }
QDate firstDepthDay() { return QDate(2023, 1, 1); }
QDate firstMetricsDay() { return QDate(2020, 9, 1); }

std::vector<DepthSample> parseDepth(QByteArrayView csv, qint64 dayStart, int* bad) {
  std::map<qint64, DepthSample> by;   // a snapshot's 12 rows come together; keyed by time all the same
  int nBad = 0;
  eachLine(csv, [&](const auto& fb, const auto& fe, int n) {
    if (n >= 1 && (*fb[0] < '0' || *fb[0] > '9')) return;   // the header
    double pct = 0, depth = 0;
    const qint64 t = n >= 3 ? timeOf(fb[0], fe[0]) : -1;
    if (t < 0 || !real(fb[1], fe[1], pct) || !real(fb[2], fe[2], depth)) { ++nBad; return; }
    const int b = bandOf(pct);
    if (b < 0 || t < dayStart || t >= dayStart + 86400) { ++nBad; return; }
    auto it = by.find(t);
    if (it == by.end()) {
      DepthSample s;
      s.t = t;
      s.depth.fill(std::numeric_limits<float>::quiet_NaN());
      it = by.emplace(t, s).first;
    }
    it->second.depth[size_t(b)] = float(depth);
  });
  if (bad) *bad = nBad;
  std::vector<DepthSample> out;
  out.reserve(by.size());
  for (auto& [t, s] : by) out.push_back(s);
  return out;
}

std::vector<OiRow> parseMetrics(QByteArrayView csv, qint64 dayStart, int* bad) {
  std::map<qint64, OiRow> by;   // the first months list each row twice: the last one kept (they are the same)
  int nBad = 0;
  eachLine(csv, [&](const auto& fb, const auto& fe, int n) {
    if (n >= 1 && (*fb[0] < '0' || *fb[0] > '9')) return;
    OiRow r;
    r.t = n >= 8 ? timeOf(fb[0], fe[0]) : -1;
    // empty ratio fields happen in some early files: NaN, the row kept
    auto opt = [&](int i, double& v) {
      if (fb[size_t(i)] == fe[size_t(i)]) { v = std::numeric_limits<double>::quiet_NaN(); return true; }
      return real(fb[size_t(i)], fe[size_t(i)], v);
    };
    if (r.t < 0 || !real(fb[2], fe[2], r.oi) || !real(fb[3], fe[3], r.oiValue) || !opt(4, r.topAccounts) || !opt(5, r.topPositions) ||
        !opt(6, r.accounts) || !opt(7, r.takerRatio) || r.t < dayStart || r.t >= dayStart + 86400) {
      ++nBad;
      return;
    }
    by[r.t] = r;
  });
  if (bad) *bad = nBad;
  std::vector<OiRow> out;
  out.reserve(by.size());
  for (auto& [t, r] : by) out.push_back(r);
  return out;
}

// v1, little-endian: "PDD1", u32 n, u32 seconds of the day [n], then band by band f32 depth [n]; qCompress.
QByteArray encodeDepth(const std::vector<DepthSample>& s) {
  QByteArray b;
  b.reserve(qsizetype(8 + s.size() * (4 + 4 * kBands)));
  auto put32 = [&b](quint32 v) {
    char raw[4];
    qToLittleEndian(v, raw);
    b.append(raw, 4);
  };
  b.append("PDD1", 4);
  put32(quint32(s.size()));
  for (const DepthSample& x : s) put32(quint32(((x.t % 86400) + 86400) % 86400));
  for (int k = 0; k < kBands; ++k)
    for (const DepthSample& x : s) {
      quint32 u;
      std::memcpy(&u, &x.depth[size_t(k)], 4);
      put32(u);
    }
  return qCompress(b, 9);
}

bool decodeDepth(QByteArrayView blob, qint64 dayStart, std::vector<DepthSample>& out) {
  out.clear();
  const QByteArray raw = qUncompress(reinterpret_cast<const uchar*>(blob.data()), blob.size());
  if (raw.size() < 8 || !raw.startsWith("PDD1")) return false;
  const auto* p = reinterpret_cast<const uchar*>(raw.constData());
  const quint32 n = qFromLittleEndian<quint32>(p + 4);
  if (n > 86400 || qsizetype(8 + qsizetype(n) * 4 * (1 + kBands)) != raw.size()) return false;
  out.resize(n);
  const uchar* q = p + 8;
  for (DepthSample& x : out) { x.t = dayStart + qFromLittleEndian<quint32>(q); q += 4; }
  for (int k = 0; k < kBands; ++k)
    for (DepthSample& x : out) {
      const quint32 u = qFromLittleEndian<quint32>(q);
      std::memcpy(&x.depth[size_t(k)], &u, 4);
      q += 4;
    }
  return true;
}

double imbalance(const DepthSample& s, int askBand) {
  if (askBand < kBands / 2 || askBand >= kBands) return std::numeric_limits<double>::quiet_NaN();
  const double ask = s.depth[size_t(askBand)], bid = s.depth[size_t(kBands - 1 - askBand)];
  if (!std::isfinite(ask) || !std::isfinite(bid) || bid + ask <= 0) return std::numeric_limits<double>::quiet_NaN();
  return bid / (bid + ask);
}

}  // namespace technicaldata::perp
