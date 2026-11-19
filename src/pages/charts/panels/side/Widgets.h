#pragma once
// Charts tab — side panel building blocks: the watchlist rows (.wl-row, painted), cards (.card) and the block
// stack that reproduces the CSS vertical margins (collapsing) of the side tab bodies
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include <QFrame>
#include <QList>
#include <QString>
#include <QWidget>
#include <functional>

class QVBoxLayout;
class QLabel;

namespace charts::ui::side {

// <div id="wlRows">: rows "TICKER EXCH | price | change | ×" (grid 1fr auto 64px 18px, gap 8, padding 6px 10px).
class WatchRows : public QWidget {
  Q_OBJECT
public:
  struct Row {
    QString sym, ticker, exchange, price, change;
    bool up = false, current = false;
  };
  explicit WatchRows(QWidget* parent = nullptr);
  void setRows(const QList<Row>& rows);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override { return {0, sizeHint().height()}; }
signals:
  void picked(const QString& sym);     // row click (JS app.setSymbol)
  void removed(const QString& sym);    // × click
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  int rowAt(int y) const;
  QRect xRect(int row) const;
  QList<Row> rows_;
  int hover_ = -1;
  bool hoverX_ = false;
};

// A vertical stack of blocks with CSS margins: the gap between two blocks is max(previous bottom, next top) like
// collapsing margins; hMargin insets a block (a .card has margin: 8px on every side).
class Blocks : public QWidget {
  Q_OBJECT
public:
  explicit Blocks(QWidget* parent = nullptr);
  void add(QWidget* w, int top = 0, int bottom = 0, int hMargin = 0);
  void addCard(QWidget* card) { add(card, 8, 8, 8); }
  void clear();                         // removes and deletes every block
  void finish();                        // the last block's bottom margin, then a stretch
private:
  QVBoxLayout* lay_;
  int lastBottom_ = -1;                 // -1: nothing added yet
};

// .card: bordered box on --panel, padding 8px 10px. `link`: a.card.news (border turns accent on hover).
// Clickable cards (cal-row, news) emit clicked().
class Card : public QFrame {
  Q_OBJECT
public:
  explicit Card(QWidget* parent = nullptr, bool clickable = false, bool link = false);
  QVBoxLayout* body() const { return lay_; }
  // .card .top: left (flexible, wraps) and right (auto width) side by side, aligned to the top.
  void addTop(QWidget* left, QWidget* right = nullptr);
  QLabel* addLine(const QString& html);                 // a <div> line (13 px)
  QLabel* addSub(const QString& html);                  // .sub: 11 px muted, 2 px above
signals:
  void clicked();
protected:
  void mouseReleaseEvent(QMouseEvent*) override;
private:
  QVBoxLayout* lay_;
  bool clickable_;
};

// Rich text label with word wrap (13 px text by default).
QLabel* label(const QString& html, int px = 13, const QString& qtc = {}, QWidget* parent = nullptr);

// Text clamped to `lines` lines with an ellipsis (-webkit-line-clamp), word wrapped.
class ClampLabel : public QWidget {
  Q_OBJECT
public:
  ClampLabel(const QString& text, int lines, QWidget* parent = nullptr);
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int w) const override;
  QSize sizeHint() const override;
  void setColor(const QString& qtcColor) { color_ = qtcColor; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QStringList layoutLines(int w) const;
  QString text_;
  int lines_;
  QString color_ = "muted";
};

}  // namespace charts::ui::side
