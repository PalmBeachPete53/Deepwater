#pragma once
#include "market/Candles.h"
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QPointer>
#include <cmath>

// Self-contained price chart of the instrument sheet (stands in for the reference's TradingView embed):
// top toolbar (timeframes, chart types, compare, indicators, snapshot), left drawing bar (crosshair, trend line,
// horizontal line, measure, zoom, magnet, hide, remove), candles/bars/line/area/Heikin Ashi, price and time
// axes with crosshair labels, OHLC legend, range buttons 1D…All, live UTC clock, %/log/auto scale.
// Data: market::candles (Yahoo), last bar kept live by market::QuoteHub. Kept in its own class so it can be
// replaced by a shared chart later: the sheet only calls setSymbol().
namespace watch {
class CandleChart : public QWidget {
  Q_OBJECT
public:
  explicit CandleChart(QWidget* parent = nullptr);
  void setSymbol(const QString& symbol);
  void setTimeframe(const QString& tf);
  void showLoading(const QString& symbol);        // test hook: skeleton without any request           // 1m 5m 15m 30m 1h 4h 1D 1W 1M
  QString timeframe() const { return m_tf; }
  bool hasData() const { return !m_bars.isEmpty(); }
  // test hook: put the crosshair at a widget position
  void hoverAt(const QPointF& p);
signals:
  void loaded(bool ok);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void mouseDoubleClickEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void leaveEvent(QEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  enum Type { Bars, Candles, Hollow, Line, Area, Heikin };
  // tools in the order of the reference's drawing bar
  enum Tool { Cross, Trend, Fib, Pattern, Position, Brush, Text, Emoji, Measure, Zoom };
  enum Kind { KTrend = 1, KHLine, KFib, KPattern, KPosition, KBrush, KText, KEmoji };
  struct Drawing { int kind = 0; QVector<qint64> t; QVector<double> p; QString text; };
  struct Hit { QRectF r; QString id; };

  void load(const QString& range = {});
  void applyRange(const QString& range);
  void onQuotes(const QStringList& syms);
  void fitInitial();
  void rebuild();                                   // static layer into m_cache
  void layout();
  void clampView();
  void autoScale();
  const QVector<market::Bar>& shown() const;       // Heikin Ashi transform when selected (cached per m_barsGen)
  // coordinates
  double xOf(double idx) const;
  double idxOf(double x) const;
  double yOf(double price) const;
  double priceOf(double y) const;
  double toScale(double v) const;                   // log / percent transform
  double fromScale(double s) const;
  qint64 timeOf(double idx) const;
  double idxOfTime(qint64 t) const;
  QString priceText(double p) const;
  QString timeLabel(qint64 t, bool full) const;
  QString hitAt(const QPointF& p) const;
  void click(const QString& id, const QPoint& globalPos);
  void paintToolbar(QPainter& p);
  void paintLeftBar(QPainter& p);
  void paintBottom(QPainter& p, bool clockOnly);
  void paintPlot(QPainter& p);
  void paintOverlay(QPainter& p);
  void paintSkeleton(QPainter& p);
  void loadCompare(const QString& sym);
  void paintDrawing(QPainter& p, const Drawing& d, const QPointF* preview) const;
  int pointsNeeded(Tool t) const;
  QVector<QPair<QRectF, QString>> navButtons() const;   // floating zoom/scroll buttons near the plot bottom
  void contextMenu(const QPoint& globalPos, double price);
  void contextMenuEvent(QContextMenuEvent*) override;
  void snapshot();

  QString m_symbol, m_tf = "4h", m_range;
  QVector<market::Bar> m_bars;
  quint64 m_barsGen = 0;                            // bumped on every change of m_bars
  mutable QVector<market::Bar> m_heikin; mutable quint64 m_heikinGen = ~0ull;
  QString m_tz;            // exchange time zone of the loaded bars (session days of 1D / 1W / 1M)
  bool m_loading = false; QString m_error; int m_req = 0;
  Type m_type = Candles; Tool m_tool = Cross;
  bool m_log = false, m_pct = false, m_auto = true, m_magnet = false, m_hideDrawings = false;
  // view
  double m_right = 0;      // index of the bar at the right edge of the plot (fractional, may exceed last)
  double m_barW = 7;       // px per bar
  double m_lo = 0, m_hi = 1; // visible scale range (in scale units)
  double m_pctBase = NAN;
  // indicators
  bool m_vol = false, m_sma20 = false, m_sma50 = false, m_sma200 = false, m_ema21 = false, m_boll = false;
  // averages of the shown series, valid for one m_barsGen and Heikin Ashi choice (filled lazily in paintPlot)
  struct IndCache { quint64 gen = ~0ull; bool heikin = false; QVector<double> sma20, sma50, sma200, ema21, bollUp, bollLo; };
  IndCache m_ind;
  QString m_cmpSym; QVector<market::Bar> m_cmp;
  QVector<Drawing> m_drawings;
  // interaction
  QPointF m_mouse{-1, -1}; bool m_in = false;
  bool m_drag = false, m_axisDrag = false; QPointF m_dragFrom; double m_dragRight = 0, m_dragLo = 0, m_dragHi = 0;
  bool m_placing = false; Drawing m_pending{}; bool m_brushing = false;
  bool m_legendCollapsed = false; QRectF m_legendPill;
  bool m_measuring = false; QPointF m_mFrom, m_mTo;
  bool m_measureShown = false;   // a finished measurement shown after the tool went back to Cross
  QString m_hoverHit;
  QRectF m_plot, m_priceAxis, m_timeAxis, m_top, m_left, m_bottom, m_clock;
  QVector<Hit> m_hits;
  QPixmap m_cache; bool m_dirty = true;
  QTimer m_clockTimer;
  QString m_toast; QTimer m_toastTimer;
};
}
