// One chart: candles + volume + any number of indicators, drawings, compare symbols, replay and live data.
// Port of quant-terminal public/js/chart.js (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include "pages/charts/ChartPane.h"
#include "charts/core/PineIndicators.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Drawings.h"
#include "pages/charts/Style.h"
#include "pages/charts/VolumeProfile.h"
#include "pages/charts/flow/FootprintFeed.h"
#include "pages/charts/flow/FootprintLayer.h"
#include "pages/charts/flow/BigTradesLayer.h"
#include "pages/charts/flow/BigTradesModel.h"
#include "pages/charts/flow/HeatmapLayer.h"
#include "pages/charts/flow/LiqMapLayer.h"
#include "pages/charts/flow/LiqMapModel.h"
#include "pages/charts/flow/SmcLayer.h"
#include "charts/core/SessionLevels.h"
#include "charts/data/DepthStream.h"
#include "charts/data/ExchangeFeed.h"
#include "pages/charts/pane/Legend.h"
#include "pages/charts/pane/Navigator.h"
#include "pages/charts/pane/ReplayBar.h"
#include "pages/charts/pane/Util.h"
#include "pages/charts/plot/Chart.h"
#include "charts/core/Series.h"
#include "charts/data/LiveFeed.h"
#include "charts/data/Providers.h"
#include "pages/charts/Format.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QToolTip>
#include <algorithm>
#include <cmath>
#include <memory>

namespace charts::ui {

using namespace charts::plot;
using Series = plot::Series;   // charts::Series (QVector<double>) is spelled in full below
namespace sr = charts::series;

namespace {


// Chart types built from price alone, with no meaningful volume per bar.
const QStringList PRICE_ONLY = {"renko", "linebreak", "kagi", "pnf"};
const QColor CLEAR(0, 0, 0, 0);
const QColor kAccent("#2962ff");   // --accent of quant-terminal: the main line / area colour (a chart series colour)
// Fresh view (new symbol / timeframe): the last kFreshBars bars, then kFreshRightPad empty slots (JS n-160 … n+8,
// i.e. 9 slots after the last bar). The navigator's domain ends kFreshRightPad slots after the last bar too.
constexpr double kFreshBars = 160;
constexpr double kFreshRightPad = 9;

bool truthy(const QJsonValue& v) {
  if (v.isBool()) return v.toBool();
  if (v.isDouble()) return v.toDouble() != 0 && !std::isnan(v.toDouble());
  if (v.isString()) return !v.toString().isEmpty();
  return !v.isNull() && !v.isUndefined();
}

double tfSeconds(const QString& tf) {
  try { return sr::parseTf(tf).seconds; } catch (...) { return 86400; }
}
bool intraday(const QString& tf) {
  try { return sr::isIntraday(tf); } catch (...) { return false; }
}

// /\(([^)]+)\)$/.exec(def.name)?.[1].split(" / ")[0] || def.name
QString shortName(const QString& name) {
  static const QRegularExpression re(R"(\(([^)]+)\)$)");
  const auto m = re.match(name);
  if (m.hasMatch()) {
    const QString s = m.captured(1).split(" / ").first();
    if (!s.isEmpty()) return s;
  }
  return name;
}

// A chart line can't skip a blank point: it joins the neighbours. A segment takes the colour of
// the point it starts from, so hiding a gap means making the point before it transparent and
// parking the gap's points (invisibly) at the next real value.
// colors (Pine plots whose colour changes bar by bar): the segment arriving at bar i takes bar i's colour, as on
// TradingView (this renderer paints a point's colour on the segment leaving it, so point i gets bar i + 1's).
QVector<DataPoint> lineData(const charts::Series& vals, const QVector<bool>& breaks,
                            const std::function<Time(int)>& timeAt, int n, const QStringList& colors = {}) {
  QVector<DataPoint> out;
  out.reserve(vals.size());
  QVector<double> next(vals.size());
  double nv = NaN;
  for (int i = int(vals.size()) - 1; i >= 0; --i) {
    if (std::isfinite(vals[i])) nv = vals[i];
    next[i] = nv;
  }
  bool started = false;
  for (int i = 0; i < vals.size(); ++i) {
    const double v = vals[i];
    const Time time = timeAt(i);
    if (!std::isfinite(v)) {
      if (i >= n) continue;
      out.append(started && std::isfinite(next[i]) ? DataPoint::val(time, next[i], CLEAR) : DataPoint::whitespace(time));
      continue;
    }
    started = true;
    const bool gapAhead = i + 1 < vals.size() && (!std::isfinite(vals[i + 1]) || breaks.value(i + 1, false));
    if (gapAhead) out.append(DataPoint::val(time, v, CLEAR));
    else if (!colors.isEmpty() && !colors.value(i + 1).isEmpty()) out.append(DataPoint::val(time, v, cssColor(colors.value(i + 1))));
    else out.append(DataPoint::val(time, v));
  }
  return out;
}

QVector<DataPoint> histData(const charts::Series& vals, const QStringList& colors,
                            const std::function<Time(int)>& timeAt, int n) {
  QVector<DataPoint> out;
  out.reserve(vals.size());
  for (int i = 0; i < vals.size(); ++i) {
    const double v = vals[i];
    if (i >= n && !std::isfinite(v)) continue;
    if (!std::isfinite(v)) out.append(DataPoint::whitespace(timeAt(i)));
    else out.append(colors.isEmpty() ? DataPoint::val(timeAt(i), v) : DataPoint::val(timeAt(i), v, cssColor(colors.value(i))));
  }
  return out;
}

// Higher-timeframe values onto chart bars. A chart bar sees a higher-timeframe bar only once that
// bar has finished, except inside the newest one, where it shows the still-forming value (as
// TradingView does in real time).
ind::Result mapToChart(const ind::Result& res, const Bars& hb, const QString& htf, const Bars& cb, const QString& ctf) {
  const double hSec = tfSeconds(htf), cSec = tfSeconds(ctf);
  const int H = int(hb.size());
  auto end = [&](int j) { return j + 1 < H ? double(hb[j + 1].time) : hb[j].time + hSec; };
  QVector<int> idx(cb.size(), -1);
  int j = -1;
  for (int i = 0; i < cb.size(); ++i) {
    const double cEnd = cb[i].time + cSec;
    while (j + 1 < H && end(j + 1) <= cEnd) ++j;
    idx[i] = j + 1 == H - 1 && cb[i].time >= hb[H - 1].time ? H - 1 : j;
  }
  ind::Result out = res;
  out.markers.clear();
  out.keys.removeAll("markers");
  for (auto it = out.series.begin(); it != out.series.end(); ++it) {
    const charts::Series& v = res.series.value(it.key());
    if (v.size() < H) continue;
    charts::Series m(cb.size());
    for (int i = 0; i < cb.size(); ++i) m[i] = idx[i] >= 0 ? v[idx[i]] : NaN;
    it.value() = m;
  }
  auto mapStr = [&](const QStringList& v) {
    if (v.size() < H) return v;
    QStringList m;
    for (int i = 0; i < cb.size(); ++i) m << (idx[i] >= 0 ? v[idx[i]] : QString());
    return m;
  };
  out.colors = mapStr(res.colors);
  out.barColors = mapStr(res.barColors);
  if (res.breaks.size() >= H) {
    QVector<bool> m(cb.size());
    for (int i = 0; i < cb.size(); ++i) m[i] = idx[i] >= 0 && res.breaks[idx[i]];
    out.breaks = m;
  }
  return out;
}

QJsonObject barJson(const Bar& b) {
  return {{"time", double(b.time)}, {"open", b.open}, {"high", b.high}, {"low", b.low}, {"close", b.close},
          {"volume", std::isnan(b.volume) ? 0 : b.volume}};
}

// The active-pane outline (.pane.active::after: 1 px accent, only when the layout has several panes).
class ActiveFrame : public QWidget {
public:
  ActiveFrame(QWidget* parent, std::function<bool()> on) : QWidget(parent), on_(std::move(on)) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
  }
protected:
  void paintEvent(QPaintEvent*) override {
    if (!on_()) return;
    QPainter p(this);
    p.setPen(QPen(style::v().accent, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, width() - 1, height() - 1));
  }
private:
  std::function<bool()> on_;
};

}  // namespace

const QList<QPair<QString, QString>>& chartTypes() {
  static const QList<QPair<QString, QString>> t = {
      {"candles", QT_TRANSLATE_NOOP("charts", "Candles")},    {"hollow", QT_TRANSLATE_NOOP("charts", "Hollow candles")},  {"bars", QT_TRANSLATE_NOOP("charts", "Bars")},
      {"heikin", QT_TRANSLATE_NOOP("charts", "Heikin Ashi")}, {"line", QT_TRANSLATE_NOOP("charts", "Line")},              {"linemarkers", QT_TRANSLATE_NOOP("charts", "Line with markers")},
      {"step", QT_TRANSLATE_NOOP("charts", "Step line")},     {"area", QT_TRANSLATE_NOOP("charts", "Area")},              {"baseline", QT_TRANSLATE_NOOP("charts", "Baseline")},
      {"columns", QT_TRANSLATE_NOOP("charts", "Columns")},    {"renko", QT_TRANSLATE_NOOP("charts", "Renko")},            {"linebreak", QT_TRANSLATE_NOOP("charts", "Line break")},
      {"range", QT_TRANSLATE_NOOP("charts", "Range bars")},   {"kagi", QT_TRANSLATE_NOOP("charts", "Kagi")},              {"pnf", QT_TRANSLATE_NOOP("charts", "Point & figure")},
      {"footprint", QT_TRANSLATE_NOOP("charts", "Footprint")},
  };
  return t;
}



// ===================================================================================================================

struct ChartPane::Impl {
  ChartPane* q;
  ChartsApp* app;
  QTimer countdownTimer;   // 1 s while shown and the countdown is on: repaints the last-value label's second line
  QColor upC, downC, volUp, volDown;   // candle and volume colours in use (settings › chart style, else the theme)
  Chart* chart = nullptr;
  Series* main = nullptr;
  Series* volume = nullptr;
  SeriesMarkers* markers = nullptr;
  QString mainType;
  std::unique_ptr<DrawingLayer> drawings;
  std::vector<std::unique_ptr<Primitive>> vpPrims;
  QHash<QString, QVector<Series*>> indSeries;
  QVector<QPair<QString, Series*>> compareSeries;
  QHash<QString, Bars> compareData;
  QStringList shownColors;                  // kagi: b.color of every shown point
  QHash<qint64, QString> barColorAt;        // JS this.barColorAt (by bar time)
  struct Mtf { Bars bars; QString error; qint64 at = 0; };
  QHash<QString, Mtf> mtf;
  QSet<QString> mtfLoading;
  int loadToken = 0;
  qint64 utcOffset = 0;                     // local UTC offset (s), read once per load / render (shift() runs per bar)
  QString shiftTf;                          // shift(): intraday(shiftTf), cached (a regex per call otherwise)
  bool shiftKnown = false, shiftIntraday = false;
  qint64 liveStoppedAt = 0;                 // ms; when the live feed was stopped by a hide (0 = not stopped)
  qint64 replayStartedAt = 0;               // ms; when this chart entered replay (live bars were not merged since)
  qint64 gapReloadBar = 0;                  // first live bar after a hole that caused a reload (one reload per hole)
  bool loadFailed = false;                  // the last load failed: retried on a timer and when shown again
  int loadFailures = 0;                     // failed loads in a row (retry backoff)
  // a bar of this chart's timeframe closed between sinceMs and now
  bool barClosedSince(qint64 sinceMs) const {
    if (!sinceMs) return false;
    const QString tf = q->cfg.value("tf").toString();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    try {
      return sr::bucketStart(sinceMs / 1000, tf) != sr::bucketStart(now / 1000, tf);
    } catch (...) {
      return now - sinceMs > qint64(tfSeconds(tf) * 1000);
    }
  }
  data::LiveFeed* live = nullptr;
  qint64 mtfCheckedAt = 0, lastFull = 0;
  QTimer replayTimer;
  pane::Legend* legend = nullptr;
  pane::Navigator* nav = nullptr;           // range slider under the chart
  QLabel* msg = nullptr;
  ActiveFrame* frame = nullptr;
  QPointer<pane::ReplayBar> replayBar;
  bool legendPress = false;
  QString pressAct;
  int pressRow = -1;
  bool nativePress = false;
  // footprint (chart type "footprint", Binance markets): trades folded into this pane's candles, drawn over them
  std::unique_ptr<flow::FootprintFeed> fp;
  std::unique_ptr<flow::FootprintLayer> fpLayer;
  // Big Trades indicators: one model (trades → orders → bubbles) and one layer per indicator uid; models outlive
  // renders (the history is computed once), layers are attached to the main series at each render.
  struct BtEntry {
    std::unique_ptr<flow::BigTradesModel> model;
    std::unique_ptr<flow::BigTradesLayer> layer;
    bool alertVisual = false, alertSound = false;
    qint64 lastAlertMs = 0;   // at most one alert a second; the others are counted into the next one
    int heldBack = 0;
  };
  std::map<QString, BtEntry> bt;
  // Liquidity heatmaps: one layer per indicator uid on the market's shared depth stream (one stream per market)
  std::map<QString, std::unique_ptr<flow::HeatmapLayer>> heat;
  void detachHeatmaps() {
    for (auto& [uid, l] : heat)
      if (l && l->series()) l->series()->detachPrimitive(l.get());
  }
  QSet<QString> heatRecords;   // heatmaps (uid) whose settings ask their stream to record
  QSet<QString> hiddenKept;    // indicators hidden with the eye whose model is kept (no status in the legend, as before)
  // Hidden pages pause (the rule of every Deepwater page): once the pane has been hidden for kFlowPauseMs, the
  // footprint feed, the Big Trades without an alert and the heatmaps that do not record are dropped, so their trade
  // and depth sockets close when no other pane uses the market. What the user asked to keep running stays: a Big
  // Trades alert, a recording heatmap. Showing the pane renders again, which rebuilds them (syncFootprint, syncBigTrades,
  // syncHeatmap). The delay: a rebuilt trade stream fetches its whole history again (up to 300 000 trades, hundreds of
  // REST pages), which a quick look at another tab must not cost.
  static constexpr int kFlowPauseMs = 30000;
  QTimer flowPauseTimer;
  bool flowPaused = false;
  void pauseFlow() {
    flowPaused = true;
    if (fpLayer && fpLayer->series()) fpLayer->series()->detachPrimitive(fpLayer.get());
    fpLayer.reset();
    fp.reset();
    for (auto it = bt.begin(); it != bt.end();) {
      BtEntry& e = it->second;
      if (e.alertVisual || e.alertSound) { ++it; continue; }
      if (e.layer && e.layer->series()) e.layer->series()->detachPrimitive(e.layer.get());
      it = bt.erase(it);
    }
    for (auto it = heat.begin(); it != heat.end();) {
      if (heatRecords.contains(it->first)) { ++it; continue; }
      if (it->second && it->second->series()) it->second->series()->detachPrimitive(it->second.get());
      it = heat.erase(it);
    }
  }
  // Liquidation heatmaps: model (worker computation) and layer per indicator uid
  struct LiqEntry {
    std::unique_ptr<flow::LiqMapModel> model;
    std::unique_ptr<flow::LiqMapLayer> layer;
  };
  std::map<QString, LiqEntry> liq;
  QHash<QString, QString> liqLegendShown;
  void detachLiqMaps() {
    for (auto& [uid, e] : liq)
      if (e.layer && e.layer->series()) e.layer->series()->detachPrimitive(e.layer.get());
  }
  // "loading open interest…", "estimate · open interest on 1 430 of 2 000 bars"
  QString liqStatus(const QString& uid) {
    const auto it = liq.find(uid);
    if (it == liq.end() || !it->second.model || hiddenKept.contains(uid)) return {};
    auto tr = [](const char* t) { return QCoreApplication::translate("charts", t); };
    const auto& m = *it->second.model;
    const auto res = m.result();
    const auto& st = m.stats()->status();
    if (!res) return m.stats()->openInterest().empty() ? (st.error.isEmpty() ? tr("loading open interest…") : st.error) : tr("computing…");
    if (res->oiKnown == 0) return tr("no open interest for these bars (Binance keeps 30 days)");
    QString s = tr("estimate");
    if (res->oiKnown < res->model.bars())
      s += QStringLiteral(" · ") + tr("open interest on %1 of %2 bars").arg(res->oiKnown).arg(res->model.bars());
    if (const auto& c = res->calibration) {
      if (!c->ok) s += QStringLiteral(" · ") + tr("calibration needs 30 real liquidations in these bars (%1 so far): default mix").arg(c->eventsInRange);
      else {
        QStringList mix;
        for (const auto& l : c->best) mix << QString("%1×%2").arg(l.leverage).arg(l.weight < 1 ? QString(" %1 %").arg(qRound(l.weight * 100)) : QString());
        // the honest figure: the last third of the liquidations, which the choice never saw
        // rank of the later liquidations among the rows their bars crossed: 0.5 = chance
        s += QStringLiteral(" · ") + tr("calibrated (%1): later liquidations ranked %2 (0.5 = chance), default %3")
                                         .arg(mix.join(", "))
                                         .arg(c->rankTestBest, 0, 'f', 2)
                                         .arg(c->rankTestDefault, 0, 'f', 2);
      }
    }
    return s;
  }
  void syncLiqMap(const QString& uid, const QJsonObject& in, ind::Result& res) {
    std::optional<data::FlowMarket> m;
    if (q->provider == "Binance" || q->provider == "Binance futures") m = data::FlowMarket::fromChartSymbol(q->cfg.value("symbol").toString());
    if (!m || !q->cfg.value("compare").toArray().isEmpty()) {
      liq.erase(uid);
      res.error = QT_TRANSLATE_NOOP("charts", "The liquidation heatmap needs a Binance crypto chart (it reads Binance USD-M statistics)");
      return;
    }
    LiqEntry& e = liq[uid];
    if (!e.model || e.model->symbol() != m->symbol) {
      e.layer.reset();
      e.model = std::make_unique<flow::LiqMapModel>(m->symbol);
      e.layer = std::make_unique<flow::LiqMapLayer>(q, e.model.get());
      flow::LiqMapLayer* layer = e.layer.get();
      QObject::connect(e.model.get(), &flow::LiqMapModel::changed, e.model.get(), [this, layer] {
        layer->requestRepaint();
        if (!legendTimer.isActive()) legendTimer.start(1000);
      });
      // new real liquidations (dots), the statistics' state (legend)
      QObject::connect(e.model->stats(), &data::FuturesStats::changed, e.model.get(), [layer] { layer->requestRepaint(); });
      QObject::connect(e.model->stats(), &data::FuturesStats::statusChanged, e.model.get(), [this] {
        if (!legendTimer.isActive()) legendTimer.start(1000);
      });
    }
    const int64_t barMs = int64_t(tfSeconds(q->cfg.value("tf").toString()) * 1000);
    flow::LiqMapModel::Config c;
    const QString mix = in.value("leverage").toString();
    if (mix.startsWith("High")) c.params.leverages = {{25, 0.25}, {50, 0.35}, {75, 0.20}, {100, 0.12}, {125, 0.08}};
    else if (mix.startsWith("Low")) c.params.leverages = {{2, 0.30}, {3, 0.25}, {5, 0.25}, {10, 0.15}, {20, 0.05}};
    c.calibrate = mix.startsWith("Calibrated");
    c.params.positionNotional = std::max(10.0, in.value("notional").toDouble(10000));
    c.params.brackets = ::charts::flow::binanceBrackets(m->symbol).brackets;
    c.params.candleBias = std::clamp(in.value("bias").toDouble(), -1.0, 1.0);
    const double hours = std::max(0.0, in.value("halfLife").toDouble());
    c.params.decayHalfLifeBars = hours > 0 && barMs > 0 ? hours * 3600000.0 / double(barMs) : 0;
    c.params.consume = std::clamp(in.value("consume").toDouble(100), 0.0, 100.0) / 100;
    c.rows = int(std::clamp(in.value("rows").toDouble(800), 100.0, 2000.0));
    c.useRatio = in.value("ratio").toString() != "50/50";
    e.model->setConfig(c);
    // the closed bars it may use: in replay, nothing after the replayed bar
    // (the cut is on the loaded bars: shown.size() differs from it on Renko, Kagi, P&F, line break and range charts)
    e.model->setBars(q->replay ? q->bars.mid(0, q->replay->index + 1) : q->bars, barMs);
    flow::LiqMapLayer::Look look;
    const QString pal = in.value("palette").toString();
    look.palette = pal == "Deep" ? 0 : pal == "Mono" ? 2 : 1;
    look.lowPct = std::clamp(in.value("low").toDouble(60), 0.0, 99.0);
    look.highPct = std::clamp(in.value("high").toDouble(99.5), look.lowPct + 0.1, 100.0);
    look.opacity = std::clamp(in.value("opacity").toDouble(85), 5.0, 100.0) / 100;
    const QString side = in.value("side").toString();
    look.longs = side != "Shorts";
    look.shorts = side != "Longs";
    look.real = in.value("real").toString() != "Off";
    e.layer->setLook(look);
    if (main && e.layer->series() != main) main->attachPrimitive(e.layer.get());
  }
  // Structural liquidity (SMC): one layer per indicator uid, computed on the GUI thread from the chart's bars
  std::map<QString, std::unique_ptr<flow::SmcLayer>> smc;
  void detachSmc() {
    for (auto& [uid, l] : smc)
      if (l && l->series()) l->series()->detachPrimitive(l.get());
  }
  void syncSmc(const QString& uid, const QJsonObject& in, const QJsonObject& colors) {
    // missing inputs (an indicator saved before one was added): the definition's default
    const QJsonObject defs = ind::defaults("smcliq");
    auto val = [&](const char* k) { const QJsonValue v = in.value(k); return v.isUndefined() || v.isNull() ? defs.value(k) : v; };
    auto& l = smc[uid];
    if (!l) l = std::make_unique<flow::SmcLayer>(q);
    flow::SmcLayer::Config c;
    auto& e = c.engine;
    const QString method = val("method").toString();
    e.swing.method = method == "Percent" ? ::charts::smc::PivotMethod::Percent : method == "ATR" ? ::charts::smc::PivotMethod::Atr : ::charts::smc::PivotMethod::Window;
    e.swing.left = e.swing.right = int(std::clamp(val("length").toDouble(5), 1.0, 100.0));
    e.swing.percent = std::clamp(val("percent").toDouble(0.5), 0.01, 50.0);
    e.swing.atrMult = std::clamp(val("atrMult").toDouble(1.5), 0.1, 20.0);
    e.toleranceUnit = val("tolUnit").toString() == "Percent" ? ::charts::smc::SizeUnit::Percent : ::charts::smc::SizeUnit::Atr;
    e.tolerance = std::max(0.0, val("tolerance").toDouble(0.1));
    const QString tu = val("thickUnit").toString();
    e.thicknessUnit = tu == "Percent" ? ::charts::smc::SizeUnit::Percent : tu == "Ticks" ? ::charts::smc::SizeUnit::Ticks : ::charts::smc::SizeUnit::Atr;
    e.thickness = std::max(0.0, val("thickness").toDouble(0.1));
    // a tick from the prices' decimals (the chart has no tick size of its own)
    int dec = 0;
    for (int i = std::max<qsizetype>(0, q->bars.size() - 50); i < q->bars.size(); ++i) {
      const QString s = QString::number(q->bars[i].close, 'f', 8);
      int d = 8;
      while (d > 0 && s.at(s.size() - 1 - (8 - d)) == '0') --d;
      dec = std::max(dec, d);
    }
    e.tick = std::pow(10.0, -dec);
    e.sweep = val("sweep").toString() == "Close" ? ::charts::smc::SweepRule::Close : ::charts::smc::SweepRule::Wick;
    const bool periods = val("periods").toString() != "Off";
    const QTimeZone ny("America/New_York");
    if (val("dayStart").toString().startsWith("17")) {
      e.periods.tz = ny;
      e.periods.dayStartMinutes = 17 * 60;
    }
    e.periods.day = e.periods.week = periods;
    const QString asia = val("asia").toString(), london = val("london").toString(), nyS = val("newYork").toString();
    if (asia.startsWith("Tokyo")) e.periods.sessions.push_back({::charts::smc::LevelKind::Asia, QTimeZone("Asia/Tokyo"), QTime(9, 0), QTime(15, 0)});
    else if (asia.startsWith("New York")) e.periods.sessions.push_back({::charts::smc::LevelKind::Asia, ny, QTime(20, 0), QTime(0, 0)});
    if (london.startsWith("London")) e.periods.sessions.push_back({::charts::smc::LevelKind::London, QTimeZone("Europe/London"), QTime(8, 0), QTime(16, 30)});
    else if (london.startsWith("New York")) e.periods.sessions.push_back({::charts::smc::LevelKind::London, ny, QTime(2, 0), QTime(5, 0)});
    if (nyS.startsWith("New York 09")) e.periods.sessions.push_back({::charts::smc::LevelKind::NewYork, ny, QTime(9, 30), QTime(16, 0)});
    else if (nyS.startsWith("New York 07")) e.periods.sessions.push_back({::charts::smc::LevelKind::NewYork, ny, QTime(7, 0), QTime(10, 0)});
    e.periodLevels = periods || !e.periods.sessions.empty();
    c.chartTfSeconds = int(tfSeconds(q->cfg.value("tf").toString()));
    const QString dtf = val("detectTf").toString();
    c.detectTf = dtf == "Chart" || dtf.isEmpty() ? QString() : dtf;
    l->setConfig(c);
    // in replay, nothing after the replayed bar (cut on the loaded bars, not on shown: Renko etc. have their own count)
    l->setBars(q->replay ? q->bars.mid(0, q->replay->index + 1) : q->bars);
    flow::SmcLayer::Look look;
    look.zones = val("style").toString() != "Lines";
    look.labels = val("labels").toString() != "Off";
    look.keepAfterSweep = int(std::clamp(val("keep").toDouble(20), 0.0, 10000.0));
    look.minScore = val("minScore").toDouble(1);
    look.buy = cssColor(colors.value("buy").toString());
    look.sell = cssColor(colors.value("sell").toString());
    l->setLook(look);
    if (main && l->series() != main) main->attachPrimitive(l.get());
  }
  QHash<QString, QString> heatLegendShown;
  // "connecting…", "syncing the order book…", "recording": the heatmap's book right now
  QString heatmapStatus(const QString& uid) {
    const auto it = heat.find(uid);
    if (it == heat.end() || !it->second || hiddenKept.contains(uid)) return {};
    const data::DepthStream& s = *it->second->stream();
    const auto& st = s.status();
    auto tr = [](const char* t) { return QCoreApplication::translate("charts", t); };
    if (!s.ready() || st.feed != data::FeedState::Open) return st.error.isEmpty() ? tr("connecting…") : tr("reconnecting…");
    if (!st.synced) return tr("syncing the order book…");
    return s.recording() ? tr("recording") : QString();   // the book's market, when not the chart's, is in the label
  }
  void syncHeatmap(const QString& uid, const QJsonObject& in, ind::Result& res) {
    std::optional<data::FlowMarket> m;
    if (q->provider == "Binance" || q->provider == "Binance futures") m = data::FlowMarket::fromChartSymbol(q->cfg.value("symbol").toString());
    const QString book = in.value("book").toString();
    if (m && book == "Spot") m->venue = data::FlowMarket::BinanceSpot;
    if (m && book == "USD-M perpetual") m->venue = data::FlowMarket::BinanceUsdm;
    if (!m || !q->cfg.value("compare").toArray().isEmpty()) {
      heat.erase(uid);
      res.error = QT_TRANSLATE_NOOP("charts", "The heatmap needs a Binance order book (spot or USD-M perpetual)");
      return;
    }
    const bool records = in.value("record").toString() != "Off";
    if (records) heatRecords.insert(uid);
    else heatRecords.remove(uid);
    if (flowPaused && !records) {   // hidden pane: rebuilt when shown (pauseFlow)
      heat.erase(uid);
      return;
    }
    auto& l = heat[uid];
    if (!l || !(l->stream()->market() == *m)) {
      auto stream = data::DepthStream::acquire(*m);
      l = std::make_unique<flow::HeatmapLayer>(q, stream);
      flow::HeatmapLayer* layer = l.get();
      QObject::connect(stream.get(), &data::DepthStream::appended, layer->context(), [layer] { layer->requestRepaint(); });
      // the book's state in the legend (at most once a second)
      QObject::connect(stream.get(), &data::DepthStream::statusChanged, layer->context(), [this] {
        if (!legendTimer.isActive()) legendTimer.start(1000);
      });
    }
    flow::HeatmapLayer::Look look;
    const QString pal = in.value("palette").toString();
    look.palette = pal == "Thermal" ? 1 : pal == "Mono" ? 2 : 0;
    look.linear = in.value("scale").toString() == "Linear";
    look.lowPct = std::clamp(in.value("low").toDouble(50), 0.0, 99.0);
    look.highPct = std::clamp(in.value("high").toDouble(99.5), look.lowPct + 0.1, 100.0);
    look.minQty = std::max(0.0, in.value("minQty").toDouble());
    look.maxQty = std::max(0.0, in.value("maxQty").toDouble());
    if (look.maxQty > 0 && look.maxQty <= look.minQty) look.maxQty = 0;   // a top under the bottom: automatic top
    look.gamma = std::clamp(in.value("gamma").toDouble(1.4), 0.2, 5.0);
    look.opacity = std::clamp(in.value("opacity").toDouble(100), 5.0, 100.0) / 100;
    look.step = std::max(0.0, in.value("step").toDouble());
    look.sum = in.value("combine").toString() == "Sum";
    look.lines = in.value("lines").toString() != "Off";
    l->setLook(look);
    // this layer's own wish (owner = its context, gone with the layer): the shared stream records if any heatmap on the
    // market asks and keeps the longest retention, so one pane's settings never delete another's recordings
    l->stream()->record(in.value("record").toString() != "Off", int(std::clamp(in.value("keepDays").toDouble(7), 1.0, 3650.0)),
                        l->context());
    if (main && l->series() != main) main->attachPrimitive(l.get());
  }
  void onBigTradeAlert(const QString& uid, const ::charts::flow::AggregatedTrade& a, double threshold) {
    const auto it = bt.find(uid);
    if (it == bt.end() || !it->second.model) return;
    BtEntry& e = it->second;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - e.lastAlertMs < 1000) { ++e.heldBack; return; }
    e.lastAlertMs = now;
    const auto& tk = e.model->ticks();
    const bool buy = a.side == ::charts::flow::Side::Buy;
    QString text = QCoreApplication::translate("charts", "%1 · %2 %3 @ %4 (%5× the threshold)")
                       .arg(pane::ticker(q->cfg.value("symbol").toString()),
                            buy ? QCoreApplication::translate("charts", "aggressive buy") : QCoreApplication::translate("charts", "aggressive sell"),
                            flow::cellVolume(a.volume), pane::fmtPrice(a.vwapTicks * tk.step), pane::fmtNum(a.volume / threshold, 1));
    if (e.heldBack) text += QCoreApplication::translate("charts", " · +%1 more").arg(e.heldBack);
    e.heldBack = 0;
    app->bigTradeAlert(text, e.alertVisual, e.alertSound);
  }
  // Big Trades tooltip: the order under the mouse (exact time, side, volume, average / min / max price, executions)
  bool btTipShown = false;
  void bigTradesHover(const MouseEventParams& p) {
    const flow::BigTradesModel* model = nullptr;
    long found = -1;
    if (p.point && p.paneIndex == 0)
      for (const auto& [uid, e] : bt)
        if (e.layer && e.model)
          if (const long k = e.layer->bubbleAt(*p.point); k >= 0) { found = k; model = e.model.get(); }
    if (found < 0 || !model || size_t(found) >= model->bubbles().size()) {
      if (btTipShown) QToolTip::hideText();
      btTipShown = false;
      return;
    }
    const auto& b = model->bubbles()[size_t(found)];
    const auto& tk = model->ticks();
    auto tr = [](const char* s) { return QCoreApplication::translate("charts", s); };
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(b.a.startMs);
    const QString when = t.date() == QDate::currentDate() ? t.toString("HH:mm:ss.zzz") : QLocale(QLocale::English).toString(t, "d MMM HH:mm:ss.zzz");
    const bool buy = b.a.side == ::charts::flow::Side::Buy;
    const QColor col = buy ? style::v().up : b.a.side == ::charts::flow::Side::Sell ? style::v().down : style::v().muted;
    QString html = QString("<b style='color:%1'>%2</b><br>").arg(col.name(), buy ? tr(QT_TRANSLATE_NOOP("charts", "Aggressive buy"))
                                                                   : b.a.side == ::charts::flow::Side::Sell ? tr(QT_TRANSLATE_NOOP("charts", "Aggressive sell"))
                                                                                                        : tr(QT_TRANSLATE_NOOP("charts", "Unknown side")));
    auto row = [&](const QString& k, const QString& v) { html += QString("%1&nbsp;&nbsp;<b>%2</b><br>").arg(k, v); };
    row(tr(QT_TRANSLATE_NOOP("charts", "Time")), when);
    row(tr(QT_TRANSLATE_NOOP("charts", "Volume")), QString::number(b.a.volume, 'f', 6).remove(QRegularExpression("\\.?0+$")));
    row(tr(QT_TRANSLATE_NOOP("charts", "Average price")), pane::fmtPrice(b.a.vwapTicks * tk.step));
    if (b.a.minTicks != b.a.maxTicks)
      row(tr(QT_TRANSLATE_NOOP("charts", "Prices")), pane::fmtPrice(tk.priceOf(b.a.minTicks)) + QStringLiteral(" – ") + pane::fmtPrice(tk.priceOf(b.a.maxTicks)));
    row(tr(QT_TRANSLATE_NOOP("charts", "Executions")), QString::number(b.a.executions));
    if (b.a.approximate) html += QString("<i>%1</i>").arg(tr(QT_TRANSLATE_NOOP("charts", "side estimated (tick rule)")));
    if (html.endsWith("<br>")) html.chop(4);
    const QRect pr = chart->panes().value(0) ? chart->panes()[0]->rect() : QRect();
    QToolTip::showText(chart->mapToGlobal(pr.topLeft() + p.point->toPoint() + QPoint(14, 14)), html, chart);
    btTipShown = true;
  }
  QTimer legendTimer;          // Big Trades thresholds: legend refresh, at most once a second
  QHash<QString, QString> btLegendShown;
  int hoverIndex = -1;         // bar under the crosshair (-1: none), so a refresh keeps what the legend shows
  QVector<QString> paneOwners;  // per chart pane: uid of the indicator that opened it ("" = the price pane)
  // "≥ 0.70 (p99 0.63 × 1.12)", "warming up 37/100", "loading trades…": what a Big Trades filters right now
  QString bigTradesStatus(const QString& uid) {
    const auto it = bt.find(uid);
    if (it == bt.end() || !it->second.model || hiddenKept.contains(uid)) return {};
    auto& m = *it->second.model;
    if (!m.ready() || (m.computing() && m.bubbles().empty())) return QCoreApplication::translate("charts", "loading trades…");
    const auto v = m.current();
    const auto& p = m.config().det;
    auto num = [](double x) { return flow::cellVolume(x); };   // 3 significant digits: "0.500", "12.3", "1.20K"
    if (p.mode == ::charts::flow::DetectorParams::Mode::Manual)
      return p.minVolume > 0 ? QString() : QCoreApplication::translate("charts", "set a minimum volume");
    if (!v.warm) {
      QString s = QCoreApplication::translate("charts", "warming up %1/%2").arg(m.samples()).arg(p.minSamples);
      if (std::isfinite(v.threshold)) s += QStringLiteral(" · ≥ ") + num(v.threshold);
      return s;
    }
    QString s = QStringLiteral("> ") + num(v.threshold) + QStringLiteral(" (p") + ind::numberToString(p.percentile) + " " + num(v.percentileValue);
    if (p.volatility) s += QStringLiteral(" × ") + pane::fmtNum(v.factor, 2);
    return s + ")";
  }
  void detachBigTrades() {
    for (auto& [uid, e] : bt)
      if (e.layer && e.layer->series()) e.layer->series()->detachPrimitive(e.layer.get());
  }
  // The indicator's inputs → a model on the chart's market (Binance only), its layer on the main series.
  void syncBigTrades(const QString& uid, const QJsonObject& in, const QJsonObject& colors, ind::Result& res) {
    std::optional<data::FlowMarket> m;
    if (q->provider == "Binance" || q->provider == "Binance futures") m = data::FlowMarket::fromChartSymbol(q->cfg.value("symbol").toString());
    if (!m || !q->cfg.value("compare").toArray().isEmpty()) {
      bt.erase(uid);
      res.error = QT_TRANSLATE_NOOP("charts", "Big Trades need Binance trades (spot or USD-M perpetual)");
      return;
    }
    if (flowPaused) {   // hidden pane: only a Big Trades with an alert keeps running (pauseFlow); the others come back on show
      const QString a = in.value("alert").toString();
      if (a != "Visual" && a != "Sound" && a != "Sound and visual") {
        bt.erase(uid);
        return;
      }
    }
    BtEntry& e = bt[uid];
    if (!e.model || !(e.model->market() == *m)) {
      e.layer.reset();
      e.model = std::make_unique<flow::BigTradesModel>(*m);
      QObject::connect(e.model.get(), &flow::BigTradesModel::changed, q, [this, uid] {
        const auto it = bt.find(uid);
        if (it != bt.end() && it->second.layer) it->second.layer->requestRepaint();
        // the threshold in the legend: redrawn when its (rounded) text changes, at most once a second
        if (!legendTimer.isActive()) legendTimer.start(1000);
      });
      e.layer = std::make_unique<flow::BigTradesLayer>(q, e.model.get());
      QObject::connect(e.model.get(), &flow::BigTradesModel::alert, q,
                       [this, uid](const ::charts::flow::AggregatedTrade& a, double threshold) { onBigTradeAlert(uid, a, threshold); });
    }
    flow::BigTradesModel::Config c;
    c.det.mode = in.value("mode").toString() == "Manual" ? ::charts::flow::DetectorParams::Mode::Manual
                                                         : ::charts::flow::DetectorParams::Mode::Auto;
    c.det.percentile = std::clamp(in.value("percentile").toDouble(99), 50.0, 99.99);
    c.det.windowMs = int64_t(std::clamp(in.value("window").toDouble(30), 1.0, 24 * 60.0) * 60000);
    c.det.volatility = in.value("volatility").toString() != "Off";
    c.det.minVolume = std::max(0.0, in.value("min").toDouble());
    c.det.maxVolume = std::max(0.0, in.value("max").toDouble());
    c.agg.windowMs = int64_t(std::clamp(in.value("aggWindow").toDouble(50), 0.0, 5000.0));
    c.agg.sweep = in.value("aggPrice").toString().startsWith("Sweep");
    e.model->setConfig(c);
    flow::BigTradesLayer::Look look;
    look.minD = std::clamp(in.value("minSize").toDouble(8), 2.0, 200.0);
    look.maxD = std::clamp(in.value("maxSize").toDouble(40), look.minD, 400.0);
    look.minOpacity = std::clamp(in.value("minOpacity").toDouble(35) / 100, 0.05, 1.0);
    const QString shape = in.value("shape").toString();
    look.shape = shape == "Square" ? flow::BigTradesLayer::Shape::Square : shape == "Diamond" ? flow::BigTradesLayer::Shape::Diamond
               : shape == "Text"   ? flow::BigTradesLayer::Shape::Text  : flow::BigTradesLayer::Shape::Circle;
    look.showVolume = in.value("showVolume").toString() == "On";
    look.buy = cssColor(colors.value("buy").toString());
    look.sell = cssColor(colors.value("sell").toString());
    e.layer->setLook(look);
    const QString alert = in.value("alert").toString();
    e.alertVisual = alert == "Visual" || alert == "Sound and visual";
    e.alertSound = alert == "Sound" || alert == "Sound and visual";
    e.model->setAlertFactor(e.alertVisual || e.alertSound ? std::max(1.0, in.value("alertFactor").toDouble(3)) : 0);
    if (main && e.layer->series() != main) main->attachPrimitive(e.layer.get());
  }

  // The footprint applies when the chart shows Binance's own candles of a market whose trades we stream, on a price
  // scale (not in compare mode, where prices become percentages).
  std::optional<data::FlowMarket> footprintMarket() const {
    if (q->cfg.value("type").toString() != "footprint") return std::nullopt;
    if (!q->cfg.value("compare").toArray().isEmpty()) return std::nullopt;
    if (q->provider != "Binance" && q->provider != "Binance futures") return std::nullopt;
    return data::FlowMarket::fromChartSymbol(q->cfg.value("symbol").toString());
  }
  // A chart bar with the open/high/low/close and volume of its trades, when its footprint is complete (user's choice
  // 2026-11-18: the candle and its cells always agree, even where Binance futures klines date a trade differently).
  Bar fpBar(const Bar& b) const {
    if (!fp) return b;
    const auto* c = fp->complete(b.time);
    if (!c) return b;
    const double t = fp->ticks().step;
    Bar o = b;
    o.open = fp->ticks().priceOf(c->open);
    o.high = fp->ticks().priceOf(c->high);
    o.low = fp->ticks().priceOf(c->low);
    o.close = fp->ticks().priceOf(c->close);
    o.volume = c->volume();
    (void)t;
    return o;
  }
  void syncFootprint() {
    const auto m = footprintMarket();
    const QString tf = q->cfg.value("tf").toString();
    if (!m || flowPaused) {   // paused: a hidden pane folds no trades (rebuilt when shown, see pauseFlow)
      if (fpLayer && fpLayer->series()) fpLayer->series()->detachPrimitive(fpLayer.get());
      fpLayer.reset();
      fp.reset();
      return;
    }
    if (!fp || !(fp->market() == *m) || fp->tf() != tf) {
      if (fpLayer && fpLayer->series()) fpLayer->series()->detachPrimitive(fpLayer.get());
      fpLayer.reset();
      fp = std::make_unique<flow::FootprintFeed>(*m, tf);
      QObject::connect(fp.get(), &flow::FootprintFeed::changed, q, [this](bool rebuilt) { onFootprint(rebuilt); });
    }
    if (!fpLayer) fpLayer = std::make_unique<flow::FootprintLayer>(q, fp.get());
    fpLayer->setSettings(::charts::flow::FootprintSettings::fromJson(q->cfg.value("footprint").toObject()));
    if (main && fpLayer->series() != main) {
      if (fpLayer->series()) fpLayer->series()->detachPrimitive(fpLayer.get());
      main->attachPrimitive(fpLayer.get());
    }
  }
  // New trades: the forming candle(s) take their trade-made OHLC; history pages: everything is drawn again.
  void onFootprint(bool rebuilt) {
    if (!fp || !main) return;
    if (rebuilt) {   // history arrived: candles that became complete take their trade-made OHLC, the view stays put
      refreshFootprintBars();
      return;
    }
    const int n = int(q->shown.size());
    if (n && !q->replay && q->shown.size() == q->bars.size()) {
      for (int i = std::max(0, n - 2); i < n; ++i) {
        const Bar b = fpBar(q->bars[i]);
        if (b.open == q->shown[i].open && b.high == q->shown[i].high && b.low == q->shown[i].low && b.close == q->shown[i].close &&
            b.volume == q->shown[i].volume)
          continue;
        q->shown[i] = b;
        main->update(pointFor(b, i), true);
        if (volume) volume->update(volPoint(b), true);
      }
    }
    if (fpLayer) fpLayer->requestRepaint();
    if (const QString note = footprintNote(); note != lastFpNote) {   // the legend only when its note changes
      lastFpNote = note;
      q->updateLegend();
    }
  }
  QString lastFpNote;
  // Called from the footprint feed's own changed() signal: a render can replace that feed (syncFootprint), so the
  // full render is queued rather than run under the feed's emit.
  void renderLater() { QTimer::singleShot(0, q, [p = q] { p->render(false); }); }
  void refreshFootprintBars() {
    if (q->times.size() != q->shown.size() || q->bars.isEmpty()) { renderLater(); return; }
    TimeScale* ts = chart->timeScale();
    const auto range = ts->getVisibleLogicalRange();
    q->shown = visibleBars();
    if (q->shown.size() != q->times.size()) { renderLater(); return; }
    QVector<DataPoint> pts, vol;
    pts.reserve(q->shown.size());
    for (int i = 0; i < q->shown.size(); ++i) pts.append(pointFor(q->shown[i], i));
    main->setData(pts);   // same times: the time scale and the view are kept
    if (volume) {
      vol.reserve(q->shown.size());
      for (const Bar& b : q->shown) vol.append(volPoint(b));
      volume->setData(vol);
    }
    if (range) ts->setVisibleLogicalRange(*range);
    nav->dataChanged();
    if (fpLayer) fpLayer->requestRepaint();
    if (const QString note = footprintNote(); note != lastFpNote) {
      lastFpNote = note;
      q->updateLegend();
    }
  }
  QString footprintNote() const {
    if (q->cfg.value("type").toString() != "footprint") return {};
    if (!fp) {
      if (q->provider.isEmpty()) return {};
      return QCoreApplication::translate("charts", "Footprint needs Binance trades (spot or USD-M perpetual): plain candles here");
    }
    if (!fp->ready()) return QCoreApplication::translate("charts", "Footprint: connecting to Binance trades…");
    const auto& st = fp->status();
    const qint64 since = fp->coveredFromMs();
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(since);
    const QString when = t.date() == QDate::currentDate() ? t.toString("HH:mm") : QLocale(QLocale::English).toString(t, "d MMM HH:mm");
    QString s = since == 0 ? QCoreApplication::translate("charts", "Footprint: waiting for trades…")
                : st.historyDone ? QCoreApplication::translate("charts", "Footprint from %1 (older bars: plain candles)").arg(when)
                                 : QCoreApplication::translate("charts", "Footprint: loading trade history, back to %1 so far…").arg(when);
    if (st.feed != data::FeedState::Open) s += QCoreApplication::translate("charts", " · live trades reconnecting");
    if (st.approximate) s += QCoreApplication::translate("charts", " · aggressor sides estimated (tick rule)");
    return s;
  }

  // ---- chart style (settings › chart style, TradingView's chart settings): absent keys follow the theme ----
  QJsonObject chartStyle() const { return app->settings.value("chartStyle").toObject(); }
  QColor styleColor(const char* key, const QColor& theme) const {
    const QColor c(chartStyle().value(QLatin1String(key)).toString());
    return c.isValid() ? c : theme;
  }
  bool styleFlag(const char* key, bool theme) const {
    const QJsonValue v = chartStyle().value(QLatin1String(key));
    return v.isBool() ? v.toBool() : theme;
  }

  // ---- countdown to the forming bar's close (TradingView's "countdown to bar close") ----
  // Seconds left, or -1 when the close is not known: only charts drawn in time (not renko, kagi, line break, P&F),
  // out of replay, while the last bar is still forming. Crypto trades round the clock: every timeframe. Forex: intraday
  // only (its daily bar follows the New York close, not UTC days). Stocks, indices and the rest: up to 30 minutes,
  // because a session's last longer bar ends at the exchange's close and the sessions are not known here.
  qint64 secondsToClose() const {
    if (q->replay || q->bars.isEmpty()) return -1;
    const QString type = q->cfg.value("type").toString();
    if (PRICE_ONLY.contains(type) || type == "range") return -1;
    const QString tf = q->cfg.value("tf").toString();
    const double sec = tfSeconds(tf);
    const QString kind = data::kindOf(q->cfg.value("symbol").toString());
    if (kind == "fx" && !intraday(tf)) return -1;
    if (kind != "crypto" && kind != "fx" && sec > 1800) return -1;
    const qint64 start = q->bars.last().time;
    qint64 end = start + qint64(sec);
    try {   // calendar weeks and months: the next bucket, not a fixed length
      qint64 t = end;
      for (int i = 0; i < 4 && sr::bucketStart(t, tf) == sr::bucketStart(start, tf); ++i) t += 86400;
      end = sr::bucketStart(t, tf);
    } catch (...) {}
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now < start || now >= end) return -1;
    return end - now;
  }
  QString countdownText() const {
    if (!truthy(app->settings.value("countdown"))) return {};
    const qint64 s = secondsToClose();
    if (s < 0) return {};
    if (s >= 86400) return QCoreApplication::translate("charts", "%1d %2h", "days and hours to the bar's close").arg(s / 86400).arg((s % 86400) / 3600, 2, 10, QChar('0'));
    const int h = int(s / 3600), m = int(s % 3600 / 60), x = int(s % 60);
    return h ? QString::asprintf("%02d:%02d:%02d", h, m, x) : QString::asprintf("%02d:%02d", m, x);
  }
  void syncCountdownTimer() {
    if (q->isVisible() && truthy(app->settings.value("countdown"))) { if (!countdownTimer.isActive()) countdownTimer.start(); }
    else countdownTimer.stop();
  }

  // ---- setup ----
  void themeOptions(ChartOptions& o) const {
    const auto& v = style::v();
    o.layout.background = styleColor("background", v.bg);
    o.layout.textColor = styleColor("scaleText", v.text);
    o.layout.fontSize = qBound(9, chartStyle().value("fontSize").toInt(11), 16);
    o.layout.panes.separatorColor = v.border;
    o.layout.panes.separatorHoverColor = v.accentSoft;
    o.layout.panes.enableResize = true;
    o.grid.vertLines.color = styleColor("gridV", v.grid);
    o.grid.horzLines.color = styleColor("gridH", v.grid);
    o.grid.vertLines.visible = styleFlag("gridVVisible", true);
    o.grid.horzLines.visible = styleFlag("gridHVisible", true);
    const QColor cross = styleColor("crosshair", QColor("#9598A1"));
    o.crosshair.vertLine.color = cross;
    o.crosshair.horzLine.color = cross;
    o.rightPriceScale.borderColor = v.border;
    o.timeScale.borderColor = v.border;
  }

  bool wantVolume() const {
    const QJsonValue v = app->settings.value("volume");
    return !PRICE_ONLY.contains(q->cfg.value("type").toString()) && !(v.isBool() && !v.toBool());
  }

  void makeMainSeries() {
    if (fpLayer && fpLayer->series()) fpLayer->series()->detachPrimitive(fpLayer.get());
    for (auto& vp : vpPrims) if (main) main->detachPrimitive(vp.get());
    vpPrims.clear();
    if (main) {
      main->detachPrimitive(drawings.get());
      chart->removeSeries(main);
      main = nullptr;
    }
    if (volume) {
      chart->removeSeries(volume);
      volume = nullptr;
    }
    const auto& sv = style::v();
    const QColor up = styleColor("upBody", sv.up), down = styleColor("downBody", sv.down), accent = kAccent;
    const QColor upBorder = styleColor("upBorder", up), downBorder = styleColor("downBorder", down);
    const QColor upWick = styleColor("upWick", up), downWick = styleColor("downWick", down);
    upC = up; downC = down;
    // volume follows the candles at 45 % when they were recoloured, else the JS's teal / red
    auto faded = [](QColor c) { c.setAlphaF(c.alphaF() * 0.45); return c; };
    const bool recoloured = chartStyle().contains("upBody") || chartStyle().contains("downBody");
    volUp = recoloured ? faded(up) : cssColor("rgba(8,153,129,0.45)");
    volDown = recoloured ? faded(down) : cssColor("rgba(242,54,69,0.45)");
    const QString t = q->cfg.value("type").toString();
    auto common = [this](SeriesOptions& o) {
      o.priceLineVisible = true;
      o.lastValueVisible = true;
      o.lastValueSubtitle = [this] { return countdownText(); };
    };
    if (t == "bars")
      main = chart->addSeries(BarSeries, [&](SeriesOptions& o) { common(o); o.upColor = up; o.downColor = down; });
    else if (t == "line" || t == "linemarkers" || t == "step" || t == "kagi")
      main = chart->addSeries(LineSeries, [&](SeriesOptions& o) {
        common(o);
        o.color = accent;
        o.lineWidth = 2;
        o.lineType = t == "step" || t == "kagi" ? LineType::WithSteps : LineType::Simple;
        o.pointMarkersVisible = t == "linemarkers";
      });
    else if (t == "area")
      main = chart->addSeries(AreaSeries, [&](SeriesOptions& o) {
        common(o);
        o.lineColor = accent;
        o.topColor = cssColor("rgba(41,98,255,0.35)");
        o.bottomColor = cssColor("rgba(41,98,255,0.02)");
        o.lineWidth = 2;
      });
    else if (t == "baseline")
      main = chart->addSeries(BaselineSeries, [&](SeriesOptions& o) {
        common(o);
        o.topLineColor = up;
        o.bottomLineColor = down;
        o.topFillColor1 = cssColor("rgba(8,153,129,0.25)");
        o.topFillColor2 = cssColor("rgba(8,153,129,0.02)");
        o.bottomFillColor1 = cssColor("rgba(242,54,69,0.02)");
        o.bottomFillColor2 = cssColor("rgba(242,54,69,0.25)");
      });
    else if (t == "columns")
      main = chart->addSeries(HistogramSeries, [&](SeriesOptions& o) { common(o); });
    else if (t == "hollow")
      main = chart->addSeries(CandlestickSeries, [&](SeriesOptions& o) {
        common(o);
        o.upColor = QColor(0, 0, 0, 0);
        o.downColor = down;
        o.borderUpColor = upBorder;
        o.borderDownColor = downBorder;
        o.wickUpColor = upWick;
        o.wickDownColor = downWick;
        o.borderVisible = true;
        o.wickVisible = styleFlag("wicks", true);
      });
    else
      main = chart->addSeries(CandlestickSeries, [&](SeriesOptions& o) {
        common(o);
        o.upColor = up;
        o.downColor = down;
        o.borderVisible = styleFlag("borders", false);
        o.borderUpColor = upBorder;
        o.borderDownColor = downBorder;
        o.wickVisible = styleFlag("wicks", true);
        o.wickUpColor = upWick;
        o.wickDownColor = downWick;
      });
    main->attachPrimitive(drawings.get());
    markers = createSeriesMarkers(main, {});
    mainType = t;
    if (wantVolume()) {
      volume = chart->addSeries(HistogramSeries, [](SeriesOptions& o) {
        o.priceScaleId = "vol";
        o.priceFormat = PriceFormat::volume();
        o.lastValueVisible = false;
        o.priceLineVisible = false;
      });
      chart->priceScale("vol")->applyOptions([](PriceScaleOptions& o) { o.scaleMargins = {0.82, 0}; });
    }
  }

  // ---- data ----
  Bars visibleBars() {
    const Bars raw = q->replay ? q->bars.mid(0, q->replay->index + 1) : q->bars;
    shownColors.clear();
    const QString t = q->cfg.value("type").toString();
    try {
      if (t == "heikin") return sr::heikinAshi(raw);
      if (t == "renko") return sr::renko(raw).bars;
      if (t == "linebreak") return sr::lineBreak(raw);
      if (t == "range") return sr::rangeBars(raw).bars;
      if (t == "kagi") {
        auto k = sr::kagi(raw);
        shownColors = k.colors;
        return k.bars;
      }
      if (t == "pnf") return sr::pointFigure(raw).bars;
      if (t == "footprint" && fp) {
        Bars out = raw;
        for (Bar& b : out) b = fpBar(b);
        return out;
      }
    } catch (...) {
      return {};
    }
    return raw;
  }

  DataPoint pointFor(const Bar& b, int i) const {
    const Time time = q->toChart(b.time);
    const QString& t = mainType;
    if (t == "line" || t == "linemarkers" || t == "step" || t == "area" || t == "baseline") return DataPoint::val(time, b.close);
    if (t == "kagi") return DataPoint::val(time, b.close, cssColor(shownColors.value(i)));
    if (t == "columns") return DataPoint::val(time, b.close, b.close >= b.open ? upC : downC);
    DataPoint p = DataPoint::ohlc(time, b.open, b.high, b.low, b.close);
    const auto it = barColorAt.constFind(b.time);
    if (it != barColorAt.constEnd() && !it->isEmpty()) {
      const QColor c = cssColor(*it);
      p.color = c;
      if (t != "bars") { p.borderColor = c; p.wickColor = c; }
    }
    return p;
  }

  DataPoint volPoint(const Bar& b) const {
    const double v = std::isnan(b.volume) ? 0 : b.volume;   // b.volume || 0 (bricks have none)
    return DataPoint::val(q->toChart(b.time), v, b.close >= b.open ? volUp : volDown);   // set by makeMainSeries
  }

  void renderCompare() {
    for (auto& [sym, s] : compareSeries) chart->removeSeries(s);
    compareSeries.clear();
    static const QStringList palette = {"#ff9800", "#e91e63", "#00bcd4", "#8bc34a", "#9c27b0"};
    const QJsonArray cmp = q->cfg.value("compare").toArray();
    for (int k = 0; k < cmp.size(); ++k) {
      const QString sym = cmp[k].toString();
      const Bars src = compareData.value(sym);
      const qint64 lastT = q->shown.isEmpty() ? 0 : q->shown.last().time;
      auto* s = chart->addSeries(LineSeries, [&](SeriesOptions& o) {
        o.color = QColor(palette[k % palette.size()]);
        o.lineWidth = 2;
        o.priceLineVisible = false;
        o.title = pane::ticker(sym);
      });
      QVector<DataPoint> pts;
      for (const Bar& b : src)
        if (!q->replay || (!q->shown.isEmpty() && b.time <= lastT)) pts.append(DataPoint::val(q->toChart(b.time), b.close));
      s->setData(pts);
      compareSeries.append({sym, s});
    }
  }

  // Indicator values for one instance: on the chart's bars, on another indicator's output
  // (source "ind:<uid>:<output>"), or on a higher timeframe mapped back without look-ahead.
  ind::Result computeIndicator(const QJsonObject& indc, const Bars& bars) {
    const Params params = indc.value("params").toObject();
    const QString src = charts::str(params, "source");
    const QString id = indc.value("id").toString();
    const QString itf = indc.value("tf").toString();
    const QString ctf = q->cfg.value("tf").toString();
    if (!itf.isEmpty() && itf != ctf) {
      if (!mtf.contains(itf)) {
        loadMtf(itf);
        ind::Result r;
        r.pending = true;
        return r;
      }
      const Mtf& h = mtf[itf];
      if (!h.error.isEmpty()) {
        ind::Result r;
        r.error = h.error;
        return r;
      }
      Bars hb = h.bars;
      if (q->replay && !bars.isEmpty()) {
        // replay: only higher-timeframe bars that had finished by the replayed bar
        const double cEnd = bars.last().time + tfSeconds(ctf), hs = tfSeconds(itf);
        Bars kept;
        for (int j = 0; j < hb.size(); ++j)
          if ((j + 1 < hb.size() ? double(hb[j + 1].time) : hb[j].time + hs) <= cEnd) kept.append(hb[j]);
        hb = kept;
      }
      if (hb.isEmpty()) return {};
      Params p2 = params;
      if (src.startsWith("ind:")) p2["source"] = "close";
      const ind::Result res = ind::compute(id, hb, p2);
      return mapToChart(res, hb, itf, bars, ctf);
    }
    if (src.startsWith("ind:")) {
      const QStringList parts = src.split(':');
      const QString uid = parts.value(1), key = parts.value(2);
      const auto it = q->indResults.constFind(uid);
      if (it == q->indResults.constEnd() || !it->series.contains(key)) {
        ind::Result r;
        r.error = QT_TRANSLATE_NOOP("charts", "Its source indicator is missing");   // translated where shown (fmt::tx)
        return r;
      }
      const charts::Series vals = it->series.value(key);
      Bars synth = bars;
      for (int i = 0; i < synth.size(); ++i) {
        const double v = i < vals.size() ? vals[i] : NaN;
        synth[i].open = synth[i].high = synth[i].low = synth[i].close = v;
      }
      Params p2 = params;
      p2["source"] = "close";
      return ind::compute(id, synth, p2);
    }
    return ind::compute(id, bars, params);
  }

  void loadMtf(const QString& tf) {
    if (mtfLoading.contains(tf)) return;
    mtfLoading.insert(tf);
    const QString ctf = q->cfg.value("tf").toString();
    if (tfSeconds(tf) < tfSeconds(ctf)) {
      // the JS throws before its first await: the error lands at once; render again on the next turn
      mtf[tf] = {{}, QCoreApplication::translate("charts", "Pick a timeframe at or above the chart's (%1)").arg(ctf), QDateTime::currentMSecsSinceEpoch()};
      mtfLoading.remove(tf);
      QTimer::singleShot(0, q, [this] { q->render(false); });
      return;
    }
    const int token = loadToken;
    data::getCandles(q->cfg.value("symbol").toString(), tf, 2000, q, [this, tf, token](const data::Result<data::CandlesResult>& r) {
      if (token != loadToken) return;   // load() bumped the token and cleared mtfLoading
      if (r.ok()) mtf[tf] = {r.value.candles, {}, QDateTime::currentMSecsSinceEpoch()};
      else mtf[tf] = {{}, r.error->message, QDateTime::currentMSecsSinceEpoch()};
      mtfLoading.remove(tf);
      q->render(false);
    });
  }

  void renderIndicators() {
    for (auto& list : indSeries)
      for (Series* s : list) chart->removeSeries(s);
    indSeries.clear();
    for (auto& vp : vpPrims) if (main) main->detachPrimitive(vp.get());
    vpPrims.clear();
    detachBigTrades();
    detachHeatmaps();
    detachLiqMaps();
    detachSmc();
    QSet<QString> btUsed, heatUsed, liqUsed, smcUsed;
    QSet<QString> hiddenUids;   // indicators hidden with the eye: their models stay (see the erase loops below)
    // the heights the user gave the panes (a dragged separator) survive the rebuild below, by the indicator that
    // opens each pane; without this every live refresh (1.5 s) snapped them back to the defaults
    QHash<QString, double> stretchOf;
    {
      const auto old = chart->panes();
      for (int i = 0; i < old.size() && i < paneOwners.size(); ++i) stretchOf.insert(paneOwners[i], old[i]->getStretchFactor());
    }
    paneOwners = {QString()};   // pane 0: the price pane
    while (chart->panes().size() > 1) chart->removePane(int(chart->panes().size()) - 1);
    q->indResults.clear();
    const Bars& bars = q->shown;
    const int n = int(bars.size());
    const double stp = q->step();
    const auto& T = q->times;
    auto timeAt = [&](int i) -> Time { return i < n ? T[i] : Time(T[n - 1] + (i - n + 1) * stp); };
    const QJsonArray inds = q->cfg.value("indicators").toArray();
    // indicators built on another indicator's output go after their source
    QHash<QString, QJsonObject> byUid;
    for (const QJsonValue& v : inds) byUid.insert(v.toObject().value("uid").toString(), v.toObject());
    std::function<int(const QJsonObject&, QSet<QString>&)> depth = [&](const QJsonObject& x, QSet<QString>& seen) -> int {
      const QString src = charts::str(x.value("params").toObject(), "source");
      const QString uid = x.value("uid").toString();
      if (!src.startsWith("ind:") || seen.contains(uid)) return 0;
      seen.insert(uid);
      const auto it = byUid.constFind(src.split(':').value(1));
      return it != byUid.constEnd() ? 1 + depth(*it, seen) : 0;
    };
    QVector<QPair<int, QJsonObject>> ordered;
    for (const QJsonValue& v : inds) {
      QSet<QString> seen;
      ordered.append({depth(v.toObject(), seen), v.toObject()});
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    int paneIdx = 0;
    QHash<QString, int> paneOf;
    QVector<ind::Marker> mk;
    QHash<qint64, QString> colorAt;
    const QColor muted = style::v().muted;
    for (const auto& [dep, indc] : ordered) {
      const QString id = indc.value("id").toString(), uid = indc.value("uid").toString();
      const ind::DefPtr def = ind::byId(id);
      if (!def) continue;
      ind::Result res;
      try {
        const pine::ChartScope scope(q->cfg.value("symbol").toString());   // Pine's syminfo.tickerid
        res = n ? computeIndicator(indc, bars) : ind::Result();
      } catch (const std::exception& e) {
        res = ind::Result();
        res.error = QString::fromUtf8(e.what());
      }
      q->indResults.insert(uid, res);
      const bool hasTf = truthy(indc.value("tf"));
      if (truthy(indc.value("hidden")) && n) hiddenUids.insert(uid);
      if (truthy(indc.value("hidden")) || !n) continue;
      if (!hasTf) mk += res.markers;
      if (!res.barColors.isEmpty() && !hasTf)
        for (int i = 0; i < res.barColors.size() && i < n; ++i)
          if (!res.barColors[i].isEmpty()) colorAt.insert(bars[i].time, res.barColors[i]);
      if (!res.volumeProfile.isEmpty()) {
        auto vp = std::make_unique<VolumeProfile>(q, res.volumeProfile);
        main->attachPrimitive(vp.get());
        vpPrims.push_back(std::move(vp));
      }
      if (res.extra.contains("heatmap")) {
        syncHeatmap(uid, res.extra.value("heatmap").toObject(), q->indResults[uid]);
        heatUsed.insert(uid);
      }
      if (res.extra.contains("smcliq")) {
        syncSmc(uid, res.extra.value("smcliq").toObject(), indc.value("colors").toObject());
        smcUsed.insert(uid);
      }
      if (res.extra.contains("liqmap")) {
        syncLiqMap(uid, res.extra.value("liqmap").toObject(), q->indResults[uid]);
        liqUsed.insert(uid);
      }
      if (res.extra.contains("bigTrades")) {
        syncBigTrades(uid, res.extra.value("bigTrades").toObject(), indc.value("colors").toObject(), q->indResults[uid]);
        btUsed.insert(uid);
      }
      if (!res.sessionProfile.isEmpty()) {
        auto sp = std::make_unique<SessionVolumeProfile>(q, res.sessionProfile);
        sp->keyOf = [](qint64 t, const QString& period) { return ind::periodKey(t, period); };
        sp->tfSeconds = tfSeconds(q->cfg.value("tf").toString());
        main->attachPrimitive(sp.get());
        vpPrims.push_back(std::move(sp));
      }
      QVector<const ind::Output*> outs;
      for (const ind::Output& o : def->outputs)
        if (res.series.contains(o.key)) outs.append(&o);
      if (outs.isEmpty()) continue;
      const QString src = charts::str(indc.value("params").toObject(), "source");
      std::optional<int> parentPane;
      if (src.startsWith("ind:")) {
        const auto it = paneOf.constFind(src.split(':').value(1));
        if (it != paneOf.constEnd()) parentPane = *it;
      }
      const int pane = parentPane ? *parentPane : def->overlay ? 0 : ++paneIdx;
      paneOf.insert(uid, pane);
      if (pane >= paneOwners.size()) paneOwners.resize(pane + 1);
      if (paneOwners[pane].isNull() && pane > 0) paneOwners[pane] = uid;
      QVector<Series*> list;
      const QJsonObject colors = indc.value("colors").toObject();
      const bool noAutoscale = truthy(def->extra.value("noAutoscale"));
      const bool volumeFormat = truthy(def->extra.value("volumeFormat"));
      for (const ind::Output* out : outs) {
        const QString cs = colors.value(out->key).toString();
        const QColor color = cssColor(cs.isEmpty() ? out->color : cs);
        auto opts = [&](SeriesOptions& o) {
          o.noAutoscale = noAutoscale;
          // the axis ticks never step below the format's minMove (default 0.01): an indicator whose values span
          // less than that (ATR or a ratio on a cheap coin) got no ticks. The labels use fmtPrice either way.
          if (volumeFormat) o.priceFormat = PriceFormat::volume();
          else o.priceFormat = PriceFormat::price(8, 1e-8);
          o.color = color;
          o.priceLineVisible = false;
          o.lastValueVisible = pane > 0;
          o.crosshairMarkerVisible = false;
          o.title = "";
        };
        Series* s;
        const charts::Series& vals = res.series[out->key];
        // a script's per bar colours, unless the user picked a colour for this output in the settings
        const QStringList ownColors = cs.isEmpty() ? res.seriesColors.value(out->key) : QStringList();
        if (out->kind == "histogram") {
          s = chart->addSeries(HistogramSeries, opts, pane);
          s->setData(histData(vals, ownColors.isEmpty() ? res.colors : ownColors, timeAt, n));
        } else {
          const bool dots = truthy(out->extra.value("dots"));
          s = chart->addSeries(LineSeries, [&](SeriesOptions& o) {
            opts(o);
            o.lineWidth = out->lineWidth ? out->lineWidth : 1;
            if (dots) {
              o.lineVisible = false;
              o.pointMarkersVisible = true;
              o.pointMarkersRadius = 1.5;
            }
          }, pane);
          s->setData(lineData(vals, res.breaks, timeAt, n, ownColors));
        }
        list.append(s);
      }
      if (!parentPane)
        for (double lv : def->levels) {
          PriceLineOptions pl;
          pl.price = lv;
          pl.color = muted;
          pl.lineWidth = 1;
          pl.lineStyle = LineStyle::Dashed;
          pl.axisLabelVisible = false;
          list[0]->createPriceLine(pl);
        }
      indSeries.insert(uid, list);
    }
    // Removed indicators go. A hidden one keeps its model while the chart's market is still the one it reads: hiding
    // threw away a history computed on a worker (Big Trades, a liquidation calibration of 72 runs) or a heatmap's
    // book, all redone at the next click on the eye. Detached above, it draws nothing; a hidden Big Trades fires no alert.
    std::optional<data::FlowMarket> chartMarket;
    if ((q->provider == "Binance" || q->provider == "Binance futures") && q->cfg.value("compare").toArray().isEmpty())
      chartMarket = data::FlowMarket::fromChartSymbol(q->cfg.value("symbol").toString());
    for (auto it = bt.begin(); it != bt.end();) {
      if (btUsed.contains(it->first)) { ++it; continue; }
      BtEntry& e = it->second;
      if (hiddenUids.contains(it->first) && chartMarket && e.model && e.model->market() == *chartMarket) {
        e.model->setAlertFactor(0);
        e.alertVisual = e.alertSound = false;
        ++it;
      } else it = bt.erase(it);
    }
    for (auto it = heat.begin(); it != heat.end();) {
      const bool keep = heatUsed.contains(it->first) ||
                        (hiddenUids.contains(it->first) && chartMarket && it->second && it->second->stream()->market().symbol == chartMarket->symbol);
      it = keep ? std::next(it) : heat.erase(it);
    }
    for (auto it = liq.begin(); it != liq.end();) {
      const bool keep = liqUsed.contains(it->first) ||
                        (hiddenUids.contains(it->first) && chartMarket && it->second.model && it->second.model->symbol() == chartMarket->symbol);
      it = keep ? std::next(it) : liq.erase(it);
    }
    for (auto it = smc.begin(); it != smc.end();) it = smcUsed.contains(it->first) || hiddenUids.contains(it->first) ? std::next(it) : smc.erase(it);
    hiddenKept = hiddenUids;
    // candle colours from indicators (Pine's barcolor), only on charts drawn from the real bars
    const QString type = q->cfg.value("type").toString();
    const QHash<qint64, QString> recolor = (type == "candles" || type == "hollow" || type == "bars") ? colorAt : QHash<qint64, QString>();
    if (!recolor.isEmpty() || !barColorAt.isEmpty()) {
      barColorAt = recolor;
      QVector<DataPoint> pts;
      pts.reserve(n);
      for (int i = 0; i < n; ++i) pts.append(pointFor(bars[i], i));
      main->setData(pts);
    }
    const auto panes = chart->panes();
    for (int i = 0; i < panes.size(); ++i) panes[i]->setStretchFactor(stretchOf.value(paneOwners.value(i), i == 0 ? 1 : 0.28));
    QVector<ind::Marker> all = mk + q->extraMarkers;
    QVector<SeriesMarker> sm;
    const Time lastT = T.isEmpty() ? 0 : T.last();
    for (const ind::Marker& m : all) {
      SeriesMarker s;
      s.time = q->toChart(m.time);
      if (q->replay && s.time > lastT) continue;
      s.position = markerPosition(m.position);
      s.shape = markerShape(m.shape);
      const QColor c = cssColor(m.color);
      if (c.isValid()) s.color = c;
      s.text = fmt::tx(m.text);   // marker texts (candle patterns, crosses…) shown translated
      if (m.extra.value("size").isDouble()) s.size = m.extra.value("size").toDouble();
      if (m.extra.value("id").isString()) s.id = m.extra.value("id").toString();
      sm.append(s);
    }
    std::stable_sort(sm.begin(), sm.end(), [](const SeriesMarker& a, const SeriesMarker& b) { return a.time < b.time; });
    markers->setMarkers(sm);
  }

  void loadCompare(std::function<void()> done) {
    compareData.clear();
    const QJsonArray cmp = q->cfg.value("compare").toArray();
    if (cmp.isEmpty()) return done();
    const int limit = app->settings.value("bars").toInt() ? app->settings.value("bars").toInt() : 3000;
    auto join = data::Join::make(int(cmp.size()), std::move(done));
    const int token = loadToken;
    for (const QJsonValue& v : cmp) {
      const QString sym = v.toString();
      data::getCandles(sym, q->cfg.value("tf").toString(), limit, q, [this, sym, join, token](const data::Result<data::CandlesResult>& r) {
        if (token != loadToken) return;   // answer of an older load (other timeframe): the newer data stays
        compareData[sym] = r.ok() ? r.value.candles : Bars();
        join->one();
      });
    }
  }

  // ---- drawing tools ----
  struct PointP { std::optional<QPointF> point; std::optional<double> logical; int paneIndex = 0; };
  std::optional<DrawingLayer::Point> pointAt(const PointP& p) const {
    if (p.paneIndex) return std::nullopt;   // drawings live on the price pane only
    if (!p.point || !p.logical) return std::nullopt;
    const auto lg = chart->timeScale()->coordinateToLogical(p.point->x());
    const double logical = lg ? *lg : *p.logical;
    const auto price = main->coordinateToPrice(p.point->y());
    const auto time = q->timeOfLogicalF(logical);
    if (!price || !time) return std::nullopt;
    return DrawingLayer::Point{*time, *price};
  }

  int rawIndexAt(double t) const {
    // Snap to the raw bar under the click (works on Renko/Heikin Ashi too).
    int i = -1;
    for (int k = 0; k < q->bars.size(); ++k)
      if (q->bars[k].time > t) { i = k; break; }
    i = i - 1;   // findIndex(...) - 1 (findIndex -1 → -2)
    if (i == -2 || i < 0) i = int(q->bars.size()) - 1;
    return i;
  }

  void onClick(const PointP& p) {
    const QString tool = app->tool;
    if (tool == "anchor-pick") {
      const auto& pick = app->pendingAnchor;
      if (!pick || pick->pane != q) return;
      QJsonArray inds = q->cfg.value("indicators").toArray();
      int at = -1;
      for (int k = 0; k < inds.size(); ++k)
        if (inds[k].toObject().value("uid").toString() == pick->uid) at = k;
      const auto pt = pointAt(p);
      if (at < 0 || !pt || q->bars.isEmpty()) return;
      const int i = rawIndexAt(pt->time);
      QJsonObject ind = inds[at].toObject();
      QJsonObject params = ind.value("params").toObject();
      params[pick->key] = double(q->bars[i].time);
      ind["params"] = params;
      inds[at] = ind;
      q->cfg["indicators"] = inds;
      app->pendingAnchor.reset();
      app->setTool("cursor");
      q->render(false);
      app->persist();
      app->toast(QCoreApplication::translate("charts", "Anchored."));
      return;
    }
    if (tool == "replay-pick") {
      const auto pt = pointAt(p);
      if (!pt || q->bars.isEmpty()) return;
      // map the clicked time back to the raw bars (works for Renko/Heikin Ashi too)
      const int i = rawIndexAt(pt->time);
      app->setTool("cursor");
      q->startReplay(std::max(1, i));
      return;
    }
    if (tool == "cursor") {
      if (!p.point) return;
      drawings->selected = drawings->drawingAt(p.point->x(), p.point->y());
      drawings->update();
      return;
    }
    const Tool* def = toolById(tool);
    const auto pt = pointAt(p);
    if (!def || !pt) return;
    DrawingLayer* d = drawings.get();
    if (!d->temp) {
      DrawingLayer::Item it;
      it.id = pane::newUid();
      it.type = tool;
      it.points = {*pt};
      if (def->points > 1) it.points.append(*pt);
      d->temp = it;
    } else {
      d->temp->points.last() = *pt;
      if (d->temp->points.size() < def->points) d->temp->points.append(*pt);
      else d->temp->done = true;
    }
    if (def->points == 1) d->temp->done = true;
    if (d->temp->done) {
      DrawingLayer::Item item = *d->temp;
      item.done = false;
      d->temp.reset();
      if (item.type == "text") {
        bool ok = false;
        // getText runs a nested event loop: the pane (and with it this Impl and its DrawingLayer) can be deleted,
        // or load another symbol, before it returns
        const QPointer<ChartPane> guard(q);
        const QPointer<ChartsApp> appGuard(app);   // the app outlives its panes
        const QString symbol = q->cfg.value("symbol").toString();
        const QString text = QInputDialog::getText(q, QString(), QCoreApplication::translate("charts", "Note text:"), QLineEdit::Normal, QString(), &ok);
        // the note can't be placed any more (pane gone, or another chart in it): like a cancelled prompt, the JS
        // goes back to the cursor
        if (!guard) {   // `this` is gone: touch only the app
          if (appGuard) appGuard->setTool("cursor");
          return;
        }
        if (q->cfg.value("symbol").toString() != symbol) {   // another chart now: its points don't belong there
          d->update();
          app->setTool("cursor");
          return;
        }
        if (!ok || text.isEmpty()) {
          d->update();
          app->setTool("cursor");
          return;
        }
        item.text = text;
      }
      d->items.append(item);
      d->selected = item.id;
      app->saveDrawings(q->cfg.value("symbol").toString(), d->itemsJson());
      if (!truthy(app->settings.value("stayInDrawMode"))) app->setTool("cursor");
    }
    d->update();
  }

  // Lightweight Charts drops a second click that lands within 500ms of the first (it waits for a
  // double-click), so drawing tools read native clicks instead.
  void nativeClick(const QPointF& pos) {
    const auto panes = chart->panes();
    if (panes.isEmpty()) return;
    const QRect r0 = panes[0]->rect();
    const double x = pos.x(), y = pos.y();
    const bool inPlot = x >= 0 && x <= chart->timeScale()->width() && y >= 0 && y <= panes[0]->getHeight();
    PointP p;
    if (inPlot) {
      p.point = QPointF(x - r0.left(), y - r0.top());
      p.logical = chart->timeScale()->coordinateToLogical(x - r0.left());
    }
    p.paneIndex = 0;
    onClick(p);
  }

  // ---- legend ----
  void legendAct(int row, const QString& act) {
    if (act == "symbol") return app->openSymbolSearch();
    const auto& rows = legend->rows();
    if (row < 0 || row >= rows.size()) return;
    const QString key = rows[row].key;
    if (act == "rmcompare") {
      QJsonArray cmp;
      for (const QJsonValue& v : q->cfg.value("compare").toArray())
        if (v.toString() != key) cmp.append(v);
      q->cfg["compare"] = cmp;
      app->persist();
      return q->load(true);
    }
    QJsonArray inds = q->cfg.value("indicators").toArray();
    int at = -1;
    for (int k = 0; k < inds.size(); ++k)
      if (inds[k].toObject().value("uid").toString() == key) at = k;
    if (at < 0) return;
    if (act == "remove") q->removeIndicator(key);
    if (act == "hide") {
      QJsonObject ind = inds[at].toObject();
      ind["hidden"] = !truthy(ind.value("hidden"));
      inds[at] = ind;
      q->cfg["indicators"] = inds;
      q->render(false);
      app->persist();
    }
    if (act == "settings") app->openIndicatorSettings(q, key);
  }

  void placeOverlays() {
    legend->move(8, 6);
    legend->setMaxWidth(std::max(0, q->width() - 90));
    msg->setGeometry(q->rect());
    frame->setGeometry(q->rect());
    if (replayBar) replayBar->place(nav ? nav->height() : 0);
  }

  void removeReplayBar() {
    if (replayBar) replayBar->deleteLater();
    replayBar = nullptr;
  }
};

// ===================================================================================================================

ChartPane::ChartPane(ChartsApp* app_, const QJsonObject& c, QWidget* parent) : QWidget(parent), app(app_), d(new Impl) {
  d->q = this;
  d->app = app_;
  d->countdownTimer.setInterval(1000);
  connect(&d->countdownTimer, &QTimer::timeout, this, [this] { if (d->chart) d->chart->refreshCountdown(); });
  d->utcOffset = QDateTime::currentDateTime().offsetFromUtc();
  cfg = QJsonObject{{"symbol", "BINANCE:BTCUSDT"}, {"tf", "1D"}, {"type", "candles"}, {"indicators", QJsonArray()},
                    {"compare", QJsonArray()}, {"log", false}};
  for (auto it = c.begin(); it != c.end(); ++it) cfg.insert(it.key(), it.value());
  setAttribute(Qt::WA_StyledBackground);
  setAutoFillBackground(true);
  setMinimumSize(0, 0);

  d->chart = new Chart(this);
  d->chart->applyOptions([this](ChartOptions& o) {
    d->themeOptions(o);
    o.crosshair.mode = CrosshairMode::Normal;
    o.timeScale.borderColor = style::v().border;
    o.timeScale.rightOffset = 8;
    // down to 0.05 px a bar (JS 0.5): the navigator's window (and the wheel) can show the whole loaded history
    o.timeScale.minBarSpacing = 0.05;
    o.timeScale.timeVisible = true;
    o.timeScale.secondsVisible = false;
    o.localization.priceFormatter = [](double p) { return pane::fmtPrice(p); };
  });
  d->chart->subscribeCrosshairMove([this](const MouseEventParams& p) {
    if (app->tool != "cursor" && d->drawings->temp && p.point) {
      if (const auto pt = d->pointAt({p.point, p.logical, p.paneIndex})) {
        d->drawings->temp->points.last() = *pt;
        d->drawings->update();
      }
    }
    int i = -1;
    if (p.logical && p.point) i = barAtLogical(*p.logical);   // the union scale's index is not a bar index (D2)
    d->hoverIndex = i;
    updateLegend(i);
    d->bigTradesHover(p);
    if (truthy(app->settings.value("syncCrosshair"))) {
      if (p.time && p.point && p.logical) app->syncCrosshair(this, *p.logical);
      else   // the mouse left this chart (or its bars): the synced crosshairs go too instead of staying at the last spot
        for (ChartPane* o : app->panes)
          if (o != this) o->clearCrosshair();
    }
  });
  d->chart->subscribeClick([this](const MouseEventParams& p) {
    if (app->tool == "cursor") d->onClick({p.point, p.logical, p.paneIndex});
  });
  d->chart->installEventFilter(this);

  // navigator: the whole history (closes of the shown bars on the chart's logical scale) and the visible window
  d->nav = new pane::Navigator(d->chart, [this](QVector<double>& xs, QVector<double>& cs) {
    const plot::TimeScale* ts = d->chart->timeScale();
    const int n = int(shown.size());
    xs.resize(n);
    cs.resize(n);
    // logical index of bar i: i itself unless compared symbols added points of their own to the time scale
    const bool identity = ts->points().size() == n;
    for (int i = 0; i < n; ++i) {
      cs[i] = shown[i].close;
      if (identity) xs[i] = i;
      else if (const auto k = ts->timeToIndex(times.value(i), true)) xs[i] = *k;
      else xs[i] = i;
    }
  }, this);
  d->nav->setRightPad(kFreshRightPad);
  d->nav->installEventFilter(this);   // a press makes this pane the active one, like a press on the chart

  d->msg = new QLabel(this);
  d->msg->setAlignment(Qt::AlignCenter);
  d->msg->setWordWrap(true);
  d->msg->setContentsMargins(20, 20, 20, 20);
  d->msg->setAttribute(Qt::WA_TransparentForMouseEvents);
  d->msg->hide();
  d->legend = new pane::Legend(this);
  d->frame = new ActiveFrame(this, [this] { return app->active == this && app->panes.size() > 1; });

  d->drawings = std::make_unique<DrawingLayer>(this);
  d->makeMainSeries();
  d->flowPauseTimer.setSingleShot(true);
  d->flowPauseTimer.setInterval(Impl::kFlowPauseMs);
  connect(&d->flowPauseTimer, &QTimer::timeout, this, [this] {
    if (!isVisible()) d->pauseFlow();
  });
  d->legendTimer.setSingleShot(true);
  connect(&d->legendTimer, &QTimer::timeout, this, [this] {
    bool changed = false;
    for (const auto& [uid, e] : d->bt) {
      const QString s = d->bigTradesStatus(uid);
      if (d->btLegendShown.value(uid) != s) changed = true;
    }
    for (const auto& [uid, e] : d->liq)
      if (d->liqLegendShown.value(uid) != d->liqStatus(uid)) changed = true;
    for (const auto& [uid, l] : d->heat)
      if (d->heatLegendShown.value(uid) != d->heatmapStatus(uid)) changed = true;
    if (changed) updateLegend(d->hoverIndex);
  });

  d->live = new data::LiveFeed(this);
  connect(d->live, &data::LiveFeed::barsUpdated, this, [this](const Bars& b) { merge(b); });
  connect(d->live, &data::LiveFeed::socketOpened, this, [this] { app->setStatus("ok"); });
  // a Pine Script indicator finished computing in the background: draw again if this chart shows it
  connect(&pine::Cache::instance(), &pine::Cache::ready, this, [this](const QString& id) {
    for (const QJsonValue& v : cfg.value("indicators").toArray())
      if (v.toObject().value("id").toString() == id) { render(false); return; }
  });
  connect(&d->replayTimer, &QTimer::timeout, this, [this] { replayStep(1); });
  connect(app, &ChartsApp::themeChanged, this, [this] {
    const auto& v = style::v();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, v.bg);
    setPalette(pal);
    d->msg->setStyleSheet(QString("color: %1; background: transparent;").arg(v.muted.name(QColor::HexArgb)));
    d->msg->setFont(style::font(13));
    if (d->replayBar) d->replayBar->restyle();
    d->nav->restyle();
    updateLegend();
    d->frame->update();
  });
  {
    const auto& v = style::v();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, v.bg);
    setPalette(pal);
    d->msg->setStyleSheet(QString("color: %1; background: transparent;").arg(v.muted.name(QColor::HexArgb)));
    d->msg->setFont(style::font(13));
  }
  d->msg->raise();
  d->legend->raise();
  d->frame->raise();
  updateLegend();
}

ChartPane::~ChartPane() {
  // no event of a dying child (Leave on the replay bar…) may reach eventFilter once d is freed
  if (d->chart) d->chart->removeEventFilter(this);
  if (d->replayBar) {
    for (QWidget* w : d->replayBar->findChildren<QWidget*>()) w->removeEventFilter(this);
    d->replayBar->removeEventFilter(this);
    delete d->replayBar.data();
  }
  stopLive();
  d->replayTimer.stop();
  // primitives are detached before the chart (and its series) go
  if (d->fpLayer && d->fpLayer->series()) d->fpLayer->series()->detachPrimitive(d->fpLayer.get());
  d->detachBigTrades();
  d->detachHeatmaps();
  d->detachLiqMaps();
  d->detachSmc();
  if (d->main) {
    for (auto& vp : d->vpPrims) d->main->detachPrimitive(vp.get());
    d->main->detachPrimitive(d->drawings.get());
  }
  delete d->chart;
  d->chart = nullptr;
  delete d;
  d = nullptr;
}

// ---------- setup ----------
void ChartPane::applyTheme() {
  // JS only applies options and renders; here the main series is rebuilt (its colours), which empties the time
  // scale for a moment and would reset zoom and scroll: the visible range is put back afterwards
  TimeScale* ts = d->chart->timeScale();
  const auto range = shown.isEmpty() ? std::nullopt : ts->getVisibleLogicalRange();
  d->chart->applyOptions([this](ChartOptions& o) { d->themeOptions(o); });
  d->makeMainSeries();   // up / down colours of the main series follow the theme
  d->syncCountdownTimer();
  render(false);
  if (range && !shown.isEmpty()) ts->setVisibleLogicalRange(*range);
}

// ---------- time helpers (drawings, legend, markers all go through these) ----------
// Called per bar: the tf check is cached per tf, and the local offset is read once per load / render (refreshed in
// both), so every time of one render uses the same offset.
qint64 ChartPane::shift() const {
  const QString tf = cfg.value("tf").toString();
  if (!d->shiftKnown || tf != d->shiftTf) {
    d->shiftTf = tf;
    d->shiftIntraday = intraday(tf);
    d->shiftKnown = true;
  }
  return d->shiftIntraday && !truthy(app->settings.value("utc")) ? d->utcOffset : 0;
}
double ChartPane::step() const {
  return times.size() > 1 ? double(times.last() - times[times.size() - 2]) : tfSeconds(cfg.value("tf").toString());
}
qint64 ChartPane::toChart(qint64 utc) const { return utc + shift(); }

// The chart's time scale: one point per shown bar, unless a compared symbol on another calendar (AAPL beside BTCUSDT)
// added points of its own. A logical index is a position on that scale, not an index into shown / times.
const QVector<qint64>& ChartPane::scalePoints() const {
  d->chart->ensureLayout();   // the union of the series' times is rebuilt lazily
  const QVector<qint64>& P = d->chart->timeScale()->points();
  return P.isEmpty() ? times : P;
}
double ChartPane::logicalOfBar(int i) const {
  const QVector<qint64>& P = scalePoints();
  if (P.size() == times.size() || i < 0 || i >= times.size()) return i;
  const auto it = std::lower_bound(P.begin(), P.end(), times[i]);
  return double(std::min<qsizetype>(it - P.begin(), P.size() - 1));
}
int ChartPane::barAtLogical(double l) const {
  const int n = int(shown.size());
  if (!n || times.size() != n) return n ? std::clamp(clampToInt(std::round(l)), 0, n - 1) : -1;
  const QVector<qint64>& P = scalePoints();
  if (P.size() == n) return std::clamp(clampToInt(std::round(l)), 0, n - 1);
  // the main bar in force at that point's time (a point only the compared symbol has: the bar before it)
  const qint64 t = P[std::clamp(clampToInt(std::round(l)), 0, int(P.size()) - 1)];
  return std::max(0, int(std::upper_bound(times.begin(), times.end(), t) - times.begin()) - 1);
}

std::optional<double> ChartPane::logicalOfF(double utc) const {
  const auto& T = scalePoints();
  const double t = utc + shift();
  const int n = int(T.size());
  if (!n || !std::isfinite(t)) return std::nullopt;
  if (t <= T[0]) return (t - T[0]) / step();
  if (t >= T[n - 1]) return n - 1 + (t - T[n - 1]) / step();
  int lo = 0, hi = n - 1;
  while (hi - lo > 1) {
    const int m = (lo + hi) >> 1;
    if (T[m] <= t) lo = m;
    else hi = m;
  }
  return lo + (t - T[lo]) / double(T[hi] - T[lo]);
}
std::optional<double> ChartPane::logicalOf(qint64 utc) const { return logicalOfF(double(utc)); }

std::optional<double> ChartPane::timeOfLogicalF(double l) const {
  const auto& T = scalePoints();   // logical indexes are on the time scale (see scalePoints)
  const int n = int(T.size());
  // a NaN logical passed both range tests below and became the index of T[i] (undefined conversion, wild read)
  if (!n || !std::isfinite(l)) return std::nullopt;
  double t;
  if (l <= 0) t = T[0] + l * step();
  else if (l >= n - 1) t = T[n - 1] + (l - (n - 1)) * step();
  else {
    const int i = int(std::floor(l));
    t = T[i] + (l - i) * double(T[i + 1] - T[i]);
  }
  return t - shift();
}
std::optional<qint64> ChartPane::timeOfLogical(double logical) const {
  const auto t = timeOfLogicalF(logical);
  if (!t || !(std::fabs(*t) < 9e18)) return std::nullopt;
  return qint64(std::llround(*t));
}

std::optional<double> ChartPane::timeToXF(double utc) const {
  const auto l = logicalOfF(utc);
  if (!l) return std::nullopt;
  return d->chart->timeScale()->logicalToCoordinate(*l);
}
std::optional<double> ChartPane::timeToX(qint64 utc) const { return timeToXF(double(utc)); }

QString ChartPane::formatPrice(double v) const { return pane::fmtPrice(v); }
plot::Chart* ChartPane::chart() const { return d->chart; }
plot::Series* ChartPane::mainSeries() const { return d->main; }
DrawingLayer* ChartPane::drawings() const { return d->drawings.get(); }

void ChartPane::setCrosshairAtLogical(double l) {
  if (shown.isEmpty() || times.size() != shown.size()) return;
  const int i = barAtLogical(l);
  d->chart->setCrosshairPosition(shown[i].close, times[i], d->main);
}
void ChartPane::clearCrosshair() { d->chart->clearCrosshairPosition(); }

// ---------- data ----------
void ChartPane::load(bool keepView) {
  const int token = ++d->loadToken;
  d->loadFailed = false;   // set again if this one fails (a show meanwhile must not start a second request)
  d->mtfLoading.clear();   // in-flight timeframe requests belong to the old token: their answers are dropped
  d->utcOffset = QDateTime::currentDateTime().offsetFromUtc();
  d->liveStoppedAt = 0;
  stopLive();
  showMsg(QCoreApplication::translate("charts", "Loading %1…").arg(pane::ticker(cfg.value("symbol").toString())));
  const int limit = app->settings.value("bars").toInt() ? app->settings.value("bars").toInt() : 3000;
  const QString sym = cfg.value("symbol").toString(), tf = cfg.value("tf").toString();
  data::getCandles(sym, tf, limit, this, [this, token, keepView, sym, tf](const data::Result<data::CandlesResult>& r) {
    if (token != d->loadToken) return;
    // a chart leading a synced replay leaves it on a reload: its followers must not stay frozen on "Synced replay"
    const bool ledReplay = replay && !replay->follower;
    if (!r.ok()) {
      bars.clear();
      provider.clear();   // the legend and the order-flow layers must not keep the previous load's source
      notes.clear();
      replay.reset();
      d->replayTimer.stop();
      d->removeReplayBar();
      render(true);
      showMsg(QCoreApplication::translate("charts", "Couldn't load %1 on %2.\n%3").arg(sym, tf, fmt::tx(r.error->message)));
      // asked again (app started offline, network back later): 15 s, 30 s … at most 5 min; a newer load cancels it
      d->loadFailed = true;
      const int delayS = std::min(300, 15 << std::min(d->loadFailures, 5));
      ++d->loadFailures;
      QTimer::singleShot(delayS * 1000, this, [this, token, keepView] {
        if (token == d->loadToken && bars.isEmpty() && isVisible()) load(keepView);   // hidden: showEvent reloads
      });
      if (ledReplay) app->onReplayStop(this);
      return;
    }
    d->loadFailed = false;
    d->loadFailures = 0;
    cfg["symbol"] = r.value.symbol;
    provider = r.value.provider;
    notes = r.value.notes;
    bars = r.value.candles;
    d->mtf.clear();
    replay.reset();
    d->replayTimer.stop();
    d->removeReplayBar();
    if (ledReplay) app->onReplayStop(this);
    d->drawings->setItems(app->getDrawings(r.value.symbol));
    showMsg(bars.isEmpty() ? QCoreApplication::translate("charts", "No data for this symbol and timeframe.") : QString());
    d->loadCompare([this, token, keepView] {
      if (token != d->loadToken) return;
      render(!keepView);
      startLive();
      app->onPaneLoaded(this);
      emit loaded();
    });
  });
}

void ChartPane::showMsg(const QString& text) {
  d->msg->setVisible(!text.isEmpty());
  d->msg->setText(text);
}

void ChartPane::render(bool resetView) {
  // a pane rendered while hidden (never shown yet, or shown again later): its order flow pauses too (see pauseFlow)
  if (!isVisible() && !d->flowPaused && !d->flowPauseTimer.isActive()) d->flowPauseTimer.start();
  TimeScale* ts = d->chart->timeScale();
  const auto prevRange = ts->getVisibleLogicalRange();
  // positions on the time scale (a compared symbol on another calendar adds points: the last bar is not at n - 1)
  const bool atEnd = prevRange && !shown.isEmpty() && prevRange->to >= logicalOfBar(int(shown.size()) - 1) + 1 - 3;
  d->utcOffset = QDateTime::currentDateTime().offsetFromUtc();
  if (d->mainType != cfg.value("type").toString() || (d->volume != nullptr) != d->wantVolume()) d->makeMainSeries();
  d->syncFootprint();
  shown = d->visibleBars();
  times.clear();
  times.reserve(shown.size());
  for (const Bar& b : shown) times.append(toChart(b.time));
  const double last = shown.isEmpty() || !shown.last().close ? 1 : shown.last().close;
  const int precision = last >= 100 ? 2 : last >= 1 ? 4 : last >= 0.01 ? 6 : 8;
  d->main->applyOptions([&](SeriesOptions& o) { o.priceFormat = PriceFormat::price(precision, 1 / std::pow(10.0, precision)); });
  if (cfg.value("type").toString() == "baseline" && !shown.isEmpty())
    d->main->applyOptions([&](SeriesOptions& o) { o.baseValue = shown[std::max(0, int(shown.size()) - 150)].close; });
  {
    QVector<DataPoint> pts, vol;
    pts.reserve(shown.size());
    for (int i = 0; i < shown.size(); ++i) pts.append(d->pointFor(shown[i], i));
    d->main->setData(pts);
    if (d->volume) {
      vol.reserve(shown.size());
      for (const Bar& b : shown) vol.append(d->volPoint(b));
      d->volume->setData(vol);
    }
  }
  const QString tf = cfg.value("tf").toString();
  d->chart->applyOptions([&](ChartOptions& o) {
    o.timeScale.timeVisible = intraday(tf);
    o.timeScale.secondsVisible = tfSeconds(tf) < 60;
  });
  d->chart->priceScale("right")->applyOptions([&](PriceScaleOptions& o) {
    o.mode = !cfg.value("compare").toArray().isEmpty() ? PriceScaleMode::Percentage
             : truthy(cfg.value("log"))                ? PriceScaleMode::Logarithmic
                                                       : PriceScaleMode::Normal;
  });
  d->renderCompare();
  d->renderIndicators();
  d->drawings->update();
  const double n = shown.isEmpty() ? 0 : logicalOfBar(int(shown.size()) - 1) + 1;
  if (resetView && n) ts->setVisibleLogicalRange({std::max(0.0, n - kFreshBars), n + kFreshRightPad - 1});
  if (!resetView && n && (replay || atEnd)) {
    const double w = prevRange ? std::max(20.0, prevRange->to - prevRange->from) : kFreshBars;
    ts->setVisibleLogicalRange({n - w + kFreshRightPad - 1, n + kFreshRightPad - 1});
  }
  d->nav->dataChanged();
  // the bar under a resting crosshair stays in the legend (a fresh view has no hover)
  if (resetView) d->hoverIndex = -1;
  updateLegend(d->hoverIndex);
}

// "14 on RSI 14 (1D)": the numeric inputs, what it's built on, and its timeframe.
QString ChartPane::indicatorParamsLabel(const QJsonObject& ind) const {
  const ind::DefPtr def = ind::byId(ind.value("id").toString());
  if (!def) return {};
  const QJsonObject params = ind.value("params").toObject();
  if (def->id == "smcliq") {   // "5 · 1h · close": the pivot window or method, the detection timeframe, a close rule
    auto val = [&](const char* k) { QJsonValue v = params.value(k); return v.isUndefined() || v.isNull() ? ind::defaults("smcliq").value(k) : v; };
    QStringList parts;
    const QString m = val("method").toString();
    parts << (m == "Window" ? ind::numberToString(val("length").toDouble())
                            : m == "Percent" ? ind::numberToString(val("percent").toDouble()) + "%" : ind::numberToString(val("atrMult").toDouble()) + " ATR");
    if (val("detectTf").toString() != "Chart") parts << val("detectTf").toString();
    if (val("sweep").toString() == "Close") parts << fmt::tx("Close");
    return parts.join(QStringLiteral(" · "));
  }
  if (def->id == "liqmap") {   // "High (25–125×) longs": only what differs from the defaults
    auto val = [&](const char* k) { QJsonValue v = params.value(k); return v.isUndefined() || v.isNull() ? ind::defaults("liqmap").value(k) : v; };
    QStringList parts;
    if (val("leverage").toString() != ind::defaults("liqmap").value("leverage").toString()) parts << fmt::tx(val("leverage").toString());
    if (val("side").toString() != "Both") parts << fmt::tx(val("side").toString());
    if (val("ratio").toString() == "50/50") parts << QStringLiteral("50/50");
    return parts.join(' ');
  }
  if (def->id == "heatmap") {   // "Spot $5 Σ": only what differs from the defaults
    auto val = [&](const char* k) { QJsonValue v = params.value(k); return v.isUndefined() || v.isNull() ? ind::defaults("heatmap").value(k) : v; };
    QStringList parts;
    if (val("book").toString() != "Chart") parts << fmt::tx(val("book").toString());
    if (val("step").toDouble() > 0) parts << ind::numberToString(val("step").toDouble());
    if (val("combine").toString() == "Sum") parts << QStringLiteral("Σ");
    return parts.join(' ');
  }
  if (def->id == "bigtrades") {   // "auto p99 30m", "≥ 2", "2–50": the filter in a few characters
    auto val = [&](const char* k) { QJsonValue v = params.value(k); return v.isUndefined() || v.isNull() ? ind::defaults("bigtrades").value(k) : v; };
    const double mn = val("min").toDouble(), mx = val("max").toDouble();
    QString s = val("mode").toString() == "Manual"
                    ? (mx > 0 ? ind::numberToString(mn) + QStringLiteral("–") + ind::numberToString(mx) : QStringLiteral("≥ ") + ind::numberToString(mn))
                    : QCoreApplication::translate("charts", "auto p%1 %2m").arg(ind::numberToString(val("percentile").toDouble()), ind::numberToString(val("window").toDouble()));
    if (val("mode").toString() != "Manual" && mx > 0) s += QStringLiteral(" ≤ ") + ind::numberToString(mx);
    return s;
  }
  QStringList nums;
  for (const ind::Input& x : def->inputs) {
    if (x.type == "source") continue;
    QJsonValue v = params.value(x.key);
    if (v.isNull() || v.isUndefined()) v = x.def;
    if (x.type != "time") {
      if (v.isDouble()) nums << ind::numberToString(v.toDouble());
      else if (v.isString()) nums << (x.type == "select" ? fmt::tx(v.toString()) : v.toString());   // option: shown translated
      else if (v.isBool()) nums << (v.toBool() ? "true" : "false");
      else nums << QString();
      continue;
    }
    if (truthy(v)) {
      const QDate dt = QDateTime::fromSecsSinceEpoch(qint64(v.toDouble())).toLocalTime().date();
      nums << (i18n::current() == i18n::Lang::French ? i18n::locale().toString(dt, "d MMM yyyy")
                                                     : QLocale(QLocale::English, QLocale::UnitedStates).toString(dt, "MMM d, yyyy"));
    } else nums << QCoreApplication::translate("charts", "pick a bar");
  }
  const QString src = charts::str(params, "source");
  QString on;
  if (src.startsWith("ind:")) {
    const QString puid = src.split(':').value(1);
    QJsonObject parent;
    for (const QJsonValue& x : cfg.value("indicators").toArray())
      if (x.toObject().value("uid").toString() == puid) parent = x.toObject();
    const ind::DefPtr pd = parent.isEmpty() ? nullptr : ind::byId(parent.value("id").toString());
    on = pd ? QCoreApplication::translate("charts", " on %1 %2").arg(shortName(fmt::tx(pd->name)), indicatorParamsLabel(parent))
            : QCoreApplication::translate("charts", " on (missing)");
  }
  const QString itf = ind.value("tf").toString();
  return nums.join(" ") + on + (!itf.isEmpty() && itf != cfg.value("tf").toString() ? QString(" (%1)").arg(itf) : QString());
}

// ---------- legend ----------
void ChartPane::updateLegend(int index) {
  using L = pane::Legend;
  const auto& v = style::v();
  const int n = int(shown.size());
  const int i = index >= 0 ? index : n - 1;
  const Bar* b = i >= 0 && i < n ? &shown[i] : nullptr;
  const Bar* prev = i - 1 >= 0 && i - 1 < n ? &shown[i - 1] : nullptr;
  const double chg = b && prev ? (b->close - prev->close) / prev->close * 100 : NaN;
  const QColor cls = std::isnan(chg) ? v.text : chg >= 0 ? d->upC : d->downC;   // the candles' colours (settings › chart style)
  const QString sym = cfg.value("symbol").toString();
  const QString name = QString("%1 · %2 · %3")
                           .arg(pane::ticker(sym), cfg.value("tf").toString(), provider.isEmpty() ? pane::exchange(sym) : fmt::tx(provider));
  const QFont f12 = style::font(12), title = style::font(13, QFont::DemiBold);
  QVector<L::Row> rows;
  {
    L::Row r;
    r.kind = L::Row::Title;
    L::Cell t;
    t.runs.append({name, v.text, title, 0, "symbol"});
    r.cells.append(t);
    if (b) {
      L::Cell o;
      auto part = [&](const QString& k, const QString& val) {
        o.runs.append({k, v.muted, f12, 2});
        o.runs.append({val, cls, f12, 6});
      };
      part("O", pane::fmtPrice(b->open));
      part("H", pane::fmtPrice(b->high));
      part("L", pane::fmtPrice(b->low));
      part("C", pane::fmtPrice(b->close));
      o.runs.append({pane::fmtPct(chg), cls, f12, 6});
      r.cells.append(o);
    }
    rows.append(r);
  }
  const QString icx = legendIcon("x"), icg = legendIcon("gear"), ice = legendIcon("eye");
  for (const QJsonValue& c : cfg.value("compare").toArray()) {
    L::Row r;
    r.kind = L::Row::Compare;
    r.key = c.toString();
    L::Cell nm;
    nm.runs.append({QCoreApplication::translate("charts", "%1 (compare)").arg(pane::ticker(r.key)), v.muted, f12});
    L::Cell acts;
    acts.acts = true;
    acts.runs.append({{}, {}, f12, 0, "rmcompare", icx, QCoreApplication::translate("charts", "Remove")});
    r.cells << nm << acts;
    rows.append(r);
  }
  for (const QJsonValue& x : cfg.value("indicators").toArray()) {
    const QJsonObject ind = x.toObject();
    const ind::DefPtr def = ind::byId(ind.value("id").toString());
    if (!def) continue;
    const QString uid = ind.value("uid").toString();
    const ind::Result res = indResults.value(uid);
    L::Row r;
    r.kind = L::Row::Ind;
    r.key = uid;
    r.opacity = truthy(ind.value("hidden")) ? 0.45 : 1;
    L::Cell nm;
    nm.runs.append({shortName(fmt::tx(def->name)) + " " + indicatorParamsLabel(ind), v.muted, f12});
    L::Cell vals;
    if (!res.error.isEmpty()) vals.runs.append({fmt::tx(res.error), v.down, f12});
    else if (res.extra.contains("liqmap")) {   // Liquidation heatmap: an estimate, and how much it rests on
      const QString s = d->liqStatus(uid);
      d->liqLegendShown.insert(uid, s);
      if (!s.isEmpty()) vals.runs.append({s, v.muted, f12});
    } else if (res.extra.contains("heatmap")) {   // Liquidity heatmap: the book's state
      const QString s = d->heatmapStatus(uid);
      d->heatLegendShown.insert(uid, s);
      if (!s.isEmpty()) vals.runs.append({s, v.muted, f12});
    } else if (res.extra.contains("bigTrades")) {   // Big Trades: what is filtered right now
      const QString s = d->bigTradesStatus(uid);
      d->btLegendShown.insert(uid, s);
      if (!s.isEmpty()) vals.runs.append({s, v.text, f12});
    } else if (res.pending) vals.runs.append({QCoreApplication::translate("charts", "loading…"), v.muted, f12});
    else {
      const QJsonObject colors = ind.value("colors").toObject();
      const QFontMetricsF fm(f12);
      bool firstOut = true;
      for (const ind::Output& o : def->outputs) {
        // "<span>…</span>" joined by " ": an empty entry still adds its space
        if (!firstOut) {
          if (vals.runs.isEmpty()) vals.runs.append({QString(), v.text, f12, 0});
          vals.runs.last().marginRight += fm.horizontalAdvance(' ');
        }
        firstOut = false;
        const auto it = res.series.constFind(o.key);
        const double val = it != res.series.constEnd() && i >= 0 && i < it->size() ? (*it)[i] : NaN;
        if (std::isfinite(val)) {
          const QString cs = colors.value(o.key).toString();
          vals.runs.append({pane::fmtPrice(val), cssColor(cs.isEmpty() ? o.color : cs), f12, 0});
        }
      }
    }
    L::Cell acts;
    acts.acts = true;
    acts.runs.append({{}, {}, f12, 0, "settings", icg, QCoreApplication::translate("charts", "Settings")});
    acts.runs.append({{}, {}, f12, 0, "hide", ice, QCoreApplication::translate("charts", "Show/hide")});
    acts.runs.append({{}, {}, f12, 0, "remove", icx, QCoreApplication::translate("charts", "Remove")});
    r.cells << nm << vals << acts;
    rows.append(r);
  }
  for (const QString& note : notes) {
    L::Row r;
    r.kind = L::Row::Note;
    r.note = fmt::tx(note);
    r.noteColor = v.warn;
    rows.append(r);
  }
  if (const QString fpn = d->footprintNote(); !fpn.isEmpty()) {
    L::Row r;
    r.kind = L::Row::Note;
    r.note = fpn;
    r.noteColor = d->fp ? v.muted : v.warn;
    rows.append(r);
  }
  d->legend->setRows(rows);
  d->legend->setMaxWidth(std::max(0, width() - 90));
}

// ---------- indicators ----------
QString ChartPane::addIndicator(const QString& id, const Params& params) {
  const QString uid = pane::newUid();
  QJsonArray inds = cfg.value("indicators").toArray();
  inds.append(QJsonObject{{"uid", uid}, {"id", id}, {"params", params}});
  cfg["indicators"] = inds;
  render(false);
  app->persist();
  return uid;
}

void ChartPane::removeIndicator(const QString& uid) {
  QJsonArray kept;
  for (const QJsonValue& v : cfg.value("indicators").toArray())
    if (v.toObject().value("uid").toString() != uid) kept.append(v);
  cfg["indicators"] = kept;
  render(false);
  app->persist();
}

// ---------- drawings ----------
QJsonArray ChartPane::drawingItems() const { return d->drawings->itemsJson(); }

void ChartPane::setDrawingItems(const QJsonArray& items) {
  // app.saveDrawings() hands the same items back to every pane of the symbol; the pane that made them keeps its
  // selection (the JS skips it: `p.drawings.items !== items`).
  if (items == d->drawings->itemsJson()) return;
  d->drawings->setItems(items);
}

std::optional<QJsonObject> ChartPane::selectedDrawing() const {
  if (const auto* it = d->drawings->selectedItem()) return it->toJson();
  return std::nullopt;
}

void ChartPane::clearSelection() {
  d->drawings->selected.clear();
  d->drawings->update();
}

bool ChartPane::deleteSelectedDrawing() {
  DrawingLayer* dl = d->drawings.get();
  if (!dl->selectedItem()) return false;
  const QString id = dl->selected;
  dl->items.erase(std::remove_if(dl->items.begin(), dl->items.end(), [&](const auto& x) { return x.id == id; }), dl->items.end());
  dl->selected.clear();
  dl->update();
  app->saveDrawings(cfg.value("symbol").toString(), dl->itemsJson());
  return true;
}

void ChartPane::clearDrawings() {
  d->drawings->items.clear();
  d->drawings->selected.clear();
  d->drawings->temp.reset();
  d->drawings->update();
  app->saveDrawings(cfg.value("symbol").toString(), {});
}

void ChartPane::setInteractive(bool on) {
  d->chart->applyOptions([on](ChartOptions& o) {
    o.handleScroll.setAll(on);
    o.handleScale.setAll(on);
  });
  if (on && d->drawings->temp) {
    d->drawings->temp.reset();
    d->drawings->update();
  }
}

// ---------- live updates ----------
void ChartPane::startLive() {
  stopLive();
  if (replay || bars.isEmpty()) return;
  if (!isVisible()) {   // hidden: showEvent starts it (and reloads if a bar closed meanwhile)
    if (!d->liveStoppedAt) d->liveStoppedAt = QDateTime::currentMSecsSinceEpoch();
    return;
  }
  d->liveStoppedAt = 0;
  d->live->start(cfg.value("symbol").toString(), cfg.value("tf").toString(), provider);
}

void ChartPane::stopLive() {
  if (d->live) d->live->stop();
}

void ChartPane::merge(const Bars& newBars) {
  if (bars.isEmpty() || replay) return;
  // a hole between the loaded bars and the live ones (socket reconnected after a sleep, bars missed meanwhile):
  // reload rather than draw across it. Once per hole: when the reload does not fill it (a market closed overnight,
  // a provider that lags), the bar is merged as before instead of reloading again at every update.
  for (const Bar& b : newBars) {
    if (b.time <= bars.last().time) continue;
    if (double(b.time - bars.last().time) > 1.5 * tfSeconds(cfg.value("tf").toString()) && b.time != d->gapReloadBar) {
      d->gapReloadBar = b.time;
      load(true);
      return;
    }
    break;   // only the first newer bar can open a hole; the next ones follow it
  }
  bool changed = false;
  int first = int(bars.size());   // first bar replaced or added
  for (const Bar& b : newBars) {
    const Bar& last = bars.last();
    if (b.time == last.time) {
      bars.last() = b;
      first = std::min(first, int(bars.size()) - 1);
      changed = true;
    } else if (b.time > last.time) {
      first = std::min(first, int(bars.size()));
      bars.append(b);
      changed = true;
    }
  }
  if (!changed) return;
  d->nav->dataChanged(true);   // a tick: the mini curve is redrawn only when it shows
  const Bar last = bars.last();
  app->onPrice(cfg.value("symbol").toString(), last.close);
  static const QStringList simpleTypes = {"candles", "hollow", "bars", "line", "linemarkers", "step", "area", "baseline", "columns", "footprint"};
  const bool simple = simpleTypes.contains(cfg.value("type").toString());
  if (simple) {
    // every bar from the first changed one (a poll answer or a hidden spell can bring several): times stay one per
    // shown bar, and the main / volume series get each of them (JS pushes only the last one)
    shown = bars;
    if (d->fp)   // footprint: every bar with complete trades keeps the trades' own OHLC (see Impl::fpBar)
      for (Bar& b : shown) b = d->fpBar(b);
    const int from = std::min(first, int(times.size()));
    times.resize(from);
    for (int i = from; i < shown.size(); ++i) {
      times.append(toChart(shown[i].time));
      d->main->update(d->pointFor(shown[i], i), true);
      if (d->volume) d->volume->update(d->volPoint(shown[i]), true);
    }
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (now - d->mtfCheckedAt > 60000) {
    d->mtfCheckedAt = now;
    const auto keys = d->mtf.keys();
    for (const QString& tf : keys)
      if (now - d->mtf[tf].at > 60000) d->loadMtf(tf);   // the old values stay until the new ones land
  }
  if (!simple || now - d->lastFull > 1500) {
    d->lastFull = now;
    if (simple) {
      d->renderIndicators();
      updateLegend(d->hoverIndex);   // not the last bar while the crosshair rests on an older one
    } else render(false);
  }
}

// ---------- bar replay ----------
void ChartPane::startReplay(int index) {
  stopLive();
  if (bars.isEmpty()) return;
  if (!replay) d->replayStartedAt = QDateTime::currentMSecsSinceEpoch();
  // a follower becoming a leader: its "Synced replay" tag gives way to the full bar
  if (replay && replay->follower) d->removeReplayBar();
  replay = Replay{std::max(1, std::min(index, int(bars.size()) - 1)), false, 2, false};
  d->replayTimer.stop();
  if (d->replayBar) d->replayBar->setPlaying(false);   // "Jump to…" while playing: the new replay starts paused
  if (!d->replayBar) {
    auto* bar = new pane::ReplayBar(this, false, app->panes.size() > 1, truthy(app->settings.value("syncReplay")));
    bar->installEventFilter(this);
    for (QWidget* w : bar->findChildren<QWidget*>()) w->installEventFilter(this);
    bar->onAct = [this](const QString& r) {
      if (r == "back") replayStep(-1);
      if (r == "fwd") replayStep(1);
      if (r == "play") toggleReplay();
      if (r == "pick") app->setTool("replay-pick");
      if (r == "exit") stopReplay();
    };
    bar->onSync = [this](bool on) {
      app->settings["syncReplay"] = on;
      app->persist();
      if (on) app->onReplayStart(this);
      else app->onReplayStop(this);
    };
    bar->onSpeed = [this](double s) {
      if (!replay) return;
      replay->speed = s;
      if (replay->playing) {
        toggleReplay();
        toggleReplay();
      }
    };
    d->replayBar = bar;
    bar->show();
    bar->raise();
    bar->place(d->nav->height());
  }
  render(false);
  app->onReplayStart(this);
}

// Follow another chart's replay: show only the bars that had finished by `endTime`.
void ChartPane::replayTo(qint64 endTime) {
  const double sec = tfSeconds(cfg.value("tf").toString());
  int i = int(bars.size()) - 1;
  while (i > 0 && bars[i].time + sec > endTime) --i;
  if (!replay) {
    stopLive();
    d->replayStartedAt = QDateTime::currentMSecsSinceEpoch();
    replay = Replay{i, false, 2, true};
    d->removeReplayBar();
    auto* tag = new pane::ReplayBar(this, true);
    d->replayBar = tag;
    tag->show();
    tag->raise();
    tag->place(d->nav->height());
  } else replay->index = i;
  render(false);
}

void ChartPane::replayStep(int dir) {
  if (!replay) return;
  const int next = replay->index + dir;
  if (next < 1 || next >= bars.size()) {
    if (replay->playing) toggleReplay();
    return;
  }
  replay->index = next;
  render(false);
  app->onReplayBar(this, barJson(bars[next]));
}

void ChartPane::toggleReplay() {
  if (!replay) return;
  replay->playing = !replay->playing;
  d->replayTimer.stop();
  if (replay->playing && isVisible()) d->replayTimer.start(int(std::lround(1000 / replay->speed)));
  if (d->replayBar) d->replayBar->setPlaying(replay->playing);
}

void ChartPane::stopReplay() {
  d->replayTimer.stop();
  const bool wasFollower = replay && replay->follower;
  replay.reset();
  d->removeReplayBar();
  // live bars were not merged during the replay: when a bar closed meanwhile, reload (or a hole stays in the chart)
  const bool closed = !bars.isEmpty() && d->barClosedSince(d->replayStartedAt);
  d->replayStartedAt = 0;
  if (closed) load(false);
  else {
    render(true);
    startLive();
  }
  if (!wasFollower) app->onReplayStop(this);
}

// ---------- export ----------
QImage ChartPane::screenshot() const {
  QImage img = d->chart->takeScreenshot(true);
  QPainter p(&img);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.setFont(style::font(14, QFont::DemiBold));
  p.setPen(style::v().text);
  p.drawText(QPointF(12, 22), QString("%1 · %2").arg(pane::ticker(cfg.value("symbol").toString()), cfg.value("tf").toString()));
  return img;
}

bool ChartPane::isActivePane() const { return app->active == this; }

void ChartPane::syncActive() {
  for (ChartPane* p : app->panes)
    if (p && p->d) p->d->frame->update();
  if (d) d->frame->update();
}

// ---------- events ----------
void ChartPane::showEvent(QShowEvent* e) {
  QWidget::showEvent(e);
  d->placeOverlays();
  d->syncCountdownTimer();
  d->flowPauseTimer.stop();
  if (d->flowPaused) {   // order flow dropped while hidden: a render makes the footprint, Big Trades and heatmaps again
    d->flowPaused = false;
    if (!bars.isEmpty()) render(false);
  }
  if (!bars.isEmpty() && !replay) {
    // the feed was stopped while hidden (Deepwater pauses hidden pages): a bar that closed meanwhile was never
    // merged (a socket only sends the forming bar), so reload then, keeping the view; otherwise just resume
    const qint64 stopped = d->liveStoppedAt;
    d->liveStoppedAt = 0;
    if (d->barClosedSince(stopped)) load(true);
    else startLive();
  } else if (bars.isEmpty() && d->loadFailed) load(true);   // a load that failed while hidden (offline start)
  if (replay && replay->playing && !d->replayTimer.isActive()) d->replayTimer.start(int(std::lround(1000 / replay->speed)));
}

void ChartPane::hideEvent(QHideEvent* e) {
  QWidget::hideEvent(e);
  d->countdownTimer.stop();
  if (!bars.isEmpty() && !replay && !d->liveStoppedAt) d->liveStoppedAt = QDateTime::currentMSecsSinceEpoch();
  stopLive();
  d->replayTimer.stop();
  if (!d->flowPaused) d->flowPauseTimer.start();
}

void ChartPane::resizeEvent(QResizeEvent* e) {
  QWidget::resizeEvent(e);
  const int nh = std::min(pane::Navigator::kHeight, height() / 3);
  d->chart->setGeometry(0, 0, width(), height() - nh);
  d->nav->setGeometry(0, height() - nh, width(), nh);
  d->placeOverlays();
}

void ChartPane::mousePressEvent(QMouseEvent* e) {
  if (app->active != this) {
    app->setActive(this);
    syncActive();
  }
  QWidget::mousePressEvent(e);
}

bool ChartPane::eventFilter(QObject* o, QEvent* e) {
  if (!d || !d->chart) return QWidget::eventFilter(o, e);
  const auto t = e->type();
  if ((t == QEvent::MouseButtonPress || t == QEvent::MouseButtonDblClick) && app->active != this) {
    app->setActive(this);   // JS: el.addEventListener("mousedown", () => app.setActive(this), true)
    syncActive();
  }
  if (o != d->chart) return QWidget::eventFilter(o, e);
  switch (t) {
    case QEvent::MouseMove: {
      auto* me = static_cast<QMouseEvent*>(e);
      const QPointF lp = (d->chart->mapTo(this, me->position()) - QPointF(d->legend->pos()));
      const auto h = d->legend->hit(lp);
      const bool rowHover = h.interactive && h.row >= 0 &&
                            (d->legend->rows()[h.row].kind == pane::Legend::Row::Ind || d->legend->rows()[h.row].kind == pane::Legend::Row::Compare);
      d->legend->setHover(rowHover ? h.row : -1, h.act);
      if (h.interactive && !(me->buttons() & Qt::LeftButton)) {
        d->chart->setCursor(h.act.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
        d->chart->clearCrosshairPosition();
        d->hoverIndex = -1;   // the crosshair is gone: later refreshes show the last bar
        updateLegend();
        return true;
      }
      break;
    }
    case QEvent::Leave:
      d->legend->setHover(-1, {});
      break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
      auto* me = static_cast<QMouseEvent*>(e);
      d->legendPress = false;
      d->nativePress = false;
      const auto h = d->legend->hit((d->chart->mapTo(this, me->position()) - QPointF(d->legend->pos())));
      if (h.interactive && me->button() == Qt::LeftButton) {
        d->legendPress = true;
        d->pressAct = h.act;
        d->pressRow = h.row;
        return true;
      }
      if (me->button() == Qt::LeftButton && app->tool != "cursor") d->nativePress = true;
      break;
    }
    case QEvent::MouseButtonRelease: {
      auto* me = static_cast<QMouseEvent*>(e);
      if (d->legendPress) {
        d->legendPress = false;
        const auto h = d->legend->hit((d->chart->mapTo(this, me->position()) - QPointF(d->legend->pos())));
        if (!d->pressAct.isEmpty() && h.act == d->pressAct && h.row == d->pressRow) {
          const QString act = d->pressAct;
          const int row = d->pressRow;
          QTimer::singleShot(0, this, [this, act, row] { d->legendAct(row, act); });
        }
        return true;
      }
      if (d->nativePress && me->button() == Qt::LeftButton && app->tool != "cursor") {
        d->nativePress = false;
        const QPointF pos = me->position();
        QTimer::singleShot(0, this, [this, pos] { d->nativeClick(pos); });
      }
      d->nativePress = false;
      break;
    }
    default:
      break;
  }
  return QWidget::eventFilter(o, e);
}

// ---------- UI step (screenshots, manual checks): the active pane's navigator ----------
// nav:press:<f>  nav:move:<f>  nav:release:<f>  nav:hover:<f>  nav:drag:<f0>,<f1>  (f = fraction of the navigator's
// width; real mouse events) · nav:log (prints the chart's visible range and the navigator's domain) · nav:perf
// (60 window moves, each painted at once; prints ms per frame for the chart and the navigator).
static const bool kNavStep = registerUiStep("nav", [](ChartsApp* app, const QString& arg) {
  if (!app || !app->active) return false;
  ChartPane* pane = app->active;
  auto* nav = static_cast<pane::Navigator*>(pane->findChild<QWidget*>("chartNavigator", Qt::FindDirectChildrenOnly));
  if (!nav) return false;
  const QString what = arg.section(':', 0, 0);
  const QStringList f = arg.section(':', 1).split(',', Qt::SkipEmptyParts);
  auto log = [&](const char* tag) {
    const auto r = pane->chart()->timeScale()->getVisibleLogicalRange();
    const int last = pane->chart()->timeScale()->baseIndex();
    nav->repaint();   // the span shown is computed at paint
    const auto sp = nav->shownSpan();
    if (r) qInfo("nav %s: visible %.2f .. %.2f (%.1f bars), last bar %d, domain -0.5 .. %.1f, shown %.1f .. %.1f", tag, r->from, r->to,
                 r->to - r->from + 1, last, last + 0.5 + kFreshRightPad, sp ? sp->a : 0.0, sp ? sp->b : 0.0);
  };
  auto send = [&](QEvent::Type t, double fx, bool down) {
    const QPointF pt(fx * nav->width(), nav->height() / 2.0);
    QMouseEvent e(t, pt, nav->mapToGlobal(pt), t == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                  down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(nav, &e);
  };
  if (what == "log") { log("log"); return true; }
  if (what == "perf") {
    const auto r0 = pane->chart()->timeScale()->getVisibleLogicalRange();
    if (!r0) return false;
    QElapsedTimer t;
    qint64 chartNs = 0, navNs = 0;
    for (int k = 0; k < 60; ++k) {
      const double d = (k % 2 ? -1 : 1) * 3.0;
      pane->chart()->timeScale()->setVisibleLogicalRange({r0->from + d, r0->to + d});
      t.start();
      pane->chart()->repaint();
      chartNs += t.nsecsElapsed();
      t.start();
      nav->repaint();
      navNs += t.nsecsElapsed();
    }
    pane->chart()->timeScale()->setVisibleLogicalRange(*r0);
    qInfo("nav perf: %.2f ms/frame chart, %.3f ms/frame navigator (%.0f bars visible)", chartNs / 60e6, navNs / 60e6,
          r0->to - r0->from + 1);
    return true;
  }
  if (f.isEmpty()) return false;
  if (what == "view") {   // nav:view:<bars>: the last <bars> bars visible (zoom levels for screenshots)
    const int last = pane->chart()->timeScale()->baseIndex();
    const double n = std::max(1.0, f[0].toDouble());
    pane->chart()->timeScale()->setVisibleLogicalRange({last - n + 1, double(last)});
    log(qPrintable(arg));
    return true;
  }
  const double a = f[0].toDouble();
  if (what == "press") send(QEvent::MouseButtonPress, a, true);
  else if (what == "move") send(QEvent::MouseMove, a, true);
  else if (what == "release") send(QEvent::MouseButtonRelease, a, false);
  else if (what == "hover") send(QEvent::MouseMove, a, false);
  else if (what == "drag" && f.size() == 2) {
    const double b = f[1].toDouble();
    send(QEvent::MouseButtonPress, a, true);
    for (int k = 1; k <= 10; ++k) send(QEvent::MouseMove, a + (b - a) * k / 10, true);
    send(QEvent::MouseButtonRelease, b, false);
  } else return false;
  log(qPrintable(arg));
  return true;
});

// ---------- UI step (screenshots, measures): the active pane's footprint ----------
// fp:bars:<n> shows the last n bars · fp:mode:<bidask|delta|profile> · fp:log prints the footprint's state (detail level, rows, bars drawn, draw time,
// trades, history) · fp:perf repaints 30 frames panned by a bar and prints ms per frame for the chart and the layer.
std::optional<double> ChartPane::footprintTick() const {
  if (!d->fp || !d->fp->ready()) return std::nullopt;
  return d->fp->ticks().step;
}

bool ChartPane::footprintStep(const QString& arg) {
  const QString what = arg.section(':', 0, 0);
  TimeScale* ts = d->chart->timeScale();
  const double n = shown.size();
  if (what == "bars") {
    const double k = std::max(2.0, arg.section(':', 1).toDouble());
    ts->setVisibleLogicalRange({n - k, n - 1 + 2});
    return true;
  }
  if (what == "mode") {   // fp:mode:bidask|delta|profile — as the settings dialog saves it
    auto st = ::charts::flow::FootprintSettings::fromJson(cfg.value("footprint").toObject());
    QJsonObject o = st.toJson();
    o["mode"] = arg.section(':', 1);
    cfg["footprint"] = ::charts::flow::FootprintSettings::fromJson(o).toJson();
    render(false);
    return true;
  }
  if (!d->fp || !d->fpLayer) { qInfo("fp: no footprint on this pane"); return what == "log"; }
  auto detail = [&] {
    switch (d->fpLayer->lastDetail()) {
      case flow::FootprintLayer::Detail::Numbers: return "numbers";
      case flow::FootprintLayer::Detail::Colours: return "colours";
      default: return "candles";
    }
  };
  if (what == "log") {
    const auto& st = d->fp->status();
    const auto r = ts->getVisibleLogicalRange();
    qInfo("fp log: detail %s, rows of %lld ticks, %d bars drawn in %.2f ms, bar spacing %.1f px, visible %.0f bars; "
          "%zu candles, history %s, feed %s",
          detail(), (long long)d->fpLayer->lastGroupTicks(), d->fpLayer->lastBarsDrawn(), d->fpLayer->lastDrawMs(), ts->barSpacing(),
          r ? r->to - r->from + 1 : 0.0, d->fp->book().candles().size(),
          st.historyDone ? "done" : "loading", st.feed == data::FeedState::Open ? "open" : "not open");
    return true;
  }
  if (what == "perf") {
    const auto r0 = ts->getVisibleLogicalRange();
    if (!r0) return false;
    QElapsedTimer t;
    qint64 chartNs = 0;
    double layerMs = 0, worst = 0;
    for (int k = 0; k < 30; ++k) {
      const double dd = (k % 2 ? -1 : 1);
      ts->setVisibleLogicalRange({r0->from + dd, r0->to + dd});
      t.start();
      d->chart->repaint();
      const qint64 ns = t.nsecsElapsed();
      chartNs += ns;
      worst = std::max(worst, ns / 1e6);
      layerMs += d->fpLayer->lastDrawMs();
    }
    ts->setVisibleLogicalRange(*r0);
    qInfo("fp perf: %.2f ms/frame chart (worst %.2f), of which footprint %.2f ms; detail %s, %d bars drawn", chartNs / 30e6, worst,
          layerMs / 30, detail(), d->fpLayer->lastBarsDrawn());
    return true;
  }
  return false;
}

// big:<input>:<value> (min, max, percentile, window, mode Auto|Manual, volatility On|Off) sets an input of the pane's first Big Trades (adding one if there is none) · big:log prints bubbles
// known, bubbles drawn and draw time of each Big Trades layer.
bool ChartPane::bigTradesStep(const QString& arg) {
  const QString what = arg.section(':', 0, 0);
  static const QStringList textInputs = {"mode", "volatility", "aggPrice", "shape", "showVolume", "alert"};
  if (QStringList{"min", "max", "percentile", "window", "aggWindow", "minSize", "maxSize", "minOpacity", "alertFactor"}.contains(what) || textInputs.contains(what)) {
    QJsonArray inds = cfg.value("indicators").toArray();
    int k = 0;
    for (; k < inds.size(); ++k)
      if (inds[k].toObject().value("id").toString() == "bigtrades") break;
    if (k == inds.size()) {
      addIndicator("bigtrades");
      inds = cfg.value("indicators").toArray();
      for (k = 0; k < inds.size() && inds[k].toObject().value("id").toString() != "bigtrades"; ++k) {}
    }
    QJsonObject o = inds[k].toObject();
    QJsonObject prm = o.value("params").toObject();
    if (textInputs.contains(what)) prm[what] = arg.section(':', 1);
    else prm[what] = arg.section(':', 1).toDouble();
    o["params"] = prm;
    inds[k] = o;
    cfg["indicators"] = inds;
    render(false);
    return true;
  }
  if (what == "hover") {   // a real mouse move onto the largest marker on screen; prints the tooltip it opened
    for (const auto& [uid, e] : d->bt) {
      const auto c = e.layer ? e.layer->largestCenter() : std::nullopt;
      if (!c) continue;
      const QPointF pos = QPointF(d->chart->panes()[0]->rect().topLeft()) + *c;
      QMouseEvent ev(QEvent::MouseMove, pos, d->chart->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(d->chart, &ev);
      qInfo("big hover: tooltip %s: %s", QToolTip::isVisible() ? "shown" : "not shown", qPrintable(QToolTip::text()));
      return true;
    }
    qInfo("big hover: no marker on screen");
    return true;
  }
  if (what == "log") {
    for (const auto& [uid, e] : d->bt)
      qInfo("big log %s: %zu bubbles known, %d drawn in %.2f ms, %s, %d alerts", qPrintable(uid), e.model->bubbles().size(),
            e.layer->lastDrawn(), e.layer->lastDrawMs(), e.model->computing() ? "computing" : "ready", e.model->alertsFired());
    if (d->bt.empty()) qInfo("big log: no Big Trades on this pane");
    return true;
  }
  return false;
}

// heat:add adds a Liquidity heatmap · heat:set:<input>=<value> · heat:replay:N starts bar replay N bars back · heat:log prints the layer's last frame (full or incremental, columns redrawn,
// records read, contrast, ms) and the stream's state · heat:perf repaints 30 frames panned by a bar.
bool ChartPane::heatmapStep(const QString& arg) {
  const QString what = arg.section(':', 0, 0);
  if (what == "add") { addIndicator("heatmap"); return true; }
  if (what == "replay") { startReplay(int(bars.size()) - std::max(1, arg.section(':', 1, 1).toInt())); return true; }
  if (what == "set") {   // heat:set:<input>=<value> on the pane's first heatmap (numbers stay numbers)
    const QString kv = arg.section(':', 1), key = kv.section('=', 0, 0), val = kv.section('=', 1);
    QJsonArray inds = cfg.value("indicators").toArray();
    int k = 0;
    while (k < inds.size() && inds[k].toObject().value("id").toString() != "heatmap") ++k;
    if (k == inds.size()) return false;
    QJsonObject o = inds[k].toObject();
    QJsonObject prm = o.value("params").toObject();
    bool isNum = false;
    const double x = val.toDouble(&isNum);
    prm[key] = isNum ? QJsonValue(x) : QJsonValue(val);
    o["params"] = prm;
    inds[k] = o;
    cfg["indicators"] = inds;
    render(false);
    return true;
  }
  if (d->heat.empty()) { qInfo("heat: no heatmap on this pane"); return what == "log"; }
  auto& l = *d->heat.begin()->second;
  if (what == "log") {
    const auto& s = l.stats();
    const auto& st = l.stream()->status();
    qInfo("heat log: %s frame, %d columns redrawn, %d records, contrast codes %d–%d, %.2f ms (raster %.2f, image %.2f); "
          "stream %s synced %d, live %zu, history %zu, recording %d, recorded %lld; legend \"%s\"",
          s.full ? "full" : "incremental", s.columnsRedrawn, s.records, int(s.low), int(s.high), s.lastMs, s.rasterMs, s.imageMs,
          qPrintable(l.stream()->market().key()), int(st.synced), l.stream()->live().size(), l.stream()->history().size(),
          int(l.stream()->recording()), (long long)l.stream()->recorded(), qPrintable(d->heatmapStatus(d->heat.begin()->first)));
    return true;
  }
  if (what == "perf") {
    TimeScale* ts = d->chart->timeScale();
    const auto r0 = ts->getVisibleLogicalRange();
    if (!r0) return false;
    QElapsedTimer t;
    qint64 chartNs = 0;
    double layerMs = 0, worst = 0;
    for (int k = 0; k < 30; ++k) {
      const double dd = (k % 2 ? -1 : 1);
      ts->setVisibleLogicalRange({r0->from + dd, r0->to + dd});
      t.start();
      d->chart->repaint();
      chartNs += t.nsecsElapsed();
      worst = std::max(worst, t.nsecsElapsed() / 1e6);
      layerMs += l.stats().lastMs;
    }
    ts->setVisibleLogicalRange(*r0);
    // and 30 frames with the view still (new data only: the incremental path)
    double stillMs = 0;
    for (int k = 0; k < 30; ++k) {
      d->chart->update();
      l.requestRepaint();
      t.start();
      d->chart->repaint();
      stillMs += l.stats().lastMs;
    }
    qInfo("heat perf: panning %.2f ms/frame chart (worst %.2f), heatmap %.2f ms; still view: heatmap %.2f ms/frame", chartNs / 30e6, worst,
          layerMs / 30, stillMs / 30);
    return true;
  }
  return false;
}

// liq:add adds a Liquidation heatmap · liq:set:<input>=<value> · liq:log prints the result and the layer's last frame ·
// liq:perf repaints 30 frames panned by a bar.
bool ChartPane::liqMapStep(const QString& arg) {
  const QString what = arg.section(':', 0, 0);
  if (what == "add") { addIndicator("liqmap"); return true; }
  if (what == "set") {
    const QString kv = arg.section(':', 1), key = kv.section('=', 0, 0), val = kv.section('=', 1);
    QJsonArray inds = cfg.value("indicators").toArray();
    int k = 0;
    while (k < inds.size() && inds[k].toObject().value("id").toString() != "liqmap") ++k;
    if (k == inds.size()) return false;
    QJsonObject o = inds[k].toObject();
    QJsonObject prm = o.value("params").toObject();
    bool isNum = false;
    const double x = val.toDouble(&isNum);
    prm[key] = isNum ? QJsonValue(x) : QJsonValue(val);
    o["params"] = prm;
    inds[k] = o;
    cfg["indicators"] = inds;
    render(false);
    return true;
  }
  if (d->liq.empty()) { qInfo("liq: no liquidation heatmap on this pane"); return what == "log"; }
  auto& e = d->liq.begin()->second;
  if (what == "log") {
    const auto res = e.model->result();
    const auto& st = e.model->stats()->status();
    const auto& sl = e.layer->stats();
    double restL = 0, restS = 0, gone = 0;
    if (res && res->model.bars()) {
      for (double x : res->model.longs()) restL += x;
      for (double x : res->model.shorts()) restS += x;
      for (int i = 0; i < res->model.bars(); ++i) gone += res->model.liquidatedLongs(i) + res->model.liquidatedShorts(i);
    }
    qInfo("liq log: stats %s OI %zu ratio %zu liquidations %zu (requests %d, errors %d, live %d); model %d bars, OI on %d, %d rows of %.2f from %.2f, "
          "computed in %.1f ms; resting longs %.1f shorts %.1f, liquidated %.1f, out of grid %.1f; frame %s, %d columns, %d dots, contrast %d–%d, %.2f ms; "
          "legend \"%s\"",
          qPrintable(e.model->symbol()), e.model->stats()->openInterest().size(), e.model->stats()->ratios().size(),
          e.model->stats()->liquidations().size(), st.requests, st.errors, int(st.live == data::FeedState::Open), res ? res->model.bars() : 0,
          res ? res->oiKnown : 0, res ? res->model.grid().rows : 0, res ? res->model.grid().rowHeight : 0, res ? res->model.grid().priceLow : 0,
          e.model->lastComputeMs(), restL, restS, gone, res ? res->model.outOfGrid() : 0, sl.full ? "full" : "cached", sl.columns, sl.dots,
          int(sl.low), int(sl.high), sl.lastMs, qPrintable(d->liqStatus(d->liq.begin()->first)));
    return true;
  }
  if (what == "perf") {
    TimeScale* ts = d->chart->timeScale();
    const auto r0 = ts->getVisibleLogicalRange();
    if (!r0) return false;
    QElapsedTimer t;
    qint64 chartNs = 0;
    double layerMs = 0, worst = 0;
    for (int k = 0; k < 30; ++k) {
      const double dd = (k % 2 ? -1 : 1);
      ts->setVisibleLogicalRange({r0->from + dd, r0->to + dd});
      t.start();
      d->chart->repaint();
      chartNs += t.nsecsElapsed();
      worst = std::max(worst, t.nsecsElapsed() / 1e6);
      layerMs += e.layer->stats().lastMs;
    }
    ts->setVisibleLogicalRange(*r0);
    double stillMs = 0;
    for (int k = 0; k < 30; ++k) {
      e.layer->requestRepaint();
      d->chart->repaint();
      stillMs += e.layer->stats().lastMs;
    }
    qInfo("liq perf: panning %.2f ms/frame chart (worst %.2f), liquidation heatmap %.2f ms; still view %.2f ms/frame", chartNs / 30e6, worst,
          layerMs / 30, stillMs / 30);
    return true;
  }
  return false;
}

// smc:add adds Liquidity levels (SMC) · smc:set:<input>=<value> · smc:log prints the levels and timings · smc:perf
bool ChartPane::smcStep(const QString& arg) {
  const QString what = arg.section(':', 0, 0);
  if (what == "add") { addIndicator("smcliq"); return true; }
  if (what == "set") {
    const QString kv = arg.section(':', 1), key = kv.section('=', 0, 0), val = kv.section('=', 1);
    QJsonArray inds = cfg.value("indicators").toArray();
    int k = 0;
    while (k < inds.size() && inds[k].toObject().value("id").toString() != "smcliq") ++k;
    if (k == inds.size()) return false;
    QJsonObject o = inds[k].toObject();
    QJsonObject prm = o.value("params").toObject();
    bool isNum = false;
    const double x = val.toDouble(&isNum);
    prm[key] = isNum ? QJsonValue(x) : QJsonValue(val);
    o["params"] = prm;
    inds[k] = o;
    cfg["indicators"] = inds;
    render(false);
    return true;
  }
  if (d->smc.empty()) { qInfo("smc: no liquidity levels on this pane"); return what == "log"; }
  auto& l = *d->smc.begin()->second;
  if (what == "log") {
    QHash<QString, int> kinds;
    int swept = 0;
    for (const auto& s : l.levels()) {
      kinds[flow::smcLabel(s.level).section(' ', 0, 0)] += 1;
      swept += s.level.swept;
    }
    QStringList parts;
    for (auto it = kinds.begin(); it != kinds.end(); ++it) parts << it.key() + " " + QString::number(it.value());
    parts.sort();
    const auto& st = l.stats();
    qInfo("smc log: %d levels (%d active, %d swept): %s; last update %.2f ms (full runs %d, bars fed %d), drawn %d in %.2f ms", st.total,
          st.active, swept, qPrintable(parts.join(", ")), st.computeMs, st.fullRuns, st.fed, st.drawn, st.drawMs);
    // the five strongest active levels
    std::vector<const flow::SmcLayer::Shown*> act;
    for (const auto& s : l.levels())
      if (!s.level.swept) act.push_back(&s);
    std::sort(act.begin(), act.end(), [](auto* a, auto* b) { return a->score > b->score; });
    for (size_t i = 0; i < std::min<size_t>(5, act.size()); ++i)
      qInfo("smc top: %s %.2f score %.2f since %s", qPrintable(flow::smcLabel(act[i]->level)), act[i]->level.price, act[i]->score,
            qPrintable(QDateTime::fromSecsSinceEpoch(act[i]->fromTime, QTimeZone::UTC).toString("MM-dd HH:mm")));
    return true;
  }
  if (what == "perf") {
    TimeScale* ts = d->chart->timeScale();
    const auto r0 = ts->getVisibleLogicalRange();
    if (!r0) return false;
    QElapsedTimer t;
    qint64 chartNs = 0;
    double layerMs = 0;
    for (int k = 0; k < 30; ++k) {
      const double dd = (k % 2 ? -1 : 1);
      ts->setVisibleLogicalRange({r0->from + dd, r0->to + dd});
      t.start();
      d->chart->repaint();
      chartNs += t.nsecsElapsed();
      layerMs += l.stats().drawMs;
    }
    ts->setVisibleLogicalRange(*r0);
    qInfo("smc perf: panning %.2f ms/frame chart, levels %.2f ms", chartNs / 30e6, layerMs / 30);
    return true;
  }
  return false;
}

static const bool kSmcStep = registerUiStep("smc", [](ChartsApp* app, const QString& arg) {
  return app && app->active && app->active->smcStep(arg);
});

static const bool kLiqStep = registerUiStep("liq", [](ChartsApp* app, const QString& arg) {
  return app && app->active && app->active->liqMapStep(arg);
});

static const bool kHeatStep = registerUiStep("heat", [](ChartsApp* app, const QString& arg) {
  return app && app->active && app->active->heatmapStep(arg);
});

static const bool kBigStep = registerUiStep("big", [](ChartsApp* app, const QString& arg) {
  return app && app->active && app->active->bigTradesStep(arg);
});

static const bool kFpStep = registerUiStep("fp", [](ChartsApp* app, const QString& arg) {
  return app && app->active && app->active->footprintStep(arg);
});

}  // namespace charts::ui
