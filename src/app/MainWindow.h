#pragma once
#include <QMainWindow>
#include <QHash>

class Header;
class QStackedWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  // "tab/sub" (e.g. "market/seasonality"); returns false if unknown
  bool openPage(const QString& id);
  void open(int main, int sub);
  QString currentPageId() const;
  void showOptions();
  // Re-create every page (after a language change); the current page is re-opened.
  void rebuildPages();
  // The slow pages built and loaded in the background after launch (app::Preloadable); off for the audits, which
  // measure one page at a time.
  void setPreload(bool on);
  // Layout audit (dev): the page shown, and dropping every other page so the window's minimum is this page's alone
  // (a QStackedWidget's minimum is the largest of all its pages).
  QWidget* currentPage() const;
  void dropPagesExcept(const QString& id);
private:
  QWidget* ensurePage(const QString& id);   // made on first use, not shown
  // Paper orders fill from the Charts page's quotes and pages are made on first visit: when the saved paper account
  // has working orders, the Charts page is made now (hidden), so its stops, targets and limits fire without a visit.
  void keepPaperOrdersWatched();
  void schedulePreloads(int delayMs);
  void preloadNext();
  bool m_preload = true;
  int m_preloadNext = 0;
  int m_preloadGen = 0;     // a rebuild restarts the chain: an older chain's timer stops
  Header* m_header;
  QStackedWidget* m_stack;
  QHash<QString, QWidget*> m_pages;
};
