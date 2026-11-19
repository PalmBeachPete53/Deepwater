#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>
#include <cmath>

class QLineEdit;

// Widgets of the Markets Watch list (all painted to match the reference frames).
namespace watch {

// "MARKET WATCH" eyebrow, big "MARKETS" title, "127 LIVE MARKETS · QUOTES REFRESH EVERY 20S", gold rule.
class HeroHeader : public QWidget {
  Q_OBJECT
public:
  explicit HeroHeader(QWidget* parent = nullptr);
  void setCount(int n) { if (n != m_count) { m_count = n; update(); } }
  // short windows: one line (smaller title, no eyebrow) so the market lists get the height
  void setCompact(bool on) { if (on != m_compact) { m_compact = on; update(); } }
  static int heightFor(bool compact) { return compact ? 62 : 126; }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  int m_count = 0; bool m_compact = false;
};

// Search field with the magnifier and the "/" key hint.
class SearchBox : public QWidget {
  Q_OBJECT
public:
  explicit SearchBox(QWidget* parent = nullptr);
  QLineEdit* edit() const { return m_edit; }
  void setCount(int n);
signals:
  void textChanged(const QString&);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
private:
  QLineEdit* m_edit;
};

// Category filter pills "ALL 127 | FOREX 42 | …" (gold fill when active, count in an inner badge).
class CategoryPills : public QWidget {
  Q_OBJECT
public:
  explicit CategoryPills(QWidget* parent = nullptr);
  void setItems(const QStringList& labels, const QList<int>& counts);
  void setCurrent(int i);
  int current() const { return m_cur; }
  QSize sizeHint() const override;
signals:
  void changed(int);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<QRectF> rects() const;
  QStringList m_labels; QList<int> m_counts; int m_cur = 0, m_hover = -1;
};

// "LABEL [A|B|C]" segmented control (gold fill on the active segment).
class Segmented : public QWidget {
  Q_OBJECT
public:
  Segmented(const QString& label, const QStringList& items, QWidget* parent = nullptr);
  void setCurrent(int i);
  int current() const { return m_cur; }
  QSize sizeHint() const override;
signals:
  void changed(int);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<QRectF> rects() const;
  qreal labelWidth() const;
  QString m_label; QStringList m_items; int m_cur = 0, m_hover = -1;
};

// One of the four "today" cards.
class StatCard : public QWidget {
  Q_OBJECT
public:
  enum Kind { Breadth, Average, Leader, Laggard };
  StatCard(Kind k, QWidget* parent = nullptr);
  void setPeriod(const QString& p);                      // "TODAY", "THIS WEEK", "THIS MONTH"
  void setBreadth(int up, int down, int flat);
  void setAverage(double pct, int n);
  void setMarket(const QString& symbol, const QString& name, double pct);
  void clear();
protected:
  void paintEvent(QPaintEvent*) override;
private:
  Kind m_kind; QString m_period = "TODAY";
  int m_up = 0, m_down = 0, m_flat = 0, m_n = 0; double m_pct = NAN; QString m_sym, m_name; bool m_has = false;
};

// A category panel listing its markets (symbol, last, change).
struct Row {
  QString symbol;     // app symbol
  QString label;      // shown text: "EUR/USD", "S&P 500", "AAPL"
  QString sub;        // company name under stock tickers
  int decimals = 2;
  double last = NAN, chg = NAN;
  QString chgTip;     // tooltip of the change cell (reference close used for 1W/1M)
};
class MarketPanel : public QWidget {
  Q_OBJECT
public:
  explicit MarketPanel(const QString& title, bool tallRows, QWidget* parent = nullptr);
  void setRows(const QVector<Row>& rows);
  void setPeriod(const QString& p) { m_period = p; update(); }
  void setTotal(int n) { m_total = n; update(); }
  void setColumns(int n);                  // >1 when one category fills the page width
  int heightFor() const;
  QString title() const { return m_title; }
  QString firstSymbol() const { return m_rows.isEmpty() ? QString() : m_rows.first().symbol; }
signals:
  void open(const QString& symbol);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool event(QEvent*) override;
private:
  int rowAt(const QPointF& p) const;
  QRectF rowRect(int i) const;
  int perColumn() const;
  QString m_title, m_period = "1D"; bool m_tall; QVector<Row> m_rows; int m_hover = -1, m_cols = 1, m_total = 0;
};
}
