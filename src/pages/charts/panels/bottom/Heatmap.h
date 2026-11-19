#pragma once
// Charts tab — bottom panel, Heatmap. Port of quant-terminal public/js/panels.js (hm, renderHeatmap, squarify,
// drawHeatmap) (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). Painted with QPainter.
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <optional>

class QWidget;

namespace charts::ui {
class BottomPanel;
namespace bottom {

// One squarified tile: the item's data plus its rectangle (JS { ...it, x, y, w, h }).
struct Tile {
  QJsonObject row;     // the screener row (empty for a sector group)
  QString name;        // sector group name
  double value = 0;
  QList<QJsonObject> rs;   // the group's rows
  double x = 0, y = 0, w = 0, h = 0;
};
// JS squarify(items, x, y, w, h): items with value > 0, in the given order.
QList<Tile> squarify(const QList<Tile>& items, double x, double y, double w, double h);

class HeatmapTab : public QObject {
  Q_OBJECT
public:
  explicit HeatmapTab(BottomPanel* panel);
  void render();                       // renderHeatmap
  void draw();                         // drawHeatmap (repaint at the canvas size)
  void setUniverse(const QString& u);  // the Market select

  // state (JS hm)
  QString universe = "us", size = "marketCap";
  std::optional<QList<QJsonObject>> rows;
  BottomPanel* P;

private:
  QPointer<QWidget> canvas_;
  int seq_ = 0;
};

}  // namespace bottom
}  // namespace charts::ui
