// What the macOS-only modules (banks/Pdf.mm, macro/Ocr.mm, pages/news/squawk/NativeTv.mm) give on the other platforms,
// as their headers promise: empty results and no web view. Bank Research then shows no PDF pages, the RBA / RBNZ
// probability pictures are not read, and the Squawk pane offers "Open on YouTube" instead of the live player.
#ifndef __APPLE__
#include "banks/Pdf.h"
#include "macro/Ocr.h"
#include "pages/news/squawk/NativeTv.h"
#include <QWidget>

namespace banks::pdf {
Info info(const QString&) { return {}; }
QImage render(const QString&, int, double, double) { return {}; }
QString text(const QString&, int) { return {}; }
bool print(const QString&) { return false; }
}  // namespace banks::pdf

namespace macro::ocr {
QList<Line> recognize(const QByteArray&) { return {}; }
}  // namespace macro::ocr

namespace wire::squawk {
NativeTv::NativeTv(QWidget* parent) : QObject(parent), m_host(parent) {}
NativeTv::~NativeTv() = default;
bool NativeTv::available() const { return false; }
void NativeTv::setGeometry(const QRect& inWindow) { m_geo = inWindow; }
void NativeTv::setVisible(bool on) { m_visible = on; }
void NativeTv::loadHtml(const QString&, const QUrl&) {}
void NativeTv::clear() {}
}  // namespace wire::squawk
#endif
