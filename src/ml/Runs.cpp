#include "ml/Runs.h"
#include "ml/Environment.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTimer>

namespace ml {
namespace {
QString sbQuote(const QString& s) {
  QString o = s;
  o.replace('\\', "\\\\").replace('"', "\\\"");
  return '"' + o + '"';
}
QJsonObject readJson(const QString& path) {
  if (path.isEmpty()) return {};
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject{};
}
bool writeJson(const QString& path, const QJsonObject& o) {
  paths::dropLink(path);   // QSaveFile writes through a link
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return false;
  f.write(QJsonDocument(o).toJson());
  return f.commit();
}
QString stateKey(RunState s) {
  switch (s) { case RunState::Running: return "running"; case RunState::Done: return "done"; case RunState::Failed: return "failed";
               case RunState::Stopped: return "stopped"; }
  return "failed";
}
RunState stateOf(const QString& k) {
  return k == "done" ? RunState::Done : k == "stopped" ? RunState::Stopped : k == "running" ? RunState::Running : RunState::Failed;
}
}  // namespace

QString runsDir() { return paths::data("ml/runs"); }

QString runFile(const Run& r, const QString& name) {
  const QString work = r.work.isEmpty() ? r.dir : r.work;
  const QString path = work + "/" + name;
  return paths::insideNoLink(work, path) ? path : QString();
}

QString stateName(RunState s) {
  switch (s) {
    case RunState::Running: return QCoreApplication::translate("ai", "RUNNING");
    case RunState::Done: return QCoreApplication::translate("ai", "DONE");
    case RunState::Failed: return QCoreApplication::translate("ai", "FAILED");
    case RunState::Stopped: return QCoreApplication::translate("ai", "STOPPED");
  }
  return {};
}

Runs& Runs::instance() { static Runs* r = new Runs; return *r; }

Runs::Runs() {
  load();
  // Deepwater closing: the training is interrupted, not failed (the finished handler below leaves a Stopped run as it is)
  connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
    if (!m_proc) return;
    if (Run* run = findMut(m_current)) {
      run->state = RunState::Stopped;
      run->error = QCoreApplication::translate("ai", "Interrupted: Deepwater was closed during the training.");
      saveMeta(*run);
    }
    m_proc->kill();
    m_proc->waitForFinished(1000);
  });
}

void Runs::load() {
  m_runs.clear();
  QDir d(runsDir());
  const auto dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
  for (const QString& id : dirs) {
    const QString dir = d.filePath(id);
    const QJsonObject meta = readJson(dir + "/run.json");
    if (meta.isEmpty()) continue;
    Run r;
    r.id = id; r.dir = dir;
    const QFileInfo wi(dir + "/work");
    r.work = wi.isDir() && !wi.isSymLink() ? wi.filePath() : dir;
    r.name = meta.value("name").toString(); r.dataset = meta.value("dataset").toString();
    r.created = QDateTime::fromString(meta.value("created").toString(), Qt::ISODate);
    r.state = stateOf(meta.value("state").toString());
    r.error = meta.value("error").toString();
    r.by = meta.value("by").toString();
    if (r.state == RunState::Running) {   // the app closed while it trained
      r.state = RunState::Stopped;
      r.error = QCoreApplication::translate("ai", "Interrupted: Deepwater was closed during the training.");
      saveMeta(r);
    }
    r.job = readJson(runFile(r, "job.json"));
    r.result = readJson(runFile(r, "result.json"));
    r.pct = r.state == RunState::Done ? 100 : 0;
    m_runs << r;
  }
}

const Run* Runs::find(const QString& id) const {
  for (const auto& r : m_runs) if (r.id == id) return &r;
  return nullptr;
}
Run* Runs::findMut(const QString& id) {
  for (auto& r : m_runs) if (r.id == id) return &r;
  return nullptr;
}

void Runs::saveMeta(const Run& r) const {
  writeJson(r.dir + "/run.json", QJsonObject{{"name", r.name}, {"dataset", r.dataset}, {"created", r.created.toString(Qt::ISODate)},
                                             {"state", stateKey(r.state)}, {"error", r.error}, {"by", r.by}});
}

QString Runs::predictionsPath(const QString& id) const {
  const Run* r = find(id);
  return r ? runFile(*r, "predictions.csv") : QString();
}

QString Runs::sandboxProfile(const QString& runDir, const QString& script, const QString& data) const {
  const auto& env = Environment::instance();
  const QString venv = QFileInfo(env.venvDir()).canonicalFilePath();
  // the Python the environment was made from: its whole installation (standard library, framework)
  QStringList reads{venv};
  const QString exe = QFileInfo(env.python()).canonicalFilePath();
  if (!exe.isEmpty()) reads << QFileInfo(QFileInfo(exe).absolutePath() + "/..").canonicalFilePath();
  // pyvenv.cfg's `home` is the folder of the Python the environment was made from: often a shared bin folder
  // (/opt/homebrew/bin, /usr/local/bin), whose parent is the whole Homebrew tree. The interpreter there is followed to
  // its real installation (Cellar/…, Python.framework/…) and only that is granted.
  QFile cfg(env.venvDir() + "/pyvenv.cfg");
  if (cfg.open(QIODevice::ReadOnly))
    for (const QString& line : QString::fromUtf8(cfg.readAll()).split('\n'))
      if (line.startsWith("home")) {
        const QString home = line.section('=', 1).trimmed();
        for (const QString& name : {QStringLiteral("python3"), QStringLiteral("python")}) {
          const QString real = QFileInfo(home + "/" + name).canonicalFilePath();
          if (real.isEmpty()) continue;
          const QString root = QFileInfo(QFileInfo(real).absolutePath() + "/..").canonicalFilePath();
          // never a top-level tree ("/usr", "/opt/homebrew", "/usr/local"): those hold everything else installed
          if (root.count('/') >= 3) reads << root;
          break;
        }
      }
  // OpenMP for LightGBM / XGBoost (Homebrew)
  for (const QString& omp : {QString("/opt/homebrew/opt/libomp"), QString("/usr/local/opt/libomp")}) {
    if (!QFileInfo::exists(omp)) continue;
    reads << omp << QFileInfo(omp).canonicalFilePath();
  }
  reads.removeAll(QString());
  reads.removeDuplicates();
  QStringList sub;
  for (const QString& r : reads) sub << "(subpath " + sbQuote(r) + ")";
  QStringList p;
  p << "(version 1)"
    << "(deny default)"
    << "(import \"system.sb\")"
    << "(allow file-read-metadata)"          // path resolution (realpath) walks parent folders: names and dates only, no content
    << "(allow process-exec " + sub.join(' ') + ")"
    << "(allow file-read* " + sub.join(' ') + " (literal " + sbQuote(QFileInfo(script).canonicalFilePath()) + ")" +
           (data.isEmpty() ? QString() : " (literal " + sbQuote(data) + ")") + ")"   // the data set: read only
    << "(allow file-read* file-write* (subpath " + sbQuote(QFileInfo(runDir).canonicalFilePath()) + "))";   // the work folder only
  return p.join('\n');
}

QString Runs::start(const QJsonObject& job, const QString& datasetPath, const QString& datasetTitle, const QString& name, QString* error,
                    const QString& by) {
  auto fail = [error](const QString& e) { if (error) *error = e; return QString(); };
  if (m_proc) return fail(QCoreApplication::translate("ai", "A training is already running."));
#ifndef __APPLE__
  // the trainer runs only inside macOS's sandbox (sandbox-exec, below); elsewhere it would start with the user's full
  // rights, which the ML page says it never has: refused with a clear reason rather than "could not start"
  return fail(QCoreApplication::translate("ai", "Trainings run in a sandbox that exists only on macOS for now."));
#endif
  if (!Environment::instance().status().ready()) return fail(QCoreApplication::translate("ai", "The Python environment is not installed."));
  // the trainer script, from the app's resources (overwritten each time: always the app's own version)
  const QString script = paths::data("ml") + "/train.py";
  {
    QFile src(":/ml/train.py");
    QSaveFile dst(script);
    if (!src.open(QIODevice::ReadOnly) || !dst.open(QIODevice::WriteOnly)) return fail(QCoreApplication::translate("ai", "The trainer script cannot be written."));
    dst.write(src.readAll());
    if (!dst.commit()) return fail(QCoreApplication::translate("ai", "The trainer script cannot be written."));
  }
  Run r;
  r.created = QDateTime::currentDateTime();
  r.id = r.created.toString("yyyyMMdd-HHmmss") + "-" + QString::number(QRandomGenerator::global()->bounded(0x1000, 0xffff), 16);
  r.dir = runsDir() + "/" + r.id;
  r.work = r.dir + "/work";
  r.name = name; r.dataset = datasetTitle; r.job = job; r.by = by;
  r.stage = QCoreApplication::translate("ai", "starting");
  if (!QDir().mkpath(r.work)) return fail(QCoreApplication::translate("ai", "The run folder cannot be created."));
  // one of the app's data sets (a plain file in its folder, no link): read in place, read-only; anything else is copied
  // as before. Each run used to copy its set, hundreds of MB for a million rows, on the window's thread.
  const QFileInfo di(datasetPath);
  const QString dataDir = QFileInfo(paths::data("ml/datasets")).canonicalFilePath();
  QString data = di.isFile() && !di.isSymLink() ? di.canonicalFilePath() : QString();
  if (data.isEmpty() || dataDir.isEmpty() || QFileInfo(data).absolutePath() != dataDir) data.clear();
  if (data.isEmpty() && !QFile::copy(datasetPath, r.work + "/data.csv")) return fail(QCoreApplication::translate("ai", "The data set cannot be copied."));
  writeJson(r.work + "/job.json", job);
  saveMeta(r);
  m_runs.prepend(r);
  m_current = r.id;
  m_dataset = data.isEmpty() ? QFileInfo(r.work + "/data.csv").canonicalFilePath() : data;
  emit listChanged();

  m_proc = new QProcess(this);
  m_buf.clear();
  QProcessEnvironment pe;
  pe.insert("PATH", "/usr/bin:/bin");
  pe.insert("HOME", r.work); pe.insert("TMPDIR", r.work);
  pe.insert("PYTHONDONTWRITEBYTECODE", "1"); pe.insert("PYTHONNOUSERSITE", "1");
  pe.insert("LANG", "en_US.UTF-8"); pe.insert("OMP_NUM_THREADS", "4");
  m_proc->setProcessEnvironment(pe);
  m_proc->setWorkingDirectory(r.work);
  m_proc->setStandardErrorFile(r.dir + "/stderr.txt");
  const QString id = r.id;
  connect(m_proc, &QProcess::readyReadStandardOutput, this, [this, id] {
    m_buf += m_proc->readAllStandardOutput();
    if (m_buf.size() > (8 << 20) && m_buf.indexOf('\n') < 0) {   // one JSON event per line: 8 MB without one is broken
      m_buf.clear();
      if (Run* run = findMut(id)) run->error = QCoreApplication::translate("ai", "The trainer sent a line over 8 MB: stopped.");
      m_proc->kill();
      return;
    }
    int nl;
    while ((nl = int(m_buf.indexOf('\n'))) >= 0) {
      const QByteArray line = m_buf.left(nl);
      m_buf.remove(0, nl + 1);
      Run* run = findMut(id);
      if (!run) continue;
      paths::dropLink(run->dir + "/log.txt");
      QFile log(run->dir + "/log.txt");
      if (log.open(QIODevice::Append)) log.write(line + "\n");
      const QJsonObject j = QJsonDocument::fromJson(line).object();
      const QString ev = j.value("event").toString();
      if (ev == "progress") { run->stage = j.value("stage").toString(); run->pct = j.value("pct").toDouble(); }
      else if (ev == "error") run->error = j.value("message").toString();
      emit runChanged(id);
    }
  });
  connect(m_proc, &QProcess::finished, this, [this, id](int code, QProcess::ExitStatus st) {
    m_proc->deleteLater();
    m_proc = nullptr;
    m_current.clear();
    Run* run = findMut(id);
    if (!run) { emit listChanged(); return; }
    if (run->state == RunState::Stopped) {}
    else if (code == 0 && st == QProcess::NormalExit) {
      run->result = readJson(runFile(*run, "result.json"));
      run->state = run->result.isEmpty() ? RunState::Failed : RunState::Done;
      if (run->result.isEmpty()) run->error = QCoreApplication::translate("ai", "The trainer gave no result.");
      run->pct = 100;
    } else {
      run->state = RunState::Failed;
      if (run->error.isEmpty()) {
        QFile e(run->dir + "/stderr.txt");
        const QString tail = e.open(QIODevice::ReadOnly) ? QString::fromUtf8(e.readAll()).trimmed().section('\n', -2) : QString();
        run->error = QCoreApplication::translate("ai", "The trainer stopped (code %1).").arg(code) + (tail.isEmpty() ? QString() : "\n" + tail);
      }
    }
    saveMeta(*run);
    emit runChanged(id);
    emit listChanged();
  });
  connect(m_proc, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError e) {
    if (e != QProcess::FailedToStart || !m_proc) return;
    if (Run* run = findMut(id)) { run->state = RunState::Failed; run->error = QCoreApplication::translate("ai", "The trainer could not start."); saveMeta(*run); }
    m_proc->deleteLater(); m_proc = nullptr; m_current.clear();
    emit listChanged();
  });
  QStringList args{"-p", sandboxProfile(r.work, script, data), Environment::instance().python(), "-I", script, r.work};
  if (!data.isEmpty()) args << data;   // train.py reads it instead of <work>/data.csv
  m_proc->start("/usr/bin/sandbox-exec", args);
  // an AI's training: a wall-clock limit (a looping model would otherwise keep the one trainer for itself)
  if (!by.isEmpty())
    QTimer::singleShot(AI_RUN_LIMIT_MS, this, [this, id] {
      if (!m_proc || m_current != id) return;
      if (Run* run = findMut(id)) {
        run->state = RunState::Stopped;
        run->error = QCoreApplication::translate("ai", "Stopped: a training started by an AI may run %1 minutes at most.").arg(AI_RUN_LIMIT_MS / 60000);
        saveMeta(*run);
        emit runChanged(id);
      }
      m_proc->kill();
    });
  return r.id;
}

void Runs::stop() {
  if (!m_proc) return;
  if (Run* run = findMut(m_current)) { run->state = RunState::Stopped; run->error = QCoreApplication::translate("ai", "Stopped by you."); saveMeta(*run); }
  m_proc->kill();
}

void Runs::remove(const QString& id) {
  if (id == m_current) return;
  for (int i = 0; i < m_runs.size(); ++i) {
    if (m_runs[i].id != id) continue;
    // only folders under runsDir(), named by the app
    const QString dir = QFileInfo(m_runs[i].dir).canonicalFilePath();
    if (!dir.isEmpty() && dir.startsWith(QFileInfo(runsDir()).canonicalFilePath() + "/")) QDir(dir).removeRecursively();
    m_runs.removeAt(i);
    emit listChanged();
    return;
  }
}
}  // namespace ml
