#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVector>
#include <QList>
#include <QHash>
#include <QSqlDatabase>
#include <cmath>
#include <functional>

class QThread;

// Facts: every economic fact Deepwater knows — a value of one indicator at one moment, real or forecast — kept in a
// SQLite database (<data>/macro-data/macro-data.sqlite) and linked into chains.
//
// Chain: the facts of ONE indicator of ONE kind, from ONE origin, e.g. "ECB policy rate (actual)", "US CPI y/y
// (ForexFactory consensus)", "EUR/USD daily close", "ING forecast of EUR/USD". A forecast never shares a chain with the
// actual values it predicts (different kind), it points at them through its target date and the chain's targetChain.
// Each fact links to the previous fact of its chain (prev), in the order of its period (then target, then recording).
//
// Revisions (user's choice): a new value for a period that already has a fact is a NEW fact in the same chain, marked
// `revises` = the fact it corrects; nothing is overwritten, so what was known at each date stays known (recorded time).
// Nothing is ever refused (user's rule, 2026-11-16): a value going back to one the period already had is a revision too.
// A chain holds ONE source: another source's figure for the same period is in that source's chain, never a revision.
// Chains in "changes" mode (policy rates) keep a fact only when the value differs from the previous one.
// Facts imported from history carry origin "import" (their value is the source's value at import time, revisions
// included); facts seen as they appear carry "live". No hashes: plain links (user's choice).
//
// Writes go through a worker thread (big imports never block the window); reads use a read-only connection on the
// GUI thread (SQLite WAL lets both work at once).
namespace macrodata {

struct ChainDef {
  QString key;          // "rate:us", "macro:fr:inflation", "px:EURUSD:close", "fc:ing:EURUSD", "cal:USD|CPI y/y:consensus"
  QString family;       // rates | macro | calendar | prices | forecasts | positioning
  QString subject;      // "Federal Reserve", "France", "EUR/USD", "USD"
  QString indicator;    // "Policy rate", "Inflation (CPI y/y)", "Close"
  QString kind;         // actual | forecast | consensus | implied | price
  QString source;       // "BIS WS_CBPOL", "Yahoo Finance", "ForexFactory"
  QString unit;         // "%", "index", "USD", "contracts", ""
  QString freq;         // D W M Q A E (event) …
  QString mode = "levels";   // levels: one fact per period | changes: a fact only when the value changes
  QString targetChain;  // forecasts: the chain of the actual values they predict
};

struct Obs {          // an observation handed to the store; becomes a fact unless it repeats a known one
  QString chain;
  QDateTime period;     // start of the period the value describes (UTC); for a forecast: when it was made
  QDateTime target;     // forecasts: the period they predict (invalid otherwise)
  QDateTime at;         // when the value was published / observed, if known
  double value = NAN;
  QString text;         // non-numeric facts (decisions) or a short qualifier
  QString note;
};

struct Fact {
  qint64 id = 0, chain = 0, prev = 0, revises = 0;
  qint64 period = 0, target = 0, at = 0, recorded = 0;   // ms since epoch, UTC (0 = none)
  double value = NAN;
  QString text, origin, note;
};

// One month of one chain, for the 3D views: the last value of the month (forecasts: the latest forecast for that
// month) and its rank among all the chain's months, 0 = lowest ever, 1 = highest (units disappear: a rate, a price
// and a COT position compare). Kept in facts_monthly by the writer.
struct MonthCell {
  int month = 0;        // year * 12 + (month - 1)
  float value = NAN;
  float rank = 0.5f;
  int count = 0;        // facts in the month
  int revisions = 0;    // of which revisions
};
using MonthMap = QHash<qint64, QVector<MonthCell>>;   // chain id → months, ascending
inline int monthIndex(const QDate& d) { return d.year() * 12 + d.month() - 1; }
inline QDate monthDate(int m) { return QDate(m / 12, m % 12 + 1, 1); }

struct Chain {
  qint64 id = 0;
  ChainDef def;
  int count = 0, revisions = 0;
  qint64 first = 0, last = 0;   // period range
};

class Store : public QObject {
  Q_OBJECT
public:
  static Store& instance();
  QString path() const;

  // asynchronous write; done(added, revised) on the GUI thread
  void add(const QList<ChainDef>& chains, const QList<Obs>& obs, std::function<void(int, int)> done = {});
  bool busy() const { return m_pending > 0; }

  // synchronous reads (GUI thread)
  QList<Chain> chains() const;
  Chain chain(const QString& key) const;
  QVector<Fact> facts(qint64 chainId) const;           // period order
  Fact fact(qint64 id) const;
  QString meta(const QString& k) const;
  void setMeta(const QString& k, const QString& v);    // through the writer
  // Once (meta repair.bisfeed): takes the central-bank feed values written before 2026-11-16 out of the BIS rate chains.
  // keep: chain key → raw BIS series (period ms → value). Queued before the next add(), on the writer thread.
  void repairBisChains(const QHash<QString, QHash<qint64, double>>& keep);
  qint64 totalFacts() const;
  // monthly cells of every chain, read on a worker thread; done(map) on the GUI thread
  void loadMonthly(QObject* ctx, std::function<void(const MonthMap&)> done) const;

signals:
  void changed(const QStringList& chainKeys);           // after a write that added facts
  void monthlyReady();                                  // the monthly cells were (re)built for every chain

private:
  Store();
  QSqlDatabase reader() const;
  class Writer* m_writer = nullptr;
  QThread* m_thread = nullptr;
  int m_pending = 0;
};

QDateTime fromMs(qint64 ms);   // UTC; 0 is 1970-01-01 (callers test target / at == 0 for "none" first)
}  // namespace macrodata
