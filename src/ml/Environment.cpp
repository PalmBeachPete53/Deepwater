#include "ml/Environment.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QtConcurrent>

namespace ml {
namespace {
QString runPython(const QString& py, const QStringList& args, int ms = 20000) {
  QProcess p;
  p.start(py, args);
  if (!p.waitForFinished(ms)) { p.kill(); p.waitForFinished(1000); return {}; }
  return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}
// What the environment holds: versions of what imports, the error of what does not (one line each).
const char* kProbe = R"(
import json, importlib
ok, bad = {}, {}
for m in ("numpy", "pandas", "scipy", "sklearn", "joblib", "lightgbm", "xgboost", "torch"):
    try:
        ok[m] = getattr(importlib.import_module(m), "__version__", "?")
    except Exception as e:
        bad[m] = (str(e) or e.__class__.__name__).splitlines()[0][:300]
print(json.dumps({"ok": ok, "bad": bad}))
)";
}

QString findPython(QString* version) {
  QStringList cands;
  for (int minor = 20; minor >= 11; --minor)
    cands << QString("/Library/Frameworks/Python.framework/Versions/3.%1/bin/python3").arg(minor);
  cands << "/opt/homebrew/bin/python3" << "/usr/local/bin/python3";
  for (const QString& c : cands) {
    if (!QFileInfo(c).isExecutable()) continue;
    const QString v = runPython(c, {"-I", "-c", "import sys; print('%d.%d.%d' % sys.version_info[:3])"}, 5000);
    const QStringList p = v.split('.');
    if (p.size() == 3 && p[0].toInt() == 3 && p[1].toInt() >= 11) { if (version) *version = v; return c; }
  }
  return {};
}

Environment& Environment::instance() { static Environment* e = new Environment; return *e; }

QString Environment::venvDir() const { return paths::data("ml") + "/venv"; }
QString Environment::python() const { return venvDir() + "/bin/python"; }

QString Environment::lockText(const QString& group) {
  QFile f(":/ml/requirements-" + group + ".lock");
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

QString Environment::writeLock(const QString& group) const {
  const QString text = lockText(group);
  if (text.isEmpty()) return {};
  const QString path = paths::data("ml") + "/requirements-" + group + ".lock";
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly) || f.write(text.toUtf8()) < 0 || !f.commit()) return {};
  return path;
}

// the version shown on the page: read from the lock, the one place versions are set (tools/ml_lock.py)
QStringList Environment::torchRequirements() {
  for (const QString& line : lockText("torch").split('\n'))
    if (line.startsWith("torch==")) return {line.section(' ', 0, 0)};
  return {};
}

void Environment::refresh() {
  if (m_refreshing) { m_refreshAgain = true; return; }
  m_refreshing = true;
  const QString py = python();
  auto* w = new QFutureWatcher<EnvStatus>(this);
  connect(w, &QFutureWatcher<EnvStatus>::finished, this, [this, w] {
    m_status = w->result();
    w->deleteLater();
    m_refreshing = false;
    emit changed();
    if (m_refreshAgain) { m_refreshAgain = false; refresh(); }
  });
  w->setFuture(QtConcurrent::run([py] {
    EnvStatus s;
    s.basePython = findPython(&s.baseVersion);
    s.venv = QFileInfo(py).isExecutable();
    if (s.venv) {
      const QJsonObject o = QJsonDocument::fromJson(runPython(py, {"-I", "-c", kProbe}, 60000).toUtf8()).object();
      const QJsonObject good = o.value("ok").toObject();
      for (auto it = good.begin(); it != good.end(); ++it) s.packages.insert(it.key(), it.value().toString());
      const QJsonObject bad = o.value("bad").toObject();
      for (auto it = bad.begin(); it != bad.end(); ++it) s.unavailable.insert(it.key(), it.value().toString());
    }
    s.probed = true;
    return s;
  }));
}

void Environment::install(bool withTorch) {
  if (m_install) return;
  m_withTorch = withTorch || hasTorch();   // never remove it once there
  m_log.clear();
  m_step = QFileInfo(python()).isExecutable() ? 1 : 0;
  runStep();
}

void Environment::runStep() {
  QString exe;
  QStringList args;
  if (m_step == 0) {
    // findPython() is not run here: it tries up to 12 interpreters for 5 s each, on the GUI thread. The probe of
    // refresh() (a worker) finds it; pressed before that probe is done, the user is asked to wait.
    if (!m_status.probed) {
      refresh();
      emit installFinished(false, QCoreApplication::translate("ai", "Still looking for Python on this computer: try again in a few seconds."));
      return;
    }
    exe = m_status.basePython;
    if (exe.isEmpty()) { emit installFinished(false, QCoreApplication::translate("ai", "No Python 3.11 or newer on this computer.")); return; }
    QDir().mkpath(QFileInfo(venvDir()).absolutePath());
    args << "-m" << "venv" << venvDir();
  } else {
    exe = python();
    // --require-hashes: every package (dependencies included) must be in the lock files, with the sha256 of the wheel
    // pip downloads; a replaced or tampered wheel is refused. The files are app resources (tools/ml_lock.py writes
    // them), copied next to the environment for pip to read.
    QStringList locks{"base"};
    if (m_withTorch) locks << "torch";
    args << "-m" << "pip" << "install" << "--disable-pip-version-check" << "--no-input" << "--only-binary=:all:"
         << "--require-hashes" << "--progress-bar" << "off";
    for (const QString& l : locks) {
      const QString path = writeLock(l);
      if (path.isEmpty()) {
        emit installFinished(false, QCoreApplication::translate("ai", "The list of verified packages could not be written next to the environment."));
        return;
      }
      args << "-r" << path;
    }
  }
  m_install = new QProcess(this);
  m_install->setProcessChannelMode(QProcess::MergedChannels);
  // Deepwater closing: pip (or venv) would go on alone, half-installing the environment with nobody to report it; the
  // environment is never destroyed (a leaked singleton), so its child process is ended here. Disconnected first: the
  // finished handler would report a failure and start a probe while the app quits. The next probe shows what is missing.
  connect(qApp, &QCoreApplication::aboutToQuit, m_install, [this, p = m_install] {
    p->disconnect(this);
    p->kill();
    p->waitForFinished(2000);
  });
  connect(m_install, &QProcess::readyRead, this, [this] {
    const QStringList lines = QString::fromUtf8(m_install->readAll()).split('\n', Qt::SkipEmptyParts);
    for (const QString& l : lines) {
      m_log += l + "\n";
      if (m_log.size() > 200000) m_log = m_log.right(100000);
      emit installLine(l.trimmed());
    }
  });
  connect(m_install, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
    m_install->deleteLater();
    m_install = nullptr;
    if (st != QProcess::NormalExit || code != 0) {
      const QString tail = m_log.trimmed().section('\n', -2);
      emit installFinished(false, (m_step == 0 ? QCoreApplication::translate("ai", "Creating the environment failed.") : QCoreApplication::translate("ai", "Installing the packages failed.")) +
                                  (tail.isEmpty() ? QString() : "\n" + tail));
      refresh();
      return;
    }
    if (++m_step < installSteps()) { runStep(); return; }
    emit installFinished(true, {});
    refresh();
  });
  m_install->start(exe, args);
  emit changed();
}

void Environment::cancelInstall() {
  if (!m_install) return;
  m_install->kill();
}
}  // namespace ml
