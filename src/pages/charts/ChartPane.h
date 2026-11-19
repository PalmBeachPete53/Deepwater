#pragma once
// Charts tab — one chart: candles + volume + any number of indicators, drawings, compare symbols, replay and
// live data. Port of quant-terminal public/js/chart.js (+ drawings.js, volume-profile.js)
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). Drawn with charts::plot (src/pages/charts/plot).
//
// CONTRACT for the UI agents (page shell, panels): members mirror the JS ChartPane. Add members freely; do not
// change or remove the ones below.
#include "charts/core/Types.h"
#include "charts/core/Indicators.h"
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QImage>
#include <optional>

namespace charts::plot { class Chart; class Series; }

namespace charts::ui {

class ChartsApp;
class DrawingLayer;

// Chart types (JS CHART_TYPES), in menu order: id, label.
const QList<QPair<QString, QString>>& chartTypes();

class ChartPane : public QWidget {
  Q_OBJECT
public:
  ChartPane(ChartsApp* app, const QJsonObject& cfg, QWidget* parent = nullptr);
  ~ChartPane() override;

  // cfg: { symbol, tf, type, indicators: [{ uid, id, params{}, colors{}, tf?, hidden? }], compare: [], log }
  // (JS defaults: BINANCE:BTCUSDT, 1D, candles, [], [], false). The app persists panes as their cfg.
  QJsonObject cfg;
  Bars bars;                         // as loaded (JS this.bars)
  Bars shown;                        // after chart-type transform and replay cut (JS this.shown)
  QVector<qint64> times;             // chart times of `shown` (UTC + local shift for intraday unless settings.utc)
  QHash<QString, ind::Result> indResults;   // by indicator uid
  QVector<ind::Marker> extraMarkers; // markers added by panels (strategy tester trades)
  QString provider;                  // data source name of the last load
  QStringList notes;                 // provider notes of the last load

  struct Replay { int index = 0; bool playing = false; double speed = 2; bool follower = false; };
  std::optional<Replay> replay;

  // ---- data ----
  void load(bool keepView = false);             // async; calls app->onPaneLoaded(this) when done
  void render(bool resetView);
  QString addIndicator(const QString& id, const Params& params = {});   // returns the new uid
  void removeIndicator(const QString& uid);
  QString indicatorParamsLabel(const QJsonObject& ind) const;           // "14 on RSI 14 (1D)"
  void applyTheme();

  // ---- time helpers (JS shift/step/toChart/logicalOf/timeOfLogical/timeToX) ----
  qint64 shift() const;
  qint64 toChart(qint64 utc) const;
  std::optional<double> logicalOf(qint64 utc) const;
  std::optional<qint64> timeOfLogical(double logical) const;
  std::optional<double> timeToX(qint64 utc) const;
  void setCrosshairAtLogical(double logical);   // used by app->syncCrosshair
  void clearCrosshair();

  // ---- drawings ----
  // Items: [{ id, type, points: [{time, price}], text?, color?, rr? }] (same JSON as the JS, saved per symbol).
  QJsonArray drawingItems() const;
  void setDrawingItems(const QJsonArray& items);
  std::optional<QJsonObject> selectedDrawing() const;
  void clearSelection();
  bool deleteSelectedDrawing();
  void clearDrawings();
  void setInteractive(bool on);                 // false while a drawing tool is active

  // ---- replay ----
  void startReplay(int index);
  void replayTo(qint64 endTime);                // follower: cut at a time (replay across charts)
  void replayStep(int dir);
  void toggleReplay();
  void stopReplay();

  // ---- live ----
  void startLive();
  void stopLive();

  QImage screenshot() const;
  bool isActivePane() const;

  // ======== ADDED by the chart pane agent (additional to the contract above) ========
  // Fractional versions of the time helpers (JS numbers: a drawing point or a synced time may fall between bars).
  std::optional<double> logicalOfF(double utc) const;
  std::optional<double> timeOfLogicalF(double logical) const;
  std::optional<double> timeToXF(double utc) const;
  double step() const;                          // JS get step()
  // Logical index (a position on the chart's time scale) <-> index into shown / times. They differ when a compared
  // symbol on another calendar adds time points of its own; barAtLogical gives the bar in force at that point.
  double logicalOfBar(int i) const;
  int barAtLogical(double logical) const;       // -1 when nothing is shown
  const QVector<qint64>& scalePoints() const;   // the time scale's points (times when the chart has none yet)
  QString formatPrice(double v) const;          // JS formatPrice (fmtPrice)
  ::charts::plot::Chart* chart() const;                 // JS this.chart
  ::charts::plot::Series* mainSeries() const;            // JS this.main
  DrawingLayer* drawings() const;               // JS this.drawings
  void updateLegend(int index = -1);            // -1 = last bar (JS updateLegend(null))
  void showMsg(const QString& text);            // pane message overlay; empty hides it
  void merge(const Bars& newBars);              // live bars (JS merge)
  bool footprintStep(const QString& arg);       // dev step fp:… (ChartPane.cpp, after the nav step)
  std::optional<double> footprintTick() const;
  bool bigTradesStep(const QString& arg);       // dev step big:… (ChartPane.cpp)
  bool heatmapStep(const QString& arg);         // dev step heat:… (ChartPane.cpp)
  bool liqMapStep(const QString& arg);          // dev step liq:… (ChartPane.cpp)
  bool smcStep(const QString& arg);             // dev step smc:… (ChartPane.cpp)  // price of one tick when the footprint's market is known
  void syncActive();                            // repaint the active-pane border after app->active changed

signals:
  void loaded();

protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject* o, QEvent* e) override;

private:
  ChartsApp* app;
  struct Impl;
  Impl* d = nullptr;
};

}  // namespace charts::ui
