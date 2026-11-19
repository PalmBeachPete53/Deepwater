// Port of quant-terminal public/lib/indicators.js (MIT, Copyright (c) 2026 Lewis Jackson): the custom indicator
// format check (indicatorProblems, validateIndicator) and the trial bars.
#include "charts/core/IndicatorsValidate.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace charts::ind {

namespace js {

const Value& Value::get(const QString& k) const {
  static const Value undef;
  if (type != Object && type != Function && type != Array) return undef;
  for (const auto& p : props)
    if (p.first == k) return p.second;
  return undef;
}
void Value::set(const QString& k, Value v) {
  for (auto& p : props)
    if (p.first == k) {
      p.second = std::move(v);
      return;
    }
  props.append({k, std::move(v)});
}
bool Value::truthy() const {
  switch (type) {
    case Undefined:
    case Null: return false;
    case Bool: return b;
    case Number: return n != 0 && !std::isnan(n);
    case String: return !s.isEmpty();
    default: return true;
  }
}

Value fromJson(const QJsonValue& v) {
  switch (v.type()) {
    case QJsonValue::Null: return Value::null();
    case QJsonValue::Bool: return Value::boolean(v.toBool());
    case QJsonValue::Double: return Value::number(v.toDouble());
    case QJsonValue::String: return Value::string(v.toString());
    case QJsonValue::Array: {
      Value a = Value::array();
      for (const QJsonValue& x : v.toArray()) a.items << fromJson(x);
      return a;
    }
    case QJsonValue::Object: {
      Value o = Value::object();
      const QJsonObject obj = v.toObject();
      for (auto it = obj.begin(); it != obj.end(); ++it) o.set(it.key(), fromJson(it.value()));
      return o;
    }
    default: return Value();
  }
}

QJsonValue toJson(const Value& v) {
  switch (v.type) {
    case Value::Null: return QJsonValue::Null;
    case Value::Bool: return v.b;
    case Value::Number: return v.n;
    case Value::String: return v.s;
    case Value::Array: {
      QJsonArray a;
      for (const Value& x : v.items) {
        const QJsonValue j = toJson(x);
        a << (j.isUndefined() ? QJsonValue(QJsonValue::Null) : j);   // JSON.stringify writes null in an array
      }
      return a;
    }
    case Value::Object: {
      QJsonObject o;
      for (const auto& p : v.props) {
        const QJsonValue j = toJson(p.second);
        if (!j.isUndefined()) o.insert(p.first, j);
      }
      return o;
    }
    default: return QJsonValue(QJsonValue::Undefined);
  }
}

Value fromDef(const Def& d) {
  Value o = Value::object();
  if (!d.id.isEmpty()) o.set("id", Value::string(d.id));
  o.set("name", Value::string(d.name));
  if (!d.group.isEmpty()) o.set("group", Value::string(d.group));
  o.set("overlay", Value::boolean(d.overlay));
  Value inputs = Value::array();
  for (const Input& i : d.inputs) {
    Value x = Value::object();
    x.set("key", Value::string(i.key));
    if (!i.label.isEmpty()) x.set("label", Value::string(i.label));
    x.set("type", Value::string(i.type));
    x.set("default", fromJson(i.def));
    if (!std::isnan(i.min)) x.set("min", Value::number(i.min));
    if (!std::isnan(i.max)) x.set("max", Value::number(i.max));
    if (!std::isnan(i.step)) x.set("step", Value::number(i.step));
    if (i.type == "select" || !i.options.isEmpty()) {
      Value opts = Value::array();
      for (const QString& s : i.options) opts.items << Value::string(s);
      x.set("options", opts);
    }
    inputs.items << x;
  }
  o.set("inputs", inputs);
  Value outputs = Value::array();
  for (const Output& out : d.outputs) {
    Value x = Value::object();
    x.set("key", Value::string(out.key));
    if (!out.label.isEmpty()) x.set("label", Value::string(out.label));
    if (!out.kind.isEmpty()) x.set("kind", Value::string(out.kind));
    if (!out.color.isEmpty()) x.set("color", Value::string(out.color));
    if (out.lineWidth != 0) x.set("lineWidth", Value::number(out.lineWidth));
    for (auto it = out.extra.begin(); it != out.extra.end(); ++it) x.set(it.key(), fromJson(it.value()));
    outputs.items << x;
  }
  o.set("outputs", outputs);
  if (!d.levels.isEmpty()) {
    Value lv = Value::array();
    for (double l : d.levels) lv.items << Value::number(l);
    o.set("levels", lv);
  }
  for (auto it = d.extra.begin(); it != d.extra.end(); ++it) o.set(it.key(), fromJson(it.value()));
  if (d.calc) o.set("calc", Value::function());
  return o;
}

Value fromResult(const Result& r) {
  QStringList keys = r.keys;
  if (keys.isEmpty()) {
    keys = r.series.keys();
    keys.sort();
    if (!r.markers.isEmpty()) keys << "markers";
    if (!r.barColors.isEmpty()) keys << "barColors";
    if (!r.colors.isEmpty()) keys << "colors";
    if (!r.breaks.isEmpty()) keys << "breaks";
    if (!r.volumeProfile.isEmpty()) keys << "volumeProfile";
    if (!r.sessionProfile.isEmpty()) keys << "sessionProfile";
    keys += r.extra.keys();
    if (!r.error.isEmpty()) keys << "error";
    if (r.pending) keys << "pending";
  }
  Value o = Value::object();
  for (const QString& k : keys) {
    if (r.series.contains(k)) {
      Value a = Value::array();
      for (double v : r.series.value(k)) a.items << Value::number(v);
      o.set(k, a);
    } else if (k == "markers") {
      Value a = Value::array();
      for (const Marker& m : r.markers) {
        Value x = Value::object();
        x.set("time", Value::number(double(m.time)));
        x.set("position", Value::string(m.position));
        x.set("shape", Value::string(m.shape));
        if (!m.color.isEmpty()) x.set("color", Value::string(m.color));
        if (!m.text.isEmpty()) x.set("text", Value::string(m.text));
        for (auto it = m.extra.begin(); it != m.extra.end(); ++it) x.set(it.key(), fromJson(it.value()));
        a.items << x;
      }
      o.set(k, a);
    } else if (k == "barColors" || k == "colors") {
      Value a = Value::array();
      for (const QString& c : k == "colors" ? r.colors : r.barColors) a.items << (c.isEmpty() ? Value::null() : Value::string(c));
      o.set(k, a);
    } else if (k == "breaks") {
      Value a = Value::array();
      for (bool b : r.breaks) a.items << Value::boolean(b);
      o.set(k, a);
    } else if (k == "volumeProfile") o.set(k, fromJson(r.volumeProfile));
    else if (k == "sessionProfile") o.set(k, fromJson(r.sessionProfile));
    else if (k == "error") o.set(k, Value::string(r.error));
    else if (k == "pending") o.set(k, Value::boolean(r.pending));
    else if (r.extra.contains(k)) o.set(k, fromJson(r.extra.value(k)));
  }
  return o;
}

namespace {
// The custom indicator format (docs/custom-indicators.md), checked before an indicator loads, so a
// malformed file is refused with every problem named instead of breaking a chart later.
const QStringList INPUT_TYPES = {"int", "float", "select", "source", "time"};
const QStringList PRICE_FIELDS = {"close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "hlcc4", "volume"};
const QStringList MARKER_POSITIONS = {"aboveBar", "belowBar", "inBar"};
const QStringList MARKER_SHAPES = {"arrowUp", "arrowDown", "circle", "square"};
bool keyOk(const Value& k) {
  static const QRegularExpression KEY_RE("^[A-Za-z][A-Za-z0-9_]*$");
  return k.type == Value::String && KEY_RE.match(k.s).hasMatch();
}
bool inList(const Value& v, const QStringList& list) { return v.type == Value::String && list.contains(v.s); }
QString trimmed(const QString& s) { return s.trimmed(); }
// `${at}` for an input or output: its key in quotes when it has a text key, else its 1-based position.
QString atOf(const char* what, const Value& x, int k) {
  const Value& key = (x.type == Value::Object || x.type == Value::Array || x.type == Value::Function) ? x.get("key") : Value();
  return QString("%1 %2").arg(what, key.type == Value::String ? "\"" + key.s + "\"" : QString::number(k + 1));
}
}  // namespace

QStringList indicatorProblems(const Value& def, const TrialRun& trial, bool run) {
  QStringList problems;
  if (def.type != Value::Object) return {QT_TRANSLATE_NOOP("charts", "the file must `export default { name, calc(bars, p, ta) { ... } }`")};
  const Value& name = def.get("name");
  if (name.type != Value::String || trimmed(name.s).isEmpty()) problems << QT_TRANSLATE_NOOP("charts", "it needs a `name` (text)");
  else if (name.s.size() > 80) problems << QT_TRANSLATE_NOOP("charts", "its `name` is longer than 80 characters");
  if (def.get("calc").type != Value::Function) problems << QT_TRANSLATE_NOOP("charts", "it needs a `calc(bars, p, ta)` function");
  const Value& overlay = def.get("overlay");
  if (!overlay.isUndefined() && overlay.type != Value::Bool) problems << QT_TRANSLATE_NOOP("charts", "`overlay` must be true or false");

  Value inputs = def.get("inputs");   // def.inputs ?? []
  if (inputs.type == Value::Undefined || inputs.type == Value::Null) inputs = Value::array();
  QStringList seen;
  if (inputs.type != Value::Array) problems << QT_TRANSLATE_NOOP("charts", "`inputs` must be a list");
  else
    for (int k = 0; k < inputs.items.size(); ++k) {
      const Value& inp = inputs.items[k];
      const QString at = atOf("input", inp, k);
      if (!inp.truthy() || !inp.typeofObject()) {
        problems << at + ": must be an object like { key, type, default }";
        continue;
      }
      const Value& key = inp.get("key");
      if (!keyOk(key)) problems << at + ": `key` must be letters, digits or _ and start with a letter";
      else if (seen.contains(key.s)) problems << at + ": the key is used twice";
      if (key.type == Value::String) seen << key.s;
      const Value& type = inp.get("type");
      if (!inList(type, INPUT_TYPES)) {
        problems << at + ": `type` must be one of " + INPUT_TYPES.join(", ");
        continue;
      }
      const Value& d = inp.get("default");
      if (type.s == "int" && !d.isInteger()) problems << at + ": an int input needs a whole-number default";
      if (type.s == "float" && !d.isFiniteNumber()) problems << at + ": a float input needs a number default";
      if (type.s == "time" && !(d.isFiniteNumber() && d.n >= 0))
        problems << at + ": a time input's default is a bar time in seconds, or 0";
      if (type.s == "select") {
        const Value& options = inp.get("options");
        bool allText = options.type == Value::Array;
        QStringList opts;
        for (const Value& o : options.items) {
          if (o.type != Value::String) allText = false;
          opts << o.s;
        }
        if (options.type != Value::Array || options.items.isEmpty() || !allText)
          problems << at + ": a select input needs `options`, a list of text choices";
        else if (!inList(d, opts))
          problems << at + ": its default must be one of its options";
      }
      if (type.s == "source") {
        if (!(key.type == Value::String && key.s == "source")) problems << at + ": a source input's key must be \"source\"";
        if (!inList(d, PRICE_FIELDS)) problems << at + ": a source input's default must be one of " + PRICE_FIELDS.join(", ");
      }
      for (const char* m : {"min", "max"}) {
        const Value& v = inp.get(m);
        if (!v.isUndefined() && !v.isFiniteNumber()) problems << at + QString(": `%1` must be a number").arg(m);
      }
    }

  const Value& outputs = def.get("outputs");
  QStringList outKeys;
  if (outputs.type != Value::Array || outputs.items.isEmpty()) problems << QT_TRANSLATE_NOOP("charts", "`outputs` must be a list with at least one output");
  else
    for (int k = 0; k < outputs.items.size(); ++k) {
      const Value& o = outputs.items[k];
      const QString at = atOf("output", o, k);
      if (!o.truthy() || !o.typeofObject()) {
        problems << at + ": must be an object like { key, kind, color }";
        continue;
      }
      const Value& key = o.get("key");
      if (!keyOk(key)) problems << at + ": `key` must be letters, digits or _ and start with a letter";
      else if (outKeys.contains(key.s)) problems << at + ": the key is used twice";
      if (key.type == Value::String) outKeys << key.s;
      const Value& kind = o.get("kind");
      if (!kind.isUndefined() && !inList(kind, {"line", "histogram"})) problems << at + ": `kind` must be \"line\" or \"histogram\"";
      const Value& color = o.get("color");
      if (!color.isUndefined() && (color.type != Value::String || trimmed(color.s).isEmpty()))
        problems << at + ": `color` must be a colour like #2962ff";
      const Value& lw = o.get("lineWidth");
      if (!lw.isUndefined() && !(lw.isInteger() && lw.n >= 1 && lw.n <= 4)) problems << at + ": `lineWidth` must be 1, 2, 3 or 4";
    }
  const Value& levels = def.get("levels");
  if (!levels.isUndefined()) {
    bool ok = levels.type == Value::Array;
    for (const Value& l : levels.items)
      if (!l.isFiniteNumber()) ok = false;
    if (!ok) problems << QT_TRANSLATE_NOOP("charts", "`levels` must be a list of numbers");
  }

  // A trial run on test bars: calc must return one value per bar for every output.
  if (run && problems.isEmpty()) {
    const Bars bars = trialBars();
    Value p = Value::object();
    for (const Value& i : inputs.items) {
      const Value& key = i.get("key");
      p.set(key.type == Value::String ? key.s : QString(), i.get("default"));
    }
    const Trial t = trial(bars, p);
    const Value& res = t.result;
    if (t.threw) problems << "calc() stopped with an error on test bars: " + t.error;
    if (problems.isEmpty()) {
      if (!res.truthy() || !res.typeofObject())
        problems << QT_TRANSLATE_NOOP("charts", "calc() must return an object, like { value: [...] }");
      else {
        for (const Value& o : outputs.items) {
          const QString key = o.get("key").s;
          const Value& v = res.get(key);
          if (v.type != Value::Array)
            problems << QString("output \"%1\": calc() returned no list for it. Return { %1: [...] }").arg(key);
          else if (v.items.size() != bars.size())
            problems << QString("output \"%1\": calc() returned %2 values, but there are %3 bars. Return one value per bar (NaN where there isn't one)")
                            .arg(key)
                            .arg(v.items.size())
                            .arg(bars.size());
          else if (std::any_of(v.items.begin(), v.items.end(), [](const Value& x) {
                     return x.type != Value::Null && x.type != Value::Undefined && x.type != Value::Number;
                   }))
            problems << QString("output \"%1\": every value must be a number (or NaN/null)").arg(key);
        }
        const Value& bc = res.get("barColors");
        if (!bc.isUndefined()) {
          bool ok = bc.type == Value::Array && bc.items.size() == bars.size();
          for (const Value& c : bc.items)
            if (!(c.type == Value::Null || c.type == Value::Undefined || c.type == Value::String)) ok = false;
          if (!ok)
            problems << QString("barColors: return one colour per bar (%1), like \"#26a69a\", or null to keep a bar's normal colour")
                            .arg(bars.size());
        }
        const Value& markers = res.get("markers");
        if (!markers.isUndefined()) {
          bool ok = markers.type == Value::Array;
          for (const Value& m : markers.items)
            if (!(m.truthy() && m.get("time").isFiniteNumber() && inList(m.get("position"), MARKER_POSITIONS) &&
                  inList(m.get("shape"), MARKER_SHAPES)))
              ok = false;
          if (!ok)
            problems << QString("markers: each needs a time in seconds, a position (%1) and a shape (%2)")
                            .arg(MARKER_POSITIONS.join(", "), MARKER_SHAPES.join(", "));
        }
      }
    }
  }
  return problems;
}

}  // namespace js

// 300 made-up daily bars, the same every time, for the trial run.
Bars trialBars(int n) {
  Bars out;
  double price = 100;
  qint64 seed = 42;
  auto rand = [&]() {
    seed = (seed * 16807) % 2147483647;
    return double(seed) / 2147483647;
  };
  for (int i = 0; i < n; ++i) {
    const double open = price;
    price = jsMax({1, price * (1 + (rand() - 0.5) * 0.04)});
    const double high = jsMax({open, price}) * (1 + rand() * 0.01);
    const double low = jsMin({open, price}) * (1 - rand() * 0.01);
    Bar b;
    b.time = 1577836800 + qint64(i) * 86400;
    b.open = open;
    b.high = high;
    b.low = low;
    b.close = price;
    b.volume = jsRound(1000 + rand() * 9000);
    out << b;
  }
  return out;
}

QStringList problems(const Def& def, bool run) {
  const js::Value v = js::fromDef(def);
  return js::indicatorProblems(
      v,
      [&def](const Bars& bars, const js::Value&) -> js::Trial {
        // p = Object.fromEntries(inputs.map((i) => [i.key, i.default]))
        Params p;
        for (const Input& i : def.inputs) p.insert(i.key, i.def);
        js::Trial t;
        try {
          t.result = js::fromResult(def.calc(bars, p));
        } catch (const std::exception& e) {
          t.threw = true;
          t.error = QString::fromUtf8(e.what());
        } catch (...) {
          t.threw = true;
          t.error = QStringLiteral("undefined");
        }
        return t;
      },
      run);
}

QString formatProblems(const QStringList& problems) {
  return "This indicator doesn't follow the format (docs/custom-indicators.md):\n- " + problems.join("\n- ");
}

void validateIndicator(const Def& def, bool run) {
  const QStringList pr = problems(def, run);
  if (!pr.isEmpty()) throw std::runtime_error(formatProblems(pr).toStdString());
}

}  // namespace charts::ind
