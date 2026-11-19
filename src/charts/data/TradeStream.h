#pragma once
// Order flow: the executed trades of one market, live and recent history, shared by every chart that needs them
// (footprint, Big Trades). One stream per market however many panes use it.
//
//   auto s = TradeStream::acquire(*FlowMarket::fromChartSymbol("BINANCE:BTCUSDT.P"));   // keep the shared_ptr
//   connect(s.get(), &TradeStream::appended, ...);   s->tape(), s->ticks()
//
// What it does, in order: tick size (exchangeInfo) → live socket → history from REST, newest first, until the
// budget (trades kept from before the chart opened; live trades come on top, up to 2× the budget, then the oldest
// go). Gaps (a reconnect, a page that failed) are listed by the tape and filled from REST before more history.
// Reconnects back off 1 s, 2 s, 4 s … 60 s; a connection that stayed up a minute resets that. Binance closes every
// socket after 24 h: the stream reconnects itself at 23 h 50 and the REST fill covers the seconds in between.
// REST errors 429 / 418 (rate limit / ban) stop the history for a minute / five minutes.
//
// Threading: GUI thread only (QWebSocket and net::get). A busy perpetual sends a few hundred trades a second; each
// costs one parse (no allocation) and one append.
#include "charts/core/Ticks.h"
#include "charts/core/Trades.h"
#include "charts/data/ExchangeFeed.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace charts::data {

class TradeStream : public QObject, private ITradeSink {
  Q_OBJECT
public:
  static constexpr int kDefaultBudget = 300000;   // ~1-4 h of BTCUSDT; user's choice (2026-11-02)
  // The stream of this market (made on first use). A larger budget than the stream's raises it.
  static std::shared_ptr<TradeStream> acquire(const FlowMarket& m, int budget = kDefaultBudget);
  ~TradeStream() override;

  const FlowMarket& market() const { return market_; }
  bool ready() const { return scale_.valid(); }          // tick size known: ticks() and tape() in use
  const flow::TickScale& ticks() const { return scale_; }
  const flow::TradeTape& tape() const { return *tape_; }

  struct Status {
    FeedState feed = FeedState::Closed;
    int reconnects = 0;          // live connections lost (planned 24 h renewals not counted)
    bool historyDone = false;    // budget reached, or the exchange has no older trades
    int budget = 0;
    qint64 liveTrades = 0;       // received on the socket
    bool approximate = false;    // some side came from the quote or tick rule (never on Binance)
    QString error;               // last problem, for the legend ("" = none)
  };
  const Status& status() const { return status_; }

  // Tests: backoff base, the 24 h renewal and the 429 pause, in ms (0 = defaults).
  static void setTimingForTests(int backoffBaseMs, int renewAfterMs, int rateLimitMs);

signals:
  void becameReady();
  void appended();          // live trades added at the end of the tape
  void historyChanged();    // trades inserted before the end (history, filled gap) or the tape rebuilt
  void statusChanged();

private:
  TradeStream(const FlowMarket& m, int budget);
  static void dispose(TradeStream* s);   // last user gone: socket closed now, object deleted on the next turn
  void start();
  void openLive();
  void reopenLater();
  void schedulePump(int delayMs);
  void pump();
  void raiseBudget(int budget);
  flow::Trade toTrade(const flow::AggTrade& t);
  void onTrade(const flow::AggTrade& t) override;
  void onFeedState(FeedState s, const QString& why) override;
  void setError(const QString& e);

  FlowMarket market_;
  int budget_;
  std::unique_ptr<IExchangeFeed> feed_;
  flow::TickScale scale_;
  std::unique_ptr<flow::TradeTape> tape_;
  flow::SideClassifier classifier_;
  Status status_;
  QTimer pumpTimer_, reopenTimer_, renewTimer_;
  bool jobRunning_ = false, latestDone_ = false, historyEnd_ = false, renewing_ = false;
  int fails_ = 0, restFails_ = 0, gapNoProgress_ = 0;
  qint64 openedAt_ = 0;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
  std::vector<flow::Trade> scratch_;
};

}  // namespace charts::data
