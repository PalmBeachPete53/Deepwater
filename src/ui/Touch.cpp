#include "ui/Touch.h"
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTextEdit>
#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QScroller>
#include <QScrollerProperties>
#include <memory>

namespace ui::touch {

bool enabled() {
#ifdef Q_OS_IOS
  return true;
#else
  static const bool sim = qEnvironmentVariable("DW_TOUCH_SIM") == "1";
  return sim;
#endif
}

namespace {
// Gives each scroll area's viewport a scroller the first time it is shown.
class Grabber : public QObject {
public:
  using QObject::QObject;
protected:
  bool eventFilter(QObject* o, QEvent* e) override {
    if (e->type() != QEvent::Show) return false;
    auto* area = qobject_cast<QAbstractScrollArea*>(o);
    if (!area || area->property("dw_touch").toBool()) return false;
    area->setProperty("dw_touch", true);
    QWidget* vp = area->viewport();
#ifdef Q_OS_IOS
    QScroller::grabGesture(vp, QScroller::TouchGesture);
#else
    QScroller::grabGesture(vp, QScroller::LeftMouseButtonGesture);
#endif
    QScrollerProperties sp = QScroller::scroller(vp)->scrollerProperties();
    // no rubber band past the ends (the app's pages paint their own background), a short drag before scrolling so a
    // tap stays a tap
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    sp.setScrollMetric(QScrollerProperties::DragStartDistance, 0.004);   // metres: about 4 mm
    QScroller::scroller(vp)->setScrollerProperties(sp);
    return false;
  }
};
// Taps versus scrolls (iOS, 2026-11-21): many widgets act on the mouse press (a heatmap row opens its chart, a list
// item is picked, a text view starts a selection), and on a phone the finger that lands to scroll presses first. Inside
// a finger-scrolled area the press is held back: it is delivered, followed by the release, when the finger lifts
// without having moved (a tap); a finger that moves (a scroll), or that lands while the area is still gliding (a tap
// that stops the glide, as iOS does), presses nothing. Sliders, scroll bars and text fields keep their press: they
// are dragged or edited where they are. Not on a Mac with DW_TOUCH_SIM: there the scroller itself is driven by the
// mouse and holds presses back the same way.
class Taps : public QObject {
public:
  using QObject::QObject;
protected:
  bool eventFilter(QObject* o, QEvent* e) override {
    const auto t = e->type();
    if (m_replaying || (t != QEvent::MouseButtonPress && t != QEvent::MouseMove && t != QEvent::MouseButtonRelease &&
                        t != QEvent::MouseButtonDblClick))
      return false;
    auto* w = qobject_cast<QWidget*>(o);
    auto* me = static_cast<QMouseEvent*>(e);
    if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonDblClick) {
      if (m_state != Idle) { m_state = Idle; m_press.reset(); }   // a press whose release never came (a cancelled touch)
      QAbstractScrollArea* area = w ? scrolledArea(w) : nullptr;
      if (!area || keepsPress(w)) return false;
      if (QScroller::hasScroller(area->viewport()) && QScroller::scroller(area->viewport())->state() == QScroller::Scrolling) {
        m_state = Dropped;                                      // the tap stops the glide, and presses nothing
        return true;
      }
      m_state = Held;
      m_target = w;
      m_press = std::make_unique<QMouseEvent>(t, me->position(), me->scenePosition(),
                                              me->globalPosition(), me->button(), me->buttons(), me->modifiers(),
                                              me->pointingDevice());
      return true;
    }
    if (m_state == Idle) return false;
    if (t == QEvent::MouseMove) {
      if (m_state == Held && (me->globalPosition() - m_press->globalPosition()).manhattanLength() > kSlop) m_state = Dropped;
      return true;                                              // nothing hovers or drags under a held or dropped press
    }
    // release
    const bool tap = m_state == Held && m_target;
    m_state = Idle;
    if (!tap) { m_press.reset(); return true; }
    m_replaying = true;
    QCoreApplication::sendEvent(m_target, m_press.get());
    m_replaying = false;
    m_press.reset();
    return false;                                               // the release follows the press (or double click) it now has
  }
private:
  static constexpr qreal kSlop = 10;                            // points the finger may wander and still tap
  enum State { Idle, Held, Dropped };
  // the finger-scrolled area this widget is in (its viewport, or a widget inside it), null when none
  static QAbstractScrollArea* scrolledArea(QWidget* w) {
    for (QWidget* p = w; p; p = p->parentWidget()) {
      auto* area = qobject_cast<QAbstractScrollArea*>(p->parentWidget());
      if (area && area->viewport() == p && area->property("dw_touch").toBool()) return area;
      if (p->isWindow()) break;
    }
    return nullptr;
  }
  static bool keepsPress(QWidget* w) {
    for (QWidget* p = w; p && !p->isWindow(); p = p->parentWidget()) {
      if (qobject_cast<QAbstractSlider*>(p) || qobject_cast<QLineEdit*>(p)) return true;
      if (auto* te = qobject_cast<QTextEdit*>(p); te && !te->isReadOnly()) return true;
    }
    return false;
  }
  State m_state = Idle;
  QPointer<QWidget> m_target;
  std::unique_ptr<QMouseEvent> m_press;
  bool m_replaying = false;
};
}  // namespace

void install() {
  if (!enabled()) return;
  qApp->installEventFilter(new Grabber(qApp));
#ifdef Q_OS_IOS
  qApp->installEventFilter(new Taps(qApp));
#endif
}
}  // namespace ui::touch
