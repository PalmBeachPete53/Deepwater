#pragma once
// Charts tab — liquidity heatmap under the candles (indicator "heatmap", step D4).
//
// Contrast: the median of the visible non-zero quantities is the bottom of the ramp (half the book stays dark, as
// thin liquidity should) and their 99.5th percentile the top; the ramp is bent (gamma 1.4) so mid sizes stay dim and
// the walls stand out.
// The pane is drawn into one image at 1 px per screen point (QImage written column by column, no painter per cell):
// each pixel column is a time slice of the chart (from the bars' own times), each pixel row a price range (from the
// price scale), coloured by the largest quantity resting there in that slice (charts/core/HeatmapRender). Data: the
// stream's 100 ms columns where they exist (the last 10 minutes), its 1 s columns before (6 hours, with what earlier
// sessions recorded). The image is rebuilt when the view changes (scroll, zoom, price scale, size); while it doesn't,
// only the newest pixel columns are redrawn as data comes. The contrast is taken at each rebuild and kept between
// them, so colours don't pulse with every new column.
// Over it: the best bid and best ask as two thin lines, drawn into the image with their column. Hatched slices: the book was not synced (no data, not empty).
// In bar replay only the columns before the replayed bar's end are shown.
#include "charts/core/HeatmapRender.h"
#include "pages/charts/plot/Primitive.h"
#include <QImage>
#include <QObject>
#include <deque>
#include <memory>
#include <vector>

namespace charts::data {
class DepthStream;
}
namespace charts::ui {
class ChartPane;
}

namespace charts::ui::flow {

class HeatmapLayer : public ::charts::plot::Primitive {
public:
  HeatmapLayer(ChartPane* pane, std::shared_ptr<data::DepthStream> stream);
  ~HeatmapLayer() override;
  data::DepthStream* stream() const { return stream_.get(); }
  QObject* context() { return &ctx_; }   // connections that must die with this layer (not with the shared stream)

  void attached(const ::charts::plot::AttachedParams& p) override { series_ = p.series; requestUpdate_ = p.requestUpdate; }
  void detached() override { series_ = nullptr; requestUpdate_ = nullptr; }
  void requestRepaint() const { if (requestUpdate_) requestUpdate_(); }
  ::charts::plot::Series* series() const { return series_; }
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Bottom; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

  // The indicator's settings (D5). Contrast: automatic percentiles of what is drawn, unless a quantity is given for the
  // bottom (hidden below) or the top (brightest from).
  struct Look {
    int palette = 0;            // 0 Deep, 1 Thermal, 2 Mono
    bool linear = false;        // colour ramp on quantities instead of their logarithm
    double gamma = 1.4;
    double lowPct = 50, highPct = 99.5;
    double minQty = 0, maxQty = 0;   // 0: automatic
    double opacity = 1;
    double step = 0;            // price step of a cell (0: the stream's rows / the pixels)
    bool sum = false;           // the price rows of a cell add up (else the largest)
    bool lines = true;          // best bid / ask
    bool operator==(const Look&) const = default;
  };
  void setLook(const Look& l);
  const Look& look() const { return look_; }

  struct Stats {
    double lastMs = 0, rasterMs = 0, imageMs = 0;
    bool full = false;
    int columnsRedrawn = 0, records = 0;
    uint16_t low = 0, high = 0;
  };
  const Stats& stats() const { return stats_; }

private:
  ChartPane* pane_;
  std::shared_ptr<data::DepthStream> stream_;
  ::charts::plot::Series* series_ = nullptr;
  std::function<void()> requestUpdate_;
  QObject ctx_;
  QImage img_;
  std::unique_ptr<::charts::flow::HeatmapLut> lut_;
  std::vector<int64_t> xTime_, rowTop_, rowBottom_;
  std::vector<double> bidY_, askY_;   // best bid / ask line per pixel column (NaN: none), kept like the image
  std::vector<const ::charts::flow::DepthRecord*> recs_;
  // a slice's records merged by band (same base and row size): the largest code per row, so a pixel reads one or two
  // records whatever the zoom; grown once, reused (no allocation per frame). A deque, not a vector: recs_ holds
  // pointers into it, and a slice can need a new merged record after pushing a pointer to an earlier one — a vector's
  // growth would leave that pointer dangling (use-after-free found by ASan); a deque never moves existing elements.
  std::deque<::charts::flow::DepthRecord> merged_;
  std::vector<uint32_t> hist_;
  std::vector<uint16_t> codes_;   // quantity code of every pixel (W × H), kept with the image
  Look look_;
  int lookVersion_ = 0;
  QByteArray viewKey_;
  int64_t drawnUpTo_ = 0;   // newest data time already in the image
  Stats stats_;
};

}  // namespace charts::ui::flow
