#include "core/Startup.h"
#include <QList>

namespace dw {
namespace {
QList<void (*)()>& kept() { static QList<void (*)()> l; return l; }
}
void addStartupFunction(void (*f)()) { kept() << f; }
void runStartupFunctions() {
  const QList<void (*)()> l = kept();
  kept().clear();
  for (auto f : l) f();
}
}  // namespace dw
