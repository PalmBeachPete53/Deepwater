#pragma once
#include "seasonality/Seasonality.h"
#include <QWidget>

namespace seasonality {
class ToolBar; class Menu; class TrendCard; class WinCard; class BarCard; class RiskCard; class SmoothScroll;
class Content;


// Market › Seasonality ("Seasonality Lab"): average calendar-year path of one instrument over 5–20 years with win
// rate, best/worst month, volatility ratio, weekday / month / per-year bars and risk metrics, all recomputed for the
// chosen period (Full, H1, H2, Q1–Q4 or a range dragged on the chart). Statistics: src/seasonality/.
//
// Test hooks (env): DW_SEASON_PERIOD=H1|H2|Q1..Q4|<slotA>-<slotB>, DW_SEASON_MENU=asset|years|filter|compare,
// DW_SEASON_HOVER=x,y (page coordinates, crosshair), DW_SEASON_SCROLL=<px>, DW_SEASON_ASSET=<symbol>,
// DW_SEASON_COMPARE=<symbol>, DW_SEASON_TOGGLES=actual,index.
class SeasonalityPage : public QWidget {
  Q_OBJECT
public:
  explicit SeasonalityPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
private:
  void load();
  void loadCompare();
  void rebuild();
  void recompute();
  void setPeriod(const seasonality::Period& p, bool clicked);
  void toolClicked(int item);
  void openMenu(int item);
  void closeMenu();
  void stepAsset(int d);
  void applyHooks();

  seasonality::ToolBar* m_tool;
  seasonality::SmoothScroll* m_scroll;
  seasonality::Content* m_content;
  seasonality::TrendCard* m_trend;
  seasonality::WinCard* m_win;
  seasonality::BarCard *m_weekday, *m_month, *m_years;
  seasonality::RiskCard* m_risk;
  seasonality::Menu *m_assetMenu, *m_yearsMenu, *m_filterMenu, *m_compareMenu;
  int m_openItem = -1;

  QString m_symbol, m_compareSymbol;
  int m_nYears = 20;
  bool m_outliers = false, m_actual = false, m_index = false, m_hooksDone = false;
  bool m_phone = false;   // toolbar wrapped inside the scrolling content (phones, short windows)
  seasonality::Period m_period;
  QVector<seasonality::DailyClose> m_closes, m_compareCloses;
  QString m_compareError;   // why the compare symbol has no path (shown in the trend card's subtitle)
  seasonality::Dataset m_ds;
  QVector<double> m_raw, m_smooth;
  int m_loadSeq = 0, m_compareSeq = 0;
};
}  // namespace seasonality
