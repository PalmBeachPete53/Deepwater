#include "macrodata/MacroDataStore.h"
#include "core/Paths.h"
#include <QDir>
#include <QCoreApplication>
#include <QTimeZone>
#include <QSqlError>
#include <QRegularExpression>
#include <QSet>
#include <QSqlQuery>
#include <QThread>
#include <QMetaObject>
#include <QVariant>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <atomic>
#include <limits>

namespace macrodata {
namespace {
qint64 ms(const QDateTime& t) { return t.isValid() ? t.toMSecsSinceEpoch() : 0; }
bool same(double a, double b) {
  if (std::isnan(a) && std::isnan(b)) return true;
  if (std::isnan(a) || std::isnan(b)) return false;
  return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b));
}
const char* kSchema[] = {
    "PRAGMA journal_mode=WAL",
    "PRAGMA synchronous=NORMAL",
    "CREATE TABLE IF NOT EXISTS chains(id INTEGER PRIMARY KEY, key TEXT UNIQUE NOT NULL, family TEXT, subject TEXT, "
    "indicator TEXT, kind TEXT, source TEXT, unit TEXT, freq TEXT, mode TEXT, target_chain TEXT, created INTEGER)",
    "CREATE TABLE IF NOT EXISTS facts(id INTEGER PRIMARY KEY, chain INTEGER NOT NULL, prev INTEGER, revises INTEGER, "
    "period INTEGER NOT NULL, target INTEGER NOT NULL DEFAULT 0, at INTEGER, recorded INTEGER NOT NULL, value REAL, "
    "text TEXT, origin TEXT, note TEXT)",
    "CREATE INDEX IF NOT EXISTS facts_slot ON facts(chain, period, target, recorded)",
    "CREATE TABLE IF NOT EXISTS meta(k TEXT PRIMARY KEY, v TEXT)",
    // per-chain totals kept by the writer, so the tab never aggregates millions of rows on the GUI thread
    "CREATE TABLE IF NOT EXISTS chain_stats(chain INTEGER PRIMARY KEY, count INTEGER, revisions INTEGER, first INTEGER, last INTEGER)",
    // one row per chain and month, for the 3D views (see MonthCell)
    "CREATE TABLE IF NOT EXISTS facts_monthly(chain INTEGER NOT NULL, month INTEGER NOT NULL, value REAL, rank REAL, n INTEGER, "
    "rev INTEGER, PRIMARY KEY(chain, month)) WITHOUT ROWID",
};
}  // namespace

// Every value is a date: 0 is 1970-01-01, a real period (the 1970 point of annual chains). Only `target` and `at` use
// 0 for "none", and their readers test that before converting.
QDateTime fromMs(qint64 v) { return QDateTime::fromMSecsSinceEpoch(v, QTimeZone::UTC); }

// ------------------------------------------------------------------------------------------------ writer (worker)
class Writer : public QObject {
public:
  explicit Writer(const QString& path) : m_path(path) {}
  // set at quit: the long monthly backfill stops between chains (rolled back, redone at the next launch) so the GUI
  // thread can wait for the writer without a timeout
  std::atomic<bool> stop{false};
  void open() {
    m_db = QSqlDatabase::addDatabase("QSQLITE", "macrodata-writer");
    m_db.setDatabaseName(m_path);
    if (!m_db.open()) { qWarning("[macro-data] cannot open %s: %s", qPrintable(m_path), qPrintable(m_db.lastError().text())); return; }
    QSqlQuery q(m_db);
    for (const char* s : kSchema) if (!q.exec(s)) qWarning("[macro-data] %s: %s", s, qPrintable(q.lastError().text()));
    // databases written before chain_stats existed: fill it once
    q.exec("SELECT COUNT(*) FROM chain_stats");
    if (q.next() && q.value(0).toInt() == 0)
      q.exec("INSERT INTO chain_stats(chain, count, revisions, first, last) SELECT chain, COUNT(*), SUM(revises IS NOT NULL), "
             "MIN(period), MAX(period) FROM facts GROUP BY chain");
  }
  void setMeta(const QString& k, const QString& v) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO meta(k, v) VALUES(?, ?) ON CONFLICT(k) DO UPDATE SET v = excluded.v");
    q.addBindValue(k); q.addBindValue(v); q.exec();
  }

  // returns {added, revised, keys of the chains that changed}
  std::tuple<int, int, QStringList> write(const QList<ChainDef>& defs, const QList<Obs>& obs) {
    int added = 0, revised = 0;
    QStringList touched;
    if (!m_db.isOpen()) return {0, 0, {}};
    m_db.transaction();
    QSqlQuery q(m_db);
    for (const auto& d : defs) {
      q.prepare("INSERT INTO chains(key, family, subject, indicator, kind, source, unit, freq, mode, target_chain, created) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(key) DO UPDATE SET family=excluded.family, subject=excluded.subject, "
                "indicator=excluded.indicator, kind=excluded.kind, source=excluded.source, unit=excluded.unit, freq=excluded.freq, "
                "mode=excluded.mode, target_chain=excluded.target_chain");
      for (const QVariant& v : {QVariant(d.key), QVariant(d.family), QVariant(d.subject), QVariant(d.indicator), QVariant(d.kind),
                                QVariant(d.source), QVariant(d.unit), QVariant(d.freq), QVariant(d.mode), QVariant(d.targetChain),
                                QVariant(QDateTime::currentMSecsSinceEpoch())})
        q.addBindValue(v);
      if (!q.exec()) qWarning("[macro-data] chain %s: %s", qPrintable(d.key), qPrintable(q.lastError().text()));
    }
    // group the observations by chain
    QHash<QString, QList<const Obs*>> byChain;
    for (const auto& o : obs) byChain[o.chain] << &o;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = byChain.begin(); it != byChain.end(); ++it) {
      q.prepare("SELECT id, mode FROM chains WHERE key = ?"); q.addBindValue(it.key());
      if (!q.exec() || !q.next()) continue;
      const qint64 chainId = q.value(0).toLongLong();
      const bool changesMode = q.value(1).toString() == "changes";
      const QString origin = live(chainId) ? "live" : "import";
      QList<const Obs*> list = it.value();
      std::sort(list.begin(), list.end(), [](const Obs* a, const Obs* b) { return a->period < b->period; });
      // Only the slots this batch can touch are read: from its first period on, plus (changes mode) the chain's latest
      // value before it. A pass brings the last days of a chain that may hold 25,000 facts: the older ones can't change.
      qint64 minP = std::numeric_limits<qint64>::max();
      // validity, not a non-zero value, marks a dated observation: 0 ms is 1970-01-01, the first point of many chains
      for (const Obs* o : list) if (o->period.isValid()) minP = std::min(minP, ms(o->period));
      if (minP == std::numeric_limits<qint64>::max()) continue;   // no dated observation: nothing to record
      // latest fact of each slot (period, target), and the latest value per period for the "changes" mode
      struct Slot { qint64 id; double value; QString text; };
      QHash<QPair<qint64, qint64>, Slot> known;
      QMap<qint64, double> byPeriod;
      q.prepare("SELECT id, period, target, value, text FROM facts WHERE chain = ? AND period >= ? ORDER BY period, target, recorded, id");
      q.addBindValue(chainId); q.addBindValue(minP); q.exec();
      while (q.next()) {
        const auto key = qMakePair(q.value(1).toLongLong(), q.value(2).toLongLong());
        const double v = q.value(3).isNull() ? NAN : q.value(3).toDouble();
        known[key] = Slot{q.value(0).toLongLong(), v, q.value(4).toString()};
        byPeriod[key.first] = v;
      }
      if (changesMode) {   // the period just before the batch: the last fact in slot order among the earlier periods
        q.prepare("SELECT period, value FROM facts WHERE chain = ? AND period < ? ORDER BY period DESC, target DESC, recorded DESC, id DESC LIMIT 1");
        q.addBindValue(chainId); q.addBindValue(minP); q.exec();
        if (q.next()) byPeriod[q.value(0).toLongLong()] = q.value(1).isNull() ? NAN : q.value(1).toDouble();
      }
      int changedHere = 0, revisedHere = 0;
      qint64 firstIns = std::numeric_limits<qint64>::max(), lastIns = std::numeric_limits<qint64>::min();
      QSet<int> months;   // months of the inserted facts (their monthly cells are rebuilt)
      QSqlQuery ins(m_db);
      ins.prepare("INSERT INTO facts(chain, revises, period, target, at, recorded, value, text, origin, note) VALUES(?,?,?,?,?,?,?,?,?,?)");
      for (const Obs* o : list) {
        if (!o->period.isValid()) continue;   // undated; a period of 0 ms (January 1970) is a real one
        const qint64 p = ms(o->period), t = ms(o->target);
        const auto key = qMakePair(p, t);
        qint64 revises = 0;
        if (auto s = known.constFind(key); s != known.cend()) {
          if (same(s->value, o->value) && s->text == o->text) continue;   // already known
          // nothing is refused (user's rule, 2026-11-16): a new figure, even one the period had before, is a revision. False
          // revisions came from mixing sources or periods in one chain; the importer keeps one source per chain instead
          revises = s->id;                                                  // a new value for a known period
        } else if (changesMode) {
          // keep only changes: skip when equal to the latest value before this period
          auto prevIt = byPeriod.lowerBound(p);
          if (prevIt != byPeriod.begin()) { --prevIt; if (same(prevIt.value(), o->value)) continue; }
        }
        ins.addBindValue(chainId);
        ins.addBindValue(revises ? QVariant(revises) : QVariant(QMetaType(QMetaType::LongLong)));
        ins.addBindValue(p); ins.addBindValue(t);
        ins.addBindValue(o->at.isValid() ? QVariant(ms(o->at)) : QVariant(QMetaType(QMetaType::LongLong)));
        ins.addBindValue(now);
        ins.addBindValue(std::isnan(o->value) ? QVariant(QMetaType(QMetaType::Double)) : QVariant(o->value));
        ins.addBindValue(o->text); ins.addBindValue(origin); ins.addBindValue(o->note);
        if (!ins.exec()) { qWarning("[macro-data] insert: %s", qPrintable(ins.lastError().text())); continue; }
        const qint64 id = ins.lastInsertId().toLongLong();
        known[key] = Slot{id, o->value, o->text};
        byPeriod[p] = o->value;
        ++changedHere;
        if (revises) { ++revised; ++revisedHere; } else ++added;
        firstIns = std::min(firstIns, p); lastIns = std::max(lastIns, p);
        months.insert(monthIndex(fromMs(p).date()));
      }
      if (changedHere) {
        // every new fact sits at or after firstIns in slot order: the links, totals and months before it stay as they are
        relink(chainId, firstIns);
        addStats(chainId, changedHere, revisedHere, firstIns, lastIns);
        updateMonthly(chainId, months);
        touched << it.key();
      }
    }
    m_db.commit();
    return {added, revised, touched};
  }

private:
  // a chain becomes "live" once its first import is over: later facts are observations as they appear
  bool live(qint64 chainId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM facts WHERE chain = ? LIMIT 1"); q.addBindValue(chainId); q.exec();
    return q.next();
  }
  // each slot (period, target) in recorded order: the first fact revises nothing, an exact repeat of the slot's latest
  // value is removed, any other fact (a return to an earlier value included) revises the slot's latest
  void renumber(qint64 chainId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id, period, target, value, text, revises FROM facts WHERE chain = ? ORDER BY period, target, recorded, id");
    q.addBindValue(chainId); q.exec();
    struct Kept { qint64 id; double value; QString text; };
    QHash<QPair<qint64, qint64>, QList<Kept>> slotMap;   // not "slots": a Qt macro
    QList<qint64> remove;
    QList<QPair<qint64, qint64>> setRevises;   // id → revises (0: none)
    while (q.next()) {
      const qint64 id = q.value(0).toLongLong();
      const auto key = qMakePair(q.value(1).toLongLong(), q.value(2).toLongLong());
      const double v = q.value(3).isNull() ? NAN : q.value(3).toDouble();
      const QString t = q.value(4).toString();
      const qint64 rev = q.value(5).isNull() ? 0 : q.value(5).toLongLong();
      QList<Kept>& kept = slotMap[key];
      if (!kept.isEmpty() && same(kept.last().value, v) && kept.last().text == t) { remove << id; continue; }
      const qint64 want = kept.isEmpty() ? 0 : kept.last().id;
      if (rev != want) setRevises << qMakePair(id, want);
      kept << Kept{id, v, t};
    }
    QSqlQuery u(m_db);
    u.prepare("DELETE FROM facts WHERE id = ?");
    for (qint64 id : remove) { u.addBindValue(id); u.exec(); }
    u.prepare("UPDATE facts SET revises = ? WHERE id = ?");
    for (const auto& [id, r] : setRevises) { u.addBindValue(r ? QVariant(r) : QVariant(QMetaType(QMetaType::LongLong))); u.addBindValue(id); u.exec(); }
  }
  // prev = the fact before in (period, target, recorded) order; only rows whose link changed are written. With `from`,
  // only the facts dated from it on are walked (the ones before are linked already), starting from the last one before.
  void relink(qint64 chainId, qint64 from = std::numeric_limits<qint64>::min()) {
    QSqlQuery q(m_db);
    qint64 before = 0;
    if (from != std::numeric_limits<qint64>::min()) {
      q.prepare("SELECT id FROM facts WHERE chain = ? AND period < ? ORDER BY period DESC, target DESC, recorded DESC, id DESC LIMIT 1");
      q.addBindValue(chainId); q.addBindValue(from); q.exec();
      if (q.next()) before = q.value(0).toLongLong();
    }
    q.prepare("SELECT id, prev FROM facts WHERE chain = ? AND period >= ? ORDER BY period, target, recorded, id");
    q.addBindValue(chainId); q.addBindValue(from); q.exec();
    QList<QPair<qint64, qint64>> fix;
    while (q.next()) {
      const qint64 id = q.value(0).toLongLong(), prev = q.value(1).isNull() ? 0 : q.value(1).toLongLong();
      if (prev != before) fix << qMakePair(id, before);
      before = id;
    }
    QSqlQuery u(m_db);
    u.prepare("UPDATE facts SET prev = ? WHERE id = ?");
    for (const auto& [id, prev] : fix) {
      u.addBindValue(prev ? QVariant(prev) : QVariant(QMetaType(QMetaType::LongLong))); u.addBindValue(id); u.exec();
    }
  }
public:
  // One-time repair (meta repair.sources = 1) of what was recorded before 2026-11-16, when one chain could mix sources and
  // periods: false revisions appeared, nothing real is thrown away here.
  //  - price "revisions" from a flat settlement bar: the close stays (the settlement, a real figure), the copies of it
  //    given as open / high / low go;
  //  - implied-path "revisions" were observations later in the day: they become observations of their own hour;
  //  - a central bank's feed written into the BIS rate chain: taken out, recorded again by the next rates pass (same date,
  //    same value) in the chain of that feed;
  //  - calendar actuals (Myfxbook, headlines, official series and ForexFactory's "previous" mixed, and YoY figures given
  //    to m/m events) and decisions (statements and inferences mixed, text in the interface language): those chains are
  //    removed and recorded again from their sources at once, one chain per source.
  // Touched chains are then renumbered (exact repeats only), relinked and counted again.
  bool repairSources() {
    QSqlQuery q(m_db);
    q.exec("SELECT v FROM meta WHERE k = 'repair.sources'");
    if (q.next() && q.value(0).toString() == "1") return false;
    QSet<qint64> drop, touched;
    // flat settlement bars: open / high / low copies of the close
    q.exec("SELECT f.id, c.key, f.period, f.recorded, f.value, f.chain FROM facts f JOIN chains c ON c.id = f.chain "
           "WHERE c.family = 'prices' AND f.revises IS NOT NULL");
    struct Px { QList<qint64> ids, chains; QList<double> values; QStringList fields; };
    QHash<QString, Px> bars;
    while (q.next()) {
      const QString key = q.value(1).toString();
      const QString k = key.section(':', 1, 1) + "|" + q.value(2).toString() + "|" + q.value(3).toString();
      Px& b = bars[k];
      b.ids << q.value(0).toLongLong(); b.chains << q.value(5).toLongLong(); b.values << q.value(4).toDouble(); b.fields << key.section(':', 2, 2);
    }
    for (const auto& b : bars) {
      if (b.ids.size() != 4) continue;
      bool flat = true;
      for (double v : b.values) if (!same(v, b.values.first())) flat = false;
      if (!flat) continue;
      for (int i = 0; i < 4; ++i) if (b.fields[i] != "close") { drop << b.ids[i]; touched << b.chains[i]; }
    }
    m_db.transaction();
    QSqlQuery u(m_db);
    // implied path: the later observations of a day become observations of their own hour
    q.exec("SELECT f.id, f.chain, f.recorded FROM facts f JOIN chains c ON c.id = f.chain WHERE c.key LIKE 'implied:%' AND f.revises IS NOT NULL");
    u.prepare("UPDATE facts SET period = ?, revises = NULL WHERE id = ?");
    while (q.next()) {
      const QDateTime r = QDateTime::fromMSecsSinceEpoch(q.value(2).toLongLong(), QTimeZone::UTC);
      u.addBindValue(QDateTime(r.date(), QTime(r.time().hour(), 0), QTimeZone::UTC).toMSecsSinceEpoch()); u.addBindValue(q.value(0)); u.exec();
      touched << q.value(1).toLongLong();
    }
    // central-bank feeds written into the BIS rate chains: out of them; the next rates pass records them again (same date,
    // same value) in the chain of the feed they come from (its English name in the key)
    q.exec("SELECT f.id, f.chain FROM facts f JOIN chains c ON c.id = f.chain WHERE c.key LIKE 'rate:%' "
           "AND IFNULL(f.note, '') NOT LIKE 'BIS %'");
    while (q.next()) { drop << q.value(0).toLongLong(); touched << q.value(1).toLongLong(); }
    q.exec("UPDATE chains SET source = 'BIS WS_CBPOL' WHERE key LIKE 'rate:%' AND key NOT LIKE 'rate:%:%'");
    // implied chains: one way of pricing per bank so far, its name was stored in the interface language and with a date;
    // now in English without the date (CentralBanks pricingSourceEn), the same chain goes on
    const QList<QPair<const char*, const char*>> priced{
        {"implied:fed", "computed by Deepwater from CME 30-day fed funds futures (ZQ, via Yahoo Finance)"},
        {"implied:ecb", "computed by Deepwater from CME 3-month €STR futures (ESR, via Yahoo Finance)"},
        {"implied:boc", "computed by Deepwater from Montréal Exchange 3-month CORRA futures (CRA, settlement prices)"},
        {"implied:boe", "computed by Deepwater from the Bank of England's daily OIS (SONIA) instantaneous forward curve"},
        {"implied:boj", "computed by Deepwater from JPX/OSE 3-month TONA futures settlement prices and the BoJ's TONA"},
        {"implied:snb", "computed by Deepwater from Eurex 3M SARON futures (FSR3) daily settlement prices and the SNB's SARON"},
        {"implied:rba", "FinancialJuice probability pictures, read by Deepwater (OCR, validated)"},
        {"implied:rbnz", "FinancialJuice probability pictures, read by Deepwater (OCR, validated)"}};
    for (const auto& [key, en] : priced) {
      u.prepare("UPDATE chains SET source = ? WHERE key = ?"); u.addBindValue(QString::fromUtf8(en)); u.addBindValue(QString::fromUtf8(key)); u.exec();
    }
    // calendar actuals and decisions: recorded again, one chain per source
    QList<qint64> rebuilt;
    q.exec("SELECT id FROM chains WHERE (family = 'calendar' AND kind = 'actual') OR key LIKE 'decision:%'");
    while (q.next()) rebuilt << q.value(0).toLongLong();
    for (qint64 id : drop) { u.prepare("DELETE FROM facts WHERE id = ?"); u.addBindValue(id); u.exec(); }
    for (qint64 c : rebuilt) {
      for (const char* sql : {"DELETE FROM facts WHERE chain = ?", "DELETE FROM chain_stats WHERE chain = ?",
                              "DELETE FROM facts_monthly WHERE chain = ?", "DELETE FROM chains WHERE id = ?"}) {
        u.prepare(sql); u.addBindValue(c); u.exec();
      }
      touched.remove(c);
    }
    // the consensus chains pointed at the old actual chain
    q.exec("UPDATE chains SET target_chain = target_chain || ':forexfactory' WHERE family = 'calendar' AND kind = 'consensus' "
           "AND target_chain NOT LIKE '%:forexfactory'");
    if (!rebuilt.isEmpty()) q.exec("DELETE FROM meta WHERE k IN ('last.calendar', 'last.rates')");   // the next start records them
    for (qint64 c : touched) { renumber(c); relink(c); stats(c); monthly(c); }
    q.exec("INSERT INTO meta(k, v) VALUES('repair.sources', '1') ON CONFLICT(k) DO UPDATE SET v = excluded.v");
    m_db.commit();
    qInfo("[macro-data] repair: %lld facts taken out (settlement-close copies, feeds in BIS chains), %lld chains renumbered, %lld calendar and "
          "decision chains to record again by source", qint64(drop.size()), qint64(touched.size()), qint64(rebuilt.size()));
    return true;
  }
  // One-time repair (meta repair.bisfeed = 1), audit 2026-11-16 D5: before that day the rates pass read
  // MacroStore::series(PolicyRate), which writes the central-bank feed's current value into this month's slot, so the BIS
  // chains ("rate:XX", note "BIS …") got feed values as BIS figures (and false revisions when the feed moved). `keep` holds,
  // per BIS chain, the raw BIS series (period ms → value) the importer now writes. A fact goes only when its (period, value)
  // isn't a BIS point AND it was recorded during the month it is dated (the feed's signature); facts before the series'
  // first point are left alone. Then the chain is renumbered (its revisions recomputed), relinked and counted again.
  int pruneBisChains(const QHash<QString, QHash<qint64, double>>& keep) {
    QSqlQuery q(m_db);
    q.exec("SELECT v FROM meta WHERE k = 'repair.bisfeed'");
    if (q.next() && q.value(0).toString() == "1") return -1;
    if (!m_db.isOpen()) return -1;
    m_db.transaction();
    QSqlQuery u(m_db);
    u.prepare("DELETE FROM facts WHERE id = ?");
    int dropped = 0;
    QList<qint64> touched;
    for (auto it = keep.cbegin(); it != keep.cend(); ++it) {
      if (it.value().isEmpty() || it.key().count(':') != 1 || !it.key().startsWith("rate:")) continue;
      q.prepare("SELECT id FROM chains WHERE key = ?"); q.addBindValue(it.key());
      if (!q.exec() || !q.next()) continue;
      const qint64 chainId = q.value(0).toLongLong();
      qint64 first = std::numeric_limits<qint64>::max();
      for (auto p = it.value().cbegin(); p != it.value().cend(); ++p) first = std::min(first, p.key());
      q.prepare("SELECT id, period, value, recorded FROM facts WHERE chain = ? AND IFNULL(note, '') LIKE 'BIS %'");
      q.addBindValue(chainId); q.exec();
      int here = 0;
      while (q.next()) {
        const qint64 period = q.value(1).toLongLong();
        if (period < first) continue;
        const double v = q.value(2).isNull() ? NAN : q.value(2).toDouble();
        const auto k = it.value().constFind(period);
        if (k != it.value().cend() && same(k.value(), v)) continue;
        // the feed's signature: recorded during the month it is dated (BIS publishes a month once it's over). A BIS
        // figure recorded later that merely differs from today's series (a BIS revision, another data set) stays.
        const QDate pm = fromMs(period).date();
        if (fromMs(q.value(3).toLongLong()).date() >= QDate(pm.year(), pm.month(), 1).addMonths(1)) continue;
        u.addBindValue(q.value(0)); u.exec();
        ++here;
      }
      if (here) { dropped += here; touched << chainId; }
    }
    for (qint64 c : touched) { renumber(c); relink(c); stats(c); monthly(c); }
    q.exec("INSERT INTO meta(k, v) VALUES('repair.bisfeed', '1') ON CONFLICT(k) DO UPDATE SET v = excluded.v");
    m_db.commit();
    qInfo("[macro-data] repair: %d central-bank feed values taken out of %lld BIS rate chains", dropped, qint64(touched.size()));
    return dropped;
  }
  // databases written before facts_monthly existed: every chain, once (worker thread, after open)
  bool needsMonthly() {
    QSqlQuery q(m_db);
    q.exec("SELECT (SELECT COUNT(*) FROM facts_monthly) = 0 AND EXISTS(SELECT 1 FROM facts)");
    return q.next() && q.value(0).toBool();
  }
  void backfillMonthly() {
    QSqlQuery q(m_db);
    q.exec("SELECT id FROM chains");
    QList<qint64> ids;
    while (q.next()) ids << q.value(0).toLongLong();
    m_db.transaction();
    for (qint64 id : ids) {
      if (stop.load(std::memory_order_relaxed)) { m_db.rollback(); return; }
      monthly(id);
    }
    m_db.commit();
  }
  // rebuilds the monthly cells of one chain: latest value of each slot (revisions counted), last slot of each month,
  // rank of each month among the chain's months
  bool forecastKind(qint64 chainId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT kind FROM chains WHERE id = ?"); q.addBindValue(chainId); q.exec();
    const QString kind = q.next() ? q.value(0).toString() : QString();
    return kind == "forecast" || kind == "consensus" || kind == "implied";
  }
  // monthly cells (ranks not set) from facts rows (period, target, value, revised) in slot order
  static QMap<int, MonthCell> cellsOf(QSqlQuery& q, bool fc) {
    struct Slot { double value = NAN; int n = 0, rev = 0; };
    QMap<QPair<qint64, qint64>, Slot> bySlot;   // ordered by x
    while (q.next()) {
      const qint64 p = q.value(0).toLongLong(), t = q.value(1).toLongLong();
      const auto key = fc ? qMakePair(t ? t : p, 0LL) : qMakePair(p, t);
      Slot& s = bySlot[key];
      s.value = q.value(2).isNull() ? NAN : q.value(2).toDouble();
      ++s.n; if (q.value(3).toBool()) ++s.rev;
    }
    QMap<int, MonthCell> months;
    for (auto it = bySlot.cbegin(); it != bySlot.cend(); ++it) {
      if (std::isnan(it->value)) continue;
      const int m = monthIndex(fromMs(it.key().first).date());
      MonthCell& c = months[m];
      c.month = m; c.value = float(it->value); c.count += it->n; c.revisions += it->rev;
    }
    return months;
  }
  // ranks: mean position of equal values, scaled to 0..1
  static void rank(QMap<int, MonthCell>& months) {
    QVector<float> sorted; sorted.reserve(months.size());
    for (const auto& c : months) sorted << c.value;
    std::sort(sorted.begin(), sorted.end());
    const int n = int(sorted.size());
    for (auto& c : months) {
      const auto lo = std::lower_bound(sorted.begin(), sorted.end(), c.value) - sorted.begin();
      const auto hi = std::upper_bound(sorted.begin(), sorted.end(), c.value) - sorted.begin();
      c.rank = n > 1 ? float((lo + hi - 1) / 2.0 / (n - 1)) : 0.5f;
    }
  }
  // After facts were added in `touched` months: those months are rebuilt from their facts, the others are read back
  // from facts_monthly, every rank is recomputed and only the rows that differ are written. Same cells as monthly().
  // Forecast kinds (a cell's month comes from the target) and chains without cells yet take the full rebuild.
  void updateMonthly(qint64 chainId, const QSet<int>& touched) {
    if (forecastKind(chainId)) return monthly(chainId);
    QSqlQuery q(m_db);
    q.prepare("SELECT month, value, rank, n, rev FROM facts_monthly WHERE chain = ?"); q.addBindValue(chainId); q.exec();
    QMap<int, MonthCell> stored;
    while (q.next()) {
      MonthCell c; c.month = q.value(0).toInt(); c.value = q.value(1).toFloat(); c.rank = q.value(2).toFloat();
      c.count = q.value(3).toInt(); c.revisions = q.value(4).toInt();
      stored.insert(c.month, c);
    }
    if (stored.isEmpty()) return monthly(chainId);
    QMap<int, MonthCell> months = stored;
    for (int m : touched) {
      months.remove(m);
      const QDate d = monthDate(m);
      q.prepare("SELECT period, target, value, revises IS NOT NULL FROM facts WHERE chain = ? AND period >= ? AND period < ? "
                "ORDER BY period, target, recorded, id");
      q.addBindValue(chainId);
      q.addBindValue(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
      q.addBindValue(QDateTime(d.addMonths(1), QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
      q.exec();
      const QMap<int, MonthCell> rebuilt = cellsOf(q, false);
      if (const auto c = rebuilt.constFind(m); c != rebuilt.cend()) months.insert(m, *c);
    }
    rank(months);
    QSqlQuery w(m_db);
    w.prepare("DELETE FROM facts_monthly WHERE chain = ? AND month = ?");
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
      if (!months.contains(it.key())) { w.addBindValue(chainId); w.addBindValue(it.key()); w.exec(); }
    w.prepare("INSERT INTO facts_monthly(chain, month, value, rank, n, rev) VALUES(?,?,?,?,?,?) ON CONFLICT(chain, month) DO UPDATE SET "
              "value = excluded.value, rank = excluded.rank, n = excluded.n, rev = excluded.rev");
    for (const auto& c : std::as_const(months)) {
      if (const auto s = stored.constFind(c.month); s != stored.cend() && s->value == c.value && s->rank == c.rank &&
                                                     s->count == c.count && s->revisions == c.revisions)
        continue;
      w.addBindValue(chainId); w.addBindValue(c.month); w.addBindValue(double(c.value)); w.addBindValue(double(c.rank));
      w.addBindValue(c.count); w.addBindValue(c.revisions); w.exec();
    }
  }
  void monthly(qint64 chainId) {
    const bool fc = forecastKind(chainId);
    QSqlQuery q(m_db);
    q.prepare("SELECT period, target, value, revises IS NOT NULL FROM facts WHERE chain = ? ORDER BY period, target, recorded, id");
    q.addBindValue(chainId); q.exec();
    QMap<int, MonthCell> months = cellsOf(q, fc);
    rank(months);
    q.prepare("DELETE FROM facts_monthly WHERE chain = ?"); q.addBindValue(chainId); q.exec();
    QSqlQuery ins(m_db);
    ins.prepare("INSERT INTO facts_monthly(chain, month, value, rank, n, rev) VALUES(?,?,?,?,?,?)");
    for (const auto& c : months) {
      ins.addBindValue(chainId); ins.addBindValue(c.month); ins.addBindValue(double(c.value)); ins.addBindValue(double(c.rank));
      ins.addBindValue(c.count); ins.addBindValue(c.revisions); ins.exec();
    }
  }
private:
  void stats(qint64 chainId) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO chain_stats(chain, count, revisions, first, last) SELECT ?, COUNT(*), SUM(revises IS NOT NULL), "
              "MIN(period), MAX(period) FROM facts WHERE chain = ? ON CONFLICT(chain) DO UPDATE SET count = excluded.count, "
              "revisions = excluded.revisions, first = excluded.first, last = excluded.last");
    q.addBindValue(chainId); q.addBindValue(chainId); q.exec();
  }
  // `inserted` new facts (`revisions` of them revise a slot) dated first..last: the totals move by that much; a chain
  // without totals yet is counted in full
  void addStats(qint64 chainId, int inserted, int revisions, qint64 first, qint64 last) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE chain_stats SET count = count + ?, revisions = revisions + ?, first = MIN(first, ?), last = MAX(last, ?) "
              "WHERE chain = ? AND count > 0 AND first IS NOT NULL AND last IS NOT NULL AND revisions IS NOT NULL");
    q.addBindValue(inserted); q.addBindValue(revisions); q.addBindValue(first); q.addBindValue(last); q.addBindValue(chainId);
    if (!q.exec() || q.numRowsAffected() != 1) stats(chainId);
  }
  QString m_path;
  QSqlDatabase m_db;
};

// ------------------------------------------------------------------------------------------------ store
Store& Store::instance() { static Store* s = new Store; return *s; }

QString Store::path() const { return paths::data("macro-data") + "/macro-data.sqlite"; }

Store::Store() {
  paths::moveDatabase(paths::data() + "/facts/facts.sqlite", path());   // before 2026-11-18
  m_thread = new QThread(this);
  m_thread->setObjectName("macrodata-writer");
  m_writer = new Writer(path());
  m_writer->moveToThread(m_thread);
  m_thread->start(QThread::LowPriority);
  QMetaObject::invokeMethod(m_writer, [w = m_writer] { w->open(); }, Qt::BlockingQueuedConnection);
  // chains that mixed sources or periods before 2026-11-16 (false revisions): once
  QMetaObject::invokeMethod(m_writer, [this] {
    if (!m_writer->repairSources()) return;
    QMetaObject::invokeMethod(this, [this] { emit changed({}); emit monthlyReady(); }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
  // monthly cells of a database written before they existed: built in the background, the 3D views wait for them
  QMetaObject::invokeMethod(m_writer, [this] {
    if (!m_writer->needsMonthly()) return;
    m_writer->backfillMonthly();
    QMetaObject::invokeMethod(this, [this] { emit monthlyReady(); }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
  // No timeout: a writer still inside SQLite when the application object and the SQL driver are torn down could crash
  // or lose the batch. The wait covers the job running when quit() is called: one batch, or the backfill, which stops.
  connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
    m_writer->stop = true;
    m_thread->quit();
    m_thread->wait();
  });
}

void Store::add(const QList<ChainDef>& chains, const QList<Obs>& obs, std::function<void(int, int)> done) {
  ++m_pending;
  QMetaObject::invokeMethod(m_writer, [this, chains, obs, done] {
    const auto [a, r, keys] = m_writer->write(chains, obs);
    QMetaObject::invokeMethod(this, [this, a, r, keys, done] {
      --m_pending;
      if (!keys.isEmpty()) emit changed(keys);
      if (done) done(a, r);
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::repairBisChains(const QHash<QString, QHash<qint64, double>>& keep) {
  ++m_pending;
  QMetaObject::invokeMethod(m_writer, [this, keep] {
    const int dropped = m_writer->pruneBisChains(keep);
    QMetaObject::invokeMethod(this, [this, dropped] {
      --m_pending;
      if (dropped > 0) { emit changed({}); emit monthlyReady(); }
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}

void Store::setMeta(const QString& k, const QString& v) {
  QMetaObject::invokeMethod(m_writer, [w = m_writer, k, v] { w->setMeta(k, v); }, Qt::QueuedConnection);
}

QSqlDatabase Store::reader() const {
  const QString name = "macrodata-reader";
  if (QSqlDatabase::contains(name)) return QSqlDatabase::database(name);
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
  db.setDatabaseName(path());
  db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=2000");
  db.open();
  return db;
}

static ChainDef defOf(const QSqlQuery& q, int o) {
  ChainDef d;
  d.key = q.value(o).toString(); d.family = q.value(o + 1).toString(); d.subject = q.value(o + 2).toString();
  d.indicator = q.value(o + 3).toString(); d.kind = q.value(o + 4).toString(); d.source = q.value(o + 5).toString();
  d.unit = q.value(o + 6).toString(); d.freq = q.value(o + 7).toString(); d.mode = q.value(o + 8).toString();
  d.targetChain = q.value(o + 9).toString();
  return d;
}

QList<Chain> Store::chains() const {
  QList<Chain> out;
  QSqlQuery q(reader());
  q.exec("SELECT c.id, c.key, c.family, c.subject, c.indicator, c.kind, c.source, c.unit, c.freq, c.mode, c.target_chain, "
         "IFNULL(s.count, 0), IFNULL(s.revisions, 0), IFNULL(s.first, 0), IFNULL(s.last, 0) FROM chains c "
         "LEFT JOIN chain_stats s ON s.chain = c.id ORDER BY c.family, c.subject, c.indicator, c.kind");
  while (q.next()) {
    Chain c; c.id = q.value(0).toLongLong(); c.def = defOf(q, 1);
    c.count = q.value(11).toInt(); c.revisions = q.value(12).toInt(); c.first = q.value(13).toLongLong(); c.last = q.value(14).toLongLong();
    out << c;
  }
  return out;
}

Chain Store::chain(const QString& key) const {
  // one row by its key (the importer asks for a few chains on the GUI thread: never the whole list for one)
  QSqlQuery q(reader());
  q.prepare("SELECT c.id, c.key, c.family, c.subject, c.indicator, c.kind, c.source, c.unit, c.freq, c.mode, c.target_chain, "
            "IFNULL(s.count, 0), IFNULL(s.revisions, 0), IFNULL(s.first, 0), IFNULL(s.last, 0) FROM chains c "
            "LEFT JOIN chain_stats s ON s.chain = c.id WHERE c.key = ?");
  q.addBindValue(key);
  Chain c;
  if (!q.exec() || !q.next()) return c;
  c.id = q.value(0).toLongLong(); c.def = defOf(q, 1);
  c.count = q.value(11).toInt(); c.revisions = q.value(12).toInt(); c.first = q.value(13).toLongLong(); c.last = q.value(14).toLongLong();
  return c;
}

QVector<Fact> Store::facts(qint64 chainId) const {
  QVector<Fact> out;
  QSqlQuery q(reader());
  q.setForwardOnly(true);
  q.prepare("SELECT id, chain, prev, revises, period, target, at, recorded, value, text, origin, note FROM facts "
            "WHERE chain = ? ORDER BY period, target, recorded, id");
  q.addBindValue(chainId); q.exec();
  while (q.next()) {
    Fact f;
    f.id = q.value(0).toLongLong(); f.chain = q.value(1).toLongLong(); f.prev = q.value(2).toLongLong(); f.revises = q.value(3).toLongLong();
    f.period = q.value(4).toLongLong(); f.target = q.value(5).toLongLong(); f.at = q.value(6).toLongLong(); f.recorded = q.value(7).toLongLong();
    f.value = q.value(8).isNull() ? NAN : q.value(8).toDouble();
    f.text = q.value(9).toString(); f.origin = q.value(10).toString(); f.note = q.value(11).toString();
    out << f;
  }
  return out;
}

Fact Store::fact(qint64 id) const {
  QSqlQuery q(reader());
  q.prepare("SELECT id, chain, prev, revises, period, target, at, recorded, value, text, origin, note FROM facts WHERE id = ?");
  q.addBindValue(id); q.exec();
  Fact f;
  if (!q.next()) return f;
  f.id = q.value(0).toLongLong(); f.chain = q.value(1).toLongLong(); f.prev = q.value(2).toLongLong(); f.revises = q.value(3).toLongLong();
  f.period = q.value(4).toLongLong(); f.target = q.value(5).toLongLong(); f.at = q.value(6).toLongLong(); f.recorded = q.value(7).toLongLong();
  f.value = q.value(8).isNull() ? NAN : q.value(8).toDouble();
  f.text = q.value(9).toString(); f.origin = q.value(10).toString(); f.note = q.value(11).toString();
  return f;
}

QString Store::meta(const QString& k) const {
  QSqlQuery q(reader());
  q.prepare("SELECT v FROM meta WHERE k = ?"); q.addBindValue(k); q.exec();
  return q.next() ? q.value(0).toString() : QString();
}

qint64 Store::totalFacts() const {
  QSqlQuery q(reader());
  q.exec("SELECT IFNULL(SUM(count), 0) FROM chain_stats");
  return q.next() ? q.value(0).toLongLong() : 0;
}
void Store::loadMonthly(QObject* ctx, std::function<void(const MonthMap&)> done) const {
  const QString file = path();
  auto* w = new QFutureWatcher<MonthMap>(ctx);
  QObject::connect(w, &QFutureWatcher<MonthMap>::finished, ctx, [w, done] { done(w->result()); w->deleteLater(); });
  w->setFuture(QtConcurrent::run([file] {
    MonthMap out;
    const QString name = QString("macrodata-monthly-%1").arg(quintptr(QThread::currentThreadId()));
    {
      QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
      db.setDatabaseName(file);
      db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=5000");
      if (db.open()) {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        q.exec("SELECT chain, month, value, rank, n, rev FROM facts_monthly ORDER BY chain, month");
        while (q.next()) {
          MonthCell c;
          c.month = q.value(1).toInt(); c.value = q.value(2).toFloat(); c.rank = q.value(3).toFloat();
          c.count = q.value(4).toInt(); c.revisions = q.value(5).toInt();
          out[q.value(0).toLongLong()] << c;
        }
      }
      db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return out;
  }));
}
}  // namespace macrodata
