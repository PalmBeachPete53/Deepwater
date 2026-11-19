#pragma once
#include <QWidget>
#include <QTimer>
#include <QHash>
#include "news/Calendar.h"
#include "pages/news/calendar/HistoryCard.h"
class QPainter;

namespace news::pages {

// The week table of News › Calendar: sticky column header and day headers, rows, inline expansion with the
// indicator description and its release history card. Painted in one widget (own smooth scrolling).
class CalendarTable : public QWidget {
  Q_OBJECT
public:
  enum Col { Time, Currency, EventCol, Actual, Forecast, Previous, Surprise, Min, Max };
  explicit CalendarTable(QWidget* parent = nullptr);
  void setEvents(const QList<news::Event>& events);
  void setEmptyNote(const QString& note) { m_note = note; update(); }
  void setFocusCell(const QString& key, int col) { m_focusKey = key; m_focusCol = col; update(); }
  void toggleExpand(const QString& key);
  QString expandedKey() const { return m_expanded; }
  void scrollToKey(const QString& key);          // bring a row near the top
  void scrollBy(double dy) { setTarget(m_target + dy); }
  void setScroll(double y);                      // immediate
  // test hooks
  void hoverAt(const QPoint& p);
  void hoverChart(double fraction);
  void setChartMode(int mode, int span);

signals:
  void valueClicked(const news::Event& e, int col);

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void mouseDoubleClickEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void leaveEvent(QEvent*) override;

private:
  struct Item { enum Kind { Group, Row, Expand } kind; double y = 0, h = 0; int ev = -1; QDate day; bool first = false; };
  void relayout();
  double rightOf(int col) const;                 // right edge of numeric columns (-1000 when hidden)
  bool shown(int col) const;                     // narrow tables hide MIN/MAX, then SURPRISE (values in tooltips)
  double scale() const;                          // column width factor (1 at the reference, 0.8…1.6)
  double proseWidth() const { return std::min(width() - 28.0, 1200.0); }   // description lines ≤ 1200 px
  QRectF cellRect(const Item& it, int col) const;   // in content coordinates
  QRectF cardRect(const Item& exp) const;        // content coordinates
  QStringList descParas(const news::Event& e) const;
  double contentHeight() const { return m_items.isEmpty() ? 0 : m_items.last().y + m_items.last().h; }
  double maxScroll() const;
  void setTarget(double y);
  int itemAt(double contentY) const;
  void paintRow(QPainter& p, const Item& it, double y, bool pinned) const;
  void paintGroup(QPainter& p, const QDate& day, double y, bool sticky) const;
  void paintExpand(QPainter& p, const Item& it, double y) const;
  void updateHover(const QPointF& pos);
  const Item* expandItem() const;

  static constexpr int kHeaderH = 34;            // column header (with its borders)
  static constexpr int kRowH = 37;
  static constexpr int kStickyH = 36;

  QList<news::Event> m_events;
  QList<Item> m_items;
  QString m_note;
  double m_scroll = 0, m_target = 0;
  QTimer m_anim;
  int m_hoverItem = -1, m_hoverCol = -1;
  QString m_expanded;
  HistoryCard m_card;
  bool m_cardPress = false;
  QString m_focusKey; int m_focusCol = -1;
  bool m_pin = false;                            // hover set by the test hook survives leave events
};
}  // namespace news::pages
