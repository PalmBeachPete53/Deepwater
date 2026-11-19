#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// The data part of ChartPane.startLive / openSocket / poll / stopLive (public/js/chart.js): live bars for one
// chart. A Binance chart (spot or USD-M perpetual) listens to Binance's kline WebSocket; everything else polls
// getCandles(symbol, tf, 5) every 3 s (crypto exchanges) or 10 s. The chart merges what barsUpdated sends
// (same time as its last bar = replace it, later = append), as the JS merge().
//
// Socket rules, as the JS: try stream.binance.com:9443, then data-stream.binance.vision (spot) or
// fstream.binance.com (perpetuals); a host that never opened passes to the next, and when none opened the
// chart polls instead, and tries the socket again in the background (5 s, 10 s … 5 min) until one opens, then stops
// polling. A socket that was open and closed reconnects after 3 s (from the first host again). An open socket is
// pinged every 30 s; without a pong in 15 s (a connection that died without a close frame: sleep, Wi-Fi change) it
// is dropped and reconnects like a closed one.
// The UI starts it only when the chart has bars and is not replaying, and stops it on hide / symbol change.
#include "charts/core/Types.h"
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QPointer>

class QWebSocket;

namespace charts::data {

class LiveFeed : public QObject {
  Q_OBJECT
public:
  explicit LiveFeed(QObject* parent = nullptr);
  ~LiveFeed() override;

  // symbol: the canonical id the chart loaded ("BINANCE:BTCUSDT", "US:AAPL"…); tf like "1m", "1D";
  // provider: CandlesResult::provider of the loaded chart (a socket only for "Binance" / "Binance futures").
  void start(const QString& symbol, const QString& tf, const QString& provider);
  void stop();

  bool isSocket() const { return ws_ != nullptr; }
  bool isPolling() const { return polling_; }
  QString socketUrl() const { return url_; }   // the stream being listened to (empty when polling)

  // JS BINANCE_WS_TF: the kline intervals Binance streams; empty = none (then the chart polls).
  static QString binanceWsTf(const QString& tf);
  // Poll interval in ms for a symbol: 3000 for BINANCE/COINBASE/KRAKEN, else 10000.
  static int pollMs(const QString& symbol);

signals:
  void barsUpdated(const charts::Bars& bars);   // bars to merge into the chart
  void socketOpened();                          // JS app.setStatus("ok")

private:
  void startLive();
  void openSocket(const QString& sym, const QString& interval, int hostIdx, const QStringList& hosts);
  void poll();
  void tick();
  void retrySocketLater(const QString& sym, const QString& interval, const QStringList& hosts);

  QString symbol_, tf_, provider_, url_;
  QWebSocket* ws_ = nullptr;
  QTimer timer_;
  QTimer socketRetry_;   // while polling a chart that wants a socket
  int socketFails_ = 0;
  bool polling_ = false;
  int generation_ = 0;   // bumps on every stop: late answers of an old poll are dropped
};

}  // namespace charts::data
