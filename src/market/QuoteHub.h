#pragma once
#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QVector>
#include <cmath>

// Live quotes for the whole app (Yahoo Finance spark endpoint, 20 symbols per request).
// Pages subscribe with an owner; only symbols of visible owners are polled:
//   market::QuoteHub::instance().subscribe(this, {"EURUSD", "SPX"});
//   connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, ...);
//   in showEvent/hideEvent: QuoteHub::instance().setActive(this, true/false);
// Symbols are app symbols from market::instruments() (Yahoo tickers also accepted).
namespace market {
struct Quote {
  QString symbol;
  double price = NAN, prevClose = NAN, change = NAN, changePct = NAN;
  double dayHigh = NAN, dayLow = NAN, volume = NAN, high52 = NAN, low52 = NAN;
  QDateTime time;                 // last trade time
  QString currency, exchangeTz;
  QVector<qint64> sparkT;         // today's intraday 5-minute closes (UTC seconds)
  QVector<double> spark;
  bool stale = false;             // from a cached answer served because the poll failed (offline): not a live quote
  bool valid() const { return std::isfinite(price); }
};

class QuoteHub : public QObject {
  Q_OBJECT
public:
  static QuoteHub& instance();
  // urgent: the owner's first quotes go ahead of the other requests waiting for Yahoo (net::Request::priority) — for a
  // few symbols shown at once at the top of a page (the Overview's tape), not for long lists.
  void subscribe(QObject* owner, const QStringList& symbols, bool active = true, bool urgent = false);
  void setActive(QObject* owner, bool active);
  void unsubscribe(QObject* owner);
  Quote quote(const QString& symbol) const;
  bool has(const QString& symbol) const { return m_quotes.contains(symbol); }
  void refreshNow();
  int intervalMs() const { return m_timer.interval(); }
signals:
  void updated(const QStringList& symbols);
private:
  QuoteHub();
  void poll();
  void request(const QStringList& symbols, bool force, bool priority = false);
  struct Sub { QSet<QString> symbols; bool active; bool urgent = false; };
  QHash<QObject*, Sub> m_subs;
  QSet<QObject*> m_watched;          // owners whose destroyed() is connected
  QHash<QString, Quote> m_quotes;   // by app symbol
  QHash<QString, qint64> m_fetched; // by app symbol: when a live (not stale) answer last landed, ms since epoch
  QTimer m_timer;
};
}
