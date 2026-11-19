#pragma once
#include <QWidget>
#include <QColor>
#include <QList>
#include <QVector>
#include <QVariantAnimation>
#include "market/Candles.h"

namespace banks::pos {

// Daily candles with dashed horizontal forecast lines (one per quarter) and the last price tag.
// Kept free of page logic so it can be swapped for a shared chart widget later: feed it bars, lines and a price.
//   chart->setBars(series.bars, 4);
//   chart->setLines({{"Q4 '26 1.1300", 1.13, QColor("#fcd400")}, …});
//   chart->setLast(1.1204);
//   chart->animateIn();      // lines draw from left to right (≈0.5 s)
class ForecastChart : public QWidget {
public:
  struct Line { QString label; double value; QColor color; };
  explicit ForecastChart(QWidget* parent = nullptr);
  void setBars(const QVector<::market::Bar>& bars, int decimals);
  void setLines(const QList<Line>& lines);
  void setLast(double price);
  void setMessage(const QString& m) { m_msg = m; update(); }
  void animateIn();
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QRectF plotRect() const;
  QVector<::market::Bar> m_bars;
  QList<Line> m_lines;
  double m_last = qQNaN();
  int m_dec = 4;
  qreal m_progress = 1.0;
  QVariantAnimation m_anim;
  QString m_msg;
};
}  // namespace banks::pos
