#pragma once
// DW_STARTUP_FUNCTION(f): a service started when the application object is built, as Q_COREAPP_STARTUP_FUNCTION.
// On iOS (2026-11-21) Qt calls those functions before its event dispatcher has a run loop, and the first timer they
// start crashes (CFRunLoopAddTimer on a null run loop): there they are kept and run by main() right after the
// QApplication is built (dw::runStartupFunctions), in the same order. Elsewhere it is Q_COREAPP_STARTUP_FUNCTION itself.
#include <QCoreApplication>

namespace dw {
void addStartupFunction(void (*f)());
void runStartupFunctions();   // main(), after QApplication: runs the kept functions (none outside iOS)
}

#if defined(Q_OS_IOS)
#define DW_STARTUP_FUNCTION(f)                                          \
  static void f##_keep() { dw::addStartupFunction(&f); }                \
  Q_COREAPP_STARTUP_FUNCTION(f##_keep)
#else
#define DW_STARTUP_FUNCTION(f) Q_COREAPP_STARTUP_FUNCTION(f)
#endif
