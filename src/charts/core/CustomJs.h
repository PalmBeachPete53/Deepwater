#pragma once
// Port of quant-terminal's custom indicator loading (server.js loadCustom, public/js/app.js importCustom; MIT,
// Copyright (c) 2026 Lewis Jackson). A custom indicator is one JavaScript file in the custom folder
// (paths::data("charts/custom")), in the format of quant-terminal/docs/custom-indicators.md:
//     export default { name, overlay, inputs, outputs, levels, calc(bars, p, ta) { ... } }
// The file runs in QuickJS-ng (third_party/quickjs-ng, ES2023+: object spread, class fields, ??=, flat, at,
// findLast, toSorted, Object.hasOwn ...) as an ES module (`export default` read from the module namespace), one
// JSRuntime + JSContext per loaded file, so a reload always reads the file again. Imports of other files by path
// ("./x.js") work; packages do not. calc gets bars as plain JS objects { time, open, high, low, close, volume },
// p = the params, and `ta` with the same members as the JS `ta` object (sma, ema, rma, wma, vwma, hma, dema, tema,
// alma, linreg, stdev, dev, highest, lowest, change, sum, rsi, atr, tr, crossover, crossunder, compute, source, macd,
// supertrend, psar, dmi), backed by the C++ primitives of Indicators.h. console.log/info/debug/warn/error go to Qt's log.
//
// LIMITS: a file's top-level code, or one calc call, running longer than 5 s is stopped ("Custom indicator <file>
// took too long (stopped after 5 s)", the load error or the std::runtime_error of compute); each runtime may use
// 256 MB, and the JS stack is what is left of the thread's stack minus 256 KB, at most 8 MB (deep recursion throws
// RangeError "Maximum call stack size exceeded": about 8000 nested calls on the macOS main thread, Node allows ~10000).
//
// THREADING: a file's runtime is confined to the thread that loaded it (QuickJS runtimes are not thread-safe). Load
// files on the GUI thread; the registered indicator's calc (and so ind::compute("custom:...")) must run on that same
// thread too: called from another thread it throws std::runtime_error("Custom indicators run on the GUI thread only")
// instead of touching the runtime. Built-in indicators stay safe on any thread. The last copy of a Def may be
// destroyed on any thread (the runtime is freed there; nothing else uses it any more).
//
// Differences from the JS that cannot be avoided: elements that are not numbers (null, strings, booleans) in arrays
// given to `ta` read as NaN; lengths are truncated to whole numbers; QuickJS's error messages are worded differently
// from V8's (e.g. syntax errors: "unexpected token in expression: '}'"); arrays passed to ta must not be mutated by
// calc before being passed again (a bars array given to calc is converted once per calc call); BigInt and Symbol
// values in a definition or a result are read as their text (they are refused where a number or object is needed).
#include "charts/core/Indicators.h"
#include <QList>

namespace charts::custom {

struct Loaded {
  QString file;          // file name in the folder, e.g. "ema-ribbon-squeeze.js"
  QString id;            // "custom:ema-ribbon-squeeze"
  QStringList problems;  // format problems (docs/custom-indicators.md); empty when it follows the format
  // What quant-terminal reports when the file did not load (customErrors[file] / the toast): the format message
  // "This indicator doesn't follow the format (docs/custom-indicators.md):\n- ...", or the JS error's message.
  // Empty = registered.
  QString error;
  bool ok() const { return error.isEmpty(); }
};

// `custom:${file.replace(/\.js$/, "")}`
QString idFor(const QString& fileName);

// Loads one file and registers it with ind::registerIndicator under idFor(file name), replacing an earlier version.
// A refused file leaves the earlier version (if any) registered, as in the JS. GUI thread only.
Loaded loadFile(const QString& path);
// Every *.js file in dir, in name order. GUI thread only.
QList<Loaded> loadFolder(const QString& dir);
// The problems of a source text without registering anything: the format problems, or one entry with the JS error
// when the code does not even load. Empty = it would load. GUI thread only.
QStringList check(const QString& source, const QString& fileName = QStringLiteral("check.js"));
// Loads a file into a Def without registering it (same checks as loadFile). nullopt-like: def.id empty on failure.
ind::Def build(const QString& path, Loaded* info);

}  // namespace charts::custom
