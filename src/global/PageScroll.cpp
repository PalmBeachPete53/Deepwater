#include "global/PageScroll.h"
#include <QScrollBar>
#include <QTimer>

namespace global {
PageScroll::PageScroll(QWidget* content, QWidget* parent) : QScrollArea(parent) {
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidgetResizable(false);
  viewport()->setAutoFillBackground(false);
  setStyleSheet("QScrollArea{background:transparent;border:none;}");
  verticalScrollBar()->setSingleStep(40);
  setWidget(content);
}
void PageScroll::relayout() {
  const int w = viewport()->width(), vh = viewport()->height();
  const int h = layout ? qMax(vh, layout(w, vh)) : vh;
  widget()->resize(w, h);
  // test hook: DW_PAGE_SCROLL=<px> scrolls the page content (check the parts below the fold on small windows)
  static const int hook = qEnvironmentVariableIntValue("DW_PAGE_SCROLL");
  if (hook > 0) QTimer::singleShot(300, this, [this] { verticalScrollBar()->setValue(hook); });
}
void PageScroll::resizeEvent(QResizeEvent* e) {
  QScrollArea::resizeEvent(e);
  relayout();
}
}
