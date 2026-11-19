// NativeTv on platforms without a system web view host yet (see NativeTv.h): no player, the pane links out.
#ifndef __APPLE__
#include "pages/news/squawk/NativeTv.h"
namespace wire::squawk {
NativeTv::NativeTv(QWidget* parent) : QObject(reinterpret_cast<QObject*>(parent)) {}
NativeTv::~NativeTv() = default;
void NativeTv::loadHtml(const QString&, const QUrl&) {}
void NativeTv::clear() {}
bool NativeTv::available() const { return false; }
void NativeTv::setGeometry(const QRect&) {}
void NativeTv::setVisible(bool) {}
}
#endif
