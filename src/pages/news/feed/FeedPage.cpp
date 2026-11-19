#include "pages/news/feed/FeedPage.h"
#include "pages/news/feed/FeedWidgets.h"
#include "pages/news/feed/Reader.h"
#include "pages/news/calendar/CalendarFilters.h"
#include "news/Translate.h"
#include "app/Pages.h"
#include <QTimer>
#include "ui/Theme.h"
#include "pages/news/ThemeColor.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>

DW_PAGE("news/news-feed", news::pages::FeedPage)

namespace news::pages {

FeedPage::FeedPage(QWidget* parent) : QWidget(parent) {
  setAutoFillBackground(true);
  QPalette pl = palette(); pl.setColor(QPalette::Window, theme::map("#040302")); setPalette(pl);
  m_st = new FeedState(this);
  const QByteArray lang = qgetenv("DW_FEED_LANG");
  if (!lang.isEmpty()) m_st->fr = lang == "fr";

  m_stack = new QStackedWidget(this);
  m_feed = new QWidget;
  m_header = new WireHeader(m_st, m_feed);
  m_ticker = new Ticker(m_st, m_feed);
  m_scroll = new QScrollArea(m_feed);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setWidgetResizable(false);
  m_scroll->verticalScrollBar()->setSingleStep(40);
  m_column = new QWidget;
  m_column->setAutoFillBackground(true);
  m_column->setPalette(pl);
  m_lead = new LeadStory(m_st, m_column);
  m_filters = new FilterBar(m_st, m_column);
  m_cards = new CardList(m_st, m_column);
  m_scroll->setWidget(m_column);
  m_headlines = new HeadlinesPanel(m_st, m_feed);
  m_footer = new NewsFooter(m_feed);
  m_reader = new ReaderView(m_st);
  m_stack->addWidget(m_feed);
  m_stack->addWidget(m_reader);

  m_coalesce.setSingleShot(true); m_coalesce.setInterval(120);
  connect(&m_coalesce, &QTimer::timeout, this, &FeedPage::refresh);
  connect(&news::Feed::instance(), &news::Feed::updated, this, [this] { m_feedDirty = true; if (!m_coalesce.isActive()) m_coalesce.start(); });
  connect(m_st, &FeedState::changed, this, [this] { refresh(); m_scroll->verticalScrollBar()->setValue(0); });
  connect(m_st, &FeedState::openStory, this, &FeedPage::openStory);
  // translations arriving (one textChanged per headline) ask for the missing ones again: once per burst, not per
  // headline, and not while the page is hidden
  m_trSoon.setSingleShot(true); m_trSoon.setInterval(500);
  connect(&m_trSoon, &QTimer::timeout, this, [this] { if (m_st->fr && isVisible()) requestTranslations(); });
  connect(m_st, &FeedState::textChanged, this, [this] { if (m_st->fr && !m_trSoon.isActive()) m_trSoon.start(); m_header->update(); });
  connect(m_reader, &ReaderView::back, this, [this] { m_stack->setCurrentWidget(m_feed); });
  refresh();
}

void FeedPage::layoutFeed() {
  const int W = m_feed->width(), H = m_feed->height();
  m_header->setGeometry(0, 0, W, 41);
  m_ticker->setGeometry(0, 41, W, 37);
  // headlines column: 326 px at the reference, 280…440 px with the window
  const int hw = std::clamp(int(std::round(W * 0.204)), 280, 440);
  const int mainW = std::max(200, W - hw - 35);
  m_scroll->setGeometry(10, 86, mainW, H - 86 - 31);
  const int leadH = LeadStory::preferredHeight(mainW, H - 86 - 31);
  const int filterH = m_filters->preferredHeight(mainW);
  m_lead->setGeometry(0, 0, mainW, leadH);
  m_filters->setGeometry(0, leadH + 12, mainW, filterH);
  const int cardsY = leadH + 12 + filterH + 12;
  m_cards->setGeometry(0, cardsY, mainW, m_cards->height());
  m_cards->resize(mainW, m_cards->height());
  m_column->setFixedSize(mainW, cardsY + m_cards->height() + 24);
  m_headlines->setGeometry(W - hw - 7, 86, hw, H - 86);
  m_footer->setGeometry(0, H - 31, W - hw - 16, 31);
}

void FeedPage::resizeEvent(QResizeEvent*) {
  m_stack->setGeometry(rect());
  m_feed->resize(size());
  layoutFeed();
}

void FeedPage::refresh() {
  // hidden: nothing is shown, so nothing is rebuilt (the feed updates every 30 s while another page is open);
  // showEvent refreshes once
  if (!isVisible()) { m_stale = true; return; }
  m_stale = false;
  const auto list = m_st->filtered();
  m_header->setCount(int(list.size()));
  const QDateTime last = news::Feed::instance().lastRefresh();
  m_header->setLive(last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < 20 * 60);
  m_footer->setTime(last);
  // the ticker and the headlines column show every story of the active sources: a search keystroke or a topic
  // chip does not change them, so they are rebuilt only for new stories or another set of sources
  if (m_feedDirty || m_st->sourcesOff != m_shownOff) {
    const auto all = m_st->visibleSources();
    m_ticker->setStories(all);
    m_headlines->setStories(all);
    m_feedDirty = false; m_shownOff = m_st->sourcesOff;
  }
  const bool tags = m_st->topic >= 0;
  // lead: the most market-moving recent story (impact score, central bank / macro topics, market words),
  // preferring one with an image; recency decides between equals. Lifestyle pieces never reach the lists.
  int lead = -1;
  const QDateTime now = QDateTime::currentDateTimeUtc();
  if (m_st->topic == 0) lead = list.isEmpty() ? -1 : 0;
  else {
    double best = -1;
    for (int hours : {6, 24, 96}) {
      for (int i = 0; i < list.size(); ++i) {
        const auto& s = list[i];
        const qint64 age = s.time.secsTo(now);
        if (age > hours * 3600) break;
        const bool macro = s.topics & (news::CentralBank | news::Inflation | news::Employment | news::Gdp);
        double sc = s.score * 2 + (macro ? 3 : 0) + std::min(s.market, 4) + (s.image.isEmpty() ? 0 : 6) - age / 3600.0 / (hours / 3.0);
        if (s.score == 0 && s.market == 0) continue;
        if (sc > best) { best = sc; lead = i; }
      }
      if (lead >= 0) break;
    }
  }
  if (lead < 0 && !list.isEmpty()) lead = 0;
  QList<news::Story> rest;
  for (int i = 0; i < list.size() && rest.size() < 120; ++i) if (i != lead) rest << list[i];
  if (lead >= 0) m_lead->setStory(list[lead], tags); else m_lead->clear();
  m_cards->setStories(rest, tags);
  layoutFeed();
  if (m_st->fr) requestTranslations();
  if (!m_hooksDone && !list.isEmpty()) QTimer::singleShot(200, this, &FeedPage::applyHooks);
}

void FeedPage::requestTranslations() {
  QStringList want;
  const auto all = m_st->visibleSources();
  const auto list = m_st->filtered();
  for (int i = 0; i < list.size() && i < 40; ++i) want << list[i].title;
  for (int i = 0; i < all.size() && i < 24; ++i) want << all[i].title;
  for (const auto& s : all) if (s.impact != news::Impact3::None && want.size() < 140) want << s.title;
  news::Translate::instance().request(want, this, [this] { m_header->update(); });
}

void FeedPage::openStory(const QString& id) {
  const news::Story* s = news::Feed::instance().find(id);
  if (!s) return;
  if (!m_st->queue.contains(id)) { m_st->queue << id; m_st->save(); emit m_st->queueChanged(); }
  m_reader->open(*s);
  m_stack->setCurrentWidget(m_reader);
}

void FeedPage::showEvent(QShowEvent*) {
  news::Feed::instance().setActive(this, true);
  if (m_stale) refresh();
  else layoutFeed();                // the cards may have re-laid out on show (translations that arrived while hidden)
  if (m_st->fr) m_trSoon.start();   // what arrived while hidden
}
void FeedPage::hideEvent(QHideEvent*) { news::Feed::instance().setActive(this, false); }

void FeedPage::applyHooks() {
  if (m_hooksDone) return;
  m_hooksDone = true;
  const QByteArray t = qgetenv("DW_FEED_TOPIC");
  if (!t.isEmpty()) {
    int topic = -1;
    if (t == "top-impact") topic = 0;
    for (auto tp : news::allTopics()) if (news::topicName(tp).toLower().replace(' ', '-') == QString::fromLatin1(t)) topic = int(tp);
    m_st->setTopic(topic);
  }
  const QByteArray hv = qgetenv("DW_FEED_HOVER");
  if (!hv.isEmpty()) QTimer::singleShot(300, this, [this, hv] { m_cards->setProperty("pin", true); m_cards->hoverAt(QPoint(500, 40 + hv.toInt() * 95)); });
  const QByteArray op = qgetenv("DW_FEED_OPEN");
  if (!op.isEmpty()) {
    QTimer::singleShot(400, this, [this, op] {
      const auto list = m_st->filtered();
      if (list.isEmpty()) return;
      int i = op == "lead" ? 0 : std::min<int>(op.toInt() + 1, int(list.size()) - 1);
      if (op.startsWith("src:"))   // first story of a source that carries an article body
        for (int k = 0; k < list.size(); ++k) if (list[k].sourceId == op.mid(4) && !list[k].contentHtml.isEmpty()) { i = k; break; }
      openStory(list[i].id);
      const QByteArray sc = qgetenv("DW_FEED_READER_SCROLL");
      if (!sc.isEmpty()) QTimer::singleShot(1500, this, [this, sc] { m_reader->scrollArticle(sc.toInt()); });
      const QByteArray fd = qgetenv("DW_FEED_FIND");
      if (!fd.isEmpty()) QTimer::singleShot(1500, this, [this, fd] { m_reader->findWord(QString::fromUtf8(fd)); });
    });
  }
}
}
