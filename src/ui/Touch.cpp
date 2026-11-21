#include "ui/Touch.h"
#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QScroller>
#include <QScrollerProperties>

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
}  // namespace

void install() {
  if (!enabled()) return;
  qApp->installEventFilter(new Grabber(qApp));
}
}  // namespace ui::touch
