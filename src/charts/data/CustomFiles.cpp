// Charts tab — port of quant-terminal server.js custom indicator routes (MIT, Copyright (c) 2026 Lewis Jackson).
#include "charts/data/CustomFiles.h"
#include "charts/core/CustomJs.h"
#include "charts/core/PineIndicators.h"
#include "charts/data/Imports.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>

namespace charts::data {

namespace {
QHash<QString, QString>& customErrors() {
  static QHash<QString, QString> e;
  return e;
}
QString dir() {
  const QString d = customDir();
  QDir().mkpath(d);   // fs.mkdirSync(CUSTOM, { recursive: true })
  return d;
}
bool isPine(const QString& file) { return file.endsWith(".pine"); }

// Writes through QSaveFile (temporary file, renamed over the target at commit): "" or the error; on an error the
// target is as it was.
QString saveBytes(const QString& path, const QByteArray& bytes) {
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return f.errorString();
  if (f.write(bytes) != bytes.size()) {
    const QString why = f.errorString();
    f.cancelWriting();
    return why;
  }
  return f.commit() ? QString() : f.errorString();
}

// The problems a save would be refused for, from the candidate code alone. Pine: pine::check (the parse and the
// registration rules). JavaScript: built from a sibling temporary file, so imports of the custom folder resolve as they
// will for the saved file; its name ends in ".tmp", which no listing picks up should a crash leave it behind.
QString checkCandidate(const QString& code, const QString& file) {
  if (isPine(file)) return pine::check(code);
  const QString tmp = dir() + "/." + file + ".check.tmp";
  if (const QString why = saveBytes(tmp, code.toUtf8()); !why.isEmpty()) return why;
  custom::Loaded l;
  custom::build(tmp, &l);
  QFile::remove(tmp);
  return l.error.replace("." + file + ".check.tmp", file);   // messages name the file the user saves
}
QString idOf(const QString& file) {
  if (isPine(file)) return pine::idFor(file);
  QString f = file;
  if (f.endsWith(".js")) f.chop(3);
  return "custom:" + f;
}
}  // namespace

QString customSlug(const QString& name) {
  static const QRegularExpression other("[^a-z0-9]+");
  static const QRegularExpression ends("^-|-$");
  QString s = name.toLower();
  s.replace(other, "-");
  s.replace(ends, "");
  s = s.left(60);
  return s.isEmpty() ? QStringLiteral("indicator") : s;
}

QJsonObject CustomEntry::toJson() const {
  return {{"file", file}, {"id", id}, {"mtime", mtime},
          {"error", error.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(error)}};
}

QVector<CustomEntry> listCustom() {
  QVector<CustomEntry> out;
  const QDir d(dir());
  for (const QFileInfo& fi : d.entryInfoList(QDir::Files, QDir::Name)) {
    if (!fi.fileName().endsWith(".js") && !isPine(fi.fileName())) continue;
    CustomEntry e;
    e.file = fi.fileName();
    e.id = idOf(e.file);
    e.mtime = double(fi.lastModified().toMSecsSinceEpoch());
    e.error = customErrors().value(e.file);
    out.append(e);
  }
  return out;
}

QJsonArray listCustomJson() {
  QJsonArray a;
  for (const auto& e : listCustom()) a.append(e.toJson());
  return a;
}

QString customIdOf(const QString& file) { return idOf(QFileInfo(file).fileName()); }

QString loadCustom(const QString& file) {
  const QString error = isPine(file) ? pine::loadFile(dir() + "/" + file).error : custom::loadFile(dir() + "/" + file).error;
  if (error.isEmpty()) customErrors().remove(file);
  else customErrors()[file] = error;
  return error;
}

void loadAllCustom() {
  for (const auto& e : listCustom()) loadCustom(e.file);
}

void loadAllCustomAsync(QObject* ctx, std::function<void(const QStringList&)> done) {
  QStringList pineFiles, pinePaths;
  for (const auto& e : listCustom()) {
    if (!isPine(e.file)) loadCustom(e.file);
    else {
      pineFiles << e.file;
      pinePaths << dir() + "/" + e.file;
    }
  }
  pine::loadFilesAsync(pinePaths, ctx, [pineFiles, done](const QVector<pine::Loaded>& loaded) {
    for (qsizetype i = 0; i < loaded.size() && i < pineFiles.size(); ++i) {
      if (loaded[i].error.isEmpty()) customErrors().remove(pineFiles[i]);
      else customErrors()[pineFiles[i]] = loaded[i].error;
    }
    if (done) done(pineFiles);
  });
}

QString customError(const QString& file) { return customErrors().value(file); }

QJsonObject CustomSave::toJson() const {
  if (status == 200) return {{"ok", true}, {"file", file}, {"id", id}};
  if (file.isEmpty()) return {{"error", error}};
  return {{"error", error}, {"file", file}};
}

CustomSave saveCustom(const QString& name, const QString& codeIn) {
  static const QRegularExpression exportDefault("export\\s+default");
  static const QRegularExpression moduleExports("module\\.exports\\s*=");
  static const QRegularExpression pine("//\\s*@version|^\\s*(indicator|study|strategy)\\s*\\(",
                                       QRegularExpression::MultilineOption);
  QString code = codeIn;
  // Agents often write CommonJS; it means the same thing, so take it.
  if (!exportDefault.match(code).hasMatch() && moduleExports.match(code).hasMatch()) {
    const auto m = moduleExports.match(code);
    code.replace(m.capturedStart(), m.capturedLength(), "export default");
  }
  CustomSave r;
  // Pine Script (a //@version line or an indicator() / strategy() call, and no `export default`): saved as .pine
  const bool pineCode = !exportDefault.match(code).hasMatch() && pine.match(code).hasMatch();
  if (!pineCode && !exportDefault.match(code).hasMatch()) {
    r.status = 400;
    r.error = QStringLiteral(QT_TRANSLATE_NOOP("charts", "The file must `export default { name, calc(bars, params, ta) { ... } }`. Start from "
                                   "the template (New indicator), or ask your Claude or Codex to write it in the format "
                                   "in docs/custom-indicators.md."));
    return r;
  }
  const QString file = customSlug(name) + (pineCode ? ".pine" : ".js");
  const QString full = dir() + "/" + file;
  auto refuse = [&](const QString& why) {
    r.status = 400;
    r.error = why;
    r.file = file;
    return r;
  };
  // Checked before the user's file is touched (audit D22: it was truncated and written first, then put back from
  // memory if refused; a crash or a full disk in between lost it).
  if (const QString why = checkCandidate(code, file); !why.isEmpty()) return refuse(why);
  QByteArray before;
  bool existed = false;
  {
    QFile f(full);
    if (f.exists() && f.open(QIODevice::ReadOnly)) { before = f.readAll(); existed = true; }
  }
  if (const QString why = saveBytes(full, code.toUtf8()); !why.isEmpty()) return refuse(why);   // the old file is intact
  const QString error = loadCustom(file);
  if (!error.isEmpty()) {
    // Refused at registration (rules the check doesn't cover): put back what was there (or nothing), so a broken save
    // never replaces a working file. The refused code never registered, so the working version is still the one loaded.
    if (!existed) QFile::remove(full);
    else saveBytes(full, before);
    customErrors().remove(file);
    return refuse(error);
  }
  r.file = file;
  r.id = idOf(file);
  return r;
}

void deleteCustom(const QString& file) {
  QFile::remove(dir() + "/" + QFileInfo(file).fileName());
}

QString readCustom(const QString& file) {
  QFile f(dir() + "/" + QFileInfo(file).fileName());
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QString::fromUtf8(f.readAll());
}

QJsonObject importSignal(const QString& rawName, const QString& text) {
  cleanName(rawName);   // the name is checked before the file is read, as the JS
  return importSignal(rawName, parseSignals(text));
}

QJsonObject importSignal(const QString& rawName, const Signals& sig) {
  const QString name = cleanName(rawName);
  dir();
  const SignalFile f = writeSignalIndicator(name, sig, rawName);
  loadCustom(f.file);
  const QString error = customErrors().value(f.file);
  if (!error.isEmpty()) {
    restoreSignalIndicator(f);
    customErrors().remove(f.file);
    throw ImportError(error, 400);
  }
  return signalImportResult(name, f, sig);
}

}  // namespace charts::data
