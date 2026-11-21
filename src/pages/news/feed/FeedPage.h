#pragma once
#include <QWidget>
#include <QTimer>
#include <QSet>

class QScrollArea;
class QStackedWidget;

namespace news::pages {
class FeedState;
class WireHeader;
class Ticker;
class LeadStory;
class FilterBar;
class CardList;
class HeadlinesPanel;
class ReaderView;
class NewsFooter;

// News › News Feed ("MARKET WIRE"): source bar, ticker, lead story, topic filters, cards, latest headlines ranked
// by impact; clicking a story opens the reader (feed content + automated read).
// Page hooks (env): DW_FEED_TOPIC=<top-impact|central-bank|inflation|employment|gdp|pmi|retail-sales|earnings>,
// DW_FEED_LANG=<en|fr>, DW_FEED_HOVER=<card index>, DW_FEED_OPEN=<card index|lead>, DW_FEED_READER_SCROLL=<px>,
// DW_FEED_FIND=<word>.
class FeedPage : public QWidget {
  Q_OBJECT
public:
  explicit FeedPage(QWidget* parent = nullptr);
protected:
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void refresh();
  void layoutFeed();
  void layoutPhone(int W, int H);
  void requestTranslations();
  void openStory(const QString& id);
  void applyHooks();

  FeedState* m_st;
  QStackedWidget* m_stack;
  QWidget* m_feed;
  WireHeader* m_header;
  Ticker* m_ticker;
  QScrollArea* m_scroll;
  QWidget* m_column;
  LeadStory* m_lead;
  FilterBar* m_filters;
  CardList* m_cards;
  HeadlinesPanel* m_headlines;
  NewsFooter* m_footer;
  ReaderView* m_reader;
  QTimer m_coalesce;
  QTimer m_trSoon;                 // coalesces translation requests (textChanged bursts)
  bool m_hooksDone = false;
  bool m_stale = true;             // something changed while hidden: refresh when shown
  bool m_feedDirty = true;         // new stories since the ticker and headlines were filled
  QSet<QString> m_shownOff;        // sources switched off when they were filled
};
}
