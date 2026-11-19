#pragma once
// Charts tab — footprint data of one chart pane: the shared trade stream of its market (src/charts/data/TradeStream)
// folded into footprint candles of the pane's timeframe (src/charts/core/Footprint).
//   - live trades are added as they come (only the new ones: the tape is in id order);
//   - history pages and filled gaps rebuild the book from the tape (order-free sums, ~5 ms for 300 000 trades),
//     at most every 250 ms while the history loads;
//   - changed() is emitted at most every 100 ms, so a busy market does not repaint the chart hundreds of times a second.
#include "charts/core/Footprint.h"
#include "charts/data/TradeStream.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace charts::ui::flow {

class FootprintFeed : public QObject {
  Q_OBJECT
public:
  // tf: the chart's timeframe ("1m", "5m", "1h" …). market: from FlowMarket::fromChartSymbol.
  FootprintFeed(const data::FlowMarket& market, const QString& tf, QObject* parent = nullptr);
  ~FootprintFeed() override;

  const data::FlowMarket& market() const { return market_; }
  const QString& tf() const { return tf_; }
  bool ready() const { return stream_->ready(); }
  const ::charts::flow::TickScale& ticks() const { return stream_->ticks(); }
  const ::charts::flow::FootprintBook& book() const { return book_; }
  const data::TradeStream::Status& status() const { return stream_->status(); }
  // The footprint candle of a chart bar (bar time in UTC seconds), or null when it has no trades or is partial (the
  // history starts inside it): such bars stay plain candles.
  const ::charts::flow::FootprintCandle* complete(qint64 barTime) const;
  qint64 coveredFromMs() const;        // time of the oldest trade we have (0 = none yet)

signals:
  void changed(bool rebuilt);         // rebuilt: candles before the last one may have changed too

private:
  void addNew();
  void rebuild();
  void notify(bool rebuilt);

  data::FlowMarket market_;
  QString tf_;
  std::shared_ptr<data::TradeStream> stream_;
  ::charts::flow::FootprintBook book_;
  int64_t lastId_ = -1;                // last tape id folded into the book
  QTimer rebuildTimer_, notifyTimer_;
  bool pendingRebuilt_ = false;
};

}  // namespace charts::ui::flow
