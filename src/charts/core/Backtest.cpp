// Port of quant-terminal public/lib/backtest.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Strategy tester. A strategy is plain JSON so the UI, alerts and your AI agent can all write one (see Backtest.h).
#include "charts/core/Backtest.h"
#include "charts/core/Series.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <array>
#include <cmath>

// Exactness with the JS: no fused multiply-add (clang contracts a*b+c by default on arm64).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#else
#pragma STDC FP_CONTRACT OFF
#endif

namespace charts::bt {

namespace {

double jsMax(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return NaN;
  if (a == b) return std::signbit(a) ? b : a;
  return a > b ? a : b;
}
double jsMin(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return NaN;
  if (a == b) return std::signbit(a) ? a : b;
  return a < b ? a : b;
}
[[noreturn]] void fail(const QString& msg) { throw std::runtime_error(msg.toStdString()); }

// Number(string): StringToNumber of the spec (whitespace trimmed, "" -> 0, Infinity, 0x / 0o / 0b, decimals).
double numberFromString(const QString& raw) {
  const QString s = raw.trimmed();
  if (s.isEmpty()) return 0;
  static const QRegularExpression dec(QStringLiteral("^[+-]?(?:(?:[0-9]+\\.?[0-9]*|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?|Infinity)$"));
  if (dec.match(s).hasMatch()) {
    if (s.endsWith(QLatin1String("Infinity")))
      return s.startsWith('-') ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    bool ok = false;
    const double v = s.toDouble(&ok);   // correctly rounded (double-conversion), as JS
    return ok ? v : NaN;
  }
  static const QRegularExpression radix(QStringLiteral("^0([xXoObB])([0-9a-fA-F]+)$"));
  const auto m = radix.match(s);
  if (m.hasMatch()) {
    const QChar r = m.captured(1).at(0).toLower();
    const int base = r == 'x' ? 16 : r == 'o' ? 8 : 2;
    double v = 0;
    for (QChar c : m.captured(2)) {
      const int d = c.isDigit() ? c.unicode() - '0' : c.toLower().unicode() - 'a' + 10;
      if (d >= base) return NaN;
      v = v * base + d;
    }
    return v;
  }
  return NaN;
}

QJsonValue jnum(double v) {
  if (!std::isfinite(v)) return QJsonValue(QJsonValue::Null);   // JSON.stringify: NaN / Infinity -> null
  return QJsonValue(v == 0 ? 0.0 : v);                            // and -0 -> 0
}

const QStringList PRICE_FIELDS = {"open", "high", "low", "close", "volume", "hl2", "hlc3", "ohlc4", "hlcc4"};

QJsonArray rulesOf(const QJsonObject& s, const QString& key) {
  const QJsonValue v = s.value(key);
  if (!v.isArray()) fail(QStringLiteral("s.%1.map is not a function").arg(key));
  return v.toArray();
}

}  // namespace

// ---------------------------------------------------------------- JS values

double jsNum(const QJsonValue& v) {
  switch (v.type()) {
    case QJsonValue::Double: return v.toDouble();
    case QJsonValue::Bool: return v.toBool() ? 1 : 0;
    case QJsonValue::Null: return 0;
    case QJsonValue::String: return numberFromString(v.toString());
    case QJsonValue::Array: return numberFromString(jsString(v));   // Number([]) = 0, Number([5]) = 5, [1,2] NaN
    default: return NaN;        // undefined, objects
  }
}

bool jsTruthy(const QJsonValue& v) {
  switch (v.type()) {
    case QJsonValue::Double: { const double d = v.toDouble(); return d != 0 && !std::isnan(d); }
    case QJsonValue::Bool: return v.toBool();
    case QJsonValue::String: return !v.toString().isEmpty();
    case QJsonValue::Array:
    case QJsonValue::Object: return true;
    default: return false;      // null, undefined
  }
}

QString jsString(const QJsonValue& v) {
  switch (v.type()) {
    case QJsonValue::Double: return series::numStr(v.toDouble());
    case QJsonValue::Bool: return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Null: return QStringLiteral("null");
    case QJsonValue::String: return v.toString();
    case QJsonValue::Object: return QStringLiteral("[object Object]");
    case QJsonValue::Array: {
      QStringList parts;
      for (const QJsonValue& x : v.toArray()) parts << (x.isNull() || x.isUndefined() ? QString() : jsString(x));
      return parts.join(QLatin1Char(','));
    }
    default: return QStringLiteral("undefined");
  }
}

// ---------------------------------------------------------------- operands

// A drawn line as an operand: { line: { t1, p1, t2, p2, extend: 'none' | 'right' | 'both' } }.
// Its price at time t, or NaN where the line doesn't reach (a trend line outside its two points,
// a ray behind its start).
std::function<double(double)> linePrice(const QJsonObject& line) {
  const double t1 = jsNum(line.value("t1")), p1 = jsNum(line.value("p1"));
  const double t2 = jsNum(line.value("t2")), p2 = jsNum(line.value("p2"));
  const QJsonValue ext = line.value("extend");
  // extend = "none" is the destructuring default: only a missing extend gets it.
  const QString extend = ext.isUndefined() ? QStringLiteral("none") : ext.isString() ? ext.toString() : QString();
  const bool extNone = (ext.isUndefined() || ext.isString()) && extend == "none";
  const bool extRight = ext.isString() && extend == "right";
  const double slope = t2 != t1 ? (p2 - p1) / (t2 - t1) : 0;
  const double lo = jsMin(t1, t2), hi = jsMax(t1, t2);
  return [=](double t) {
    if (extNone && (t < lo || t > hi)) return NaN;
    if (extRight && t2 != t1 && (t - t1) * (t2 - t1) < 0) return NaN;
    return p1 + slope * (t - t1);
  };
}

Resolver resolveSeries(const Bars& bars, const QJsonObject& indicators, std::shared_ptr<OperandCache> cache) {
  if (!cache) cache = std::make_shared<OperandCache>();
  return [bars, indicators, cache](const QJsonValue& operand) -> Operand {
    const int n = bars.size();
    if (operand.isObject() && jsTruthy(operand.toObject().value("line"))) {
      const auto at = linePrice(operand.toObject().value("line").toObject());
      return [bars, at, n](int i) { return i >= 0 && i < n ? at(double(bars[i].time)) : NaN; };
    }
    if (operand.isDouble()) {
      const double v = operand.toDouble();
      return [v](int) { return v; };
    }
    if (operand.isString() && !operand.toString().trimmed().isEmpty() &&
        !std::isnan(numberFromString(operand.toString()))) {
      const double v = numberFromString(operand.toString());
      return [v](int) { return v; };
    }
    if (operand.isString() && PRICE_FIELDS.contains(operand.toString())) {
      const QString f = operand.toString();
      if (!cache->prices.contains(f)) cache->prices.insert(f, ind::source(bars, f));
      const Series s = cache->prices.value(f);
      return [s](int i) { return i >= 0 && i < s.size() ? s[i] : NaN; };
    }
    // const [key, out] = String(operand).split(".");
    const QStringList parts = jsString(operand).split(QLatin1Char('.'));
    const QString key = parts.value(0);
    const bool hasOut = parts.size() > 1;
    const QString out = parts.value(1);
    const QJsonValue spec = indicators.value(key);
    if (!jsTruthy(spec))
      fail(QStringLiteral(QT_TRANSLATE_NOOP("charts", "Unknown operand \"%1\". Define it under indicators or use a price field or number."))
               .arg(jsString(operand)));
    const QJsonObject sp = spec.toObject();
    const QString id = jsString(sp.value("id"));
    const QString ck = QLatin1Char('@') + key;
    if (!cache->indicators.contains(ck)) {
      const QJsonValue pv = sp.value("params");
      cache->indicators.insert(ck, ind::compute(id, bars, jsTruthy(pv) ? pv.toObject() : QJsonObject{}));
    }
    const ind::Result& res = cache->indicators[ck];
    QString outKey;
    if (hasOut && !out.isEmpty()) outKey = out;
    else if (jsTruthy(sp.value("output"))) outKey = jsString(sp.value("output"));
    else {
      const ind::DefPtr def = ind::byId(id);
      outKey = def && !def->outputs.isEmpty() && !def->outputs[0].key.isEmpty() ? def->outputs[0].key
                                                                                 : QStringLiteral("value");
    }
    if (!res.series.contains(outKey))
      fail(QStringLiteral(QT_TRANSLATE_NOOP("charts", "Indicator \"%1\" (%2) has no output \"%3\"")).arg(key, id, outKey));
    // Outputs drawn from later bars (Ichimoku's lagging span, a centred DPO, Zig Zag) would let a rule see the future
    // (audit D12): a backtest would trade on it, an alert would fire on it. Refused, with the reason.
    if (const ind::DefPtr def = ind::byId(id); def && ind::readsAhead(*def, outKey, jsTruthy(sp.value("params")) ? sp.value("params").toObject() : QJsonObject{}))
      fail(QStringLiteral(QT_TRANSLATE_NOOP("charts", "Indicator \"%1\" (%2): output \"%3\" is computed from later bars (it is only for the chart), so a rule can't use it"))
               .arg(key, id, outKey));
    const Series s = res.series.value(outKey);
    return [s](int i) { return i >= 0 && i < s.size() ? s[i] : NaN; };
  };
}

std::function<Condition(const QJsonObject&)> conditionFn(const Bars& bars, const QJsonObject& indicators,
                                                         std::shared_ptr<OperandCache> cache) {
  const Resolver get = resolveSeries(bars, indicators, cache);
  return [get](const QJsonObject& rule) -> Condition {
    const Operand L = get(rule.value("left")), R = get(rule.value("right"));
    const QJsonValue opv = rule.value("op");
    const int op = opv.isString() ? OPS.indexOf(opv.toString()) : -1;
    const QString opName = jsString(opv);
    return [L, R, op, opName](int i) {
      const double l = L(i), r = R(i);
      if (std::isnan(l) || std::isnan(r)) return false;
      switch (op) {
        case 0: return l > r;
        case 1: return l < r;
        case 2: return l >= r;
        case 3: return l <= r;
        case 4: return i > 0 && l > r && L(i - 1) <= R(i - 1);
        case 5: return i > 0 && l < r && L(i - 1) >= R(i - 1);
        case 6: return i > 0 && ((l > r && L(i - 1) <= R(i - 1)) || (l < r && L(i - 1) >= R(i - 1)));
        default: fail(QStringLiteral(QT_TRANSLATE_NOOP("charts", "Unknown operator \"%1\"")).arg(opName));
      }
    };
  };
}

// FILLS: a bar only gives open, high, low and close, so where a stop or target filled inside it is a guess.
//   'worst'      stop first whenever a bar touches both (cautious)
//   'ohlc'       TradingView's broker emulator: open, then whichever of high or low is nearer the open, then the
//                other, then close
//   'magnifier'  TradingView's bar magnifier: walk the real lower-timeframe bars inside each bar, each on the
//                'ohlc' path. Pass them as backtest(bars, strategy, subBars); bars without any use 'ohlc'.
namespace {
std::array<double, 4> pathOf(const Bar& x) {
  if (std::fabs(x.open - x.high) < std::fabs(x.open - x.low)) return {x.open, x.high, x.low, x.close};
  return {x.open, x.low, x.high, x.close};
}
}  // namespace

// The lower timeframe the magnifier uses: the finest that covers the whole test in at most `max` bars.
std::optional<MagnifierTf> magnifierTf(double tfSeconds, double spanSeconds, double max) {
  static const QVector<QPair<QString, int>> tfs = {{"1m", 60}, {"5m", 300}, {"15m", 900}, {"1h", 3600}, {"4h", 14400}};
  for (const auto& [tf, sec] : tfs)
    if (sec < tfSeconds && spanSeconds / sec <= max) return MagnifierTf{tf, sec};
  return std::nullopt;
}

namespace {

Condition allOf(QVector<Condition> fns) {
  return [fns](int i) {
    if (fns.isEmpty()) return false;
    for (const auto& f : fns) if (!f(i)) return false;
    return true;
  };
}
Condition anyOf(QVector<Condition> fns) {
  return [fns](int i) {
    for (const auto& f : fns) if (f(i)) return true;
    return false;
  };
}

double periodsPerYear(const Bars& bars) {
  if (bars.size() < 2) return 252;
  const double step = double(bars.last().time - bars[0].time) / (bars.size() - 1);
  return (365.0 * 86400) / step;
}

}  // namespace

Result backtest(const Bars& bars, const QJsonObject& strategy, const Bars& subBars) {
  QJsonObject s{{"initialCapital", 10000}, {"positionPct", 100}, {"feePct", 0.1}, {"slippagePct", 0.05},
                {"spreadPct", 0}, {"intrabar", "worst"}, {"stopLossPct", 0}, {"takeProfitPct", 0},
                {"trailingStopPct", 0}, {"exitMatch", "all"}, {"maxBarsInTrade", 0}, {"entryLong", QJsonArray()},
                {"exitLong", QJsonArray()}, {"entryShort", QJsonArray()}, {"exitShort", QJsonArray()}};
  for (auto it = strategy.begin(); it != strategy.end(); ++it) s.insert(it.key(), it.value());

  const QJsonValue indV = s.value("indicators");
  const auto cond = conditionFn(bars, jsTruthy(indV) ? indV.toObject() : QJsonObject{});
  const bool anyExit = s.value("exitMatch").toString() == "any" && s.value("exitMatch").isString();
  auto build = [&](const QString& key) {
    QVector<Condition> fns;
    for (const QJsonValue& r : rulesOf(s, key)) fns.push_back(cond(r.toObject()));
    return fns;
  };
  const Condition eL = allOf(build("entryLong"));
  const Condition xL = anyExit ? anyOf(build("exitLong")) : allOf(build("exitLong"));
  const Condition eS = allOf(build("entryShort"));
  const Condition xS = anyExit ? anyOf(build("exitShort")) : allOf(build("exitShort"));
  const double fee = jsNum(s.value("feePct")) / 100, slip = jsNum(s.value("slippagePct")) / 100,
               half = jsNum(s.value("spreadPct")) / 200;
  const double positionPct = jsNum(s.value("positionPct"));
  const QJsonValue stopV = s.value("stopLossPct"), targetV = s.value("takeProfitPct"),
                   trailV = s.value("trailingStopPct");
  const bool hasTrail = jsTruthy(trailV);
  const double stopPct = jsNum(stopV), targetPct = jsNum(targetV), trailPct = jsNum(trailV);
  const double maxBarsInTrade = jsNum(s.value("maxBarsInTrade"));
  const QJsonValue intrabarV = s.value("intrabar");
  const QString intrabar = intrabarV.isString() ? intrabarV.toString() : QString();
  const bool walk = intrabar == "ohlc" || intrabar == "magnifier";

  // Sub-bars inside each bar, for the magnifier: subAt[i] = [from, to) into subBars.
  QVector<QPair<int, int>> subAt;
  int covered = 0;
  if (intrabar == "magnifier" && !subBars.isEmpty()) {
    int j = 0;
    for (int i = 0; i < bars.size(); ++i) {
      const qint64 end = i + 1 < bars.size() ? bars[i + 1].time
                                             : bars[i].time + (i ? bars[i].time - bars[i - 1].time : 86400);
      while (j < subBars.size() && subBars[j].time < bars[i].time) j++;
      const int from = j;
      while (j < subBars.size() && subBars[j].time < end) j++;
      subAt.push_back({from, j});
      if (j > from) covered++;
    }
  }

  double cash = jsNum(s.value("initialCapital"));
  struct Pos {
    int side = 0;
    double qty = 0, entry = 0;
    qint64 entryTime = 0;
    int entryIndex = 0;
    QString reason;
    double peak = 0;
    std::optional<double> stop, target;
  };
  std::optional<Pos> pos;   // { side: 1|-1, qty, entry, entryTime, entryIndex, stop, target, peak }
  Result result;
  QVector<Trade>& trades = result.trades;
  QVector<EquityPoint>& equity = result.equity;
  equity.reserve(bars.size());
  // order to fill at next open: { side, reason } or { close: true, reason }
  struct Pending { bool close = false; int side = 0; QString reason; };
  std::optional<Pending> pending;

  // What the position would really fill at: the ask to buy, the bid to sell (price is the mid).
  auto exitQuote = [&](double price) { return price * (1 - pos->side * half); };
  auto openPos = [&](int i, int side, double price, const QString& reason) {
    const double fill = price * (1 + side * half) * (1 + side * slip);
    const double alloc = (cash * positionPct) / 100;
    const double qty = alloc / (fill * (1 + fee));
    if (qty <= 0) return;
    cash -= qty * fill * (1 + fee);
    Pos p;
    p.side = side;
    p.qty = qty;
    p.entry = fill;
    p.entryTime = bars[i].time;
    p.entryIndex = i;
    p.reason = reason;
    p.peak = fill;
    if (jsTruthy(stopV)) p.stop = fill * (1 - (side * stopPct) / 100);
    if (jsTruthy(targetV)) p.target = fill * (1 + (side * targetPct) / 100);
    pos = p;
  };

  auto closePos = [&](int i, double price, const QString& reason) {
    const double fill = price * (1 - pos->side * slip);
    const double proceeds = pos->side == 1 ? pos->qty * fill : pos->qty * (2 * pos->entry - fill);
    const double net = proceeds * (1 - fee);
    const double cost = pos->qty * pos->entry * (1 + fee);
    cash += net;
    Trade t;
    t.side = pos->side == 1 ? QStringLiteral("long") : QStringLiteral("short");
    t.entryTime = pos->entryTime;
    t.exitTime = bars[i].time;
    t.entryPrice = pos->entry;
    t.exitPrice = fill;
    t.qty = pos->qty;
    t.pnl = net - cost;
    t.pnlPct = ((net - cost) / cost) * 100;
    t.bars = i - pos->entryIndex;
    t.entryReason = pos->reason;
    t.exitReason = reason;
    trades.push_back(t);
    pos.reset();
  };

  auto trail = [&](double p) {
    pos->peak = pos->side == 1 ? jsMax(pos->peak, p) : jsMin(pos->peak, p);
    const double t = pos->peak * (1 - (pos->side * trailPct) / 100);
    pos->stop = !pos->stop ? t : pos->side == 1 ? jsMax(*pos->stop, t) : jsMin(*pos->stop, t);
  };
  auto beyond = [&](double p, const std::optional<double>& level, bool favour) {
    return level.has_value() && ((pos->side == 1) == favour ? p >= *level : p <= *level);
  };
  // 'worst': one look at the bar, stop first when both are touched.
  auto worstExit = [&](int i, const Bar& b) {
    const double hi = exitQuote(b.high), lo = exitQuote(b.low), op = exitQuote(b.open);
    if (hasTrail) trail(pos->side == 1 ? hi : lo);
    if (beyond(pos->side == 1 ? lo : hi, pos->stop, false))
      closePos(i, pos->side == 1 ? jsMin(*pos->stop, op) : jsMax(*pos->stop, op), QStringLiteral(QT_TRANSLATE_NOOP("charts", "Stop")));
    else if (beyond(pos->side == 1 ? hi : lo, pos->target, true))
      closePos(i, pos->side == 1 ? jsMax(*pos->target, op) : jsMin(*pos->target, op), QStringLiteral(QT_TRANSLATE_NOOP("charts", "Target")));
  };
  // 'ohlc' and 'magnifier': walk each segment's open, near extreme, far extreme, close in order.
  auto walkExit = [&](int i, const Bar* segs, int count) {
    for (int sgi = 0; sgi < count; ++sgi) {
      std::array<double, 4> pts = pathOf(segs[sgi]);
      for (double& p : pts) p = exitQuote(p);
      for (int k = 0; k < 4; k++) {
        const double p = pts[size_t(k)];
        if (k == 0) {
          // A segment that opens past a level gapped through it: filled at the open.
          if (hasTrail) trail(p);
          if (beyond(p, pos->stop, false)) return closePos(i, p, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Stop")));
          if (beyond(p, pos->target, true)) return closePos(i, p, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Target")));
          continue;
        }
        const bool favour = pos->side == 1 ? p > pts[size_t(k - 1)] : p < pts[size_t(k - 1)];
        if (favour) {
          if (beyond(p, pos->target, true)) return closePos(i, *pos->target, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Target")));
          if (hasTrail) trail(p);
        } else if (beyond(p, pos->stop, false)) return closePos(i, *pos->stop, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Stop")));
      }
    }
  };

  for (int i = 0; i < bars.size(); i++) {
    const Bar& b = bars[i];
    // 1. fill orders decided on the previous close
    if (pending) {
      if (pending->close && pos) closePos(i, exitQuote(b.open), pending->reason);
      if (pending->side && !pos) openPos(i, pending->side, b.open, pending->reason);
      pending.reset();
    }
    // 2. stops and targets inside this bar (see FILLS)
    if (pos && (pos->stop || pos->target || hasTrail)) {
      if (walk) {
        const QPair<int, int> r = i < subAt.size() ? subAt[i] : QPair<int, int>(0, 0);
        if (r.second > r.first) walkExit(i, subBars.constData() + r.first, r.second - r.first);
        else walkExit(i, &b, 1);
      } else worstExit(i, b);
    }
    // 3. signals on this close -> orders for the next open
    if (i < bars.size() - 1) {
      if (pos && pos->side == 1 && (xL(i) || eS(i)))
        pending = Pending{true, eS(i) ? -1 : 0, xL(i) ? QStringLiteral(QT_TRANSLATE_NOOP("charts", "Exit long")) : QStringLiteral(QT_TRANSLATE_NOOP("charts", "Reverse"))};
      else if (pos && pos->side == -1 && (xS(i) || eL(i)))
        pending = Pending{true, eL(i) ? 1 : 0, xS(i) ? QStringLiteral(QT_TRANSLATE_NOOP("charts", "Exit short")) : QStringLiteral(QT_TRANSLATE_NOOP("charts", "Reverse"))};
      // Time exit: the position has been open maxBarsInTrade bars at this close, so it goes at the next open.
      else if (pos && maxBarsInTrade > 0 && i - pos->entryIndex + 1 >= maxBarsInTrade)
        pending = Pending{true, 0, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Time exit"))};
      else if (!pos && eL(i)) pending = Pending{false, 1, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Long entry"))};
      else if (!pos && eS(i)) pending = Pending{false, -1, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Short entry"))};
    }
    // 4. mark to market
    const double mtm = pos ? pos->side == 1 ? pos->qty * b.close : pos->qty * (2 * pos->entry - b.close) : 0;
    equity.push_back({b.time, cash + mtm});
  }
  if (pos) closePos(bars.size() - 1, exitQuote(bars.last().close), QStringLiteral(QT_TRANSLATE_NOOP("charts", "End of data")));
  if (!equity.isEmpty()) equity.last().value = cash;

  result.stats = stats(bars, trades, equity, s);
  Stats& st = result.stats;
  st.hasFills = true;
  st.fills = intrabar == "magnifier"
                 ? QStringLiteral(QT_TRANSLATE_NOOP("charts", "Bar magnifier: lower-timeframe bars inside %1 of %2 bars%3"))
                       .arg(covered)
                       .arg(bars.size())
                       .arg(covered < bars.size() ? QStringLiteral(QT_TRANSLATE_NOOP("charts", ", the rest on the OHLC path")) : QString())
             : intrabar == "ohlc" ? QStringLiteral(QT_TRANSLATE_NOOP("charts", "OHLC path: open, nearer extreme, far extreme, close"))
                                  : QStringLiteral(QT_TRANSLATE_NOOP("charts", "Stop first when a bar touches both"));
  st.intrabarCovered = covered;
  return result;
}

Stats stats(const Bars& bars, const QVector<Trade>& trades, const QVector<EquityPoint>& equity, const QJsonObject& s) {
  const double start = jsNum(s.value("initialCapital")), end = equity.isEmpty() ? start : equity.last().value;
  double peak = -std::numeric_limits<double>::infinity(), maxDd = 0;
  for (const EquityPoint& e : equity) {
    peak = jsMax(peak, e.value);
    maxDd = jsMax(maxDd, (peak - e.value) / peak);
  }
  QVector<double> rets;
  for (int i = 1; i < equity.size(); ++i) rets.push_back(equity[i].value / equity[i - 1].value - 1);
  const double cnt = rets.isEmpty() ? 1 : rets.size();
  double sum = 0;
  for (double r : rets) sum += r;
  const double mean = sum / cnt;
  double sq = 0, dq = 0;
  for (double r : rets) { sq += (r - mean) * (r - mean); const double m = jsMin(r, 0); dq += m * m; }
  const double sd = std::sqrt(sq / cnt), downside = std::sqrt(dq / cnt);
  const double ppy = periodsPerYear(bars);
  QVector<const Trade*> wins, losses;
  for (const Trade& t : trades) {
    if (t.pnl > 0) wins.push_back(&t);
    if (t.pnl <= 0) losses.push_back(&t);
  }
  double grossWin = 0, lossSum = 0;
  for (auto t : wins) grossWin += t->pnl;
  for (auto t : losses) lossSum += t->pnl;
  const double grossLoss = -lossSum;
  const double years = bars.size() > 1 ? double(bars.last().time - bars[0].time) / (365.0 * 86400) : 0;
  int inMarket = 0;
  for (const Trade& t : trades) inMarket += t.bars;
  auto round = [](double v, int d = 2) { return std::isfinite(v) ? series::toFixed(v, d).toDouble() : NaN; };
  auto avgPct = [](const QVector<const Trade*>& ts) { double a = 0; for (auto t : ts) a += t->pnlPct; return a / ts.size(); };

  Stats o;
  o.netProfit = round(end - start);
  o.netProfitPct = round(((end - start) / start) * 100);
  o.buyHoldPct = bars.isEmpty() ? NaN : round(((bars.last().close - bars[0].close) / bars[0].close) * 100);
  o.cagrPct = years > 0 && end > 0 ? round((std::pow(end / start, 1 / years) - 1) * 100) : NaN;
  o.totalTrades = trades.size();
  o.winRatePct = trades.isEmpty() ? NaN : round((double(wins.size()) / trades.size()) * 100);
  o.profitFactor = grossLoss > 0 ? round(grossWin / grossLoss) : !wins.isEmpty() ? NaN : 0;
  o.maxDrawdownPct = round(maxDd * 100);
  if (!trades.isEmpty()) { double a = 0; for (const Trade& t : trades) a += t.pnlPct; o.avgTradePct = round(a / trades.size()); }
  o.avgWinPct = wins.isEmpty() ? NaN : round(avgPct(wins));
  o.avgLossPct = losses.isEmpty() ? NaN : round(avgPct(losses));
  if (!wins.isEmpty()) { double m = -std::numeric_limits<double>::infinity(); for (auto t : wins) m = jsMax(m, t->pnl); o.largestWin = round(m); }
  if (!losses.isEmpty()) { double m = std::numeric_limits<double>::infinity(); for (auto t : losses) m = jsMin(m, t->pnl); o.largestLoss = round(m); }
  o.sharpe = sd > 0 ? round((mean / sd) * std::sqrt(ppy)) : NaN;
  o.sortino = downside > 0 ? round((mean / downside) * std::sqrt(ppy)) : NaN;
  o.exposurePct = bars.isEmpty() ? NaN : round((double(inMarket) / bars.size()) * 100);
  const QJsonValue spread = s.value("spreadPct");
  o.feesAndSlippage = QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1% fee + %2% slippage per side%3"))
                          .arg(jsString(s.value("feePct")), jsString(s.value("slippagePct")),
                               jsTruthy(spread) ? QStringLiteral(QT_TRANSLATE_NOOP("charts", " + %1% spread")).arg(jsString(spread)) : QString());
  return o;
}

QString tradesCsv(const QVector<Trade>& trades) {
  QStringList lines = {QStringLiteral(
      "side,entry_time,exit_time,entry_price,exit_price,qty,pnl,pnl_pct,bars,entry_reason,exit_reason")};
  for (const Trade& t : trades)
    lines << QStringList{t.side,
                         series::isoString(t.entryTime),
                         series::isoString(t.exitTime),
                         series::numStr(t.entryPrice),
                         series::numStr(t.exitPrice),
                         series::numStr(t.qty),
                         series::toFixed(t.pnl, 2),
                         series::toFixed(t.pnlPct, 2),
                         QString::number(t.bars),
                         t.entryReason,
                         t.exitReason}
                 .join(QLatin1Char(','));
  return lines.join(QLatin1Char('\n'));
}

QJsonObject Trade::toJson() const {
  return {{"side", side}, {"entryTime", double(entryTime)}, {"exitTime", double(exitTime)},
          {"entryPrice", jnum(entryPrice)}, {"exitPrice", jnum(exitPrice)}, {"qty", jnum(qty)},
          {"pnl", jnum(pnl)}, {"pnlPct", jnum(pnlPct)}, {"bars", bars}, {"entryReason", entryReason},
          {"exitReason", exitReason}};
}

QJsonObject Stats::toJson() const {
  QJsonObject o{{"netProfit", jnum(netProfit)}, {"netProfitPct", jnum(netProfitPct)}, {"buyHoldPct", jnum(buyHoldPct)},
                {"cagrPct", jnum(cagrPct)}, {"totalTrades", totalTrades}, {"winRatePct", jnum(winRatePct)},
                {"profitFactor", jnum(profitFactor)}, {"maxDrawdownPct", jnum(maxDrawdownPct)},
                {"avgTradePct", jnum(avgTradePct)}, {"avgWinPct", jnum(avgWinPct)}, {"avgLossPct", jnum(avgLossPct)},
                {"largestWin", jnum(largestWin)}, {"largestLoss", jnum(largestLoss)}, {"sharpe", jnum(sharpe)},
                {"sortino", jnum(sortino)}, {"exposurePct", jnum(exposurePct)}, {"feesAndSlippage", feesAndSlippage}};
  if (hasFills) {
    o.insert("fills", fills);
    o.insert("intrabarCovered", intrabarCovered);
  }
  return o;
}

QJsonObject Result::toJson() const {
  QJsonArray t, e;
  for (const Trade& x : trades) t.append(x.toJson());
  for (const EquityPoint& p : equity) e.append(QJsonObject{{"time", double(p.time)}, {"value", jnum(p.value)}});
  return {{"trades", t}, {"equity", e}, {"stats", stats.toJson()}};
}

const QVector<QPair<QString, QJsonObject>>& EXAMPLE_STRATEGIES() {
  static const QVector<QPair<QString, QJsonObject>> list = [] {
    auto obj = [](const char* json) { return QJsonDocument::fromJson(QByteArray(json)).object(); };
    return QVector<QPair<QString, QJsonObject>>{
        {QT_TRANSLATE_NOOP("charts", "EMA cross (9/21)"), obj(R"({
          "indicators": { "fast": { "id": "ema", "params": { "length": 9 } }, "slow": { "id": "ema", "params": { "length": 21 } } },
          "entryLong": [{ "left": "fast", "op": "crossesAbove", "right": "slow" }],
          "exitLong": [{ "left": "fast", "op": "crossesBelow", "right": "slow" }] })")},
        {QT_TRANSLATE_NOOP("charts", "RSI mean reversion (30/55)"), obj(R"({
          "indicators": { "rsi": { "id": "rsi", "params": { "length": 14 } } },
          "entryLong": [{ "left": "rsi", "op": "crossesAbove", "right": 30 }],
          "exitLong": [{ "left": "rsi", "op": "crossesAbove", "right": 55 }],
          "stopLossPct": 8 })")},
        {QT_TRANSLATE_NOOP("charts", "Supertrend follow"), obj(R"({
          "indicators": { "st": { "id": "supertrend", "params": { "atrLength": 10, "factor": 3 }, "output": "direction" } },
          "entryLong": [{ "left": "st", "op": "<", "right": 0 }],
          "exitLong": [{ "left": "st", "op": ">", "right": 0 }] })")},
        {QT_TRANSLATE_NOOP("charts", "MACD + 200 EMA trend filter"), obj(R"({
          "indicators": { "macd": { "id": "macd", "params": {} }, "trend": { "id": "ema", "params": { "length": 200 } } },
          "entryLong": [
            { "left": "macd.macd", "op": "crossesAbove", "right": "macd.signal" },
            { "left": "close", "op": ">", "right": "trend" }
          ],
          "exitLong": [{ "left": "macd.macd", "op": "crossesBelow", "right": "macd.signal" }] })")},
        {QT_TRANSLATE_NOOP("charts", "Bollinger breakout"), obj(R"({
          "indicators": { "bb": { "id": "bb", "params": { "length": 20, "mult": 2 } } },
          "entryLong": [{ "left": "close", "op": "crossesAbove", "right": "bb.upper" }],
          "exitLong": [{ "left": "close", "op": "crossesBelow", "right": "bb.basis" }],
          "trailingStopPct": 10 })")},
    };
  }();
  return list;
}

QJsonObject exampleStrategy(const QString& name) {
  for (const auto& [n, s] : EXAMPLE_STRATEGIES())
    if (n == name) return s;
  return {};
}

}  // namespace charts::bt
