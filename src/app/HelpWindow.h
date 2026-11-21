#pragma once
#include <QDialog>
#include <QHash>
#include <QList>
#include <QPointer>

class QLineEdit;
class QTextBrowser;
class QTextDocument;
class QLabel;
class QScrollArea;
class QVariantAnimation;
namespace help { class NavList; class Pane; class Scrim; }

// Deepwater's documentation (header ?, F1 / Cmd+?): a floating window over almost the whole main window.
// Left: the chapters (Deepwater, then one per main tab, in the header's order) and a search field; middle: the chapter's
// text; right: its sections (## and ###), the one being read highlighted.
// Chapters are Markdown files embedded at :/help/<en|fr>/<id>.md (resources/help), picked by the interface language
// with English as fallback; line 1 "# Title" is the chapter's name. Links `help:<id>` open another chapter.
// Qt's Markdown importer ignores style sheets: fonts and colours are set on the document after loading (style()).
class HelpWindow : public QDialog {
  Q_OBJECT
public:
  explicit HelpWindow(QWidget* mainWindow);
  // Opens `chapter` (a main tab id, or "start"), optionally scrolled to the section titled `section`.
  void showChapter(const QString& chapter, const QString& section = {});
  QString currentChapter() const { return m_current; }
  // Narrow windows: pulls a side panel over the text (1 the chapters, 2 the sections) or closes it (0). Test hook:
  // DW_HELP_PANE=1|2 with --help-page.
  void openPane(int which);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  bool eventFilter(QObject* o, QEvent* e) override;   // follows the main window's moves and resizes
private:
  struct Heading { QString title; int level; int pos; };
  struct Chapter { QString id, title; QTextDocument* doc = nullptr; QList<Heading> headings; QString folded; };
  void load();                      // (re)reads every chapter in the current language
  void restyle();                   // fonts and colours of every document for the current theme
  void style(QTextDocument* doc);
  void fitToMain();
  void relayout();
  void fillChapterList();
  void fillToc();
  void syncTocToScroll();
  void scrollToPos(int pos);
  void runSearch();
  void stepMatch(int dir);
  void showMatches();
  int chapterIndex(const QString& id) const;
  QRectF closeRect() const;
  // narrow windows: the side columns become panels pulled over the text (a phone: both; under 980 px: the sections)
  bool leftIsPanel() const;
  bool rightIsPanel() const;
  int panelWidth() const;
  void placePanes(double t);
  QRectF chaptersButton() const;     // null when the chapters are a column
  QRectF sectionsButton() const;

  QList<Chapter> m_chapters;
  QString m_current;
  QPointer<QWidget> m_main;
  QLineEdit* m_search;
  QLabel* m_count;
  help::NavList* m_chapterList;
  help::NavList* m_toc;
  QScrollArea* m_chapterScroll;
  QScrollArea* m_tocScroll;
  QTextBrowser* m_text;
  help::Pane* m_leftPane;
  help::Pane* m_rightPane;
  help::Scrim* m_scrim;
  QVariantAnimation* m_paneAnim;
  bool m_leftPaneOpen = false, m_rightPaneOpen = false;
  double m_paneT = 0;
  QString m_query;
  QList<int> m_matches;             // positions of the query in the current chapter
  int m_match = -1;
  int m_pendingPos = -1;            // a section asked for before the window had its size
  bool m_hoverClose = false, m_dragging = false;
  QPointF m_dragFrom;
};
