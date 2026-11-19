#pragma once
#include <QWidget>
#include <QElapsedTimer>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QPixmap>
#include <QDateTime>
#include "news/Feed.h"

class QLineEdit;

namespace news::pages {

// Shared state of the News Feed page: language, source chips, topic filter, search, reading queue.
class FeedState : public QObject {
  Q_OBJECT
public:
  explicit FeedState(QObject* parent = nullptr);
  bool fr = true;                    // show news text in French (auto-translated)
  QSet<QString> sourcesOff;          // source ids switched off
  int topic = -1;                    // -1 none, 0 TOP IMPACT, else a news::Topic bit
  QString search;
  QStringList queue;                 // story ids
  QString tx(const QString& en) const;          // displayed text (translated when available)
  bool translating() const;                     // FR on and translations pending
  QList<news::Story> filtered() const;          // stories of the current filters, newest first (TOP IMPACT: ranked)
  QList<news::Story> visibleSources() const;    // all stories of the active sources
  void save();
  void setTopic(int t) { topic = t; emit changed(); }
signals:
  void changed();                    // filters
  void textChanged();                // language or translations arrived
  void queueChanged();
  void openStory(const QString& id);
};

// Source name as displayed: publishers' names unchanged; Deepwater's own group names translated.
QString sourceLabel(const QString& name);
// Age texts: "29M AGO" (cards) / "14h ago" (ticker).
QString ageUpper(const QDateTime& t);
QString ageCompact(const QDateTime& t);   // "6H" (FR "6 H")
QString ageLower(const QDateTime& t);
// Image of a story (cover-cropped to `size`), or a generated tile when the feed has none / not loaded yet.
// Calls `w->update()` when the image arrives.
QPixmap storyPixmap(const news::Story& s, QSize size, QWidget* w);

// "MARKET WIRE" bar: source chips, live clock, language chip, story count.
class WireHeader : public QWidget {
  Q_OBJECT
public:
  WireHeader(FeedState* st, QWidget* parent = nullptr);
  void setCount(int n) { m_count = n; update(); }
  void setLive(bool on) { m_live = on; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  struct Hit { QRectF r; QString id; };
  QList<Hit> hits() const;                 // chips that fit, "more" (+N menu), then "lang" last
  mutable bool m_caption = true;           // room for the AUTO-TRANSLATED caption
  mutable QStringList m_hidden;            // sources folded into the +N chip
  FeedState* m_st;
  int m_count = 0, m_hover = -1;
  bool m_live = true;
  QTimer m_clock;
};

// Endless right→left headline marquee (≈40 px/s, 30 fps while visible; painted from a cached strip).
class Ticker : public QWidget {
  Q_OBJECT
public:
  Ticker(FeedState* st, QWidget* parent = nullptr);
  void setStories(const QList<news::Story>& s);
  void rebuild();
protected:
  void paintEvent(QPaintEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
private:
  FeedState* m_st;
  QList<news::Story> m_stories;
  QPixmap m_strip;
  QList<QPair<double, QString>> m_spans;   // item start x in the strip → story id
  double m_offset = 0;
  QTimer m_timer;
  QTimer m_rebuildSoon;                    // coalesces translation bursts into one rebuild
  QElapsedTimer m_lastRebuild;
  qint64 m_lastMs = 0;
  bool m_paused = false;
  bool m_dirty = false;                    // texts changed while hidden
};

// "● LEAD STORY" hero panel with brackets.
class LeadStory : public QWidget {
  Q_OBJECT
public:
  // 300 at the reference; taller on wide windows (bigger image); short viewports (< 560 px, laptop windows)
  // cap the image at 30 % of the viewport height so the cards below stay in view
  static int preferredHeight(int width, int viewHeight = 0);
  LeadStory(FeedState* st, QWidget* parent = nullptr);
  void setStory(const news::Story& s, bool showTag);
  void clear() { m_has = false; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  FeedState* m_st;
  news::Story m_s;
  bool m_has = false, m_tag = false, m_hover = false;
};

// Search box + topic chips.
class FilterBar : public QWidget {
  Q_OBJECT
public:
  int preferredHeight(int width) const;   // 51 on one row; chips wrap to more rows on narrow windows
  FilterBar(FeedState* st, QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  struct Hit { QRectF r; int topic; QString text; };
  QList<Hit> hits() const;
  QList<Hit> hits(bool tight) const;
  bool tight() const;                      // chips with less padding / letter spacing when that saves a row
  QFont chipFont(bool tight) const;
  FeedState* m_st;
  QLineEdit* m_search;
  int m_hover = -1;
};

// Story cards (thumbnail, tag, title, source · age), hover "+" adds to the reading queue.
class CardList : public QWidget {
  Q_OBJECT
public:
  CardList(FeedState* st, QWidget* parent = nullptr);
  void setStories(const QList<news::Story>& s, bool showTags);
  int heightForWidth(int w) const override;
  bool hasHeightForWidth() const override { return true; }
  QSize sizeHint() const override { return QSize(600, heightForWidth(width())); }
  void hoverAt(const QPoint& p);     // test hook
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
private:
  struct Card { double x, y, w, h; QStringList lines; };   // two columns of cards on very wide windows
  void relayout();
  QRectF plusRect(const QRectF& card) const { return QRectF(card.right() - 40, card.center().y() - 15.5, 31, 31); }
  FeedState* m_st;
  QList<news::Story> m_s;
  QList<Card> m_cards;
  bool m_tags = false;
  int m_hover = -1; bool m_hoverPlus = false;
  int m_layoutW = -1;
  bool m_pinHover = false;           // test hook keeps the hover state
};

// Right column: "LATEST HEADLINES" ranked by impact (high first, then important, newest first).
class HeadlinesPanel : public QWidget {
  Q_OBJECT
public:
  HeadlinesPanel(FeedState* st, QWidget* parent = nullptr);
  void setStories(const QList<news::Story>& s);
protected:
  void paintEvent(QPaintEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
private:
  struct Row { double y, h; QStringList lines; };
  void relayout();
  FeedState* m_st;
  QList<news::Story> m_s;
  QList<Row> m_rows;
  double m_scroll = 0, m_target = 0;
  QTimer m_anim;
  int m_hover = -1;
  bool m_dirty = false;              // texts changed while hidden
};
}  // namespace news::pages
