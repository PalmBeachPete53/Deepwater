#pragma once
// One series (JS ISeriesApi): created by Chart::addSeries, owned by the chart, deleted by Chart::removeSeries.
#include "pages/charts/plot/Plot.h"
#include "pages/charts/plot/Primitive.h"
#include <memory>
#include <vector>

namespace charts::plot {

class PriceLine {
public:
  const PriceLineOptions& options() const { return o_; }
  void applyOptions(const PriceLineOptions& o);
  void applyOptions(const std::function<void(PriceLineOptions&)>& f) { PriceLineOptions o = o_; f(o); applyOptions(o); }
private:
  friend class Series;
  friend class Chart;
  PriceLine(Series* s, const PriceLineOptions& o) : series_(s), o_(o) {}
  Series* series_;
  PriceLineOptions o_;
};

// JS createSeriesMarkers(series, markers) → plugin with setMarkers / markers / detach. One per series.
class SeriesMarkers {
public:
  void setMarkers(const QVector<SeriesMarker>& markers);
  const QVector<SeriesMarker>& markers() const;
  void detach();                      // removes the markers
  Series* getSeries() const { return series_; }
private:
  friend class Series;
  explicit SeriesMarkers(Series* s) : series_(s) {}
  Series* series_;
};
SeriesMarkers* createSeriesMarkers(Series* series, const QVector<SeriesMarker>& markers = {});

class Series {
public:
  ~Series();
  SeriesType seriesType() const { return type_; }

  // ---- data ----
  // Points must be in ascending time order with unique times (as the JS library requires; violations are sorted
  // and de-duplicated, last point wins).
  void setData(const QVector<DataPoint>& points);
  // Live bar: same time as the last point → replaces it; newer → appended; older → ignored unless historicalUpdate.
  void update(const DataPoint& point, bool historicalUpdate = false);
  QVector<DataPoint> data() const;
  int dataSize() const { return int(rows_.size()); }
  std::optional<DataPoint> dataByIndex(int logicalIndex, MismatchDirection dir = MismatchDirection::None) const;
  std::optional<BarsInfo> barsInLogicalRange(const LogicalRange& range) const;

  // ---- options ----
  const SeriesOptions& options() const { return o_; }
  void applyOptions(const SeriesOptions& o);
  void applyOptions(const std::function<void(SeriesOptions&)>& f) { SeriesOptions o = o_; f(o); applyOptions(o); }

  // ---- coordinates (pane coordinates; null when the scale has no range yet) ----
  std::optional<double> priceToCoordinate(double price) const;
  std::optional<double> coordinateToPrice(double y) const;
  QString formatPrice(double price) const;   // as the axis label shows it

  // ---- price lines, markers, primitives ----
  PriceLine* createPriceLine(const PriceLineOptions& o);
  void removePriceLine(PriceLine* line);
  QVector<PriceLine*> priceLines() const;
  void setMarkers(const QVector<SeriesMarker>& markers);   // same as createSeriesMarkers(this)->setMarkers(...)
  const QVector<SeriesMarker>& markers() const { return markers_; }
  SeriesMarkers* markersPlugin();
  void attachPrimitive(Primitive* p);         // not owned
  void detachPrimitive(Primitive* p);

  // ---- placement ----
  PriceScale* priceScale() const;
  Pane* getPane() const { return pane_; }
  int paneIndex() const;
  void moveToPane(int paneIndex);
  Chart* chart() const { return chart_; }

  // ---- engine side ----
  struct Row {
    Time time;
    double o, h, l, c;                // value series keep the value in all four
    QRgb color, border, wick;
    quint8 flags;                     // HasValue | HasColor | HasBorder | HasWick
    int idx;                          // index on the time scale
  };
  enum : quint8 { HasValue = 1, HasColor = 2, HasBorder = 4, HasWick = 8 };
  const std::vector<Row>& rows() const { return rows_; }
  // first row whose time-scale index is >= idx (rows_.size() if none)
  int rowAtOrAfter(int idx) const;
  const Row* rowAtIndex(int idx) const;      // exact index match or null
  const Row* lastValueRow() const;           // last row with a value
  bool isBarType() const { return type_ == SeriesType::Candlestick || type_ == SeriesType::Bar; }
  double firstValue() const { return firstValue_; }   // base of Percentage / IndexedTo100 (set each layout)
  QColor barColor(const Row& r) const;       // main colour of a row (candle body, line point, histogram column)
  const std::vector<Primitive*>& primitives() const { return primitives_; }
  const std::vector<std::unique_ptr<PriceLine>>& priceLineObjects() const { return priceLines_; }

private:
  friend class Chart;
  friend class PriceLine;
  Series(Chart* chart, SeriesType type, const SeriesOptions& o, Pane* pane);
  Row toRow(const DataPoint& p) const;
  void changed(bool timesChanged);

  Chart* chart_;
  SeriesType type_;
  SeriesOptions o_;
  Pane* pane_;
  std::vector<Row> rows_;
  std::vector<std::unique_ptr<PriceLine>> priceLines_;
  std::vector<Primitive*> primitives_;
  QVector<SeriesMarker> markers_;
  std::unique_ptr<SeriesMarkers> markersApi_;
  double firstValue_ = kNaN;
};

}  // namespace charts::plot
