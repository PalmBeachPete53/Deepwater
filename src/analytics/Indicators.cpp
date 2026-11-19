#include "analytics/Indicators.h"
#include <algorithm>

namespace analytics {
QVector<double> closes(const QVector<Bar>& b) {
  QVector<double> c(b.size());
  for (int i = 0; i < b.size(); ++i) c[i] = b[i].c;
  return c;
}

QVector<double> sma(const QVector<double>& v, int n) {
  QVector<double> o(v.size(), NAN);
  if (n <= 0) return o;
  double s = 0; int cnt = 0;
  for (int i = 0; i < v.size(); ++i) {
    s += v[i]; ++cnt;
    if (cnt > n) { s -= v[i - n]; --cnt; }
    if (cnt == n) o[i] = s / n;
  }
  return o;
}

QVector<double> ema(const QVector<double>& v, int n) {
  QVector<double> o(v.size(), NAN);
  if (n <= 0 || v.size() < n) return o;
  double s = 0;
  for (int i = 0; i < n; ++i) s += v[i];
  double e = s / n; o[n - 1] = e;
  const double k = 2.0 / (n + 1);
  for (int i = n; i < v.size(); ++i) { e += k * (v[i] - e); o[i] = e; }
  return o;
}

QVector<double> rsi(const QVector<double>& v, int n) {
  QVector<double> o(v.size(), NAN);
  if (n <= 0 || v.size() <= n) return o;
  double g = 0, l = 0;
  for (int i = 1; i <= n; ++i) { const double d = v[i] - v[i - 1]; (d > 0 ? g : l) += std::abs(d); }
  g /= n; l /= n;
  auto val = [](double g, double l) { return l == 0 ? (g == 0 ? 50.0 : 100.0) : 100.0 - 100.0 / (1.0 + g / l); };
  o[n] = val(g, l);
  for (int i = n + 1; i < v.size(); ++i) {
    const double d = v[i] - v[i - 1];
    g = (g * (n - 1) + (d > 0 ? d : 0)) / n;
    l = (l * (n - 1) + (d < 0 ? -d : 0)) / n;
    o[i] = val(g, l);
  }
  return o;
}

QVector<double> atr(const QVector<Bar>& b, int n) {
  QVector<double> o(b.size(), NAN);
  if (n <= 0 || b.size() <= n) return o;
  auto tr = [&](int i) {
    const double hl = b[i].h - b[i].l;
    if (i == 0) return hl;
    return std::max({hl, std::abs(b[i].h - b[i - 1].c), std::abs(b[i].l - b[i - 1].c)});
  };
  double a = 0;
  for (int i = 1; i <= n; ++i) a += tr(i);
  a /= n; o[n] = a;
  for (int i = n + 1; i < b.size(); ++i) { a = (a * (n - 1) + tr(i)) / n; o[i] = a; }
  return o;
}

QVector<double> roc(const QVector<double>& v, int n) {
  QVector<double> o(v.size(), NAN);
  for (int i = n; i < v.size(); ++i) if (v[i - n] != 0) o[i] = (v[i] / v[i - n] - 1.0) * 100.0;
  return o;
}

QVector<double> realizedVol(const QVector<double>& c, int n, double ppy) {
  QVector<double> o(c.size(), NAN);
  if (n < 2) return o;
  QVector<double> r(c.size(), NAN);
  for (int i = 1; i < c.size(); ++i) if (c[i] > 0 && c[i - 1] > 0) r[i] = std::log(c[i] / c[i - 1]);
  for (int i = n; i < c.size(); ++i) {
    double s = 0, s2 = 0; int k = 0;
    for (int j = i - n + 1; j <= i; ++j) if (finite(r[j])) { s += r[j]; s2 += r[j] * r[j]; ++k; }
    if (k < n / 2 + 1) continue;
    const double m = s / k, var = std::max(0.0, (s2 - k * m * m) / (k - 1));
    o[i] = std::sqrt(var) * std::sqrt(ppy) * 100.0;
  }
  return o;
}

double percentileRank(const QVector<double>& v, int from, int to, double x) {
  from = std::max(0, from); to = std::min<int>(v.size(), to);
  int n = 0, le = 0;
  for (int i = from; i < to; ++i) if (finite(v[i])) { ++n; if (v[i] <= x) ++le; }
  return n ? 100.0 * le / n : NAN;
}

QVector<Swing> swings(const QVector<Bar>& b, int order) {
  QVector<Swing> out;
  for (int i = order; i + order < b.size(); ++i) {
    bool hi = true, lo = true;
    for (int k = 1; k <= order && (hi || lo); ++k) {
      if (!(b[i].h > b[i - k].h && b[i].h > b[i + k].h)) hi = false;
      if (!(b[i].l < b[i - k].l && b[i].l < b[i + k].l)) lo = false;
    }
    if (hi) out.push_back({i, b[i].h, true});
    if (lo) out.push_back({i, b[i].l, false});
  }
  return out;
}
}
