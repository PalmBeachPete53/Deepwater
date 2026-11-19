#pragma once
#include "account/Journal.h"
#include "account/Stats.h"
#include "pages/account/AccountKit.h"
#include <QWidget>
#include <QScrollArea>
#include <QTimer>

class QLineEdit;
class QVariantAnimation;

namespace acct {

// What every view shows: the selected account's trades after the year / instrument / side / strategy filters.
struct TrData {
  QString accountName, currency = "USD";
  double balance = 0;
  bool hasAccount = false, demo = false;
  int total = 0;                          // trades of the account (before filters)
  QList<account::Trade> trades;           // filtered, by close time ascending
  account::Stats stats;
};

class TrView : public QWidget {
  Q_OBJECT
public:
  using QWidget::QWidget;
  // positions children for the viewport size (height: the visible height, used to grow charts on tall windows),
  // returns the content height (the page scrolls when it is taller than the viewport)
  virtual int relayout(int width, int height) = 0;
  virtual void setData(const TrData& d) = 0;
  virtual void playEntry() {}
signals:
  void tradeEdit(const QString& tradeId);
  void tradeDelete(const QString& tradeId);
  void relayoutNeeded();                  // content height changed
};

class DashboardView : public TrView {
  Q_OBJECT
public:
  explicit DashboardView(QWidget* parent = nullptr);
  int relayout(int width, int height) override;
  void setData(const TrData& d) override;
  void playEntry() override;
protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  TrData m_d;
  QList<PaintPanel*> m_kpi;
  PaintPanel *m_score, *m_cum, *m_month, *m_quarter, *m_last, *m_read;
  QTimer m_pulse; double m_phase = 0;
};

class TradeTable;
class TradeLogView : public TrView {
  Q_OBJECT
public:
  explicit TradeLogView(QWidget* parent = nullptr);
  int relayout(int width, int height) override;
  void setData(const TrData& d) override;
private:
  void refilter();
  TrData m_d;
  QList<PaintPanel*> m_kpi;
  PaintPanel* m_panel;
  PaintPanel* m_pills;
  QLineEdit* m_search;
  TradeTable* m_table;
  int m_outcome = 0;                      // 0 all, 1 wins, 2 losses, 3 breakeven
  int m_tableCount = 0;
  QStringList m_pillNames;
};

class AdvancedView : public TrView {
  Q_OBJECT
public:
  explicit AdvancedView(QWidget* parent = nullptr);
  int relayout(int width, int height) override;
  void setData(const TrData& d) override;
  void playEntry() override;
private:
  void rebuild();
  TrData m_d;
  QList<account::Trade> m_top5, m_bottom5;   // TOP 5 / BOTTOM 5 TRADES (setData)
  bool m_pct = false;
  PaintPanel* m_head;
  QList<PaintPanel*> m_charts;
};

// Toolbar: view tabs, FILTERS toggle, year selector, account box, last import, LOG TRADE, IMPORT TRADES.
class TrToolbar : public QWidget {
  Q_OBJECT
public:
  explicit TrToolbar(QWidget* parent = nullptr);
  void setView(int v) { m_view = v; update(); }
  void setFiltersOn(bool on) { m_filters = on; update(); }
  void setYearText(const QString& t) { m_year = t; update(); }
  void setAccount(const QString& name, const QString& balance, const QString& lastImport);
  QRect yearRect() const { return layout().year.toRect(); }
  QRect accountRect() const { return layout().account.toRect(); }
  static constexpr int kHeight = 86;
  static QString tabName(int i);
  static QString filtersText();
  static QString logText();
  static QString importText();
signals:
  void viewClicked(int v);
  void filtersClicked();
  void yearClicked();
  void accountClicked();
  void logClicked();
  void importClicked();
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  bool event(QEvent*) override;
private:
  struct L { QRectF tabs[3], filters, year, account, lastImport, log, import; bool showImport = true; int yearPx = 19; };
  L layout() const;
  int hitTest(const QPointF& p) const;    // 0..2 tabs, 3 filters, 4 year, 5 account, 6 log, 7 import, -1
  int hoverAt() const;                    // hovered element (real pointer or the DW_HOVER test pointer)
  int m_view = 0, m_hover = -1, m_focus = -1;   // focus: 0..2 tabs, 3 FILTERS (ring drawn once m_focusShown)
  bool m_focusShown = false;
  QTimer m_focusDelay;
  bool m_filters = false;
  QString m_year = "All years", m_name, m_balance, m_last;
};

// Scroll area with smooth wheel scrolling and no visible scroll bar (the reference shows none).
class SmoothScroll : public QScrollArea {
  Q_OBJECT
public:
  explicit SmoothScroll(QWidget* parent = nullptr);
  void scrollTo(int y, bool animated = true);
protected:
  void wheelEvent(QWheelEvent* e) override;
private:
  QVariantAnimation* m_anim; int m_target = 0;
};

class TrackRecordPage : public QWidget {
  Q_OBJECT
public:
  explicit TrackRecordPage(QWidget* parent = nullptr);
protected:
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void paintEvent(QPaintEvent*) override;
private:
  void reload();                          // journal changed: rebuild filter lists and data
  void applyFilters();
  void setView(int v, bool animate = true);
  void layoutPage();
  void yearMenu();
  void accountMenu();
  void logTrade(const QString& editId = {});
  void importTrades();
  void deleteTrade(const QString& id);
  bool ensureAccount();
  void showJournalError();                // journal.json unreadable / not saved (shown once per message, when visible)

  QString m_shownError;
  TrToolbar* m_bar;
  QWidget* m_filterBar;
  ComboChip *m_fInstr, *m_fSide, *m_fStrat;
  PaintPanel* m_counter;
  SmoothScroll* m_scroll;
  QWidget* m_content;
  TrView* m_views[3];
  PaintPanel* m_empty;
  int m_view = 0, m_year = 0;             // 0 = all years
  bool m_filtersOn = false;
  QList<int> m_years;
  TrData m_data;
};
}
