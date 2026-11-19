#pragma once
#include <QObject>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QDate>
#include <cmath>

// Daily closes of the last three months for the list's 1W / 1M change (Yahoo, the same source as market::candles
// but through the batched spark endpoint: 20 markets per request, so 127 markets cost 7 requests instead of 127
// chart requests that would hold up the shared Yahoo queue). Cached 30 min by net::get.
namespace watch {
class WatchHistory : public QObject {
  Q_OBJECT
public:
  explicit WatchHistory(QObject* parent = nullptr);
  void ensure(const QStringList& symbols);           // app symbols; fetches what is missing or older than 30 min (a
                                                     // failed fetch is asked again by a later call after 1 min; a
                                                     // market Yahoo had nothing for: 10 min)
  bool has(const QString& symbol) const { return m_closes.contains(symbol); }
  int pending() const { return m_pending; }
  // Close of the last session on or before `day` (NaN if unknown); `when` receives that session's date.
  double closeOnOrBefore(const QString& symbol, const QDate& day, QDate* when = nullptr) const;
signals:
  void updated();
private:
  struct Closes { QVector<qint64> t; QVector<double> c; };
  QHash<QString, Closes> m_closes;
  QSet<QString> m_asked;                             // in flight
  QHash<QString, qint64> m_retryAt;                  // not asked again before this time (ms since epoch): 30 min after
                                                     // an answer (the cache's TTL), sooner after a failure
  int m_pending = 0;
};
}
