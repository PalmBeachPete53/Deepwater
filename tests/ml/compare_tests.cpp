// ml_compare's figures (src/ml/Compare): what the agents' comparison of trainings rests on.
#include "Check.h"
#include "ml/Compare.h"
#include <QFile>
#include <QTemporaryDir>
#include <random>

using namespace ml::compare;

TEST("balanced accuracy: the mean of each class's share predicted right") {
  // class 1: 3 of 4 right; class 0: 1 of 2 right → (0.75 + 0.5) / 2
  const QVector<double> a{1, 1, 1, 1, 0, 0}, p{1, 1, 1, 0, 0, 1};
  CHECK_NEAR(balancedAccuracy(a, p), 0.625, 1e-12);
  // always the majority class: 0.5, whatever the imbalance
  CHECK_NEAR(balancedAccuracy(a, QVector<double>(6, 1)), 0.5, 1e-12);
  CHECK_NEAR(meanAbsError({1, 2, 3}, {1, 3, 5}), 1.0, 1e-12);
}

TEST("predictions are read by column name, bad rows left out") {
  QTemporaryDir dir;
  QFile f(dir.filePath("predictions.csv"));
  CHECK(f.open(QIODevice::WriteOnly));
  f.write("date,actual,predicted,probability\n2026-01-01,1,1,0.7\n2026-01-02,0,1,0.6\nbroken\n2026-01-03,nan,1,0.5\n2026-01-04,1,0,0.4\n");
  f.close();
  const Preds p = readPredictions(f.fileName());
  CHECK(p.ok());
  CHECK_EQ(int(p.actual.size()), 3);
  CHECK_EQ(p.dates.value(2), QString("2026-01-04"));
  CHECK_NEAR(p.predicted.value(2), 0.0, 0);
}

TEST("common rows: matched by date, in the first run's order") {
  const Common c = commonRows({"d1", "d2", "d3", "d4"}, {"d4", "d2", "d9"});
  CHECK_EQ(c.ia, (QVector<int>{1, 3}));
  CHECK_EQ(c.ib, (QVector<int>{1, 0}));
}

TEST("paired test: the same predictions do not differ") {
  std::mt19937 rng(1);
  QVector<double> a, p;
  for (int i = 0; i < 500; ++i) { a << double(rng() % 2); p << double(rng() % 2); }
  const Paired r = pairedBootstrap(a, p, p, true, 5);
  CHECK_NEAR(r.diff, 0.0, 0);
  CHECK_NEAR(r.p, 1.0, 0);
}

TEST("paired test: a clearly better classifier is significant, a coin flip against no skill is not") {
  std::mt19937 rng(7);
  const int n = 800;
  QVector<double> a, good, coin, majority(n, 1);
  for (int i = 0; i < n; ++i) {
    const double y = double(rng() % 2);
    a << y;
    good << ((rng() % 100) < 70 ? y : 1 - y);   // right 70 % of the time
    coin << double(rng() % 2);                    // no skill
  }
  const Paired g = pairedBootstrap(a, good, majority, true, 5);
  CHECK(g.diff > 0.15);
  CHECK(g.p < 0.01);
  const Paired c = pairedBootstrap(a, coin, majority, true, 5);
  CHECK(std::abs(c.diff) < 0.06);
  CHECK(c.p > 0.05);
}

TEST("paired test on values: lower error is the better side, p two-sided") {
  std::mt19937 rng(3);
  std::normal_distribution<double> noise(0, 1);
  QVector<double> a, close, far;
  for (int i = 0; i < 600; ++i) { const double y = noise(rng); a << y; close << y + 0.3 * noise(rng); far << y + 1.0 * noise(rng); }
  const Paired r = pairedBootstrap(a, close, far, false, 5);
  CHECK(r.scoreA > r.scoreB);   // −MAE: higher is better
  CHECK(r.diff > 0);
  CHECK(r.p < 0.01);
  const Paired back = pairedBootstrap(a, far, close, false, 5);
  CHECK(back.diff < 0);
  CHECK(back.p < 0.01);
  // the same seed gives the same answer
  CHECK_NEAR(pairedBootstrap(a, close, far, false, 5).p, r.p, 0);
}

TEST("many tries: Bonferroni, capped at 1") {
  CHECK_NEAR(adjusted(0.01, 14), 0.14, 1e-12);
  CHECK_NEAR(adjusted(0.2, 14), 1.0, 0);
  CHECK_NEAR(adjusted(0.03, 0), 0.03, 1e-12);
}

TEST("predictions with their references: no_skill and persistence read, an empty persistence is NaN") {
  QTemporaryDir dir;
  QFile f(dir.filePath("predictions.csv"));
  CHECK(f.open(QIODevice::WriteOnly));
  f.write("date,actual,predicted,probability,no_skill,persistence\n2026-01-01,1,1,0.7,1,\n2026-01-02,0,1,0.6,1,0.0\n");
  f.close();
  const Preds p = readPredictions(f.fileName());
  CHECK(p.ok());
  CHECK_EQ(int(p.noSkill.size()), 2);
  CHECK_EQ(int(p.persistence.size()), 2);
  CHECK(std::isnan(p.persistence[0]));
  CHECK_NEAR(p.persistence[1], 0.0, 0);
  CHECK_NEAR(p.noSkill[1], 1.0, 0);
}

TEST("row scores: their mean is the balanced accuracy; for values, minus the absolute error") {
  const QVector<double> act{1, 1, 1, 0, 1, 0, 1, 1};
  const QVector<double> pred{1, 0, 1, 0, 1, 1, 0, 1};
  const QVector<double> r = rowScores(act, pred, true);
  double mean = 0;
  for (double x : r) mean += x / r.size();
  CHECK_NEAR(mean, balancedAccuracy(act, pred), 1e-12);
  const QVector<double> v = rowScores({1.0, 2.0}, {1.5, 1.0}, false);
  CHECK_NEAR(v[0], -0.5, 1e-12);
  CHECK_NEAR(v[1], -1.0, 1e-12);
}

TEST("pbo across setups: needs 3 setups and 10 rows a block; low when one is better everywhere") {
  auto noisy = [](int n, double edge, uint32_t seed) {
    std::mt19937 g(seed);
    std::bernoulli_distribution d(0.5 + edge);
    QVector<double> r(n);
    for (auto& x : r) x = d(g) ? 1.0 : 0.0;
    return r;
  };
  CHECK_EQ(pbo({noisy(200, 0, 1), noisy(200, 0, 2)}).splits, 0);
  CHECK_EQ(pbo({noisy(60, 0, 1), noisy(60, 0, 2), noisy(60, 0, 3)}).splits, 0);
  const Pbo good = pbo({noisy(1600, 0.15, 1), noisy(1600, 0, 2), noisy(1600, 0, 3), noisy(1600, 0, 4)});
  CHECK_EQ(good.splits, 70);
  CHECK_EQ(good.setups, 4);
  CHECK(good.pbo < 0.05);
  // ten setups of pure chance: on one sample the PBO varies widely, but on average the best of one half is no better
  // than the median on the other (about one chance in two)
  double sum = 0;
  for (uint32_t rep = 0; rep < 30; ++rep) {
    QVector<QVector<double>> luck;
    for (uint32_t k = 0; k < 10; ++k) luck << noisy(400, 0, 1000 * rep + k);
    sum += pbo(luck).pbo;
  }
  CHECK(sum / 30 > 0.35);
  CHECK(sum / 30 < 0.75);
}

int main(int argc, char** argv) { return check::runAll(argc, argv); }
