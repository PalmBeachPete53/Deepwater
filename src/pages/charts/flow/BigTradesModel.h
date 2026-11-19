#pragma once
// Charts tab — the big trades of one Big Trades indicator: the shared trade stream of its market folded into
// aggregated orders (charts/core/BigTrades: Aggregator → Detector), keeping those judged big.
//   - the history (and any change of settings) is computed on a worker thread from a copy of the tape, in time order,
//     so the threshold of each order only uses the orders before it; the result is adopted on the GUI thread, then the
//     trades that arrived meanwhile are added;
//   - live trades are added one by one; an order still open is closed once no trade can join it any more (a 100 ms
//     timer on the exchange's clock: last trade time + time elapsed since it arrived);
//   - changed() at most every 100 ms.
#include "charts/core/BigTrades.h"
#include "charts/data/TradeStream.h"
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>
#include <vector>

namespace charts::ui::flow {

class BigTradesModel : public QObject {
  Q_OBJECT
public:
  struct Config {
    ::charts::flow::AggregatorParams agg;
    ::charts::flow::DetectorParams det;
    bool operator==(const Config& o) const;
  };
  struct Bubble {
    ::charts::flow::AggregatedTrade a;
    double threshold = 0;   // the minimum it passed (the bubble's size reference)
  };

  explicit BigTradesModel(const data::FlowMarket& market, QObject* parent = nullptr);
  ~BigTradesModel() override;
  void setConfig(const Config& c);          // a change recomputes everything
  const Config& config() const { return cfg_; }
  const data::FlowMarket& market() const { return market_; }
  bool ready() const { return stream_->ready(); }
  bool computing() const { return watcher_.isRunning(); }
  const ::charts::flow::TickScale& ticks() const { return stream_->ticks(); }
  const data::TradeStream::Status& status() const { return stream_->status(); }
  qint64 coveredFromMs() const;
  const std::vector<Bubble>& bubbles() const { return bubbles_; }   // by start time
  // The threshold the next order would be judged with, now (exchange clock), and the sizes in the window.
  ::charts::flow::Detector::Verdict current();
  size_t samples() const { return det_.samples(); }
  // Bubbles whose start time is in [fromMs, toMs).
  std::pair<size_t, size_t> range(int64_t fromMs, int64_t toMs) const;

  // Live alerts: an order judged big now (not while computing the history), at least factor × its threshold, from the
  // last minute. factor ≤ 0: off.
  void setAlertFactor(double factor) { alertFactor_ = factor; }
  int alertsFired() const { return alertsFired_; }

signals:
  void changed();
  void alert(const ::charts::flow::AggregatedTrade& a, double threshold);

private:
  struct Snapshot {
    ::charts::flow::Aggregator agg;
    ::charts::flow::Detector det;
    std::vector<Bubble> bubbles;
    int64_t lastId = -1, lastTradeMs = 0;
  };
  static Snapshot compute(std::vector<::charts::flow::Trade> trades, Config cfg);
  void startRebuild();
  void adopt(Snapshot s);
  void addNew();
  void judge(const ::charts::flow::AggregatedTrade& a);
  void tick();
  void notify();

  data::FlowMarket market_;
  std::shared_ptr<data::TradeStream> stream_;
  Config cfg_;
  bool configured_ = false;
  ::charts::flow::Aggregator agg_;
  ::charts::flow::Detector det_;
  std::vector<Bubble> bubbles_;
  int64_t lastId_ = -1;
  int64_t lastTradeMs_ = 0;   // exchange time of the last trade folded in
  qint64 lastTradeWall_ = 0;  // when it arrived (wall clock, ms)
  QFutureWatcher<Snapshot> watcher_;
  int gen_ = 0;
  bool rebuildAgain_ = false;
  QTimer rebuildTimer_, flushTimer_, notifyTimer_;
  double alertFactor_ = 0;
  int alertsFired_ = 0;
};

}  // namespace charts::ui::flow
