#pragma once
#include <QByteArray>
#include <QList>
#include <QRectF>
#include <QString>

// On-device text recognition of a picture (Apple Vision, VNRecognizeTextRequest, accurate level).
// Synchronous: call from a worker thread. Empty on other platforms or on failure.
namespace macro::ocr {
struct Line { QString text; QRectF box; };   // box in image units 0..1, origin top-left
QList<Line> recognize(const QByteArray& imageData);
}
