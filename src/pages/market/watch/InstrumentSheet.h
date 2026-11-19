#pragma once
#include <QWidget>
#include <QHash>
#include <QVector>
#include "market/Candles.h"

class QScrollArea;
class QVariantAnimation;

namespace watch {
class CandleChart;
class SheetCard;
class RetailCard;
class CotCard;
class DataCompareCard;
class MeetingCard;
class TechTilesCard;
class IndexPerfCard;
class SeasonMonthCard;
class YearPathCard;

// Full-page instrument sheet opened from a Markets Watch row: fixed header (symbol, ×) over a scrolling grid of
// cards. Each card loads on its own: chart (market::candles), technical tiles (analytics::technicalRead), index
// performance (computed here), seasonality (seasonality engine), retail (positioning::Retail), COT
// (positioning::Cot), data compare (macro::MacroStore), meeting compare (macro::CentralBanks).
class InstrumentSheet : public QWidget {
  Q_OBJECT
public:
  explicit InstrumentSheet(QWidget* parent = nullptr);
  void open(const QString& symbol);
  QString symbol() const { return m_symbol; }
  // The economies compared by the sheet: {base, quote} for a currency pair, {home currency} otherwise.
  static QStringList legsOf(const QString& symbol);
  static QString bankOf(const QString& ccy);       // "ECB", "FEDERAL RESERVE"…
  void scrollTo(int y);                            // test hook / programmatic scroll

  RetailCard* retail() const { return m_retail; }
  CotCard* cot() const { return m_cot; }
  DataCompareCard* dataCompare() const { return m_data; }
  MeetingCard* meeting() const { return m_meeting; }
  CandleChart* chart() const { return m_chart; }
signals:
  void opened(const QString& symbol, const QStringList& legs);
  void closed();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void feedRetail();
  void loadCot();
  void feedMacro();
  void feedMeetings();
  void relayout();
  void loadTechnicals();
  void loadSeasonality();
  void loadPerformance();
  QRectF closeRect() const;

  QString m_symbol; int m_gen = 0; bool m_closeHover = false;
  QScrollArea* m_scroll; QWidget* m_content;
  QVariantAnimation* m_scrollAnim; int m_scrollTarget = 0;
  CandleChart* m_chart;
  RetailCard* m_retail; CotCard* m_cot; DataCompareCard* m_data; MeetingCard* m_meeting;
  TechTilesCard* m_tech; IndexPerfCard* m_perf; SeasonMonthCard* m_month; YearPathCard* m_path;
  QHash<QString, QVector<market::Bar>> m_perfBars; int m_perfPending = 0;
};
}
