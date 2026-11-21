// Binance's daily archives into technical-data.sqlite. See ArchiveCollector.h.
#include "technicaldata/ArchiveCollector.h"
#include "net/Http.h"
#include "net/Sources.h"
#include "technicaldata/FootprintArchive.h"
#include "technicaldata/PerpArchive.h"
#include "technicaldata/TechnicalDataSpec.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFutureWatcher>
#include <QPointer>
#include <QTimeZone>
#include <QtConcurrent>
#include <algorithm>

namespace technicaldata {

namespace {
const sources::Registrar regArchive({"binance-archive", "Binance (public data archives)", {"data.binance.vision"},
                                     sources::Access::Keyless, 250,
                                     "One zip a day per market: every trade (footprint history), and for the USD-M "
                                     "perpetual its order book depth and open interest (Data › Technical data)",
                                     "Binance's public market-data downloads (data.binance.vision), no key", {"data/technical-data"},
                                     {}, ArchiveCollector::kInFlight});

using Kind = ArchiveCollector::Kind;
qint64 dayStart(const QDate& d) { return QDateTime(d, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(); }
QDate dayOf(qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date(); }
QString key(const QString& market, Kind k) { return market + "|" + ArchiveCollector::kindId(k); }
QString dayKey(const QString& market, Kind k, const QDate& d) { return key(market, k) + "|" + d.toString(Qt::ISODate); }
bool isFootprint(Kind k) { return k == Kind::FootprintSpot || k == Kind::FootprintPerp; }
fp::Venue venueOf(Kind k) { return k == Kind::FootprintSpot ? fp::Venue::Spot : fp::Venue::Perp; }

QUrl zipUrl(Kind k, const QString& symbol, const QDate& day) {
  switch (k) {
    case Kind::Depth: return perp::depthUrl(symbol, day);
    case Kind::OpenInterest: return perp::metricsUrl(symbol, day);
    default: return fp::dailyUrl(venueOf(k), symbol, day);
  }
}
QUrl sumUrl(Kind k, const QString& symbol, const QDate& day) { return QUrl(zipUrl(k, symbol, day).toString() + ".CHECKSUM"); }

struct Built {
  QByteArray data;
  FpDay fpInfo;
  ArchDay info;
  std::vector<perp::OiRow> oi;
  QString error;
};

Built build(Kind k, const QByteArray& zip, const QDate& day, qint64 builtAt) {
  Built b;
  if (isFootprint(k)) {
    const fp::DayResult r = fp::buildDay(zip, day);
    if (!r.error.isEmpty()) { b.error = r.error; return b; }
    if (r.bad > 0 || r.outside > 0) {   // never seen: a file of another format, or a damaged one, is not stored
      b.error = QString("%1 unreadable lines, %2 trades of another day").arg(r.bad).arg(r.outside);
      return b;
    }
    b.data = fp::encodeDay(r.minutes);
    b.fpInfo.day = dayStart(day);
    b.fpInfo.minutes = int(r.minutes.size());
    b.fpInfo.trades = r.trades;
    for (const fp::Minute& m : r.minutes) b.fpInfo.levels += qint64(m.levels.size());
    b.fpInfo.bytes = b.data.size();
    b.fpInfo.zipBytes = zip.size();
    b.fpInfo.built = builtAt;
    return b;
  }
  // depth and metrics files are small (≤ 1 MB unzipped): read whole
  QByteArray csv;
  QString err;
  if (!fp::inflateZip(zip, [&csv](const char* p, size_t n) { csv.append(p, qsizetype(n)); }, &err)) { b.error = err; return b; }
  int bad = 0;
  b.info.day = dayStart(day);
  b.info.zipBytes = zip.size();
  b.info.built = builtAt;
  if (k == Kind::Depth) {
    const auto s = perp::parseDepth(csv, b.info.day, &bad);
    if (bad > 0 || s.empty()) { b.error = QString("%1 unreadable lines, %2 snapshots").arg(bad).arg(s.size()); return b; }
    b.data = perp::encodeDepth(s);
    b.info.rows = int(s.size());
    b.info.bytes = b.data.size();
  } else {
    b.oi = perp::parseMetrics(csv, b.info.day, &bad);
    if (bad > 0 || b.oi.empty()) { b.error = QString("%1 unreadable lines, %2 rows").arg(bad).arg(b.oi.size()); return b; }
    b.info.rows = int(b.oi.size());
  }
  return b;
}
}  // namespace

QString ArchiveCollector::kindId(Kind k) {
  switch (k) {
    case Kind::FootprintSpot: return QStringLiteral("spot");
    case Kind::FootprintPerp: return QStringLiteral("perp");
    case Kind::Depth: return QStringLiteral("depth");
    case Kind::OpenInterest: return QStringLiteral("oi");
  }
  return {};
}

ArchiveCollector& ArchiveCollector::instance() {
  static ArchiveCollector* c = new ArchiveCollector(&Store::instance());
  return *c;
}

ArchiveCollector::ArchiveCollector(Store* store, QObject* parent) : QObject(parent), m_store(store) {
  m_fetch = [](const QUrl& url, QObject* ctx, Collector::Reply reply) {
    net::Request r;
    r.url = url;
    r.ttlSec = 0;
    r.staleOnError = false;   // no disk cache: files of up to 100 MB, each read once
    r.timeoutMs = 60000;      // without a byte for that long (Qt's transfer timeout), not the whole download
    net::get(r, ctx, [reply](const net::Reply& rep) { reply(rep.status, rep.body, rep.ok() ? QString() : rep.error); });
  };
  m_timer.setInterval(15 * 60 * 1000);   // a new day published, a day to ask again
  connect(&m_timer, &QTimer::timeout, this, &ArchiveCollector::syncNow);
  auto changed = [this](const QString& market) {
    // its settings, its pause: what it lacks is listed again; the days on their way are dropped (asked again if still wanted)
    bool hit = market.isEmpty();
    for (const Job& j : m_inFlight) hit |= j.market == market;
    if (hit) { ++m_gen; m_inFlight.clear(); }
    m_queue.clear();
    if (m_started) syncNow();
    emit progressChanged();
  };
  connect(store, &Store::pairsChanged, this, changed);
  connect(store, &Store::pausedChanged, this, changed);
}

qint64 ArchiveCollector::now() const { return m_now ? m_now() : QDateTime::currentMSecsSinceEpoch(); }
QDate ArchiveCollector::lastPublished() const { return dayOf(now() / 1000).addDays(-1); }

QStringList ArchiveCollector::markets() const {
  QStringList out;
  for (const QString& m : m_markets.isEmpty() ? m_store->pairs() : m_markets) {
    if (!m.startsWith("BINANCE:")) continue;
    const PairSpec s = specFromJson(m_store->pairSpec(m));
    if (!s.footprint.isEmpty() || s.depth || s.openInterest) out << m;
  }
  return out;
}

bool ArchiveCollector::active(const QString& market) const { return !m_store->isPaused(market); }

QVector<ArchiveCollector::Want> ArchiveCollector::wants(const QString& market) const {
  QVector<Want> out;
  const PairSpec spec = specFromJson(m_store->pairSpec(market));
  const QString symbol = binanceSymbol(market);
  for (const QString& id : spec.footprint) {
    const auto v = fp::venueFromId(id);
    if (!v) continue;
    Want w{*v == fp::Venue::Spot ? Kind::FootprintSpot : Kind::FootprintPerp,
           std::max(fp::firstDay(*v, symbol), spec.footprintStart > 0 ? dayOf(spec.footprintStart) : QDate()), {}};
    for (const FpDay& d : m_store->footprintDays(market, id)) w.have.insert(d.day);
    const QHash<qint64, QString> missing = m_store->footprintMissing(market, id);
    for (auto it = missing.cbegin(); it != missing.cend(); ++it) w.have.insert(it.key());
    out << w;
  }
  auto arch = [&](bool on, Kind k, const QDate& first) {
    if (!on) return;
    Want w{k, first, {}};
    for (const ArchDay& d : m_store->archiveDays(market, kindId(k))) w.have.insert(d.day);
    const QHash<qint64, QString> missing = m_store->archiveMissing(market, kindId(k));
    for (auto it = missing.cbegin(); it != missing.cend(); ++it) w.have.insert(it.key());
    out << w;
  };
  arch(spec.depth, Kind::Depth, perp::firstDepthDay());
  arch(spec.openInterest, Kind::OpenInterest, perp::firstMetricsDay());
  return out;
}

QVector<ArchiveCollector::Job> ArchiveCollector::todo(const QString& market) const {
  QVector<Job> out;
  if (!active(market)) return out;
  const QVector<Want> ws = wants(market);
  const QDate to = lastPublished();
  const qint64 ms = now();
  QDate from = to;
  for (const Want& w : ws) from = std::min(from, w.from);
  // newest day first, every kind of a day together
  for (QDate d = to; d.isValid() && d >= from; d = d.addDays(-1))
    for (const Want& w : ws) {
      if (d < w.from || w.have.contains(dayStart(d))) continue;
      if (m_retryAt.value(dayKey(market, w.kind, d)) > ms) continue;
      if (inFlight(market, w.kind, d)) continue;   // on its way
      out << Job{market, w.kind, d};
    }
  return out;
}

void ArchiveCollector::start() {
  if (m_started) return;
  m_started = true;
  m_timer.start();
  syncNow();
}

void ArchiveCollector::stop() {
  m_started = false;
  m_timer.stop();
  m_queue.clear();
  ++m_gen;
  m_inFlight.clear();
  emit progressChanged();
}

bool ArchiveCollector::inFlight(const QString& market, Kind kind, const QDate& day) const {
  for (const Job& j : m_inFlight)
    if (j.market == market && j.kind == kind && j.day == day) return true;
  return false;
}

void ArchiveCollector::syncNow() {
  if (!m_started) return;
  m_queue.clear();
  for (const QString& m : markets()) for (const Job& j : todo(m)) m_queue << j;
  emit progressChanged();
  next();
}

void ArchiveCollector::next() {
  m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(), [this](const Job& j) { return !active(j.market); }), m_queue.end());
  bool started = false;
  while (m_inFlight.size() < kInFlight) {
    bool bigOnWay = false;   // one footprint day at a time (its zip is large)
    for (const Job& j : m_inFlight) bigOnWay |= isFootprint(j.kind);
    int pick = -1;
    for (int i = 0; i < m_queue.size() && pick < 0; ++i)
      if (!(bigOnWay && isFootprint(m_queue[i].kind))) pick = i;
    if (pick < 0) break;
    const Job j = m_queue.takeAt(pick);
    m_inFlight << j;
    started = true;
    run(j);
  }
  if (started) emit progressChanged();
  if (m_queue.isEmpty() && m_inFlight.isEmpty()) { emit progressChanged(); emit idle(); }
}

void ArchiveCollector::run(const Job& j) {
  const int gen = m_gen;
  const QString symbol = binanceSymbol(j.market);
  QPointer<ArchiveCollector> self(this);
  m_fetch(sumUrl(j.kind, symbol, j.day), this, [this, self, j, gen, symbol](int status, const QByteArray& sumFile, const QString& err) {
    if (!self || gen != m_gen) return;
    if (status == 404 || status == 403) {   // no archive for that day (S3 answers 403 or 404 for a missing file)
      if (j.day < lastPublished().addDays(-2)) {
        if (isFootprint(j.kind)) m_store->setFootprintMissing(j.market, kindId(j.kind), dayStart(j.day), QStringLiteral("no archive"));
        else m_store->setArchiveMissing(j.market, kindId(j.kind), dayStart(j.day), QStringLiteral("no archive"));
      } else {   // not published yet: in an hour
        m_retryAt.insert(dayKey(j.market, j.kind, j.day), now() + 3600 * 1000);
      }
      finish(j, {});
      return;
    }
    if (status != 200) { finish(j, err.isEmpty() ? QString("HTTP %1").arg(status) : err, true); return; }
    const QByteArray sha = fp::checksumOf(sumFile);
    if (sha.isEmpty()) { finish(j, QStringLiteral("unreadable checksum file")); return; }
    m_fetch(zipUrl(j.kind, symbol, j.day), this, [this, self, j, gen, sha](int status, const QByteArray& zip, const QString& err) {
      if (!self || gen != m_gen) return;
      if (status != 200) { finish(j, err.isEmpty() ? QString("HTTP %1").arg(status) : err, true); return; }
      if (QCryptographicHash::hash(zip, QCryptographicHash::Sha256).toHex() != sha) {
        finish(j, QStringLiteral("the archive does not match its checksum"));
        return;
      }
      auto* watch = new QFutureWatcher<Built>(this);
      connect(watch, &QFutureWatcher<Built>::finished, this, [this, self, watch, j, gen] {
        watch->deleteLater();
        if (!self || gen != m_gen) return;
        const Built b = watch->result();
        if (!b.error.isEmpty()) { finish(j, b.error); return; }
        auto done = [this, j, gen](bool) {
          if (gen == m_gen) finish(j, {}, false, true);
        };
        if (isFootprint(j.kind)) m_store->addFootprintDay(j.market, kindId(j.kind), b.fpInfo, b.data, this, done);
        else if (j.kind == Kind::Depth) m_store->addDepthDay(j.market, b.info, b.data, this, done);
        else m_store->addOiDay(j.market, b.info, b.oi, this, done);
      });
      watch->setFuture(QtConcurrent::run(build, j.kind, zip, j.day, QDateTime::currentMSecsSinceEpoch()));
    });
  });
}

void ArchiveCollector::finish(const Job& j, const QString& error, bool stopTurn, bool stored) {
  for (int i = 0; i < m_inFlight.size(); ++i)
    if (m_inFlight[i].market == j.market && m_inFlight[i].kind == j.kind && m_inFlight[i].day == j.day) { m_inFlight.removeAt(i); break; }
  const QString k = key(j.market, j.kind);
  if (stored) {
    m_errors.remove(k);
  } else if (!error.isEmpty()) {
    m_errors.insert(k, error);
    m_retryAt.insert(dayKey(j.market, j.kind, j.day), now() + 10 * 60 * 1000);   // on a later turn
    if (stopTurn) m_queue.clear();   // the network or the server failing: the turn stops, the timer starts the next one
  }
  emit progressChanged();
  QTimer::singleShot(0, this, &ArchiveCollector::next);
}

QVector<ArchiveCollector::Progress> ArchiveCollector::progress() const {
  QVector<Progress> out;
  const QDate to = lastPublished();
  for (const QString& market : m_markets.isEmpty() ? m_store->pairs() : m_markets) {
    if (!market.startsWith("BINANCE:")) continue;
    for (const Want& w : wants(market)) {
      Progress p;
      p.market = market;
      p.kind = kindId(w.kind);
      p.from = w.from;
      p.to = to;
      p.wanted = p.from.isValid() && p.from <= to ? int(p.from.daysTo(to)) + 1 : 0;
      auto day = [&](qint64 d, qint64 bytes, qint64 rows) {
        ++p.stored;
        p.bytes += bytes;
        p.rows += rows;
        const QDate x = dayOf(d);
        if (!p.first.isValid() || x < p.first) p.first = x;
        if (!p.last.isValid() || x > p.last) p.last = x;
      };
      if (isFootprint(w.kind)) {
        for (const FpDay& d : m_store->footprintDays(market, p.kind)) day(d.day, d.bytes, d.trades);
        p.missing = int(m_store->footprintMissing(market, p.kind).size());
      } else {
        for (const ArchDay& d : m_store->archiveDays(market, p.kind)) day(d.day, d.bytes, d.rows);
        p.missing = int(m_store->archiveMissing(market, p.kind).size());
      }
      for (QDate d = to; p.from.isValid() && d >= std::max(p.from, to.addDays(-2)); d = d.addDays(-1)) p.unpublished += !w.have.contains(dayStart(d));
      p.paused = m_store->isPaused(market);
      for (const Job& j : m_inFlight)
        if (j.market == market && j.kind == w.kind) { p.running = true; p.current = j.day; }
      for (const Job& j : m_queue) if (j.market == market && j.kind == w.kind) { p.queued = true; break; }
      p.error = m_errors.value(key(market, w.kind));
      out << p;
    }
  }
  return out;
}

}  // namespace technicaldata
