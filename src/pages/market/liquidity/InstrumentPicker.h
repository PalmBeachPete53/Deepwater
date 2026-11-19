#pragma once
#include <QWidget>
#include <QStringList>

namespace mkit { class SearchBox; }

// Instrument button ("EUR/USD ⌄") and its dropdown (search field + scrollable list with asset-class tags).
// The dropdown is a child of `host` (the page) drawn above everything; it closes on a pick, Escape, a second
// click on the button or a click outside it.
class InstrumentButton : public QWidget {
  Q_OBJECT
public:
  explicit InstrumentButton(QWidget* parent = nullptr);
  void setText(const QString& t);
  void setOpen(bool o);
  QSize sizeHint() const override;
signals:
  void clicked();
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QString m_text; bool m_open = false, m_hover = false;
};

class InstrumentList : public QWidget {
  Q_OBJECT
public:
  explicit InstrumentList(QWidget* parent = nullptr);
  void setFilter(const QString& f);
  void setCurrent(const QString& symbol);
  void scrollToCurrent();
signals:
  void picked(const QString& symbol);
protected:
  void paintEvent(QPaintEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  struct Item { QString symbol, label, tag; };
  QList<Item> m_all, m_items;
  QString m_current; int m_hover = -1; double m_scroll = 0;
};

class InstrumentDropdown : public QWidget {
  Q_OBJECT
public:
  explicit InstrumentDropdown(QWidget* host);
  void popup(const QPoint& at, const QString& current);
  void dismiss();
signals:
  void picked(const QString& symbol);
  void closed();
protected:
  void paintEvent(QPaintEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
private:
  mkit::SearchBox* m_search; InstrumentList* m_list;
public:
  QWidget* button = nullptr;    // clicks on it are left to the button
};
QString instrumentLabel(const QString& symbol);
