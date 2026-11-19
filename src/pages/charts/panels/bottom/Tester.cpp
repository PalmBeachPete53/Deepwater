// Charts tab — Strategy tester. Port of quant-terminal public/js/panels.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Tester.h"
#include "charts/core/Indicators.h"
#include "charts/core/Series.h"
#include "charts/data/Imports.h"
#include "charts/data/Providers.h"
#include "charts/data/Store.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/RuleBuilder.h"
#include "pages/charts/panels/bottom/Common.h"
#include "charts/data/CustomFiles.h"
#include "charts/core/PineStrategy.h"
#include "pages/charts/panels/bottom/QuantChecks.h"
#include "technicaldata/TechnicalDataBars.h"
#include "technicaldata/TechnicalDataStore.h"
#include <QtConcurrent/QtConcurrentRun>
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Series.h"
#include "pages/charts/plot/TimeScale.h"
#include "pages/charts/plot/PriceScale.h"
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <cmath>
#include <memory>

namespace charts::ui::bottom {

static const QStringList kGroups = {"entryLong", "exitLong", "entryShort", "exitShort"};

const QStringList& settingsOrder() {
  static const QStringList k = {"direction", "stopLossPct", "takeProfitPct", "trailingStopPct", "exitMatch",
                                "maxBarsInTrade", "initialCapital", "positionPct", "feePct", "slippagePct",
                                "spreadPct", "intrabar"};
  return k;
}

// ---------- JSON.stringify(value, null, 2) with the JS key order ----------
namespace {
QString jsonString(const QString& s) {
  QString o = "\"";
  for (QChar c : s) {
    switch (c.unicode()) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      case '\b': o += "\\b"; break;
      case '\f': o += "\\f"; break;
      default:
        if (c.unicode() < 0x20) o += QString("\\u%1").arg(int(c.unicode()), 4, 16, QChar('0'));
        else o += c;
    }
  }
  return o + "\"";
}

QStringList keyOrder(const QJsonObject& o, const QStringList& preferred) {
  QStringList out;
  for (const QString& k : preferred)
    if (o.contains(k)) out << k;
  for (const QString& k : o.keys())
    if (!out.contains(k)) out << k;
  return out;
}

void writeJson(QString& out, const QJsonValue& v, int level, const QString& key, const QStringList& indOrder) {
  const QString pad(2 * (level + 1), ' '), padEnd(2 * level, ' ');
  if (v.isObject()) {
    const QJsonObject o = v.toObject();
    if (o.isEmpty()) { out += "{}"; return; }
    static const QStringList pref = [] {
      QStringList p = {"name", "indicators"};
      p << kGroups << settingsOrder();
      p << QStringList{"left", "op", "right", "id", "params", "output", "line", "t1", "p1", "t2", "p2", "extend"};
      return p;
    }();
    const QStringList keys = key == "indicators" && level == 1 ? keyOrder(o, indOrder) : keyOrder(o, pref);
    out += "{\n";
    for (int i = 0; i < keys.size(); ++i) {
      out += pad + jsonString(keys[i]) + ": ";
      writeJson(out, o.value(keys[i]), level + 1, keys[i], indOrder);
      out += i + 1 < keys.size() ? ",\n" : "\n";
    }
    out += padEnd + "}";
  } else if (v.isArray()) {
    const QJsonArray a = v.toArray();
    if (a.isEmpty()) { out += "[]"; return; }
    out += "[\n";
    for (int i = 0; i < a.size(); ++i) {
      out += pad;
      writeJson(out, a[i], level + 1, {}, indOrder);
      out += i + 1 < a.size() ? ",\n" : "\n";
    }
    out += padEnd + "]";
  } else if (v.isString()) {
    out += jsonString(v.toString());
  } else if (v.isDouble()) {
    const double d = v.toDouble();
    out += std::isfinite(d) ? charts::store::numberToString(d) : QStringLiteral("null");
  } else if (v.isBool()) {
    out += v.toBool() ? "true" : "false";
  } else {
    out += "null";
  }
}

// Keys of strategy.indicators in the order the JS compileRules inserted them (rules walked in order).
QStringList indicatorOrderOf(const QJsonObject& strat) {
  QStringList order;
  const QJsonObject inds = strat.value("indicators").toObject();
  auto see = [&](const QJsonValue& x) {
    if (!x.isString()) return;
    const QString k = x.toString().section('.', 0, 0);
    if (inds.contains(k) && !order.contains(k)) order << k;
  };
  for (const QString& g : kGroups)
    for (const auto& r : strat.value(g).toArray()) {
      see(r.toObject().value("left"));
      see(r.toObject().value("right"));
    }
  for (const QString& k : inds.keys())
    if (!order.contains(k)) order << k;
  return order;
}

QString settingText(const QJsonValue& v) {
  if (v.isDouble()) return charts::store::numberToString(v.toDouble());
  if (v.isString()) return v.toString();
  if (v.isBool()) return v.toBool() ? "true" : "false";
  if (v.isNull()) return "null";
  return {};
}

QString pcText(double v) {   // pc(v): v == null ? "—" : `${v >= 0 ? "+" : ""}${v}%`
  if (std::isnan(v)) return QStringLiteral("—");
  return (v >= 0 ? "+" : "") + charts::store::numberToString(v) + "%";
}
QString numText(double v) { return std::isnan(v) ? QStringLiteral("—") : charts::store::numberToString(v); }
}  // namespace

// ==========================================================================================
TesterTab::TesterTab(BottomPanel* panel) : QObject(panel), P(panel) {
  tState.settings = QJsonObject{{"direction", "long"}, {"stopLossPct", 0}, {"takeProfitPct", 0},
                                {"trailingStopPct", 0}, {"exitMatch", "all"}, {"maxBarsInTrade", 0},
                                {"initialCapital", 10000}, {"positionPct", 100}, {"feePct", 0.1},
                                {"slippagePct", 0.05}, {"spreadPct", 0}, {"intrabar", "worst"}};
  strategyToUi(bt::exampleStrategy("EMA cross (9/21)"));
}

void TesterTab::strategyToUi(const QJsonObject& s) {
  const QJsonObject ind = s.value("indicators").toObject();
  QJsonObject groups;
  for (const QString& k : kGroups) groups[k] = rules::toRules(s.value(k).toArray(), ind);
  tState.groups = groups;
  for (const char* k : {"stopLossPct", "takeProfitPct", "trailingStopPct", "maxBarsInTrade"}) {
    const QJsonValue v = s.value(k);
    tState.settings[k] = bt::jsTruthy(v) ? v : QJsonValue(0);   // s[k] || 0
  }
  tState.settings["exitMatch"] = s.value("exitMatch").toString() == "any" ? "any" : "all";
}

QJsonObject TesterTab::readTester(QStringList* order) {
  QList<QPair<QString, QJsonArray>> groups;
  QJsonObject g;
  for (const QString& k : kGroups) {
    const QJsonArray rs = lists_.value(k) ? lists_.value(k)->rules() : tState.groups.value(k).toArray();
    groups.append({k, rs});
    g[k] = rs;
  }
  tState.groups = g;
  for (auto it = inputs_.begin(); it != inputs_.end(); ++it) {
    if (!it.value()) continue;
    if (auto* e = qobject_cast<QLineEdit*>(it.value().data())) tState.settings[it.key()] = inputNumber(e);
    else if (auto* c = qobject_cast<QComboBox*>(it.value().data())) tState.settings[it.key()] = optValue(c);
  }
  QJsonObject out = rules::compileRules(groups);
  for (auto it = tState.settings.begin(); it != tState.settings.end(); ++it) out[it.key()] = it.value();
  if (order) *order = indicatorOrderOf(out);
  return out;
}

QString TesterTab::strategyJsonText() {
  QStringList order;
  const QJsonObject s = readTester(&order);
  QString out;
  writeJson(out, s, 0, {}, order);
  return out;
}

// ---------- renderTester ----------
void TesterTab::render() {
  // The old form's last edit (a field losing focus as its body hides) must not overwrite what the caller just put in
  // tState (a preset, the JSON editor): the form handlers below only write while their body is the current one.
  body_ = nullptr;
  QWidget* body = P->newBody();
  body_ = body;
  lists_.clear();
  inputs_.clear();
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  TwoCol tc = twoCol(body);
  bl->addWidget(tc.root);
  Col left{tc.leftLay};

  // preset select + Run on chart
  QList<Opt> opts{{QCoreApplication::translate("charts", "Load a strategy…"), ""}};
  opts.append({QCoreApplication::translate("charts", "Examples"), {}, true});
  for (const auto& [n, s] : bt::EXAMPLE_STRATEGIES()) opts.append({fmt::tx(n), "ex:" + n, false, true});
  const QJsonObject saved = P->app->state.value("strategies").toObject();
  if (!saved.isEmpty()) {
    opts.append({QCoreApplication::translate("charts", "Saved"), {}, true});
    for (const QString& n : saved.keys()) opts.append({n, "sv:" + n, false, true});
  }
  bool pineGroup = false;
  for (const auto& e : data::listCustom()) {
    if (!e.file.endsWith(".pine") || !pine::isStrategySource(data::readCustom(e.file))) continue;
    if (!pineGroup) { opts.append({QCoreApplication::translate("charts", "Pine strategies"), {}, true}); pineGroup = true; }
    opts.append({e.file, "pine:" + e.file, false, true});
  }
  QComboBox* preset = optSelect(opts, pineFile_.isEmpty() ? QString() : "pine:" + pineFile_);
  auto* run = style::btn(QCoreApplication::translate("charts", "Run on chart"), "primary");
  left.add(rowFlex({preset, run}));
  barsSel_ = new QComboBox;
  barsSel_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  left.add(rowFlex({style::field(QCoreApplication::translate("charts", "Bars"), barsSel_)}));
  connect(barsSel_, &QComboBox::activated, this, [this](int i) {
    if (!barsSel_) return;
    P->app->settings["testerStoredBars"] = barsSel_->itemData(i).toString() == "stored";
    P->app->persist();
  });
  refreshBarsSource();
  if (!pineFile_.isEmpty())
    left.add(hint(QCoreApplication::translate("charts", "<b>%1</b> is a Pine strategy: its own entries, exits, position size and costs apply, filled by "
                                              "PineTS as TradingView fills orders. Pick another strategy to go back to the rules. To change its "
                                              "inputs, edit the file in the Indicator editor.").arg(fmt::esc(pineFile_))),
             kPMargin, kPMargin);
  // a Pine strategy brings its own rules, sizing and costs: the rule editor is not shown
  QPushButton *impBars = nullptr, *impSig = nullptr, *spread = nullptr, *save = nullptr, *json = nullptr;
  if (pineFile_.isEmpty()) {
    left.add(hint(QCoreApplication::translate("charts", "Entry conditions must all be true. Exits can need all of theirs or just one. Leave a section "
                    "empty to skip it. Signals use the bar's close and fill at the next bar's open.")),
             kPMargin, kPMargin);
    const QList<QPair<QString, QString>> titles = {{"entryLong", QCoreApplication::translate("charts", "Buy (enter long) when")},
                                                   {"exitLong", QCoreApplication::translate("charts", "Sell (exit long) when")},
                                                   {"entryShort", QCoreApplication::translate("charts", "Enter short when")},
                                                   {"exitShort", QCoreApplication::translate("charts", "Exit short when")}};
    for (const auto& [k, title] : titles) {
      left.add(h4(title), kH4Top, kH4Bottom);
      auto* list = new rules::RuleList(tState.groups.value(k).toArray());
      list->setOuterMargins(false);   // margins come from Col (.rules margin 4px 0 10px)
      lists_[k] = list;
      left.add(list, kRulesTop, kRulesBottom);
      // Keep every edit in tState as it happens: a re-render (collapse, another tab, theme change, PUT /api/state)
      // rebuilds the form from tState, and only Run / Save used to read the form.
      const QString key = k;
      connect(list, &rules::RuleList::changed, this, [this, body, key, list] {
        if (body_ == body) tState.groups[key] = list->rules();
      });
      auto* add = style::btn(QCoreApplication::translate("charts", "+ Condition"), "small");
      connect(add, &QPushButton::clicked, list, [this, body, key, list] {
        list->addRule(rules::defaultRule());
        if (body_ == body) tState.groups[key] = list->rules();
      });
      left.add(rowFlex({add}));
    }
    auto num = [&](const QString& k, const QString& label) {
      QLineEdit* e = numInput(settingText(tState.settings.value(k)));
      inputs_[k] = e;
      connect(e, &QLineEdit::textChanged, this, [this, body, k, e] {   // kept for a re-render (see the rules above)
        if (body_ == body) tState.settings[k] = inputNumber(e);
      });
      return style::field(label, e);
    };
    auto sel = [&](const QString& k, const QList<Opt>& o) {
      QComboBox* c = optSelect(o, tState.settings.value(k).toString());
      inputs_[k] = c;
      connect(c, &QComboBox::activated, this, [this, body, k, c] {
        if (body_ == body) tState.settings[k] = optValue(c);
      });
      return c;
    };
    left.add(h4(QCoreApplication::translate("charts", "Exits")), kH4Top, kH4Bottom);
    left.add(rowFlex({style::field(QCoreApplication::translate("charts", "Exit when"), sel("exitMatch", {{QCoreApplication::translate("charts", "all exit conditions are true"), "all"},
                                                                      {QCoreApplication::translate("charts", "any one exit condition is true"), "any"}})),
                      num("maxBarsInTrade", QCoreApplication::translate("charts", "Exit after this many bars (0 = off)"))}));
    left.add(h4(QCoreApplication::translate("charts", "Risk")), kH4Top, kH4Bottom);
    left.add(rowFlex({num("stopLossPct", QCoreApplication::translate("charts", "Stop loss %")), num("takeProfitPct", QCoreApplication::translate("charts", "Take profit %")),
                      num("trailingStopPct", QCoreApplication::translate("charts", "Trailing stop %"))}));
    left.add(h4(QCoreApplication::translate("charts", "Money")), kH4Top, kH4Bottom);
    spread = style::btn(QCoreApplication::translate("charts", "Live spread"), "small");
    spread->setToolTip(QCoreApplication::translate("charts", "Read the live bid and ask (crypto)"));
    left.add(rowFlex({num("initialCapital", QCoreApplication::translate("charts", "Starting capital")), num("positionPct", QCoreApplication::translate("charts", "Position size %")),
                      num("feePct", QCoreApplication::translate("charts", "Fee % per side")), num("slippagePct", QCoreApplication::translate("charts", "Slippage % per side")),
                      num("spreadPct", QCoreApplication::translate("charts", "Spread % (bid to ask)")), spread}));
    left.add(h4(QCoreApplication::translate("charts", "Fills inside a bar")), kH4Top, kH4Bottom);
    left.add(rowFlex({style::field(
        QCoreApplication::translate("charts", "Stops and targets"),
        sel("intrabar", {{QCoreApplication::translate("charts", "Stop first when a bar touches both (cautious)"), "worst"},
                         {QCoreApplication::translate("charts", "TradingView's path: open, nearer extreme, far extreme, close"), "ohlc"},
                         {QCoreApplication::translate("charts", "Bar magnifier: real lower-timeframe bars inside each bar"), "magnifier"}}))}));
    left.add(h4(QCoreApplication::translate("charts", "Your own data")), kH4Top, kH4Bottom);
    impBars = style::btn(QCoreApplication::translate("charts", "Import bars (CSV)"), "small");
    impSig = style::btn(QCoreApplication::translate("charts", "Import signal (CSV)"), "small");
    left.add(rowFlex({impBars, impSig}));
    left.add(hint(QCoreApplication::translate("charts", "Bars: time, open, high, low, close and volume, one symbol per file (an Alpaca export works as it "
                    "is). They chart as CSV:NAME on their own timeframe and longer. Signal: time and position, 1 long, 0 "
                    "flat, -1 short, from that bar's close. It loads here as rules, so Run on chart and the quant checks "
                    "test your strategy as written.")),
             kPMargin, kPMargin);
    save = style::btn(QCoreApplication::translate("charts", "Save strategy"));
    json = style::btn(QCoreApplication::translate("charts", "Edit as JSON"));
    left.add(rowFlex({save, json}), 12, 0);
  }
  left.finish();

  out_ = tc.right;
  outArea_ = tc.rightArea;
  outLay_ = tc.rightLay;
  outLay_->addWidget(hint(QCoreApplication::translate("charts", "Pick or build a strategy, then press <b>Run on chart</b>. It runs on the active chart's "
                            "symbol and timeframe.")));
  outLay_->addStretch(1);

  connect(preset, &QComboBox::activated, this, [this, preset](int) { loadPreset(optValue(preset)); });
  connect(run, &QPushButton::clicked, this, [this] { runTester(); });
  if (impBars) {
    connect(impBars, &QPushButton::clicked, this, [this] { importFile("bars"); });
    connect(impSig, &QPushButton::clicked, this, [this] { importFile("signal"); });
    connect(spread, &QPushButton::clicked, this, [this] { liveSpread(); });
    connect(save, &QPushButton::clicked, this, [this] { saveStrategy(); });
    connect(json, &QPushButton::clicked, this, [this] { editJson(); });
  }
  if (lastResult) showResult();
}

void TesterTab::applyLoaded(const QJsonObject& s) {
  strategyToUi(s);
  for (const char* k : {"initialCapital", "positionPct", "feePct", "slippagePct", "spreadPct", "intrabar"})
    if (s.contains(k) && !s.value(k).isNull()) tState.settings[k] = s.value(k);
}

void TesterTab::loadPreset(const QString& v) {
  if (v.isEmpty()) return;
  if (v.startsWith("pine:")) {
    pineFile_ = v.mid(5);
    render();
    return;
  }
  pineFile_.clear();
  const QJsonObject s = v.startsWith("ex:") ? bt::exampleStrategy(v.mid(3))
                                            : P->app->state.value("strategies").toObject().value(v.mid(3)).toObject();
  applyLoaded(s);
  tState.name = v.mid(3);
  render();
}

// ---------- Your own data ----------
void TesterTab::importFile(const QString& kind) {
  const QString path = QFileDialog::getOpenFileName(P->window(), QString(), QString(), QCoreApplication::translate("charts", "CSV (*.csv *.txt)"));
  if (path.isEmpty()) return;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    P->app->toast(fmt::esc(f.errorString()), "err");
    return;
  }
  const QString name = QFileInfo(path).fileName();
  P->app->toast(QString(QCoreApplication::translate("charts", "Reading %1…")).arg(fmt::esc(name)));
  const QString text = QString::fromUtf8(f.readAll());
  QPointer<TesterTab> self(this);
  if (kind == "bars") {
    auto* w = new QFutureWatcher<QPair<data::ImportSummary, QString>>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w] {
      w->deleteLater();
      const auto [r, err] = w->result();
      if (!err.isEmpty()) return P->app->toast(fmt::esc(fmt::tx(err)), "err");
      ChartPane* pane = P->app->active;
      try {
        if (pane && series::parseTf(pane->cfg.value("tf").toString()).seconds < series::parseTf(r.tf).seconds)
          pane->cfg["tf"] = r.tf;
      } catch (...) {
      }
      P->app->setSymbol(r.symbol);
      P->app->toast(QString(QCoreApplication::translate("charts", "Imported %1 %2 bars as %3%4. Pick any timeframe from %2 up."))
                        .arg(localeInt(r.bars), fmt::esc(r.tf), fmt::esc(r.symbol),
                             r.skipped ? QString(QCoreApplication::translate("charts", " (skipped %1 rows with no price)")).arg(r.skipped) : QString()));
    });
    w->setFuture(QtConcurrent::run([name, text]() -> QPair<data::ImportSummary, QString> {
      try {
        return {data::importBars(name, text), {}};
      } catch (const data::ImportError& e) {
        return {{}, e.message};
      } catch (const std::exception& e) {
        return {{}, QString::fromUtf8(e.what())};
      }
    }));
    return;
  }
  // signal: the name is checked first (cleanName throws for an unusable one), parsing runs on a worker thread
  try {
    data::cleanName(name);
  } catch (const data::ImportError& e) {
    return P->app->toast(fmt::esc(fmt::tx(e.message)), "err");
  }
  auto* w = new QFutureWatcher<QPair<data::Signals, QString>>(this);
  connect(w, &QFutureWatcherBase::finished, this, [this, w, name] {
    w->deleteLater();
    const auto [sig, err] = w->result();
    if (!err.isEmpty()) return P->app->toast(fmt::esc(fmt::tx(err)), "err");
    QJsonObject r;
    try {
      r = data::importSignal(name, sig);
    } catch (const data::ImportError& e) {
      return P->app->toast(fmt::esc(fmt::tx(e.message)), "err");
    } catch (const std::exception& e) {
      return P->app->toast(fmt::esc(fmt::tx(QString::fromUtf8(e.what()))), "err");
    }
    // importSignal already loaded and registered the indicator (it throws when the file doesn't load)
    if (!ind::byId(r.value("id").toString())) return;
    strategyToUi(r.value("strategy").toObject());
    tState.name = "Signal " + r.value("name").toString();
    if (P->isOpen("tester")) render();
    P->app->toast(QString(QCoreApplication::translate("charts", "Loaded your signal %1: %2 position changes. Open the bars it was made on, then press Run "
                            "on chart."))
                      .arg(fmt::esc(r.value("name").toString()), localeInt(r.value("changes").toDouble())));
  });
  w->setFuture(QtConcurrent::run([text]() -> QPair<data::Signals, QString> {
    try {
      return {data::parseSignals(text), {}};
    } catch (const data::ImportError& e) {
      return {{}, e.message};
    } catch (const std::exception& e) {
      return {{}, QString::fromUtf8(e.what())};
    }
  }));
}

void TesterTab::liveSpread() {
  const QString sym = P->app->active ? P->app->active->cfg.value("symbol").toString() : QString();
  QPointer<QWidget> input = inputs_.value("spreadPct");
  data::getSpread(sym, this, [this, input](const data::Result<data::SpreadQuote>& r) {
    if (!r.ok()) return P->app->toast(fmt::esc(fmt::tx(r.error->message)), "err");
    const auto& q = r.value;
    const QString pct = jsPrecision(q.spreadPct, 2);
    if (auto* e = qobject_cast<QLineEdit*>(input.data())) e->setText(pct);
    P->app->toast(QString(QCoreApplication::translate("charts", "%1 on %2: bid %3, ask %4. Spread set to %5%."))
                      .arg(fmt::esc(ticker(q.symbol)), fmt::esc(q.source), fmtPrice(q.bid), fmtPrice(q.ask), pct));
  });
}

void TesterTab::saveStrategy() {
  const QString name = prompt(P->window(), QCoreApplication::translate("charts", "Name this strategy:"));
  if (name.isEmpty()) return;
  QJsonObject all = P->app->state.value("strategies").toObject();
  all[name] = readTester();
  P->app->state["strategies"] = all;
  P->app->persist();
  P->app->toast(QString(QCoreApplication::translate("charts", "Saved \"%1\".")).arg(fmt::esc(name)));
  // the "Load a strategy…" list is built by render(): without this the new strategy was missing from it until the
  // next re-render. readTester() above put the form in tState, so nothing typed is lost.
  tState.name = name;
  render();
}

void TesterTab::editJson() {
  auto* box = new QWidget;
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  Col col{v};
  col.add(hint(QCoreApplication::translate("charts", "The same format your AI agent can write. Operands: numbers, price fields, indicator keys, or "
                 "key.output.")),
          0, kPMargin);
  auto* ta = new QPlainTextEdit;
  ta->setProperty("qtc", "in");
  ta->setFont(style::monoFont(12));
  ta->setPlainText(strategyJsonText());
  ta->setMinimumHeight(int(22 * 12 * 1.5 + 16));   // rows="22", line-height 1.5
  ta->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  col.add(ta);
  col.finish(false);
  auto* ok = style::btn(QCoreApplication::translate("charts", "Apply"), "primary");
  P->app->modal(QCoreApplication::translate("charts", "Strategy JSON"), box, {ok}, true);
  QPointer<QPlainTextEdit> pta(ta);
  connect(ok, &QPushButton::clicked, this, [this, pta] {
    if (!pta) return;
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(pta->toPlainText().toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError) {
      P->app->toast(QString(QCoreApplication::translate("charts", "Invalid JSON: %1")).arg(fmt::esc(pe.errorString() + QCoreApplication::translate("charts", " at position %1").arg(pe.offset))),
                    "err");
      return;
    }
    const QJsonObject s = d.object();
    applyLoaded(s);
    P->app->closeModal();
    render();
  });
}

// ---------- bars: Data › Technical data or the chart's ----------
namespace {
constexpr int STORED_RULES_CAP = 100000, STORED_PINE_CAP = 20000;   // = STORED_RULES / STORED_PINE below
}
TesterTab::StoredSource TesterTab::storedSource(ChartPane* p) const {
  StoredSource s;
  if (!p || p->replay) return s;   // bar replay tests up to the replayed bar: the chart's bars
  int sec = 0;
  try { sec = int(series::parseTf(p->cfg.value("tf").toString()).seconds); } catch (const std::exception&) {}
  sec = technicaldata::storedTimeframe(sec);
  const QString mk = sec ? technicaldata::storedMarket(p->cfg.value("symbol").toString()) : QString();
  if (mk.isEmpty()) return s;
  const auto st = technicaldata::Store::instance().state(mk, sec);
  if (st.count <= p->bars.size()) return s;   // the chart already holds as much
  s.market = mk;
  s.tf = sec;
  s.count = st.count;
  s.first = st.first;
  return s;
}
bool TesterTab::useStored() const { return P->app->settings.value("testerStoredBars").toBool(true); }

void TesterTab::refreshBarsSource() {
  if (!barsSel_) return;
  ChartPane* p = P->app->active;
  const StoredSource src = storedSource(p);
  const QSignalBlocker block(barsSel_);
  barsSel_->clear();
  const int cap = pineFile_.isEmpty() ? STORED_RULES_CAP : STORED_PINE_CAP;
  if (!src.market.isEmpty()) {
    barsSel_->addItem(src.count > cap ? QCoreApplication::translate("charts", "Data › Technical data: the newest %1 of %2 stored bars")
                                            .arg(QLocale().toString(cap), QLocale().toString(src.count))
                                      : QCoreApplication::translate("charts", "Data › Technical data: %1 bars since %2")
                                            .arg(QLocale().toString(src.count), localeDate(src.first * 1000)),
                      "stored");
  }
  if (p && !p->bars.isEmpty())
    barsSel_->addItem(p->replay ? QCoreApplication::translate("charts", "The chart's bars, up to the replayed bar")
                                : QCoreApplication::translate("charts", "The chart's bars: %1 since %2")
                                      .arg(QLocale().toString(qlonglong(p->bars.size())), localeDate(p->bars.first().time * 1000)),
                      "chart");
  else barsSel_->addItem(QCoreApplication::translate("charts", "The chart has no bars yet"), "chart");
  barsSel_->setCurrentIndex(std::max(0, barsSel_->findData(!src.market.isEmpty() && useStored() ? "stored" : "chart")));
  barsSel_->setEnabled(barsSel_->count() > 1);
  barsSel_->setToolTip(src.market.isEmpty()
                           ? QCoreApplication::translate("charts", "Data › Technical data stores BTCUSDT (Binance) on 1m, 5m, 15m, 1h, 4h, 1D and 1W; "
                                                                   "other charts test on their own bars.")
                           : QCoreApplication::translate("charts", "The stored history goes back to the exchange's first day; the bar magnifier then "
                                                                   "uses the stored lower-timeframe bars."));
}

// ---------- runTester ----------
void TesterTab::runTester() {
  ChartPane* p = P->app->active;
  if (!p || p->bars.isEmpty()) return P->app->toast(QCoreApplication::translate("charts", "The active chart has no data yet."), "err");
  // Right after a change of symbol or timeframe the cfg names the new market while the bars are still the old one's:
  // a run then would be titled and marked on the wrong market.
  if (barsStale(p)) return P->app->toast(QCoreApplication::translate("charts", "The chart is still loading."), "err");
  const BarsKey key = barsKey(p);
  // Data › Technical data holds more of this market and timeframe than the chart: the test runs on the stored history
  // (not in bar replay, which tests up to the replayed bar)
  if (useStored()) {
    const StoredSource src = storedSource(p);
    if (!src.market.isEmpty()) return runStored(p, src.market, src.tf);
  }
  if (!pineFile_.isEmpty()) return runPine(p, p->replay ? p->bars.mid(0, p->replay->index + 1) : p->bars, key.symbol, key.tf);
  QJsonObject strat;
  QStringList order;
  try {
    strat = readTester(&order);
  } catch (const std::exception& e) {
    return P->app->toast(fmt::esc(fmt::tx(QString::fromUtf8(e.what()))), "err");
  }
  if (strat.value("entryLong").toArray().isEmpty() && strat.value("entryShort").toArray().isEmpty())
    return P->app->toast(QCoreApplication::translate("charts", "Add at least one entry condition."), "err");
  const Bars bars = p->replay ? p->bars.mid(0, p->replay->index + 1) : p->bars;
  if (strat.value("intrabar").toString() == "magnifier") {
    double sec = 0;
    try {
      sec = series::parseTf(key.tf).seconds;
    } catch (const std::exception& e) {
      return P->app->toast(fmt::esc(fmt::tx(QString::fromUtf8(e.what()))), "err");
    }
    const double span = double(bars.last().time - bars.first().time) + sec;
    const auto m = bt::magnifierTf(sec, span);
    if (!m) {
      P->app->toast(QCoreApplication::translate("charts", "This chart is already as fine as the bar magnifier goes, so it uses the OHLC path."));
    } else {
      P->app->toast(QString(QCoreApplication::translate("charts", "Loading %1 bars for the bar magnifier…")).arg(m->tf));
      const qint64 first = bars.first().time, last = bars.last().time;
      const int limit = int(std::ceil(span / m->seconds)) + 10;
      // The run belongs to the pane it started on (JS keeps `p` across the await), not to whichever chart is
      // active when the lower-timeframe bars arrive.
      QPointer<ChartPane> pane(p);
      const int gen = ++runGen_;
      data::getCandles(key.symbol, m->tf, limit, this,
                       [this, pane, bars, strat, order, first, last, sec, key, gen](const data::Result<data::CandlesResult>& r) {
                         if (gen != runGen_) return;   // a newer run started meanwhile
                         if (!r.ok()) return P->app->toast(fmt::esc(fmt::tx(r.error->message)), "err");
                         Bars sub;
                         for (const Bar& c : r.value.candles)
                           if (c.time >= first && c.time < last + sec) sub.append(c);
                         finishRun(pane, bars, strat, order, sub, key.symbol, key.tf);
                       });
      return;
    }
  }
  ++runGen_;
  finishRun(p, bars, strat, order, {}, key.symbol, key.tf);
}

void TesterTab::finishRun(QPointer<ChartPane> pane, const Bars& bars, const QJsonObject& strat,
                          const QStringList& order, const Bars& subBars, const QString& symbol, const QString& tf) {
  ChartPane* p = pane.data();
  if (!p) return;   // the chart was closed while the magnifier bars loaded
  try {
    bt::Result r = bt::backtest(bars, strat, subBars);
    TestResult tr;
    tr.r = r;
    tr.symbol = symbol;
    tr.tf = tf;
    tr.bars = bars;
    tr.strat = strat;
    tr.subBars = subBars;
    tr.indicatorOrder = order;
    showRun(p, tr);
  } catch (const std::exception& e) {
    P->app->toast(fmt::esc(fmt::tx(QString::fromUtf8(e.what()))), "err");
  }
}

namespace {
constexpr int STORED_RULES = STORED_RULES_CAP, STORED_PINE = STORED_PINE_CAP;   // newest stored bars a test takes (the gold box runs 26 more)
struct StoredRun {
  Bars bars, sub;
  QString subTf, error;
  qint64 stored = 0;
  bool ran = false;
  bt::Result r;
};
QString storedNote(const StoredRun& o, int cap) {
  QString n = o.stored > cap ? QCoreApplication::translate("charts", "Data › Technical data: the newest %1 of %2 stored bars")
                                   .arg(QLocale().toString(cap), QLocale().toString(o.stored))
                             : QCoreApplication::translate("charts", "Data › Technical data: the whole stored history");
  if (!o.sub.isEmpty()) n += " · " + QCoreApplication::translate("charts", "magnifier on %1 stored %2 bars").arg(QLocale().toString(qlonglong(o.sub.size())), o.subTf);
  return n;
}
}  // namespace

void TesterTab::runStored(ChartPane* p, const QString& market, int tfSeconds) {
  if (storedRunning_) return P->app->toast(QCoreApplication::translate("charts", "The test is still running."));
  const bool pine = !pineFile_.isEmpty();
  QJsonObject strat;
  QStringList order;
  if (!pine) {
    try {
      strat = readTester(&order);
    } catch (const std::exception& e) {
      return P->app->toast(fmt::esc(fmt::tx(QString::fromUtf8(e.what()))), "err");
    }
    if (strat.value("entryLong").toArray().isEmpty() && strat.value("entryShort").toArray().isEmpty())
      return P->app->toast(QCoreApplication::translate("charts", "Add at least one entry condition."), "err");
  }
  // the Pine file as it is now: an edit saved while the stored bars load must not change the run already started
  const QString pineFile = pineFile_;
  const QString pineSource = pine ? data::readCustom(pineFile) : QString();
  if (pine && pineSource.isEmpty()) return P->app->toast(QCoreApplication::translate("charts", "Couldn't read %1").arg(fmt::esc(pineFile)), "err");
  storedRunning_ = true;
  const int gen = ++runGen_;
  P->app->toast(QCoreApplication::translate("charts", "Testing on Data › Technical data…"));
  const int cap = pine ? STORED_PINE : STORED_RULES;
  // custom JavaScript indicators run on the GUI thread only: then the worker only reads the bars
  const bool runHere = !pine && usesCustom(strat);
  const bool magnify = !pine && strat.value("intrabar").toString() == "magnifier";
  auto* w = new QFutureWatcher<StoredRun>(this);
  QPointer<ChartPane> pane(p);
  const QString symbol = p->cfg.value("symbol").toString(), tf = p->cfg.value("tf").toString();
  connect(w, &QFutureWatcher<StoredRun>::finished, this, [=, this] {
    StoredRun o = w->result();
    w->deleteLater();
    storedRunning_ = false;
    if (!pane || gen != runGen_) return;   // the chart closed, or a newer run started meanwhile
    if (!o.error.isEmpty()) return P->app->toast(fmt::esc(fmt::tx(o.error)), "err");
    if (o.bars.isEmpty()) return P->app->toast(QCoreApplication::translate("charts", "Data › Technical data has no bars for this chart yet."), "err");
    const QString note = storedNote(o, cap);
    if (pine) return runPine(pane, o.bars, symbol, tf, note, pineFile, pineSource);
    try {
      TestResult tr;
      tr.r = o.ran ? o.r : bt::backtest(o.bars, strat, o.sub);
      tr.symbol = symbol;
      tr.tf = tf;
      tr.bars = o.bars;
      tr.strat = strat;
      tr.subBars = o.sub;
      tr.indicatorOrder = order;
      tr.dataNote = note;
      showRun(pane, tr);
    } catch (const std::exception& e) {
      P->app->toast(fmt::esc(fmt::tx(QString::fromUtf8(e.what()))), "err");
    }
  });
  w->setFuture(QtConcurrent::run([market, tfSeconds, cap, strat, runHere, magnify]() {
    StoredRun o;
    o.stored = technicaldata::storedCount(market, tfSeconds);
    o.bars = technicaldata::newestBars(market, tfSeconds, cap);
    if (magnify) o.sub = technicaldata::magnifierBars(market, tfSeconds, o.bars, 1000000, &o.subTf);
    if (!runHere && !o.bars.isEmpty() && !strat.isEmpty()) {
      try {
        o.r = bt::backtest(o.bars, strat, o.sub);
        o.ran = true;
      } catch (const std::exception& e) {
        o.error = QString::fromUtf8(e.what());
      }
    }
    return o;
  }));
}

void TesterTab::runPine(ChartPane* p, const Bars& bars, const QString& symbol, const QString& tf,
                        const QString& dataNote, const QString& fileIn, const QString& sourceIn) {
  if (pineRunning_) return P->app->toast(QCoreApplication::translate("charts", "The Pine strategy is still running."));
  const QString file = fileIn.isEmpty() ? pineFile_ : fileIn;
  const QString source = sourceIn.isEmpty() ? data::readCustom(file) : sourceIn;
  if (source.isEmpty()) return P->app->toast(QCoreApplication::translate("charts", "Couldn't read %1").arg(fmt::esc(file)), "err");
  pine::Options o;
  o.symbol = symbol;
  o.timeframe = pine::timeframeFromCharts(tf);
  pineRunning_ = true;
  const int gen = ++runGen_;
  P->app->toast(QCoreApplication::translate("charts", "Running %1…").arg(fmt::esc(file)));
  QPointer<ChartPane> pane(p);
  pine::runAsync(source, bars, o, this, [this, pane, bars, file, symbol, tf, dataNote, gen](const pine::Result& run) {
    pineRunning_ = false;
    if (gen != runGen_) return;   // a newer run started meanwhile: its result stands
    const pine::StrategyResult sr = pine::fromRun(run, bars);
    if (!sr.ok()) return P->app->toast(QCoreApplication::translate("charts", "%1: %2").arg(fmt::esc(file), fmt::esc(fmt::tx(sr.error))), "err");
    if (!pane) return;
    TestResult tr;
    tr.r = sr.result;
    tr.symbol = symbol;
    tr.tf = tf;
    tr.bars = bars;
    tr.strat = QJsonObject{{"pine", file}, {"initialCapital", sr.config.value("initial_capital").toDouble(1000000)}};
    tr.dataNote = dataNote;
    showRun(pane, tr);
  });
}

void TesterTab::showRun(ChartPane* p, const TestResult& tr) {
  {
    const bt::Result& r = tr.r;
    static int resultSeq = 0;
    lastResult = tr;
    lastResult->runId = ++resultSeq;
    QVector<ind::Marker> marks;
    for (const auto& t : r.trades) {
      const bool lg = t.side == "long";
      ind::Marker a;
      a.time = t.entryTime;
      a.position = lg ? "belowBar" : "aboveBar";
      a.shape = lg ? "arrowUp" : "arrowDown";
      a.color = lg ? "#2962ff" : "#ff6d00";
      a.text = lg ? QCoreApplication::translate("charts", "Buy") : QCoreApplication::translate("charts", "Short");
      marks.append(a);
      ind::Marker b;
      b.time = t.exitTime;
      b.position = lg ? "aboveBar" : "belowBar";
      b.shape = "circle";
      b.color = t.pnl >= 0 ? "#089981" : "#f23645";
      b.text = (t.pnlPct >= 0 ? "+" : "") + jsFixed(t.pnlPct, 1) + "%";
      marks.append(b);
    }
    // the previous result's marks go with it, wherever they were drawn
    if (marksPane_ && marksPane_ != p) {
      marksPane_->extraMarkers.clear();
      marksPane_->render(false);
    }
    marksPane_ = p;
    p->extraMarkers = marks;
    p->render(false);
    showResult();
  }
}

// ---------- showResult ----------
void TesterTab::showResult() {
  if (!out_ || !lastResult || P->body() != body_) return;
  const TestResult& R = *lastResult;
  const bt::Stats& s = R.r.stats;
  // out.innerHTML = …
  while (QLayoutItem* it = outLay_->takeAt(0)) {
    if (it->widget()) it->widget()->deleteLater();
    delete it;
  }
  const auto& V = style::v();
  // header row
  auto* title = new QLabel("<b>" + fmt::esc(ticker(R.symbol) + " " + R.tf) + "</b>");
  title->setFont(style::font(13));
  textColor(title);
  QString info = QCoreApplication::translate("charts", "%n bars · %1 to %2 · %3", nullptr, int(R.bars.size()))
                     .arg(localeDate(R.bars.first().time * 1000), localeDate(R.bars.last().time * 1000),
                          fmt::esc(fmt::tx(s.feesAndSlippage)));
  info += " · " + fmt::esc(fmt::tx(s.fills));   // ${esc(s.fills)}: "undefined" never happens, backtest() sets it
  auto* infoL = new QLabel(info);
  infoL->setTextFormat(Qt::RichText);   // escaped text (a French "d'abord" is &#39;)
  infoL->setProperty("qtc", "hint");
  auto* csv = style::btn(QCoreApplication::translate("charts", "Export trades (CSV)"), "small");
  auto* clear = style::btn(QCoreApplication::translate("charts", "Clear chart marks"), "small");
  outLay_->addWidget(rowFlex({title, infoL, nullptr, csv, clear}));
  if (!R.dataNote.isEmpty()) {   // the bars are Data › Technical data's stored history, not the chart's
    auto* noteL = new QLabel(fmt::esc(R.dataNote));
    noteL->setTextFormat(Qt::RichText);
    noteL->setWordWrap(true);
    noteL->setProperty("qtc", "hint");
    outLay_->addWidget(noteL);
  }
  // The trades shown here, named after the market they were tested on (captured now, not read at click time).
  const QString csvName = ticker(R.symbol) + "-" + R.tf + "-trades.csv";
  const auto trades = std::make_shared<const decltype(R.r.trades)>(R.r.trades);
  connect(csv, &QPushButton::clicked, this, [this, csvName, trades] {
    fmt::download(P->window(), csvName, bt::tradesCsv(*trades).toUtf8());
  });
  // the marks are on the chart the run was shown on, which need not be the active one any more
  connect(clear, &QPushButton::clicked, this, [this] {
    ChartPane* mp = marksPane_ ? marksPane_.data() : P->app->active;
    if (!mp) return;
    mp->extraMarkers.clear();
    mp->render(false);
  });
  // stats
  auto* stats = new StatsGrid(130);
  auto upDown = [](double v) { return std::isnan(v) || v >= 0 ? QString("up") : QString("down"); };   // null >= 0
  stats->addStat(QCoreApplication::translate("charts", "Net profit"), pcText(s.netProfitPct), upDown(s.netProfitPct));
  stats->addStat(QCoreApplication::translate("charts", "Buy and hold"), pcText(s.buyHoldPct), upDown(s.buyHoldPct));
  stats->addStat(QCoreApplication::translate("charts", "Max drawdown"), std::isnan(s.maxDrawdownPct) ? "—" : "-" + numText(s.maxDrawdownPct) + "%", "down");
  stats->addStat(QCoreApplication::translate("charts", "Trades"), QString::number(s.totalTrades));
  stats->addStat(QCoreApplication::translate("charts", "Win rate"), std::isnan(s.winRatePct) ? "—" : numText(s.winRatePct) + "%");
  stats->addStat(QCoreApplication::translate("charts", "Profit factor"), numText(s.profitFactor));
  stats->addStat(QCoreApplication::translate("charts", "Sharpe"), numText(s.sharpe));
  stats->addStat(QCoreApplication::translate("charts", "Sortino"), numText(s.sortino));
  stats->addStat(QCoreApplication::translate("charts", "Avg trade"), pcText(s.avgTradePct));
  stats->addStat(QCoreApplication::translate("charts", "Time in market"), std::isnan(s.exposurePct) ? "—" : numText(s.exposurePct) + "%");
  outLay_->addWidget(stats);

  // equity curve (.equity: 150 px, 1 px border, 6 px radius)
  auto* eqBox = new QFrame;
  eqBox->setFixedHeight(150);
  eqBox->setStyleSheet(QString("QFrame{border:1px solid %1;border-radius:6px;background:transparent;}")
                           .arg(V.border.name(QColor::HexArgb)));
  auto* eqLay = new QVBoxLayout(eqBox);
  eqLay->setContentsMargins(1, 1, 1, 1);
  auto* chart = new plot::Chart(eqBox);
  chart->setStyleSheet({});
  chart->applyOptions([&](plot::ChartOptions& o) {
    o.layout.background = V.bg;
    o.layout.textColor = V.text;
    o.layout.fontSize = 10;
    o.grid.vertLines.visible = false;
    o.grid.horzLines.color = V.grid;
    o.rightPriceScale.borderVisible = false;
    o.timeScale.borderVisible = false;
  });
  auto* eq = chart->addSeries(plot::AreaSeries, [](plot::SeriesOptions& o) {
    o.lineColor = QColor("#2962ff");
    o.topColor = QColor(41, 98, 255, 77);
    o.bottomColor = QColor(41, 98, 255, 0);
    o.lineWidth = 2;
    o.title = QCoreApplication::translate("charts", "Strategy");
  });
  QVector<plot::DataPoint> eqPts;
  for (const auto& e : R.r.equity) eqPts.append(plot::DataPoint::val(e.time, e.value));
  eq->setData(eqPts);
  auto* bh = chart->addSeries(plot::LineSeries, [](plot::SeriesOptions& o) {
    o.color = QColor("#787b86");
    o.lineWidth = 1;
    o.title = QCoreApplication::translate("charts", "Buy & hold");
    o.priceLineVisible = false;
  });
  QVector<plot::DataPoint> bhPts;
  const double c0 = R.bars.first().close;
  const double cap = bt::jsNum(R.strat.value("initialCapital"));
  for (const Bar& b : R.bars) bhPts.append(plot::DataPoint::val(b.time, cap * b.close / c0));
  bh->setData(bhPts);
  chart->timeScale()->fitContent();
  eqLay->addWidget(chart);
  outLay_->addWidget(eqBox);

  // quant checks (the gold box)
  outLay_->addWidget(mountQuantChecks(R, P->app));   // a Pine strategy too: its versions run by PineTS

  // trades
  auto* table = new Table;
  TableStyle ts;
  ts.wrap = true;
  table->setStyle(ts);
  table->setHeader({{QCoreApplication::translate("charts", "Side")}, {QCoreApplication::translate("charts", "Entry")}, {QCoreApplication::translate("charts", "Exit", "trade exit time")}, {QCoreApplication::translate("charts", "Entry price")}, {QCoreApplication::translate("charts", "Exit price")}, {QCoreApplication::translate("charts", "P&L %")},
                    {QCoreApplication::translate("charts", "Bars")}, Cell{QCoreApplication::translate("charts", "Exit reason"), {}, true}});
  QList<Row> rows;
  for (int i = int(R.r.trades.size()) - 1, n = 0; i >= 0 && n < 300; --i, ++n) {
    const auto& t = R.r.trades[i];
    Row row;
    row.data = QString::number(t.entryTime);
    row.cells = {{t.side},
                 {localeDateTime(t.entryTime * 1000)},
                 {localeDateTime(t.exitTime * 1000)},
                 {fmtPrice(t.entryPrice)},
                 {fmtPrice(t.exitPrice)},
                 Cell{jsFixed(t.pnlPct, 2) + "%", t.pnl >= 0 ? "up" : "down"},
                 {QString::number(t.bars)},
                 Cell{fmt::tx(t.exitReason), {}, true}};
    rows.append(row);
  }
  table->setRows(rows);
  table->setStickyIn(outArea_);
  outLay_->addWidget(table);
  outLay_->addStretch(1);
}

}  // namespace charts::ui::bottom
