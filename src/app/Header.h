#pragma once
#include <QWidget>
#include <QTimer>

// App header: wordmark, six main tabs, clock and the options gear on the first row; sub-tabs of the active tab
// on the second.
class Header : public QWidget {
  Q_OBJECT
public:
  explicit Header(QWidget* parent = nullptr);
  void setCurrent(int main, int sub);
  int currentMain() const { return m_main; }
  int currentSub() const { return m_sub; }
  static constexpr int kHeight = 76;
  struct Hit { QRectF r; int main; int sub; };   // a tab's rectangle (sub < 0: main tab)
signals:
  void navigate(int main, int sub);
  void optionsRequested();
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<Hit> hits() const;
  int m_main = 0, m_sub = 0;
  int m_hoverMain = -1, m_hoverSub = -1;
  bool m_hoverGear = false;
  QRectF gearRect() const;
  QList<int> m_lastSub;     // remembered sub-tab per main tab
  QTimer m_clock;
};
