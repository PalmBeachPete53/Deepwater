#pragma once
// Charts tab — the chart legend (chart.js updateLegend: <div class="legend"> over the top-left of the chart), painted.
// Port of quant-terminal public/js/chart.js + public/css/app.css .legend rules (MIT, Copyright (c) 2026 Lewis
// Jackson — docs/charts/NOTICE.md). Transparent for the mouse: ChartPane routes the chart's mouse events here
// (hit()), so only the parts the CSS gives `pointer-events: auto` (title, indicator / compare rows, notes) catch them.
#include <QColor>
#include <QFont>
#include <QRectF>
#include <QVector>
#include <QWidget>

namespace charts::ui::pane {

class Legend : public QWidget {
public:
  struct Run {
    QString text;
    QColor color;
    QFont font;
    double marginRight = 0;
    QString act;          // non-empty: a button (icon) or the clickable title ("symbol")
    QString icon;         // SVG markup of a button
    QString tip;
    double w = 0;         // layout
  };
  struct Cell { QVector<Run> runs; bool acts = false; };   // acts: shown only while the row is hovered
  struct Row {
    enum Kind { Title, Ind, Compare, Note } kind = Title;
    QString key;          // indicator uid / compare symbol
    QVector<Cell> cells;
    double opacity = 1;
    QString note;         // Note rows: wrapped text
    QColor noteColor;
    QRectF rect;          // layout, in legend coordinates
  };
  struct Hit { int row = -1; QString act; bool interactive = false; };

  explicit Legend(QWidget* parent);
  void setRows(QVector<Row> rows);
  void setMaxWidth(double w);             // calc(100% - 90px) of the pane
  Hit hit(const QPointF& legendPos) const;
  void setHover(int row, const QString& act);
  const QVector<Row>& rows() const { return rows_; }
  int hoverRow() const { return hover_; }

protected:
  void paintEvent(QPaintEvent*) override;

private:
  void relayout();
  QVector<Row> rows_;
  double maxW_ = 400;
  int hover_ = -1;
  QString hoverAct_;
};

}  // namespace charts::ui::pane
