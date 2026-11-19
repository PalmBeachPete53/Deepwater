// Port of quant-terminal public/lib/series.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Timeframes, resampling and chart-type transforms. Shared by the browser and the server.
#include "charts/core/Series.h"
#include <QRegularExpression>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Exactness with the JS: no fused multiply-add (clang contracts a*b+c by default on arm64).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#else
#pragma STDC FP_CONTRACT OFF
#endif

namespace charts::series {

namespace {

// JS Math.max / Math.min of two numbers: NaN wins, +0 beats -0.
double jsMax(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return NaN;
  if (a == b) return std::signbit(a) ? b : a;
  return a > b ? a : b;
}
double jsMin(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return NaN;
  if (a == b) return std::signbit(a) ? a : b;
  return a < b ? a : b;
}
// JS Math.round: halves go up (towards +Infinity).
double jsRound(double x) {
  if (!std::isfinite(x)) return x;
  const double f = std::floor(x);
  return x - f >= 0.5 ? f + 1 : f;
}
// JS `%` on numbers (sign of the dividend).
double jsMod(double a, double b) { return std::fmod(a, b); }
// `x || fallback` for a number: 0 and NaN are falsy.
double orElse(double x, double fallback) { return (x == 0 || std::isnan(x)) ? fallback : x; }

double unitSec(const QString& u) {
  if (u == "s") return 1;
  if (u == "m") return 60;
  if (u == "h") return 3600;
  if (u == "D") return 86400;
  if (u == "W") return 604800;
  return 2592000;   // M
}

qint64 floorDiv(qint64 a, qint64 b) { qint64 q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }

// Howard Hinnant's civil calendar algorithms (proleptic Gregorian, as JS Date).
qint64 daysFromCivil(qint64 y, qint64 m, qint64 d) {   // m 1..12
  y -= m <= 2;
  const qint64 era = (y >= 0 ? y : y - 399) / 400;
  const qint64 yoe = y - era * 400;
  const qint64 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const qint64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
void civilFromDays(qint64 z, qint64& y, int& m, int& d) {
  z += 719468;
  const qint64 era = (z >= 0 ? z : z - 146096) / 146097;
  const qint64 doe = z - era * 146097;
  const qint64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = yoe + era * 400;
  const qint64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const qint64 mp = (5 * doy + 2) / 153;
  d = int(doy - (153 * mp + 2) / 5 + 1);
  m = int(mp < 10 ? mp + 3 : mp - 9);
  y += (m <= 2);
}

// A bar without a volume (JS bricks): volume NaN, see Series.h.
Bar brick(qint64 time, double open, double high, double low, double close) {
  Bar b; b.time = time; b.open = open; b.high = high; b.low = low; b.close = close; b.volume = NaN; return b;
}

// const uniqueTimes = () => { let last = -Infinity; return (t) => (last = Math.max(t, last + 1)); };
struct UniqueTimes {
  double last = -std::numeric_limits<double>::infinity();
  qint64 operator()(qint64 t) { last = jsMax(double(t), last + 1); return qint64(last); }
};

}  // namespace

// ---------------------------------------------------------------- JS formatting helpers

QString numStr(double v) {
  if (std::isnan(v)) return QStringLiteral("NaN");
  if (std::isinf(v)) return v > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
  if (v == 0) return QStringLiteral("0");
  // Shortest round-trip digits (Qt and V8 both use double-conversion), then Number::toString's layout.
  const QString e = QString::number(std::fabs(v), 'e', QLocale::FloatingPointShortest);
  const int ePos = e.indexOf(QLatin1Char('e'));
  QString digits = e.left(ePos);
  digits.remove(QLatin1Char('.'));
  const int exp = e.mid(ePos + 1).toInt();
  const int k = digits.size(), n = exp + 1;
  QString out;
  if (k <= n && n <= 21) out = digits + QString(n - k, QLatin1Char('0'));
  else if (0 < n && n <= 21) out = digits.left(n) + QLatin1Char('.') + digits.mid(n);
  else if (-6 < n && n <= 0) out = QStringLiteral("0.") + QString(-n, QLatin1Char('0')) + digits;
  else {
    out = digits.left(1);
    if (k > 1) out += QLatin1Char('.') + digits.mid(1);
    out += QLatin1Char('e') + QString(n - 1 >= 0 ? "+" : "-") + QString::number(std::abs(n - 1));
  }
  return v < 0 ? QLatin1Char('-') + out : out;
}

QString toFixed(double x, int f) {
  if (std::isnan(x)) return QStringLiteral("NaN");
  if (std::fabs(x) >= 1e21) return numStr(x);
  if (x < 0) return QLatin1Char('-') + toFixed(-x, f);
  // The exact decimal value of the double (macOS / glibc printf print it exactly), then the spec's rounding:
  // the n nearest x * 10^f, the larger one on a tie.
  std::vector<char> buf(1200 + 32);
  std::snprintf(buf.data(), buf.size(), "%.1100f", x);
  std::string s(buf.data());
  const size_t dot = s.find('.');
  std::string intPart = s.substr(0, dot), frac = s.substr(dot + 1);
  std::string keep = intPart + frac.substr(0, size_t(f));
  const bool up = size_t(f) < frac.size() && frac[size_t(f)] >= '5';
  if (up) {
    int i = int(keep.size()) - 1;
    while (i >= 0 && keep[size_t(i)] == '9') { keep[size_t(i)] = '0'; --i; }
    if (i < 0) keep.insert(keep.begin(), '1');
    else ++keep[size_t(i)];
  }
  const size_t intLen = keep.size() - size_t(f);
  std::string out = keep.substr(0, intLen);
  if (f > 0) out += "." + keep.substr(intLen);
  return QString::fromStdString(out);
}

void utcDate(qint64 t, int& year, int& month, int& day) {
  qint64 y; int m, d;
  civilFromDays(floorDiv(t, 86400), y, m, d);
  year = int(y); month = m - 1; day = d;
}

qint64 utcSeconds(qint64 year, qint64 month0, qint64 day) {
  const qint64 y = year + floorDiv(month0, 12), m = month0 - floorDiv(month0, 12) * 12;
  return (daysFromCivil(y, m + 1, 1) + day - 1) * 86400;
}

QString isoString(qint64 t) {
  // TimeClip: beyond 8.64e15 ms the Date is invalid and toISOString throws a RangeError.
  if (t > 8640000000000LL || t < -8640000000000LL) throw std::runtime_error("Invalid time value");
  const qint64 day = floorDiv(t, 86400), sec = t - day * 86400;
  qint64 y; int m, d;
  civilFromDays(day, y, m, d);
  char buf[64];
  if (y >= 0 && y <= 9999)
    std::snprintf(buf, sizeof buf, "%04lld-%02d-%02dT%02lld:%02lld:%02lld.000Z", (long long)y, m, d,
                  (long long)(sec / 3600), (long long)(sec / 60 % 60), (long long)(sec % 60));
  else
    std::snprintf(buf, sizeof buf, "%c%06lld-%02d-%02dT%02lld:%02lld:%02lld.000Z", y < 0 ? '-' : '+',
                  (long long)std::llabs(y), m, d, (long long)(sec / 3600), (long long)(sec / 60 % 60),
                  (long long)(sec % 60));
  return QString::fromLatin1(buf);
}

// ---------------------------------------------------------------- timeframes

Tf parseTf(const QString& tf) {
  static const QRegularExpression re(QStringLiteral("^([0-9]+)?([smhDWM])$"));
  const auto m = re.match(tf.trimmed());
  if (!m.hasMatch())
    throw std::runtime_error(
        QStringLiteral(QT_TRANSLATE_NOOP("charts", "Bad timeframe \"%1\". Use forms like 1s, 5m, 4h, 1D, 1W, 1M.")).arg(tf).toStdString());
  const double n = m.captured(1).isEmpty() ? 1 : m.captured(1).toDouble();
  // above a million units (1000000m is 1.9 years) the count only overflows later sums (bucket sizes, limit × ratio)
  if (n < 1 || n > 1e6) throw std::runtime_error(QStringLiteral(QT_TRANSLATE_NOOP("charts", "Bad timeframe \"%1\"")).arg(tf).toStdString());
  Tf out;
  out.n = n;
  out.unit = m.captured(2);
  out.seconds = n * unitSec(out.unit);
  return out;
}

bool isIntraday(const QString& tf) { return parseTf(tf).seconds < 86400; }

qint64 bucketStart(qint64 t, const Tf& tf) {
  const double n = tf.n;
  if (tf.unit == "M") {
    int y, mo, d;
    utcDate(t, y, mo, d);
    const double months = double(y) * 12 + mo;
    const double start = months - jsMod(months, n);
    return utcSeconds(qint64(std::floor(start / 12)), qint64(jsMod(start, 12)), 1);
  }
  if (tf.unit == "W") {
    const double day = std::floor(double(t) / 86400);
    const double monday = day - jsMod(day + 3, 7);   // 1970-01-01 was a Thursday
    const double week = std::floor(monday / 7);
    return qint64((monday - jsMod(week, n) * 7) * 86400);
  }
  const double size = n * unitSec(tf.unit);
  return qint64(std::floor(double(t) / size) * size);
}

qint64 bucketStart(qint64 t, const QString& tf) { return bucketStart(t, parseTf(tf)); }

// Merge bars into a larger timeframe. Bars must be sorted ascending.
Bars resample(const Bars& bars, const QString& tf) {
  Bars out;
  if (bars.isEmpty()) return out;
  const Tf p = parseTf(tf);
  bool have = false;
  for (const Bar& b : bars) {
    const qint64 start = bucketStart(b.time, p);
    const double vol = orElse(b.volume, 0);
    if (!have || out.last().time != start) {
      Bar c;
      c.time = start; c.open = b.open; c.high = b.high; c.low = b.low; c.close = b.close; c.volume = vol;
      out.push_back(c);
      have = true;
    } else {
      Bar& c = out.last();
      c.high = jsMax(c.high, b.high);
      c.low = jsMin(c.low, b.low);
      c.close = b.close;
      c.volume += vol;
    }
  }
  return out;
}

// Pick the largest native timeframe that divides the requested one.
QString nativeBase(const QString& tf, const QStringList& natives) {
  const Tf want = parseTf(tf);
  if (natives.contains(tf)) return tf;
  struct C { QString tf; Tf p; };
  QVector<C> candidates;
  for (const QString& n : natives) {
    const Tf c = parseTf(n);
    bool keep;
    if (want.unit == "M")
      keep = c.unit == "M" ? jsMod(want.n, c.n) == 0 : c.seconds <= 86400;
    else if (want.unit == "W")
      keep = c.unit == "W" ? jsMod(want.n, c.n) == 0 : c.seconds <= 86400 && jsMod(86400, c.seconds) == 0;
    else
      keep = c.unit != "M" && c.unit != "W" && c.seconds <= want.seconds && jsMod(want.seconds, c.seconds) == 0;
    if (keep) candidates.push_back({n, c});
  }
  std::stable_sort(candidates.begin(), candidates.end(), [](const C& a, const C& b) { return b.p.seconds < a.p.seconds; });
  return candidates.isEmpty() ? QString() : candidates.first().tf;
}

// ---------------------------------------------------------------- chart types

Bars heikinAshi(const Bars& bars) {
  Bars out;
  out.reserve(bars.size());
  for (int i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    const double close = (b.open + b.high + b.low + b.close) / 4;
    const double open = i == 0 ? (b.open + b.close) / 2 : (out[i - 1].open + out[i - 1].close) / 2;
    Bar h;
    h.time = b.time;
    h.open = open;
    h.close = close;
    h.high = jsMax(jsMax(b.high, open), close);
    h.low = jsMin(jsMin(b.low, open), close);
    h.volume = b.volume;
    out.push_back(h);
  }
  return out;
}

// Renko from closes. boxSize 0 = ATR(14) of the last bar. Bricks get unique, increasing times.
namespace {
// Port guard for the brick / range charts (renko, range bars): the box or range they get, made safe.
//  - nothing to draw (0) when it is below the prices' float resolution: `base + box == base` kept the builders'
//    `while (true)` from advancing and they pushed bricks until memory ran out (flat history, one absurd print);
//  - raised so the bars' whole path makes at most kBrickBudget bricks (each brick costs a bar on the GUI thread).
constexpr double kBrickBudget = 200000;
double safeBox(const Bars& bars, double box) {
  if (!(box > 0) || !std::isfinite(box)) return 0;
  double maxAbs = 0, path = 0;
  for (qsizetype i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    for (double v : {b.open, b.high, b.low, b.close})
      if (std::isfinite(v)) maxAbs = std::max(maxAbs, std::fabs(v));
    if (std::isfinite(b.high) && std::isfinite(b.low)) path += 2 * std::fabs(b.high - b.low);
    if (i > 0 && std::isfinite(b.open) && std::isfinite(bars[i - 1].close)) path += std::fabs(b.open - bars[i - 1].close);
  }
  if (!std::isfinite(path)) return 0;
  if (path / box > kBrickBudget) box = path / kBrickBudget;
  // a box a million times below the largest price is still well above its resolution (2^-52 ≈ 2e-16)
  if (!(maxAbs + box * 1e6 > maxAbs) || !(maxAbs + box > maxAbs)) return 0;
  return box;
}
}  // namespace

Transformed renko(const Bars& bars, double boxSize) {
  Transformed res;
  if (bars.size() < 2) return res;
  double box = boxSize;
  if (box == 0 || std::isnan(box)) {
    double s = 0;
    const int n = std::min(14, int(bars.size()) - 1);
    for (int i = bars.size() - n; i < bars.size(); ++i) {
      const Bar &p = bars[i - 1], &b = bars[i];
      s += jsMax(jsMax(b.high - b.low, std::fabs(b.high - p.close)), std::fabs(b.low - p.close));
    }
    box = orElse(s / n, bars.last().close * 0.01);
  }
  // Port guard: a box that is not positive would loop for ever (the JS hangs); nothing to draw instead. See safeBox.
  box = safeBox(bars, box);
  if (!(box > 0)) return res;
  double base = bars[0].close, lastTime = -std::numeric_limits<double>::infinity();
  auto push = [&](qint64 t, double open, double close) {
    const double time = jsMax(double(t), lastTime + 1);
    lastTime = time;
    res.bars.push_back(brick(qint64(time), open, jsMax(open, close), jsMin(open, close), close));
  };
  int dir = 0;
  for (const Bar& b : bars) {
    while (true) {
      if (b.close >= base + box * (dir == -1 ? 2 : 1)) {
        const double open = dir == -1 ? base + box : base;
        push(b.time, open, open + box);
        base = open + box;
        dir = 1;
      } else if (b.close <= base - box * (dir == 1 ? 2 : 1)) {
        const double open = dir == 1 ? base - box : base;
        push(b.time, open, open - box);
        base = open - box;
        dir = -1;
      } else break;
    }
  }
  res.box = box;
  return res;
}

// Three-line break.
Bars lineBreak(const Bars& bars, int lines) {
  Bars out;
  double lastTime = -std::numeric_limits<double>::infinity();
  for (const Bar& b : bars) {
    if (out.isEmpty()) {
      out.push_back(brick(b.time, b.open, jsMax(b.open, b.close), jsMin(b.open, b.close), b.close));
      lastTime = double(b.time);
      continue;
    }
    // out.slice(-lines)
    const int len = out.size();
    const int k = -lines;
    const int from = k < 0 ? std::max(len + k, 0) : std::min(k, len);
    double hi = -std::numeric_limits<double>::infinity(), lo = std::numeric_limits<double>::infinity();
    for (int j = from; j < len; ++j) { hi = jsMax(hi, out[j].high); lo = jsMin(lo, out[j].low); }
    const Bar last = out.last();
    const bool up = last.close > last.open;
    bool have = false;
    double bo = 0, bc = 0;
    if (b.close > (up ? last.close : hi)) { have = true; bo = up ? last.close : last.open; bc = b.close; }
    else if (b.close < (up ? lo : last.close)) { have = true; bo = up ? last.open : last.close; bc = b.close; }
    if (have) {
      const double time = jsMax(double(b.time), lastTime + 1);
      lastTime = time;
      out.push_back(brick(qint64(time), bo, jsMax(bo, bc), jsMin(bo, bc), bc));
    }
  }
  return out;
}

QString toCsv(const Bars& bars, const CsvColumns& extraCols) {
  QStringList head = {"time", "iso", "open", "high", "low", "close", "volume"};
  for (const auto& c : extraCols) head << c.first;
  QStringList lines;
  lines.reserve(bars.size() + 1);
  lines << head.join(QLatin1Char(','));
  for (int i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    QStringList row = {numStr(double(b.time)), isoString(b.time), numStr(b.open), numStr(b.high), numStr(b.low),
                       numStr(b.close), std::isnan(b.volume) ? QString() : numStr(b.volume)};
    for (const auto& c : extraCols) {
      const double v = i < c.second.size() ? c.second[i] : NaN;
      row << (std::isnan(v) ? QString() : numStr(v));
    }
    lines << row.join(QLatin1Char(','));
  }
  return lines.join(QLatin1Char('\n'));
}

// Average true range of the last 14 bars, the default box/range size for the price-only charts.
double recentAtr(const Bars& bars, int n) {
  const int k = std::min(n, int(bars.size()) - 1);
  if (k < 1) return bars.isEmpty() ? 1 : bars[0].close * 0.01;
  double s = 0;
  for (int i = bars.size() - k; i < bars.size(); ++i) {
    const Bar &p = bars[i - 1], &b = bars[i];
    s += jsMax(jsMax(b.high - b.low, std::fabs(b.high - p.close)), std::fabs(b.low - p.close));
  }
  return orElse(s / k, bars.last().close * 0.01);
}

// Range bars: a new bar starts each time price has covered `range` (default: ATR 14). Each source
// bar is walked open -> low -> high -> close (or open -> high -> low -> close on down bars).
Transformed rangeBars(const Bars& bars, double range) {
  Transformed res;
  if (bars.isEmpty()) return res;
  // Port guard: with a range that is not positive the JS loops for ever; nothing to draw instead. See safeBox.
  const double R = safeBox(bars, orElse(range, recentAtr(bars)));
  if (!(R > 0)) return res;
  UniqueTimes stamp;
  Bar cur;
  bool have = false;
  auto emitBar = [&] { Bar e = cur; e.time = stamp(cur.time); res.bars.push_back(e); };
  auto fresh = [&](qint64 time, double p) { cur = Bar{time, p, p, p, p, 0}; };
  auto feed = [&](double price, qint64 time) {
    if (!have) { fresh(time, price); have = true; return; }
    for (;;) {
      if (price > cur.high && price - cur.low >= R) {
        const double top = cur.low + R;
        cur.high = top; cur.close = top; emitBar();
        fresh(time, top);
      } else if (price < cur.low && cur.high - price >= R) {
        const double bot = cur.high - R;
        cur.low = bot; cur.close = bot; emitBar();
        fresh(time, bot);
      } else {
        cur.high = jsMax(cur.high, price); cur.low = jsMin(cur.low, price); cur.close = price;
        return;
      }
    }
  };
  for (const Bar& b : bars) {
    const double path[4] = {b.open, b.close >= b.open ? b.low : b.high, b.close >= b.open ? b.high : b.low, b.close};
    for (double p : path) feed(p, b.time);
    cur.volume += orElse(b.volume, 0);
  }
  emitBar();
  res.range = R;
  return res;
}

// Kagi: turning points of the close, reversing after `reversal` (default ATR 14). Yang (green)
// once the line breaks the previous high, yin (red) once it breaks the previous low.
Transformed kagi(const Bars& bars, double reversal) {
  Transformed res;
  if (bars.size() < 2) return res;
  const double R = orElse(reversal, recentAtr(bars));
  UniqueTimes stamp;
  struct Pt { qint64 time; double value; bool yang; };
  QVector<Pt> pts;
  int dir = 0;
  double extreme = bars[0].close;
  qint64 extremeTime = bars[0].time;
  bool yang = true;
  double lastHigh = std::numeric_limits<double>::infinity(), lastLow = -std::numeric_limits<double>::infinity();
  auto push = [&](qint64 time, double v) { pts.push_back({stamp(time), v, yang}); };
  push(bars[0].time, bars[0].close);
  for (int i = 1; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    const double c = b.close;
    if (dir >= 0 && c > extreme) { extreme = c; extremeTime = b.time; if (dir == 0) dir = 1; if (c > lastHigh) yang = true; continue; }
    if (dir <= 0 && c < extreme) { extreme = c; extremeTime = b.time; if (dir == 0) dir = -1; if (c < lastLow) yang = false; continue; }
    if (dir == 1 && extreme - c >= R) { push(extremeTime, extreme); lastHigh = extreme; dir = -1; extreme = c; extremeTime = b.time; if (c < lastLow) yang = false; }
    else if (dir == -1 && c - extreme >= R) { push(extremeTime, extreme); lastLow = extreme; dir = 1; extreme = c; extremeTime = b.time; if (c > lastHigh) yang = true; }
  }
  push(extremeTime, extreme);
  for (const Pt& p : pts) {
    res.bars.push_back(brick(p.time, p.value, p.value, p.value, p.value));
    res.colors << (p.yang ? QStringLiteral("#089981") : QStringLiteral("#f23645"));
  }
  res.reversal = R;
  return res;
}

// Point & figure from closes: X columns up, O columns down, a box of `box` (default ATR 14) and a
// `reversal`-box turn. Each column comes back as one candle spanning the column.
Transformed pointFigure(const Bars& bars, double box, double reversal) {
  Transformed res;
  if (bars.size() < 2) return res;
  const double B = orElse(box, recentAtr(bars));
  UniqueTimes stamp;
  auto fl = [B](double v) { return std::floor(v / B + 1e-9) * B; };
  auto ce = [B](double v) { return std::ceil(v / B - 1e-9) * B; };
  struct Col { int dir; double lo, hi; qint64 time; };
  QVector<Col> cols;
  const double base = fl(bars[0].close);
  Col col{};
  bool have = false;
  for (const Bar& b : bars) {
    const double c = b.close;
    if (!have) {
      if (c >= base + B) { col = {1, base, fl(c), b.time}; have = true; }
      else if (c <= base - B) { col = {-1, ce(c), base, b.time}; have = true; }
      continue;
    }
    if (col.dir == 1) {
      if (c >= col.hi + B) col.hi = fl(c);
      else if (c <= col.hi - reversal * B) { cols.push_back(col); col = {-1, ce(c), col.hi - B, b.time}; }
    } else if (c <= col.lo - B) col.lo = ce(c);
    else if (c >= col.lo + reversal * B) { cols.push_back(col); col = {1, col.lo + B, fl(c), b.time}; }
  }
  if (have) cols.push_back(col);
  for (const Col& k : cols) {
    res.bars.push_back(brick(stamp(k.time), k.dir == 1 ? k.lo : k.hi, k.hi, k.lo, k.dir == 1 ? k.hi : k.lo));
    // clamped before the int conversion (a tiny box makes the count huge: undefined behaviour)
    res.boxes << int(std::clamp(jsRound((k.hi - k.lo) / B) + 1, -1e9, 1e9));
    res.dirs << k.dir;
  }
  res.box = B;
  return res;
}

}  // namespace charts::series
