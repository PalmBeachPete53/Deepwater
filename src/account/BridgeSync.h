#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QHash>
#include <QDateTime>

// Live, read-only sync with MT4 / MT5 / cTrader. A small EA / cBot (sources in resources/account/bridge)
// runs in the user's terminal and rewrites "Deepwater_<platform>_<login>.csv" in a folder the app watches
// (MetaTrader: the terminals' Common/Files folder; cTrader: Documents/Deepwater). No password, no broker API:
// the file only lists closed trades. Each file feeds one journal account (created on first sight).
namespace account {

class BridgeSync : public QObject {
  Q_OBJECT
public:
  static BridgeSync& instance();
  QString folder() const { return m_folder; }
  void setFolder(const QString& dir);          // empty: stop watching (saved under account.bridge.folder)
  QDateTime lastSync() const { return m_last; }
  QString status() const;                      // translated when read (the singleton outlives a language change)
  // Folders where the terminals usually keep Common/Files (existing ones only).
  static QStringList candidateFolders();
  // Writes the EA / cBot sources into dir; returns the files written.
  static QStringList exportBridgeSources(const QString& dir);
signals:
  void synced(int added);
  void statusChanged();
private:
  BridgeSync();
  void scan();
  void readFile(const QString& path);
  QFileSystemWatcher m_watch;
  QTimer m_debounce;
  QString m_folder, m_statusArg;
  enum class St { Off, NotFound, Watching, Synced } m_st = St::Off;
  int m_added = 0;
  QDateTime m_last;
  QHash<QString, QDateTime> m_seen;           // file → last modified read
};
}
