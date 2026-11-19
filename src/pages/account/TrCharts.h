#pragma once
#include <QRectF>
#include <QStringList>
#include <QVector>
#include <QColor>
#include <functional>

class QPainter;

// Bar chart painters shared by the Dashboard and Advanced Details views. Each returns the hovered category
// (-1 if none) so the caller can draw its own tooltip on top.
namespace acct {

struct VBars {
  QRectF plot;
  QStringList labels;                 // x labels (one per bar; drawn every labelEvery)
  QVector<double> values;
  QVector<QColor> colors;
  std::function<QString(double)> yfmt;
  int labelEvery = 1;
  double barWidth = 30;               // px, or a fraction of the slot when < 1
  int nticks = 7;
};
int drawVBars(QPainter& p, const VBars& b, double anim, const QPointF& mouse);

struct HBars {
  QRectF plot;
  QStringList labels, sub;            // category label, optional second line
  QVector<double> values;
  QVector<QColor> colors;
  std::function<QString(double)> xfmt;
  int nticks = 6;
  bool anchorRight = false;           // negative-only charts: $0 on the right
  bool countAxis = false;             // 0..max integer axis (counts)
};
int drawHBars(QPainter& p, const HBars& b, double anim, const QPointF& mouse);
}
