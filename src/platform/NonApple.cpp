// What the macOS-only modules (banks/Pdf.mm, macro/Ocr.mm) give on the other platforms, as their headers promise:
// empty results. Bank Research then shows no PDF pages and the RBA / RBNZ probability pictures are not read.
// (The Squawk pane's web view has its own stand-in, pages/news/squawk/NativeTvStub.cpp.)
#ifndef __APPLE__
#include "banks/Pdf.h"
#include "macro/Ocr.h"

namespace banks::pdf {
Info info(const QString&) { return {}; }
QImage render(const QString&, int, double, double) { return {}; }
QString text(const QString&, int) { return {}; }
bool print(const QString&) { return false; }
}  // namespace banks::pdf

namespace macro::ocr {
QList<Line> recognize(const QByteArray&) { return {}; }
}  // namespace macro::ocr

#endif
