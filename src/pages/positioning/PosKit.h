#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QStringList>
#include <QTimer>
#include <QPointer>
#include <QImage>
#include <QColor>
#include <QFont>
#include "ui/Theme.h"
#include <functional>

class QPainter;
class QHBoxLayout;

// Widgets and painters shared by the Positioning pages (Retail Sentiment, COT, Risk Sentiment).
// Colours are the ones measured on the Positioning reference frames (docs/reference/Positioning/_shared-notes.md);
// long/short/neutral are the pages' series palette.
namespace positioning::pk {

namespace col {
inline QColor kLong() { return theme::map(0x22, 0xd3, 0xee); }      // long / up / risk-on
inline QColor kShort() { return theme::map(0xef, 0x5f, 0x5f); }     // short / down / risk-off
inline QColor kNeutral() { return theme::map(0xfa, 0xcc, 0x15); }   // neutral zone, range
inline QColor kPurple() { return theme::map(0xa7, 0x8b, 0xfa); }
inline QColor kGreen() { return theme::map(0x4a, 0xde, 0x80); }
inline QColor kPage() { return theme::map(0x05, 0x04, 0x02); }
inline QColor kPanel() { return theme::map(0x0c, 0x06, 0x01); }
inline QColor kPanelCot() { return theme::map(0x08, 0x06, 0x03); }
inline QColor kInput() { return theme::map(0x14, 0x0f, 0x05); }
inline QColor kBorder() { return theme::map(0x3a, 0x37, 0x1f); }
inline QColor kBorderSubtle() { return theme::map(0x2b, 0x2a, 0x1d); }
inline QColor kSeparator() { return theme::map(0x20, 0x1c, 0x10); }
inline QColor kText() { return theme::map(0xfd, 0xfc, 0xf7); }
inline QColor kTextSec() { return theme::map(0x9e, 0x99, 0x90); }
inline QColor kTextMuted() { return theme::map(0x7e, 0x7e, 0x76); }
inline QColor kTextDim() { return theme::map(0x5f, 0x5c, 0x55); }
inline QColor kTextFaint() { return theme::map(0x45, 0x42, 0x3b); }
inline QColor kGold() { return theme::map(0xea, 0xdb, 0x9f); }
inline QColor kYellow() { return theme::map(0xff, 0xd7, 0x06); }
inline QColor kGoldDark() { return theme::map(0x33, 0x2a, 0x04); }
inline QColor kRowHover() { return theme::map(0x1b, 0x15, 0x03); }
}

// Style sheet with every #rrggbb colour passed through theme::map() (call where the sheet is set, never cache).
QString mapQss(const QString& deepwaterQss);

// Shared animation clock: every pulsing dot and blinking cursor of the app's Positioning pages is in phase.
double pulse();          // 0..1, 2 s period (dim → bright → dim)
bool cursorOn();         // block cursor blink, 3 s period, on ≈55 %

// Drives repaints of animated widgets of one page; runs only while the page is visible.
class Ticker : public QObject {
public:
  explicit Ticker(QObject* parent, int intervalMs = 66);
  void add(QWidget* w, const QRect& area = {});   // empty area = whole widget
  void add(QWidget* w, std::function<QRect()> area);
  void addCursor(QWidget* w, const QRect& area);    // a block cursor (cursorOn()): repainted when it turns on or off only
  void setRunning(bool on);
private:
  QTimer m_t; QList<QPair<QPointer<QWidget>, std::function<QRect()>>> m_w, m_cursors;
  bool m_cursorOn = false;
};

// Page title bar: [icon] ● TITLE — SUBTITLE ▌ … [right badge]. Children may be added to rightLayout().
class TitleBar : public QWidget {
public:
  enum Icon { Users, Gauge, None };
  TitleBar(Icon icon, const QString& title, QWidget* parent, int height = 34);
  void setBadge(const QString& text, bool pulsingDot);   // gold pill at the right ("" hides it)
  void setPulse(bool on) { m_pulse = on; update(); }
  void registerAnimation(Ticker* t);                     // repaint only the dot, the cursor and the badge dot
  QHBoxLayout* rightLayout() const { return m_right; }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  Icon m_icon; QString m_title, m_badge; bool m_badgeDot = false, m_pulse = false; QRect m_badgeRect;
  QHBoxLayout* m_right;
  QImage m_static;   // gradient, icon, title, badge: what the 15 fps ticker repaints under the dots and the cursor
  qreal m_titleW = 0, m_badgeW = 0;   // text advances, measured with m_static
  void resizeEvent(QResizeEvent*) override { m_static = QImage(); }
};

// Footer: "Terminal Data · HH:MM:SS" and a state badge (DELAYED, OFFICIAL, …).
class Footer : public QWidget {
public:
  enum Kind { Delayed, Official, Muted };
  explicit Footer(QWidget* parent, int height = 26);
  void setTime(const QString& hhmmss) { m_time = hhmmss; update(); }
  void setBadge(const QString& text, Kind k, const QString& tooltip = {});
  void setNote(const QString& note) { m_note = note; update(); }    // dim text after the time
protected:
  void paintEvent(QPaintEvent*) override;
  bool event(QEvent*) override;
private:
  QString m_time = "—", m_badge, m_note, m_tip; Kind m_kind = Delayed;
};

// Segmented control. Yellow: solid yellow active pill (Retail, COT asset classes, EN|FR); Gold: dark-gold active
// (COT categories, Unweighted); Grey: neutral dark active (COT range).
class Segmented : public QWidget {
  Q_OBJECT
public:
  enum Style { Yellow, Gold, Grey };
  Segmented(const QStringList& items, Style s, const QFont& f, QWidget* parent, bool frame = true);
  void setItems(const QStringList& items);
  void setCurrent(int i);
  int current() const { return m_cur; }
  void setPadding(int h, int itemH) { m_padH = h; m_itemH = itemH; updateGeometry(); update(); }
  void setGap(int g) { m_gap = g; updateGeometry(); update(); }
  void setPillInset(qreal px) { m_inset = px; update(); }   // active pill narrower than its slot
  void setInactiveColor(const QColor& c) { m_inactive = c; update(); }
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override { return sizeHint(); }
signals:
  void changed(int index);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<QRectF> rects() const;
  QStringList m_items; Style m_style; QFont m_font; bool m_frame; int m_cur = 0, m_hover = -1;
  int m_padH = 7, m_itemH = 20, m_gap = 0; qreal m_inset = 0; QColor m_inactive{0x81, 0x7d, 0x75};
};

// Scroll area without visible scroll bars (wheel / trackpad scrolling only), as in the reference recordings.
class Scroll : public QScrollArea {
public:
  explicit Scroll(QWidget* parent);
};

// Retail / COT long-short status from the long %: ≥65 crowded, >50 leaning, =50 balanced.
enum class Status { CrowdedLong, LeaningLong, Balanced, LeaningShort, CrowdedShort };
Status statusOf(double longPct);
QString statusText(Status s);
// Badge right-aligned at rightX, vertically centred on cy. Returns its rect.
QRectF drawStatusBadge(QPainter& p, qreal rightX, qreal cy, Status s, qreal height = 16);

// Fake-3D pie (holeFrac = 0) or donut: tilted ellipse top (rx, ry), extruded side wall of `depth` px, soft shadow.
// Long goes clockwise from 12 o'clock, short counter-clockwise. Labels are drawn by the caller.
void drawPie3D(QPainter& p, const QPointF& centre, qreal rx, qreal ry, qreal depth, double longPct, double holeFrac,
               qreal shadowAlpha = 1.0);
// Point on the top face at a fraction of the radius, for the middle of the long (true) or short slice.
QPointF pieLabelPoint(const QPointF& centre, qreal rx, qreal ry, double longPct, bool longSlice, qreal radiusFrac);

// "● SUMMARY ───────" section heading (COT).
void drawSectionHeading(QPainter& p, const QRectF& r, const QString& text);
// Letter-spaced caps text, returns the advance.
qreal drawSpaced(QPainter& p, const QPointF& baseline, const QString& text, const QFont& f, const QColor& c);
QFont monoSpaced(int px, qreal spacing, int weight = 400);
// Two-person outline (Users) and gauge icons of the title bars.
void drawIcon(QPainter& p, TitleBar::Icon icon, const QRectF& r, const QColor& c);
// Blinking yellow block cursor.
void drawBlockCursor(QPainter& p, const QRectF& r);
// Pulsing status dot (dim olive ↔ bright yellow).
void drawPulseDot(QPainter& p, const QPointF& c, qreal radius, bool animate);
}
