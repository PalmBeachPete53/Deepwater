#pragma once
// The trial registry's figures (src/trials/Registry.h): what a backtest writes there, and what is read back from every
// try on the same data (the deflated Sharpe of the best, who tried what). Pure computing on charts::quant.
#include "charts/core/Backtest.h"
#include "charts/core/Quant.h"
#include "trials/Registry.h"
#include <optional>

namespace trials {

QString backtestFamily(const QString& market, int tfSeconds);   // "bt|BINANCE:BTCUSDT|3600"
QString fingerprint(const QByteArray& what);                     // 12 hex digits of SHA-1
QString strategyConfig(const QJsonObject& strategy);            // a rule strategy's setup (its name left out)
QString pineConfig(const QString& source);
QString ruleLabel(const QJsonObject& strategy);                 // "rules: entryLong: … | stopLossPct 2" (English)

// sr, skew, kurt, n, sharpe (annualised from the bars' spacing), from / to and the daily returns, from an equity curve.
void setReturns(Trial& t, const QVector<charts::bt::EquityPoint>& equity);

// The best of these tries (latest row of each setup, see latestPerConfig) and its deflated Sharpe (Bailey & López de
// Prado): the probability its Sharpe beats the best that pure noise would show after that many tries.
struct Deflated {
  bool ok = false;      // at least one try
  int tries = 0;
  Trial best;
  double sr0 = 0;       // the best Sharpe noise would show (per period)
  double dsr = NAN;     // NaN when the best has too few returns
};
Deflated deflate(const QList<Trial>& latest);
double psrOf(double sr, double n, double skew, double kurt, double benchmark);

// Distinct setups of these rows tried by the user, by agent `me` (a who prefix: "environment 2 · ", whatever model the
// environment ran; empty = none) and by other agents.
// A setup tried by several counts for each of them.
struct Tried { int all = 0, user = 0, me = 0, others = 0; };
Tried tried(const QList<Trial>& rows, const QString& me);

// The quant checks of one strategy (Charts › Tester, bt_quant_checks) with every other strategy tried on the same data
// (step 2 of the roadmap, 2026-11-20):
//  - the deflated Sharpe counts the other strategies' tries besides the strategy's own nearby settings (its gate text
//    says how many of each);
//  - a gate "Overfitting across tries (PBO)": the best of every strategy tried on this data, picked on half of the
//    history, against the others on the other half (CSCV on the returns per UTC day over this run's days; strategies
//    tried on at least 75 % of those days, a day it was not run counting as flat; needs 3 strategies and 40 days).
// `config` is this strategy's setup (left out of the others). Any thread (the registry is thread-safe).
void withRegistry(charts::quant::Checks& c, const QVector<charts::bt::EquityPoint>& baseEquity, const QString& family,
                  const QString& config, const Registry& reg = Registry::instance(), qint64 asOf = 0);

// Evaluation mode (step 3, 2026-11-20): the tries an agent may see at moment asOf, those whose tested period had ended
// by then (a backtest family's last bar closed: to + its timeframe <= asOf). A try over later data would tell how a
// strategy did after asOf. Tries without a period (the agents' ledgers before the registry) are left out. asOf 0: all.
QList<Trial> visibleAt(const QList<Trial>& rows, qint64 asOf);
int familySeconds(const QString& family);   // "bt|…|3600" → 3600; 0 for other families

// PBO across tries: `self` (this run's daily returns from day0) and the others' rows (with their daily returns) →
// the PBO and how many strategies it compared; nullopt when there are too few strategies or days.
struct TriesPbo { charts::quant::Pbo pbo; int strategies = 0, days = 0; };
std::optional<TriesPbo> pboAcross(qint64 day0, const QVector<float>& self, const QList<QPair<qint64, QVector<float>>>& others);

}  // namespace trials
