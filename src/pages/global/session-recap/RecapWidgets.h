#pragma once
#include "pages/global/session-recap/RecapModel.h"
#include <QAbstractScrollArea>
#include <QWidget>

class QVariantAnimation;

namespace wire::recap {

// Smooth wheel scrolling shared by the three columns (≈100 px per notch, animated 150 ms, free).
class SmoothScroll : public QAbstractScrollArea {
  Q_OBJECT
public:
  explicit SmoothScroll(QWidget* parent = nullptr);
  void scrollBy(int dy);
  void setContentHeight(int h);
  int contentHeight() const { return m_h; }
  int offset() const;
  // phone page: a box shown whole (nothing to scroll) hands the wheel to the page's scroll instead of eating it
  void setPassWheel(bool on) { m_passWheel = on; }
protected:
  void wheelEvent(QWheelEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  QVariantAnimation* m_anim;
  int m_target = 0, m_h = 0;
  bool m_passWheel = false;
};

// Left column list: day groups of sessions (or weeks) with their top mover.
class RegistryList : public SmoothScroll {
  Q_OBJECT
public:
  explicit RegistryList(QWidget* parent = nullptr);
  struct Row { bool header = false; QString label; Window w; QString line2; double chg = NAN; };
  void setRows(const QList<Row>& rows);
  void setSelected(const QString& key);
signals:
  void picked(const wire::recap::Window& w);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  int rowAt(int y) const;
  QList<Row> m_rows;
  QVector<int> m_y;
  QString m_sel;
  int m_hover = -1;
};

// Header band + SESSIONS / WEEKS tabs above the registry list.
class RegistryHead : public QWidget {
  Q_OBJECT
public:
  explicit RegistryHead(QWidget* parent = nullptr);
  void setTab(int t) { m_tab = t; update(); }
  void setBlink(bool on) { m_blink = on; update(0, 0, width(), 36); }
signals:
  void tabChanged(int t);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  int m_tab = 0, m_hover = -1;
  bool m_blink = true;
};

// Right column: RELEASES IN THE WINDOW (top) and WIRE IN THE WINDOW (rest, scrolls).
class ReleasesBox : public SmoothScroll {
  Q_OBJECT
public:
  explicit ReleasesBox(QWidget* parent = nullptr);
  void setData(const QList<wire::Release>& r, bool connected);
  int preferredHeight() const;
protected:
  void paintEvent(QPaintEvent*) override;
  bool viewportEvent(QEvent*) override;   // tooltip with the full event name (elided in narrow columns)
private:
  QList<wire::Release> m_rel;
  bool m_connected = false;
};
class WireBox : public SmoothScroll {
  Q_OBJECT
public:
  explicit WireBox(QWidget* parent = nullptr);
  void setData(const QList<wire::Item>& items, const QString& empty);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool viewportEvent(QEvent*) override;
private:
  int rowAt(int y) const;
  QList<wire::Item> m_items;
  QStringList m_tags;
  QVector<int> m_y;
  QString m_empty;
  int m_hover = -1;
};
// Column header band ("RELEASES IN THE WINDOW ……… UTC-5").
void columnHeader(class QPainter& p, const QRect& r, const QString& title, const QString& right);
}
