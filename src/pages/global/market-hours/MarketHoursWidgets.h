#pragma once
#include <QWidget>
#include <QRegion>
#include <QTimer>
#include <QPixmap>
#include <QDateTime>
#include <QElapsedTimer>

// Private widgets of Global › Market Hours (all painted, no layouts).
namespace hours {

// "YOUR CLOCK · <ZONE>": analogue clock on a 24 h session dial with a radar sweep, digital time, date, status.
class ClockPanel : public QWidget {
public:
  explicit ClockPanel(QWidget* parent);
  void setNow(const QDateTime& utc);    // called every second
  QRect cursorRect() const;
  void updateRect(const QRect& r);       // repaint r in the coming pass, as one rectangle with the rest
  void blinkCursor();                    // the title cursor's half-second repaint
  void paintLayersAt(QPainter& p) const; // dial layers + cursor (no wedge), for the cursor child
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void geometryFor(QPointF& c, qreal& s) const;
  void rebuild();                        // static layers (face, dial, arcs) into pixmaps
  void rebuildTop();                     // dial + hands + digital time, once per second
  void scheduleSweep();                  // next sweep frame on the wall clock's 20ths of a second
  void paintCursor(QPainter& p) const;
  QWidget* m_cursor = nullptr;           // ClockCursor
  QRect secondRect() const;              // what a new second repaints (hands, digital time)
  QRect m_dirty;                         // what this pass repaints so far (updateRect)
  QDateTime m_utc;
  QString m_arcKey;                      // minute + live states: rebuild arcs when it changes
  QPixmap m_under, m_over, m_top;
  QRectF m_lastWedge;
  QTimer m_sweep;
  QElapsedTimer m_clock;
};

// "24-HOUR TIMELINE · YOUR TIME": one row per session with today's bars, overlaps row, now line.
class TimelinePanel : public QWidget {
public:
  explicit TimelinePanel(QWidget* parent);
  void setNow(const QDateTime& utc);
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QDateTime m_utc;
};

// One session card: name, state pill, centre time, countdown, progress, hours local / your time.
class SessionCard : public QWidget {
public:
  SessionCard(int session, QWidget* parent);
  void setNow(const QDateTime& utc);
protected:
  void paintEvent(QPaintEvent*) override;
private:
  int m_idx;
  QDateTime m_utc;
};
}
