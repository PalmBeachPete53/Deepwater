#pragma once
#include <QWidget>
#include <QTimer>
#include <QDateTime>

namespace global { class PageScroll; }

namespace hours {
class ClockPanel; class TimelinePanel; class SessionCard; class HoursCanvas;

// Global › Market Hours: the four FX sessions on the user's own clock (analogue 24 h dial with session arcs,
// 24-hour timeline with today's sessions and overlaps, one card per session with its countdown).
// Everything is computed locally (global::sessions(), tz database). Test hooks: DW_TZ, DW_NOW (see Sessions.h).
class MarketHoursPage : public QWidget {
public:
  explicit MarketHoursPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void tick();
  int layoutChildren(int width, int viewportH);
  QRect badgeRect() const;
  QTimer m_timer;
  qint64 m_lastSec = -1;
  int m_lastLive = -1;
  ClockPanel* m_clock;
  TimelinePanel* m_timeline;
  QList<SessionCard*> m_cards;
  int m_cardsTop = 627;
  HoursCanvas* m_canvas;
  global::PageScroll* m_scroll;
};
}  // namespace hours
