#pragma once
#include <QWidget>
#include <QFont>
#include <QColor>
#include "market/Candles.h"

// Candlestick chart painted with QPainter (Retail detail modal's 1 h chart, COT price panel).
// Kept self-contained so it can later be swapped for a shared chart widget:
//   auto* c = new pos::CandleChart(this);
//   c->setStyle(pos::CandleChart::modalStyle());
//   c->setBars(series.bars, 5, true);   c->setVisibleCount(240);
// Right price axis, time axis, last-price dotted line + axis tag, crosshair with price/date tags, optional
// OHLC legend line (legendH > 0) that follows the hovered candle. Mouse wheel is left to the parent (page scroll).
namespace positioning::pos {
class CandleChart : public QWidget {
  Q_OBJECT
public:
  struct Style {
    QColor bg, up, down, axisText, grid, cross, tagBg, tagFg, legendText, legendLabel;
    QFont axisFont, timeFont, legendFont, tagFont;
    int axisW = 64, timeH = 28, legendH = 0, padTop = 8, padLeft = 0;
    bool gridLines = false, boldMonths = false, darkTagOnUp = false;
    qreal bodyFrac = 0.7;
    int tickSpacing = 38;   // px between price-axis ticks (approx.)
    int rightPadBars = 0;   // empty slots between the last candle and the price axis
    int timeLabelGap = 54;  // minimum px between time-axis labels
  };
  static Style modalStyle();   // TradingView-like dark (Retail modal)
  static Style cotStyle();     // COT price panel

  explicit CandleChart(QWidget* parent = nullptr);
  void setStyle(const Style& s) { m_s = s; update(); }
  // timezone: the series' exchange zone (market::Series::timezone); daily bars are dated by their session day there
  void setBars(const QVector<market::Bar>& bars, int decimals, bool intraday, const QString& timezone = {});
  void setVisibleCount(int n) { m_visible = n; update(); }
  void setLegendTitle(const QString& t, const QString& shortTitle = {}) { m_title = t; m_shortTitle = shortTitle; update(); }
  void setLoading(bool on) { m_loading = on; update(); }
  void setMessage(const QString& m) { m_msg = m; update(); }
  void setHover(const QPointF& p) { m_hover = p; m_hasHover = true; update(); }   // test hook / programmatic
  bool hasBars() const { return !m_bars.isEmpty(); }
  bool isLoading() const { return m_loading; }

protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;

private:
  QRectF plotRect() const;
  QString timeTag(qint64 t) const;
  QDate dayOf(qint64 t) const;   // the date shown for a bar's time
  Style m_s;
  QVector<market::Bar> m_bars;
  int m_dec = 2, m_visible = 0;
  bool m_intraday = false, m_loading = false, m_hasHover = false;
  QPointF m_hover;
  QString m_title, m_shortTitle, m_msg, m_tz;
};
}
