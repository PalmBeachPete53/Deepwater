#include "ai/ClaudeCode.h"
#include "core/Settings.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <memory>

namespace ai::claudecode {

bool isKey(const QString& repo, const QString& file) { return repo == kRepo && file == kFile; }

QProcessEnvironment environment() {
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QString home = QDir::homePath();
  QStringList path = env.value("PATH").split(':', Qt::SkipEmptyParts);
  for (const QString& d : {home + "/.local/bin", home + "/.claude/local", QString("/opt/homebrew/bin"), QString("/usr/local/bin"),
                           QString("/usr/bin"), QString("/bin")})
    if (!path.contains(d)) path << d;
  env.insert("PATH", path.join(':'));
  return env;
}

QString binary() {
  const QString home = QDir::homePath();
  for (const QString& p : {home + "/.local/bin/claude", home + "/.claude/local/claude", QString("/opt/homebrew/bin/claude"),
                           QString("/usr/local/bin/claude")}) {
    const QFileInfo f(p);
    if (f.exists() && f.isExecutable()) return f.absoluteFilePath();
  }
  return QStandardPaths::findExecutable("claude", environment().value("PATH").split(':', Qt::SkipEmptyParts));
}

Probe& Probe::instance() { static Probe* p = new Probe; return *p; }

void Probe::refresh() {
  const int run = ++m_run;
  m_status.checking = true;
  m_status.path = binary();
  emit changed();
  if (m_status.path.isEmpty()) {
    m_status = Status{true, false, {}, {}, false, {}, {}};
    emit changed();
    return;
  }
  // `claude --version`, then `claude auth status` (JSON); each with its own time limit
  auto runOne = [this](const QStringList& args, std::function<void(int, const QByteArray&)> done) {
    auto* p = new QProcess(this);
    p->setProcessEnvironment(environment());
    p->setProgram(m_status.path);
    p->setArguments(args);
    auto fired = std::make_shared<bool>(false);
    connect(p, &QProcess::finished, this, [p, done, fired](int code, QProcess::ExitStatus) {
      if (*fired) return; *fired = true;
      done(code, p->readAllStandardOutput());
      p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, this, [p, done, fired](QProcess::ProcessError e) {
      if (e != QProcess::FailedToStart || *fired) return; *fired = true;
      done(-1, {});
      p->deleteLater();
    });
    QTimer::singleShot(15000, p, [p] { p->kill(); });
    p->start();
  };
  runOne({"--version"}, [this, run, runOne](int code, const QByteArray& out) {
    if (run != m_run) return;
    if (code != 0) {
      m_status.probed = true; m_status.checking = false;
      m_status.error = QStringLiteral("claude --version failed");
      emit changed();
      return;
    }
    m_status.version = QString::fromUtf8(out).trimmed().section(' ', 0, 0);
    runOne({"auth", "status"}, [this, run](int, const QByteArray& out) {
      if (run != m_run) return;
      const QJsonObject j = QJsonDocument::fromJson(out).object();
      m_status.loggedIn = j.value("loggedIn").toBool();
      const QString m = j.value("authMethod").toString();
      m_status.authMethod = m;
      m_status.error.clear();
      m_status.probed = true; m_status.checking = false;
      emit changed();
    });
  });
}

QStringList modelAliases() { return {"", "opus", "sonnet", "fable", "haiku"}; }
QStringList effortLevels() { return {"", "low", "medium", "high", "xhigh", "max"}; }
QString model() { const QString m = settings::value("ai.claude.model").toString(); return modelAliases().contains(m) ? m : QString(); }
QString effort() { const QString e = settings::value("ai.claude.effort").toString(); return effortLevels().contains(e) ? e : QString(); }
void setModel(const QString& alias) { settings::setValue("ai.claude.model", alias); }
void setEffort(const QString& level) { settings::setValue("ai.claude.effort", level); }
}  // namespace ai::claudecode
