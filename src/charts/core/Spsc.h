#pragma once
// A single-producer / single-consumer ring of N fixed slots (N a power of two), lock-free, allocated once.
//
// Memory model (the only exchange between the two threads):
//   producer: writes the slot, then tail.store(t + 1, release)
//   consumer: tail.load(acquire) → sees the slot written before; reads it; then head.store(h + 1, release)
//   producer: head.load(acquire) before reusing a slot → the consumer has finished reading it
// head and tail only grow (size_t wraps after 2^64 operations: never). Neither side ever waits or locks: push()
// fails when full (the producer drops or retries), pop() fails when empty.
// For large slots, beginPush()/commitPush() and front()/popFront() work in place (no copy through a temporary).
#include <atomic>
#include <cstddef>
#include <memory>

namespace charts::flow {

template <class T, size_t N>
class SpscRing {
  static_assert(N >= 2 && (N & (N - 1)) == 0, "N must be a power of two");

public:
  SpscRing() : slots_(new T[N]) {}

  // ---- producer thread
  T* beginPush() {
    const size_t t = tail_.load(std::memory_order_relaxed);
    if (t - head_.load(std::memory_order_acquire) == N) return nullptr;   // full
    return &slots_[t & (N - 1)];
  }
  void commitPush() { tail_.store(tail_.load(std::memory_order_relaxed) + 1, std::memory_order_release); }
  bool push(const T& v) {
    T* s = beginPush();
    if (!s) return false;
    *s = v;
    commitPush();
    return true;
  }

  // ---- consumer thread
  const T* front() const {
    const size_t h = head_.load(std::memory_order_relaxed);
    if (h == tail_.load(std::memory_order_acquire)) return nullptr;      // empty
    return &slots_[h & (N - 1)];
  }
  void popFront() { head_.store(head_.load(std::memory_order_relaxed) + 1, std::memory_order_release); }
  bool pop(T& out) {
    const T* s = front();
    if (!s) return false;
    out = *s;
    popFront();
    return true;
  }

  // either thread (a snapshot, exact only when the other side is idle)
  size_t size() const { return tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire); }
  static constexpr size_t capacity() { return N; }

private:
  std::unique_ptr<T[]> slots_;
  alignas(64) std::atomic<size_t> head_{0};   // next slot to read (consumer)
  alignas(64) std::atomic<size_t> tail_{0};   // next slot to write (producer)
};

}  // namespace charts::flow
