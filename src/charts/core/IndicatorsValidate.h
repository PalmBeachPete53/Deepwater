#pragma once
// Port of quant-terminal public/lib/indicators.js (MIT, Copyright (c) 2026 Lewis Jackson): indicatorProblems.
// Internal to the indicators port (used by IndicatorsValidate.cpp and CustomJs.cpp). The JS check looks at the
// JS types of a definition object (typeof, Array.isArray, Number.isInteger ...), so it runs here on a small JS value
// tree: a C++ Def is turned into the object it stands for (fromDef), a custom file's object is read from QuickJS
// (CustomJs.cpp) as it is. Both then get the same checks and the same messages.
#include "charts/core/Indicators.h"
#include <QPair>

namespace charts::ind::js {

struct Value {
  enum Type { Undefined, Null, Bool, Number, String, Array, Object, Function };
  Type type = Undefined;
  double n = 0;
  bool b = false;
  QString s;
  QVector<Value> items;                    // Array
  QVector<QPair<QString, Value>> props;    // Object / Function: own enumerable properties, in order

  static Value number(double v) { Value x; x.type = Number; x.n = v; return x; }
  static Value string(const QString& v) { Value x; x.type = String; x.s = v; return x; }
  static Value boolean(bool v) { Value x; x.type = Bool; x.b = v; return x; }
  static Value null() { Value x; x.type = Null; return x; }
  static Value array() { Value x; x.type = Array; return x; }
  static Value object() { Value x; x.type = Object; return x; }
  static Value function() { Value x; x.type = Function; return x; }

  const Value& get(const QString& k) const;     // obj[k]; undefined when absent or not an object
  void set(const QString& k, Value v);
  bool isUndefined() const { return type == Undefined; }
  bool truthy() const;                          // JS ToBoolean
  bool isFiniteNumber() const { return type == Number && std::isfinite(n); }   // Number.isFinite
  bool isInteger() const { return isFiniteNumber() && std::trunc(n) == n; }   // Number.isInteger
  bool typeofObject() const { return type == Null || type == Array || type == Object; }   // typeof x === "object"
};

Value fromJson(const QJsonValue& v);
QJsonValue toJson(const Value& v);              // functions and undefined -> undefined (QJsonValue::Undefined)
Value fromDef(const Def& d);                    // the JS object a C++ Def stands for (calc becomes a Function)
Value fromResult(const Result& r);              // the JS object a calc returning r would have returned

// The trial run: call calc(bars, p, ta); give what it returned, or that it threw and the error's message.
struct Trial { Value result; bool threw = false; QString error; };
using TrialRun = std::function<Trial(const Bars& bars, const Value& p)>;
// JS indicatorProblems(def, { run }).
QStringList indicatorProblems(const Value& def, const TrialRun& trial, bool run = true);

}  // namespace charts::ind::js
