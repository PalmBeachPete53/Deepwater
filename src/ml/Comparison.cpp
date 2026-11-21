#include "ml/Comparison.h"
#include "ml/Compare.h"
#include "ml/Runs.h"
#include "trials/Registry.h"
#include "trials/Stats.h"
#include <QFile>
#include <QTimeZone>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace ml::comparison {
namespace {
double jn(const QJsonValue& v) { return v.isDouble() ? v.toDouble() : NAN; }
QString familyGroup(const QString& f) { return f == "semi_supervised" ? QStringLiteral("supervised") : f; }

Item itemOf(const Run& r) {
  Item it;
  it.id = r.id; it.name = r.name; it.market = r.dataset; it.created = r.created; it.result = r.result; it.job = r.job; it.by = r.by;
  it.family = r.job.value("family").toString("supervised");
  it.algorithm = r.job.value("algorithm").toString();
  it.task = r.result.value("task").toString(r.job.value("task").toString());
  it.target = r.job.value("target").toString();
  it.transform = r.job.value("target_transform").toString("raw");
  it.horizon = r.job.value("horizon").toInt(1);
  it.lags = r.job.value("lags").toInt();
  it.testSize = r.job.value("test_size").toDouble(0.2);
  it.kind = r.result.value("kind").toString();
  it.params = QString::fromUtf8(QJsonDocument(r.job.value("params").toObject()).toJson(QJsonDocument::Compact));
  QStringList feats;
  for (const auto& f : r.result.value("features").toArray()) feats << f.toString();
  if (feats.isEmpty()) for (const auto& f : r.job.value("features").toArray()) feats << f.toString();
  feats.sort();
  it.features = feats.join(',');
  const QJsonArray tp = r.result.value("test_period").toArray();
  it.testPeriod = tp.size() == 2 ? tp.at(0).toString().left(10) + " → " + tp.at(1).toString().left(10) : QString();
  if (familyGroup(it.family) == "supervised") it.predictions = runFile(r, "predictions.csv");
  if (it.family == "self_supervised") {   // its probe's test windows
    it.predictions = runFile(r, "probe_predictions.csv");
    it.task = r.result.value("probe").toObject().value("task").toString();
    it.horizon = r.result.value("probe").toObject().value("horizon").toInt(1);
  }
  it.dataHash = r.result.value("data").toObject().value("sha256").toString().left(12);
  if (const QString rj = runFile(r, "refs.json"); !rj.isEmpty()) {
    QFile f(rj);
    if (f.open(QIODevice::ReadOnly)) {
      const QJsonObject o = QJsonDocument::fromJson(f.read(4096)).object();
      if (!o.value("ok").toBool(true)) it.refsWhy = o.value("why").toString().left(160);
    }
  }
  const QString fg = familyGroup(it.family);
  if (fg == "supervised")
    it.question = QString("%1 of %2 %3, %4 row(s) ahead").arg(it.task, it.transform, it.target).arg(it.horizon);
  else if (fg == "reinforcement")
    it.question = QString("trading %1 (%2, cost %3 bp)").arg(r.result.value("price").toString(it.target), r.result.value("positions").toString())
                      .arg(jn(r.result.value("cost_bp")), 0, 'f', 1);
  else if (fg == "unsupervised")
    it.question = QString("%1 on features %2").arg(it.kind.isEmpty() ? QStringLiteral("unsupervised") : it.kind, it.features.left(120));
  else {
    const QJsonObject pr = r.result.value("probe").toObject();
    it.question = QString("embeddings, probe: %1 of %2 %3, %4 row(s) ahead").arg(pr.value("task").toString(), pr.value("transform").toString(),
                                                                                 pr.value("target").toString()).arg(pr.value("horizon").toInt());
  }
  it.question = fg + " · " + it.question;
  it.setup = QStringList{fg, it.algorithm, it.params, it.features, QString::number(it.lags), QString::number(it.testSize, 'f', 3), it.question}.join('|');
  // walk-forward is another setup (the same with one cut is not); runs with one cut keep the setup they always had
  if (r.job.value("validation").toString() == "walk_forward") {
    it.setup += QString("|wf%1").arg(r.job.value("wf_folds").toInt(5));
    it.algorithm += QString(" · walk-forward %1").arg(r.job.value("wf_folds").toInt(5));   // shown apart from its one-cut twin
  }
  return it;
}

// the family of a training in the trial registry: its market and question, as ml_compare groups tries
QString trialFamily(const Item& it) { return "ml|" + it.market + "\x1f" + it.question; }

Kind kindOf(const Item& it) {
  const QString fg = familyGroup(it.family);
  return fg == "reinforcement" ? Kind::Trade : fg == "unsupervised" ? Kind::Groups : fg == "self_supervised" ? Kind::Probe : Kind::Predict;
}

// The score that ranks runs of one family (higher is better) and its reference on the same market.
struct Score { double value = NAN, reference = NAN, edge = NAN; QString key; bool values = false; };
Score scoreOf(const Item& it) {
  Score s;
  const QJsonObject res = it.result;
  switch (kindOf(it)) {
    case Kind::Predict: {
      const QJsonObject te = res.value("test").toObject(), base = res.value("baseline").toObject();
      if (it.task == "classification") {
        s.key = "balanced_accuracy";
        s.value = jn(te.value("balanced_accuracy")); s.reference = jn(base.value("balanced_accuracy"));
        s.edge = s.value - s.reference;
      } else {
        s.key = "mae"; s.values = true;
        s.value = jn(te.value("mae")); s.reference = jn(base.value("mae"));
        s.edge = s.reference > 0 ? (s.reference - s.value) / s.reference : NAN;   // share of the no-skill error removed
      }
      break;
    }
    case Kind::Trade:
      s.key = "test_return";
      s.value = jn(res.value("test").toObject().value("total_return"));
      s.reference = jn(res.value("benchmarks").toObject().value("buy_hold").toObject().value("total_return"));
      s.edge = s.value - s.reference;
      break;
    case Kind::Groups:
      s.key = "silhouette"; s.value = jn(res.value("quality").toObject().value("silhouette")); s.edge = s.value;
      break;
    case Kind::Probe: {
      const QJsonObject pr = res.value("probe").toObject(), emb = pr.value("embeddings").toObject(), nos = pr.value("no_skill").toObject();
      if (pr.value("task").toString() == "classification") {
        s.key = "probe_balanced_accuracy";
        s.value = jn(emb.value("balanced_accuracy")); s.reference = jn(nos.value("balanced_accuracy")); s.edge = s.value - s.reference;
      } else {
        s.key = "probe_mae"; s.values = true;
        s.value = jn(emb.value("mae")); s.reference = jn(nos.value("mae"));
        s.edge = s.reference > 0 ? (s.reference - s.value) / s.reference : NAN;
      }
      break;
    }
  }
  return s;
}

// The pieces read from files, once per run.
struct Cache {
  QHash<QString, compare::Preds> preds;
  const compare::Preds& of(const Item& it) {
    auto f = preds.find(it.id);
    if (f == preds.end()) f = preds.insert(it.id, compare::readPredictions(it.predictions));
    return *f;
  }
};

int resamples(qsizetype rows) { return rows > 5000 ? 400 : 1000; }

// A run against its no skill, on its own test rows: the no_skill column when the file has it, else the constant from
// result.json; NaN when neither is known (older runs predicting values, before references were written).
double pVsNoSkill(const Item& it, Cache& cache) {
  const auto& p = cache.of(it);
  if (!p.ok()) return NAN;
  const bool cls = it.task == "classification";
  QVector<double> ref;
  if (p.noSkill.size() == p.actual.size() && std::all_of(p.noSkill.cbegin(), p.noSkill.cend(), [](double v) { return std::isfinite(v); })) ref = p.noSkill;
  else {
    const QJsonObject base = it.family == "self_supervised" ? it.result.value("probe").toObject().value("no_skill").toObject()
                                                            : it.result.value("baseline").toObject();
    const double constant = cls ? jn(base.value("class")) : jn(base.value("value"));
    if (!std::isfinite(constant)) return NAN;
    ref = QVector<double>(p.actual.size(), constant);
  }
  return compare::pairedBootstrap(p.actual, p.predicted, ref, cls, std::max(5, it.horizon), resamples(p.actual.size())).p;
}

// Against persistence ("what was last seen happens again"), on the test rows where it is known.
struct VsPersistence { double score = NAN, edge = NAN, p = NAN; };
VsPersistence vsPersistence(const Item& it, Cache& cache) {
  VsPersistence v;
  const auto& p = cache.of(it);
  if (!p.ok() || p.persistence.size() != p.actual.size()) return v;
  QVector<double> a, m, r;
  for (int i = 0; i < p.actual.size(); ++i)
    if (std::isfinite(p.persistence[i])) { a << p.actual[i]; m << p.predicted[i]; r << p.persistence[i]; }
  if (a.size() < 10) return v;
  const bool cls = it.task == "classification";
  const auto t = compare::pairedBootstrap(a, m, r, cls, std::max(5, it.horizon), resamples(a.size()));
  v.p = t.p;
  if (cls) { v.score = t.scoreB; v.edge = t.scoreA - t.scoreB; }
  else { v.score = -t.scoreB; v.edge = -t.scoreB > 0 ? (-t.scoreB - -t.scoreA) / -t.scoreB : NAN; }   // scores are −MAE
  return v;
}

double adj(double p, int n) { return std::isfinite(p) ? compare::adjusted(p, n) : NAN; }

void predictRow(Row& row, const Item& it, int n, Cache& cache) {
  const Score s = scoreOf(it);
  row.value = s.value; row.reference = s.reference; row.edge = s.edge;
  row.p = pVsNoSkill(it, cache); row.adjP = adj(row.p, n);
  const VsPersistence vp = vsPersistence(it, cache);
  row.persScore = vp.score; row.persEdge = vp.edge; row.persP = vp.p; row.persAdjP = adj(vp.p, n);
  const QJsonObject refs = it.result.value("references").toObject();
  if (!refs.isEmpty()) {
    row.harP = jn(refs.value("vs_har").toObject().value("p")); row.harPWorse = jn(refs.value("vs_har").toObject().value("p_worse"));
    row.ewmaP = jn(refs.value("vs_ewma").toObject().value("p")); row.ewmaPWorse = jn(refs.value("vs_ewma").toObject().value("p_worse"));
  }
  QJsonObject tr = it.result.value("trading").toObject();
  if (tr.isEmpty()) tr = it.result.value("probe").toObject().value("trading").toObject();   // a self-supervised probe
  if (!tr.isEmpty()) {
    const QJsonObject x1 = tr.value("model").toObject().value("x1").toObject();
    row.tradeNet = jn(x1.value("total_return"));
    row.tradeSharpe = jn(x1.value("sharpe"));
    row.tradeBuyHold = jn(tr.value("buy_hold").toObject().value("total_return"));
    row.tradeP = jn(tr.value("vs_constant").toObject().value("p"));
    row.tradeCostBp = jn(tr.value("cost_bp"));
  }
}

// The best run against both references, after the tries.
void verdictOf(Group& g) {
  const Row& b = g.rows.first();
  auto state = [](double edge, double a) {   // 1 beats, 0 not distinguishable, -1 worse (significantly), 2 unknown
    if (!std::isfinite(edge)) return 2;
    if (!std::isfinite(a)) return edge > 0 ? 3 : -2;   // edge known, no test
    if (a < 0.05) return edge > 0 ? 1 : -1;
    return 0;
  };
  const int sN = state(b.edge, b.adjP), sP = state(b.persEdge, b.persAdjP);
  g.verdictAdjP = b.adjP;
  g.beatsPersistence = sP == 1;
  if (sN == 1 && sP == 1) g.verdict = Verdict::BeatsBoth;
  else if (sN == 1 && (sP == 0 || sP == -1)) { g.verdict = Verdict::BeatsNoSkillOnly; g.persBetter = sP == -1; }
  else if (sN == 1) g.verdict = Verdict::BeatsNoSkill;
  else if (sN == -1) g.verdict = Verdict::Worse;
  else if (sN == -2 || (std::isfinite(b.edge) && b.edge <= 0)) g.verdict = Verdict::NotBetter;
  else if (sN == 0) g.verdict = Verdict::Luck;
  else g.verdict = Verdict::Unknown;
}
}  // namespace

QList<const Run*> choose(const QJsonObject& a, int env, QString* error) {
  QList<const Run*> chosen;
  QStringList ids;
  if (a.value("run_ids").isArray()) for (const auto& v : a.value("run_ids").toArray()) if (!v.toString().trimmed().isEmpty()) ids << v.toString().trimmed();
  if (!ids.isEmpty()) {
    QStringList unknown;
    for (const QString& id : ids) {
      const Run* r = Runs::instance().find(id);
      if (r) chosen << r; else unknown << id;
    }
    if (!unknown.isEmpty() && error) *error = "unknown run id(s): " + unknown.join(", ");
    return chosen;
  }
  const QString market = a.value("market").toString().trimmed(), family = a.value("family").toString().trimmed(),
                algorithm = a.value("algorithm").toString().trimmed(), target = a.value("target").toString().trimmed();
  const int last = qBound(2, a.value("last").toInt(40), 200);
  const bool mine = a.value("only_this_environment").toBool(false);
  const QString myTag = env >= 0 ? QString("environment %1 · ").arg(env + 1) : QString();
  for (const auto& r : Runs::instance().list()) {
    if (chosen.size() >= last) break;
    if (!market.isEmpty() && !r.dataset.contains(market, Qt::CaseInsensitive)) continue;
    if (!family.isEmpty() && r.job.value("family").toString() != family) continue;
    if (!algorithm.isEmpty() && r.job.value("algorithm").toString() != algorithm) continue;
    if (!target.isEmpty() && r.job.value("target").toString() != target) continue;
    if (mine && (myTag.isEmpty() || !r.by.startsWith(myTag))) continue;
    chosen << &r;
  }
  return chosen;
}

Input gather(const QList<const Run*>& chosen) {
  Input in;
  for (const Run* r : chosen) {
    if (r->state != RunState::Done || r->result.isEmpty()) {
      in.leftOut << r->id + (r->state == RunState::Running ? " running" : r->state == RunState::Failed ? " failed" : r->state == RunState::Stopped ? " stopped" : " no result");
      continue;
    }
    in.items << itemOf(*r);
  }
  // the tries on each market and question: every setup the trial registry holds (the user's and every environment's,
  // deleted runs included), and every finished run here in case one was not written; re-runs of one setup give the same
  // result: they are one try
  QHash<QString, QSet<QString>> setups = trials::Registry::instance().configsByFamily("ml");
  for (const auto& r : Runs::instance().list()) {
    if (r.state != RunState::Done || r.result.isEmpty()) continue;
    const Item it = itemOf(r);
    setups[trialFamily(it)].insert(trials::fingerprint(it.setup.toUtf8()));
  }
  for (auto it = setups.cbegin(); it != setups.cend(); ++it)
    if (it.key().startsWith("ml|")) in.tries[it.key().mid(3)] = int(it->size());
  return in;
}

void recordTrial(const Run& r) {
  if (r.state != RunState::Done || r.result.isEmpty()) return;
  auto& reg = trials::Registry::instance();
  const Item it = itemOf(r);
  const Score sc = scoreOf(it);
  trials::Trial t;
  t.at = r.created.isValid() ? r.created.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch();
  t.kind = "ml";
  t.origin = r.by.isEmpty() ? "user" : "agent";
  t.who = r.by;
  t.place = r.by.isEmpty() ? "AI › ML" : "ml_train";
  t.family = trialFamily(it);
  t.config = trials::fingerprint(it.setup.toUtf8());
  t.label = it.algorithm + " · " + it.question;
  t.market = it.market;
  t.ref = r.id;
  const QJsonArray tp = r.result.value("test_period").toArray();
  auto secs = [](const QString& s) {
    QDateTime d = QDateTime::fromString(s, Qt::ISODate);
    if (!d.isValid()) d = QDateTime(QDate::fromString(s.left(10), Qt::ISODate), QTime(0, 0), QTimeZone::UTC);
    return d.isValid() ? d.toSecsSinceEpoch() : qint64(0);
  };
  if (tp.size() == 2) { t.from = secs(tp.at(0).toString()); t.to = secs(tp.at(1).toString()); }
  t.n = r.result.value("test_rows").toInt();
  t.scoreKey = sc.key;
  t.score = sc.value;
  t.reference = sc.reference;
  t.edge = sc.edge;
  if (kindOf(it) == Kind::Trade) {
    const QJsonObject te = r.result.value("test").toObject();
    t.sharpe = jn(te.value("sharpe"));
    t.netPct = jn(te.value("total_return")) * 100;
  } else if (const QJsonObject x1 = (r.result.contains("trading") ? r.result.value("trading") : r.result.value("probe").toObject().value("trading"))
                                         .toObject().value("model").toObject().value("x1").toObject(); !x1.isEmpty()) {
    t.sharpe = jn(x1.value("sharpe"));   // the predictions traded after costs (since 2026-11-20)
    t.netPct = jn(x1.value("total_return")) * 100;
  }
  t.detail = QJsonObject{{"job", r.job}, {"name", r.name}, {"data", it.dataHash}, {"test_period", it.testPeriod}};
  reg.add(t);
}

void recordTrials() {
  const QStringList done = trials::Registry::instance().refs("ml");
  const QSet<QString> known(done.cbegin(), done.cend());
  for (const auto& r : Runs::instance().list())
    if (!known.contains(r.id)) recordTrial(r);
}

QStringList similarTo(const QString& runId) {
  const Run* me = Runs::instance().find(runId);
  if (!me || me->state != RunState::Done || me->result.isEmpty()) return {};
  const Item mine = itemOf(*me);
  QStringList out{runId};
  for (const auto& r : Runs::instance().list()) {
    if (r.id == runId || r.state != RunState::Done || r.result.isEmpty()) continue;
    const Item it = itemOf(r);
    if ((it.market == mine.market && it.question == mine.question) || it.setup == mine.setup) out << r.id;
  }
  return out;
}

Report build(const Input& in) {
  Cache cache;
  Report rep;
  rep.compared = int(in.items.size());
  rep.leftOut = in.leftOut;
  QSet<QString> placed;
  const QList<Item>& items = in.items;

  // ---- SAME MARKET: one question, one market, one test period, one data version
  QMap<QString, QList<Item>> byMarket;
  for (const auto& it : items) byMarket[it.market + "\x1f" + it.question + "\x1f" + it.testPeriod + "\x1f" + it.dataHash] << it;
  QHash<QString, int> copies;   // run id shown → runs of its setup in the group
  for (auto g = byMarket.begin(); g != byMarket.end(); ++g) {
    QMap<QString, Item> newest;
    QHash<QString, int> count;
    for (const auto& it : g.value()) {
      ++count[it.setup];
      if (!newest.contains(it.setup) || newest[it.setup].created < it.created) newest[it.setup] = it;
    }
    for (const auto& it : g.value()) placed.insert(it.id);   // compared, as a copy of the line shown
    QList<Item> kept;
    for (const auto& it : newest) { copies[it.id] = count.value(it.setup); kept << it; }
    g.value() = kept;
  }
  for (auto g = byMarket.cbegin(); g != byMarket.cend(); ++g) {
    QList<Item> grp = g.value();
    if (grp.isEmpty()) continue;
    const Item& one = grp.first();
    if (grp.size() < 2) {
      if (copies.value(one.id) < 2) { placed.remove(one.id); continue; }
      Group og;
      og.mode = Group::OneSetupOnly; og.kind = kindOf(one); og.market = one.market; og.question = one.question;
      og.onlyRun = one.id; og.copiesOnly = copies.value(one.id);
      rep.groups << og;
      continue;
    }
    std::sort(grp.begin(), grp.end(), [](const Item& a, const Item& b) {
      const double ea = scoreOf(a).edge, eb = scoreOf(b).edge;
      return (std::isfinite(ea) ? ea : -1e9) > (std::isfinite(eb) ? eb : -1e9);
    });
    const Item& f0 = grp.first();
    Group G;
    G.mode = Group::SameMarket; G.kind = kindOf(f0);
    const Score s0 = scoreOf(f0);
    G.values = s0.values; G.scoreKey = s0.key;
    G.market = f0.market; G.question = f0.question; G.testPeriod = f0.testPeriod; G.dataHash = f0.dataHash;
    G.setups = int(grp.size());
    G.tries = in.tries.value(f0.market + "\x1f" + f0.question, int(grp.size()));
    for (const auto& it : grp) {
      Row row;
      row.id = it.id; row.name = it.name; row.market = it.market; row.algorithm = it.algorithm; row.params = it.params;
      row.copies = copies.value(it.id, 1);
      if (G.kind == Kind::Predict || G.kind == Kind::Probe) predictRow(row, it, G.tries, cache);
      else if (G.kind == Kind::Trade) {
        const Score s = scoreOf(it);
        const QJsonObject te = it.result.value("test").toObject();
        row.value = s.value; row.reference = s.reference; row.edge = s.edge;
        row.beaten = jn(it.result.value("benchmarks").toObject().value("random").toObject().value("beaten"));
        row.sharpe = jn(te.value("sharpe")); row.maxDrawdown = jn(te.value("max_drawdown"));
      } else {
        const QJsonObject q = it.result.value("quality").toObject();
        row.groups = int(it.result.value("groups").toArray().size());
        row.silhouette = jn(q.value("silhouette")); row.daviesBouldin = jn(q.value("davies_bouldin")); row.bic = jn(it.result.value("bic"));
      }
      G.rows << row;
    }
    if (G.kind == Kind::Predict || G.kind == Kind::Probe) {
      const auto& pa = cache.of(grp[0]);
      const auto& pb = cache.of(grp[1]);
      if (pa.ok() && pb.ok()) {
        const auto c = compare::commonRows(pa.dates, pb.dates);
        QVector<double> act, A, B;
        for (int i = 0; i < c.ia.size(); ++i) { act << pa.actual[c.ia[i]]; A << pa.predicted[c.ia[i]]; B << pb.predicted[c.ib[i]]; }
        const auto r = compare::pairedBootstrap(act, A, B, f0.task == "classification", std::max(5, f0.horizon), resamples(act.size()));
        G.topA = grp[0].id; G.topB = grp[1].id; G.topRows = r.rows; G.topDiff = r.diff; G.topP = r.p;
      }
      // overfitting across the setups: each one's score row by row on the test rows they all share
      if (grp.size() >= 3) {
        QList<const compare::Preds*> ps;
        for (const auto& it : grp)
          if (const auto& p = cache.of(it); p.ok()) ps << &p;
        if (ps.size() >= 3) {
          QSet<QString> common(ps[0]->dates.cbegin(), ps[0]->dates.cend());
          for (int k = 1; k < ps.size(); ++k) common &= QSet<QString>(ps[k]->dates.cbegin(), ps[k]->dates.cend());
          QVector<QVector<double>> m;
          for (const auto* p : ps) {
            QVector<double> act, pred;
            for (int i = 0; i < p->dates.size(); ++i)   // the file's order: time order
              if (common.contains(p->dates[i])) { act << p->actual[i]; pred << p->predicted[i]; }
            m << compare::rowScores(act, pred, f0.task == "classification");
          }
          const auto pb = compare::pbo(m);
          if (pb.splits) { G.pbo = pb.pbo; G.pboRows = pb.rows; G.pboSplits = pb.splits; }
        }
      }
      verdictOf(G);
    } else if (G.kind == Kind::Trade) {
      const QJsonObject rnd = f0.result.value("benchmarks").toObject().value("random").toObject();
      G.noiseLo = jn(rnd.value("p05")); G.noiseHi = jn(rnd.value("p95"));
    }
    rep.groups << G;
  }

  // ---- SEVERAL MARKETS: one setup, its newest run on each market
  QMap<QString, QMap<QString, Item>> bySetup;   // setup → market → newest run
  for (const auto& it : items) {
    auto& m = bySetup[it.setup];
    if (!m.contains(it.market) || m[it.market].created < it.created) m[it.market] = it;
  }
  for (auto g = bySetup.cbegin(); g != bySetup.cend(); ++g) {
    if (g.value().size() < 2) continue;
    const QList<Item> grp = g.value().values();
    const Item& f0 = grp.first();
    Group G;
    G.mode = Group::SeveralMarkets; G.kind = kindOf(f0);
    const Score s0 = scoreOf(f0);
    G.values = s0.values; G.scoreKey = s0.key;
    G.algorithm = f0.algorithm; G.params = f0.params; G.question = f0.question;
    double sumEdge = 0;
    for (const auto& it : grp) {
      placed.insert(it.id);
      Row row;
      row.id = it.id; row.name = it.name; row.market = it.market; row.algorithm = it.algorithm; row.params = it.params;
      const Score s = scoreOf(it);
      if (G.kind == Kind::Trade) {
        row.value = s.value; row.reference = s.reference; row.edge = s.edge;
        row.beaten = jn(it.result.value("benchmarks").toObject().value("random").toObject().value("beaten"));
        if (std::isfinite(s.edge)) { ++G.known; sumEdge += s.edge; if (s.edge > 0) ++G.beat; if (row.beaten >= 0.95) ++G.sig; }
      } else if (G.kind == Kind::Groups) {
        row.groups = int(it.result.value("groups").toArray().size());
        row.silhouette = s.value;
        for (const auto& gv : it.result.value("groups").toArray()) {
          const double st = jn(gv.toObject().value("avg_stretch"));
          row.longest = std::isfinite(row.longest) ? std::max(row.longest, st) : st;
        }
      } else {
        row.tries = in.tries.value(it.market + "\x1f" + it.question, 1);
        predictRow(row, it, row.tries, cache);
        if (std::isfinite(s.edge)) {
          ++G.known; sumEdge += s.edge;
          if (s.edge > 0) ++G.beat;
          if (std::isfinite(row.adjP) && row.adjP < 0.05 && s.edge > 0) ++G.sig;
        }
      }
      G.rows << row;
    }
    G.meanEdge = G.known ? sumEdge / G.known : NAN;
    rep.groups << G;
  }

  // ---- the rest
  for (const auto& it : items) {
    if (placed.contains(it.id)) continue;
    Excluded e{it.id, it.market, it.algorithm, it.question, Excluded::Alone};
    for (const auto& o : items) {
      if (o.id == it.id || o.market != it.market || o.question != it.question) continue;
      e.why = o.testPeriod != it.testPeriod ? Excluded::OtherPeriod : Excluded::OtherData;
      break;
    }
    rep.notCompared << e;
  }
  for (const auto& it : items) if (!it.refsWhy.isEmpty()) rep.withoutRefs << qMakePair(it.id, it.refsWhy);
  rep.nothing = placed.isEmpty();
  return rep;
}

// ------------------------------------------------------------------------------------------------ the agents' text
namespace {
constexpr int kMaxChars = 9000;
constexpr int kMaxRows = 12;
QString num(double v, int dec = 4) { return std::isfinite(v) ? QString::number(v, 'f', dec) : QStringLiteral("n/a"); }
QString pct(double v, int dec = 1) { return std::isfinite(v) ? QString::number(v * 100, 'f', dec) + "%" : QStringLiteral("n/a"); }
QString signedPct(double v, int dec = 1) { return std::isfinite(v) ? (v >= 0 ? "+" : "") + QString::number(v * 100, 'f', dec) + "%" : QStringLiteral("n/a"); }
QString pval(double p) { return !std::isfinite(p) ? QStringLiteral("n/a") : p <= 0 ? QStringLiteral("< 0.001") : QString::number(p, 'f', 3); }
QString shortParams(const QString& p) { return p == "{}" ? QStringLiteral("defaults") : p.size() > 40 ? p.left(37) + "…" : p; }
QString scoreName(const QString& key) {
  if (key == "balanced_accuracy") return "balanced accuracy";
  if (key == "mae") return "MAE";
  if (key == "probe_balanced_accuracy") return "probe balanced accuracy";
  if (key == "probe_mae") return "probe MAE";
  if (key == "test_return") return "test return";
  return key;
}
QString refName(const QString& key) { return key.endsWith("mae") ? QStringLiteral("no-skill MAE") : key == "test_return" ? QStringLiteral("buy & hold") : QStringLiteral("no skill"); }

QString verdictText(const Group& g) {
  const QString tries = QString("counting the %1 setup(s) tried").arg(g.tries);
  switch (g.verdict) {
    case Verdict::BeatsBoth: return "verdict: the best beats both no skill and persistence, " + tries + ".";
    case Verdict::BeatsNoSkillOnly:
      return "verdict: the best beats no skill (" + tries + ") but not persistence: a naive \"what was last seen happens again\" rule does " +
             QString(g.persBetter ? "better" : "as well") + ". Nothing a simple rule does not already give.";
    case Verdict::BeatsNoSkill: return "verdict: the best beats no skill, " + tries + " (no persistence on record for this run: made by an earlier version).";
    case Verdict::Worse:
    case Verdict::NotBetter:
      return QString(g.verdict == Verdict::Worse ? "verdict: even the best does worse than no skill, and not by chance" : "verdict: even the best does no better than no skill") +
             (g.beatsPersistence ? QString(", though it beats persistence here (on these series the training mean is the harder reference)") : QString()) +
             ": none of these setups shows skill.";
    case Verdict::Luck:
      return QString("verdict: after %1 setup(s) tried, the best is not distinguishable from no skill (adj. p %2): its edge can be luck%3.")
          .arg(g.tries).arg(pval(g.verdictAdjP), g.beatsPersistence ? QString(", even though it beats persistence") : QString());
    case Verdict::Unknown: return "verdict: no p against no skill for these runs (their no-skill prediction was not recorded: made by an earlier version).";
    case Verdict::None: break;
  }
  return {};
}
}  // namespace

QString toText(const Report& r) {
  QString out = QString("ml_compare: %1 finished training(s) compared").arg(r.compared);
  if (!r.leftOut.isEmpty()) out += QString(", %1 left out (%2)").arg(r.leftOut.size()).arg(r.leftOut.mid(0, 6).join("; "));
  out += ". Every figure below is computed by Deepwater from the runs' result and prediction files: quote them, do not "
         "recompute them. p = paired block bootstrap on the same test rows, two-sided (a small p says the scores differ; the edge's "
         "sign says which side); adj. p = p × the different setups tried on that market and question (Bonferroni; re-runs of one "
         "setup count once). Below 0.05: unlikely to be chance.\n";
  for (const Group& g : r.groups) {
    if (g.mode == Group::OneSetupOnly) {
      out += QString("\n== SAME MARKET · %1 · %2 · %3 runs of one setup only (%4): the same result each time; nothing to rank ==\n")
                 .arg(g.market, g.question).arg(g.copiesOnly).arg(g.onlyRun);
      continue;
    }
    if (g.mode == Group::SameMarket) {
      out += QString("\n== SAME MARKET · %1 · %2%3 · data %4 · %5 setup(s) here; %6 different setup(s) tried on this market and question in all ==\n")
                 .arg(g.market, g.question, g.testPeriod.isEmpty() ? QString() : " · test " + g.testPeriod,
                      g.dataHash.isEmpty() ? QStringLiteral("version not recorded (older runs)") : g.dataHash)
                 .arg(g.setups).arg(g.tries);
      auto runCell = [](const Row& row) { return row.copies > 1 ? QString("%1 (×%2 same setup)").arg(row.id).arg(row.copies) : row.id; };
      int k = 0;
      if (g.kind == Kind::Predict || g.kind == Kind::Probe) {
        out += QString("rank | run | algorithm | params | %1 | %2 | edge%3 | p vs no skill | adj. p | persistence | edge vs persistence | p vs persistence | adj. p\n")
                   .arg(scoreName(g.scoreKey), refName(g.scoreKey), g.values ? " (error removed)" : "");
        for (const Row& row : g.rows) {
          if (++k > kMaxRows) { out += QString("… %1 more\n").arg(g.rows.size() - kMaxRows); break; }
          out += QString("%1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9").arg(k).arg(runCell(row), row.algorithm, shortParams(row.params))
                     .arg(g.values ? num(row.value) : pct(row.value), g.values ? num(row.reference) : pct(row.reference), g.values ? pct(row.edge) : signedPct(row.edge))
                     .arg(pval(row.p), pval(row.adjP));
          out += QString(" | %1 | %2 | %3 | %4\n").arg(g.values ? num(row.persScore) : pct(row.persScore), g.values ? pct(row.persEdge) : signedPct(row.persEdge),
                                                       pval(row.persP), pval(row.persAdjP));
        }
        // volatility targets: each run against the HAR and EWMA references (the trainer's paired test on the same rows)
        QStringList vsRefs;
        auto word = [](double p, double pw) { return !std::isfinite(p) ? QString("n/a") : p < 0.05 ? QString("better") : pw < 0.05 ? QString("WORSE") : QString("not distinguishable"); };
        for (const Row& row : g.rows)
          if (std::isfinite(row.harP))
            vsRefs << QString("%1 %2: vs HAR %3, vs EWMA %4").arg(row.id, row.algorithm, word(row.harP, row.harPWorse), word(row.ewmaP, row.ewmaPWorse));
        if (!vsRefs.isEmpty())
          out += "against the volatility references (HAR, EWMA; p < 0.05 one way or the other): " + vsRefs.join("; ") + "\n";
        // the same runs in money: their test predictions traded after costs (runs since 2026-11-20)
        QStringList money;
        double bh = NAN, bp = NAN;
        for (const Row& row : g.rows) {
          if (!std::isfinite(row.tradeNet)) continue;
          if (money.size() >= kMaxRows) break;
          money << QString("%1 %2 net %3, Sharpe %4, timing vs its own average exposure p %5").arg(row.id, row.algorithm, signedPct(row.tradeNet),
                                                                                                   num(row.tradeSharpe, 2), pval(row.tradeP));
          bh = row.tradeBuyHold; bp = row.tradeCostBp;
        }
        if (!money.isEmpty())
          out += QString("traded after costs (%1 bp a side, long when up, flat otherwise; buy & hold %2 on the same rows): %3\n")
                     .arg(num(bp, 1), signedPct(bh), money.join("; "));
        if (!g.topA.isEmpty())
          out += QString("best vs second (%1 vs %2) on their %3 shared test rows: %4 %5, p %6 → %7\n")
                     .arg(g.topA, g.topB).arg(g.topRows)
                     .arg(g.values ? "MAE difference" : "balanced accuracy difference")
                     .arg(g.values ? num(-g.topDiff) : signedPct(g.topDiff), pval(g.topP),
                          g.topP < 0.05 ? "the best is really better on these rows" : "not significantly different: choose on other grounds (simpler model, steadier folds)");
        out += verdictText(g) + "\n";
        if (std::isfinite(g.pbo))
          out += QString("overfitting across these %1 setups (PBO, %2 shared test rows in 8 blocks, %3 splits): %4% chance that the setup best on "
                         "half the rows is at or below the median on the other half%5\n")
                     .arg(g.setups).arg(g.pboRows).arg(g.pboSplits).arg(g.pbo * 100, 0, 'f', 0)
                     .arg(g.pbo > 0.5 ? QString(": picking the best here is mostly picking luck") : QString());
      } else if (g.kind == Kind::Trade) {
        out += "rank | run | algorithm | params | test return | buy & hold | excess | random agents beaten | Sharpe | max drawdown\n";
        for (const Row& row : g.rows) {
          if (++k > kMaxRows) { out += QString("… %1 more\n").arg(g.rows.size() - kMaxRows); break; }
          out += QString("%1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10\n").arg(k).arg(runCell(row), row.algorithm, shortParams(row.params))
                     .arg(signedPct(row.value), signedPct(row.reference), signedPct(row.edge))
                     .arg(pct(row.beaten, 0), num(row.sharpe, 2), signedPct(row.maxDrawdown));
        }
        out += QString("noise: random agents trading as often returned %1 to %2 (5th–95th percentile); differences inside that band are chance. "
                       "No paired test for simulated trading.\n").arg(signedPct(g.noiseLo), signedPct(g.noiseHi));
      } else {
        out += "rank | run | algorithm | params | groups | silhouette | Davies-Bouldin | BIC\n";
        for (const Row& row : g.rows) {
          if (++k > kMaxRows) { out += QString("… %1 more\n").arg(g.rows.size() - kMaxRows); break; }
          out += QString("%1 | %2 | %3 | %4 | %5 | %6 | %7 | %8\n").arg(k).arg(runCell(row), row.algorithm, shortParams(row.params))
                     .arg(row.groups).arg(num(row.silhouette, 3), num(row.daviesBouldin, 3), num(row.bic, 0));
        }
        out += "silhouette: higher = groups further apart (above 0.5 well apart, near 0 overlapping); Davies-Bouldin: lower is better; "
               "BIC: lower is better, between Gaussian-mixture runs only. No test of significance for groupings.\n";
      }
      continue;
    }
    // SEVERAL MARKETS
    out += QString("\n== SEVERAL MARKETS · %1 %2 · %3 · %4 markets ==\nEach market against its own reference (raw scores do not compare across markets).\n")
               .arg(g.algorithm, shortParams(g.params), g.question).arg(g.rows.size());
    if (g.kind == Kind::Trade) {
      out += "market | run | test return | buy & hold | excess | random agents beaten\n";
      for (const Row& row : g.rows)
        out += QString("%1 | %2 | %3 | %4 | %5 | %6\n").arg(row.market, row.id, signedPct(row.value), signedPct(row.reference), signedPct(row.edge), pct(row.beaten, 0));
      out += QString("summary: above buy & hold on %1 of %2 markets, beats 95% of random agents on %3; mean excess %4.\n")
                 .arg(g.beat).arg(g.known).arg(g.sig).arg(signedPct(g.meanEdge));
    } else if (g.kind == Kind::Groups) {
      out += "market | run | groups | silhouette | longest-lasting group (rows)\n";
      for (const Row& row : g.rows)
        out += QString("%1 | %2 | %3 | %4 | %5\n").arg(row.market, row.id).arg(row.groups).arg(num(row.silhouette, 3), num(row.longest, 1));
      out += "summary: descriptive only (groupings on different markets have no common score); compare how many groups and how long they last.\n";
    } else {
      out += QString("market | run | %1 | %2 | edge | p vs no skill | adj. p (tries on that market) | edge vs persistence | p vs persistence\n")
                 .arg(scoreName(g.scoreKey), refName(g.scoreKey));
      for (const Row& row : g.rows) {
        out += QString("%1 | %2 | %3 | %4 | %5 | %6 | %7 (%8)").arg(row.market, row.id)
                   .arg(g.values ? num(row.value) : pct(row.value), g.values ? num(row.reference) : pct(row.reference), g.values ? pct(row.edge) : signedPct(row.edge))
                   .arg(pval(row.p), pval(row.adjP)).arg(row.tries);
        out += QString(" | %1 | %2\n").arg(g.values ? pct(row.persEdge) : signedPct(row.persEdge), pval(row.persP));
      }
      out += QString("summary: beats its own no skill on %1 of %2 markets, significantly (after tries) on %3; mean edge %4.%5\n")
                 .arg(g.beat).arg(g.known).arg(g.sig).arg(g.values ? pct(g.meanEdge) : signedPct(g.meanEdge),
                      g.sig == 1 && g.known > 1 ? QStringLiteral(" Significant on one market only: a setup that works on one market only is likely fitted to it.")
                      : g.beat == 0             ? QStringLiteral(" It shows skill on none of them.")
                                                : QString());
    }
  }
  if (!r.notCompared.isEmpty()) {
    QStringList alone;
    for (const auto& e : r.notCompared.mid(0, 15))
      alone << QString("%1 (%2 · %3 · %4): %5").arg(e.id, e.market, e.algorithm, e.question,
                                                   e.why == Excluded::OtherPeriod ? "same market and question as another run but a different test period: not on the same rows"
                                                   : e.why == Excluded::OtherData ? "same market, question and test period as another run but another version of the data set (rebuilt since, or not recorded for older runs): not the same rows"
                                                                                  : "no other run of this question on this market, and its setup was not run on another market");
    out += "\n== NOT COMPARED ==\n" + alone.join("\n") + (r.notCompared.size() > 15 ? QString("\n… %1 more").arg(r.notCompared.size() - 15) : QString()) + "\n";
  }
  if (!r.withoutRefs.isEmpty()) {
    QStringList l;
    for (const auto& w : r.withoutRefs.mid(0, 10)) l << w.first + ": " + w.second;
    out += "\n== WITHOUT REFERENCES ==\nThese older runs could not get their no-skill and persistence columns afterwards (their n/a above): " +
           l.join("; ") + ". Re-running the same setup on today's data set gives a run with them.\n";
  }
  if (r.nothing) out += "\nNothing to compare: give runs of the same question on one market, or one setup run on several markets.\n";
  if (out.size() > kMaxChars) out = out.left(kMaxChars) + "\n[cut: ask for fewer runs]";
  return out;
}

}  // namespace ml::comparison
