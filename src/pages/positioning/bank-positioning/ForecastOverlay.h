#pragma once
#include <QWidget>
#include <QList>
#include <QRectF>
#include <functional>
#include "banks/Forecasts.h"

namespace banks::pos {

class ForecastChart;

// FX BANK FORECASTS overlay: pair pills, bank × quarter table (left), daily candles with one dashed line per
// quarter at the average of the banks' forecasts (right). Clicking a bank row shows that bank's own forecasts.
class ForecastOverlay : public QWidget {
public:
  explicit ForecastOverlay(QWidget* parent);
  void open(const QString& pair = {});          // shows the overlay on that pair (first pair if empty)
  void refresh();                               // forecasts or quotes changed
  QString pair() const { return m_pair; }
  std::function<void()> onClose;
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void showEvent(QShowEvent*) override;   // the spot quote is polled only while the overlay is on screen
  void hideEvent(QHideEvent*) override;
private:
  QRectF panelRect() const;
  QRectF closeRect() const;
  QList<QRectF> pairRects() const;
  QRectF moreRect() const;
  QString footnote() const;
  bool footnoteTwoLines() const;
  QList<QRectF> quarterRects() const;
  QRectF rowRect(int i) const;
  void loadPair(bool animate);
  void updateLines(bool animate);
  QStringList m_pairs;
  QString m_pair, m_bank;                       // m_bank: row clicked (own lines) or empty (averages)
  QList<banks::Quarter> m_quarters;
  QList<bool> m_qOn;
  QStringList m_rows;                           // bank ids of the table
  ForecastChart* m_chart;
  double m_last = qQNaN();
  int m_hoverPill = -1;
  bool m_hoverClose = false;
};

// Fixed colour per quarter column (shared by the table header, pills and lines).
QColor quarterColor(int i);
QColor quarterTextColor(int i);
}  // namespace banks::pos
