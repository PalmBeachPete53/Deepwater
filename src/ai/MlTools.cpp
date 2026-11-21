// Skill "ml" (user's request, 2026-11-16): what AI › ML offers, for the models of the cubes and Claude Code.
//
// Reading: ml_status, ml_methods, ml_datasets, ml_read_dataset, ml_runs, ml_run, ml_run_rows, ml_compare (MlCompare.cpp).
// Acting:  ml_load_dataset (prices / macro / COT, as the page's DATA panel), ml_train (one training at a time, in the
//          trainer's sandbox, named after the environment that asked).
// Never: deleting or stopping a run, installing anything, reading a file that is not one of the app's data sets.
//
// Models invent (memory: a real model made up a trade's figures): every argument of ml_train is checked against the
// catalogue (ml/Catalog) and the data set itself — family, algorithm, target and feature columns, parameter names and
// ranges — and refused with the valid choices listed, never guessed.
#include "ai/Tools.h"
#include "macro/Registry.h"
#include "market/Instruments.h"
#include "ml/Catalog.h"
#include "ml/Datasets.h"
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataIndicators.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "technicaldata/TechnicalDataBars.h"
#include "technicaldata/TechnicalDataStore.h"
#include "ml/Environment.h"
#include "ml/Runs.h"
#include "positioning/Cot.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
#include <cmath>
#include <memory>

namespace ai {
namespace {
constexpr int kMaxChars = 6000;
ToolResult okText(QString s) {
  if (s.size() > kMaxChars) s = s.left(kMaxChars) + "\n[cut: longer than " + QString::number(kMaxChars) + " characters; ask for less]";
  return {true, s};
}
ToolResult fail(const QString& s) { return {false, s}; }
QString strArg(const QJsonObject& a, const char* k) { return a.value(k).isString() ? a.value(k).toString().trimmed() : QString(); }
int intArg(const QJsonObject& a, const char* k, int def, int lo, int hi) {
  const QJsonValue v = a.value(k);
  bool ok = v.isDouble();
  int n = ok ? v.toInt() : 0;
  if (v.isString()) n = v.toString().toInt(&ok);
  return qBound(lo, ok ? n : def, hi);
}
double numArg(const QJsonObject& a, const char* k, double def) {
  const QJsonValue v = a.value(k);
  if (v.isDouble()) return v.toDouble();
  bool ok = false; const double d = v.toString().toDouble(&ok);
  return ok ? d : def;
}
QStringList listArg(const QJsonObject& a, const char* k) {
  QStringList out;
  const QJsonValue v = a.value(k);
  if (v.isArray()) for (const auto& x : v.toArray()) out << x.toString().trimmed();
  else if (v.isString()) out = v.toString().split(QRegularExpression("[,;]"), Qt::SkipEmptyParts);
  for (auto& s : out) s = s.trimmed();
  out.removeAll(QString());
  return out;
}
QString num(double v, int dec = 4) { return std::isfinite(v) ? QString::number(v, 'f', dec) : QStringLiteral("n/a"); }
QString pct(double v, int dec = 1) { return std::isfinite(v) ? QString::number(v * 100, 'f', dec) + "%" : QStringLiteral("n/a"); }
double jn(const QJsonValue& v) { return v.isDouble() ? v.toDouble() : NAN; }
// The supervised run's test predictions traded after costs (train.py trading_eval; step 4 of the roadmap, 2026-11-20)
QString tradingText(const QJsonObject& t) {
  if (t.isEmpty()) return {};
  auto signedPct = [](double v) { return std::isfinite(v) ? (v >= 0 ? "+" : "") + QString::number(v * 100, 'f', 2) + "%" : QStringLiteral("n/a"); };
  auto line = [&](const QJsonObject& o) {
    return "net " + signedPct(jn(o.value("total_return"))) + " · Sharpe " + num(jn(o.value("sharpe")), 2) + " · max drawdown " +
           pct(-jn(o.value("max_drawdown"))) + " · " + QString::number(o.value("trades").toInt()) + " trades · in the market " +
           pct(jn(o.value("exposure")), 0);
  };
  const QJsonObject m = t.value("model").toObject(), vc = t.value("vs_constant").toObject(), c = t.value("constant").toObject();
  QString out = QString("\nTRADED on the %1 test rows after costs (%2 bp a side; %3):").arg(t.value("rows").toInt())
                    .arg(num(jn(t.value("cost_bp")), 1), t.value("rule").toString());
  out += "\n  model: " + line(m.value("x1").toObject()) + " | before costs net " + signedPct(jn(m.value("x0").toObject().value("total_return"))) +
         " | costs ×2 net " + signedPct(jn(m.value("x2").toObject().value("total_return"))) + " | ×3 net " +
         signedPct(jn(m.value("x3").toObject().value("total_return")));
  out += "\n  buy & hold: " + line(t.value("buy_hold").toObject());
  out += "\n  trend (" + t.value("trend").toObject().value("rule").toString() + "): " + line(t.value("trend").toObject());
  out += "\n  constant exposure (" + pct(jn(c.value("level")), 0) + " long all along, the model's average): " + line(c);
  const double p = jn(vc.value("p")), pw = jn(vc.value("p_worse"));
  out += QString("\n  timing against that constant exposure: %1 bp a row after costs, p %2 (%3)")
             .arg(num(jn(vc.value("mean_diff_bp")), 2), num(p, 3),
                  p < 0.05 ? QStringLiteral("the timing earns more than being in the market that much")
                  : pw < 0.05 ? QStringLiteral("the timing LOSES against being in the market that much, not by chance")
                              : QStringLiteral("not distinguishable from being in the market that much"));
  return out;
}

QJsonObject schema(std::initializer_list<std::pair<const char*, QJsonObject>> props, QStringList required = {}) {
  QJsonObject p;
  for (const auto& [k, v] : props) p[k] = v;
  QJsonObject o{{"type", "object"}, {"properties", p}};
  if (!required.isEmpty()) o["required"] = QJsonArray::fromStringList(required);
  return o;
}
QJsonObject str(const char* d) { return {{"type", "string"}, {"description", d}}; }
QJsonObject integer(const char* d) { return {{"type", "integer"}, {"description", d}}; }
QJsonObject number(const char* d) { return {{"type", "number"}, {"description", d}}; }
QJsonObject strings(const char* d) { return {{"type", "array"}, {"items", QJsonObject{{"type", "string"}}}, {"description", d}}; }

const char* kFamilyWhat[] = {
    "learn to predict a target column from the features (a class: direction up/down, or a value)",
    "find structure without a target: groups / regimes, unusual rows, fewer dimensions; an optional outcome column shows what followed each group",
    "classes from few labels: keeps labelled_share of the training labels and learns from the unlabelled rows too",
    "learns a representation of windows of rows from the data alone (PyTorch); an optional probe target tests it",
    "an agent trades the target price column on a simulation (no order anywhere), judged on later rows vs buy & hold and random agents"};

// The Python environment is checked when the ML page opens; a tool may come first (2026-11-16: a real model's first
// ml_train was refused as "not installed" on a ready machine). Runs then() once the check has answered (15 s at most).
void withEnvironment(std::function<void()> then) {
  auto& env = ml::Environment::instance();
  if (env.status().probed) { then(); return; }
  env.refresh();
  auto* ctx = new QObject(qApp);
  auto once = std::make_shared<bool>(false);
  auto fire = [ctx, once, then] { if (*once) return; *once = true; ctx->deleteLater(); then(); };
  QObject::connect(&env, &ml::Environment::changed, ctx, [fire] { if (ml::Environment::instance().status().probed) fire(); });
  QTimer::singleShot(15000, ctx, fire);
}

// ------------------------------------------------------------------------------------------------ status
void status(const QJsonObject&, ToolDone done) {
  auto answer = [done] {
    const auto& s = ml::Environment::instance().status();
    QString out;
    if (s.ready()) {
      out = "ML environment: ready · Python " + s.baseVersion + " · scikit-learn " + s.packages.value("sklearn");
      for (const char* k : {"lightgbm", "xgboost", "torch"})
        out += QString(" · %1 %2").arg(k, s.packages.contains(k) ? s.packages.value(k) : QStringLiteral("NOT AVAILABLE"));
      if (!s.packages.contains("torch")) out += "\n(self-supervised methods, DQN and A2C need PyTorch: only the user can install it, in AI › ML)";
    } else {
      out = "ML environment: NOT INSTALLED. Only the user can install it (AI › ML, INSTALL button); no training is possible until then.";
    }
    const auto& runs = ml::Runs::instance();
    QString cur = "none";
    for (const auto& r : runs.list()) if (r.state == ml::RunState::Running) cur = r.id + " \"" + r.name + "\" (" + r.stage + ", " + QString::number(qRound(r.pct)) + "%)";
    out += "\nTraining running now: " + cur + " (one at a time)";
    // counted, not described: describing parsed every CSV (up to a million rows each) on the window's thread
    out += QString("\nData sets on disk: %1 (ml_datasets) · runs: %2 (ml_runs)").arg(ml::datasetCount()).arg(runs.list().size());
    done(okText(out));
  };
  withEnvironment(answer);
}

// ------------------------------------------------------------------------------------------------ methods
QString paramText(const ml::Param& p) {
  if (p.type == 'c') return p.key + " (one of " + p.options.join("/") + ", default " + p.text + ")";
  if (p.type == 's')
    return p.key + " (text, default \"" + p.text + "\"" + (p.max > 0 ? QString(", each number %1..%2").arg(p.min).arg(p.max) : QString()) + ")";
  return p.key + QString(" (%1 %2..%3, default %4)").arg(p.type == 'i' ? "integer" : "number").arg(p.min).arg(p.max).arg(p.def);
}
void methodsNow(const QJsonObject& a, ToolDone done);
void methods(const QJsonObject& a, ToolDone done) { withEnvironment([a, done] { methodsNow(a, done); }); }
void methodsNow(const QJsonObject& a, ToolDone done) {
  const QString fam = strArg(a, "family");
  const int f = fam.isEmpty() ? -1 : ml::familyOf(fam);
  if (!fam.isEmpty() && f < 0) { done(fail("error: unknown family \"" + fam + "\"; families: supervised, unsupervised, semi_supervised, self_supervised, reinforcement")); return; }
  const auto& s = ml::Environment::instance().status();
  QString out;
  for (int k = 0; k < 5; ++k) {
    if (f >= 0 && k != f) continue;
    out += "\n## " + ml::familyId(k) + ": " + kFamilyWhat[k] + "\n";
    for (const auto& al : ml::algosFor(k)) {
      const bool off = !al.needs.isEmpty() && !s.packages.contains(al.needs);
      out += "- " + al.id + (off ? " [UNAVAILABLE: " + al.needs + " not installed]" : QString()) + ": " + al.desc;
      if (f >= 0) {
        QStringList ps;
        for (const auto& p : al.params) ps << paramText(p);
        out += (ps.isEmpty() ? QString() : "\n  params: " + ps.join("; "));
      }
      out += "\n";
    }
  }
  if (f < 0) out += "\nCall ml_methods with a family for the parameters of its algorithms.";
  out += "\nTarget: a numeric column of the data set; target_transform: raw (its value), change, pct_change or direction "
         "(1 if higher, 0 if not), looking horizon rows ahead (0 = same row, raw only); for a price, how much it moves rather "
         "than where: realized_vol (% over the next horizon rows), range (high − low, %), abs_return (|move|, %) or big_move "
         "(1 when the furthest move reaches move_pct, default the training rows' 80th percentile) — those are judged against "
         "HAR and EWMA, the volatility references a forecast must beat (the result's AGAINST HAR / EWMA lines). Supervised and semi_supervised need a "
         "target (semi_supervised: classes, i.e. direction); unsupervised (outcome) and self_supervised (probe) may have one; "
         "reinforcement: target = the price column traded, transform and horizon unused.";
  done(okText(out.trimmed()));
}

// ------------------------------------------------------------------------------------------------ data sets
QString describeSet(const ml::Dataset& d, bool columns = true) {
  QString s = ml::idOf(d) + " · " + d.title + " · " + QString::number(d.rows) + " rows";
  if (!d.first.isEmpty()) s += " · " + d.first.left(10) + " → " + d.last.left(10);
  if (!d.source.isEmpty()) s += " · " + d.source;
  if (columns) s += "\n  columns: " + d.numeric.join(", ");
  return s;
}

// ------------------------------------------------------------------------------------------------ evaluation clock
// An environment in evaluation mode (ai::Slots::asOf) sees only the data sets cut at or before its moment (built by
// ml_load_dataset in that mode: "-asof-…" in their id) and the trainings made on them: a data set of the present holds
// the rows after the moment, and a training on it learned from them (2026-11-20).
bool visibleSet(const QString& pathOrId, qint64 asOf) {
  if (asOf <= 0) return true;
  const qint64 at = ml::datasetAsOf(pathOrId);
  return at > 0 && at <= asOf;
}
bool visibleRun(const ml::Run& r, qint64 asOf) { return asOf <= 0 || (!r.dataPath.isEmpty() && visibleSet(r.dataPath, asOf)); }
QString setRefused(qint64 asOf) {
  return "refused: this environment is in evaluation mode at " + asOfStamp(asOf) + ": this data set holds data after that moment. "
         "Build one with ml_load_dataset (source technical-data): in evaluation mode it is cut at the moment.";
}

void datasets(const QJsonObject&, ToolDone done) {
  const qint64 asOf = asOfFor("ml_datasets");
  QList<ml::Dataset> list;
  for (const auto& d : ml::listDatasets()) if (visibleSet(d.path, asOf)) list << d;
  QString out = list.isEmpty() ? QStringLiteral("No data set on disk yet.") : QString("%1 data set(s), newest first:").arg(list.size());
  for (const auto& d : list) if (d.ok()) out += "\n- " + describeSet(d);
  out += "\nMore with ml_load_dataset(source: prices | technical-data | macro | cot, key, interval). The user's own CSV files are reachable only once "
         "the user has loaded them on the page.";
  if (asOf > 0) out = "EVALUATION MODE: as of " + asOfStamp(asOf) + " (only the data sets cut at or before that moment).\n" + out;
  done(okText(out));
}

void loadDataset(const QJsonObject& a, ToolDone done) {
  const QString src = strArg(a, "source").toLower(), key = strArg(a, "key");
  ml::DatasetSpec spec;
  spec.asOf = asOfFor("ml_load_dataset");   // evaluation mode: technical-data only, cut at the moment
  if (spec.asOf > 0 && src != "technical-data" && src != "technical_data")
    { done(fail("refused: this environment is in evaluation mode at " + asOfStamp(spec.asOf) + ": only source technical-data can be cut at that moment")); return; }
  if (src == "prices") {
    const market::Instrument* ins = market::find(key);
    if (!ins) for (const auto& i : market::instruments()) if (i.name.compare(key, Qt::CaseInsensitive) == 0) { ins = &i; break; }
    if (!ins) {
      QStringList some; for (const auto& i : market::instruments()) { if (some.size() >= 40) break; some << i.symbol; }
      done(fail("error: no market \"" + key + "\". Symbols include: " + some.join(", ") + " …")); return;
    }
    spec.kind = ml::SourceKind::Prices; spec.key = ins->symbol;
    spec.interval = strArg(a, "interval").isEmpty() ? QStringLiteral("1d") : strArg(a, "interval");
    if (!QStringList{"1d", "1wk", "1h"}.contains(spec.interval)) { done(fail("error: interval must be 1d, 1wk or 1h")); return; }
  } else if (src == "macro") {
    const macro::Economy* e = macro::economy(key);
    if (!e) for (const auto& x : macro::economies()) if (x.name.compare(key, Qt::CaseInsensitive) == 0) { e = &x; break; }
    if (!e) { done(fail("error: no economy \"" + key + "\" (use an ISO code such as us, fr, de, jp, gb, ez, or the English name)")); return; }
    spec.kind = ml::SourceKind::Macro; spec.key = e->iso2;
  } else if (src == "cot") {
    const positioning::CotMarket* m = positioning::Cot::market(key.toUpper());
    if (!m) for (const auto& x : positioning::Cot::markets())
      if (x.shortName.compare(key, Qt::CaseInsensitive) == 0 || x.label.compare(key, Qt::CaseInsensitive) == 0) { m = &x; break; }
    if (!m) {
      QStringList ids; for (const auto& x : positioning::Cot::markets()) ids << x.id;
      done(fail("error: no COT market \"" + key + "\". Ids: " + ids.join(", "))); return;
    }
    spec.kind = ml::SourceKind::Cot; spec.key = m->id;
  } else if (src == "technical-data" || src == "technical_data" || src == "charts-data" || src == "markets") {   // its earlier names (2026-11-18)
    // Data › Technical data: what technical-data.sqlite holds (Binance bars and the indicators of Charts' engine)
    const QString mk = technicaldata::storedMarket(key.isEmpty() ? QStringLiteral("BTCUSDT") : key);   // crypto or forex
    const QStringList have = technicaldata::Store::instance().pairs();
    if (mk.isEmpty() || !have.contains(mk)) { done(fail("error: no stored market \"" + key + "\". Stored: " + (have.isEmpty() ? QStringLiteral("none yet") : have.join(", ")))); return; }
    spec.kind = ml::SourceKind::TechnicalData; spec.key = mk;
    spec.interval = strArg(a, "interval").isEmpty() ? QStringLiteral("1h") : strArg(a, "interval");
    QStringList tfs;
    for (const auto& t : technicaldata::timeframes()) tfs << t.name;
    if (!tfs.contains(spec.interval)) { done(fail("error: for technical-data, interval must be one of " + tfs.join(", "))); return; }
    // the page's choices only: one large file per distinct number asked would fill the disk
    spec.rows = ml::snapTechnicalDataRows(intArg(a, "rows", ml::TECHNICAL_DATA_ROWS_DEFAULT, 1000, ml::TECHNICAL_DATA_ROWS_MAX));
    spec.keepWarm = a.value("keep_warm").toBool(false);
    spec.orderFlow = a.value("order_flow").toBool(false);
    // the whole series inside the warm-up (BTCUSDT 1W: 483 bars < 600): the table would be empty
    int tfSec = 0;
    for (const auto& t : technicaldata::timeframes()) if (t.name == spec.interval) tfSec = t.seconds;
    const auto st = technicaldata::Store::instance().state(mk, tfSec);
    const int warm = technicaldata::warmupOf(technicaldata::specFromJson(technicaldata::Store::instance().pairSpec(mk)));
    if (!spec.keepWarm && st.count > 0 && st.last < st.first + qint64(warm) * tfSec) {
      done(fail(QString("error: %1 %2 has only %3 stored bars, all inside the indicators' warm-up (the first %4 bars of a series, "
                        "where long averages such as sma200 have not settled), so nothing is left once it is dropped. Use "
                        "keep_warm: true to keep them (their long indicators are unsettled or empty), or a shorter interval.")
                    .arg(mk, spec.interval).arg(st.count).arg(warm)));
      return;
    }
  } else {
    done(fail("error: source must be prices, technical-data, macro or cot (the user's CSV files are loaded by the user on the page)")); return;
  }
  auto once = std::make_shared<bool>(false);
  auto answer = [done, once](const ToolResult& r) { if (*once) return; *once = true; done(r); };
  QTimer::singleShot(25000, qApp, [answer] {
    answer(okText("still loading (a macro data set can take a minute the first time); call ml_datasets again in a moment"));
  });
  ml::build(spec, qApp, [answer](const ml::Dataset& d) {
    if (!d.ok()) { answer(fail("error: " + d.error)); return; }
    QString text = "Loaded: " + describeSet(d);
    if (!d.pruned.isEmpty())   // the model should not look for them again
      text += QString("\nDeleted to keep the data sets under %1 GB: %2").arg(ml::DATASETS_MAX_BYTES >> 30).arg(d.pruned.join(", "));
    answer(okText(text));
  });
}

void readDataset(const QJsonObject& a, ToolDone done) {
  const ml::Dataset d = ml::datasetById(strArg(a, "dataset_id"));
  if (!d.ok()) { done(fail("error: unknown dataset_id; see ml_datasets")); return; }
  if (const qint64 asOf = asOfFor("ml_read_dataset"); !visibleSet(d.path, asOf)) { done(fail(setRefused(asOf))); return; }
  QStringList want = listArg(a, "columns");
  for (const auto& c : want) if (!d.columns.contains(c)) { done(fail("error: no column \"" + c + "\" in " + ml::idOf(d) + ". Columns: " + d.columns.join(", "))); return; }
  const int last = intArg(a, "last_rows", 20, 1, 200);
  QFile f(d.path);
  if (!f.open(QIODevice::ReadOnly)) { done(fail("error: the data set cannot be read")); return; }
  QTextStream in(&f);
  const QStringList header = ml::csvCells(in.readLine());   // the same reading as describe(): quotes, spaces
  QList<int> idx;
  if (!d.dateColumn.isEmpty()) idx << int(header.indexOf(d.dateColumn));
  for (const auto& c : want.isEmpty() ? d.numeric : want) { const int k = int(header.indexOf(c)); if (k >= 0 && !idx.contains(k)) idx << k; }
  const QStringList rows = ml::lastLines(d.path, last);   // from the end: not the whole file for its last rows
  QString out = ml::idOf(d) + ": last " + QString::number(rows.size()) + " of " + QString::number(d.rows) + " rows\n";
  QStringList h; for (int k : idx) h << header.value(k);
  out += h.join(",") + "\n";
  for (const auto& r : rows) {
    const QStringList cells = ml::csvCells(r);
    QStringList o;
    for (int k : idx) {
      QString v = cells.value(k);
      if (v.contains(',') || v.contains('"')) v = '"' + v.replace('"', "\"\"") + '"';   // stays one cell when written back
      o << v;
    }
    out += o.join(",") + "\n";
  }
  done(okText(out));
}

// ------------------------------------------------------------------------------------------------ train
void trainNow(const QJsonObject& a, ToolDone done, const ToolCaller& who);
// the caller is read now: by the time the environment check answers, another environment may have called a tool
void train(const QJsonObject& a, ToolDone done) {
  ToolCaller who = currentCaller();
  who.asOf = asOfFor("ml_train");
  withEnvironment([a, done, who] { trainNow(a, done, who); });
}
void trainNow(const QJsonObject& a, ToolDone done, const ToolCaller& who) {
  const auto& envs = ml::Environment::instance().status();
  if (!envs.ready()) { done(fail("refused: the ML environment is not installed; only the user can install it (AI › ML, INSTALL)")); return; }
  auto& runs = ml::Runs::instance();
  for (const auto& r : runs.list())
    if (r.state == ml::RunState::Running) { done(fail("refused: a training is already running (" + r.id + "); wait for it with ml_run(run_id, wait_seconds)")); return; }
  const ml::Dataset d = ml::datasetById(strArg(a, "dataset_id"));
  if (!d.ok()) { done(fail("error: unknown dataset_id; see ml_datasets, or ml_load_dataset first")); return; }
  if (!visibleSet(d.path, who.asOf)) { done(fail(setRefused(who.asOf))); return; }
  const int fam = ml::familyOf(strArg(a, "family"));
  if (fam < 0) { done(fail("error: family must be one of supervised, unsupervised, semi_supervised, self_supervised, reinforcement")); return; }
  const ml::Algo al = ml::algoById(strArg(a, "algorithm"));
  if (al.id.isEmpty() || al.family != fam) {
    QStringList ids; for (const auto& x : ml::algosFor(fam)) ids << x.id;
    done(fail("error: algorithm must be one of " + ids.join(", ") + " for " + ml::familyId(fam))); return;
  }
  if (!al.needs.isEmpty() && !envs.packages.contains(al.needs)) {
    done(fail("refused: " + al.id + " needs " + al.needs + ", which is not installed (only the user can install it, in AI › ML)")); return;
  }
  // target
  const QString target = strArg(a, "target");
  const bool needsTarget = fam == 0 || fam == 2 || fam == 4;
  if (needsTarget && target.isEmpty()) { done(fail("error: " + ml::familyId(fam) + " needs a target column. Numeric columns: " + d.numeric.join(", "))); return; }
  if (!target.isEmpty() && !d.numeric.contains(target)) { done(fail("error: no numeric column \"" + target + "\". Numeric columns: " + d.numeric.join(", "))); return; }
  QString transform = strArg(a, "target_transform");
  if (transform.isEmpty()) transform = fam == 4 ? "raw" : d.dateColumn.isEmpty() ? "raw" : fam == 1 || fam == 3 ? "pct_change" : "direction";
  static const QStringList kTransforms{"raw", "change", "pct_change", "direction", "realized_vol", "range", "abs_return", "big_move"};
  if (!kTransforms.contains(transform)) { done(fail("error: target_transform must be one of " + kTransforms.join(", "))); return; }
  int horizon = intArg(a, "horizon", transform == "raw" && d.dateColumn.isEmpty() ? 0 : 1, 0, 250);
  if (fam == 4) { transform = "raw"; horizon = 0; }
  if (!target.isEmpty() && transform != "raw" && horizon < 1) { done(fail("error: a change, a direction or a volatility target needs horizon ≥ 1")); return; }
  // features: given ones must exist; by default every numeric column but raw price levels (and the target at its own row)
  QStringList feats = listArg(a, "features");
  for (const auto& c : feats)
    if (!d.numeric.contains(c)) { done(fail("error: no numeric column \"" + c + "\" for a feature. Numeric columns: " + d.numeric.join(", "))); return; }
  if (feats.isEmpty()) for (const auto& c : d.numeric) if (!ml::isLevelColumn(c)) feats << c;
  if ((fam == 0 || fam == 2) && transform == "raw" && horizon == 0) feats.removeAll(target);   // it would read the answer
  if (feats.isEmpty()) { done(fail("error: no feature left; give features explicitly. Numeric columns: " + d.numeric.join(", "))); return; }
  // parameters: only the algorithm's, kept inside the page's ranges
  QJsonObject params, given = a.value("params").toObject();
  QStringList notes;
  for (auto it = given.begin(); it != given.end(); ++it) {
    bool known = false; for (const auto& p : al.params) if (p.key == it.key()) known = true;
    if (!known) {
      QStringList keys; for (const auto& p : al.params) keys << p.key;
      done(fail("error: " + al.id + " has no parameter \"" + it.key() + "\"; its parameters: " + (keys.isEmpty() ? QStringLiteral("none") : keys.join(", "))));
      return;
    }
  }
  for (const auto& p : al.params) {
    if (p.type == 'c') {
      const QString v = given.contains(p.key) ? given.value(p.key).toString() : p.text;
      if (!p.options.contains(v)) { done(fail("error: " + p.key + " must be one of " + p.options.join(", "))); return; }
      params[p.key] = v;
    } else if (p.type == 's') {
      const QString v = given.contains(p.key) ? given.value(p.key).toString().remove(' ') : p.text;
      if (!QRegularExpression("^\\d{1,4}(,\\d{1,4}){0,5}$").match(v).hasMatch()) { done(fail("error: " + p.key + " must look like \"64,32\"")); return; }
      if (p.max > 0)   // each number in range (hidden_layers: 9999 units a layer is gigabytes of weights)
        for (const QString& x : v.split(','))
          if (x.toInt() < p.min || x.toInt() > p.max) {
            done(fail(QString("error: each number of %1 must be %2..%3").arg(p.key).arg(p.min).arg(p.max)));
            return;
          }
      params[p.key] = v;
    } else {
      double v = given.contains(p.key) ? numArg(given, p.key.toUtf8().constData(), p.def) : p.def;
      const double c = qBound(p.min, v, p.max);
      if (c != v) notes << QString("%1 brought to %2 (allowed %3..%4)").arg(p.key).arg(c).arg(p.min).arg(p.max);
      params[p.key] = p.type == 'i' ? QJsonValue(qRound(c)) : QJsonValue(c);
    }
  }
  const double testSize = qBound(0.05, numArg(a, "test_size", 0.2), 0.5);
  const int folds = intArg(a, "cv_folds", fam == 0 ? 3 : 0, 0, 10);
  const QString task = strArg(a, "task").isEmpty() ? QStringLiteral("auto") : strArg(a, "task");
  if (!QStringList{"auto", "classification", "regression"}.contains(task)) { done(fail("error: task must be auto, classification or regression")); return; }
  QJsonObject job{{"family", ml::familyId(fam)}, {"target", target}, {"target_transform", transform}, {"horizon", horizon},
                  {"features", QJsonArray::fromStringList(feats)}, {"lags", intArg(a, "lags", 0, 0, 10)}, {"algorithm", al.id},
                  {"params", params}, {"test_size", testSize}, {"cv_folds", folds}, {"task", task}, {"seed", 42}};
  if (!d.dateColumn.isEmpty()) job["date_column"] = d.dateColumn;
  if (transform == "big_move" && a.contains("move_pct")) job["move_pct"] = qBound(0.0, numArg(a, "move_pct", 0), 100.0);
  if (fam == 0 || fam == 2 || fam == 3) {   // how the test predictions are traded (train.py trading_eval); the cost defaults by market (ml::Runs)
    const QString side = strArg(a, "trade_side");
    if (!side.isEmpty() && side != "long" && side != "long_short") { done(fail("error: trade_side must be long or long_short")); return; }
    if (!side.isEmpty()) job["trade_side"] = side;
    if (a.contains("cost_bp")) job["cost_bp"] = qBound(0.0, numArg(a, "cost_bp", 15), 200.0);
  }
  const QString validation = strArg(a, "validation");
  if (!validation.isEmpty() && validation != "split" && validation != "walk_forward") { done(fail("error: validation must be split or walk_forward")); return; }
  if (validation == "walk_forward") {
    if (fam != 0) { done(fail("error: walk_forward is for the supervised family")); return; }
    job["validation"] = "walk_forward";
    job["wf_folds"] = intArg(a, "wf_folds", 5, 2, 20);
    job["cv_folds"] = 0;
  }
  // named after the environment that asked: the user sees on the page who trained what
  const QString envTag = who.env >= 0 ? QString("Env %1").arg(who.env + 1) : QStringLiteral("AI");
  QString name = strArg(a, "name").left(80);
  name.remove(QRegularExpression("[\\r\\n\\t]"));
  if (name.isEmpty())
    name = al.name + " · " + d.title + (target.isEmpty() ? QString() : fam == 4 ? " · trades " + target : " · " + transform + " " + target);
  name = envTag + " · " + name;
  const QString by = who.env >= 0 ? QString("environment %1 · %2").arg(who.env + 1).arg(who.model) : QStringLiteral("AI");
  QString err;
  const QString id = runs.start(job, d.path, d.title, name, &err, by);
  if (id.isEmpty()) { done(fail("error: " + err)); return; }
  QString out = "Training started: run " + id + " \"" + name + "\"\n" + ml::familyId(fam) + " · " + al.id + " · data " + ml::idOf(d);
  if (!target.isEmpty()) out += fam == 4 ? " · trades " + target : " · target " + transform + " of " + target + QString(", %1 row(s) ahead").arg(horizon);
  out += " · " + QString::number(feats.size()) + " features: " + feats.join(", ");
  out += "\nparams: " + QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
  if (!notes.isEmpty()) out += "\nnote: " + notes.join("; ");
  out += "\nIt runs in the sandbox (seconds to a minute). Read it with ml_run(run_id=\"" + id + "\", wait_seconds=25).";
  done(okText(out));
}

// ------------------------------------------------------------------------------------------------ runs
QString familyOfRun(const ml::Run& r) { return r.job.value("family").toString("supervised"); }
QString headline(const ml::Run& r) {
  const QJsonObject res = r.result;
  if (r.state == ml::RunState::Running) return "running: " + r.stage + " " + QString::number(qRound(r.pct)) + "%";
  if (r.state != ml::RunState::Done) return (r.state == ml::RunState::Failed ? "failed: " : "stopped: ") + r.error.section('\n', 0, 0);
  const QString fam = res.value("family").toString();
  if (fam == "reinforcement") {
    const QJsonObject b = res.value("benchmarks").toObject();
    return "test return " + pct(jn(res.value("test").toObject().value("total_return"))) + " vs buy & hold " +
           pct(jn(b.value("buy_hold").toObject().value("total_return"))) + ", beats " + pct(jn(b.value("random").toObject().value("beaten")), 0) + " of random agents";
  }
  if (fam == "unsupervised") {
    const QString k = res.value("kind").toString();
    if (k == "clustering") return QString("%1 groups, silhouette %2").arg(res.value("groups").toArray().size()).arg(num(jn(res.value("quality").toObject().value("silhouette")), 2));
    if (k == "anomaly") return pct(jn(res.value("anomaly_share"))) + " unusual rows";
    return "reduction to " + QString::number(res.value("components").toArray().size()) + " axes";
  }
  if (fam == "self_supervised") {
    const QJsonArray h = res.value("history").toArray();
    return h.isEmpty() ? QStringLiteral("done") : "loss " + num(h.first().toObject().value("train").toDouble(), 3) + " → " + num(h.last().toObject().value("train").toDouble(), 3);
  }
  const QJsonObject te = res.value("test").toObject(), base = res.value("baseline").toObject();
  if (res.value("task").toString() == "classification")
    return "test balanced accuracy " + pct(jn(te.value("balanced_accuracy"))) + " vs no skill " + pct(jn(base.value("balanced_accuracy")));
  return "test MAE " + num(jn(te.value("mae"))) + " vs no skill " + num(jn(base.value("mae")));
}
void runsList(const QJsonObject& a, ToolDone done) {
  const int n = intArg(a, "last", 10, 1, 30);
  const qint64 asOf = asOfFor("ml_runs");   // evaluation mode: the trainings on data sets cut at or before the moment
  QList<ml::Run> list;
  for (const auto& r : ml::Runs::instance().list()) if (visibleRun(r, asOf)) list << r;
  if (list.isEmpty()) { done(okText("No run yet.")); return; }
  QString out = QString("%1 run(s), newest first:").arg(list.size());
  for (int i = 0; i < list.size() && i < n; ++i) {
    const auto& r = list[i];
    out += "\n- " + r.id + " · " + r.created.toString("yyyy-MM-dd HH:mm") + " · \"" + r.name + "\" · " + familyOfRun(r) + "/" +
           r.job.value("algorithm").toString() + " · data " + r.dataset + (r.by.isEmpty() ? QStringLiteral(" · by the user") : " · by " + r.by) +
           "\n  " + headline(r);
  }
  done(okText(out));
}

QString metrics(const QJsonObject& o, bool cls) {
  QStringList m;
  const QStringList keys = cls ? QStringList{"balanced_accuracy", "accuracy", "roc_auc", "f1"} : QStringList{"mae", "rmse", "r2", "direction_accuracy"};
  for (const auto& k : keys) if (o.contains(k)) m << k + " " + ((cls || k == "direction_accuracy") ? pct(jn(o.value(k))) : num(jn(o.value(k))));
  return m.join(", ");
}
QList<int> thinned(int n, int keep = 20) {
  QList<int> idx;
  if (n <= 0) return idx;
  const double step = n <= keep ? 1.0 : double(n - 1) / (keep - 1);
  for (double x = 0; qRound(x) < n; x += step) { const int i = qRound(x); if (idx.isEmpty() || idx.last() != i) idx << i; if (step == 0) break; }
  if (idx.last() != n - 1) idx << n - 1;
  return idx;
}
QString runReport(const ml::Run& r) {
  QString out = "run " + r.id + " \"" + r.name + "\" · " + familyOfRun(r) + "/" + r.job.value("algorithm").toString() + " · data " + r.dataset;
  if (r.state != ml::RunState::Done) return out + "\n" + headline(r);
  const QJsonObject res = r.result, job = r.job;
  const QString fam = res.value("family").toString();
  if (fam == "supervised" || fam == "semi_supervised") {
    const bool cls = res.value("task").toString() == "classification";
    const QJsonArray trp = res.value("train_period").toArray(), tep = res.value("test_period").toArray();
    const bool wfRun = res.value("walk_forward").isObject();
    out += QString(wfRun ? "\n%1 · target %2 of %3, %4 row(s) ahead · walk-forward: retrained before each test block, the last time on %5 rows "
                           "(%6 → %7); tested on %8 rows (%9 → %10)"
                         : "\n%1 · target %2 of %3, %4 row(s) ahead · trained on %5 rows (%6 → %7), tested on the next %8 (%9 → %10)")
               .arg(res.value("task").toString(), job.value("target_transform").toString(), job.value("target").toString())
               .arg(job.value("horizon").toInt()).arg(res.value("train_rows").toInt())
               .arg(trp.at(0).toString().left(10), trp.at(1).toString().left(10)).arg(res.value("test_rows").toInt())
               .arg(tep.at(0).toString().left(10), tep.at(1).toString().left(10));
    out += "\nTEST: " + metrics(res.value("test").toObject(), cls);
    out += "\nNO SKILL (" + res.value("baseline").toObject().value("rule").toString() + "): " + metrics(res.value("baseline").toObject(), cls);
    if (res.value("persistence").isObject())   // the last seen value of the target, again: a naive rule (written since 2026-11-19)
      out += QString("\nPERSISTENCE (\"what was last seen happens again\", %1 rows): ").arg(res.value("persistence").toObject().value("rows").toInt()) +
             metrics(res.value("persistence").toObject(), cls);
    if (res.value("data").isObject())
      out += "\ndata version: " + res.value("data").toObject().value("sha256").toString().left(12) + " · " +
             QString::number(res.value("data").toObject().value("rows").toInt()) + " rows";
    out += "\nTRAIN: " + metrics(res.value("train").toObject(), cls) + "  (far above TEST = learned the past by heart)";
    out += tradingText(res.value("trading").toObject());
    if (const QJsonObject refs = res.value("references").toObject(); !refs.isEmpty()) {   // volatility targets
      for (const char* name : {"har", "ewma"}) {
        const QJsonObject vs = refs.value(QString("vs_") + name).toObject();
        const double p = jn(vs.value("p")), pw = jn(vs.value("p_worse"));
        out += QString("\nAGAINST %1 (%2): %3 → the model %4 (paired moving-block bootstrap on the test rows, p %5, p_worse %6)")
                   .arg(QString(name).toUpper(), refs.value(name).toObject().value("rule").toString(), metrics(refs.value(name).toObject(), cls),
                        p < 0.05 ? "does BETTER" : pw < 0.05 ? "does WORSE" : "is not distinguishable", num(p, 3), num(pw, 3));
      }
      if (res.contains("move_threshold_pct")) out += QString("\nbig move = the furthest move reaches %1 %").arg(num(jn(res.value("move_threshold_pct")), 3));
    }
    const QJsonObject wfr = res.value("walk_forward").toObject();
    if (!wfr.isEmpty()) {
      QStringList bl;
      const QString key = wfr.value("metric").toString();
      for (const auto& v : wfr.value("blocks").toArray()) {
        const QJsonObject b = v.toObject();
        bl << b.value("from").toString().left(10) + "→" + b.value("to").toString().left(10) + " " + (cls ? pct(jn(b.value(key))) : num(jn(b.value(key)))) +
                  QString(" (trained on %1 rows)").arg(b.value("train_rows").toInt());
      }
      out += QString("\nwalk-forward: TEST is over %1 blocks, each predicted by a model retrained on every row before it (%2 row(s) left out "
                     "before each block); NO SKILL is each block's own; %3 by block: %4").arg(wfr.value("folds").toInt())
                 .arg(wfr.value("gap_rows").toInt()).arg(key, bl.join(" | "));
    }
    const QJsonObject cv = res.value("cv").toObject();
    if (!cv.isEmpty()) out += QString("\ncross-validation (%1 folds in time order): %2 = %3 ± %4").arg(cv.value("folds").toInt())
                                  .arg(cv.value("metric").toString(), cls ? pct(jn(cv.value("mean"))) : num(jn(cv.value("mean"))), cls ? pct(jn(cv.value("std"))) : num(jn(cv.value("std"))));
    const QJsonObject sm = res.value("semi").toObject();
    if (!sm.isEmpty())
      out += QString("\nsemi: %1 labels kept (%2), %3 hidden · same model, kept labels only: %4 · every label: %5 (balanced accuracy)")
                 .arg(sm.value("labelled").toInt()).arg(pct(jn(sm.value("share")))).arg(sm.value("hidden").toInt())
                 .arg(pct(jn(sm.value("labels_only").toObject().value("balanced_accuracy"))), pct(jn(sm.value("all_labels").toObject().value("balanced_accuracy"))))
             + (sm.contains("pseudo_labelled") ? QString(" · it labelled %1 rows itself in %2 round(s)").arg(sm.value("pseudo_labelled").toInt()).arg(sm.value("iterations").toInt()) : QString())
             + (sm.value("blank").toInt() > 0 ? QString(" · %1 rows without a label in the file").arg(sm.value("blank").toInt()) : QString());
    QStringList imp;
    for (const auto& v : res.value("importances").toObject().value("items").toArray()) {
      if (imp.size() >= 8) break;
      imp << v.toObject().value("feature").toString() + " " + pct(jn(v.toObject().value("share")));
    }
    if (!imp.isEmpty()) out += "\nrelies on (" + res.value("importances").toObject().value("method").toString() + "): " + imp.join(", ");
    const QJsonObject cm = res.value("confusion").toObject();
    if (!cm.isEmpty()) {   // rows: actual class, columns: predicted class
      const QJsonArray labels = cm.value("labels").toArray(), mx = cm.value("matrix").toArray();
      QStringList rows;
      for (int i = 0; i < labels.size(); ++i) {
        QStringList cells; for (const auto& v : mx.at(i).toArray()) cells << QString::number(v.toInt());
        rows << "actual " + QString::number(labels[i].toInt()) + ": " + cells.join(" / ");
      }
      QStringList cols; for (const auto& l : labels) cols << QString::number(l.toInt());
      out += "\nconfusion on the test rows (columns = predicted " + cols.join(" / ") + "): " + rows.join("; ");
    }
    const QJsonObject lt = res.value("latest").toObject();
    if (!lt.isEmpty()) out += "\nlatest row " + lt.value("date").toString().left(16) + ": prediction " + num(jn(lt.value("prediction"))) +
                              (lt.contains("probability") ? ", probability of class 1 " + pct(jn(lt.value("probability"))) : QString());
  } else if (fam == "unsupervised") {
    out += "\n" + res.value("kind").toString() + " · " + QString::number(res.value("rows").toInt()) + " rows · all used, nothing predicted";
    const QJsonObject q = res.value("quality").toObject();
    if (!q.isEmpty()) out += "\nquality: silhouette " + num(jn(q.value("silhouette")), 2) + " (−1..1), Davies-Bouldin " + num(jn(q.value("davies_bouldin")), 2) + " (lower is better)";
    if (res.contains("bic")) out += "\nBIC " + num(jn(res.value("bic")), 0) + ", AIC " + num(jn(res.value("aic")), 0) + " (lower is better, to compare runs with another number of groups)";
    if (jn(res.value("noise_share")) > 0) out += "\n" + pct(jn(res.value("noise_share"))) + " of the rows fit no group (noise, label -1)";
    if (res.value("clipped").toInt() > 0)
      out += QString("\n%1 extreme values (beyond ±%2σ) were brought back to ±%2σ before grouping").arg(res.value("clipped").toInt()).arg(num(jn(res.value("clip_sigma")), 0));
    const QJsonArray scan = res.value("k_scan").toArray();
    if (!scan.isEmpty()) {
      QStringList ks;
      for (const auto& v : scan) ks << "k=" + QString::number(v.toObject().value("k").toInt()) + " " + num(v.toObject().value("silhouette").toDouble(), 3);
      out += "\nk scan, silhouette per number of groups (higher = better separated): " + ks.join(", ");
    }
    for (const auto& gv : res.value("groups").toArray()) {
      const QJsonObject g = gv.toObject();
      QStringList why;
      for (const auto& w : g.value("defining").toArray()) { if (why.size() >= 3) break; why << w.toObject().value("feature").toString() + " " + num(w.toObject().value("z").toDouble(), 1) + "σ"; }
      out += QString("\ngroup %1: %2 rows (%3), lasts ≈ %4 rows, sets apart: %5").arg(g.value("label").toInt()).arg(g.value("size").toInt())
                 .arg(pct(jn(g.value("share")), 0)).arg(num(jn(g.value("avg_stretch")), 1)).arg(why.join(", "));
      const QJsonObject o = g.value("outcome").toObject();
      if (!o.isEmpty()) out += " · outcome after it: mean " + num(jn(o.value("mean"))) + (o.value("up_share").isDouble() ? ", up " + pct(jn(o.value("up_share")), 0) : QString());
    }
    QStringList an;
    for (const auto& v : res.value("anomalies").toArray()) { if (an.size() >= 10) break; an << v.toObject().value("date").toString().left(10); }
    if (!an.isEmpty()) out += "\nmost unusual rows: " + an.join(", ");
    for (const auto& c : res.value("components").toArray()) {
      QStringList t; for (const auto& x : c.toObject().value("top").toArray()) t << x.toObject().value("feature").toString() + " " + num(x.toObject().value("loading").toDouble(), 2);
      out += "\naxis: explains " + pct(jn(c.toObject().value("explained"))) + ", made of " + t.join(", ");
    }
    const QJsonObject lt = res.value("latest").toObject();
    if (!lt.isEmpty()) out += "\nlatest row " + lt.value("date").toString().left(10) + ": group/flag " + QString::number(lt.value("label").toInt());
  } else if (fam == "self_supervised") {
    out += "\n" + headline(r) + " · " + QString::number(res.value("windows").toInt()) + " windows of " + QString::number(res.value("window").toInt()) + " rows";
    const QJsonObject p = res.value("probe").toObject();
    if (!p.isEmpty()) {
      const bool cls = p.value("task").toString() == "classification";
      const char* k = cls ? "balanced_accuracy" : "mae";
      auto v = [&](const char* n) { const double x = jn(p.value(n).toObject().value(k)); return cls ? pct(x) : num(x); };
      out += QString("\nprobe (%1 on later windows): embeddings %2, raw last row %3, raw window %4, no skill %5").arg(k).arg(v("embeddings"), v("raw_now"), v("raw_window"), v("no_skill"));
      if (p.value("persistence").isObject()) out += ", persistence (last seen value again) " + v("persistence");
      out += tradingText(p.value("trading").toObject());   // the probe's predictions traded after costs
    }
    const QJsonArray hist = res.value("history").toArray();
    if (!hist.isEmpty()) {
      QStringList ep;
      for (int i : thinned(int(hist.size()))) {
        const QJsonObject h = hist[i].toObject();
        ep << QString("%1: %2 / %3").arg(h.value("epoch").toInt()).arg(num(h.value("train").toDouble(), 3), h.value("valid").isDouble() ? num(h.value("valid").toDouble(), 3) : QStringLiteral("n/a"));
      }
      out += "\nloss per epoch (training windows / later windows never trained on): " + ep.join(", ");
    }
    QStringList un; for (const auto& x : res.value("unusual").toArray()) { if (un.size() >= 8) break; un << x.toObject().value("date").toString().left(10); }
    if (!un.isEmpty()) out += "\nwindows hardest to rebuild/predict: " + un.join(", ");
  } else if (fam == "reinforcement") {
    auto line = [](const QString& n, const QJsonObject& o) {
      return "\n" + n + ": return " + pct(jn(o.value("total_return"))) + ", per year " + pct(jn(o.value("annual_return"))) + ", Sharpe " +
             num(jn(o.value("sharpe")), 2) + ", max drawdown " + pct(jn(o.value("max_drawdown"))) + ", trades " + QString::number(o.value("trades").toInt()) +
             ", in the market " + pct(jn(o.value("exposure")), 0) + " of the time";
    };
    const QJsonObject b = res.value("benchmarks").toObject(), rnd = b.value("random").toObject();
    const QJsonArray tep = res.value("test_period").toArray();
    out += "\ntest period " + tep.at(0).toString().left(10) + " → " + tep.at(1).toString().left(10) + " (never seen while learning), costs " +
           num(jn(res.value("cost_bp")), 1) + " bp, simulation only";
    out += line("agent, test", res.value("test").toObject()) + line("buy & hold, test", b.value("buy_hold").toObject()) +
           line("flat (no position), test", b.value("flat").toObject());
    out += "\nrandom agents trading as often: mean " + pct(jn(rnd.value("mean"))) + ", 5–95% " + pct(jn(rnd.value("p05"))) + " to " + pct(jn(rnd.value("p95"))) +
           "; the agent beats " + pct(jn(rnd.value("beaten")), 0) + " of them (below 95% = within luck)";
    out += line("agent, training", res.value("train").toObject()) + "  (far above test = learned by heart)";
    const QJsonArray hist = res.value("history").toArray();
    if (!hist.isEmpty()) {
      QStringList ep;
      for (int i : thinned(int(hist.size()))) {
        const QJsonObject h = hist[i].toObject();
        ep << QString("%1: %2 / %3").arg(h.value("episode").toInt()).arg(pct(jn(h.value("train_return"))), pct(jn(h.value("test_return"))));
      }
      out += "\nresult per episode (training rows / test rows): " + ep.join(", ");
    }
    out += "\nlatest row: " + res.value("latest").toObject().value("action").toString();
  }
  return out;
}
void runGet(const QJsonObject& a, ToolDone done) {
  const QString id = strArg(a, "run_id");
  const qint64 asOf = asOfFor("ml_run");
  if (!ml::Runs::instance().find(id) || !visibleRun(*ml::Runs::instance().find(id), asOf)) { done(fail("error: no run \"" + id + "\"; see ml_runs")); return; }
  const int wait = intArg(a, "wait_seconds", 0, 0, 25);
  auto answer = [id, done] {
    const ml::Run* r = ml::Runs::instance().find(id);
    if (!r) { done(fail("error: the run is gone")); return; }
    done(okText(runReport(*r)));
  };
  const ml::Run* r = ml::Runs::instance().find(id);
  if (wait == 0 || r->state != ml::RunState::Running) { answer(); return; }
  auto* ctx = new QObject(qApp);
  auto once = std::make_shared<bool>(false);
  auto fire = [ctx, once, answer] { if (*once) return; *once = true; ctx->deleteLater(); answer(); };
  QObject::connect(&ml::Runs::instance(), &ml::Runs::runChanged, ctx, [id, fire](const QString& rid) {
    const ml::Run* x = ml::Runs::instance().find(rid);
    if (rid == id && x && x->state != ml::RunState::Running) fire();
  });
  QTimer::singleShot(wait * 1000, ctx, fire);
}
void runRows(const QJsonObject& a, ToolDone done) {
  const ml::Run* r = ml::Runs::instance().find(strArg(a, "run_id"));
  if (!r || !visibleRun(*r, asOfFor("ml_run_rows"))) { done(fail("error: no such run; see ml_runs")); return; }
  if (r->state != ml::RunState::Done) { done(fail("error: the run is not finished")); return; }
  const QString fam = familyOfRun(*r);
  const QString file = fam == "reinforcement" ? "equity.csv" : (fam == "unsupervised" || fam == "self_supervised") ? "assignments.csv" : "predictions.csv";
  QFile f(ml::runFile(*r, file));   // never through a link the trainer planted (its rows go to a model)
  if (!f.open(QIODevice::ReadOnly)) { done(fail("error: no rows for this run")); return; }
  QTextStream in(&f);
  const QStringList header = ml::csvCells(in.readLine());   // the same reading as describe(): quotes, spaces
  QList<int> keep;   // embeddings (emb_0…) left out: numbers without meaning on their own
  for (int k = 0; k < header.size(); ++k) if (!header[k].startsWith("emb_")) keep << k;
  const int last = intArg(a, "last_rows", 20, 1, 200);
  f.close();
  const QStringList rows = ml::lastLines(f.fileName(), last);   // from the end (a prediction per row of the set)
  QStringList h; for (int k : keep) h << header[k];
  QString out = file + " of run " + r->id + ", last " + QString::number(rows.size()) + " rows\n" + h.join(",") + "\n";
  for (const auto& row : rows) { const QStringList c = row.split(','); QStringList o; for (int k : keep) o << c.value(k); out += o.join(",") + "\n"; }
  done(okText(out));
}
}  // namespace

void mlCompare(const QJsonObject& args, ToolDone done);   // MlCompare.cpp

void registerMlTools(Tools& t) {
  const QString S = "ml";
  t.add({"ml_status", S, "AI › ML: whether the Python environment is ready, which libraries are available, the training running now.", schema({}), status});
  t.add({"ml_methods", S, "The learning families and their algorithms; with a family, each algorithm's parameters (names, ranges, defaults).",
         schema({{"family", str("supervised, unsupervised, semi_supervised, self_supervised or reinforcement; empty = overview")}}), methods});
  t.add({"ml_datasets", S, "Data sets available for training: id, rows, period, numeric columns.", schema({}), datasets});
  t.add({"ml_load_dataset", S, "Builds a data set from Deepwater's data: prices of a market (with ready-made features); technical-data: Data › Technical data's stored "
         "bars of a recorded pair (Binance crypto with volumes and taker-buy share, or Dukascopy forex), gaps and the pair's indicators of Charts' "
         "engine (and their distance to the close), the newest rows, warm-up bars left out; a country's macro table; or a COT market with its futures price.",
         schema({{"source", str("prices, technical-data, macro or cot")},
                 {"key", str("prices: symbol (EURUSD, XAUUSD, SPX…); technical-data: a recorded pair, BINANCE:BTCUSDT (default), ETHUSDT, EURUSD…; macro: country code (us, fr, ez…); cot: market id (EUR, GOLD…)")},
                 {"interval", str("prices: 1d (default), 1wk or 1h; technical-data: 1m, 5m, 15m, 1h (default), 4h, 1D or 1W")},
                 {"rows", integer("technical-data only: the newest bars to take: 50000, 200000 (default), 500000 or 1000000 (another number goes up to the next of these)")},
                 {"keep_warm", QJsonObject{{"type", "boolean"}, {"description", "technical-data only: keep the first 600 bars of the series (long averages not settled), default false"}}},
                 {"order_flow", QJsonObject{{"type", "boolean"}, {"description",
                     "technical-data only (Binance pairs recording them, 1m to 1D): add the order flow columns (fp_spot_* / fp_perp_* footprint "
                     "per bar: delta, POC, value area, imbalances, stacks, deltas at the lows and highs; depth_*: the perpetual's book "
                     "imbalance at ±0.2/1/5 % and depth changes; oi_*: open interest change and long/short ratios) and keep only the "
                     "bars they cover, so a run on the bar columns alone and one with them are trained on the same rows. Default false"}}}},
                {"source"}), loadDataset});
  t.add({"ml_read_dataset", S, "The latest rows of a data set (date and chosen columns).",
         schema({{"dataset_id", str("from ml_datasets")}, {"columns", strings("columns to read; empty = every numeric column")},
                 {"last_rows", integer("rows from the end, default 20, max 200")}}, {"dataset_id"}), readDataset});
  t.add({"ml_train", S, "Starts one training on a data set (one at a time; it runs in a sandbox). Returns its run id; read the result with ml_run.",
         schema({{"dataset_id", str("from ml_datasets")}, {"family", str("supervised, unsupervised, semi_supervised, self_supervised or reinforcement")},
                 {"algorithm", str("an algorithm id of that family (ml_methods)")}, {"target", str("a numeric column: target, outcome, probe or traded price depending on the family")},
                 {"target_transform", str("raw, change, pct_change, direction; or how much a price moves: realized_vol, range, abs_return, big_move")},
                 {"move_pct", number("big_move: the move (% from the price now) that counts as big; default the training rows' 80th percentile")}, {"horizon", integer("rows ahead for the target (default 1)")},
                 {"features", strings("numeric columns to learn from; empty = all but raw price levels")}, {"lags", integer("past values of each feature too, 0..10")},
                 {"params", QJsonObject{{"type", "object"}, {"description", "the algorithm's parameters (ml_methods with its family); omitted = defaults"}}},
                 {"test_size", number("share of the latest rows held out for testing, 0.05..0.5, default 0.2")},
                 {"cv_folds", integer("supervised: time-series cross-validation folds, 0..10")},
                 {"validation", str("supervised: split (default: one model trained once before the test part) or walk_forward (the test "
                                    "part cut into wf_folds blocks, each predicted by a model retrained on every row before it, the rows "
                                    "whose target reaches into the block left out: how a model retrained regularly would have done; same "
                                    "test rows as split, so ml_compare compares the two)")},
                 {"wf_folds", integer("walk_forward: blocks of the test part, 2..20, default 5")},
                 {"trade_side", str("supervised, a price target: how the test predictions are traded after costs (the result's "
                                    "TRADED block): long (default: long when the model says up, flat otherwise) or long_short")},
                 {"cost_bp", number("supervised: the cost a side for that trading, in basis points; default by market: Binance 15 "
                                    "(0.10 % fee + 0.05 % slippage, as the Strategy tester), forex 2, others 5")}, {"task", str("supervised: auto, classification or regression")},
                 {"name", str("optional short name")}}, {"dataset_id", "family", "algorithm"}), train});
  t.add({"ml_runs", S, "The latest trainings (the user's and the AIs'): id, family, algorithm, data, state, main result.",
         schema({{"last", integer("how many, default 10, max 30")}}), runsList});
  t.add({"ml_run", S, "One training's full result, read from its result file: metrics against the no-skill baseline, groups, probe, simulated trading… Can wait for it to finish.",
         schema({{"run_id", str("from ml_train or ml_runs")}, {"wait_seconds", integer("wait up to this many seconds (max 25) while it is still running")}}, {"run_id"}), runGet});
  t.add({"ml_run_rows", S, "The latest rows a finished training wrote: predictions vs actual (supervised), group per row (unsupervised), simulated equity (reinforcement).",
         schema({{"run_id", str("a finished run")}, {"last_rows", integer("rows from the end, default 20, max 200")}}, {"run_id"}), runRows});
  t.add({"ml_compare", S,
         "Compares finished trainings with figures Deepwater computes from their files (quote them; do not recompute). Two modes, "
         "chosen from the runs: SAME MARKET (one question — what is predicted, traded or grouped — on one market and test period): "
         "ranked, each against no skill and against persistence (the last seen value again) with a paired test on its test rows, the top "
         "two against each other, and the number of tries "
         "on that question and market (the best of many tries looks good by chance); SEVERAL MARKETS (one setup run on several "
         "markets): each against its own market's no skill or buy & hold, how many markets it beats. Other runs are listed apart. "
         "Give run_ids, or filters (newest first).",
         schema({{"run_ids", strings("runs to compare (from ml_runs / ml_train); omitted = the filters below")},
                 {"market", str("filter: part of the data set's name, e.g. BTCUSDT, EUR/USD")},
                 {"family", str("filter: supervised, unsupervised, semi_supervised, self_supervised or reinforcement")},
                 {"algorithm", str("filter: an algorithm id")}, {"target", str("filter: the target column")},
                 {"only_this_environment", QJsonObject{{"type", "boolean"}, {"description", "filter: only the runs this environment started"}}},
                 {"last", integer("filters: how many of the newest matching runs, default 40, max 200")}}),
         mlCompare, 120000});
}
}  // namespace ai
