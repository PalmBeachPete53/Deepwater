#pragma once
// Order flow, chantier E, step E2: one futures symbol's statistics for the liquidation heatmap, shared by everything
// that uses that symbol (acquire). GUI thread.
//
//   open interest, 5 min   /futures/data/openInterestHist     Binance keeps 30 days: we fetch from the last stored point
//   long/short accounts    /futures/data/globalLongShortAccountRatio   (or 29.5 days back), windows of 500 periods
//                                                                       (start and end: a start alone gives the latest)
//   funding rates          /fapi/v1/fundingRate                         forward, then again every 5 minutes. Kept on disk, so the
//   real liquidations      …/market/ws/<symbol>@forceOrder (live)       history grows past Binance's 30 days while
//                                                                       Deepwater runs (FuturesCollector keeps BTCUSDT).
// Files: data/charts/futures/<SYMBOL>/{oi5m,ratio5m,funding,liquidations}.dat (FuturesData.h record files). New points
// are appended; a point landing between stored ones (a hole filled) rewrites the file.
// Requests go one at a time through the app's HTTP queue (paced by the sources registry); an error waits (1 s … 5 min)
// and goes on from the same place (429: a minute at least, 418: ten minutes). An old start Binance refuses (HTTP 400)
// moves the start to 29 days back, once per job; a second 400 waits like any error. Real liquidations older than a
// year are dropped from memory and from the file (they would otherwise grow without limit while Deepwater runs).
// One writer per symbol folder across processes (<dir>/.lock, QLockFile): a second Deepwater on the same data folder
// keeps its points in memory only; when the first one quits it takes the lock over at its next flush, merges the
// files with what it holds and writes them whole (two appenders wrote the same points twice, out of order).
#include "charts/core/FuturesData.h"
#include "charts/data/ExchangeFeed.h"
#include <QObject>
#include <QTimer>
class QLockFile;
#include <deque>
#include <functional>
#include <memory>

namespace charts::data {

class FuturesStats : public QObject, public ILiquidationSink {
  Q_OBJECT
public:
  static std::shared_ptr<FuturesStats> acquire(const QString& symbol);   // "BTCUSDT"
  ~FuturesStats() override;

  const QString& symbol() const { return symbol_; }
  QString dir() const;
  const std::vector<flow::OiPoint>& openInterest() const { return oi_; }
  const std::vector<flow::RatioPoint>& ratios() const { return ratio_; }
  const std::vector<flow::FundingPoint>& funding() const { return funding_; }
  const std::vector<flow::LiqEvent>& liquidations() const { return liq_; }

  struct Status {
    bool caughtUp = false;   // every series reached now at least once
    int requests = 0, errors = 0;
    FeedState live = FeedState::Closed;
    QString error;           // the last one (empty once a request succeeds)
  };
  const Status& status() const { return status_; }

  // Tests: poll interval, pause between pages, error backoff base (ms; 0 = defaults), and the clock.
  static void setTimingForTests(int pollMs, int pageGapMs, int backoffMs);
  static void setClockForTests(std::function<qint64()> nowMs);

  // ILiquidationSink
  void onLiquidation(const flow::LiqEvent& e) override;
  void onLiquidationState(FeedState s, const QString& why) override;

signals:
  void changed();         // new points or liquidations
  void statusChanged();

private:
  explicit FuturesStats(const QString& symbol);
  void start();
  void loadDisk();
  void queueCatchUp();
  void next();
  void onPage(int kind, qint64 start, const Result<QByteArray>& r);
  void openLive();
  void flushLiquidations();
  void dropOldLiquidations();
  bool acquireLock();     // true when this process writes the folder (now or already)
  void takeOverFiles();   // the lock just came free: the files and memory merged, files rewritten

  enum Kind { Oi = 1, Ratio = 2, Funding = 3, Liquidations = 4 };
  // moved: the start was already moved later once after an HTTP 400; a second 400 is not Binance's 30 days counted
  // differently but a symbol it refuses (delisted, not a futures market), and it backs off instead (audit G3).
  struct Job { int kind; qint64 start; bool moved = false; };
  QString file(int kind) const;
  qint64 startFor(int kind) const;

  QString symbol_;
  std::unique_ptr<IExchangeFeed> feed_;
  std::vector<flow::OiPoint> oi_;
  std::vector<flow::RatioPoint> ratio_;
  std::vector<flow::FundingPoint> funding_;
  std::vector<flow::LiqEvent> liq_;
  size_t liqSaved_ = 0;
  bool liqRewrite_ = false;   // old events were dropped: the file must be rewritten, not appended to
  std::unique_ptr<QLockFile> lock_;   // held while this process writes <dir>; null: another Deepwater does
  std::deque<Job> jobs_;
  bool busy_ = false;
  int fails_ = 0, liveFails_ = 0;
  QTimer pollTimer_, retryTimer_, liveTimer_, flushTimer_;
  Status status_;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

}  // namespace charts::data
