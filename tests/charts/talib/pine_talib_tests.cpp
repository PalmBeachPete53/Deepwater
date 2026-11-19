// The PineTS engine checked against TA-Lib (user's decision, 2026-11-18): a Pine script's own logic can be read and
// corrected, the engine underneath can't — if its series, history (x[n]), na handling or ta.* functions were wrong,
// every script would be. TA-Lib (BSD, written independently, C) is the reference for everything the two share.
//
// One Pine script plots each shared function (and a few engine constructions written in Pine: history, nested
// averages); TA-Lib computes the same on the same real BTCUSDT bars (tests/charts/core/fixtures: 1D since 2017, the last
// 5000 bars of 1h, 5m and 1m). Every value must agree within 1e-9 relative, from the first bar both have a value — or,
// where the two libraries START differently (their seeding conventions, listed with each case), after a warm-up long
// enough for that start to fade below 1e-9. Both must have a value at the same bars after that point.
// Built only when TA-Lib is installed (brew install ta-lib); not part of the app.
#include "Check.h"
#include "charts/core/Pine.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <cmath>
#include <functional>
#include <ta-lib/ta_libc.h>

using namespace charts;

namespace {
QString root() { return QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("../../.."); }

Bars csvBars(const QString& name) {
  Bars out;
  QFile f(root() + "/tests/charts/core/fixtures/" + name);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { check::fail(__FILE__, __LINE__, "cannot open " + name); return out; }
  while (!f.atEnd()) {
    const QByteArray line = f.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#') || line.startsWith("time")) continue;
    const QList<QByteArray> c = line.split(',');
    if (c.size() < 6) continue;
    out << Bar{c[0].toLongLong(), c[1].toDouble(), c[2].toDouble(), c[3].toDouble(), c[4].toDouble(), c[5].toDouble()};
  }
  return out;
}

struct Set { QString name; Bars bars; };
const QVector<Set>& sets() {
  static const QVector<Set> s = {{"1D", csvBars("binance-btcusdt-1d.csv")}, {"1h", csvBars("binance-btcusdt-1h.csv")},
                                 {"5m", csvBars("binance-btcusdt-5m.csv")}, {"1m", csvBars("binance-btcusdt-1m.csv")}};
  return s;
}

// The Pine side: one plot per checked value, titled with its key.
const char* kScript = R"PINE(//@version=5
indicator("PineTS vs TA-Lib", precision=16)
// moving averages and dispersion
plot(ta.sma(close, 14), "sma")
plot(ta.ema(close, 14), "ema")
plot(ta.wma(close, 14), "wma")
plot(ta.stdev(close, 20), "stdev")
plot(ta.variance(close, 20), "variance")
[bbm, bbu, bbl] = ta.bb(close, 20, 2)
plot(bbm, "bb_mid")
plot(bbu, "bb_up")
plot(bbl, "bb_low")
plot(ta.linreg(close, 20, 0), "linreg")
plot(ta.correlation(high, low, 20), "correl")
// oscillators
plot(ta.rsi(close, 14), "rsi")
[m, s, h] = ta.macd(close, 12, 26, 9)
plot(m, "macd")
plot(s, "macd_signal")
plot(h, "macd_hist")
plot(ta.stoch(close, high, low, 14), "stoch_k")
plot(ta.cci(hlc3, 20), "cci")
plot(ta.mom(close, 10), "mom")
plot(ta.roc(close, 10), "roc")
plot(ta.wpr(14), "wpr")
plot(ta.mfi(hlc3, 14), "mfi")
plot(ta.cmo(close, 14), "cmo")
// volatility, trend, volume
plot(ta.tr(true), "tr")
plot(ta.atr(14), "atr")
[dip, dim, adx] = ta.dmi(14, 14)
plot(dip, "di_plus")
plot(dim, "di_minus")
plot(adx, "adx")
plot(ta.sar(0.02, 0.02, 0.2), "sar")
plot(ta.obv, "obv")
// windows
plot(ta.highest(high, 20), "highest")
plot(ta.lowest(low, 20), "lowest")
plot(ta.highestbars(high, 20), "highestbars")
plot(ta.lowestbars(low, 20), "lowestbars")
plot(math.sum(close, 10), "sum")
plot(ta.change(close), "change")
// the engine itself: history, nested series, arithmetic on series
plot(close - close[10], "hist_mom")
e1 = ta.ema(close, 10)
e2 = ta.ema(e1, 10)
plot(2 * e1 - e2, "dema")
e3 = ta.ema(e2, 10)
plot(3 * e1 - 3 * e2 + e3, "tema")
plot(ta.sma(ta.sma(close, 10), 11), "trima")
plot(ta.stdev(ta.ema(close, 10), 20), "stdev_of_ema")
)PINE";

// ---- the TA-Lib side ----
using Vec = std::vector<double>;
struct In { Vec o, h, l, c, v; };
In inputs(const Bars& b) {
  In in;
  for (const Bar& x : b) { in.o.push_back(x.open); in.h.push_back(x.high); in.l.push_back(x.low); in.c.push_back(x.close); in.v.push_back(x.volume); }
  return in;
}
// a TA-Lib output (outBegIdx, outNBElement) laid on the bars, NaN before
Vec placed(int n, int beg, int nb, const double* out) {
  Vec v(size_t(n), NAN);
  for (int k = 0; k < nb; ++k) v[size_t(beg + k)] = out[k];
  return v;
}
template <class F>
Vec one(int n, F f) {
  Vec out(static_cast<size_t>(n));
  int beg = 0, nb = 0;
  if (f(&beg, &nb, out.data()) != TA_SUCCESS) return Vec(size_t(n), NAN);
  return placed(n, beg, nb, out.data());
}

struct Case {
  QString key;
  std::function<Vec(const In&)> ref;
  int warm = 0;          // bars after the first common value to skip (a different start that has to fade)
  QString note;          // the convention behind a warm-up, or what the case checks
  bool diff = false;     // compare bar-to-bar changes (a constant offset is a convention)
  double tol = 1e-9;     // relative
};

QVector<Case> cases() {
  const int L = 0;   // startIdx
  auto e = [](const In& in) { return int(in.c.size()) - 1; };
  QVector<Case> c;
  auto add = [&](QString k, std::function<Vec(const In&)> f, int warm = 0, QString note = {}, bool diff = false, double tol = 1e-9) {
    c.push_back({std::move(k), std::move(f), warm, std::move(note), diff, tol});
  };
  // PineTS's ta.variance and ta.correlation lose digits on large prices (a running E[x²] − E[x]²): measured against an
  // exact two-pass sum on these bars, PineTS is off by up to 1.2e-8 relative, TA-Lib by 2.4e-10. A real weakness of the
  // engine, too small to matter on a chart (a cent on a million): checked at 2e-8.
  const double kLostDigits = 2e-8;
  add("sma", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_SMA(L, e(in), in.c.data(), 14, b, n, o); }); });
  add("ema", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_EMA(L, e(in), in.c.data(), 14, b, n, o); }); });
  add("wma", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_WMA(L, e(in), in.c.data(), 14, b, n, o); }); });
  add("stdev", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_STDDEV(L, e(in), in.c.data(), 20, 1.0, b, n, o); }); });
  add("variance", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_VAR(L, e(in), in.c.data(), 20, 1.0, b, n, o); }); },
      0, "PineTS loses digits here (see kLostDigits)", false, kLostDigits);
  for (int k = 0; k < 3; ++k)
    add(QStringList{"bb_up", "bb_mid", "bb_low"}[k], [&, k](const In& in) {
      const int n = int(in.c.size());
      Vec u(static_cast<size_t>(n)), m(static_cast<size_t>(n)), lo(static_cast<size_t>(n));
      int beg = 0, nb = 0;
      TA_BBANDS(L, n - 1, in.c.data(), 20, 2, 2, TA_MAType_SMA, &beg, &nb, u.data(), m.data(), lo.data());
      return placed(n, beg, nb, (k == 0 ? u : k == 1 ? m : lo).data());
    });
  add("linreg", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_LINEARREG(L, e(in), in.c.data(), 20, b, n, o); }); });
  add("correl", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_CORREL(L, e(in), in.h.data(), in.l.data(), 20, b, n, o); }); },
      0, "PineTS loses digits here (see kLostDigits)", false, kLostDigits);
  add("rsi", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_RSI(L, e(in), in.c.data(), 14, b, n, o); }); });
  for (int k = 0; k < 3; ++k)
    add(QStringList{"macd", "macd_signal", "macd_hist"}[k], [&, k](const In& in) {
      const int n = int(in.c.size());
      Vec a(static_cast<size_t>(n)), s(static_cast<size_t>(n)), h(static_cast<size_t>(n));
      int beg = 0, nb = 0;
      TA_MACD(L, n - 1, in.c.data(), 12, 26, 9, &beg, &nb, a.data(), s.data(), h.data());
      return placed(n, beg, nb, (k == 0 ? a : k == 1 ? s : h).data());
    }, 400, "TA-Lib starts the fast EMA on the slow one's first bar; Pine starts each EMA on its own (fades by 2/13 a bar)");
  add("stoch_k", [&](const In& in) {
    const int n = int(in.c.size());
    Vec k(static_cast<size_t>(n)), d(static_cast<size_t>(n));
    int beg = 0, nb = 0;
    TA_STOCHF(L, n - 1, in.h.data(), in.l.data(), in.c.data(), 14, 1, TA_MAType_SMA, &beg, &nb, k.data(), d.data());
    return placed(n, beg, nb, k.data());
  });
  add("cci", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_CCI(L, e(in), in.h.data(), in.l.data(), in.c.data(), 20, b, n, o); }); });
  add("mom", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MOM(L, e(in), in.c.data(), 10, b, n, o); }); });
  add("hist_mom", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MOM(L, e(in), in.c.data(), 10, b, n, o); }); },
      0, "the engine's history operator: close - close[10] against TA-Lib's momentum");
  add("change", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MOM(L, e(in), in.c.data(), 1, b, n, o); }); });
  add("roc", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_ROC(L, e(in), in.c.data(), 10, b, n, o); }); });
  add("wpr", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_WILLR(L, e(in), in.h.data(), in.l.data(), in.c.data(), 14, b, n, o); }); });
  add("mfi", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MFI(L, e(in), in.h.data(), in.l.data(), in.c.data(), in.v.data(), 14, b, n, o); }); });
  // Pine's CMO is 100·(Σ ups − Σ downs)/(Σ ups + Σ downs) over plain sums; TA-Lib's own CMO smooths like its RSI (it is
  // 2·RSI − 100, checked to 1e-13): another definition. Pine's is built here from TA-Lib's sums.
  add("cmo", [&](const In& in) {
    const int n = int(in.c.size());
    Vec up(static_cast<size_t>(n), 0.0), dn(static_cast<size_t>(n), 0.0);
    for (int i = 1; i < n; ++i) {
      const double d = in.c[size_t(i)] - in.c[size_t(i - 1)];
      (d > 0 ? up : dn)[size_t(i)] = std::fabs(d);
    }
    const Vec su = one(n - 1, [&](int* b, int* nb, double* o) { return TA_SUM(0, n - 2, up.data() + 1, 14, b, nb, o); });
    const Vec sd = one(n - 1, [&](int* b, int* nb, double* o) { return TA_SUM(0, n - 2, dn.data() + 1, 14, b, nb, o); });
    Vec out(static_cast<size_t>(n), NAN);
    for (int i = 1; i < n; ++i) {
      const double a = su[size_t(i - 1)], d = sd[size_t(i - 1)];
      if (std::isfinite(a) && std::isfinite(d) && a + d != 0) out[size_t(i)] = 100 * (a - d) / (a + d);
    }
    return out;
  }, 0, "Pine's definition from TA-Lib's sums (TA-Lib's CMO is Wilder-smoothed)");
  add("tr", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_TRANGE(L, e(in), in.h.data(), in.l.data(), in.c.data(), b, n, o); }); });
  add("atr", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_ATR(L, e(in), in.h.data(), in.l.data(), in.c.data(), 14, b, n, o); }); },
      400, "TA-Lib's first ATR averages the true ranges from bar 1, Pine's from bar 0 (high − low): fades by 1/14 a bar");
  add("di_plus", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_PLUS_DI(L, e(in), in.h.data(), in.l.data(), in.c.data(), 14, b, n, o); }); },
      400, "TA-Lib seeds Wilder's sums with a plain sum; Pine's rma with a mean (fades by 1/14 a bar)");
  add("di_minus", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MINUS_DI(L, e(in), in.h.data(), in.l.data(), in.c.data(), 14, b, n, o); }); },
      400, "as di_plus");
  add("adx", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_ADX(L, e(in), in.h.data(), in.l.data(), in.c.data(), 14, b, n, o); }); },
      800, "two Wilder smoothings seeded differently (fades by 1/14 a bar, twice)");
  // Parabolic SAR: TA-Lib follows Wilder's 1978 rules (another first trend and other limits) and never meets Pine's on
  // these bars; Pine's ta.sar is TradingView's documented algorithm (reference manual), written out here in C++.
  add("sar", [&](const In& in) {
    const int n = int(in.c.size());
    const Vec &c = in.c, &h = in.h, &lo = in.l;
    Vec ps(static_cast<size_t>(n), NAN);
    double result = NAN, maxMin = NAN, acc = NAN;
    bool isBelow = false;
    const double start = 0.02, inc = 0.02, mx = 0.2;
    for (int i = 1; i < n; ++i) {
      bool first = false;
      if (i == 1) {
        if (c[1] > c[0]) { isBelow = true; maxMin = h[1]; result = lo[0]; }
        else { isBelow = false; maxMin = lo[1]; result = h[0]; }
        first = true;
        acc = start;
      }
      result = result + acc * (maxMin - result);
      if (isBelow) {
        if (result > lo[size_t(i)]) { first = true; isBelow = false; result = std::max(h[size_t(i)], maxMin); maxMin = lo[size_t(i)]; acc = start; }
      } else if (result < h[size_t(i)]) {
        first = true; isBelow = true; result = std::min(lo[size_t(i)], maxMin); maxMin = h[size_t(i)]; acc = start;
      }
      if (!first) {
        if (isBelow) { if (h[size_t(i)] > maxMin) { maxMin = h[size_t(i)]; acc = std::min(acc + inc, mx); } }
        else if (lo[size_t(i)] < maxMin) { maxMin = lo[size_t(i)]; acc = std::min(acc + inc, mx); }
      }
      if (isBelow) { result = std::min(result, lo[size_t(i - 1)]); if (i > 1) result = std::min(result, lo[size_t(i - 2)]); }
      else { result = std::max(result, h[size_t(i - 1)]); if (i > 1) result = std::max(result, h[size_t(i - 2)]); }
      ps[size_t(i)] = result;
    }
    return ps;
  }, 0, "Pine's documented algorithm (TA-Lib's SAR is Wilder's original)");
  add("obv", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_OBV(L, e(in), in.c.data(), in.v.data(), b, n, o); }); },
      0, "TA-Lib starts at the first bar's volume, Pine at 0: a constant offset — the bar-to-bar changes are compared", true);
  add("highest", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MAX(L, e(in), in.h.data(), 20, b, n, o); }); });
  add("lowest", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_MIN(L, e(in), in.l.data(), 20, b, n, o); }); });
  // index of the extreme → Pine's offset (0 = this bar, −k = k bars ago)
  for (int k = 0; k < 2; ++k)
    add(k ? "lowestbars" : "highestbars", [&, k](const In& in) {
      const int n = int(in.c.size());
      std::vector<int> idx(static_cast<size_t>(n));
      int beg = 0, nb = 0;
      if (k) TA_MININDEX(L, n - 1, in.l.data(), 20, &beg, &nb, idx.data());
      else TA_MAXINDEX(L, n - 1, in.h.data(), 20, &beg, &nb, idx.data());
      Vec v(size_t(n), NAN);
      for (int j = 0; j < nb; ++j) v[size_t(beg + j)] = double(idx[size_t(j)] - (beg + j));
      return v;
    });
  add("sum", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_SUM(L, e(in), in.c.data(), 10, b, n, o); }); });
  add("dema", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_DEMA(L, e(in), in.c.data(), 10, b, n, o); }); },
      400, "nested EMAs written in Pine (2·ema − ema(ema)): TA-Lib starts the inner EMA later (fades by 2/11 a bar)");
  add("tema", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_TEMA(L, e(in), in.c.data(), 10, b, n, o); }); },
      400, "three nested EMAs written in Pine, as dema");
  add("trima", [&](const In& in) { return one(int(in.c.size()), [&](int* b, int* n, double* o) { return TA_TRIMA(L, e(in), in.c.data(), 20, b, n, o); }); },
      0, "an average of an average written in Pine (sma(sma(close, 10), 11)) against TA-Lib's triangular average");
  add("stdev_of_ema", [&](const In& in) {
    const int n = int(in.c.size());
    const Vec em = one(n, [&](int* b, int* nb, double* o) { return TA_EMA(L, n - 1, in.c.data(), 10, b, nb, o); });
    // TA-Lib needs a series without leading NaN: the EMA from its first value
    int first = 0;
    while (first < n && std::isnan(em[size_t(first)])) ++first;
    Vec out(size_t(n), NAN);
    if (first >= n) return out;
    Vec tmp(size_t(n - first));
    int beg = 0, nb = 0;
    TA_STDDEV(0, n - first - 1, em.data() + first, 20, 1.0, &beg, &nb, tmp.data());
    for (int j = 0; j < nb; ++j) out[size_t(first + beg + j)] = tmp[size_t(j)];
    return out;
  }, 0, "a function of a derived series (stdev of an EMA): series feeding series");
  return c;
}

struct Outcome { QString where, error; double worst = 0; qint64 values = 0; int from = -1; };

Outcome compare(const Case& c, const Set& s, const pine::Result& ref, const In& in) {
  Outcome o;
  o.where = c.key + " " + s.name;
  const pine::Plot* p = ref.plot(c.key);
  if (!p) { o.error = "no Pine plot"; return o; }
  const Vec t = c.ref(in);
  const qsizetype n = s.bars.size();
  // from the first bar both have a value, plus the warm-up of a different start
  int first = -1;
  for (qsizetype i = 0; i < n; ++i)
    if (std::isfinite(t[size_t(i)]) && std::isfinite(p->values[i])) { first = int(i); break; }
  if (first < 0) { o.error = "never a common value"; return o; }
  o.from = first + c.warm + (c.diff ? 1 : 0);
  for (qsizetype i = o.from; i < n; ++i) {
    double a = p->values[i], b = t[size_t(i)];
    if (c.diff) { a -= p->values[i - 1]; b -= t[size_t(i - 1)]; }
    if (std::isnan(a) != std::isnan(b)) {
      o.error = QString("[%1]: PineTS %2, TA-Lib %3 (one has no value)").arg(i).arg(a, 0, 'g', 17).arg(b, 0, 'g', 17);
      return o;
    }
    if (std::isnan(a)) continue;
    ++o.values;
    const double rel = std::fabs(a - b) / std::max(1.0, std::fabs(b));
    o.worst = std::max(o.worst, rel);
    if (rel > c.tol) {
      o.error = QString("[%1]: PineTS %2, TA-Lib %3").arg(i).arg(a, 0, 'g', 17).arg(b, 0, 'g', 17);
      return o;
    }
  }
  return o;
}
}  // namespace

TEST("PineTS vs TA-Lib: every shared function and engine construction, on 1D, 1h, 5m and 1m BTCUSDT") {
  TA_Initialize();
  const QVector<Case> cs = cases();
  QVector<pine::Result> runs(sets().size());
  QThreadPool pool;
  pool.setStackSize(16u << 20);
  QElapsedTimer t;
  t.start();
  for (int k = 0; k < sets().size(); ++k)
    pool.start(QRunnable::create([&, k] {
      pine::Options opt;
      opt.symbol = "BINANCE:BTCUSDT";
      opt.timeoutMs = 300000;
      runs[k] = pine::run(QString::fromUtf8(kScript), sets()[k].bars, opt);
    }));
  pool.waitForDone();
  const double pineSec = t.elapsed() / 1000.0;
  QHash<QString, double> worst;
  QHash<QString, qint64> count;
  int failed = 0;
  for (int k = 0; k < sets().size(); ++k) {
    if (!runs[k].ok()) { check::fail(__FILE__, __LINE__, sets()[k].name + ": Pine: " + runs[k].error); continue; }
    const In in = inputs(sets()[k].bars);
    for (const Case& c : cs) {
      const Outcome o = compare(c, sets()[k], runs[k], in);
      if (!o.error.isEmpty()) {
        ++failed;
        check::fail(__FILE__, __LINE__, o.where + (c.note.isEmpty() ? QString() : " (" + c.note + ")") + ": " + o.error);
      }
      worst[c.key] = std::max(worst.value(c.key), o.worst);
      count[c.key] += o.values;
    }
  }
  qint64 total = 0;
  QStringList line;
  for (const Case& c : cs) {
    line << QString("%1 %2").arg(c.key).arg(worst.value(c.key), 0, 'g', 2);
    total += count.value(c.key);
  }
  std::printf("    %d functions × %d bar sets, %lld values compared (PineTS %.1f s); largest relative difference: %s\n", int(cs.size()),
              int(sets().size()), static_cast<long long>(total), pineSec, line.join(", ").toUtf8().constData());
  TA_Shutdown();
}
