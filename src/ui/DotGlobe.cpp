#include "ui/DotGlobe.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QImage>
#include <QRadialGradient>
#include <QtMath>
#include <cmath>

namespace ui {

namespace {
constexpr double kRefR = 269.0;   // radius of the Bank Research globe at the reference size: sizes scale from it
const QColor kGrey(70, 72, 78), kGold(214, 190, 92), kGoldHot(238, 214, 110);
}

QVector3D DotGlobe::unit(double latDeg, double lonDeg) {
  const double la = qDegreesToRadians(latDeg), lo = qDegreesToRadians(lonDeg);
  return QVector3D(float(std::cos(la) * std::sin(lo)), float(std::sin(la)), float(std::cos(la) * std::cos(lo)));
}

const QVector<QVector3D>& DotGlobe::dots() {
  static const QVector<QVector3D> d = [] {
    QVector<QVector3D> out;
    QImage mask(":/banks/land.pbm");
    if (mask.isNull()) return out;
    mask = mask.convertToFormat(QImage::Format_Grayscale8);
    const double step = 1.1;   // same grid as Bank Research
    for (double lat = -84; lat <= 84; lat += step) {
      const int n = qMax(1, int(std::round(360.0 * std::cos(qDegreesToRadians(lat)) / step)));
      for (int k = 0; k < n; ++k) {
        const double lon = -180.0 + (k + 0.5) * 360.0 / n;
        const int px = qBound(0, int((lon + 180.0) / 360.0 * mask.width()), mask.width() - 1);
        const int py = qBound(0, int((90.0 - lat) / 180.0 * mask.height()), mask.height() - 1);
        if (mask.constScanLine(py)[px] < 128) out << unit(lat, lon);   // PBM: 1 (black) = land
      }
    }
    return out;
  }();
  return d;
}

void DotGlobe::setCamera(const Camera& c) {
  m_cam = c;
  const double s = qDegreesToRadians(c.lon), t = qDegreesToRadians(c.tilt);
  m_cc = float(std::cos(s)); m_sc = float(std::sin(s)); m_ct = float(std::cos(t)); m_st = float(std::sin(t));
  m_k = c.R * std::sqrt(double(kDist) * kDist - 1.0);
}

QVector3D DotGlobe::view(const QVector3D& v) const {
  const float x = v.x() * m_cc - v.z() * m_sc;
  const float z = v.z() * m_cc + v.x() * m_sc;
  const float y = v.y();
  return QVector3D(x, y * m_ct - z * m_st, y * m_st + z * m_ct);
}

// Perspective camera kDist radii from the centre: the silhouette has radius R on screen.
QPointF DotGlobe::projectView(const QVector3D& v) const {
  const double w = kDist - v.z();
  return QPointF(m_cam.centre.x() + m_k * v.x() / w, m_cam.centre.y() - m_k * v.y() / w);
}

QPointF DotGlobe::project(double lat, double lon, bool* visible) const {
  const QVector3D v = view(unit(lat, lon));
  if (visible) *visible = v.z() > kHorizon;
  return projectView(v);
}

bool DotGlobe::unproject(const QPointF& s, double* lat, double* lon) const {
  // ray from the camera (0, 0, D) through the screen point; nearest intersection with the unit sphere
  const double k = m_cam.R * std::sqrt(double(kDist) * kDist - 1.0);
  const double u = (s.x() - m_cam.centre.x()) / k, v = -(s.y() - m_cam.centre.y()) / k;
  const double D = kDist, a = u * u + v * v + 1.0;
  const double disc = D * D - a * (D * D - 1.0);
  if (disc < 0) return false;
  const double w = (D - std::sqrt(disc)) / a;
  const double x2 = u * w, y2 = v * w, z2 = D - w;
  // undo the tilt, then the spin
  const double y = y2 * m_ct + z2 * m_st;
  const double z1 = -y2 * m_st + z2 * m_ct;
  const double x = x2 * m_cc + z1 * m_sc;
  const double z = -x2 * m_sc + z1 * m_cc;
  *lat = qRadiansToDegrees(std::asin(qBound(-1.0, y, 1.0)));
  *lon = qRadiansToDegrees(std::atan2(x, z));
  return true;
}

QPixmap DotGlobe::body(qreal dpr) const {
  const double R = m_cam.R;
  QPixmap pm(QSize(int(2 * R + 4), int(2 * R + 4)) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter b(&pm); b.setRenderHint(QPainter::Antialiasing);
  const QPointF bc(R + 2, R + 2);
  QRadialGradient rg(bc + QPointF(-R * 0.35, -R * 0.4), R * 1.45);
  if (theme::isDefault()) {
    rg.setColorAt(0.0, QColor(34, 38, 48));
    rg.setColorAt(0.45, QColor(20, 23, 31));
    rg.setColorAt(0.85, QColor(12, 14, 19));
    rg.setColorAt(1.0, QColor(8, 9, 12));
  } else {   // preview roles: ocean inner → outer, limb shade at the edge
    const auto& r = theme::roles();
    rg.setColorAt(0.0, theme::mix(r.oceanInner, Qt::white, theme::isDark() ? 0.06 : 0.0));
    rg.setColorAt(0.5, r.oceanInner);
    rg.setColorAt(0.9, r.oceanOuter);
    rg.setColorAt(1.0, theme::mix(r.oceanOuter, QColor(r.limbShade.red(), r.limbShade.green(), r.limbShade.blue()), r.limbShade.alphaF()));
  }
  b.setPen(Qt::NoPen); b.setBrush(rg);
  b.drawEllipse(bc, R, R);
  b.setBrush(Qt::NoBrush); b.setPen(QPen(theme::isDefault() ? QColor(60, 62, 68, 120) : theme::roles().landBorder, 1.0));
  b.drawEllipse(bc, R, R);
  return pm;
}

void DotGlobe::paintGraticule(QPainter& p) const {
  // polylines (much cheaper to stroke than one big antialiased path)
  p.save();
  QVector<QPointF> run; run.reserve(64);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(QPen(theme::isDefault() ? QColor(58, 62, 72) : theme::roles().graticule, 1.0)); p.setBrush(Qt::NoBrush);
  auto flush = [&] { if (run.size() > 1) p.drawPolyline(run.constData(), int(run.size())); run.clear(); };
  auto trace = [&](auto pointAt, int n) {
    for (int i = 0; i <= n; ++i) {
      const QVector3D v = view(pointAt(i));
      if (v.z() <= kHorizon) { flush(); continue; }
      run << projectView(v);
    }
    flush();
  };
  for (int lat = -75; lat <= 75; lat += 15) trace([lat](int i) { return unit(lat, -180 + i * 6.0); }, 60);
  for (int lon = -180; lon < 180; lon += 15) trace([lon](int i) { return unit(-90 + i * 5.0, lon); }, 36);
  p.restore();
}

void DotGlobe::paintLand(QPainter& p) const {
  // land dots, batched by colour level (same light and colours as Bank Research)
  const QVector3D L = QVector3D(-0.50f, 0.30f, 0.81f).normalized();
  const double s = qMax(0.45, m_cam.R / kRefR);
  constexpr int kLevels = 10;
  QVector<QRectF>* buckets = m_buckets;   // cleared, not freed: no reallocation every frame
  for (int i = 0; i < kLevels; ++i) buckets[i].clear();
  for (const auto& d : dots()) {
    const QVector3D v = view(d);
    if (v.z() <= kHorizon + 0.01f) continue;
    const float lit = QVector3D::dotProduct(v, L);
    const float f = qBound(0.0f, (lit - 0.74f) / 0.14f, 1.0f);           // 0 grey … 1 gold
    const float limb = qMin(1.0f, (v.z() - kHorizon) * 3.4f);
    int lvl = int(f * (kLevels / 2 - 1) + 0.5f) * 2 + (limb > 0.6f ? 1 : 0);
    lvl = qBound(0, lvl, kLevels - 1);
    const qreal sz = (1.5 + 1.25 * limb) * s;
    const QPointF q = projectView(v);
    buckets[lvl] << QRectF(q.x() - sz / 2, q.y() - sz / 2, sz, sz);
  }
  p.save();
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing, false);
  for (int i = 0; i < kLevels; ++i) {
    if (buckets[i].isEmpty()) continue;
    const qreal f = (i / 2) / qreal(kLevels / 2 - 1);
    // Deepwater: grey → gold; other themes: land border (unlit) → accent (lit), as the preview's globe roles
    // light themes: the land border alone is too pale on a light ocean, pull it toward the section text
    const QColor grey = theme::isDefault() ? kGrey
                      : theme::isDark() ? theme::roles().landBorder
                                        : theme::mix(theme::roles().landBorder, theme::roles().textSection, 0.55);
    const QColor gold = theme::isDefault() ? kGold : theme::roles().accent;
    const QColor hot = theme::isDefault() ? kGoldHot : theme::mix(theme::roles().accent, theme::roles().text, 0.2);
    QColor col = theme::mix(grey, f > 0.85 ? hot : gold, f);
    col.setAlpha(i % 2 ? 255 : 150);
    p.setBrush(col);
    p.drawRects(buckets[i].constData(), int(buckets[i].size()));
  }
  p.restore();
}
}  // namespace ui
