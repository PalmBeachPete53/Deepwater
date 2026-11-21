#pragma once
#include <QWidget>
class QLineEdit;
class QTimer;
namespace global { class PageScroll; }

namespace positioning {
namespace pk { class TitleBar; class Ticker; class Scroll; class Segmented; class Footer; }

// Positioning › Retail Sentiment: Myfxbook community outlook (positioning::Retail) as KPI cards and one row per
// instrument; a row click opens the instrument modal (RetailModal). Needs the user's Myfxbook account: login form
// in the page (password used once, never stored).
// Settings: positioning.retail.category, positioning.retail.sort, positioning.retail.custom (symbols),
//           positioning.retail.customOnly.
// Test hooks: DW_MYFXBOOK_FIXTURE (see positioning/Retail.h), DW_RETAIL_OPEN=EURUSD (open the modal),
//             DW_RETAIL_HOVER=x,y (page coordinates, list or modal chart), DW_RETAIL_SCROLL=px (modal scroll),
//             DW_RETAIL_LOGIN=1 (show the login form).
class RetailSentimentPage : public QWidget {
public:
  explicit RetailSentimentPage(QWidget* parent = nullptr);
protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  void refresh();
  void rebuildRows();
  void openModal(const QString& symbol);
  void showLogin(bool on);
  void editCustom();
  // Phones (upright, or sideways: page under 420 px tall): the cards, filters and the whole table scroll as one page
  // (global::PageScroll, made only in that mode) above the fixed footer; upright, the title bar gives way to the
  // Myfxbook badge at the top of the page.
  void setPhoneMode(bool on);
  int layoutPhone(int w, int viewportH);
  void placeConn(bool inPage);

  pk::TitleBar* m_title;
  class RetailConnBadge* m_conn;
  class RetailKpis* m_kpis;
  QLineEdit* m_filter;
  pk::Segmented *m_cat, *m_sort;
  class RetailCustomButton* m_custom;
  class RetailHeaderRow* m_head;
  pk::Scroll* m_scroll;
  class RetailTable* m_table;
  class RetailLogin* m_login;
  pk::Footer* m_footer;
  class RetailModal* m_modal = nullptr;
  pk::Ticker* m_ticker;
  QTimer* m_clock;
  bool m_forceLogin = false;
  global::PageScroll* m_pageScroll = nullptr;   // phone mode only
  QWidget* m_phoneBox = nullptr;                // its content
  bool m_connInPage = false;
};
}  // namespace positioning
