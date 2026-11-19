#pragma once
// Charts tab — the liquidation heatmap's data (indicator "liqmap", step E3): the chart's bars and the symbol's futures
// statistics (charts/data/FuturesStats) → the liquidation model (charts/core/Liquidation), computed on a worker thread.
//
// The whole model is computed again (it takes ~10 ms for 2 000 bars) when its inputs change: the settings, a new closed
// bar (a still forming bar never counts), new open interest points (every 5 minutes), or the chart's history. At most one
// computation runs at a time; when inputs change during one, the newest run follows. The result is read-only once
// published (shared_ptr to const), so the layer can keep drawing it while the next one is computed.
#include "charts/core/LiqCalibration.h"
#include "charts/core/LiqRender.h"
#include "charts/data/FuturesStats.h"
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>

namespace charts::ui::flow {

class LiqMapModel : public QObject {
  Q_OBJECT
public:
  struct Config {
    ::charts::flow::LiqParams params;
    int rows = 800;
    int maxBars = 3000;
    bool useRatio = true;
    bool calibrate = false;   // the leverage mix chosen from the real liquidations (step E4) instead of params.leverages
    bool operator==(const Config& o) const;
  };
  struct Result {
    ::charts::flow::LiquidationModel model;
    std::vector<int64_t> openMs;   // one per model column
    int64_t barMs = 0;
    int oiKnown = 0;
    int generation = 0;
    double computeMs = 0;
    std::shared_ptr<const ::charts::flow::Calibration> calibration;   // when Config::calibrate
  };

  explicit LiqMapModel(const QString& symbol, QObject* parent = nullptr);
  ~LiqMapModel() override;
  const QString& symbol() const { return symbol_; }
  data::FuturesStats* stats() const { return stats_.get(); }

  void setConfig(const Config& c);
  // The chart's bars (as loaded; in replay, cut at the replayed bar) and their timeframe.
  void setBars(const Bars& bars, int64_t barMs);
  std::shared_ptr<const Result> result() const { return result_; }
  bool computing() const { return watcher_.isRunning(); }
  double lastComputeMs() const { return result_ ? result_->computeMs : 0; }

signals:
  void changed();

private:
  void schedule(int delayMs);
  void start();

  QString symbol_;
  std::shared_ptr<data::FuturesStats> stats_;
  Config cfg_;
  bool configured_ = false;
  Bars bars_;
  int64_t barMs_ = 0;
  QByteArray barsKey_;
  std::shared_ptr<const Result> result_;
  QFutureWatcher<std::shared_ptr<const Result>> watcher_;
  QTimer timer_;
  bool again_ = false;
  int gen_ = 0;
  // the last calibration and what it was made from (a new one when the real liquidations grew by 10 %, the bars moved,
  // or an hour passed — it runs the model 72 times)
  std::shared_ptr<const ::charts::flow::Calibration> calib_;
  size_t calibEvents_ = 0;
  int64_t calibFirstBar_ = 0, calibAt_ = 0;
};

}  // namespace charts::ui::flow
