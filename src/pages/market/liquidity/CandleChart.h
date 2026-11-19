#pragma once
#include "analytics/Liquidity.h"
#include <QWidget>
#include <QPixmap>
#include <QSet>

// Candlestick chart of the Liquidity page (QPainter, no web view): hourly candles on a bar-index time scale
// (no weekend gaps), pool lines (BSL dotted red, SSL dotted cyan), PDH/PDL dashed gold, stacked level tags on the
// price axis, last-price tag, dashed crosshair with price and English date labels (UTC), wheel zoom around the
// cursor, drag to pan, price auto-scale over the visible candles and nearby levels, bottom-right grip resizes
// the chart height. Static layers are cached in a pixmap; the crosshair repaints on top of it.
class CandleChart : public QWidget {
  Q_OBJECT
public:
  explicit CandleChart(QWidget* parent = nullptr);
  void setData(const QVector<market::Bar>& bars, const QVector<analytics::Pool>& pools, double pdh, double pdl,
               int decimals, bool resetView);
  void setLastPrice(double p);
  void setStatus(const QString& s);            // shown instead of "LOADING…" while there are no bars (failed load)
  void zoomAt(double x, int steps);            // also used by the DW_LIQ_ZOOM test hook
  void setHover(const QPointF& p);             // test hook (DW_LIQ_HOVER)
  int preferredHeight() const { return m_height; }
  bool userSized() const { return m_userSized; }   // the user dragged the grip: keep that height
signals:
  void heightRequested(int h);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QRectF plot() const;
  double xOf(double index) const;
  double indexAt(double x) const;
  double yOf(double price) const;
  double priceAt(double y) const;
  void autoscale();
  void applyReset();
  bool shown(const analytics::Pool& p, int firstVisible) const;
  void pickNearest();
  void renderStatic();
  void clampView();
  QString priceText(double v) const;
  QVector<market::Bar> m_bars;
  QString m_status;                            // cleared by setData
  QVector<analytics::Pool> m_pools;
  QSet<int> m_nearest;
  double m_pdh = NAN, m_pdl = NAN, m_last = NAN, m_lo = 0, m_hi = 1;
  int m_dec = 5;
  double m_spacing = 3, m_right = 0;           // px per bar, index at the right edge of the plot
  QPixmap m_cache; bool m_dirty = true, m_resetPending = true;
  QPointF m_mouse{-1, -1};
  bool m_pinned = false;
  bool m_drag = false, m_resize = false; QPointF m_dragFrom; double m_dragRight = 0; int m_dragH = 0;
  int m_height = 521;
  bool m_userSized = false;
};
