// Port of quant-terminal public/lib/indicators.js (MIT, Copyright (c) 2026 Lewis Jackson): the ta primitives and
// the helpers the registry uses. The registry itself is in IndicatorsDefs.cpp, the format check in
// IndicatorsValidate.cpp.
//
// Technical indicators. Pure functions, no dependencies.
// Formulas follow the Pine Script reference (ta.* functions) so values line up with
// TradingView's built-ins: RMA-based RSI/ATR/ADX, SMA-seeded EMA, population stdev.
// Every series is a plain array the same length as the bars; missing values are NaN.
//
// Porting notes: the JS reads past either end of an array as `undefined`, which behaves like NaN in arithmetic and
// makes every comparison false; at() gives NaN there. A JS write to a negative index (out[-1] = x) only sets a
// property, never an element, so the loops below that the JS starts at n - 1 start at max(0, n - 1).
#include "charts/core/Indicators.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>

namespace charts::ind {

namespace {
const double N = NaN;
inline bool isN(double v) { return !std::isnan(v); }   // the JS file's isNum (Infinity is a number)
inline double at(const Series& s, qsizetype k) { return k >= 0 && k < s.size() ? s[k] : N; }
inline Series filled(qsizetype n, double v = N) { return Series(std::max<qsizetype>(0, n), v); }
// A JS read of a missing bar's field throws a TypeError; the C++ throws too.
inline const Bar& barAt(const Bars& bars, qsizetype k, const char* field) {
  if (k < 0 || k >= bars.size())
    throw std::runtime_error(std::string("Cannot read properties of undefined (reading '") + field + "')");
  return bars[k];
}
}  // namespace

double jsMax(std::initializer_list<double> v) {
  double m = -INFINITY;
  for (double x : v) {
    if (std::isnan(x)) return N;
    if (x > m) m = x;
  }
  return m;
}
double jsMin(std::initializer_list<double> v) {
  double m = INFINITY;
  for (double x : v) {
    if (std::isnan(x)) return N;
    if (x < m) m = x;
  }
  return m;
}
double jsRound(double v) {
  if (!std::isfinite(v)) return v;
  const double r = std::floor(v);
  return v - r >= 0.5 ? r + 1 : r;
}

QString numberToString(double v) {
  if (std::isnan(v)) return QStringLiteral("NaN");
  if (v == 0) return QStringLiteral("0");
  if (std::isinf(v)) return v > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
  if (v < 0) return "-" + numberToString(-v);
  // Shortest round-trip digits, then ECMAScript's Number::toString layout.
  const QString e = QString::number(v, 'e', QLocale::FloatingPointShortest);   // d.ddde+XX
  const int ep = e.indexOf('e');
  QString digits = e.left(ep);
  digits.remove('.');
  while (digits.size() > 1 && digits.endsWith('0')) digits.chop(1);
  const int k = digits.size(), n = e.mid(ep + 1).toInt() + 1;
  if (k <= n && n <= 21) return digits + QString(n - k, '0');
  if (0 < n && n <= 21) return digits.left(n) + "." + digits.mid(n);
  if (-6 < n && n <= 0) return "0." + QString(-n, '0') + digits;
  const int x = n - 1;
  return digits.left(1) + (k > 1 ? "." + digits.mid(1) : QString()) + "e" + (x >= 0 ? "+" : "-") +
         QString::number(std::abs(x));
}

// ---------- sources ----------
Series source(const Bars& bars, const QString& name) {
  Series out(bars.size());
  for (qsizetype i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    if (name == "open") out[i] = b.open;
    else if (name == "high") out[i] = b.high;
    else if (name == "low") out[i] = b.low;
    else if (name == "volume") out[i] = b.volume;   // b.volume ?? 0: a C++ bar always has one
    else if (name == "hl2") out[i] = (b.high + b.low) / 2;
    else if (name == "hlc3") out[i] = (b.high + b.low + b.close) / 3;
    else if (name == "ohlc4") out[i] = (b.open + b.high + b.low + b.close) / 4;
    else if (name == "hlcc4") out[i] = (b.high + b.low + 2 * b.close) / 4;
    else out[i] = b.close;
  }
  return out;
}

// ---------- building blocks (ta.*) ----------
Series change(const Series& src, int n) {
  Series out(src.size());
  for (qsizetype i = 0; i < src.size(); ++i) out[i] = i >= n ? src[i] - at(src, i - n) : N;
  return out;
}

Series sum(const Series& src, int n) {
  Series out = filled(src.size());
  double s = 0;
  int valid = 0;
  for (qsizetype i = 0; i < src.size(); ++i) {
    if (isN(src[i])) {
      s += src[i];
      valid++;
    }
    if (i >= n && isN(at(src, i - n))) {
      s -= at(src, i - n);
      valid--;
    }
    if (i >= n - 1 && valid == n) out[i] = s;
  }
  return out;
}

Series sma(const Series& src, int n) {
  Series out = sum(src, n);
  for (double& v : out) v = v / double(n);
  return out;
}

// Shared core for EMA and RMA: seeded with the SMA of the first n valid values, as Pine does.
static Series smoothed(const Series& src, int n, double alpha) {
  Series out = filled(src.size());
  double prev = N, seedSum = 0;
  int seedCount = 0;
  for (qsizetype i = 0; i < src.size(); ++i) {
    const double v = src[i];
    if (!isN(v)) continue;
    if (!isN(prev)) {
      seedSum += v;
      seedCount++;
      if (seedCount == n) {
        prev = seedSum / double(n);
        out[i] = prev;
      }
      continue;
    }
    prev = alpha * v + (1 - alpha) * prev;
    out[i] = prev;
  }
  return out;
}

Series ema(const Series& src, int n) { return smoothed(src, n, 2.0 / (double(n) + 1)); }
Series rma(const Series& src, int n) { return smoothed(src, n, 1.0 / double(n)); }

Series wma(const Series& src, int n) {
  Series out = filled(src.size());
  const double denom = (double(n) * (double(n) + 1)) / 2;
  for (qsizetype i = std::max(0, n - 1); i < src.size(); ++i) {
    double s = 0;
    bool ok = true;
    for (int j = 0; j < n; ++j) {
      const double v = at(src, i - j);
      if (!isN(v)) {
        ok = false;
        break;
      }
      s += v * double(n - j);
    }
    if (ok) out[i] = s / denom;
  }
  return out;
}

Series vwma(const Series& src, const Series& vol, int n) {
  Series x(src.size());
  for (qsizetype i = 0; i < src.size(); ++i) x[i] = src[i] * at(vol, i);
  const Series pv = sma(x, n), vv = sma(vol, n);
  Series out(pv.size());
  for (qsizetype i = 0; i < pv.size(); ++i) out[i] = jsTruthy(at(vv, i)) ? pv[i] / vv[i] : N;
  return out;
}

Series hma(const Series& src, int n) {
  // Math.max(1, Math.floor(Math.sqrt(n))) is NaN for a negative n, and a NaN length gives an all-NaN wma.
  const double root = std::floor(std::sqrt(double(n)));
  if (std::isnan(root)) return filled(src.size());
  const Series half = wma(src, std::max(1, int(std::floor(n / 2.0))));
  const Series full = wma(src, n);
  Series d(half.size());
  for (qsizetype i = 0; i < half.size(); ++i) d[i] = 2 * half[i] - at(full, i);
  return wma(d, std::max(1, int(root)));
}

Series dema(const Series& src, int n) {
  const Series e1 = ema(src, n), e2 = ema(e1, n);
  Series out(e1.size());
  for (qsizetype i = 0; i < e1.size(); ++i) out[i] = 2 * e1[i] - e2[i];
  return out;
}

Series tema(const Series& src, int n) {
  const Series e1 = ema(src, n), e2 = ema(e1, n), e3 = ema(e2, n);
  Series out(e1.size());
  for (qsizetype i = 0; i < e1.size(); ++i) out[i] = 3 * (e1[i] - e2[i]) + e3[i];
  return out;
}

Series alma(const Series& src, int n, double offset, double sigma) {
  // No window fits (n > bars, or a NaN length): every value is NaN, as in the JS, without building n weights.
  if (n > src.size()) return filled(src.size());
  const double m = offset * (double(n) - 1), s = double(n) / sigma;
  QVector<double> w;
  for (int i = 0; i < n; ++i) w << std::exp(-((i - m) * (i - m)) / (2 * s * s));
  double norm = 0;
  for (double x : w) norm += x;
  Series out = filled(src.size());
  for (qsizetype t = std::max(0, n - 1); t < src.size(); ++t) {
    double acc = 0;
    bool ok = true;
    for (int i = 0; i < n; ++i) {
      const double v = at(src, t - (n - 1 - i));
      if (!isN(v)) {
        ok = false;
        break;
      }
      acc += v * w[i];
    }
    if (ok) out[t] = acc / norm;
  }
  return out;
}

Series linreg(const Series& src, int n, double offset) {
  Series out = filled(src.size());
  const double dn = n;
  for (qsizetype t = std::max(0, n - 1); t < src.size(); ++t) {
    double sx = 0, sy = 0, sxy = 0, sxx = 0;
    bool ok = true;
    for (int i = 0; i < n; ++i) {
      const double y = at(src, t - n + 1 + i);
      if (!isN(y)) {
        ok = false;
        break;
      }
      sx += i;
      sy += y;
      sxy += i * y;
      sxx += double(i) * i;
    }
    if (!ok) continue;
    const double slope = (dn * sxy - sx * sy) / (dn * sxx - sx * sx);
    const double intercept = (sy - slope * sx) / dn;
    out[t] = intercept + slope * (dn - 1 - offset);
  }
  return out;
}

// Pine's ta.stdev (biased), as TradingView publishes it and PineTS runs it: the window's mean, then the mean of the
// squared deviations from it (two passes). quant-terminal's E[x²] − E[x]² (one pass) lost digits to cancellation:
// ~1e-11 relative on Bollinger bands, a few 1e-4 instead of 0 on flat windows. Changed 2026-11-18 (the user's rule:
// Pine is the definition; tests/charts/core/pine_reference_tests.cpp).
Series stdev(const Series& src, int n) {
  const Series m = sma(src, n);
  Series out(m.size(), N);
  for (qsizetype i = 0; i < m.size(); ++i) {
    const double mean = m[i];
    if (!isN(mean)) continue;
    double s = 0;
    for (int j = 0; j < n; ++j) {
      const double d = at(src, i - j) - mean;
      s += d * d;
    }
    out[i] = std::sqrt(s / double(n));
  }
  return out;
}

// Mean absolute deviation (ta.dev), used by CCI.
Series dev(const Series& src, int n) {
  const Series m = sma(src, n);
  Series out(m.size());
  for (qsizetype i = 0; i < m.size(); ++i) {
    const double mean = m[i];
    if (!isN(mean)) {
      out[i] = N;
      continue;
    }
    double s = 0;
    for (int j = 0; j < n; ++j) s += std::fabs(at(src, i - j) - mean);
    out[i] = s / double(n);
  }
  return out;
}

Series highest(const Series& src, int n) {
  Series out = filled(src.size());
  for (qsizetype i = std::max(0, n - 1); i < src.size(); ++i) {
    double h = -INFINITY;
    for (int j = 0; j < n; ++j)
      if (at(src, i - j) > h) h = src[i - j];
    out[i] = h;
  }
  return out;
}

Series lowest(const Series& src, int n) {
  Series out = filled(src.size());
  for (qsizetype i = std::max(0, n - 1); i < src.size(); ++i) {
    double l = INFINITY;
    for (int j = 0; j < n; ++j)
      if (at(src, i - j) < l) l = src[i - j];
    out[i] = l;
  }
  return out;
}

// Offset (0 or negative) of the highest/lowest value within the last n bars, like ta.highestbars.
Series extremeBars(const Series& src, int n, bool highest) {
  Series out = filled(src.size());
  for (qsizetype i = std::max(0, n - 1); i < src.size(); ++i) {
    double best = src[i], off = 0;
    for (int j = 1; j < n; ++j) {
      const double v = at(src, i - j);
      if (highest ? v > best : v < best) {
        best = v;
        off = -j;
      }
    }
    out[i] = off;
  }
  return out;
}

Series tr(const Bars& bars) {
  Series out(bars.size());
  for (qsizetype i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    out[i] = i == 0 ? b.high - b.low
                    : jsMax({b.high - b.low, std::fabs(b.high - bars[i - 1].close), std::fabs(b.low - bars[i - 1].close)});
  }
  return out;
}

Series atr(const Bars& bars, int n) { return rma(tr(bars), n); }

QVector<bool> crossover(const Series& a, const Series& b) {
  QVector<bool> out(a.size());
  for (qsizetype i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] > at(b, i) && a[i - 1] <= at(b, i - 1);
  return out;
}
QVector<bool> crossunder(const Series& a, const Series& b) {
  QVector<bool> out(a.size());
  for (qsizetype i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] < at(b, i) && a[i - 1] >= at(b, i - 1);
  return out;
}
QVector<bool> crossover(const Series& a, double b) {
  QVector<bool> out(a.size());
  for (qsizetype i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] > b && a[i - 1] <= b;
  return out;
}
QVector<bool> crossunder(const Series& a, double b) {
  QVector<bool> out(a.size());
  for (qsizetype i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] < b && a[i - 1] >= b;
  return out;
}
Series crossNumbers(const QVector<bool>& b) {
  Series out(b.size());
  for (qsizetype i = 0; i < b.size(); ++i) out[i] = b[i] ? 1 : 0;
  return out;
}

Series rsi(const Series& src, int n) {
  const Series ch = change(src);
  Series u(ch.size()), d(ch.size());
  for (qsizetype i = 0; i < ch.size(); ++i) {
    u[i] = isN(ch[i]) ? jsMax({ch[i], 0}) : N;
    d[i] = isN(ch[i]) ? jsMax({-ch[i], 0}) : N;
  }
  const Series up = rma(u, n), dn = rma(d, n);
  Series out(up.size());
  for (qsizetype i = 0; i < up.size(); ++i) {
    const double uu = up[i], dd = dn[i];
    if (!isN(uu) || !isN(dd)) out[i] = N;
    else if (dd == 0) out[i] = 100;
    else if (uu == 0) out[i] = 0;
    else out[i] = 100 - 100 / (1 + uu / dd);
  }
  return out;
}

Series stochOf(const Series& src, const Series& high, const Series& low, int n) {
  const Series hh = highest(high, n), ll = lowest(low, n);
  Series out(src.size());
  for (qsizetype i = 0; i < src.size(); ++i) {
    const double h = at(hh, i), l = at(ll, i);
    out[i] = h - l == 0 ? N : (100 * (src[i] - l)) / (h - l);
  }
  return out;
}

Series cum(const Series& src) {
  Series out(src.size());
  double s = 0;
  for (qsizetype i = 0; i < src.size(); ++i) out[i] = (s += isN(src[i]) ? src[i] : 0);
  return out;
}

Series shiftForward(const Series& src, int k) {
  // Value computed at bar i is drawn at bar i + k (k may be negative). A NaN k: new Array(NaN) throws in the JS.
  if (k == kNaNLength) throw std::runtime_error("Invalid array length");
  // Capped at 100,000 bars ahead: nobody looks that far into the future, and a shift of 1e9 allocated 8 GB.
  k = std::min(k, 100000);
  Series out = filled(src.size() + std::max(0, k));
  for (qsizetype i = 0; i < src.size(); ++i)
    if (i + k >= 0) out[i + k] = src[i];
  return out;
}

// highest/lowest skip a NaN inside their window; Pine gives na until the whole window is real.
Series whole(const Series& out, const Series& src, int n) {
  Series r(out.size());
  for (qsizetype i = 0; i < out.size(); ++i) {
    r[i] = out[i];
    for (int j = 0; j < n; ++j)
      if (!isN(at(src, i - j))) {
        r[i] = N;
        break;
      }
  }
  return r;
}

// Pearson correlation of two equal-length arrays (ta.correlation). NaN when either one is flat.
double pearson(const Series& x, const Series& y) {
  const qsizetype n = x.size();
  double sx = 0, sy = 0;
  for (qsizetype i = 0; i < n; ++i) {
    sx += x[i];
    sy += at(y, i);
  }
  const double mx = sx / double(n), my = sy / double(n);
  double sxy = 0, sxx = 0, syy = 0;
  for (qsizetype i = 0; i < n; ++i) {
    const double dx = x[i] - mx, dy = at(y, i) - my;
    sxy += dx * dy;
    sxx += dx * dx;
    syy += dy * dy;
  }
  return jsTruthy(sxx) && jsTruthy(syy) ? sxy / std::sqrt(sxx * syy) : N;
}

// Ranks 1..n from lowest to highest; tied values share the average of the ranks they span.
// By sorting (n log n; it compared every pair, n², which made rci a hang for long windows). Same numbers as the pair
// count: a value's rank is less + (eq + 1) / 2 with less / eq counted among the non-NaN values; a NaN (never less than
// or equal to anything) gets 0.5.
Series avgRanks(const Series& v) {
  Series out(v.size(), 0.5);
  std::vector<qsizetype> idx;
  idx.reserve(size_t(v.size()));
  for (qsizetype i = 0; i < v.size(); ++i) if (!std::isnan(v[i])) idx.push_back(i);
  std::sort(idx.begin(), idx.end(), [&](qsizetype a, qsizetype b) { return v[a] < v[b]; });
  for (size_t i = 0; i < idx.size();) {
    size_t j = i + 1;
    while (j < idx.size() && v[idx[j]] == v[idx[i]]) ++j;
    const double rank = double(i) + (double(j - i) + 1) / 2.0;
    for (size_t k = i; k < j; ++k) out[idx[k]] = rank;
    i = j;
  }
  return out;
}

// Share of the n values before this one that are lower or equal, in percent (ta.percentrank).
Series percentRank(const Series& src, int n) {
  Series out(src.size());
  for (qsizetype i = 0; i < src.size(); ++i) {
    const double v = src[i];
    if (i < n || !isN(v)) {
      out[i] = N;
      continue;
    }
    int less = 0;
    bool bad = false;
    for (int j = 1; j <= n; ++j) {
      const double w = at(src, i - j);
      if (!isN(w)) {
        bad = true;
        break;
      }
      if (w <= v) less++;
    }
    out[i] = bad ? N : (100.0 * less) / double(n);
  }
  return out;
}

// Consecutive-close streak: +1, +2, ... on up closes, -1, -2, ... on down closes, 0 when unchanged.
Series updownStreak(const Series& src) {
  Series out(src.size());
  double ud = 0;
  for (qsizetype i = 0; i < src.size(); ++i) {
    const double v = src[i];
    if (i == 0 || v == src[i - 1]) ud = 0;
    else if (v > src[i - 1]) ud = ud <= 0 ? 1 : ud + 1;
    else ud = ud >= 0 ? -1 : ud - 1;
    out[i] = ud;
  }
  return out;
}

// ta.swma: weights 1, 2, 2, 1 over the last four values.
Series swma(const Series& src) {
  Series out(src.size());
  for (qsizetype i = 0; i < src.size(); ++i)
    out[i] = i >= 3 ? (src[i - 3] + 2 * src[i - 2] + 2 * src[i - 1] + src[i]) / 6 : N;
  return out;
}

// Period key used by VWAP resets and pivot points. Times are UTC seconds.
QString periodKey(qint64 t, const QString& period) {
  auto floorDiv = [](qint64 a, qint64 b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); };
  const qint64 day = floorDiv(t, 86400);
  if (period == "day" || period == "month" || period == "year") {
    // new Date(t * 1000) is an Invalid Date beyond +-8.64e15 ms.
    if (t > 8640000000000LL || t < -8640000000000LL)
      return period == "day" ? QStringLiteral("NaN-NaN-NaN") : period == "month" ? QStringLiteral("NaN-NaN") : QStringLiteral("NaN");
    // Civil date from days since 1970-01-01 (H. Hinnant's algorithm).
    const qint64 z = day + 719468;
    const qint64 era = floorDiv(z, 146097);
    const qint64 doe = z - era * 146097;
    const qint64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const qint64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const qint64 mp = (5 * doy + 2) / 153;
    const qint64 d = doy - (153 * mp + 2) / 5 + 1;
    const qint64 m = mp < 10 ? mp + 3 : mp - 9;
    const qint64 y = yoe + era * 400 + (m <= 2 ? 1 : 0);
    if (period == "day") return QStringLiteral("%1-%2-%3").arg(y).arg(m - 1).arg(d);
    if (period == "month") return QStringLiteral("%1-%2").arg(y).arg(m - 1);
    return QString::number(y);
  }
  // ISO week, Monday start
  return QStringLiteral("w%1").arg(floorDiv(day + 3, 7));
}

// ---------- candle patterns ----------
QVector<Marker> candlePatterns(const Bars& bars) {
  QVector<Marker> markers;
  for (qsizetype i = 2; i < bars.size(); ++i) {
    const Bar &b = bars[i], &p = bars[i - 1], &pp = bars[i - 2];
    const double body = std::fabs(b.close - b.open), range = jsOr(b.high - b.low, 1e-12);
    const double upper = b.high - jsMax({b.open, b.close}), lower = jsMin({b.open, b.close}) - b.low;
    const double pBody = std::fabs(p.close - p.open);
    const bool bull = b.close > b.open, pBull = p.close > p.open;
    QString name;
    bool up = true;
    if (!pBull && bull && b.open <= p.close && b.close >= p.open && body > pBody)
      name = QT_TRANSLATE_NOOP("charts", "Engulfing");
    else if (pBull && !bull && b.open >= p.close && b.close <= p.open && body > pBody) {
      name = QT_TRANSLATE_NOOP("charts", "Engulfing");
      up = false;
    } else if (lower >= 2 * body && upper <= body * 0.5 && body / range < 0.35)
      name = QT_TRANSLATE_NOOP("charts", "Hammer");
    else if (upper >= 2 * body && lower <= body * 0.5 && body / range < 0.35) {
      name = QT_TRANSLATE_NOOP("charts", "Star");
      up = false;
    } else if (pp.close < pp.open && std::fabs(p.close - p.open) / jsOr(p.high - p.low, 1) < 0.3 && bull &&
               b.close > (pp.open + pp.close) / 2)
      name = QT_TRANSLATE_NOOP("charts", "Star");
    else if (pp.close > pp.open && std::fabs(p.close - p.open) / jsOr(p.high - p.low, 1) < 0.3 && !bull &&
             b.close < (pp.open + pp.close) / 2) {
      name = QT_TRANSLATE_NOOP("charts", "Star");
      up = false;
    }
    if (!name.isEmpty()) {
      Marker m;
      m.time = b.time;
      m.position = up == false ? "aboveBar" : "belowBar";
      m.shape = up ? "arrowUp" : "arrowDown";   // the JS also has a "circle" / "#9598a1" case for up === null, never reached
      m.color = up ? "#26a69a" : "#ef5350";
      m.text = name;
      markers << m;
    }
  }
  return markers;
}

// Pivot highs and lows like ta.pivothigh / ta.pivotlow: a bar is a pivot once `right` later bars have closed and
// nothing in its window of left + right + 1 bars beats it. Ties go to the earlier bar: the pivot must be strictly
// beyond the `left` bars before it and at least level with the `right` bars after it. The label sits on the pivot
// bar itself and shows its price.
QVector<Marker> pivotMarkers(const Bars& bars, int left, int right) {
  QVector<Marker> markers;
  for (qsizetype c = left; c + right < bars.size(); ++c) {
    bool isHigh = true, isLow = true;
    for (int j = 1; j <= left; ++j) {
      if (barAt(bars, c - j, "high").high >= barAt(bars, c, "high").high) isHigh = false;
      if (barAt(bars, c - j, "low").low <= barAt(bars, c, "low").low) isLow = false;
    }
    for (int j = 1; j <= right; ++j) {
      if (barAt(bars, c + j, "high").high > barAt(bars, c, "high").high) isHigh = false;
      if (barAt(bars, c + j, "low").low < barAt(bars, c, "low").low) isLow = false;
    }
    if (isHigh) {
      const Bar& b = barAt(bars, c, "time");
      markers << Marker{b.time, "aboveBar", "circle", "#ef5350", numberToString(b.high), {}};
    }
    if (isLow) {
      const Bar& b = barAt(bars, c, "time");
      markers << Marker{b.time, "belowBar", "circle", "#26a69a", numberToString(b.low), {}};
    }
  }
  return markers;
}

// Williams fractals with the rules of TradingView's built-in: the n bars after the fractal bar must be strictly
// lower (higher for a down fractal); before it there must be n strictly lower bars, optionally behind up to four
// bars that only match it. Marked on the fractal bar, so the newest n bars never carry one.
QVector<Marker> fractalMarkers(const Bars& bars, int n) {
  QVector<Marker> markers;
  // val(i) is "more extreme is higher": highs for an up fractal, negated lows for a down fractal.
  auto isFractal = [&](qsizetype c, const std::function<double(qsizetype)>& val) {
    const double x = val(c);
    for (int j = 1; j <= n; ++j)
      if (!(val(c + j) < x)) return false;
    for (int k = 0; k <= 4; ++k) {
      bool ok = true;
      for (int j = 1; j <= k; ++j)
        if (!(val(c - j) <= x)) ok = false;
      for (int j = 1; j <= n && ok; ++j)
        if (!(val(c - k - j) < x)) ok = false;
      if (ok) return true;
    }
    return false;
  };
  // bars[i]?.high: undefined (NaN) past either end.
  const auto high = [&](qsizetype i) { return i >= 0 && i < bars.size() ? bars[i].high : N; };
  const auto low = [&](qsizetype i) { return i >= 0 && i < bars.size() ? -bars[i].low : N; };
  for (qsizetype c = n; c + n < bars.size(); ++c) {
    if (isFractal(c, high)) markers << Marker{barAt(bars, c, "time").time, "aboveBar", "arrowDown", "#ef5350", {}, {}};
    if (isFractal(c, low)) markers << Marker{barAt(bars, c, "time").time, "belowBar", "arrowUp", "#26a69a", {}, {}};
  }
  return markers;
}

Macd macdFrom(const Series& s, int fast, int slow, int signal) {
  const Series f = ema(s, fast), sl = ema(s, slow);
  Macd r;
  r.macd.resize(f.size());
  for (qsizetype i = 0; i < f.size(); ++i) r.macd[i] = f[i] - sl[i];
  r.signal = ema(r.macd, signal);
  r.hist.resize(r.macd.size());
  for (qsizetype i = 0; i < r.macd.size(); ++i) r.hist[i] = r.macd[i] - r.signal[i];
  for (qsizetype i = 0; i < r.hist.size(); ++i) {
    const double v = r.hist[i], prev = i ? r.hist[i - 1] : 0;
    if (v >= 0) r.colors << (v >= prev ? "#26a69a" : "#b2dfdb");
    else r.colors << (v <= prev ? "#ef5350" : "#ffcdd2");
  }
  return r;
}

Dmi dmiOf(const Bars& bars, int diLen, int adxLen) {
  const Series t = tr(bars);
  const qsizetype n = bars.size();
  Series plusDM(n), minusDM(n), t1(n);
  for (qsizetype i = 0; i < n; ++i) {
    if (!i) {
      plusDM[i] = minusDM[i] = t1[i] = N;
      continue;
    }
    const double up = bars[i].high - bars[i - 1].high, down = bars[i - 1].low - bars[i].low;
    plusDM[i] = up > down && up > 0 ? up : 0;
    minusDM[i] = down > up && down > 0 ? down : 0;
    t1[i] = t[i];
  }
  const Series trur = rma(t1, diLen);
  Dmi r;
  r.plus = rma(plusDM, diLen);
  r.minus = rma(minusDM, diLen);
  for (qsizetype i = 0; i < n; ++i) {
    r.plus[i] = (100 * r.plus[i]) / trur[i];
    r.minus[i] = (100 * r.minus[i]) / trur[i];
  }
  Series dx(n);
  for (qsizetype i = 0; i < n; ++i) {
    const double s = r.plus[i] + r.minus[i];
    dx[i] = std::fabs(r.plus[i] - r.minus[i]) / (s == 0 ? 1 : s);
  }
  r.adx = rma(dx, adxLen);
  for (double& v : r.adx) v = 100 * v;
  return r;
}

Series psarOf(const Bars& bars, double start, double inc, double max) {
  Series out = filled(bars.size());
  if (bars.size() < 2) return out;
  bool up = bars[1].close >= bars[0].close;
  double af = start, ep = up ? bars[0].high : bars[0].low, sar = up ? bars[0].low : bars[0].high;
  for (qsizetype i = 1; i < bars.size(); ++i) {
    const Bar &b = bars[i], &p = bars[i - 1];
    sar = sar + af * (ep - sar);
    if (up) {
      sar = jsMin({sar, p.low, i > 1 ? bars[i - 2].low : p.low});
      if (b.low < sar) {
        up = false;
        sar = ep;
        ep = b.low;
        af = start;
      } else if (b.high > ep) {
        ep = b.high;
        af = jsMin({af + inc, max});
      }
    } else {
      sar = jsMax({sar, p.high, i > 1 ? bars[i - 2].high : p.high});
      if (b.high > sar) {
        up = true;
        sar = ep;
        ep = b.high;
        af = start;
      } else if (b.low < ep) {
        ep = b.low;
        af = jsMin({af + inc, max});
      }
    }
    out[i] = sar;
  }
  return out;
}

Supertrend supertrendOf(const Bars& bars, double factor, int atrLen) {
  const Series a = atr(bars, atrLen);
  Supertrend r{filled(bars.size()), filled(bars.size())};
  double prevUpper = N, prevLower = N, prevSt = N;
  for (qsizetype i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    const double mid = (b.high + b.low) / 2;
    if (!isN(a[i])) continue;
    double upper = mid + factor * a[i], lower = mid - factor * a[i];
    const double prevClose = i ? bars[i - 1].close : N;
    if (isN(prevLower)) lower = lower > prevLower || prevClose < prevLower ? lower : prevLower;
    if (isN(prevUpper)) upper = upper < prevUpper || prevClose > prevUpper ? upper : prevUpper;
    double d;
    if (!isN(prevSt)) d = 1;
    else if (prevSt == prevUpper) d = b.close > upper ? -1 : 1;
    else d = b.close < lower ? 1 : -1;
    r.st[i] = d == -1 ? lower : upper;
    r.dir[i] = d;
    prevUpper = upper;
    prevLower = lower;
    prevSt = r.st[i];
  }
  return r;
}

// Volatility Stop: a trailing stop that rides the best close of the trend minus a multiple of ATR, and flips
// (restarting from the closing price) when the close crosses it. While ATR is still warming up it uses the raw true range.
Result vstopOf(const Bars& bars, int length, double factor) {
  const Series a = atr(bars, length), t = tr(bars);
  Series up = filled(bars.size()), down = filled(bars.size());
  double max = N, min = N, stop = N;
  bool uptrend = true;
  for (qsizetype i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    const double m = isN(a[i]) ? a[i] * factor : t[i];
    max = i ? jsMax({max, b.close}) : b.close;
    min = i ? jsMin({min, b.close}) : b.close;
    stop = !isN(stop) ? b.close : uptrend ? jsMax({stop, max - m}) : jsMin({stop, min + m});
    const bool was = uptrend;
    uptrend = b.close - stop >= 0;
    if (uptrend != was) {
      max = min = b.close;
      stop = uptrend ? max - m : min + m;
    }
    (uptrend ? up : down)[i] = stop;
  }
  Result r;
  r.put("up", up);
  r.put("down", down);
  return r;
}

// Zig Zag: follow the current leg's extreme and call it a pivot once price has reversed `deviation` percent from it,
// provided the extreme is at least `depth` bars after the previous pivot. A move through the previous pivot's
// price before that happens means the pivot was not the end of its swing, so it moves to the new extreme.
// The line runs straight between pivots and the unconfirmed last leg ends at its latest extreme.
Series zigzagOf(const Bars& bars, double deviation, double depth) {
  struct Piv { qsizetype i; double price; };
  const qsizetype n = bars.size();
  Series out = filled(n);
  const double dev = deviation / 100;
  QVector<Piv> piv;   // confirmed pivots { i, price }, alternating high and low
  int dir = 0;        // 0 no leg yet, 1 rising (tracking the high), -1 falling (tracking the low)
  qsizetype ext = 0;  // bar of the current leg's extreme
  qsizetype hiI = 0, loI = 0;   // running high and low while no leg has been confirmed
  for (qsizetype i = 1; i < n; ++i) {
    const Bar& b = bars[i];
    if (dir == 0) {
      if (b.high > bars[hiI].high) hiI = i;
      if (b.low < bars[loI].low) loI = i;
      if (i > hiI && b.low <= bars[hiI].high * (1 - dev)) {
        piv << Piv{hiI, bars[hiI].high};
        dir = -1;
        ext = i;
      } else if (i > loI && b.high >= bars[loI].low * (1 + dev)) {
        piv << Piv{loI, bars[loI].low};
        dir = 1;
        ext = i;
      }
      continue;
    }
    const Piv last = piv.last();
    if (dir == 1) {
      if (b.high > bars[ext].high) ext = i;
      else if (b.low <= bars[ext].high * (1 - dev) && double(ext - last.i) >= depth) {
        piv << Piv{ext, bars[ext].high};
        dir = -1;
        ext = i;
      } else if (b.low < last.price) {
        piv.last() = Piv{i, b.low};
        ext = i;
      }
    } else if (b.low < bars[ext].low) ext = i;
    else if (b.high >= bars[ext].low * (1 + dev) && double(ext - last.i) >= depth) {
      piv << Piv{ext, bars[ext].low};
      dir = 1;
      ext = i;
    } else if (b.high > last.price) {
      piv.last() = Piv{i, b.high};
      ext = i;
    }
  }
  if (dir != 0 && ext > piv.last().i) piv << Piv{ext, dir == 1 ? bars[ext].high : bars[ext].low};
  for (qsizetype k = 0; k < piv.size(); ++k) {
    const Piv& a = piv[k];
    out[a.i] = a.price;
    if (k + 1 < piv.size()) {
      const Piv& b = piv[k + 1];
      for (qsizetype i = a.i + 1; i < b.i; ++i)
        out[i] = a.price + ((b.price - a.price) * double(i - a.i)) / double(b.i - a.i);
    }
  }
  return out;
}

Result pivotsOf(const Bars& bars, const QString& period) {
  QString per = period;
  if (per == "auto") {
    const qint64 step = bars.size() > 1 ? bars[1].time - bars[0].time : 86400;
    per = step <= 900 ? "day" : step < 86400 ? "week" : step < 7 * 86400 ? "month" : "year";
  }
  const qsizetype n = bars.size();
  Series P_ = filled(n), r1 = filled(n), s1 = filled(n), r2 = filled(n), s2 = filled(n), r3 = filled(n), s3 = filled(n);
  QVector<bool> colorsBreak(n, false);
  struct Hlc { double h, l, c; };
  QString key;
  bool hasKey = false, hasCur = false, hasPrev = false;
  Hlc cur{}, prev{};
  for (qsizetype i = 0; i < n; ++i) {
    const Bar& b = bars[i];
    const QString k = periodKey(b.time, per);
    if (!hasKey || k != key) {
      if (hasCur) {
        prev = cur;
        hasPrev = true;
      }
      cur = {b.high, b.low, b.close};
      hasCur = true;
      key = k;
      hasKey = true;
      colorsBreak[i] = true;
    } else {
      cur.h = jsMax({cur.h, b.high});
      cur.l = jsMin({cur.l, b.low});
      cur.c = b.close;
    }
    if (hasPrev) {
      const double P = (prev.h + prev.l + prev.c) / 3;
      P_[i] = P;
      r1[i] = 2 * P - prev.l;
      s1[i] = 2 * P - prev.h;
      r2[i] = P + (prev.h - prev.l);
      s2[i] = P - (prev.h - prev.l);
      r3[i] = prev.h + 2 * (P - prev.l);
      s3[i] = prev.l - 2 * (prev.h - P);
    }
  }
  Result r;
  r.put("p", P_);
  r.put("r1", r1);
  r.put("s1", s1);
  r.put("r2", r2);
  r.put("s2", s2);
  r.put("r3", r3);
  r.put("s3", s3);
  r.putBreaks(colorsBreak);
  return r;
}

}  // namespace charts::ind
