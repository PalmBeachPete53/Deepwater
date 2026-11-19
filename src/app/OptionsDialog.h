#pragma once
#include <QDialog>
#include <QList>
#include <QRectF>

// Options (gear at the right of the header, Cmd+,): interface language (English / Français) and theme
// (Deepwater and the nine themes of preview.html, in a 2-column grid). Changes apply immediately and are saved (ui.language, ui.theme).
class OptionsDialog : public QDialog {
  Q_OBJECT
public:
  explicit OptionsDialog(QWidget* parent = nullptr);
  QSize sizeHint() const override { return QSize(640, 612); }
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
private:
  enum Hit { None, Close, LangEn, LangFr, ThemeFirst = 100 };
  struct Area { QRectF r; int id; };
  QList<Area> areas() const;
  int hitAt(const QPointF& p) const;
  int m_hover = None;
  QPointF m_dragFrom; bool m_dragging = false;
};
