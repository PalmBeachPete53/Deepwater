#pragma once
// Charts tab — drawing tools, rendered as a charts::plot series primitive. Port of quant-terminal public/js/drawings.js
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). Points are stored as { time (UTC seconds), price }
// so drawings stay put when you change timeframe.
//
// For the page shell: tools() is the JS TOOLS table in its order (cursor first). `icon` is the JS SVG markup that
// goes inside <svg viewBox="0 0 24 24">; svgIcon() wraps it with the CSS `svg` defaults (fill none, stroke
// currentColor 1.6, round caps/joins) so QSvgRenderer draws it like the browser; renderIcon() paints it.
#include "pages/charts/plot/Primitive.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QVector>
#include <limits>
#include <optional>

namespace charts::ui {

class ChartPane;

struct Tool {
  QString id, label;
  int points = 0;     // clicks needed (0 for the cursor)
  QString icon;       // SVG markup inside <svg viewBox="0 0 24 24">
};
const QList<Tool>& tools();                  // JS TOOLS
const Tool* toolById(const QString& id);     // JS TOOL_BY_ID; nullptr if unknown
const QVector<double>& fibLevels();          // JS FIB_LEVELS: 0, 0.236, 0.382, 0.5, 0.618, 0.786, 1, 1.618

// <svg viewBox="0 0 24 24" ...>inner</svg> with the app.css `svg` rule applied and currentColor = color.
QByteArray svgIcon(const QString& inner, const QColor& color, double strokeWidth = 1.6);
// The icon at `px` CSS pixels (square), for the device pixel ratio dpr.
QPixmap renderIcon(const QString& inner, const QColor& color, int px, double dpr = 2.0, double strokeWidth = 1.6);

// The legend's icons (chart.js ICON): gear, eye, x.
QString legendIcon(const QString& name);

class DrawingLayer : public ::charts::plot::Primitive {
public:
  struct Point { double time = 0; double price = 0; };
  struct Item {
    QString id, type;
    QVector<Point> points;
    QString text, color;            // empty = not set
    double rr = std::numeric_limits<double>::quiet_NaN();   // long/short reward:risk; NaN = not set (2)
    bool done = false;              // temp only
    QJsonObject toJson() const;     // { id, type, points, text?, color?, rr? } (app.saveDrawings shape)
    static Item fromJson(const QJsonObject& o);
  };

  explicit DrawingLayer(ChartPane* pane);

  QVector<Item> items;
  std::optional<Item> temp;
  QString selected;                 // id of the selected item; empty = none (JS this.selected)

  // ISeriesPrimitive
  void attached(const ::charts::plot::AttachedParams& p) override;
  void detached() override;
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Top; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

  void update();                    // repaint
  void setItems(const QJsonArray& items);
  QJsonArray itemsJson() const;
  const Item* selectedItem() const;
  // Nearest drawing within a few pixels of (x, y), for selecting. Returns its id (empty = none).
  QString drawingAt(double x, double y) const;   // (not Primitive::hitTest: the chart's hover asks that one)

private:
  std::optional<double> x(double t) const;
  std::optional<double> y(double p) const;
  void drawOne(QPainter& ctx, const QSize& size, const Item& d, bool active);

  ChartPane* pane;
  ::charts::plot::Series* series = nullptr;
  std::function<void()> requestUpdate;
};

}  // namespace charts::ui
