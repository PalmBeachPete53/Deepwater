// ml_compare (skill "ml", user's request 2026-11-19): the agents compare trainings with figures the app computes from the
// runs' own files, never with numbers a model copies or works out (memory: models invent). The comparison itself is
// ml::comparison (src/ml/Comparison.h), shared with AI › ML's COMPARE button; this file picks the runs and answers in text.
#include "ai/Tools.h"
#include "ml/Comparison.h"
#include "ml/Runs.h"
#include "ml/Datasets.h"
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QtConcurrent>

namespace ai {

void mlCompare(const QJsonObject& a, ToolDone done) {
  QString error;
  QList<const ml::Run*> chosen = ml::comparison::choose(a, currentCaller().env, &error);
  if (!error.isEmpty()) { done({false, "error: " + error + "; see ml_runs"}); return; }
  // evaluation mode: only the trainings on data sets cut at or before the moment (MlTools.cpp, visibleRun)
  if (const qint64 asOf = asOfFor("ml_compare"); asOf > 0) {
    QList<const ml::Run*> seen;
    for (const ml::Run* r : chosen) {
      const qint64 at = ml::datasetAsOf(r->dataPath);
      if (!r->dataPath.isEmpty() && at > 0 && at <= asOf) seen << r;
      else if (a.contains("run_ids")) { done({false, "error: no run \"" + r->id + "\"; see ml_runs"}); return; }
    }
    chosen = seen;
  }
  const ml::comparison::Input in = ml::comparison::gather(chosen);
  if (in.items.size() < 2) {
    done({false, QString("error: %1 finished training(s) selected; at least two are needed (ml_runs lists them; filters: market, family, "
                         "algorithm, target, only_this_environment)").arg(in.items.size())});
    return;
  }
  // prediction files can hold tens of thousands of rows and the test is resampled: off the window's thread
  auto* w = new QFutureWatcher<QString>(qApp);
  QObject::connect(w, &QFutureWatcher<QString>::finished, qApp, [w, done] {
    w->deleteLater();
    done({true, w->result()});
  });
  w->setFuture(QtConcurrent::run([in] { return ml::comparison::toText(ml::comparison::build(in)); }));
}
}  // namespace ai
