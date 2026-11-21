#pragma once
#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QColor>
#include <QList>
#include <functional>
#include <QDateTime>

class QPainter;
class QHBoxLayout;
class QLineEdit;

// Widgets shared by the Technical Heatmap and Liquidity pages (market analytics package), measured on their
// reference frames (docs/reference/Market/_shared-notes-2.md).
namespace mkit {

// Semantic colours of the two pages (measured; the heat scale is a chart palette, not UI chrome).
struct Sem {
  QColor cyan{"#22d3ee"}, red{"#f87171"}, gold{"#facc15"}, violet{"#a78bfa"}, grey{"#a3a09a"};
  QColor goldFill{"#fcd515"}, goldText{"#fce56b"}, chipText{"#95928d"}, chipCount{"#62605a"};
  QColor muted{"#6b6a5e"}, dim{"#51504e"}, subtle{"#1a1815"}, page{"#050402"}, cell{"#060503"};
  QColor panel{"#0b0802"}, panelBorder{"#3f3826"}, bracket{"#8a7d60"}, strip{"#3f3a1a"}, white{"#fffffa"};
};
const Sem& sem();

// Page background: warm near-black, a faint 64 px grid and a soft warm glow under the title strip.
void paintBackdrop(QPainter& p, const QRect& r, int glowTop = 0);

// Title strip: glyph, pulsing gold dot, letter-spaced title, blinking block cursor; widgets at the right
// (rightLayout) and after the title (afterTitleLayout). Animations run only while visible (≤ 15 fps, dot only).
class TitleStrip : public QWidget {
  Q_OBJECT
public:
  enum Glyph { Pulse, Menu };
  TitleStrip(const QString& title, Glyph g, int height, QWidget* parent = nullptr);
  QHBoxLayout* rightLayout() const { return m_right; }
  QHBoxLayout* afterTitleLayout() const { return m_after; }
  int titleRight() const;          // x just after the block cursor
  // Phones: the title is elided to `px` (0 = no title and no cursor, the widgets after it move left); < 0 = whole
  // title (default, every desktop width).
  void setTitleRoom(int px);
protected:
  void paintEvent(QPaintEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void paintStatic(QPainter& p);
  QString shownTitle() const;
  QString m_title; Glyph m_glyph; QTimer m_tick; qint64 m_t0; bool m_cursorOn = true; int m_frame = 0; int m_room = -1;
  QHBoxLayout* m_row;
  QImage m_static;   // gradient, glyph, title: the 10 fps dot and the cursor are drawn over it
  QHBoxLayout* m_right; QHBoxLayout* m_after;
};

// Gold-bordered info badge ("• 127 INSTRUMENTS · DAILY CANDLES · COMPUTED").
class Badge : public QWidget {
  Q_OBJECT
public:
  explicit Badge(QWidget* parent = nullptr);
  void setText(const QString& t);
  void setProgress(double f);      // 0..1 thin fill under the text while loading, < 0 hides it
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QString m_text; double m_progress = -1;
};

// KPI card: label, big value (with glow) or a custom painter, optional sub-line, 2 px top accent.
class KpiCard : public QWidget {
  Q_OBJECT
public:
  explicit KpiCard(const QString& label, QWidget* parent = nullptr);
  void setValue(const QString& v, const QColor& c);
  void setSub(const QString& s);
  void setAccent(const QColor& c);          // invalid colour → no accent bar
  void setValuePx(int px, int weight);
  void setCustom(std::function<void(QPainter&, const QRectF&)> f);   // replaces the value
  void setLabelPx(int px, qreal spacing);
  void setTooltipText(const QString& t) { setToolTip(t); }
protected:
  bool event(QEvent* e) override;
  void paintEvent(QPaintEvent*) override;
private:
  QString m_label, m_value, m_sub; QColor m_valueColor, m_accent;
  int m_valuePx = 30, m_valueWeight = 500, m_labelPx = 10; qreal m_labelSpacing = 2.6;
  std::function<void(QPainter&, const QRectF&)> m_custom;
  bool labelElided() const;
  QStringList labelLines(QFont* font) const;
};

// Group of filter chips in a 1 px box: gold filled active chip, gold text on hover, optional dim count.
class ChipGroup : public QWidget {
  Q_OBJECT
public:
  struct Chip { QString label; int count = -1; QColor color; };
  explicit ChipGroup(QWidget* parent = nullptr);
  void setChips(const QList<Chip>& c);
  void setCounts(const QList<int>& counts);
  void setCurrent(int i);
  int current() const { return m_cur; }
  QSize sizeHint() const override;
  QRectF chipRect(int i) const;
  // Phones: chips flow onto rows `rowH` tall within `width` (each row stretched to the full width, finger-sized),
  // the group's height follows. width 0 = one row 27 px tall (default).
  void setWrap(int width, int rowH);
signals:
  void changed(int i);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<QRectF> rects() const;
  QList<Chip> m_chips; int m_cur = 0, m_hover = -1;
  int m_wrapW = 0, m_rowH = 27;
  void fitWrap();
};

// Search field drawn like the reference (magnifier + letter-spaced uppercase placeholder).
class SearchBox : public QWidget {
  Q_OBJECT
public:
  SearchBox(const QString& placeholder, bool uppercase, QWidget* parent = nullptr);
  QString text() const;
  void setFocusToEdit();
  void clear();
  void setText(const QString& t);
signals:
  void textChanged(const QString&);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  QLineEdit* m_edit;
};

// Paint a value tag with a glow-free soft text shadow (KPI values in the reference glow in their colour).
void glowText(QPainter& p, const QPointF& baseline, const QString& s, const QColor& c, int strength = 3);
// Test hook helper: deliver a mouse move at `pt` (root coordinates) to the deepest child there (hover states).
void simulateHover(QWidget* root, const QPoint& pt);
// "9d ago" / "il y a 9 j" in the interface language (fmt::ago is English only).
QString ago(const QDateTime& t, const QDateTime& now);
// Other themes: text drawn on a filled tile/tag may lose contrast after mapping (light themes, Bloomberg greens);
// below 3.5:1 it switches to near-black or white, whichever reads better. Identity for Deepwater.
QColor readable(const QColor& text, const QColor& fill);
// Letter-spaced uppercase label helper widths.
qreal textWidth(const QFont& f, const QString& s);
}
