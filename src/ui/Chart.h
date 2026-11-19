#pragma once
#include <QVector>
#include <QRectF>
#include <QColor>
#include <QString>
#include <QStringList>
#include <functional>
#include <cmath>

class QPainter;

// Small painting helpers shared by the custom charts (pages paint their charts with QPainter).
namespace chart {
// "Nice" axis ticks covering [lo, hi] with about n steps.
QVector<double> niceTicks(double lo, double hi, int n = 5);
// Min/max of finite values with a relative margin.
QPair<double, double> range(const QVector<double>& v, double margin = 0.05);
// Polyline of values in rect (x spread evenly), optional fill gradient to the bottom.
void sparkline(QPainter& p, const QRectF& r, const QVector<double>& v, const QColor& c, qreal width = 1.4,
               bool fill = false, double lo = NAN, double hi = NAN);
// Horizontal grid lines with right- or left-aligned labels.
void hGrid(QPainter& p, const QRectF& plot, const QVector<double>& ticks, double lo, double hi,
           const std::function<QString(double)>& label, bool labelsRight = true);
// Tooltip box (dark, amber border) near a point, kept inside bounds. lines[0] may be a title.
void tooltip(QPainter& p, const QPointF& at, const QStringList& lines, const QRectF& bounds,
             const QList<QColor>& colors = {});
// Value pill on the axis (e.g. last price on the right edge).
void axisTag(QPainter& p, const QPointF& anchorLeftMid, const QString& text, const QColor& bg, const QColor& fg);
}
