#pragma once
#include <QWidget>
#include "news/Calendar.h"

namespace news::pages {

// Right-hand detail panel of News › Calendar: economists' range, banks' own forecasts, consensus / range /
// previous / actual boxes, legend and sources (with links). Opened from a value cell of the table.
class RangePanel : public QWidget {
  Q_OBJECT
public:
  explicit RangePanel(QWidget* parent = nullptr);
  void setEvent(const news::Event& e);
  const news::Event& shownEvent() const { return m_e; }   // (not event(): QWidget::event is a virtual)
  void hoverAt(const QPoint& p) { m_pin = true; updateHover(p); }   // test hook
signals:
  void closeRequested();
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void wheelEvent(QWheelEvent*) override;
private:
  struct Link { QRectF r; QString url; QString full; };
  QList<Link> links() const;
  void updateHover(const QPointF& p);
  news::Event m_e;
  int m_hoverLink = -1;
  bool m_hoverClose = false;
  bool m_pin = false;
  double m_off = 0, m_contentH = 0;
  mutable QList<Link> m_links;
};
}  // namespace news::pages
