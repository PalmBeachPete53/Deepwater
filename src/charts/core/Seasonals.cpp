// Port of quant-terminal public/lib/seasonals.js (MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Seasonals: how a symbol moved through each calendar year, from daily bars (UTC dates).
// Each year's path is measured from the previous year's last close, so 1 January's move counts.
#include "charts/core/Seasonals.h"
#include "charts/core/Series.h"
#include <QDateTime>
#include <QJsonArray>
#include <QMap>

// Exactness with the JS: no fused multiply-add (clang contracts a*b+c by default on arm64).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#else
#pragma STDC FP_CONTRACT OFF
#endif

namespace charts::seasonals {

namespace {

int dayOfYear(qint64 t) {
  int y, m, d;
  series::utcDate(t, y, m, d);
  return int((series::utcSeconds(y, m, d) - series::utcSeconds(y, 0, 1)) / 86400) + 1;
}
int yearOf(qint64 t) { int y, m, d; series::utcDate(t, y, m, d); return y; }
int monthOf(qint64 t) { int y, m, d; series::utcDate(t, y, m, d); return m; }

QJsonValue jnum(double v) { return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null); }

}  // namespace

Result seasonals(const Bars& bars, int thisYear) {
  QMap<int, Bars> byYear;   // sorted by year, as [...byYear.keys()].sort((a, b) => a - b)
  for (const Bar& b : bars) byYear[yearOf(b.time)].push_back(b);
  const QList<int> yearsList = byYear.keys();
  if (thisYear == 0) thisYear = QDateTime::currentDateTimeUtc().date().year();
  Result res;
  for (int k = 0; k < yearsList.size(); ++k) {
    const int y = yearsList[k];
    const Bars& rows = byYear[y];
    const bool hasPrev = k > 0 && yearsList[k - 1] == y - 1;
    const double prev = hasPrev ? byYear[y - 1].last().close : NaN;
    const double base = hasPrev ? prev : rows[0].open;
    Year yr;
    yr.year = y;
    for (const Bar& b : rows) yr.points.push_back({dayOfYear(b.time), (b.close / base - 1) * 100});
    // complete = we hold the whole year (starts within the first week, and it's not the current year)
    yr.complete = y != thisYear && hasPrev && dayOfYear(rows.last().time) > 350;
    yr.total = yr.points.last().pct;
    res.years.push_back(yr);
    MonthRow mr;
    mr.year = y;
    mr.months.fill(NaN);
    double last = base;
    for (int m = 0; m < 12; ++m) {
      double close = NaN;
      bool any = false;
      for (const Bar& b : rows)
        if (monthOf(b.time) == m) { close = b.close; any = true; }
      if (!any) continue;
      mr.months[m] = (close / last - 1) * 100;
      last = close;
    }
    mr.total = yr.total;
    res.monthly.push_back(mr);
  }
  // Average path over complete years: each year sampled per day of year, carrying the last value
  // over weekends and holidays.
  QVector<const Year*> done;
  for (const Year& y : res.years)
    if (y.complete) done.push_back(&y);
  if (!done.isEmpty()) {
    QVector<std::array<double, 367>> sampled;
    for (const Year* y : done) {
      std::array<double, 367> out;
      out.fill(NaN);
      int i = 0;
      double cur = 0;
      for (int d = 1; d <= 366; ++d) {
        while (i < y->points.size() && y->points[i].doy <= d) cur = y->points[i++].pct;
        out[size_t(d)] = cur;
      }
      sampled.push_back(out);
    }
    for (int d = 1; d <= 366; ++d) {
      double s = 0;
      for (const auto& x : sampled) s += x[size_t(d)];
      res.average.push_back({d, s / sampled.size()});
    }
  }
  for (int m = 0; m < 12; ++m) {
    double sum = 0;
    int n = 0, up = 0;
    for (const MonthRow& r : res.monthly) {
      const double v = r.months[size_t(m)];
      if (std::isnan(v)) continue;
      sum += v; ++n;
      if (v > 0) ++up;
    }
    MonthAvg a;
    if (n) { a.valid = true; a.avg = sum / n; a.upPct = double(up) / n * 100; a.n = n; }
    res.monthAvg[size_t(m)] = a;
  }
  res.completeYears = done.size();
  return res;
}

QJsonObject Result::toJson() const {
  auto pts = [](const QVector<Point>& p) {
    QJsonArray a;
    for (const Point& x : p) a.append(QJsonObject{{"doy", x.doy}, {"pct", jnum(x.pct)}});
    return a;
  };
  QJsonArray ys, ms, avg;
  for (const Year& y : years)
    ys.append(QJsonObject{{"year", y.year}, {"points", pts(y.points)}, {"total", jnum(y.total)}, {"complete", y.complete}});
  for (const MonthRow& r : monthly) {
    QJsonArray m;
    for (double v : r.months) m.append(jnum(v));
    ms.append(QJsonObject{{"year", r.year}, {"months", m}, {"total", jnum(r.total)}});
  }
  QJsonArray ma;
  for (const MonthAvg& a : monthAvg)
    ma.append(a.valid ? QJsonValue(QJsonObject{{"avg", jnum(a.avg)}, {"upPct", jnum(a.upPct)}, {"n", a.n}})
                      : QJsonValue(QJsonValue::Null));
  return {{"years", ys}, {"monthly", ms}, {"average", pts(average)}, {"monthAvg", ma}, {"completeYears", completeYears}};
}

}  // namespace charts::seasonals
