#pragma once
// Charts tab — bottom panel, Strategy tester. Port of quant-terminal public/js/panels.js (tState, strategyToUi,
// readTester, renderTester, runTester, showResult) (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
// No "Send to PTQ" box under the result (user's choice).
#include "charts/core/Backtest.h"
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <cmath>
#include <optional>

class QComboBox;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;

namespace charts::ui {
class BottomPanel;
class ChartPane;
namespace rules { class RuleList; }

namespace bottom {

// What the quant checks need from a result (JS lastResult: { ...backtest, symbol, tf, bars, strat, subBars }).
struct TestResult {
  bt::Result r;
  QString symbol, tf;
  Bars bars, subBars;
  QJsonObject strat;
  QStringList indicatorOrder;   // keys of strat.indicators in the JS insertion order
  QString dataNote;             // where the bars came from when not the chart's (Data › Technical data), shown with the result
  int runId = 0;                // set by showRun, one per result: the quant checks box finds its run again after a rebuild
  QString config, label;        // a Pine run's fingerprint and name (rules: from strat); recordTrial sets config for both
  QString family;               // its family in the trial registry (empty: not written there)
  // The trial registry (src/trials) after this run was written there: distinct strategies tried on this market and
  // timeframe, by the user and by the agents, and the best one's deflated Sharpe (tried = 0: not written)
  int tried = 0, triedUser = 0, triedAgents = 0;
  double bestDsr = NAN;
  bool bestIsThis = false;
};

class TesterTab : public QObject {
  Q_OBJECT
public:
  explicit TesterTab(BottomPanel* panel);
  void render();                                   // renderTester
  void strategyToUi(const QJsonObject& s);
  // readTester: { ...compileRules(groups), ...settings }. order (optional) gets the indicator keys in JS order.
  QJsonObject readTester(QStringList* indicatorOrder = nullptr);
  void runTester();
  // The strategy JSON as the JS JSON.stringify(readTester(), null, 2) writes it (same key order).
  QString strategyJsonText();
  // Picks an entry of the strategy list ("ex:<name>", "sv:<name>", "pine:<file>"), as the list does (UI test steps)
  void pick(const QString& value) { loadPreset(value); }

private:
  void showResult();
  void loadPreset(const QString& value);
  void applyLoaded(const QJsonObject& s);          // strategyToUi + the money settings (preset, JSON)
  void importFile(const QString& kind);            // "bars" | "signal"
  void liveSpread();
  void saveStrategy();
  void editJson();
  // symbol / tf: the market the bars belong to, taken when the run started (the chart may switch meanwhile)
  void finishRun(QPointer<ChartPane> pane, const Bars& bars, const QJsonObject& strat, const QStringList& order,
                 const Bars& subBars, const QString& symbol, const QString& tf);
  // Pine strategy() files of the custom folder (charts/core/PineStrategy): picked in the strategy list, run by
  // PineTS's broker emulator instead of the rules below
  // symbol / tf: the bars' market. file / source empty: the Pine strategy picked now, read now; runStored passes the
  // ones it started with, so the run tests the file as it was when Run was pressed.
  void runPine(ChartPane* p, const Bars& bars, const QString& symbol, const QString& tf, const QString& dataNote = {},
               const QString& file = {}, const QString& source = {});
  // The active chart's market and timeframe are stored in Data › Technical data with more bars than the chart holds: the
  // test runs on that history (newest STORED_RULES / STORED_PINE bars), the magnifier on its lower-timeframe bars.
  void runStored(ChartPane* p, const QString& market, int tfSeconds);
  struct StoredSource { QString market; int tf = 0; qint64 count = 0, first = 0; };   // market empty: the chart's bars
  StoredSource storedSource(ChartPane* p) const;
  bool useStored() const;
  bool storedRunning_ = false;
  // Bumped when a run starts: an older run that answers later (magnifier bars, stored history, Pine) is dropped
  // instead of replacing the newer result.
  int runGen_ = 0;
  QPointer<ChartPane> marksPane_;   // the chart the last result's trades were marked on ("Clear chart marks")
public:
  // The "Bars" choice above Run: the stored history (Data › Technical data) or the chart's own bars; follows the active
  // chart (BottomPanel calls it when a chart loads or replay starts / stops). Saved in the Charts settings.
  void refreshBarsSource();
private:
  QPointer<QComboBox> barsSel_;
  void showRun(ChartPane* p, const TestResult& tr);   // keeps the result, marks the trades on the chart, shows it
  void recordTrial(ChartPane* p, TestResult& tr);     // writes the run in the trial registry, sets tr.tried…
  QString pineFile_;   // the Pine strategy picked ("" = the rules)
  bool pineRunning_ = false;

  BottomPanel* P;
  struct {
    QJsonObject groups;            // entryLong, exitLong, entryShort, exitShort: UI rules
    QJsonObject settings;
    QString name;
  } tState;
  std::optional<TestResult> lastResult;

  // widgets of the current render
  QPointer<QWidget> body_;
  QPointer<QWidget> out_;          // #tOut
  QPointer<QScrollArea> outArea_;
  QVBoxLayout* outLay_ = nullptr;
  QHash<QString, QPointer<rules::RuleList>> lists_;
  QHash<QString, QPointer<QWidget>> inputs_;   // data-s="<key>": QLineEdit or QComboBox
};

// The settings keys in tState order (JSON key order).
const QStringList& settingsOrder();

}  // namespace bottom
}  // namespace charts::ui
