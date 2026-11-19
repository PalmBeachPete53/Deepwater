#pragma once
// Port of quant-terminal public/lib/indicators.js (MIT, Copyright (c) 2026 Lewis Jackson).
// CONTRACT shared by the whole Charts port: other modules (backtest, quant, alerts, chart pane, panels) compile
// against these declarations. Add declarations freely; do not change or remove the ones below.
#include "charts/core/Types.h"
#include <QStringList>
#include <QHash>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <initializer_list>
#include <stdexcept>

namespace charts::ind {

// One setting of an indicator (JS: { key, label, type, default, min, max, options }).
struct Input {
  QString key, label;
  QString type;            // "int" | "float" | "select" | "source" | "time"
  QJsonValue def;          // JS `default`
  double min = NaN, max = NaN, step = NaN;
  QStringList options;     // for "select"
};

// One plot (JS: { key, label, kind, color, lineWidth, ... }).
struct Output {
  QString key, label;
  QString kind = "line";   // "line" | "histogram"
  QString color;           // "#rrggbb" or "rgba(...)" as in the JS
  int lineWidth = 0;       // 0 = not set (renderer default)
  QJsonObject extra;       // any other field the JS sets on an output, kept verbatim
  // ADDED (audit D12): set when the output's value at a bar depends on later bars (drawn shifted back in time, or
  // redrawn once the future confirms it) with these params. Such an output is for the chart only: a backtest or
  // alert rule reading it would see the future (Backtest's resolveSeries refuses it).
  std::function<bool(const Params&)> readsAhead;
};

// A chart marker (JS: { time, position, shape, color, text }).
struct Marker {
  qint64 time = 0;
  QString position;        // "aboveBar" | "belowBar" | "inBar"
  QString shape;           // "arrowUp" | "arrowDown" | "circle" | "square"
  QString color, text;
  QJsonObject extra;       // e.g. size, price — anything else the JS sets
};

// What calc() returns: { <outputKey>: number[], markers?, barColors?, colors?, breaks?, volumeProfile?, ... }.
struct Result {
  QHash<QString, Series> series;   // one array per output key (and any other numeric array the JS returns)
  QVector<Marker> markers;
  QStringList barColors;           // per bar, "" = default colour
  QStringList colors;              // per bar colours of a histogram output (res.colors)
  QVector<bool> breaks;            // res.breaks: true where a line must not join the previous point
  QJsonObject volumeProfile;       // res.volumeProfile (vpvr): parameters for the visible-range profile primitive
  QJsonObject sessionProfile;      // res.sessionProfile (svp)
  QJsonObject extra;               // any other non-array field, kept verbatim
  QString error;                   // res.error
  bool pending = false;            // res.pending
  // ADDED (indicators port): the keys of the JS result object in the JS order, empty arrays included, so a result
  // can be turned back into the same JS/JSON object (custom indicators' ta.compute, the local API). The put*()
  // helpers fill it; code that fills the fields above directly may leave it empty (then: series keys, then the rest).
  QStringList keys;
  // ADDED (Pine Script): per bar colours of one output (a Pine plot whose colour changes bar by bar), by output key;
  // "" = the output's own colour. Lines and histograms use it before `colors`.
  QHash<QString, QStringList> seriesColors;
  void note(const QString& k) { if (!keys.contains(k)) keys << k; }
  void put(const QString& k, Series s) { note(k); series.insert(k, std::move(s)); }
  void putMarkers(QVector<Marker> m) { note(QStringLiteral("markers")); markers = std::move(m); }
  void putBarColors(QStringList c) { note(QStringLiteral("barColors")); barColors = std::move(c); }
  void putColors(QStringList c) { note(QStringLiteral("colors")); colors = std::move(c); }
  void putBreaks(QVector<bool> b) { note(QStringLiteral("breaks")); breaks = std::move(b); }
  void putExtra(const QString& k, const QJsonValue& v) { note(k); extra.insert(k, v); }
  bool has(const QString& k) const { return keys.contains(k) || series.contains(k); }
  Series value(const QString& k) const { return series.value(k); }   // empty when missing
};

// An indicator definition (JS registry entry).
struct Def {
  QString id, name, group;
  bool overlay = false;
  QVector<Input> inputs;
  QVector<Output> outputs;
  QVector<double> levels;          // dashed lines in the indicator's own pane
  QJsonObject extra;               // other definition fields (noAutoscale, scale, precision, ...), kept verbatim
  std::function<Result(const Bars&, const Params&)> calc;
  bool custom = false;             // loaded from custom/*.js
  QString file;                    // custom: file name in the custom folder
};

// ---- Registry (JS: INDICATORS, BY_ID, defaults, compute, registerIndicator, validateIndicator)
// A definition shared with whoever holds it: a reload of the same id makes a new Def, the old one lives as long as a
// caller (a backtest on a worker thread, say) still holds this pointer.
using DefPtr = std::shared_ptr<const Def>;
QVector<Def> all();                             // built-ins in the JS order, then registered custom ones (a snapshot)
DefPtr byId(const QString& id);                 // null if unknown
Params defaults(const QString& id);             // { key: default } of its inputs
// Unknown params fall back to defaults (JS: { ...defaults(id), ...params }). Throws std::runtime_error
// ("Unknown indicator: <id>") like the JS for an unknown id.
Result compute(const QString& id, const Bars& bars, const Params& params = {});
// ADDED (audit D12): whether output `key` of def reads later bars with these params (defaults filled in).
bool readsAhead(const Def& def, const QString& key, const Params& params);
// Every problem of a definition against the custom format (docs/custom-indicators.md); empty = valid.
// run = true also runs calc on the 300 deterministic trial bars and checks what it returns.
QStringList problems(const Def& def, bool run = true);
// Validates and registers (replacing an entry with the same id). Returns the problems (empty = registered).
// run = false skips the trial calc of the check, for a definition whose trial already passed (custom::build runs it
// on the JS object, as the JS loader does once): one load = one trial run, as indicators.js.
QStringList registerIndicator(const Def& def, bool run = true);
void unregisterIndicator(const QString& id);

// ---- ta primitives (JS exports and the `ta` object given to custom indicators)
Series source(const Bars& bars, const QString& name = "close");   // close/open/high/low/volume/hl2/hlc3/ohlc4/hlcc4
Series change(const Series& src, int n = 1);
Series sum(const Series& src, int n);
Series sma(const Series& src, int n);
Series ema(const Series& src, int n);
Series rma(const Series& src, int n);
Series wma(const Series& src, int n);
Series vwma(const Series& src, const Series& vol, int n);
Series hma(const Series& src, int n);
Series dema(const Series& src, int n);
Series tema(const Series& src, int n);
Series alma(const Series& src, int n, double offset = 0.85, double sigma = 6);
Series linreg(const Series& src, int n, double offset = 0);   // offset: a JS number (NaN gives NaN)
Series stdev(const Series& src, int n);
Series dev(const Series& src, int n);
Series highest(const Series& src, int n);
Series lowest(const Series& src, int n);
Series tr(const Bars& bars);
Series atr(const Bars& bars, int n = 14);
Series rsi(const Series& src, int n = 14);      // JS rsiOf
QVector<bool> crossover(const Series& a, const Series& b);
QVector<bool> crossunder(const Series& a, const Series& b);
// JS periodKey(t, period): the key of the session/day/week/month/... a bar time falls in.
QString periodKey(qint64 t, const QString& period);
QVector<Marker> candlePatterns(const Bars& bars);

// ======== ADDED by the indicators port (everything below is additional to the contract above) ========
// Notes on the port:
// - Lengths are int: the JS accepts any number, but a non-integer length makes the JS loops index between
//   elements (undefined values); the inputs are "int" so the UI never sends one. Params read with charts::num/inum.
// - A NaN length (a non-numeric "abc" from the API or a custom indicator's ta call, or +Infinity) is kNaNLength:
//   in the JS every `i >= n` / `count === n` test is then false, so no window ever fills and the results are NaN.
//   kNaNLength is larger than any clamped length (+-1e9), so the primitives' loops never start the same way; code
//   that allocates per length unit (alma's weights, shiftForward) checks for it. -Infinity is clamped to -1e9 (the
//   JS loops forever on it).
// - Out-of-range reads give NaN like JS `undefined`; where the JS would throw ("Cannot read properties of
//   undefined", e.g. Markov with a negative lookback), the C++ throws std::runtime_error from compute().
// - Registry: all()/byId()/registerIndicator() are guarded by a lock, so compute() of built-ins is safe from worker
//   threads. A byId() pointer keeps its Def alive (shared), even after that id is registered again or unregistered. Custom (JS)
//   indicators' calc throws std::runtime_error when called off the thread that loaded them (see CustomJs.h).

constexpr int kNaNLength = 1 << 30;
// A JS number used as a length: whole number clamped to +-1e9; NaN and +Infinity give kNaNLength.
inline int lengthOf(double d) {
  if (std::isnan(d) || d == INFINITY) return kNaNLength;
  return int(std::clamp(std::trunc(d), -1e9, 1e9));
}

// JS semantics helpers (exact equivalents of the JS operators the formulas use).
inline bool jsIsNum(double v) { return !std::isnan(v); }               // the JS file's isNum: Infinity counts
inline bool jsTruthy(double v) { return v != 0 && !std::isnan(v); }    // `if (x)` on a number
inline double jsOr(double v, double d) { return jsTruthy(v) ? v : d; }  // `x || d` on a number
double jsMax(std::initializer_list<double> v);                          // Math.max: NaN if any is NaN
double jsMin(std::initializer_list<double> v);                          // Math.min
double jsRound(double v);                                               // Math.round: halves go up (-2.5 -> -2)
QString numberToString(double v);                                       // String(number): 0.1, 1e+21, NaN ...

Series crossNumbers(const QVector<bool>& b);                            // true/false -> 1/0
QVector<bool> crossover(const Series& a, double b);                     // ta.crossover(a, 70)
QVector<bool> crossunder(const Series& a, double b);
Series stochOf(const Series& src, const Series& high, const Series& low, int n);
Series cum(const Series& src);
Series shiftForward(const Series& src, int k);   // value of bar i drawn at bar i + k; longer by max(0, k)
Series whole(const Series& out, const Series& src, int n);   // NaN unless the whole window of src is real
double pearson(const Series& x, const Series& y);
Series avgRanks(const Series& v);
Series percentRank(const Series& src, int n);
Series updownStreak(const Series& src);
Series swma(const Series& src);
// Offset (0 or negative) of the highest (highest = true) or lowest value within the last n bars (ta.highestbars).
Series extremeBars(const Series& src, int n, bool highest);
// sessionBreak(key, start, i) with key === null given as hasKey = false.
inline bool sessionBreak(bool hasKey, int start, int i) { return hasKey && i - start > 1; }

struct Macd { Series macd, signal, hist; QStringList colors; };
Macd macdFrom(const Series& s, int fast = 12, int slow = 26, int signal = 9);
struct Dmi { Series plus, minus, adx; };
Dmi dmiOf(const Bars& bars, int diLen = 14, int adxLen = 14);
Series psarOf(const Bars& bars, double start = 0.02, double inc = 0.02, double max = 0.2);
struct Supertrend { Series st, dir; };
Supertrend supertrendOf(const Bars& bars, double factor = 3, int atrLen = 10);
Result vstopOf(const Bars& bars, int length, double factor);          // { up, down }
Series zigzagOf(const Bars& bars, double deviation, double depth);
Result pivotsOf(const Bars& bars, const QString& period);             // { p, r1, s1, r2, s2, r3, s3, breaks }
QVector<Marker> pivotMarkers(const Bars& bars, int left, int right);
QVector<Marker> fractalMarkers(const Bars& bars, int n);

// The custom indicator format check (JS indicatorProblems / validateIndicator).
Bars trialBars(int n = 300);    // 300 made-up daily bars, the same every time, for the trial run
// JS validateIndicator's message: "This indicator doesn't follow the format (docs/custom-indicators.md):\n- ..."
QString formatProblems(const QStringList& problems);
void validateIndicator(const Def& def, bool run = true);   // throws std::runtime_error(formatProblems(...))
// JS registerIndicator(def) fills these before validating: group "Custom" when empty, and one orange line `value`
// labelled with the name (or id) when there are no outputs. (JS also defaults overlay to true; a C++ Def cannot tell
// "unset" from false, so overlay is kept as given. CustomJs applies the JS defaults on the JS object itself.)
Def withCustomDefaults(Def def);

}  // namespace charts::ind
