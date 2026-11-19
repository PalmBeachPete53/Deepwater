#pragma once
// Series / pane primitives: custom drawing inside a pane (Lightweight Charts' ISeriesPrimitive with one pane view).
// The JS pattern
//     attached({ series, requestUpdate }) { this.series = series; this.requestUpdate = requestUpdate; }
//     paneViews() { return [{ zOrder: () => "top", renderer: () => ({ draw: t => t.useMediaCoordinateSpace(
//         ({ context, mediaSize }) => this.draw(context, mediaSize)) }) }]; }
// becomes
//     void attached(const AttachedParams& p) override { series = p.series; requestUpdate = p.requestUpdate; }
//     PrimitiveZOrder zOrder() const override { return PrimitiveZOrder::Top; }
//     void draw(QPainter& ctx, const QSize& size, const PrimitiveContext&) override { ... }
// draw() paints in media (CSS-pixel) coordinates of the pane's plot area: (0,0) = its top-left corner, size = its
// size; the painter is clipped to it and has antialiasing on. Coordinates come from series->priceToCoordinate() and
// chart->timeScale()->logicalToCoordinate(), exactly as in the JS.
#include "pages/charts/plot/Plot.h"
#include <QPainter>
#include <QSize>

namespace charts::plot {

struct AttachedParams {
  Chart* chart = nullptr;
  Series* series = nullptr;           // null for a pane primitive
  std::function<void()> requestUpdate;   // repaint the chart (cheap; coalesced)
};

struct PrimitiveContext {
  Chart* chart = nullptr;
  Series* series = nullptr;
  int paneIndex = 0;
  double pixelRatio = 1;              // device pixels per media pixel
  TimeScale* timeScale() const;
  std::optional<double> priceToY(double price) const;   // series->priceToCoordinate (pane right scale if no series)
  std::optional<double> logicalToX(double logical) const;
};

struct PrimitiveHit {
  QString externalId;
  Qt::CursorShape cursor = Qt::ArrowCursor;
  PrimitiveZOrder zOrder = PrimitiveZOrder::Normal;
};

class Primitive {
public:
  virtual ~Primitive() = default;
  virtual void attached(const AttachedParams&) {}
  virtual void detached() {}
  virtual void updateAllViews() {}
  virtual PrimitiveZOrder zOrder() const { return PrimitiveZOrder::Normal; }
  virtual void draw(QPainter& ctx, const QSize& mediaSize, const PrimitiveContext& c) = 0;
  // Optional: what is under (x, y) in pane coordinates (sets the cursor and MouseEventParams::hoveredObjectId).
  virtual std::optional<PrimitiveHit> hitTest(double /*x*/, double /*y*/) { return std::nullopt; }
};

}  // namespace charts::plot
