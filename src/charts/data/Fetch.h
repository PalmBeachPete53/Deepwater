#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// The async plumbing shared by Providers and Feeds (the JS promises, errors, `cached` and `tryChain`).
//
// Everything runs on the GUI thread: requests go through net::get (per-host pacing, disk cache), callbacks come
// back on the GUI thread, never synchronously (a cache hit is delivered on the next event-loop turn, like a
// resolved promise). Large JSON answers are parsed on a worker thread.
#include "charts/core/Types.h"
#include "net/Http.h"
#include <QString>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <functional>
#include <memory>
#include <optional>

namespace charts::data {

// A JS Error as the providers throw it: message (the text shown to the user), status (HTTP status the local API
// answers with, or the upstream's status for upstream errors; 0 = none), upstream (the JS `upstream: true`: the
// server answers 502), detail (a key provider's own message, for logs only).
struct Error {
  QString message;
  int status = 0;
  bool upstream = false;
  QString detail;
};
inline Error badInput(const QString& m) { return Error{m, 400, false, {}}; }

// The settled value of a promise: a value, or an error.
template <class T>
struct Result {
  T value{};
  std::optional<Error> error;
  bool ok() const { return !error.has_value(); }
  static Result success(T v) { Result r; r.value = std::move(v); return r; }
  static Result failure(Error e) { Result r; r.error = std::move(e); return r; }
};
template <class T>
using Callback = std::function<void(const Result<T>&)>;

// Run fn on the next event-loop turn (GUI thread): how a synchronous answer is delivered asynchronously.
void later(std::function<void()> fn);

// Wrap a caller's callback so it is skipped once its context object is gone (ctx null = always called).
template <class T>
Callback<T> guarded(QObject* ctx, Callback<T> cb) {
  if (!ctx) return cb;
  QPointer<QObject> p(ctx);
  return [p, cb = std::move(cb)](const Result<T>& r) { if (p) cb(r); };
}

// Promise.all over n tasks started at once: done() runs once when all n reported.
struct Join {
  int left;
  std::function<void()> done;
  static std::shared_ptr<Join> make(int n, std::function<void()> done);
  void one();   // a task finished
};

// ---------- requests ----------
using Headers = QList<QPair<QByteArray, QByteArray>>;
struct GetOpts {
  int ttlSec = 0;              // > 0: net::get's disk cache serves an answer younger than this
  bool staleOnError = false;   // JS has no stale fallback: off by default so errors reach the chains
};

// The JS getJson: an upstream error for a host that can't be reached (skipped for a minute after that) or answers
// an HTTP error ("<host> returned HTTP 451"); otherwise the parsed JSON.
void getJson(const QString& url, const Headers& headers, Callback<QJsonDocument> cb, const GetOpts& o = {});
// Raw request through the module's transport (net::get, or the test override). Used by getJson and Feeds.
void send(const net::Request& r, net::Callback cb);
// The one-minute "host down" mark getJson keeps (host = "name" or "name:port"), for raw senders such as MarketInfo:
// hostDown() is true while marked; noteHostResult() marks it after a status-0 failure (timeout, refused, not found),
// after HTTP 429 / 418 (rate limit, ban) for the server's Retry-After, else 1 / 5 minutes, and clears it after any other answer; skipIfDown() fails the request without sending it if the host got marked down
// while it waited in net::Http's per-host queue (its reply then has status 0 and a non-empty error).
bool hostDown(const QString& host);
void noteHostResult(const QString& host, const net::Reply& rep);
void skipIfDown(net::Request& r, const QString& host);
int timeoutMs();   // JS TIMEOUT_MS: env QT_TIMEOUT_MS or 10000

// JSON of a body, parsed on a worker thread when large; cb on the GUI thread.
void parseJson(const QByteArray& body, std::function<void(const QJsonDocument&, const QString& parseError)> cb);

// ---------- cache ----------
// The JS `cached(key, ttlMs, fn)`: in-memory, with in-flight de-duplication (ten charts asking for the same thing
// make one request), bounded to 400 entries (oldest settled ones go first). Errors are not kept.
using AnyPtr = std::shared_ptr<const void>;
using AnyCb = std::function<void(const AnyPtr& value, const Error* error)>;
void cachedAny(const QString& key, qint64 ttlMs, std::function<void(AnyCb)> fn, AnyCb cb);

template <class T>
void cached(const QString& key, qint64 ttlMs, std::function<void(Callback<T>)> fn, Callback<T> cb) {
  cachedAny(
      key, ttlMs,
      [fn = std::move(fn)](AnyCb done) {
        fn([done](const Result<T>& r) {
          if (r.ok()) done(std::make_shared<const T>(r.value), nullptr);
          else done({}, &*r.error);
        });
      },
      [cb = std::move(cb)](const AnyPtr& v, const Error* e) {
        if (e) cb(Result<T>::failure(*e));
        else cb(Result<T>::success(*static_cast<const T*>(v.get())));
      });
}

// ---------- chains ----------
// One provider of a fallback chain: its name as shown on the chart, and how to load the bars.
struct ChainStep {
  QString provider;
  std::function<void(Callback<Bars>)> run;
};
using Chain = QList<ChainStep>;
struct ChainResult {
  Bars candles;
  QString provider;
  QStringList fallbacks;   // "Provider: why" for every provider tried before
};
// The JS tryChain: providers in order until one gives bars; "<provider>: no data" for an empty answer; when all
// fail, Error 502 "No source could load this chart. A: why | B: why".
void tryChain(const Chain& chain, Callback<ChainResult> cb);

// ---------- timeframe helpers (series.js through charts::series, errors as values) ----------
double tfSeconds(const QString& tf);              // parseTf(tf).seconds (tf already validated)
bool intraday(const QString& tf);                 // isIntraday
Bars resampleTo(const Bars& bars, const QString& tf);
QString nativeBaseOf(const QString& tf, const QStringList& natives);   // empty = JS null
Bars sliceLast(const Bars& b, int limit);          // JS slice(-limit)
Bars dedupe(const Bars& bars);                     // providers.js dedupe: ascending, last bar of a time wins
QString isoNow();                                  // new Date().toISOString()
QString isoMs(qint64 ms);                          // new Date(ms).toISOString()
QString isoDate(qint64 ms);                        // new Date(ms).toISOString().slice(0, 10)
double parseIsoMs(const QString& s);               // Date.parse of an ISO 8601 time (any fraction digits); NaN if bad
QString encodeUriComponent(const QString& s);      // JS encodeURIComponent
QString formEncode(const QList<QPair<QString, QString>>& q);   // URLSearchParams.toString()

// ---------- tests ----------
namespace testing {
// Replaces net::get for this module (tests only): fake answers, no network, no disk cache.
using Transport = std::function<void(const net::Request&, net::Callback)>;
void setTransport(Transport t);
void setTimeoutMs(int ms);
void clearFetchState();   // forget hosts marked down and the in-memory cache
}
}  // namespace charts::data
