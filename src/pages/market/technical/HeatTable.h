#pragma once
#include "pages/market/technical/HeatCells.h"
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>

// The heatmap table: header row + one row of seven tiles per instrument, painted (only visible rows), scrolled
// with the wheel (thin scrollbar), rows fading in top → bottom after a filter change, legend line under the rows.
class HeatTable : public QWidget {
  Q_OBJECT
public:
  struct Row { QString symbol, name, price; analytics::TechRead read; QVector<heat::Cell> cells; };
  explicit HeatTable(QWidget* parent = nullptr);
  void setRows(const QVector<Row>& rows, bool filterChanged);
  void setSelected(const QString& symbol);
  void setPrice(const QString& symbol, const QString& price);   // repaints that cell only, if it changed
  void setSort(int column, bool desc);           // column 0 = instrument, 1..6 = heat columns
  void setLoadingText(const QString& t);         // shown instead of the empty message while loading
  int sortColumn() const { return m_sortCol; }
  bool sortDesc() const { return m_desc; }
signals:
  void rowClicked(const QString& symbol);
  void headerClicked(int column);
protected:
  void paintEvent(QPaintEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void layoutColumns();
  void prepareText();                            // elided texts + advances of every row (after layoutColumns)
  QRect tileRect(int row) const;                 // the instrument tile of a row (hover repaints)
  double contentHeight() const;
  double viewportHeight() const;
  void clampScroll();
  int rowAt(const QPointF& p) const;
  void paintLegend(QPainter& p, double y);
  double legendHeight() const;
  QVector<Row> m_rows;
  struct RowText { QString name, word[6], detail[6]; double tw[6], dw[6]; };
  QVector<RowText> m_text;                       // per row, from prepareText()
  QVector<double> m_x, m_w;                      // 7 columns (width 0 = dropped on narrow tables)
  QVector<bool> m_hidden;
  bool m_tight = false;                          // narrow table: tighter letter spacing (see layoutColumns)
  QString m_selected, m_loading;
  int m_sortCol = 1, m_hover = -1, m_hoverHeader = -1; bool m_desc = true;
  double m_scroll = 0;
  QTimer m_anim; QElapsedTimer m_fade; bool m_fading = false;
};
