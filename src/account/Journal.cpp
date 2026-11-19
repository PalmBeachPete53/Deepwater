#include "account/Journal.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QSet>
#include <QHash>
#include <algorithm>

namespace account {

double Trade::rMultiple() const {
  if (!std::isfinite(sl) || !std::isfinite(entry) || !std::isfinite(exit)) return NAN;
  const double risk = side == Side::Long ? entry - sl : sl - entry;
  if (risk <= 0) return NAN;   // stop at or beyond entry (moved to breakeven / in profit): no initial risk known
  const double move = side == Side::Long ? exit - entry : entry - exit;
  return move / risk;
}

QString sideName(Side s) { return s == Side::Long ? "LONG" : "SHORT"; }
QString newId(const QString& prefix) { return prefix + ":" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12); }

namespace {
const char* kCurrentKey = "account.track-record.account";

QJsonValue num(double v) { return std::isfinite(v) ? QJsonValue(v) : QJsonValue(); }
double num(const QJsonValue& v) { return v.isDouble() ? v.toDouble() : NAN; }
QString iso(const QDateTime& t) { return t.isValid() ? t.toUTC().toString(Qt::ISODate) : QString(); }
QDateTime fromIso(const QString& s) { auto t = QDateTime::fromString(s, Qt::ISODate); if (t.isValid()) t = t.toUTC(); return t; }

QJsonObject toJson(const Trade& t) {
  QJsonObject o;
  o["id"] = t.id; o["symbol"] = t.symbol; o["side"] = t.side == Side::Long ? "long" : "short";
  o["open"] = iso(t.open); o["close"] = iso(t.close);
  o["entry"] = num(t.entry); o["exit"] = num(t.exit); o["lots"] = num(t.lots);
  o["sl"] = num(t.sl); o["tp"] = num(t.tp); o["pnl"] = t.pnl; o["commission"] = t.commission; o["swap"] = t.swap;
  if (!t.strategy.isEmpty()) o["strategy"] = t.strategy;
  if (!t.comment.isEmpty()) o["comment"] = t.comment;
  o["source"] = t.source;
  if (!t.ticket.isEmpty()) o["ticket"] = t.ticket;
  return o;
}
Trade tradeFrom(const QJsonObject& o) {
  Trade t;
  t.id = o["id"].toString(); t.symbol = o["symbol"].toString();
  t.side = o["side"].toString().toLower() == "short" ? Side::Short : Side::Long;
  t.open = fromIso(o["open"].toString()); t.close = fromIso(o["close"].toString());
  t.entry = num(o["entry"]); t.exit = num(o["exit"]); t.lots = num(o["lots"]);
  t.sl = num(o["sl"]); t.tp = num(o["tp"]);
  t.pnl = o["pnl"].toDouble(); t.commission = o["commission"].toDouble(); t.swap = o["swap"].toDouble();
  t.strategy = o["strategy"].toString(); t.comment = o["comment"].toString();
  t.source = o["source"].toString(); t.ticket = o["ticket"].toString();
  if (t.id.isEmpty()) t.id = newId(t.source.isEmpty() ? "trade" : t.source);
  if (!t.close.isValid() && t.open.isValid()) t.close = t.open;   // hand-edited file without a close time (A2)
  return t;
}
// "mt4#12345" (bridge-mt4 and mt4 share tickets), or the id without a ticket
QString tradeKey(const Trade& t) { return t.ticket.isEmpty() ? t.id : t.source.section('-', -1) + "#" + t.ticket; }
void sortTrades(QList<Trade>& l) {
  std::stable_sort(l.begin(), l.end(), [](const Trade& a, const Trade& b) { return a.close < b.close; });
}
}

Journal& Journal::instance() { static Journal* j = new Journal; return *j; }

Journal::Journal() {
  load();
  m_current = settings::value(kCurrentKey).toString();
  if (!account(m_current)) m_current = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
}

QString Journal::filePath() const { return paths::data("account") + "/journal.json"; }

void Journal::setError(const QString& e) {
  if (e == m_error) return;
  m_error = e;
  emit errorChanged();
}

// A journal that cannot be read is never overwritten (A10): it is copied aside first (journal.json.bad-<time>),
// and when even that fails nothing is written until the user fixes or moves the file.
void Journal::load() {
  const QString path = filePath();
  QFile f(path);
  if (!f.exists()) return;
  QString problem;
  QJsonDocument doc;
  if (!f.open(QIODevice::ReadOnly)) problem = f.errorString();
  else {
    QJsonParseError pe;
    doc = QJsonDocument::fromJson(f.readAll(), &pe);
    f.close();
    if (pe.error != QJsonParseError::NoError) problem = pe.errorString() + QString(" @%1").arg(pe.offset);
    else if (!doc.isObject() || (doc.object().contains("accounts") && !doc.object()["accounts"].isArray()))
      problem = QCoreApplication::translate("account", "not a Deepwater journal");
  }
  if (!problem.isEmpty()) {
    const QString bad = path + ".bad-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    if (QFile::copy(path, bad))
      setError(QCoreApplication::translate("account", "The trading journal could not be read (%1). It was kept as %2 and the journal starts empty.")
                 .arg(problem, QDir::toNativeSeparators(bad)));
    else {
      m_readOnly = true;
      setError(QCoreApplication::translate("account", "The trading journal could not be read (%1) nor copied aside. Deepwater will not write it "
                                           "until the file is fixed or moved: %2").arg(problem, QDir::toNativeSeparators(path)));
    }
    return;
  }
  for (const auto& v : doc.object()["ignoredBridgeFiles"].toArray()) if (!v.toString().isEmpty()) m_ignoredBridgeFiles << v.toString();
  for (const auto& av : doc.object()["accounts"].toArray()) {
    const auto o = av.toObject();
    Account a;
    a.id = o["id"].toString(); a.name = o["name"].toString(); a.currency = o["currency"].toString("USD");
    a.balance = o["balance"].toDouble(); a.platform = o["platform"].toString(); a.broker = o["broker"].toString();
    a.login = o["login"].toString(); a.demo = o["demo"].toBool(); a.lastImport = fromIso(o["lastImport"].toString());
    a.bridgeFile = o["bridgeFile"].toString(); a.balanceByUser = o["balanceByUser"].toBool();
    for (const auto& v : o["removed"].toArray()) a.removed << v.toString();
    for (const auto& tv : o["trades"].toArray()) a.trades << tradeFrom(tv.toObject());
    sortTrades(a.trades);
    if (!a.id.isEmpty()) m_accounts << a;
  }
}

bool Journal::save() {
  if (m_readOnly) return false;                         // the unreadable file is still there (error already shown)
  QJsonArray arr;
  for (const auto& a : m_accounts) {
    QJsonObject o;
    o["id"] = a.id; o["name"] = a.name; o["currency"] = a.currency; o["balance"] = a.balance;
    o["platform"] = a.platform; o["broker"] = a.broker; o["login"] = a.login; o["demo"] = a.demo;
    o["lastImport"] = iso(a.lastImport); o["bridgeFile"] = a.bridgeFile;
    if (a.balanceByUser) o["balanceByUser"] = true;
    if (!a.removed.isEmpty()) o["removed"] = QJsonArray::fromStringList(a.removed);
    QJsonArray ts; for (const auto& t : a.trades) ts << toJson(t);
    o["trades"] = ts;
    arr << o;
  }
  QJsonObject root; root["version"] = 1; root["accounts"] = arr;
  if (!m_ignoredBridgeFiles.isEmpty()) root["ignoredBridgeFiles"] = QJsonArray::fromStringList(m_ignoredBridgeFiles);
  QSaveFile f(filePath());
  const auto failed = [&] {
    setError(QCoreApplication::translate("account", "The trading journal could not be saved (%1): the last change is only in memory.")
               .arg(f.errorString()));
    m_saveError = true;
    return false;
  };
  if (!f.open(QIODevice::WriteOnly)) return failed();
  const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (f.write(data) != data.size()) { f.cancelWriting(); return failed(); }
  if (!f.commit()) return failed();
  if (m_saveError) { m_saveError = false; setError(QString()); }   // a load problem stays visible until shown
  return true;
}

Account* Journal::find(const QString& id) {
  for (auto& a : m_accounts) if (a.id == id) return &a;
  return nullptr;
}
const Account* Journal::account(const QString& id) const {
  for (const auto& a : m_accounts) if (a.id == id) return &a;
  return nullptr;
}
const Account* Journal::current() const { return account(m_current); }

void Journal::setCurrent(const QString& id) {
  if (id == m_current || !account(id)) return;
  m_current = id;
  settings::setValue(kCurrentKey, id);
  emit changed();
}

QString Journal::addAccount(Account a) {
  if (a.id.isEmpty()) a.id = newId("acct");
  sortTrades(a.trades);
  m_accounts << a;
  m_current = a.id;
  settings::setValue(kCurrentKey, a.id);
  save();
  emit changed();
  return a.id;
}

void Journal::updateAccount(const Account& u) {
  Account* a = find(u.id);
  if (!a) return;
  a->name = u.name; a->currency = u.currency; a->balance = u.balance; a->platform = u.platform;
  a->broker = u.broker; a->login = u.login; a->bridgeFile = u.bridgeFile; a->balanceByUser = u.balanceByUser;
  save();
  emit changed();
}

void Journal::removeAccount(const QString& id) {
  // a live-sync account would be created again from its file at the next sync: remember the file (A11)
  if (const Account* a = account(id); a && !a->bridgeFile.isEmpty() && !m_ignoredBridgeFiles.contains(a->bridgeFile))
    m_ignoredBridgeFiles << a->bridgeFile;
  m_accounts.erase(std::remove_if(m_accounts.begin(), m_accounts.end(), [&](const Account& a) { return a.id == id; }),
                   m_accounts.end());
  if (m_current == id) {
    m_current = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
    settings::setValue(kCurrentKey, m_current);
  }
  save();
  emit changed();
}

void Journal::resumeBridgeFiles() {
  if (m_ignoredBridgeFiles.isEmpty()) return;
  m_ignoredBridgeFiles.clear();
  save();
}

int Journal::addTrades(const QString& accountId, const QList<Trade>& trades, bool markImport, int fixShiftSecs, int* timesFixed) {
  if (timesFixed) *timesFixed = 0;
  Account* a = find(accountId);
  if (!a) return 0;
  QHash<QString, int> byId, byTicket;                 // → index in a->trades (appends below keep them valid)
  for (int i = 0; i < a->trades.size(); ++i) {
    const auto& t = a->trades[i];
    byId.insert(t.id, i);
    if (!t.ticket.isEmpty()) byTicket.insert(tradeKey(t), i);
  }
  const QSet<QString> removed(a->removed.begin(), a->removed.end());
  int added = 0, fixed = 0;
  bool touched = false;
  for (const auto& t : trades) {
    const QString tk = tradeKey(t);                    // bridge-mt4 and mt4 share tickets
    int at = byId.value(t.id, -1);
    if (at < 0 && !t.ticket.isEmpty()) at = byTicket.value(tk, -1);
    if (at < 0 && !t.legacyId.isEmpty() && (at = byId.value(t.legacyId, -1)) >= 0) {
      // a trade imported by an older version (id from the converted times): it takes the new, offset-free id
      byId.remove(t.legacyId);
      a->trades[at].id = t.id; byId.insert(t.id, at);
      touched = true;
    }
    if (at >= 0) {
      Trade& x = a->trades[at];
      auto shift = [](const QDateTime& a, const QDateTime& b) { return a.isValid() && b.isValid() ? std::abs(a.secsTo(b)) : qint64(1) << 40; };
      const qint64 moved = std::max(shift(x.open, t.open), shift(x.close, t.close));
      if (fixShiftSecs >= 0 && x.source == t.source && t.close.isValid() && (x.open != t.open || x.close != t.close) &&
          moved > fixShiftSecs) {
        x.open = t.open; x.close = t.close; ++fixed; touched = true;
      }
      continue;
    }
    if (t.source.startsWith("bridge-") && removed.contains(tk)) continue;   // deleted by the user (A11)
    Trade n = t; n.legacyId.clear();
    a->trades << n; byId.insert(n.id, int(a->trades.size()) - 1);
    if (!n.ticket.isEmpty()) byTicket.insert(tk, int(a->trades.size()) - 1);
    ++added;
  }
  if (timesFixed) *timesFixed = fixed;
  if (markImport) a->lastImport = QDateTime::currentDateTimeUtc();
  if (added || fixed) sortTrades(a->trades);
  if (added || touched || markImport) { save(); emit changed(); }
  return added;
}

void Journal::updateTrade(const QString& accountId, const Trade& t) {
  Account* a = find(accountId);
  if (!a) return;
  for (auto& x : a->trades) if (x.id == t.id) { x = t; sortTrades(a->trades); save(); emit changed(); return; }
}

void Journal::removeTrade(const QString& accountId, const QString& tradeId) {
  Account* a = find(accountId);
  if (!a) return;
  const auto n = a->trades.size();
  for (const auto& t : a->trades)
    if (t.id == tradeId && t.source.startsWith("bridge-") && !a->removed.contains(tradeKey(t))) a->removed << tradeKey(t);
  a->trades.erase(std::remove_if(a->trades.begin(), a->trades.end(), [&](const Trade& t) { return t.id == tradeId; }),
                  a->trades.end());
  if (a->trades.size() != n) { save(); emit changed(); }
}

QString Journal::addDemoAccount() {
  for (const auto& a : m_accounts) if (a.demo) { setCurrent(a.id); return a.id; }
  QFile f(":/account/demo-trades.json");
  if (!f.open(QIODevice::ReadOnly)) return {};
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  Account a;
  a.name = demoName(); a.demo = true; a.platform = "Demo";
  a.currency = o["currency"].toString("USD"); a.balance = o["balance"].toDouble(10000);
  a.lastImport = QDateTime::currentDateTimeUtc();
  for (const auto& tv : o["trades"].toArray()) a.trades << tradeFrom(tv.toObject());
  a.id = "demo";
  return addAccount(a);
}
}
