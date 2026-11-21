#include "trials/Registry.h"
#include "core/Paths.h"
#include <QAtomicInt>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtEndian>
#include <cstring>

namespace trials {

namespace {

QMutex& lock() {
  static QMutex m;
  return m;
}

// One connection for one call, closed after it: callers come from any thread (QSqlDatabase belongs to the thread that
// opened it), and a try is written once per backtest or training, so opening is no cost worth sharing.
class Conn {
public:
  explicit Conn(const QString& path) {
    static QAtomicInt seq;
    m_name = QString("trials-%1").arg(seq.fetchAndAddRelaxed(1));
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_name);
    m_db.setDatabaseName(path);
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    m_open = m_db.open();
    if (m_open) QSqlQuery(m_db).exec("PRAGMA journal_mode=WAL");
  }
  ~Conn() {
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_name);
  }
  bool open() const { return m_open; }
  QSqlDatabase& db() { return m_db; }

private:
  QString m_name;
  QSqlDatabase m_db;
  bool m_open = false;
};

const char* const kColumns =
    "id, at, kind, origin, who, place, family, config, label, market, timeframe, ref, period_from, period_to, n, sr, skew, "
    "kurt, sharpe, net_pct, score_key, score, reference, edge, detail, day0";

// a null QString binds as SQL NULL: the text columns are NOT NULL, an empty one is ''
QVariant text(const QString& s) { return s.isNull() ? QVariant(QStringLiteral("")) : QVariant(s); }
QVariant real(double v) { return std::isfinite(v) ? QVariant(v) : QVariant(QMetaType::fromType<double>()); }
double realOf(const QVariant& v, double fallback) { return v.isNull() ? fallback : v.toDouble(); }

Trial rowOf(const QSqlQuery& q) {
  Trial t;
  int i = 0;
  t.id = q.value(i++).toLongLong();
  t.at = q.value(i++).toLongLong();
  t.kind = q.value(i++).toString();
  t.origin = q.value(i++).toString();
  t.who = q.value(i++).toString();
  t.place = q.value(i++).toString();
  t.family = q.value(i++).toString();
  t.config = q.value(i++).toString();
  t.label = q.value(i++).toString();
  t.market = q.value(i++).toString();
  t.timeframe = q.value(i++).toString();
  t.ref = q.value(i++).toString();
  t.from = q.value(i++).toLongLong();
  t.to = q.value(i++).toLongLong();
  t.n = q.value(i++).toInt();
  t.sr = realOf(q.value(i++), NAN);
  t.skew = realOf(q.value(i++), 0);
  t.kurt = realOf(q.value(i++), 3);
  t.sharpe = realOf(q.value(i++), NAN);
  t.netPct = realOf(q.value(i++), NAN);
  t.scoreKey = q.value(i++).toString();
  t.score = realOf(q.value(i++), NAN);
  t.reference = realOf(q.value(i++), NAN);
  t.edge = realOf(q.value(i++), NAN);
  t.detail = QJsonDocument::fromJson(q.value(i++).toString().toUtf8()).object();
  t.day0 = q.value(i++).toLongLong();
  return t;
}

QByteArray packDaily(const QVector<float>& d) {
  if (d.isEmpty()) return {};
  QByteArray raw(d.size() * 4, Qt::Uninitialized);
  for (int i = 0; i < d.size(); ++i) {
    quint32 u;
    std::memcpy(&u, &d[i], 4);
    qToLittleEndian(u, raw.data() + i * 4);
  }
  return qCompress(raw, 9);
}
QVector<float> unpackDaily(const QByteArray& blob) {
  const QByteArray raw = qUncompress(blob);
  QVector<float> d(raw.size() / 4);
  for (int i = 0; i < d.size(); ++i) {
    const quint32 u = qFromLittleEndian<quint32>(raw.constData() + i * 4);
    std::memcpy(&d[i], &u, 4);
  }
  return d;
}

}  // namespace

Registry& Registry::instance() {
  static Registry* r = new Registry(paths::data("trials") + "/trials.sqlite");
  return *r;
}

Registry::Registry(const QString& path) : m_path(path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QMutexLocker l(&lock());
  Conn c(m_path);
  if (!c.open()) return;
  QSqlQuery q(c.db());
  const char* const schema[] = {
      "CREATE TABLE IF NOT EXISTS trials (id INTEGER PRIMARY KEY, at INTEGER NOT NULL, kind TEXT NOT NULL, origin TEXT NOT NULL, "
      "who TEXT NOT NULL DEFAULT '', place TEXT NOT NULL DEFAULT '', family TEXT NOT NULL, config TEXT NOT NULL, "
      "label TEXT NOT NULL DEFAULT '', market TEXT NOT NULL DEFAULT '', timeframe TEXT NOT NULL DEFAULT '', "
      "ref TEXT NOT NULL DEFAULT '', period_from INTEGER, period_to INTEGER, n INTEGER, sr REAL, skew REAL, kurt REAL, "
      "sharpe REAL, net_pct REAL, score_key TEXT NOT NULL DEFAULT '', score REAL, reference REAL, edge REAL, "
      "detail TEXT NOT NULL DEFAULT '{}', day0 INTEGER NOT NULL DEFAULT 0, daily BLOB)",
      "CREATE INDEX IF NOT EXISTS trials_family ON trials (family, config)",
      "CREATE INDEX IF NOT EXISTS trials_kind ON trials (kind, family)",
      "CREATE UNIQUE INDEX IF NOT EXISTS trials_ref ON trials (ref) WHERE ref <> ''",
      // append-only: a try, once written, can be neither changed nor removed
      "CREATE TRIGGER IF NOT EXISTS trials_no_update BEFORE UPDATE ON trials "
      "BEGIN SELECT RAISE(ABORT, 'the trial registry is append-only'); END",
      "CREATE TRIGGER IF NOT EXISTS trials_no_delete BEFORE DELETE ON trials "
      "BEGIN SELECT RAISE(ABORT, 'the trial registry is append-only'); END",
      "CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v TEXT NOT NULL)",
  };
  for (const char* s : schema)
    if (!q.exec(s)) { qWarning("trials: %s", qPrintable(q.lastError().text())); return; }
  m_ok = true;
}

qint64 Registry::add(const Trial& t) {
  if (!m_ok || t.family.isEmpty() || t.config.isEmpty()) return 0;
  QMutexLocker l(&lock());
  Conn c(m_path);
  if (!c.open()) return 0;
  QSqlQuery q(c.db());
  if (!t.ref.isEmpty()) {
    q.prepare("SELECT id FROM trials WHERE ref = ?");
    q.addBindValue(text(t.ref));
    if (q.exec() && q.next()) return q.value(0).toLongLong();
  }
  q.prepare("INSERT INTO trials (at, kind, origin, who, place, family, config, label, market, timeframe, ref, period_from, "
            "period_to, n, sr, skew, kurt, sharpe, net_pct, score_key, score, reference, edge, detail, day0, daily) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
  q.addBindValue(t.at);
  q.addBindValue(text(t.kind));
  q.addBindValue(text(t.origin));
  q.addBindValue(text(t.who));
  q.addBindValue(text(t.place));
  q.addBindValue(text(t.family));
  q.addBindValue(text(t.config));
  q.addBindValue(text(t.label));
  q.addBindValue(text(t.market));
  q.addBindValue(text(t.timeframe));
  q.addBindValue(text(t.ref));
  q.addBindValue(t.from);
  q.addBindValue(t.to);
  q.addBindValue(t.n);
  q.addBindValue(real(t.sr));
  q.addBindValue(real(t.skew));
  q.addBindValue(real(t.kurt));
  q.addBindValue(real(t.sharpe));
  q.addBindValue(real(t.netPct));
  q.addBindValue(text(t.scoreKey));
  q.addBindValue(real(t.score));
  q.addBindValue(real(t.reference));
  q.addBindValue(real(t.edge));
  q.addBindValue(QString::fromUtf8(QJsonDocument(t.detail).toJson(QJsonDocument::Compact)));
  q.addBindValue(t.day0);
  q.addBindValue(t.daily.isEmpty() ? QVariant(QMetaType::fromType<QByteArray>()) : QVariant(packDaily(t.daily)));
  if (!q.exec()) { qWarning("trials: %s", qPrintable(q.lastError().text())); return 0; }
  return q.lastInsertId().toLongLong();
}

bool Registry::hasRef(const QString& ref) const {
  if (!m_ok || ref.isEmpty()) return false;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("SELECT 1 FROM trials WHERE ref = ?");
  q.addBindValue(ref);
  return q.exec() && q.next();
}

QStringList Registry::refs(const QString& kind) const {
  QStringList out;
  if (!m_ok) return out;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("SELECT ref FROM trials WHERE kind = ? AND ref <> ''");
  q.addBindValue(kind);
  if (q.exec())
    while (q.next()) out << q.value(0).toString();
  return out;
}

QList<Trial> Registry::family(const QString& family) const {
  QList<Trial> out;
  if (!m_ok) return out;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare(QString("SELECT %1 FROM trials WHERE family = ? ORDER BY id").arg(kColumns));
  q.addBindValue(family);
  if (q.exec())
    while (q.next()) out << rowOf(q);
  return out;
}

QList<Registry::Family> Registry::families(const QString& kind) const {
  QList<Family> out;
  if (!m_ok) return out;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("SELECT family, MAX(market), MAX(timeframe), COUNT(DISTINCT config), MAX(at) FROM trials WHERE kind = ? "
            "GROUP BY family ORDER BY family");
  q.addBindValue(kind);
  if (q.exec())
    while (q.next()) out << Family{q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toInt(), q.value(4).toLongLong()};
  return out;
}

QHash<QString, QSet<QString>> Registry::configsByFamily(const QString& kind) const {
  QHash<QString, QSet<QString>> out;
  if (!m_ok) return out;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("SELECT DISTINCT family, config FROM trials WHERE kind = ?");
  q.addBindValue(kind);
  if (q.exec())
    while (q.next()) out[q.value(0).toString()].insert(q.value(1).toString());
  return out;
}

QVector<float> Registry::daily(qint64 id, qint64* day0) const {
  if (!m_ok) return {};
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("SELECT day0, daily FROM trials WHERE id = ?");
  q.addBindValue(id);
  if (!q.exec() || !q.next() || q.value(1).isNull()) return {};
  if (day0) *day0 = q.value(0).toLongLong();
  return unpackDaily(q.value(1).toByteArray());
}

qint64 Registry::count() const {
  if (!m_ok) return 0;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  return q.exec("SELECT COUNT(*) FROM trials") && q.next() ? q.value(0).toLongLong() : 0;
}

QString Registry::meta(const QString& k) const {
  if (!m_ok) return {};
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("SELECT v FROM meta WHERE k = ?");
  q.addBindValue(k);
  return q.exec() && q.next() ? q.value(0).toString() : QString();
}

void Registry::setMeta(const QString& k, const QString& v) {
  if (!m_ok) return;
  QMutexLocker l(&lock());
  Conn c(m_path);
  QSqlQuery q(c.db());
  q.prepare("INSERT OR REPLACE INTO meta (k, v) VALUES (?, ?)");
  q.addBindValue(k);
  q.addBindValue(v);
  q.exec();
}

QList<Trial> latestPerConfig(const QList<Trial>& rows) {
  QList<Trial> out;
  QHash<QString, int> at;
  for (const Trial& t : rows) {
    if (const auto it = at.constFind(t.config); it != at.cend()) { out[*it] = t; continue; }
    at.insert(t.config, int(out.size()));
    out << t;
  }
  return out;
}

}  // namespace trials
