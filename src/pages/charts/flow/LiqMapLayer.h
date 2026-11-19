#pragma once
// Charts tab — the liquidation heatmap under the candles (indicator "liqmap", step E3). An ESTIMATE (see
// charts/core/Liquidation.h for every assumption): where leveraged positions opened on this chart's bars would be
// liquidated, brighter where more would be.
//
// One image at 1 px per screen point, like the order book heatmap: each pixel column shows the model's column of the bar
// under it (after the last closed bar: the levels resting now, carried to the right edge, so the zones ahead of the
// price stay visible), each pixel row the largest quantity among the model's price rows under it. The image is redrawn
// only when the view or the result changes. The contrast (percentiles of what is drawn) and the colours use the order
// book heatmap's table (charts/core/HeatmapRender).
// Over it, optionally: the real liquidation orders Binance published (a sample: at most one a second per symbol) as
// dots — red for longs liquidated, green for shorts — sized by quantity.
#include "charts/core/HeatmapRender.h"
#include "pages/charts/plot/Primitive.h"
#include <QImage>
#include <functional>
#include <memory>
#include <vector>

namespace charts::ui {
class ChartPane;
}

namespace charts::ui::flow {

class LiqMapModel;

class LiqMapLayer : public ::charts::plot::Primitive {
public:
  struct Look {
    int palette = 1;            // 0 Deep, 1 Thermal (default here, to tell it from the order book), 2 Mono
    double lowPct = 60, highPct = 99.5, gamma = 1.2, opacity = 0.85;
    bool longs = true, shorts = true;
    bool real = true;           // dots of the real liquidations
    bool operator==(const Look&) const = default;
  };

  LiqMapLayer(ChartPane* pane, LiqMapModel* model);
  void setLook(const Look& l);
  const Look& look() const { return look_; }

  void attached(const ::charts::plot::AttachedParams& p) override { series_ = p.series; requestUpdate_ = p.requestUpdate; }
  void detached() override { series_ = nullptr; requestUpdate_ = nullptr; }
  void requestRepaint() const { if (requestUpdate_) requestUpdate_(); }
  ::charts::plot::Series* series() const { return series_; }
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Bottom; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

  struct Stats { double lastMs = 0; bool full = false; int columns = 0, dots = 0; uint16_t low = 0, high = 0; };
  const Stats& stats() const { return stats_; }

private:
  ChartPane* pane_;
  LiqMapModel* model_;
  ::charts::plot::Series* series_ = nullptr;
  std::function<void()> requestUpdate_;
  Look look_;
  int lookVersion_ = 0;
  QImage img_;
  std::unique_ptr<::charts::flow::HeatmapLut> lut_;
  std::vector<int64_t> xTime_;
  std::vector<int> r0_, r1_;
  std::vector<uint16_t> codes_;
  std::vector<uint32_t> hist_;
  QByteArray viewKey_;
  Stats stats_;
};

}  // namespace charts::ui::flow
