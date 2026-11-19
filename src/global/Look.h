#pragma once
#include <QColor>
#include <QFont>
#include <QRectF>
#include <QString>
#include "ui/Theme.h"

class QPainter;
class QWidget;

// Painting helpers for the Global pages (Overview, Market Hours): the reference's panel frame, corner
// brackets, terminal titles with the blinking block cursor, the ⓘ icon, pills and the page grid.
// Values measured on the reference frames (docs/reference/Global/_shared-notes.md).
namespace global::look {

// Page background: warm near-black with a faint 40 px grid.
void pageBackground(QPainter& p, const QRectF& r);
// Panel: 1 px border #433d2c, dark warm body, top-left and bottom-right corner brackets.
void panel(QPainter& p, const QRectF& r, const QColor& body = theme::map("#080502"), const QColor& border = theme::map("#433d2c"));
void brackets(QPainter& p, const QRectF& panelRect, qreal inset = 5, qreal arm = 10);
// Yellow panel title (JetBrains Mono Bold 13, spacing 3.5) at panel x+11, baseline y+21; returns the x after the text.
qreal panelTitle(QPainter& p, const QRectF& panelRect, const QString& text, bool info = true);
// ⓘ: 10 px circle with a small i, centred at c.
void infoIcon(QPainter& p, const QPointF& c, const QColor& col = theme::map("#5f5a50"));
// Block cursor 6x11 (yellow) with its top-left at (x, y).
void cursorBlock(QPainter& p, qreal x, qreal y, qreal w = 6, qreal h = 11);
// Shared blink phase: every cursor on screen blinks in sync (period 1 s).
bool cursorOn();
// Draw text with its left at x and its baseline at y. Returns the advance.
qreal text(QPainter& p, qreal x, qreal baseline, const QString& s, const QFont& f, const QColor& c);
qreal textRight(QPainter& p, qreal right, qreal baseline, const QString& s, const QFont& f, const QColor& c);
qreal textCenter(QPainter& p, qreal cx, qreal baseline, const QString& s, const QFont& f, const QColor& c);
qreal width(const QString& s, const QFont& f);
QFont titleFont();   // panel titles: JetBrains Mono Bold 13, spacing 1.7 (measured)
// Soft glow behind a stroke/shape: draws the path several times with growing width and fading alpha.
void glowLine(QPainter& p, const QPointF& a, const QPointF& b, const QColor& c, qreal w, qreal spread, int alpha);
}
