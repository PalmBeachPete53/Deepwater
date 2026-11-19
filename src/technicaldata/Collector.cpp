// Binance klines → technical-data.sqlite. See Collector.h.
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "net/Http.h"
#include "net/Sources.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QJsonDocument>
#include <QTimeZone>
#include <QUrlQuery>
#include <algorithm>

namespace technicaldata {

namespace {
// the same Binance entries as the Charts tab, with this page added (sources::declare merges by id)
const sources::Registrar regBinance({"binance", "Binance (spot market data API)", {"api.binance.com"}, sources::Access::Keyless, 200,
                                     "Market bars of every timeframe since the exchange's first day (Data › Technical data)", {}, {"data/technical-data"}});
const sources::Registrar regVision({"binance-vision", "Binance (public market-data mirror)", {"data-api.binance.vision"},
                                    sources::Access::Keyless, 200, {}, {}, {"data/technical-data"}});

const char* const MAIN = "https://api.binance.com";
const char* const MIRROR = "https://data-api.binance.vision";

double num(const QJsonValue& v) { return v.isString() ? v.toString().toDouble() : v.toDouble(); }
}  // namespace

const QVector<Timeframe>& timeframes() {
  static const QVector<Timeframe> t = {{"1m", 60, "1m"},   {"5m", 300, "5m"},     {"15m", 900, "15m"}, {"1h", 3600, "1h"},
                                       {"4h", 14400, "4h"}, {"1D", 86400, "1d"}, {"1W", 604800, "1w"}};
  return t;
}

QString binanceSymbol(const QString& market) { return market.section(':', 1); }

QVector<Kline> parseKlines(const QByteArray& json, qint64 nowMs, bool* forming, QString* error) {
  QVector<Kline> out;
  if (forming) *forming = false;
  QJsonParseError pe;
  const QJsonDocument d = QJsonDocument::fromJson(json, &pe);
  if (!d.isArray()) {
    if (error) *error = d.isObject() ? d.object().value("msg").toString("unexpected answer") : pe.errorString();
    return out;
  }
  for (const QJsonValue& v : d.array()) {
    const QJsonArray a = v.toArray();
    if (a.size() < 11) continue;
    const qint64 closeMs = qint64(num(a[6]));
    if (closeMs >= nowMs) { if (forming) *forming = true; continue; }   // still forming
    Kline k;
    k.t = qint64(num(a[0])) / 1000;
    k.o = num(a[1]); k.h = num(a[2]); k.l = num(a[3]); k.c = num(a[4]); k.v = num(a[5]);
    k.qv = num(a[7]); k.n = qint64(num(a[8])); k.tbv = num(a[9]); k.tbqv = num(a[10]);
    out << k;
  }
  return out;
}

// ------------------------------------------------------------------------------------------------ collector
Collector& Collector::instance() {
  static Collector* c = new Collector(&Store::instance());
  return *c;
}

Collector::Collector(Store* store, QObject* parent) : QObject(parent), m_store(store), m_tfs(timeframes()) {
  m_fetch = [](const QUrl& url, QObject* ctx, Reply reply) {
    net::Request r;
    r.url = url;
    r.ttlSec = 0;
    r.staleOnError = false;   // no disk cache: thousands of pages
    r.timeoutMs = 30000;
    net::get(r, ctx, [reply](const net::Reply& rep) { reply(rep.status, rep.body, rep.error); });
  };
  // a pair added, changed or removed: what it lacks now is fetched (largest timeframe first), what it no longer has waits no more
  connect(store, &Store::pairsChanged, this, &Collector::pairChanged);
  // paused: its jobs leave the queue and the one running stops after its page (wanted()); resumed: it goes on
  connect(store, &Store::pausedChanged, this, [this](const QString& market) {
    if (!market.isEmpty()) { pairChanged(market); return; }
    for (const QString& m : markets()) pairChanged(m);
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(), [this](const Job& j) { return !wanted(j); }), m_queue.end());
    emit progressChanged();
  });
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    syncNow();
    // the next check a few seconds after the next minute, when its bars have closed
    const qint64 ms = now();
    m_timer.start(int(60000 - ms % 60000 + 3000));
  });
}

void Collector::pairChanged(const QString& market) {
  const QString prefix = market + "|";
  for (auto it = m_gaps.begin(); it != m_gaps.end();) it = it.key().startsWith(prefix) ? m_gaps.erase(it) : std::next(it);
  // a cancel or new settings: the last failure and the page count were of what was there before
  for (auto it = m_errors.begin(); it != m_errors.end();) it = it.key().startsWith(prefix) ? m_errors.erase(it) : std::next(it);
  for (auto it = m_pages.begin(); it != m_pages.end();) it = it.key().startsWith(prefix) ? m_pages.erase(it) : std::next(it);
  m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(), [this](const Job& j) { return !wanted(j); }), m_queue.end());
  if (m_started) {
    const auto tfs = timeframesOf(market);
    for (auto it = tfs.crbegin(); it != tfs.crend(); ++it) enqueue(market, it->seconds);
  }
  emit progressChanged();
}

qint64 Collector::now() const { return m_now ? m_now() : QDateTime::currentMSecsSinceEpoch(); }

QStringList Collector::markets() const {
  if (!m_markets.isEmpty()) return m_markets;
  QStringList out;   // Binance's pairs; the forex ones are ForexCollector's
  for (const QString& m : m_store->pairs()) if (m.startsWith("BINANCE:")) out << m;
  return out;
}

QVector<Timeframe> Collector::timeframesOf(const QString& market) const {
  if (m_store->isPaused(market)) return {};
  return recordedOf(market);
}

QVector<Timeframe> Collector::recordedOf(const QString& market) const {
  const QJsonObject spec = m_store->pairSpec(market);
  if (spec.isEmpty()) return {};   // not a pair (any more)
  const PairSpec p = specFromJson(spec);
  QVector<Timeframe> out;
  for (const auto& t : m_tfs) if (p.timeframes.contains(t.seconds)) out << t;
  return out;
}

qint64 Collector::startOf(const QString& market) const { return specFromJson(m_store->pairSpec(market)).start; }

bool Collector::wanted(const Job& j) const {
  if (!m_started) return false;   // stopped (Deepwater closing): an answer still coming writes nothing more
  for (const auto& t : timeframesOf(j.market)) if (t.seconds == j.tf) return true;
  return false;
}

void Collector::start() {
  if (m_started) return;
  m_started = true;
  // largest timeframe first: the long history comes in quickly, 1m (thousands of pages) last
  for (const QString& m : markets()) {
    const auto tfs = timeframesOf(m);
    for (auto it = tfs.crbegin(); it != tfs.crend(); ++it) enqueue(m, it->seconds);
  }
  m_timer.start(int(60000 - now() % 60000 + 3000));
}

void Collector::stop() {
  m_started = false;
  m_timer.stop();
  m_queue.clear();
}

void Collector::syncNow() {
  const qint64 nowS = now() / 1000;
  for (const QString& m : markets()) {
    const auto tfs = timeframesOf(m);
    const qint64 start = startOf(m);
    for (auto it = tfs.crbegin(); it != tfs.crend(); ++it) {
      const SyncState s = m_store->state(m, it->seconds);
      // a newer bar has closed when its close (last + 2 steps) has passed; a first day moved earlier
      if (s.count == 0 || !s.historyDone || nowS >= s.last + 2 * it->seconds || start < s.startReq) enqueue(m, it->seconds);
    }
  }
}

void Collector::enqueue(const QString& market, int tf, bool gaps) {
  for (const Job& j : m_queue)
    if (j.market == market && j.tf == tf && j.gaps == gaps) return;
  if (!gaps && m_running && m_current == key(market, tf)) return;
  // looking for holes goes first: a short job, and the gaps should show before the long 1m history ends
  if (gaps) {
    m_queue.prepend(Job{market, tf, gaps});
  } else {
    m_queue << Job{market, tf, gaps};
  }
  emit progressChanged();
  if (!m_running) QTimer::singleShot(0, this, &Collector::next);
}

void Collector::next() {
  if (m_running) return;
  if (m_queue.isEmpty()) { m_status.clear(); emit progressChanged(); emit idle(); return; }
  const Job j = m_queue.takeFirst();
  m_running = true;
  m_current = key(j.market, j.tf);
  emit progressChanged();
  if (j.gaps) runGaps(j); else runSync(j);
}

// One page of klines from startMs (and before endMs if > 0); stores the closed bars; done(more, last bar's time) —
// more: a full page came back whose bars have all closed, so there may be more after it.
void Collector::page(const Job& j, qint64 startMs, qint64 endMs, std::function<void(bool, qint64)> done, bool mirror) {
  const QString tfName = [&] { for (const auto& t : timeframes()) if (t.seconds == j.tf) return t.binance; return QString(); }();
  QUrl url(QString(mirror ? MIRROR : MAIN) + "/api/v3/klines");
  QUrlQuery q;
  q.addQueryItem("symbol", binanceSymbol(j.market));
  q.addQueryItem("interval", tfName);
  q.addQueryItem("startTime", QString::number(startMs));
  if (endMs > 0) q.addQueryItem("endTime", QString::number(endMs));
  q.addQueryItem("limit", QString::number(m_page));
  url.setQuery(q);
  const QString k = key(j.market, j.tf);
  m_fetch(url, this, [this, j, startMs, endMs, done, mirror, k](int status, const QByteArray& body, const QString& err) {
    if (!err.isEmpty() || status < 200 || status >= 300) {
      if (!mirror) { page(j, startMs, endMs, done, true); return; }   // the mirror, once
      m_errors[k] = err.isEmpty() ? QString("HTTP %1").arg(status) : err;
      done(false, 0);
      return;
    }
    bool forming = false;
    QString perr;
    const QVector<Kline> rows = parseKlines(body, now(), &forming, &perr);
    if (!perr.isEmpty()) { m_errors[k] = perr; done(false, 0); return; }
    m_errors.remove(k);
    ++m_pages[k];
    const QJsonDocument d = QJsonDocument::fromJson(body);
    const int returned = int(d.array().size());
    const bool more = returned >= m_page && !forming;
    if (rows.isEmpty()) { done(more, 0); return; }
    const qint64 lastT = rows.last().t;
    m_store->add(j.market, j.tf, rows, this, [done, more, lastT](int) { done(more, lastT); });
  });
}

QString tfNameOf(int seconds) {
  for (const auto& t : timeframes()) if (t.seconds == seconds) return t.name;
  return QString::number(seconds);
}

void Collector::finish() {
  m_running = false;
  m_current.clear();
  next();
}

// The bars before the first stored one when the pair's first day moved earlier, then forward from the last one.
void Collector::runSync(const Job& j) {
  const SyncState s0 = m_store->state(j.market, j.tf);
  const qint64 start = startOf(j.market);
  if (s0.count == 0 || start >= s0.startReq) { runForward(j); return; }
  auto cursor = std::make_shared<qint64>(start);
  auto step = std::make_shared<std::function<void()>>();
  // its indicators are computed once at the end, not again after every page (each lands before the later history)
  m_store->setBackfilling(j.market, j.tf, true);
  *step = [this, j, start, cursor, step, first = s0.first] {
    if (!wanted(j)) {
      QTimer::singleShot(0, this, [step] { *step = nullptr; });
      m_store->setBackfilling(j.market, j.tf, false);   // what came is computed; the rest waits for its next turn
      finish();
      return;
    }
    m_status = QCoreApplication::translate("data", "%1 %2: earlier bars, from %3")
                   .arg(j.market, tfNameOf(j.tf), QDateTime::fromSecsSinceEpoch(*cursor, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm"));
    emit progressChanged();
    page(j, *cursor * 1000, first * 1000 - 1, [this, j, start, cursor, step](bool more, qint64 lastT) {
      const bool failed = m_errors.contains(key(j.market, j.tf));
      if (!failed && more && lastT > 0) { *cursor = lastT + j.tf; QTimer::singleShot(0, this, [step] { (*step)(); }); return; }
      QTimer::singleShot(0, this, [step] { *step = nullptr; });
      m_store->setBackfilling(j.market, j.tf, false);   // every page is written by now (done follows the write)
      if (failed) { finish(); return; }   // asked again at the next minute's check
      m_store->setStartRequested(j.market, j.tf, start);
      m_gaps.remove(key(j.market, j.tf));   // the junction is looked at with the rest
      runForward(j);
    });
  };
  (*step)();
}

void Collector::runForward(const Job& j) {
  auto step = std::make_shared<std::function<void()>>();
  *step = [this, j, step] {
    if (!wanted(j)) { QTimer::singleShot(0, this, [step] { *step = nullptr; }); finish(); return; }
    const SyncState s = m_store->state(j.market, j.tf);
    // from the bar after the last stored one; from the pair's first day (0: the exchange's beginning) the first time
    const qint64 start = s.count ? (s.last + j.tf) * 1000 : startOf(j.market) * 1000;
    m_status = QCoreApplication::translate("data", "%1 %2: %3 bars, up to %4")
                   .arg(j.market, tfNameOf(j.tf), QLocale(QLocale::English).toString(s.count))
                   .arg(s.last ? QDateTime::fromSecsSinceEpoch(s.last, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm") : QStringLiteral("—"));
    emit progressChanged();
    page(j, start, 0, [this, j, step](bool more, qint64) {
      if (more && wanted(j)) { QTimer::singleShot(0, this, [step] { (*step)(); }); return; }
      const bool failed = m_errors.contains(key(j.market, j.tf));
      const SyncState s = m_store->state(j.market, j.tf);
      if (!wanted(j)) {
      } else if (!failed && !s.historyDone && s.count > 0) {
        m_store->setHistoryDone(j.market, j.tf);
        enqueue(j.market, j.tf, true);   // the whole history is in: look for holes
      } else if (!failed && s.historyDone && !m_gaps.contains(key(j.market, j.tf))) {
        enqueue(j.market, j.tf, true);   // once per run: holes left from an earlier run
      }
      QTimer::singleShot(0, this, [step] { *step = nullptr; });
      finish();
    });
  };
  (*step)();
}

// Each hole asked for again, page after page to its end. A hole whose asking went through (no failure, the pair not
// paused meanwhile) and that is still there is the exchange's own: marked checked, never asked again. A failure leaves
// it open for the next run. (scanGaps runs on the writer after every page queued before it: no wait.)
void Collector::runGaps(const Job& j) {
  m_store->scanGaps(j.market, j.tf, this, [this, j](const QVector<Gap>& found) {
    m_gaps[key(j.market, j.tf)] = found;
    auto open = std::make_shared<QVector<Gap>>();
    for (const Gap& g : found) if (!g.checked) *open << g;
    auto asked = std::make_shared<QVector<QPair<qint64, qint64>>>();   // holes asked for in full, without a failure
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, j, open, asked, step] {
      if (!wanted(j)) open->clear();   // paused or removed meanwhile: what is left waits
      if (open->isEmpty()) {
        // what is filled now leaves the list; what the exchange lacks stays, marked checked
        m_store->scanGaps(j.market, j.tf, this, [this, j, asked](const QVector<Gap>& left) {
          QVector<Gap> kept = left;
          for (Gap& g : kept)
            for (const auto& a : *asked)
              if (!g.checked && g.from >= a.first && g.to <= a.second) {
                g.checked = true;
                m_store->setGapChecked(j.market, j.tf, g.from);
              }
          m_gaps[key(j.market, j.tf)] = kept;
          m_running = false;
          m_current.clear();
          emit progressChanged();
          next();
        });
        QTimer::singleShot(0, this, [step] { *step = nullptr; });
        return;
      }
      const Gap g = open->takeFirst();
      page(j, (g.from + j.tf) * 1000, g.to * 1000 - 1, [this, j, g, open, asked, step](bool more, qint64 lastT) {
        const bool failed = m_errors.contains(key(j.market, j.tf));
        if (!failed && wanted(j)) {
          if (more && lastT > 0 && lastT + j.tf < g.to) open->prepend(Gap{lastT, g.to, 0, false});   // a long hole: on
          else *asked << qMakePair(g.from, g.to);
        }
        QTimer::singleShot(0, this, [step] { (*step)(); });
      });
    };
    (*step)();
  });
}

QVector<Collector::Progress> Collector::progress() const {
  QVector<Progress> out;
  for (const QString& m : markets())
    for (const auto& t : recordedOf(m)) {
      Progress p;
      p.paused = m_store->isPaused(m);
      p.cancelled = m_store->cancelled(m);
      p.market = m;
      p.tf = t.name;
      p.seconds = t.seconds;
      p.state = m_store->state(m, t.seconds);
      const QString k = key(m, t.seconds);
      p.running = m_running && m_current == k;
      for (const Job& j : m_queue) if (j.market == m && j.tf == t.seconds) p.queued = true;
      p.pagesDone = m_pages.value(k);
      p.error = m_errors.value(k);
      const auto g = m_gaps.constFind(k);
      if (g != m_gaps.cend()) {
        p.gaps = int(g->size());
        for (const Gap& x : *g) if (!x.checked) ++p.gapsOpen;
      }
      out << p;
    }
  return out;
}

}  // namespace technicaldata
