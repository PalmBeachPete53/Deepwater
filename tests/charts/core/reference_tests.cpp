// Port of quant-terminal test/reference.test.js (MIT, Copyright (c) 2026 Lewis Jackson).
// Cross-check every built-in indicator against a second, deliberately naive implementation
// written straight from TradingView's published Pine formulas. Nothing here imports the
// app's helpers: every window is recomputed from scratch, bar by bar, so a shared bug can't
// hide. Checked on the last 400 bars of real BTC daily data.
// (The two volume profile tests of the JS file test public/js/volume-profile.js, which is ported with the chart
// UI, not here.)
#include "Check.h"
#include "charts/core/Indicators.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QSet>
#include <QTimeZone>
#include <optional>
#include <tuple>

using namespace charts;

namespace {
using Out = QVector<QPair<QString, Series>>;   // { key: values } in order
using Ref = std::function<Out()>;

const Bars& bars() {
  static const Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    if (!f.open(QIODevice::ReadOnly)) return Bars();
    Bars out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out << Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(), o.value("high").toDouble(),
                 o.value("low").toDouble(), o.value("close").toDouble(), o.value("volume").toDouble()};
    }
    return out;
  }();
  return b;
}
template <class F> Series field(F f) {
  Series s;
  for (const Bar& b : bars()) s << f(b);
  return s;
}
int NB() { return bars().size(); }
Series O() { return field([](const Bar& b) { return b.open; }); }
Series H() { return field([](const Bar& b) { return b.high; }); }
Series L() { return field([](const Bar& b) { return b.low; }); }
Series C() { return field([](const Bar& b) { return b.close; }); }
Series V() { return field([](const Bar& b) { return b.volume; }); }
Series HL2() { return field([](const Bar& b) { return (b.high + b.low) / 2; }); }
Series HLC3() { return field([](const Bar& b) { return (b.high + b.low + b.close) / 3; }); }
bool ok(double v) { return std::isfinite(v); }
template <class F> Series mapi(qsizetype n, F f) {
  Series s(n);
  for (qsizetype i = 0; i < n; ++i) s[i] = f(i);
  return s;
}

// ---- naive building blocks: each one recomputes its window from scratch ----
std::optional<Series> win(const Series& s, qsizetype i, int n) {
  if (i - n + 1 < 0) return std::nullopt;
  return s.mid(i - n + 1, n);
}
bool allOk(const std::optional<Series>& w) {
  if (!w) return false;
  for (double v : *w)
    if (!ok(v)) return false;
  return true;
}
double total(const Series& w) {
  double a = 0;
  for (double v : w) a += v;
  return a;
}
Series SMA(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) { const auto w = win(s, i, n); return allOk(w) ? total(*w) / n : NaN; });
}
Series SUM(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) { const auto w = win(s, i, n); return allOk(w) ? total(*w) : NaN; });
}
Series WMA(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) {
    const auto w = win(s, i, n);
    if (!allOk(w)) return NaN;
    double a = 0, d = 0;
    for (int k = 0; k < w->size(); ++k) {
      a += (*w)[k] * (k + 1);
      d += k + 1;
    }
    return a / d;
  });
}
Series STDEV(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) {
    const auto w = win(s, i, n);
    if (!allOk(w)) return NaN;
    const double m = total(*w) / n;
    double a = 0;
    for (double v : *w) a += (v - m) * (v - m);
    return std::sqrt(a / n);
  });
}
Series DEV(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) {
    const auto w = win(s, i, n);
    if (!allOk(w)) return NaN;
    const double m = total(*w) / n;
    double a = 0;
    for (double v : *w) a += std::fabs(v - m);
    return a / n;
  });
}
Series HIGHEST(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) { const auto w = win(s, i, n); return allOk(w) ? *std::max_element(w->begin(), w->end()) : NaN; });
}
Series LOWEST(const Series& s, int n) {
  return mapi(s.size(), [&](qsizetype i) { const auto w = win(s, i, n); return allOk(w) ? *std::min_element(w->begin(), w->end()) : NaN; });
}
// Pine's ta.ema / ta.rma: seeded with an SMA of the first full window, then recursive.
Series smooth(const Series& s, int n, double alpha) {
  Series out(s.size(), NaN);
  double prev = NaN;
  for (qsizetype i = 0; i < s.size(); ++i) {
    if (!ok(s[i])) {
      prev = NaN;
      continue;
    }
    if (!ok(prev)) {
      const auto w = win(s, i, n);
      if (allOk(w)) prev = total(*w) / n;
      else continue;
    } else prev = alpha * s[i] + (1 - alpha) * prev;
    out[i] = prev;
  }
  return out;
}
Series EMA(const Series& s, int n) { return smooth(s, n, 2.0 / (n + 1)); }
Series RMA(const Series& s, int n) { return smooth(s, n, 1.0 / n); }
Series CHANGE(const Series& s, int n = 1) { return mapi(s.size(), [&](qsizetype i) { return i >= n ? s[i] - s[i - n] : NaN; }); }
Series TR() {
  const Series c = C();
  return mapi(NB(), [&](qsizetype i) {
    const Bar& b = bars()[i];
    return i ? std::max({b.high - b.low, std::fabs(b.high - c[i - 1]), std::fabs(b.low - c[i - 1])}) : b.high - b.low;
  });
}
Series ATR(int n) { return RMA(TR(), n); }
Series RSI(const Series& s, int n) {
  const Series ch = CHANGE(s);
  const Series u = RMA(mapi(ch.size(), [&](qsizetype i) { return ok(ch[i]) ? std::max(ch[i], 0.0) : NaN; }), n);
  const Series d = RMA(mapi(ch.size(), [&](qsizetype i) { return ok(ch[i]) ? std::max(-ch[i], 0.0) : NaN; }), n);
  return mapi(u.size(), [&](qsizetype i) { return d[i] == 0 ? 100 : u[i] == 0 ? 0 : 100 - 100 / (1 + u[i] / d[i]); });
}
Series add(const Series& a, const Series& b) { return mapi(a.size(), [&](qsizetype i) { return a[i] + b[i]; }); }
Series sub(const Series& a, const Series& b) { return mapi(a.size(), [&](qsizetype i) { return a[i] - b[i]; }); }
Series mul(const Series& a, double k) { return mapi(a.size(), [&](qsizetype i) { return a[i] * k; }); }
double jsRoundRef(double v) { return std::floor(v + 0.5); }
double sign(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

// Chop Zone's angle in whole degrees: the EMA slope as an angle, negative when the 34 EMA is falling.
// acos(1 / sqrt(1 + y^2)) is atan(|y|), so this uses the arctangent form.
Series chopAngle() {
  const Series e = EMA(C(), 34), hh = HIGHEST(H(), 30), ll = LOWEST(L(), 30), hlc3 = HLC3();
  return mapi(NB(), [&](qsizetype i) {
    if (!i) return NaN;
    const double y = ((e[i - 1] - e[i]) / hlc3[i]) * ((25 / (hh[i] - ll[i])) * ll[i]);
    const double deg = jsRoundRef((180 * std::atan(std::fabs(y))) / 3.14159265358979323846);
    return y > 0 ? -deg : deg;
  });
}

Out dmiRef() {
  const Series up = CHANGE(H()), dn = mul(CHANGE(L()), -1);
  const Series pdm = mapi(up.size(), [&](qsizetype i) { return ok(up[i]) ? (up[i] > dn[i] && up[i] > 0 ? up[i] : 0) : NaN; });
  const Series mdm = mapi(dn.size(), [&](qsizetype i) { return ok(dn[i]) ? (dn[i] > up[i] && dn[i] > 0 ? dn[i] : 0) : NaN; });
  const Series t = TR();
  const Series tr = RMA(mapi(t.size(), [&](qsizetype i) { return i ? t[i] : NaN; }), 14);
  const Series pr = RMA(pdm, 14), mr = RMA(mdm, 14);
  const Series plus = mapi(pr.size(), [&](qsizetype i) { return (100 * pr[i]) / tr[i]; });
  const Series minus = mapi(mr.size(), [&](qsizetype i) { return (100 * mr[i]) / tr[i]; });
  const Series dx = mapi(plus.size(), [&](qsizetype i) {
    const double s = plus[i] + minus[i];
    return (100 * std::fabs(plus[i] - minus[i])) / (s == 0 ? 1 : s);
  });
  return {{"adx", RMA(dx, 14)}, {"plus", plus}, {"minus", minus}};
}
Out bbtrendRef() {
  struct Band { Series m, u, l; };
  auto band = [](int n) {
    const Series m = SMA(C(), n), sd = STDEV(C(), n);
    return Band{m, add(m, mul(sd, 2)), sub(m, mul(sd, 2))};
  };
  const Band s = band(20), l = band(50);
  return {{"value", mapi(s.m.size(), [&](qsizetype i) {
             return ((std::fabs(s.l[i] - l.l[i]) - std::fabs(s.u[i] - l.u[i])) / s.m[i]) * 100;
           })}};
}

// ---- TradingView's formulas, one per indicator ----
const QVector<QPair<QString, Ref>>& REF() {
  static const QVector<QPair<QString, Ref>> refs = {
      {"sma", [] { return Out{{"value", SMA(C(), 20)}}; }},
      {"ema", [] { return Out{{"value", EMA(C(), 9)}}; }},
      {"wma", [] { return Out{{"value", WMA(C(), 20)}}; }},
      {"vwma", [] {
         const Series c = C(), v = V();
         const Series pv = mapi(c.size(), [&](qsizetype i) { return c[i] * v[i]; });
         const Series a = SMA(pv, 20), b = SMA(v, 20);
         return Out{{"value", mapi(a.size(), [&](qsizetype i) { return a[i] / b[i]; })}};
       }},
      {"hma", [] { return Out{{"value", WMA(sub(mul(WMA(C(), 10), 2), WMA(C(), 20)), int(std::floor(std::sqrt(20))))}}; }},
      {"dema", [] {
         const Series e = EMA(C(), 20);
         return Out{{"value", sub(mul(e, 2), EMA(e, 20))}};
       }},
      {"tema", [] {
         const Series e1 = EMA(C(), 20), e2 = EMA(e1, 20), e3 = EMA(e2, 20);
         return Out{{"value", add(mul(sub(e1, e2), 3), e3)}};
       }},
      {"smma", [] { return Out{{"value", RMA(C(), 20)}}; }},
      {"alma", [] {
         const int n = 20;
         const double m = 0.85 * (n - 1), s = n / 6.0;
         const Series c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) {
                       if (i < n - 1) return NaN;
                       double norm = 0, sum = 0;
                       for (int k = 0; k < n; ++k) {
                         const double w = std::exp(-((k - m) * (k - m)) / (2 * s * s));
                         norm += w;
                         sum += c[i - (n - 1 - k)] * w;
                       }
                       return sum / norm;
                     })}};
       }},
      {"lsma", [] {
         const Series c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) {
                       const auto w = win(c, i, 20);
                       if (!w) return NaN;
                       const double mx = 9.5, my = total(*w) / 20;
                       double num = 0, den = 0;
                       for (int k = 0; k < 20; ++k) {
                         num += (k - mx) * ((*w)[k] - my);
                         den += (k - mx) * (k - mx);
                       }
                       return my + num / den * (19 - mx);
                     })}};
       }},
      {"macross", [] { return Out{{"fast", EMA(C(), 9)}, {"slow", EMA(C(), 21)}}; }},
      {"ribbon", [] { return Out{{"m20", EMA(C(), 20)}, {"m50", EMA(C(), 50)}, {"m100", EMA(C(), 100)}, {"m200", EMA(C(), 200)}}; }},
      {"vwap", [] { return Out{{"value", HLC3()}}; }},   // daily bars: every bar is its own session
      {"bb", [] {
         const Series b = SMA(C(), 20), sd = STDEV(C(), 20);
         return Out{{"upper", add(b, mul(sd, 2))}, {"basis", b}, {"lower", sub(b, mul(sd, 2))}};
       }},
      {"bbpb", [] {
         const Series b = SMA(C(), 20), sd = STDEV(C(), 20), c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return (c[i] - (b[i] - 2 * sd[i])) / (4 * sd[i]); })}};
       }},
      {"bbw", [] {
         const Series b = SMA(C(), 20), sd = STDEV(C(), 20);
         return Out{{"value", mapi(b.size(), [&](qsizetype i) { return ((4 * sd[i]) / b[i]) * 100; })}};
       }},
      {"kc", [] {
         const Series b = EMA(C(), 20), a = ATR(10);
         return Out{{"upper", add(b, mul(a, 2))}, {"basis", b}, {"lower", sub(b, mul(a, 2))}};
       }},
      {"donchian", [] {
         const Series u = HIGHEST(H(), 20), l = LOWEST(L(), 20);
         return Out{{"upper", u}, {"lower", l}, {"basis", mapi(u.size(), [&](qsizetype i) { return (u[i] + l[i]) / 2; })}};
       }},
      {"envelope", [] {
         const Series b = SMA(C(), 20);
         return Out{{"upper", mul(b, 1.1)}, {"basis", b}, {"lower", mul(b, 0.9)}};
       }},
      {"atr", [] { return Out{{"value", ATR(14)}}; }},
      {"stdev", [] { return Out{{"value", STDEV(C(), 20)}}; }},
      {"hv", [] {
         const Series c = C();
         return Out{{"value", mul(STDEV(mapi(c.size(), [&](qsizetype i) { return i ? std::log(c[i] / c[i - 1]) : NaN; }), 10),
                                   100 * std::sqrt(365))}};
       }},
      {"rsi", [] {
         const Series r = RSI(C(), 14);
         return Out{{"value", r}, {"ma", SMA(r, 14)}};
       }},
      {"stoch", [] {
         const Series hh = HIGHEST(H(), 14), ll = LOWEST(L(), 14), c = C();
         const Series k = mapi(c.size(), [&](qsizetype i) { return (100 * (c[i] - ll[i])) / (hh[i] - ll[i]); });
         return Out{{"k", k}, {"d", SMA(k, 3)}};
       }},
      {"stochrsi", [] {
         const Series r = RSI(C(), 14), hh = HIGHEST(r, 14), ll = LOWEST(r, 14);
         const Series raw = mapi(r.size(), [&](qsizetype i) { return (100 * (r[i] - ll[i])) / (hh[i] - ll[i]); });
         const Series k = SMA(raw, 3);
         return Out{{"k", k}, {"d", SMA(k, 3)}};
       }},
      {"macd", [] {
         const Series m = sub(EMA(C(), 12), EMA(C(), 26)), s = EMA(m, 9);
         return Out{{"macd", m}, {"signal", s}, {"hist", sub(m, s)}};
       }},
      {"cci", [] {
         const Series h = HLC3(), s = SMA(h, 20), d = DEV(h, 20);
         return Out{{"value", mapi(h.size(), [&](qsizetype i) { return (h[i] - s[i]) / (0.015 * d[i]); })}};
       }},
      {"willr", [] {
         const Series hh = HIGHEST(H(), 14), ll = LOWEST(L(), 14), c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return (100 * (c[i] - hh[i])) / (hh[i] - ll[i]); })}};
       }},
      {"roc", [] {
         const Series c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return i >= 9 ? (100 * (c[i] - c[i - 9])) / c[i - 9] : NaN; })}};
       }},
      {"mom", [] { return Out{{"value", CHANGE(C(), 10)}}; }},
      {"mfi", [] {
         const Series h = HLC3(), ch = CHANGE(h), v = V();
         const Series up = SUM(mapi(h.size(), [&](qsizetype i) { return ch[i] > 0 ? h[i] * v[i] : ok(ch[i]) ? 0 : NaN; }), 14);
         const Series dn = SUM(mapi(h.size(), [&](qsizetype i) { return ch[i] < 0 ? h[i] * v[i] : ok(ch[i]) ? 0 : NaN; }), 14);
         return Out{{"value", mapi(up.size(), [&](qsizetype i) { return 100 - 100 / (1 + up[i] / dn[i]); })}};
       }},
      {"cmo", [] {
         const Series m = CHANGE(C());
         const Series s1 = SUM(mapi(m.size(), [&](qsizetype i) { return ok(m[i]) ? std::max(m[i], 0.0) : NaN; }), 9);
         const Series s2 = SUM(mapi(m.size(), [&](qsizetype i) { return ok(m[i]) ? std::max(-m[i], 0.0) : NaN; }), 9);
         return Out{{"value", mapi(s1.size(), [&](qsizetype i) { return (100 * (s1[i] - s2[i])) / (s1[i] + s2[i]); })}};
       }},
      {"trix", [] {
         const Series c = C();
         const Series lc = mapi(c.size(), [&](qsizetype i) { return std::log(c[i]); });
         return Out{{"value", mul(CHANGE(EMA(EMA(EMA(lc, 18), 18), 18)), 10000)}};
       }},
      {"ao", [] { return Out{{"value", sub(SMA(HL2(), 5), SMA(HL2(), 34))}}; }},
      {"uo", [] {
         const Series c = C(), l = L(), h = H();
         const Series pc = mapi(c.size(), [&](qsizetype i) { return i ? c[i - 1] : NaN; });
         // Math.min / Math.max with NaN give NaN
         const Series lo = mapi(l.size(), [&](qsizetype i) { return ok(pc[i]) ? std::min(l[i], pc[i]) : NaN; });
         const Series hi = mapi(h.size(), [&](qsizetype i) { return ok(pc[i]) ? std::max(h[i], pc[i]) : NaN; });
         const Series bp = mapi(c.size(), [&](qsizetype i) { return c[i] - lo[i]; });
         const Series tr = mapi(hi.size(), [&](qsizetype i) { return hi[i] - lo[i]; });
         auto avg = [&](int n) {
           const Series a = SUM(bp, n), b = SUM(tr, n);
           return mapi(a.size(), [&](qsizetype i) { return a[i] / b[i]; });
         };
         const Series a7 = avg(7), a14 = avg(14), a28 = avg(28);
         return Out{{"value", mapi(a7.size(), [&](qsizetype i) { return (100 * (4 * a7[i] + 2 * a14[i] + a28[i])) / 7; })}};
       }},
      {"tsi", [] {
         const Series pc = CHANGE(C());
         auto ds = [](const Series& s) { return EMA(EMA(s, 25), 13); };
         const Series a = ds(pc), b = ds(mapi(pc.size(), [&](qsizetype i) { return std::fabs(pc[i]); }));
         const Series t = mapi(a.size(), [&](qsizetype i) { return (100 * a[i]) / b[i]; });
         return Out{{"value", t}, {"signal", EMA(t, 13)}};
       }},
      {"ppo", [] {
         const Series f = EMA(C(), 12), s = EMA(C(), 26);
         const Series v = mapi(f.size(), [&](qsizetype i) { return (100 * (f[i] - s[i])) / s[i]; });
         return Out{{"value", v}, {"signal", EMA(v, 9)}};
       }},
      {"coppock", [] {
         const Series c = C();
         auto roc = [&](int n) { return mapi(c.size(), [&](qsizetype i) { return i >= n ? (100 * (c[i] - c[i - n])) / c[i - n] : NaN; }); };
         return Out{{"value", WMA(add(roc(14), roc(11)), 10)}};
       }},
      {"elder", [] {
         const Series e = EMA(C(), 13);
         return Out{{"bull", sub(H(), e)}, {"bear", sub(L(), e)}};
       }},
      {"bop", [] {
         const Series c = C(), o = O(), h = H(), l = L();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return (c[i] - o[i]) / (h[i] - l[i]); })}};
       }},
      {"dmi", [] { return dmiRef(); }},
      {"adx", [] { return Out{{"adx", dmiRef()[0].second}}; }},
      {"aroon", [] {
         const Series h = H(), l = L();
         return Out{{"up", mapi(h.size(), [&](qsizetype i) {
                       const auto w = win(h, i, 15);
                       if (!w) return NaN;
                       int k = 0;
                       for (int j = 0; j < w->size(); ++j)
                         if ((*w)[j] >= (*w)[k]) k = j;
                       return (100.0 * k) / 14;
                     })},
                    {"down", mapi(l.size(), [&](qsizetype i) {
                       const auto w = win(l, i, 15);
                       if (!w) return NaN;
                       int k = 0;
                       for (int j = 0; j < w->size(); ++j)
                         if ((*w)[j] <= (*w)[k]) k = j;
                       return (100.0 * k) / 14;
                     })}};
       }},
      {"vortex", [] {
         const Series h = H(), l = L();
         const Series vmp = SUM(mapi(h.size(), [&](qsizetype i) { return i ? std::fabs(h[i] - l[i - 1]) : NaN; }), 14);
         const Series vmm = SUM(mapi(l.size(), [&](qsizetype i) { return i ? std::fabs(l[i] - h[i - 1]) : NaN; }), 14);
         const Series str = SUM(TR(), 14);
         return Out{{"plus", mapi(vmp.size(), [&](qsizetype i) { return vmp[i] / str[i]; })},
                    {"minus", mapi(vmm.size(), [&](qsizetype i) { return vmm[i] / str[i]; })}};
       }},
      {"chop", [] {
         const Series s = SUM(TR(), 14), hh = HIGHEST(H(), 14), ll = LOWEST(L(), 14);
         return Out{{"value", mapi(s.size(), [&](qsizetype i) { return (100 * std::log10(s[i] / (hh[i] - ll[i]))) / std::log10(14); })}};
       }},
      {"mass", [] {
         const Series span = sub(H(), L()), e1 = EMA(span, 9), e2 = EMA(e1, 9);
         return Out{{"value", SUM(mapi(e1.size(), [&](qsizetype i) { return e1[i] / e2[i]; }), 10)}};
       }},
      {"psar", nullptr},   // matched to technicalindicators above, and a step-by-step test lives in indicators.test.js
      {"supertrend", [] {
         const Series a = ATR(10), hl2 = HL2(), c = C();
         Series up, down;
         double pu = NaN, pl = NaN, pst = NaN;
         for (int i = 0; i < NB(); ++i) {
           if (!ok(a[i])) {
             up << NaN;
             down << NaN;
             continue;
           }
           double u = hl2[i] + 3 * a[i], l = hl2[i] - 3 * a[i];
           if (ok(pl)) l = l > pl || c[i - 1] < pl ? l : pl;
           if (ok(pu)) u = u < pu || c[i - 1] > pu ? u : pu;
           const int dir = !ok(pst) ? 1 : pst == pu ? (c[i] > u ? -1 : 1) : (c[i] < l ? 1 : -1);
           const double st = dir == -1 ? l : u;
           up << (dir == -1 ? st : NaN);
           down << (dir == 1 ? st : NaN);
           pu = u;
           pl = l;
           pst = st;
         }
         return Out{{"up", up}, {"down", down}};
       }},
      {"ichimoku", [] {
         auto mid = [](int n) {
           const Series hh = HIGHEST(H(), n), ll = LOWEST(L(), n);
           return mapi(hh.size(), [&](qsizetype i) { return (hh[i] + ll[i]) / 2; });
         };
         return Out{{"conversion", mid(9)}, {"base", mid(26)}};
       }},
      {"pivots", [] {
         // Daily bars, auto period = month: levels for a month come from the previous calendar month.
         auto mk = [](qint64 t) {
           const QDate d = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date();
           return d.year() * 12 + d.month() - 1;
         };
         Out out = {{"p", {}}, {"r1", {}}, {"s1", {}}, {"r2", {}}, {"s2", {}}, {"r3", {}}, {"s3", {}}};
         for (const Bar& b : bars()) {
           Bars prev;
           for (const Bar& x : bars())
             if (mk(x.time) == mk(b.time) - 1) prev << x;
           if (prev.isEmpty() || mk(bars()[0].time) == mk(b.time) - 1) {
             for (auto& o : out) o.second << NaN;
             continue;
           }
           double h = -INFINITY, l = INFINITY;
           for (const Bar& x : prev) {
             h = std::max(h, x.high);
             l = std::min(l, x.low);
           }
           const double c = prev.last().close, P = (h + l + c) / 3;
           out[0].second << P;
           out[1].second << 2 * P - l;
           out[2].second << 2 * P - h;
           out[3].second << P + h - l;
           out[4].second << P - (h - l);
           out[5].second << h + 2 * (P - l);
           out[6].second << l - 2 * (h - P);
         }
         return out;
       }},
      {"volume", [] { return Out{{"value", V()}, {"ma", SMA(V(), 20)}}; }},
      {"obv", [] {
         const Series c = C(), v = V();
         double s = 0;
         return Out{{"value", mapi(c.size(), [&](qsizetype i) {
                       return s += i == 0 ? 0 : c[i] > c[i - 1] ? v[i] : c[i] < c[i - 1] ? -v[i] : 0;
                     })}};
       }},
      {"cmf", [] {
         const Series ad = field([](const Bar& b) {
           return b.high == b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * b.volume;
         });
         const Series a = SUM(ad, 20), v = SUM(V(), 20);
         return Out{{"value", mapi(a.size(), [&](qsizetype i) { return a[i] / v[i]; })}};
       }},
      {"ad", [] {
         double s = 0;
         return Out{{"value", field([&](const Bar& b) {
                       return s += b.high == b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * b.volume;
                     })}};
       }},
      {"pvt", [] {
         const Series c = C(), v = V();
         double s = 0;
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return i ? (s += ((c[i] - c[i - 1]) / c[i - 1]) * v[i]) : NaN; })}};
       }},
      {"eom", [] {
         const Series ch = CHANGE(HL2()), h = H(), l = L(), v = V();
         return Out{{"value", SMA(mapi(ch.size(), [&](qsizetype i) { return (10000 * ch[i] * (h[i] - l[i])) / v[i]; }), 14)}};
       }},
      {"patterns", nullptr},     // markers, not a series
      {"rsibars", nullptr},      // candle colours, checked in indicators.test.js
      {"vpvr", nullptr},         // drawn by the chart, checked in the volume profile test below
      {"markov", nullptr},       // checked in the Markov regime test below
      {"avwap", nullptr},        // checked in the anchored VWAP test below
      {"svp", nullptr},          // drawn by the chart, checked in the session volume profile test below
      {"bigtrades", nullptr},
      {"heatmap", nullptr},
      {"liqmap", nullptr},
      {"smcliq", nullptr},       // drawn levels from charts/core/LiquidityLevels, checked in liquiditylevels_tests.cpp and sessionlevels_tests.cpp       // an estimate drawn from Binance statistics, checked in liquidation_tests.cpp / liqrender_tests.cpp      // drawn by the chart from the order book stream, checked in heatmaprender_tests.cpp    // drawn by the chart from the trade stream, checked in bigtrades_tests.cpp
      {"pdhl", [] {
         // Daily bars, period = day: each bar shows the bar before it.
         return Out{{"high", mapi(NB(), [](qsizetype i) { return i ? bars()[i - 1].high : NaN; })},
                    {"low", mapi(NB(), [](qsizetype i) { return i ? bars()[i - 1].low : NaN; })}};
       }},
      {"chaikinosc", [] {
         double s = 0;
         const Series adl = field([&](const Bar& b) {
           return s += b.high == b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * b.volume;
         });
         return Out{{"value", sub(EMA(adl, 3), EMA(adl, 10))}};
       }},
      {"cks", [] {
         const Series a = ATR(10), hh = HIGHEST(H(), 10), ll = LOWEST(L(), 10);
         const Series fh = mapi(hh.size(), [&](qsizetype i) { return hh[i] - a[i]; });
         const Series fl = mapi(ll.size(), [&](qsizetype i) { return ll[i] + a[i]; });
         return Out{{"stopShort", HIGHEST(fh, 9)}, {"stopLong", LOWEST(fl, 9)}};
       }},
      {"chopzone", [] {
         const Series a = chopAngle();
         return Out{{"value", mapi(a.size(), [&](qsizetype i) { return ok(a[i]) ? 1 : NaN; })}};   // the colours are checked below
       }},
      {"crsi", [] {
         // Streak by looking back for the run of same-direction closes, rather than carrying a counter.
         const Series c = C();
         const Series streak = mapi(c.size(), [&](qsizetype i) {
           const double d = i ? sign(c[i] - c[i - 1]) : 0;
           int k = 0;
           while (d != 0 && i - k >= 1 && sign(c[i - k] - c[i - k - 1]) == d) k++;
           return d * k;
         });
         const Series roc1 = mapi(c.size(), [&](qsizetype i) { return i ? (100 * (c[i] - c[i - 1])) / c[i - 1] : NaN; });
         const Series rank = mapi(roc1.size(), [&](qsizetype i) {
           if (i < 100) return NaN;
           const Series w = roc1.mid(i - 100, 100);
           if (!allOk(w)) return NaN;
           return double(std::count_if(w.begin(), w.end(), [&](double y) { return y <= roc1[i]; }));
         });
         const Series a = RSI(c, 3), b = RSI(streak, 2);
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return (a[i] + b[i] + rank[i]) / 3; })}};
       }},
      {"dpo", [] {
         const Series ma = SMA(C(), 21), c = C();   // close - the average from floor(21 / 2) + 1 = 11 bars back
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return i - 11 >= 0 ? c[i] - ma[i - 11] : NaN; })}};
       }},
      {"efi", [] {
         const Series ch = CHANGE(C()), v = V();
         return Out{{"value", EMA(mapi(ch.size(), [&](qsizetype i) { return ch[i] * v[i]; }), 13)}};
       }},
      {"fisher", [] {
         const Series hl2 = HL2(), hi = HIGHEST(hl2, 9), lo = LOWEST(hl2, 9);
         Series fisher(NB(), NaN);
         double v = 0, f = 0;
         for (int i = 0; i < NB(); ++i) {
           if (!ok(hi[i])) continue;
           const double r = hi[i] == lo[i] ? 0 : (hl2[i] - lo[i]) / (hi[i] - lo[i]);
           v = 0.66 * (r - 0.5) + 0.67 * v;
           v = v > 0.99 ? 0.999 : v < -0.99 ? -0.999 : v;
           f = std::atanh(v) + 0.5 * f;   // 0.5 * ln((1 + v) / (1 - v)) is atanh(v)
           fisher[i] = f;
         }
         return Out{{"fisher", fisher}, {"trigger", mapi(NB(), [&](qsizetype i) { return i ? fisher[i - 1] : NaN; })}};
       }},
      {"kvo", [] {
         const Series ch = CHANGE(HLC3()), v = V();
         const Series sv = mapi(ch.size(), [&](qsizetype i) { return ch[i] >= 0 ? v[i] : -v[i]; });
         const Series k = sub(EMA(sv, 34), EMA(sv, 55));
         return Out{{"kvo", k}, {"signal", EMA(k, 13)}};
       }},
      {"kst", [] {
         const Series c = C();
         auto sr = [&](int r, int s) {
           return SMA(mapi(c.size(), [&](qsizetype i) { return i >= r ? (100 * (c[i] - c[i - r])) / c[i - r] : NaN; }), s);
         };
         const Series k = add(add(sr(10, 10), mul(sr(15, 10), 2)), add(mul(sr(20, 10), 3), mul(sr(30, 15), 4)));
         return Out{{"kst", k}, {"signal", SMA(k, 9)}};
       }},
      {"mcginley", [] {
         const Series e = EMA(C(), 14), c = C();
         double mg = NaN;
         return Out{{"value", mapi(c.size(), [&](qsizetype i) {
                       mg = ok(mg) ? mg + (c[i] - mg) / (14 * std::pow(c[i] / mg, 4)) : e[i];
                       return mg;
                     })}};
       }},
      {"netvol", [] {
         const Series c = C(), v = V();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) { return i && c[i] > c[i - 1] ? v[i] : i && c[i] < c[i - 1] ? -v[i] : 0; })}};
       }},
      {"pivotshl", nullptr},   // markers, checked in the pivot high/low test below
      {"rvgi", [] {
         auto sw = [](const Series& s) {
           return mapi(s.size(), [&](qsizetype i) { return i >= 3 ? (s[i - 3] + 2 * s[i - 2] + 2 * s[i - 1] + s[i]) / 6 : NaN; });
         };
         const Series num = SUM(sw(sub(C(), O())), 10), den = SUM(sw(sub(H(), L())), 10);
         const Series v = mapi(num.size(), [&](qsizetype i) { return num[i] / den[i]; });
         return Out{{"value", v}, {"signal", sw(v)}};
       }},
      {"rvi", [] {
         const Series sd = STDEV(C(), 10), ch = CHANGE(C());
         const Series up = EMA(mapi(ch.size(), [&](qsizetype i) { return ch[i] <= 0 ? 0 : sd[i]; }), 14);
         const Series dn = EMA(mapi(ch.size(), [&](qsizetype i) { return ch[i] > 0 ? 0 : sd[i]; }), 14);
         return Out{{"value", mapi(up.size(), [&](qsizetype i) { return (100 * up[i]) / (up[i] + dn[i]); })}};
       }},
      {"rci", [] {
         // These closes never tie inside a 9-bar window, so Spearman's 6 * sum(d^2) form is exact here.
         const Series c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) {
                       const auto w = win(c, i, 9);
                       if (!w) return NaN;
                       double d2 = 0;
                       for (int k = 0; k < 9; ++k) {
                         const int rank = 1 + int(std::count_if(w->begin(), w->end(), [&](double y) { return y < (*w)[k]; }));
                         d2 += double(rank - (k + 1)) * (rank - (k + 1));
                       }
                       return 100 * (1 - (6 * d2) / (9 * (81 - 1)));
                     })}};
       }},
      {"smiergodic", [] {
         const Series pc = CHANGE(C());
         // Pine's ta.tsi(close, 5, 20): the long EMA first, then the short (PineTS, third_party/pinets; audit D19)
         const Series num = EMA(EMA(pc, 20), 5), den = EMA(EMA(mapi(pc.size(), [&](qsizetype i) { return std::fabs(pc[i]); }), 20), 5);
         const Series smi = mapi(num.size(), [&](qsizetype i) { return num[i] / den[i]; }), signal = EMA(smi, 5);
         return Out{{"smi", smi}, {"signal", signal}, {"osc", sub(smi, signal)}};
       }},
      {"vstop", [] {
         const Series a = ATR(20), c = C(), tr = TR();
         Series up(NB(), NaN), down(NB(), NaN);
         double hiC = c[0], loC = c[0], stop = NaN;
         bool trendUp = true;
         for (int i = 0; i < NB(); ++i) {
           const double m = ok(a[i]) ? 2 * a[i] : tr[i];
           hiC = std::max(hiC, c[i]);
           loC = std::min(loC, c[i]);
           if (!ok(stop)) stop = c[i];
           else stop = trendUp ? std::max(stop, hiC - m) : std::min(stop, loC + m);
           const bool nowUp = c[i] - stop >= 0;
           if (nowUp != trendUp) {
             hiC = loC = c[i];
             stop = nowUp ? hiC - m : loC + m;
           }
           trendUp = nowUp;
           (trendUp ? up : down)[i] = stop;
         }
         return Out{{"up", up}, {"down", down}};
       }},
      {"volosc", [] {
         const Series f = EMA(V(), 5), s = EMA(V(), 10);
         return Out{{"value", mapi(f.size(), [&](qsizetype i) { return (100 * (f[i] - s[i])) / s[i]; })}};
       }},
      {"alligator", [] {
         // Each line is drawn that many bars ahead of the bar it was computed on.
         auto ahead = [](const Series& s, int k) { return mapi(s.size(), [&](qsizetype i) { return i >= k ? s[i - k] : NaN; }); };
         return Out{{"jaw", ahead(RMA(HL2(), 13), 8)}, {"teeth", ahead(RMA(HL2(), 8), 5)}, {"lips", ahead(RMA(HL2(), 5), 3)}};
       }},
      {"fractals", nullptr},   // markers, checked in the Williams fractals test below
      {"zigzag", nullptr},     // checked in the Zig Zag tests below
      {"twap", [] { return Out{{"value", field([](const Bar& b) { return (b.open + b.high + b.low + b.close) / 4; })}}; }},   // daily bars: every bar is its own day
      {"bbtrend", [] { return bbtrendRef(); }},
      {"trendstrength", [] {
         // r = (n*sum(xy) - sum(x)*sum(y)) / sqrt((n*sum(x^2) - sum(x)^2) * (n*sum(y^2) - sum(y)^2)), x = 0..13
         const Series c = C();
         return Out{{"value", mapi(c.size(), [&](qsizetype i) {
                       const auto w = win(c, i, 14);
                       if (!w) return NaN;
                       double sx = 0, sy = 0, sxy = 0, sxx = 0, syy = 0;
                       for (int x = 0; x < 14; ++x) {
                         const double y = (*w)[x];
                         sx += x;
                         sy += y;
                         sxy += x * y;
                         sxx += x * x;
                         syy += y * y;
                       }
                       return (14 * sxy - sx * sy) / std::sqrt((14 * sxx - sx * sx) * (14 * syy - sy * sy));
                     })}};
       }},
      {"guppy", [] {
         Out out;
         for (int n : {3, 5, 8, 10, 12, 15, 30, 35, 40, 45, 50, 60}) out << qMakePair(QString("ema%1").arg(n), EMA(C(), n));
         return out;
       }},
  };
  return refs;
}

// Small hand-made bars: highs and lows only matter, the rest follows from them.
Bars hand(const QVector<double>& highs, const QVector<double>& lows) {
  Bars out;
  for (int i = 0; i < highs.size(); ++i)
    out << Bar{1700000000 + qint64(i) * 86400, (highs[i] + lows[i]) / 2, highs[i], lows[i], (highs[i] + lows[i]) / 2, 1};
  return out;
}
Bars pricesToBars(const QVector<double>& p) { return hand(p, p); }
Bars flatBars(const QVector<double>& closes, qint64 t0 = 1700000000) {
  Bars out;
  for (int i = 0; i < closes.size(); ++i) out << Bar{t0 + qint64(i) * 86400, closes[i], closes[i], closes[i], closes[i], 1};
  return out;
}
bool near(double a, double b) { return std::fabs(a - b) <= 1e-7 * std::max(1.0, std::fabs(b)); }
int indexOfTime(const Bars& b, qint64 t) {
  for (int i = 0; i < b.size(); ++i)
    if (b[i].time == t) return i;
  return -1;
}
QVector<int> breakIdx(const ind::Result& r) {
  QVector<int> out;
  for (int i = 0; i < r.breaks.size(); ++i)
    if (r.breaks[i]) out << i;
  return out;
}
}  // namespace

TEST("reference: the reference covers every built-in indicator") {
  QSet<QString> ids;
  for (const auto& r : REF()) ids << r.first;
  QStringList missing;
  for (const ind::Def& d : ind::all())
    if (!d.id.startsWith("custom:") && !ids.contains(d.id)) missing << d.id;
  CHECK_EQ(missing, QStringList());
}

TEST("reference: every indicator matches TradingView's formula on the last 400 bars") {
  const int N = NB();
  for (const auto& [id, ref] : REF()) {
    if (!ref) continue;
    const ind::Result ours = ind::compute(id, bars());
    for (const auto& [key, want] : ref()) {
      if (!ours.series.contains(key)) {
        check::fail(__FILE__, __LINE__, id + "." + key + " missing");
        continue;
      }
      const Series got = ours.value(key);
      int compared = 0;
      for (int i = N - 400; i < N; ++i) {
        if (!ok(want[i])) continue;
        if (!ok(got.value(i, NaN))) {
          check::fail(__FILE__, __LINE__, QString("%1.%2[%3] is %4, expected %5").arg(id, key).arg(i).arg(got.value(i, NaN)).arg(want[i]));
          break;
        }
        const double tol = 1e-7 * std::max(1.0, std::fabs(want[i]));
        if (std::fabs(got[i] - want[i]) > tol) {
          check::fail(__FILE__, __LINE__, QString("%1.%2[%3] = %4, expected %5").arg(id, key).arg(i).arg(got[i], 0, 'g', 15).arg(want[i], 0, 'g', 15));
          break;
        }
        compared++;
      }
      if (compared <= 100) check::fail(__FILE__, __LINE__, QString("%1.%2 only had %3 comparable bars").arg(id, key).arg(compared));
    }
  }
}

// Markov regime: states follow the trailing-return rule, odds are probabilities, and no bar uses
// a transition that hadn't happened yet.
TEST("reference: Markov regime labels states by trailing return and its odds never look ahead") {
  const Params p{{"lookback", 20}, {"threshold", 5}, {"minHistory", 50}};
  const ind::Result r = ind::compute("markov", bars(), p);
  const Series state = r.value("state"), signal = r.value("signal"), pbull = r.value("pbull"), pbear = r.value("pbear");
  for (int i = 20; i < NB(); ++i) {
    const double ret = bars()[i].close / bars()[i - 20].close - 1;
    CHECK_EQ(state[i], ret >= 0.05 ? 1.0 : ret <= -0.05 ? -1.0 : 0.0);
    if (std::isfinite(signal[i])) {
      CHECK(pbull[i] >= 0 && pbull[i] <= 1 && pbear[i] >= 0 && pbear[i] <= 1);
      CHECK(std::fabs(signal[i] - (pbull[i] - pbear[i])) < 1e-12);
    }
  }
  const Series cut = ind::compute("markov", bars().mid(0, 600), p).value("signal");
  for (int i = 0; i < 600; ++i)
    if (std::isfinite(cut[i])) CHECK(std::fabs(cut[i] - signal[i]) < 1e-12);
}

TEST("reference: anchored VWAP starts at the anchor bar and is the volume-weighted mean of hlc3 from there") {
  const int a = 200;
  const qint64 anchor = bars()[a].time;
  const ind::Result r = ind::compute("avwap", bars(), {{"anchor", double(anchor)}, {"source", "hlc3"}, {"band1", 1}, {"band2", 2}});
  const Series value = r.value("value"), upper1 = r.value("upper1"), lower2 = r.value("lower2");
  for (int i = 0; i < a; ++i) CHECK(std::isnan(value[i]));
  auto h = [](const Bar& b) { return (b.high + b.low + b.close) / 3; };
  CHECK(std::fabs(value[a] - h(bars()[a])) < 1e-9);   // the anchor bar is its own price
  double pv = 0, pv2 = 0, v = 0;
  for (int i = a; i < a + 60; ++i) {
    pv += h(bars()[i]) * bars()[i].volume;
    pv2 += h(bars()[i]) * h(bars()[i]) * bars()[i].volume;
    v += bars()[i].volume;
    const double vw = pv / v, sd = std::sqrt(std::max(0.0, pv2 / v - vw * vw));
    CHECK(std::fabs(value[i] - vw) < 1e-6 * vw);
    CHECK(std::fabs(upper1[i] - (vw + sd)) < 1e-6 * vw && std::fabs(lower2[i] - (vw - 2 * sd)) < 1e-6 * vw);
  }
  const ind::Result later =
      ind::compute("avwap", bars(), {{"anchor", double(bars().last().time + 86400)}, {"source", "hlc3"}, {"band1", 1}, {"band2", 0}});
  for (double x : later.value("value")) CHECK(std::isnan(x));   // an anchor after the last bar draws nothing
  const ind::Result none = ind::compute("avwap", bars(), {{"anchor", 0}, {"source", "hlc3"}, {"band1", 0}, {"band2", 0}});
  const Series nv = none.value("value");
  int first = -1;
  for (int i = 0; i < nv.size() && first < 0; ++i)
    if (!std::isnan(nv[i])) first = i;
  CHECK_EQ(first, NB() - 150);   // no anchor yet: starts 150 bars back
  for (double x : none.value("upper1")) CHECK(std::isnan(x));   // band 0 is off
}

TEST("reference: periodKey groups daily bars into calendar months with no gaps") {
  // The JS session volume profile test groups bars by periodKey(t, "month"); the grouping half is checked here.
  QStringList keys;
  for (const Bar& b : bars()) keys << ind::periodKey(b.time, "month");
  int sessions = 1;
  for (int i = 1; i < keys.size(); ++i)
    if (keys[i] != keys[i - 1]) sessions++;
  CHECK(sessions > 12);
  for (const Bar& b : bars()) {
    const QDate d = QDateTime::fromSecsSinceEpoch(b.time, QTimeZone::UTC).date();
    CHECK_EQ(ind::periodKey(b.time, "month"), QString("%1-%2").arg(d.year()).arg(d.month() - 1));
    CHECK_EQ(ind::periodKey(b.time, "day"), QString("%1-%2-%3").arg(d.year()).arg(d.month() - 1).arg(d.day()));
  }
}

TEST("reference: previous day high/low on intraday bars uses the whole previous day") {
  const qint64 t0 = 1767571200;   // Date.UTC(2026, 0, 5) / 1000
  const QVector<QPair<double, double>> hl = {{10, 5}, {12, 7}, {11, 6}, {9, 4}, {20, 15}, {18, 16}, {17, 14}, {19, 13}, {30, 25}};
  Bars six;
  for (int i = 0; i < hl.size(); ++i) six << Bar{t0 + i * 6 * 3600, hl[i].second, hl[i].first, hl[i].second, hl[i].first, 1};
  const ind::Result r = ind::compute("pdhl", six);
  const Series high = r.value("high"), low = r.value("low");
  for (int i = 0; i < 4; ++i) CHECK(std::isnan(high[i]));   // nothing before day two
  CHECK_EQ(high.mid(4, 4), Series({12, 12, 12, 12}));
  CHECK_EQ(low.mid(4, 4), Series({4, 4, 4, 4}));
  CHECK_EQ(high[8], 20.0);
  CHECK_EQ(low[8], 13.0);
  CHECK_EQ(breakIdx(r), QVector<int>({0, 4, 8}));
  const ind::Result w = ind::compute("pdhl", six, {{"period", "week"}});
  for (double x : w.value("high")) CHECK(std::isnan(x));   // one week of bars has no previous week
}

TEST("reference: histogram colours follow their rules") {
  const ind::Result chop = ind::compute("chopzone", bars());
  const Series ang = chopAngle();
  auto want = [](double a) -> QString {
    for (const auto& [t, c] : QVector<QPair<double, QString>>{{5, "#26C6DA"}, {3.57, "#43A047"}, {2.14, "#A5D6A7"}, {0.71, "#009688"}})
      if (a >= t) return c;
    for (const auto& [t, c] : QVector<QPair<double, QString>>{{-5, "#D50000"}, {-3.57, "#E91E63"}, {-2.14, "#FF6D00"}, {-0.71, "#FFB74D"}})
      if (a <= t) return c;
    return "#FDD835";
  };
  QSet<QString> seen;
  for (int i = 0; i < NB(); ++i)
    if (ok(ang[i])) {
      CHECK_EQ(chop.colors[i], want(ang[i]));
      seen << chop.colors[i];
    }
  CHECK(seen.size() >= 5);   // the fixture should visit most of the palette

  const ind::Result nv = ind::compute("netvol", bars());
  const Series c = C(), v = V();
  for (int i = 1; i < NB(); ++i) {
    if (c[i] == c[i - 1]) continue;
    const bool rose = c[i] > c[i - 1];
    CHECK_EQ(nv.value("value")[i], rose ? v[i] : -v[i]);
    CHECK_EQ(nv.colors[i], QString(rose ? "rgba(38,166,154,0.6)" : "rgba(239,83,80,0.6)"));
  }

  const ind::Result bt = ind::compute("bbtrend", bars());
  const Series ref = bbtrendRef()[0].second;
  for (int i = 0; i < NB(); ++i)
    if (ok(ref[i])) CHECK_EQ(bt.colors[i], QString(ref[i] >= 0 ? "#26a69a" : "#ef5350"));
}

TEST("reference: detrended price oscillator, centered: each close against the average centred on it") {
  const Series ma = SMA(C(), 21), c = C();
  const Series r = ind::compute("dpo", bars(), {{"centered", "On"}}).value("value");
  const int N = NB();
  CHECK_EQ(r.size(), N);
  for (int i = N - 400; i < N - 11; ++i) CHECK(near(r[i], c[i] - ma[i + 11]));
  for (int i = N - 11; i < N; ++i) CHECK(std::isnan(r[i]));   // the newest 11 bars have no average ahead of them yet
}

TEST("reference: alligator lines run past the last bar by their offsets") {
  const ind::Result r = ind::compute("alligator", bars());
  const int N = NB();
  CHECK_EQ(r.extra.value("extendBars").toDouble(), 8.0);
  CHECK_EQ(r.value("jaw").size(), N + 8);
  CHECK_EQ(r.value("teeth").size(), N + 5);
  CHECK_EQ(r.value("lips").size(), N + 3);
  for (const auto& [key, len, k] : {std::tuple{"jaw", 13, 8}, std::tuple{"teeth", 8, 5}, std::tuple{"lips", 5, 3}}) {
    const Series line = RMA(HL2(), len), got = r.value(key);
    for (int j = 0; j < k; ++j) CHECK(near(got[N + j], line[N - k + j]));
  }
}

TEST("reference: TWAP averages ohlc4 since the period began and starts over each period") {
  const qint64 t0 = 1767571200;   // a Monday
  const QVector<double> px = {10, 20, 30, 40, 100, 200, 300, 400, 1000};
  const QVector<int> hours = {0, 6, 12, 18, 24, 30, 36, 42, 7 * 24};
  Bars six;
  for (int i = 0; i < px.size(); ++i) six << Bar{t0 + hours[i] * 3600, px[i], px[i], px[i], px[i], 1};
  const ind::Result day = ind::compute("twap", six);
  CHECK_EQ(day.value("value"), Series({10, 15, 20, 25, 100, 150, 200, 250, 1000}));
  CHECK_EQ(breakIdx(day), QVector<int>({4, 8}));
  const ind::Result week = ind::compute("twap", six, {{"anchor", "week"}});
  Series rounded;
  for (int i = 0; i < 8; ++i) rounded << std::round(week.value("value")[i] * 1e6) / 1e6;
  CHECK_EQ(rounded, Series({10, 15, 20, 25, 40, 66.666667, 100, 137.5}));
  CHECK_EQ(week.value("value")[8], 1000.0);
  CHECK_EQ(breakIdx(week), QVector<int>({8}));
  const ind::Result month = ind::compute("twap", six, {{"anchor", "month"}});
  CHECK(near(month.value("value")[8], 2100.0 / 9));
  CHECK_EQ(breakIdx(month), QVector<int>());
}

TEST("reference: VWAP on daily bars draws a joined line, and an anchor makes it run over the week") {
  const qint64 t0 = 1767571200;   // a Monday
  Bars daily;
  for (int i = 0; i < 6; ++i) {
    const double p = 10.0 * (i + 1);
    daily << Bar{t0 + i * 86400, p, p, p, p, 1};
  }
  const ind::Result day = ind::compute("vwap", daily);
  CHECK_EQ(day.value("value"), Series({10, 20, 30, 40, 50, 60}));
  CHECK(!day.breaks.contains(true));   // no breaks when each session is one bar
  CHECK_EQ(ind::compute("vwap", daily, {{"anchor", "week"}}).value("value"), Series({10, 15, 20, 25, 30, 35}));
  Bars hourly;
  const int at[] = {0, 6, 24, 30};
  for (int i = 0; i < 4; ++i) hourly << Bar{t0 + at[i] * 3600, double(i + 1), double(i + 1), double(i + 1), double(i + 1), 1};
  CHECK_EQ(ind::compute("vwap", hourly).breaks, QVector<bool>({false, false, true, false}));
}

TEST("reference: pivot points high low: a bar is a pivot once the bars either side are lower, ties go to the earlier bar") {
  const Bars b = hand({5, 6, 9, 7, 6, 6, 8, 10, 10, 7, 7}, {4, 5, 8, 6, 5, 5, 7, 9, 3, 6, 6});
  QStringList got;
  for (const ind::Marker& m : ind::compute("pivotshl", b, {{"left", 2}, {"right", 2}}).markers)
    got << QString("%1 %2 %3").arg(indexOfTime(b, m.time)).arg(m.position, m.text);
  // bar 5 has the same low as bar 4, but the earlier bar wins; so does bar 8 with the same high as bar 7
  CHECK_EQ(got, QStringList({"2 aboveBar 9", "4 belowBar 5", "7 aboveBar 10", "8 belowBar 3"}));
  // On real data every marker is the extreme of its own window of left + right + 1 bars.
  const QVector<ind::Marker> m = ind::compute("pivotshl", bars()).markers;
  CHECK(m.size() > 10);
  for (const ind::Marker& k : m) {
    const int c = indexOfTime(bars(), k.time);
    CHECK(c >= 10 && c + 10 < NB());   // pivots need ten bars on each side
    double hi = -INFINITY, lo = INFINITY;
    for (int i = c - 10; i <= c + 10; ++i) {
      hi = std::max(hi, bars()[i].high);
      lo = std::min(lo, bars()[i].low);
    }
    CHECK_EQ(k.position == "aboveBar" ? hi : lo, k.text.toDouble());
  }
}

TEST("reference: Williams fractals: two lower bars each side, equal bars tolerated on the older side only") {
  const Bars b = hand({15, 16, 19, 17, 16, 16, 18, 18, 17, 16, 15, 14}, {6, 5, 4, 5, 3, 4, 5, 5, 2, 2, 4, 5});
  QStringList got;
  for (const ind::Marker& m : ind::compute("fractals", b).markers)
    got << QString("%1 %2 %3").arg(indexOfTime(b, m.time)).arg(m.position, m.shape);
  // bar 6 matches bar 7's high, bar 7 still counts; bar 8 matches bar 9's low, bar 9 still counts
  CHECK_EQ(got, QStringList({"2 aboveBar arrowDown", "4 belowBar arrowUp", "7 aboveBar arrowDown", "9 belowBar arrowUp"}));
  const QVector<ind::Marker> real = ind::compute("fractals", bars()).markers;
  CHECK(real.size() > 50);
  for (const ind::Marker& m : real) CHECK(indexOfTime(bars(), m.time) <= NB() - 3);   // the newest two bars carry no fractal
}

TEST("reference: Zig Zag: straight legs between pivots, the last leg ends at the latest extreme") {
  auto z = [](const QVector<double>& p, double deviation, double depth) {
    return ind::compute("zigzag", pricesToBars(p), {{"deviation", deviation}, {"depth", depth}}).value("value");
  };
  auto same = [](const Series& got, const Series& want) {
    CHECK_EQ(got.size(), want.size());
    for (int i = 0; i < want.size() && i < got.size(); ++i)
      if (std::isnan(want[i]) ? !std::isnan(got[i]) : !near(got[i], want[i]))
        check::fail(__FILE__, __LINE__, QString("bar %1: %2, expected %3").arg(i).arg(got[i]).arg(want[i]));
  };
  const double nan = NaN;
  // Pivots: high 109 (bar 3), low 94 (7), high 117 (11), then the open leg down to 99 (15).
  same(z({100, 102, 105, 109, 107, 103, 97, 94, 99, 105, 111, 117, 114, 108, 103, 99, 100, 101}, 10, 3),
       {nan, nan, nan, 109, 105.25, 101.5, 97.75, 94, 99.75, 105.5, 111.25, 117, 112.5, 108, 103.5, 99, nan, nan});
  // A 10% swing that is too close to the last pivot is ignored with depth 3 and kept with depth 1.
  same(z({100, 120, 105, 130, 100}, 10, 3), {100, 110, 120, 130, 100});
  same(z({100, 120, 105, 130, 100}, 10, 1), {100, 120, 105, 130, 100});
  // Falling through the last low before a pivot can be confirmed moves that low down.
  same(z({100, 120, 90, 95, 100, 105}, 10, 3), {nan, nan, 90, 95, 100, 105});
  same(z({100, 120, 90, 95, 100, 105}, 10, 1), {100, 120, 90, 95, 100, 105});
  same(z({100, 80, 110, 105, 100, 95}, 10, 3), {nan, nan, 110, 105, 100, 95});
  same(z({100, 80, 110, 105, 100, 95}, 10, 1), {100, 80, 110, 105, 100, 95});
  same(z({100, 101, 102, 101, 100}, 10, 3), {nan, nan, nan, nan, nan});

  // On real data the legs alternate, each is at least 5% of the pivot it leaves, and confirmed pivots sit 10+ bars apart.
  const Series v = ind::compute("zigzag", bars()).value("value");
  const int N = NB();
  int first = 0, last = N - 1;
  while (first < N && !ok(v[first])) first++;
  while (last >= 0 && !ok(v[last])) last--;
  QVector<int> turns{first};
  for (int i = first + 1; i < last; ++i)
    if (sign(v[i] - v[i - 1]) != sign(v[i + 1] - v[i])) turns << i;
  turns << last;
  CHECK(turns.size() > 10);
  for (int k = 0; k + 1 < turns.size(); ++k) {
    const double a = v[turns[k]], b = v[turns[k + 1]];
    CHECK(std::fabs(b - a) / a >= 0.05 - 1e-9);
    if (k + 2 < turns.size()) CHECK(turns[k + 1] - turns[k] >= 10);
    if (k + 2 < turns.size()) CHECK(sign(b - a) != sign(v[turns[k + 2]] - b));
  }
}

TEST("reference: Fisher Transform clamps its value, so a straight run settles instead of blowing up") {
  QVector<double> upP, downP;
  for (int i = 0; i < 80; ++i) {
    upP << 100 + i;
    downP << 200 - i;
  }
  const double limit = 2 * std::atanh(0.999);   // fish = atanh(0.999) + 0.5 * fish settles at twice that
  CHECK(near(ind::compute("fisher", pricesToBars(upP)).value("fisher")[79], limit));
  CHECK(near(ind::compute("fisher", pricesToBars(downP)).value("fisher")[79], -limit));
}

TEST("reference: Connors RSI ranks today's move against the values at or below it, like ta.percentrank") {
  // Alternating closes repeat the same two rates of change exactly.
  const QVector<double> closes = {100, 110, 100, 110, 100, 110, 100, 110}, streak = {0, 1, -1, 1, -1, 1, -1, 1};
  QVector<double> streakCloses;
  for (double s : streak) streakCloses << 100 + s;
  const Series crsi = ind::compute("crsi", flatBars(closes), {{"rankLength", 2}}).value("value");
  const Series price = ind::compute("rsi", flatBars(closes), {{"length", 3}, {"maLength", 0}}).value("value");
  const Series run = ind::compute("rsi", flatBars(streakCloses), {{"length", 2}, {"maLength", 0}}).value("value");
  // At bar 5 the move is +10%; the two before it are -9.09% and +10%, both at or below it: 100.
  CHECK(near(3 * crsi[5] - price[5] - run[5], 100));
  CHECK(near(3 * crsi[6] - price[6] - run[6], 50));   // -9.09% against +10% and -9.09%: one at or below
}

TEST("reference: Rank Correlation Index gives tied closes the average of their ranks") {
  // Price ranks 1, 2.5, 2.5, 4 against time ranks 1..4: r = 4.5 / sqrt(5 * 4.5) = sqrt(0.9).
  CHECK(near(ind::compute("rci", flatBars({1, 2, 2, 3}), {{"length", 4}}).value("value")[3], 100 * std::sqrt(0.9)));
  CHECK(near(ind::compute("rci", flatBars({1, 2, 3, 4}), {{"length", 4}}).value("value")[3], 100));
  CHECK(near(ind::compute("rci", flatBars({4, 3, 2, 1}), {{"length", 4}}).value("value")[3], -100));
}

TEST("reference: Chande Kroll Stop starts once both of its windows are full") {
  const ind::Result r = ind::compute("cks", bars());   // ATR and the 10-bar extreme need 10 bars, the stop needs 9 of those
  auto firstOk = [](const Series& s) {
    for (int i = 0; i < s.size(); ++i)
      if (ok(s[i])) return i;
    return -1;
  };
  CHECK_EQ(firstOk(r.value("stopShort")), 17);
  CHECK_EQ(firstOk(r.value("stopLong")), 17);
}
