#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

class QProcess;

// AI › ML trainings. A run = a folder <data>/ml/runs/<id>/ holding run.json (name, state, data set), log.txt and
// stderr.txt, written by the app only, and work/: what was asked (job.json) and the outcome (result.json,
// predictions.csv, model.joblib...), the one folder the trainer can write. The data set it learns from is one of the
// app's own (<data>/ml/datasets): the sandbox lets the trainer read that one file, read-only, instead of a copy per run
// (a million-row set is hundreds of MB); any other file is copied to work/data.csv as before. Runs made before
// 2026-11-19 have everything in the run folder itself (work == dir). One training at a time.
// A training an AI started (by not empty) is stopped after AI_RUN_LIMIT_MS: a model must not hold the trainer for hours.
// Files of work/ are read through runFile(): never through a symbolic link (the trainer could have planted one).
//
// The trainer (resources/ml/train.py, written to <data>/ml/train.py) runs with the environment's Python inside
// sandbox-exec: deny by default; it may read the Python installation, the environment, OpenMP and the script, and read and
// write its own run folder; nothing else: no network, no preferences (API keys), no other file, no other process.
namespace ml {

enum class RunState { Running, Done, Failed, Stopped };

struct Run {
  QString id, dir, name, dataset;   // dataset: title shown ("EUR/USD · 1d")
  QString work;                      // the trainer's folder (dir + "/work", or dir for older runs)
  QString by;                        // "" (the user, from the page) or "environment 2 · Claude Code" (an AI's ML skill)
  QDateTime created;
  QJsonObject job, result;
  RunState state = RunState::Running;
  QString stage, error;
  double pct = 0;
};

class Runs : public QObject {
  Q_OBJECT
public:
  static Runs& instance();
  const QList<Run>& list() const { return m_runs; }   // newest first
  const Run* find(const QString& id) const;
  bool running() const { return m_proc != nullptr; }
  QString runningDataset() const { return m_proc ? m_dataset : QString(); }   // canonical path of the set being read
  // Starts training on the data set in a new run folder; returns the run id, or empty with *error set.
  QString start(const QJsonObject& job, const QString& datasetPath, const QString& datasetTitle, const QString& name, QString* error,
                const QString& by = {});
  void stop();
  void remove(const QString& id);
  QString predictionsPath(const QString& id) const;

signals:
  void listChanged();
  void runChanged(const QString& id);

private:
  Runs();
  void load();
  void saveMeta(const Run& r) const;
  Run* findMut(const QString& id);
  QString sandboxProfile(const QString& runDir, const QString& script, const QString& data = {}) const;
  QList<Run> m_runs;
  QProcess* m_proc = nullptr;
  QString m_current, m_dataset;
  QByteArray m_buf;
};

constexpr int AI_RUN_LIMIT_MS = 30 * 60 * 1000;
QString runsDir();
// <work>/<name> of a run, or empty when that file is a symbolic link or resolves outside the work folder.
QString runFile(const Run& r, const QString& name);
QString stateName(RunState s);   // translated: RUNNING, DONE, FAILED, STOPPED
}  // namespace ml
