#pragma once
#include <QObject>
#include <QUrl>
#include <QDateTime>
#include <QPointer>
#include <functional>

class QNetworkAccessManager;

// One HTTP client for the whole app: per-host pacing from sources::, disk cache with TTL,
// stale fallback when the network fails. Callbacks run on the GUI thread, always asynchronously; a callback
// whose context object was destroyed is skipped. Identical GET/HEAD requests waiting for one host (same URL, cache
// key, headers) share one network request, and a queued request whose cached answer became fresh while it waited
// (an identical earlier request) is served from the cache. Redirects to another host are checked like a request.
namespace net {

struct Request {
  QUrl url;
  QByteArray method = "GET";
  QByteArray body;
  QList<QPair<QByteArray, QByteArray>> headers;
  int ttlSec = 0;           // > 0: a cached answer younger than this is served without a request
  bool staleOnError = true; // serve any cached answer if the request fails
  int timeoutMs = 30000;
  QString cacheKey;         // default: method + url (+ body hash)
  bool priority = false;    // ahead of the host's other queued requests (priority ones keep their order)
  // behind every other request to the host, even ones queued later: preloading a page nobody looks at yet (set by
  // net::BackgroundScope, with the page's id as the tag). A foreground caller asking for the same URL, or
  // promote(tag) when that page is opened, brings it back into line.
  bool background = false;
  QString backgroundTag;
  // Called when the request leaves its host queue, just before it is sent (requests to one host go one at a time):
  // a non-empty text fails it with that error (status 0) without sending it. E.g. a host found down by an earlier
  // request in the same queue, so each queued request doesn't wait for its own timeout.
  std::function<QString()> preflight;
};

struct Reply {
  int status = 0;
  QByteArray body;
  QString error;            // empty when ok
  bool fromCache = false;
  bool stale = false;       // served from cache after a failure
  QDateTime fetched;        // when the body was downloaded (UTC)
  // A 429 / 503's Retry-After, in seconds (-1: none). While it runs, the host's later requests are held when the wait
  // is short (<= kHoldMaxSec) and answered 429 at once otherwise, with this field set to the time left.
  int retryAfterSec = -1;
  bool ok() const { return error.isEmpty() && status >= 200 && status < 300; }
};

using Callback = std::function<void(const Reply&)>;

constexpr int kHoldMaxSec = 10;          // a shorter Retry-After delays the queue, a longer one fails it fast
constexpr int kRetryAfterCapSec = 3600;  // a server asking for more is still asked again after an hour
// The Retry-After header (RFC 9110: delay-seconds or an HTTP-date) in seconds from nowUtc, capped at
// kRetryAfterCapSec; -1 when absent or unreadable.
int retryAfterSeconds(const QByteArray& value, const QDateTime& nowUtc);

void get(const Request& r, QObject* context, Callback cb);

// Every request made on this thread while one is alive is a background request (Request::background) tagged `tag`:
// the services a page uses can be started ahead of the page without changing them.
class BackgroundScope {
public:
  explicit BackgroundScope(const QString& tag);
  ~BackgroundScope();
  BackgroundScope(const BackgroundScope&) = delete;
  BackgroundScope& operator=(const BackgroundScope&) = delete;
private:
  QString m_previous;
};
// The background requests with this tag still queued become ordinary ones (the page they were preloading for is now
// on screen). Returns how many.
int promote(const QString& tag);
inline void get(const QUrl& u, int ttlSec, QObject* context, Callback cb) {
  Request r; r.url = u; r.ttlSec = ttlSec; get(r, context, std::move(cb));
}
// Cached body (any age) or empty; does not touch the network.
Reply cached(const Request& r);
void invalidate(const Request& r);   // drop a cached answer (e.g. a 200 error page)
QNetworkAccessManager* manager();
QByteArray userAgent();
}
