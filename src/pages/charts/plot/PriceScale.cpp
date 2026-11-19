#include "pages/charts/plot/PriceScale.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Format.h"
#include <algorithm>

namespace charts::plot {

void PriceScale::applyOptions(const PriceScaleOptions& o) {
  const PriceScaleOptions old = o_;
  auto apply = [&](PriceScale* sc) {
    const bool modeChanged = o.mode != sc->o_.mode;
    Chart::mergeScaleOptions(old, o, sc->o_);
    if (modeChanged) sc->o_.autoScale = true;   // a manual range in other units means nothing in the new mode
  };
  if (isOverlay()) {
    apply(this);
  } else {
    // "right" / "left": one set of options shared by that scale in every pane, as in the JS library
    // (chart.priceScale("right").applyOptions({ mode }) also switches the indicator panes)
    for (Pane* p : chart_->panes()) apply(p->priceScale(id_));
  }
  chart_->invalidate();
}

int PriceScale::width() const {
  if (isOverlay() || !o_.visible) return 0;
  chart_->ensureLayout();
  return chart_->priceAxisWidth();
}

void PriceScale::setAutoScale(bool on) {
  o_.autoScale = on;
  chart_->invalidate();
}

void PriceScale::setVisibleRange(const PriceRange& r) {
  chart_->ensureLayout();
  const double base = formatSource_ ? formatSource_->firstValue() : kNaN;
  min_ = toInternal(r.minValue, base);
  max_ = toInternal(r.maxValue, base);
  if (min_ > max_) std::swap(min_, max_);
  clampLogRange();
  hasRange_ = true;
  o_.autoScale = false;
  chart_->invalidate();
}

std::optional<PriceRange> PriceScale::getVisibleRange() const {
  chart_->ensureLayout();
  if (!hasRange_ || height_ <= 0) return std::nullopt;
  const double base = formatSource_ ? formatSource_->firstValue() : kNaN;
  double a = fromInternal(yToInternal(height_ - 1), base), b = fromInternal(yToInternal(0), base);
  if (a > b) std::swap(a, b);
  return PriceRange{a, b};
}

// ---------- conversions ----------

static double safeLog(double v) { return std::log10(qMax(v, 1e-12)); }

double PriceScale::toInternal(double price, double base) const {
  switch (mode()) {
    case PriceScaleMode::Normal: return price;
    case PriceScaleMode::Logarithmic: return safeLog(price);
    case PriceScaleMode::Percentage:
      return (std::isfinite(base) && base != 0) ? (price - base) / std::fabs(base) * 100 : 0;
    case PriceScaleMode::IndexedTo100:
      return (std::isfinite(base) && base != 0) ? (price - base) / std::fabs(base) * 100 + 100 : 100;
  }
  return price;
}

double PriceScale::fromInternal(double u, double base) const {
  switch (mode()) {
    case PriceScaleMode::Normal: return u;
    case PriceScaleMode::Logarithmic: return std::pow(10.0, u);
    case PriceScaleMode::Percentage: return (std::isfinite(base) && base != 0) ? base + u / 100 * std::fabs(base) : u;
    case PriceScaleMode::IndexedTo100:
      return (std::isfinite(base) && base != 0) ? base + (u - 100) / 100 * std::fabs(base) : u;
  }
  return u;
}

double PriceScale::internalToY(double u) const {
  const double top = height_ * o_.scaleMargins.top, bottom = height_ * o_.scaleMargins.bottom;
  const double inner = qMax(1.0, height_ - 1 - top - bottom);
  const double span = max_ - min_;
  const double t = span > 0 ? (u - min_) / span : 0.5;
  return o_.invertScale ? top + t * inner : top + (1 - t) * inner;
}

double PriceScale::yToInternal(double y) const {
  const double top = height_ * o_.scaleMargins.top, bottom = height_ * o_.scaleMargins.bottom;
  const double inner = qMax(1.0, height_ - 1 - top - bottom);
  const double t = o_.invertScale ? (y - top) / inner : 1 - (y - top) / inner;
  return min_ + t * (max_ - min_);
}

std::optional<double> PriceScale::priceToY(double price, double base) const {
  if (!hasRange_ || height_ <= 0 || !std::isfinite(price)) return std::nullopt;
  return internalToY(toInternal(price, base));
}

std::optional<double> PriceScale::yToPrice(double y, double base) const {
  if (!hasRange_ || height_ <= 0) return std::nullopt;
  return fromInternal(yToInternal(y), base);
}

QString PriceScale::formatInternal(double u) const {
  switch (mode()) {
    case PriceScaleMode::Percentage: return fmt::percent(u);
    case PriceScaleMode::IndexedTo100: return fmt::fixed(u, 2);
    case PriceScaleMode::Logarithmic: return chart_->formatAxisPrice(nullptr, formatSource_, std::pow(10.0, u));
    case PriceScaleMode::Normal: break;
  }
  return chart_->formatAxisPrice(nullptr, formatSource_, u);
}

// ---------- ticks ----------

// Tick step for a range: start from the power of ten above the range and divide by 2, 2.5 or 2 in turn (three
// rotations of that cycle, keep the smallest result) while the step stays >= minSpan, never below the smallest
// price increment (1 / base). Gives steps of 1, 2, 2.5, 4 or 5 x 10^k, as the JS library's axis does.
static double tickSpan(double high, double low, double minSpan, double base) {
  // an infinite bound kept `e /= h` at inf: the loops below never ended (a log scale zoomed out past 10^308)
  if (!std::isfinite(high) || !std::isfinite(low) || !std::isfinite(minSpan)) return 1;
  const double eps = 1e-14;
  const double smallest = base > 0 ? 1 / base : 0;
  // fractional dividers: base a power of ten → the same cycle; otherwise its factors (2 → 2, 5 → 2 then 2.5)
  QVector<double> frac;
  {
    bool pow10 = base >= 1;
    for (double t = base; t > 1; t /= 10) if (std::fmod(t, 10) != 0) { pow10 = false; break; }
    if (pow10) frac = {2, 2.5, 2};
    else
      for (double t = std::round(base); t > 1 && frac.size() < 100;) {
        if (std::fmod(t, 2) == 0) { frac << 2; t /= 2; }
        else if (std::fmod(t, 5) == 0) { frac << 2 << 2.5; t /= 5; }
        else break;
      }
  }
  auto one = [&](const QVector<double>& div) {
    double e = std::pow(10.0, qMax(0.0, std::ceil(std::log10(qMax(high - low, 1e-300)))));
    int r = 0;
    double h = div[0];
    while (e >= smallest - eps && e > smallest + eps && e >= minSpan * h - eps && e >= 1 - eps) {
      e /= h;
      h = div[++r % div.size()];
    }
    if (e <= smallest + eps) e = smallest;
    e = qMax(1.0, e);
    if (!frac.isEmpty() && std::fabs(e - 1) < eps) {
      r = 0;
      h = frac[0];
      while (e >= minSpan * h - eps && e > smallest + eps) {
        e /= h;
        h = frac[++r % frac.size()];
      }
    }
    return e;
  };
  return qMin(one({2, 2.5, 2}), qMin(one({2, 2, 2.5}), one({2.5, 2, 2})));
}

void PriceScale::buildTicks(int fontSize) {
  ticks_.clear();
  if (!hasRange_ || height_ <= 2 || !(max_ > min_)) return;
  const double minSpacing = std::ceil(fontSize * o_.tickMarkDensity);
  const double H = height_;
  // base = 1 / smallest price increment of the scale's formatter
  double base = 100;
  if (formatSource_ && !isRelative()) {
    const PriceFormat& pf = formatSource_->options().priceFormat;
    const double mm = pf.type == PriceFormat::Price && pf.minMove > 0 ? pf.minMove : std::pow(10.0, -pf.precision);
    base = std::round(1 / mm);
    if (pf.type == PriceFormat::Volume) base = 1;
  }
  const bool log = isLog();
  // the axis works in prices for Normal and Logarithmic, in percent / indexed values for the relative modes
  auto valueAt = [&](double y) { const double u = yToInternal(y); return log ? std::pow(10.0, u) : u; };
  auto yOf = [&](double v) { return internalToY(log ? safeLog(v) : v); };
  const double a = valueAt(H - 1), b = valueAt(0);
  const double high = qMax(a, b), low = qMin(a, b);
  if (!(high > low) || !std::isfinite(high) || !std::isfinite(low)) return;
  auto spanFor = [&](double hi, double lo) { return tickSpan(hi, lo, (hi - lo) * minSpacing / H, base); };
  double step = spanFor(high, low);
  double rem = std::fmod(high, step);
  if (rem < 0) rem += step;
  const double top = o_.entireTextOnly ? fontSize / 2.0 : 0, bottom = H - 1 - top;
  double lastY = kNaN;
  // iterations counted, not ticks: both `continue`s below skip the append, and a step lost in the value's resolution
  // would never reach `low`
  int guard = 0;
  for (double c = high - rem; c > low && ticks_.size() < 500 && ++guard < 5000; c -= step) {
    const double y = yOf(c);
    if (!std::isnan(lastY) && std::fabs(y - lastY) < minSpacing) continue;
    if (y < top || y > bottom) continue;
    const double v = std::fabs(c) < step * 1e-9 ? 0 : c;
    ticks_.append({y, isRelative() ? formatInternal(v) : chart_->formatAxisPrice(nullptr, formatSource_, v)});
    lastY = y;
    if (log) step = spanFor(c, low);   // the price density changes along a log axis
  }
}

// ---------- dragging ----------

void PriceScale::startScale(double y) {
  if (!hasRange_) return;
  scaleStart_ = height_ - 1 - y;
  startMin_ = min_;
  startMax_ = max_;
  o_.autoScale = false;
}

// A log scale holds log10 values: beyond about 10^18 (or below 10^-15) prices are meaningless, and past 10^308 they
// are infinite.
void PriceScale::clampLogRange() {
  if (!isLog()) return;
  min_ = std::clamp(min_, -15.0, 18.0);
  max_ = std::clamp(max_, -15.0, 18.0);
}

void PriceScale::scaleTo(double y) {
  if (std::isnan(scaleStart_)) return;
  // the pointer below the pane counts as its bottom edge: further down, x + k0 neared 0 and the scale jumped ×1000
  const double x = qMax(0.0, height_ - 1 - y);
  const double k0 = (height_ - 1) * 0.2;
  double coeff = (scaleStart_ + k0) / (x + k0);
  if (!(coeff > 0) || !std::isfinite(coeff)) coeff = 1;
  coeff = qBound(1e-3, coeff, 1e3);
  const double center = (startMin_ + startMax_) / 2, half = (startMax_ - startMin_) / 2 * coeff;
  min_ = center - half;
  max_ = center + half;
  clampLogRange();
  chart_->invalidate();
}

void PriceScale::startScroll(double y) {
  if (!hasRange_ || o_.autoScale || isRelative()) return;
  scrollStart_ = y;
  startMin_ = min_;
  startMax_ = max_;
}

void PriceScale::scrollTo(double y) {
  if (std::isnan(scrollStart_) || o_.autoScale) return;
  const double top = height_ * o_.scaleMargins.top, bottom = height_ * o_.scaleMargins.bottom;
  const double inner = qMax(1.0, height_ - 1 - top - bottom);
  double d = (y - scrollStart_) / inner * (startMax_ - startMin_);
  if (o_.invertScale) d = -d;
  min_ = startMin_ + d;
  max_ = startMax_ + d;
  clampLogRange();
  chart_->invalidate();
}

}  // namespace charts::plot
