#pragma once
#include <QWidget>
#include <QPointer>
#include "positioning/Retail.h"
#include "market/Candles.h"
class QVariantAnimation;
class QTimer;

namespace positioning {
namespace pk { class Scroll; class Segmented; }
namespace pos { class CandleChart; }

// Instrument detail modal of Retail Sentiment: average retail entries, 1 h candles (Yahoo Finance), 3D long/short
// pie, statistics (Myfxbook figures + values computed by Deepwater from the live quote), history charts from the
// app's own recordings, CFTC open interest of the matching contract, and a generated narrative.
class RetailModal : public QWidget {
  Q_OBJECT
public:
  explicit RetailModal(QWidget* page);
  void open(const QString& symbol);
  void close();
  QString symbol() const { return m_symbol; }
  void setHover(const QPoint& pagePoint);   // test hook
  void setScroll(int px);                   // test hook
  void refreshData();                       // new Myfxbook snapshot
signals:
  void closed();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  QRect box() const;
  void loadChart();
  void applyChart(const market::Series& s, const QString& interval, int days);
  QString m_symbol;
  int m_chartReq = 0;               // chart request generation: a late answer for an older range or symbol is dropped
  class RetailModalHeader* m_header;
  pk::Scroll* m_scroll;
  class RetailModalBody* m_body;
  QTimer* m_clock;
};
}  // namespace positioning
