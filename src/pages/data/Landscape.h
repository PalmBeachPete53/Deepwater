#pragma once
#include "macrodata/MacroDataStore.h"
#include "pages/data/Gl3d.h"
#include <QOpenGLWidget>
#include <QColor>
#include <functional>

// Data › Macro data, 3D landscape (optional view; the 2D chains stay the default).
// X = time (one box per month), Z = the chains, grouped in plateaus by family, a forecast chain just behind the chain
// it predicts; box height and brightness = the month's rank in its own chain's history (0 = lowest ever, 100 =
// highest), so every unit compares and moves that happen together show as ridges across the landscape. Revised
// months carry a thin orange slab; forecasts are see-through; a translucent plane marks today, another the slice
// date (slider). Left drag: orbit · right drag / shift: move · wheel: zoom · click: details · double-click: open in 2D.
namespace macrodatapage {

class Landscape : public QOpenGLWidget {
public:
  struct Row {
    macrodata::Chain chain;
    QVector<macrodata::MonthCell> cells;
    QString group;          // label of the plateau (family name) shown once at its first row
    QColor colour;          // family colour
    bool forecast = false;
  };
  std::function<void(const QString& info)> onInfo;                        // hover / click text
  std::function<void(const QString& chainKey, int month)> onOpen2D;       // double-click

  explicit Landscape(QWidget* parent);
  ~Landscape() override;
  void setRows(QVector<Row> rows);
  void setSlice(int month);              // -1: none
  int firstMonth() const { return m_m0; }
  int lastMonth() const { return m_m1; }
  int slice() const { return m_slice; }
  void resetCamera();

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override { m_overlay->resize(w, h); }
  void drawLabels(QPainter& p);
  void paintGL() override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void mouseDoubleClickEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;

private:
  void rebuild();
  void rebuildSlice();
  void plane(QVector<macrodata3d::Instance>& v, int month, const QColor& col, float alpha) const;
  QPair<int, int> pick(const QPointF& pos) const;   // row, cell
  QString describe(int row, int cell) const;
  float rowZ(int i) const { return m_z.value(i); }
  float height(const macrodata::MonthCell& c) const { return m_h * 0.03f + c.rank * m_h; }
  int span(int row) const;

  static constexpr float kX = 0.25f;
  // row spacing, plateau gap and relief height follow the scene's size, so many chains stay a square-ish landscape
  float kRow = 0.8f, kGap = 4.0f, m_h = 7.0f;
  QVector<Row> m_rows;
  QVector<float> m_z;
  int m_m0 = 0, m_m1 = 0, m_slice = -1;
  macrodata3d::BoxRenderer m_boxes;
  macrodata3d::BoxRenderer m_slicePlane;   // the slider's plane alone: moving it re-uploads one box, not all of them
  macrodata3d::OrbitCamera m_cam;
  macrodata3d::Overlay* m_overlay = nullptr;
  bool m_dirty = true, m_sliceDirty = true, m_glReady = false;
  QPointF m_press, m_last;
  bool m_moved = false;
  QPair<int, int> m_hover{-1, -1}, m_sel{-1, -1};
};
}  // namespace macrodatapage
