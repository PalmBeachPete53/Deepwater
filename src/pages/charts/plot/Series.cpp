#include "pages/charts/plot/Series.h"
#include "pages/charts/plot/Chart.h"
#include <algorithm>

namespace charts::plot {

// ---------- PriceLine / SeriesMarkers ----------

void PriceLine::applyOptions(const PriceLineOptions& o) {
  o_ = o;
  series_->chart_->invalidate();
}

void SeriesMarkers::setMarkers(const QVector<SeriesMarker>& m) { series_->setMarkers(m); }
const QVector<SeriesMarker>& SeriesMarkers::markers() const { return series_->markers(); }
void SeriesMarkers::detach() { series_->setMarkers({}); }

SeriesMarkers* createSeriesMarkers(Series* series, const QVector<SeriesMarker>& markers) {
  SeriesMarkers* m = series->markersPlugin();
  m->setMarkers(markers);
  return m;
}

// ---------- Series ----------

Series::Series(Chart* chart, SeriesType type, const SeriesOptions& o, Pane* pane)
    : chart_(chart), type_(type), o_(o), pane_(pane) {}

Series::~Series() {
  for (Primitive* p : primitives_) p->detached();
}

Series::Row Series::toRow(const DataPoint& p) const {
  Row r{};
  r.time = p.time;
  r.idx = -1;
  if (isBarType()) {
    if (std::isfinite(p.close)) {
      r.c = p.close;
      r.o = std::isfinite(p.open) ? p.open : p.close;
      r.h = std::isfinite(p.high) ? p.high : qMax(r.o, r.c);
      r.l = std::isfinite(p.low) ? p.low : qMin(r.o, r.c);
      r.flags |= HasValue;
    }
  } else if (std::isfinite(p.value)) {
    r.o = r.h = r.l = r.c = p.value;
    r.flags |= HasValue;
  }
  if (p.color.isValid()) { r.color = p.color.rgba(); r.flags |= HasColor; }
  if (p.borderColor.isValid()) { r.border = p.borderColor.rgba(); r.flags |= HasBorder; }
  if (p.wickColor.isValid()) { r.wick = p.wickColor.rgba(); r.flags |= HasWick; }
  return r;
}

void Series::setData(const QVector<DataPoint>& points) {
  std::vector<Row> rows;
  rows.reserve(points.size());
  bool sorted = true;
  for (const DataPoint& p : points) {
    if (!rows.empty() && p.time <= rows.back().time) sorted = false;
    rows.push_back(toRow(p));
  }
  if (!sorted) {
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.time < b.time; });
    std::vector<Row> uniq;
    uniq.reserve(rows.size());
    for (const Row& r : rows) {
      if (!uniq.empty() && uniq.back().time == r.time) uniq.back() = r;
      else uniq.push_back(r);
    }
    rows.swap(uniq);
  }
  // same times as before? then the time scale does not need rebuilding
  bool sameTimes = rows.size() == rows_.size();
  for (size_t i = 0; sameTimes && i < rows.size(); ++i) sameTimes = rows[i].time == rows_[i].time;
  if (sameTimes)
    for (size_t i = 0; i < rows.size(); ++i) rows[i].idx = rows_[i].idx;
  rows_.swap(rows);
  changed(!sameTimes);
}

void Series::update(const DataPoint& p, bool historicalUpdate) {
  Row r = toRow(p);
  if (!rows_.empty() && r.time == rows_.back().time) {
    r.idx = rows_.back().idx;
    rows_.back() = r;
    return changed(false);
  }
  if (rows_.empty() || r.time > rows_.back().time) {
    rows_.push_back(r);
    return changed(true);
  }
  if (!historicalUpdate) return;
  auto it = std::lower_bound(rows_.begin(), rows_.end(), r.time, [](const Row& a, Time t) { return a.time < t; });
  if (it != rows_.end() && it->time == r.time) {
    r.idx = it->idx;
    *it = r;
    return changed(false);
  }
  rows_.insert(it, r);
  changed(true);
}

void Series::changed(bool timesChanged) { chart_->dataChanged(timesChanged); }

QVector<DataPoint> Series::data() const {
  QVector<DataPoint> out;
  out.reserve(int(rows_.size()));
  for (const Row& r : rows_) {
    DataPoint p;
    p.time = r.time;
    if (r.flags & HasValue) {
      if (isBarType()) { p.open = r.o; p.high = r.h; p.low = r.l; p.close = r.c; }
      else p.value = r.c;
    }
    if (r.flags & HasColor) p.color = QColor::fromRgba(r.color);
    if (r.flags & HasBorder) p.borderColor = QColor::fromRgba(r.border);
    if (r.flags & HasWick) p.wickColor = QColor::fromRgba(r.wick);
    out.append(p);
  }
  return out;
}

int Series::rowAtOrAfter(int idx) const {
  auto it = std::lower_bound(rows_.begin(), rows_.end(), idx, [](const Row& a, int i) { return a.idx < i; });
  return int(it - rows_.begin());
}

const Series::Row* Series::rowAtIndex(int idx) const {
  const int k = rowAtOrAfter(idx);
  return (k < int(rows_.size()) && rows_[k].idx == idx) ? &rows_[k] : nullptr;
}

const Series::Row* Series::lastValueRow() const {
  for (auto it = rows_.rbegin(); it != rows_.rend(); ++it)
    if (it->flags & HasValue) return &*it;
  return nullptr;
}

std::optional<DataPoint> Series::dataByIndex(int idx, MismatchDirection dir) const {
  chart_->ensureLayout();
  int k = rowAtOrAfter(idx);
  if (k < int(rows_.size()) && rows_[k].idx == idx) {
  } else if (dir == MismatchDirection::NearestLeft) {
    if (--k < 0) return std::nullopt;
  } else if (dir == MismatchDirection::NearestRight) {
    if (k >= int(rows_.size())) return std::nullopt;
  } else {
    return std::nullopt;
  }
  const Row& r = rows_[k];
  DataPoint p;
  p.time = r.time;
  if (r.flags & HasValue) {
    if (isBarType()) { p.open = r.o; p.high = r.h; p.low = r.l; p.close = r.c; }
    else p.value = r.c;
  }
  if (r.flags & HasColor) p.color = QColor::fromRgba(r.color);
  return p;
}

std::optional<BarsInfo> Series::barsInLogicalRange(const LogicalRange& range) const {
  chart_->ensureLayout();
  if (rows_.empty()) return std::nullopt;
  BarsInfo b;
  const int a = rowAtOrAfter(int(std::ceil(range.from)));
  const int z = rowAtOrAfter(int(std::floor(range.to)) + 1) - 1;
  b.barsBefore = a;
  b.barsAfter = int(rows_.size()) - 1 - z;
  if (a <= z) { b.from = rows_[a].time; b.to = rows_[z].time; }
  return b;
}

void Series::applyOptions(const SeriesOptions& o) {
  o_ = o;
  chart_->invalidate();
}

std::optional<double> Series::priceToCoordinate(double price) const {
  chart_->ensureLayout();
  const PriceScale* sc = priceScale();
  return sc ? sc->priceToY(price, firstValue_) : std::nullopt;
}

std::optional<double> Series::coordinateToPrice(double y) const {
  chart_->ensureLayout();
  const PriceScale* sc = priceScale();
  return sc ? sc->yToPrice(y, firstValue_) : std::nullopt;
}

QString Series::formatPrice(double price) const {
  chart_->ensureLayout();
  return chart_->formatAxisPrice(priceScale(), this, price);
}

PriceLine* Series::createPriceLine(const PriceLineOptions& o) {
  priceLines_.push_back(std::unique_ptr<PriceLine>(new PriceLine(this, o)));
  chart_->invalidate();
  return priceLines_.back().get();
}

void Series::removePriceLine(PriceLine* line) {
  auto it = std::find_if(priceLines_.begin(), priceLines_.end(), [&](const auto& p) { return p.get() == line; });
  if (it == priceLines_.end()) return;
  priceLines_.erase(it);
  chart_->invalidate();
}

QVector<PriceLine*> Series::priceLines() const {
  QVector<PriceLine*> out;
  for (const auto& p : priceLines_) out.append(p.get());
  return out;
}

void Series::setMarkers(const QVector<SeriesMarker>& markers) {
  markers_ = markers;
  std::stable_sort(markers_.begin(), markers_.end(), [](const SeriesMarker& a, const SeriesMarker& b) { return a.time < b.time; });
  chart_->invalidate();
}

SeriesMarkers* Series::markersPlugin() {
  if (!markersApi_) markersApi_.reset(new SeriesMarkers(this));
  return markersApi_.get();
}

void Series::attachPrimitive(Primitive* p) {
  if (!p || std::find(primitives_.begin(), primitives_.end(), p) != primitives_.end()) return;
  primitives_.push_back(p);
  Chart* c = chart_;
  p->attached({c, this, [c] { c->repaintBase(); }});
  chart_->invalidate();
}

void Series::detachPrimitive(Primitive* p) {
  auto it = std::find(primitives_.begin(), primitives_.end(), p);
  if (it == primitives_.end()) return;
  primitives_.erase(it);
  p->detached();
  chart_->invalidate();
}

PriceScale* Series::priceScale() const { return pane_ ? pane_->priceScale(o_.priceScaleId) : nullptr; }

int Series::paneIndex() const { return pane_ ? pane_->paneIndex() : -1; }

void Series::moveToPane(int index) {
  Pane* to = chart_->ensurePane(index);
  if (to == pane_) return;
  auto& v = pane_->series_;
  v.erase(std::remove(v.begin(), v.end(), this), v.end());
  pane_ = to;
  to->series_.push_back(this);
  chart_->removeEmptyPanes();
  chart_->invalidate();
}

QColor Series::barColor(const Row& r) const {
  if (r.flags & HasColor) return QColor::fromRgba(r.color);
  switch (type_) {
    case SeriesType::Candlestick:
    case SeriesType::Bar: return r.c >= r.o ? o_.upColor : o_.downColor;
    case SeriesType::Line: return o_.color;
    case SeriesType::Area: return o_.lineColor;
    case SeriesType::Baseline: return r.c >= o_.baseValue ? o_.topLineColor : o_.bottomLineColor;
    case SeriesType::Histogram: return o_.color;
  }
  return o_.color;
}

}  // namespace charts::plot
