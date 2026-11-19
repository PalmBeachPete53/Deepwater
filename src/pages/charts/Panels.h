#pragma once
// Charts tab — side panel (Watchlist, Alerts, Calendar, News) and bottom panel (Strategy tester, Screener, Heatmap,
// Seasonals, Financials, Paper trading, Indicator editor). Port of quant-terminal public/js/panels.js, research.js,
// quantui.js (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). The PTQ tab is left out (user's choice).
//
// CONTRACT: the page shell (ChartsApp) creates Panels and places side() and bottom() in its layout; it calls the
// hooks below exactly where the JS app calls app.panels.<hook>. Two agents implement it: SidePanel.cpp and
// BottomPanel.cpp (+ their private files under src/pages/charts/panels/). Add members freely; do not change or
// remove the ones below.
#include <QObject>
#include <QJsonObject>
#include <QString>

class QWidget;

namespace charts::ui {

class ChartsApp;
class ChartPane;
class SidePanel;     // src/pages/charts/panels/SidePanel.{h,cpp}
class BottomPanel;   // src/pages/charts/panels/BottomPanel.{h,cpp}

class Panels : public QObject {
  Q_OBJECT
public:
  explicit Panels(ChartsApp* app);
  QWidget* side() const;     // <aside class="side">: tabs + body
  QWidget* bottom() const;   // <div class="bottom collapsed">: tabs + body, starts collapsed like the JS

  // ---- hooks for the app (JS P.showBottom, P.openAlertDialog, P.onActiveChange, …) ----
  void showBottom(const QString& tab, bool toggle = false);   // "tester" | "screener" | "heatmap" | "seasonals" |
                                                              // "financials" | "paper" | "editor"
  void showSide(const QString& tab);                          // "watchlist" | "alerts" | "calendar" | "news"
  // preset: { rules: [...], ... } as in the JS openAlertDialog(preset)
  void openAlertDialog(const QJsonObject& preset = {});
  void onActiveChange();
  void onAlertFired(const QJsonObject& event);
  void onTheme();
  void onReplayChange();
  void onReplayBar(ChartPane* pane, const QJsonObject& bar);

  SidePanel* sidePanel = nullptr;
  BottomPanel* bottomPanel = nullptr;

private:
  ChartsApp* app;
};

}  // namespace charts::ui
