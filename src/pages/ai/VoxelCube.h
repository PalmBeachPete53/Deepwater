#pragma once
#include <QPixmap>
#include <QSize>
#include <QColor>

// The environment cube of AI › Agents, after the reference picture (complex metallic cube with a glowing core):
// a 10×10×10 block of small steel cubes in isometric view, hollowed by stepped square tunnels on its two front faces,
// with a core voxel at the centre. Rendered once per size into two layers:
//   metal() — the steel cube, unlit (what an empty environment shows);
//   glow()  — the light of a loaded environment (lit tunnel walls, glowing tiles, core halo), drawn over the metal
//             with additive blending at the current intensity (0 = off, 1 = fully lit).
namespace aipage {
class VoxelCube {
public:
  void render(QSize size, qreal dpr, const QColor& glowColour);   // rebuilds the layers (cheap enough per resize)
  const QPixmap& metal() const { return m_metal; }
  const QPixmap& glow() const { return m_glow; }
  QSize size() const { return m_size; }
private:
  QPixmap m_metal, m_glow;
  QSize m_size;
};
}  // namespace aipage
