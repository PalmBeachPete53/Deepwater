#include "ml/Runs.h"
#include "ml/Comparison.h"
#include "ml/Environment.h"
#include "ml/Datasets.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include "core/Process.h"
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

const Run* Runs::withPoints(const QString& id) {
  Run* r = findMut(id);
  if (!r || r->pointsRead) return r;
  r->pointsRead = true;
  paths::dropLink(r->dir + "/log.txt");
  QFile f(r->dir + "/log.txt");
  if (!f.open(QIODevice::ReadOnly) || f.size() > (64 << 20)) return r;
  while (!f.atEnd() && r->points.size() < 5000) {
    const QByteArray line = f.readLine();
    if (!line.contains("\"point\"")) continue;
    const QJsonObject j = QJsonDocument::fromJson(line).object();
    if (j.value("event").toString() != "point") continue;
    r->curve = j.value("curve").toString().left(40);
    r->points << QJsonObject{{"x", j.value("x")}, {"values", j.value("values")}};
  }
  return r;
}

Runs::Runs() {
  load();
  QTimer::singleShot(3000, this, [] { comparison::recordTrials(); });   // runs made before the trial registry, once each
  QTimer::singleShot(20000, this, &Runs::completeReferences);   // older runs' references, quietly, once the app has settled
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
    r.dataPath = meta.value("data_path").toString();
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
                                             {"state", stateKey(r.state)}, {"error", r.error}, {"by", r.by}, {"data_path", r.dataPath}});
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

QString Runs::start(const QJsonObject& jobIn, const QString& datasetPath, const QString& datasetTitle, const QString& name, QString* error,
                    const QString& by) {
  auto fail = [error](const QString& e) { if (error) *error = e; return QString(); };
  QJsonObject job = jobIn;
  // supervised, semi-supervised, self-supervised probes: the predictions are also traded on the test rows after costs
  // (train.py trading_eval). The cost a side,
  // unless given: Binance crypto as the Strategy tester (0.10 % taker fee + 0.05 % slippage), forex a spread of 2 bp,
  // anything else 5 bp
  const QString fam = job.value("family").toString();
  if ((fam == "supervised" || fam == "semi_supervised" || fam == "self_supervised") && !job.contains("cost_bp")) {
    const QString what = (datasetTitle + " " + QFileInfo(datasetPath).fileName()).toLower();
    job["cost_bp"] = what.contains("binance") || what.contains("usdt") ? 15.0
                   : what.contains("dukascopy") || what.contains("forex") || datasetTitle.contains('/') ? 2.0 : 5.0;
  }
  if (m_proc) return fail(QCoreApplication::translate("ai", "A training is already running."));
#ifndef __APPLE__
  // the trainer runs only inside macOS's sandbox (sandbox-exec, below); elsewhere it would start with the user's full
  // rights, which the ML page says it never has: refused with a clear reason rather than "could not start"
  return fail(QCoreApplication::translate("ai", "Trainings run in a sandbox that exists only on macOS for now."));
#endif
  if (!Environment::instance().status().ready()) return fail(QCoreApplication::translate("ai", "The Python environment is not installed."));
  // the trainer script, from the app's resources (overwritten each time: always the app's own version)
  QString script;
  if (!writeScript(&script)) return fail(QCoreApplication::translate("ai", "The trainer script cannot be written."));
  Run r;
  r.created = QDateTime::currentDateTime();
  r.id = r.created.toString("yyyyMMdd-HHmmss") + "-" + QString::number(QRandomGenerator::global()->bounded(0x1000, 0xffff), 16);
  r.dir = runsDir() + "/" + r.id;
  r.work = r.dir + "/work";
  r.name = name; r.dataset = datasetTitle; r.job = job; r.by = by;
  r.stage = QCoreApplication::translate("ai", "starting");
  r.pointsRead = true;   // its points arrive live
  if (!QDir().mkpath(r.work)) return fail(QCoreApplication::translate("ai", "The run folder cannot be created."));
  // one of the app's data sets (a plain file in its folder, no link): read in place, read-only; anything else is copied
  // as before. Each run used to copy its set, hundreds of MB for a million rows, on the window's thread.
  const QFileInfo di(datasetPath);
  const QString dataDir = QFileInfo(paths::data("ml/datasets")).canonicalFilePath();
  QString data = di.isFile() && !di.isSymLink() ? di.canonicalFilePath() : QString();
  if (data.isEmpty() || dataDir.isEmpty() || QFileInfo(data).absolutePath() != dataDir) data.clear();
  if (data.isEmpty() && !QFile::copy(datasetPath, r.work + "/data.csv")) return fail(QCoreApplication::translate("ai", "The data set cannot be copied."));
  r.dataPath = data;
  writeJson(r.work + "/job.json", job);
  saveMeta(r);
  m_runs.prepend(r);
  m_current = r.id;
  m_dataset = data.isEmpty() ? QFileInfo(r.work + "/data.csv").canonicalFilePath() : data;
  emit listChanged();

  m_proc = new dw::Process(this);
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
  connect(m_proc, &dw::Process::readyReadStandardOutput, this, [this, id] {
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
      else if (ev == "point" && run->points.size() < 5000) {
        run->curve = j.value("curve").toString().left(40);
        run->points << QJsonObject{{"x", j.value("x")}, {"values", j.value("values")}};
      }
      else if (ev == "error") run->error = j.value("message").toString();
      emit runChanged(id);
    }
  });
  connect(m_proc, &dw::Process::finished, this, [this, id](int code, dw::Process::ExitStatus st) {
    m_proc->deleteLater();
    m_proc = nullptr;
    m_current.clear();
    Run* run = findMut(id);
    if (!run) { emit listChanged(); return; }
    if (run->state == RunState::Stopped) {}
    else if (code == 0 && st == dw::Process::NormalExit) {
      run->result = readJson(runFile(*run, "result.json"));
      run->state = run->result.isEmpty() ? RunState::Failed : RunState::Done;
      if (run->result.isEmpty()) run->error = QCoreApplication::translate("ai", "The trainer gave no result.");
      else comparison::recordTrial(*run);   // one try in the trial registry, whoever started it
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
  connect(m_proc, &dw::Process::errorOccurred, this, [this, id](dw::Process::ProcessError e) {
    if (e != dw::Process::FailedToStart || !m_proc) return;
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

bool Runs::writeScript(QString* path) const {
  *path = paths::data("ml") + "/train.py";
  QFile src(":/ml/train.py");
  QSaveFile dst(*path);
  if (!src.open(QIODevice::ReadOnly) || !dst.open(QIODevice::WriteOnly)) return false;
  dst.write(src.readAll());
  return dst.commit();
}

void Runs::completeReferences() {
#ifdef __APPLE__
  if (m_refsProc) return;
  auto& env = Environment::instance();
  if (!env.status().probed) {   // the environment is checked asynchronously: once it has answered
    auto* once = new QObject(this);
    connect(&env, &Environment::changed, once, [this, once] { if (Environment::instance().status().probed) { once->deleteLater(); completeReferences(); } });
    env.refresh();
    return;
  }
  if (!env.status().ready()) { qInfo("[ml] references: the Python environment is not ready"); emit referencesDone(m_refsAdded, m_refsRefused); return; }
  for (Run& r : m_runs) {
    const QString fam = r.job.value("family").toString();
    if (r.state != RunState::Done || (fam != "supervised" && fam != "semi_supervised") || r.result.isEmpty()) continue;
    const QString tried = runFile(r, "refs.json");
    if (r.result.contains("persistence") || (!tried.isEmpty() && QFileInfo::exists(tried))) continue;   // done, or tried
    // the data it learned from: its copy, the set it read in place, or (runs before data_path was kept) the set of that title
    QString data;
    if (!QFileInfo::exists(r.work + "/data.csv")) {
      if (!r.dataPath.isEmpty() && QFileInfo(r.dataPath).isFile()) data = QFileInfo(r.dataPath).canonicalFilePath();
      else for (const Dataset& d : listDatasets()) if (d.title == r.dataset) { data = QFileInfo(d.path).canonicalFilePath(); break; }
      const QString dataDir = QFileInfo(paths::data("ml/datasets")).canonicalFilePath();
      if (data.isEmpty() || QFileInfo(data).absolutePath() != dataDir) {
        writeJson(r.work + "/refs.json", QJsonObject{{"ok", false}, {"why", "its data set is no longer on disk"}});
        ++m_refsRefused;
        qInfo("[ml] references: %s refused: its data set is no longer on disk", qPrintable(r.id));
        continue;
      }
    }
    QString script;
    if (!writeScript(&script)) return;
    m_refsProc = new dw::Process(this);
    QProcessEnvironment pe;
    pe.insert("PATH", "/usr/bin:/bin");
    pe.insert("HOME", r.work); pe.insert("TMPDIR", r.work);
    pe.insert("PYTHONDONTWRITEBYTECODE", "1"); pe.insert("PYTHONNOUSERSITE", "1");
    pe.insert("LANG", "en_US.UTF-8"); pe.insert("OMP_NUM_THREADS", "2");
    m_refsProc->setProcessEnvironment(pe);
    m_refsProc->setWorkingDirectory(r.work);
    m_refsProc->setStandardOutputFile(dw::Process::nullDevice());
    m_refsProc->setStandardErrorFile(r.dir + "/refs-stderr.txt");
    const QString id = r.id;
    connect(m_refsProc, &dw::Process::finished, this, [this, id] {
      m_refsProc->deleteLater();
      m_refsProc = nullptr;
      if (Run* run = findMut(id)) {
        run->result = readJson(runFile(*run, "result.json"));
        if (!QFileInfo::exists(runFile(*run, "refs.json")))   // it stopped before saying: not tried again at each launch
          writeJson(run->work + "/refs.json", QJsonObject{{"ok", false}, {"why", "the trainer stopped while adding them"}});
        const QJsonObject said = readJson(runFile(*run, "refs.json"));
        if (said.value("ok").toBool()) ++m_refsAdded; else ++m_refsRefused;
        qInfo("[ml] references: %s %s%s", qPrintable(id), said.value("ok").toBool() ? "added" : "refused: ",
              qPrintable(said.value("why").toString()));
        emit runChanged(id);
      }
      QTimer::singleShot(200, this, &Runs::completeReferences);   // the next one
    });
    QStringList args{"-p", sandboxProfile(r.work, script, data), env.python(), "-I", script, r.work};
    if (!data.isEmpty()) args << data;
    args << "--refs";
    m_refsProc->start("/usr/bin/sandbox-exec", args);
    return;   // one at a time
  }
#endif
  emit referencesDone(m_refsAdded, m_refsRefused);
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
