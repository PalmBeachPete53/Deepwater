#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/alerts.js. Alerts run in the app's data layer, so they keep firing while the Charts tab is hidden
// (as long as Deepwater is running), like the JS server. An alert is a condition in the same format the strategy
// tester uses:
//
// { symbol: 'BINANCE:BTCUSDT', tf: '1h', left: 'close', op: 'crossesAbove', right: 100000,
//   indicators: {}, trigger: 'once' | 'oncePerBar' | 'oncePerBarClose', webhook: 'https://...',
//   message: 'BTC broke 100k' }
//
// Give `watchlist: 'Main'` instead of a symbol and the alert checks every symbol on that list
// (read fresh each time from state.json, so edits to the list count). "Only once" then means once per symbol.
//
// Alerts stay QJsonObject so the UI, the local HTTP API and your AI agent pass the same JSON as the JS. Files:
// <data>/charts/alerts.json (the list) and alert-log.json (the last 500 fired events, newest first).
//
// The files are the shared truth (the JS keeps its own copy in memory): two Deepwaters on one data folder (the second
// one's API gets port 3778) or a hand edit must not undo each other. So every change (add/update/remove, and each
// change the checker makes: lastBar, lastBars, fired, active, error, the log) re-reads the file first when it changed
// since this instance last read or wrote it, applies the change to that fresh list and writes it back, under a short
// lock file <data>/charts/alerts.lock. list()/history()/find() re-read a file that changed (size and modification
// time, then the bytes), and a change seen on disk (a watcher on the folder) emits changed(). One instance alone
// writes the same files and JSON as the JS.
//
// Everything runs on the GUI thread (the thread that first calls instance()); callbacks come back there.
#include "charts/data/Fetch.h"
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QByteArray>
#include <functional>
#include <optional>
#include <memory>

class QFileSystemWatcher;

namespace charts::alerts {

class Alerts : public QObject {
  Q_OBJECT
public:
  static Alerts& instance();

  QJsonArray list() const;      // every alert (JS list())
  QJsonArray history() const;   // fired events, newest first (JS history())
  std::optional<QJsonObject> find(const QString& id) const;

  // Up to five conditions (TradingView's multi-condition alerts); `match` says whether all or any must hold.
  // Older alerts with a single left/op/right still work.
  static QJsonArray conditionsOf(const QJsonObject& a);
  // The JS validate: nullopt when the alert is fine, else Error{message, 400}. Messages as the JS.
  static std::optional<data::Error> validate(const QJsonObject& a);
  // "BTC broke 100k" / "BINANCE:BTCUSDT 1h: close crosses above 100000" (symbol defaults to a.symbol).
  static QString describe(const QJsonObject& a);
  static QString describe(const QJsonObject& a, const QString& symbol);

  // input as sent to POST /api/alerts. Error 400 "Send the alert as a JSON object" for a non-object, or the
  // validate() message. On success the stored alert (with id, createdAt, ...).
  data::Result<QJsonObject> add(const QJsonValue& input);
  // Error 404 "No such alert", or the validate() message for the merged alert.
  data::Result<QJsonObject> update(const QString& id, const QJsonObject& patch);
  void remove(const QString& id);

  // POSTs the alert to a webhook: Discord {content}, Slack {text}, Telegram {text}, anything else
  // {text, content, ...payload}. done(error): empty = sent, else "Webhook returned HTTP 500" or the network error.
  static void sendWebhook(const QString& url, const QString& text, const QJsonObject& payload,
                          std::function<void(const QString& error)> done);
  // POSTs JSON to a webhook URL (10 s timeout, never cached). done(status, error): status 0 = no answer, then error
  // is the JS fetch message ("fetch failed", "The operation was aborted due to timeout", "Invalid URL").
  // Hosts nobody declared join the "webhook" source (net::Http paces them, the Sources tab lists them).
  static void postJson(const QString& url, const QJsonObject& body, std::function<void(int status, const QString& error)> done);
  // The JSON body sendWebhook posts to `url` (exposed for tests and the webhook test route).
  static QJsonObject webhookBody(const QString& url, const QString& text, const QJsonObject& payload);

  // One evaluation at a time: the timer and a manual "check now" must not both fire a "once" alert. done() runs when
  // the current (or just started) evaluation finishes.
  void evaluate(std::function<void()> done = {});
  bool evaluating() const { return running_; }

  // The checker: first check 2 s after start, then every intervalMs after the previous check ended.
  void start(int intervalMs = 15000);
  void stop();
  bool started() const { return started_; }

  // Re-reads alerts.json and alert-log.json (tests, after DW_PROFILE / the data folder changed).
  void reload();
  // Re-reads both files if they changed on disk (another Deepwater, a hand edit) and emits changed() (queued) if the
  // list or the log did. The folder watcher calls it; list()/history()/find() do the same check.
  void syncFromDisk();

signals:
  // A fired event, the payload the JS server broadcast as SSE "alert":
  // {id, alertId, symbol, tf, text, price, barTime, at, webhook?: "sent" | "failed: ..."}
  void fired(const QJsonObject& event);
  // The alert list changed (added, updated, removed, or fired: fired count, active, lastBar...), here or on disk
  // (another Deepwater on this data folder; the log too).
  void changed();

private:
  Alerts();
  // What this instance last read from or wrote to a file: its stamp (size, modification time in ns) and bytes.
  struct Disk {
    bool exists = false;
    qint64 size = -1, mtime = 0;
    QByteArray bytes;
    bool known = false;   // read or written at least once
    QByteArray badCopied;   // the unparsable content last copied aside (store::keepAside)
  };
  // Re-reads <name>.json into `into` when it differs from `d` (force: compare the bytes even when the stamp is the
  // same). true when `into` changed; then changed() is queued unless quiet.
  bool syncFile(const QString& name, QJsonArray& into, Disk& d, bool force, bool quiet = false) const;
  // Writes value unless the file already holds exactly that (audit G11: the checker rewrote alerts.json every 15 s
  // with nothing changed: disk writes, watcher callbacks, the view rebuilt). True when it wrote.
  bool writeFile(const QString& name, const QJsonArray& value, Disk& d);
  // The lock around read-modify-write of alerts.json / alert-log.json, then the fresh lists (force-synced).
  class Lock;
  std::unique_ptr<Lock> lockAndSync();
  void save(std::unique_ptr<Lock>& lock);   // writes alerts.json, releases the lock, emits changed()
  void noteLoadError(const QStringList& ids, const QString& error);   // "loadError" on these alerts ("" clears it)
  void saveQuiet();     // writes alerts.json only (the checker's changes during a run; save() at its end emits)
  void queueChanged() const;
  void watch();
  int indexOf(const QString& id) const;
  void fire(const QString& id, const Bar& bar, double price, const QString& symbol,
            std::function<void(QJsonObject&)> mark, std::function<void()> done);
  void evaluateOnce(std::function<void()> done);
  void finishRun();

  mutable QJsonArray alerts_;
  mutable QJsonArray log_;
  mutable Disk alertsDisk_, logDisk_;
  mutable bool changeQueued_ = false;
  QFileSystemWatcher* watcher_ = nullptr;
  QString watched_;
  bool running_ = false;
  QList<std::function<void()>> waiters_;
  QTimer timer_;
  bool started_ = false;
  int interval_ = 15000;
};

inline Alerts& instance() { return Alerts::instance(); }

}  // namespace charts::alerts
