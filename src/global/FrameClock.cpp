#include "global/FrameClock.h"
#include <QDateTime>
#include <cmath>

namespace global {
namespace {
constexpr double kPeriodMs = 1000.0 / 30;
// Frames land on the wall clock's 30ths of a second (+2 ms), the phase of the pages' half-second blinks
// (look::cursorOn, timers started at 500 − now % 500 + 2): a blink then shares its repaint pass with a frame instead
// of costing a pass of its own.
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
}
FrameClock& FrameClock::instance() { static FrameClock* c = new FrameClock; return *c; }
FrameClock::FrameClock() {
  m_timer.setTimerType(Qt::PreciseTimer);
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    emit tick(++m_frame);
    if (!m_users.isEmpty()) schedule();
  });
}
void FrameClock::schedule() {
  const qint64 now = nowMs();
  // next 30th of a second after now (a timer firing a little early never yields two frames in a row)
  const double next = (std::floor((now - 2) / kPeriodMs + 0.5) + 1) * kPeriodMs + 2;
  m_timer.start(qMax(1, int(std::lround(next - now))));
}
void FrameClock::acquire(QObject* who) {
  if (m_users.contains(who)) return;
  m_users.insert(who);
  // one destroyed hook per object (Qt::UniqueConnection does not work with a lambda: acquire runs on every show)
  if (!m_watched.contains(who)) {
    m_watched.insert(who);
    connect(who, &QObject::destroyed, this, [this, who] { m_watched.remove(who); release(who); });
  }
  if (!m_timer.isActive()) schedule();
}
void FrameClock::release(QObject* who) {
  m_users.remove(who);
  if (m_users.isEmpty()) m_timer.stop();
}
}
