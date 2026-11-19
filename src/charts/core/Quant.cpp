// Port of quant-terminal public/lib/quant.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Quant checks for a strategy tester result: is the backtest real, or luck and overfitting? (see Quant.h)
#include "charts/core/Quant.h"
#include "charts/core/Series.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <cstring>

// Exactness with the JS: no fused multiply-add (clang contracts a*b+c by default on arm64).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#else
#pragma STDC FP_CONTRACT OFF
#endif

namespace charts::quant {

namespace {

constexpr double INF = std::numeric_limits<double>::infinity();

double jsMax(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return NaN;
  if (a == b) return std::signbit(a) ? b : a;
  return a > b ? a : b;
}
double jsRound(double x) {
  if (!std::isfinite(x)) return x;
  const double f = std::floor(x);
  return x - f >= 0.5 ? f + 1 : f;
}
QJsonValue jnum(double v) {
  if (!std::isfinite(v)) return QJsonValue(QJsonValue::Null);
  return QJsonValue(v == 0 ? 0.0 : v);
}
QJsonArray jarr(const QVector<double>& v) {
  QJsonArray a;
  for (double x : v) a.append(jnum(x));
  return a;
}
// arr.indexOf(Math.max(...arr)): first index of the largest, -1 if a NaN is in the array.
int argMax(const QVector<double>& v) {
  double m = -INF;
  for (double x : v) m = jsMax(m, x);
  for (int i = 0; i < v.size(); ++i)
    if (v[i] == m) return i;
  return -1;
}
QVector<double> slice(const QVector<double>& r, qsizetype from, qsizetype to) {
  from = std::clamp<qsizetype>(from, 0, r.size());
  to = std::clamp<qsizetype>(to, 0, r.size());
  return to > from ? r.mid(from, to - from) : QVector<double>();
}
// sharpe(x) ?? -Infinity
double sharpeOr(const QVector<double>& r, double fallback) { const double s = sharpe(r); return std::isnan(s) ? fallback : s; }

}  // namespace

// ---------- maths ----------
double normCdf(double x) {
  // Abramowitz & Stegun 7.1.26 on erf, accurate to about 1e-7
  const double t = 1 / (1 + (0.3275911 * std::fabs(x)) / 1.4142135623730951 /* Math.SQRT2 */);
  const double y =
      1 - ((((1.061405429 * t - 1.453152027) * t + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t *
              std::exp(-(x * x) / 2);
  return x >= 0 ? (1 + y) / 2 : (1 - y) / 2;
}

double normInv(double p) {
  // Acklam's rational approximation, accurate to about 1e-9
  if (p <= 0) return -INF;
  if (p >= 1) return INF;
  static const double a[] = {-39.69683028665376, 220.9460984245205, -275.9285104469687, 138.357751867269,
                             -30.66479806614716, 2.506628277459239};
  static const double b[] = {-54.47609879822406, 161.5858368580409, -155.6989798598866, 66.80131188771972,
                             -13.28068155288572};
  static const double c[] = {-0.007784894002430293, -0.3223964580411365, -2.400758277161838, -2.549732539343734,
                             4.374664141464968, 2.938163982698783};
  static const double d[] = {0.007784695709041462, 0.3224671290700398, 2.445134137142996, 3.754408661907416};
  const double lo = 0.02425, hi = 1 - lo;
  if (p < lo) {
    const double q = std::sqrt(-2 * std::log(p));
    return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
           ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
  }
  if (p > hi) {
    const double q = std::sqrt(-2 * std::log(1 - p));
    return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
           ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
  }
  const double q = p - 0.5, r = q * q;
  return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
         (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
}

Moments moments(const QVector<double>& r) {
  const double n = r.size();
  double sum = 0;
  for (double x : r) sum += x;
  const double mean = sum / n;
  double m2 = 0, m3 = 0, m4 = 0;
  for (double x : r) {
    const double d = x - mean;
    m2 += d * d;
    m3 += d * d * d;
    m4 += d * d * d * d;
  }
  m2 /= n;
  m3 /= n;
  m4 /= n;
  const double sd = std::sqrt(m2);
  Moments m;
  m.mean = mean;
  m.sd = sd;
  m.skew = sd > 0 ? m3 / std::pow(sd, 3) : 0;
  m.kurt = sd > 0 ? m4 / std::pow(sd, 4) : 3;
  return m;
}

double sharpe(const QVector<double>& r) {
  if (r.size() < 2) return NaN;
  const Moments m = moments(r);
  return m.sd > 0 ? m.mean / m.sd : NaN;
}

// Probability the true (per-period) Sharpe beats `benchmark`, given the sample's skew and kurtosis.
double psr(const QVector<double>& r, double benchmark) {
  if (r.size() < 10) return NaN;
  const Moments m = moments(r);
  if (!(m.sd > 0)) return NaN;
  const double sr = m.mean / m.sd;
  const double denom = std::sqrt(jsMax(1e-12, 1 - m.skew * sr + ((m.kurt - 1) / 4) * sr * sr));
  return normCdf(((sr - benchmark) * std::sqrt(double(r.size() - 1))) / denom);
}

// The best Sharpe you'd expect from n tries of pure noise with this spread of Sharpes.
double expectedMaxSharpe(double n, double variance) {
  if (n < 2 || !(variance > 0)) return 0;
  const double g = 0.5772156649015329;
  return std::sqrt(variance) * ((1 - g) * normInv(1 - 1 / n) + g * normInv(1 - 1 / (n * 2.718281828459045 /* Math.E */)));
}

// Probability of backtest overfitting: split the history into `blocks`, try every half/half split,
// pick the best variant on one half and see where it ranks on the other.
std::optional<Pbo> pbo(const QVector<QVector<double>>& matrix, int blocks) {
  const int V = matrix.size();
  const int T = matrix.isEmpty() ? 0 : matrix[0].size();
  if (V < 2 || T < blocks * 5) return std::nullopt;
  const int size = T / blocks;
  auto sliceIds = [size](const QVector<double>& r, const QVector<int>& ids) {
    QVector<double> out;
    for (int b : ids) out += slice(r, qsizetype(b) * size, qsizetype(b + 1) * size);
    return out;
  };
  QVector<QVector<int>> combos;
  QVector<int> chosen;
  std::function<void(int)> pick = [&](int start) {
    if (chosen.size() == blocks / 2.0) {
      combos.push_back(chosen);
      return;
    }
    for (int b = start; b < blocks; b++) {
      chosen.push_back(b);
      pick(b + 1);
      chosen.pop_back();
    }
  };
  pick(0);
  int below = 0, counted = 0;
  for (const QVector<int>& is : combos) {
    QVector<int> oos;
    for (int b = 0; b < blocks; ++b)
      if (!is.contains(b)) oos.push_back(b);
    QVector<double> isS, oosS;
    for (const auto& r : matrix) isS.push_back(sharpeOr(sliceIds(r, is), -INF));
    const int best = argMax(isS);
    for (const auto& r : matrix) oosS.push_back(sharpeOr(sliceIds(r, oos), -INF));
    const double ref = best >= 0 ? oosS[best] : NaN;
    int rank = 1;   // 1 = worst
    for (double s : oosS)
      if (s < ref) ++rank;
    const double w = double(rank) / (V + 1);
    counted++;
    if (std::log(w / (1 - w)) <= 0) below++;
  }
  return Pbo{double(below) / counted, counted};
}

// Seeded random numbers, so the same strategy always shows the same Monte Carlo.
double Rng::operator()() {
  a = a + 0x6d2b79f5u;
  quint32 t = a;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return double(t ^ (t >> 14)) / 4294967296.0;
}

Rng rng(double seed) {
  // seed >>> 0 (ToUint32)
  double t = std::isfinite(seed) ? std::trunc(seed) : 0;
  t = std::fmod(t, 4294967296.0);
  if (t < 0) t += 4294967296.0;
  return Rng{quint32(t)};
}

std::optional<MonteCarlo> monteCarlo(const QVector<double>& tradePcts, int runs, double seed) {
  if (tradePcts.size() < 5) return std::nullopt;
  Rng rand = rng(seed);
  QVector<double> finals, dds;
  finals.reserve(runs);
  dds.reserve(runs);
  const int n = tradePcts.size();
  for (int k = 0; k < runs; k++) {
    double eq = 1, peak = 1, dd = 0;
    for (int i = 0; i < n; i++) {
      eq *= 1 + tradePcts[int(std::floor(rand() * n))] / 100;
      peak = jsMax(peak, eq);
      dd = jsMax(dd, (peak - eq) / peak);
    }
    finals.push_back((eq - 1) * 100);
    dds.push_back(dd * 100);
  }
  auto q = [](QVector<double> s, double p) {
    std::stable_sort(s.begin(), s.end(), [](double x, double y) { return x - y < 0; });
    const qsizetype i = std::min<qsizetype>(s.size() - 1, qsizetype(std::floor(p * s.size())));
    return i >= 0 && i < s.size() ? s[i] : NaN;
  };
  MonteCarlo mc;
  mc.runs = runs;
  mc.p5 = q(finals, 0.05);
  mc.p50 = q(finals, 0.5);
  mc.p95 = q(finals, 0.95);
  mc.ddP50 = q(dds, 0.5);
  mc.ddP95 = q(dds, 0.95);
  int losses = 0;
  for (double x : finals)
    if (x < 0) ++losses;
  mc.lossProb = (double(losses) / runs) * 100;
  return mc;
}

QJsonObject MonteCarlo::toJson() const {
  return {{"runs", runs}, {"p5", jnum(p5)}, {"p50", jnum(p50)}, {"p95", jnum(p95)},
          {"ddP50", jnum(ddP50)}, {"ddP95", jnum(ddP95)}, {"lossProb", jnum(lossProb)}};
}

// ---------- the checks ----------
namespace {

QVector<double> perBar(const QVector<bt::EquityPoint>& equity) {
  QVector<double> r;
  for (int i = 1; i < equity.size(); ++i) r.push_back(equity[i].value / equity[i - 1].value - 1);
  return r;
}
double periodsPerYear(const Bars& bars) {
  return bars.size() > 1 ? (365.0 * 86400 * (bars.size() - 1)) / double(bars.last().time - bars[0].time) : 252;
}

struct Tun { QString key, param; double base; QString label; };

// Up to two whole-number settings to vary (indicator lengths and the like).
QVector<Tun> tunables(const QJsonObject& strategy, const QStringList& order) {
  QVector<Tun> out;
  const QJsonObject inds = strategy.value("indicators").toObject();
  QStringList keys;
  for (const QString& k : order)
    if (inds.contains(k) && !keys.contains(k)) keys << k;
  for (const QString& k : inds.keys())
    if (!keys.contains(k)) keys << k;
  static const QRegularExpression paren(QStringLiteral(" \\(.*\\)\\z"));
  for (const QString& key : keys) {
    const QJsonObject spec = inds.value(key).toObject();
    const QString id = bt::jsString(spec.value("id"));
    const ind::DefPtr def = ind::byId(id);
    if (!def) continue;
    QJsonObject p = ind::defaults(id);
    const QJsonObject params = spec.value("params").toObject();
    for (auto it = params.begin(); it != params.end(); ++it) p.insert(it.key(), it.value());
    for (const ind::Input& inp : def->inputs) {
      const double v = bt::jsNum(p.value(inp.key));
      if (inp.type != "int" || !(v >= 2)) continue;
      QString name = def->name;
      name.remove(paren);
      out.push_back({key, inp.key, v, name + QLatin1Char(' ') + inp.label.toLower()});
      if (out.size() == 2) return out;
    }
  }
  return out;
}

struct Setting { Tun t; double value; };

QJsonObject withParams(const QJsonObject& strategy, const QVector<Setting>& settings) {
  QJsonObject s = strategy;
  QJsonObject inds = s.value("indicators").toObject();
  for (const Setting& x : settings) {
    QJsonObject spec = inds.value(x.t.key).toObject();
    QJsonObject params = spec.value("params").toObject();
    params.insert(x.t.param, x.value);
    spec.insert("params", params);
    inds.insert(x.t.key, spec);
  }
  s.insert("indicators", inds);
  return s;
}

// `x ?? def` on a strategy field, then as a number.
double orDefault(const QJsonObject& s, const QString& k, double def) {
  const QJsonValue v = s.value(k);
  return v.isUndefined() || v.isNull() ? def : bt::jsNum(v);
}

}  // namespace

Checks checksFrom(const Bars& bars, const RunSet& runs, int mcRuns) {
  const double ppy = periodsPerYear(bars), ann = std::sqrt(ppy);
  const bt::Result& base = runs.base;
  const QVector<double> baseR = perBar(base.equity);
  struct V { Variant v; QVector<double> returns; double rawSharpe; };
  QVector<V> variants;
  for (const RunSet::One& one : runs.variants) {
    const bt::Result& r = one.isBase ? base : one.result;
    V x;
    x.returns = perBar(r.equity);
    x.rawSharpe = sharpe(x.returns);
    x.v.values = one.values;
    x.v.isBase = one.isBase;
    x.v.netPct = r.stats.netProfitPct;
    x.v.trades = r.stats.totalTrades;
    variants.push_back(x);
  }
  QVector<QVector<double>> matrix;
  for (const V& x : variants) matrix.push_back(x.returns);
  Checks out;
  QVector<Gate>& gates = out.gates;
  // walk-forward: anchored, 5 test windows after the first 30% of history
  if (variants.size() > 1 && baseR.size() > 100) {
    const int T = baseR.size(), start = int(std::floor(T * 0.3)), folds = 5,
              size = int(std::floor(double(T - start) / folds));
    QVector<double> oos, isChosen;
    WalkForward wf;
    for (int k = 0; k < folds; k++) {
      const int s = start + k * size, e = k == folds - 1 ? T : s + size;
      QVector<double> trainS;
      for (const auto& r : matrix) trainS.push_back(sharpeOr(slice(r, 0, s), -INF));
      const int best = argMax(trainS);
      if (best < 0) throw std::runtime_error("Cannot read properties of undefined (reading 'slice')");
      const QVector<double> test = slice(matrix[best], s, e);
      oos += test;
      isChosen.push_back(trainS[best]);
      Fold f;
      f.from = s + 1 < bars.size() ? bars[s + 1].time : 0;
      f.to = e < bars.size() ? bars[e].time : 0;
      f.chosen = variants[best].v.values;
      const double ts = sharpe(test);
      f.testSharpe = (std::isnan(ts) ? 0 : ts) * ann;
      double acc = 1;
      for (double x : test) acc = acc * (1 + x);
      f.testReturnPct = (acc - 1) * 100;
      wf.folds.push_back(f);
    }
    const double so = sharpe(oos);
    double isSum = 0;
    for (double x : isChosen) isSum += x;
    const double oosS = (std::isnan(so) ? 0 : so) * ann, isS = (isSum / folds) * ann;
    wf.oosSharpe = oosS;
    wf.isSharpe = isS;
    wf.efficiency = isS > 0 ? oosS / isS : NaN;
    for (const Fold& f : wf.folds)
      if (f.testReturnPct > 0) wf.positiveFolds++;
    out.walkForward = wf;
    gates.push_back({"wf", QT_TRANSLATE_NOOP("charts", "Walk-forward"),
                     QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1 Sharpe on data the settings were never picked on (needs above 0)"))
                         .arg(series::toFixed(oosS, 2)),
                     oosS > 0});
  }

  // overfitting
  if (variants.size() > 2) out.pbo = pbo(matrix);
  if (out.pbo)
    gates.push_back({"pbo", QT_TRANSLATE_NOOP("charts", "Overfitting (PBO)"),
                     QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1% chance the best-looking settings are below average on unseen data (needs under 50%)"))
                         .arg(series::toFixed(out.pbo->pbo * 100, 1)),
                     out.pbo->pbo < 0.5});

  // Sharpe significance
  QVector<double> srs;
  for (const V& x : variants)
    if (!std::isnan(x.rawSharpe)) srs.push_back(x.rawSharpe);
  const double varSr = srs.size() > 1 ? std::pow(moments(srs).sd, 2) * (double(srs.size()) / (srs.size() - 1)) : 0;
  const double sr0 = expectedMaxSharpe(std::max<double>(srs.size(), 1), varSr);
  const double probSharpe = psr(baseR, 0), deflated = psr(baseR, sr0);
  if (!std::isnan(deflated))
    gates.push_back({"dsr", QT_TRANSLATE_NOOP("charts", "Deflated Sharpe"),
                     QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1 probability the edge is real after %2 version%3 tried (needs above 0.95)"))
                         .arg(series::toFixed(deflated, 3))
                         .arg(srs.size())
                         .arg(srs.size() == 1 ? QString() : QStringLiteral("s")),
                     deflated > 0.95});

  // costs: the strategy run again with costs raised (how, the caller says)
  const double np = runs.stressed.stats.netProfitPct;
  gates.push_back({"cost", QT_TRANSLATE_NOOP("charts", "Cost stress"), runs.costText ? runs.costText(np) : QString(), np > 0});

  // robustness: share of neighbouring settings that keep at least half the base Sharpe
  const QVector<Tunable>& tunOut = runs.tunables;
  if (variants.size() > 1) {
    double b = 0;
    for (const V& x : variants)
      if (x.v.isBase) { b = std::isnan(x.rawSharpe) ? 0 : x.rawSharpe; break; }
    int others = 0, ok = 0;
    for (const V& x : variants) {
      if (x.v.isBase) continue;
      ++others;
      if (!std::isnan(x.rawSharpe) && b > 0 && x.rawSharpe >= b * 0.5) ++ok;
    }
    Robustness rb;
    rb.share = (double(ok) / others) * 100;
    rb.params = tunOut;
    out.robustness = rb;
    gates.push_back({"robust", QT_TRANSLATE_NOOP("charts", "Nearby settings"),
                     QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1% of nearby settings keep at least half the Sharpe (needs 60%+)"))
                         .arg(series::toFixed(rb.share, 0)),
                     rb.share >= 60});
  }

  // a trade's % is on the money in it; the account only moves by the share sizePct put in
  QVector<double> pcts;
  for (const bt::Trade& t : base.trades) pcts.push_back((t.pnlPct * runs.sizePct) / 100);
  out.monteCarlo = monteCarlo(pcts, mcRuns);

  const double bs = sharpe(baseR);
  out.base.sharpe = (std::isnan(bs) ? 0 : bs) * ann;
  out.base.netPct = base.stats.netProfitPct;
  out.base.trades = base.stats.totalTrades;
  for (const V& x : variants) {
    Variant v = x.v;
    v.sharpe = std::isnan(x.rawSharpe) ? NaN : x.rawSharpe * ann;
    out.variants.push_back(v);
  }
  out.tunables = tunOut;
  out.psr = probSharpe;
  out.dsr = deflated;
  out.sr0 = sr0 * ann;
  out.trials = srs.size();
  out.costStress = np;
  for (const Gate& g : gates)
    if (g.pass) out.passed++;
  return out;
}


Checks quantChecks(const Bars& bars, const QJsonObject& strategy, const Options& opt, const std::atomic<bool>* cancel) {
  const auto stopped = [cancel] { return cancel && cancel->load(std::memory_order_relaxed); };
  const auto cancelledChecks = [] {
    Checks c;
    c.cancelled = true;
    return c;
  };
  RunSet runs;
  if (stopped()) return cancelledChecks();
  runs.base = bt::backtest(bars, strategy, opt.subBars);
  const QVector<Tun> tun = tunables(strategy, opt.indicatorOrder);
  static const double MULT[] = {0.6, 0.8, 1, 1.2, 1.4};
  QVector<QVector<double>> axes;
  for (const Tun& t : tun) {
    QVector<double> ax;
    for (double m : MULT) {
      const double v = jsMax(2, jsRound(t.base * m));
      if (!ax.contains(v)) ax.push_back(v);
    }
    axes.push_back(ax);
  }
  for (int k = 0; k < tun.size(); ++k) runs.tunables.push_back({tun[k].label, tun[k].base, axes[k]});
  QVector<QVector<double>> grid;
  if (tun.isEmpty()) grid = {{}};
  else if (tun.size() == 1) for (double v : axes[0]) grid.push_back({v});
  else for (double a : axes[0]) for (double b : axes[1]) grid.push_back({a, b});
  for (const QVector<double>& vals : grid) {
    QVector<Setting> settings;
    bool isBase = true;
    for (int k = 0; k < tun.size(); ++k) {
      settings.push_back({tun[k], vals[k]});
      if (vals[k] != tun[k].base) isBase = false;
    }
    RunSet::One one;
    one.values = vals;
    one.isBase = isBase;
    if (stopped()) return cancelledChecks();   // up to 25 backtests: a superseded run stops between two
    if (!isBase) one.result = bt::backtest(bars, withParams(strategy, settings), opt.subBars);
    runs.variants.push_back(one);
  }
  // costs: double the fees, triple the slippage (at least 0.05% a side), double the spread
  const double stressSlip = jsMax(orDefault(strategy, "slippagePct", 0.05) * 3, 0.05);
  QJsonObject stressedStrategy = strategy;
  stressedStrategy.insert("feePct", orDefault(strategy, "feePct", 0.1) * 2);
  stressedStrategy.insert("slippagePct", stressSlip);
  stressedStrategy.insert("spreadPct", orDefault(strategy, "spreadPct", 0) * 2);
  if (stopped()) return cancelledChecks();
  runs.stressed = bt::backtest(bars, stressedStrategy, opt.subBars);
  const bool spread = bt::jsTruthy(strategy.value("spreadPct"));
  runs.costText = [stressSlip, spread](double np) {
    // `${np >= 0 ? "+" : ""}${np}`: a null netProfitPct reads "+null" in the JS
    const QString npText = std::isnan(np) ? QStringLiteral("+null") : (np >= 0 ? QStringLiteral("+") : QString()) + series::numStr(np);
    return QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1% with double fees and %2% slippage a side%3 (needs above 0)"))
        .arg(npText, series::numStr(series::toFixed(stressSlip, 3).toDouble()),
             spread ? QStringLiteral(QT_TRANSLATE_NOOP("charts", " and double the spread")) : QString());
  };
  runs.sizePct = orDefault(strategy, "positionPct", 100);
  if (stopped()) return cancelledChecks();
  return checksFrom(bars, runs, opt.mcRuns);
}

QJsonObject Checks::toJson() const {
  auto tunJson = [](const Tunable& t) {
    return QJsonObject{{"label", t.label}, {"base", jnum(t.base)}, {"values", jarr(t.values)}};
  };
  QJsonArray vs, ts, gs;
  for (const Variant& v : variants)
    vs.append(QJsonObject{{"values", jarr(v.values)}, {"isBase", v.isBase}, {"sharpe", jnum(v.sharpe)},
                          {"netPct", jnum(v.netPct)}, {"trades", v.trades}});
  for (const Tunable& t : tunables) ts.append(tunJson(t));
  for (const Gate& g : gates)
    gs.append(QJsonObject{{"key", g.key}, {"label", g.label}, {"value", g.value}, {"pass", g.pass}});
  QJsonValue wf(QJsonValue::Null), pb(QJsonValue::Null), rb(QJsonValue::Null), mc(QJsonValue::Null);
  if (walkForward) {
    QJsonArray folds;
    for (const Fold& f : walkForward->folds)
      folds.append(QJsonObject{{"from", double(f.from)}, {"to", double(f.to)}, {"chosen", jarr(f.chosen)},
                               {"testSharpe", jnum(f.testSharpe)}, {"testReturnPct", jnum(f.testReturnPct)}});
    wf = QJsonObject{{"folds", folds}, {"oosSharpe", jnum(walkForward->oosSharpe)},
                     {"isSharpe", jnum(walkForward->isSharpe)}, {"efficiency", jnum(walkForward->efficiency)},
                     {"positiveFolds", walkForward->positiveFolds}};
  }
  if (pbo) pb = QJsonObject{{"pbo", jnum(pbo->pbo)}, {"splits", pbo->splits}};
  if (robustness) {
    QJsonArray params;
    for (const Tunable& t : robustness->params)
      params.append(QJsonObject{{"label", t.label}, {"values", jarr(t.values)}, {"base", jnum(t.base)}});
    rb = QJsonObject{{"share", jnum(robustness->share)}, {"params", params}};
  }
  if (monteCarlo) mc = monteCarlo->toJson();
  return {{"base", QJsonObject{{"sharpe", jnum(base.sharpe)}, {"netPct", jnum(base.netPct)}, {"trades", base.trades}}},
          {"variants", vs}, {"tunables", ts}, {"walkForward", wf}, {"pbo", pb}, {"psr", jnum(psr)},
          {"dsr", jnum(dsr)}, {"sr0", jnum(sr0)}, {"trials", trials}, {"costStress", jnum(costStress)},
          {"robustness", rb}, {"monteCarlo", mc}, {"gates", gs}, {"passed", passed}};
}

// ---------- key order of strategy.indicators in the JSON text ----------
namespace {

struct Scanner {
  const QByteArray& s;
  int i = 0;
  void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
  bool str(QString* out) {   // at '"'
    ws();
    if (i >= s.size() || s[i] != '"') return false;
    const int from = i++;
    while (i < s.size() && s[i] != '"') i += s[i] == '\\' ? 2 : 1;
    if (i >= s.size()) return false;
    ++i;
    if (out) *out = QJsonDocument::fromJson("[" + s.mid(from, i - from) + "]").array().at(0).toString();
    return true;
  }
  bool skip() {   // any value
    ws();
    if (i >= s.size()) return false;
    const char c = s[i];
    if (c == '"') return str(nullptr);
    if (c == '{' || c == '[') {
      const char close = c == '{' ? '}' : ']';
      ++i;
      ws();
      if (i < s.size() && s[i] == close) { ++i; return true; }
      for (;;) {
        if (c == '{') { if (!str(nullptr)) return false; ws(); if (i >= s.size() || s[i] != ':') return false; ++i; }
        if (!skip()) return false;
        ws();
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == close) { ++i; return true; }
        return false;
      }
    }
    while (i < s.size() && !strchr(",}] \t\r\n", s[i])) ++i;   // number, true, false, null
    return true;
  }
  // Keys of the object at i, in text order (duplicates once, at their first place).
  bool keys(QStringList& out, const std::function<bool(const QString&)>& onValue) {
    ws();
    if (i >= s.size() || s[i] != '{') return false;
    ++i;
    ws();
    if (i < s.size() && s[i] == '}') { ++i; return true; }
    for (;;) {
      QString k;
      if (!str(&k)) return false;
      ws();
      if (i >= s.size() || s[i] != ':') return false;
      ++i;
      if (!out.contains(k)) out << k;
      if (!onValue(k)) return false;
      ws();
      if (i < s.size() && s[i] == ',') { ++i; continue; }
      if (i < s.size() && s[i] == '}') { ++i; return true; }
      return false;
    }
  }
};

bool isArrayIndex(const QString& k) {
  if (k.isEmpty() || k.size() > 10 || (k.size() > 1 && k[0] == '0')) return false;
  for (QChar c : k) if (c < '0' || c > '9') return false;
  return k.toULongLong() < 4294967295ULL;
}

}  // namespace

QStringList indicatorOrder(const QByteArray& strategyJson) {
  Scanner sc{strategyJson};
  QStringList top, found;
  bool have = false;
  sc.keys(top, [&](const QString& k) {
    if (k != "indicators") return sc.skip();
    // a later duplicate "indicators" replaces the value (JSON.parse keeps the last one)
    QStringList ks;
    const int at = sc.i;
    Scanner inner{strategyJson, at};
    if (inner.keys(ks, [&](const QString&) { return inner.skip(); })) { found = ks; have = true; }
    else { found.clear(); have = false; }
    return sc.skip();
  });
  if (!have) return {};
  // JS own-key order: integer indices ascending, then the other keys in insertion order.
  QStringList ints, rest;
  for (const QString& k : found) (isArrayIndex(k) ? ints : rest) << k;
  std::sort(ints.begin(), ints.end(), [](const QString& a, const QString& b) { return a.toULongLong() < b.toULongLong(); });
  return ints + rest;
}

}  // namespace charts::quant
