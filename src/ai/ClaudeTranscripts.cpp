#include "ai/ClaudeTranscripts.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUuid>

namespace ai::claudecode {
namespace {
QString slug(const QString& path) {
  QString s = path;
  for (QChar& c : s) if (!(c.isLetterOrNumber() && c.unicode() < 128)) c = '-';
  return s;
}
// the "cwd" a transcript records (its first lines may be summaries without one); empty when none is found
QString recordedCwd(const QString& file) {
  QFile f(file);
  if (!f.open(QIODevice::ReadOnly)) return {};
  for (int n = 0; n < 50 && !f.atEnd(); ++n) {
    const QByteArray line = f.readLine(4 << 20);
    const QJsonObject o = QJsonDocument::fromJson(line).object();
    if (o.contains("cwd")) return o.value("cwd").toString();
  }
  return {};
}
}  // namespace

int clearTranscripts(const QString& envFolder, const QString& configDir) {
  QString config = configDir;
  if (config.isEmpty()) config = qEnvironmentVariable("CLAUDE_CONFIG_DIR");
  if (config.isEmpty()) config = QDir::homePath() + "/.claude";
  const QFileInfo projectsInfo(config + "/projects");
  if (!projectsInfo.isDir() || projectsInfo.isSymLink()) return 0;
  const QString projects = projectsInfo.canonicalFilePath();
  if (projects.isEmpty()) return 0;

  // the paths Claude Code may have recorded for the folder: as given and with symbolic links resolved (/tmp → /private/tmp)
  QStringList cwds;
  for (const QString& p : {QFileInfo(envFolder).absoluteFilePath(), QFileInfo(envFolder).canonicalFilePath()})
    if (!p.isEmpty() && !cwds.contains(p)) cwds << QDir::cleanPath(p);
  int removed = 0;
  QStringList done;
  for (const QString& cwd : cwds) {
    const QString name = slug(cwd);
    if (name.size() > 200 || name.count('-') == name.size() || done.contains(name)) continue;   // hashed, or nothing left
    done << name;
    const QFileInfo dirInfo(projects + "/" + name);
    if (!dirInfo.isDir() || dirInfo.isSymLink()) continue;
    if (QFileInfo(dirInfo.canonicalFilePath()).absolutePath() != projects) continue;   // directly under projects/, nowhere else
    QDir dir(dirInfo.absoluteFilePath());
    for (const QFileInfo& f : dir.entryInfoList({"*.jsonl"}, QDir::Files | QDir::Hidden | QDir::NoSymLinks)) {
      if (f.isSymLink() || !cwds.contains(QDir::cleanPath(recordedCwd(f.absoluteFilePath())))) continue;
      if (!QFile::remove(f.absoluteFilePath())) continue;
      ++removed;
      // the session's side folder (tool results, sub-agents), named after its id
      const QString id = f.completeBaseName();
      const QFileInfo side(dir.absoluteFilePath(id));
      if (!QUuid::fromString(id).isNull() && side.isDir() && !side.isSymLink()) QDir(side.absoluteFilePath()).removeRecursively();
    }
    if (dir.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) QDir(projects).rmdir(name);
  }
  return removed;
}
}  // namespace ai::claudecode
