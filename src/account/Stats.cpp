#include "account/Stats.h"
#include "account/Contracts.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QMap>
#include <QHash>
#include <QLocale>
#include <algorithm>

namespace account {

QString session(const QDateTime& openUtc) {
  const int h = openUtc.toUTC().time().hour();
  if (h >= 7 && h < 12) return QCoreApplication::translate("account", "London");
  if (h >= 12 && h < 16) return QCoreApplication::translate("account", "NY Overlap");
  if (h >= 16 && h < 21) return QCoreApplication::translate("account", "New York");
  return QCoreApplication::translate("account", "Asian");
}

Stats compute(const QList<Trade>& trades, double balance) {
  Stats s;
  s.n = trades.size();
  double cum = 0, peak = 0;
  // drawdown in % needs a starting balance: 0 / unknown (CSV without balance, manual account, MT4 partial
  // history) gives NAN ("—"), never 0 % (A5). It is the largest drop in %, tracked on its own: a small
  // account's early -90 % can be worse than a later, bigger drop in money.
  const bool pctKnown = std::isfinite(balance) && balance > 0;
  s.maxDDPct = pctKnown ? 0 : NAN;
  QMap<int, double> month;          // y*12+m-1
  QMap<int, double> quarter;        // y*4+q-1
  QMap<int, int> monthN, quarterN;
  double wd[7] = {}; int wdN[7] = {};
  QHash<QString, Bucket> sym;
  for (const auto& t : trades) {
    const double v = t.net();
    if (v > 0) { ++s.wins; s.grossProfit += v; }
    else if (v < 0) { ++s.losses; s.grossLoss -= v; }
    else ++s.breakeven;
    cum += v;
    s.cumulative << cum;
    peak = std::max(peak, cum);
    const double dd = cum - peak;
    s.drawdown << dd;
    if (dd < s.maxDD) s.maxDD = dd;
    if (pctKnown) s.maxDDPct = std::min(s.maxDDPct, std::max(-1.0, dd / (balance + peak)));
    // a trade without a usable close date (hand-edited journal.json) counts in the totals, not in the calendars
    // (A2: it indexed weekday -1 and spread the months over 2,000 years)
    const QDate d = t.close.isValid() ? t.close.toLocalTime().date() : QDate();
    if (!d.isValid() || d.year() < 1970 || d.year() > 2200) {
      const QString name = displaySymbol(t.symbol);
      auto& b = sym[name]; b.label = name; b.pnl += v; b.count++;
      continue;
    }
    if (!s.days.isEmpty() && s.days.last().date == d) { s.days.last().pnl += v; s.days.last().cum = cum; }
    else s.days.push_back({d, v, cum});
    const int mk = d.year() * 12 + d.month() - 1, qk = d.year() * 4 + (d.month() - 1) / 3;
    month[mk] += v; monthN[mk]++; quarter[qk] += v; quarterN[qk]++;
    wd[d.dayOfWeek() - 1] += v; wdN[d.dayOfWeek() - 1]++;
    const QString name = displaySymbol(t.symbol);
    auto& b = sym[name]; b.label = name; b.pnl += v; b.count++;
  }
  s.net = cum;
  s.currentDD = s.drawdown.isEmpty() ? 0 : s.drawdown.last();
  if (s.n) s.winRate = double(s.wins) / s.n;
  if (s.grossLoss > 0) s.profitFactor = s.grossProfit / s.grossLoss;
  else if (s.grossProfit > 0) s.profitFactor = INFINITY;
  if (s.wins) s.avgWin = s.grossProfit / s.wins;
  if (s.losses) s.avgLoss = -s.grossLoss / s.losses;
  if (std::isfinite(s.avgWin) && std::isfinite(s.avgLoss) && s.avgLoss < 0) s.winLossRatio = s.avgWin / -s.avgLoss;
  if (s.maxDD < 0) s.recoveryFactor = s.net / -s.maxDD;

  if (!month.isEmpty()) {
    for (int k = month.firstKey(); k <= month.lastKey(); ++k) {
      Bucket b; b.label = i18n::locale().toString(QDate(k / 12, k % 12 + 1, 1), "MMM yy");   // "Apr 26" / "avr. 26"
      b.pnl = month.value(k); b.count = monthN.value(k);
      s.byMonth << b;
      if (b.count) { s.months++; if (b.pnl > 0) s.profitableMonths++; }
    }
    for (int k = quarter.firstKey(); k <= quarter.lastKey(); ++k) {
      Bucket b; b.label = QCoreApplication::translate("account", "Q%1 %2").arg(k % 4 + 1).arg(k / 4); b.pnl = quarter.value(k); b.count = quarterN.value(k);
      s.byQuarter << b;
    }
    s.consistency = s.months ? double(s.profitableMonths) / s.months : NAN;
  }
  for (int i = 0; i < 7; ++i) if (wdN[i]) {
    QString day = i18n::locale().dayName(i + 1, QLocale::LongFormat);     // "Monday" / "lundi" → "Lundi"
    if (!day.isEmpty()) day[0] = day[0].toUpper();
    s.byWeekday << Bucket{day, wd[i], wdN[i]};
  }
  for (const auto& b : sym) s.bySymbol << b;
  std::sort(s.bySymbol.begin(), s.bySymbol.end(), [](const Bucket& a, const Bucket& b) { return a.pnl > b.pnl; });

  // Performance score (Deepwater's own scale, documented in the panel's tooltip).
  auto clamp = [](double v) { return std::isfinite(v) ? std::clamp(v, 0.0, 100.0) : NAN; };
  const double pf = std::isinf(s.profitFactor) ? 3.0 : s.profitFactor;
  s.axes = {
    {QCoreApplication::translate("account", "Win %"), clamp(s.winRate * 100), QCoreApplication::translate("account", "win rate itself")},
    {QCoreApplication::translate("account", "Profit factor"), clamp(pf / 3.0 * 100), QCoreApplication::translate("account", "profit factor ÷ 3 (3 or more = 100)")},
    {QCoreApplication::translate("account", "Avg win/loss"), clamp(s.winLossRatio / 2.5 * 100), QCoreApplication::translate("account", "average win ÷ average loss ÷ 2.5 (2.5 or more = 100)")},
    {QCoreApplication::translate("account", "Recovery factor"), clamp(s.maxDD < 0 ? s.recoveryFactor / 5.0 * 100 : (s.net > 0 ? 100 : NAN)),
     QCoreApplication::translate("account", "net P&L ÷ max drawdown ÷ 5 (5 or more = 100)")},
    {QCoreApplication::translate("account", "Max drawdown"), clamp(s.n && std::isfinite(s.maxDDPct) ? 100.0 * (1.0 + s.maxDDPct * 2.0) : NAN), QCoreApplication::translate("account", "100 at no drawdown, 0 at a 50% drawdown")},
    {QCoreApplication::translate("account", "Consistency"), clamp(s.consistency * 100), QCoreApplication::translate("account", "share of profitable months")},
  };
  double sum = 0; int k = 0;
  for (const auto& a : s.axes) if (std::isfinite(a.score)) { sum += a.score; ++k; }
  if (k == int(s.axes.size())) s.score = sum / k;
  return s;
}
}
