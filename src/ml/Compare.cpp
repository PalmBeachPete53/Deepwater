#include "ml/Compare.h"
#include <QFile>
#include <QHash>
#include <QMap>
#include <QTextStream>
#include <algorithm>
#include <functional>
#include <cmath>
#include <random>

namespace ml::compare {

Preds readPredictions(const QString& csvPath) {
  Preds p;
  QFile f(csvPath);
  if (!f.open(QIODevice::ReadOnly)) return p;
  QTextStream in(&f);
  const QStringList header = in.readLine().split(',');
  const int cd = int(header.indexOf("date")), ca = int(header.indexOf("actual")), cp = int(header.indexOf("predicted"));
  const int cn = int(header.indexOf("no_skill")), cr = int(header.indexOf("persistence"));
  if (cd < 0 || ca < 0 || cp < 0) return p;
  const int need = std::max({cd, ca, cp});
  while (!in.atEnd()) {
    const QStringList c = in.readLine().split(',');
    if (c.size() <= need) continue;
    bool okA = false, okP = false;
    const double a = c[ca].toDouble(&okA), v = c[cp].toDouble(&okP);
    if (!okA || !okP || !std::isfinite(a) || !std::isfinite(v)) continue;
    p.dates << c[cd].left(19);
    p.actual << a;
    p.predicted << v;
    auto opt = [&](int col) { bool ok = false; const double x = col >= 0 ? c.value(col).toDouble(&ok) : NAN; return ok && std::isfinite(x) ? x : NAN; };
    if (cn >= 0) p.noSkill << opt(cn);
    if (cr >= 0) p.persistence << opt(cr);
  }
  return p;
}

namespace {
// balanced accuracy of the rows idx (all rows when idx is empty)
double ba(const QVector<double>& a, const QVector<double>& p, const QVector<int>* idx) {
  QMap<long long, std::pair<int, int>> perClass;   // class → (right, count)
  const int n = idx ? int(idx->size()) : int(a.size());
  for (int k = 0; k < n; ++k) {
    const int i = idx ? (*idx)[k] : k;
    const long long cls = std::llround(a[i]);
    auto& rc = perClass[cls];
    ++rc.second;
    if (std::llround(p[i]) == cls) ++rc.first;
  }
  if (perClass.isEmpty()) return NAN;
  double s = 0;
  for (const auto& rc : perClass) s += double(rc.first) / rc.second;
  return s / perClass.size();
}
double mae(const QVector<double>& a, const QVector<double>& p, const QVector<int>* idx) {
  const int n = idx ? int(idx->size()) : int(a.size());
  if (n == 0) return NAN;
  double s = 0;
  for (int k = 0; k < n; ++k) { const int i = idx ? (*idx)[k] : k; s += std::abs(a[i] - p[i]); }
  return s / n;
}
}  // namespace

double balancedAccuracy(const QVector<double>& actual, const QVector<double>& predicted) {
  if (actual.size() != predicted.size()) return NAN;
  return ba(actual, predicted, nullptr);
}
double meanAbsError(const QVector<double>& actual, const QVector<double>& predicted) {
  if (actual.size() != predicted.size()) return NAN;
  return mae(actual, predicted, nullptr);
}

Common commonRows(const QStringList& a, const QStringList& b) {
  Common c;
  QHash<QString, int> where;
  for (int j = 0; j < b.size(); ++j) where.insert(b[j], j);
  for (int i = 0; i < a.size(); ++i) {
    const auto it = where.constFind(a[i]);
    if (it == where.cend()) continue;
    c.ia << i;
    c.ib << *it;
  }
  return c;
}

Paired pairedBootstrap(const QVector<double>& actual, const QVector<double>& predA, const QVector<double>& predB, bool classification,
                       int block, int resamples, uint32_t seed) {
  Paired out;
  const int n = int(actual.size());
  if (n < 2 || predA.size() != n || predB.size() != n) return out;
  auto score = [&](const QVector<double>& pred, const QVector<int>* idx) {
    return classification ? ba(actual, pred, idx) : -mae(actual, pred, idx);
  };
  out.rows = n;
  out.scoreA = score(predA, nullptr);
  out.scoreB = score(predB, nullptr);
  out.diff = out.scoreA - out.scoreB;
  block = std::clamp(block, 1, n);
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> start(0, n - block);
  QVector<int> idx(n);
  int below = 0, above = 0, used = 0;
  for (int r = 0; r < resamples; ++r) {
    for (int k = 0; k < n;) {   // blocks of consecutive rows until n rows are drawn
      const int s = start(rng);
      for (int j = 0; j < block && k < n; ++j) idx[k++] = s + j;
    }
    const double d = score(predA, &idx) - score(predB, &idx);
    if (!std::isfinite(d)) continue;
    ++used;
    if (d <= 0) ++below;
    if (d >= 0) ++above;
  }
  if (used == 0) return out;
  // two-sided: twice the share on the far side of zero from the observed difference (1 when they are equal)
  const double far = out.diff > 0 ? double(below) / used : out.diff < 0 ? double(above) / used : 1.0;
  out.p = std::min(1.0, 2.0 * far);
  return out;
}

double adjusted(double p, int tries) { return std::min(1.0, p * std::max(1, tries)); }

QVector<double> rowScores(const QVector<double>& actual, const QVector<double>& predicted, bool classification) {
  QVector<double> out(actual.size(), 0.0);
  if (!classification) {
    for (int i = 0; i < actual.size(); ++i) out[i] = -std::fabs(predicted[i] - actual[i]);
    return out;
  }
  QHash<long long, int> count;
  for (double a : actual) ++count[std::llround(a)];
  const double n = actual.size(), k = count.size();
  for (int i = 0; i < actual.size(); ++i) {
    const long long a = std::llround(actual[i]);
    if (std::llround(predicted[i]) == a) out[i] = n / (k * count.value(a));
  }
  return out;
}

Pbo pbo(const QVector<QVector<double>>& m, int blocks) {
  Pbo out;
  out.setups = int(m.size());
  out.rows = m.isEmpty() ? 0 : int(m[0].size());
  if (out.setups < 3 || blocks < 2 || blocks % 2 || out.rows < blocks * 10) return out;
  for (const auto& r : m)
    if (r.size() != out.rows) return out;
  const int size = out.rows / blocks;
  auto meanOn = [&](const QVector<double>& r, const QVector<int>& ids) {
    double s = 0;
    for (int b : ids)
      for (int i = b * size; i < (b + 1) * size; ++i) s += r[i];
    return s / (double(ids.size()) * size);
  };
  int below = 0, splits = 0;
  QVector<int> chosen;
  std::function<void(int)> pick = [&](int start) {
    if (chosen.size() == blocks / 2) {
      QVector<int> rest;
      for (int b = 0; b < blocks; ++b)
        if (!chosen.contains(b)) rest << b;
      int best = 0;
      double bestIs = -INFINITY;
      for (int v = 0; v < m.size(); ++v)
        if (const double x = meanOn(m[v], chosen); x > bestIs) { bestIs = x; best = v; }
      const double ref = meanOn(m[best], rest);
      int rank = 1;   // 1 = worst
      for (int v = 0; v < m.size(); ++v)
        if (meanOn(m[v], rest) < ref) ++rank;
      const double w = double(rank) / (m.size() + 1);
      if (std::log(w / (1 - w)) <= 0) ++below;
      ++splits;
      return;
    }
    for (int b = start; b < blocks; ++b) {
      chosen << b;
      pick(b + 1);
      chosen.removeLast();
    }
  };
  pick(0);
  out.splits = splits;
  out.pbo = splits ? double(below) / splits : NAN;
  return out;
}

}  // namespace ml::compare
