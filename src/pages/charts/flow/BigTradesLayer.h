#pragma once
// Charts tab — Big Trades bubbles over the price (one layer per Big Trades indicator).
// A marker sits at the order's average price, at its moment inside its bar (x moves across the bar with the time),
// its colour the aggressor's side, its area proportional to the volume (minD at the threshold, up to maxD), more
// opaque as it grows. Shapes: circle, square, diamond, or the volume written in the side's colour. Only the markers
// of the bars on screen are drawn, the largest first so small ones stay visible; each is drawn once into a small
// image (sprite) and copied, since antialiased shapes with alpha cost ~60 µs each.
// bubbleAt() finds the marker under the mouse for the tooltip (ChartPane shows it).
#include "charts/core/BigTrades.h"
#include "pages/charts/plot/Primitive.h"
#include <QColor>
#include <QHash>
#include <QImage>
#include <vector>

namespace charts::ui {
class ChartPane;
}

namespace charts::ui::flow {

class BigTradesModel;

class BigTradesLayer : public ::charts::plot::Primitive {
public:
  enum class Shape { Circle, Square, Diamond, Text };
  struct Look {
    double minD = 8, maxD = 40, minOpacity = 0.35;
    Shape shape = Shape::Circle;
    bool showVolume = false;      // the volume written inside markers large enough for it
    QColor buy, sell;             // invalid = the theme's up / down
  };
  BigTradesLayer(ChartPane* pane, BigTradesModel* model);
  void setLook(const Look& l) { look_ = l; }

  void attached(const ::charts::plot::AttachedParams& p) override { series_ = p.series; requestUpdate_ = p.requestUpdate; }
  void detached() override { series_ = nullptr; requestUpdate_ = nullptr; }
  void requestRepaint() const { if (requestUpdate_) requestUpdate_(); }
  ::charts::plot::Series* series() const { return series_; }
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Top; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

  // The order drawn under a point of the pane (the one on top), as of the last frame; -1 if none.
  long bubbleAt(const QPointF& pt) const;
  std::optional<QPointF> largestCenter() const { return hits_.empty() ? std::nullopt : std::optional<QPointF>(hits_.front().c); }

  double lastDrawMs() const { return lastMs_; }
  int lastDrawn() const { return drawn_; }

private:
  const QImage& sprite(Shape shape, const QColor& col, double d, double opacity, double dpr);
  const QImage& textSprite(const QString& text, const QColor& col, int px, double dpr);

  ChartPane* pane_;
  BigTradesModel* model_;
  ::charts::plot::Series* series_ = nullptr;
  std::function<void()> requestUpdate_;
  Look look_;
  QHash<quint64, QImage> sprites_;
  QHash<QString, QImage> textSprites_;
  std::vector<size_t> order_;   // reused: visible bubbles, largest first
  struct Hit { QPointF c; double r; size_t idx; };
  std::vector<Hit> hits_;       // drawn last frame, in drawing order
  double lastMs_ = 0;
  int drawn_ = 0;
};

}  // namespace charts::ui::flow
