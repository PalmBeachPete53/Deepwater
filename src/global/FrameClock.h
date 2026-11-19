#pragma once
#include <QObject>
#include <QSet>
#include <QTimer>

// One 30 Hz clock for the animations of a page (tape, wire, globe): their frames land in the same repaint pass,
// so the window syncs and flushes once per frame instead of once per animation and timer.
// Runs only while a widget holds it (acquire in showEvent, release in hideEvent).
namespace global {
class FrameClock : public QObject {
  Q_OBJECT
public:
  static FrameClock& instance();
  void acquire(QObject* who);
  void release(QObject* who);
  int frame() const { return m_frame; }
signals:
  void tick(int frame);
private:
  FrameClock();
  void schedule();
  QTimer m_timer;
  QSet<QObject*> m_users;
  QSet<QObject*> m_watched;   // objects whose destroyed() is already connected
  int m_frame = 0;
};
}
