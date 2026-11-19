#include "pages/ai/VoxelCube.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <algorithm>

namespace aipage {
namespace {
constexpr int N = 13;
constexpr double C = (N - 1) / 2.0;   // centre index

// deterministic hash → [0,1)
double h01(int x, int y, int z, int salt) {
  quint32 v = (quint32(x) * 73856093u) ^ (quint32(y) * 19349663u) ^ (quint32(z) * 83492791u) ^ (quint32(salt) * 2654435761u);
  v ^= v >> 13; v *= 0x5bd1e995u; v ^= v >> 15;
  return (v & 0xffffff) / double(0x1000000);
}

// Solid voxel? Full block minus two stepped tunnels opening on the front faces (+x and +y), meeting at the core,
// minus a few surface voxels (the reference's broken, irregular skin).
bool solid(int x, int y, int z) {
  if (x < 0 || y < 0 || z < 0 || x >= N || y >= N || z >= N) return false;
  const double c = C;
  // tunnel opening on a front face: depth from that face, square aperture shrinking with depth (terraces)
  auto tunnel = [&](int depth, int u, int v) {
    if (depth > int(N * 0.55)) return false;
    const double r = N * 0.36 - depth * 0.62;
    return std::abs(u - c) <= r && std::abs(v - c) <= r;
  };
  if (tunnel(N - 1 - y, x, z) || tunnel(N - 1 - x, y, z)) {
    // the core: a small block kept at the centre of the cavity
    return std::abs(x - c) <= 0.6 && std::abs(y - c) <= 0.6 && std::abs(z - c) <= 0.6;
  }
  const bool surface = x == N - 1 || y == N - 1 || z == N - 1;
  if (surface && h01(x, y, z, 7) < 0.07) return false;
  return true;
}
}

void VoxelCube::render(QSize size, qreal dpr, const QColor& glow) {
  m_size = size;
  m_metal = QPixmap(size * dpr); m_metal.setDevicePixelRatio(dpr); m_metal.fill(Qt::transparent);
  m_glow = QPixmap(size * dpr); m_glow.setDevicePixelRatio(dpr); m_glow.fill(Qt::transparent);
  if (size.isEmpty()) return;

  // isometric basis: x goes down-right, y goes down-left, z goes up
  const double s = std::min(size.width() / (2.0 * N * 0.866), size.height() / (2.0 * N)) * 0.98;
  const double ax = 0.866 * s, ay = 0.5 * s, az = s;
  const QPointF origin(size.width() / 2.0, size.height() / 2.0 - N * az / 2.0 + N * ay / 2.0 - N * ay / 2.0);
  auto P = [&](double x, double y, double z) {
    return QPointF(origin.x() + (x - y) * ax, origin.y() + (x + y) * ay - z * az + N * az * 0.5 - N * ay * 0.5 + 0.0);
  };
  // centre the whole block vertically
  const QPointF top = P(0, 0, N), bottom = P(N, N, 0);
  const double shiftY = size.height() / 2.0 - (top.y() + bottom.y()) / 2.0;
  auto Q = [&](double x, double y, double z) { QPointF p = P(x, y, z); p.ry() += shiftY; return p; };

  struct Face { QPolygonF poly; int kind; double shade; bool inner; bool lit; double depth; };   // kind: 0 top, 1 +x, 2 +y
  QVector<Face> faces;
  faces.reserve(1600);
  const double c = (N - 1) / 2.0;
  for (int z = 0; z < N; ++z)
    for (int y = 0; y < N; ++y)
      for (int x = 0; x < N; ++x) {
        if (!solid(x, y, z)) continue;
        const bool core = std::abs(x - c) <= 0.6 && std::abs(y - c) <= 0.6 && std::abs(z - c) <= 0.6;
        // inner = voxel faces looking into the cavity (lit strongly when on)
        const bool nearCavity = (!solid(x + 1, y, z) && x < N - 1) || (!solid(x, y + 1, z) && y < N - 1) || (!solid(x, y, z + 1) && z < N - 1);
        const double jitter = (h01(x, y, z, 1) - 0.5) * 0.10;
        const double order = x + y + z;
        if (!solid(x, y, z + 1)) {
          QPolygonF f{Q(x, y, z + 1), Q(x + 1, y, z + 1), Q(x + 1, y + 1, z + 1), Q(x, y + 1, z + 1)};
          faces << Face{f, 0, 0.80 + jitter, nearCavity || core, core || h01(x, y, z, 3) < 0.10, order + 0.3};
        }
        if (!solid(x + 1, y, z)) {
          QPolygonF f{Q(x + 1, y, z), Q(x + 1, y + 1, z), Q(x + 1, y + 1, z + 1), Q(x + 1, y, z + 1)};
          faces << Face{f, 1, 0.40 + jitter, nearCavity || core, core || h01(x, y, z, 4) < 0.13, order + 0.2};
        }
        if (!solid(x, y + 1, z)) {
          QPolygonF f{Q(x, y + 1, z), Q(x + 1, y + 1, z), Q(x + 1, y + 1, z + 1), Q(x, y + 1, z + 1)};
          faces << Face{f, 2, 0.58 + jitter, nearCavity || core, core || h01(x, y, z, 5) < 0.13, order + 0.1};
        }
      }
  std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth < b.depth; });

  // ---- metal layer: brushed steel, darker inside the cavity, thin dark seams between voxels
  {
    QPainter p(&m_metal);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor steel(138, 150, 168);   // cool blue steel, as the reference
    for (const auto& f : faces) {
      double k = f.shade * (f.inner ? 0.55 : 1.0);
      QColor col = QColor::fromRgbF(qBound(0.0, steel.redF() * k, 1.0), qBound(0.0, steel.greenF() * k, 1.0), qBound(0.0, steel.blueF() * k, 1.0));
      // soft vertical sheen on the side faces
      QLinearGradient g(f.poly.boundingRect().topLeft(), f.poly.boundingRect().bottomLeft());
      // brushed sheen: brighter towards the light (upper left of the block)
      const double sheen = 1.0 + 0.18 * qBound(0.0, 1.0 - f.poly.boundingRect().center().x() / size.width() * 1.3, 1.0);
      g.setColorAt(0, col.lighter(int((f.kind == 0 ? 112 : 118) * sheen))); g.setColorAt(1, col.darker(112));
      p.setPen(QPen(QColor(10, 12, 16, 210), qMax(0.5, s * 0.06)));
      p.setBrush(g);
      p.drawPolygon(f.poly);
    }
    // contact shadow under the block
    const QPointF b = Q(N, N, 0);
    QRadialGradient sh(QPointF(b.x(), b.y() + s * 0.6), s * N * 0.9);
    sh.setColorAt(0, QColor(0, 0, 0, 90)); sh.setColorAt(1, QColor(0, 0, 0, 0));
    p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    p.setPen(Qt::NoPen); p.setBrush(sh);
    p.drawEllipse(QPointF(b.x(), b.y() + s * 0.4), s * N * 0.9, s * N * 0.28);
  }

  // ---- glow layer: lit cavity walls, scattered glowing tiles, core halo (drawn additively at runtime)
  {
    QPainter p(&m_glow);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const QPointF coreP = Q(c + 0.5, c + 0.5, c + 0.5);
    for (const auto& f : faces) {
      if (!f.inner && !f.lit) continue;
      const QPointF ctr = f.poly.boundingRect().center();
      const double dist = QLineF(ctr, coreP).length() / (s * N * 0.7);
      double a = f.inner ? qBound(0.0, 1.05 - dist, 1.0) : (0.45 + 0.4 * h01(int(ctr.x()), int(ctr.y()), 0, 9));
      if (f.kind == 1) a *= 0.8;
      QColor col = glow; col.setAlphaF(qBound(0.0, a * 0.85, 1.0));
      const QRectF br = f.poly.boundingRect();
      QRadialGradient g(br.center(), br.width() * 0.75);
      g.setColorAt(0, col.lighter(130)); g.setColorAt(1, col);
      p.setBrush(g);
      p.drawPolygon(f.poly);
    }
    // core halo
    QRadialGradient halo(coreP, s * N * 0.42);
    QColor h0 = glow; h0.setAlpha(230);
    QColor h1 = glow; h1.setAlpha(110);
    QColor h2 = glow; h2.setAlpha(0);
    halo.setColorAt(0, QColor(255, 255, 255, 220)); halo.setColorAt(0.12, h0); halo.setColorAt(0.45, h1); halo.setColorAt(1, h2);
    p.setBrush(halo);
    p.drawEllipse(coreP, s * N * 0.42, s * N * 0.42);
    // light spilling under the block
    const QPointF b = Q(N, N, 0);
    QRadialGradient spill(QPointF(b.x(), b.y() + s), s * N * 0.8);
    QColor sp = glow; sp.setAlpha(60);
    QColor sp0 = glow; sp0.setAlpha(0);
    spill.setColorAt(0, sp); spill.setColorAt(1, sp0);
    p.setBrush(spill);
    p.drawEllipse(QPointF(b.x(), b.y() + s * 0.6), s * N * 0.8, s * N * 0.22);
  }
}
}  // namespace aipage
