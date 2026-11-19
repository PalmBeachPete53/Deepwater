#pragma once
#include "analytics/Liquidity.h"
#include <QWidget>
#include <QTimer>
#include <QDateTime>

namespace mkit { class TitleStrip; class KpiCard; }
class QScrollArea;
class QLabel;
class CandleChart;
class LiquidityPanel;
class InstrumentButton;
class InstrumentDropdown;

// Market › Liquidity: stop pools (equal highs / lows) and sweeps of one instrument from real hourly OHLC (Yahoo,
// 2 years), computed by Deepwater (analytics/Liquidity.h holds the rules). Fixed title strip and footer, content
// scrolls between them.
// Page hooks (tests): DW_LIQ_SYMBOL=<app symbol>, DW_LIQ_TAB=<0..2>, DW_LIQ_HOVER=<x,y page coords, after the scroll
// hook; on the chart → crosshair, elsewhere → hover>,
// DW_LIQ_ZOOM=<wheel steps at the chart centre>, DW_LIQ_DROPDOWN=1, DW_LIQ_SCROLL=<px>.
class LiquidityPage : public QWidget {
  Q_OBJECT
public:
  explicit LiquidityPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  bool eventFilter(QObject* o, QEvent* e) override;   // the viewport narrows when the vertical bar appears
private:
  void load(const QString& symbol, bool resetView);
  void relayout();
  void updateLast();
  void updateKpis();
  void applyHooks();
  static QString footnote();
  QString m_symbol;
  mkit::TitleStrip* m_title; InstrumentButton* m_button; InstrumentDropdown* m_drop; QLabel* m_price;
  QScrollArea* m_scroll; QWidget* m_content;
  QVector<mkit::KpiCard*> m_kpi;
  CandleChart* m_chart; LiquidityPanel* m_panel;
  QVector<market::Bar> m_bars;
  QVector<analytics::Pool> m_pools;
  double m_atr = NAN, m_last = NAN;
  int m_dec = 5, m_loadId = 0;
  QDateTime m_dataTime;
  QString m_status;
  QTimer m_refresh;
  bool m_started = false, m_hooksDone = false;
};
