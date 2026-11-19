#pragma once
// Charts tab — the navigator: a thin range slider under a chart pane. It shows the loaded history as a mini curve of
// the closes and the part visible in the chart as a window with two handles. The window's width is the zoom
// (nav::ZoomMap, user's rule 2026-11-19): kMinWindowPx is always the chart's deepest zoom, from kLinearPx up the
// navigator is the plain map of the whole history, between them it shows a narrower span around the view, the window
// staying exact:
//   drag inside = pan (same zoom), press outside = centre the window there then keep panning, drag a handle = zoom
//   (horizontal only; the chart's price scale fits the visible bars by itself).
// The chart's visible logical range is the only source of truth: the navigator reads it (repaints on the time
// scale's range-change notification) and writes it with TimeScale::setVisibleLogicalRange. Rules: charts::nav
// (src/charts/core/RangeWindow).
#include "charts/core/RangeWindow.h"
#include <QPixmap>
#include <QPointer>
#include <QVector>
#include <QWidget>
#include <functional>
#include <optional>

namespace charts::plot { class Chart; }

namespace charts::ui::pane {

class Navigator : public QWidget {
public:
  static constexpr int kHeight = 36;
  static constexpr double kMinWindowPx = 30;   // the window at the chart's deepest zoom
  static constexpr double kLinearPx = 120;     // from this window width up: the whole history

  // curve(xs, closes): fills the logical index (on the chart's time scale) and the close of every bar to draw.
  using CurveFn = std::function<void(QVector<double>& xs, QVector<double>& closes)>;
  Navigator(plot::Chart* chart, CurveFn curve, QWidget* parent);
  ~Navigator() override;

  void setRightPad(double bars) { rightPad_ = bars; update(); }   // empty slots after the last bar in the domain
  // bars changed: rebuild the mini curve at the next paint. lastBarOnly: a live tick (the last bar updated or bars
  // appended), redrawn only when it shows.
  void dataChanged(bool lastBarOnly = false);
  void restyle();                // theme changed
  std::optional<nav::Span> shownSpan() const { return disp_; }   // the span drawn last (test hook log)

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;

private:
  nav::Span domain() const;                 // everything loaded
  nav::Span display();                      // the part the navigator shows (the gesture's during a drag)
  nav::ZoomMap zoomMap() const;
  std::optional<nav::Span> view() const;   // chart's visible span (edges), not clipped
  void apply(const nav::Span& s);
  void setHover(nav::Part p);
  void updateCursor();
  void rebuildCurve(bool force, const nav::Span& dom);

  QPointer<plot::Chart> chart_;
  CurveFn curve_;
  int sub_ = 0;
  double rightPad_ = 0;
  std::optional<nav::Gesture> drag_;
  nav::Part hover_ = nav::Part::None;
  QPixmap curvePix_;
  bool curveDirty_ = true, curveFull_ = true;
  int curveRev_ = -1;
  nav::Span curveDom_;
  std::optional<nav::Span> disp_;   // last displayed span (shownSpan keeps it while it still works)
  nav::Span dragShown_;             // the span the running gesture shows
  // what the drawn curve was made from (rebuildCurve skips a redraw that would draw the same pixels)
  int curveN_ = -1, curveLastY_ = 0;
  double curveLo_ = 0, curveHi_ = 0, curveLastX_ = 0;
};

}  // namespace charts::ui::pane
