// See JsStack.h. Moved from CustomJs.cpp (2026-11-18) so Pine.cpp shares it.
#include "charts/core/JsStack.h"
#include <QThread>
#include <algorithm>
#include <cstdint>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace charts::js {
size_t stackLimit() {
  constexpr size_t KB = 1024, MARGIN = 256 * KB, MAX = 8192 * KB, FALLBACK = 512 * KB;
  char here = 0;
  const auto sp = reinterpret_cast<uintptr_t>(&here);
  uintptr_t low = 0;
#if defined(__APPLE__)
  const pthread_t t = pthread_self();
  low = reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(t)) - pthread_get_stacksize_np(t);
#elif defined(_WIN32) && _WIN32_WINNT >= 0x0602   // GetCurrentThreadStackLimits: Windows 8 and later
  ULONG_PTR lo = 0, hi = 0;
  GetCurrentThreadStackLimits(&lo, &hi);
  low = uintptr_t(lo);
#elif defined(__linux__)
  pthread_attr_t a;
  if (pthread_getattr_np(pthread_self(), &a) == 0) {
    void* addr = nullptr;
    size_t size = 0;
    if (pthread_attr_getstack(&a, &addr, &size) == 0) low = reinterpret_cast<uintptr_t>(addr);
    pthread_attr_destroy(&a);
  }
#endif
  if (low == 0 || sp <= low) return FALLBACK;
  const size_t left = sp - low;
  return left <= 2 * MARGIN ? left / 2 : std::min(left - MARGIN, MAX);
}

void withJsStack(const std::function<void()>& fn) {
  if (stackLimit() >= (size_t(4) << 20)) { fn(); return; }
  QThread* t = QThread::create(fn);
  t->setObjectName(QStringLiteral("js-stack"));
  t->setStackSize(16u << 20);
  t->start();
  t->wait();
  delete t;
}
}  // namespace charts::js
