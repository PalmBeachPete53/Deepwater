#pragma once
#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include <QVector>
#include "global/FxStrength.h"

namespace overview {

// "CURRENCY STRENGTH · UNWEIGHTED": the eight majors as % change since the first bar on screen (global::FxStrength),
// timeframes 5m 15m 1H 4H D W, FOREX universe dropdown, crosshair with x/y pills, wheel zoom anchored on the
// hovered bar (re-based on every zoom step), stacked end labels and pulsing last points.
// Test hooks: DW_STRENGTH_HOVER=x,y (crosshair at panel coords), DW_STRENGTH_ZOOM=n (wheel steps, + = in).
class StrengthPanel : public QWidget {
public:
  explicit StrengthPanel(QWidget* parent);
  void preload();   // the 28 pairs' candles and first quotes, without the live refresh (starts when shown)
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseDoubleClickEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  bool event(QEvent*) override;
private:
  QRectF plotRect() const;
  bool phone() const;
  qreal subY() const;   // the subtitle's baseline
  QRectF hoverRect() const;
  QRectF dropdownRect() const;
  QList<QRectF> tfRects() const;
  QRectF infoRect() const;
  QString title() const;
  void resetView();
  void zoomAt(double anchorBar, double factor);
  void rebuild();                  // static layer: frame, header, axes, lines, end labels
  void pulseRects(QVector<QRectF>& out) const;
  // view state
  struct Vis { int first = 0, last = -1; double lo = -1, hi = 1; bool ok = false; };
  Vis visible() const;
  double xOf(double bar) const;
  double barAt(double x) const;
  double yOf(double v, const Vis& vis) const;
  QString barTime(int i, bool pill) const;

  global::FxStrength* m_fx;
  double m_x0 = 0, m_x1 = 0;       // visible range in bar units (can extend past the last bar)
  bool m_viewSet = false;
  int m_lastN = 0;
  QPointF m_mouse;
  bool m_hover = false;
  int m_hoverTf = -1;
  QPixmap m_cache;
  bool m_dirty = true;
  bool m_pulsing = false;   // shown: the pulse runs on global::FrameClock
  Vis m_vis;                // visible() at the last rebuild
  QVector<QPointF> m_ends;         // last-point positions (for the pulse)
};
}  // namespace overview
