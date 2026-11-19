#include "core/Paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
QString ensure(const QString& base, const QString& sub) {
  const QString p = sub.isEmpty() ? base : base + "/" + sub;
  QDir().mkpath(p);
  return p;
}
QString root() {
  const QByteArray prof = qgetenv("DW_PROFILE");
  if (!prof.isEmpty()) return QString::fromUtf8(prof) + "/data";
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
}
namespace paths {
QString data(const QString& sub) { return ensure(root(), sub); }

bool moveDatabase(const QString& from, const QString& to) {
  if (QFileInfo::exists(to) || !QFileInfo::exists(from)) return true;
  QDir().mkpath(QFileInfo(to).absolutePath());
  // the journal files first: a database renamed without its WAL would lose the last writes; on a failure, back as it was
  QStringList moved;
  for (const char* ext : {"-wal", "-shm", ""}) {
    const QString a = from + ext, b = to + ext;
    if (!QFileInfo::exists(a)) continue;
    if (!QFile::rename(a, b)) {
      qWarning("[paths] cannot move %s to %s", qPrintable(a), qPrintable(b));
      for (const QString& ext2 : moved) QFile::rename(to + ext2, from + ext2);
      return false;
    }
    moved << QString::fromLatin1(ext);
  }
  qInfo("[paths] moved %s to %s", qPrintable(from), qPrintable(to));
  QDir().rmdir(QFileInfo(from).absolutePath());   // only when nothing else is left in it
  return true;
}
QString cache(const QString& sub) {
  const QByteArray c = qgetenv("DW_CACHE_DIR");
  return ensure(c.isEmpty() ? root() + "/cache" : QString::fromUtf8(c), sub);
}
bool insideNoLink(const QString& folder, const QString& path) {
  const QString root = QFileInfo(folder).canonicalFilePath();
  if (root.isEmpty()) return false;
  const QFileInfo fi(path);
  if (fi.isSymLink()) return false;
  const QString real = fi.exists() ? fi.canonicalFilePath() : QFileInfo(fi.absolutePath()).canonicalFilePath() + "/" + fi.fileName();
  return real == root || real.startsWith(root + "/");
}
void dropLink(const QString& path) {
  if (QFileInfo(path).isSymLink()) QFile::remove(path);   // removes the link itself, not its target
}
}
