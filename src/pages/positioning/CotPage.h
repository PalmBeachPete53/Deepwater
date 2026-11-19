#pragma once
#include <QWidget>
#include <QHash>
#include "positioning/Cot.h"
#include "market/Candles.h"

namespace positioning {
namespace pk { class Segmented; class Footer; class Scroll; }

// Positioning › COT ("COT Advanced"): CFTC Commitments of Traders for one market (positioning::Cot), price panel
// (unweighted currency index computed from Yahoo daily FX candles, or the contract's future), weekly long:short split,
// net positions by category, Legacy and TFF/Disaggregated tables, sector donuts, stats and the automated read.
// Settings: positioning.cot.class, positioning.cot.market, positioning.cot.range, positioning.cot.category,
//           (the automated read starts in the interface language; its EN/FR toggle is not saved).
// Test hooks: DW_COT_MARKET=EUR, DW_COT_SCROLL=px, DW_COT_HOVER=x,y (page coordinates), DW_COT_LANG=fr.
class CotPage : public QWidget {
public:
  explicit CotPage(QWidget* parent = nullptr);
protected:
  void showEvent(QShowEvent*) override;
private:
  void setAssetClass(const QString& cls, bool keepMarket = false);
  void selectMarket(const QString& id);
  void loadPrice();
  void apply();
  int rangeWeeks() const;

  class CotHeader* m_header;
  class CotMarketBar* m_markets;
  pk::Segmented* m_range;
  pk::Scroll* m_scroll;
  class CotContent* m_content;
  pk::Footer* m_footer;
  QString m_class = "fx", m_market = "USD";
  int m_category = 2, m_priceReq = 0;
  positioning::CotHistory m_hist;
  bool m_loaded = false;
  QDateTime m_histAt;               // when m_hist arrived (the page reloads on show once it is 6 h old)
};
}  // namespace positioning
