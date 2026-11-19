// technical-data.sqlite: bars of several markets and timeframes. See TechnicalDataStore.h.
#include "technicaldata/TechnicalDataStore.h"
#include "core/Paths.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "technicaldata/Dukascopy.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>
#include <QAtomicInt>
#include <QHash>
#include <algorithm>
#include <cmath>

namespace technicaldata {

namespace {
void schema(QSqlDatabase& db) {
  QSqlQuery q(db);
  q.exec("PRAGMA journal_mode=WAL");
  q.exec("PRAGMA synchronous=NORMAL");
  q.exec("PRAGMA journal_size_limit=67108864");   // the WAL back to 64 MB after a large delete (a pair removed)
  q.exec("CREATE TABLE IF NOT EXISTS meta(k TEXT PRIMARY KEY, v TEXT)");
  q.exec("CREATE TABLE IF NOT EXISTS markets(id INTEGER PRIMARY KEY, symbol TEXT UNIQUE NOT NULL, source TEXT)");
  q.exec("CREATE TABLE IF NOT EXISTS bars(market INTEGER NOT NULL, tf INTEGER NOT NULL, t INTEGER NOT NULL, o REAL, h REAL, "
         "l REAL, c REAL, v REAL, qv REAL, n INTEGER, tbv REAL, tbqv REAL, PRIMARY KEY(market, tf, t)) WITHOUT ROWID");
  q.exec("CREATE TABLE IF NOT EXISTS sync(market INTEGER NOT NULL, tf INTEGER NOT NULL, first INTEGER, last INTEGER, "
         "count INTEGER, history_done INTEGER DEFAULT 0, updated INTEGER, PRIMARY KEY(market, tf))");
  q.exec("CREATE TABLE IF NOT EXISTS ind_sync(market INTEGER NOT NULL, tf INTEGER NOT NULL, last INTEGER, count INTEGER, "
         "version TEXT, PRIMARY KEY(market, tf))");
  q.exec("CREATE TABLE IF NOT EXISTS gaps(market INTEGER NOT NULL, tf INTEGER NOT NULL, from_t INTEGER NOT NULL, to_t INTEGER, "
         "missing INTEGER, checked INTEGER DEFAULT 0, PRIMARY KEY(market, tf, from_t))");
  q.exec("CREATE TABLE IF NOT EXISTS pairs(market INTEGER PRIMARY KEY, spec TEXT NOT NULL, added INTEGER)");
  // 2026-11-18: the first day a series was asked from (pairs with their own first day)
  bool hasStart = false;
  q.exec("PRAGMA table_info(sync)");
  while (q.next()) hasStart |= q.value(1).toString() == "start_req";
  if (!hasStart) q.exec("ALTER TABLE sync ADD COLUMN start_req INTEGER DEFAULT 0");
}

QSqlDatabase open(const QString& name, const QString& path) {
  if (QSqlDatabase::contains(name)) return QSqlDatabase::database(name);
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
  db.setDatabaseName(path);
  db.open();
  QSqlQuery(db).exec("PRAGMA busy_timeout=5000");
  return db;
}

Kline klineOf(const QSqlQuery& q) {
  Kline k;
  k.t = q.value(0).toLongLong();
  k.o = q.value(1).toDouble(); k.h = q.value(2).toDouble(); k.l = q.value(3).toDouble(); k.c = q.value(4).toDouble();
  k.v = q.value(5).toDouble(); k.qv = q.value(6).toDouble(); k.n = q.value(7).toLongLong();
  k.tbv = q.value(8).toDouble(); k.tbqv = q.value(9).toDouble();
  return k;
}
constexpr const char* COLS = "t, o, h, l, c, v, qv, n, tbv, tbqv";
}  // namespace

// ------------------------------------------------------------------------------------------------ writer
class Writer : public QObject {
public:
  explicit Writer(const QString& path) : m_path(path) {}
  ~Writer() override {
    if (m_db.isOpen()) m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_name);
  }
  void openDb() {
    m_name = "technicaldata-write-" + m_path;
    m_db = open(m_name, m_path);
    schema(m_db);
    // the first pair, once: BTCUSDT as it was recorded before pairs could be chosen (a user who removes it keeps it removed)
    QSqlQuery q(m_db);
    q.exec("SELECT v FROM meta WHERE k = 'pairs_seeded'");
    if (!q.next()) {
      q.exec("SELECT COUNT(*) FROM pairs");
      if (q.next() && q.value(0).toInt() == 0) putPair("BINANCE:BTCUSDT", specToJson(defaultSpec()));
      q.exec("INSERT OR REPLACE INTO meta(k, v) VALUES('pairs_seeded', '1')");
    }
    // the forex majors, once (user's request 2026-11-18: Dukascopy, 5m to 1W from 2003)
    q.exec("SELECT v FROM meta WHERE k = 'fx_seeded'");
    if (!q.next()) {
      for (const auto& i : dukascopy::majors()) {
        const QString m = dukascopy::PREFIX + i.symbol;
        QSqlQuery has(m_db);
        has.prepare("SELECT 1 FROM pairs p JOIN markets k ON k.id = p.market WHERE k.symbol = ?");
        has.addBindValue(m); has.exec();
        if (!has.next()) putPair(m, specToJson(defaultForexSpec()));
      }
      q.exec("INSERT OR REPLACE INTO meta(k, v) VALUES('fx_seeded', '1')");
    }
    loadPairs();
    q.exec("SELECT v FROM meta WHERE k = 'collection_paused'");
    m_allPaused = q.next() && q.value(0).toString() == "1";
  }
  bool m_allPaused = false;
  // the pairs paused and everything, as stored (Store keeps its own copy, known before the writes)
  QStringList flaggedSymbols(const char* flag) {
    QStringList out;
    QSqlQuery q(m_db);
    q.exec("SELECT k.symbol, p.spec FROM pairs p JOIN markets k ON k.id = p.market");
    while (q.next()) if (QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object().value(flag).toBool()) out << q.value(0).toString();
    return out;
  }
  void setPausedFlag(const QString& symbol, bool paused, bool cancelled = false) {
    const qint64 id = findId(symbol);
    if (id < 0) return;
    QSqlQuery q(m_db);
    q.prepare("SELECT spec FROM pairs WHERE market = ?");
    q.addBindValue(id); q.exec();
    if (!q.next()) return;
    QJsonObject spec = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
    q.finish();
    if (paused) spec.insert("paused", true); else spec.remove("paused");
    if (cancelled) spec.insert("cancelled", true); else spec.remove("cancelled");   // resumed: no longer cancelled
    q.prepare("UPDATE pairs SET spec = ? WHERE market = ?");
    q.addBindValue(QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact))); q.addBindValue(id); q.exec();
    m_pairs[id].paused = paused;
    m_pairs[id].cancelled = cancelled;
  }
  void setAllPausedFlag(bool paused) {
    m_allPaused = paused;
    putMeta("collection_paused", paused ? "1" : "0");
  }
  // everything the pair downloaded goes; the pair stays, paused
  void cancelPair(const QString& symbol) {
    const qint64 id = findId(symbol);
    if (id < 0) return;
    m_db.transaction();
    QSqlQuery q(m_db);
    for (const char* t : {"bars", "ind", "sync", "ind_sync", "gaps"}) {
      q.prepare(QString("DELETE FROM %1 WHERE market = ?").arg(t));
      q.addBindValue(id); q.exec();
    }
    clearMeta(symbol);
    setPausedFlag(symbol, true, true);
    m_db.commit();
  }
  struct Pair { QSet<int> tfs; qint64 start = 0; bool paused = false, cancelled = false; };
  static Pair pairOf(const QJsonObject& spec) {
    Pair p;
    for (const QJsonValue& t : spec.value("timeframes").toArray()) p.tfs.insert(t.toInt());
    p.start = qint64(spec.value("start").toDouble(0));
    p.paused = spec.value("paused").toBool();
    p.cancelled = spec.value("cancelled").toBool();
    return p;
  }
  void loadPairs() {
    m_pairs.clear();
    QSqlQuery q(m_db);
    q.exec("SELECT market, spec FROM pairs");
    while (q.next()) m_pairs.insert(q.value(0).toLongLong(), pairOf(QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object()));
  }
  void putPair(const QString& symbol, const QJsonObject& spec) {
    const qint64 id = marketId(symbol);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO pairs(market, spec, added) VALUES(?, ?, ?) ON CONFLICT(market) DO UPDATE SET spec = excluded.spec");
    q.addBindValue(id); q.addBindValue(QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact)));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
    m_pairs.insert(id, pairOf(spec));
  }
  qint64 findId(const QString& symbol) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM markets WHERE symbol = ?");
    q.addBindValue(symbol); q.exec();
    return q.next() ? q.value(0).toLongLong() : -1;
  }
  // a pair records this timeframe: what is not one is not written (a page arriving after it was removed)
  bool wanted(qint64 id, int tf) const {
    const auto it = m_pairs.constFind(id);
    return it != m_pairs.cend() && it->tfs.contains(tf);
  }
  void deleteSeries(qint64 id, int tf) {
    QSqlQuery q(m_db);
    for (const char* t : {"bars", "ind", "sync", "ind_sync", "gaps"}) {
      q.prepare(QString("DELETE FROM %1 WHERE market = ? AND tf = ?").arg(t));
      q.addBindValue(id); q.addBindValue(tf); q.exec();
    }
  }
  // the bars before a later first day go; what is left is counted again
  void trimBefore(qint64 id, int tf, qint64 start) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM bars WHERE market = ? AND tf = ? AND t < ?");
    q.addBindValue(id); q.addBindValue(tf); q.addBindValue(start); q.exec();
    q.prepare("DELETE FROM ind WHERE market = ? AND tf = ? AND t < ?");
    q.addBindValue(id); q.addBindValue(tf); q.addBindValue(start); q.exec();
    q.prepare("DELETE FROM gaps WHERE market = ? AND tf = ? AND from_t < ?");
    q.addBindValue(id); q.addBindValue(tf); q.addBindValue(start); q.exec();
    q.prepare("DELETE FROM ind_sync WHERE market = ? AND tf = ?");   // the first bar moved: computed again
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    q.prepare("SELECT MIN(t), COUNT(*) FROM bars WHERE market = ? AND tf = ?");
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    const qint64 first = q.next() ? q.value(0).toLongLong() : 0, count = q.value(1).toLongLong();
    q.finish();
    if (count == 0) { deleteSeries(id, tf); return; }
    SyncState s = state(id, tf);
    s.first = first;
    s.count = count;
    s.startReq = start;
    putState(id, tf, s);
  }
  QHash<QString, int> m_gen;   // the generation of the last change written per pair (Store::generation)
  void setGen(const QString& symbol, int gen) { if (gen >= 0) m_gen[symbol] = std::max(m_gen.value(symbol), gen); }
  void setPair(const QString& symbol, const QJsonObject& specIn) {
    // forex: 4h, 1D, 1W are made from the stored hours (Dukascopy.h): 1h comes with them
    QJsonObject spec = dukascopy::isForex(symbol) ? specToJson(withForexHours(specFromJson(specIn))) : specIn;
    const qint64 id = marketId(symbol);
    // the pause is the store's (setPaused): a spec saved from the settings window keeps it as it is
    spec.remove("paused");
    spec.remove("cancelled");
    if (m_pairs.value(id).paused) spec.insert("paused", true);
    if (m_pairs.value(id).cancelled) spec.insert("cancelled", true);
    const Pair now = pairOf(spec);
    m_db.transaction();
    const auto it = m_pairs.constFind(id);
    if (it != m_pairs.cend()) {
      const Pair was = *it;
      for (int tf : was.tfs) if (!now.tfs.contains(tf)) deleteSeries(id, tf);
      if (now.start > was.start) for (int tf : now.tfs) trimBefore(id, tf, now.start);
      // forex, an earlier first day: Dukascopy's files go forward from it again; the minute series are made again
      // (written in order, so their indicators are computed once, not again at every day added before them)
      if (dukascopy::isForex(symbol) && now.start < was.start) {
        for (int tf : {60, 300, 900}) deleteSeries(id, tf);
        clearMeta(symbol);
      }
    }
    putPair(symbol, spec);
    m_db.commit();
  }
  void removePair(const QString& symbol) {
    const qint64 id = findId(symbol);
    if (id < 0) return;
    m_db.transaction();
    QSqlQuery q(m_db);
    for (const char* t : {"bars", "ind", "sync", "ind_sync", "gaps", "pairs"}) {
      q.prepare(QString("DELETE FROM %1 WHERE market = ?").arg(t));
      q.addBindValue(id); q.exec();
    }
    q.prepare("DELETE FROM markets WHERE id = ?");
    q.addBindValue(id); q.exec();
    clearMeta(symbol);
    m_db.commit();
    m_pairs.remove(id);
  }
  // per pair notes of a collector (Dukascopy's cursors): "<name>:<market>"
  void putMeta(const QString& k, const QString& v) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO meta(k, v) VALUES(?, ?)");
    q.addBindValue(k); q.addBindValue(v); q.exec();
  }
  void clearMeta(const QString& symbol) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM meta WHERE k LIKE ?");
    q.addBindValue("%:" + symbol);
    q.exec();
  }
  void startRequested(const QString& symbol, int tf, qint64 start) {
    const qint64 id = findId(symbol);
    if (id < 0 || !wanted(id, tf)) return;
    SyncState s = state(id, tf);
    s.startReq = start;
    putState(id, tf, s);
  }
  void clearIndicators(const QString& symbol, int tf) {
    const qint64 id = findId(symbol);
    if (id < 0) return;
    QSqlQuery q(m_db);
    m_db.transaction();
    q.prepare("DELETE FROM ind WHERE market = ? AND tf = ?");
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    q.prepare("DELETE FROM ind_sync WHERE market = ? AND tf = ?");
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    m_db.commit();
  }
  qint64 marketId(const QString& symbol) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM markets WHERE symbol = ?");
    q.addBindValue(symbol); q.exec();
    if (q.next()) return q.value(0).toLongLong();
    q.prepare("INSERT INTO markets(symbol, source) VALUES(?, ?)");
    q.addBindValue(symbol); q.addBindValue(symbol.section(':', 0, 0)); q.exec();
    return q.lastInsertId().toLongLong();
  }
  SyncState state(qint64 id, int tf) {
    SyncState s;
    QSqlQuery q(m_db);
    q.prepare("SELECT first, last, count, history_done, updated, start_req FROM sync WHERE market = ? AND tf = ?");
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    if (q.next()) {
      s.first = q.value(0).toLongLong(); s.last = q.value(1).toLongLong(); s.count = q.value(2).toLongLong();
      s.historyDone = q.value(3).toInt() != 0; s.updated = q.value(4).toLongLong(); s.startReq = q.value(5).toLongLong();
    }
    return s;
  }
  void putState(qint64 id, int tf, const SyncState& s) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO sync(market, tf, first, last, count, history_done, updated, start_req) VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
              "ON CONFLICT(market, tf) DO UPDATE SET first = excluded.first, last = excluded.last, count = excluded.count, "
              "history_done = excluded.history_done, updated = excluded.updated, start_req = excluded.start_req");
    q.addBindValue(id); q.addBindValue(tf); q.addBindValue(s.first); q.addBindValue(s.last); q.addBindValue(s.count);
    q.addBindValue(s.historyDone ? 1 : 0); q.addBindValue(s.updated); q.addBindValue(s.startReq);
    q.exec();
  }
  int add(const QString& symbol, int tf, const QVector<Kline>& rows, qint64* minNew = nullptr) {
    if (minNew) *minNew = 0;
    if (rows.isEmpty()) return 0;
    const qint64 id = findId(symbol);
    if (id < 0 || !wanted(id, tf)) return 0;
    if (m_allPaused || m_pairs.value(id).paused) return 0;   // paused: a page still on its way is not written
    const qint64 start = m_pairs.value(id).start;
    SyncState s = state(id, tf);
    if (s.count == 0) s.startReq = start;
    m_db.transaction();
    QSqlQuery exists(m_db), up(m_db);
    exists.prepare("SELECT 1 FROM bars WHERE market = ? AND tf = ? AND t = ?");
    up.prepare("INSERT INTO bars(market, tf, t, o, h, l, c, v, qv, n, tbv, tbqv) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
               "ON CONFLICT(market, tf, t) DO UPDATE SET o = excluded.o, h = excluded.h, l = excluded.l, c = excluded.c, "
               "v = excluded.v, qv = excluded.qv, n = excluded.n, tbv = excluded.tbv, tbqv = excluded.tbqv");
    int added = 0;
    for (const Kline& k : rows) {
      if (k.t < start) continue;   // before the pair's first day
      bool isNew = s.count == 0 || k.t > s.last || k.t < s.first;
      if (!isNew) {   // inside what is stored: a hole being filled, or a bar seen again
        exists.addBindValue(id); exists.addBindValue(tf); exists.addBindValue(k.t);
        exists.exec();
        isNew = !exists.next();
        exists.finish();
      }
      up.addBindValue(id); up.addBindValue(tf); up.addBindValue(k.t);
      up.addBindValue(k.o); up.addBindValue(k.h); up.addBindValue(k.l); up.addBindValue(k.c); up.addBindValue(k.v);
      up.addBindValue(k.qv); up.addBindValue(k.n); up.addBindValue(k.tbv); up.addBindValue(k.tbqv);
      up.exec();
      if (isNew) {
        ++added;
        if (minNew && (*minNew == 0 || k.t < *minNew)) *minNew = k.t;
        s.first = s.count == 0 ? k.t : std::min(s.first, k.t);
        s.last = s.count == 0 ? k.t : std::max(s.last, k.t);
        ++s.count;
      }
    }
    s.updated = QDateTime::currentMSecsSinceEpoch();
    putState(id, tf, s);
    m_db.commit();
    return added;
  }
  void historyDone(const QString& symbol, int tf) {
    const qint64 id = findId(symbol);
    if (id < 0 || !wanted(id, tf)) return;
    SyncState s = state(id, tf);
    s.historyDone = true;
    s.updated = QDateTime::currentMSecsSinceEpoch();
    putState(id, tf, s);
  }
  QVector<Gap> scanGaps(const QString& symbol, int tf) {
    QVector<Gap> found;
    const qint64 id = findId(symbol);
    if (id < 0 || !wanted(id, tf)) return found;
    QSqlQuery q(m_db);
    q.prepare("SELECT prev, t FROM (SELECT t, LAG(t) OVER (ORDER BY t) AS prev FROM bars WHERE market = ? AND tf = ?) "
              "WHERE prev IS NOT NULL AND t - prev > ?");
    q.addBindValue(id); q.addBindValue(tf); q.addBindValue(tf);
    q.exec();
    // forex: the weekend (and a daily bar's summer-time hour) is no hole; Dukascopy's files were whole, so what is
    // missing is a time nothing traded (a holiday): known at once, never asked again
    const bool fx = dukascopy::isForex(symbol);
    while (q.next()) {
      Gap g;
      g.from = q.value(0).toLongLong();
      g.to = q.value(1).toLongLong();
      if (fx) {
        const qint64 next = dukascopy::nextExpected(g.from, tf);
        if (g.to <= next) continue;
        g.missing = std::max<qint64>(1, (g.to - next) / tf);
      } else {
        g.missing = (g.to - g.from) / tf - 1;
      }
      found << g;
    }
    // keep the checked flags of holes already known; forget the ones filled since
    QHash<qint64, bool> checked;
    q.prepare("SELECT from_t, checked FROM gaps WHERE market = ? AND tf = ?");
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    while (q.next()) checked.insert(q.value(0).toLongLong(), q.value(1).toInt() != 0);
    m_db.transaction();
    q.prepare("DELETE FROM gaps WHERE market = ? AND tf = ?");
    q.addBindValue(id); q.addBindValue(tf); q.exec();
    q.prepare("INSERT INTO gaps(market, tf, from_t, to_t, missing, checked) VALUES(?, ?, ?, ?, ?, ?)");
    for (Gap& g : found) {
      g.checked = fx || checked.value(g.from, false);
      q.addBindValue(id); q.addBindValue(tf); q.addBindValue(g.from); q.addBindValue(g.to); q.addBindValue(g.missing);
      q.addBindValue(g.checked ? 1 : 0);
      q.exec();
    }
    m_db.commit();
    return found;
  }
  // the ind table holds these value columns: a new one is added (its rows NULL until computed), none is dropped
  void indTable(const QStringList& columns) {
    if (m_indHave.isEmpty()) {
      QSqlQuery q(m_db);
      q.exec("PRAGMA table_info(ind)");
      while (q.next()) m_indHave.insert(q.value(1).toString());
    }
    QSqlQuery q(m_db);
    if (m_indHave.isEmpty()) {
      QStringList cols;
      for (const QString& c : columns) cols << "\"" + c + "\" REAL";
      q.exec("CREATE TABLE ind(market INTEGER NOT NULL, tf INTEGER NOT NULL, t INTEGER NOT NULL, warm INTEGER, " + cols.join(", ") +
             ", PRIMARY KEY(market, tf, t)) WITHOUT ROWID");
      m_indHave = QSet<QString>{"market", "tf", "t", "warm"};
      for (const QString& c : columns) m_indHave.insert(c);
      return;
    }
    for (const QString& c : columns)
      if (!m_indHave.contains(c)) {
        q.exec("ALTER TABLE ind ADD COLUMN \"" + c + "\" REAL");
        m_indHave.insert(c);
      }
  }
  // false: dropped (computed from bars changed since)
  bool addIndicators(const QString& symbol, int tf, const QStringList& columns, const QVector<IndRow>& rows,
                     const QString& version, bool replace, int gen) {
    const qint64 id = findId(symbol);
    if (id < 0 || !wanted(id, tf)) return false;
    if (gen >= 0 && gen < m_gen.value(symbol)) return false;   // computed before a cancel, a later first day, …
    const SyncState bars = state(id, tf);
    if (bars.count == 0) return false;   // its bars deleted meanwhile
    indTable(columns);
    m_db.transaction();
    QSqlQuery q(m_db);
    if (replace) {   // computed again: each row is written over (INSERT OR REPLACE: the columns no longer kept become NULL)
      q.prepare("DELETE FROM ind_sync WHERE market = ? AND tf = ?");
      q.addBindValue(id); q.addBindValue(tf); q.exec();
      q.prepare("DELETE FROM ind WHERE market = ? AND tf = ? AND t < ?");   // rows of bars no longer there
      q.addBindValue(id); q.addBindValue(tf); q.addBindValue(bars.first); q.exec();
    }
    QSqlQuery c(m_db);
    c.prepare("SELECT last FROM ind_sync WHERE market = ? AND tf = ?");
    c.addBindValue(id); c.addBindValue(tf); c.exec();
    const qint64 prevLast = c.next() ? c.value(0).toLongLong() : 0;
    c.finish();
    QSqlQuery exists(m_db);
    exists.prepare("SELECT 1 FROM ind WHERE market = ? AND tf = ? AND t = ?");
    QStringList names, marks;
    for (const QString& c : columns) { names << "\"" + c + "\""; marks << "?"; }
    q.prepare("INSERT OR REPLACE INTO ind(market, tf, t, warm, " + names.join(", ") + ") VALUES(?, ?, ?, ?, " + marks.join(", ") + ")");
    qint64 last = 0, added = 0;
    for (const IndRow& r : rows) {
      // after the last computed bar: new; before it (a hole filled): new only if not there yet
      bool isNew = r.t > prevLast || replace;
      if (!isNew) {
        exists.addBindValue(id); exists.addBindValue(tf); exists.addBindValue(r.t);
        exists.exec();
        isNew = !exists.next();
        exists.finish();
      }
      added += isNew;
      q.addBindValue(id); q.addBindValue(tf); q.addBindValue(r.t); q.addBindValue(r.warm ? 1 : 0);
      for (int i = 0; i < columns.size(); ++i) {
        const double v = i < r.v.size() ? r.v[i] : NAN;
        q.addBindValue(std::isfinite(v) ? QVariant(v) : QVariant());
      }
      q.exec();
      last = std::max(last, r.t);
    }
    c.prepare("INSERT INTO ind_sync(market, tf, last, count, version) VALUES(?, ?, ?, ?, ?) ON CONFLICT(market, tf) DO UPDATE SET "
              "last = MAX(ind_sync.last, excluded.last), count = ind_sync.count + excluded.count, version = excluded.version");
    c.addBindValue(id); c.addBindValue(tf); c.addBindValue(std::max(last, prevLast)); c.addBindValue(added); c.addBindValue(version);
    c.exec();
    m_db.commit();
    return true;
  }

  void gapChecked(const QString& symbol, int tf, qint64 from) {
    const qint64 id = findId(symbol);
    if (id < 0) return;
    if (m_allPaused || m_pairs.value(id).paused) return;   // its page was dropped: the hole is not the exchange's
    QSqlQuery q(m_db);
    q.prepare("UPDATE gaps SET checked = 1 WHERE market = ? AND tf = ? AND from_t = ?");
    q.addBindValue(id); q.addBindValue(tf); q.addBindValue(from); q.exec();
  }

private:
  QString m_path, m_name;
  QSqlDatabase m_db;
  QSet<QString> m_indHave;
  QHash<qint64, Pair> m_pairs;
};

// ------------------------------------------------------------------------------------------------ store
Store& Store::instance() {
  static Store* s = [] {
    const QString path = paths::data("technical-data") + "/technical-data.sqlite";
    // its earlier names, 2026-11-18: Facts › Markets (facts/market.sqlite), then Charts Data (charts-data/charts-data.sqlite)
    paths::moveDatabase(paths::data() + "/charts-data/charts-data.sqlite", path);
    paths::moveDatabase(paths::data() + "/facts/market.sqlite", path);
    return new Store(path);
  }();
  return *s;
}

Store::Store(const QString& path, QObject* parent) : QObject(parent), m_path(path) {
  m_thread = new QThread(this);
  m_thread->setObjectName("technicaldata-writer");
  m_writer = new Writer(path);
  m_writer->moveToThread(m_thread);
  m_thread->start(QThread::LowPriority);
  QStringList paused, cancelled;
  bool all = false;
  QMetaObject::invokeMethod(m_writer, [w = m_writer, &paused, &cancelled, &all] {
    w->openDb();
    paused = w->flaggedSymbols("paused");
    cancelled = w->flaggedSymbols("cancelled");
    all = w->m_allPaused;
  }, Qt::BlockingQueuedConnection);
  m_paused = QSet<QString>(paused.cbegin(), paused.cend());
  m_cancelled = QSet<QString>(cancelled.cbegin(), cancelled.cend());
  m_allPaused = all;
  if (QCoreApplication::instance())
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { wait(); });
}

Store::~Store() {
  QMetaObject::invokeMethod(m_writer, [w = m_writer] { delete w; }, Qt::BlockingQueuedConnection);
  m_thread->quit();
  m_thread->wait();
  const QString prefix = "technicaldata-read-" + m_path + "-";
  for (const QString& n : QSqlDatabase::connectionNames())
    if (n.startsWith(prefix)) QSqlDatabase::removeDatabase(n);
}

void Store::add(const QString& market, int tf, const QVector<Kline>& rows, QObject* ctx, std::function<void(int)> done) {
  QPointer<QObject> c(ctx);
  const bool hasCtx = ctx != nullptr;
  QPointer<Store> self(this);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, self, market, tf, rows, c, hasCtx, done] {
    qint64 from = 0;
    const int added = w->add(market, tf, rows, &from);
    if (self) QMetaObject::invokeMethod(self.data(), [self, market, tf, added, from, c, hasCtx, done] {
      if (added > 0 && self) emit self->changed(market, tf, from);
      if (done && (!hasCtx || c)) done(added);
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::addIndicators(const QString& market, int tf, const QStringList& columns, const QVector<IndRow>& rows,
                          const QString& version, bool replace, QObject* ctx, std::function<void()> done, int generation) {
  QPointer<QObject> c(ctx);
  const bool hasCtx = ctx != nullptr;
  QPointer<Store> self(this);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, self, market, tf, columns, rows, version, replace, c, hasCtx, done, generation] {
    const bool written = w->addIndicators(market, tf, columns, rows, version, replace, generation);
    if (self) QMetaObject::invokeMethod(self.data(), [self, market, tf, c, hasCtx, done, written] {
      if (self && written) emit self->indicatorsChanged(market, tf);
      if (done && (!hasCtx || c)) done();
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::setHistoryDone(const QString& market, int tf) {
  QMetaObject::invokeMethod(m_writer, [w = m_writer, market, tf] { w->historyDone(market, tf); }, Qt::QueuedConnection);
}

void Store::scanGaps(const QString& market, int tf, QObject* ctx, std::function<void(const QVector<Gap>&)> done) {
  QPointer<QObject> c(ctx);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, market, tf, c, done] {
    const QVector<Gap> g = w->scanGaps(market, tf);
    if (c) QMetaObject::invokeMethod(c.data(), [g, done] { if (done) done(g); }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::setGapChecked(const QString& market, int tf, qint64 from) {
  QMetaObject::invokeMethod(m_writer, [w = m_writer, market, tf, from] { w->gapChecked(market, tf, from); }, Qt::QueuedConnection);
}

void Store::setPairSpec(const QString& market, const QJsonObject& spec, QObject* ctx, std::function<void()> done) {
  const int gen = beginChange(market);
  QPointer<QObject> c(ctx);
  const bool hasCtx = ctx != nullptr;
  QPointer<Store> self(this);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, self, market, spec, c, hasCtx, done, gen] {
    w->setGen(market, gen);
    w->setPair(market, spec);
    if (self) QMetaObject::invokeMethod(self.data(), [self, market, c, hasCtx, done] {
      if (self) self->endChange(market);
      if (self) emit self->pairsChanged(market);
      if (done && (!hasCtx || c)) done();
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::removePair(const QString& market, QObject* ctx, std::function<void()> done) {
  const int gen = beginChange(market);
  QPointer<QObject> c(ctx);
  const bool hasCtx = ctx != nullptr;
  QPointer<Store> self(this);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, self, market, c, hasCtx, done, gen] {
    w->setGen(market, gen);
    w->removePair(market);
    if (self) QMetaObject::invokeMethod(self.data(), [self, market, c, hasCtx, done] {
      if (self) {
        {
          QMutexLocker l(&self->m_pauseLock);   // gone: its pause with it (kept until now: nothing ran on it meanwhile)
          self->m_paused.remove(market);
          self->m_cancelled.remove(market);
        }
        self->endChange(market);
      }
      if (self) emit self->pairsChanged(market);
      if (done && (!hasCtx || c)) done();
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::setMeta(const QString& key, const QString& value) {
  QMetaObject::invokeMethod(m_writer, [w = m_writer, key, value] { w->putMeta(key, value); }, Qt::QueuedConnection);
}

bool Store::isPaused(const QString& market) const {
  QMutexLocker l(&m_pauseLock);
  return m_allPaused || m_paused.contains(market) || m_busy.value(market) > 0;
}
bool Store::pairPaused(const QString& market) const { QMutexLocker l(&m_pauseLock); return m_paused.contains(market); }
bool Store::allPaused() const { QMutexLocker l(&m_pauseLock); return m_allPaused; }
bool Store::cancelled(const QString& market) const { QMutexLocker l(&m_pauseLock); return m_cancelled.contains(market); }
bool Store::busy(const QString& market) const { QMutexLocker l(&m_pauseLock); return m_busy.value(market) > 0; }
int Store::generation(const QString& market) const { QMutexLocker l(&m_pauseLock); return m_gen.value(market); }

int Store::beginChange(const QString& market) {
  int gen;
  {
    QMutexLocker l(&m_pauseLock);
    ++m_busy[market];
    gen = ++m_gen[market];
  }
  emit pausedChanged(market);   // the collectors let go of it now
  return gen;
}

void Store::endChange(const QString& market) {
  QMutexLocker l(&m_pauseLock);
  if (--m_busy[market] <= 0) m_busy.remove(market);
}

void Store::whenWritten(QObject* ctx, std::function<void()> fn) {
  QPointer<QObject> c(ctx);
  QMetaObject::invokeMethod(m_writer, [c, fn] {
    if (c) QMetaObject::invokeMethod(c.data(), [c, fn] { if (c && fn) fn(); }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::setPaused(const QString& market, bool paused) {
  {
    QMutexLocker l(&m_pauseLock);
    if (paused) m_paused.insert(market); else { m_paused.remove(market); m_cancelled.remove(market); }
  }
  QMetaObject::invokeMethod(m_writer, [w = m_writer, market, paused] { w->setPausedFlag(market, paused); }, Qt::QueuedConnection);
  emit pausedChanged(market);
}

void Store::setAllPaused(bool paused) {
  {
    QMutexLocker l(&m_pauseLock);
    m_allPaused = paused;
  }
  QMetaObject::invokeMethod(m_writer, [w = m_writer, paused] { w->setAllPausedFlag(paused); }, Qt::QueuedConnection);
  emit pausedChanged(QString());
}

void Store::cancelPair(const QString& market, QObject* ctx, std::function<void()> done) {
  {
    QMutexLocker l(&m_pauseLock);
    m_paused.insert(market);   // at once: nothing more is written for it
    m_cancelled.insert(market);
  }
  const int gen = beginChange(market);   // and nothing started again before the delete is written
  QPointer<QObject> c(ctx);
  const bool hasCtx = ctx != nullptr;
  QPointer<Store> self(this);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, self, market, c, hasCtx, done, gen] {
    w->setGen(market, gen);
    w->cancelPair(market);
    if (self) QMetaObject::invokeMethod(self.data(), [self, market, c, hasCtx, done] {
      if (self) self->endChange(market);
      if (self) emit self->pairsChanged(market);
      if (done && (!hasCtx || c)) done();
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::setStartRequested(const QString& market, int tf, qint64 start) {
  QMetaObject::invokeMethod(m_writer, [w = m_writer, market, tf, start] { w->startRequested(market, tf, start); }, Qt::QueuedConnection);
}

void Store::setBackfilling(const QString& market, int tf, bool on) {
  const QString k = market + "|" + QString::number(tf);
  if (on) { m_backfill.insert(k); return; }
  if (m_backfill.remove(k)) emit backfillDone(market, tf);
}

void Store::clearIndicators(const QString& market, int tf) {
  QPointer<Store> self(this);
  QMetaObject::invokeMethod(m_writer, [w = m_writer, self, market, tf] {
    w->clearIndicators(market, tf);
    if (self) QMetaObject::invokeMethod(self.data(), [self, market, tf] { if (self) emit self->indicatorsChanged(market, tf); }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::wait() {
  if (QThread::currentThread() == m_thread) return;
  QMetaObject::invokeMethod(m_writer, [] {}, Qt::BlockingQueuedConnection);
}

namespace {
// One read connection per thread and database, closed when the thread ends (a thread id is reused by the next thread,
// so a name made from it would hand a dead thread's connection to a new one)
struct ThreadReaders {
  QHash<QString, QString> names;   // path → connection name
  ~ThreadReaders() {
    if (!QCoreApplication::instance()) return;   // the main thread's, at exit: Qt is already gone
    for (const QString& n : std::as_const(names)) {
      { QSqlDatabase db = QSqlDatabase::database(n, false); if (db.isOpen()) db.close(); }
      QSqlDatabase::removeDatabase(n);
    }
  }
};
QSqlDatabase reader(const QString& path) {
  thread_local ThreadReaders readers;
  static QAtomicInt counter;
  auto it = readers.names.find(path);
  if (it == readers.names.end()) it = readers.names.insert(path, "technicaldata-read-" + path + "-" + QString::number(counter.fetchAndAddRelaxed(1)));
  return open(*it, path);
}
qint64 idOf(QSqlDatabase& db, const QString& symbol) {
  QSqlQuery q(db);
  q.prepare("SELECT id FROM markets WHERE symbol = ?");
  q.addBindValue(symbol); q.exec();
  return q.next() ? q.value(0).toLongLong() : -1;
}
}  // namespace

SyncState Store::state(const QString& market, int tf) const {
  SyncState s;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0) return s;
  QSqlQuery q(db);
  q.prepare("SELECT first, last, count, history_done, updated, start_req FROM sync WHERE market = ? AND tf = ?");
  q.addBindValue(id); q.addBindValue(tf); q.exec();
  if (q.next()) {
    s.first = q.value(0).toLongLong(); s.last = q.value(1).toLongLong(); s.count = q.value(2).toLongLong();
    s.historyDone = q.value(3).toInt() != 0; s.updated = q.value(4).toLongLong(); s.startReq = q.value(5).toLongLong();
  }
  return s;
}

QVector<Kline> Store::range(const QString& market, int tf, qint64 from, qint64 to, int limit) const {
  QVector<Kline> out;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0) return out;
  QSqlQuery q(db);
  q.setForwardOnly(true);
  q.prepare(QString("SELECT %1 FROM bars WHERE market = ? AND tf = ? AND t >= ? AND t <= ? ORDER BY t%2")
                .arg(COLS, limit > 0 ? QString(" LIMIT %1").arg(limit) : QString()));
  q.addBindValue(id); q.addBindValue(tf); q.addBindValue(from); q.addBindValue(to);
  q.exec();
  while (q.next()) out << klineOf(q);
  return out;
}

QVector<Kline> Store::last(const QString& market, int tf, int n) const {
  QVector<Kline> out;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0 || n <= 0) return out;
  QSqlQuery q(db);
  q.setForwardOnly(true);
  q.prepare(QString("SELECT %1 FROM bars WHERE market = ? AND tf = ? ORDER BY t DESC LIMIT ?").arg(COLS));
  q.addBindValue(id); q.addBindValue(tf); q.addBindValue(n);
  q.exec();
  while (q.next()) out << klineOf(q);
  std::reverse(out.begin(), out.end());
  return out;
}

QVector<Kline> Store::before(const QString& market, int tf, qint64 t, int n) const {
  QVector<Kline> out;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0 || n <= 0) return out;
  QSqlQuery q(db);
  q.setForwardOnly(true);
  q.prepare(QString("SELECT %1 FROM bars WHERE market = ? AND tf = ? AND t < ? ORDER BY t DESC LIMIT ?").arg(COLS));
  q.addBindValue(id); q.addBindValue(tf); q.addBindValue(t); q.addBindValue(n);
  q.exec();
  while (q.next()) out << klineOf(q);
  std::reverse(out.begin(), out.end());
  return out;
}

QVector<Gap> Store::gaps(const QString& market, int tf) const {
  QVector<Gap> out;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0) return out;
  QSqlQuery q(db);
  q.prepare("SELECT from_t, to_t, missing, checked FROM gaps WHERE market = ? AND tf = ? ORDER BY from_t");
  q.addBindValue(id); q.addBindValue(tf); q.exec();
  while (q.next()) out << Gap{q.value(0).toLongLong(), q.value(1).toLongLong(), q.value(2).toLongLong(), q.value(3).toInt() != 0};
  return out;
}

QStringList Store::markets() const {
  QStringList out;
  QSqlDatabase db = reader(m_path);
  QSqlQuery q(db);
  q.exec("SELECT symbol FROM markets ORDER BY id");
  while (q.next()) out << q.value(0).toString();
  return out;
}

QString Store::meta(const QString& key) const {
  QSqlDatabase db = reader(m_path);
  QSqlQuery q(db);
  q.prepare("SELECT v FROM meta WHERE k = ?");
  q.addBindValue(key); q.exec();
  return q.next() ? q.value(0).toString() : QString();
}

QStringList Store::pairs() const {
  QStringList out;
  QSqlDatabase db = reader(m_path);
  QSqlQuery q(db);
  q.exec("SELECT m.symbol FROM pairs p JOIN markets m ON m.id = p.market ORDER BY p.added, p.market");
  while (q.next()) out << q.value(0).toString();
  return out;
}

QJsonObject Store::pairSpec(const QString& market) const {
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0) return {};
  QSqlQuery q(db);
  q.prepare("SELECT spec FROM pairs WHERE market = ?");
  q.addBindValue(id); q.exec();
  return q.next() ? QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object() : QJsonObject();
}

QStringList Store::indicatorColumns(const QString& market) const {
  return columnNames(specFromJson(pairSpec(market)));
}

IndState Store::indState(const QString& market, int tf) const {
  IndState s;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  if (id < 0) return s;
  QSqlQuery q(db);
  q.prepare("SELECT last, count, version FROM ind_sync WHERE market = ? AND tf = ?");
  q.addBindValue(id); q.addBindValue(tf); q.exec();
  if (q.next()) { s.last = q.value(0).toLongLong(); s.count = q.value(1).toLongLong(); s.version = q.value(2).toString(); }
  return s;
}

QStringList Store::indicatorColumns() const {
  QStringList out;
  QSqlDatabase db = reader(m_path);
  QSqlQuery q(db);
  q.exec("PRAGMA table_info(ind)");
  while (q.next()) out << q.value(1).toString();
  return out.mid(4);   // after market, tf, t, warm
}

QVector<IndRow> Store::indicators(const QString& market, int tf, qint64 from, qint64 to, int limit) const {
  QVector<IndRow> out;
  QSqlDatabase db = reader(m_path);
  const qint64 id = idOf(db, market);
  const QStringList cols = indicatorColumns(market);
  const QStringList have = indicatorColumns();
  if (id < 0 || cols.isEmpty() || have.isEmpty()) return out;
  QStringList names;   // a column not in the table yet (just added, not computed): NULL
  for (const QString& c : cols) names << (have.contains(c) ? "\"" + c + "\"" : QStringLiteral("NULL"));
  QSqlQuery q(db);
  q.setForwardOnly(true);
  q.prepare(QString("SELECT t, warm, %1 FROM ind WHERE market = ? AND tf = ? AND t >= ? AND t <= ? ORDER BY t%2")
                .arg(names.join(", "), limit > 0 ? QString(" LIMIT %1").arg(limit) : QString()));
  q.addBindValue(id); q.addBindValue(tf); q.addBindValue(from); q.addBindValue(to);
  q.exec();
  while (q.next()) {
    IndRow r;
    r.t = q.value(0).toLongLong();
    r.warm = q.value(1).toInt() != 0;
    r.v.resize(cols.size());
    for (int i = 0; i < cols.size(); ++i) r.v[i] = q.value(2 + i).isNull() ? NAN : q.value(2 + i).toDouble();
    out << r;
  }
  return out;
}

qint64 Store::fileBytes() const {
  return QFileInfo(m_path).size() + QFileInfo(m_path + "-wal").size();
}

}  // namespace technicaldata
