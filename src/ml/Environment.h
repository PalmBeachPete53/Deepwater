#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include "core/Process.h"

// AI › ML: Deepwater's own Python environment, <data>/ml/venv, made from a Python 3.11+ already on this computer
// (python.org, Homebrew). Created and filled from the ML page only (INSTALL), with pinned versions and binary wheels only
// (pip --only-binary=:all: : no package code runs at install time), each wheel checked against its sha256 in the lock
// files (pip --require-hashes, resources/ml/*.lock). Training then runs inside a sandbox (ml::Runs).
//
// PyTorch (self-supervised learning) is a separate, optional install (INSTALL PYTORCH), CPU only inside the sandbox.
// LightGBM and XGBoost load OpenMP from Homebrew (libomp): when it is missing they are listed as unavailable with the
// reason, and the other algorithms (scikit-learn ships its own OpenMP) still work.
namespace ml {

struct EnvStatus {
  bool probed = false;
  QString basePython;          // the Python the environment is made from (empty: none found)
  QString baseVersion;         // "3.15.0"
  bool venv = false;           // <data>/ml/venv/bin/python exists
  QMap<QString, QString> packages;      // import name → version, for what imports
  QMap<QString, QString> unavailable;   // import name → why it does not import
  bool ready() const { return venv && packages.contains("sklearn") && packages.contains("pandas") && packages.contains("joblib"); }
};

class Environment : public QObject {
  Q_OBJECT
public:
  static Environment& instance();
  const EnvStatus& status() const { return m_status; }
  QString venvDir() const;
  QString python() const;      // <venv>/bin/python
  bool installing() const { return m_install != nullptr; }
  QString installLog() const { return m_log; }
  int installStep() const { return m_step; }   // 0 venv, 1 packages; with installSteps()
  static int installSteps() { return 2; }
  static QStringList torchRequirements();      // "torch==<version>" from the lock; installed only when asked, ~700 MB on disk
  // resources/ml/requirements-<group>.lock ("base", "torch"): pinned packages and their dependencies, with wheel hashes
  static QString lockText(const QString& group);

  void refresh();              // finds Python, reads what the environment holds (a second or two, off the GUI thread)
  void install(bool withTorch = false);   // creates the environment if needed and installs the pinned packages (~300 MB)
  bool hasTorch() const { return m_status.packages.contains("torch"); }
  void cancelInstall();

signals:
  void changed();
  void installLine(const QString& line);
  void installFinished(bool ok, const QString& error);

private:
  Environment() = default;
  void runStep();
  QString writeLock(const QString& group) const;   // the lock copied under <data>/ml for pip; its path, "" on failure
  EnvStatus m_status;
  dw::Process* m_install = nullptr;
  QString m_log;
  int m_step = 0;
  bool m_refreshing = false, m_refreshAgain = false, m_withTorch = false;
};

QString findPython(QString* version = nullptr);   // newest Python ≥ 3.11 found, empty if none
}  // namespace ml
