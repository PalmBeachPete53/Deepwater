#pragma once
#include <QWidget>
#include <QStringList>
#include <QVector>
#include <QDate>
#include <functional>
#include "banks/Research.h"

class QScrollArea;
class QLabel;
class QVariantAnimation;

// Private widgets of the Bank Research page (no signals: plain callbacks, so no moc is needed).
namespace banks::res {

// Shared blink state for the block cursors and gold dots (≈1 s period).
bool blinkOn();
void setBlink(bool on);
void drawBlockCursor(class QPainter& p, const QPointF& topLeft, qreal w = 7, qreal h = 13);

// Page strip: icon · dot · GLOBAL BANKING NETWORK — LIVE SURVEILLANCE ▮   [• 21 INSTITUTIONS · 3 REGIONS · SEC EDGAR]
class Strip : public QWidget {
public:
  explicit Strip(QWidget* parent);
  void setBadge(const QString& b) { m_badge = b; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QString m_badge;
};

// Header-style dropdown: "ALL BANKS ⌄", opens a menu.
class DropButton : public QWidget {
public:
  explicit DropButton(QWidget* parent);
  void setItems(const QStringList& labels, const QStringList& keys);
  void setCurrentKey(const QString& key);
  QString currentKey() const { return m_keys.value(m_cur); }
  std::function<void(const QString& key)> onChanged;
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QStringList m_labels, m_keys;
  int m_cur = 0;
  bool m_hover = false;
};

// Painted list of notes grouped by day (TODAY, YESTERDAY, dates).
class NoteList : public QWidget {
public:
  explicit NoteList(QWidget* parent);
  void setNotes(const QList<banks::Note>& notes);
  void setEmptyText(const QString& title, const QString& body, const QString& linkText, const QString& linkUrl);
  void setSelected(const QString& id) { m_selected = id; update(); }
  void setForcedHover(int row) { m_forcedHover = row; update(); }
  int rowTop(int row) const;
  QString noteAt(int row) const { return row >= 0 && row < m_notes.size() ? m_notes[row].id : QString(); }
  std::function<void(const QString& noteId)> onOpen;
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  struct Item { int note = -1; QString group; int y = 0, h = 0; QStringList lines; };
  void relayout();
  int itemAt(const QPointF& p) const;
  QList<banks::Note> m_notes;
  QVector<Item> m_items;
  QString m_selected;
  int m_hover = -1, m_pressed = -1, m_forcedHover = -1;
  QString m_emptyTitle, m_emptyBody, m_emptyLink, m_emptyUrl;
  QRectF m_linkRect;
  QDate m_layoutDay;                     // the day the group labels (TODAY / YESTERDAY) were computed for
};

// Left column: tabs RESEARCH / NEWS / DATA / FILINGS, header band, dropdowns, list, sources footer.
class LeftColumn : public QWidget {
public:
  explicit LeftColumn(QWidget* parent);
  void reload();                         // re-read the services
  void setBankFilter(const QString& bankId);
  void setTab(int t);
  void setSelected(const QString& noteId) { m_list->setSelected(noteId); }
  NoteList* list() const { return m_list; }
  QScrollArea* scroll() const { return m_scroll; }
  std::function<void(const QString& noteId)> onOpen;
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
private:
  QRectF tabRect(int i) const;
  int footerHeight() const;
  QStringList footerLines() const;
  void refill();
  void fillData();
  void fillFilings();
  int m_tab = 0, m_tabHover = -1;
  QString m_bank, m_type;
  QString m_shownView;                   // bank|type|tab of the list on screen (refill keeps the place when unchanged)
  DropButton* m_banks;
  DropButton* m_types;
  QScrollArea* m_scroll;
  NoteList* m_list;
  QScrollArea* m_textScroll;
  QLabel* m_text;
  QString m_footer;
  QVariantAnimation* m_wheel;
};

// Right column: BANK REGISTRY — SYSTEMIC INSTITUTIONS.
class RegistryColumn : public QWidget {
public:
  explicit RegistryColumn(QWidget* parent);
  void setSelected(const QString& bankId) { m_selected = bankId; update(); }
  std::function<void(const QString& bankId)> onClick;
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool event(QEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  int rowAt(const QPointF& p) const;
  qreal maxOffset() const;
  QString m_selected;
  int m_hover = -1;
  qreal m_off = 0;                       // vertical scroll of the rows (short windows)
  QVariantAnimation* m_wheel = nullptr;
};
}
