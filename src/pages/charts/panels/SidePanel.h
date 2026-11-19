#pragma once
// Charts tab — the side panel: Watchlist, Alerts, Calendar, News. Port of quant-terminal public/js/panels.js
// (showSide, renderSide, watchlist, alerts + openAlertDialog, calendar) and research.js renderNews
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
//
// Panels members (Panels.h) are split between the two panel files:
//   SidePanel.cpp:   Panels::Panels (creates both panels), side(), showSide(), openAlertDialog(), onAlertFired(),
//                    onActiveChange() (side part, then BottomPanel::onActiveChange() when BottomPanel declares it)
//   BottomPanel.cpp: bottom(), showBottom(), onTheme(), onReplayChange(), onReplayBar()
// Panels::Panels builds the bottom panel as `new BottomPanel(app, this)` (or `new BottomPanel(app)`), whichever
// constructor BottomPanel.h declares.
#include "charts/data/Providers.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <limits>

class QScrollArea;
class QVBoxLayout;
class QComboBox;

namespace charts::ui {

class ChartsApp;
namespace style { class TabBar; }
namespace side { class WatchRows; }

class SidePanel : public QWidget {
  Q_OBJECT
public:
  explicit SidePanel(ChartsApp* app, QWidget* parent = nullptr);
  ~SidePanel() override;

  QString tab() const { return sideTab_; }
  void showSide(const QString& tab);            // "watchlist" | "alerts" | "calendar" | "news"
  void renderSide();
  void openAlertDialog(const QJsonObject& preset = {});
  void onActiveChange();                        // the side part of Panels::onActiveChange
  void onAlertFired(const QJsonObject& event);
  void refreshQuotes();                         // every 5 s while visible (JS setInterval)

  // Last quotes of the watchlist and paper symbols (JS `quotes`): price / changePct, NaN when absent.
  struct Q { double price = std::numeric_limits<double>::quiet_NaN(), changePct = std::numeric_limits<double>::quiet_NaN(); };
  const QHash<QString, Q>& quotes() const { return quotes_; }

  QSize sizeHint() const override { return {300, 600}; }

  // UI steps for screenshots (the harness --eval, through the page shell's runUiStep, or env DW_CHARTS_SIDE=
  // "step;step" run once 1.5 s after the panel is first shown): "side:<tab>", "wl:<list name>",
  // "cal:<economic|earnings|dividends>", "calf:<all|Medium|High>", "alert" (empty dialog), "alert3" (dialog with
  // 3 conditions, the second an indicator), "alert-create" (alert3, then Create alert). false for an unknown step.
  bool runUiStep(const QString& step);

protected:
  void paintEvent(QPaintEvent*) override;       // .side border-left
  void showEvent(QShowEvent*) override;
  bool eventFilter(QObject* o, QEvent* e) override;   // the Charts page shown / hidden drives the quote timer

private:
  // ---- body ----
  QWidget* newBody();                           // replaces the tab body (side.innerHTML = ...)
  // ---- watchlist ----
  QStringList wlSymbols() const;
  QJsonObject watchlists() const;
  void renderWatchlist();
  void drawRows();
  void newList();
  void deleteList();
  void removeSymbol(const QString& sym);
  // ---- alerts ----
  void renderAlerts();
  QString describeAlert(const QJsonObject& a) const;
  void testWebhook(const QString& url);
  // ---- calendar ----
  void renderCalendar();
  // ---- news ----
  void renderNews();

  ChartsApp* app;
  style::TabBar* tabs_ = nullptr;
  QScrollArea* scroll_ = nullptr;
  QPointer<QWidget> body_;
  QString sideTab_ = "watchlist";
  int renderSeq_ = 0;                           // stale async answers check (a newer render replaced the body)

  QHash<QString, Q> quotes_;
  QTimer quoteTimer_;
  // The Charts page is shown (the panel itself may be hidden by a narrow window). While it is not, the timer keeps
  // running only for the symbols of pending paper orders, so stops, targets and limits still fill (user's choice,
  // 2026-11-19).
  bool pageShown_ = false;
  QStringList pendingOrderSymbols() const;
  void syncQuoteTimer();
  QPointer<side::WatchRows> rows_;

  QString calFilter_ = "High", calView_ = "economic";
  bool calMine_ = false;
  QString lastSymbol_;
};

}  // namespace charts::ui
