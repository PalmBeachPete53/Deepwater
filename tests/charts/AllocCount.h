#pragma once
// Counts heap allocations made by the current thread while a NoAlloc scope is alive (operator new is replaced in
// tests/charts/core/alloc_count.cpp). Used to check that order-flow hot paths allocate nothing:
//   { alloccount::Scope s; hotLoop(); CHECK_EQ(s.count(), 0); }
#include <cstddef>

namespace alloccount {
extern thread_local bool g_on;
extern thread_local std::size_t g_count;
struct Scope {
  Scope() { g_count = 0; g_on = true; }
  ~Scope() { g_on = false; }
  std::size_t count() const { return g_count; }
};
}  // namespace alloccount
