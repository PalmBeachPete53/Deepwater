#pragma once
// charts::plot — Deepwater's own QPainter chart engine for the Charts tab.
//
// Its public API mirrors the part of TradingView Lightweight Charts v5 that quant-terminal uses (createChart,
// addSeries, timeScale(), priceScale(id), panes(), primitives, markers, price lines, crosshair callbacks), so
// quant-terminal/public/js/chart.js, drawings.js and volume-profile.js translate almost line by line. No code of
// that library is used: this is an independent implementation that behaves the same way (same axis layout,
// crosshair, zoom/scroll feel, label formats). Options are plain structs with Lightweight Charts' defaults;
// `applyOptions([](auto& o) { ... })` plays the role of the JS partial-options objects.
//
// Time values are what the JS passes to Lightweight Charts: Unix seconds, already shifted to local time for intraday
// charts by the caller (chart.js toChart()). The engine formats them as UTC, as Lightweight Charts does.
#include <QColor>
#include <QFont>
#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>

namespace charts::plot {

class Chart;
class Series;
class Pane;
class PriceScale;
class TimeScale;

using Time = qint64;
inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---- enums (same numeric values as Lightweight Charts) ----
enum class SeriesType { Candlestick, Bar, Line, Area, Baseline, Histogram };
// JS-like names: chart->addSeries(plot::LineSeries, ...)
inline constexpr SeriesType CandlestickSeries = SeriesType::Candlestick;
inline constexpr SeriesType BarSeries = SeriesType::Bar;
inline constexpr SeriesType LineSeries = SeriesType::Line;
inline constexpr SeriesType AreaSeries = SeriesType::Area;
inline constexpr SeriesType BaselineSeries = SeriesType::Baseline;
inline constexpr SeriesType HistogramSeries = SeriesType::Histogram;

enum class LineStyle : int { Solid = 0, Dotted = 1, Dashed = 2, LargeDashed = 3, SparseDotted = 4 };
enum class LineType : int { Simple = 0, WithSteps = 1, Curved = 2 };   // Curved is drawn as Simple
enum class PriceScaleMode : int { Normal = 0, Logarithmic = 1, Percentage = 2, IndexedTo100 = 3 };
enum class CrosshairMode : int { Normal = 0, Magnet = 1, Hidden = 2, MagnetOHLC = 3 };
enum class TickMarkType : int { Year = 0, Month = 1, DayOfMonth = 2, Time = 3, TimeWithSeconds = 4 };
enum class MarkerShape { Circle, Square, ArrowUp, ArrowDown };
enum class MarkerPosition { AboveBar, BelowBar, InBar };
enum class PrimitiveZOrder { Bottom, Normal, Top };
enum class MismatchDirection { None = 0, NearestLeft = -1, NearestRight = 1 };

// CSS colour strings as the JS uses them: "#rgb", "#rrggbb", "#rrggbbaa", "rgb(r,g,b)", "rgba(r,g,b,a)",
// "transparent", named colours. Invalid input → invalid QColor ("not set").
QColor cssColor(const QString& css);
// "aboveBar" | "belowBar" | "inBar" and "arrowUp" | "arrowDown" | "circle" | "square" (JS marker strings).
MarkerPosition markerPosition(const QString& s);
MarkerShape markerShape(const QString& s);

// ---- data ----
// One point of any series type (the JS objects passed to setData/update):
//   candlestick / bar: {time, open, high, low, close, color?, borderColor?, wickColor?}
//   line / area / baseline / histogram: {time, value?, color?}
// A point without value (value NaN; close NaN for OHLC types) is a whitespace point: it takes a slot on the time
// scale but draws nothing. color invalid = the series colour; a transparent color on a line point hides the segment
// that starts at that point (chart.js lineData relies on this).
struct DataPoint {
  Time time = 0;
  double value = kNaN;
  double open = kNaN, high = kNaN, low = kNaN, close = kNaN;
  QColor color, borderColor, wickColor;

  static DataPoint whitespace(Time t) { DataPoint p; p.time = t; return p; }
  static DataPoint val(Time t, double v, const QColor& c = {}) { DataPoint p; p.time = t; p.value = v; p.color = c; return p; }
  static DataPoint ohlc(Time t, double o, double h, double l, double c) {
    DataPoint p; p.time = t; p.open = o; p.high = h; p.low = l; p.close = c; return p;
  }
  bool isWhitespace(SeriesType type) const;
};

struct LogicalRange { double from = 0, to = 0; };
struct TimeRange { Time from = 0, to = 0; };
struct PriceRange { double minValue = 0, maxValue = 0; };
struct BarsInfo { std::optional<Time> from, to; int barsBefore = 0, barsAfter = 0; };

// priceFormat: { type: "price", precision, minMove } | { type: "volume" } | { type: "percent" } | custom formatter.
struct PriceFormat {
  enum Type { Price, Volume, Percent, Custom } type = Price;
  int precision = 2;
  double minMove = 0.01;
  std::function<QString(double)> formatter;   // type Custom
  static PriceFormat price(int precision, double minMove) { PriceFormat f; f.precision = precision; f.minMove = minMove; return f; }
  static PriceFormat volume() { PriceFormat f; f.type = Volume; return f; }
};

// All series options in one struct; seriesDefaults(type) returns Lightweight Charts' defaults for that type.
struct SeriesOptions {
  // common
  QString title;
  bool visible = true;
  bool lastValueVisible = true;
  // A second line under the last-value label on the price axis, in its colours (TradingView's countdown to the bar's
  // close); an empty text draws nothing. Drawn over the cached chart image, so it can change every second through
  // Chart::refreshCountdown() without the chart being painted again.
  std::function<QString()> lastValueSubtitle;
  bool priceLineVisible = true;
  int priceLineWidth = 1;
  QColor priceLineColor;                        // invalid = colour of the last bar
  LineStyle priceLineStyle = LineStyle::Dashed;
  bool baseLineVisible = true;                  // Percentage / IndexedTo100 base line
  QColor baseLineColor = QColor(0xB2, 0xB5, 0xBE);
  int baseLineWidth = 1;
  LineStyle baseLineStyle = LineStyle::Solid;
  PriceFormat priceFormat;
  QString priceScaleId = "right";               // "right", "left" or any overlay id ("vol")
  bool noAutoscale = false;                     // JS autoscaleInfoProvider: () => null
  // candlestick (+ bar uses upColor/downColor)
  QColor upColor = QColor("#26a69a"), downColor = QColor("#ef5350");
  bool wickVisible = true, borderVisible = true;
  QColor borderColor = QColor("#378658"), borderUpColor = QColor("#26a69a"), borderDownColor = QColor("#ef5350");
  QColor wickColor = QColor("#737375"), wickUpColor = QColor("#26a69a"), wickDownColor = QColor("#ef5350");
  // bar
  bool openVisible = true, thinBars = true;
  // line / area / baseline; histogram uses color
  QColor color = QColor("#2196f3");
  int lineWidth = 3;
  LineStyle lineStyle = LineStyle::Solid;
  LineType lineType = LineType::Simple;
  bool lineVisible = true;
  bool pointMarkersVisible = false;
  double pointMarkersRadius = 0;                // 0 = lineWidth / 2 + 2
  bool crosshairMarkerVisible = true;
  double crosshairMarkerRadius = 4;
  QColor crosshairMarkerBorderColor;            // invalid = chart background
  double crosshairMarkerBorderWidth = 2;
  QColor crosshairMarkerBackgroundColor;        // invalid = series colour
  // area
  QColor lineColor = QColor("#33D778");
  QColor topColor = QColor(46, 220, 135, 102), bottomColor = QColor(40, 221, 100, 0);
  bool invertFilledArea = false;
  // baseline
  double baseValue = 0;                         // baseValue: { type: "price", price }
  QColor topLineColor = QColor(38, 166, 154), bottomLineColor = QColor(239, 83, 80);
  QColor topFillColor1 = QColor(38, 166, 154, 71), topFillColor2 = QColor(38, 166, 154, 13);
  QColor bottomFillColor1 = QColor(239, 83, 80, 13), bottomFillColor2 = QColor(239, 83, 80, 71);
  // histogram
  double base = 0;
};
SeriesOptions seriesDefaults(SeriesType type);

struct PriceLineOptions {
  QString id;
  double price = 0;
  QColor color;                       // invalid = series colour
  int lineWidth = 1;
  LineStyle lineStyle = LineStyle::Dashed;
  bool lineVisible = true;
  bool axisLabelVisible = true;
  QString title;
  QColor axisLabelColor, axisLabelTextColor;
};

struct SeriesMarker {
  Time time = 0;
  MarkerPosition position = MarkerPosition::AboveBar;
  MarkerShape shape = MarkerShape::Circle;
  QColor color = QColor("#2196f3");
  QString text;
  double size = 1;
  QString id;
};

struct ScaleMargins { double top = 0.2, bottom = 0.1; };
struct PriceScaleOptions {
  bool autoScale = true;
  PriceScaleMode mode = PriceScaleMode::Normal;
  bool invertScale = false;
  bool alignLabels = true;
  bool borderVisible = true;
  QColor borderColor = QColor("#2B2B43");
  QColor textColor;                   // invalid = layout.textColor
  bool entireTextOnly = false;
  bool visible = true;                // right scale; overlay scales are never drawn
  bool ticksVisible = false;
  ScaleMargins scaleMargins;
  int minimumWidth = 0;
  double tickMarkDensity = 2.5;
};

struct TimeScaleOptions {
  double rightOffset = 0;
  double barSpacing = 6;
  double minBarSpacing = 0.5;
  double maxBarSpacing = 0;           // 0 = half the width
  double minVisibleBars = 0;          // > 0: the deepest zoom shows that many bars (Deepwater; wins over maxBarSpacing)
  bool fixLeftEdge = false, fixRightEdge = false;
  bool lockVisibleTimeRangeOnResize = false;
  bool rightBarStaysOnScroll = false;
  bool borderVisible = true;
  QColor borderColor = QColor("#2B2B43");
  bool visible = true;
  bool timeVisible = false, secondsVisible = true;
  bool shiftVisibleRangeOnNewBar = true;
  bool ticksVisible = false;
  bool allowBoldLabels = true;
  int minimumHeight = 0;
  // (time, type) → label; return a null QString to keep the default label.
  std::function<QString(Time, TickMarkType)> tickMarkFormatter;
};

struct CrosshairLineOptions {
  QColor color = QColor("#9598A1");
  int width = 1;
  LineStyle style = LineStyle::LargeDashed;
  bool visible = true;
  bool labelVisible = true;
  QColor labelBackgroundColor = QColor("#131722");
};
struct CrosshairOptions {
  CrosshairMode mode = CrosshairMode::Magnet;
  CrosshairLineOptions vertLine, horzLine;
};

struct GridLineOptions { QColor color = QColor("#D6DCDE"); LineStyle style = LineStyle::Solid; bool visible = true; };
struct GridOptions { GridLineOptions vertLines, horzLines; };

struct LayoutOptions {
  QColor background = Qt::white;      // background: { type: "solid", color }
  QColor textColor = QColor("#191919");
  int fontSize = 12;
  QString fontFamily;                 // empty = Deepwater's mono font (theme::mono)
  struct Panes {
    bool enableResize = true;
    QColor separatorColor = QColor("#E0E3EB");
    QColor separatorHoverColor = QColor(178, 181, 189, 51);
  } panes;
  bool attributionLogo = false;       // accepted and ignored: no logo is ever drawn
};

struct LocalizationOptions {
  QString locale;
  std::function<QString(double)> priceFormatter;   // used by every series with a "price" format
  std::function<QString(Time)> timeFormatter;      // crosshair time label
  QString dateFormat = "dd MMM 'yy";
};

struct HandleScrollOptions {
  bool mouseWheel = true, pressedMouseMove = true, horzTouchDrag = true, vertTouchDrag = true;
  void setAll(bool on) { mouseWheel = pressedMouseMove = horzTouchDrag = vertTouchDrag = on; }
};
struct HandleScaleOptions {
  struct Axes { bool time = true, price = true; };
  Axes axisPressedMouseMove, axisDoubleClickReset;
  bool mouseWheel = true, pinch = true;
  void setAll(bool on) {
    axisPressedMouseMove = {on, on}; axisDoubleClickReset = {on, on}; mouseWheel = pinch = on;
  }
};

struct ChartOptions {
  bool autoSize = true;               // the widget follows its layout; kept for parity
  LayoutOptions layout;
  GridOptions grid;
  CrosshairOptions crosshair;
  PriceScaleOptions rightPriceScale;
  PriceScaleOptions leftPriceScale = [] { PriceScaleOptions o; o.visible = false; return o; }();
  PriceScaleOptions overlayPriceScales;
  TimeScaleOptions timeScale;
  LocalizationOptions localization;
  HandleScrollOptions handleScroll;
  HandleScaleOptions handleScale;
};

// subscribeCrosshairMove / subscribeClick / subscribeDblClick parameter (JS MouseEventParams).
struct MouseEventParams {
  std::optional<Time> time;           // time of the data point under the crosshair (none in the right offset area)
  std::optional<double> logical;      // logical index (integer, like Lightweight Charts' coordinateToLogical)
  std::optional<QPointF> point;       // pane coordinates (x from the plot's left, y from the pane's top)
  int paneIndex = -1;
  QHash<const Series*, DataPoint> seriesData;
  Series* hoveredSeries = nullptr;
  QString hoveredObjectId;            // from a primitive's hitTest
  Qt::KeyboardModifiers modifiers;
  Qt::MouseButton button = Qt::NoButton;
};
using MouseEventHandler = std::function<void(const MouseEventParams&)>;

}  // namespace charts::plot
