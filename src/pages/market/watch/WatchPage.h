#pragma once
#include <QWidget>
#include "app/Preload.h"
#include <QVector>
#include <QList>
#include <QHash>
#include "market/Instruments.h"

class QScrollArea;
class QStackedLayout;
class QVariantAnimation;

namespace watch {
class HeroHeader; class SearchBox; class CategoryPills; class Segmented; class StatCard; class MarketPanel;
class WatchHistory; class InstrumentSheet;

// Market › Markets Watch: every market of the catalogue in five category columns, live (QuoteHub, 20 s),
// with search, category filter, "today" summary cards, change period 1D / 1W / 1M and ordering.
// A row click opens the instrument sheet over the list.
// Page hooks (tests): DW_WATCH_SHEET=<symbol> opens the sheet, DW_WATCH_SCROLL=<px> scrolls it,
// DW_WATCH_HOVER=x,y puts the chart crosshair (chart-relative), DW_WATCH_STATE="cat,period,order,search",
// DW_WATCH_LOADING=1 freezes the sheet in its loading state (no request).
class WatchPage : public QWidget, public app::Preloadable {
  Q_OBJECT
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit WatchPage(QWidget* parent = nullptr);
  InstrumentSheet* sheet() const { return m_sheet; }
  void openSheet(const QString& symbol);
  void closeSheet();
protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
private:
  void refresh();                        // rows, counts, cards from quotes + filters
  void ensureHistory();                  // 1W / 1M closes of every list (WatchHistory fetches what is missing)
  void layoutBoard();
  double changeOf(const QString& symbol, QString* tip) const;
  void setActive(bool on);

  QStackedLayout* m_stack;
  QScrollArea* m_scroll; QWidget* m_board;
  HeroHeader* m_hero; SearchBox* m_search; CategoryPills* m_pills;
  StatCard* m_cards[4]; Segmented* m_period; Segmented* m_order;
  QList<MarketPanel*> m_panels;          // Forex, Indices, Commodities, Stocks, Crypto, Bonds
  QList<QList<QString>> m_lists;         // curated symbols per panel
  WatchHistory* m_history;
  InstrumentSheet* m_sheet = nullptr;
  QVariantAnimation* m_scrollAnim; int m_scrollTarget = 0;
  int m_cat = 0;
};
}
