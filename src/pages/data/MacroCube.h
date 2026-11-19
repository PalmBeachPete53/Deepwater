#pragma once
#include "macrodata/MacroDataStore.h"
#include "pages/data/Gl3d.h"
#include <QOpenGLWidget>
#include <QStringList>
#include <functional>

// Data › Macro data, macro cube (optional view): countries × indicators × time. Each cell is one month (or quarter /
// year) of one macro chain, coloured by its rank in that chain's own history (blue = lowest ever, grey = middle,
// orange = highest). The cube is read by slices: a date (every country and indicator that month), a country (all its
// indicators through time) or an indicator (every country through time); the rest of the cube stays as a faint ghost.
namespace macrodatapage {

class MacroCube : public QOpenGLWidget {
public:
  struct Series { int country = 0, indicator = 0, span = 1; QString key; QVector<macrodata::MonthCell> cells; QString unit; };
  std::function<void(const QString& info)> onInfo;
  std::function<void(const QString& chainKey, int month)> onOpen2D;

  explicit MacroCube(QWidget* parent);
  ~MacroCube() override;
  void setData(const QStringList& countries, const QStringList& indicators, QVector<Series> series);
  enum Axis { Date = 0, Country = 1, Indicator = 2 };
  void setSlice(Axis axis, int value);   // value: month index, country index or indicator index
  Axis axis() const { return m_axis; }
  int value() const { return m_value; }
  int firstMonth() const { return m_m0; }
  int lastMonth() const { return m_m1; }
  QString sliceTitle() const;
  void frame();                          // camera facing the slice

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
  int pick(const QPointF& pos, int* cell) const;   // series index, cell index
  QString describe(int s, int cell) const;
  float xOf(int month) const { return (month - m_m0) * kX; }
  float yOf(int country) const { return country * kY; }
  float zOf(int ind) const { return ind * kZ; }

  static constexpr float kX = 0.12f, kY = 1.1f, kZ = 4.0f;
  QStringList m_countries, m_indicators;
  QVector<Series> m_series;
  QHash<QPair<int, int>, int> m_index;   // (country, indicator) → series
  int m_m0 = 0, m_m1 = 0, m_value = 0;
  Axis m_axis = Date;
  macrodata3d::BoxRenderer m_boxes;
  macrodata3d::OrbitCamera m_cam;
  macrodata3d::Overlay* m_overlay = nullptr;
  bool m_dirty = true, m_glReady = false;
  QPointF m_press, m_last;
  bool m_moved = false;
  int m_hoverS = -1, m_hoverC = -1;
};
}  // namespace macrodatapage
