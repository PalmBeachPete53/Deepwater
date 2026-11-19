#pragma once
#include <QObject>
#include <QDateTime>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QSet>
#include <cmath>

// Retail positioning from the Myfxbook community outlook API (https://www.myfxbook.com/api), which needs the user's
// own Myfxbook account:
//   login.json?email&password  → session token (the password is sent once and never stored)
//   get-community-outlook.json?session → per symbol: long/short %, volume (lots), positions, average entry prices
//   logout.json?session
// Settings: positioning.myfxbookEmail (prefill), positioning.myfxbookSession (token, cleared on logout / expiry),
// positioning.myfxbookLastRequest (UTC ISO time of the last outlook request: at most one request per 30 minutes).
//
// Every outlook answer is recorded under paths::data("positioning")/myfxbook/outlook-YYYY-MM.jsonl, so 24 h changes
// and history charts come from the app's own recordings (they start with the first snapshot).
//
// Test hook (test profiles only, DW_PROFILE set): DW_MYFXBOOK_FIXTURE=<file.json> replaces the network with a
// recorded answer shaped like Myfxbook's (either one answer, or {"snapshots":[{"time":ISO,"response":{…}}, …]});
// nothing is recorded to disk and the session setting is not read in that mode.
//
//   auto& r = positioning::Retail::instance();
//   connect(&r, &positioning::Retail::changed, this, [&] { r.latest() … });
//   r.setActive(this, true);     // pages: polls (every 30 min at most) only while an active owner exists
namespace positioning {

struct RetailSymbol {
  QString name;                         // Myfxbook symbol, "EURUSD", "XAUUSD", "LTCUSD"
  double longPct = NAN, shortPct = NAN; // as published (volume-based)
  double longVolume = NAN, shortVolume = NAN;     // lots
  double longPositions = NAN, shortPositions = NAN, totalPositions = NAN;
  double avgLong = NAN, avgShort = NAN; // average entry price of each side
  QString display() const;              // "EUR/USD"
  QString group() const;                // "FX", "Commodities", "Crypto", "Indices", "Other"
};

struct RetailSnapshot {
  QDateTime time;                       // UTC, when the answer was received
  QVector<RetailSymbol> symbols;
  bool isEmpty() const { return symbols.isEmpty(); }
  const RetailSymbol* find(const QString& name) const;
};

class Retail : public QObject {
  Q_OBJECT
public:
  enum class State { NotConnected, LoggingIn, Connected, Expired, Error };
  static Retail& instance();
  static constexpr int kMinIntervalSec = 30 * 60;
  static constexpr int kKeepDays = 60;              // recordings kept in memory (read at launch off the GUI thread)

  State state() const { return m_state; }
  QString message() const { return m_message; }     // last error or info, for the UI
  bool isFixture() const { return m_fixture; }
  QString email() const;                            // saved email (prefill)

  void login(const QString& email, const QString& password);
  void logout();
  void refresh();                                   // requests now if 30 min have passed (and connected)
  void setActive(QObject* owner, bool active);      // poll only while some page shows the data

  QDateTime lastRequest() const;                    // UTC
  QDateTime nextAllowed() const;                    // UTC
  const RetailSnapshot& latest() const { return m_latest; }
  // Own recordings, oldest first (all of the last 60 days; empty for a moment at launch: changed() once read).
  const QVector<RetailSnapshot>& recordings() const { return m_history; }
  // Recorded value of one symbol closest to time t (within toleranceSec), or nullptr.
  const RetailSymbol* symbolAt(const QString& name, const QDateTime& t, int toleranceSec, QDateTime* when = nullptr) const;

signals:
  void changed();

private:
  Retail();
  void setState(State s, const QString& msg = {});
  void request();
  void record(const RetailSnapshot& s);
  void loadRecordings();
  void loadFixture(const QString& path);
  static RetailSnapshot parse(const QByteArray& body, QString* error, bool* sessionError);

  State m_state = State::NotConnected;
  QString m_message;
  bool m_fixture = false, m_busy = false;
  RetailSnapshot m_latest;
  QVector<RetailSnapshot> m_history;
  QSet<QObject*> m_active;
  QSet<QObject*> m_watched;                         // owners whose destroyed() is connected
  QTimer m_timer;
};
}
