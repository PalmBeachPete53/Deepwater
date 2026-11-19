#pragma once
#include "pages/market/technical/HeatCells.h"
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>

// Right panel of the heatmap: id line with blinking cursor, name + price, trend pill, six-axis radar (grows from
// the centre on selection change), six read rows and the method footnote. Empty when nothing is selected.
class SignalFile : public QWidget {
  Q_OBJECT
public:
  explicit SignalFile(QWidget* parent = nullptr);
  void setInstrument(const QString& id, const QString& name, const QString& price, const analytics::TechRead& r,
                     bool animate);
  void setPrice(const QString& price);
  void clear();
  bool isEmpty() const { return m_id.isEmpty(); }
  static QString footnote();
  int preferredHeight() const;    // full content height at the current width (the page scrolls the panel below it)
  double radius() const;
protected:
  void paintEvent(QPaintEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  QString m_id, m_name, m_price;
  analytics::TechRead m_read;
  QVector<heat::Cell> m_cells;
  QTimer m_blink, m_anim; bool m_cursor = true;
  QElapsedTimer m_grow; bool m_growing = false;
};
