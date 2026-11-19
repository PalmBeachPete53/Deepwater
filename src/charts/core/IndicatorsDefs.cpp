// Port of quant-terminal public/lib/indicators.js (MIT, Copyright (c) 2026 Lewis Jackson): the registry
// (INDICATORS, BY_ID, defaults, compute, registerIndicator). Same order, ids, names, groups, inputs, outputs and
// result keys as the JS.
#include "charts/core/Indicators.h"
#include <QJsonArray>
#include <QReadWriteLock>
#include <algorithm>
#include <cmath>
#include <list>
#include <memory>

namespace charts::ind {

namespace {
const double N = NaN;
inline bool isN(double v) { return !std::isnan(v); }
inline double at(const Series& s, qsizetype k) { return k >= 0 && k < s.size() ? s[k] : N; }
inline Series nans(qsizetype n) { return Series(std::max<qsizetype>(0, n), N); }
inline double jsSign(double v) { return std::isnan(v) ? v : v > 0 ? 1 : v < 0 ? -1 : v; }
inline const Bar& barAt(const Bars& bars, qsizetype k, const char* field) {
  if (k < 0 || k >= bars.size())
    throw std::runtime_error(std::string("Cannot read properties of undefined (reading '") + field + "')");
  return bars[k];
}

// Param readers. Lengths are whole numbers (see Indicators.h).
inline double D(const Params& p, const char* k) { return num(p, QString::fromLatin1(k)); }
// A length as the JS formulas use p[k]: null and "" count as 0 (null - 1, i >= null), text that isn't a number
// ("abc") as NaN, which gives kNaNLength (the JS windows never fill: NaN results, see Indicators.h).
inline int I(const Params& p, const char* k) {
  const QString key = QString::fromLatin1(k);
  const QJsonValue v = p.value(key);
  if (v.isNull() || (v.isString() && v.toString().trimmed().isEmpty())) return 0;
  return lengthOf(num(p, key));
}
inline QString S(const Params& p, const char* k) { return str(p, QString::fromLatin1(k)); }

// ---------- registry ----------
// Each indicator: inputs (shown in its settings), outputs (one chart series each), and calc().
// overlay: true draws on the price pane; false gets its own pane.
Input inp(const QString& key, const QString& label, const QString& type, const QJsonValue& def, double min = NaN,
          const QStringList& options = {}) {
  Input i;
  i.key = key;
  i.label = label;
  i.type = type;
  i.def = def;
  i.min = min;
  i.options = options;
  return i;
}
const Input src = inp("source", QT_TRANSLATE_NOOP("charts", "Source"), "source", "close");
Input srcDefault(const QString& d) {
  Input i = src;
  i.def = d;
  return i;
}
Input len(int d, const QString& label = QT_TRANSLATE_NOOP("charts", "Length")) { return inp("length", label, "int", d, 1); }
Input intIn(const QString& key, const QString& label, int d) { return inp(key, label, "int", d); }
Input floatIn(const QString& key, const QString& label, double d) { return inp(key, label, "float", d); }
Output line(const QString& key, const QString& label, const QString& color, int lineWidth = 0, QJsonObject extra = {}) {
  Output o;
  o.key = key;
  o.label = label;
  o.kind = "line";
  o.color = color;
  o.lineWidth = lineWidth;
  o.extra = extra;
  return o;
}
Output hist(const QString& key, const QString& label, const QString& color) {
  Output o = line(key, label, color);
  o.kind = "histogram";
  return o;
}
// An output that reads later bars (with the params for which `when` is true): drawn, never used by a rule (audit D12).
Output drawOnly(Output o, std::function<bool(const Params&)> when) {
  o.readsAhead = std::move(when);
  return o;
}

const QString MA_COLORS = "#2962ff";
constexpr double kPi = 3.14159265358979323846;   // Math.PI (M_PI is not standard C++)

// Guppy: six fast EMAs in shades of cyan, six slow ones in shades of red.
const QVector<QPair<int, QString>> GUPPY = {
    {3, "#80deea"},  {5, "#4dd0e1"},  {8, "#26c6da"},  {10, "#00bcd4"}, {12, "#00acc1"}, {15, "#0097a7"},
    {30, "#ef9a9a"}, {35, "#e57373"}, {40, "#ef5350"}, {45, "#e53935"}, {50, "#d32f2f"}, {60, "#b71c1c"},
};

using Calc = std::function<Result(const Bars&, const Params&)>;
Def def(const QString& id, const QString& name, const QString& group, bool overlay, QVector<Input> inputs,
        QVector<Output> outputs, Calc calc, QVector<double> levels = {}, QJsonObject extra = {}) {
  Def d;
  d.id = id;
  d.name = name;
  d.group = group;
  d.overlay = overlay;
  d.inputs = std::move(inputs);
  d.outputs = std::move(outputs);
  d.calc = std::move(calc);
  d.levels = std::move(levels);
  d.extra = std::move(extra);
  return d;
}
Result one(const QString& key, Series s) {
  Result r;
  r.put(key, std::move(s));
  return r;
}
template <class F> Series mapi(qsizetype n, F f) {
  Series out(n);
  for (qsizetype i = 0; i < n; ++i) out[i] = f(i);
  return out;
}

Def maDef(const QString& id, const QString& name, std::function<Series(const Series&, int, const Params&, const Bars&)> fn,
          QVector<Input> extraInputs = {}) {
  QVector<Input> inputs{len(id == "ema" ? 9 : 20), src};
  inputs += extraInputs;
  return def(id, name, QT_TRANSLATE_NOOP("charts", "Moving averages"), true, inputs, {line("value", name, MA_COLORS)},
             [fn](const Bars& bars, const Params& p) { return one("value", fn(source(bars, S(p, "source")), I(p, "length"), p, bars)); });
}

QStringList bandColors(const Series& value, const char* up, const char* down) {
  QStringList c;
  for (double v : value) c << (v >= 0 ? up : down);
  return c;
}

// Each bar shows the high and low of the whole period before the one it's in.
Result pdhlCalc(const Bars& bars, const Params& p) {
  const qsizetype n = bars.size();
  Series high = nans(n), low = nans(n);
  QVector<bool> breaks(n, false);
  struct HL { double h, l; };
  QString key;
  bool hasKey = false, hasCur = false, hasPrev = false;
  HL cur{}, prev{};
  const QString period = S(p, "period");
  for (qsizetype i = 0; i < n; ++i) {
    const Bar& b = bars[i];
    const QString k = periodKey(b.time, period);
    if (!hasKey || k != key) {
      prev = cur;
      hasPrev = hasCur;
      cur = {b.high, b.low};
      hasCur = true;
      key = k;
      hasKey = true;
      breaks[i] = true;
    } else {
      cur.h = jsMax({cur.h, b.high});
      cur.l = jsMin({cur.l, b.low});
    }
    if (hasPrev) {
      high[i] = prev.h;
      low[i] = prev.l;
    }
  }
  Result r;
  r.put("high", high);
  r.put("low", low);
  r.putBreaks(breaks);
  return r;
}

QVector<Def> builtins() {
  QVector<Def> L;
  L << maDef("sma", QT_TRANSLATE_NOOP("charts", "Moving Average (SMA)"), [](const Series& s, int n, const Params&, const Bars&) { return sma(s, n); });
  L << maDef("ema", QT_TRANSLATE_NOOP("charts", "Exponential Moving Average (EMA)"), [](const Series& s, int n, const Params&, const Bars&) { return ema(s, n); });
  L << maDef("wma", QT_TRANSLATE_NOOP("charts", "Weighted Moving Average (WMA)"), [](const Series& s, int n, const Params&, const Bars&) { return wma(s, n); });
  L << maDef("vwma", QT_TRANSLATE_NOOP("charts", "Volume Weighted Moving Average (VWMA)"),
             [](const Series& s, int n, const Params&, const Bars& bars) { return vwma(s, source(bars, "volume"), n); });
  L << maDef("hma", QT_TRANSLATE_NOOP("charts", "Hull Moving Average (HMA)"), [](const Series& s, int n, const Params&, const Bars&) { return hma(s, n); });
  L << maDef("dema", QT_TRANSLATE_NOOP("charts", "Double EMA (DEMA)"), [](const Series& s, int n, const Params&, const Bars&) { return dema(s, n); });
  L << maDef("tema", QT_TRANSLATE_NOOP("charts", "Triple EMA (TEMA)"), [](const Series& s, int n, const Params&, const Bars&) { return tema(s, n); });
  L << maDef("smma", QT_TRANSLATE_NOOP("charts", "Smoothed Moving Average (SMMA / RMA)"), [](const Series& s, int n, const Params&, const Bars&) { return rma(s, n); });
  L << maDef("alma", QT_TRANSLATE_NOOP("charts", "Arnaud Legoux Moving Average (ALMA)"),
             [](const Series& s, int n, const Params& p, const Bars&) { return alma(s, n, D(p, "offset"), D(p, "sigma")); },
             {floatIn("offset", QT_TRANSLATE_NOOP("charts", "Offset"), 0.85), floatIn("sigma", QT_TRANSLATE_NOOP("charts", "Sigma"), 6)});
  L << maDef("lsma", QT_TRANSLATE_NOOP("charts", "Least Squares Moving Average (LSMA)"), [](const Series& s, int n, const Params&, const Bars&) { return linreg(s, n, 0); });
  L << def("mcginley", QT_TRANSLATE_NOOP("charts", "McGinley Dynamic"), QT_TRANSLATE_NOOP("charts", "Moving averages"), true, {len(14), src}, {line("value", QT_TRANSLATE_NOOP("charts", "McGinley Dynamic"), MA_COLORS)},
           [](const Bars& bars, const Params& p) {
             // Starts on the first EMA, then chases price faster when price runs away from it.
             const int n = I(p, "length");
             const Series s = source(bars, S(p, "source")), e = ema(s, n);
             double prev = N;
             return one("value", mapi(s.size(), [&](qsizetype i) {
                          const double v = s[i];
                          prev = isN(prev) ? prev + (v - prev) / (n * std::pow(v / prev, 4)) : e[i];
                          return prev;
                        }));
           });
  L << def("macross", QT_TRANSLATE_NOOP("charts", "MA Cross"), QT_TRANSLATE_NOOP("charts", "Moving averages"), true,
           {inp("fast", QT_TRANSLATE_NOOP("charts", "Fast length"), "int", 9, 1), inp("slow", QT_TRANSLATE_NOOP("charts", "Slow length"), "int", 21, 1),
            inp("type", QT_TRANSLATE_NOOP("charts", "Type"), "select", "EMA", NaN, {"EMA", "SMA"}), src},
           {line("fast", QT_TRANSLATE_NOOP("charts", "Fast"), "#2962ff"), line("slow", QT_TRANSLATE_NOOP("charts", "Slow"), "#ff6d00")}, [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source"));
             const bool useSma = S(p, "type") == "SMA";
             const Series fast = useSma ? sma(s, I(p, "fast")) : ema(s, I(p, "fast"));
             const Series slow = useSma ? sma(s, I(p, "slow")) : ema(s, I(p, "slow"));
             const QVector<bool> up = crossover(fast, slow), dn = crossunder(fast, slow);
             QVector<Marker> markers;
             for (qsizetype i = 0; i < bars.size(); ++i) {
               if (up.value(i)) markers << Marker{bars[i].time, "belowBar", "arrowUp", "#26a69a", QT_TRANSLATE_NOOP("charts", "Cross up"), {}};
               if (dn.value(i)) markers << Marker{bars[i].time, "aboveBar", "arrowDown", "#ef5350", QT_TRANSLATE_NOOP("charts", "Cross down"), {}};
             }
             Result r;
             r.put("fast", fast);
             r.put("slow", slow);
             r.putMarkers(markers);
             return r;
           });
  L << def("ribbon", QT_TRANSLATE_NOOP("charts", "Moving Average Ribbon"), QT_TRANSLATE_NOOP("charts", "Moving averages"), true, {src},
           {line("m20", QT_TRANSLATE_NOOP("charts", "MA 20"), "#f6c309"), line("m50", QT_TRANSLATE_NOOP("charts", "MA 50"), "#fb9800"), line("m100", QT_TRANSLATE_NOOP("charts", "MA 100"), "#fb6500"),
            line("m200", QT_TRANSLATE_NOOP("charts", "MA 200"), "#f60c0c")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source"));
             Result r;
             r.put("m20", ema(s, 20));
             r.put("m50", ema(s, 50));
             r.put("m100", ema(s, 100));
             r.put("m200", ema(s, 200));
             return r;
           });
  {
    QVector<Output> outs;
    for (const auto& [n, color] : GUPPY) outs << line(QString("ema%1").arg(n), QString("EMA %1").arg(n), color, 1);
    L << def("guppy", QT_TRANSLATE_NOOP("charts", "Guppy Multiple Moving Average"), QT_TRANSLATE_NOOP("charts", "Moving averages"), true, {}, outs, [](const Bars& bars, const Params&) {
      const Series c = source(bars, "close");
      Result r;
      for (const auto& g : GUPPY) r.put(QString("ema%1").arg(g.first), ema(c, g.first));
      return r;
    });
  }
  L << def("vwap", QT_TRANSLATE_NOOP("charts", "Volume Weighted Average Price (VWAP)"), QT_TRANSLATE_NOOP("charts", "Volume"), true,
           {srcDefault("hlc3"), inp("anchor", QT_TRANSLATE_NOOP("charts", "Anchor"), "select", "day", NaN, {QT_TRANSLATE_NOOP("charts", "day"), QT_TRANSLATE_NOOP("charts", "week"), QT_TRANSLATE_NOOP("charts", "month"), QT_TRANSLATE_NOOP("charts", "year")})},
           {line("value", QT_TRANSLATE_NOOP("charts", "VWAP"), "#2962ff", 2)}, [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source"));
             QString anchor = S(p, "anchor");
             if (anchor.isEmpty()) anchor = "day";
             QString key;
             bool hasKey = false;
             int start = 0;
             double pv = 0, v = 0;
             QVector<bool> breaks(bars.size(), false);
             Series value(bars.size());
             for (qsizetype i = 0; i < bars.size(); ++i) {
               const QString k = periodKey(bars[i].time, anchor);
               if (!hasKey || k != key) {
                 breaks[i] = sessionBreak(hasKey, start, int(i));   // new session: don't join it to yesterday's line
                 key = k;
                 hasKey = true;
                 start = int(i);
                 pv = 0;
                 v = 0;
               }
               const double vol = jsOr(bars[i].volume, 0);
               pv += s[i] * vol;
               v += vol;
               value[i] = jsTruthy(v) ? pv / v : s[i];
             }
             Result r;
             r.put("value", value);
             r.putBreaks(breaks);
             return r;
           });
  // VWAP from a bar you pick instead of each session's start. Adding it from the picker asks for a click on the
  // chart; the settings take a date and time. Bands are volume-weighted standard deviations around it.
  L << def("avwap", QT_TRANSLATE_NOOP("charts", "Anchored VWAP"), QT_TRANSLATE_NOOP("charts", "Volume"), true,
           {inp("anchor", QT_TRANSLATE_NOOP("charts", "Anchor"), "time", 0), srcDefault("hlc3"), floatIn("band1", QT_TRANSLATE_NOOP("charts", "Band 1 (StdDev, 0 = off)"), 1),
            floatIn("band2", QT_TRANSLATE_NOOP("charts", "Band 2 (StdDev, 0 = off)"), 2)},
           {line("value", QT_TRANSLATE_NOOP("charts", "Anchored VWAP"), "#ff9800", 2), line("upper1", QT_TRANSLATE_NOOP("charts", "Upper band 1"), "#26a69a"),
            line("lower1", QT_TRANSLATE_NOOP("charts", "Lower band 1"), "#26a69a"), line("upper2", QT_TRANSLATE_NOOP("charts", "Upper band 2"), "#7e57c2"),
            line("lower2", QT_TRANSLATE_NOOP("charts", "Lower band 2"), "#7e57c2")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source"));
             const qsizetype n = bars.size();
             const double anchor = D(p, "anchor"), band1 = D(p, "band1"), band2 = D(p, "band2");
             // No anchor yet: start 150 bars back so the line shows straight away. An anchor before the loaded history
             // starts at the first bar; one after the last bar draws nothing.
             qsizetype a;
             if (!jsTruthy(anchor)) a = std::max<qsizetype>(0, n - 150);
             else if (n && anchor <= double(bars[0].time)) a = 0;
             else {
               a = -1;
               for (qsizetype i = 0; i < n && a < 0; ++i)
                 if (double(bars[i].time) >= anchor) a = i;
               if (a < 0) a = n;
             }
             Series value = nans(n), upper1 = nans(n), lower1 = nans(n), upper2 = nans(n), lower2 = nans(n);
             double pv = 0, pv2 = 0, v = 0;
             for (qsizetype i = a; i < n; ++i) {
               const double vol = jsOr(bars[i].volume, 0);
               pv += s[i] * vol;
               pv2 += s[i] * s[i] * vol;
               v += vol;
               const double vw = jsTruthy(v) ? pv / v : s[i];
               const double sd = jsTruthy(v) ? std::sqrt(jsMax({0, pv2 / v - vw * vw})) : 0;
               value[i] = vw;
               upper1[i] = jsTruthy(band1) ? vw + band1 * sd : N;
               lower1[i] = jsTruthy(band1) ? vw - band1 * sd : N;
               upper2[i] = jsTruthy(band2) ? vw + band2 * sd : N;
               lower2[i] = jsTruthy(band2) ? vw - band2 * sd : N;
             }
             Result r;
             r.put("value", value);
             r.put("upper1", upper1);
             r.put("lower1", lower1);
             r.put("upper2", upper2);
             r.put("lower2", lower2);
             return r;
           });
  L << def("twap", QT_TRANSLATE_NOOP("charts", "Time Weighted Average Price (TWAP)"), QT_TRANSLATE_NOOP("charts", "Volume"), true,
           {inp("anchor", QT_TRANSLATE_NOOP("charts", "Anchor"), "select", "day", NaN, {QT_TRANSLATE_NOOP("charts", "day"), QT_TRANSLATE_NOOP("charts", "week"), QT_TRANSLATE_NOOP("charts", "month")})}, {line("value", QT_TRANSLATE_NOOP("charts", "TWAP"), "#ff6d00", 2)},
           [](const Bars& bars, const Params& p) {
             // Plain average of ohlc4 since the period began, volume ignored. Starts over at each new period.
             const Series s = source(bars, "ohlc4");
             const QString anchor = S(p, "anchor");
             QString key;
             bool hasKey = false;
             int start = 0;
             double total = 0, count = 0;
             QVector<bool> breaks(bars.size(), false);
             Series value(bars.size());
             for (qsizetype i = 0; i < bars.size(); ++i) {
               const QString k = periodKey(bars[i].time, anchor);
               if (!hasKey || k != key) {
                 breaks[i] = sessionBreak(hasKey, start, int(i));
                 key = k;
                 hasKey = true;
                 start = int(i);
                 total = 0;
                 count = 0;
               }
               total += s[i];
               count++;
               value[i] = total / count;
             }
             Result r;
             r.put("value", value);
             r.putBreaks(breaks);
             return r;
           });
  L << def("bb", QT_TRANSLATE_NOOP("charts", "Bollinger Bands"), QT_TRANSLATE_NOOP("charts", "Volatility"), true, {len(20), src, floatIn("mult", QT_TRANSLATE_NOOP("charts", "StdDev"), 2)},
           {line("upper", QT_TRANSLATE_NOOP("charts", "Upper"), "#f23645"), line("basis", QT_TRANSLATE_NOOP("charts", "Basis"), "#2962ff"), line("lower", QT_TRANSLATE_NOOP("charts", "Lower"), "#089981")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source")), basis = sma(s, I(p, "length")), sd = stdev(s, I(p, "length"));
             const double mult = D(p, "mult");
             Result r;
             r.put("basis", basis);
             r.put("upper", mapi(basis.size(), [&](qsizetype i) { return basis[i] + mult * sd[i]; }));
             r.put("lower", mapi(basis.size(), [&](qsizetype i) { return basis[i] - mult * sd[i]; }));
             return r;
           });
  L << def("bbpb", QT_TRANSLATE_NOOP("charts", "Bollinger Bands %B"), QT_TRANSLATE_NOOP("charts", "Volatility"), false, {len(20), src, floatIn("mult", QT_TRANSLATE_NOOP("charts", "StdDev"), 2)},
           {line("value", QT_TRANSLATE_NOOP("charts", "%B"), "#22ab94")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source")), basis = sma(s, I(p, "length")), sd = stdev(s, I(p, "length"));
             const double mult = D(p, "mult");
             return one("value", mapi(s.size(), [&](qsizetype i) {
                          return (s[i] - (basis[i] - mult * sd[i])) / (2 * mult * sd[i]);
                        }));
           },
           {0, 0.5, 1});
  L << def("bbw", QT_TRANSLATE_NOOP("charts", "Bollinger BandWidth"), QT_TRANSLATE_NOOP("charts", "Volatility"), false, {len(20), src, floatIn("mult", QT_TRANSLATE_NOOP("charts", "StdDev"), 2)},
           {line("value", QT_TRANSLATE_NOOP("charts", "BBW"), "#2962ff")}, [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source")), basis = sma(s, I(p, "length")), sd = stdev(s, I(p, "length"));
             const double mult = D(p, "mult");
             return one("value", mapi(basis.size(), [&](qsizetype i) { return ((2 * mult * sd[i]) / basis[i]) * 100; }));
           });
  L << def("bbtrend", QT_TRANSLATE_NOOP("charts", "BBTrend"), QT_TRANSLATE_NOOP("charts", "Volatility"), false,
           {intIn("short", QT_TRANSLATE_NOOP("charts", "Short length"), 20), intIn("long", QT_TRANSLATE_NOOP("charts", "Long length"), 50), floatIn("mult", QT_TRANSLATE_NOOP("charts", "StdDev"), 2)},
           {hist("value", QT_TRANSLATE_NOOP("charts", "BBTrend"), "#26a69a")},
           [](const Bars& bars, const Params& p) {
             const Series c = source(bars, "close");
             const double mult = D(p, "mult");
             struct Bands { Series mid, upper, lower; };
             auto bands = [&](int n) {
               Bands b{sma(c, n), {}, {}};
               const Series sd = stdev(c, n);
               b.upper = mapi(b.mid.size(), [&](qsizetype i) { return b.mid[i] + mult * sd[i]; });
               b.lower = mapi(b.mid.size(), [&](qsizetype i) { return b.mid[i] - mult * sd[i]; });
               return b;
             };
             const Bands s = bands(I(p, "short")), l = bands(I(p, "long"));
             const Series value = mapi(s.mid.size(), [&](qsizetype i) {
               return ((std::fabs(s.lower[i] - l.lower[i]) - std::fabs(s.upper[i] - l.upper[i])) / s.mid[i]) * 100;
             });
             Result r;
             r.put("value", value);
             r.putColors(bandColors(value, "#26a69a", "#ef5350"));
             return r;
           },
           {0});
  L << def("kc", QT_TRANSLATE_NOOP("charts", "Keltner Channels"), QT_TRANSLATE_NOOP("charts", "Volatility"), true,
           {len(20), floatIn("mult", QT_TRANSLATE_NOOP("charts", "Multiplier"), 2), intIn("atrLength", QT_TRANSLATE_NOOP("charts", "ATR length"), 10), src},
           {line("upper", QT_TRANSLATE_NOOP("charts", "Upper"), "#2962ff"), line("basis", QT_TRANSLATE_NOOP("charts", "Basis"), "#2962ff"), line("lower", QT_TRANSLATE_NOOP("charts", "Lower"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series basis = ema(source(bars, S(p, "source")), I(p, "length")), a = atr(bars, I(p, "atrLength"));
             const double mult = D(p, "mult");
             Result r;
             r.put("basis", basis);
             r.put("upper", mapi(basis.size(), [&](qsizetype i) { return basis[i] + mult * at(a, i); }));
             r.put("lower", mapi(basis.size(), [&](qsizetype i) { return basis[i] - mult * at(a, i); }));
             return r;
           });
  L << def("donchian", QT_TRANSLATE_NOOP("charts", "Donchian Channels"), QT_TRANSLATE_NOOP("charts", "Volatility"), true, {len(20)},
           {line("upper", QT_TRANSLATE_NOOP("charts", "Upper"), "#2962ff"), line("basis", QT_TRANSLATE_NOOP("charts", "Basis"), "#ff6d00"), line("lower", QT_TRANSLATE_NOOP("charts", "Lower"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series upper = highest(source(bars, "high"), I(p, "length")), lower = lowest(source(bars, "low"), I(p, "length"));
             Result r;
             r.put("upper", upper);
             r.put("lower", lower);
             r.put("basis", mapi(upper.size(), [&](qsizetype i) { return (upper[i] + lower[i]) / 2; }));
             return r;
           });
  L << def("envelope", QT_TRANSLATE_NOOP("charts", "Envelope"), QT_TRANSLATE_NOOP("charts", "Volatility"), true, {len(20), floatIn("percent", QT_TRANSLATE_NOOP("charts", "Percent"), 10), src},
           {line("upper", QT_TRANSLATE_NOOP("charts", "Upper"), "#2962ff"), line("basis", QT_TRANSLATE_NOOP("charts", "Basis"), "#ff6d00"), line("lower", QT_TRANSLATE_NOOP("charts", "Lower"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series basis = sma(source(bars, S(p, "source")), I(p, "length"));
             const double k = D(p, "percent") / 100;
             Result r;
             r.put("basis", basis);
             r.put("upper", mapi(basis.size(), [&](qsizetype i) { return basis[i] * (1 + k); }));
             r.put("lower", mapi(basis.size(), [&](qsizetype i) { return basis[i] * (1 - k); }));
             return r;
           });
  L << def("atr", QT_TRANSLATE_NOOP("charts", "Average True Range (ATR)"), QT_TRANSLATE_NOOP("charts", "Volatility"), false, {len(14)}, {line("value", QT_TRANSLATE_NOOP("charts", "ATR"), "#b71c1c")},
           [](const Bars& bars, const Params& p) { return one("value", atr(bars, I(p, "length"))); });
  L << def("stdev", QT_TRANSLATE_NOOP("charts", "Standard Deviation"), QT_TRANSLATE_NOOP("charts", "Volatility"), false, {len(20), src}, {line("value", QT_TRANSLATE_NOOP("charts", "StdDev"), "#2962ff")},
           [](const Bars& bars, const Params& p) { return one("value", stdev(source(bars, S(p, "source")), I(p, "length"))); });
  L << def("hv", QT_TRANSLATE_NOOP("charts", "Historical Volatility"), QT_TRANSLATE_NOOP("charts", "Volatility"), false, {len(10), intIn("annual", QT_TRANSLATE_NOOP("charts", "Periods per year"), 365)},
           {line("value", QT_TRANSLATE_NOOP("charts", "HV"), "#2962ff")}, [](const Bars& bars, const Params& p) {
             const Series lr = mapi(bars.size(), [&](qsizetype i) { return i ? std::log(bars[i].close / bars[i - 1].close) : N; });
             const double annual = D(p, "annual");
             Series v = stdev(lr, I(p, "length"));
             for (double& x : v) x = 100 * x * std::sqrt(annual);
             return one("value", v);
           });
  L << def("rvi", QT_TRANSLATE_NOOP("charts", "Relative Volatility Index"), QT_TRANSLATE_NOOP("charts", "Volatility"), false,
           {len(10, QT_TRANSLATE_NOOP("charts", "StdDev length")), intIn("smoothing", QT_TRANSLATE_NOOP("charts", "Smoothing length"), 14)}, {line("value", QT_TRANSLATE_NOOP("charts", "RVI"), "#7e57c2")},
           [](const Bars& bars, const Params& p) {
             // Like RSI, but the up and down halves are the bar-by-bar standard deviation instead of the price change.
             const Series c = source(bars, "close"), ch = change(c), sd = stdev(c, I(p, "length"));
             const Series up = ema(mapi(ch.size(), [&](qsizetype i) { return ch[i] <= 0 ? 0 : sd[i]; }), I(p, "smoothing"));
             const Series down = ema(mapi(ch.size(), [&](qsizetype i) { return ch[i] > 0 ? 0 : sd[i]; }), I(p, "smoothing"));
             return one("value", mapi(up.size(), [&](qsizetype i) { return (100 * up[i]) / (up[i] + down[i]); }));
           });
  L << def("rsi", QT_TRANSLATE_NOOP("charts", "Relative Strength Index (RSI)"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {len(14), src, inp("maLength", QT_TRANSLATE_NOOP("charts", "Smoothing (SMA, 0 = off)"), "int", 14, 0)},
           {line("value", QT_TRANSLATE_NOOP("charts", "RSI"), "#7e57c2"), line("ma", QT_TRANSLATE_NOOP("charts", "RSI-based MA"), "#f7c600", 1)},
           [](const Bars& bars, const Params& p) {
             const Series value = rsi(source(bars, S(p, "source")), I(p, "length"));
             Result r;
             r.put("value", value);
             r.put("ma", D(p, "maLength") > 0 ? sma(value, I(p, "maLength")) : nans(value.size()));
             return r;
           },
           {30, 50, 70});
  // Pine's barcolor(): each candle takes the colour of the RSI zone it closed in.
  L << def("rsibars", QT_TRANSLATE_NOOP("charts", "RSI Candle Colours"), QT_TRANSLATE_NOOP("charts", "Momentum"), true,
           {len(14), src, floatIn("z1", QT_TRANSLATE_NOOP("charts", "Zone 1 below"), 20), floatIn("z2", QT_TRANSLATE_NOOP("charts", "Zone 2 below"), 30), floatIn("z3", QT_TRANSLATE_NOOP("charts", "Zone 3 below"), 50),
            floatIn("z4", QT_TRANSLATE_NOOP("charts", "Zone 4 below"), 70), floatIn("z5", QT_TRANSLATE_NOOP("charts", "Zone 5 below (zone 6 above)"), 80)},
           {}, [](const Bars& bars, const Params& p) {
             static const char* zones[] = {"#1565c0", "#26c6da", "#ef9a9a", "#a5d6a7", "#ff9800", "#d500f9"};
             const double cuts[] = {D(p, "z1"), D(p, "z2"), D(p, "z3"), D(p, "z4"), D(p, "z5")};
             QStringList colors;
             for (double v : rsi(source(bars, S(p, "source")), I(p, "length"))) {
               if (!std::isfinite(v)) {
                 colors << QString();   // null: keep the bar's normal colour
                 continue;
               }
               int k = 0;
               for (double c : cuts)
                 if (v >= c) k++;
               colors << zones[k];
             }
             Result r;
             r.putBarColors(colors);
             return r;
           });
  L << def("crsi", QT_TRANSLATE_NOOP("charts", "Connors RSI"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("rsiLength", QT_TRANSLATE_NOOP("charts", "RSI length"), 3), intIn("streakLength", QT_TRANSLATE_NOOP("charts", "Up/Down length"), 2), intIn("rankLength", QT_TRANSLATE_NOOP("charts", "ROC percent rank"), 100)},
           {line("value", QT_TRANSLATE_NOOP("charts", "CRSI"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             // The average of three readings: RSI of price, RSI of the up/down streak, and where today's 1-bar ROC ranks.
             const Series c = source(bars, "close");
             const Series price = rsi(c, I(p, "rsiLength")), streak = rsi(updownStreak(c), I(p, "streakLength"));
             const Series rank = percentRank(mapi(c.size(), [&](qsizetype i) { return i ? (100 * (c[i] - c[i - 1])) / c[i - 1] : N; }),
                                             I(p, "rankLength"));
             return one("value", mapi(price.size(), [&](qsizetype i) { return (price[i] + streak[i] + rank[i]) / 3; }));
           },
           {30, 70});
  L << def("stoch", QT_TRANSLATE_NOOP("charts", "Stochastic"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("k", QT_TRANSLATE_NOOP("charts", "%K length"), 14), intIn("smoothK", QT_TRANSLATE_NOOP("charts", "%K smoothing"), 1), intIn("d", QT_TRANSLATE_NOOP("charts", "%D smoothing"), 3)},
           {line("k", QT_TRANSLATE_NOOP("charts", "%K"), "#2962ff"), line("d", QT_TRANSLATE_NOOP("charts", "%D"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             const Series k = sma(stochOf(source(bars, "close"), source(bars, "high"), source(bars, "low"), I(p, "k")), I(p, "smoothK"));
             Result r;
             r.put("k", k);
             r.put("d", sma(k, I(p, "d")));
             return r;
           },
           {20, 50, 80});
  L << def("stochrsi", QT_TRANSLATE_NOOP("charts", "Stochastic RSI"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("smoothK", QT_TRANSLATE_NOOP("charts", "K"), 3), intIn("smoothD", QT_TRANSLATE_NOOP("charts", "D"), 3), intIn("rsiLength", QT_TRANSLATE_NOOP("charts", "RSI length"), 14),
            intIn("stochLength", QT_TRANSLATE_NOOP("charts", "Stochastic length"), 14), src},
           {line("k", QT_TRANSLATE_NOOP("charts", "K"), "#2962ff"), line("d", QT_TRANSLATE_NOOP("charts", "D"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             const Series r0 = rsi(source(bars, S(p, "source")), I(p, "rsiLength"));
             const Series k = sma(stochOf(r0, r0, r0, I(p, "stochLength")), I(p, "smoothK"));
             Result r;
             r.put("k", k);
             r.put("d", sma(k, I(p, "smoothD")));
             return r;
           },
           {20, 50, 80});
  L << def("macd", QT_TRANSLATE_NOOP("charts", "MACD"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("fast", QT_TRANSLATE_NOOP("charts", "Fast length"), 12), intIn("slow", QT_TRANSLATE_NOOP("charts", "Slow length"), 26), intIn("signal", QT_TRANSLATE_NOOP("charts", "Signal smoothing"), 9), src},
           {hist("hist", QT_TRANSLATE_NOOP("charts", "Histogram"), "#26a69a"), line("macd", QT_TRANSLATE_NOOP("charts", "MACD"), "#2962ff"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             const Macd m = macdFrom(source(bars, S(p, "source")), I(p, "fast"), I(p, "slow"), I(p, "signal"));
             Result r;
             r.put("macd", m.macd);
             r.put("signal", m.signal);
             r.put("hist", m.hist);
             r.putColors(m.colors);
             return r;
           });
  L << def("cci", QT_TRANSLATE_NOOP("charts", "Commodity Channel Index (CCI)"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(20), srcDefault("hlc3")},
           {line("value", QT_TRANSLATE_NOOP("charts", "CCI"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source")), m = sma(s, I(p, "length")), d = dev(s, I(p, "length"));
             return one("value", mapi(s.size(), [&](qsizetype i) { return (s[i] - m[i]) / (0.015 * d[i]); }));
           },
           {-100, 0, 100});
  L << def("willr", QT_TRANSLATE_NOOP("charts", "Williams %R"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(14)}, {line("value", QT_TRANSLATE_NOOP("charts", "%R"), "#7e57c2")},
           [](const Bars& bars, const Params& p) {
             const Series hh = highest(source(bars, "high"), I(p, "length")), ll = lowest(source(bars, "low"), I(p, "length"));
             return one("value", mapi(bars.size(), [&](qsizetype i) { return (100 * (bars[i].close - hh[i])) / (hh[i] - ll[i]); }));
           },
           {-80, -50, -20});
  L << def("fisher", QT_TRANSLATE_NOOP("charts", "Fisher Transform"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(9)},
           {line("fisher", QT_TRANSLATE_NOOP("charts", "Fisher"), "#2962ff"), line("trigger", QT_TRANSLATE_NOOP("charts", "Trigger"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             const int n = I(p, "length");
             const Series h2 = source(bars, "hl2"), hi = highest(h2, n), lo = lowest(h2, n);
             Series fisher = nans(bars.size());
             double value = 0, fish = 0;
             // The JS loop starts at length - 1 even when that is negative: those steps read undefined for both
             // hi and lo (equal, so ratio 0) and still move value and fish.
             // Those steps converge (value to -0.999, fish to its fixed point) within a few hundred, so a start before
             // -100000 changes nothing but would loop up to 1e9 times for length -1e9.
             for (qsizetype i = std::max<qsizetype>(qsizetype(n) - 1, -100000); i < bars.size(); ++i) {
               // A flat window counts as 0 on the 0..1 scale.
               const double ratio = i < 0 || hi[i] == lo[i] ? 0 : (h2[i] - lo[i]) / (hi[i] - lo[i]);
               // TradingView snaps anything past 0.99 to 0.999 rather than clamping it.
               const double v = 0.66 * (ratio - 0.5) + 0.67 * value;
               value = v > 0.99 ? 0.999 : v < -0.99 ? -0.999 : v;
               fish = 0.5 * std::log((1 + value) / (1 - value)) + 0.5 * fish;
               if (i >= 0) fisher[i] = fish;
             }
             Result r;
             r.put("fisher", fisher);
             r.put("trigger", mapi(fisher.size(), [&](qsizetype i) { return i ? fisher[i - 1] : N; }));
             return r;
           },
           {-1.5, -0.75, 0, 0.75, 1.5});
  L << def("roc", QT_TRANSLATE_NOOP("charts", "Rate Of Change (ROC)"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(9), src}, {line("value", QT_TRANSLATE_NOOP("charts", "ROC"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source"));
             const int n = I(p, "length");
             return one("value", mapi(s.size(), [&](qsizetype i) { return i >= n ? (100 * (s[i] - at(s, i - n))) / at(s, i - n) : N; }));
           },
           {0});
  L << def("mom", QT_TRANSLATE_NOOP("charts", "Momentum"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(10), src}, {line("value", QT_TRANSLATE_NOOP("charts", "Momentum"), "#2962ff")},
           [](const Bars& bars, const Params& p) { return one("value", change(source(bars, S(p, "source")), I(p, "length"))); }, {0});
  L << def("mfi", QT_TRANSLATE_NOOP("charts", "Money Flow Index (MFI)"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {len(14)}, {line("value", QT_TRANSLATE_NOOP("charts", "MFI"), "#7e57c2")},
           [](const Bars& bars, const Params& p) {
             const Series tp = source(bars, "hlc3"), ch = change(tp);
             const Series up = sum(mapi(bars.size(), [&](qsizetype i) {
                                     return isN(ch[i]) ? (ch[i] <= 0 ? 0 : jsOr(bars[i].volume, 0) * tp[i]) : N;
                                   }),
                                   I(p, "length"));
             const Series dn = sum(mapi(bars.size(), [&](qsizetype i) {
                                     return isN(ch[i]) ? (ch[i] >= 0 ? 0 : jsOr(bars[i].volume, 0) * tp[i]) : N;
                                   }),
                                   I(p, "length"));
             return one("value", mapi(up.size(), [&](qsizetype i) { return dn[i] == 0 ? 100 : 100 - 100 / (1 + up[i] / dn[i]); }));
           },
           {20, 50, 80});
  L << def("cmo", QT_TRANSLATE_NOOP("charts", "Chande Momentum Oscillator"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(9), src}, {line("value", QT_TRANSLATE_NOOP("charts", "CMO"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series ch = change(source(bars, S(p, "source")));
             const Series su = sum(mapi(ch.size(), [&](qsizetype i) { return isN(ch[i]) ? jsMax({ch[i], 0}) : N; }), I(p, "length"));
             const Series sd = sum(mapi(ch.size(), [&](qsizetype i) { return isN(ch[i]) ? jsMax({-ch[i], 0}) : N; }), I(p, "length"));
             return one("value", mapi(su.size(), [&](qsizetype i) { return (100 * (su[i] - sd[i])) / (su[i] + sd[i]); }));
           },
           {-50, 0, 50});
  L << def("rci", QT_TRANSLATE_NOOP("charts", "Rank Correlation Index"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(9)}, {line("value", QT_TRANSLATE_NOOP("charts", "RCI"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             // Spearman correlation of time order against price order over the window, x100. Ties share an average rank.
             // A window longer than the bars never fills (all NaN, as in the JS): capped there so a length of 1e9 does not
             // build a 1e9-element time axis (8 GB). And at most 1000 bars: each bar ranks its whole window (an RCI is
             // read on 9 to 52 bars; 5000 on 100,000 bars took minutes on the tester's thread).
             const qsizetype want = I(p, "length");
             const int n = int(want > bars.size() ? bars.size() + 1 : std::min<qsizetype>(want, 1000));
             const Series c = source(bars, "close");
             Series time;
             for (int k = 0; k < n; ++k) time << k + 1;
             return one("value", mapi(c.size(), [&](qsizetype i) {
                          if (i < n - 1) return N;
                          const Series w = n > 0 ? c.mid(i - n + 1, n) : Series();
                          return std::all_of(w.begin(), w.end(), isN) ? 100 * pearson(time, avgRanks(w)) : N;
                        }));
           },
           {0});
  L << def("trix", QT_TRANSLATE_NOOP("charts", "TRIX"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(18)}, {line("value", QT_TRANSLATE_NOOP("charts", "TRIX"), "#f23645")},
           [](const Bars& bars, const Params& p) {
             const int n = I(p, "length");
             const Series l = mapi(bars.size(), [&](qsizetype i) { return std::log(bars[i].close); });
             Series v = change(ema(ema(ema(l, n), n), n));
             for (double& x : v) x = 10000 * x;
             return one("value", v);
           },
           {0});
  L << def("ao", QT_TRANSLATE_NOOP("charts", "Awesome Oscillator"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {}, {hist("value", QT_TRANSLATE_NOOP("charts", "AO"), "#26a69a")}, [](const Bars& bars, const Params&) {
    const Series hl2 = source(bars, "hl2"), slow = sma(hl2, 34), fast = sma(hl2, 5);
    const Series v = mapi(fast.size(), [&](qsizetype i) { return fast[i] - slow[i]; });
    QStringList colors;
    for (qsizetype i = 0; i < v.size(); ++i) colors << (i && v[i] >= v[i - 1] ? "#26a69a" : "#ef5350");
    Result r;
    r.put("value", v);
    r.putColors(colors);
    return r;
  });
  L << def("uo", QT_TRANSLATE_NOOP("charts", "Ultimate Oscillator"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("fast", QT_TRANSLATE_NOOP("charts", "Fast"), 7), intIn("middle", QT_TRANSLATE_NOOP("charts", "Middle"), 14), intIn("slow", QT_TRANSLATE_NOOP("charts", "Slow"), 28)}, {line("value", QT_TRANSLATE_NOOP("charts", "UO"), "#f23645")},
           [](const Bars& bars, const Params& p) {
             const Series bp = mapi(bars.size(), [&](qsizetype i) { return i ? bars[i].close - jsMin({bars[i].low, bars[i - 1].close}) : N; });
             const Series trr = mapi(bars.size(), [&](qsizetype i) {
               return i ? jsMax({bars[i].high, bars[i - 1].close}) - jsMin({bars[i].low, bars[i - 1].close}) : N;
             });
             auto avg = [&](int n) {
               const Series t = sum(trr, n), b = sum(bp, n);
               return mapi(b.size(), [&](qsizetype i) { return b[i] / t[i]; });
             };
             const Series a = avg(I(p, "fast")), b = avg(I(p, "middle")), c = avg(I(p, "slow"));
             return one("value", mapi(a.size(), [&](qsizetype i) { return (100 * (4 * a[i] + 2 * b[i] + c[i])) / 7; }));
           },
           {30, 50, 70});
  L << def("tsi", QT_TRANSLATE_NOOP("charts", "True Strength Index"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("long", QT_TRANSLATE_NOOP("charts", "Long length"), 25), intIn("short", QT_TRANSLATE_NOOP("charts", "Short length"), 13), intIn("signal", QT_TRANSLATE_NOOP("charts", "Signal length"), 13)},
           {line("value", QT_TRANSLATE_NOOP("charts", "TSI"), "#2962ff"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#f23645")},
           [](const Bars& bars, const Params& p) {
             const Series pc = change(source(bars, "close"));
             const Series absPc = mapi(pc.size(), [&](qsizetype i) { return std::fabs(pc[i]); });
             const Series num = ema(ema(pc, I(p, "long")), I(p, "short")), den = ema(ema(absPc, I(p, "long")), I(p, "short"));
             const Series value = mapi(num.size(), [&](qsizetype i) { return (100 * num[i]) / den[i]; });
             Result r;
             r.put("value", value);
             r.put("signal", ema(value, I(p, "signal")));
             return r;
           },
           {0});
  L << def("smiergodic", QT_TRANSLATE_NOOP("charts", "SMI Ergodic Indicator/Oscillator"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("long", QT_TRANSLATE_NOOP("charts", "Long length"), 20), intIn("short", QT_TRANSLATE_NOOP("charts", "Short length"), 5), intIn("signal", QT_TRANSLATE_NOOP("charts", "Signal length"), 5)},
           {hist("osc", QT_TRANSLATE_NOOP("charts", "Oscillator"), "#26a69a"), line("smi", QT_TRANSLATE_NOOP("charts", "SMI"), "#2962ff"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             // The true strength index on a -1..1 scale (not x100), as Pine's ta.tsi(close, short, long): the long EMA
             // first, then the short one (checked against PineTS's ta.tsi in third_party/pinets, audit D19; the order
             // was the other way round).
             const Series pc = change(source(bars, "close"));
             const Series absPc = mapi(pc.size(), [&](qsizetype i) { return std::fabs(pc[i]); });
             const Series num = ema(ema(pc, I(p, "long")), I(p, "short")), den = ema(ema(absPc, I(p, "long")), I(p, "short"));
             const Series smi = mapi(num.size(), [&](qsizetype i) { return num[i] / den[i]; });
             const Series signal = ema(smi, I(p, "signal"));
             Result r;
             r.put("smi", smi);
             r.put("signal", signal);
             r.put("osc", mapi(smi.size(), [&](qsizetype i) { return smi[i] - signal[i]; }));
             return r;
           },
           {0});
  L << def("ppo", QT_TRANSLATE_NOOP("charts", "Price Oscillator (PPO)"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("fast", QT_TRANSLATE_NOOP("charts", "Fast"), 12), intIn("slow", QT_TRANSLATE_NOOP("charts", "Slow"), 26), intIn("signal", QT_TRANSLATE_NOOP("charts", "Signal"), 9), src},
           {line("value", QT_TRANSLATE_NOOP("charts", "PPO"), "#2962ff"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             const Series s = source(bars, S(p, "source")), f = ema(s, I(p, "fast")), sl = ema(s, I(p, "slow"));
             const Series value = mapi(f.size(), [&](qsizetype i) { return ((f[i] - sl[i]) / sl[i]) * 100; });
             Result r;
             r.put("value", value);
             r.put("signal", ema(value, I(p, "signal")));
             return r;
           },
           {0});
  L << def("coppock", QT_TRANSLATE_NOOP("charts", "Coppock Curve"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("wma", QT_TRANSLATE_NOOP("charts", "WMA length"), 10), intIn("long", QT_TRANSLATE_NOOP("charts", "Long ROC"), 14), intIn("short", QT_TRANSLATE_NOOP("charts", "Short ROC"), 11)},
           {line("value", QT_TRANSLATE_NOOP("charts", "Coppock"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series c = source(bars, "close");
             auto roc = [&](int n) {
               return mapi(c.size(), [&](qsizetype i) { return i >= n ? (100 * (c[i] - at(c, i - n))) / at(c, i - n) : N; });
             };
             const Series l = roc(I(p, "long")), s = roc(I(p, "short"));
             return one("value", wma(mapi(l.size(), [&](qsizetype i) { return l[i] + s[i]; }), I(p, "wma")));
           },
           {0});
  L << def("kst", QT_TRANSLATE_NOOP("charts", "Know Sure Thing"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {intIn("roc1", QT_TRANSLATE_NOOP("charts", "ROC 1"), 10), intIn("roc2", QT_TRANSLATE_NOOP("charts", "ROC 2"), 15), intIn("roc3", QT_TRANSLATE_NOOP("charts", "ROC 3"), 20), intIn("roc4", QT_TRANSLATE_NOOP("charts", "ROC 4"), 30),
            intIn("sma1", QT_TRANSLATE_NOOP("charts", "SMA 1"), 10), intIn("sma2", QT_TRANSLATE_NOOP("charts", "SMA 2"), 10), intIn("sma3", QT_TRANSLATE_NOOP("charts", "SMA 3"), 10), intIn("sma4", QT_TRANSLATE_NOOP("charts", "SMA 4"), 15),
            intIn("signal", QT_TRANSLATE_NOOP("charts", "Signal length"), 9)},
           {line("kst", QT_TRANSLATE_NOOP("charts", "KST"), "#089981"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#f23645")},
           [](const Bars& bars, const Params& p) {
             const Series c = source(bars, "close");
             auto smaRoc = [&](int r, int s) {
               return sma(mapi(c.size(), [&](qsizetype i) { return i >= r ? (100 * (c[i] - at(c, i - r))) / at(c, i - r) : N; }), s);
             };
             const Series a = smaRoc(I(p, "roc1"), I(p, "sma1")), b = smaRoc(I(p, "roc2"), I(p, "sma2")),
                          d = smaRoc(I(p, "roc3"), I(p, "sma3")), e = smaRoc(I(p, "roc4"), I(p, "sma4"));
             const Series kst = mapi(a.size(), [&](qsizetype i) { return a[i] + 2 * b[i] + 3 * d[i] + 4 * e[i]; });
             Result r;
             r.put("kst", kst);
             r.put("signal", sma(kst, I(p, "signal")));
             return r;
           },
           {0});
  L << def("dpo", QT_TRANSLATE_NOOP("charts", "Detrended Price Oscillator"), QT_TRANSLATE_NOOP("charts", "Momentum"), false,
           {len(21, QT_TRANSLATE_NOOP("charts", "Period")), inp("centered", QT_TRANSLATE_NOOP("charts", "Centered"), "select", "Off", NaN, {QT_TRANSLATE_NOOP("charts", "Off"), QT_TRANSLATE_NOOP("charts", "On")})}, {drawOnly(line("value", QT_TRANSLATE_NOOP("charts", "DPO"), "#43a047"), [](const Params& p) { return S(p, "centered") == "On"; })},
           [](const Bars& bars, const Params& p) {
             const int n = I(p, "length");
             const Series c = source(bars, "close"), ma = sma(c, n);
             const int back = int(std::floor(n / 2.0)) + 1;
             // Off: today's close against the average from `back` bars ago. On: each close against the average centred
             // on it, which means looking `back` bars ahead, so the whole line is drawn `back` bars earlier.
             if (S(p, "centered") == "On")
               return one("value", shiftForward(mapi(ma.size(), [&](qsizetype i) { return at(c, i - back) - ma[i]; }), -back));
             return one("value", mapi(c.size(), [&](qsizetype i) { return c[i] - at(ma, i - back); }));
           },
           {0});
  L << def("elder", QT_TRANSLATE_NOOP("charts", "Elder Ray (Bull/Bear Power)"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(13)},
           {hist("bull", QT_TRANSLATE_NOOP("charts", "Bull power"), "#26a69a"), hist("bear", QT_TRANSLATE_NOOP("charts", "Bear power"), "#ef5350")},
           [](const Bars& bars, const Params& p) {
             const Series e = ema(source(bars, "close"), I(p, "length"));
             Result r;
             r.put("bull", mapi(bars.size(), [&](qsizetype i) { return bars[i].high - e[i]; }));
             r.put("bear", mapi(bars.size(), [&](qsizetype i) { return bars[i].low - e[i]; }));
             return r;
           },
           {0});
  L << def("bop", QT_TRANSLATE_NOOP("charts", "Balance of Power"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {}, {line("value", QT_TRANSLATE_NOOP("charts", "BOP"), "#f23645")},
           [](const Bars& bars, const Params&) {
             return one("value", mapi(bars.size(), [&](qsizetype i) {
                          const Bar& b = bars[i];
                          return b.high == b.low ? 0 : (b.close - b.open) / (b.high - b.low);
                        }));
           },
           {0});
  L << def("rvgi", QT_TRANSLATE_NOOP("charts", "Relative Vigor Index"), QT_TRANSLATE_NOOP("charts", "Momentum"), false, {len(10)},
           {line("value", QT_TRANSLATE_NOOP("charts", "RVGI"), "#089981"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#f23645")},
           [](const Bars& bars, const Params& p) {
             const Series num = swma(mapi(bars.size(), [&](qsizetype i) { return bars[i].close - bars[i].open; }));
             const Series den = swma(mapi(bars.size(), [&](qsizetype i) { return bars[i].high - bars[i].low; }));
             const Series sn = sum(num, I(p, "length")), sd = sum(den, I(p, "length"));
             const Series value = mapi(sn.size(), [&](qsizetype i) { return sn[i] / sd[i]; });
             Result r;
             r.put("value", value);
             r.put("signal", swma(value));
             return r;
           },
           {0});
  L << def("dmi", QT_TRANSLATE_NOOP("charts", "Directional Movement (DMI / ADX)"), QT_TRANSLATE_NOOP("charts", "Trend"), false,
           {intIn("diLength", QT_TRANSLATE_NOOP("charts", "DI length"), 14), intIn("adxSmoothing", QT_TRANSLATE_NOOP("charts", "ADX smoothing"), 14)},
           {line("adx", QT_TRANSLATE_NOOP("charts", "ADX"), "#f23645"), line("plus", QT_TRANSLATE_NOOP("charts", "+DI"), "#2962ff"), line("minus", QT_TRANSLATE_NOOP("charts", "-DI"), "#ff6d00")},
           [](const Bars& bars, const Params& p) {
             const Dmi d = dmiOf(bars, I(p, "diLength"), I(p, "adxSmoothing"));
             Result r;
             r.put("plus", d.plus);
             r.put("minus", d.minus);
             r.put("adx", d.adx);
             return r;
           });
  L << def("adx", QT_TRANSLATE_NOOP("charts", "Average Directional Index (ADX)"), QT_TRANSLATE_NOOP("charts", "Trend"), false,
           {intIn("diLength", QT_TRANSLATE_NOOP("charts", "DI length"), 14), intIn("adxSmoothing", QT_TRANSLATE_NOOP("charts", "ADX smoothing"), 14)}, {line("adx", QT_TRANSLATE_NOOP("charts", "ADX"), "#f23645")},
           [](const Bars& bars, const Params& p) { return one("adx", dmiOf(bars, I(p, "diLength"), I(p, "adxSmoothing")).adx); },
           {20, 25});
  L << def("aroon", QT_TRANSLATE_NOOP("charts", "Aroon"), QT_TRANSLATE_NOOP("charts", "Trend"), false, {len(14)}, {line("up", QT_TRANSLATE_NOOP("charts", "Aroon up"), "#ff6d00"), line("down", QT_TRANSLATE_NOOP("charts", "Aroon down"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const int n = I(p, "length");
             const Series hb = extremeBars(source(bars, "high"), n + 1, true), lb = extremeBars(source(bars, "low"), n + 1, false);
             Result r;
             r.put("up", mapi(hb.size(), [&](qsizetype i) { return (100 * (hb[i] + n)) / double(n); }));
             r.put("down", mapi(lb.size(), [&](qsizetype i) { return (100 * (lb[i] + n)) / double(n); }));
             return r;
           });
  L << def("vortex", QT_TRANSLATE_NOOP("charts", "Vortex Indicator"), QT_TRANSLATE_NOOP("charts", "Trend"), false, {len(14)}, {line("plus", QT_TRANSLATE_NOOP("charts", "VI +"), "#2962ff"), line("minus", QT_TRANSLATE_NOOP("charts", "VI -"), "#e91e63")},
           [](const Bars& bars, const Params& p) {
             const int n = I(p, "length");
             const Series vmp = sum(mapi(bars.size(), [&](qsizetype i) { return i ? std::fabs(bars[i].high - bars[i - 1].low) : N; }), n);
             const Series vmm = sum(mapi(bars.size(), [&](qsizetype i) { return i ? std::fabs(bars[i].low - bars[i - 1].high) : N; }), n);
             const Series t = tr(bars);
             const Series strr = sum(mapi(t.size(), [&](qsizetype i) { return i ? t[i] : N; }), n);
             Result r;
             r.put("plus", mapi(vmp.size(), [&](qsizetype i) { return vmp[i] / strr[i]; }));
             r.put("minus", mapi(vmm.size(), [&](qsizetype i) { return vmm[i] / strr[i]; }));
             return r;
           });
  L << def("chop", QT_TRANSLATE_NOOP("charts", "Choppiness Index"), QT_TRANSLATE_NOOP("charts", "Trend"), false, {len(14)}, {line("value", QT_TRANSLATE_NOOP("charts", "CHOP"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const int n = I(p, "length");
             const Series s = sum(tr(bars), n), hh = highest(source(bars, "high"), n), ll = lowest(source(bars, "low"), n);
             return one("value", mapi(s.size(), [&](qsizetype i) { return (100 * std::log10(s[i] / (hh[i] - ll[i]))) / std::log10(double(n)); }));
           },
           {38.2, 61.8});
  L << def("chopzone", QT_TRANSLATE_NOOP("charts", "Chop Zone"), QT_TRANSLATE_NOOP("charts", "Trend"), false, {}, {hist("value", QT_TRANSLATE_NOOP("charts", "Chop Zone"), "#fdd835")}, [](const Bars& bars, const Params&) {
    // Colours the bar by the angle of the 34 EMA: cyan and green for a rising trend, orange and red for a
    // falling one, yellow when it is going nowhere.
    const Series e = ema(source(bars, "close"), 34), avg = source(bars, "hlc3"), hh = highest(source(bars, "high"), 30),
                 ll = lowest(source(bars, "low"), 30);
    QStringList colors;
    for (qsizetype i = 0; i < bars.size(); ++i) colors << "#FDD835";
    const Series value = mapi(bars.size(), [&](qsizetype i) {
      if (!i || !isN(e[i - 1]) || !isN(e[i])) return N;
      const double span = (25 / (hh[i] - ll[i])) * ll[i], y2 = ((e[i - 1] - e[i]) / avg[i]) * span;
      const double a1 = jsRound((180 * std::acos(1 / std::sqrt(1 + y2 * y2))) / kPi);
      const double a = y2 > 0 ? -a1 : a1;
      colors[i] = a >= 5      ? "#26C6DA"
                  : a >= 3.57 ? "#43A047"
                  : a >= 2.14 ? "#A5D6A7"
                  : a >= 0.71 ? "#009688"
                  : a <= -5   ? "#D50000"
                  : a <= -3.57 ? "#E91E63"
                  : a <= -2.14 ? "#FF6D00"
                  : a <= -0.71 ? "#FFB74D"
                               : "#FDD835";
      return 1.0;
    });
    Result r;
    r.put("value", value);
    r.putColors(colors);
    return r;
  });
  L << def("trendstrength", QT_TRANSLATE_NOOP("charts", "Trend Strength Index"), QT_TRANSLATE_NOOP("charts", "Trend"), false, {len(14)}, {line("value", QT_TRANSLATE_NOOP("charts", "Trend strength"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             // Correlation of price with the bar count: +1 a straight climb, -1 a straight fall, 0 no trend.
             // Capped like rci: a window longer than the bars never fills anyway.
             const int n = int(std::min<qsizetype>(I(p, "length"), bars.size() + 1));
             const Series c = source(bars, "close");
             Series idx;
             for (int k = 0; k < n; ++k) idx << k;
             return one("value", mapi(c.size(), [&](qsizetype i) {
                          if (i < n - 1) return N;
                          const Series w = n > 0 ? c.mid(i - n + 1, n) : Series();
                          return std::all_of(w.begin(), w.end(), isN) ? pearson(idx, w) : N;
                        }));
           },
           {0});
  L << def("mass", QT_TRANSLATE_NOOP("charts", "Mass Index"), QT_TRANSLATE_NOOP("charts", "Trend"), false, {len(10)}, {line("value", QT_TRANSLATE_NOOP("charts", "Mass"), "#2962ff")}, [](const Bars& bars, const Params& p) {
    const Series span = mapi(bars.size(), [&](qsizetype i) { return bars[i].high - bars[i].low; });
    const Series e1 = ema(span, 9), e2 = ema(e1, 9);
    return one("value", sum(mapi(e1.size(), [&](qsizetype i) { return e1[i] / e2[i]; }), I(p, "length")));
  });
  L << def("psar", QT_TRANSLATE_NOOP("charts", "Parabolic SAR"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {floatIn("start", QT_TRANSLATE_NOOP("charts", "Start"), 0.02), floatIn("inc", QT_TRANSLATE_NOOP("charts", "Increment"), 0.02), floatIn("max", QT_TRANSLATE_NOOP("charts", "Max value"), 0.2)},
           {line("value", QT_TRANSLATE_NOOP("charts", "SAR"), "#2962ff", 0, QJsonObject{{"dots", true}})},
           [](const Bars& bars, const Params& p) { return one("value", psarOf(bars, D(p, "start"), D(p, "inc"), D(p, "max"))); });
  L << def("supertrend", QT_TRANSLATE_NOOP("charts", "Supertrend"), QT_TRANSLATE_NOOP("charts", "Trend"), true, {intIn("atrLength", QT_TRANSLATE_NOOP("charts", "ATR length"), 10), floatIn("factor", QT_TRANSLATE_NOOP("charts", "Factor"), 3)},
           {line("up", QT_TRANSLATE_NOOP("charts", "Up trend"), "#26a69a", 2), line("down", QT_TRANSLATE_NOOP("charts", "Down trend"), "#ef5350", 2)},
           [](const Bars& bars, const Params& p) {
             const Supertrend s = supertrendOf(bars, D(p, "factor"), I(p, "atrLength"));
             Result r;
             r.put("up", mapi(s.st.size(), [&](qsizetype i) { return s.dir[i] < 0 ? s.st[i] : N; }));
             r.put("down", mapi(s.st.size(), [&](qsizetype i) { return s.dir[i] > 0 ? s.st[i] : N; }));
             r.put("direction", s.dir);
             return r;
           });
  L << def("vstop", QT_TRANSLATE_NOOP("charts", "Volatility Stop"), QT_TRANSLATE_NOOP("charts", "Trend"), true, {len(20, QT_TRANSLATE_NOOP("charts", "ATR length")), floatIn("factor", QT_TRANSLATE_NOOP("charts", "Factor"), 2)},
           {line("up", QT_TRANSLATE_NOOP("charts", "Up trend"), "#26a69a", 2), line("down", QT_TRANSLATE_NOOP("charts", "Down trend"), "#ef5350", 2)},
           [](const Bars& bars, const Params& p) { return vstopOf(bars, I(p, "length"), D(p, "factor")); });
  L << def("cks", QT_TRANSLATE_NOOP("charts", "Chande Kroll Stop"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {intIn("p", QT_TRANSLATE_NOOP("charts", "ATR length (p)"), 10), floatIn("x", QT_TRANSLATE_NOOP("charts", "ATR multiplier (x)"), 1), intIn("q", QT_TRANSLATE_NOOP("charts", "Stop length (q)"), 9)},
           {line("stopLong", QT_TRANSLATE_NOOP("charts", "Stop long"), "#26a69a"), line("stopShort", QT_TRANSLATE_NOOP("charts", "Stop short"), "#ef5350")},
           [](const Bars& bars, const Params& prm) {
             const int p = I(prm, "p"), q = I(prm, "q");
             const double x = D(prm, "x");
             const Series a = atr(bars, p), hh = highest(source(bars, "high"), p), ll = lowest(source(bars, "low"), p);
             const Series firstHigh = mapi(hh.size(), [&](qsizetype i) { return hh[i] - x * a[i]; });
             const Series firstLow = mapi(ll.size(), [&](qsizetype i) { return ll[i] + x * a[i]; });
             Result r;
             r.put("stopShort", whole(highest(firstHigh, q), firstHigh, q));
             r.put("stopLong", whole(lowest(firstLow, q), firstLow, q));
             return r;
           });
  L << def("ichimoku", QT_TRANSLATE_NOOP("charts", "Ichimoku Cloud"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {intIn("conversion", QT_TRANSLATE_NOOP("charts", "Conversion line"), 9), intIn("base", QT_TRANSLATE_NOOP("charts", "Base line"), 26), intIn("spanB", QT_TRANSLATE_NOOP("charts", "Leading span B"), 52),
            intIn("displacement", QT_TRANSLATE_NOOP("charts", "Lagging span"), 26)},
           {line("conversion", QT_TRANSLATE_NOOP("charts", "Conversion"), "#2962ff"), line("base", QT_TRANSLATE_NOOP("charts", "Base"), "#b71c1c"), drawOnly(line("lagging", QT_TRANSLATE_NOOP("charts", "Lagging span"), "#43a047"), [](const Params& p) { return I(p, "displacement") > 1; }),
            line("spanA", QT_TRANSLATE_NOOP("charts", "Leading span A"), "#a5d6a7"), line("spanB", QT_TRANSLATE_NOOP("charts", "Leading span B"), "#ef9a9a")},
           [](const Bars& bars, const Params& p) {
             const Series hi = source(bars, "high"), lo = source(bars, "low");
             auto mid = [&](int n) {
               const Series ll = lowest(lo, n), hh = highest(hi, n);
               return mapi(hh.size(), [&](qsizetype i) { return (hh[i] + ll[i]) / 2; });
             };
             const Series conversion = mid(I(p, "conversion")), base = mid(I(p, "base"));
             const Series a = mapi(conversion.size(), [&](qsizetype i) { return (conversion[i] + base[i]) / 2; }), b = mid(I(p, "spanB"));
             if (I(p, "displacement") == kNaNLength) throw std::runtime_error("Invalid array length");   // new Array(NaN)
             const int off = I(p, "displacement") - 1;
             Result r;
             r.put("conversion", conversion);
             r.put("base", base);
             r.put("lagging", shiftForward(source(bars, "close"), -off));
             r.put("spanA", shiftForward(a, off));
             r.put("spanB", shiftForward(b, off));
             r.putExtra("extendBars", off);
             return r;
           });
  L << def("alligator", QT_TRANSLATE_NOOP("charts", "Williams Alligator"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {intIn("jawLength", QT_TRANSLATE_NOOP("charts", "Jaw length"), 13), intIn("jawOffset", QT_TRANSLATE_NOOP("charts", "Jaw offset"), 8), intIn("teethLength", QT_TRANSLATE_NOOP("charts", "Teeth length"), 8),
            intIn("teethOffset", QT_TRANSLATE_NOOP("charts", "Teeth offset"), 5), intIn("lipsLength", QT_TRANSLATE_NOOP("charts", "Lips length"), 5), intIn("lipsOffset", QT_TRANSLATE_NOOP("charts", "Lips offset"), 3)},
           {line("jaw", QT_TRANSLATE_NOOP("charts", "Jaw"), "#2962ff"), line("teeth", QT_TRANSLATE_NOOP("charts", "Teeth"), "#e91e63"), line("lips", QT_TRANSLATE_NOOP("charts", "Lips"), "#66bb6a")},
           [](const Bars& bars, const Params& p) {
             // Three smoothed averages of the bar midpoint, each drawn a few bars ahead of the bar it was computed on.
             const Series h2 = source(bars, "hl2");
             Result r;
             r.put("jaw", shiftForward(rma(h2, I(p, "jawLength")), I(p, "jawOffset")));
             r.put("teeth", shiftForward(rma(h2, I(p, "teethLength")), I(p, "teethOffset")));
             r.put("lips", shiftForward(rma(h2, I(p, "lipsLength")), I(p, "lipsOffset")));
             r.putExtra("extendBars", jsMax({D(p, "jawOffset"), D(p, "teethOffset"), D(p, "lipsOffset")}));
             return r;
           });
  L << def("pivots", QT_TRANSLATE_NOOP("charts", "Pivot Points Standard"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {inp("period", QT_TRANSLATE_NOOP("charts", "Pivot period"), "select", "auto", NaN, {QT_TRANSLATE_NOOP("charts", "auto"), QT_TRANSLATE_NOOP("charts", "day"), QT_TRANSLATE_NOOP("charts", "week"), QT_TRANSLATE_NOOP("charts", "month"), QT_TRANSLATE_NOOP("charts", "year")})},
           {line("r3", QT_TRANSLATE_NOOP("charts", "R3"), "#ef5350", 1), line("r2", QT_TRANSLATE_NOOP("charts", "R2"), "#ef5350", 1), line("r1", QT_TRANSLATE_NOOP("charts", "R1"), "#ef5350", 1), line("p", QT_TRANSLATE_NOOP("charts", "P"), "#ff9800", 1),
            line("s1", QT_TRANSLATE_NOOP("charts", "S1"), "#26a69a", 1), line("s2", QT_TRANSLATE_NOOP("charts", "S2"), "#26a69a", 1), line("s3", QT_TRANSLATE_NOOP("charts", "S3"), "#26a69a", 1)},
           [](const Bars& bars, const Params& p) { return pivotsOf(bars, S(p, "period")); }, {}, QJsonObject{{"noAutoscale", true}});
  L << def("pdhl", QT_TRANSLATE_NOOP("charts", "Previous Day High/Low"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {inp("period", QT_TRANSLATE_NOOP("charts", "Previous"), "select", "day", NaN, {QT_TRANSLATE_NOOP("charts", "day"), QT_TRANSLATE_NOOP("charts", "week"), QT_TRANSLATE_NOOP("charts", "month")})},
           {line("high", QT_TRANSLATE_NOOP("charts", "Prev high"), "#ef5350", 1), line("low", QT_TRANSLATE_NOOP("charts", "Prev low"), "#26a69a", 1)}, pdhlCalc, {},
           QJsonObject{{"noAutoscale", true}});
  L << def("pivotshl", QT_TRANSLATE_NOOP("charts", "Pivot Points High Low"), QT_TRANSLATE_NOOP("charts", "Trend"), true,
           {intIn("left", QT_TRANSLATE_NOOP("charts", "Bars to the left"), 10), intIn("right", QT_TRANSLATE_NOOP("charts", "Bars to the right"), 10)}, {},
           [](const Bars& bars, const Params& p) {
             Result r;
             r.putMarkers(pivotMarkers(bars, I(p, "left"), I(p, "right")));
             return r;
           });
  L << def("zigzag", QT_TRANSLATE_NOOP("charts", "Zig Zag"), QT_TRANSLATE_NOOP("charts", "Trend"), true, {floatIn("deviation", QT_TRANSLATE_NOOP("charts", "Deviation (%)"), 5), intIn("depth", QT_TRANSLATE_NOOP("charts", "Depth"), 10)},
           {drawOnly(line("value", QT_TRANSLATE_NOOP("charts", "Zig Zag"), "#2962ff", 2), [](const Params&) { return true; })},
           [](const Bars& bars, const Params& p) { return one("value", zigzagOf(bars, D(p, "deviation"), D(p, "depth"))); });
  L << def("volume", QT_TRANSLATE_NOOP("charts", "Volume"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {inp("maLength", QT_TRANSLATE_NOOP("charts", "MA length (0 = off)"), "int", 20, 0)},
           {hist("value", QT_TRANSLATE_NOOP("charts", "Volume"), "#26a69a"), line("ma", QT_TRANSLATE_NOOP("charts", "Volume MA"), "#2962ff", 1)},
           [](const Bars& bars, const Params& p) {
             const Series v = source(bars, "volume");
             QStringList colors;
             for (const Bar& b : bars) colors << (b.close >= b.open ? "rgba(38,166,154,0.6)" : "rgba(239,83,80,0.6)");
             Result r;
             r.put("value", v);
             r.putColors(colors);
             r.put("ma", D(p, "maLength") > 0 ? sma(v, I(p, "maLength")) : nans(v.size()));
             return r;
           },
           {}, QJsonObject{{"volumeFormat", true}});
  L << def("obv", QT_TRANSLATE_NOOP("charts", "On Balance Volume (OBV)"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {}, {line("value", QT_TRANSLATE_NOOP("charts", "OBV"), "#2962ff")},
           [](const Bars& bars, const Params&) {
             return one("value", cum(mapi(bars.size(), [&](qsizetype i) {
                          return i ? jsSign(bars[i].close - bars[i - 1].close) * jsOr(bars[i].volume, 0) : 0;
                        })));
           },
           {}, QJsonObject{{"volumeFormat", true}});
  L << def("cmf", QT_TRANSLATE_NOOP("charts", "Chaikin Money Flow"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {len(20)}, {line("value", QT_TRANSLATE_NOOP("charts", "CMF"), "#43a047")},
           [](const Bars& bars, const Params& p) {
             const Series ad = mapi(bars.size(), [&](qsizetype i) {
               const Bar& b = bars[i];
               return b.high == b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * jsOr(b.volume, 0);
             });
             const Series sv = sum(source(bars, "volume"), I(p, "length")), sa = sum(ad, I(p, "length"));
             return one("value", mapi(sa.size(), [&](qsizetype i) { return jsTruthy(sv[i]) ? sa[i] / sv[i] : 0; }));
           },
           {0});
  L << def("ad", QT_TRANSLATE_NOOP("charts", "Accumulation/Distribution"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {}, {line("value", QT_TRANSLATE_NOOP("charts", "A/D"), "#999915")},
           [](const Bars& bars, const Params&) {
             return one("value", cum(mapi(bars.size(), [&](qsizetype i) {
                          const Bar& b = bars[i];
                          return b.high == b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * jsOr(b.volume, 0);
                        })));
           },
           {}, QJsonObject{{"volumeFormat", true}});
  L << def("chaikinosc", QT_TRANSLATE_NOOP("charts", "Chaikin Oscillator"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {intIn("fast", QT_TRANSLATE_NOOP("charts", "Fast length"), 3), intIn("slow", QT_TRANSLATE_NOOP("charts", "Slow length"), 10)},
           {line("value", QT_TRANSLATE_NOOP("charts", "Chaikin Osc"), "#2962ff")},
           [](const Bars& bars, const Params& p) {
             const Series ad = compute("ad", bars).value("value"), fast = ema(ad, I(p, "fast")), slow = ema(ad, I(p, "slow"));
             return one("value", mapi(fast.size(), [&](qsizetype i) { return fast[i] - slow[i]; }));
           },
           {0}, QJsonObject{{"volumeFormat", true}});
  L << def("pvt", QT_TRANSLATE_NOOP("charts", "Price Volume Trend"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {}, {line("value", QT_TRANSLATE_NOOP("charts", "PVT"), "#2962ff")},
           [](const Bars& bars, const Params&) {
             return one("value", cum(mapi(bars.size(), [&](qsizetype i) {
                          return i ? ((bars[i].close - bars[i - 1].close) / bars[i - 1].close) * jsOr(bars[i].volume, 0) : 0;
                        })));
           },
           {}, QJsonObject{{"volumeFormat", true}});
  L << def("eom", QT_TRANSLATE_NOOP("charts", "Ease of Movement"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {len(14), intIn("divisor", QT_TRANSLATE_NOOP("charts", "Divisor"), 10000)}, {line("value", QT_TRANSLATE_NOOP("charts", "EOM"), "#43a047")},
           [](const Bars& bars, const Params& p) {
             const Series hl2 = source(bars, "hl2"), ch = change(hl2);
             const double divisor = D(p, "divisor");
             return one("value", sma(mapi(bars.size(), [&](qsizetype i) {
                                       const Bar& b = bars[i];
                                       return jsTruthy(b.volume) ? (divisor * ch[i] * (b.high - b.low)) / b.volume : 0;
                                     }),
                                     I(p, "length")));
           },
           {0});
  L << def("efi", QT_TRANSLATE_NOOP("charts", "Elder Force Index"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {len(13)}, {line("value", QT_TRANSLATE_NOOP("charts", "EFI"), "#26a69a")},
           [](const Bars& bars, const Params& p) {
             const Series ch = change(source(bars, "close")), vol = source(bars, "volume");
             return one("value", ema(mapi(ch.size(), [&](qsizetype i) { return ch[i] * vol[i]; }), I(p, "length")));
           },
           {0}, QJsonObject{{"volumeFormat", true}});
  L << def("kvo", QT_TRANSLATE_NOOP("charts", "Klinger Oscillator"), QT_TRANSLATE_NOOP("charts", "Volume"), false,
           {intIn("fast", QT_TRANSLATE_NOOP("charts", "Fast length"), 34), intIn("slow", QT_TRANSLATE_NOOP("charts", "Slow length"), 55), intIn("signal", QT_TRANSLATE_NOOP("charts", "Signal length"), 13)},
           {line("kvo", QT_TRANSLATE_NOOP("charts", "KVO"), "#2962ff"), line("signal", QT_TRANSLATE_NOOP("charts", "Signal"), "#43a047")},
           [](const Bars& bars, const Params& p) {
             // Volume counts for the bar when the typical price rose or held, against it when the price fell.
             const Series ch = change(source(bars, "hlc3")), vol = source(bars, "volume");
             const Series sv = mapi(ch.size(), [&](qsizetype i) { return ch[i] >= 0 ? vol[i] : -vol[i]; });
             const Series f = ema(sv, I(p, "fast")), s = ema(sv, I(p, "slow"));
             const Series kvo = mapi(f.size(), [&](qsizetype i) { return f[i] - s[i]; });
             Result r;
             r.put("kvo", kvo);
             r.put("signal", ema(kvo, I(p, "signal")));
             return r;
           },
           {0}, QJsonObject{{"volumeFormat", true}});
  L << def("volosc", QT_TRANSLATE_NOOP("charts", "Volume Oscillator"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {intIn("short", QT_TRANSLATE_NOOP("charts", "Short length"), 5), intIn("long", QT_TRANSLATE_NOOP("charts", "Long length"), 10)},
           {line("value", QT_TRANSLATE_NOOP("charts", "Volume Osc"), "#43a047")},
           [](const Bars& bars, const Params& p) {
             const Series vol = source(bars, "volume"), s = ema(vol, I(p, "short")), l = ema(vol, I(p, "long"));
             return one("value", mapi(s.size(), [&](qsizetype i) { return (100 * (s[i] - l[i])) / l[i]; }));
           },
           {0});
  L << def("netvol", QT_TRANSLATE_NOOP("charts", "Net Volume"), QT_TRANSLATE_NOOP("charts", "Volume"), false, {}, {hist("value", QT_TRANSLATE_NOOP("charts", "Net volume"), "#26a69a")},
           [](const Bars& bars, const Params&) {
             const Series ch = change(source(bars, "close")), vol = source(bars, "volume");
             const Series value = mapi(ch.size(), [&](qsizetype i) { return ch[i] > 0 ? vol[i] : ch[i] < 0 ? -vol[i] : 0; });
             Result r;
             r.put("value", value);
             r.putColors(bandColors(value, "rgba(38,166,154,0.6)", "rgba(239,83,80,0.6)"));
             return r;
           },
           {}, QJsonObject{{"volumeFormat", true}});
  // Lewis's hedge fund method: label each bar bull, bear or sideways by its trailing return,
  // count how often each state follows each other one, and read tomorrow's odds from today's
  // row. Walk-forward: each bar only uses transitions that had already happened.
  L << def("markov", QT_TRANSLATE_NOOP("charts", "Markov Regime (bull, bear, sideways)"), QT_TRANSLATE_NOOP("charts", "Quant (bonus)"), false,
           {inp("lookback", QT_TRANSLATE_NOOP("charts", "Trailing return (bars)"), "int", 20, 2), floatIn("threshold", QT_TRANSLATE_NOOP("charts", "Bull/bear threshold %"), 5),
            inp("minHistory", QT_TRANSLATE_NOOP("charts", "Transitions before odds show"), "int", 50, 5)},
           {hist("signal", QT_TRANSLATE_NOOP("charts", "Bull minus bear odds"), "#9598a1"), line("pbull", QT_TRANSLATE_NOOP("charts", "Odds of bull next"), "#26a69a"),
            line("pbear", QT_TRANSLATE_NOOP("charts", "Odds of bear next"), "#ef5350")},
           [](const Bars& bars, const Params& p) {
             const qsizetype n = bars.size();
             const int lookback = I(p, "lookback");
             const double th = D(p, "threshold") / 100, minHistory = D(p, "minHistory");
             Series state = nans(n), signal = nans(n), pbull = nans(n), pbear = nans(n);
             QStringList colors;
             for (qsizetype i = 0; i < n; ++i) colors << "#9598a1";
             int counts[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};   // rows: from bear, sideways, bull
             int seen = 0;
             for (qsizetype i = lookback; i < n; ++i) {
               const double r = barAt(bars, i, "close").close / barAt(bars, i - lookback, "close").close - 1;
               // A negative i only happens for a negative lookback, and then the read above has thrown already.
               state[i] = r >= th ? 1 : r <= -th ? -1 : 0;
               if (std::isfinite(at(state, i - 1))) {
                 counts[int(state[i - 1]) + 1][int(state[i]) + 1]++;
                 seen++;
               }
               const int* row = counts[int(state[i]) + 1];
               const int total = row[0] + row[1] + row[2];
               colors[i] = state[i] == 1 ? "#26a69a" : state[i] == -1 ? "#ef5350" : "#9598a1";
               if (seen >= minHistory && total > 0) {
                 pbull[i] = double(row[2]) / total;
                 pbear[i] = double(row[0]) / total;
                 signal[i] = pbull[i] - pbear[i];
               }
             }
             Result r;
             r.put("signal", signal);
             r.put("pbull", pbull);
             r.put("pbear", pbear);
             r.put("state", state);
             r.putColors(colors);
             return r;
           },
           {0});
  L << def("vpvr", QT_TRANSLATE_NOOP("charts", "Volume Profile, visible range (VPVR)"), QT_TRANSLATE_NOOP("charts", "Volume"), true,
           {inp("rows", QT_TRANSLATE_NOOP("charts", "Rows"), "int", 24, 4), inp("width", QT_TRANSLATE_NOOP("charts", "Width (% of chart)"), "int", 30, 5), inp("valueArea", QT_TRANSLATE_NOOP("charts", "Value area %"), "int", 70, 10)},
           {}, [](const Bars&, const Params& p) {
             Result r;
             r.note("volumeProfile");
             r.volumeProfile = QJsonObject{{"rows", p.value("rows")}, {"width", p.value("width")}, {"valueArea", p.value("valueArea")}};
             return r;
           });
  L << def("svp", QT_TRANSLATE_NOOP("charts", "Session Volume Profile (SVP)"), QT_TRANSLATE_NOOP("charts", "Volume"), true,
           {inp("period", QT_TRANSLATE_NOOP("charts", "Session"), "select", "Auto", NaN, {QT_TRANSLATE_NOOP("charts", "Auto"), QT_TRANSLATE_NOOP("charts", "Day"), QT_TRANSLATE_NOOP("charts", "Week"), QT_TRANSLATE_NOOP("charts", "Month")}), inp("rows", QT_TRANSLATE_NOOP("charts", "Rows"), "int", 24, 4),
            inp("width", QT_TRANSLATE_NOOP("charts", "Width (% of session)"), "int", 70, 5), inp("valueArea", QT_TRANSLATE_NOOP("charts", "Value area %"), "int", 70, 10)},
           {}, [](const Bars&, const Params& p) {
             Result r;
             r.note("sessionProfile");
             r.sessionProfile = QJsonObject{
                 {"period", p.value("period")}, {"rows", p.value("rows")}, {"width", p.value("width")}, {"valueArea", p.value("valueArea")}};
             return r;
           });
  L << def("patterns", QT_TRANSLATE_NOOP("charts", "Candlestick Patterns"), QT_TRANSLATE_NOOP("charts", "Patterns"), true, {}, {}, [](const Bars& bars, const Params&) {
    Result r;
    r.putMarkers(candlePatterns(bars));
    return r;
  });
  L << def("fractals", QT_TRANSLATE_NOOP("charts", "Williams Fractals"), QT_TRANSLATE_NOOP("charts", "Patterns"), true, {intIn("periods", QT_TRANSLATE_NOOP("charts", "Periods"), 2)}, {},
           [](const Bars& bars, const Params& p) {
             Result r;
             r.putMarkers(fractalMarkers(bars, I(p, "periods")));
             return r;
           });
  // ---- Deepwater additions (not in quant-terminal), after the JS built-ins ----
  // Liquidity heatmap (order flow): resting quantity per price over time, from the market's order book (Binance spot
  // and USD-M), drawn under the candles by ChartPane (pages/charts/flow/HeatmapLayer) from charts/data/DepthStream.
  L << def("heatmap", QT_TRANSLATE_NOOP("charts", "Liquidity heatmap"), QT_TRANSLATE_NOOP("charts", "Order flow"), true,
           {inp("book", QT_TRANSLATE_NOOP("charts", "Order book"), "select", "Chart", NaN,
                {QT_TRANSLATE_NOOP("charts", "Chart"), QT_TRANSLATE_NOOP("charts", "Spot"), QT_TRANSLATE_NOOP("charts", "USD-M perpetual")}),
            floatIn("step", QT_TRANSLATE_NOOP("charts", "Price step of a cell (0 = auto)"), 0),
            inp("combine", QT_TRANSLATE_NOOP("charts", "Prices in one cell"), "select", "Largest", NaN,
                {QT_TRANSLATE_NOOP("charts", "Largest"), QT_TRANSLATE_NOOP("charts", "Sum")}),
            inp("palette", QT_TRANSLATE_NOOP("charts", "Palette"), "select", "Deep", NaN,
                {QT_TRANSLATE_NOOP("charts", "Deep"), QT_TRANSLATE_NOOP("charts", "Thermal"), QT_TRANSLATE_NOOP("charts", "Mono")}),
            inp("scale", QT_TRANSLATE_NOOP("charts", "Colour scale"), "select", "Log", NaN, {QT_TRANSLATE_NOOP("charts", "Log"), QT_TRANSLATE_NOOP("charts", "Linear")}),
            floatIn("low", QT_TRANSLATE_NOOP("charts", "Auto contrast: dark up to percentile"), 50),
            floatIn("high", QT_TRANSLATE_NOOP("charts", "Auto contrast: brightest from percentile"), 99.5),
            floatIn("minQty", QT_TRANSLATE_NOOP("charts", "Hide below quantity (0 = auto)"), 0),
            floatIn("maxQty", QT_TRANSLATE_NOOP("charts", "Brightest from quantity (0 = auto)"), 0),
            floatIn("gamma", QT_TRANSLATE_NOOP("charts", "Contrast curve (1 = straight)"), 1.4),
            intIn("opacity", QT_TRANSLATE_NOOP("charts", "Opacity (%)"), 100),
            inp("lines", QT_TRANSLATE_NOOP("charts", "Best bid / ask lines"), "select", "On", NaN, {QT_TRANSLATE_NOOP("charts", "On"), QT_TRANSLATE_NOOP("charts", "Off")}),
            inp("record", QT_TRANSLATE_NOOP("charts", "Record to disk (1 s)"), "select", "On", NaN, {QT_TRANSLATE_NOOP("charts", "On"), QT_TRANSLATE_NOOP("charts", "Off")}),
            intIn("keepDays", QT_TRANSLATE_NOOP("charts", "Days kept on disk"), 7)},
           {},
           [](const Bars&, const Params& p) {
             Result r;
             r.putExtra("heatmap", QJsonObject{{"book", str(p, "book", "Chart")}, {"step", num(p, "step", 0)}, {"combine", str(p, "combine", "Largest")},
                                               {"palette", str(p, "palette", "Deep")}, {"scale", str(p, "scale", "Log")},
                                               {"low", num(p, "low", 50)}, {"high", num(p, "high", 99.5)}, {"minQty", num(p, "minQty", 0)},
                                               {"maxQty", num(p, "maxQty", 0)}, {"gamma", num(p, "gamma", 1.4)}, {"opacity", num(p, "opacity", 100)},
                                               {"lines", str(p, "lines", "On")}, {"record", str(p, "record", "On")}, {"keepDays", num(p, "keepDays", 7)}});
             return r;
           },
           {}, QJsonObject{{"drawnOnly", true}});
  // Liquidation heatmap (order flow, an estimate): where leveraged positions opened on the chart's bars would be
  // liquidated, from Binance USD-M open interest and accounts ratio (charts/core/Liquidation.h), drawn under the candles
  // by ChartPane (pages/charts/flow/LiqMapLayer).
  L << def("liqmap", QT_TRANSLATE_NOOP("charts", "Liquidation heatmap"), QT_TRANSLATE_NOOP("charts", "Order flow"), true,
           {inp("side", QT_TRANSLATE_NOOP("charts", "Positions"), "select", "Both", NaN,
                {QT_TRANSLATE_NOOP("charts", "Both"), QT_TRANSLATE_NOOP("charts", "Longs"), QT_TRANSLATE_NOOP("charts", "Shorts")}),
            inp("leverage", QT_TRANSLATE_NOOP("charts", "Leverage mix"), "select", "Standard (5–100×)", NaN,
                {QT_TRANSLATE_NOOP("charts", "Standard (5–100×)"), QT_TRANSLATE_NOOP("charts", "High (25–125×)"), QT_TRANSLATE_NOOP("charts", "Low (2–20×)"),
                 QT_TRANSLATE_NOOP("charts", "Calibrated on real liquidations")}),
            floatIn("notional", QT_TRANSLATE_NOOP("charts", "Typical position (USDT)"), 10000),
            inp("ratio", QT_TRANSLATE_NOOP("charts", "Longs vs shorts"), "select", "Accounts ratio", NaN,
                {QT_TRANSLATE_NOOP("charts", "Accounts ratio"), QT_TRANSLATE_NOOP("charts", "50/50")}),
            floatIn("bias", QT_TRANSLATE_NOOP("charts", "Candle tilt (0 = off)"), 0),
            floatIn("halfLife", QT_TRANSLATE_NOOP("charts", "Fade half-life, hours (0 = never)"), 0),
            intIn("consume", QT_TRANSLATE_NOOP("charts", "Removed when the price reaches it (%)"), 100),
            intIn("rows", QT_TRANSLATE_NOOP("charts", "Price rows"), 800),
            inp("palette", QT_TRANSLATE_NOOP("charts", "Palette"), "select", "Thermal", NaN,
                {QT_TRANSLATE_NOOP("charts", "Deep"), QT_TRANSLATE_NOOP("charts", "Thermal"), QT_TRANSLATE_NOOP("charts", "Mono")}),
            floatIn("low", QT_TRANSLATE_NOOP("charts", "Auto contrast: dark up to percentile"), 60),
            floatIn("high", QT_TRANSLATE_NOOP("charts", "Auto contrast: brightest from percentile"), 99.5),
            intIn("opacity", QT_TRANSLATE_NOOP("charts", "Opacity (%)"), 85),
            inp("real", QT_TRANSLATE_NOOP("charts", "Real liquidations (dots)"), "select", "On", NaN, {QT_TRANSLATE_NOOP("charts", "On"), QT_TRANSLATE_NOOP("charts", "Off")})},
           {},
           [](const Bars&, const Params& p) {
             Result r;
             r.putExtra("liqmap", QJsonObject{{"side", str(p, "side", "Both")}, {"leverage", str(p, "leverage", "Standard (5–100×)")},
                                              {"notional", num(p, "notional", 10000)}, {"ratio", str(p, "ratio", "Accounts ratio")},
                                              {"bias", num(p, "bias", 0)}, {"halfLife", num(p, "halfLife", 0)}, {"consume", num(p, "consume", 100)},
                                              {"rows", num(p, "rows", 800)}, {"palette", str(p, "palette", "Thermal")}, {"low", num(p, "low", 60)},
                                              {"high", num(p, "high", 99.5)}, {"opacity", num(p, "opacity", 85)}, {"real", str(p, "real", "On")}});
             return r;
           },
           {}, QJsonObject{{"drawnOnly", true}});
  // Structural liquidity (SMC / ICT): buy-side above swing highs, sell-side below swing lows, equal highs / lows,
  // previous day / week and session highs and lows, swept when the price goes through (charts/core/LiquidityLevels,
  // SessionLevels; drawn by ChartPane with pages/charts/flow/SmcLayer).
  L << def("smcliq", QT_TRANSLATE_NOOP("charts", "Liquidity levels (SMC)"), QT_TRANSLATE_NOOP("charts", "Order flow"), true,
           {inp("method", QT_TRANSLATE_NOOP("charts", "Pivots"), "select", "Window", NaN,
                {QT_TRANSLATE_NOOP("charts", "Window"), QT_TRANSLATE_NOOP("charts", "Percent"), QT_TRANSLATE_NOOP("charts", "ATR")}),
            intIn("length", QT_TRANSLATE_NOOP("charts", "Window: bars on each side"), 5),
            floatIn("percent", QT_TRANSLATE_NOOP("charts", "Percent: reversal (%)"), 0.5),
            floatIn("atrMult", QT_TRANSLATE_NOOP("charts", "ATR: reversal (ATRs)"), 1.5),
            inp("tolUnit", QT_TRANSLATE_NOOP("charts", "Equal highs / lows tolerance in"), "select", "ATR", NaN,
                {QT_TRANSLATE_NOOP("charts", "ATR"), QT_TRANSLATE_NOOP("charts", "Percent")}),
            floatIn("tolerance", QT_TRANSLATE_NOOP("charts", "Equal highs / lows tolerance"), 0.1),
            inp("thickUnit", QT_TRANSLATE_NOOP("charts", "Zone thickness in"), "select", "ATR", NaN,
                {QT_TRANSLATE_NOOP("charts", "ATR"), QT_TRANSLATE_NOOP("charts", "Percent"), QT_TRANSLATE_NOOP("charts", "Ticks")}),
            floatIn("thickness", QT_TRANSLATE_NOOP("charts", "Zone thickness"), 0.1),
            inp("sweep", QT_TRANSLATE_NOOP("charts", "Swept by"), "select", "Wick", NaN, {QT_TRANSLATE_NOOP("charts", "Wick"), QT_TRANSLATE_NOOP("charts", "Close")}),
            inp("detectTf", QT_TRANSLATE_NOOP("charts", "Detection timeframe"), "select", "Chart", NaN,
                {QT_TRANSLATE_NOOP("charts", "Chart"), "15m", "1h", "4h", "1D", "1W"}),
            inp("periods", QT_TRANSLATE_NOOP("charts", "Day and week levels"), "select", "On", NaN, {QT_TRANSLATE_NOOP("charts", "On"), QT_TRANSLATE_NOOP("charts", "Off")}),
            inp("dayStart", QT_TRANSLATE_NOOP("charts", "Day starts"), "select", "00:00 UTC", NaN,
                {QT_TRANSLATE_NOOP("charts", "00:00 UTC"), QT_TRANSLATE_NOOP("charts", "17:00 New York")}),
            inp("asia", QT_TRANSLATE_NOOP("charts", "Asia session"), "select", "Tokyo 09:00–15:00", NaN,
                {QT_TRANSLATE_NOOP("charts", "Off"), QT_TRANSLATE_NOOP("charts", "Tokyo 09:00–15:00"), QT_TRANSLATE_NOOP("charts", "New York 20:00–00:00 (ICT)")}),
            inp("london", QT_TRANSLATE_NOOP("charts", "London session"), "select", "London 08:00–16:30", NaN,
                {QT_TRANSLATE_NOOP("charts", "Off"), QT_TRANSLATE_NOOP("charts", "London 08:00–16:30"), QT_TRANSLATE_NOOP("charts", "New York 02:00–05:00 (ICT)")}),
            inp("newYork", QT_TRANSLATE_NOOP("charts", "New York session"), "select", "New York 09:30–16:00", NaN,
                {QT_TRANSLATE_NOOP("charts", "Off"), QT_TRANSLATE_NOOP("charts", "New York 09:30–16:00"), QT_TRANSLATE_NOOP("charts", "New York 07:00–10:00 (ICT)")}),
            intIn("keep", QT_TRANSLATE_NOOP("charts", "Bars shown after a sweep (greyed)"), 20),
            floatIn("minScore", QT_TRANSLATE_NOOP("charts", "Minimum score shown"), 1),
            inp("style", QT_TRANSLATE_NOOP("charts", "Drawn as"), "select", "Zones", NaN, {QT_TRANSLATE_NOOP("charts", "Zones"), QT_TRANSLATE_NOOP("charts", "Lines")}),
            inp("labels", QT_TRANSLATE_NOOP("charts", "Labels"), "select", "On", NaN, {QT_TRANSLATE_NOOP("charts", "On"), QT_TRANSLATE_NOOP("charts", "Off")})},
           {[] { Output o; o.key = "buy"; o.label = QT_TRANSLATE_NOOP("charts", "Buy-side liquidity"); o.extra = QJsonObject{{"themeColor", "down"}}; return o; }(),
            [] { Output o; o.key = "sell"; o.label = QT_TRANSLATE_NOOP("charts", "Sell-side liquidity"); o.extra = QJsonObject{{"themeColor", "up"}}; return o; }()},
           [](const Bars&, const Params& p) {
             Result r;
             QJsonObject o;
             for (const char* k : {"method", "tolUnit", "thickUnit", "sweep", "detectTf", "periods", "dayStart", "asia", "london", "newYork", "style", "labels"})
               o.insert(k, p.value(k));
             for (const char* k : {"length", "percent", "atrMult", "tolerance", "thickness", "keep", "minScore"}) o.insert(k, p.value(k));
             r.putExtra("smcliq", o);
             return r;
           },
           {}, QJsonObject{{"drawnOnly", true}});
  // Big Trades (order flow): bubbles of large aggressive orders found in the market's trades (Binance spot and USD-M
  // perpetuals). The trades are not in the bars: calc only hands the inputs to ChartPane, which draws the bubbles
  // (pages/charts/flow/BigTradesLayer) from the shared trade stream (charts/data/TradeStream).
  L << def("bigtrades", QT_TRANSLATE_NOOP("charts", "Big Trades"), QT_TRANSLATE_NOOP("charts", "Order flow"), true,
           {inp("mode", QT_TRANSLATE_NOOP("charts", "Threshold"), "select", "Auto", NaN, {QT_TRANSLATE_NOOP("charts", "Auto"), QT_TRANSLATE_NOOP("charts", "Manual")}),
            floatIn("percentile", QT_TRANSLATE_NOOP("charts", "Auto: percentile of recent orders"), 99),
            intIn("window", QT_TRANSLATE_NOOP("charts", "Auto: window (minutes)"), 30),
            inp("volatility", QT_TRANSLATE_NOOP("charts", "Auto: adjust to volatility"), "select", "On", NaN, {QT_TRANSLATE_NOOP("charts", "On"), QT_TRANSLATE_NOOP("charts", "Off")}),
            floatIn("min", QT_TRANSLATE_NOOP("charts", "Minimum volume (manual; auto while warming up)"), 0),
            floatIn("max", QT_TRANSLATE_NOOP("charts", "Maximum volume (0 = none)"), 0),
            intIn("aggWindow", QT_TRANSLATE_NOOP("charts", "Grouping, ms (0 = same timestamp)"), 50),
            inp("aggPrice", QT_TRANSLATE_NOOP("charts", "Prices in one order"), "select", "Same price", NaN, {QT_TRANSLATE_NOOP("charts", "Same price"), QT_TRANSLATE_NOOP("charts", "Sweep (several levels)")}),
            inp("shape", QT_TRANSLATE_NOOP("charts", "Marker"), "select", "Circle", NaN,
                {QT_TRANSLATE_NOOP("charts", "Circle"), QT_TRANSLATE_NOOP("charts", "Square"), QT_TRANSLATE_NOOP("charts", "Diamond"), QT_TRANSLATE_NOOP("charts", "Text")}),
            intIn("minSize", QT_TRANSLATE_NOOP("charts", "Smallest size (px)"), 8), intIn("maxSize", QT_TRANSLATE_NOOP("charts", "Largest size (px)"), 40),
            intIn("minOpacity", QT_TRANSLATE_NOOP("charts", "Smallest opacity (%)"), 35),
            inp("showVolume", QT_TRANSLATE_NOOP("charts", "Volume inside large markers"), "select", "Off", NaN, {QT_TRANSLATE_NOOP("charts", "Off"), QT_TRANSLATE_NOOP("charts", "On")}),
            inp("alert", QT_TRANSLATE_NOOP("charts", "Alert"), "select", "Off", NaN,
                {QT_TRANSLATE_NOOP("charts", "Off"), QT_TRANSLATE_NOOP("charts", "Visual"), QT_TRANSLATE_NOOP("charts", "Sound"), QT_TRANSLATE_NOOP("charts", "Sound and visual")}),
            floatIn("alertFactor", QT_TRANSLATE_NOOP("charts", "Alert: at least × the display threshold"), 3)},
           // colours only (drawnOnly: no values, so rules, alerts and "indicator on indicator" never list them)
           {[] { Output o; o.key = "buy"; o.label = QT_TRANSLATE_NOOP("charts", "Aggressive buys"); o.extra = QJsonObject{{"themeColor", "up"}}; return o; }(),
            [] { Output o; o.key = "sell"; o.label = QT_TRANSLATE_NOOP("charts", "Aggressive sells"); o.extra = QJsonObject{{"themeColor", "down"}}; return o; }()},
           [](const Bars&, const Params& p) {
             Result r;
             r.putExtra("bigTrades", QJsonObject{{"mode", str(p, "mode", "Auto")}, {"percentile", num(p, "percentile", 99)},
                                                 {"window", num(p, "window", 30)}, {"volatility", str(p, "volatility", "On")},
                                                 {"min", num(p, "min", 0)}, {"max", num(p, "max", 0)}, {"aggWindow", num(p, "aggWindow", 50)},
                                                 {"aggPrice", str(p, "aggPrice", "Same price")}, {"shape", str(p, "shape", "Circle")},
                                                 {"minSize", num(p, "minSize", 8)}, {"maxSize", num(p, "maxSize", 40)},
                                                 {"minOpacity", num(p, "minOpacity", 35)}, {"showVolume", str(p, "showVolume", "Off")},
                                                 {"alert", str(p, "alert", "Off")}, {"alertFactor", num(p, "alertFactor", 3)}});
             return r;
           },
           {}, QJsonObject{{"drawnOnly", true}});
  return L;
}

// The registry: INDICATORS (in order) plus BY_ID. Each Def is shared (DefPtr): replacing or removing an id drops the
// registry's reference only, so a worker still reading the old Def (byId() on another thread) keeps it alive — a raw
// pointer here was a use-after-free when a custom file was saved during a backtest.
struct Registry {
  QReadWriteLock lock;
  std::vector<DefPtr> store;   // in order
  QHash<QString, DefPtr> byId;
  QVector<Def> ordered;        // what all() hands out (copied under the lock), rebuilt on every change
  Registry() {
    for (Def& d : builtins()) {
      auto p = std::make_shared<const Def>(std::move(d));
      byId.insert(p->id, p);
      store.push_back(std::move(p));
    }
    rebuild();
  }
  // A new array every time, never `ordered.clear()`: readers hold copies of the old one (all() snapshots), and clear()
  // reuses an array in place once Qt's relaxed share count reads 1 — no happens-before with the reader that just let go
  // of it (ThreadSanitizer reports it). Replacing it only drops a reference.
  void rebuild() {
    QVector<Def> next;
    next.reserve(qsizetype(store.size()));
    for (const DefPtr& d : store) next << *d;
    ordered = std::move(next);
  }
};
Registry& reg() {
  static Registry r;
  return r;
}
}  // namespace

// By value: the copy is made under the lock (implicitly shared, so cheap); a reference to `ordered` was read while
// registerIndicator() rebuilt it on another thread.
QVector<Def> all() {
  Registry& r = reg();
  QReadLocker l(&r.lock);
  return r.ordered;
}

DefPtr byId(const QString& id) {
  Registry& r = reg();
  QReadLocker l(&r.lock);
  return r.byId.value(id);
}

Params defaults(const QString& id) {
  Params p;
  Registry& r = reg();
  QReadLocker l(&r.lock);
  if (const DefPtr d = r.byId.value(id))
    for (const Input& i : d->inputs) p.insert(i.key, i.def);
  return p;
}

// Compute an indicator. Unknown params fall back to defaults; returns { outputKey: number[] , markers?, barColors? }.
Result compute(const QString& id, const Bars& bars, const Params& params) {
  std::function<Result(const Bars&, const Params&)> calc;
  Params p;
  {
    Registry& r = reg();
    QReadLocker l(&r.lock);
    const DefPtr d = r.byId.value(id);
    if (!d) throw std::runtime_error(("Unknown indicator: " + id).toStdString());
    for (const Input& i : d->inputs) p.insert(i.key, i.def);
    calc = d->calc;
  }
  for (auto it = params.begin(); it != params.end(); ++it) p.insert(it.key(), it.value());
  if (!calc) throw std::runtime_error("def.calc is not a function");
  return calc(bars, p);
}

bool readsAhead(const Def& def, const QString& key, const Params& params) {
  for (const Output& o : def.outputs) {
    if (o.key != key || !o.readsAhead) continue;
    Params p;
    for (const Input& i : def.inputs) p.insert(i.key, i.def);
    for (auto it = params.begin(); it != params.end(); ++it) p.insert(it.key(), it.value());
    return o.readsAhead(p);
  }
  return false;
}

Def withCustomDefaults(Def d) {
  if (d.group.isEmpty()) d.group = QT_TRANSLATE_NOOP("charts", "Custom");
  if (d.outputs.isEmpty()) d.outputs << line("value", d.name.isEmpty() ? d.id : d.name, "#ff9800");
  return d;
}

// Custom indicators (files in /custom) register here. calc(bars, params, ta) gets the helpers.
// Each one is checked against the format first, so a malformed file is refused with its problems listed.
QStringList registerIndicator(const Def& in, bool run) {
  if (in.id.isEmpty()) return {QStringLiteral(QT_TRANSLATE_NOOP("charts", "A custom indicator needs an id"))};
  const Def full = withCustomDefaults(in);
  const QStringList pr = problems(full, run);
  if (!pr.isEmpty()) return pr;
  Registry& r = reg();
  QWriteLocker l(&r.lock);
  auto nw = std::make_shared<const Def>(full);
  if (const DefPtr old = r.byId.value(full.id)) {
    // Replace in place (same position in the list), as INDICATORS[i] = full. Holders of the old Def keep it.
    for (DefPtr& d : r.store)
      if (d == old) { d = nw; break; }
  } else {
    r.store.push_back(nw);
  }
  r.byId.insert(full.id, nw);
  r.rebuild();
  return {};
}

void unregisterIndicator(const QString& id) {
  Registry& r = reg();
  QWriteLocker l(&r.lock);
  const DefPtr d = r.byId.take(id);
  if (!d) return;
  r.store.erase(std::remove(r.store.begin(), r.store.end(), d), r.store.end());
  r.rebuild();
}

}  // namespace charts::ind
