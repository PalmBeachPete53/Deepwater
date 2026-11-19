#pragma once
#include <QWidget>
#include <QColor>
#include <QFont>
#include <QDateTime>
#include <functional>
#include <cmath>

class QPainter;
class QVariantAnimation;
class QMenu;

// Painting kit of the Account pages (Track Record, Risk Calculator). Colours are the values measured on the
// Account reference frames (docs/reference/Account/_shared-notes.md); they are this tab's chart/series palette.
namespace acct {

struct Colors {
  QColor page{"#040302"}, toolbar{"#0f0a02"}, toolbar2{"#140f03"}, panel{"#0c0601"}, panel2{"#0d0802"}, input{"#050401"};
  QColor border{"#3a351b"}, borderSide{"#2c2512"}, divider{"#2d2a15"}, bracketTL{"#615942"}, bracketBR{"#776f59"};
  QColor bracketHi{"#efd860"}, accent{"#fdd51f"}, accentHi{"#ffda44"}, tabText{"#fde695"}, selector{"#f0d878"};
  QColor label{"#e6c200"}, big{"#ffd709"};
  QColor cyanBar{"#1ddbf0"}, cyanBig{"#3adae5"}, cyanDot{"#2bc3d3"}, cyanLine{"#3fd0de"}, cyanText{"#3fc6ee"};
  QColor redBar{"#ff817d"}, redBig{"#f78a8b"}, redDot{"#ef786e"}, redText{"#e0605a"};
  QColor neutralBar{"#9c9788"};
  QColor white{"#ece8de"}, white2{"#f4f1ea"}, grey{"#a29c8c"}, dim{"#8a8478"}, faint{"#5d584c"}, ghost{"#3d3a32"};
  QColor grid{"#1b170b"}, zero{"#4c4937"}, axis{"#9a958a"}, category{"#e3e0db"}, axisLine{"#47433b"};
  QColor hoverBand{"#312c28"}, tipBg{"#0f0602"}, tipBorder{"#544f2d"};
};
const Colors& c();                    // mapped onto the current theme (theme::map)
// Style sheet written with Deepwater hex colours → the same sheet with every #rrggbb mapped to the theme.
QString qss(QString deepwaterCss);

// Fonts: mono (numbers, titles), chart (axis/category labels), cond (account names, footnotes, radar labels).
QFont mono(int px, bool bold = false, qreal spacing = 0);
QFont chart(int px, bool bold = false);
QFont cond(int px, bool bold = false);

// Money in the account currency: "$1,805.00", "-$100.00"; signed: "+$55.50". axisMoney: "$1.5K", "−$200".
QString ccySymbol(const QString& ccy);
QString money(double v, const QString& ccy, int decimals = 2);
QString signedMoney(double v, const QString& ccy, int decimals = 2);
QString axisMoney(double v, const QString& ccy);
QString trimNum(double v, int maxDecimals);        // 4,155.5 / 0.70001 / 0.5 (trailing zeros trimmed)
QString tradeDate(const QDateTime& utc);           // "18 Oct 26, 22:37" local, September as "Sept"
QString longDate(const QDateTime& utc);            // "16 Aug 2026, 20:34"
QString held(double hours);
QString unitLabel(const QString& unit);         // contract unit for display ("units" → "unités")
QString tagLabel(const QString& tag);           // category tag for display ("METAL" → "MÉTAL")                        // "36m", "6.5h"

// Panel frame: bg, 1 px border, corner brackets (highlight: four bright yellow brackets with 12 px arms).
void panelFrame(QPainter& p, const QRectF& r, bool highlight = false);
// Title at (x+13, y+19) with an optional ⓘ; returns the ⓘ rect (empty if none).
QRectF panelTitle(QPainter& p, const QRectF& panel, const QString& title, bool info, const QColor& col = QColor(),
                  int px = 10, qreal spacing = 3);
void infoIcon(QPainter& p, const QRectF& r);

// Tooltip: lines of (text, colour, bold). Placed 16 px right/below the pointer, flipped inside bounds.
struct TipLine { QString text; QColor color; bool bold = false; QString value; QColor valueColor; bool valueBold = false; };
void tooltip(QPainter& p, const QPointF& at, const QList<TipLine>& lines, const QRectF& bounds);

// A painted panel whose body is drawn by a closure. The closure gets the mouse position (NaN when the pointer
// is outside) and the entry animation (0→1). The widget repaints on pointer moves and while animating.
class PaintPanel : public QWidget {
  Q_OBJECT
public:
  using PaintFn = std::function<void(QPainter&, const QRectF& panel, const QPointF& mouse, double anim)>;
  explicit PaintPanel(QWidget* parent = nullptr);
  void setPaint(PaintFn f) { m_fn = std::move(f); update(); }
  void setTitle(const QString& t, const QString& info = {}, const QColor& col = QColor("#807d6c"), int px = 10, qreal sp = 2.2) {
    m_title = t; m_info = info; m_tcol = col; m_tpx = px; m_tsp = sp; update();
  }
  void setHighlight(bool on) { m_hi = on; update(); }
  void setFrame(bool on) { m_frame = on; update(); }
  void setHoverable(bool on) { setMouseTracking(on); m_hover = on; }
  void animate(int ms = 500);                       // replays the entry animation
  QPointF mousePos() const;
  // Test hook: a fixed pointer position in global coordinates (DW_HOVER).
  static void setFakeCursor(const QPoint& global);
  static bool hasFakeCursor();
  static QPoint fakeCursor();
signals:
  void clicked(const QPointF& pos);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool event(QEvent*) override;
private:
  PaintFn m_fn;
  QString m_title, m_info;
  QColor m_tcol; int m_tpx = 10; qreal m_tsp = 2.2;
  QRectF m_infoRect;
  bool m_hi = false, m_frame = true, m_hover = false;
  double m_anim = 1;
  QVariantAnimation* m_va = nullptr;
};

// Small painted combo (filter bar, form fields): text + chevron, opens a menu of options.
class ComboChip : public QWidget {
  Q_OBJECT
public:
  explicit ComboChip(QWidget* parent = nullptr);
  void setItems(const QStringList& items, int current = 0);
  void setCurrent(int i);
  int current() const { return m_cur; }
  QString currentText() const { return m_items.value(m_cur); }
  QSize sizeHint() const override;
signals:
  void changed(int index);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QStringList m_items; int m_cur = 0; bool m_hov = false;
};

// Test hooks shared by the Account pages (page coordinates):
//   DW_HOVER=x,y         fixed pointer for hover states (tooltips, crosshair, row bands)
//   DW_CLICK=x,y[;x,y]   left clicks sent 0.8 s after start, 0.4 s apart
//   DW_GRAB_MODAL=f.png  saves the dialog opened by a DW_CLICK 2.5 s after start, then closes it
void installTestHooks(QWidget* page);

// Menu styled like the app (dark, yellow selection).
void styleMenu(QMenu* m);

// "Nice" ticks including zero over [lo, hi], extended to whole steps.
QVector<double> ticks(double lo, double hi, int n, double* outLo = nullptr, double* outHi = nullptr);
}
