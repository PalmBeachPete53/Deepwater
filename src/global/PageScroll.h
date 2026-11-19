#pragma once
#include <QScrollArea>
#include <functional>

// Vertical page scroller for the Global pages: no visible scroll bars (wheel / trackpad only, like the other
// pages), no frame, transparent. The content widget is laid out by the page through `layout(width, viewportH)`,
// which returns the content height (≥ viewportH); below the reference height the page then scrolls.
namespace global {
class PageScroll : public QScrollArea {
public:
  explicit PageScroll(QWidget* content, QWidget* parent);
  std::function<int(int width, int viewportHeight)> layout;   // returns the content height
  void relayout();
protected:
  void resizeEvent(QResizeEvent* e) override;
};
}
