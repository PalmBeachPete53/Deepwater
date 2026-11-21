#pragma once
#include <QWidget>
#include <QSet>
#include <QDate>
#include "news/Calendar.h"

class QLineEdit;
class QVariantAnimation;

namespace global { class PageScroll; }

namespace news::pages {
class CalendarTable;
class RangePanel;
class CalendarFilters;
class NewsFooter;

// News › Calendar: week table with filters, inline release history and the economists' range panel.
// Page hooks (env, for screenshots/tests): DW_CAL_WEEK=yyyy-MM-dd, DW_CAL_PANEL=<title part>,
// DW_CAL_EXPAND=<title part>, DW_CAL_SCROLL=<px>, DW_CAL_CHART=<bars|line>,<5y|10y|20y|max>[,<hover 0..1>],
// DW_CAL_PANEL_CLOSE=1 (open then close: focus outline), DW_CAL_HOVER=x,y (page coordinates).
class CalendarPage : public QWidget {
  Q_OBJECT
public:
  explicit CalendarPage(QWidget* parent = nullptr);
protected:
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
private:
  void loadWeek(const QDate& w);
  void refill();
  void openPanel(const news::Event& e, int col);
  void closePanel();
  void layoutChildren();
  // Phones (ui::compact) and short windows (a phone held sideways): the filters, the whole table and the footer
  // scroll as one page (finger scrolling needs a scroll area); the range panel still slides over it.
  void setScrollMode(bool on);
  int layoutScroll(int w, int viewportH);
  void applyHooks();

  QDate m_week;
  QList<news::Event> m_all;
  QWidget* m_left;
  global::PageScroll* m_pageScroll = nullptr;   // only in scroll mode (it then holds m_left)
  CalendarFilters* m_filters;
  CalendarTable* m_table;
  NewsFooter* m_footer;
  RangePanel* m_panel;
  QVariantAnimation* m_slide;
  double m_progress = 0;
  QString m_panelKey; int m_panelCol = -1;
  bool m_hooksDone = false;
};
}  // namespace news::pages
