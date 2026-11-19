#pragma once
// The JS stack limit of a QuickJS runtime made on the calling thread: what is left of this thread's stack (8 MB on the
// macOS main thread, 1 MB on Windows', 512 KB on a macOS secondary thread) minus a margin for the C++ frames, at most
// 8 MB (about 8000 nested JS calls; Node allows about 10000). QuickJS measures from where the runtime was created, so
// the runtime must be made on the thread it runs on. Shared by the custom indicators (CustomJs) and Pine (Pine).
#include <cstddef>
#include <functional>

namespace charts::js {
size_t stackLimit();
// Runs fn on this thread when its stack leaves at least 4 MB to the JS limit, else on a new thread with a 16 MB stack
// and waits for it (audit D19: Pine indicators inside rule strategies ran on pool threads of 512 KB and failed with
// "Maximum call stack size exceeded" in the tester, the quant checks and /api/backtest). fn must not need this
// thread's thread_local state.
void withJsStack(const std::function<void()>& fn);
}
