#pragma once
// Vertical scale of one pane (JS chart.priceScale(id) / pane.priceScale(id)). Every pane has a "right" scale (drawn)
// and any number of overlay scales (any other id, e.g. "vol"; not drawn, own range and scaleMargins).
// Internally a scale works in "internal units": the price (Normal), log10(price) (Logarithmic), percent change from
// the series' first visible value (Percentage) or that value indexed to 100 (IndexedTo100).
#include "pages/charts/plot/Plot.h"

namespace charts::plot {

class PriceScale {
public:
  QString id() const { return id_; }
  const PriceScaleOptions& options() const { return o_; }
  void applyOptions(const PriceScaleOptions& o);
  void applyOptions(const std::function<void(PriceScaleOptions&)>& f) { PriceScaleOptions o = o_; f(o); applyOptions(o); }
  int width() const;                                      // axis width in px (0 for overlay scales)
  void setVisibleRange(const PriceRange& r);              // prices; turns autoScale off
  std::optional<PriceRange> getVisibleRange() const;
  void setAutoScale(bool on);
  Pane* pane() const { return pane_; }

  // ---- engine side ----
  bool isOverlay() const { return id_ != "right" && id_ != "left"; }
  bool hasRange() const { return hasRange_; }
  // Logarithmic acts as Normal while a value the scale shows is <= 0 (MACD, CCI… panes under a log price chart)
  bool isLog() const { return o_.mode == PriceScaleMode::Logarithmic && !logOff_; }
  PriceScaleMode mode() const { return logOff_ ? PriceScaleMode::Normal : o_.mode; }   // the mode in effect
  bool isRelative() const { return o_.mode == PriceScaleMode::Percentage || o_.mode == PriceScaleMode::IndexedTo100; }
  double toInternal(double price, double base) const;
  double fromInternal(double u, double base) const;
  double internalToY(double u) const;
  double yToInternal(double y) const;
  std::optional<double> priceToY(double price, double base) const;
  std::optional<double> yToPrice(double y, double base) const;
  double height() const { return height_; }
  struct Tick { double y; QString label; };
  const QVector<Tick>& ticks() const { return ticks_; }
  QString formatInternal(double u) const;                 // label text for an internal value
  Series* formatSource() const { return formatSource_; }

  // drag on the axis (scale) and in the pane (scroll when not autoscaled)
  void startScale(double y);
  void scaleTo(double y);
  void endScale() { scaleStart_ = kNaN; }
  void startScroll(double y);
  void scrollTo(double y);
  void endScroll() { scrollStart_ = kNaN; }

private:
  void clampLogRange();   // log10 range kept within [-15, 18] (see PriceScale.cpp)
  friend class Chart;
  friend class Pane;
  PriceScale(Chart* c, Pane* p, const QString& id, const PriceScaleOptions& o) : chart_(c), pane_(p), id_(id), o_(o) {}
  void buildTicks(int fontSize);

  Chart* chart_;
  Pane* pane_;
  QString id_;
  PriceScaleOptions o_;
  bool hasRange_ = false;
  bool logOff_ = false;               // Logarithmic set, but a visible value is <= 0 (set by Chart::computeScales)
  double min_ = 0, max_ = 1;          // internal units
  double height_ = 0;
  Series* formatSource_ = nullptr;
  QVector<Tick> ticks_;
  double scaleStart_ = kNaN, scrollStart_ = kNaN, startMin_ = 0, startMax_ = 1;
};

}  // namespace charts::plot
