#pragma once
// Order flow (footprint, Big Trades, heatmaps): prices as whole numbers of ticks, never as floating-point keys.
//
// Exchanges write prices as decimal text ("63951.20"). parseDecimal keeps them exact (mantissa × 10^exp), and
// TickScale::ticksOf turns one into its tick count with integer arithmetic only, so two trades at the same price always
// land on the same level. Prices computed by the app (a zoom level, a grid row) use the double path, rounded to the
// nearest tick. Pure, header-only, no Qt.
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#if defined(_MSC_VER) && !defined(__clang__)
#include <__msvc_int128.hpp>   // MSVC has no __int128; its standard library's 128-bit integer does the same sums
#endif

namespace charts::flow {

#if defined(_MSC_VER) && !defined(__clang__)
using Int128 = std::_Signed128;
#else
using Int128 = __int128;
#endif

// value = mant × 10^exp, with exp <= 0 and trailing fraction zeros dropped ("63981.93000000" → 6398193, -2).
struct Decimal {
  int64_t mant = 0;
  int exp = 0;
  bool ok = false;

  // Correctly rounded (the same double strtod gives) while |mant| < 2^53 and exp >= -22: both operands are exact
  // doubles and IEEE division rounds once. Outside that range (never seen in Binance prices or sizes): pow.
  double toDouble() const {
    static constexpr double kPow10[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                        1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    const int64_t lim = int64_t(1) << 53;
    if (exp <= 0 && exp >= -22 && mant < lim && mant > -lim) return double(mant) / kPow10[-exp];
    return double(mant) * std::pow(10.0, exp);
  }
};

// Parses [p, end) from its first character: optional '-', digits, optional '.' and digits. No exponent, no spaces
// (exchanges never send them). *stop (if given) = the first character not taken. ok = false for no digits or more
// significant digits than an int64 holds.
template <class C>
Decimal parseDecimal(const C* p, const C* end, const C** stop = nullptr) {
  Decimal d;
  bool neg = false, any = false, frac = false;
  int pendingZeros = 0;          // fraction zeros not yet known to be significant
  if (p < end && *p == C('-')) { neg = true; ++p; }
  constexpr int64_t kMax = std::numeric_limits<int64_t>::max() / 10 - 9;
  constexpr int kMinExp = -30;
  for (; p < end; ++p) {
    const C c = *p;
    if (c == C('.') && !frac) { frac = true; continue; }
    if (c < C('0') || c > C('9')) break;
    any = true;
    const int digit = int(c - C('0'));
    if (frac && digit == 0) { ++pendingZeros; continue; }
    for (; pendingZeros > 0; --pendingZeros) {
      if (d.mant > kMax || d.exp <= kMinExp) { if (stop) *stop = p; return Decimal{}; }
      d.mant *= 10;
      --d.exp;
    }
    // more than 30 fraction digits: no exchange sends that; refused so the exponent stays bounded (ticksOf scales by
    // powers of ten of the exponent gap)
    if (d.mant > kMax || (frac && d.exp <= kMinExp)) { if (stop) *stop = p; return Decimal{}; }
    d.mant = d.mant * 10 + digit;
    if (frac) --d.exp;
  }
  if (stop) *stop = p;
  if (!any) return Decimal{};
  // Integer zeros ("100") are kept in the mantissa; only fraction zeros after the last non-zero digit are dropped.
  if (neg) d.mant = -d.mant;
  d.ok = true;
  return d;
}

// A price grid: one tick = `tick` (the exchange's PRICE_FILTER tickSize).
struct TickScale {
  Decimal tick;                  // exact
  double step = 0;               // tick.toDouble()

  TickScale() = default;
  explicit TickScale(const Decimal& t) : tick(t), step(t.toDouble()) {}
  bool valid() const { return tick.ok && tick.mant > 0; }

  // Exact tick count of an exchange price: price / tick with integers (__int128, no overflow for any real price).
  // A price between two ticks (an exchange that changed its tick size) goes to the nearest one, halves away from 0.
  // Nothing when it can't be counted: an invalid grid or price, an exponent gap over 18 digits (the __int128 product
  // would overflow; mantissas hold up to 19 digits) or a count beyond ±2^62 (the book and the heatmap add and
  // subtract counts in int64).
  std::optional<int64_t> tryTicksOf(const Decimal& price) const {
    if (!price.ok || !valid()) return std::nullopt;
    const int e = price.exp - tick.exp;          // price = pm·10^pe, tick = tm·10^te → pm·10^(pe-te) / tm
    if (e > 18 || e < -18) return std::nullopt;
    Int128 num = price.mant, den = tick.mant;
    for (int k = 0; k < e; ++k) num *= 10;
    for (int k = 0; k < -e; ++k) den *= 10;
    Int128 q = num / den, r = num % den;
    if (r != 0) {
      const Int128 twice = (r < 0 ? -r : r) * 2;
      if (twice >= den) q += num < 0 ? -1 : 1;
    }
    const Int128 kLim = Int128(1) << 62;
    if (q > kLim || q < -kLim) return std::nullopt;
    return int64_t(q);
  }
  // The same, 0 when it can't be counted (tests and callers that checked the price already).
  int64_t ticksOf(const Decimal& price) const { return tryTicksOf(price).value_or(0); }
  // A computed price (zoom, grid): nearest tick. Out of range or NaN gives 0 (llround of those is undefined).
  int64_t ticksOf(double price) const {
    const double q = price / step;
    return std::fabs(q) < 4e18 ? int64_t(std::llround(q)) : 0;
  }
  // Price of a tick count. Correctly rounded while t·tm < 2^53 (BTCUSDT at 0.01: up to 90 trillion USD).
  double priceOf(int64_t t) const { return Decimal{t * tick.mant, tick.exp, true}.toDouble(); }
};

// Level of t when levels are n ticks wide: floor(t / n) · n, also for negative t (spreads can be negative).
inline int64_t groupFloor(int64_t t, int64_t n) {
  if (n <= 1) return t;
  int64_t q = t / n;
  if ((t % n != 0) && ((t < 0) != (n < 0))) --q;
  return q * n;
}

}  // namespace charts::flow
