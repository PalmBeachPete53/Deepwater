// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
#include "charts/data/Fetch.h"
#include "charts/core/Series.h"
#include <QTimer>
#include <QDateTime>
#include <QTimeZone>
#include <QHash>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <list>

namespace charts::data {
namespace {
QObject* anchor() { static QObject* o = new QObject; return o; }
testing::Transport& transport() { static testing::Transport t; return t; }
int& timeoutOverride() { static int ms = 0; return ms; }
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

// A host that timed out or couldn't be reached is skipped for a minute, so a firewall that
// silently drops one provider costs one timeout, not one per chart, before the fallback runs.
QHash<QString, qint64>& hostDownUntil() { static QHash<QString, qint64> h; return h; }
// Why a marked host is skipped: 0 = unreachable, 429 / 418 = it asked us to slow down (rate limit / ban).
QHash<QString, int>& hostDownStatus() { static QHash<QString, int> h; return h; }
QString downText(const QString& host) {
  const int st = hostDownStatus().value(host, 0);
  if (st == 429 || st == 418)
    return QString("%1 asked us to slow down (HTTP %2); trying again later").arg(host).arg(st);
  return host + " isn't reachable from here (trying again in a minute)";
}
// net::Http sends one request per host at a time: requests already queued behind the one that found the host down are
// failed by their preflight with this text instead of each waiting for its own timeout (the JS fetches in parallel).
const QString kSkippedDown = QStringLiteral("charts: host marked down");

Error upstreamError(const QString& message, int status = 0) { return Error{message, status, true, {}}; }

// Node's fetch reports e.cause.code (ENOTFOUND, ECONNREFUSED…) or "timed out"; Qt gives an error string.
QString whyOf(const QString& qtError) {
  const QString e = qtError.toLower();
  if (e.contains("operation canceled") || e.contains("timed out") || e.contains("timeout")) return QT_TRANSLATE_NOOP("charts", "timed out");
  if (e.contains("not found")) return "ENOTFOUND";
  if (e.contains("refused")) return "ECONNREFUSED";
  if (e.contains("closed")) return "ECONNRESET";
  if (e.contains("unreachable") || e.contains("network access")) return "ENETUNREACH";
  return qtError;
}
}  // namespace

void later(std::function<void()> fn) { QTimer::singleShot(0, anchor(), std::move(fn)); }

std::shared_ptr<Join> Join::make(int n, std::function<void()> done) {
  auto j = std::make_shared<Join>(Join{n, std::move(done)});
  if (n <= 0) later(j->done);
  return j;
}
void Join::one() {
  if (--left == 0 && done) { auto d = std::move(done); done = nullptr; d(); }
}

int timeoutMs() {
  if (timeoutOverride() > 0) return timeoutOverride();
  bool ok = false;
  const int env = qEnvironmentVariable("QT_TIMEOUT_MS").toInt(&ok);
  return ok && env > 0 ? env : 10000;
}

bool hostDown(const QString& host) { return hostDownUntil().value(host, 0) > nowMs(); }

void noteHostResult(const QString& host, const net::Reply& rep) {
  if (rep.error == kSkippedDown || rep.error.startsWith("source ")) return;
  if (rep.status == 0 && !rep.error.isEmpty()) {
    hostDownUntil().insert(host, nowMs() + 60000);
    hostDownStatus().insert(host, 0);
  } else if (rep.status == 429 || rep.status == 418) {
    // Rate limit / IP ban (audit D19): asking again at once (Binance's next call even asked every host in parallel)
    // makes a ban longer. The server's Retry-After when it sent one (Binance does, on both), else a minute for 429
    // and five for 418 (Binance's bans start at 2 minutes), as TradeStream pauses its history.
    const qint64 fallback = rep.status == 418 ? 300000 : 60000;
    hostDownUntil().insert(host, nowMs() + (rep.retryAfterSec > 0 ? qint64(rep.retryAfterSec) * 1000 : fallback));
    hostDownStatus().insert(host, rep.status);
  } else {
    hostDownUntil().remove(host);
    hostDownStatus().remove(host);
  }
}

void skipIfDown(net::Request& r, const QString& host) {
  r.preflight = [host]() { return hostDown(host) ? kSkippedDown : QString(); };
}

void send(const net::Request& r, net::Callback cb) {
  if (transport()) {
    if (r.preflight) {
      if (const QString why = r.preflight(); !why.isEmpty()) {
        net::Reply rep; rep.error = why;
        later([cb, rep] { cb(rep); });
        return;
      }
    }
    transport()(r, std::move(cb));
    return;
  }
  net::get(r, anchor(), std::move(cb));
}

void parseJson(const QByteArray& body, std::function<void(const QJsonDocument&, const QString&)> cb) {
  auto parse = [](const QByteArray& b) {
    QJsonParseError pe;
    QJsonDocument d = QJsonDocument::fromJson(b, &pe);
    return std::make_pair(d, pe.error == QJsonParseError::NoError ? QString() : pe.errorString());
  };
  // Measured 2026-11-16 (M-series Mac, release build): 1000 Binance klines = 170 KB parse in 0.7 ms. Past 256 KB
  // (Binance exchangeInfo, Nasdaq screener and 10-year history, all-symbol tickers) the parse leaves the GUI thread.
  if (body.size() < 256 * 1024) {
    const auto [d, err] = parse(body);
    cb(d, err);
    return;
  }
  QtConcurrent::run([body, parse] { return parse(body); })
      .then(anchor(), [cb](const std::pair<QJsonDocument, QString>& r) { cb(r.first, r.second); });
}

void getJson(const QString& url, const Headers& headers, Callback<QJsonDocument> cb, const GetOpts& o) {
  const QUrl u(url);
  const QString host = u.port() > 0 ? u.host() + ":" + QString::number(u.port()) : u.host();
  if (hostDown(host)) {
    const QString why = downText(host);
    later([cb, why] { cb(Result<QJsonDocument>::failure(upstreamError(why))); });
    return;
  }
  net::Request r;
  r.url = u;
  r.headers = {{"Accept", "application/json, text/plain, */*"}, {"Accept-Language", "en-US,en;q=0.9"}};
  for (const auto& h : headers) {
    bool replaced = false;
    for (auto& x : r.headers) if (x.first.compare(h.first, Qt::CaseInsensitive) == 0) { x.second = h.second; replaced = true; }
    if (!replaced) r.headers << h;
  }
  r.ttlSec = o.ttlSec;
  r.staleOnError = o.staleOnError;
  r.timeoutMs = timeoutMs();
  skipIfDown(r, host);
  send(r, [cb, host, r](const net::Reply& rep) {
    if (rep.error == kSkippedDown) {   // queued behind the request that found the host down
      cb(Result<QJsonDocument>::failure(upstreamError(downText(host))));
      return;
    }
    noteHostResult(host, rep);
    if (rep.status == 0 && !rep.error.isEmpty()) {
      // net::Http refusing a host (switched off in the Sources tab, or forbidden): say so, it isn't down
      if (rep.error.startsWith("source ")) { cb(Result<QJsonDocument>::failure(upstreamError(host + ": " + rep.error))); return; }
      cb(Result<QJsonDocument>::failure(upstreamError(QString("%1 couldn't be reached (%2)").arg(host, whyOf(rep.error)))));
      return;
    }
    if (rep.status < 200 || rep.status >= 300) {
      cb(Result<QJsonDocument>::failure(upstreamError(QString("%1 returned HTTP %2").arg(host).arg(rep.status), rep.status)));
      return;
    }
    parseJson(rep.body, [cb, host, r](const QJsonDocument& d, const QString& err) {
      if (!err.isEmpty()) {
        // A 200 that isn't JSON (an HTML error page, a body cut off) must not stay in the disk cache for its TTL:
        // the JS cached() forgets a failed entry, so the next call asks again.
        if (r.ttlSec > 0 || r.staleOnError) net::invalidate(r);
        cb(Result<QJsonDocument>::failure(Error{host + " sent an answer that isn't JSON (" + err + ")", 0, false, {}}));
        return;
      }
      else cb(Result<QJsonDocument>::success(d));
    });
  });
}

// ---------- cache ----------
namespace {
struct Waiter { AnyCb cb; };
struct Entry {
  qint64 at = 0;
  bool pending = false;
  AnyPtr value;
  std::vector<AnyCb> waiters;
};
struct Cache {
  QHash<QString, Entry> map;
  std::list<QString> order;   // insertion order, as a JS Map iterates
  void erase(const QString& k) {
    map.remove(k);
    order.remove(k);
  }
};
Cache& cache() { static Cache c; return c; }
constexpr int CACHE_MAX = 400;
}  // namespace

void cachedAny(const QString& key, qint64 ttlMs, std::function<void(AnyCb)> fn, AnyCb cb) {
  auto& c = cache();
  const qint64 now = nowMs();
  auto it = c.map.find(key);
  if (it != c.map.end()) {
    if (it->pending) { it->waiters.push_back(std::move(cb)); return; }
    if (now - it->at < ttlMs) {
      AnyPtr v = it->value;
      later([cb, v] { cb(v, nullptr); });
      return;
    }
  }
  if (c.map.size() >= CACHE_MAX)
    for (auto o = c.order.begin(); o != c.order.end();) {
      if (c.map.size() < CACHE_MAX * 0.9) break;
      auto e = c.map.find(*o);
      if (e != c.map.end() && !e->pending) { c.map.erase(e); o = c.order.erase(o); }
      else ++o;
    }
  if (!c.map.contains(key)) c.order.push_back(key);
  Entry& e = c.map[key];
  e.at = now;
  e.pending = true;
  e.value.reset();
  e.waiters.clear();
  e.waiters.push_back(std::move(cb));
  fn([key](const AnyPtr& v, const Error* err) {
    auto& c = cache();
    auto it = c.map.find(key);
    if (it == c.map.end()) return;   // cleared meanwhile (tests)
    std::vector<AnyCb> waiters = std::move(it->waiters);
    it->waiters.clear();
    if (err) c.erase(key);
    else { it->pending = false; it->at = nowMs(); it->value = v; }
    const Error copy = err ? *err : Error{};
    for (auto& w : waiters) w(v, err ? &copy : nullptr);
  });
}

// ---------- chains ----------
void tryChain(const Chain& chain, Callback<ChainResult> cb) {
  struct St { Chain chain; int i = 0; QStringList errors; Callback<ChainResult> cb; };
  auto st = std::make_shared<St>(St{chain, 0, {}, std::move(cb)});
  auto step = std::make_shared<std::function<void()>>();
  *step = [st, step]() {
    if (st->i >= st->chain.size()) {
      Error e{"No source could load this chart. " + st->errors.join(" | "), 502, false, {}};
      st->cb(Result<ChainResult>::failure(e));
      *step = nullptr;   // break the cycle
      return;
    }
    const ChainStep s = st->chain[st->i++];
    s.run([st, step, name = s.provider](const Result<Bars>& r) {
      if (r.ok() && !r.value.isEmpty()) {
        st->cb(Result<ChainResult>::success(ChainResult{r.value, name, st->errors}));
        *step = nullptr;
        return;
      }
      st->errors << (r.ok() ? name + ": no data" : name + ": " + r.error->message);
      (*step)();
    });
  };
  if (st->chain.isEmpty()) later(*step);
  else (*step)();
}

// ---------- timeframe and JS helpers ----------
double tfSeconds(const QString& tf) { return series::parseTf(tf).seconds; }
bool intraday(const QString& tf) { return series::isIntraday(tf); }
Bars resampleTo(const Bars& bars, const QString& tf) { return series::resample(bars, tf); }
QString nativeBaseOf(const QString& tf, const QStringList& natives) { return series::nativeBase(tf, natives); }

Bars sliceLast(const Bars& b, int limit) {
  if (limit <= 0 || limit >= b.size()) return b;
  return b.mid(b.size() - limit);
}

Bars dedupe(const Bars& bars) {
  Bars out;
  out.reserve(bars.size());
  for (const Bar& b : bars) {
    if (!out.isEmpty() && b.time <= out.last().time) {
      if (b.time == out.last().time) out.last() = b;
      continue;
    }
    out.push_back(b);
  }
  return out;
}

QString isoMs(qint64 ms) { return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"); }
QString isoNow() { return isoMs(nowMs()); }
QString isoDate(qint64 ms) { return isoMs(ms).left(10); }

double parseIsoMs(const QString& s) {
  static const QRegularExpression re(
      R"(^(\d{4})-(\d{2})-(\d{2})(?:[T ](\d{2}):(\d{2})(?::(\d{2})(?:\.(\d+))?)?)?(Z|[+-]\d{2}:?\d{2})?$)");
  const auto m = re.match(s.trimmed());
  if (!m.hasMatch()) return NaN;
  const QDate d(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
  if (!d.isValid()) return NaN;
  const QTime t(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt());
  if (!t.isValid()) return NaN;
  qint64 ms = QDateTime(d, t, QTimeZone::UTC).toMSecsSinceEpoch();
  const QString frac = m.captured(7);
  if (!frac.isEmpty()) ms += (frac + "00").left(3).toInt();
  const QString z = m.captured(8);
  if (!z.isEmpty() && z != "Z") {
    const QString digits = QString(z).remove(':');
    const int off = digits.mid(1, 2).toInt() * 60 + digits.mid(3, 2).toInt();
    ms -= (z[0] == '-' ? -1 : 1) * qint64(off) * 60000;
  }
  return double(ms);
}

QString encodeUriComponent(const QString& s) { return QString::fromLatin1(QUrl::toPercentEncoding(s, "!*'()")); }

QString formEncode(const QList<QPair<QString, QString>>& q) {
  QStringList parts;
  for (const auto& [k, v] : q) {
    auto enc = [](const QString& x) { return QString::fromLatin1(QUrl::toPercentEncoding(x, "*", "~")).replace("%20", "+"); };
    parts << enc(k) + "=" + enc(v);
  }
  return parts.join('&');
}

namespace testing {
void setTransport(Transport t) { transport() = std::move(t); }
void setTimeoutMs(int ms) { timeoutOverride() = ms; }
void clearFetchState() {
  hostDownUntil().clear();
  hostDownStatus().clear();
  cache().map.clear();
  cache().order.clear();
}
}
}  // namespace charts::data
