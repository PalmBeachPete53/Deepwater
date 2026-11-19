#pragma once
// Charts tab — structural liquidity (SMC / ICT) drawn on the chart (indicator "smcliq", steps F4–F5). The levels come
// from charts/core/LiquidityLevels (pivots, equal highs / lows, sweeps) and SessionLevels (previous day / week, sessions).
//
// Detection timeframe: the chart's own bars, or a larger timeframe made from them by the Resampler (detect on 1h, show
// on 5m). On a larger timeframe only its CLOSED bars make levels (a bar still forming would make pivots appear and
// vanish); the chart's bars after the last closed one still sweep levels, live. Levels travel by time: a level swept by
// a 1h bar ends on the first chart bar inside that hour that went through it.
// Incremental: the engine (and the resampler) keep their state; each new CLOSED chart bar is fed to them once — the
// bar still forming only checks sweeps (cheap). The whole history is computed again only when it changes (a new load,
// replay going back) or the settings do. Same results either way: the same steps in the same order.
// Drawing: a zone (or a line) from the bar that made the level to its sweep, or to the right edge while it holds;
// buy-side (above highs) and sell-side (below lows) in their colours, opacity from the score (the strongest on screen
// opaque); a swept level stays greyed for `keepAfterSweep` bars, then goes. Only levels in the visible price range.
#include "charts/core/LiquidityLevels.h"
#include "charts/core/Resampler.h"
#include <optional>
#include "pages/charts/plot/Primitive.h"
#include <QColor>
#include <QString>
#include <functional>
#include <memory>
#include <vector>

namespace charts::ui {
class ChartPane;
}

namespace charts::ui::flow {

class SmcLayer : public ::charts::plot::Primitive {
public:
  struct Config {
    ::charts::smc::EngineParams engine;
    QString detectTf;              // empty: the chart's timeframe
    int chartTfSeconds = 60;
    bool operator==(const Config& o) const;
  };
  struct Look {
    bool zones = true, labels = true;
    int keepAfterSweep = 20;       // bars a swept level stays (greyed); 0: gone at once
    double minScore = 1;
    QColor buy, sell;              // invalid: the theme's down / up
    bool operator==(const Look&) const = default;
  };

  explicit SmcLayer(ChartPane* pane);
  void setConfig(const Config& c);
  void setLook(const Look& l);
  // Recompute when the chart's bars changed (cheap check; the engine runs only on a real change).
  void setBars(const Bars& bars);

  void attached(const ::charts::plot::AttachedParams& p) override { series_ = p.series; requestUpdate_ = p.requestUpdate; }
  void detached() override { series_ = nullptr; requestUpdate_ = nullptr; }
  void requestRepaint() const { if (requestUpdate_) requestUpdate_(); }
  ::charts::plot::Series* series() const { return series_; }
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Bottom; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

  // A level ready to draw: its times on the chart (s, UTC).
  struct Shown {
    ::charts::smc::Level level;
    double score = 0;
    qint64 fromTime = 0, sweptTime = 0;   // sweptTime 0: still holding
    int sweptBar = -1;                    // index in the chart's bars
  };
  const std::vector<Shown>& levels() const { return shown_; }
  struct Stats { double computeMs = 0, drawMs = 0; int drawn = 0, total = 0, active = 0, fullRuns = 0, fed = 0; };
  const Stats& stats() const { return stats_; }

private:
  void rebuild();                  // the engine from scratch on the closed bars
  void feed(int upTo);             // closed chart bars [fed_, upTo) into the engine
  void publish();                  // levels to draw (+ sweeps by the pending bars)

  ChartPane* pane_;
  ::charts::plot::Series* series_ = nullptr;
  std::function<void()> requestUpdate_;
  Config cfg_;
  bool configured_ = false;
  Look look_;
  Bars bars_;
  QByteArray key_;
  std::unique_ptr<::charts::smc::LiquidityLevelEngine> engine_;
  std::optional<Resampler> resampler_;
  qint64 htfSec_ = 0;
  int fed_ = 0;                    // chart bars fed so far (closed ones)
  Bar lastFed_{};
  int pendingFrom_ = 0;            // chart bars from here are not in the engine yet (forming)
  qint64 firstTime_ = 0;
  int closedCount() const;
  std::vector<Shown> shown_;
  Stats stats_;
};

QString smcLabel(const ::charts::smc::Level& l);   // "EQH ×3", "PDL", "London H"…

}  // namespace charts::ui::flow
