#include "pages/charts/flow/LiqMapModel.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QtConcurrent>

namespace charts::ui::flow {

using namespace ::charts::flow;

bool LiqMapModel::Config::operator==(const Config& o) const {
  if (rows != o.rows || maxBars != o.maxBars || useRatio != o.useRatio || calibrate != o.calibrate ||
      params.leverages.size() != o.params.leverages.size())
    return false;
  for (size_t i = 0; i < params.leverages.size(); ++i)
    if (params.leverages[i].leverage != o.params.leverages[i].leverage || params.leverages[i].weight != o.params.leverages[i].weight) return false;
  if (params.brackets.size() != o.params.brackets.size()) return false;
  return params.positionNotional == o.params.positionNotional && params.candleBias == o.params.candleBias &&
         params.decayHalfLifeBars == o.params.decayHalfLifeBars && params.consume == o.params.consume;
}

LiqMapModel::LiqMapModel(const QString& symbol, QObject* parent)
    : QObject(parent), symbol_(symbol), stats_(data::FuturesStats::acquire(symbol)) {
  timer_.setSingleShot(true);
  connect(&timer_, &QTimer::timeout, this, &LiqMapModel::start);
  connect(&watcher_, &QFutureWatcher<std::shared_ptr<const Result>>::finished, this, [this] {
    // result() rethrows what the worker threw (only bad_alloc is possible): on the GUI thread that would end the app
    std::shared_ptr<const Result> r;
    try { r = watcher_.result(); } catch (const std::exception&) {}
    if (r && r->calibration && r->calibration != calib_) {   // a new calibration: remember what it was made from
      calib_ = r->calibration;
      calibEvents_ = stats_->liquidations().size();
      calibFirstBar_ = bars_.isEmpty() ? 0 : bars_.front().time;
      calibAt_ = QDateTime::currentMSecsSinceEpoch();
    }
    if (r && r->generation == gen_) {
      result_ = std::move(r);
      emit changed();
    }
    if (again_) {
      again_ = false;
      start();
    }
  });
  // new open interest every 5 minutes, pages of history while it fills: at most one computation every 2 s
  connect(stats_.get(), &data::FuturesStats::changed, this, [this] { schedule(2000); });
}

LiqMapModel::~LiqMapModel() {
  // no waitForFinished(): a calibration runs the model 72 times, and removing or hiding the indicator blocked the GUI
  // until it ended. The worker's lambda captures copies only (bars, open interest, ratios, events, settings), so it
  // may finish after this object is gone: QFuture's shared state holds the result, the disconnected watcher does not wait.
  ++gen_;
  watcher_.disconnect(this);
}

void LiqMapModel::schedule(int delayMs) {
  if (!timer_.isActive() || timer_.remainingTime() > delayMs) timer_.start(delayMs);
}

void LiqMapModel::setConfig(const Config& c) {
  if (configured_ && c == cfg_) return;
  cfg_ = c;
  configured_ = true;
  schedule(0);
}

void LiqMapModel::setBars(const Bars& bars, int64_t barMs) {
  // what matters: the closed bars (first, count, last closed one, its values) — a live tick of the forming bar changes nothing
  const int64_t now = QDateTime::currentMSecsSinceEpoch();
  qsizetype closed = bars.size();
  while (closed > 0 && bars[closed - 1].time * 1000 + barMs > now) --closed;
  QByteArray key;
  auto add = [&](auto v) { key.append(reinterpret_cast<const char*>(&v), sizeof v); };
  add(barMs);
  add(closed);
  if (closed) {
    add(bars.front().time);
    const Bar& b = bars[closed - 1];
    add(b.time);
    add(b.high);
    add(b.low);
    add(b.close);
    add(b.volume);
  }
  if (key == barsKey_) return;
  barsKey_ = key;
  bars_ = bars;
  barMs_ = barMs;
  schedule(0);
}

void LiqMapModel::start() {
  if (!configured_ || bars_.isEmpty() || barMs_ <= 0) return;
  if (watcher_.isRunning()) {   // one at a time; the newest inputs follow
    again_ = true;
    return;
  }
  const int gen = ++gen_;
  // copies for the worker (the GUI thread keeps changing its own)
  std::vector<Bar> bars(bars_.begin(), bars_.end());
  std::vector<OiPoint> oi = stats_->openInterest();
  std::vector<RatioPoint> ratios = stats_->ratios();
  Config cfg = cfg_;
  const int64_t barMs = barMs_, now = QDateTime::currentMSecsSinceEpoch();
  // calibrated mix: the last calibration while it still holds, else a new one on the worker from the real liquidations
  std::shared_ptr<const Calibration> calib;
  std::vector<LiqEvent> events;
  if (cfg.calibrate) {
    const size_t n = stats_->liquidations().size();
    const bool fresh = calib_ && n < calibEvents_ + std::max<size_t>(10, calibEvents_ / 10) && calibFirstBar_ == bars_.front().time &&
                       now - calibAt_ < 3600000;
    if (fresh) calib = calib_;
    else events = stats_->liquidations();
    // the grid must cover the widest mix tried (5×)
    cfg.params.leverages = calibrationLeverages();
  }
  watcher_.setFuture(QtConcurrent::run([bars = std::move(bars), oi = std::move(oi), ratios = std::move(ratios), events = std::move(events), calib,
                                        cfg, barMs, now, gen]() mutable {
    QElapsedTimer t;
    t.start();
    LiqInput in = prepareLiq(bars, barMs, now, oi, ratios, cfg.params, cfg.maxBars, cfg.rows, cfg.useRatio);
    if (cfg.calibrate) {
      if (!calib) calib = std::make_shared<const Calibration>(calibrate(in, cfg.params, events));
      cfg.params.leverages = calib->best;
    }
    auto r = std::make_shared<Result>(Result{LiquidationModel(in.grid, cfg.params), std::move(in.openMs), barMs, in.oiKnown, gen, 0, calib});
    if (in.grid.rows > 0) {
      r->model.reserve(in.close.size());
      r->model.run(in.series(), 0, in.close.size());
    }
    r->computeMs = double(t.nsecsElapsed()) / 1e6;
    return std::shared_ptr<const Result>(std::move(r));
  }));
}

}  // namespace charts::ui::flow
