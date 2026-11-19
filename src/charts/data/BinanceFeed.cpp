// Order flow: Binance's public trade data (spot and USD-M perpetuals) behind IExchangeFeed.
//   live      wss <host>/ws/<symbol>@aggTrade (spot), wss://fstream.binance.com/market/ws/<symbol>@aggTrade (futures)
//   history   GET /api/v3/aggTrades (weight 4) or /fapi/v1/aggTrades (weight 20; only the last 48 h), 1000 per page
//   tick size GET /api/v3/exchangeInfo?symbol=X or /fapi/v1/exchangeInfo, filter PRICE_FILTER
// Spot REST and WS hosts are the same exchange (api/data-api, stream/data-stream mirrors share trade ids).
// Binance.US is a different exchange with its own ids, so it is never mixed in here.
#include "charts/data/ExchangeFeed.h"
#include "net/Http.h"
#include "net/Sources.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <algorithm>

namespace charts::data {

std::optional<FlowMarket> FlowMarket::fromChartSymbol(const QString& chartSymbol) {
  QString s = chartSymbol.trimmed().toUpper();
  if (s.contains(':')) {
    if (s.section(':', 0, 0) != "BINANCE") return std::nullopt;
    s = s.section(':', 1);
  }
  FlowMarket m;
  if (s.endsWith(".P")) {
    m.venue = BinanceUsdm;
    s.chop(2);
  }
  static const QRegularExpression ok("^[A-Z0-9]{2,20}$");
  if (!ok.match(s).hasMatch()) return std::nullopt;
  m.symbol = s;
  return m;
}

QString FlowMarket::key() const { return (venue == BinanceUsdm ? "binance-usdm:" : "binance-spot:") + symbol; }

QStringList binanceTradeSocketUrls(const FlowMarket& m) {
  const QString stream = m.symbol.toLower() + "@aggTrade";
  if (m.venue == FlowMarket::BinanceUsdm) return {"wss://fstream.binance.com/market/ws/" + stream};
  return {"wss://stream.binance.com:9443/ws/" + stream, "wss://data-stream.binance.vision/ws/" + stream};
}

QStringList binanceRestBases(const FlowMarket& m) {
  if (m.venue == FlowMarket::BinanceUsdm) return {"https://fapi.binance.com/fapi/v1"};
  return {"https://api.binance.com/api/v3", "https://data-api.binance.vision/api/v3"};
}

QString binanceAggTradesPath(const FlowMarket& m, qint64 fromId, int limit) {
  QString p = "/aggTrades?symbol=" + m.symbol + "&limit=" + QString::number(std::clamp(limit, 1, 1000));
  if (fromId >= 0) p += "&fromId=" + QString::number(fromId);
  return p;
}

QString binanceLiquidationSocketUrl(const QString& symbol) {
  return "wss://fstream.binance.com/market/ws/" + symbol.toLower() + "@forceOrder";
}

QStringList binanceDepthSocketUrls(const FlowMarket& m) {
  const QString stream = m.symbol.toLower() + "@depth@100ms";
  if (m.venue == FlowMarket::BinanceUsdm) return {"wss://fstream.binance.com/public/ws/" + stream};
  return {"wss://stream.binance.com:9443/ws/" + stream, "wss://data-stream.binance.vision/ws/" + stream};
}

int binanceDepthLimit(const FlowMarket& m) { return m.venue == FlowMarket::BinanceUsdm ? 1000 : 5000; }

namespace {
// A timer can be dropped from inside its own timeout: the ping slot aborts the socket, and if the close handler runs
// synchronously it lands here. Stopped now, deleted later — never under its own emit.
void dropTimer(std::unique_ptr<QTimer>& t) {
  if (QTimer* p = t.release()) { p->stop(); p->deleteLater(); }
}

std::function<std::unique_ptr<IDepthSocket>()>& depthFactory() {
  static std::function<std::unique_ptr<IDepthSocket>()> f;
  return f;
}

// The depth diff stream, in the depth thread: parse each message into one reused DepthDiff (no allocation once warm)
// and hand it to the sink. Same host and ping rules as the trade socket.
class BinanceDepthSocket : public IDepthSocket {
public:
  BinanceDepthSocket() {
    diff_.bids.reserve(4096);
    diff_.asks.reserve(4096);
  }
  ~BinanceDepthSocket() override { close(); }
  void open(const QStringList& urls, const flow::TickScale& scale, IOrderBookSink* sink) override {
    close();
    urls_ = urls;
    scale_ = scale;
    sink_ = sink;
    sink_->onDepthState(FeedState::Connecting, {});
    openAt(0);
  }
  void close() override {
    ++gen_;
    dropTimer(pingTimer_);
    if (ws_) {
      QWebSocket* ws = ws_;
      ws_ = nullptr;
      ws->disconnect();
      ws->abort();
      ws->deleteLater();
    }
  }

private:
  void openAt(int idx) {
    if (idx >= urls_.size()) {
      sink_->onDepthState(FeedState::Closed, opened_ ? "connection lost" : "no Binance stream host could be reached");
      return;
    }
    const int gen = gen_;
    opened_ = false;
    auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, &ctx_);
    ws_ = ws;
    QObject::connect(ws, &QWebSocket::connected, &ctx_, [this, ws, gen] {
      if (gen != gen_) return;
      opened_ = true;
      sink_->onDepthState(FeedState::Open, {});
      pingTimer_ = std::make_unique<QTimer>();
      waitingPong_ = false;
      QObject::connect(pingTimer_.get(), &QTimer::timeout, &ctx_, [this, ws, gen] {
        if (gen != gen_ || ws_ != ws) return;
        if (waitingPong_) { ws->abort(); return; }
        waitingPong_ = true;
        ws->ping();
      });
      pingTimer_->start(20000);   // a pong must come back before the next tick (20 s)
    });
    QObject::connect(ws, &QWebSocket::pong, &ctx_, [this, gen](quint64, const QByteArray&) { if (gen == gen_) waitingPong_ = false; });
    QObject::connect(ws, &QWebSocket::textMessageReceived, &ctx_, [this, gen](const QString& msg) {
      if (gen != gen_) return;
      if (flow::parseDepthUpdate(QStringView(msg), scale_, diff_)) sink_->onDepthDiff(diff_);
    });
    auto closed = std::make_shared<bool>(false);
    auto onClose = [this, ws, gen, idx, closed] {
      if (*closed || gen != gen_) return;
      *closed = true;
      const QString why = ws->errorString();
      ws_ = nullptr;
      dropTimer(pingTimer_);
      ws->disconnect();
      ws->deleteLater();
      if (!opened_) { openAt(idx + 1); return; }
      sink_->onDepthState(FeedState::Closed, why.isEmpty() ? QStringLiteral("closed by the server") : why);
    };
    QObject::connect(ws, &QWebSocket::disconnected, &ctx_, onClose);
    QObject::connect(ws, &QWebSocket::errorOccurred, &ctx_, onClose);
    QNetworkRequest req{QUrl(urls_[idx])};
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent_);
    ws->open(req);
  }

  QObject ctx_;
  QStringList urls_;
  flow::TickScale scale_;
  IOrderBookSink* sink_ = nullptr;
  QWebSocket* ws_ = nullptr;
  std::unique_ptr<QTimer> pingTimer_;
  bool opened_ = false, waitingPong_ = false;
  int gen_ = 0;
  flow::DepthDiff diff_;
  const QString userAgent_ = net::userAgent();
};

std::function<std::unique_ptr<IExchangeFeed>(const FlowMarket&)>& factory() {
  static std::function<std::unique_ptr<IExchangeFeed>(const FlowMarket&)> f;
  return f;
}

// Which spot REST base answered last (both are the same exchange; keep using the one that works).
int& spotRestIdx() { static int i = 0; return i; }

class BinanceFeed : public IExchangeFeed {
public:
  ~BinanceFeed() override {
    *alive_ = false;
    closeTrades();
    closeLiquidations();
  }

  void fetchFuturesData(const QString& pathAndQuery, std::function<void(const Result<QByteArray>&)> cb) override {
    rawGet({"https://fapi.binance.com"}, 0, 0, pathAndQuery, std::move(cb));
  }

  // One socket, few messages (Binance sends at most one liquidation per symbol per second, often none for minutes):
  // a ping every 30 s tells a quiet socket from a dead one.
  void openLiquidations(const QString& symbol, ILiquidationSink* sink) override {
    closeLiquidations();
    liqSink_ = sink;
    const int gen = ++liqGen_;
    const QUrl url(binanceLiquidationSocketUrl(symbol));
    if (sources::isHostDisabled(url.host())) {
      later([this, gen, alive = alive_] { if (*alive && gen == liqGen_) liqSink_->onLiquidationState(FeedState::Closed, "switched off in Sources"); });
      return;
    }
    liqSink_->onLiquidationState(FeedState::Connecting, {});
    auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, &ctx_);
    liqWs_ = ws;
    liqWaitingPong_ = false;
    QObject::connect(ws, &QWebSocket::connected, &ctx_, [this, ws, gen] {
      if (gen != liqGen_) return;
      liqSink_->onLiquidationState(FeedState::Open, {});
      liqPing_ = std::make_unique<QTimer>();
      QObject::connect(liqPing_.get(), &QTimer::timeout, &ctx_, [this, ws, gen] {
        if (gen != liqGen_ || liqWs_ != ws) return;
        if (liqWaitingPong_) { ws->abort(); return; }
        liqWaitingPong_ = true;
        ws->ping();
      });
      liqPing_->start(30000);
    });
    QObject::connect(ws, &QWebSocket::pong, &ctx_, [this, gen](quint64, const QByteArray&) { if (gen == liqGen_) liqWaitingPong_ = false; });
    QObject::connect(ws, &QWebSocket::textMessageReceived, &ctx_, [this, gen](const QString& msg) {
      if (gen != liqGen_) return;
      flow::LiqEvent e;
      if (flow::parseForceOrder(msg.toUtf8(), e)) liqSink_->onLiquidation(e);
    });
    auto closed = std::make_shared<bool>(false);
    auto onClose = [this, ws, gen, closed] {
      if (*closed || gen != liqGen_) return;
      *closed = true;
      const QString why = ws->errorString();
      liqWs_ = nullptr;
      liqPing_.reset();
      ws->disconnect();
      ws->deleteLater();
      liqSink_->onLiquidationState(FeedState::Closed, why.isEmpty() ? QStringLiteral("closed by the server") : why);
    };
    QObject::connect(ws, &QWebSocket::disconnected, &ctx_, onClose);
    QObject::connect(ws, &QWebSocket::errorOccurred, &ctx_, onClose);
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::UserAgentHeader, net::userAgent());
    ws->open(req);
  }

  void closeLiquidations() override {
    ++liqGen_;
    liqPing_.reset();
    if (liqWs_) {
      QWebSocket* ws = liqWs_;
      liqWs_ = nullptr;
      ws->disconnect();
      ws->abort();
      ws->deleteLater();
    }
  }

  void openTrades(const FlowMarket& m, ITradeSink* sink) override {
    closeTrades();
    sink_ = sink;
    urls_ = binanceTradeSocketUrls(m);
    sink_->onFeedState(FeedState::Connecting, {});
    openSocket(goodUrl_);
  }

  void closeTrades() override {
    ++gen_;
    dropTimer(pingTimer_);
    if (ws_) {
      QWebSocket* ws = ws_;
      ws_ = nullptr;
      ws->disconnect();
      ws->abort();
      ws->deleteLater();
    }
  }

  void fetchTrades(const FlowMarket& m, qint64 fromId, int limit, TradesCb cb) override {
    const QStringList bases = binanceRestBases(m);
    const int first = m.venue == FlowMarket::BinanceSpot ? spotRestIdx() % int(bases.size()) : 0;
    tryRest(bases, first, 0, binanceAggTradesPath(m, fromId, limit), m.venue == FlowMarket::BinanceSpot, std::move(cb));
  }

  void fetchTickSize(const FlowMarket& m, std::function<void(const Result<flow::Decimal>&)> cb) override {
    fetchTickSizeFrom(m, m.venue == FlowMarket::BinanceSpot ? spotRestIdx() % 2 : 0, 0, std::move(cb));
  }

  // Spot tries the other base when one fails (where api.binance.com is blocked, the data-api mirror still answers and
  // no order flow started at all; audit D9) and remembers the one that answered, as the trades do.
  void fetchTickSizeFrom(const FlowMarket& m, int idx, int tried, std::function<void(const Result<flow::Decimal>&)> cb) {
    const bool fut = m.venue == FlowMarket::BinanceUsdm;
    const QStringList bases = binanceRestBases(m);
    // Spot: the one-symbol answer is small. Futures has no symbol filter: the whole list (~1 MB), cached 6 h.
    const QString url = fut ? "https://fapi.binance.com/fapi/v1/exchangeInfo" : bases.value(idx) + "/exchangeInfo?symbol=" + m.symbol;
    auto alive = alive_;
    const QString sym = m.symbol;
    getJson(url, {}, [this, alive, cb, sym, m, idx, tried, fut, n = int(bases.size())](const Result<QJsonDocument>& r) {
      if (!*alive) return;
      if (!r.ok()) {
        // 400 (unknown symbol) is the same on every host, and 429 / 418 (rate limit, ban) count against the IP on
        // both: not reasons to ask the other one
        const int st = r.error->status;
        if (!fut && tried + 1 < n && st != 400 && st != 429 && st != 418) { fetchTickSizeFrom(m, (idx + 1) % n, tried + 1, cb); return; }
        cb(Result<flow::Decimal>::failure(*r.error));
        return;
      }
      if (!fut) spotRestIdx() = idx;
      for (const QJsonValue& v : r.value.object().value("symbols").toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value("symbol").toString() != sym) continue;
        for (const QJsonValue& f : o.value("filters").toArray()) {
          const QJsonObject fo = f.toObject();
          if (fo.value("filterType").toString() != "PRICE_FILTER") continue;
          const QByteArray t = fo.value("tickSize").toString().toLatin1();
          const flow::Decimal d = flow::parseDecimal(t.constData(), t.constData() + t.size());
          if (d.ok && d.mant > 0) { cb(Result<flow::Decimal>::success(d)); return; }
        }
      }
      cb(Result<flow::Decimal>::failure(Error{"Binance doesn't list " + sym + " (no tick size)", 404, true, {}}));
    }, GetOpts{fut ? 6 * 3600 : 24 * 3600, true});
  }

  void fetchDepthSnapshot(const FlowMarket& m, std::function<void(const Result<QByteArray>&)> cb) override {
    const QStringList bases = binanceRestBases(m);
    const int first = m.venue == FlowMarket::BinanceSpot ? spotRestIdx() % int(bases.size()) : 0;
    rawGet(bases, first, 0, "/depth?symbol=" + m.symbol + "&limit=" + QString::number(binanceDepthLimit(m)), std::move(cb));
  }

  int historyPaceMs(const FlowMarket& m) const override {
    // spot: 4 weight × 200/min = 800 of 6000/min; futures: 20 × 60/min = 1200 of 2400/min (charts' klines need room)
    return m.venue == FlowMarket::BinanceUsdm ? 1000 : 300;
  }

private:
  void openSocket(int idx) {
    while (idx < urls_.size() && sources::isHostDisabled(QUrl(urls_[idx]).host())) ++idx;
    if (idx >= urls_.size()) {
      // every host failed before opening (or is switched off): next time start from the first again
      goodUrl_ = 0;
      sink_->onFeedState(FeedState::Closed, opened_ ? "connection lost" : "no Binance stream host could be reached");
      return;
    }
    const int gen = gen_;
    opened_ = false;
    auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, &ctx_);
    ws_ = ws;
    QObject::connect(ws, &QWebSocket::connected, &ctx_, [this, ws, gen, idx] {
      if (gen != gen_) return;
      opened_ = true;
      goodUrl_ = idx;
      sink_->onFeedState(FeedState::Open, {});
      startPings(ws, gen);
    });
    QObject::connect(ws, &QWebSocket::textMessageReceived, &ctx_, [this, gen](const QString& msg) {
      if (gen != gen_) return;
      flow::AggTrade t;
      if (flow::parseAggTrade(QStringView(msg), t)) sink_->onTrade(t);
    });
    QObject::connect(ws, &QWebSocket::pong, &ctx_, [this, gen](quint64, const QByteArray&) {
      if (gen == gen_) waitingPong_ = false;
    });
    auto closed = std::make_shared<bool>(false);
    auto onClose = [this, ws, gen, idx, closed] {
      if (*closed || gen != gen_) return;
      *closed = true;
      const QString why = ws->errorString();
      ws_ = nullptr;
      dropTimer(pingTimer_);
      ws->disconnect();
      ws->deleteLater();
      if (!opened_) { openSocket(idx + 1); return; }   // never opened: next host
      sink_->onFeedState(FeedState::Closed, why.isEmpty() ? QStringLiteral("closed by the server") : why);
    };
    QObject::connect(ws, &QWebSocket::disconnected, &ctx_, onClose);
    QObject::connect(ws, &QWebSocket::errorOccurred, &ctx_, onClose);
    QNetworkRequest req{QUrl(urls_[idx])};
    req.setHeader(QNetworkRequest::UserAgentHeader, net::userAgent());
    ws->open(req);
  }

  // A connection can die without a close frame (sleep, Wi-Fi change): ping every 30 s, give up without a pong in
  // 15 s. (Qt answers the server's own pings by itself.)
  void startPings(QWebSocket* ws, int gen) {
    pingTimer_ = std::make_unique<QTimer>();
    waitingPong_ = false;
    QObject::connect(pingTimer_.get(), &QTimer::timeout, &ctx_, [this, ws, gen] {
      if (gen != gen_ || ws_ != ws) return;
      if (waitingPong_) { ws->abort(); return; }
      waitingPong_ = true;
      ws->ping();
      QTimer::singleShot(15000, &ctx_, [this, ws, gen] {
        if (gen == gen_ && ws_ == ws && waitingPong_) ws->abort();
      });
    });
    pingTimer_->start(30000);
  }

  void tryRest(const QStringList& bases, int idx, int tried, const QString& path, bool spot, TradesCb cb) {
    const QString base = bases.value(idx);
    net::Request r;
    r.url = QUrl(base + path);
    r.ttlSec = 0;
    r.staleOnError = false;   // never an old copy: trades must be the exchange's answer of now
    r.timeoutMs = timeoutMs();
    const QString host = r.url.host();
    if (hostDown(host) && tried + 1 < bases.size()) {
      tryRest(bases, (idx + 1) % int(bases.size()), tried + 1, path, spot, std::move(cb));
      return;
    }
    skipIfDown(r, host);
    auto alive = alive_;
    send(r, [this, alive, bases, idx, tried, path, spot, cb, host](const net::Reply& rep) {
      if (!*alive) return;
      noteHostResult(host, rep);
      if (rep.status == 429 || rep.status == 418 || rep.status == 400) {   // rate limit / ban / bad symbol: not a host problem
        cb(Result<std::vector<flow::AggTrade>>::failure(Error{QString("%1 returned HTTP %2").arg(host).arg(rep.status), rep.status, true, {}}));
        return;
      }
      if (rep.status < 200 || rep.status >= 300) {
        if (tried + 1 < bases.size()) { tryRest(bases, (idx + 1) % int(bases.size()), tried + 1, path, spot, cb); return; }
        const QString why = rep.status ? QString("%1 returned HTTP %2").arg(host).arg(rep.status) : QString("%1 couldn't be reached (%2)").arg(host, rep.error);
        cb(Result<std::vector<flow::AggTrade>>::failure(Error{why, rep.status, true, {}}));
        return;
      }
      std::vector<flow::AggTrade> page;
      page.reserve(1000);
      if (!flow::parseAggTradesPage(QByteArrayView(rep.body), page)) {
        cb(Result<std::vector<flow::AggTrade>>::failure(Error{host + " sent trades we can't read", 502, true, {}}));
        return;
      }
      if (spot) spotRestIdx() = idx;
      cb(Result<std::vector<flow::AggTrade>>::success(std::move(page)));
    });
  }

  // A REST body as is (status errors as Error; 429/418/400 never hop to the next host).
  void rawGet(const QStringList& bases, int idx, int tried, const QString& path, std::function<void(const Result<QByteArray>&)> cb) {
    net::Request r;
    r.url = QUrl(bases.value(idx) + path);
    r.ttlSec = 0;
    r.staleOnError = false;
    r.timeoutMs = timeoutMs();
    const QString host = r.url.host();
    skipIfDown(r, host);
    auto alive = alive_;
    send(r, [this, alive, bases, idx, tried, path, cb, host](const net::Reply& rep) {
      if (!*alive) return;
      noteHostResult(host, rep);
      if (rep.status >= 200 && rep.status < 300) { cb(Result<QByteArray>::success(rep.body)); return; }
      const bool hop = rep.status != 429 && rep.status != 418 && rep.status != 400 && tried + 1 < bases.size();
      if (hop) { rawGet(bases, (idx + 1) % int(bases.size()), tried + 1, path, cb); return; }
      const QString why = rep.status ? QString("%1 returned HTTP %2").arg(host).arg(rep.status) : QString("%1 couldn't be reached (%2)").arg(host, rep.error);
      cb(Result<QByteArray>::failure(Error{why, rep.status, true, {}}));
    });
  }

  QObject ctx_;                       // owns the sockets and the connections' lifetime
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
  ITradeSink* sink_ = nullptr;
  QStringList urls_;
  QWebSocket* ws_ = nullptr;
  std::unique_ptr<QTimer> pingTimer_;
  bool waitingPong_ = false, opened_ = false;
  int goodUrl_ = 0;                   // the host that opened last time is tried first
  int gen_ = 0;
  // liquidation stream (its own socket)
  ILiquidationSink* liqSink_ = nullptr;
  QWebSocket* liqWs_ = nullptr;
  std::unique_ptr<QTimer> liqPing_;
  bool liqWaitingPong_ = false;
  int liqGen_ = 0;
};

}  // namespace

std::unique_ptr<IExchangeFeed> makeExchangeFeed(const FlowMarket& m) {
  if (factory()) return factory()(m);
  return std::make_unique<BinanceFeed>();
}

std::unique_ptr<IDepthSocket> makeDepthSocket() {
  if (depthFactory()) return depthFactory()();
  return std::make_unique<BinanceDepthSocket>();
}

namespace testing {
void setFeedFactory(std::function<std::unique_ptr<IExchangeFeed>(const FlowMarket&)> f) { factory() = std::move(f); }
void setDepthSocketFactory(std::function<std::unique_ptr<IDepthSocket>()> f) { depthFactory() = std::move(f); }
}

}  // namespace charts::data
