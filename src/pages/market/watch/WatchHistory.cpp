#include <QCoreApplication>
#include "pages/market/watch/WatchHistory.h"
#include "market/Instruments.h"
#include "net/Http.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimeZone>

namespace watch {
WatchHistory::WatchHistory(QObject* parent) : QObject(parent) {}

void WatchHistory::ensure(const QStringList& symbols) {
  QStringList yahoo, apps;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (const auto& s : symbols) {
    if (m_asked.contains(s) || m_retryAt.value(s, 0) > now) continue;
    const auto* i = market::find(s);
    if (!i) continue;
    m_asked.insert(s); yahoo << i->yahoo; apps << i->symbol;
  }
  for (int k = 0; k < yahoo.size(); k += 20) {
    QUrl u("https://query1.finance.yahoo.com/v7/finance/spark");
    QUrlQuery q; q.addQueryItem("symbols", yahoo.mid(k, 20).join(',')); q.addQueryItem("range", "3mo"); q.addQueryItem("interval", "1d");
    u.setQuery(q);
    net::Request r; r.url = u; r.ttlSec = 1800;
    ++m_pending;
    const QStringList batch = apps.mid(k, 20);
    net::get(r, this, [this, batch](const net::Reply& rep) {
      --m_pending;
      QSet<QString> got;
      if (rep.ok()) {
        const auto res = QJsonDocument::fromJson(rep.body).object()["spark"].toObject()["result"].toArray();
        for (const auto& v : res) {
          const auto o = v.toObject();
          const auto* ins = market::find(o["symbol"].toString());
          const auto resp = o["response"].toArray();
          if (!ins || resp.isEmpty()) continue;
          const auto r0 = resp[0].toObject();
          const auto ts = r0["timestamp"].toArray();
          const auto cl = r0["indicators"].toObject()["quote"].toArray().first().toObject()["close"].toArray();
          // Yahoo stamps daily bars at the exchange's session open; the local date of the exchange is what counts
          const int gmtoff = r0["meta"].toObject()["gmtoffset"].toInt();
          Closes c;
          for (int i = 0; i < ts.size() && i < cl.size(); ++i) {
            if (cl[i].isNull()) continue;
            c.t << qint64(ts[i].toDouble()) + gmtoff; c.c << cl[i].toDouble();
          }
          if (!c.t.isEmpty()) { m_closes.insert(ins->symbol, c); got.insert(ins->symbol); }
        }
      }
      // every market of the batch is asked again by a later ensure(): what arrived once its 30 min cache has expired
      // (it was never asked again, so a long session kept the 1W / 1M references of its first hour); what did not
      // arrive soon after a failed request, later when Yahoo simply had nothing for it (closes already known stay
      // shown meanwhile)
      const qint64 now = QDateTime::currentMSecsSinceEpoch();
      const qint64 missing = now + (rep.ok() ? 10 * 60000 : 60000);
      for (const auto& s : batch) {
        m_asked.remove(s);
        m_retryAt.insert(s, got.contains(s) ? now + 1800 * 1000 : missing);
      }
      emit updated();
    });
  }
}

double WatchHistory::closeOnOrBefore(const QString& s, const QDate& day, QDate* when) const {
  auto it = m_closes.constFind(s);
  if (it == m_closes.cend()) return NAN;
  const qint64 lim = QDateTime(day.addDays(1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  for (int i = it->t.size() - 1; i >= 0; --i)
    if (it->t[i] < lim) {
      if (when) *when = QDateTime::fromSecsSinceEpoch(it->t[i], QTimeZone::UTC).date();
      return it->c[i];
    }
  return NAN;
}
}
