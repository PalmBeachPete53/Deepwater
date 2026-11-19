#include <QCoreApplication>
#include "pages/market/watch/WatchCalc.h"
#include "market/Instruments.h"
#include "ui/Format.h"
#include <QDate>
#include <QDateTime>
#include <QTimeZone>
#include <QMap>
#include <algorithm>

namespace watch {

// ------------------------------------------------------------------ technicals
static double smaAt(const QVector<market::Bar>& b, int end, int n) {
  if (end + 1 < n) return NAN;
  double s = 0; for (int i = end - n + 1; i <= end; ++i) s += b[i].c;
  return s / n;
}
QVector<double> sma(const QVector<market::Bar>& b, int n) {
  QVector<double> out(b.size(), NAN); double s = 0;
  for (int i = 0; i < b.size(); ++i) { s += b[i].c; if (i >= n) s -= b[i - n].c; if (i >= n - 1) out[i] = s / n; }
  return out;
}
QVector<double> ema(const QVector<market::Bar>& b, int n) {
  QVector<double> out(b.size(), NAN); const double k = 2.0 / (n + 1); double e = NAN;
  for (int i = 0; i < b.size(); ++i) {
    if (i == n - 1) e = smaAt(b, i, n); else if (i >= n) e = b[i].c * k + e * (1 - k);
    if (i >= n - 1) out[i] = e;
  }
  return out;
}
// ------------------------------------------------------------------ performance
QStringList indexPairs(const QString& ccy) {
  QStringList out;
  for (const auto& o : market::majorCurrencies()) {
    if (o == ccy) continue;
    if (market::find(ccy + o)) out << ccy + o;
    else if (market::find(o + ccy)) out << o + ccy;
  }
  return out;
}

static void rebase(PerfLine& l) {
  if (l.pct.isEmpty()) return;
  const double b = l.pct.first();
  for (auto& v : l.pct) v = (std::exp(v - b) - 1.0) * 100.0;
}

PerfLine currencyIndex(const QString& ccy, const QHash<QString, QVector<market::Bar>>& pairs, qint64 from) {
  PerfLine out; out.name = ccy;
  struct P { QMap<qint64, double> byDay; double sign; double last = NAN; };
  QList<P> ps;
  QMap<qint64, int> days;
  for (auto it = pairs.cbegin(); it != pairs.cend(); ++it) {
    const auto* ins = market::find(it.key());
    if (!ins || it.value().isEmpty()) continue;
    P p; p.sign = ins->base == ccy ? 1.0 : -1.0;
    for (const auto& b : it.value()) if (b.t >= from && b.c > 0) { const qint64 d = b.t / 86400; p.byDay[d] = std::log(b.c); days[d] = 1; }
    ps << p;
  }
  if (ps.size() < 3) return out;
  for (auto d = days.cbegin(); d != days.cend(); ++d) {
    double s = 0; int k = 0;
    for (auto& p : ps) {
      auto v = p.byDay.constFind(d.key());
      if (v != p.byDay.cend()) p.last = *v;
      if (std::isfinite(p.last)) { s += p.sign * p.last; ++k; }
    }
    if (k < ps.size()) continue;   // start when every pair has a value
    out.t << d.key() * 86400; out.pct << s / k;
  }
  rebase(out);
  return out;
}

PerfLine rebased(const QString& name, const QVector<market::Bar>& daily, qint64 from) {
  PerfLine l; l.name = name;
  for (const auto& b : daily) if (b.t >= from && b.c > 0) { l.t << (b.t / 86400) * 86400; l.pct << std::log(b.c); }
  rebase(l);
  return l;
}

void alignPair(PerfLine& a, PerfLine& b) {
  if (a.t.isEmpty() || b.t.isEmpty()) return;
  const qint64 start = std::max(a.t.first(), b.t.first());
  auto cut = [start](PerfLine& l) {
    int i = 0; while (i < l.t.size() && l.t[i] < start) ++i;
    if (i == 0) return;
    l.t.remove(0, i); l.pct.remove(0, i);
    if (l.pct.isEmpty()) return;
    const double b0 = 1.0 + l.pct.first() / 100.0;
    for (auto& v : l.pct) v = ((1.0 + v / 100.0) / b0 - 1.0) * 100.0;
  };
  cut(a); cut(b);
}

QVector<market::Bar> aggregate(const QVector<market::Bar>& in, qint64 sec) {
  QVector<market::Bar> out;
  for (const auto& b : in) {
    const qint64 k = b.t - (b.t % sec);
    if (!out.isEmpty() && out.last().t == k) {
      auto& o = out.last(); o.h = std::max(o.h, b.h); o.l = std::min(o.l, b.l); o.c = b.c; o.v += b.v;
    } else { market::Bar n = b; n.t = k; out << n; }
  }
  return out;
}
}
