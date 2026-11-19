#pragma once
// Horizontal scale shared by every pane (JS chart.timeScale()). The scale is the sorted union of the times of all
// series (whitespace points included); a logical index is a position in that union. Bar i is drawn at
//     x(i) = width - (baseIndex + rightOffset - i + 0.5) * barSpacing - 1
// where baseIndex is the last index — the same geometry as Lightweight Charts, so logical ranges, rightOffset and
// barSpacing mean the same thing in the JS and here.
#include "pages/charts/plot/Plot.h"
#include <QMap>

namespace charts::plot {

class TimeScale {
public:
  explicit TimeScale(Chart* chart) : chart_(chart) {}

  // ---- JS ITimeScaleApi ----
  double scrollPosition() const { return rightOffset_; }
  void scrollToPosition(double position, bool animated = false);
  void scrollToRealTime();
  std::optional<TimeRange> getVisibleRange() const;
  void setVisibleRange(const TimeRange& range);
  std::optional<LogicalRange> getVisibleLogicalRange() const;
  void setVisibleLogicalRange(const LogicalRange& range);
  void resetTimeScale();
  void fitContent();
  std::optional<double> logicalToCoordinate(double logical) const;
  std::optional<double> coordinateToLogical(double x) const;   // index of the bar slot under x (integer)
  double coordinateToLogicalExact(double x) const;             // fractional logical (extra)
  std::optional<int> timeToIndex(Time time, bool findNearest = false) const;
  std::optional<double> timeToCoordinate(Time time) const;
  std::optional<Time> coordinateToTime(double x) const;
  int width() const { return int(width_); }
  int height() const { return height_; }
  const TimeScaleOptions& options() const { return o_; }
  void applyOptions(const TimeScaleOptions& o);
  void applyOptions(const std::function<void(TimeScaleOptions&)>& f) { TimeScaleOptions o = o_; f(o); applyOptions(o); }
  int subscribeVisibleLogicalRangeChange(std::function<void(std::optional<LogicalRange>)> fn);
  void unsubscribeVisibleLogicalRangeChange(int id) { logicalSubs_.remove(id); }
  int subscribeVisibleTimeRangeChange(std::function<void(std::optional<TimeRange>)> fn);
  void unsubscribeVisibleTimeRangeChange(int id) { timeSubs_.remove(id); }
  int subscribeSizeChange(std::function<void(int width, int height)> fn);
  void unsubscribeSizeChange(int id) { sizeSubs_.remove(id); }

  // ---- engine side ----
  double barSpacing() const { return spacing_; }      // current bar spacing (options().barSpacing is the configured one)
  double rightOffset() const { return rightOffset_; }
  int baseIndex() const { return int(times_.size()) - 1; }
  bool isEmpty() const { return times_.isEmpty(); }
  const QVector<Time>& points() const { return times_; }
  double indexToX(double i) const { return width_ - (baseIndex() + rightOffset_ - i + 0.5) * spacing_ - 1; }
  double xToFloatIndex(double x) const { return baseIndex() + rightOffset_ - (width_ - 1 - x) / spacing_; }
  // visible index range rounded outwards (for drawing), clamped to [0, baseIndex]; empty when nothing is visible.
  bool visibleIndexes(int& from, int& to) const;
  void setBarSpacing(double s);
  void setRightOffset(double r);
  void zoom(double x, double scale);         // wheel: scale in [-1, 1], 10 % of bar spacing per unit
  void scrollChart(double pixels);          // like a drag of `pixels` (positive = towards older bars)
  // drag states
  void startScroll(double x) { scrollStart_ = x; scrollStartOffset_ = rightOffset_; }
  void scrollTo(double x);
  void endScroll() { scrollStart_ = kNaN; }
  void startScale(double x) { scaleStart_ = x; scaleStartSpacing_ = spacing_; }
  void scaleTo(double x);
  void endScale() { scaleStart_ = kNaN; }

  struct Mark { double x; QString label; int weight; };
  // tick marks for the current view; maxWeight = weight of the bold labels
  const QVector<Mark>& marks(const QFont& font, int fontSize, int& maxWeight);
  QString formatCrosshairTime(Time t) const;

private:
  friend class Chart;
  void setPoints(QVector<Time> times);      // called by Chart when the union of series times changes
  void setWidth(double w, int h);
  void clampOffsets();
  void applyRange(const LogicalRange& r);
  void changed();                          // view changed: notify + repaint
  void notify();
  QString tickLabel(Time t, int weight) const;
  int tickMarkType(int weight) const;

  Chart* chart_;
  TimeScaleOptions o_;
  QVector<Time> times_;
  QVector<quint8> weights_;
  double spacing_ = 6, rightOffset_ = 0, width_ = 0;
  int height_ = 0;
  double scrollStart_ = kNaN, scrollStartOffset_ = 0, scaleStart_ = kNaN, scaleStartSpacing_ = 6;
  // tick mark selection cache (by indexes per label and data version)
  QVector<int> selected_;
  int selectedStep_ = -1;
  quint64 version_ = 0, selectedVersion_ = ~0ull;
  QVector<Mark> marks_;
  // subscribers
  int nextSub_ = 1;
  QMap<int, std::function<void(std::optional<LogicalRange>)>> logicalSubs_;
  QMap<int, std::function<void(std::optional<TimeRange>)>> timeSubs_;
  QMap<int, std::function<void(int, int)>> sizeSubs_;
  std::optional<LogicalRange> lastNotified_;
  std::optional<LogicalRange> pendingRange_;   // set before the first layout
};

}  // namespace charts::plot
