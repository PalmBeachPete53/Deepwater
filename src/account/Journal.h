#pragma once
#include <QObject>
#include <QDateTime>
#include <QList>
#include <QString>
#include <cmath>

// The trading journal: accounts and their closed trades, stored as JSON under paths::data("account")
// (journal.json, written atomically with QSaveFile). One account is selected at a time (the Track Record
// toolbar's account box). Trades come from statement imports, the live bridge (EA / cBot files) or the
// LOG TRADE form. A "Demo account · sample trades" exists only when the user adds it explicitly.
namespace account {

enum class Side { Long, Short };

struct Trade {
  QString id;                 // unique in the account ("mt4:12345", "manual:<uuid>", "sample-01")
  QString symbol;             // as the broker writes it ("EURUSD", "XAUUSD.r", "US100"); see displaySymbol()
  Side side = Side::Long;
  QDateTime open, close;      // UTC
  double entry = NAN, exit = NAN, lots = NAN;
  double sl = NAN, tp = NAN;  // NAN when unknown
  double pnl = 0;             // gross profit in the account currency (as the broker reports it)
  double commission = 0, swap = 0;
  QString strategy, comment;
  QString source;             // manual, mt4, mt5, ctrader, csv, bridge-mt4, bridge-mt5, bridge-ctrader, sample
  QString ticket;             // broker ticket / position id when known
  QString legacyId;           // importers only, not stored: the id older versions gave this trade (migrated on import)

  double net() const { return pnl + commission + swap; }
  double rMultiple() const;   // (exit-entry)/(entry-sl), signed by side; NAN without a stop
  double heldHours() const { return open.isValid() && close.isValid() ? open.secsTo(close) / 3600.0 : NAN; }
};

struct Account {
  QString id;
  QString name;
  QString currency = "USD";
  double balance = 0;         // starting balance (sum of deposits); % figures are relative to it (0: unknown)
  bool balanceByUser = false; // set in the account form: the live sync no longer replaces it with the file's deposits
  QString platform;           // MT4, MT5, cTrader, Manual, Demo
  QString broker, login;
  bool demo = false;          // sample data, never real
  QDateTime lastImport;       // UTC
  QString bridgeFile;         // live sync: the EA / cBot file this account follows
  QList<Trade> trades;        // sorted by close time ascending
  QStringList removed;        // bridge trades the user deleted ("mt4#12345"): the live sync does not add them again
};

class Journal : public QObject {
  Q_OBJECT
public:
  static Journal& instance();

  const QList<Account>& accounts() const { return m_accounts; }
  const Account* account(const QString& id) const;
  const Account* current() const;
  QString currentId() const { return m_current; }
  void setCurrent(const QString& id);

  QString addAccount(Account a);                         // returns its id
  void updateAccount(const Account& a);                  // name, currency, balance, platform (keeps trades)
  void removeAccount(const QString& id);              // a live-sync account: its bridge file is ignored from then on
  bool bridgeFileIgnored(const QString& fileName) const { return m_ignoredBridgeFiles.contains(fileName); }
  void resumeBridgeFiles();                           // the user chose the live-sync folder again
  // Adds trades not already in the account (same id, or same ticket+source); returns how many were added.
  // fixShiftSecs >= 0: trades already there from the same source take the open / close times of `trades` when
  // they moved by more than that (a statement imported again with a corrected offset: 0; a bridge file written
  // with a verified offset: 90 min, so that the server's own summer / winter change does not move every older
  // trade by an hour twice a year); *timesFixed counts them. Bridge trades the user deleted are not added again.
  int addTrades(const QString& accountId, const QList<Trade>& trades, bool markImport = true, int fixShiftSecs = -1,
                int* timesFixed = nullptr);
  void updateTrade(const QString& accountId, const Trade& t);
  void removeTrade(const QString& accountId, const QString& tradeId);

  // Adds (or re-selects) the clearly labelled "Demo account · sample trades" with 60 invented trades.
  QString addDemoAccount();
  static QString demoName() { return QStringLiteral("Demo account · sample trades"); }

  QString filePath() const;
  QString lastError() const { return m_error; }        // translated; empty when the last load / save went fine
  bool readOnly() const { return m_readOnly; }          // journal.json could not be read nor kept aside: never written

signals:
  void changed();
  void errorChanged();                                  // lastError() changed (load / save problem)

private:
  Journal();
  void load();
  bool save();
  void setError(const QString& e);
  Account* find(const QString& id);
  QList<Account> m_accounts;
  QString m_current, m_error;
  QStringList m_ignoredBridgeFiles;
  bool m_readOnly = false, m_saveError = false;
};

QString sideName(Side s);                 // "LONG" / "SHORT"
QString newId(const QString& prefix);
}
