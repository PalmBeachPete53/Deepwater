#pragma once
#include <QWidget>
#include "app/Preload.h"
#include <QTimer>

class QScrollArea;

namespace economy {
class RatesBody;

// Economy › Rates: interest-rate probabilities of the 8 central banks, implied path, next-meeting odds,
// odds history, Polymarket status, committees and votes. One framed panel; the body scrolls under the title bar.
// Dev hooks (env): DW_RATES_SCROLL=px, DW_RATES_HOVER=x,y (page coordinates), DW_RATES_BANK=FED|ECB|BOC…
class RatesPage : public QWidget, public app::Preloadable {
  Q_OBJECT
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit RatesPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
private:
  QRectF iconRect(int i) const;
  QScrollArea* m_scroll;
  RatesBody* m_body;
  bool m_collapsed = false, m_hooksDone = false;
  int m_hoverIcon = -1;
};
}  // namespace economy
