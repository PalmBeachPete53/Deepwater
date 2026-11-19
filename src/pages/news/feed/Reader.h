#pragma once
#include <QWidget>
#include <QTimer>
#include <QTextDocument>
#include <QTextCursor>
#include "news/Feed.h"
#include "news/AutoRead.h"

class QScrollArea;
class QLineEdit;
class QVariantAnimation;
class QPainter;

namespace news::pages {
class FeedState;

// Article body: meta, language chip, title, image, what the feed gives (content or summary), "Read on <site>".
class ArticleWidget : public QWidget {
  Q_OBJECT
public:
  ArticleWidget(FeedState* st, QWidget* parent = nullptr);
  void setStory(const news::Story& s);
  void retranslate();                       // rebuild the text after a language change / new translations
  int find(const QString& word);            // highlight; returns matches; y of the first in firstMatchY()
  double firstMatchY() const { return m_firstMatchY; }
  QString plainText() const;                // English plain text (for the automated read)
  bool pendingTranslation() const { return m_pending; }
signals:
  void languageToggled();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void showEvent(QShowEvent*) override;
private:
  void rebuild();
  QString trSignature() const;
  QRectF colRect() const;
  QRectF chipRect() const;
  QRectF readRect() const { return m_readRect; }
  FeedState* m_st;
  news::Story m_s;
  QTextDocument m_doc;
  QStringList m_titleLines;
  int m_titlePx = 46;
  double m_bodyTop = 0, m_imgH = 0;
  QRectF m_readRect;
  QList<QTextCursor> m_hits;
  double m_firstMatchY = -1;
  bool m_pending = false, m_hoverChip = false, m_hoverRead = false;
  QStringList m_paras;                      // English paragraphs (plain) for translation
  QList<bool> m_heads;
  QString m_trSig;                          // trSignature() when the text was last built
  QString m_findWord;                       // highlighted word, found again after a rebuild of the text
  bool m_trDirty = false;                   // translations arrived while hidden
};

// "AUTOMATED READ" side panel: rule-based key points, mentions, live quotes, tone; animated orb; find box.
class AutoReadPanel : public QWidget {
  Q_OBJECT
public:
  explicit AutoReadPanel(QWidget* parent = nullptr);
  void setArticle(const QString& title, const QString& text, bool french = false);
  void setFrench(bool french);   // key points follow the article's EN/FR chip
  QLineEdit* findBox() const { return m_find; }
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void paintOrb(QPainter& p, const QPointF& c, double r) const;
  int orbBoxH() const { return height() < 560 ? 130 : 201; }   // short windows: smaller orb box
  news::AutoRead m_r;
  QLineEdit* m_find;
  QTimer m_orb;
  double m_phase = 0;
  bool m_loaded = false;
  bool m_french = false;
};

// Whole reader page: top bar (back, queue), article column, automated read panel.
class ReaderView : public QWidget {
  Q_OBJECT
public:
  ReaderView(FeedState* st, QWidget* parent = nullptr);
  void open(const news::Story& s);
  QString currentId() const { return m_s.id; }
  void scrollArticle(int y);               // test hook
  void findWord(const QString& w);
signals:
  void back();
protected:
  bool eventFilter(QObject* o, QEvent* e) override;   // smooth wheel scrolling of the article (≈160 px per step)
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  struct Chip { QRectF r, x; QString id; };
  QList<Chip> chips() const;
  QRectF backRect() const;
  double queueX() const;
  FeedState* m_st;
  news::Story m_s;
  QScrollArea* m_scroll;
  QVariantAnimation* m_wheel = nullptr;
  int m_wheelTarget = 0;
  ArticleWidget* m_article;
  AutoReadPanel* m_panel;
  int m_hover = -1; bool m_hoverX = false, m_hoverBack = false;
};
}
