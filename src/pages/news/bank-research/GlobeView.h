#pragma once
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPixmap>
#include <QVector>
#include <QVector3D>
#include <QRectF>
#include <QRegion>

namespace banks::res {

// Rotating dot-matrix globe of Bank Research: land as a dot grid (Natural Earth mask, resources/banks/land.pbm),
// graticule, beacons at the banks' head offices, arcs between financial centres, radar sweep, orbit rings and a
// starfield. Painted with QPainter at ≤ 30 fps; the timer only runs while the widget is visible.
// Env hook DW_GLOBE_LON=<deg> freezes the rotation at that central longitude (screenshots).
class GlobeView : public QWidget {
  Q_OBJECT
public:
  explicit GlobeView(QWidget* parent = nullptr);
  void setHint(const QString& h) { m_hint = h; m_bg = {}; update(); }
  void setSelectedBank(const QString& id) { m_selected = id; update(); }
  void setPaused(bool p);
  bool animating() const { return m_timer.isActive(); }
signals:
  void bankClicked(const QString& bankId);
  void frame();                        // after each animation step (the page's blinks ride on it)
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  struct Beacon { QString bankId, label; QVector3D p; };
  void buildDots();
  void buildBackground();
  void tick();
  QPointF centre() const;
  qreal radius() const;
  // rotate a unit vector by the current spin and tilt; returns view-space vector (z > 0 faces the viewer)
  QVector3D view(const QVector3D& v) const;
  QPointF project(const QVector3D& viewSpace) const;
  void setupFrame();                   // camera of the current spin/tilt/size for view() and project()
  QRect contentRect() const;           // disc, arcs and beacons of the current frame
  QRect wedgeRect() const;             // radar wedge of the current frame
  QVector<QVector3D> m_dots;           // unit vectors of land dots
  QVector<Beacon> m_beacons;
  QVector<QPair<int, int>> m_arcs;     // beacon index pairs
  QPixmap m_bg, m_body, m_bgBody;     // background, sphere, background with the sphere on it
  double m_cosS = 1, m_sinS = 0, m_cosT = 1, m_sinT = 0, m_pk = 0;   // setupFrame()
  QPointF m_pc;
  QRect m_painted;                     // bounding box of what moves, as the last paintEvent drew it
  QVector<qreal> m_labelW;             // beacon label widths (chip font)
  QVector<QRectF> m_buckets[10];       // land dots per colour level, kept between frames
  QTimer m_timer;
  QElapsedTimer m_clock;
  qint64 m_elapsedBase = 0;
  double m_lon0 = -48.0;               // central longitude at t = 0
  double m_spin = 0;                   // current central longitude
  double m_sweep = 0;                  // sweep angle (deg)
  double m_tilt = 36.0;                // north pole tipped towards the viewer (deg, fitted on the reference)
  bool m_frozen = false, m_paused = false;
  QString m_hint, m_selected;
  int m_hover = -1;
  QVector<QPair<QRectF, int>> m_hits; // label rects of the last frame → beacon index
};
}  // namespace banks::res
