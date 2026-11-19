#pragma once
#include "wire/Wire.h"
#include <QAbstractScrollArea>
#include <QHash>
#include <QPixmap>
#include <QWidget>
#include <QElapsedTimer>
#include <QTimer>

class QVariantAnimation;

namespace wire::squawk {

// Title row: ((•)) • SQUAWK [LIVE] subtitle …… [文A EN] 465 ITEMS
class Header : public QWidget {
  Q_OBJECT
public:
  explicit Header(QWidget* parent = nullptr);
  void setCount(int n) { m_count = n; update(); }
  void setLanguage(const QString& lang, bool available);
signals:
  void languageToggled();
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QRectF langRect() const;
  int m_count = 0;
  QString m_lang = "EN";
  bool m_langAvailable = false, m_hoverLang = false;
};

// Endless marquee of live price moves (right to left, 48 px/s, ≤ 30 fps, only while visible).
class Ticker : public QWidget {
  Q_OBJECT
public:
  explicit Ticker(QWidget* parent = nullptr);
  void setMoves(const QList<wire::Move>& moves);
  void setRunning(bool on);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  void rebuild();
  QList<wire::Move> m_moves;
  QPixmap m_strip;          // one cycle of items, painted once per data change
  qreal m_offset = 0;
  QTimer m_timer;
  QElapsedTimer m_clock;
  qint64 m_lastMs = 0;
  QString m_empty;
};

// ALL n | DATA n | ⚡ MOVERS n | RELEASES n
class FilterTabs : public QWidget {
  Q_OBJECT
public:
  explicit FilterTabs(QWidget* parent = nullptr);
  void setCounts(const QList<int>& c) { m_counts = c; update(); }
  void setCurrent(int i) { m_cur = i; update(); }
  void setHoverAt(const QPoint& p);              // test hook DW_SQUAWK_HOVER
  int current() const { return m_cur; }
signals:
  void changed(int i);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<QRectF> rects() const;
  QList<int> m_counts{-1, -1, -1, -1};
  int m_cur = 1, m_hover = -1;
};

// The wire list: painted rows (time, age, source coin, wrapped headline), smooth wheel scroll, hover.
class WireList : public QAbstractScrollArea {
  Q_OBJECT
public:
  explicit WireList(QWidget* parent = nullptr);
  void setItems(const QList<wire::Item>& items);
  void setTranslations(const QHash<QString, QString>* tr) { m_tr = tr; m_heights.clear(); relayout(); }
  void setEmptyText(const QString& t) { m_empty = t; viewport()->update(); }
  void refreshAges() { viewport()->update(); }
  void scrollToTop() { scrollTo(0); }
  void scrollBy(int dy);    // smooth, like a wheel gesture (test hook DW_SQUAWK_SCROLL)
  void setHoverAt(const QPoint& viewportPos);   // test hook DW_SQUAWK_HOVER
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool viewportEvent(QEvent*) override;
private:
  QString textOf(const wire::Item& i) const;
  void relayout();
  int rowAt(int y) const;
  void scrollTo(int v);
  QList<wire::Item> m_items;
  QVector<int> m_y;           // top of each row (content coords), size n+1
  QHash<QString, int> m_heights;
  int m_layoutWidth = -1;
  int m_hover = -1;
  const QHash<QString, QString>* m_tr = nullptr;
  QVariantAnimation* m_anim;
  int m_target = 0;
  QString m_empty;
};

// "Terminal Data · HH:MM:SS" ……… [● LIVE]
class Footer : public QWidget {
  Q_OBJECT
public:
  explicit Footer(QWidget* parent = nullptr);
  void setStamp(const QDateTime& t, bool live);
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QDateTime m_t; bool m_live = false;
};
}
