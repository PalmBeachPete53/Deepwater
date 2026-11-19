#pragma once
#include <QDateTime>
#include "wire/ReleaseSource.h"
#include <QAbstractScrollArea>
#include <QDate>
#include <QSet>
#include <QTimer>
#include <QWidget>

class QLineEdit;
class QVariantAnimation;

namespace wire::squawk {
class Footer;

// Rows of the compact calendar (day headers + events), countdowns ticking every second.
class CalendarRows : public QAbstractScrollArea {
  Q_OBJECT
public:
  explicit CalendarRows(QWidget* parent = nullptr);
  struct Row { bool day = false; QDate date; wire::Release ev; };
  void setRows(const QList<Row>& rows, const QString& empty);
  void scrollToFirstUpcoming();
  void scrollBy(int dy);
  void setHoverAt(const QPoint& p);
  void tick();                     // every second: the countdowns (the whole list when the next event changes)
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool viewportEvent(QEvent*) override;
private:
  int rowHeight(int i) const;
  void relayout();
  int rowAt(int y) const;
  QList<Row> m_rows;
  QVector<int> m_y;
  QSet<QString> m_expanded;
  int m_hover = -1;
  int m_eventH = 59;               // 59 px (reference), 46 px on short windows
  QString m_empty;
  QVariantAnimation* m_anim;
  int m_target = 0;
  QDateTime m_next;                // the next upcoming event at the last tick
  QDate m_today;                   // the local date at the last tick: a new day repaints the "TODAY" badge
};

// Compact economic calendar under the TV: search, week ‹ ›, country chips, impact chips, rows.
// Fed by wire::releaseSource() (the News package's calendar). Without it: quiet "calendar connecting" state.
class CalendarPane : public QWidget {
  Q_OBJECT
public:
  explicit CalendarPane(QWidget* parent = nullptr);
  void setRunning(bool on);
  CalendarRows* rows() const { return m_rows; }
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  void connectSource();
  void rebuild();
  QDate weekStart() const { return m_week; }
  QList<QRectF> countryRects() const;
  QRectF moreRect() const;
  QList<QRectF> impactRects() const;
  QRectF prevRect() const;
  QRectF nextRect() const;
  QList<wire::Release> weekEvents() const;
  QLineEdit* m_search;
  CalendarRows* m_rows;
  Footer* m_footer;
  QDate m_week;                    // Sunday of the shown week (local)
  QSet<QString> m_countries;       // selected currencies
  QStringList m_extra;             // currencies seen in the data beyond the 8 majors
  int m_impact = 0;                // 0 all, 1 high, 2 medium, 3 low
  QList<int> m_counts{-1, -1, -1, -1};
  int m_hover = -1;                // 0..7 countries, 8 more, 10..13 impact, 20 prev, 21 next
  bool m_scrolled = false;
  bool m_userWeek = false;         // the user navigated (no weekend roll-over)
  QTimer m_tick;
};
}
