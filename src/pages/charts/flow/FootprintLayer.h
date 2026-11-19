#pragma once
// Charts tab — footprint drawn over the main candles (chart type "footprint").
//
// Each bar with complete footprint data is drawn in its slot: a thin candle on the left (open/close, high/low from the
// trades), then one row per price level, by mode (settings: cfg "footprint", FootprintSettings):
//   Bid x Ask       sells hitting the bid | buys lifting the ask, shaded by volume, imbalances solid and bold
//   Delta           ask − bid per level, green/red by sign, stronger the larger; imbalances as edge marks
//   Volume profile  a bar per level as long as its volume (sells then buys); imbalances as edge marks
// In every mode: POC outlined, stacked imbalances as a strip right of the cells, delta and volume under the candle.
// Bars without footprint data (before the trade history, other markets) stay plain candles.
//
// Level of detail, decided once per frame for every bar on screen so the chart reads evenly:
//   numbers  rows ≥ 11 px and a cell column wide enough for "88.8K | 88.8K"
//   colours  cell columns ≥ 3 px: shading only, no text, finer rows
//   candles  narrower than that: nothing is drawn, the plain candles show
// Rows: "Auto" picks the smallest grouping of 1-2-5 × 10^k ticks giving rows ≥ 14 px for numbers (kept while 11-34 px)
// or ≥ 4 px for colours (kept while 3-10 px), so zooming does not make rows flicker.
#include "charts/core/Footprint.h"
#include "charts/core/FootprintSettings.h"
#include "pages/charts/plot/Primitive.h"
#include <QColor>

namespace charts::ui {
class ChartPane;
}

namespace charts::ui::flow {

class FootprintFeed;

class FootprintLayer : public ::charts::plot::Primitive {
public:
  FootprintLayer(ChartPane* pane, FootprintFeed* feed);
  void setFeed(FootprintFeed* feed) { feed_ = feed; }
  void setSettings(const ::charts::flow::FootprintSettings& s);   // cfg "footprint" (mode, rows, thresholds, colours)
  const ::charts::flow::FootprintSettings& settings() const { return settings_; }

  void attached(const ::charts::plot::AttachedParams& p) override { series_ = p.series; requestUpdate_ = p.requestUpdate; }
  void detached() override { series_ = nullptr; requestUpdate_ = nullptr; }
  void requestRepaint() const { if (requestUpdate_) requestUpdate_(); }   // redraw the chart (coalesced by Qt)
  ::charts::plot::Series* series() const { return series_; }
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Top; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

  enum class Detail { Candles, Colours, Numbers };
  Detail lastDetail() const { return detail_; }
  int64_t lastGroupTicks() const { return lastG_; }
  double lastDrawMs() const { return lastMs_; }
  int lastBarsDrawn() const { return barsDrawn_; }

private:
  static int64_t pickGroup(double pxPerTick, double minPx, double lo, double hi, int64_t& last);

  ChartPane* pane_;
  FootprintFeed* feed_;
  ::charts::plot::Series* series_ = nullptr;
  std::function<void()> requestUpdate_;
  ::charts::flow::FootprintSettings settings_;
  int64_t lastG_ = 0, lastGText_ = 0, lastGColour_ = 0;
  ::charts::flow::FootprintView view_;   // reused: analyzing allocates nothing once warm
  Detail detail_ = Detail::Candles;
  double lastMs_ = 0;
  int barsDrawn_ = 0;
};

// Short volume text for a cell: at most 5 characters ("0.012", "12.3", "1.2K", "<.001").
QString cellVolume(double v);
// Signed candle delta: "+14", "−0.31", "+1.2K".
QString signedVolume(double v);

}  // namespace charts::ui::flow
