#pragma once
// The trial registry (step 1 of the roadmap in reports/Architecture agents ML évaluation.md, 2026-11-20; user's go the
// same day):
// every strategy backtested and every ML training, whoever ran it (the user in Charts › Tester or AI › ML, an agent
// through bt_run / bt_quant_checks / ml_train), is written here once it has a result. Trying many things on the same
// data and keeping the best is how luck looks like an edge: the deflated Sharpe and ml_compare's correction count the
// tries from here, so a try counts whoever made it and stays counted when its run is deleted.
//
// <data>/trials/trials.sqlite, table `trials`, append-only BY CONSTRUCTION: SQLite triggers abort any UPDATE or
// DELETE, so no code path (and no later change) can rewrite or forget a try. A row is never changed: what a try showed
// when it was made stays what it showed.
//
//   family   what the tries compete on: "bt|<market>|<timeframe seconds>" (backtests on that market and timeframe, any
//            period), "ml|<market>\x1f<question>" (trainings answering one question on one market, as ml_compare groups
//            them)
//   config   the setup's fingerprint: the same setup tried twice is one try (its latest row is the one that counts)
//   origin   "user" or "agent"; who: "" for the user, "environment 2 · Claude Code" for an agent; place: where it ran
//   returns  backtests: the per-period Sharpe, skew and kurtosis of the per-bar returns (what the deflated Sharpe needs)
//            and the returns compounded per UTC day (float32, compressed), kept for later checks across tries (PBO over
//            every try, the effective number of tries)
//   score    ML: the test score, the no-skill reference and the edge over it
//
// Thread-safe: any thread may add or read (a connection per call, under one lock). Pure QtCore + QtSql.
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cmath>

namespace trials {

struct Trial {
  qint64 id = 0, at = 0;      // at: Unix s
  QString kind;               // "backtest" | "ml"
  QString origin;             // "user" | "agent"
  QString who, place;
  QString family, config, label;
  QString market, timeframe;  // as shown ("BINANCE:BTCUSDT", "1h"; ML: the data set's title, "")
  QString ref;                // ML: the run id (one row per run); empty for backtests
  qint64 from = 0, to = 0;    // the period tested (Unix s)
  int n = 0;                  // returns (backtest) or test rows (ML)
  double sr = NAN, skew = 0, kurt = 3;     // per period
  double sharpe = NAN, netPct = NAN;       // annualised Sharpe, net profit %
  QString scoreKey;                        // ML: balanced_accuracy, mae, test_return, silhouette…
  double score = NAN, reference = NAN, edge = NAN;
  QJsonObject detail;                      // the setup itself (strategy JSON, Pine source's name, ML job)
  qint64 day0 = 0;                         // first UTC day (Unix s / 86400) of `daily`
  QVector<float> daily;                    // returns compounded per UTC day; empty for ML
};

class Registry {
public:
  static Registry& instance();                 // <data>/trials/trials.sqlite
  explicit Registry(const QString& path);      // tests: another file
  QString path() const { return m_path; }
  bool ok() const { return m_ok; }

  // Appends one try; its id, or 0 when it could not be written. A row with a `ref` already recorded is not added again
  // (returns that row's id): an ML run is one try however often it is offered.
  qint64 add(const Trial& t);
  bool hasRef(const QString& ref) const;
  QStringList refs(const QString& kind) const;
  // Every row of a family, oldest first, without the daily returns.
  QList<Trial> family(const QString& family) const;
  // Every family of a kind, with how many distinct setups each has.
  struct Family { QString family, market, timeframe; int configs = 0; qint64 last = 0; };
  QList<Family> families(const QString& kind) const;
  // The distinct setups (configs) of each family of a kind.
  QHash<QString, QSet<QString>> configsByFamily(const QString& kind) const;
  // One row's daily returns (day0 set too); empty when it has none.
  QVector<float> daily(qint64 id, qint64* day0 = nullptr) const;
  qint64 count() const;
  QString meta(const QString& k) const;
  void setMeta(const QString& k, const QString& v);   // meta is the one table that may change (import markers)

private:
  QString m_path;
  bool m_ok = false;
};

// The latest row of each setup of a family, in first-tried order.
QList<Trial> latestPerConfig(const QList<Trial>& rows);

}  // namespace trials
