#include "pages/charts/plot/TimeScale.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Format.h"
#include <QTimer>
#include <algorithm>

namespace charts::plot {

// ---------- data ----------

void TimeScale::setPoints(QVector<Time> times) {
  const bool wasEmpty = times_.isEmpty();
  const Time oldLast = wasEmpty ? 0 : times_.last();
  // was the last bar on screen? (then a new bar keeps the view glued to the right edge)
  const bool lastVisible = !wasEmpty && rightOffset_ >= -0.5 && (width_ <= 0 || rightOffset_ <= width_ / spacing_);
  // a live chart only appends bars: when the old points are a prefix of the new ones, their weights stand (each one
  // depends on its point and the one before) and only the tail is computed (timeWeight converts dates: not free)
  const bool extends = !wasEmpty && times.size() >= times_.size() && std::equal(times_.begin(), times_.end(), times.begin());
  const int keep = extends && weights_.size() == times_.size() ? int(times_.size()) : 0;
  times_ = std::move(times);
  ++version_;
  weights_.resize(times_.size());
  for (int i = std::max(1, keep); i < times_.size(); ++i) weights_[i] = quint8(fmt::timeWeight(times_[i], times_[i - 1]));
  if (!times_.isEmpty()) {
    // first point: compared with an imaginary point one average step before it
    const qint64 step = times_.size() > 1 ? (times_.last() - times_.first() + times_.size() - 2) / (times_.size() - 1) : 86400;
    weights_[0] = quint8(fmt::timeWeight(times_[0], times_[0] - qMax<qint64>(1, step)));
  }
  if (times_.isEmpty()) return changed();
  if (wasEmpty) {
    spacing_ = o_.barSpacing;
    rightOffset_ = o_.rightOffset;
  } else {
    const int added = int(times_.end() - std::upper_bound(times_.begin(), times_.end(), oldLast));
    if (added > 0 && added < times_.size() && !(o_.shiftVisibleRangeOnNewBar && lastVisible)) rightOffset_ -= added;
  }
  if (pendingRange_ && width_ > 0) applyRange(*pendingRange_);
  clampOffsets();
  changed();
}

void TimeScale::setWidth(double w, int h) {
  if (w == width_ && h == height_) return;
  const bool sizeChanged = int(w) != int(width_) || h != height_;
  const auto before = getVisibleLogicalRange();
  const double oldW = width_;
  width_ = w;
  height_ = h;
  if (pendingRange_ && width_ > 0 && !times_.isEmpty()) {
    applyRange(*pendingRange_);
    if (chart_->isVisible()) pendingRange_.reset();
  } else if (o_.lockVisibleTimeRangeOnResize && before && oldW > 0) {
    spacing_ = width_ / qMax(1e-9, before->to - before->from + 1);
  }
  clampOffsets();
  if (sizeChanged && !sizeSubs_.isEmpty())
    QTimer::singleShot(0, chart_, [this] { for (const auto& f : sizeSubs_) f(int(width_), height_); });
}

void TimeScale::applyOptions(const TimeScaleOptions& o) {
  const bool spacingChanged = o.barSpacing != o_.barSpacing, offsetChanged = o.rightOffset != o_.rightOffset;
  o_ = o;
  chart_->o_.timeScale = o_;
  if (spacingChanged) spacing_ = o_.barSpacing;
  if (offsetChanged) rightOffset_ = o_.rightOffset;
  ++version_;   // labels may change (timeVisible, formatter)
  clampOffsets();
  changed();
}

// ---------- geometry ----------

void TimeScale::clampOffsets() {
  const double maxSp = o_.minVisibleBars > 0 && width_ > 0 ? width_ / o_.minVisibleBars
                      : o_.maxBarSpacing > 0 ? o_.maxBarSpacing : (width_ > 0 ? 0.5 * width_ : 1e9);
  const double minSp = (o_.fixLeftEdge && o_.fixRightEdge && !times_.isEmpty() && width_ > 0) ? width_ / times_.size()
                                                                                                : o_.minBarSpacing;
  spacing_ = qBound(minSp, spacing_, qMax(minSp, maxSp));
  if (times_.isEmpty() || width_ <= 0) return;
  const int n = int(times_.size());
  const double lo = -baseIndex() - 1 + (o_.fixLeftEdge ? width_ / spacing_ : qMin(2, n));
  const double hi = o_.fixRightEdge ? 0 : width_ / spacing_ - qMin(2, n);
  if (rightOffset_ < lo) rightOffset_ = lo;
  if (rightOffset_ > hi) rightOffset_ = hi;
}

void TimeScale::setBarSpacing(double s) {
  pendingRange_.reset();
  spacing_ = s;
  clampOffsets();
  changed();
}

void TimeScale::setRightOffset(double r) {
  pendingRange_.reset();
  rightOffset_ = r;
  clampOffsets();
  changed();
}

std::optional<LogicalRange> TimeScale::getVisibleLogicalRange() const {
  chart_->ensureLayout();
  if (times_.isEmpty() || width_ <= 0) {
    if (pendingRange_) return pendingRange_;
    return std::nullopt;
  }
  const double right = baseIndex() + rightOffset_;
  return LogicalRange{right - width_ / spacing_ + 1, right};
}

void TimeScale::applyRange(const LogicalRange& r) {
  spacing_ = width_ / qMax(1e-9, r.to - r.from + 1);
  rightOffset_ = r.to - baseIndex();
  clampOffsets();
}

// Before the chart is first shown its width is not final (a widget gets its real size when its layout shows it), so
// the requested range is also kept and applied again with the final width; afterwards a resize keeps the bar
// spacing and the right edge, as in the JS library.
void TimeScale::setVisibleLogicalRange(const LogicalRange& r) {
  chart_->ensureLayout();
  if (!chart_->isVisible() || width_ <= 0 || times_.isEmpty()) pendingRange_ = r;
  else pendingRange_.reset();
  if (width_ > 0 && !times_.isEmpty()) applyRange(r);
  changed();
}

bool TimeScale::visibleIndexes(int& from, int& to) const {
  const auto r = getVisibleLogicalRange();
  if (!r || times_.isEmpty()) return false;
  from = qMax(0, int(std::floor(r->from)));
  to = qMin(baseIndex(), int(std::ceil(r->to)));
  return from <= to;
}

std::optional<TimeRange> TimeScale::getVisibleRange() const {
  int a, b;
  if (!visibleIndexes(a, b)) return std::nullopt;
  return TimeRange{times_[a], times_[b]};
}

void TimeScale::setVisibleRange(const TimeRange& range) {
  const auto a = timeToIndex(range.from, true), b = timeToIndex(range.to, true);
  if (!a || !b) return;
  setVisibleLogicalRange({double(*a), double(*b)});
}

void TimeScale::resetTimeScale() {
  pendingRange_.reset();
  spacing_ = o_.barSpacing;
  rightOffset_ = o_.rightOffset;
  clampOffsets();
  changed();
}

void TimeScale::fitContent() {
  chart_->ensureLayout();
  if (times_.isEmpty()) return;
  setVisibleLogicalRange({0, baseIndex() + o_.rightOffset});
}

void TimeScale::scrollToPosition(double position, bool) { setRightOffset(position); }
void TimeScale::scrollToRealTime() { setRightOffset(o_.rightOffset); }

std::optional<double> TimeScale::logicalToCoordinate(double logical) const {
  if (times_.isEmpty()) return std::nullopt;
  chart_->ensureLayout();
  return indexToX(logical);
}

std::optional<double> TimeScale::coordinateToLogical(double x) const {
  if (times_.isEmpty()) return std::nullopt;
  chart_->ensureLayout();
  return std::ceil(std::round(xToFloatIndex(x) * 1e6) / 1e6);
}

double TimeScale::coordinateToLogicalExact(double x) const {
  chart_->ensureLayout();
  return xToFloatIndex(x) - 0.5;
}

std::optional<int> TimeScale::timeToIndex(Time t, bool findNearest) const {
  chart_->ensureLayout();
  if (times_.isEmpty()) return std::nullopt;
  const auto it = std::lower_bound(times_.begin(), times_.end(), t);
  if (it != times_.end() && *it == t) return int(it - times_.begin());
  if (!findNearest) return std::nullopt;
  if (it == times_.end()) return baseIndex();
  return int(it - times_.begin());   // like the JS library: the next point on the right
}

std::optional<double> TimeScale::timeToCoordinate(Time t) const {
  const auto i = timeToIndex(t, false);
  if (!i) return std::nullopt;
  return logicalToCoordinate(*i);
}

std::optional<Time> TimeScale::coordinateToTime(double x) const {
  const auto l = coordinateToLogical(x);
  if (!l) return std::nullopt;
  const int i = int(*l);
  if (i < 0 || i > baseIndex()) return std::nullopt;
  return times_[i];
}

// ---------- interaction ----------

void TimeScale::zoom(double x, double scale) {
  pendingRange_.reset();
  if (times_.isEmpty() || scale == 0) return;
  x = qBound(1.0, x, qMax(1.0, width_));
  const double before = xToFloatIndex(x);
  spacing_ = spacing_ + scale * (spacing_ / 10);
  clampOffsets();
  if (!o_.rightBarStaysOnScroll) rightOffset_ += before - xToFloatIndex(x);
  clampOffsets();
  changed();
}

void TimeScale::scrollChart(double pixels) {
  pendingRange_.reset();
  if (times_.isEmpty()) return;
  rightOffset_ -= pixels / spacing_;
  clampOffsets();
  changed();
}

void TimeScale::scrollTo(double x) {
  pendingRange_.reset();
  if (std::isnan(scrollStart_) || times_.isEmpty()) return;
  rightOffset_ = scrollStartOffset_ + (scrollStart_ - x) / spacing_;
  clampOffsets();
  changed();
}

void TimeScale::scaleTo(double x) {
  pendingRange_.reset();
  if (std::isnan(scaleStart_) || times_.isEmpty()) return;
  const double now = qBound(0.0, width_ - x, width_), start = qBound(0.0, width_ - scaleStart_, width_);
  if (now == 0 || start == 0) return;
  spacing_ = scaleStartSpacing_ * now / start;
  clampOffsets();
  changed();
}

// ---------- labels ----------

int TimeScale::tickMarkType(int w) const {
  if (w >= fmt::WYear) return int(TickMarkType::Year);
  if (w >= fmt::WMonth) return int(TickMarkType::Month);
  if (w >= fmt::WDay) return int(TickMarkType::DayOfMonth);
  if (!o_.timeVisible) return int(TickMarkType::DayOfMonth);
  if (w <= fmt::WSecond && o_.secondsVisible) return int(TickMarkType::TimeWithSeconds);
  return int(TickMarkType::Time);
}

QString TimeScale::tickLabel(Time t, int weight) const {
  const auto type = TickMarkType(tickMarkType(weight));
  if (o_.tickMarkFormatter) {
    const QString s = o_.tickMarkFormatter(t, type);
    if (!s.isNull()) return s;
  }
  switch (type) {
    case TickMarkType::Year: return fmt::date(t, "yyyy");
    case TickMarkType::Month: return fmt::date(t, "MMM");
    case TickMarkType::DayOfMonth: return fmt::date(t, "d");
    case TickMarkType::Time: return fmt::clock(t, false);
    case TickMarkType::TimeWithSeconds: return fmt::clock(t, true);
  }
  return {};
}

const QVector<TimeScale::Mark>& TimeScale::marks(const QFont&, int fontSize, int& maxWeight) {
  marks_.clear();
  maxWeight = 0;
  if (times_.isEmpty() || width_ <= 0) return marks_;
  const double maxLabelWidth = 5.0 * (fontSize + 4);   // room for ~8 characters
  const int step = qMax(1, int(std::ceil(maxLabelWidth / spacing_)));
  if (step != selectedStep_ || version_ != selectedVersion_) {
    selectedStep_ = step;
    selectedVersion_ = version_;
    // Coarse boundaries first (years, months, days, hours ...); a finer point joins the selection only if it is at
    // least `step` indexes away from its selected neighbours on both sides. Stable while scrolling.
    QVector<QVector<int>> byWeight(fmt::WYear + 1);
    for (int i = 0; i < times_.size(); ++i) byWeight[weights_[i]].append(i);
    QVector<int> sel, merged;
    for (int w = fmt::WYear; w >= 0; --w) {
      const QVector<int>& cand = byWeight[w];
      if (cand.isEmpty()) continue;
      merged.clear();
      merged.reserve(sel.size() + cand.size());
      int a = 0, last = std::numeric_limits<int>::min() / 2;
      for (int c : cand) {
        while (a < sel.size() && sel[a] < c) { last = sel[a]; merged.append(sel[a++]); }
        const int next = a < sel.size() ? sel[a] : std::numeric_limits<int>::max() / 2;
        if (next - c >= step && c - last >= step) { merged.append(c); last = c; }
      }
      while (a < sel.size()) merged.append(sel[a++]);
      sel.swap(merged);
    }
    selected_ = sel;
  }
  const auto r = getVisibleLogicalRange();
  const int extra = int(std::lround(maxLabelWidth / spacing_));
  const double from = r->from - extra, to = r->to + extra;
  auto it = std::lower_bound(selected_.begin(), selected_.end(), int(std::floor(from)));
  for (; it != selected_.end() && *it <= to; ++it) {
    const int i = *it;
    marks_.append({indexToX(i), tickLabel(times_[i], weights_[i]), weights_[i]});
    maxWeight = qMax(maxWeight, int(weights_[i]));
  }
  if (maxWeight > fmt::WHour1 && maxWeight < fmt::WDay) maxWeight = fmt::WHour1;
  return marks_;
}

QString TimeScale::formatCrosshairTime(Time t) const {
  const LocalizationOptions& loc = chart_->options().localization;
  if (loc.timeFormatter) return loc.timeFormatter(t);
  QString s = fmt::date(t, loc.dateFormat);
  if (o_.timeVisible) s += "   " + fmt::clock(t, o_.secondsVisible);
  return s;
}

// ---------- notifications ----------

void TimeScale::changed() {
  chart_->viewChanged();
}

void TimeScale::notify() {
  const auto r = getVisibleLogicalRange();
  const bool same = (!r && !lastNotified_) ||
                    (r && lastNotified_ && r->from == lastNotified_->from && r->to == lastNotified_->to);
  if (same) return;
  lastNotified_ = r;
  for (const auto& f : logicalSubs_) f(r);
  if (!timeSubs_.isEmpty()) {
    const auto tr = getVisibleRange();
    for (const auto& f : timeSubs_) f(tr);
  }
}

int TimeScale::subscribeVisibleLogicalRangeChange(std::function<void(std::optional<LogicalRange>)> fn) {
  logicalSubs_.insert(nextSub_, std::move(fn));
  return nextSub_++;
}
int TimeScale::subscribeVisibleTimeRangeChange(std::function<void(std::optional<TimeRange>)> fn) {
  timeSubs_.insert(nextSub_, std::move(fn));
  return nextSub_++;
}
int TimeScale::subscribeSizeChange(std::function<void(int, int)> fn) {
  sizeSubs_.insert(nextSub_, std::move(fn));
  return nextSub_++;
}

}  // namespace charts::plot
