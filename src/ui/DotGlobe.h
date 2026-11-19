#pragma once
#include <QPointF>
#include <QPixmap>
#include <QVector>
#include <QVector3D>
#include <QRectF>

class QPainter;

// The dot-matrix globe of News › Bank Research, reusable at any size (Overview's global map, Economy › Macro).
// Same look as banks::res::GlobeView: navy sphere lit from the upper left, land as a dot grid (Natural Earth mask
// resources/banks/land.pbm, 1.1° grid) turning gold on the lit side, grey graticule every 15°, perspective camera
// 3 radii away. Everything scales with the radius (the Bank Research globe is the 269 px reference).
// The caller owns the timer and the surroundings (panel, halo, markers); this only paints the globe.
namespace ui {
class DotGlobe {
public:
  struct Camera {
    QPointF centre;      // widget coordinates
    double R = 100;      // silhouette radius on screen (logical px)
    double lon = 0;      // longitude at the centre of the view (deg)
    double tilt = 36;    // latitude at the centre of the view (deg): the north pole leans toward the viewer
  };
  static constexpr float kDist = 3.0f;                 // camera distance in sphere radii
  static constexpr float kHorizon = 1.0f / kDist;      // view-space z of the silhouette

  void setCamera(const Camera& c);
  const Camera& camera() const { return m_cam; }

  static QVector3D unit(double latDeg, double lonDeg);
  QVector3D view(const QVector3D& unitVec) const;             // rotate into view space (z > kHorizon faces us)
  QPointF projectView(const QVector3D& viewVec) const;        // view space → screen
  // lat/lon → screen; *visible is false behind the horizon
  QPointF project(double latDeg, double lonDeg, bool* visible = nullptr) const;
  // screen → lat/lon on the front surface; false when off the globe
  bool unproject(const QPointF& screen, double* latDeg, double* lonDeg) const;

  // Static sphere (body gradient and rim) for the current radius, transparent around it, centred on a square of
  // side 2R + 4. Cache it: it depends on the radius only.
  QPixmap body(qreal dpr) const;
  void paintGraticule(QPainter& p) const;
  void paintLand(QPainter& p) const;

private:
  static const QVector<QVector3D>& dots();
  Camera m_cam;
  float m_cc = 1, m_sc = 0, m_ct = 1, m_st = 0;
  double m_k = 0;                                   // projection scale R·√(D² − 1), set with the camera
  mutable QVector<QRectF> m_buckets[10];            // paintLand's dots per colour level, kept between frames
};
}  // namespace ui
