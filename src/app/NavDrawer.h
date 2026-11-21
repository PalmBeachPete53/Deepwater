#pragma once
// The phone menu (2026-11-21): the ☰ of the compact header opens this drawer over the page, from the left. It lists
// every main tab with its sub-tabs (the current one marked); a tap on a sub-tab opens it and closes the drawer, a tap
// beside the drawer closes it. The list scrolls (by finger with ui/Touch).
#include <QWidget>

class QScrollArea;
class QVariantAnimation;

namespace app {
class NavList;

class NavDrawer : public QWidget {
  Q_OBJECT
public:
  explicit NavDrawer(QWidget* parent);
  void open(int main, int sub);
  void close();
signals:
  void navigate(int main, int sub);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  int panelWidth() const;
  void place(double t);       // 0 hidden … 1 open
  QWidget* m_panel;
  QScrollArea* m_scroll;
  NavList* m_list;
  QVariantAnimation* m_anim;
  double m_t = 0;
};
}  // namespace app
