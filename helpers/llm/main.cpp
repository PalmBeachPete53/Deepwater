// deepwater-llm — runs ONE local GGUF model for ONE AI › Agents environment.
//
// The app starts one process per lit cube, inside a macOS sandbox (no network, reads only its model file and the app
// bundle, writes only in its environment folder): see src/ai/Engine.cpp. The model is loaded at start-up.
//
//   deepwater-llm --model <file.gguf> [--ctx <tokens>]
//
// Protocol: JSON lines. stdout carries events, stdin carries commands; stderr is a log the app keeps for errors.
//   → {"event":"ready","ctx":8192,"train_ctx":32768,"desc":"qwen2 0.5B Q4_K - Medium","params":494032768,"template":"model|chatml|none",
//      "device":"MTL0 · Apple M3 Pro"}
//   → {"event":"error","code":"load|context","message":"…"} (start-up failure: the process exits after it)
//   ← {"cmd":"chat","id":3,"messages":[{"role":"system|user|assistant","content":"…"}],
//      "max_tokens":1024,"temperature":0.7,"top_p":0.95,"top_k":40,"min_p":0.05,"repeat_penalty":1.0}   (1.0: off; a penalty made Qwen3 drop the figure it had just reasoned about)
//   → {"event":"token","id":3,"text":"…"}                   (whole UTF-8 characters only)
//   → {"event":"done","id":3,"reason":"eog|length|stopped|context|error","prompt_tokens":…,"cached_tokens":…,
//      "gen_tokens":…,"dropped_messages":…,"prompt_ms":…,"gen_ms":…,"message":"…"}
//   ← {"cmd":"stop","id":3}                                  (ends answer 3, or skips it if it has not started yet;
//                                                             without "id": the current answer)
//   stdin closed → the process exits.
//
// The KV cache is kept between answers: a new request that starts with the previous tokens (same conversation, one more
// turn) only decodes what is new. When a conversation no longer fits in the context, the oldest turns after the system
// message are dropped (reported in "dropped_messages").
#include "llama.h"
#include "ggml-backend.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace {
std::mutex g_out;
void emit(const json& j) {
  std::lock_guard<std::mutex> lock(g_out);
  std::string s = j.dump(-1, ' ', false, json::error_handler_t::replace);
  std::fwrite(s.data(), 1, s.size(), stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
}

std::atomic<bool> g_stop{false};   // stdin closed: every answer ends
// A stop names the request it ends: a bare flag set by the reader was cleared by chat() when the request it was meant
// for had not started yet (the stop read just after its chat line), so that answer ran to the end. Ids only grow.
std::atomic<int> g_current{0};     // the request being answered
std::atomic<int> g_stopId{0};      // every request with an id up to this one is stopped
bool stopped(int id) { return g_stop.load() || (id > 0 && id <= g_stopId.load()); }
std::mutex g_qm;
std::condition_variable g_qc;
std::deque<json> g_queue;
bool g_eof = false;

void readerThread() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    json j = json::parse(line, nullptr, false);
    if (j.is_discarded() || !j.is_object()) continue;
    const std::string cmd = j.value("cmd", "");
    if (cmd == "stop") {
      const int want = j.value("id", 0) > 0 ? j.value("id", 0) : g_current.load();
      int cur = g_stopId.load();
      while (want > cur && !g_stopId.compare_exchange_weak(cur, want)) {}
      continue;
    }
    std::lock_guard<std::mutex> lock(g_qm);
    g_queue.push_back(std::move(j));
    g_qc.notify_one();
  }
  g_stop = true;
  std::lock_guard<std::mutex> lock(g_qm);
  g_eof = true;
  g_qc.notify_one();
}

// length of the longest prefix of `s` made of complete UTF-8 characters
size_t completeUtf8(const std::string& s) {
  size_t i = 0, n = s.size(), ok = 0;
  while (i < n) {
    const unsigned char c = (unsigned char)s[i];
    size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    if (i + len > n) break;
    i += len; ok = i;
  }
  return ok;
}

std::string piece(const llama_vocab* vocab, llama_token t) {
  char buf[256];
  int n = llama_token_to_piece(vocab, t, buf, sizeof buf, 0, false);
  if (n < 0) {
    std::string big(size_t(-n), '\0');
    n = llama_token_to_piece(vocab, t, big.data(), int(big.size()), 0, false);
    return n > 0 ? big.substr(0, size_t(n)) : std::string();
  }
  return std::string(buf, size_t(n));
}

std::vector<llama_token> tokenize(const llama_vocab* vocab, const std::string& text, bool addSpecial) {
  int n = -llama_tokenize(vocab, text.data(), int(text.size()), nullptr, 0, addSpecial, true);
  std::vector<llama_token> out(size_t(std::max(n, 0)));
  if (n > 0 && llama_tokenize(vocab, text.data(), int(text.size()), out.data(), n, addSpecial, true) < 0) out.clear();
  return out;
}

struct Model {
  llama_model* model = nullptr;
  llama_context* ctx = nullptr;
  const llama_vocab* vocab = nullptr;
  std::string tmpl;          // GGUF chat template (empty: none)
  std::string tmplName;      // what the ready event reports
  std::vector<llama_token> cached;   // tokens currently in the KV cache (sequence 0)
};

// Formats the conversation with the model's chat template; models without one get a plain transcript.
std::string format(const Model& m, const std::vector<std::pair<std::string, std::string>>& msgs) {
  std::vector<llama_chat_message> chat;
  for (const auto& [r, c] : msgs) chat.push_back({r.c_str(), c.c_str()});
  if (!m.tmpl.empty() || m.tmplName == "chatml") {
    const char* t = m.tmpl.empty() ? "chatml" : m.tmpl.c_str();
    size_t cap = 1024;
    for (const auto& [r, c] : msgs) cap += r.size() + c.size() + 64;
    std::string buf(cap * 2, '\0');
    int n = llama_chat_apply_template(t, chat.data(), chat.size(), true, buf.data(), int(buf.size()));
    if (n > int(buf.size())) { buf.resize(size_t(n)); n = llama_chat_apply_template(t, chat.data(), chat.size(), true, buf.data(), n); }
    if (n >= 0) return buf.substr(0, size_t(n));
  }
  std::string s;
  for (const auto& [r, c] : msgs) s += (r == "user" ? "User: " : r == "assistant" ? "Assistant: " : "") + c + "\n\n";
  return s + "Assistant:";
}

bool abortCb(void*) { return stopped(g_current.load()); }

void chat(Model& m, const json& req) {
  const int id = req.value("id", 0);
  g_current = id;
  json done{{"event", "done"}, {"id", id}};
  auto fail = [&](const std::string& reason, const std::string& msg) {
    done["reason"] = reason; done["message"] = msg; emit(done);
  };
  if (stopped(id)) return fail("stopped", "");   // stopped before it started
  std::vector<std::pair<std::string, std::string>> msgs;
  for (const auto& x : req.value("messages", json::array()))
    if (x.is_object()) msgs.emplace_back(x.value("role", "user"), x.value("content", ""));
  if (msgs.empty()) return fail("error", "no messages");

  const int nCtx = int(llama_n_ctx(m.ctx));
  const int maxTokens = std::clamp(req.value("max_tokens", 1024), 16, nCtx / 2);
  // drop the oldest turns (never the system message nor the last user message) until the prompt fits
  std::vector<llama_token> prompt;
  int dropped = 0;
  for (;;) {
    prompt = tokenize(m.vocab, format(m, msgs), true);
    if (int(prompt.size()) + maxTokens <= nCtx) break;
    const size_t first = msgs.front().first == "system" ? 1 : 0;
    if (msgs.size() <= first + 1) return fail("context", "the message alone does not fit in the context");
    msgs.erase(msgs.begin() + long(first)); ++dropped;
  }
  done["dropped_messages"] = dropped;
  done["prompt_tokens"] = int(prompt.size());

  // reuse the KV cache for the common prefix; keep at least one token to decode so there are logits to sample
  size_t common = 0;
  while (common < m.cached.size() && common < prompt.size() && m.cached[common] == prompt[common]) ++common;
  if (common == prompt.size()) --common;
  llama_memory_t mem = llama_get_memory(m.ctx);
  if (!llama_memory_seq_rm(mem, 0, llama_pos(common), -1)) { llama_memory_clear(mem, true); common = 0; }
  m.cached.resize(common);
  done["cached_tokens"] = int(common);

  const auto t0 = std::chrono::steady_clock::now();
  const int nBatch = int(llama_n_batch(m.ctx));
  for (size_t i = common; i < prompt.size(); i += size_t(nBatch)) {
    const int n = int(std::min(prompt.size() - i, size_t(nBatch)));
    if (llama_decode(m.ctx, llama_batch_get_one(prompt.data() + i, n)) != 0) {
      llama_memory_clear(mem, true); m.cached.clear();
      return fail(stopped(id) ? "stopped" : "error", stopped(id) ? "" : "decode failed while reading the prompt");
    }
    m.cached.insert(m.cached.end(), prompt.begin() + long(i), prompt.begin() + long(i) + n);
  }
  const auto t1 = std::chrono::steady_clock::now();

  llama_sampler* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
  const float rp = req.value("repeat_penalty", 1.0f);
  if (rp != 1.0f) llama_sampler_chain_add(smpl, llama_sampler_init_penalties(llama_vocab_n_tokens(m.vocab), 64, rp, 0.0f, 0.0f));
  const float temp = req.value("temperature", 0.7f);
  if (temp <= 0.0f) {
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
  } else {
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(req.value("top_k", 40)));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(req.value("top_p", 0.95f), 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(req.value("min_p", 0.05f), 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(temp));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count())));
  }

  std::string pending;   // bytes of a UTF-8 character split across tokens
  int gen = 0;
  std::string reason = "length";
  while (gen < maxTokens) {
    if (stopped(id)) { reason = "stopped"; break; }
    if (int(m.cached.size()) >= nCtx) { reason = "context"; break; }
    llama_token t = llama_sampler_sample(smpl, m.ctx, -1);
    if (llama_vocab_is_eog(m.vocab, t)) { reason = "eog"; break; }
    ++gen;
    pending += piece(m.vocab, t);
    const size_t ok = completeUtf8(pending);
    if (ok > 0) { emit({{"event", "token"}, {"id", id}, {"text", pending.substr(0, ok)}}); pending.erase(0, ok); }
    if (llama_decode(m.ctx, llama_batch_get_one(&t, 1)) != 0) { reason = stopped(id) ? "stopped" : "error"; break; }
    m.cached.push_back(t);
  }
  if (!pending.empty()) emit({{"event", "token"}, {"id", id}, {"text", pending}});
  llama_sampler_free(smpl);
  const auto t2 = std::chrono::steady_clock::now();
  done["reason"] = reason;
  done["gen_tokens"] = gen;
  done["prompt_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
  done["gen_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  emit(done);
}
}  // namespace

int main(int argc, char** argv) {
  std::string path;
  int ctxWanted = 8192;
  for (int i = 1; i + 1 < argc; ++i) {
    if (!std::strcmp(argv[i], "--model")) path = argv[++i];
    else if (!std::strcmp(argv[i], "--ctx")) ctxWanted = std::atoi(argv[++i]);
  }
  if (path.empty()) { emit({{"event", "error"}, {"message", "no --model given"}}); return 2; }

  // llama.cpp logs go to stderr, errors and warnings only
  llama_log_set([](ggml_log_level level, const char* text, void*) {
    if (level >= GGML_LOG_LEVEL_WARN) std::fputs(text, stderr);
  }, nullptr);
  llama_backend_init();

  Model m;
  llama_model_params mp = llama_model_default_params();
  mp.n_gpu_layers = 999;   // everything on the GPU when there is one (Metal on Apple silicon)
  m.model = llama_model_load_from_file(path.c_str(), mp);
  if (!m.model) { emit({{"event", "error"}, {"code", "load"}, {"message", "could not load the model file"}}); return 1; }
  m.vocab = llama_model_get_vocab(m.model);

  char meta[256] = {0};
  int trainCtx = 0;
  {
    char arch[64] = {0};
    if (llama_model_meta_val_str(m.model, "general.architecture", arch, sizeof arch) > 0) {
      const std::string key = std::string(arch) + ".context_length";
      if (llama_model_meta_val_str(m.model, key.c_str(), meta, sizeof meta) > 0) trainCtx = std::atoi(meta);
    }
  }
  llama_context_params cp = llama_context_default_params();
  cp.n_ctx = uint32_t(std::max(512, trainCtx > 0 ? std::min(ctxWanted, trainCtx) : ctxWanted));
  cp.n_batch = 512;
  const unsigned hw = std::thread::hardware_concurrency();
  cp.n_threads = cp.n_threads_batch = int(std::max(1u, std::min(8u, hw > 2 ? hw - 2 : 1u)));
  m.ctx = llama_init_from_model(m.model, cp);
  if (!m.ctx) { emit({{"event", "error"}, {"code", "context"}, {"message", "not enough memory for the context"}}); return 1; }
  llama_set_abort_callback(m.ctx, abortCb, nullptr);

  if (const char* t = llama_model_chat_template(m.model, nullptr)) {
    m.tmpl = t;
    // libllama only knows a fixed list of templates: test this one, fall back to ChatML when it is unknown
    llama_chat_message probe{"user", "hi"};
    char buf[512];
    if (llama_chat_apply_template(m.tmpl.c_str(), &probe, 1, true, buf, sizeof buf) < 0) { m.tmpl.clear(); m.tmplName = "chatml"; }
    else m.tmplName = "model";
  } else {
    m.tmplName = "none";
  }

  // where the weights run: the first GPU device when there is one (Metal on Apple silicon), else the CPU
  std::string device = "CPU";
  for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
    ggml_backend_dev_t d = ggml_backend_dev_get(i);
    if (ggml_backend_dev_type(d) == GGML_BACKEND_DEVICE_TYPE_GPU) { device = std::string(ggml_backend_dev_name(d)) + " · " + ggml_backend_dev_description(d); break; }
  }
  char desc[256] = {0};
  llama_model_desc(m.model, desc, sizeof desc);
  emit({{"event", "ready"}, {"ctx", int(llama_n_ctx(m.ctx))}, {"train_ctx", trainCtx}, {"desc", desc},
        {"params", llama_model_n_params(m.model)}, {"template", m.tmplName}, {"device", device}});

  std::thread reader(readerThread);
  reader.detach();
  for (;;) {
    json req;
    {
      std::unique_lock<std::mutex> lock(g_qm);
      g_qc.wait(lock, [] { return !g_queue.empty() || g_eof; });
      if (g_queue.empty()) break;
      req = std::move(g_queue.front());
      g_queue.pop_front();
    }
    if (req.value("cmd", "") == "chat") chat(m, req);
  }
  llama_free(m.ctx);
  llama_model_free(m.model);
  std::_Exit(0);   // the reader thread may still be blocked on stdin
}
