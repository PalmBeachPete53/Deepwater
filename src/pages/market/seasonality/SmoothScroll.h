#pragma once
#include <QScrollArea>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QEasingCurve>

// Scroll area of the Market pages: no visible scroll bar (the reference shows none), eased wheel scrolling
// (≈0.45 s) for mouse wheels, direct pixel scrolling for trackpads. Shared by Seasonality and Fundamental Heatmap.
namespace seasonality {
class SmoothScroll : public QScrollArea {
public:
  explicit SmoothScroll(QWidget* parent) : QScrollArea(parent) {
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWidgetResizable(true);
    m_anim.setDuration(450);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&m_anim, &QVariantAnimation::valueChanged, this,
                     [this](const QVariant& v) { verticalScrollBar()->setValue(v.toInt()); });
  }
  void scrollTo(int y, bool animated = true) {
    y = qBound(0, y, verticalScrollBar()->maximum());
    if (!animated) { m_anim.stop(); verticalScrollBar()->setValue(y); return; }
    m_anim.stop(); m_anim.setStartValue(verticalScrollBar()->value()); m_anim.setEndValue(y); m_target = y; m_anim.start();
  }
protected:
  void wheelEvent(QWheelEvent* e) override {
    auto* sb = verticalScrollBar();
    if (!e->pixelDelta().isNull()) { m_anim.stop(); sb->setValue(sb->value() - e->pixelDelta().y()); e->accept(); return; }
    const int base = m_anim.state() == QAbstractAnimation::Running ? m_target : sb->value();
    scrollTo(base - e->angleDelta().y() * 120 / 120);
    e->accept();
  }
private:
  QVariantAnimation m_anim;
  int m_target = 0;
};
}
