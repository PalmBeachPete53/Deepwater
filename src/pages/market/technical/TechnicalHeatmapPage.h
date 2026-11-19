#pragma once
#include <QWidget>
#include "app/Preload.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>

namespace analytics { class Scanner; }
namespace mkit { class TitleStrip; class Badge; class KpiCard; class ChipGroup; class SearchBox; }
class HeatTable;
class QScrollArea;
class SignalFile;

// Market › Technical Heatmap: six technical reads per instrument from daily candles (all computed by Deepwater),
// KPI strip, category / trend / text filters, sortable table and the selected instrument's signal file.
// Page hooks (tests): DW_TH_CATEGORY=<chip index>, DW_TH_TREND=<chip index>, DW_TH_SEARCH=<text>,
// DW_TH_CATEGORY_AT=<ms>:<chip> (switch later, to capture the fade/radar transition), DW_TH_HOVER=<x,y> page coords
// (hover state, sent at DW_TH_HOVER_AT ms, default 5000).
class TechnicalHeatmapPage : public QWidget, public app::Preloadable {
  Q_OBJECT
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit TechnicalHeatmapPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void rebuild(bool filterChanged);
  void updateKpis();
  void updateBadge();
  void updatePrices(const QStringList& symbols);
  void select(const QString& symbol, bool animate);
  QString priceText(int item) const;
  analytics::Scanner* m_scan;
  mkit::TitleStrip* m_title; mkit::Badge* m_badge;
  QVector<mkit::KpiCard*> m_kpi;
  mkit::ChipGroup* m_cat; mkit::ChipGroup* m_trend; mkit::SearchBox* m_search;
  HeatTable* m_table; SignalFile* m_file; QScrollArea* m_fileScroll;
  int m_divX = 1226;
  QString m_selected;
  QTimer m_refresh, m_rebuildSoon;
  QElapsedTimer m_lastScan;                 // started with each scan (the daily candles' cache TTL counts from it)
  void startScan();
  bool m_userPick = false;
  double m_bull = 0.5;
};
