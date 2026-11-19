#include "analytics/Liquidity.h"
#include <QDateTime>
#include <QTimeZone>
#include <QCoreApplication>
#include <algorithm>
#include <climits>

namespace analytics {

static void cluster(const QVector<Bar>& b, QVector<Swing> sw, PoolSide side, const PoolParams& p, QVector<Pool>& out) {
  std::sort(sw.begin(), sw.end(), [](const Swing& a, const Swing& c) {
    return a.price < c.price || (a.price == c.price && a.index < c.index);
  });
  int i = 0;
  while (i < sw.size()) {
    const double p0 = sw[i].price;
    int j = i + 1;
    while (j < sw.size() && (sw[j].price - p0) / p0 * 100.0 <= p.tolPct + 1e-12) ++j;
    if (j - i >= p.minMembers) {
      Pool pl; pl.side = side; pl.members = j - i;
      pl.level = side == PoolSide::BuySide ? sw[j - 1].price : sw[i].price;
      pl.firstMember = INT_MAX; pl.lastMember = -1;
      for (int k = i; k < j; ++k) { pl.firstMember = std::min(pl.firstMember, sw[k].index); pl.lastMember = std::max(pl.lastMember, sw[k].index); }
      out.push_back(pl);
    }
    i = j;
  }
  Q_UNUSED(b);
}

QVector<Pool> findPools(const QVector<Bar>& b, const PoolParams& p) {
  QVector<Pool> pools;
  if (b.size() < 3) return pools;
  const auto sw = swings(b, p.fractalOrder);
  QVector<Swing> hi, lo;
  for (const auto& s : sw) (s.high ? hi : lo).push_back(s);
  cluster(b, hi, PoolSide::BuySide, p, pools);
  cluster(b, lo, PoolSide::SellSide, p, pools);
  const auto a = atr(b, p.atrPeriod);
  for (auto& pl : pools) {
    const bool bsl = pl.side == PoolSide::BuySide;
    for (const auto& x : b) if (x.l <= pl.level && x.h >= pl.level) ++pl.touches;
    for (int k = pl.lastMember + 1; k < b.size(); ++k) {
      const bool beyond = bsl ? b[k].h > pl.level : b[k].l < pl.level;
      if (!beyond) continue;
      const bool back = bsl ? b[k].c < pl.level : b[k].c > pl.level;
      pl.state = back ? PoolState::Swept : PoolState::Broken;
      pl.eventIndex = k;
      break;
    }
    if (pl.state != PoolState::Swept) continue;
    const int k = pl.eventIndex;
    double atrK = finite(a[k]) ? a[k] : NAN;
    if (!finite(atrK)) {   // early in the series: plain mean range of what exists
      double s = 0; int n = 0;
      for (int j = std::max(0, k - p.atrPeriod); j <= k; ++j) { s += b[j].h - b[j].l; ++n; }
      atrK = n ? s / n : NAN;
    }
    double best = bsl ? b[k].l : b[k].h;
    const int end = std::min<int>(b.size() - 1, k + p.reversalBars);
    for (int j = k + 1; j <= end; ++j) best = bsl ? std::min(best, b[j].l) : std::max(best, b[j].h);
    const double move = bsl ? pl.level - best : best - pl.level;
    pl.reversalAtr = atrK > 0 ? move / atrK : NAN;
    const bool complete = end - k >= p.reversalBars;
    if (!finite(pl.reversalAtr)) pl.reversal = Reversal::Pending;
    else if (pl.reversalAtr >= 1.5) pl.reversal = Reversal::Strong;
    else if (!complete) pl.reversal = Reversal::Pending;
    else if (pl.reversalAtr >= 0.75) pl.reversal = Reversal::Moderate;
    else pl.reversal = Reversal::Weak;
  }
  std::sort(pools.begin(), pools.end(), [](const Pool& x, const Pool& y) { return x.level > y.level; });
  return pools;
}

QString reversalName(Reversal r) {
  switch (r) {
    case Reversal::Strong: return QCoreApplication::translate("technical", "Strong");
    case Reversal::Moderate: return QCoreApplication::translate("technical", "Moderate");
    case Reversal::Weak: return QCoreApplication::translate("technical", "Weak");
    case Reversal::Pending: return QCoreApplication::translate("technical", "Pending");
    case Reversal::None: break;
  }
  return {};
}

StopRun stopRun(const QVector<Pool>& pools, double last, double atrAbs) {
  StopRun s;
  if (!finite(last) || last <= 0) return s;
  double best = INFINITY;
  for (int i = 0; i < pools.size(); ++i) {
    const auto& p = pools[i];
    if (p.state != PoolState::Untested) continue;
    const double d = std::abs(p.level - last);
    if (d < best) { best = d; s.poolIndex = i; }
  }
  if (s.poolIndex < 0) return s;
  const auto& p = pools[s.poolIndex];
  s.closer = p.side;
  s.distPct = (p.level - last) / last * 100.0;
  s.distAtr = atrAbs > 0 ? best / atrAbs : NAN;
  if (!finite(s.distAtr)) s.level = StopRunLevel::None;
  else s.level = s.distAtr <= 2 ? StopRunLevel::High : s.distAtr <= 6 ? StopRunLevel::Medium : StopRunLevel::Low;
  return s;
}

QString stopRunName(StopRunLevel l) {
  switch (l) {
    case StopRunLevel::High: return QCoreApplication::translate("technical", "High");
    case StopRunLevel::Medium: return QCoreApplication::translate("technical", "Medium");
    case StopRunLevel::Low: return QCoreApplication::translate("technical", "Low");
    case StopRunLevel::None: break;
  }
  return QStringLiteral("—");
}

SessionTable sessionExtremes(const QVector<Bar>& b, const QDate& todayUtc) {
  SessionTable t;
  auto dt = [](qint64 s) { return QDateTime::fromSecsSinceEpoch(s, QTimeZone::UTC); };
  if (b.isEmpty()) return t;
  const QDate day = dt(b.last().t).date();
  t.day = day;
  const QString sub = day == todayUtc ? QCoreApplication::translate("technical", "today · UTC") : QCoreApplication::translate("technical", "%1 · UTC").arg(day.toString("yyyy-MM-dd"));
  // local business hours of each centre, daylight saving through the tz database (as global::sessions())
  struct W { const char* name; const char* tz; int open, close; };
  const W wins[] = {{QT_TRANSLATE_NOOP("technical", "Sydney"), "Australia/Sydney", 7, 16},
                    {QT_TRANSLATE_NOOP("technical", "Asian (Tokyo)"), "Asia/Tokyo", 9, 18},
                    {QT_TRANSLATE_NOOP("technical", "London"), "Europe/London", 8, 17},
                    {QT_TRANSLATE_NOOP("technical", "New York"), "America/New_York", 8, 17}};
  const qint64 lastT = b.last().t;
  for (const auto& w : wins) {
    SessionRow r; r.name = QCoreApplication::translate("technical", w.name); r.sub = sub;
    const QTimeZone tz(w.tz);
    // Tokyo, London, New York: the session of the latest bar's UTC day. Sydney opens the evening before in UTC: the
    // session that opened most recently (by the latest bar), so the row isn't empty between 06:00 and 21:00 UTC.
    QDate local = day;
    if (qstrcmp(w.tz, "Australia/Sydney") == 0)
      for (QDate d = day.addDays(1); d >= day.addDays(-1); d = d.addDays(-1))
        if (QDateTime(d, QTime(w.open, 0), tz).toSecsSinceEpoch() <= lastT) { local = d; break; }
    const qint64 a = QDateTime(local, QTime(w.open, 0), tz).toSecsSinceEpoch(), e = QDateTime(local, QTime(w.close, 0), tz).toSecsSinceEpoch();
    for (const auto& x : b) if (x.t >= a && x.t < e) {
      r.high = finite(r.high) ? std::max(r.high, x.h) : x.h;
      r.low = finite(r.low) ? std::min(r.low, x.l) : x.l;
    }
    t.rows << r;
  }
  // previous day: last UTC day before `day` with bars
  QDate prev;
  for (int i = b.size() - 1; i >= 0; --i) { const QDate d = dt(b[i].t).date(); if (d < day) { prev = d; break; } }
  SessionRow pd; pd.name = QCoreApplication::translate("technical", "Previous day"); pd.summary = true;
  if (prev.isValid()) {
    pd.sub = prev.toString("yyyy-MM-dd");
    for (const auto& x : b) if (dt(x.t).date() == prev) {
      pd.high = finite(pd.high) ? std::max(pd.high, x.h) : x.h;
      pd.low = finite(pd.low) ? std::min(pd.low, x.l) : x.l;
    }
  } else pd.sub = QStringLiteral("—");
  t.prevDay = prev; t.pdh = pd.high; t.pdl = pd.low;
  t.rows << pd;
  SessionRow pw; pw.name = QCoreApplication::translate("technical", "Previous week"); pw.sub = QCoreApplication::translate("technical", "high / low"); pw.summary = true;
  const QDate monday = day.addDays(1 - day.dayOfWeek());
  const qint64 w1 = QDateTime(monday, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(), w0 = w1 - 7 * 86400;
  for (const auto& x : b) if (x.t >= w0 && x.t < w1) {
    pw.high = finite(pw.high) ? std::max(pw.high, x.h) : x.h;
    pw.low = finite(pw.low) ? std::min(pw.low, x.l) : x.l;
  }
  t.rows << pw;
  return t;
}

LiquidityRead liquidityRead(const QVector<Bar>& d, int recentBars) {
  LiquidityRead r;
  if (d.size() < 30) return r;
  const auto a = atr(d, 14);
  const double last = d.last().c;
  const double atrPct = finite(a.last()) && last > 0 ? a.last() / last * 100.0 : NAN;
  if (!finite(atrPct)) return r;
  PoolParams p; p.tolPct = std::max(0.06, 0.25 * atrPct); p.reversalBars = 3;
  const auto pools = findPools(d, p);
  r.total = pools.size();
  int recent = -1;
  for (int i = 0; i < pools.size(); ++i) {
    const auto& pl = pools[i];
    if (pl.state == PoolState::Untested) ++r.untested;
    if (pl.state == PoolState::Swept && pl.eventIndex >= d.size() - recentBars &&
        (recent < 0 || pl.eventIndex > pools[recent].eventIndex)) recent = i;
  }
  auto closeness = [&](double dist) { return 100.0 * std::exp(-std::abs(dist) / (2.0 * atrPct)); };
  if (recent >= 0) {
    r.state = LiqRead::RecentlySwept;
    r.distPct = (pools[recent].level - last) / last * 100.0;
    r.closeness = closeness(r.distPct);
    return r;
  }
  int best = -1;
  for (int i = 0; i < pools.size(); ++i)
    if (pools[i].state == PoolState::Untested &&
        (best < 0 || std::abs(pools[i].level - last) < std::abs(pools[best].level - last))) best = i;
  if (best < 0) return r;
  r.distPct = (pools[best].level - last) / last * 100.0;
  r.closeness = closeness(r.distPct);
  r.state = std::abs(r.distPct) <= atrPct ? LiqRead::NearPool : r.distPct > 0 ? LiqRead::PoolAbove : LiqRead::PoolBelow;
  return r;
}
}
