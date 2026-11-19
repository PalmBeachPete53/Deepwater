// Pine Script® runs through PineTS (third_party/pinets, AGPL-3.0) in QuickJS-ng. See Pine.h.
#include "charts/core/Pine.h"
#include "charts/core/JsStack.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>
#include <QTimeZone>
#include <quickjs.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace charts::pine {

namespace {
using Clock = std::chrono::steady_clock;
constexpr int MAX_LOG = 1000;

// Runs started by runTracked (runAsync, the script loads) and the quit flag every run's interrupt handler reads (audit
// G7: the threads were never joined, so a run still going at quit was destroyed with the app around it).
std::atomic<bool> g_quitting{false};
std::mutex g_runsMu;
std::condition_variable g_runsCv;
int g_runs = 0;   // under g_runsMu

void watchQuit() {
  static std::once_flag once;
  std::call_once(once, [] {
    if (QCoreApplication* app = QCoreApplication::instance())
      QObject::connect(app, &QCoreApplication::aboutToQuit, app, [] { stopRunsForQuit(2000); });
  });
}

// The embedded PineTS build (resource :/pinets/pinets.js, from third_party/pinets). Read once.
const QByteArray& bundle() {
  static const QByteArray b = [] {
    QFile f(QStringLiteral(":/pinets/pinets.js"));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  }();
  return b;
}

// What QuickJS lacks (see Pine.h): console, setTimeout, Intl.DateTimeFormat. Called with the natives N:
// N.log(level, text), N.tz(zone, ms) → [year, month, day, hour, minute, second, weekday, dst, offsetSeconds] | null.
const char* const PRELUDE = R"JS((function (N) {
"use strict";
const show = (x) => {
  if (typeof x === "string") return x;
  try { const s = JSON.stringify(x); return s === undefined ? String(x) : s; } catch (e) { return String(x); }
};
const line = (level) => (...a) => N.log(level, a.map(show).join(" "));
globalThis.console = { log: line(0), info: line(0), debug: line(0), trace: line(0), warn: line(1), error: line(2) };

// setTimeout: queued, run by the host once the promise jobs are done (no delay: a run has no clock to wait for)
const timers = [];
let nextId = 1;
globalThis.setTimeout = (f, ms, ...args) => {
  const id = nextId++;
  if (typeof f === "function") timers.push({ id, run: () => f(...args) });
  return id;
};
globalThis.clearTimeout = (id) => { const i = timers.findIndex((t) => t.id === id); if (i >= 0) timers.splice(i, 1); };
globalThis.__dwNextTimer = () => { const t = timers.shift(); if (!t) return false; t.run(); return true; };

// Intl.DateTimeFormat over QTimeZone: the parts PineTS reads (year month day hour minute second weekday
// timeZoneName), in the en-US order ("11/18/2026, 14:05:09") or the en-CA one ("2026-11-18").
const WD = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const WD_LONG = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
const MON = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
const MON_LONG = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
const pad = (n) => String(n).padStart(2, "0");
const FIELDS = ["weekday", "year", "month", "day", "hour", "minute", "second"];
function DateTimeFormat(locale, options) {
  if (!(this instanceof DateTimeFormat)) return new DateTimeFormat(locale, options);
  const o = Object.assign({}, options || {});
  const loc = Array.isArray(locale) ? locale[0] : locale;
  this._locale = loc ? String(loc) : "en-US";
  this._tz = o.timeZone === undefined ? "UTC" : String(o.timeZone);
  if (!N.tz(this._tz, 0)) throw new RangeError("Invalid time zone specified: " + this._tz);
  if (!FIELDS.some((f) => o[f] !== undefined)) { o.year = "numeric"; o.month = "numeric"; o.day = "numeric"; }
  o.h12 = o.hour12 !== undefined ? !!o.hour12 : o.hourCycle !== undefined ? o.hourCycle === "h11" || o.hourCycle === "h12" : !this._locale.startsWith("en-CA");
  this._o = o;
}
DateTimeFormat.prototype.formatToParts = function (when) {
  const ms = when === undefined ? Date.now() : +when;
  if (!Number.isFinite(ms)) throw new RangeError("Invalid time value");
  const w = N.tz(this._tz, ms);
  const o = this._o, out = [];
  const lit = (v) => out.push({ type: "literal", value: v });
  const put = (type, value) => out.push({ type, value: String(value) });
  if (o.weekday) { put("weekday", o.weekday === "long" ? WD_LONG[w[6]] : WD[w[6]]); if (o.year || o.month || o.day || o.hour) lit(", "); }
  const year = o.year === "2-digit" ? pad(w[0] % 100) : String(w[0]);
  const month = o.month === "2-digit" ? pad(w[1]) : o.month === "short" ? MON[w[1] - 1] : o.month === "long" ? MON_LONG[w[1] - 1] : String(w[1]);
  const day = o.day === "2-digit" ? pad(w[2]) : String(w[2]);
  const date = [];
  if (this._locale.startsWith("en-CA")) {
    if (o.year) date.push(["year", year]);
    if (o.month) date.push(["month", month]);
    if (o.day) date.push(["day", day]);
    date.forEach(([t, v], i) => { if (i) lit("-"); put(t, v); });
  } else {
    if (o.month) date.push(["month", month]);
    if (o.day) date.push(["day", day]);
    if (o.year) date.push(["year", year]);
    date.forEach(([t, v], i) => { if (i) lit("/"); put(t, v); });
  }
  if (o.hour || o.minute || o.second) {
    if (date.length) lit(", ");
    const time = [];
    if (o.hour) time.push(["hour", o.h12 ? String(w[3] % 12 || 12) : pad(w[3])]);
    if (o.minute) time.push(["minute", o.hour || o.minute === "2-digit" ? pad(w[4]) : String(w[4])]);
    if (o.second) time.push(["second", o.hour || o.minute || o.second === "2-digit" ? pad(w[5]) : String(w[5])]);
    time.forEach(([t, v], i) => { if (i) lit(":"); put(t, v); });
    if (o.hour && o.h12) { lit(" "); put("dayPeriod", w[3] < 12 ? "AM" : "PM"); }
  }
  if (o.timeZoneName) {
    if (out.length) lit(" ");
    const off = w[8], utc = this._tz === "UTC" || this._tz === "Etc/UTC";
    if (o.timeZoneName === "long") put("timeZoneName", utc ? "Coordinated Universal Time" : w[7] ? "Daylight Time" : "Standard Time");
    else {
      const a = Math.abs(off), h = Math.floor(a / 3600), m = Math.floor((a % 3600) / 60);
      put("timeZoneName", off === 0 ? (utc ? "UTC" : "GMT") : "GMT" + (off < 0 ? "-" : "+") + h + (m ? ":" + pad(m) : ""));
    }
  }
  return out;
};
DateTimeFormat.prototype.format = function (when) { return this.formatToParts(when).map((p) => p.value).join(""); };
DateTimeFormat.prototype.resolvedOptions = function () {
  const r = Object.assign({}, this._o, { locale: this._locale, timeZone: this._tz, calendar: "gregory", numberingSystem: "latn" });
  delete r.h12;
  return r;
};
DateTimeFormat.supportedLocalesOf = (l) => (l === undefined ? [] : [].concat(l));
globalThis.Intl = { DateTimeFormat };
}))JS";

// One run: hands Deepwater's bars to PineTS, packs the plots into Float64Arrays (meta as JSON).
const char* const RUNNER = R"JS((function () {
"use strict";
const safe = (x) => {
  try {
    const s = JSON.stringify(x, (k, v) => (typeof v === "bigint" ? String(v) : typeof v === "function" ? undefined : v));
    return s === undefined ? null : JSON.parse(s);
  } catch (e) { return null; }
};
globalThis.__dwRun = async function (source, buf, meta) {
  const a = new Float64Array(buf), n = a.length / 6, step = meta.stepMs;
  const candles = new Array(n);
  for (let i = 0; i < n; i++) {
    const t = a[i * 6] * 1000, next = i + 1 < n ? a[(i + 1) * 6] * 1000 : t + step;
    candles[i] = { openTime: t, open: a[i * 6 + 1], high: a[i * 6 + 2], low: a[i * 6 + 3], close: a[i * 6 + 4],
                   volume: a[i * 6 + 5], closeTime: Math.min(next, t + step) - 1 };
  }
  const pine = new PineTSLib.PineTS(candles, meta.symbol, meta.timeframe);
  if (meta.timezone) pine.setTimezone(meta.timezone);
  const ind = new PineTSLib.Indicator(source, meta.inputs || {});
  for (const [k, v] of Object.entries(meta.props || {})) ind.prop[k] = v;   // strategy() / indicator() arguments replaced
  const ctx = await pine.run(ind);
  const index = new Map();
  for (let i = 0; i < n; i++) index.set(candles[i].openTime, i);
  const plots = [], arrays = [], colors = [], drawings = {};
  for (const [key, p] of Object.entries(ctx.plots || {})) {
    const data = (p && p.data) || [];
    if (key.startsWith("__") && key.endsWith("__")) {
      drawings[key] = data.length ? safe(data[data.length - 1].value) : [];
      continue;
    }
    const v = new Float64Array(n).fill(NaN);
    const options = safe(p.options || {}) || {};
    const col = new Array(n).fill("");
    let first, varies = false;
    for (let j = 0; j < data.length; j++) {
      const d = data[j];
      const i = index.has(d.time) ? index.get(d.time) : data.length === n ? j : -1;
      if (i < 0) continue;
      const x = d.value;
      v[i] = typeof x === "number" ? x : x === true ? 1 : x === false ? 0 : NaN;
      const c = d.options ? d.options.color : undefined;
      if (c === undefined) continue;
      col[i] = c == null ? "na" : String(c);
      if (first === undefined) first = col[i]; else if (col[i] !== first) varies = true;
    }
    // per bar colours only when they change; one colour for every bar becomes the plot's own
    if (!varies && first !== undefined && first !== "na" && options.color == null) options.color = first;
    // markers: the bars where a plotshape / plotchar / plotarrow shows, with their own options
    const style = String(options.style || "").replace(/^style_/, "");
    let points = null;
    if (style === "shape" || style === "char" || style === "arrow") {
      points = [];
      for (let j = 0; j < data.length; j++) {
        const d = data[j];
        const i = index.has(d.time) ? index.get(d.time) : data.length === n ? j : -1;
        const x = d.value;
        if (i < 0 || x == null || x === false || x === 0 || (typeof x === "number" && !Number.isFinite(x))) continue;
        points.push(Object.assign({ i, value: typeof x === "number" ? x : 1 }, safe(d.options || {}) || {}));
      }
    }
    plots.push({ key, title: p.title == null ? key : String(p.title), options, points });
    arrays.push(v);
    colors.push(varies ? col : null);
  }
  for (const w of ctx.warnings || []) console.warn("warning: " + (typeof w === "string" ? w : (w && w.message) || JSON.stringify(w)));
  // strategy(): PineTS's broker emulator, its trades and totals
  let strategy = null;
  const st = ctx.strategy;
  if (st && (st.closedtrades || st.opentrades)) {
    const trade = (t) => ({ entry_id: t.entry_id, entry_comment: t.entry_comment, entry_price: t.entry_price,
                            entry_bar_index: t.entry_bar_index, entry_time: t.entry_time, size: t.size,
                            commission: t.commission, entry_commission: t._entry_commission, exit_price: t.exit_price,
                            exit_bar_index: t.exit_bar_index, exit_time: t.exit_time, exit_id: t.exit_id,
                            exit_comment: t.exit_comment, profit: t.profit, status: t.status });
    strategy = safe({ config: st.config || {}, closedtrades: (st.closedtrades || []).map(trade),
                      opentrades: (st.opentrades || []).map(trade), initial_capital: st.initial_capital,
                      equity: st.equity, netprofit: st.netprofit, openprofit: st.openprofit,
                      grossprofit: st.grossprofit, grossloss: st.grossloss, max_drawdown: st.max_drawdown,
                      wintrades: st.wintrades, losstrades: st.losstrades, eventrades: st.eventrades,
                      position_size: st.position_size, account_currency: st.account_currency });
  }
  return { meta: JSON.stringify({ plots, indicator: safe(ctx.indicator || {}) || {}, drawings, strategy }), arrays, colors };
};
globalThis.__dwDescribe = function (source) {
  const ind = new PineTSLib.Indicator(source);
  return JSON.stringify({ declaration: ind.getDeclarationType() || "", inputs: safe(ind.getInputsMeta() || []) || [] });
};
}))JS";

std::optional<WallClock> wallIn(const QTimeZone& z, qint64 ms, int* offset) {
  if (!z.isValid()) return std::nullopt;
  const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms, z);
  if (!dt.isValid()) return std::nullopt;
  const QDate d = dt.date();
  const QTime t = dt.time();
  WallClock w;
  w.year = d.year(); w.month = d.month(); w.day = d.day();
  w.hour = t.hour(); w.minute = t.minute(); w.second = t.second();
  w.weekday = d.dayOfWeek() % 7;
  const int off = z.offsetFromUtc(dt);
  const auto at = [&](int month) { return z.offsetFromUtc(QDateTime(QDate(d.year(), month, 1), QTime(12, 0), QTimeZone::UTC)); };
  w.dst = off > std::min(at(1), at(7));
  if (offset) *offset = off;
  return w;
}

// The state of one run, reachable from the natives through the context's opaque pointer.
struct Run {
  JSRuntime* rt = nullptr;
  JSContext* ctx = nullptr;
  Clock::time_point deadline;
  bool timedOut = false;
  bool cancelled = false;                        // the caller's flag or the app's quit stopped it
  std::shared_ptr<const std::atomic<bool>> cancel;   // Options::cancel
  QStringList log;
  int dropped = 0;
  QHash<QString, QTimeZone> zones;

  explicit Run(const Options& o) : cancel(o.cancel) {
    rt = JS_NewRuntime();
    if (!rt) return;
    JS_SetMemoryLimit(rt, size_t(std::max(16, o.memoryMB)) << 20);
    JS_SetMaxStackSize(rt, js::stackLimit());
    JS_SetInterruptHandler(rt, &Run::interrupt, this);
    ctx = JS_NewContext(rt);
    if (ctx) JS_SetContextOpaque(ctx, this);
  }
  ~Run() {
    if (ctx) JS_FreeContext(ctx);
    if (rt) JS_FreeRuntime(rt);
  }
  Run(const Run&) = delete;
  Run& operator=(const Run&) = delete;

  static int interrupt(JSContext*, void* opaque) {
    Run* r = static_cast<Run*>(opaque);
    if (Clock::now() > r->deadline) { r->timedOut = true; return 1; }
    if (g_quitting.load(std::memory_order_relaxed) || (r->cancel && r->cancel->load(std::memory_order_relaxed))) {
      r->cancelled = true;
      return 1;
    }
    return 0;
  }
  const QTimeZone& zone(const QString& id) {
    auto it = zones.find(id);
    if (it == zones.end()) it = zones.insert(id, id == "UTC" || id == "Etc/UTC" ? QTimeZone(QTimeZone::UTC) : QTimeZone(id.toUtf8()));
    return *it;
  }

  QString str(JSValueConst v) const {
    size_t len = 0;
    const char* p = JS_ToCStringLen(ctx, &len, v);
    if (!p) { JS_FreeValue(ctx, JS_GetException(ctx)); return {}; }
    QString s = QString::fromUtf8(p, qsizetype(len));
    JS_FreeCString(ctx, p);
    return s;
  }
  // The text of a thrown value: its message (Error) or String(value)
  QString errorText(JSValueConst e) const {
    if (JS_IsError(e)) {
      JSValue m = JS_GetPropertyStr(ctx, e, "message");
      QString s = str(m);
      JS_FreeValue(ctx, m);
      if (!s.isEmpty()) return s;
    }
    return str(e);
  }
  QString exception() {
    JSValue e = JS_GetException(ctx);
    QString s = errorText(e);
    JS_FreeValue(ctx, e);
    return s;
  }

  static JSValue nLog(JSContext* c, JSValueConst, int argc, JSValueConst* argv) {
    Run* r = static_cast<Run*>(JS_GetContextOpaque(c));
    if (argc < 2) return JS_UNDEFINED;
    int level = 0;
    JS_ToInt32(c, &level, argv[0]);
    if (r->log.size() >= MAX_LOG) { ++r->dropped; return JS_UNDEFINED; }
    r->log << (level >= 2 ? QStringLiteral("error: ") : QString()) + r->str(argv[1]);
    return JS_UNDEFINED;
  }
  static JSValue nTz(JSContext* c, JSValueConst, int argc, JSValueConst* argv) {
    Run* r = static_cast<Run*>(JS_GetContextOpaque(c));
    if (argc < 2) return JS_NULL;
    double ms = 0;
    if (JS_ToFloat64(c, &ms, argv[1]) < 0) return JS_EXCEPTION;
    if (!std::isfinite(ms)) return JS_NULL;
    int off = 0;
    if (std::fabs(ms) > 8.64e15) return JS_NULL;   // beyond JS Date's range (and qint64-safe)
    const auto w = wallIn(r->zone(r->str(argv[0])), qint64(std::floor(ms)), &off);
    if (!w) return JS_NULL;
    JSValue a = JS_NewArray(c);
    const int v[] = {w->year, w->month, w->day, w->hour, w->minute, w->second, w->weekday, w->dst ? 1 : 0, off};
    for (int i = 0; i < 9; ++i) JS_SetPropertyUint32(c, a, uint32_t(i), JS_NewInt32(c, v[i]));
    return a;
  }

  // Evaluates code; false (and *err) when it throws
  bool eval(const QByteArray& code, const char* name, JSValue* out, QString* err) {
    JSValue r = JS_Eval(ctx, code.constData(), size_t(code.size()), name, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) { *err = exception(); return false; }
    if (out) *out = r; else JS_FreeValue(ctx, r);
    return true;
  }
  // Runs the promise jobs, then one queued timer at a time, until p settles or nothing is left to run
  bool settle(JSValueConst p, QString* err) {
    for (;;) {
      for (;;) {
        JSContext* jc = nullptr;
        const int k = JS_ExecutePendingJob(rt, &jc);
        if (k == 0) break;
        if (k < 0) { *err = exception(); return false; }
      }
      if (JS_PromiseState(ctx, p) != JS_PROMISE_PENDING) return true;
      // no job left: the next timer, if any
      JSValue g = JS_GetGlobalObject(ctx);
      JSValue f = JS_GetPropertyStr(ctx, g, "__dwNextTimer");
      JSValue ran = JS_Call(ctx, f, JS_UNDEFINED, 0, nullptr);
      JS_FreeValue(ctx, f);
      JS_FreeValue(ctx, g);
      if (JS_IsException(ran)) { *err = exception(); return false; }
      const bool more = JS_ToBool(ctx, ran) == 1;
      JS_FreeValue(ctx, ran);
      if (!more) {
        *err = QCoreApplication::translate("charts", "The script never finished (it waits for something that never comes).");
        return false;
      }
    }
  }
};

double stepSeconds(const QString& tf) {
  static const QRegularExpression re(QStringLiteral("^(\\d*)([SDWM]?)$"));
  const auto m = re.match(tf);
  if (!m.hasMatch()) return 60;
  const double n = m.captured(1).isEmpty() ? 1 : m.captured(1).toDouble();
  const QString u = m.captured(2);
  if (u == "S") return n;
  if (u == "D") return n * 86400;
  if (u == "W") return n * 7 * 86400;
  if (u == "M") return n * 31 * 86400;
  return n * 60;
}

Series typed(JSContext* ctx, JSValueConst v, qsizetype n) {
  Series out(n, NaN);
  size_t off = 0, len = 0, bpe = 0;
  JSValue ab = JS_GetTypedArrayBuffer(ctx, v, &off, &len, &bpe);
  if (JS_IsException(ab)) { JS_FreeValue(ctx, JS_GetException(ctx)); return out; }
  size_t size = 0;
  const uint8_t* p = JS_GetArrayBuffer(ctx, &size, ab);
  if (p && bpe == sizeof(double) && off + len <= size)
    std::memcpy(out.data(), p + off, std::min(len, size_t(n) * sizeof(double)));
  JS_FreeValue(ctx, ab);
  return out;
}
// A fresh runtime with the prelude, PineTS and the runner loaded. False (and *err) when that fails.
bool boot(Run& r, QString* errOut) {
  QString& err = *errOut;
  JSValue prelude = JS_UNDEFINED, runner = JS_UNDEFINED;
  bool ok = r.eval(QByteArray(PRELUDE), "<deepwater-prelude>", &prelude, &err);
  if (ok) {
    JSValue natives = JS_NewObject(r.ctx);
    JS_SetPropertyStr(r.ctx, natives, "log", JS_NewCFunction(r.ctx, &Run::nLog, "log", 2));
    JS_SetPropertyStr(r.ctx, natives, "tz", JS_NewCFunction(r.ctx, &Run::nTz, "tz", 2));
    JSValue v = JS_Call(r.ctx, prelude, JS_UNDEFINED, 1, &natives);
    JS_FreeValue(r.ctx, natives);
    if (JS_IsException(v)) { err = r.exception(); ok = false; }
    JS_FreeValue(r.ctx, v);
  }
  JS_FreeValue(r.ctx, prelude);
  ok = ok && r.eval(bundle(), "pinets.js", nullptr, &err);
  if (ok) {
    ok = r.eval(QByteArray(RUNNER), "<deepwater-runner>", &runner, &err);
    if (ok) {
      JSValue v = JS_Call(r.ctx, runner, JS_UNDEFINED, 0, nullptr);
      if (JS_IsException(v)) { err = r.exception(); ok = false; }
      JS_FreeValue(r.ctx, v);
    }
    JS_FreeValue(r.ctx, runner);
  }
  return ok;
}
}  // namespace

const Plot* Result::plot(const QString& key) const {
  for (const Plot& p : plots)
    if (p.key == key) return &p;
  return nullptr;
}

QString pinetsVersion() { return QStringLiteral("0.13.1"); }

std::optional<WallClock> wallClock(const QString& timezone, qint64 msUtc) {
  const QTimeZone z = timezone == "UTC" || timezone == "Etc/UTC" ? QTimeZone(QTimeZone::UTC) : QTimeZone(timezone.toUtf8());
  return wallIn(z, msUtc, nullptr);
}

QString timeframeOf(const Bars& bars) {
  qint64 g = 0;
  for (qsizetype i = 1; i < bars.size(); ++i) {
    const qint64 d = bars[i].time - bars[i - 1].time;
    if (d > 0 && (g == 0 || d < g)) g = d;
  }
  if (g <= 0) return {};
  if (g < 60) return QString::number(g) + "S";
  if (g < 86400) return QString::number(g / 60);
  if (g < 7 * 86400) return g / 86400 == 1 ? QStringLiteral("D") : QString::number(g / 86400) + "D";
  if (g < 28 * 86400) return g / (7 * 86400) == 1 ? QStringLiteral("W") : QString::number(g / (7 * 86400)) + "W";
  const qint64 m = std::max<qint64>(1, (g + 15 * 86400) / (30 * 86400));
  return m == 1 ? QStringLiteral("M") : QString::number(m) + "M";
}

QString timeframeFromCharts(const QString& tf) {
  static const QRegularExpression re(QStringLiteral("^(\\d+)(s|m|h|D|W|M)$"));
  const auto m = re.match(tf);
  if (!m.hasMatch()) return {};
  const int n = m.captured(1).toInt();   // 0 when it overflows int
  const QString u = m.captured(2);
  if (n <= 0 || n > 1000000) return {};   // n * 60 below must not overflow
  if (u == "s") return QString::number(n) + "S";
  if (u == "m") return QString::number(n);
  if (u == "h") return QString::number(n * 60);
  const QString one = u;   // D W M
  return n == 1 ? one : QString::number(n) + one;
}

Script describe(const QString& source) {
  Script out;
  if (bundle().isEmpty()) { out.error = QCoreApplication::translate("charts", "Pine Script isn't available in this build (PineTS is missing)."); return out; }
  Options o;
  Run r(o);
  if (!r.ctx) { out.error = QCoreApplication::translate("charts", "Couldn't start the Pine Script engine."); return out; }
  r.deadline = Clock::now() + std::chrono::seconds(30);
  QString err;
  if (!boot(r, &err)) { out.error = QCoreApplication::translate("charts", "Couldn't load PineTS: %1").arg(err); return out; }
  const QByteArray src = source.toUtf8();
  JSValue arg = JS_NewStringLen(r.ctx, src.constData(), size_t(src.size()));
  JSValue g = JS_GetGlobalObject(r.ctx);
  JSValue fn = JS_GetPropertyStr(r.ctx, g, "__dwDescribe");
  JS_FreeValue(r.ctx, g);
  JSValue v = JS_Call(r.ctx, fn, JS_UNDEFINED, 1, &arg);
  JS_FreeValue(r.ctx, fn);
  JS_FreeValue(r.ctx, arg);
  if (JS_IsException(v)) {
    out.error = r.exception();
  } else {
    const QJsonObject m = QJsonDocument::fromJson(r.str(v).toUtf8()).object();
    out.declaration = m.value("declaration").toString();
    out.inputs = m.value("inputs").toArray();
  }
  JS_FreeValue(r.ctx, v);
  return out;
}

Result run(const QString& source, const Bars& bars, const Options& options) {
  Result res;
  if (bundle().isEmpty()) { res.error = QCoreApplication::translate("charts", "Pine Script isn't available in this build (PineTS is missing)."); return res; }
  if (bars.isEmpty()) { res.error = QCoreApplication::translate("charts", "No bars to run the script on."); return res; }
  Run r(options);
  if (!r.ctx) { res.error = QCoreApplication::translate("charts", "Couldn't start the Pine Script engine."); return res; }
  const int timeout = std::max(100, options.timeoutMs);
  // Loading PineTS gets its own clock (a fixed cost, ~50 ms), the script the configured one
  r.deadline = Clock::now() + std::chrono::seconds(30);

  QString err;
  if (!boot(r, &err)) {
    res.stopped = r.cancelled;
    res.error = r.cancelled ? QCoreApplication::translate("charts", "The script was stopped.")
                            : QCoreApplication::translate("charts", "Couldn't load PineTS: %1").arg(err);
    return res;
  }

  // the bars, 6 doubles each, and the run's settings
  std::vector<double> buf(size_t(bars.size()) * 6);
  for (qsizetype i = 0; i < bars.size(); ++i) {
    const Bar& b = bars[i];
    double* p = buf.data() + i * 6;
    p[0] = double(b.time); p[1] = b.open; p[2] = b.high; p[3] = b.low; p[4] = b.close; p[5] = b.volume;
  }
  const QString tf = options.timeframe.isEmpty() ? timeframeOf(bars) : options.timeframe;
  QJsonObject meta{{"symbol", options.symbol}, {"timeframe", tf.isEmpty() ? QStringLiteral("D") : tf},
                   {"timezone", options.timezone.isEmpty() ? QStringLiteral("UTC") : options.timezone},
                   {"inputs", options.inputs}, {"props", options.props}, {"stepMs", stepSeconds(tf.isEmpty() ? QStringLiteral("D") : tf) * 1000}};
  const QByteArray metaJson = QJsonDocument(meta).toJson(QJsonDocument::Compact);
  const QByteArray src = source.toUtf8();

  JSValue args[3];
  args[0] = JS_NewStringLen(r.ctx, src.constData(), size_t(src.size()));
  args[1] = JS_NewArrayBufferCopy(r.ctx, reinterpret_cast<const uint8_t*>(buf.data()), buf.size() * sizeof(double));
  args[2] = JS_ParseJSON(r.ctx, metaJson.constData(), size_t(metaJson.size()), "<meta>");
  JSValue g = JS_GetGlobalObject(r.ctx);
  JSValue fn = JS_GetPropertyStr(r.ctx, g, "__dwRun");
  JS_FreeValue(r.ctx, g);
  const auto t0 = Clock::now();
  r.deadline = t0 + std::chrono::milliseconds(timeout);
  JSValue promise = JS_Call(r.ctx, fn, JS_UNDEFINED, 3, args);
  JS_FreeValue(r.ctx, fn);
  for (JSValue& a : args) JS_FreeValue(r.ctx, a);

  const auto fail = [&](const QString& why) {
    const bool oom = why.contains(QLatin1String("out of memory"));
    res.stopped = r.timedOut || r.cancelled || oom;
    if (r.cancelled) res.error = QCoreApplication::translate("charts", "The script was stopped.");
    else if (r.timedOut) res.error = QCoreApplication::translate("charts", "The script took too long (stopped after %1 s).").arg(timeout / 1000.0);
    else if (oom) res.error = QCoreApplication::translate("charts", "The script used more than %1 MB of memory.").arg(options.memoryMB);
    else res.error = why;
  };
  if (JS_IsException(promise)) {
    fail(r.exception());
  } else if (!r.settle(promise, &err)) {
    fail(err);
  } else if (JS_PromiseState(r.ctx, promise) == JS_PROMISE_REJECTED) {
    JSValue why = JS_PromiseResult(r.ctx, promise);
    fail(r.errorText(why));
    JS_FreeValue(r.ctx, why);
  } else {
    JSValue out = JS_PromiseResult(r.ctx, promise);
    JSValue metaV = JS_GetPropertyStr(r.ctx, out, "meta");
    const QJsonObject m = QJsonDocument::fromJson(r.str(metaV).toUtf8()).object();
    JS_FreeValue(r.ctx, metaV);
    JSValue arrays = JS_GetPropertyStr(r.ctx, out, "arrays");
    JSValue colors = JS_GetPropertyStr(r.ctx, out, "colors");
    const QJsonArray plots = m.value("plots").toArray();
    for (qsizetype i = 0; i < plots.size(); ++i) {
      const QJsonObject p = plots[i].toObject();
      Plot plot;
      plot.key = p.value("key").toString();
      plot.title = p.value("title").toString();
      plot.options = p.value("options").toObject();
      plot.points = p.value("points").toArray();
      JSValue a = JS_GetPropertyUint32(r.ctx, arrays, uint32_t(i));
      plot.values = typed(r.ctx, a, bars.size());
      JS_FreeValue(r.ctx, a);
      JSValue c = JS_GetPropertyUint32(r.ctx, colors, uint32_t(i));
      if (JS_IsObject(c)) {
        plot.colors.reserve(bars.size());
        for (qsizetype k = 0; k < bars.size(); ++k) {
          JSValue s = JS_GetPropertyUint32(r.ctx, c, uint32_t(k));
          plot.colors << r.str(s);
          JS_FreeValue(r.ctx, s);
        }
      }
      JS_FreeValue(r.ctx, c);
      res.plots << plot;
    }
    JS_FreeValue(r.ctx, arrays);
    JS_FreeValue(r.ctx, colors);
    JS_FreeValue(r.ctx, out);
    res.indicator = m.value("indicator").toObject();
    res.strategy = m.value("strategy").toObject();
    res.drawings = m.value("drawings").toObject();
  }
  JS_FreeValue(r.ctx, promise);
  res.ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  res.log = r.log;
  if (r.dropped) res.log << QCoreApplication::translate("charts", "(%1 more lines not kept)").arg(r.dropped);
  return res;
}

void runTracked(std::function<void()> work, QObject* ctx, std::function<void()> done) {
  watchQuit();
  {
    std::lock_guard l(g_runsMu);
    ++g_runs;
  }
  QThread* t = QThread::create([work = std::move(work)] {
    work();
    {
      std::lock_guard l(g_runsMu);
      --g_runs;
    }
    g_runsCv.notify_all();
  });
  t->setObjectName(QStringLiteral("pine-run"));
  t->setStackSize(16u << 20);
  QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
  if (ctx) QObject::connect(t, &QThread::finished, ctx, [done = std::move(done)] { if (done) done(); });
  t->start();
}

void runAsync(const QString& source, const Bars& bars, const Options& options, QObject* ctx, std::function<void(const Result&)> done) {
  auto out = std::make_shared<Result>();
  runTracked([source, bars, options, out] { *out = run(source, bars, options); }, ctx, [out, done] { if (done) done(*out); });
}

bool stopRunsForQuit(int waitMs) {
  g_quitting.store(true, std::memory_order_relaxed);   // every run's interrupt handler stops its JS at the next check
  std::unique_lock l(g_runsMu);
  return g_runsCv.wait_for(l, std::chrono::milliseconds(std::max(0, waitMs)), [] { return g_runs == 0; });
}

int runningTracked() {
  std::lock_guard l(g_runsMu);
  return g_runs;
}

void resetQuitForTests() { g_quitting.store(false, std::memory_order_relaxed); }

}  // namespace charts::pine
