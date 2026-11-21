#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

// The figures of ml_compare (AI › Agents' "ml" skill), computed by the app from the trainings' own files, never by a model:
// a test period's predictions, their score, and whether two sets of predictions on the same rows really differ.
// Pure QtCore: tested in tests/ml (tests/ml/compare_tests.cpp).
namespace ml::compare {

// predictions.csv of a supervised or semi-supervised run (probe_predictions.csv of a self-supervised one): date, actual,
// predicted (a class, or a value), and since 2026-11-19 the two references next to each row: no_skill (the baseline's
// constant) and persistence (the target's last seen value; NaN where unknown). Older files: those two are empty.
struct Preds {
  QStringList dates;
  QVector<double> actual, predicted;
  QVector<double> noSkill, persistence;   // empty, or one per row
  bool ok() const { return !actual.isEmpty() && actual.size() == predicted.size() && actual.size() == dates.size(); }
};
Preds readPredictions(const QString& csvPath);

// Balanced accuracy: the mean, over the classes the actual values hold, of the share of each class predicted right
// (rounded values are the classes). Mean absolute error for values.
double balancedAccuracy(const QVector<double>& actual, const QVector<double>& predicted);
double meanAbsError(const QVector<double>& actual, const QVector<double>& predicted);

// The rows two runs were both tested on, by date: for each common date its index in a and in b (a's order).
struct Common { QVector<int> ia, ib; };
Common commonRows(const QStringList& a, const QStringList& b);

// Do predictions A and B score differently on the same rows? Paired moving-block bootstrap (blocks of `block` rows keep
// the dependence of overlapping targets and of markets' regimes): both are scored on the same resampled rows each time.
// diff = score(A) − score(B), "higher is better" (balanced accuracy, or −MAE for values); p = two-sided share of
// resamples on the other side of zero. Deterministic for a given seed.
struct Paired {
  double scoreA = 0, scoreB = 0, diff = 0, p = 1;
  int rows = 0;
};
Paired pairedBootstrap(const QVector<double>& actual, const QVector<double>& predA, const QVector<double>& predB, bool classification,
                       int block, int resamples = 1000, uint32_t seed = 12345);

// Bonferroni: a p-value among `tries` (the trainings that tried the same question on the same market).
double adjusted(double p, int tries);

// Probability of backtest overfitting across setups (CSCV, Bailey, Borwein, López de Prado & Zhu; step 2 of the roadmap,
// 2026-11-20): `perRow` = one series per setup of the same rows in time order, each row's score (higher is better). The
// rows are cut into `blocks` in time order; for every choice of half the blocks, the setup with the best mean on that
// half is ranked on the other half: PBO = the share of choices where it lands at or below the median. Above 0.5,
// picking the best of these setups is mostly picking luck. Needs 3 setups and 10 rows a block.
struct Pbo { double pbo = NAN; int splits = 0, rows = 0, setups = 0; };
Pbo pbo(const QVector<QVector<double>>& perRow, int blocks = 8);   // splits = 0: not enough to compute
// Each row's score for pbo(): classification, right = 1 / (classes × share of the row's class) so that the mean is the
// balanced accuracy; values, minus the absolute error.
QVector<double> rowScores(const QVector<double>& actual, const QVector<double>& predicted, bool classification);

}  // namespace ml::compare
