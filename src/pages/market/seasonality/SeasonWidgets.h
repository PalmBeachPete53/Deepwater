#pragma once
#include "seasonality/Seasonality.h"
#include <QWidget>
#include <QFont>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QPainter;

// Private widgets of Market › Seasonality (all painted, geometry measured on the 1600x966 reference).
namespace seasonality {

QFont ui(int px, int weight = QFont::Medium);             // proportional (Rajdhani look of the reference)
QFont mono(int px, int weight = QFont::Normal, qreal ls = 0);
// Violet period pill ("Jan-Jun") with its right edge at `right`, top at `top`; returns its rect.
QRectF drawPeriodPill(QPainter& p, qreal right, qreal top, const QString& text);
void drawCardFrame(QPainter& p, const QRect& r);
void drawCardTitle(QPainter& p, const QPointF& at, const QString& text, bool centred = false, qreal width = 0);
// Narrow cards (phones): the title font steps down (letter spacing, then size) until the text fits maxWidth.
void drawCardTitleFit(QPainter& p, const QPointF& at, const QString& text, qreal maxWidth);
QString signedPct(double v, int dec = 2);                  // +0.41% / -0.47% / "—"

// ---------------------------------------------------------------- toolbar
class ToolBar : public QWidget {
  Q_OBJECT
public:
  enum Item { Asset, Help, Prev, Next, Years, Filter, Seg, Compare = Seg + 7, Actual, Index, Refresh, None = -1 };
  explicit ToolBar(QWidget* parent);
  void setTexts(const QString& asset, const QString& years, const QString& compare);
  void setOpen(int item);                 // dropdown currently open (-1 none)
  void setSegment(int seg, bool clicked); // 0 Full .. 6 Q4, -1 custom; clicked: gold ring
  void setToggles(bool actual, bool index);
  void setActualLabel(const QString& s) { m_actualLabel = s; relayout(); }
  QRect itemRect(int item) const;
  // phones: the items wrap onto rows of finger-sized buttons; the height then follows heightFor(width)
  void setWrap(bool on) { if (on != m_wrap) { m_wrap = on; relayout(); } }
  bool wraps() const { return m_wrap; }
  int heightFor(int w) const { QVector<QRect> r(Refresh + 1); return place(w, r); }
signals:
  void clicked(int item);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override { relayout(); }
  bool event(QEvent* e) override;
private:
  void relayout();
  int place(int w, QVector<QRect>& rects) const;   // item rects for width w; returns the bar's height
  int hit(const QPoint& p) const;
  bool m_wrap = false;
  QString m_asset, m_years, m_compare, m_actualLabel = "2026 actual";
  QVector<QRect> m_rects;
  int m_hover = None, m_open = None, m_seg = 0; bool m_segClicked = false, m_actual = false, m_index = false;
};

// ---------------------------------------------------------------- dropdown
class Menu : public QWidget {
  Q_OBJECT
public:
  struct Row { QString text, tag, key, hint; };   // hint: extra searchable text (full name)
  explicit Menu(QWidget* parent, bool search = false);
  void setRows(const QList<Row>& rows, int selected);
  void setNote(const QString& note) { m_note = note; relayout(); }
  void setRowFont(const QFont& f) { m_font = f; relayout(); }
  void setStrongSelection(bool on) { m_strong = on; update(); }   // selected row in the bright gold-brown (years menu)
  void popup(const QPoint& topLeft, int width);
  QLineEdit* search() const { return m_search; }
signals:
  void picked(const QString& key);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void leaveEvent(QEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
private:
  void relayout();
  void filter();
  int rowAt(const QPoint& p) const;
  QRect listRect() const;
  QList<Row> m_all, m_rows;
  QString m_note; QFont m_font;
  QLineEdit* m_search = nullptr;
  int m_sel = -1, m_hover = -1, m_scroll = 0, m_maxRows = 8;
  QString m_selKey;
  bool m_strong = false;
};

// ---------------------------------------------------------------- seasonal trend chart
class TrendCard : public QWidget {
  Q_OBJECT
public:
  explicit TrendCard(QWidget* parent);
  void setTitle(const QString& t, const QString& sub) { m_title = t; m_sub = sub; update(); }
  void setPaths(const QVector<double>& smooth, const QVector<double>& raw, int todaySlot);
  void setOverlays(const QVector<double>& compare, const QString& compareName, const QColor& compareColor,
                   const QVector<double>& actual, const QString& actualName);
  void setIndexMode(bool on) { m_index = on; update(); }
  void setPeriod(const seasonality::Period& p) { m_period = p; update(); }
  void setStatus(const QString& s) { m_status = s; update(); }     // non-empty: loading / error text instead of data
  void setHover(const QPoint& p);                                  // test hook (DW_SEASON_HOVER)
signals:
  void periodDragged(const seasonality::Period& p);
  void cleared();
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QRectF plot() const;
  double xOf(double slot) const;
  int slotAt(double x) const;
  double yOf(double v) const;
  QRectF clearRect() const;
  void computeRange();
  QString title() const { return m_title; }
  QString m_title, m_sub, m_status = "Loading daily history…", m_compareName, m_actualName;
  QVector<double> m_smooth, m_raw, m_compare, m_actual;
  QColor m_compareColor;
  int m_today = -1;
  double m_lo = -1, m_hi = 1, m_step = 0.5;
  bool m_index = false;
  seasonality::Period m_period;
  int m_hoverX = -1, m_hoverY = -1;
  int m_dragFrom = -1, m_dragTo = -1;
  bool m_dragging = false;
  bool m_pinned = false;   // hover set by the test hook survives a leave event
  QRectF m_clear, m_badge;
};

// ---------------------------------------------------------------- win rate
class WinCard : public QWidget {
  Q_OBJECT
public:
  explicit WinCard(QWidget* parent);
  void setStats(const seasonality::Stats& s, int yearsRequested);
  void setStatus(const QString& s) { m_status = s; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  void paintPie(QPainter& p, const QPointF& c, double share);
  seasonality::Stats m_s; int m_years = 20; bool m_has = false;
  QString m_status = "Loading…";
};

// ---------------------------------------------------------------- bars
class BarCard : public QWidget {
  Q_OBJECT
public:
  BarCard(const QString& title, QWidget* parent);
  void setBars(const QStringList& labels, const QVector<double>& values, const QVector<bool>& dim = {},
               const QStringList& tips = {});
  void setPill(const QString& pill) { m_pill = pill; update(); }
  void setNote(const QString& n) { m_note = n; update(); }
  void setStatus(const QString& s) { m_status = s; update(); }
  void setGap(double g) { m_gap = g; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QRectF plot() const;
  QString m_title, m_pill, m_status = "Loading…", m_note;
  QStringList m_labels, m_tips; QVector<double> m_values; QVector<bool> m_dim;
  double m_gap = 11; int m_hover = -1;
};

// ---------------------------------------------------------------- risk metrics
class RiskCard : public QWidget {
  Q_OBJECT
public:
  explicit RiskCard(QWidget* parent);
  void setStats(const seasonality::Stats& s);
  int heightFor(int w) const;   // phones: tall enough for the wrapped footnote (the desktop card is 315 px)
  static QString footnote(const QString& period);
  void setStatus(const QString& s) { m_status = s; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  seasonality::Stats m_s; bool m_has = false; QString m_status = "Loading…";
};
}
