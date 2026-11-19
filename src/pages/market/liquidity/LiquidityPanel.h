#pragma once
#include "analytics/Liquidity.h"
#include <QWidget>
#include <QVariantAnimation>
#include <QTimer>

// Bottom panel of the Liquidity page: tabs LIQUIDITY POOLS (all pools by price with a LAST band at the current
// price), SWEEP ALERTS (swept pools, most recent first) and SESSION EXTREMES (UTC session highs/lows). Its height
// follows the content (the page scrolls).
class LiquidityPanel : public QWidget {
  Q_OBJECT
public:
  explicit LiquidityPanel(QWidget* parent = nullptr);
  void setData(const QVector<analytics::Pool>& pools, const QVector<market::Bar>& bars,
               const analytics::SessionTable& sessions, int decimals);
  void setLastPrice(double p);
  void setTab(int t);
  int tab() const { return m_tab; }
  int contentHeight() const;
signals:
  void heightChanged();
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QRectF tabRect(int i) const;
  void paintPools(QPainter& p, double y0, double w, int from, int to);
  void paintSweeps(QPainter& p, double y0, double w, int from, int to);
  int columns() const;
  void paintSessions(QPainter& p, double y0);
  QVector<analytics::Pool> m_pools, m_swept;
  QVector<qint64> m_sweptTime;
  analytics::SessionTable m_sessions;
  int m_dec = 5, m_tab = 0, m_hoverTab = -1;
  double m_last = NAN;
  QVariantAnimation m_focus; double m_focusA = 0; QTimer m_focusDelay;
};
