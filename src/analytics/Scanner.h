#pragma once
#include "analytics/Technical.h"
#include "market/Instruments.h"
#include <QObject>
#include <QHash>
#include <QSet>

// Loads the daily candles of every catalogue instrument (plus the ETF volume proxies) progressively through
// market::candles() (paced and cached by net::get), and computes their technical reads off the GUI thread.
namespace analytics {
class Scanner : public QObject {
  Q_OBJECT
public:
  struct Item {
    market::Instrument ins;
    TechRead read;
    bool loaded = false;     // read computed at least once
    bool failed = false;
    QString error;
  };
  explicit Scanner(QObject* parent = nullptr);
  void start();                                  // (re)load everything; cached answers return at once
  const QVector<Item>& items() const { return m_items; }
  int pending() const { return m_pending; }
  int requested() const { return m_requested; }
  bool running() const { return m_pending > 0; }
signals:
  void progress(int done, int total);
  void itemUpdated(int index);
  void finished();
private:
  void onDaily(int index, const market::Series& s);
  void onProxy(const QString& ticker, const market::Series& s);
  void compute(int index);
  QVector<Item> m_items;
  QHash<QString, QVector<Bar>> m_daily;      // app symbol → bars
  QHash<QString, QVector<Bar>> m_proxy;      // proxy ticker → bars (empty when it failed)
  QSet<QString> m_proxyDone;
  QVector<int> m_computeSeq;                 // per item: the latest compute(); older results that finish later are dropped
  int m_pending = 0, m_requested = 0, m_generation = 0;
};
}
