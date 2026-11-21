#pragma once
#include <QWidget>
#include <QVariantAnimation>
#include <QPainter>
#include <functional>
#include "banks/Forecasts.h"

// Private widgets of Bank Positioning (plain callbacks, no moc).
namespace banks::pos {

// One asset class: icon, title, subtitle, LONG / SHORT / RANGE counters, proportion bar, coverage footer.
// Hover lifts the card 3 px (≈200 ms) and lights the ↗ arrow.
class MarketCard : public QWidget {
public:
  MarketCard(banks::AssetClass c, QWidget* parent);
  void setSummary(const banks::ClassSummary& s) { m_s = s; update(); }
  void setForcedHover(bool on);
  banks::AssetClass assetClass() const { return m_class; }
  std::function<void(banks::AssetClass)> onClick;
  static constexpr int kLift = 3;        // the widget is kLift px taller than the card, room for the lift
protected:
  void paintEvent(QPaintEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
private:
  void animateTo(qreal v);
  banks::AssetClass m_class;
  banks::ClassSummary m_s;
  QVariantAnimation m_anim;
  qreal m_lift = 0;
  bool m_forced = false;   // test hook: stays lifted
};

// ALL CALLS: every bank × market call, grouped by asset class.
class CallsTable : public QWidget {
public:
  explicit CallsTable(QWidget* parent);
  void setCalls(const QList<QPair<banks::AssetClass, QList<banks::Call>>>& groups);
  int groupTop(banks::AssetClass c) const;
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  bool phone() const;                    // < 520 px: two-line rows
  int headH() const;
  int rowH() const;
  void relayout();                       // height from the groups and the row pitch
  void paintPhone(QPainter& p, const QRect& clip);
  QList<QPair<banks::AssetClass, QList<banks::Call>>> m_groups;
  int m_height = 200;
};

// Scrolling body of the DESK view (cards are its children); paints through a callback so the page keeps the text.
class DeskBody : public QWidget {
public:
  explicit DeskBody(QWidget* parent) : QWidget(parent) {}
  std::function<void(QPainter&)> painter;
protected:
  void paintEvent(QPaintEvent*) override;
};

void drawClassIcon(QPainter& p, banks::AssetClass c, const QRectF& r, const QColor& col);
}
