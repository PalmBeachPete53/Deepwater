#pragma once
// Charts tab — the bottom panel: Strategy tester, Screener, Heatmap, Seasonals, Financials, Paper trading,
// Indicator editor. Port of quant-terminal public/js/panels.js (bottom half), research.js (renderSeasonals,
// renderFinancials) and quantui.js (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
// No PTQ tab and no PTQ buttons (user's choice).
//
// <div id="bottom" class="bottom collapsed">: 320 px high (CSS .bottom), 39 px collapsed (tabs only), 45 % of the
// centre column when the window is 900 px wide or less (the CSS media query). Starts collapsed on "tester".
// Each tab is one class under panels/bottom/ that rebuilds the body when rendered, like the JS innerHTML renders.
//
// Panels.cpp members implemented in BottomPanel.cpp: bottom(), showBottom(), onTheme(), onReplayChange(),
// onReplayBar(). Panels::Panels (SidePanel.cpp) creates `new BottomPanel(app)`; Panels::onActiveChange calls
// BottomPanel::onActiveChange().
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QWidget>

class QVBoxLayout;
class QPushButton;

namespace charts::ui {

class ChartsApp;
class ChartPane;
namespace style { class TabBar; }

namespace bottom {
class TesterTab;
class ScreenerTab;
class HeatmapTab;
class SeasonalsTab;
class FinancialsTab;
class PaperTab;
class EditorTab;
}

class BottomPanel : public QWidget {
  Q_OBJECT
public:
  explicit BottomPanel(ChartsApp* app, QWidget* parent = nullptr);
  ~BottomPanel() override;

  // JS showBottom(t, toggle): toggle on the open tab collapses; otherwise opens the panel on that tab.
  void showBottom(const QString& tab, bool toggle = false);
  void renderBottom();                       // re-renders the current tab when the panel is open
  QString tab() const { return bottomTab_; }
  bool collapsed() const { return collapsed_; }
  void setCollapsed(bool on);                // the chevron (#bottomToggle)

  // ---- hooks (Panels) ----
  void onActiveChange();
  void onTheme();
  void onReplayChange();
  void onReplayBar(ChartPane* pane, const QJsonObject& bar);

  // Symbols of the live paper account (positions, then working orders): the side panel's refreshQuotes asks
  // quotes for them too (JS paperSymbols()).
  QStringList paperSymbols() const;
  // Symbols of the live paper account's working orders only (stops, targets, limits waiting for a price).
  QStringList paperOrderSymbols() const;

  // ---- for the tabs ----
  ChartsApp* app;
  // bottom.innerHTML = …: drops the current body content and returns a fresh widget filling the body.
  QWidget* newBody();
  // The body widget made by the last newBody() (async renders check they still own it: JS el.isConnected).
  QWidget* body() const { return body_; }
  bool isOpen(const QString& tab) const { return !collapsed_ && bottomTab_ == tab; }

  bottom::TesterTab* tester = nullptr;
  bottom::ScreenerTab* screener = nullptr;
  bottom::HeatmapTab* heatmap = nullptr;
  bottom::SeasonalsTab* seasonals = nullptr;
  bottom::FinancialsTab* financials = nullptr;
  bottom::PaperTab* paper = nullptr;
  bottom::EditorTab* editor = nullptr;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent*) override;    // background + border-top
  bool eventFilter(QObject* o, QEvent* e) override;
  bool event(QEvent* e) override;

private:
  void applyHeight();
  int expandedHeight() const;

  style::TabBar* tabs_ = nullptr;
  QPushButton* toggle_ = nullptr;
  QWidget* bodyHost_ = nullptr;
  QVBoxLayout* bodyLay_ = nullptr;
  QPointer<QWidget> body_;
  QPointer<QWidget> watched_;
  QString bottomTab_ = "tester";
  bool collapsed_ = true;
};

}  // namespace charts::ui
