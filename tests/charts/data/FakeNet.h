#pragma once
// The fake network of providers_tests / feeds_tests (the JS tests replace globalThis.fetch): every request of
// src/charts/data goes to a handler through charts::data::testing::setTransport, nothing leaves the machine.
#include "charts/data/Fetch.h"
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <memory>
#include <vector>

namespace fakenet {

struct Reply {
  int status = 200;
  QByteArray body;
  enum { Normal, Drop, Dns } kind = Normal;
  static Reply json(const QJsonValue& v, int status = 200) {
    Reply r;
    r.status = status;
    r.body = v.isArray() ? QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact) : QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
    return r;
  }
  static Reply text(const QByteArray& b, int status) { Reply r; r.status = status; r.body = b; return r; }
  static Reply drop() { Reply r; r.kind = Drop; return r; }
  static Reply dns() { Reply r; r.kind = Dns; return r; }
};

struct Call {
  QString host, url;
  QList<QPair<QByteArray, QByteArray>> headers;
  QByteArray header(const QByteArray& name) const {
    for (const auto& h : headers) if (h.first.compare(name, Qt::CaseInsensitive) == 0) return h.second;
    return {};
  }
};
inline std::vector<Call>& calls() { static std::vector<Call> c; return c; }
inline QString param(const QUrl& u, const QString& k) { return QUrlQuery(u).queryItemValue(k, QUrl::FullyDecoded); }

using Handler = std::function<Reply(const QUrl&)>;
inline void install(Handler h) {
  charts::data::testing::setTransport([h](const net::Request& r, net::Callback cb) {
    calls().push_back(Call{r.url.host(), r.url.toString(QUrl::FullyEncoded), r.headers});
    const Reply f = h(r.url);
    net::Reply rep;
    rep.fetched = QDateTime::currentDateTimeUtc();
    if (f.kind == Reply::Drop) {
      // A dropped connection: nothing until the request's own timeout, then Qt's "Operation canceled".
      rep.error = "Operation canceled";
      QTimer::singleShot(r.timeoutMs, [cb, rep] { cb(rep); });
      return;
    }
    if (f.kind == Reply::Dns) rep.error = "Host " + r.url.host() + " not found";
    else {
      rep.status = f.status;
      rep.body = f.body;
      if (f.status < 200 || f.status >= 300) rep.error = QString("HTTP %1").arg(f.status);
    }
    QTimer::singleShot(0, [cb, rep] { cb(rep); });
  });
}

// Waits (event loop) for an async call of the module, like `await`.
template <class T>
charts::data::Result<T> await(std::function<void(charts::data::Callback<T>)> fn, int maxMs = 15000) {
  struct St { std::optional<charts::data::Result<T>> out; QEventLoop* loop = nullptr; };
  auto st = std::make_shared<St>();
  fn([st](const charts::data::Result<T>& r) {
    st->out = r;
    if (st->loop) st->loop->quit();
  });
  if (!st->out) {
    QEventLoop loop;
    st->loop = &loop;
    QTimer::singleShot(maxMs, &loop, &QEventLoop::quit);
    loop.exec();
    st->loop = nullptr;
  }
  if (!st->out) return charts::data::Result<T>::failure(charts::data::Error{"test: no answer in time"});
  return *st->out;
}
}  // namespace fakenet
