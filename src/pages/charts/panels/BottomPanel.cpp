// Charts tab — bottom panel shell (tabs, collapse, hooks). Port of quant-terminal public/js/panels.js
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Icons.h"
#include "pages/charts/Panels.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/bottom/Common.h"
#include "pages/charts/panels/bottom/Editor.h"
#include "pages/charts/panels/bottom/Financials.h"
#include "pages/charts/panels/bottom/Heatmap.h"
#include "pages/charts/panels/bottom/Paper.h"
#include "pages/charts/panels/bottom/Screener.h"
#include "pages/charts/panels/bottom/Seasonals.h"
#include "pages/charts/panels/bottom/Tester.h"
#include <QCoreApplication>
#include <QEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace charts::ui {

static const char* kChevronDown = R"(<path d="M18 9l-6 6-6-6" />)";   // the chevron turned 180° (collapsed)


BottomPanel::BottomPanel(ChartsApp* a, QWidget* parent) : QWidget(parent), app(a) {
  setAttribute(Qt::WA_StyledBackground, false);
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 1, 0, 0);   // border-top
  lay->setSpacing(0);
  tabs_ = new style::TabBar(this);
  tabs_->addTab("tester", QCoreApplication::translate("charts", "Strategy tester"));
  tabs_->addTab("screener", QCoreApplication::translate("charts", "Screener"));
  tabs_->addTab("heatmap", QCoreApplication::translate("charts", "Heatmap"));
  tabs_->addTab("seasonals", QCoreApplication::translate("charts", "Seasonals"));
  tabs_->addTab("financials", QCoreApplication::translate("charts", "Financials"));
  tabs_->addTab("paper", QCoreApplication::translate("charts", "Paper trading"));
  tabs_->addTab("editor", QCoreApplication::translate("charts", "Indicator editor"));
  tabs_->addStretch();
  toggle_ = tabs_->addIcon(icons::kChevronUp, QCoreApplication::translate("charts", "Show / hide panel"));
  tabs_->setCurrent("tester");
  lay->addWidget(tabs_);
  bodyHost_ = new QWidget(this);
  bodyLay_ = new QVBoxLayout(bodyHost_);
  bodyLay_->setContentsMargins(0, 0, 0, 0);
  bodyLay_->setSpacing(0);
  lay->addWidget(bodyHost_, 1);

  tester = new bottom::TesterTab(this);
  screener = new bottom::ScreenerTab(this);
  heatmap = new bottom::HeatmapTab(this);
  seasonals = new bottom::SeasonalsTab(this);
  financials = new bottom::FinancialsTab(this);
  paper = new bottom::PaperTab(this);
  editor = new bottom::EditorTab(this);

  connect(tabs_, &style::TabBar::tabClicked, this, [this](const QString& t) { showBottom(t, true); });
  connect(toggle_, &QPushButton::clicked, this, [this] {
    setCollapsed(!collapsed_);
    if (!collapsed_) renderBottom();
  });
  if (app) connect(app, &ChartsApp::priceChanged, this, [this](const QString& s, double px) { paper->onPrice(s, px); });
  setCollapsed(true);
}

BottomPanel::~BottomPanel() = default;   // the tabs are QObject children

void BottomPanel::setCollapsed(bool on) {
  collapsed_ = on;
  bodyHost_->setVisible(!on);
  style::setIconSvg(toggle_, on ? QString(kChevronDown) : QString(icons::kChevronUp), 18);
  applyHeight();
}

int BottomPanel::expandedHeight() const {
  // @media (max-width: 900px) { .bottom { height: 45% } } — 45 % of the centre column.
  const QWidget* w = window();
  if (w && w->width() <= 900 && parentWidget()) return int(parentWidget()->height() * 0.45);
  return 320;
}

void BottomPanel::applyHeight() { setFixedHeight(collapsed_ ? 39 : expandedHeight()); }

QSize BottomPanel::sizeHint() const { return {800, collapsed_ ? 39 : expandedHeight()}; }
QSize BottomPanel::minimumSizeHint() const { return {0, collapsed_ ? 39 : expandedHeight()}; }

bool BottomPanel::event(QEvent* e) {
  if (e->type() == QEvent::ParentChange || e->type() == QEvent::Show) {
    if (watched_) watched_->removeEventFilter(this);
    watched_ = parentWidget();
    if (watched_) watched_->installEventFilter(this);
    if (window() && window() != this) window()->installEventFilter(this);
    applyHeight();
  }
  return QWidget::event(e);
}

bool BottomPanel::eventFilter(QObject* o, QEvent* e) {
  if (e->type() == QEvent::Resize && (o == watched_ || o == window())) {
    const int before = height();
    applyHeight();
    if (before != height() && !collapsed_ && bottomTab_ == "heatmap") heatmap->draw();
  }
  return QWidget::eventFilter(o, e);
}

void BottomPanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), style::v().bg);
  p.fillRect(QRect(0, 0, width(), 1), style::v().border);
}

QWidget* BottomPanel::newBody() {
  if (body_) {
    body_->hide();
    body_->deleteLater();
  }
  auto* w = new QWidget(bodyHost_);
  bodyLay_->addWidget(w);
  body_ = w;
  return w;
}

void BottomPanel::showBottom(const QString& t, bool toggle) {
  if (toggle && t == bottomTab_ && !collapsed_) {
    setCollapsed(true);
    return;
  }
  bottomTab_ = t;
  setCollapsed(false);
  tabs_->setCurrent(t);
  renderBottom();
}

void BottomPanel::renderBottom() {
  if (collapsed_) return;
  if (bottomTab_ == "tester") tester->render();
  else if (bottomTab_ == "screener") screener->render();
  else if (bottomTab_ == "heatmap") heatmap->render();
  else if (bottomTab_ == "seasonals") seasonals->render();
  else if (bottomTab_ == "financials") financials->render();
  else if (bottomTab_ == "paper") paper->render();
  else if (bottomTab_ == "editor") editor->render();
}

QStringList BottomPanel::paperSymbols() const { return paper->paperSymbols(); }
QStringList BottomPanel::paperOrderSymbols() const { return paper->orderSymbols(); }

void BottomPanel::onActiveChange() {
  // follow every pane's loads, so a result or a price is attributed to the market its bars belong to (Common.h)
  if (app)
    for (ChartPane* p : app->panes) bottom::watchPane(p);
  static QString lastSymbol;   // JS lastSymbol (one panel per page)
  const QString sym = app && app->active ? app->active->cfg.value("symbol").toString() : QString();
  if (sym != lastSymbol) {
    lastSymbol = sym;
    if (!collapsed_ && bottomTab_ == "seasonals") seasonals->render();
    if (!collapsed_ && bottomTab_ == "financials") financials->render();
  }
  if (bottomTab_ == "paper" && !collapsed_) paper->render();
  if (bottomTab_ == "tester" && !collapsed_) tester->refreshBarsSource();
}

void BottomPanel::onTheme() { renderBottom(); }

void BottomPanel::onReplayChange() {
  if (bottomTab_ == "paper" && !collapsed_) paper->render();
  if (bottomTab_ == "tester" && !collapsed_) tester->refreshBarsSource();
}

void BottomPanel::onReplayBar(ChartPane* pane, const QJsonObject& bar) { paper->onReplayBar(pane, bar); }

// ---------- Panels members of the bottom panel (see SidePanel.h for the split) ----------
QWidget* Panels::bottom() const { return bottomPanel; }
void Panels::showBottom(const QString& tab, bool toggle) {
  if (bottomPanel) bottomPanel->showBottom(tab, toggle);
}
void Panels::onTheme() {
  if (bottomPanel) bottomPanel->onTheme();
}
void Panels::onReplayChange() {
  if (bottomPanel) bottomPanel->onReplayChange();
}
void Panels::onReplayBar(ChartPane* pane, const QJsonObject& bar) {
  if (bottomPanel) bottomPanel->onReplayBar(pane, bar);
}

}  // namespace charts::ui

// ---------- screenshot steps: "bt:<what>" (see tests/charts/app main.cpp --eval) ----------
#include <QAbstractButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <cstdio>
namespace charts::ui {
static bool clickButton(QWidget* root, const QString& text) {
  for (auto* b : root->findChildren<QAbstractButton*>())
    if ((b->text() == text || b->text() == QCoreApplication::translate("charts", text.toUtf8().constData())) && b->isVisible()) {
      b->click();
      return true;
    }
  return false;
}
static const bool kBtStep = registerUiStep("bt", [](ChartsApp* app, const QString& arg) {
  if (!app || !app->panels || !app->panels->bottomPanel) return false;
  BottomPanel* b = app->panels->bottomPanel;
  const QString what = arg.section(':', 0, 0), rest = arg.section(':', 1);
  if (what == "run") { b->tester->runTester(); return true; }
  if (what == "pick") { b->tester->pick(rest); return true; }   // bt:pick:pine:my-strategy.pine
  if (what == "bars") {   // bars:stored | bars:chart — the tester's Bars choice
    app->settings["testerStoredBars"] = rest == "stored";
    b->tester->refreshBarsSource();
    return true;
  }
  if (what == "click") {   // click:<button text> (after the queued shows)
    QTimer::singleShot(150, b, [b, rest] {
      if (!clickButton(b, rest)) fprintf(stderr, "bt:click: no visible button \"%s\"\n", qPrintable(rest));
    });
    return true;
  }
  if (what == "scroll") {   // scroll:<index of scroll area>:<pixels> (after the queued shows and layouts)
    QTimer::singleShot(200, b, [b, rest] {
      QList<QScrollArea*> vis;
      for (auto* a : b->findChildren<QScrollArea*>()) if (a->isVisible()) vis << a;
      const int i = rest.section(':', 0, 0).toInt();
      if (i >= 0 && i < vis.size()) vis[i]->verticalScrollBar()->setValue(rest.section(':', 1).toInt());
    });
    return true;
  }
  if (what == "screener") { b->showBottom("screener"); b->screener->setUniverse(rest); return true; }
  if (what == "heatmap") { b->showBottom("heatmap"); b->heatmap->setUniverse(rest); return true; }
  if (what == "height") { b->setFixedHeight(rest.toInt()); return true; }
  return false;
});
}  // namespace charts::ui
