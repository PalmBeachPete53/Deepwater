#include "account/BridgeSync.h"
#include <QCoreApplication>
#include "account/Importers.h"
#include "account/Journal.h"
#include "core/Settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace account {

QString BridgeSync::status() const {
  switch (m_st) {
    case St::Off: return QCoreApplication::translate("account", "Live sync off");
    case St::NotFound: return QCoreApplication::translate("account", "Folder not found: %1").arg(m_statusArg);
    case St::Watching: return QCoreApplication::translate("account", "Watching %1").arg(m_statusArg);
    case St::Synced: return (m_added == 1 ? QCoreApplication::translate("account", "Synced %1 · %2 new trade") : QCoreApplication::translate("account", "Synced %1 · %2 new trades")).arg(m_statusArg).arg(m_added);
  }
  return {};
}

BridgeSync& BridgeSync::instance() { static BridgeSync* b = new BridgeSync; return *b; }

BridgeSync::BridgeSync() {
  m_debounce.setSingleShot(true);
  m_debounce.setInterval(700);                  // the EA rewrites the file: wait until it is complete
  connect(&m_debounce, &QTimer::timeout, this, &BridgeSync::scan);
  // not restarted by later changes (A9): another program writing to Common/Files more often than every 700 ms
  // would otherwise postpone the scan for ever. The bridges replace their file in one move (tmp → csv).
  const auto kick = [this] { if (!m_debounce.isActive()) m_debounce.start(); };
  connect(&m_watch, &QFileSystemWatcher::directoryChanged, this, kick);
  connect(&m_watch, &QFileSystemWatcher::fileChanged, this, kick);
  const QString saved = settings::value("account.bridge.folder").toString();
  if (!saved.isEmpty()) setFolder(saved);
}

void BridgeSync::setFolder(const QString& dir) {
  if (!m_watch.directories().isEmpty()) m_watch.removePaths(m_watch.directories());
  if (!m_watch.files().isEmpty()) m_watch.removePaths(m_watch.files());
  m_folder = dir;
  settings::setValue("account.bridge.folder", dir);
  m_seen.clear();
  if (dir.isEmpty()) { m_st = St::Off; emit statusChanged(); return; }
  if (!QFileInfo(dir).isDir()) { m_st = St::NotFound; m_statusArg = dir; emit statusChanged(); return; }
  m_watch.addPath(dir);
  m_st = St::Watching; m_statusArg = QDir::toNativeSeparators(dir);
  scan();
  emit statusChanged();
}

void BridgeSync::scan() {
  if (m_folder.isEmpty()) return;
  QDir d(m_folder);
  const auto files = d.entryInfoList({"Deepwater_*.csv"}, QDir::Files);
  for (const auto& fi : files) {
    if (!m_watch.files().contains(fi.absoluteFilePath())) m_watch.addPath(fi.absoluteFilePath());
    if (m_seen.value(fi.absoluteFilePath()) == fi.lastModified()) continue;
    m_seen.insert(fi.absoluteFilePath(), fi.lastModified());
    readFile(fi.absoluteFilePath());
  }
}

void BridgeSync::readFile(const QString& path) {
  // any program can write the watched folder: a regular file only (no link, no FIFO that would block the GUI thread),
  // and the 64 MB cap importFile has (a bridge file is a few hundred KB)
  const QFileInfo fi(path);
  if (fi.isSymLink() || !fi.isFile() || fi.size() > (64LL << 20)) return;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return;
  const ImportResult r = importData(f.read(64LL << 20), QFileInfo(path).fileName());
  if (r.format != "Deepwater bridge" || r.login.isEmpty()) return;
  auto& j = Journal::instance();
  if (j.bridgeFileIgnored(QFileInfo(path).fileName())) return;   // its account was removed by the user (A11)
  QString id;
  for (const auto& a : j.accounts()) if (a.bridgeFile == QFileInfo(path).fileName()) id = a.id;
  if (id.isEmpty()) {
    Account a;
    a.name = QString("%1 %2").arg(r.platform.isEmpty() ? "Bridge" : r.platform, r.login);
    if (!r.broker.isEmpty()) a.name += " · " + r.broker;
    a.platform = r.platform; a.login = r.login; a.broker = r.broker;
    a.currency = r.currency.isEmpty() ? "USD" : r.currency;
    a.balance = std::isfinite(r.deposits) && r.deposits > 0 ? r.deposits : 0;   // 0: unknown, % shown as "—"
    a.bridgeFile = QFileInfo(path).fileName();
    const QString cur = j.currentId();
    id = j.addAccount(a);
    if (!cur.isEmpty()) j.setCurrent(cur);      // a new bridge account does not steal the selection
  } else if (const Account* cur = j.account(id); cur && !cur->balanceByUser && std::isfinite(r.deposits) && r.deposits > 0 &&
             cur->balance != r.deposits) {
    // the file's deposits follow new deposits, but never replace a balance the user typed, nor become 0 / negative
    // (MT4 sees only the history shown in its tab: often no deposit at all) (A11)
    Account a = *cur;
    a.balance = r.deposits;
    j.updateAccount(a);
  }
  // a file whose times were converted with a verified offset also corrects trades synced earlier with a wrong
  // one (A1: older MT4 EAs attached on a weekend saved a stale offset, hours off)
  const int added = j.addTrades(id, r.trades, true, r.timesVerified ? 90 * 60 : -1);
  m_last = QDateTime::currentDateTimeUtc();
  m_st = St::Synced; m_statusArg = QFileInfo(path).fileName(); m_added = added;
  emit synced(added);
  emit statusChanged();
}

QStringList BridgeSync::candidateFolders() {
  QStringList out;
  const QString home = QDir::homePath();
#ifdef Q_OS_WIN
  const QString appData = qEnvironmentVariable("APPDATA");
  out << appData + "/MetaQuotes/Terminal/Common/Files";
#else
  // MetaTrader for macOS runs in Wine: one prefix per terminal build.
  for (const char* app : {"net.metaquotes.wine.metatrader5", "net.metaquotes.wine.metatrader4", "com.metaquotes.metatrader5"}) {
    const QString base = home + "/Library/Application Support/" + app + "/drive_c/users";
    QDir users(base);
    for (const auto& u : users.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
      out << base + "/" + u + "/AppData/Roaming/MetaQuotes/Terminal/Common/Files";
  }
#endif
  out << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Deepwater";
  QStringList existing;
  for (const auto& p : out) if (QFileInfo(p).isDir()) existing << QDir::cleanPath(p);
  return existing;
}

QStringList BridgeSync::exportBridgeSources(const QString& dir) {
  QStringList written;
  QDir().mkpath(dir);
  for (const char* n : {"DeepwaterBridge.mq4", "DeepwaterBridge.mq5", "DeepwaterBridge.cs", "README.txt"}) {
    QFile src(QString(":/account/bridge/") + n);
    if (!src.open(QIODevice::ReadOnly)) continue;
    const QString dst = dir + "/" + n;
    QFile::remove(dst);
    QFile out(dst);
    if (out.open(QIODevice::WriteOnly)) { out.write(src.readAll()); written << dst; }
  }
  return written;
}
}
