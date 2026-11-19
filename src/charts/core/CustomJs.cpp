// Port of quant-terminal's custom indicator loading (MIT, Copyright (c) 2026 Lewis Jackson). See CustomJs.h.
// JavaScript engine: QuickJS-ng (third_party/quickjs-ng, MIT), one JSRuntime + JSContext per loaded file.
#include "charts/core/CustomJs.h"
#include "charts/core/IndicatorsValidate.h"
#include "charts/core/JsStack.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <quickjs.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace charts::custom {

namespace {
using ind::js::Value;
using Clock = std::chrono::steady_clock;

// Limits of one file's runtime. A calc (or the file's top-level code) running longer than TIMEOUT_SECONDS is stopped
// by the interrupt handler.
constexpr int TIMEOUT_SECONDS = 5;
constexpr size_t MEMORY_LIMIT = size_t(256) << 20;
// The C++ copies of JS arrays (ta inputs, calc results) live outside MEMORY_LIMIT, so their length is capped: at most
// ITEMS_PER_BAR items per bar of the run (MIN_ITEMS at least). A valid indicator gives one value per bar (the format
// check refuses anything else), so only runaway arrays (`new Array(5e6)`) are refused, with an error, before they
// are copied.
constexpr int64_t MIN_ITEMS = 100'000;
constexpr int64_t ITEMS_PER_BAR = 4;
int64_t itemCap(qsizetype bars) { return std::max<int64_t>(MIN_ITEMS, ITEMS_PER_BAR * int64_t(bars)); }
// Nested lists are capped in total too: a calc result (numbers, cheap) at RESULT_TOTAL caps, a value tree (the format
// check's copy, bigger per item) at TREE_TOTAL caps.
constexpr int64_t RESULT_TOTAL = 64, TREE_TOTAL = 8;
// The long C++ loops over JS values look at the clock every this many items.
constexpr int64_t CLOCK_EVERY = 4096;

// The JS stack limit: charts/core/JsStack.h.
using ::charts::js::stackLimit;

QString tooLong(const QString& file) {
  return QString("Custom indicator %1 took too long (stopped after %2 s)").arg(file).arg(TIMEOUT_SECONDS);
}

// Helpers exposed to custom indicator scripts as `ta` (same members, same order, same defaults as the JS).
const char* TA_JS = R"JS((function (n) {
  return {
    sma: (src, len) => n.sma(src, len),
    ema: (src, len) => n.ema(src, len),
    rma: (src, len) => n.rma(src, len),
    wma: (src, len) => n.wma(src, len),
    vwma: (src, vol, len) => n.vwma(src, vol, len),
    hma: (src, len) => n.hma(src, len),
    dema: (src, len) => n.dema(src, len),
    tema: (src, len) => n.tema(src, len),
    alma: (src, len, offset = 0.85, sigma = 6) => n.alma(src, len, offset, sigma),
    linreg: (src, len, offset = 0) => n.linreg(src, len, offset),
    stdev: (src, len) => n.stdev(src, len),
    dev: (src, len) => n.dev(src, len),
    highest: (src, len) => n.highest(src, len),
    lowest: (src, len) => n.lowest(src, len),
    change: (src, len = 1) => n.change(src, len),
    sum: (src, len) => n.sum(src, len),
    rsi: (src, len = 14) => n.rsi(src, len),
    atr: (bars, len = 14) => n.atr(bars, len),
    tr: (bars) => n.tr(bars),
    crossover: (a, b) => n.crossover(a, b),
    crossunder: (a, b) => n.crossunder(a, b),
    compute: (id, bars, params = {}) => n.compute(id, bars, params),
    source: (bars, name = "close") => n.source(bars, name),
    macd: (s, fast = 12, slow = 26, signal = 9) => n.macd(s, fast, slow, signal),
    supertrend: (bars, factor = 3, len = 10) => n.supertrend(bars, factor, len),
    psar: (bars, start = 0.02, inc = 0.02, max = 0.2) => n.psar(bars, start, inc, max),
    dmi: (bars, diLen = 14, adxLen = 14) => n.dmi(bars, diLen, adxLen),
  };
}))JS";

// registerIndicator({ ...mod.default, id }) fills the defaults before validating (same code as the JS).
const char* FULL_JS = R"JS((function (m, id) {
  const def = { ...m, id };
  return { group: "Custom", overlay: true, inputs: [],
    outputs: [{ key: "value", label: def.name || def.id, kind: "line", color: "#ff9800" }], ...def };
}))JS";

// console.log / info / debug / warn / error go to Qt's log (as QJSEngine's console did), one line per call.
const char* CONSOLE_JS = R"JS((function (g, log) {
  const text = (a) => {
    try {
      if (typeof a === "string") return a;
      if (a !== null && typeof a === "object") return JSON.stringify(a) ?? String(a);
      return String(a);
    } catch { return Object.prototype.toString.call(a); }
  };
  const out = (level) => (...args) => log(level, args.map(text).join(" "));
  g.console = { log: out(0), info: out(0), debug: out(0), warn: out(1), error: out(2) };
}))JS";

// An owned JSValue (freed when it goes out of scope).
class Js {
 public:
  Js() = default;
  Js(JSContext* c, JSValue v) : c_(c), v_(v) {}
  Js(const Js&) = delete;
  Js& operator=(const Js&) = delete;
  Js(Js&& o) noexcept : c_(o.c_), v_(o.v_) { o.v_ = JS_UNDEFINED; }
  Js& operator=(Js&& o) noexcept {
    if (this != &o) {
      reset();
      c_ = o.c_;
      v_ = o.v_;
      o.v_ = JS_UNDEFINED;
    }
    return *this;
  }
  ~Js() { reset(); }
  void reset() {
    if (c_) JS_FreeValue(c_, v_);
    v_ = JS_UNDEFINED;
  }
  JSValueConst get() const { return v_; }
  JSValue release() {
    const JSValue v = v_;
    v_ = JS_UNDEFINED;
    return v;
  }
  bool exception() const { return JS_IsException(v_); }

 private:
  JSContext* c_ = nullptr;
  JSValue v_ = JS_UNDEFINED;
};

enum Native {
  Sma, Ema, Rma, Wma, Hma, Dema, Tema, Stdev, Dev, Highest, Lowest, Change, Sum, Rsi,
  Vwma, Alma, Linreg, Atr, Tr, Crossover, Crossunder, Compute, Source, Macd, Supertrend, Psar, Dmi, Log,
};
struct NativeDef { const char* name; int length; Native fn; };
const NativeDef NATIVES[] = {
    {"sma", 2, Sma}, {"ema", 2, Ema}, {"rma", 2, Rma}, {"wma", 2, Wma}, {"vwma", 3, Vwma}, {"hma", 2, Hma},
    {"dema", 2, Dema}, {"tema", 2, Tema}, {"alma", 4, Alma}, {"linreg", 3, Linreg}, {"stdev", 2, Stdev},
    {"dev", 2, Dev}, {"highest", 2, Highest}, {"lowest", 2, Lowest}, {"change", 2, Change}, {"sum", 2, Sum},
    {"rsi", 2, Rsi}, {"atr", 2, Atr}, {"tr", 1, Tr}, {"crossover", 2, Crossover}, {"crossunder", 2, Crossunder},
    {"compute", 3, Compute}, {"source", 2, Source}, {"macd", 4, Macd}, {"supertrend", 3, Supertrend},
    {"psar", 4, Psar}, {"dmi", 3, Dmi},
};

JSValue callNative(JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv, int magic);
int onInterrupt(JSContext* ctx, void* opaque);
char* normalizeImport(JSContext* ctx, const char* base, const char* name, void* opaque);
JSModuleDef* loadImport(JSContext* ctx, const char* name, void* opaque);

// One runtime + context per loaded file, owned by the Def's calc (shared_ptr). Confined to the thread that created
// it (owner): calc refuses other threads; the last owner may free it from any thread (nothing else holds it then).
struct Engine {
  JSRuntime* rt = nullptr;
  JSContext* ctx = nullptr;
  JSValue ta = JS_UNDEFINED, fullFn = JS_UNDEFINED, full = JS_UNDEFINED, calc = JS_UNDEFINED;
  JSAtom aTime = 0, aOpen = 0, aHigh = 0, aLow = 0, aClose = 0, aVolume = 0;
  std::thread::id owner = std::this_thread::get_id();
  QString file;
  QString root;   // canonical folder of the indicator file: the only place its imports may come from
  // The interrupt handler's clock: set by Watch around every run of user code.
  bool watching = false, timedOut = false;
  Clock::time_point deadline;
  struct Cached { JSValueConst js; const Bars* bars; };
  std::vector<Cached> barsStack;   // the bars arrays handed to calc, so ta.source(bars) etc. skip converting them back
  // Set when a conversion met an array (or object) over the item cap: what to report. Reset by Watch.
  QString tooBig;
  // Items the outermost conversion running may still copy (see Budget).
  mutable int64_t left = 0;
  mutable int converting = 0;
  // The total item budget of one outermost conversion (nested ones share it).
  struct Budget {
    const Engine& e;
    Budget(const Engine& e, int64_t total) : e(e) {
      if (e.converting++ == 0) e.left = total;
    }
    ~Budget() { --e.converting; }
    Budget(const Budget&) = delete;
    Budget& operator=(const Budget&) = delete;
  };

  explicit Engine(QString f) : file(std::move(f)) {
    rt = JS_NewRuntime();
    if (!rt) return;
    JS_SetMemoryLimit(rt, MEMORY_LIMIT);
    JS_SetMaxStackSize(rt, stackLimit());
    JS_SetInterruptHandler(rt, onInterrupt, this);
    JS_SetModuleLoaderFunc(rt, normalizeImport, loadImport, this);
    ctx = JS_NewContext(rt);
    if (!ctx) return;
    JS_SetContextOpaque(ctx, this);
    aTime = JS_NewAtom(ctx, "time");
    aOpen = JS_NewAtom(ctx, "open");
    aHigh = JS_NewAtom(ctx, "high");
    aLow = JS_NewAtom(ctx, "low");
    aClose = JS_NewAtom(ctx, "close");
    aVolume = JS_NewAtom(ctx, "volume");
    Js n(ctx, JS_NewObject(ctx));
    for (const NativeDef& d : NATIVES)
      JS_DefinePropertyValueStr(ctx, n.get(), d.name, JS_NewCFunctionMagic(ctx, callNative, d.name, d.length, JS_CFUNC_generic_magic, d.fn),
                                JS_PROP_C_W_E);
    ta = callScript(TA_JS, n.get()).release();
    fullFn = script(FULL_JS).release();
    Js global(ctx, JS_GetGlobalObject(ctx));
    Js log(ctx, JS_NewCFunctionMagic(ctx, callNative, "log", 2, JS_CFUNC_generic_magic, Log));
    callScript(CONSOLE_JS, global.get(), log.get());
  }
  ~Engine() {
    if (!rt) return;
    if (std::this_thread::get_id() != owner) JS_UpdateStackTop(rt);
    if (ctx) {
      for (JSValue v : {ta, fullFn, full, calc}) JS_FreeValue(ctx, v);
      for (JSAtom a : {aTime, aOpen, aHigh, aLow, aClose, aVolume}) JS_FreeAtom(ctx, a);
      JS_RunGC(rt);
      JS_FreeContext(ctx);
    }
    JS_FreeRuntime(rt);
  }
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  bool ok() const { return ctx && JS_IsObject(ta) && JS_IsFunction(ctx, fullFn); }

  // The run's time is up (checked in the C++ loops over JS values, which the interrupt handler doesn't reach). Sets
  // timedOut, so the interrupt handler stops the JS at its next check and the error says "took too long".
  bool expired(int64_t i) const {
    if (timedOut) return true;
    if (i % CLOCK_EVERY != 0 || !watching || Clock::now() <= deadline) return false;
    const_cast<Engine*>(this)->timedOut = true;
    return true;
  }
  // The item cap of a ta call: per bar of the calc running (the innermost), MIN_ITEMS outside calc.
  int64_t taCap() const { return itemCap(barsStack.empty() ? 0 : barsStack.back().bars->size()); }

  Js own(JSValue v) const { return Js(ctx, v); }
  Js script(const char* src) const { return own(JS_Eval(ctx, src, std::strlen(src), "<deepwater>", JS_EVAL_TYPE_GLOBAL)); }
  template <class... A>
  Js callScript(const char* src, A... args) const {
    const Js fn = script(src);
    JSValueConst argv[] = {args...};
    return own(JS_Call(ctx, fn.get(), JS_UNDEFINED, int(sizeof...(A)), argv));
  }

  // Runs the queued promise jobs (a calc may queue some), under the same clock.
  void runJobs() {
    JSContext* c = nullptr;
    while (JS_IsJobPending(rt)) {
      if (JS_ExecutePendingJob(rt, &c) < 0) {
        JS_FreeValue(c, JS_GetException(c));
        if (timedOut) break;
      }
    }
  }

  // ---- JS -> C++. Functions returning bool leave a pending JS exception when they return false.
  QString str(JSValueConst v) const {   // String(v); "" when that throws
    size_t len = 0;
    const char* p = JS_ToCStringLen(ctx, &len, v);
    if (!p) {
      JS_FreeValue(ctx, JS_GetException(ctx));
      return {};
    }
    const QString s = QString::fromUtf8(p, qsizetype(len));
    JS_FreeCString(ctx, p);
    return s;
  }
  QString atomName(JSAtom a) const {
    size_t len = 0;
    const char* p = JS_AtomToCStringLen(ctx, &len, a);
    if (!p) {
      JS_FreeValue(ctx, JS_GetException(ctx));
      return {};
    }
    const QString s = QString::fromUtf8(p, qsizetype(len));
    JS_FreeCString(ctx, p);
    return s;
  }
  Js newString(const QString& s) const {
    const QByteArray u = s.toUtf8();
    return own(JS_NewStringLen(ctx, u.constData(), size_t(u.size())));
  }
  bool number(JSValueConst v, double* out) const { return JS_ToFloat64(ctx, out, v) == 0; }   // Number(v)
  // A length as the JS primitives use Number(len): NaN ("abc", undefined) gives kNaNLength (all-NaN results).
  bool length(JSValueConst v, int* out) const {
    double d;
    if (!number(v, &d)) return false;
    *out = ind::lengthOf(d);
    return true;
  }
  bool arrayLength(JSValueConst a, int64_t* n) const {
    if (JS_GetLength(ctx, a, n) < 0) return false;
    const int64_t cap = taCap();
    if (*n > cap) {
      JS_ThrowRangeError(ctx, "array too long for ta (%lld elements, at most %lld: %lld per bar)", static_cast<long long>(*n),
                         static_cast<long long>(cap), static_cast<long long>(ITEMS_PER_BAR));
      return false;
    }
    *n = std::max<int64_t>(0, *n);
    return true;
  }
  // The pending JS exception of a loop stopped by the clock (the interrupt handler then ends the run).
  bool stopLoop() const {
    JS_ThrowInternalError(ctx, "interrupted");
    return false;
  }
  bool toSeries(JSValueConst a, Series* out) const {
    if (!JS_IsObject(a)) {
      JS_ThrowTypeError(ctx, "expected an array of numbers");
      return false;
    }
    int64_t n;
    if (!arrayLength(a, &n)) return false;
    out->resize(qsizetype(n));
    for (int64_t i = 0; i < n; ++i) {
      if (expired(i)) return stopLoop();
      const Js v = own(JS_GetPropertyInt64(ctx, a, i));
      if (v.exception()) return false;
      double d = NaN;
      if (JS_IsNumber(v.get())) JS_ToFloat64(ctx, &d, v.get());
      (*out)[qsizetype(i)] = d;
    }
    return true;
  }
  bool field(JSValueConst b, JSAtom a, double* out) const {
    const Js v = own(JS_GetProperty(ctx, b, a));
    return !v.exception() && number(v.get(), out);
  }
  bool toBars(JSValueConst a, Bars* out) const {
    for (auto it = barsStack.rbegin(); it != barsStack.rend(); ++it)
      if (JS_IsObject(a) && JS_VALUE_GET_PTR(it->js) == JS_VALUE_GET_PTR(a)) {
        *out = *it->bars;
        return true;
      }
    if (!JS_IsObject(a)) {
      JS_ThrowTypeError(ctx, "expected an array of bars");
      return false;
    }
    int64_t n;
    if (!arrayLength(a, &n)) return false;
    out->resize(qsizetype(n));
    for (int64_t i = 0; i < n; ++i) {
      if (expired(i)) return stopLoop();
      const Js b = own(JS_GetPropertyInt64(ctx, a, i));
      if (b.exception()) return false;
      Bar& x = (*out)[qsizetype(i)];
      if (!JS_IsObject(b.get())) {   // a hole or a non-object reads as a bar of undefined fields
        x = Bar{0, NaN, NaN, NaN, NaN, 0};
        continue;
      }
      double t;
      if (!field(b.get(), aTime, &t) || !field(b.get(), aOpen, &x.open) || !field(b.get(), aHigh, &x.high) ||
          !field(b.get(), aLow, &x.low) || !field(b.get(), aClose, &x.close))
        return false;
      x.time = std::isfinite(t) ? clampToInt64(t) : 0;   // a finite 1e300 is out of qint64's range
      const Js vol = own(JS_GetProperty(ctx, b.get(), aVolume));
      if (vol.exception()) return false;
      if (JS_IsUndefined(vol.get()) || JS_IsNull(vol.get())) x.volume = 0;   // b.volume ?? 0
      else if (!number(vol.get(), &x.volume)) return false;
    }
    return true;
  }

  // ---- C++ -> JS (an exception value when out of memory)
  JSValue fromSeries(const Series& s) const {
    std::vector<JSValue> v(size_t(s.size()));
    for (qsizetype i = 0; i < s.size(); ++i) v[size_t(i)] = JS_NewFloat64(ctx, s[i]);
    return JS_NewArrayFrom(ctx, int(v.size()), v.data());
  }
  JSValue fromBools(const QVector<bool>& s) const {
    std::vector<JSValue> v(size_t(s.size()));
    for (qsizetype i = 0; i < s.size(); ++i) v[size_t(i)] = JS_NewBool(ctx, s[i]);
    return JS_NewArrayFrom(ctx, int(v.size()), v.data());
  }
  JSValue fromStrings(const QStringList& s) const {
    std::vector<JSValue> v(size_t(s.size()));
    for (qsizetype i = 0; i < s.size(); ++i) v[size_t(i)] = newString(s[i]).release();
    return JS_NewArrayFrom(ctx, int(v.size()), v.data());
  }
  JSValue fromBars(const Bars& bars) const {
    std::vector<JSValue> v(size_t(bars.size()), JS_UNDEFINED);
    for (qsizetype i = 0; i < bars.size(); ++i) {
      const Bar& b = bars[i];
      JSValue o = JS_NewObject(ctx);
      if (JS_IsException(o)) {
        for (JSValue x : v) JS_FreeValue(ctx, x);
        return JS_EXCEPTION;
      }
      JS_DefinePropertyValue(ctx, o, aTime, JS_NewFloat64(ctx, double(b.time)), JS_PROP_C_W_E);
      JS_DefinePropertyValue(ctx, o, aOpen, JS_NewFloat64(ctx, b.open), JS_PROP_C_W_E);
      JS_DefinePropertyValue(ctx, o, aHigh, JS_NewFloat64(ctx, b.high), JS_PROP_C_W_E);
      JS_DefinePropertyValue(ctx, o, aLow, JS_NewFloat64(ctx, b.low), JS_PROP_C_W_E);
      JS_DefinePropertyValue(ctx, o, aClose, JS_NewFloat64(ctx, b.close), JS_PROP_C_W_E);
      JS_DefinePropertyValue(ctx, o, aVolume, JS_NewFloat64(ctx, b.volume), JS_PROP_C_W_E);
      v[size_t(i)] = o;
    }
    return JS_NewArrayFrom(ctx, int(v.size()), v.data());
  }
  // An object of named JS values (takes them).
  JSValue object(std::initializer_list<std::pair<const char*, JSValue>> props) const {
    JSValue o = JS_NewObject(ctx);
    for (const auto& p : props) {
      if (JS_IsException(o)) JS_FreeValue(ctx, p.second);
      else if (JS_IsException(p.second) || JS_DefinePropertyValueStr(ctx, o, p.first, p.second, JS_PROP_C_W_E) < 0) {
        JS_FreeValue(ctx, o);
        o = JS_EXCEPTION;
      }
    }
    return o;
  }
  JSValue fromValue(const Value& v) const {
    switch (v.type) {
      case Value::Null: return JS_NULL;
      case Value::Bool: return JS_NewBool(ctx, v.b);
      case Value::Number: return JS_NewFloat64(ctx, v.n);
      case Value::String: return newString(v.s).release();
      case Value::Array: {
        std::vector<JSValue> items;
        items.reserve(size_t(v.items.size()));
        for (const Value& x : v.items) items.push_back(fromValue(x));
        return JS_NewArrayFrom(ctx, int(items.size()), items.data());
      }
      case Value::Object: {
        JSValue o = JS_NewObject(ctx);
        if (JS_IsException(o)) return o;
        for (const auto& p : v.props)
          JS_DefinePropertyValueStr(ctx, o, p.first.toUtf8().constData(), fromValue(p.second), JS_PROP_C_W_E);
        return o;
      }
      default: return JS_UNDEFINED;
    }
  }
  JSValue fromJson(const QJsonValue& v) const { return fromValue(ind::js::fromJson(v)); }

  // Own enumerable string-keyed properties in JS order (integer keys first, then insertion order), like
  // Object.keys. A getter that throws reads as undefined.
  // Both stop (and set tooBig) when the object has more than `cap` items, and stop when the run's time is up.
  template <class F>
  void forEachProp(JSValueConst obj, int64_t cap, F f) const {
    JSPropertyEnum* tab = nullptr;
    uint32_t len = 0;
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
      JS_FreeValue(ctx, JS_GetException(ctx));
      return;
    }
    if (int64_t(len) > cap || (left -= int64_t(len)) < 0) {
      setTooBig(int64_t(len) > cap ? QString("an object with %1 keys").arg(len) : QString(), cap);
      JS_FreePropertyEnum(ctx, tab, len);
      return;
    }
    for (uint32_t i = 0; i < len; ++i) {
      if (expired(i)) break;
      Js v = own(JS_GetProperty(ctx, obj, tab[i].atom));
      if (v.exception()) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        v = Js();
      }
      f(atomName(tab[i].atom), v.get());
    }
    JS_FreePropertyEnum(ctx, tab, len);
  }
  template <class F>
  void forEachItem(JSValueConst arr, int64_t cap, F f) const {
    int64_t n = 0;
    if (JS_GetLength(ctx, arr, &n) < 0) {
      JS_FreeValue(ctx, JS_GetException(ctx));
      return;
    }
    if (n > cap || (left -= std::max<int64_t>(0, n)) < 0) {
      setTooBig(n > cap ? QString("a list of %1 values").arg(n) : QString(), cap);
      return;
    }
    for (int64_t i = 0; i < n; ++i) {
      if (expired(i)) break;
      Js v = own(JS_GetPropertyInt64(ctx, arr, i));
      if (v.exception()) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        v = Js();
      }
      f(v.get());
    }
  }

  // what: the list or object over the cap, or empty when the lists together are over the total.
  void setTooBig(const QString& what, int64_t cap) const {
    if (!tooBig.isEmpty()) return;
    const_cast<Engine*>(this)->tooBig =
        what.isEmpty() ? QStringLiteral(QT_TRANSLATE_NOOP("charts", "the lists returned hold too many values together (one value per bar is expected)"))
                       : QString("%1 is too long for Deepwater: at most %2 (%3 per bar, one value per bar is expected)")
                             .arg(what)
                             .arg(cap)
                             .arg(ITEMS_PER_BAR);
  }

  // A JS value as a value tree (for the format check). Depth-limited: definitions and results are shallow. Arrays and
  // objects over `cap` items are left empty and set tooBig.
  Value toValue(JSValueConst v, int64_t cap = MIN_ITEMS, int depth = 0) const {
    const Budget budget(*this, TREE_TOTAL * cap);
    if (JS_IsUndefined(v)) return Value();
    if (JS_IsNull(v)) return Value::null();
    if (JS_IsBool(v)) return Value::boolean(JS_ToBool(ctx, v) > 0);
    if (JS_IsNumber(v)) {
      double d = NaN;
      JS_ToFloat64(ctx, &d, v);
      return Value::number(d);
    }
    if (JS_IsString(v)) return Value::string(str(v));
    if (!JS_IsObject(v)) return Value::string(str(v));   // BigInt, Symbol: not numbers, not objects (see CustomJs.h)
    if (JS_IsFunction(ctx, v)) return Value::function();
    Value out = JS_IsArray(v) ? Value::array() : Value::object();
    if (depth > 8) return out;
    if (out.type == Value::Array) forEachItem(v, cap, [&](JSValueConst x) { out.items << toValue(x, cap, depth + 1); });
    else forEachProp(v, cap, [&](const QString& k, JSValueConst x) { out.props.append({k, toValue(x, cap, depth + 1)}); });
    return out;
  }
  QJsonValue toJson(JSValueConst v, int64_t cap = MIN_ITEMS) const { return ind::js::toJson(toValue(v, cap)); }

  // What calc returned, as an ind::Result (keys kept in their JS order). cap: itemCap of the bars it ran on.
  ind::Result toResult(JSValueConst res, int64_t cap) const {
    const Budget budget(*this, RESULT_TOTAL * cap);
    ind::Result r;
    if (!JS_IsObject(res)) return r;
    forEachProp(res, cap, [&](const QString& k, JSValueConst v) {
      const bool array = JS_IsArray(v);
      if (k == "markers" && array) {
        QVector<ind::Marker> ms;
        forEachItem(v, cap, [&](JSValueConst m) {
          if (!JS_IsObject(m)) return;
          ind::Marker mk;
          forEachProp(m, cap, [&](const QString& fk, JSValueConst f) {
            if (fk == "time") {
              double t = NaN;
              if (!number(f, &t)) JS_FreeValue(ctx, JS_GetException(ctx));
              mk.time = std::isfinite(t) ? clampToInt64(t) : 0;
            } else if (fk == "position") mk.position = str(f);
            else if (fk == "shape") mk.shape = str(f);
            else if (fk == "color") mk.color = JS_IsString(f) ? str(f) : QString();
            else if (fk == "text") mk.text = JS_IsUndefined(f) || JS_IsNull(f) ? QString() : str(f);
            else {
              const QJsonValue j = toJson(f, cap);
              if (!j.isUndefined()) mk.extra.insert(fk, j);
            }
          });
          ms << mk;
        });
        r.putMarkers(ms);
      } else if ((k == "barColors" || k == "colors") && array) {
        QStringList c;
        forEachItem(v, cap, [&](JSValueConst x) { c << (JS_IsString(x) ? str(x) : QString()); });
        if (k == "colors") r.putColors(c);
        else r.putBarColors(c);
      } else if (k == "breaks" && array) {
        QVector<bool> b;
        forEachItem(v, cap, [&](JSValueConst x) { b << (JS_ToBool(ctx, x) > 0); });
        r.putBreaks(b);
      } else if (array) {
        Series s;
        forEachItem(v, cap, [&](JSValueConst x) {
          double d = NaN;
          if (JS_IsNumber(x)) JS_ToFloat64(ctx, &d, x);
          s << d;
        });
        r.put(k, s);
      } else if (k == "volumeProfile" || k == "sessionProfile") {
        r.note(k);
        (k == "volumeProfile" ? r.volumeProfile : r.sessionProfile) = toJson(v, cap).toObject();
      } else if (k == "error") {
        r.note(k);
        r.error = str(v);
      } else if (k == "pending") {
        r.note(k);
        r.pending = JS_ToBool(ctx, v) > 0;
      } else {
        const QJsonValue j = toJson(v, cap);
        if (!j.isUndefined()) r.putExtra(k, j);
      }
    });
    return r;
  }

  // The message of a thrown value: e.message (undefined for a thrown non-object, as `${e.message}`).
  QString thrownMessage(JSValueConst e) const {
    if (timedOut) return tooLong(file);
    if (!JS_IsObject(e)) return QStringLiteral("undefined");
    const Js m = own(JS_GetPropertyStr(ctx, e, "message"));
    if (m.exception()) {
      JS_FreeValue(ctx, JS_GetException(ctx));
      return QStringLiteral("undefined");
    }
    return JS_IsUndefined(m.get()) ? QStringLiteral("undefined") : str(m.get());
  }
  QString takeError() const {
    const Js e = own(JS_GetException(ctx));
    return thrownMessage(e.get());
  }
  // The error of a file that did not load: its message (a syntax error also says where), or String(e).
  QString loadError(JSValueConst e) const {
    if (timedOut) return tooLong(file);
    QString msg;
    if (JS_IsObject(e)) {
      const Js m = own(JS_GetPropertyStr(ctx, e, "message"));
      if (!m.exception() && !JS_IsUndefined(m.get())) msg = str(m.get());
      const Js name = own(JS_GetPropertyStr(ctx, e, "name"));
      if (!msg.isEmpty() && !name.exception() && str(name.get()) == "SyntaxError") {
        // QuickJS puts the place in the stack: "    at /path/file.js:3:14"
        static const QRegularExpression AT(R"(at [^\n]*:(\d+):(\d+))");
        const Js stack = own(JS_GetPropertyStr(ctx, e, "stack"));
        const QRegularExpressionMatch at = stack.exception() ? QRegularExpressionMatch() : AT.match(str(stack.get()));
        if (at.hasMatch()) msg += QString(" (line %1, column %2)").arg(at.captured(1), at.captured(2));
      }
      if (JS_HasException(ctx)) JS_FreeValue(ctx, JS_GetException(ctx));
    }
    if (msg.isEmpty()) msg = str(e);
    return msg;
  }
  QString takeLoadError() const {
    const Js e = own(JS_GetException(ctx));
    return loadError(e.get());
  }

  JSValue native(Native fn, int argc, JSValueConst* argv);
};

// The interrupt handler's clock around user code. Nested runs (ta.compute of a custom indicator of the same file,
// trial runs inside build) keep the outer deadline.
struct Watch {
  Engine& e;
  bool mine;
  explicit Watch(Engine& e) : e(e), mine(!e.watching) {
    if (!mine) return;
    e.watching = true;
    e.timedOut = false;
    e.tooBig.clear();
    e.deadline = Clock::now() + std::chrono::seconds(TIMEOUT_SECONDS);
  }
  ~Watch() {
    if (mine) e.watching = false;
  }
  Watch(const Watch&) = delete;
  Watch& operator=(const Watch&) = delete;
};

int onInterrupt(JSContext*, void* opaque) {
  Engine* e = static_cast<Engine*>(opaque);
  if (e->watching && Clock::now() > e->deadline) {
    e->timedOut = true;
    return 1;
  }
  return 0;
}

// Imports: paths relative to the importing file ("./x.js", "../x.js") or absolute, read from disk, and only inside the
// indicator's own folder (ADDED, audit 2026-11-18: an indicator file, which can come from anywhere, read any file of
// the disk through an import). Bare names (packages) do not exist here.
bool insideRoot(const Engine* e, const QString& path) {
  if (!e || e->root.isEmpty()) return false;
  const QFileInfo fi(path);
  const QString real = fi.exists() ? fi.canonicalFilePath() : QDir::cleanPath(fi.absoluteFilePath());   // links resolved
  return real.startsWith(e->root + '/');
}
char* normalizeImport(JSContext* ctx, const char* base, const char* name, void* opaque) {
  const QString n = QString::fromUtf8(name);
  QString path;
  if (n.startsWith('/')) path = QDir::cleanPath(n);
  else if (n.startsWith("./") || n.startsWith("../"))
    path = QDir::cleanPath(QFileInfo(QString::fromUtf8(base)).absolutePath() + '/' + n);
  else {
    JS_ThrowReferenceError(ctx, "Cannot find package '%s' (custom indicators can only import files)", name);
    return nullptr;
  }
  if (!insideRoot(static_cast<const Engine*>(opaque), path)) {
    JS_ThrowReferenceError(ctx, "Cannot import '%s': only files of the custom indicators folder can be imported", name);
    return nullptr;
  }
  return js_strdup(ctx, path.toUtf8().constData());
}
JSModuleDef* loadImport(JSContext* ctx, const char* name, void* opaque) {
  if (!insideRoot(static_cast<const Engine*>(opaque), QString::fromUtf8(name))) {   // checked again: a link made meanwhile
    JS_ThrowReferenceError(ctx, "Cannot import '%s': only files of the custom indicators folder can be imported", name);
    return nullptr;
  }
  QFile f(QString::fromUtf8(name));
  if (!f.open(QIODevice::ReadOnly)) {
    JS_ThrowReferenceError(ctx, "Cannot find module '%s'", name);
    return nullptr;
  }
  const QByteArray src = f.readAll();
  const JSValue fn = JS_Eval(ctx, src.constData(), size_t(src.size()), name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
  if (JS_IsException(fn)) return nullptr;
  JSModuleDef* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(fn));
  JS_FreeValue(ctx, fn);
  return m;
}

// ---- The native half of `ta`: each helper converts its arguments, runs the C++ primitive and converts back.
// C++ exceptions never cross QuickJS's C frames: they become JS errors with the same message.
JSValue callNative(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
  Engine* e = static_cast<Engine*>(JS_GetContextOpaque(ctx));
  // the clock is checked around every native call too: QuickJS polls it only every 10,000 checkpoints, and a ta.*
  // call on the calc's own bars never converts a series (where the other check is), so a loop of slow native calls
  // ran far past the 5 s limit on the GUI thread
  auto late = [e] { return e->watching && Clock::now() > e->deadline; };
  if (late()) { e->timedOut = true; return JS_ThrowInternalError(ctx, "interrupted"); }
  try {
    const JSValue r = e->native(Native(magic), argc, argv);
    if (late() && !JS_IsException(r)) {
      JS_FreeValue(ctx, r);
      e->timedOut = true;
      return JS_ThrowInternalError(ctx, "interrupted");
    }
    return r;
  } catch (const std::exception& ex) {
    return JS_ThrowPlainError(ctx, "%s", ex.what());
  } catch (...) {
    return JS_ThrowPlainError(ctx, "unknown error");
  }
}

JSValue Engine::native(Native fn, int argc, JSValueConst* argv) {
  auto arg = [&](int i) -> JSValueConst { return i < argc ? argv[i] : JS_UNDEFINED; };
  Series s;
  Bars b;
  int n = 0;
  switch (fn) {
    case Sma: case Ema: case Rma: case Wma: case Hma: case Dema: case Tema: case Stdev: case Dev:
    case Highest: case Lowest: case Change: case Sum: case Rsi: {
      if (!toSeries(arg(0), &s) || !length(arg(1), &n)) return JS_EXCEPTION;
      switch (fn) {
        case Sma: return fromSeries(ind::sma(s, n));
        case Ema: return fromSeries(ind::ema(s, n));
        case Rma: return fromSeries(ind::rma(s, n));
        case Wma: return fromSeries(ind::wma(s, n));
        case Hma: return fromSeries(ind::hma(s, n));
        case Dema: return fromSeries(ind::dema(s, n));
        case Tema: return fromSeries(ind::tema(s, n));
        case Stdev: return fromSeries(ind::stdev(s, n));
        case Dev: return fromSeries(ind::dev(s, n));
        case Highest: return fromSeries(ind::highest(s, n));
        case Lowest: return fromSeries(ind::lowest(s, n));
        case Change: return fromSeries(ind::change(s, n));
        case Sum: return fromSeries(ind::sum(s, n));
        default: return fromSeries(ind::rsi(s, n));
      }
    }
    case Vwma: {
      Series v;
      if (!toSeries(arg(0), &s) || !toSeries(arg(1), &v) || !length(arg(2), &n)) return JS_EXCEPTION;
      return fromSeries(ind::vwma(s, v, n));
    }
    case Alma: {
      double offset, sigma;
      if (!toSeries(arg(0), &s) || !length(arg(1), &n) || !number(arg(2), &offset) || !number(arg(3), &sigma)) return JS_EXCEPTION;
      return fromSeries(ind::alma(s, n, offset, sigma));
    }
    case Linreg: {
      double offset;   // a plain number in the JS formula (NaN gives NaN)
      if (!toSeries(arg(0), &s) || !length(arg(1), &n) || !number(arg(2), &offset)) return JS_EXCEPTION;
      return fromSeries(ind::linreg(s, n, offset));
    }
    case Atr:
      if (!toBars(arg(0), &b) || !length(arg(1), &n)) return JS_EXCEPTION;
      return fromSeries(ind::atr(b, n));
    case Tr:
      if (!toBars(arg(0), &b)) return JS_EXCEPTION;
      return fromSeries(ind::tr(b));
    case Crossover:
    case Crossunder: {   // b is an array or a number (val(b, i) in the JS)
      if (!toSeries(arg(0), &s)) return JS_EXCEPTION;
      if (JS_IsArray(arg(1))) {
        Series y;
        if (!toSeries(arg(1), &y)) return JS_EXCEPTION;
        return fromBools(fn == Crossover ? ind::crossover(s, y) : ind::crossunder(s, y));
      }
      double y;
      if (!number(arg(1), &y)) return JS_EXCEPTION;
      return fromBools(fn == Crossover ? ind::crossover(s, y) : ind::crossunder(s, y));
    }
    case Compute: {
      const QString id = str(arg(0));
      if (!toBars(arg(1), &b)) return JS_EXCEPTION;
      const QJsonObject params = toJson(arg(2)).toObject();
      if (!tooBig.isEmpty()) {   // a runaway params object: refused like a runaway ta input
        const QByteArray m = tooBig.toUtf8();
        tooBig.clear();
        return JS_ThrowRangeError(ctx, "%s", m.constData());
      }
      const ind::Result r = ind::compute(id, b, params);   // throws -> callNative makes it a JS error
      return fromValue(ind::js::fromResult(r));
    }
    case Source:
      if (!toBars(arg(0), &b)) return JS_EXCEPTION;
      return fromSeries(ind::source(b, JS_IsString(arg(1)) ? str(arg(1)) : QStringLiteral("close")));
    case Macd: {
      int slow, signal;
      if (!toSeries(arg(0), &s) || !length(arg(1), &n) || !length(arg(2), &slow) || !length(arg(3), &signal)) return JS_EXCEPTION;
      const ind::Macd m = ind::macdFrom(s, n, slow, signal);
      return object({{"macd", fromSeries(m.macd)}, {"signal", fromSeries(m.signal)}, {"hist", fromSeries(m.hist)},
                     {"colors", fromStrings(m.colors)}});
    }
    case Supertrend: {
      double factor;
      if (!toBars(arg(0), &b) || !number(arg(1), &factor) || !length(arg(2), &n)) return JS_EXCEPTION;
      const ind::Supertrend st = ind::supertrendOf(b, factor, n);
      return object({{"st", fromSeries(st.st)}, {"dir", fromSeries(st.dir)}});
    }
    case Psar: {
      double start, inc, max;
      if (!toBars(arg(0), &b) || !number(arg(1), &start) || !number(arg(2), &inc) || !number(arg(3), &max)) return JS_EXCEPTION;
      return fromSeries(ind::psarOf(b, start, inc, max));
    }
    case Dmi: {
      int adxLen;
      if (!toBars(arg(0), &b) || !length(arg(1), &n) || !length(arg(2), &adxLen)) return JS_EXCEPTION;
      const ind::Dmi d = ind::dmiOf(b, n, adxLen);
      return object({{"plus", fromSeries(d.plus)}, {"minus", fromSeries(d.minus)}, {"adx", fromSeries(d.adx)}});
    }
    case Log: {
      double level = 0;
      number(arg(0), &level);
      const QByteArray line = (file + ": " + str(arg(1))).toUtf8();
      if (level >= 2) qCritical("%s", line.constData());
      else if (level >= 1) qWarning("%s", line.constData());
      else qDebug("%s", line.constData());
      return JS_UNDEFINED;
    }
  }
  return JS_UNDEFINED;
}

// calc(bars, p, ta) with this = the definition, as def.calc(bars, p, ta) in the JS. Takes p. On success the caller
// owns value.
struct CallResult { Js value; bool threw = false; QString error; };
CallResult callCalc(Engine& e, const Bars& bars, JSValue p) {
  Watch w(e);
  CallResult out;
  const Js param = e.own(p);
  const Js jb = e.own(e.fromBars(bars));
  if (jb.exception() || param.exception()) {
    out.threw = true;
    out.error = e.takeError();
    return out;
  }
  e.barsStack.push_back({jb.get(), &bars});
  JSValueConst args[3] = {jb.get(), param.get(), e.ta};
  Js r = e.own(JS_Call(e.ctx, e.calc, e.full, 3, args));
  e.barsStack.pop_back();
  if (r.exception()) {
    out.threw = true;
    out.error = e.takeError();
    return out;
  }
  e.runJobs();
  out.value = std::move(r);
  return out;
}

// Runs the file as an ES module and gives its default export (undefined when it has none).
bool loadModule(Engine& e, const QString& path, Js* def, QString* error) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    *error = QString("Cannot find module '%1'").arg(QFileInfo(path).absoluteFilePath());
    return false;
  }
  const QByteArray src = f.readAll();   // NUL-terminated, as JS_Eval needs
  const QByteArray name = QFileInfo(path).absoluteFilePath().toUtf8();
  Watch w(e);
  const JSValue fn = JS_Eval(e.ctx, src.constData(), size_t(src.size()), name.constData(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
  if (JS_IsException(fn)) {
    *error = e.takeLoadError();
    return false;
  }
  JSModuleDef* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(fn));
  const Js done = e.own(JS_EvalFunction(e.ctx, fn));   // a promise (top-level await)
  if (done.exception()) {
    *error = e.takeLoadError();
    return false;
  }
  e.runJobs();
  switch (JS_PromiseState(e.ctx, done.get())) {
    case JS_PROMISE_REJECTED: {
      const Js reason = e.own(JS_PromiseResult(e.ctx, done.get()));
      *error = e.loadError(reason.get());
      return false;
    }
    case JS_PROMISE_PENDING:
      *error = e.timedOut ? tooLong(e.file) : QStringLiteral(QT_TRANSLATE_NOOP("charts", "the file's top-level code did not finish (an await that never ends)"));
      return false;
    default: break;
  }
  const Js ns = e.own(JS_GetModuleNamespace(e.ctx, m));
  if (ns.exception()) {
    *error = e.takeLoadError();
    return false;
  }
  *def = e.own(JS_GetPropertyStr(e.ctx, ns.get(), "default"));
  if (def->exception()) {
    *error = e.takeLoadError();
    return false;
  }
  return true;
}

// The Def a validated definition object stands for.
ind::Def toDef(const Value& full, const QString& file, std::shared_ptr<Engine> eng) {
  ind::Def d;
  d.id = full.get("id").s;
  d.name = full.get("name").s;
  const Value& group = full.get("group");
  d.group = group.type == Value::String ? group.s : QString();
  d.overlay = full.get("overlay").truthy();
  for (const Value& x : full.get("inputs").items) {
    ind::Input i;
    i.key = x.get("key").s;
    i.label = x.get("label").type == Value::String ? x.get("label").s : QString();
    i.type = x.get("type").s;
    i.def = ind::js::toJson(x.get("default"));
    if (i.def.isUndefined()) i.def = QJsonValue(QJsonValue::Null);
    if (x.get("min").type == Value::Number) i.min = x.get("min").n;
    if (x.get("max").type == Value::Number) i.max = x.get("max").n;
    if (x.get("step").type == Value::Number) i.step = x.get("step").n;
    for (const Value& o : x.get("options").items) i.options << o.s;
    d.inputs << i;
  }
  for (const Value& x : full.get("outputs").items) {
    ind::Output o;
    for (const auto& p : x.props) {
      if (p.first == "key") o.key = p.second.s;
      else if (p.first == "label") o.label = p.second.type == Value::String ? p.second.s : QString();
      else if (p.first == "kind") o.kind = p.second.type == Value::String ? p.second.s : QStringLiteral("line");
      else if (p.first == "color") o.color = p.second.s;
      else if (p.first == "lineWidth") o.lineWidth = p.second.type == Value::Number ? int(p.second.n) : 0;
      else {
        const QJsonValue j = ind::js::toJson(p.second);
        if (!j.isUndefined()) o.extra.insert(p.first, j);
      }
    }
    d.outputs << o;
  }
  for (const Value& l : full.get("levels").items) d.levels << l.n;
  static const QStringList known = {"id", "name", "group", "overlay", "inputs", "outputs", "levels", "calc"};
  for (const auto& p : full.props) {
    if (known.contains(p.first)) continue;
    const QJsonValue j = ind::js::toJson(p.second);
    if (!j.isUndefined()) d.extra.insert(p.first, j);
  }
  d.custom = true;
  d.file = file;
  d.calc = [eng](const Bars& bars, const Params& p) -> ind::Result {
    if (std::this_thread::get_id() != eng->owner) throw std::runtime_error("Custom indicators run on the GUI thread only");
    Watch w(*eng);
    const CallResult c = callCalc(*eng, bars, eng->fromJson(p));
    if (c.threw) throw std::runtime_error(c.error.toStdString());
    ind::Result r = eng->toResult(c.value.get(), itemCap(bars.size()));
    if (eng->timedOut) throw std::runtime_error(tooLong(eng->file).toStdString());   // a getter in the result looped
    if (!eng->tooBig.isEmpty()) throw std::runtime_error(eng->tooBig.toStdString());
    return r;
  };
  return d;
}

}  // namespace

QString idFor(const QString& fileName) {
  QString f = fileName;
  if (f.endsWith(".js")) f.chop(3);
  return "custom:" + f;
}

ind::Def build(const QString& path, Loaded* info) {
  Loaded l;
  l.file = QFileInfo(path).fileName();
  l.id = idFor(l.file);
  auto done = [&](ind::Def d) {
    if (info) *info = l;
    return d;
  };
  auto eng = std::make_shared<Engine>(l.file);
  eng->root = QFileInfo(QFileInfo(path).absolutePath()).canonicalFilePath();
  if (!eng->ok()) {
    l.error = QStringLiteral(QT_TRANSLATE_NOOP("charts", "the JavaScript engine could not start"));
    return done({});
  }
  Js def;
  if (!loadModule(*eng, path, &def, &l.error)) return done({});
  Value full;
  {
    Watch w(*eng);
    const Js id = eng->newString(l.id);
    JSValueConst args[2] = {def.get(), id.get()};
    Js f = eng->own(JS_Call(eng->ctx, eng->fullFn, JS_UNDEFINED, 2, args));   // { ...mod.default, id } can throw
    if (f.exception()) {
      l.error = eng->takeLoadError();
      return done({});
    }
    Js calc = eng->own(JS_GetPropertyStr(eng->ctx, f.get(), "calc"));
    if (calc.exception()) {
      l.error = eng->takeLoadError();
      return done({});
    }
    eng->full = f.release();
    eng->calc = calc.release();
    full = eng->toValue(eng->full);
    if (eng->timedOut) {
      l.error = tooLong(l.file);
      return done({});
    }
  }
  l.problems = ind::js::indicatorProblems(full, [&](const Bars& bars, const Value& p) {
    ind::js::Trial t;
    Watch w(*eng);
    const CallResult c = callCalc(*eng, bars, eng->fromValue(p));
    t.threw = c.threw;
    t.error = c.error;
    if (!c.threw) t.result = eng->toValue(c.value.get(), itemCap(bars.size()));
    if (!eng->tooBig.isEmpty() && !eng->timedOut) {   // refused before the copy: an error of the trial run
      t.threw = true;
      t.error = eng->tooBig;
      t.result = Value();
    }
    return t;
  });
  if (eng->timedOut) {   // not a format problem: say so plainly
    l.problems.clear();
    l.error = tooLong(l.file);
    return done({});
  }
  if (!l.problems.isEmpty()) {
    l.error = ind::formatProblems(l.problems);
    return done({});
  }
  return done(toDef(full, l.file, eng));
}

Loaded loadFile(const QString& path) {
  Loaded l;
  const ind::Def d = build(path, &l);
  if (!l.ok()) return l;
  // build() ran the format check with its trial calc on the JS definition: don't run calc a second time here.
  const QStringList pr = ind::registerIndicator(d, /*run=*/false);
  if (!pr.isEmpty()) {
    l.problems = pr;
    l.error = pr.size() == 1 && pr[0] == QT_TRANSLATE_NOOP("charts", "A custom indicator needs an id") ? pr[0] : ind::formatProblems(pr);
  }
  return l;
}

QList<Loaded> loadFolder(const QString& dir) {
  QList<Loaded> out;
  const QStringList files = QDir(dir).entryList({"*.js"}, QDir::Files, QDir::Name);
  for (const QString& f : files) out << loadFile(QDir(dir).filePath(f));
  return out;
}

QStringList check(const QString& source, const QString& fileName) {
  QTemporaryDir tmp;
  if (!tmp.isValid()) return {QStringLiteral(QT_TRANSLATE_NOOP("charts", "could not write a temporary file"))};
  const QString path = tmp.filePath(QFileInfo(fileName).fileName().isEmpty() ? QStringLiteral("check.js") : QFileInfo(fileName).fileName());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return {QStringLiteral(QT_TRANSLATE_NOOP("charts", "could not write a temporary file"))};
  f.write(source.toUtf8());
  f.close();
  Loaded l;
  build(path, &l);
  if (!l.problems.isEmpty()) return l.problems;
  if (!l.error.isEmpty()) return {l.error};
  return {};
}

}  // namespace charts::custom
