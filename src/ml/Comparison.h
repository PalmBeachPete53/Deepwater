#pragma once
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <cmath>

// Comparing finished trainings (user's request, 2026-11-19): the same report for the agents (ml_compare, as English text,
// src/ai/MlCompare.cpp) and for the user (AI › ML, COMPARE: tables in the interface language, src/pages/ai/MlCompareView).
// Every figure comes from the runs' own files (result.json, predictions.csv / probe_predictions.csv) and ml::compare.
//
// Two questions, never mixed (the user's choice: both):
//   SAME MARKET    — several setups of one question (what is predicted, traded or grouped) on one market, one test period
//                    and one version of the data set: ranked by edge over no skill, each against no skill and persistence
//                    (paired block bootstrap on its test rows), the top two against each other, the setups tried on that
//                    market and question (Bonferroni; re-runs of one setup give the same result and count once).
//   SEVERAL MARKETS — one setup on several markets: each against its own market's no skill / persistence / buy & hold.
// Anything else is listed apart, with the reason.
namespace ml {
struct Run;
}

namespace ml::comparison {

// What a run brings to a comparison, copied on the window's thread (the report is built on another).
struct Item {
  QString id, name, market, family, algorithm, task, target, transform, kind, params, features, by, predictions;
  int horizon = 1, lags = 0;
  double testSize = 0.2;
  QString testPeriod;
  QDateTime created;
  QJsonObject result, job;
  QString dataHash;   // the data set's SHA-256 as the run read it (12 hex digits); empty for runs before fingerprints
  QString refsWhy;    // why its references could not be added afterwards (refs.json); empty otherwise
  QString question;   // what is predicted / traded / grouped
  QString setup;      // the method: algorithm, parameters, features, lags, test share, question
};

struct Input {
  QList<Item> items;
  QHash<QString, int> tries;   // market + question → different setups tried, over every finished run
  QStringList leftOut;         // "id running" / "id failed"…: chosen but not finished
};
// ml_compare's arguments → the runs: run_ids, or the filters (market, family, algorithm, target, last,
// only_this_environment: the runs environment `env` started; -1 = none). error set for unknown run ids. Window's thread.
QList<const Run*> choose(const QJsonObject& args, int env, QString* error);
// Window's thread: the chosen runs (unfinished ones listed apart) and the tries over every run.
Input gather(const QList<const Run*>& chosen);
// The runs a comparison of `runId` takes: the same question on the same market (any setup), and the same setup on other
// markets; the run itself first. Window's thread.
QStringList similarTo(const QString& runId);

enum class Kind { Predict, Trade, Groups, Probe };   // supervised / semi, reinforcement, unsupervised, self-supervised

struct Row {
  QString id, name, market, algorithm, params;
  int copies = 1;              // runs of this setup in the group (same result each time)
  int tries = 0;               // SEVERAL MARKETS: setups tried on that row's market and question
  // Predict / Probe: score (balanced accuracy, or MAE), no skill, edge (points, or share of the no-skill error removed)
  double value = NAN, reference = NAN, edge = NAN, p = NAN, adjP = NAN;
  double persScore = NAN, persEdge = NAN, persP = NAN, persAdjP = NAN;   // against persistence
  // Predict: the test predictions traded after costs (result "trading", since 2026-11-20): net return, Sharpe, buy & hold
  // on the same rows, and p of the timing against the model's own average exposure held all along
  double tradeNet = NAN, tradeSharpe = NAN, tradeBuyHold = NAN, tradeP = NAN, tradeCostBp = NAN;
  // Predict, volatility targets (result "references", since 2026-11-20): the run against HAR and EWMA on its test rows
  // (p: the run better; pWorse: the run worse), as the trainer measured them
  double harP = NAN, harPWorse = NAN, ewmaP = NAN, ewmaPWorse = NAN;
  // Trade: value = test return, reference = buy & hold, edge = excess
  double beaten = NAN, sharpe = NAN, maxDrawdown = NAN;
  // Groups
  int groups = 0;
  double silhouette = NAN, daviesBouldin = NAN, bic = NAN, longest = NAN;
};

enum class Verdict {
  None,               // not a ranked prediction group
  BeatsBoth,          // beats no skill and persistence after the tries
  BeatsNoSkillOnly,   // beats no skill, not persistence (persBetter: persistence does better)
  BeatsNoSkill,       // beats no skill; no persistence on record
  Worse,              // even the best does worse than no skill, not by chance
  NotBetter,          // even the best does no better than no skill
  Luck,               // the best's edge is not distinguishable from chance after the tries
  Unknown             // no test against no skill possible (older runs)
};

struct Group {
  enum Mode { SameMarket, OneSetupOnly, SeveralMarkets } mode = SameMarket;
  Kind kind = Kind::Predict;
  bool values = false;         // scores are errors (MAE): lower is better
  QString market, question, testPeriod, dataHash, algorithm, params;
  QString scoreKey;            // balanced_accuracy, mae, test_return, silhouette (probe_ prefix for probes)
  int setups = 0, tries = 0;
  QList<Row> rows;             // ranked (SAME MARKET) or one per market (SEVERAL MARKETS)
  // SAME MARKET, predictions: the top two on their shared test rows
  QString topA, topB;
  int topRows = 0;
  double topDiff = NAN, topP = NAN;
  Verdict verdict = Verdict::None;
  bool persBetter = false, beatsPersistence = false;
  double verdictAdjP = NAN;
  // SAME MARKET, predictions, 3 setups or more: overfitting across them (compare::pbo on the rows they share)
  double pbo = NAN;
  int pboRows = 0, pboSplits = 0;
  // Trade: random agents' 5th–95th percentile (the noise band)
  double noiseLo = NAN, noiseHi = NAN;
  // SEVERAL MARKETS: markets beaten, beaten significantly (after tries) or beating 95 % of random agents, known, mean edge
  int beat = 0, sig = 0, known = 0;
  double meanEdge = NAN;
  // OneSetupOnly: the runs of the single setup
  QString onlyRun;
  int copiesOnly = 0;
};

struct Excluded {
  QString id, market, algorithm, question;
  enum Why { Alone, OtherPeriod, OtherData } why = Alone;
};

struct Report {
  int compared = 0;
  QStringList leftOut;
  QList<Group> groups;
  QList<Excluded> notCompared;
  QList<QPair<QString, QString>> withoutRefs;   // run id, why (the trainer's words)
  bool nothing = false;                         // no group at all
};

Report build(const Input& in);          // any thread: reads prediction files, runs the tests

// The trial registry (src/trials, 2026-11-20): a finished training is one try of its setup on its market and question,
// written once (by run id) whoever started it, the user in AI › ML or an agent. The tries ml_compare corrects for come
// from there too, so a deleted run still counts. GUI thread.
void recordTrial(const Run& r);
void recordTrials();                    // every finished run not written yet (runs made before the registry)
QString toText(const Report& r);        // the agents' answer (English; ml_compare)

}  // namespace ml::comparison
