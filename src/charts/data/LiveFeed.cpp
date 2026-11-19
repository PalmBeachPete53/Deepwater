// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
#include "charts/data/LiveFeed.h"
#include "charts/data/Providers.h"
#include "net/Http.h"
#include "net/Sources.h"
#include <QWebSocket>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>

namespace charts::data {

// The WebSocket hosts join the sources that serve the same data over REST (declare() merges hosts by id).
static const sources::Registrar regWsSpot({"binance", "Binance (spot market data API)", {"stream.binance.com"}, sources::Access::Keyless, 200,
    "Live klines over WebSocket", "Public market streams without key (one connection per chart; 300 connections per 5 min per IP allowed)",
    {"charts/charts"}});
static const sources::Registrar regWsVision({"binance-vision", "Binance (public market-data mirror)", {"data-stream.binance.vision"},
    sources::Access::Keyless, 200, "Live klines over WebSocket (mirror)", "Binance's public market-data stream mirror", {"charts/charts"}});
static const sources::Registrar regWsFut({"binance-futures", "Binance USD-M futures (market data API)", {"fstream.binance.com"},
    sources::Access::Keyless, 200, "Live perpetual klines over WebSocket", "Public market streams without key", {"charts/charts"}});

namespace {
const QStringList SPOT_HOSTS = {"wss://stream.binance.com:9443/ws/", "wss://data-stream.binance.vision/ws/"};
// Futures market streams (kline, aggTrade) moved to /market/ws/: the legacy /ws/ path opens but sends no kline data
// since Binance retired it on 2026-04-23 (measured 2026-11-18: 0 messages in 5 s, 7 on /market/ws/).
const QStringList FUTURES_HOSTS = {"wss://fstream.binance.com/market/ws/"};
QString exchangeOf(const QString& id) { return id.contains(':') ? id.section(':', 0, 0) : QString(); }
QString tickerOf(const QString& id) { return id.contains(':') ? id.section(':', 1) : id; }
double plusStr(const QJsonValue& v) { return v.isString() ? v.toString().toDouble() : v.toDouble(); }
}  // namespace

QString LiveFeed::binanceWsTf(const QString& tf) {
  static const QHash<QString, QString> m = {{"1s", "1s"},   {"1m", "1m"},   {"3m", "3m"}, {"5m", "5m"}, {"15m", "15m"}, {"30m", "30m"},
                                            {"1h", "1h"},   {"2h", "2h"},   {"4h", "4h"}, {"6h", "6h"}, {"8h", "8h"},   {"12h", "12h"},
                                            {"1D", "1d"},   {"3D", "3d"},   {"1W", "1w"}, {"1M", "1M"}};
  return m.value(tf);
}

int LiveFeed::pollMs(const QString& symbol) {
  return QStringList{"BINANCE", "COINBASE", "KRAKEN"}.contains(exchangeOf(symbol)) ? 3000 : 10000;
}

LiveFeed::LiveFeed(QObject* parent) : QObject(parent) {
  timer_.setSingleShot(true);
  socketRetry_.setSingleShot(true);
}

LiveFeed::~LiveFeed() { stop(); }

void LiveFeed::start(const QString& symbol, const QString& tf, const QString& provider) {
  symbol_ = symbol;
  tf_ = tf;
  provider_ = provider;
  socketFails_ = 0;
  startLive();
}

void LiveFeed::startLive() {
  stop();
  const QString ex = exchangeOf(symbol_);
  const QString wsTf = binanceWsTf(tf_);
  if (ex == "BINANCE" && provider_ == "Binance" && !wsTf.isEmpty())
    openSocket(tickerOf(symbol_).toLower(), wsTf, 0, SPOT_HOSTS);
  else if (ex == "BINANCE" && provider_ == "Binance futures" && !wsTf.isEmpty() && wsTf != "1s") {
    QString sym = tickerOf(symbol_).toLower();
    if (sym.endsWith(".p")) sym.chop(2);
    openSocket(sym, wsTf, 0, FUTURES_HOSTS);
  } else
    poll();
}

void LiveFeed::openSocket(const QString& sym, const QString& interval, int hostIdx, const QStringList& hosts) {
  // A host switched off in the Sources tab is skipped like one that never opened.
  while (hostIdx < hosts.size() && sources::isHostDisabled(QUrl(hosts[hostIdx]).host())) hostIdx++;
  if (hostIdx >= hosts.size()) {
    // No host opened: poll meanwhile, and try the socket again later (one failed reconnect used to leave the chart
    // on REST polling for good; audit D19). A retry that fails again lands here while the polling goes on.
    if (!polling_) poll();
    retrySocketLater(sym, interval, hosts);
    return;
  }
  auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
  ws_ = ws;
  url_ = hosts[hostIdx] + sym + "@kline_" + interval;
  auto opened = std::make_shared<bool>(false);
  auto closed = std::make_shared<bool>(false);
  auto waitingPong = std::make_shared<bool>(false);
  connect(ws, &QWebSocket::connected, this, [this, ws, opened, waitingPong] {
    if (ws_ != ws) return;
    *opened = true;
    socketFails_ = 0;
    if (polling_) {   // a background retry opened: the socket takes over from the polling
      ++generation_;
      timer_.stop();
      disconnect(&timer_, &QTimer::timeout, this, nullptr);
      polling_ = false;
    }
    // Ping logic of BinanceFeed's trade socket (audit D9): a kline socket that died without a close frame froze the
    // chart after a sleep. The timer is the socket's child: it goes with it.
    auto* ping = new QTimer(ws);
    connect(ping, &QTimer::timeout, ws, [ws, waitingPong] {
      if (*waitingPong) { ws->abort(); return; }
      *waitingPong = true;
      ws->ping();
      QTimer::singleShot(15000, ws, [ws, waitingPong] { if (*waitingPong) ws->abort(); });
    });
    connect(ws, &QWebSocket::pong, ws, [waitingPong](quint64, const QByteArray&) { *waitingPong = false; });
    ping->start(30000);
    emit socketOpened();
  });
  connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString& m) {
    if (ws_ != ws) return;
    const QJsonObject k = QJsonDocument::fromJson(m.toUtf8()).object().value("k").toObject();
    if (k.isEmpty()) return;
    emit barsUpdated(Bars{Bar{qint64(k.value("t").toDouble() / 1000), plusStr(k.value("o")), plusStr(k.value("h")), plusStr(k.value("l")),
                              plusStr(k.value("c")), plusStr(k.value("v"))}});
  });
  // JS onclose. A failed connection reports an error (and not always a disconnect): both end here, once.
  auto onClose = [this, ws, opened, closed, sym, interval, hostIdx, hosts]() {
    if (*closed) return;
    *closed = true;
    ws->deleteLater();
    if (ws_ != ws) return;
    ws_ = nullptr;
    url_.clear();
    if (!*opened) openSocket(sym, interval, hostIdx + 1, hosts);
    else {
      connect(&timer_, &QTimer::timeout, this, &LiveFeed::startLive, Qt::SingleShotConnection);
      timer_.start(3000);
    }
  };
  connect(ws, &QWebSocket::disconnected, this, onClose);
  connect(ws, &QWebSocket::errorOccurred, this, onClose);
  QNetworkRequest req{QUrl(url_)};
  req.setHeader(QNetworkRequest::UserAgentHeader, net::userAgent());
  ws->open(req);
}

void LiveFeed::retrySocketLater(const QString& sym, const QString& interval, const QStringList& hosts) {
  disconnect(&socketRetry_, &QTimer::timeout, this, nullptr);
  connect(&socketRetry_, &QTimer::timeout, this, [this, sym, interval, hosts] {
    if (!polling_ || ws_) return;
    openSocket(sym, interval, 0, hosts);
  }, Qt::SingleShotConnection);
  socketRetry_.start(std::min(300000, 5000 << std::min(socketFails_++, 6)));
}

void LiveFeed::poll() {
  polling_ = true;
  connect(&timer_, &QTimer::timeout, this, &LiveFeed::tick, Qt::SingleShotConnection);
  timer_.start(pollMs(symbol_));
}

void LiveFeed::tick() {
  const int gen = generation_;
  getCandles(symbol_, tf_, 5, this, [this, gen](const Result<CandlesResult>& r) {
    if (gen != generation_) return;
    if (r.ok()) emit barsUpdated(r.value.candles);   // errors: keep trying
    poll();
  });
}

void LiveFeed::stop() {
  ++generation_;
  timer_.stop();
  disconnect(&timer_, &QTimer::timeout, this, nullptr);
  socketRetry_.stop();
  disconnect(&socketRetry_, &QTimer::timeout, this, nullptr);
  polling_ = false;
  if (ws_) {
    QWebSocket* ws = ws_;
    ws_ = nullptr;
    url_.clear();
    ws->close();
    ws->deleteLater();
  }
}

}  // namespace charts::data
