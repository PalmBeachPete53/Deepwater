#include "charts/core/BinanceMsg.h"

namespace charts::flow {
namespace {

template <class C>
inline void skipWs(const C*& p, const C* end) {
  while (p < end && (*p == C(' ') || *p == C('\n') || *p == C('\r') || *p == C('\t'))) ++p;
}

// The key text between quotes, compared without copying.
template <class C>
struct Key {
  const C* b = nullptr;
  const C* e = nullptr;
  bool is(const char* s) const {
    const C* p = b;
    for (; *s; ++s, ++p)
      if (p >= e || *p != C(*s)) return false;
    return p == e;
  }
};

// A JSON string without escapes (Binance keys and values never have any); p on the opening quote.
template <class C>
bool readString(const C*& p, const C* end, Key<C>& out) {
  if (p >= end || *p != C('"')) return false;
  out.b = ++p;
  while (p < end && *p != C('"')) {
    if (*p == C('\\')) return false;
    ++p;
  }
  if (p >= end) return false;
  out.e = p++;
  return true;
}

template <class C>
bool readInt(const C*& p, const C* end, int64_t& v) {
  const Decimal d = parseDecimal(p, end, &p);
  if (!d.ok || d.exp != 0) return false;
  v = d.mant;
  return true;
}

// A value we don't use: string, number, true/false/null (nested objects or arrays are refused: not in these messages).
template <class C>
bool skipValue(const C*& p, const C* end) {
  if (p >= end) return false;
  if (*p == C('"')) { Key<C> k; return readString(p, end, k); }
  if (*p == C('{') || *p == C('[')) return false;
  while (p < end && *p != C(',') && *p != C('}')) ++p;
  return true;
}

template <class C>
bool parseObject(const C*& p, const C* end, AggTrade& out) {
  out = AggTrade{};
  skipWs(p, end);
  if (p >= end || *p != C('{')) return false;
  ++p;
  bool haveA = false, haveP = false, haveQ = false, haveT = false, haveM = false;
  skipWs(p, end);
  if (p < end && *p == C('}')) return false;
  while (p < end) {
    skipWs(p, end);
    Key<C> key;
    if (!readString(p, end, key)) return false;
    skipWs(p, end);
    if (p >= end || *p != C(':')) return false;
    ++p;
    skipWs(p, end);
    if (key.is("e")) {
      Key<C> v;
      if (!readString(p, end, v) || !v.is("aggTrade")) return false;
    } else if (key.is("a")) {
      if (!readInt(p, end, out.id)) return false;
      haveA = true;
    } else if (key.is("p") || key.is("q")) {
      Key<C> v;
      if (!readString(p, end, v)) return false;
      const C* stop = nullptr;
      const Decimal d = parseDecimal(v.b, v.e, &stop);
      if (!d.ok || stop != v.e) return false;
      if (key.is("p")) { out.price = d; haveP = true; }
      else { out.qty = d; haveQ = true; }
    } else if (key.is("f")) {
      if (!readInt(p, end, out.firstId)) return false;
    } else if (key.is("l")) {
      if (!readInt(p, end, out.lastId)) return false;
    } else if (key.is("T")) {
      if (!readInt(p, end, out.timeMs)) return false;
      haveT = true;
    } else if (key.is("E")) {
      if (!readInt(p, end, out.eventMs)) return false;
    } else if (key.is("m")) {
      if (end - p >= 4 && p[0] == C('t') && p[1] == C('r') && p[2] == C('u') && p[3] == C('e')) { out.buyerMaker = true; p += 4; }
      else if (end - p >= 5 && p[0] == C('f') && p[1] == C('a') && p[2] == C('l') && p[3] == C('s') && p[4] == C('e')) { out.buyerMaker = false; p += 5; }
      else return false;
      haveM = true;
    } else if (!skipValue(p, end)) {
      return false;
    }
    skipWs(p, end);
    if (p >= end) return false;
    if (*p == C(',')) { ++p; continue; }
    if (*p == C('}')) { ++p; return haveA && haveP && haveQ && haveT && haveM; }
    return false;
  }
  return false;
}

// [["price","qty"], ...] → levels in ticks
template <class C>
bool readLevels(const C*& p, const C* end, const TickScale& sc, std::vector<BookLevel>& out) {
  if (p >= end || *p != C('[')) return false;
  ++p;
  skipWs(p, end);
  if (p < end && *p == C(']')) { ++p; return true; }
  while (p < end) {
    skipWs(p, end);
    if (p >= end || *p != C('[')) return false;
    ++p;
    skipWs(p, end);
    Key<C> ps, qs;
    if (!readString(p, end, ps)) return false;
    skipWs(p, end);
    if (p >= end || *p != C(',')) return false;
    ++p;
    skipWs(p, end);
    if (!readString(p, end, qs)) return false;
    skipWs(p, end);
    if (p >= end || *p != C(']')) return false;
    ++p;
    const C* s1 = nullptr;
    const C* s2 = nullptr;
    const Decimal pd = parseDecimal(ps.b, ps.e, &s1), qd = parseDecimal(qs.b, qs.e, &s2);
    if (!pd.ok || !qd.ok || s1 != ps.e || s2 != qs.e) return false;
    const std::optional<int64_t> t = sc.tryTicksOf(pd);
    if (!t) return false;   // a price that can't be counted on the grid: the message is malformed
    out.push_back(BookLevel{*t, qd.toDouble()});
    skipWs(p, end);
    if (p >= end) return false;
    if (*p == C(',')) { ++p; continue; }
    if (*p == C(']')) { ++p; return true; }
    return false;
  }
  return false;
}

// One depth object: diff (WS) or snapshot (REST), by its keys.
template <class C>
bool parseDepthObject(const C* p, const C* end, const TickScale& sc, DepthDiff* diff, DepthSnapshot* snap) {
  skipWs(p, end);
  if (p >= end || *p != C('{')) return false;
  ++p;
  bool haveU = false, haveu = false, haveB = false, haveA = false, haveId = false;
  while (p < end) {
    skipWs(p, end);
    Key<C> key;
    if (!readString(p, end, key)) return false;
    skipWs(p, end);
    if (p >= end || *p != C(':')) return false;
    ++p;
    skipWs(p, end);
    if (key.is("e")) {
      Key<C> v;
      if (!readString(p, end, v) || !v.is("depthUpdate")) return false;
    } else if (diff && key.is("U")) {
      if (!readInt(p, end, diff->U)) return false;
      haveU = true;
    } else if (diff && key.is("u")) {
      if (!readInt(p, end, diff->u)) return false;
      haveu = true;
    } else if (diff && key.is("pu")) {
      if (!readInt(p, end, diff->pu)) return false;
    } else if (diff && key.is("E")) {
      if (!readInt(p, end, diff->eventMs)) return false;
    } else if (diff && key.is("b")) {
      if (!readLevels(p, end, sc, diff->bids)) return false;
      haveB = true;
    } else if (diff && key.is("a")) {
      if (!readLevels(p, end, sc, diff->asks)) return false;
      haveA = true;
    } else if (snap && key.is("lastUpdateId")) {
      if (!readInt(p, end, snap->lastUpdateId)) return false;
      haveId = true;
    } else if (snap && key.is("bids")) {
      if (!readLevels(p, end, sc, snap->bids)) return false;
      haveB = true;
    } else if (snap && key.is("asks")) {
      if (!readLevels(p, end, sc, snap->asks)) return false;
      haveA = true;
    } else if (!skipValue(p, end)) {
      return false;
    }
    skipWs(p, end);
    if (p >= end) return false;
    if (*p == C(',')) { ++p; continue; }
    if (*p == C('}')) {
      ++p;
      skipWs(p, end);
      if (p != end) return false;
      return diff ? haveU && haveu && haveB && haveA : haveId && haveB && haveA;
    }
    return false;
  }
  return false;
}

template <class C>
bool parseOne(const C* p, const C* end, AggTrade& out) {
  if (!parseObject(p, end, out)) return false;
  skipWs(p, end);
  return p == end;
}

}  // namespace

bool parseAggTrade(QStringView text, AggTrade& out) {
  const char16_t* p = text.utf16();
  return parseOne(p, p + text.size(), out);
}

bool parseAggTrade(QByteArrayView text, AggTrade& out) {
  const char* p = text.data();
  return parseOne(p, p + text.size(), out);
}

bool parseDepthUpdate(QStringView text, const TickScale& scale, DepthDiff& out) {
  out.bids.clear();
  out.asks.clear();
  out.pu = -1;
  out.eventMs = 0;
  const char16_t* p = text.utf16();
  return parseDepthObject(p, p + text.size(), scale, &out, nullptr);
}

bool parseDepthUpdate(QByteArrayView text, const TickScale& scale, DepthDiff& out) {
  out.bids.clear();
  out.asks.clear();
  out.pu = -1;
  out.eventMs = 0;
  const char* p = text.data();
  return parseDepthObject(p, p + text.size(), scale, &out, nullptr);
}

bool parseDepthSnapshot(QByteArrayView body, const TickScale& scale, DepthSnapshot& out) {
  out.bids.clear();
  out.asks.clear();
  out.lastUpdateId = -1;
  const char* p = body.data();
  return parseDepthObject(p, p + body.size(), scale, nullptr, &out);
}

bool parseAggTradesPage(QByteArrayView body, std::vector<AggTrade>& out) {
  const char* p = body.data();
  const char* end = p + body.size();
  skipWs(p, end);
  if (p >= end || *p != '[') return false;
  ++p;
  skipWs(p, end);
  if (p < end && *p == ']') { ++p; skipWs(p, end); return p == end; }
  while (p < end) {
    AggTrade t;
    if (!parseObject(p, end, t)) return false;
    out.push_back(t);
    skipWs(p, end);
    if (p >= end) return false;
    if (*p == ',') { ++p; continue; }
    if (*p == ']') { ++p; skipWs(p, end); return p == end; }
    return false;
  }
  return false;
}

}  // namespace charts::flow
