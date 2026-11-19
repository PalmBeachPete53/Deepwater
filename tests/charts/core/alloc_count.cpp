// Replaced global operator new / delete for charts_core_tests: counts allocations of the current thread while an
// alloccount::Scope is alive (AllocCount.h). Everything else behaves as the default (malloc / free).
#include "AllocCount.h"
#include <cstdlib>
#include <new>

namespace alloccount {
thread_local bool g_on = false;
thread_local std::size_t g_count = 0;
}  // namespace alloccount

namespace {
void* alloc(std::size_t n) {
  if (alloccount::g_on) ++alloccount::g_count;
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* allocAligned(std::size_t n, std::align_val_t a) {
  if (alloccount::g_on) ++alloccount::g_count;
  const std::size_t al = std::size_t(a);
  void* p = nullptr;
  if (posix_memalign(&p, al < sizeof(void*) ? sizeof(void*) : al, n ? n : 1) != 0) throw std::bad_alloc();
  return p;
}
}  // namespace

void* operator new(std::size_t n) { return alloc(n); }
void* operator new[](std::size_t n) { return alloc(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return alloc(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return alloc(n); } catch (...) { return nullptr; } }
void* operator new(std::size_t n, std::align_val_t a) { return allocAligned(n, a); }
void* operator new[](std::size_t n, std::align_val_t a) { return allocAligned(n, a); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
