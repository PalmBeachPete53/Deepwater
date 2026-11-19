#include "seasonality/Seasonality.h"
#include "market/Candles.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>
#include <QSet>
#include <algorithm>
#include <numeric>

namespace seasonality {

// ------------------------------------------------------------------ calendar
int slotOf(int month, int day) { return QDate(2000, month, day).dayOfYear() - 1; }
int slotOf(const QDate& d) { return d.isValid() ? slotOf(d.month(), d.day()) : -1; }
QDate dateOfSlot(int slot, int year) {
  const QDate t = QDate(2000, 1, 1).addDays(qBound(0, slot, kSlots - 1));
  if (t.month() == 2 && t.day() == 29 && !QDate::isLeapYear(year)) return QDate(year, 2, 28);
  return QDate(year, t.month(), t.day());
}
QString slotLabel(int slot) {
  const QDate t = QDate(2000, 1, 1).addDays(qBound(0, slot, kSlots - 1));
  // "Sep 13" in English, "13 sept." in French
  return i18n::code() == "fr" ? QString("%1 %2").arg(t.day()).arg(monthShort(t.month()))
                               : QString("%1 %2").arg(monthShort(t.month())).arg(t.day());
}
int monthFirstSlot(int m) { return slotOf(m, 1); }
int monthLastSlot(int m) { return slotOf(m, QDate(2000, m, 1).daysInMonth()); }
int monthOfSlot(int slot) { return QDate(2000, 1, 1).addDays(qBound(0, slot, kSlots - 1)).month(); }
QString monthShort(int m) {
  return (m >= 1 && m <= 12) ? i18n::locale().monthName(m, QLocale::ShortFormat) : QString();   // "Jan" / "janv."
}
QString monthAxis(int m) { QString s = monthShort(m).toUpper(); if (s.endsWith('.')) s.chop(1); return s; }
QString weekdayAxis(int d) {
  QString s = i18n::locale().dayName(d, QLocale::ShortFormat).toUpper();
  if (s.endsWith('.')) s.chop(1);
  return s;
}
QString dateLabel(const QDate& d) { return i18n::locale().toString(d, i18n::code() == "fr" ? "d MMM yyyy" : "MMM d yyyy"); }
QString monthLong(int m) {
  return (m >= 1 && m <= 12) ? i18n::locale().monthName(m, QLocale::LongFormat) : QString();
}

// ------------------------------------------------------------------ period
Period Period::preset(Kind k) {
  Period p; p.kind = k;
  auto set = [&](int m0, int m1) { p.a = monthFirstSlot(m0); p.b = monthLastSlot(m1); };
  switch (k) {
    case H1: set(1, 6); break;
    case H2: set(7, 12); break;
    case Q1: set(1, 3); break;
    case Q2: set(4, 6); break;
    case Q3: set(7, 9); break;
    case Q4: set(10, 12); break;
    default: p.kind = Full; p.a = 0; p.b = kSlots - 1; break;
  }
  return p;
}
Period Period::custom(int a, int b) {
  Period p; p.kind = Custom;
  p.a = qBound(0, qMin(a, b), kSlots - 1); p.b = qBound(0, qMax(a, b), kSlots - 1);
  if (p.isFull()) p.kind = Full;
  return p;
}
QList<int> Period::months() const {
  QList<int> m;
  for (int i = monthOfSlot(a); i <= monthOfSlot(b); ++i) m << i;
  return m;
}
QString Period::pill() const {
  if (isFull()) return QCoreApplication::translate("seasonal", "Full year");
  const int m0 = monthOfSlot(a), m1 = monthOfSlot(b);
  if (a == monthFirstSlot(m0) && b == monthLastSlot(m1))
    return m0 == m1 ? monthShort(m0) : monthShort(m0) + "-" + monthShort(m1);
  return slotLabel(a) + "-" + slotLabel(b);
}
QString Period::rangeLabel() const { return slotLabel(a) + QString::fromUtf8(" ⟶ ") + slotLabel(b); }

// ------------------------------------------------------------------ dataset
int Dataset::index(int year) const {
  for (int i = 0; i < years.size(); ++i) if (years[i].year == year) return i;
  return -1;
}

Dataset build(const QVector<DailyClose>& in, int nYears, const QDate& today, const QString& symbol) {
  Dataset ds; ds.symbol = symbol; ds.yearsRequested = nYears; ds.today = today;
  QVector<DailyClose> c;
  c.reserve(in.size());
  for (const auto& x : in) if (x.date.isValid() && std::isfinite(x.close) && x.close > 0) c << x;
  std::sort(c.begin(), c.end(), [](const DailyClose& l, const DailyClose& r) { return l.date < r.date; });
  // one close per date (keep the last)
  QVector<DailyClose> u;
  for (const auto& x : c) { if (!u.isEmpty() && u.last().date == x.date) u.last() = x; else u << x; }
  if (u.isEmpty() || nYears <= 0) return ds;
  ds.last = u.last().date;
  const int lastYear = qMin(today.year(), ds.last.year());

  for (int y = lastYear - nYears + 1; y <= lastYear; ++y) {
    const QDate jan1(y, 1, 1), dec31(y, 12, 31);
    // base: last close in the 10 days before Jan 1, else the first close if it is within the first 10 days
    auto it = std::lower_bound(u.begin(), u.end(), jan1, [](const DailyClose& x, const QDate& d) { return x.date < d; });
    double base = NAN; QDate baseDate;
    if (it != u.begin() && (it - 1)->date.daysTo(jan1) <= 10) { base = (it - 1)->close; baseDate = (it - 1)->date; }
    else if (it != u.end() && it->date.year() == y && jan1.daysTo(it->date) <= 10) { base = it->close; baseDate = it->date; }
    if (!std::isfinite(base)) continue;
    YearData yd; yd.year = y; yd.base = base; yd.close = QVector<double>(kSlots, base);
    int lastSlot = -1; double cur = base;
    QVector<double> real(kSlots, NAN);
    for (auto j = it; j != u.end() && j->date <= dec31; ++j) { real[slotOf(j->date)] = j->close; lastSlot = slotOf(j->date); }
    if (lastSlot < 0) continue;
    for (int s = 0; s < kSlots; ++s) { if (std::isfinite(real[s])) cur = real[s]; yd.close[s] = cur; }
    yd.finished = ds.last > dec31 || (ds.last.year() == y && ds.last.month() == 12 && ds.last.day() >= 24 && today > dec31);
    yd.reach = yd.finished ? kSlots - 1 : lastSlot;
    if (!ds.first.isValid()) ds.first = baseDate;
    ds.years << yd;
  }
  // daily returns between consecutive closes inside the used years
  if (!ds.years.isEmpty()) {
    const int y0 = ds.years.first().year;
    QSet<int> used; for (const auto& y : ds.years) used.insert(y.year);
    for (int i = 1; i < u.size(); ++i) {
      const QDate& d = u[i].date;
      if (d.year() < y0 || !used.contains(d.year())) continue;
      if (u[i - 1].date.daysTo(d) > 7) continue;   // gap in the data: no daily return
      ds.daily.push_back({d, slotOf(d), d.year(), (u[i].close / u[i - 1].close - 1.0) * 100.0});
    }
  }
  return ds;
}

// ------------------------------------------------------------------ paths
QVector<double> yearPath(const Dataset& ds, int year) {
  QVector<double> p(kSlots, NAN);
  const int i = ds.index(year);
  if (i < 0) return p;
  const auto& y = ds.years[i];
  for (int s = 0; s <= y.reach; ++s) p[s] = (y.close[s] / y.base - 1.0) * 100.0;
  return p;
}

QVector<double> averagePath(const Dataset& ds) {
  QVector<double> sum(kSlots, 0.0); QVector<int> n(kSlots, 0);
  for (const auto& y : ds.years) {
    if (!y.finished) continue;   // the unfinished current year is its own overlay (yearPath), never in the average
    for (int s = 0; s <= y.reach; ++s) { sum[s] += (y.close[s] / y.base - 1.0) * 100.0; ++n[s]; }
  }
  QVector<double> p(kSlots, NAN);
  for (int s = 0; s < kSlots; ++s) if (n[s] > 0) p[s] = sum[s] / n[s];
  return p;
}

QVector<double> smoothPath(const QVector<double>& path, double sigma, int hw) {
  QVector<double> out(path.size(), NAN);
  for (int s = 0; s < path.size(); ++s) {
    if (!std::isfinite(path[s])) continue;
    double w = 0, acc = 0;
    for (int k = -hw; k <= hw; ++k) {
      const int j = s + k;
      if (j < 0 || j >= path.size() || !std::isfinite(path[j])) continue;
      const double g = std::exp(-0.5 * (k * k) / (sigma * sigma));
      w += g; acc += g * path[j];
    }
    out[s] = acc / w;
  }
  return out;
}

double pathChange(const QVector<double>& p, int a, int b) {
  if (b >= p.size() || a < 0 || !std::isfinite(p[b])) return NAN;
  const double start = a == 0 ? 0.0 : p[a - 1];
  if (!std::isfinite(start)) return NAN;
  return ((1.0 + p[b] / 100.0) / (1.0 + start / 100.0) - 1.0) * 100.0;
}

double periodReturn(const YearData& y, int a, int b, bool* counted) {
  if (counted) *counted = false;
  if (a > y.reach) return NAN;
  const int e = qMin(b, y.reach);
  const double c0 = a == 0 ? y.base : y.close[a - 1];
  const double r = (y.close[e] / c0 - 1.0) * 100.0;
  if (counted) *counted = b <= y.reach;
  return r;
}

// ------------------------------------------------------------------ statistics
static double mean(const QVector<double>& v) {
  if (v.isEmpty()) return NAN;
  return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}
static double stdev(const QVector<double>& v) {   // sample standard deviation
  if (v.size() < 2) return NAN;
  const double m = mean(v);
  double s = 0; for (double x : v) s += (x - m) * (x - m);
  return std::sqrt(s / (v.size() - 1));
}
static double median(QVector<double> v) {
  if (v.isEmpty()) return NAN;
  std::sort(v.begin(), v.end());
  const int n = v.size();
  return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

static MonthStat monthStat(const Dataset& ds, int m, bool excludeOutliers) {
  QVector<double> r;
  for (const auto& y : ds.years) {
    bool counted = false;
    const double x = periodReturn(y, monthFirstSlot(m), monthLastSlot(m), &counted);
    if (counted && std::isfinite(x)) r << x;
  }
  if (excludeOutliers && r.size() >= 4) {
    const double mu = mean(r), sd = stdev(r);
    if (std::isfinite(sd) && sd > 0) {
      QVector<double> k;
      for (double x : r) if (std::abs(x - mu) <= 1.5 * sd) k << x;
      r = k;
    }
  }
  return {m, mean(r), int(r.size())};
}

QVector<MonthStat> monthlyAverages(const Dataset& ds, bool excludeOutliers) {
  QVector<MonthStat> out;
  for (int m = 1; m <= 12; ++m) out << monthStat(ds, m, excludeOutliers);
  return out;
}

int volState(double r) {
  if (!std::isfinite(r)) return 0;
  return r < 0.95 ? -1 : r > 1.05 ? 1 : 0;
}

Stats compute(const Dataset& ds, const QVector<double>& raw, const Period& p, bool excludeOutliers) {
  Stats st; st.period = p;
  if (!ds.ok()) return st;
  QVector<double> rets;
  for (const auto& y : ds.years) {
    YearReturn yr; yr.year = y.year;
    yr.ret = periodReturn(y, p.a, p.b, &yr.counted);
    st.perYear << yr;
    if (yr.counted && std::isfinite(yr.ret)) {
      rets << yr.ret;
      if (yr.ret > 0) ++st.positive; else ++st.negative;
      if (!std::isfinite(st.bestYear.ret) || yr.ret > st.bestYear.ret) st.bestYear = yr;
      if (!std::isfinite(st.worstYear.ret) || yr.ret < st.worstYear.ret) st.worstYear = yr;
    }
  }
  st.yearsCounted = rets.size();
  if (st.yearsCounted > 0) st.winRate = double(st.positive) / st.yearsCounted;
  st.meanReturn = mean(rets); st.medianReturn = median(rets); st.stdReturn = stdev(rets);
  if (!rets.isEmpty()) {
    double w = 1.0; for (double r : rets) w *= 1.0 + r / 100.0;
    st.compounded = (w - 1.0) * 100.0;
  }
  if (std::isfinite(st.stdReturn) && st.stdReturn > 0) st.returnRisk = st.meanReturn / st.stdReturn;
  st.pathReturn = pathChange(raw, p.a, p.b);

  // months of the period
  for (int m : p.months()) {
    const MonthStat ms = monthStat(ds, m, excludeOutliers);
    st.months << ms;
    const MonthStat plain = excludeOutliers ? monthStat(ds, m, false) : ms;   // best/worst ignore the filter
    if (!std::isfinite(plain.mean)) continue;
    if (!std::isfinite(st.best.mean) || plain.mean > st.best.mean) st.best = plain;
    if (!std::isfinite(st.worst.mean) || plain.mean < st.worst.mean) st.worst = plain;
  }

  // daily returns: weekdays and volatility
  QVector<double> all, inP, wd[5];
  for (const auto& d : ds.daily) {
    all << d.r;
    if (d.slot < p.a || d.slot > p.b) continue;
    inP << d.r;
    const int w = d.date.dayOfWeek();
    if (w >= 1 && w <= 5) wd[w - 1] << d.r;
  }
  for (int i = 0; i < 5; ++i) { st.weekday[i] = mean(wd[i]); st.weekdayN[i] = wd[i].size(); }
  st.sigmaGlobal = stdev(all); st.sigmaPeriod = stdev(inP);
  if (std::isfinite(st.sigmaGlobal) && st.sigmaGlobal > 0) st.volRatio = st.sigmaPeriod / st.sigmaGlobal;

  // max drawdown on the raw average path inside the period (wealth 1 + p, starting at the close before it)
  const double w0 = p.a == 0 ? 1.0 : (std::isfinite(raw.value(p.a - 1, NAN)) ? 1.0 + raw[p.a - 1] / 100.0 : NAN);
  if (std::isfinite(w0)) {
    double peak = w0, dd = 0; bool any = false;
    for (int s = p.a; s <= p.b && s < raw.size(); ++s) {
      if (!std::isfinite(raw[s])) continue;
      const double w = 1.0 + raw[s] / 100.0;
      peak = qMax(peak, w); dd = qMin(dd, (w / peak - 1.0) * 100.0); any = true;
    }
    if (any) st.maxDrawdown = dd;   // no average path in the period (no finished year): unknown, not 0
  }
  return st;
}

// ------------------------------------------------------------------ history
void loadHistory(const QString& symbol, int years, QObject* ctx, HistoryCallback cb) {
  const QDate today = QDate::currentDate();
  const QDate from(today.year() - years, 12, 1);   // December before the first year: base close
  const qint64 t0 = QDateTime(from, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  const qint64 t1 = QDateTime::currentSecsSinceEpoch() + 86400;
  market::candlesBetween(symbol, "1d", t0, t1, ctx, [cb](const market::Series& s) {
    QVector<DailyClose> out;
    out.reserve(s.bars.size());
    for (const auto& b : s.bars) out.push_back({market::sessionDay(s, b.t), b.c});
    cb(out, s.ok() ? QString() : (s.error.isEmpty() ? QCoreApplication::translate("seasonal", "no data") : s.error), s.stale);
  });
}
}
