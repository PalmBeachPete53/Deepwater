#pragma once
#include <QColor>
#include <QFont>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QList>
#include <QTimer>

class QPainter;
class QScrollArea;
class QWidget;

namespace economy {

// Painting helpers shared by Economy › Macro and Economy › Rates (colours measured on the reference frames,
// derived from theme tokens; chart series colours are literal like every chart palette).
namespace eco {

// ---- colours
QColor pageBg();          // #080500
QColor boxBorder();       // #282212 summary/dashboard boxes
QColor boxBorderHi();     // #474331 chart box
QColor hair();            // #211d12 row separators
QColor amber();           // #F5C518
QColor amberFill();       // #ffd900 selected tab / button fill
QColor amberText();       // #E5B112 headings
QColor amberPale();       // #FDE575 big values
QColor amberDimFill();    // #3d2901
QColor onAmber();         // near-black text on amber
QColor txt();             // #e7e3d8
QColor txtBody();         // #bbbab6
QColor txtSec();          // #8a877e
QColor txtMute();         // #5b574e
QColor txtFaint();        // #3b3a36
QColor cyan();            // #22D3E0
QColor red();             // #F05A50
QColor grey();            // #aab1ba
// outcome colours (-50+, -25, 0, +25, +50+)
QColor outcome(int bucket);
QColor semantic(int goodWhenUp, double change);   // cyan good / red bad / grey neutral

// ---- fonts (px sizes measured at 1600x966)
QFont mono(int px, int weight = 400, qreal ls = 0);
QFont sans(int px, int weight = 400);
QFont cond(int px, int weight = 600);
QFont display(int px, int weight = 700, qreal ls = 0);

// ---- painters
// "• SUMMARY ─────" heading: amber dot, amber bold letter-spaced text, 1 px hairline to `right`.
void sectionHeading(QPainter& p, qreal x, qreal yMid, qreal right, const QString& text, int px = 11);
// Letter-spaced micro label (uppercase), returns its advance.
qreal micro(QPainter& p, qreal x, qreal yBase, const QString& text, const QColor& c, int px = 9, qreal ls = 2.5);
// Tooltip: rows of (swatch colour, label, value). `title` muted first line. Square swatch if squareSwatch.
struct TipRow { QColor swatch; QString label; QString value; };
QRectF tooltip(QPainter& p, const QPointF& cursor, const QString& title, const QList<TipRow>& rows,
               const QRectF& bounds, bool squareSwatch = false, QPointF offset = {25, 20});
// Filled-amber / outlined segmented button. Returns its rect.
void button(QPainter& p, const QRectF& r, const QString& text, bool selected, bool hover = false, int px = 11,
            bool enabled = true);
// Small pill "• Hold" etc.
void pill(QPainter& p, const QRectF& r, const QString& text, const QColor& fill, const QColor& fg, bool dot, qreal radius);
// Corner bracket at the top-left of a box.
void bracket(QPainter& p, const QRectF& r, const QColor& c);
// Format helpers
QString pctTrim(double v, int maxDecimals = 1);   // 4% / 3.2% (trailing zeros dropped)
QString probText(double p);                        // "<1%" or "92.5%"
QString signedBp(double bp);                       // +1.7 / -0.8

// Shared blink clock for carets and status dots (on/off every 500 ms). Pages call hold(this) while visible.
class Blink : public QObject {
  Q_OBJECT
public:
  static Blink& instance();
  bool on() const { return m_on; }
  int phase() const { return m_phase; }   // 0,1,2… every 500 ms
  void hold(QObject* who, bool visible);
signals:
  void tick();
private:
  Blink();
  QTimer m_timer; bool m_on = true; int m_phase = 0; QList<QObject*> m_holders;
};

// Scroll area with hidden scrollbars and ~100 px per wheel notch.
void setupScroll(QScrollArea* a);
}
}  // namespace economy
