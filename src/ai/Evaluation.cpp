#include "ai/Evaluation.h"
#include "ai/Engine.h"
#include "ai/Slots.h"
#include "ai/Tools.h"
#include "charts/data/Providers.h"
#include "core/Paths.h"
#include "ml/Compare.h"
#include "ml/Runs.h"
#include "technicaldata/TechnicalDataStore.h"
#include "trials/Registry.h"
#include "trials/Stats.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimeZone>
#include <QRandomGenerator>
#include "ai/Blind.h"
#include <cmath>

namespace ai {
namespace ae = trials::agenteval;

namespace {
// not T(): inside Evaluation (a QObject) that name is QObject::tr, whose context is "ai::Evaluation"
QString T(const char* s) { return QCoreApplication::translate("ai", s); }
QString day(qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm"); }
QString pctText(double v, int dec = 2) { return std::isfinite(v) ? (v >= 0 ? "+" : "−") + QString::number(std::fabs(v) * 100, 'f', dec) + " %" : QStringLiteral("—"); }
QString pText(double p) { return !std::isfinite(p) ? QStringLiteral("—") : p < 0.001 ? QStringLiteral("< 0,001") : QString::number(p, 'f', 3); }
constexpr int kDecisionTimeoutMs = 8 * 60 * 1000;
// the market's 1-hour bars: the close of the last one closed by t, and the average of the 20 last closes
ae::Prices pricesOf(const QString& market) {
  ae::Prices p;
  p.closeAt = [market](qint64 t) -> std::optional<double> {
    const auto b = technicaldata::Store::instance().before(market, 3600, t - 3600 + 1, 1);
    if (b.isEmpty() || t - (b.last().t + 3600) > 3 * 3600) return std::nullopt;   // a hole of hours: no price there
    return b.last().c;
  };
  p.smaAt = [market](qint64 t) -> std::optional<double> {
    const auto b = technicaldata::Store::instance().before(market, 3600, t - 3600 + 1, 20);
    if (b.size() < 20) return std::nullopt;
    double s = 0;
    for (const auto& k : b) s += k.c;
    return s / b.size();
  };
  return p;
}
QString envFile(int env, const QString& name) { return Slots::instance().folder(env) + "/" + name; }
}  // namespace

Evaluation& Evaluation::instance() {
  static Evaluation* e = [] {
    auto* x = new Evaluation;
    x->m_timeout.setSingleShot(true);
    x->m_settle.setSingleShot(true);
    x->m_settle.setInterval(1500);
    QObject::connect(&x->m_timeout, &QTimer::timeout, x, [x] {
      if (Engine* e = Engines::instance().at(x->m_spec.env)) e->cancel();
      x->collect();
    });
    QObject::connect(&x->m_settle, &QTimer::timeout, x, [x] {
      Engine* e = Engines::instance().at(x->m_spec.env);
      if (!e || !x->m_waiting) return;
      if (e->state() == Engine::State::Busy || e->pending()) return;   // still writing: stateChanged will come again
      x->collect();
    });
    return x;
  }();
  return *e;
}

QString Evaluation::prompt(const QString& market, int h) {
  return QString("Evaluation task. Market: %1. It is the moment your tools answer as of. Using only your tools (the bars, the "
                 "order flow, the macro facts…), decide the position to hold from now for the next %2 hours: LONG, SHORT or "
                 "FLAT. Explain briefly what you read, then end with exactly one line: DECISION: LONG, DECISION: SHORT or "
                 "DECISION: FLAT.").arg(market).arg(h);
}

qint64 Evaluation::frontierOf(int env, QString* lastKnown) {
  QFile f(envFile(env, "knowledge-probe.json"));
  if (!paths::insideNoLink(Slots::instance().folder(env), f.fileName()) || !f.open(QIODevice::ReadOnly)) return 0;
  const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
  if (lastKnown) *lastKnown = o.value("last_known").toString();
  return qint64(o.value("frontier").toDouble());
}

void Evaluation::watch() {
  Engine* e = Engines::instance().at(m_spec.env);
  QObject::disconnect(m_conn);
  m_conn = connect(e, &Engine::stateChanged, this, [this, e] {
    if (m_waiting && e->state() != Engine::State::Busy) m_settle.start();
  });
}

QString Evaluation::answerSince(int index) const {
  const auto& msgs = Engines::instance().at(m_spec.env)->messages();
  QString out;
  for (int i = std::max(0, index); i < msgs.size(); ++i)
    if (msgs[i].role == "assistant") out += msgs[i].text + "\n";
  return out;
}

QString Evaluation::startProtocol(const EvalSpec& s) {
  if (m_running) return T("An evaluation is already running.");
  if (s.env < 0 || s.env >= Slots::kCount || Slots::instance().slot(s.env).isEmpty()) return T("This environment holds no model.");
  if (!Slots::instance().hasSkill(s.env, "read_data")) return T("Tick \"Read Deepwater's data\" for this environment: the model decides from its tools.");
  if (s.horizonHours < 1 || s.decisions < 2 || s.repeats < 1 || s.to <= s.from) return T("Give a period, a horizon and at least two decisions.");
  auto& st = technicaldata::Store::instance();
  const auto state = st.state(s.market, 3600);
  if (state.count == 0) return T("%1 has no 1-hour bars in Data › Technical data: the decisions are scored on them.").arg(s.market);
  const qint64 H = qint64(s.horizonHours) * 3600;
  if (s.to + H > state.last + 3600) return T("The period must end %1 hours before the last stored bar (%2), so each decision has its outcome.")
                                         .arg(s.horizonHours).arg(day(state.last + 3600));
  const qint64 step = (s.to - s.from) / (s.decisions - 1);
  if (step < H) return T("The decisions are closer than the horizon (%1 h): they would overlap. Fewer decisions or a longer period.").arg(s.horizonHours);
  m_spec = s;
  m_queue.clear();
  m_queueBlind.clear();
  for (int r = 0; r < s.repeats; ++r)
    for (int k = 0; k < s.decisions; ++k)
      for (bool b : {false, true}) {   // named, blind, or both at the same moment
        if ((b && s.mode == 0) || (!b && s.mode == 1)) continue;
        m_queue << qMakePair(r, (s.from + k * step) / 3600 * 3600);
        m_queueBlind << b;
      }
  m_decisions.clear();
  m_questionIds.clear();
  m_decBlind.clear();
  m_total = int(m_queue.size());
  m_done = 0;
  m_prevAsOf = Slots::instance().asOf(s.env);
  m_prevBlind = Slots::instance().blindSeed(s.env);
  m_blindSeed = QRandomGenerator::global()->generate() | 1u;   // the secret shift and factor of this evaluation
  m_running = true;
  m_probe = false;
  watch();
  emit progress();
  QTimer::singleShot(0, this, [this] { askNext(); });
  return {};
}

void Evaluation::askNext() {
  if (!m_running) return;
  if (m_queue.isEmpty()) { finishProtocol(); return; }
  Engine* e = Engines::instance().at(m_spec.env);
  // the previous answer may still be ending (a question cut by the timeout: its process stops asynchronously): wait
  // for the environment to be free, up to a minute, before the next question
  if (e->state() == Engine::State::Busy || e->pending()) {
    e->cancel();
    if (++m_idleTries <= 60) { QTimer::singleShot(1000, this, [this] { askNext(); }); return; }
    finishProtocol(T("the model did not take the question"));
    return;
  }
  m_idleTries = 0;
  m_current = m_queue.takeFirst();
  m_currentBlind = m_queueBlind.takeFirst();
  Slots::instance().setBlind(m_spec.env, m_currentBlind ? m_blindSeed : 0);
  Slots::instance().setAsOf(m_spec.env, m_current.second);   // a fresh context at that moment (Engine::syncClock)
  const QString market = m_currentBlind ? blind::alias(maskFor(m_spec.env)) : m_spec.market;
  if (!e->ask(prompt(market, m_spec.horizonHours))) { finishProtocol(T("the model did not take the question")); return; }
  m_askIndex = int(e->messages().size()) - 1;
  m_waiting = true;
  m_timeout.start(kDecisionTimeoutMs);
}

void Evaluation::collect() {
  if (!m_waiting) return;
  m_waiting = false;
  m_timeout.stop();
  const QString answer = answerSince(m_askIndex);
  if (m_probe) { finishProbe(answer); return; }
  if (m_forwardEnv >= 0) { finishForwardDecision(answer); return; }
  ae::Decision d;
  d.t = m_current.second;
  d.repeat = m_current.first;
  const auto pos = ae::parseDecision(answer);
  d.parsed = pos.has_value();
  d.pos = pos.value_or(0);
  m_decisions << d;
  m_questionIds << Engines::instance().at(m_spec.env)->questionId();
  m_decBlind << m_currentBlind;
  ++m_done;
  emit progress();
  QTimer::singleShot(500, this, [this] { askNext(); });
}

void Evaluation::stop() {
  if (!m_running) return;
  m_waiting = false;
  m_timeout.stop();
  if (Engine* e = Engines::instance().at(m_spec.env)) e->cancel();
  if (m_probe) {
    m_running = false;
    for (const QString& sk : m_probeSkills) Slots::instance().setSkill(m_spec.env, sk, true);
    Slots::instance().setBlind(m_spec.env, m_prevBlind);
    Slots::instance().setAsOf(m_spec.env, m_prevAsOf);
    emit progress();
    emit finished(m_spec.env);
    return;
  }
  finishProtocol(T("stopped by you"));
}

void Evaluation::finishProtocol(const QString& stopped) {
  m_running = false;
  m_waiting = false;
  m_timeout.stop();
  QObject::disconnect(m_conn);
  const int env = m_spec.env;
  Slots::instance().setBlind(env, m_prevBlind);
  Slots::instance().setAsOf(env, m_prevAsOf);   // the environment's own clock again
  const qint64 H = qint64(m_spec.horizonHours) * 3600;

  // the same question answered by an ML model without LLM: the newest finished supervised run predicting the direction
  // of the market's close H hours ahead on its 1-hour bars, at the decisions' moments (its last closed bar's row)
  QString mlRun;
  const QString shortName = m_spec.market.section(':', -1);
  for (const auto& r : ml::Runs::instance().list()) {
    if (r.state != ml::RunState::Done || r.job.value("family").toString() != "supervised" ||
        r.job.value("target_transform").toString() != "direction" || r.job.value("horizon").toInt() != m_spec.horizonHours ||
        !r.dataset.contains(shortName) || !r.dataset.contains("1h"))
      continue;
    const ml::compare::Preds p = ml::compare::readPredictions(ml::runFile(r, "predictions.csv"));
    if (!p.ok()) continue;
    QHash<qint64, int> at;
    for (int i = 0; i < p.dates.size(); ++i) {
      QDateTime t = QDateTime::fromString(p.dates[i].left(19), "yyyy-MM-dd HH:mm:ss");
      if (!t.isValid()) continue;
      t.setTimeZone(QTimeZone::UTC);
      at.insert(t.toSecsSinceEpoch(), std::lround(p.predicted[i]) == 1 ? 1 : 0);
    }
    int known = 0;
    for (auto& d : m_decisions) {
      const auto it = at.constFind(d.t - 3600);   // the row of the last bar closed at the moment
      d.mlKnown = it != at.cend();
      d.ml = d.mlKnown ? *it : 0;
      known += d.mlKnown;
    }
    if (known * 2 >= m_decisions.size()) { mlRun = r.id; break; }
    for (auto& d : m_decisions) { d.mlKnown = false; d.ml = 0; }
  }

  QString lastKnown;
  qint64 frontier = frontierOf(env, &lastKnown);
  const qint64 tierFrom = frontier > 0 ? QDateTime::fromSecsSinceEpoch(frontier, QTimeZone::UTC).addMonths(1).toSecsSinceEpoch() : 0;
  const ae::Summary s = ae::score(m_decisions, pricesOf(m_spec.market), H, m_spec.costBp / 1e4, tierFrom);

  // the report, in the environment's conversation
  const QString model = Slots::instance().slot(env).displayName();
  QString md = "### " + T("Evaluation of %1 on %2, %3 hours ahead").arg(model, m_spec.market).arg(m_spec.horizonHours) + "\n\n";
  if (!stopped.isEmpty()) md += "**" + T("Interrupted: %1.").arg(stopped) + "**\n\n";
  md += T("%1 decisions (%2 repeat(s)) from %3 to %4 UTC, each held %5 h after costs (%6 bp a side). %7 without a DECISION line (counted as flat), %8 without a price.")
            .arg(s.decisions).arg(s.repeats).arg(day(m_spec.from), day(m_spec.to)).arg(m_spec.horizonHours).arg(m_spec.costBp, 0, 'f', 1)
            .arg(s.all.unparsed).arg(s.skipped) + "\n\n";
  md += (frontier > 0 ? T("Measured memory frontier: %1 (knowledge probe). Tier A = decisions from %2 on, the only ones that count; tier B = before, where the model may remember what followed.")
                            .arg(lastKnown, day(tierFrom).left(10))
                      : lastKnown == "inconclusive" ? T("The knowledge probe was inconclusive (the model gave no price): every decision is tier B. Probe its memory again.")
                      : T("No knowledge probe yet for this environment: every decision is tier B (the model may remember what followed). Probe its memory first.")) + "\n\n";
  auto table = [&](const ae::Tier& t) {
    QString o = "| " + T("Rule") + " | " + T("Decisions") + " | " + T("Total") + " | " + T("Per decision") + " | " + T("In the market") + " | " + T("Won") + " |\n|---|---|---|---|---|---|\n";
    auto row = [&](const QString& name, const ae::Line& l) {
      if (!l.n) return;
      o += "| " + name + " | " + QString::number(l.n) + " | " + pctText(l.total) + " | " + pctText(l.mean, 3) + " | " +
           (std::isfinite(l.exposure) ? QString::number(l.exposure * 100, 'f', 0) + " %" : QStringLiteral("—")) + " | " +
           (std::isfinite(l.hit) ? QString::number(l.hit * 100, 'f', 0) + " %" : QStringLiteral("—")) + " |\n";
    };
    row("**" + T("Agent") + "**", t.agent);
    row(T("Buy & hold"), t.buyHold);
    row(T("Trend (above its 20-hour average)"), t.trend);
    row(T("Flat"), t.flat);
    row(T("ML without LLM (%1)").arg(mlRun), t.ml);
    auto vs = [&](const QString& name, const ae::Versus& v) {
      if (v.n < 2) return QString();
      if (!std::isfinite(v.p))
        return "- " + T("Agent against %1: %2 per decision; too few decisions (%3) for a test.").arg(name, pctText(v.meanDiff, 3)).arg(v.n) + "\n";
      return "- " + T("Agent against %1: %2 per decision, p %3 → %4").arg(name, pctText(v.meanDiff, 3), pText(v.p < 0.05 || v.pWorse >= 0.05 ? v.p : v.pWorse),
                v.p < 0.05 ? T("better, not by chance") : v.pWorse < 0.05 ? T("worse, not by chance") : T("not distinguishable")) + "\n";
    };
    o += "\n" + vs(T("buy & hold"), t.vsBuyHold) + vs(T("the trend rule"), t.vsTrend) + vs(T("the ML model"), t.vsMl);
    return o;
  };
  auto section = [&](const ae::Summary& x) {
    QString o;
    if (frontier > 0) {
      o += "#### " + T("Tier A (after the frontier)") + "\n\n" + (x.a.agent.n ? table(x.a) : T("No decision after the frontier.") + "\n") + "\n";
      o += "#### " + T("Tier B (before: memory leakage)") + "\n\n" + (x.b.agent.n ? table(x.b) : T("No decision before the frontier.") + "\n") + "\n";
    } else {
      o += table(x.all) + "\n";
    }
    if (x.repeats > 1) {
      QStringList tot;
      for (double v : x.repeatTotals) tot << pctText(v);
      o += T("Repeats: %1 (spread %2). A gap between two agents smaller than this spread ranks nothing.").arg(tot.join(", "), pctText(x.repeatSd)) + "\n\n";
    }
    return o;
  };
  if (m_spec.mode == 0) md += section(s);
  else {
    // blind trials: the same moments with the dates, the name and the price level masked (ai/Blind.h)
    QList<ae::Decision> named, blindDec;
    QHash<QString, int> namedPos;
    for (int i = 0; i < m_decisions.size(); ++i) {
      const auto& d = m_decisions[i];
      (m_decBlind.value(i) ? blindDec : named) << d;
      if (!m_decBlind.value(i)) namedPos.insert(QString("%1|%2").arg(d.repeat).arg(d.t), d.pos);
    }
    const qint64 tf = tierFrom;
    auto sc = [&](const QList<ae::Decision>& ds) { return ae::score(ds, pricesOf(m_spec.market), H, m_spec.costBp / 1e4, tf); };
    if (!named.isEmpty()) md += "### " + T("Named (the market and the dates as they are)") + "\n\n" + section(sc(named));
    md += "### " + T("Blind (dates shifted, the market's name and price level masked)") + "\n\n" + section(sc(blindDec));
    if (!named.isEmpty()) {
      int pairs = 0, same = 0;
      for (const auto& d : blindDec)
        if (const auto it = namedPos.constFind(QString("%1|%2").arg(d.repeat).arg(d.t)); it != namedPos.cend()) { ++pairs; same += *it == d.pos; }
      md += T("Same decision named and blind at %1 of %2 moments. A model that does clearly better named than blind on tier B decisions is helped by its memory; on tier A, by recognising the market.")
                .arg(same).arg(pairs) + "\n\n";
    }
    md += T("Masking limits: a number of the price's order that is not a price (an open interest in coins) is scaled too; the asset's behaviour itself (its volatility) is not hidden.") + "\n\n";
  }
  if (mlRun.isEmpty())
    md += T("No ML run answers the same question yet: a supervised training of the direction of close, %1 row(s) ahead, on %2 1-hour bars (walk-forward over this period) would give the comparison without LLM.")
              .arg(m_spec.horizonHours).arg(shortName) + "\n\n";
  md += T("Every figure is computed by Deepwater from the stored bars; the model only gave LONG / SHORT / FLAT. A few dozen decisions are an anecdote, not a proof.");
  if (Engine* e = Engines::instance().at(env)) e->appendNote("report", md);

  // the evaluation's file and its try in the registry
  QJsonArray dec;
  for (int i = 0; i < m_decisions.size(); ++i) {
    const auto& d = m_decisions[i];
    dec.append(QJsonObject{{"t", double(d.t)}, {"repeat", d.repeat}, {"pos", d.pos}, {"parsed", d.parsed}, {"ml", d.mlKnown ? QJsonValue(d.ml) : QJsonValue()},
                           {"question_id", m_questionIds.value(i)}, {"blind", m_decBlind.value(i)}});
  }
  const QJsonObject spec{{"market", m_spec.market}, {"horizon_hours", m_spec.horizonHours}, {"from", double(m_spec.from)}, {"to", double(m_spec.to)},
                         {"decisions", m_spec.decisions}, {"repeats", m_spec.repeats}, {"cost_bp", m_spec.costBp},
                         {"mode", m_spec.mode == 0 ? "named" : m_spec.mode == 1 ? "blind" : "both"}};
  const QJsonObject out{{"model", model}, {"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"spec", spec},
                        {"prompt", prompt(m_spec.market, m_spec.horizonHours)}, {"frontier", double(frontier)}, {"tier_a_from", double(tierFrom)},
                        {"ml_run", mlRun}, {"stopped", stopped}, {"decisions", dec},
                        {"total", s.all.agent.total}, {"buy_hold", s.all.buyHold.total}, {"tier_a_total", s.a.agent.total}};
  QDir().mkpath(envFile(env, "evaluations"));
  QSaveFile f(envFile(env, "evaluations/" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss") + ".json"));
  if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(out).toJson()); f.commit(); }
  if (s.decisions > 0) {
    trials::Trial t;
    t.at = QDateTime::currentSecsSinceEpoch();
    t.kind = "agent";
    t.origin = "agent";
    t.who = QString("environment %1 · %2").arg(env + 1).arg(model);
    t.place = "evaluation protocol";
    t.family = QString("agent|%1|%2h").arg(m_spec.market).arg(m_spec.horizonHours);
    t.config = trials::fingerprint((model + "\n" + prompt(m_spec.market, m_spec.horizonHours) + "\n" + Slots::instance().skills(env).join(',')).toUtf8());
    t.label = model + " · " + T("evaluation protocol");
    t.market = m_spec.market;
    t.timeframe = QString("%1h").arg(m_spec.horizonHours);
    t.from = m_spec.from;
    t.to = m_spec.to;
    t.n = s.decisions;
    t.netPct = s.all.agent.total * 100;
    t.detail = spec;
    trials::Registry::instance().add(t);
  }
  emit progress();
  emit finished(env);
}


// ------------------------------------------------------------------------------------------------ forward test (tier C)
namespace {
QString forwardPath(int env) { return envFile(env, "forward-test.json"); }
QJsonObject readForward(int env) {
  QFile f(forwardPath(env));
  if (!paths::insideNoLink(Slots::instance().folder(env), f.fileName()) || !f.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(f.readAll()).object();
}
void writeForward(int env, const QJsonObject& o) {
  paths::dropLink(forwardPath(env));
  QSaveFile f(forwardPath(env));
  if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(o).toJson()); f.commit(); }
}
// what makes the agent the same agent: its model and version, its system prompt (without the date line), its skills
QString agentPrint(int env) {
  const Engine* e = Engines::instance().at(env);
  return Slots::instance().slot(env).displayName() + "|" + Slots::instance().skills(env).join(',') + "|" +
         QString(e ? e->device() : QString()).section(" · ", 0, 0);
}
}  // namespace

QString Evaluation::startForward(const EvalSpec& s, int stepHours) {
  if (s.env < 0 || s.env >= Slots::kCount || Slots::instance().slot(s.env).isEmpty()) return T("This environment holds no model.");
  if (!Slots::instance().hasSkill(s.env, "read_data")) return T("Tick \"Read Deepwater's data\" for this environment: the model decides from its tools.");
  if (stepHours < s.horizonHours) return T("The time between two decisions must be at least the horizon (%1 h): they would overlap.").arg(s.horizonHours);
  if (!readForward(s.env).isEmpty() && readForward(s.env).value("state").toString() == "running") return T("A forward test is already running in this environment.");
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  QJsonObject st{{"state", "running"}, {"market", s.market}, {"horizon_hours", s.horizonHours}, {"step_hours", stepHours},
                 {"decisions_wanted", s.decisions}, {"cost_bp", s.costBp}, {"created", double(now)}, {"next_at", double(now)},
                 {"model", Slots::instance().slot(s.env).displayName()}, {"agent", agentPrint(s.env)},
                 {"prompt", prompt(s.market, s.horizonHours)}, {"decisions", QJsonArray()}};
  writeForward(s.env, st);
  resumeForward();
  QTimer::singleShot(0, this, [this] { forwardTick(); });
  emit progress();
  return {};
}

void Evaluation::stopForward(int env) {
  QJsonObject st = readForward(env);
  if (st.isEmpty() || st.value("state").toString() != "running") return;
  st["state"] = "stopped";
  st["next_at"] = 0;
  writeForward(env, st);
  reportForward(env, st);   // what is scored so far
  emit progress();
}

QString Evaluation::forwardStatus(int env) const {
  const QJsonObject st = readForward(env);
  if (st.isEmpty()) return {};
  int asked = 0, scored = 0, missed = 0;
  for (const auto& v : st.value("decisions").toArray()) {
    const QJsonObject d = v.toObject();
    if (d.value("status").toString() == "missed") ++missed;
    else { ++asked; if (d.contains("ret")) ++scored; }
  }
  const QString state = st.value("state").toString();
  return T("Forward test (%1): %2 of %3 decisions asked, %4 scored, %5 missed.")
      .arg(state == "running" ? T("running") : state == "stopped" ? T("stopped") : T("finished"))
      .arg(asked).arg(st.value("decisions_wanted").toInt()).arg(scored).arg(missed);
}

void Evaluation::resumeForward() {
  static bool connected = false;   // a lambda cannot be a Qt::UniqueConnection (Qt asserts): connected once by hand
  if (!connected) {
    connected = true;
    m_forwardTimer.setInterval(60 * 1000);
    connect(&m_forwardTimer, &QTimer::timeout, this, [this] { forwardTick(); });
  }
  m_forwardTimer.start();
  QTimer::singleShot(30 * 1000, this, [this] { forwardTick(); });   // after the stores and the collection have started
}

void Evaluation::forwardTick() {
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  for (int env = 0; env < Slots::kCount; ++env) {
    QJsonObject st = readForward(env);
    if (st.isEmpty() || st.value("reported").toBool()) continue;
    const qint64 H = qint64(st.value("horizon_hours").toInt()) * 3600, step = qint64(st.value("step_hours").toInt()) * 3600;
    QJsonArray dec = st.value("decisions").toArray();
    bool changed = false;
    // score the decisions whose horizon has passed on the stored bars
    const ae::Prices pr = pricesOf(st.value("market").toString());
    for (int i = 0; i < dec.size(); ++i) {
      QJsonObject d = dec[i].toObject();
      if (d.value("status").toString() != "asked" || d.contains("ret")) continue;
      const qint64 t = qint64(d.value("t").toDouble());
      if (now < t + H + 3600) continue;
      const auto p0 = pr.closeAt(t), p1 = pr.closeAt(t + H);
      if (!p0 || !p1) { if (now > t + H + 3 * 86400) { d["status"] = "no price"; dec[i] = d; changed = true; } continue; }
      const double cost = st.value("cost_bp").toDouble(15) / 1e4, r = *p1 / *p0 - 1;
      const int pos = d.value("pos").toInt();
      const auto sma = pr.smaAt(t);
      d["ret"] = pos * r - (pos ? 2 * cost : 0.0);
      d["ret_buy_hold"] = r - 2 * cost;
      d["ret_trend"] = sma && *p0 > *sma ? r - 2 * cost : 0.0;
      dec[i] = d;
      changed = true;
    }
    // the next time to ask: missed when it went by (the app closed, the environment busy), never asked late
    if (st.value("state").toString() == "running") {
      qint64 next = qint64(st.value("next_at").toDouble());
      const int wanted = st.value("decisions_wanted").toInt();
      int used = st.value("used").toInt();   // scheduled times consumed: asked (the answer may still be coming) or missed
      while (used < wanted && next > 0 && now > next + std::min<qint64>(step / 2, 3600)) {
        dec.append(QJsonObject{{"t", double(next)}, {"status", "missed"}, {"why", "the app was closed"}});
        next += step;
        ++used;
        changed = true;
      }
      if (used >= wanted) { st["state"] = "done"; next = 0; changed = true; }
      st["next_at"] = double(next);
      st["used"] = used;
      if (next > 0 && now >= next && !m_running) {
        Engine* e = Engines::instance().at(env);
        const bool idle = e && e->state() != Engine::State::Busy && !e->pending();
        if (!idle || Slots::instance().asOf(env) > 0) {
          dec.append(QJsonObject{{"t", double(next)}, {"status", "missed"},
                                 {"why", Slots::instance().asOf(env) > 0 ? "the environment was in evaluation mode" : "the environment was busy"}});
        } else {
          // ask now: one decision, collected as the protocol's are
          m_running = true;
          m_probe = false;
          m_forwardEnv = env;
          m_spec = EvalSpec{};
          m_spec.env = env;
          m_current = qMakePair(0, now);
          watch();
          if (!e->ask(st.value("prompt").toString())) { m_running = false; m_forwardEnv = -1; }
          else { m_askIndex = int(e->messages().size()) - 1; m_waiting = true; m_timeout.start(kDecisionTimeoutMs); }
        }
        st["next_at"] = double(next + step);
        st["used"] = used + 1;
        if (used + 1 >= wanted) { st["state"] = "done"; st["next_at"] = 0; }
        changed = true;
      }
    }
    st["decisions"] = dec;
    if (changed) writeForward(env, st);
    // all asked (or the test stopped) and every asked decision scored: the report, once
    bool pending = false;
    for (const auto& v : dec) if (v.toObject().value("status").toString() == "asked" && !v.toObject().contains("ret")) pending = true;
    if (st.value("state").toString() != "running" && !pending && !(m_running && m_forwardEnv == env)) reportForward(env, st);
    if (changed) emit progress();
  }
}

void Evaluation::finishForwardDecision(const QString& answer) {
  const int env = m_forwardEnv;
  m_forwardEnv = -1;
  m_running = false;
  QObject::disconnect(m_conn);
  QJsonObject st = readForward(env);
  QJsonArray dec = st.value("decisions").toArray();
  const auto pos = ae::parseDecision(answer);
  const Engine* e = Engines::instance().at(env);
  // the moment of the decision: when it was asked; scored from the last bar closed then
  dec.append(QJsonObject{{"t", double(m_current.second)}, {"status", "asked"}, {"pos", pos.value_or(0)}, {"parsed", pos.has_value()},
                         {"question_id", e ? e->questionId() : QString()}, {"agent", agentPrint(env)}});
  st["decisions"] = dec;
  writeForward(env, st);
  emit progress();
}

void Evaluation::reportForward(int env, QJsonObject& st) {
  if (st.value("reported").toBool()) return;
  QVector<double> a, b, t;
  int missed = 0, unparsed = 0, changedAgent = 0, noPrice = 0;
  // the agent as it was at the first decision answered (its version is known only once it has answered)
  QString agent0;
  for (const auto& v : st.value("decisions").toArray())
    if (v.toObject().value("status").toString() == "asked") { agent0 = v.toObject().value("agent").toString(); break; }
  for (const auto& v : st.value("decisions").toArray()) {
    const QJsonObject d = v.toObject();
    const QString s = d.value("status").toString();
    if (s == "missed") { ++missed; continue; }
    if (s == "no price") { ++noPrice; continue; }
    if (!d.contains("ret")) continue;
    a << d.value("ret").toDouble(); b << d.value("ret_buy_hold").toDouble(); t << d.value("ret_trend").toDouble();
    if (!d.value("parsed").toBool()) ++unparsed;
    if (d.value("agent").toString() != agent0) ++changedAgent;
  }
  auto sum = [](const QVector<double>& x) { double s = 0; for (double v : x) s += v; return s; };
  const QString model = st.value("model").toString();
  QString md = "### " + T("Forward test (tier C) of %1 on %2, %3 hours ahead").arg(model, st.value("market").toString()).arg(st.value("horizon_hours").toInt()) + "\n\n";
  md += T("Asked in the present every %1 h from %2 UTC: no memory of the model can know what followed. %3 decisions scored, %4 missed, %5 without a price, %6 without a DECISION line.")
            .arg(st.value("step_hours").toInt()).arg(day(qint64(st.value("created").toDouble())))
            .arg(a.size()).arg(missed).arg(noPrice).arg(unparsed) + "\n\n";
  md += "| " + T("Rule") + " | " + T("Total") + " |\n|---|---|\n| **" + T("Agent") + "** | " + pctText(sum(a)) + " |\n| " + T("Buy & hold") + " | " +
        pctText(sum(b)) + " |\n| " + T("Trend (above its 20-hour average)") + " | " + pctText(sum(t)) + " |\n| " + T("Flat") + " | " + pctText(0) + " |\n\n";
  if (changedAgent) md += "**" + T("The agent changed during the test (model, version, prompt or skills) for %1 decision(s): it was not the same agent throughout.").arg(changedAgent) + "**\n\n";
  md += T("Every figure is computed by Deepwater from the stored bars; the model only gave LONG / SHORT / FLAT. A few dozen decisions are an anecdote, not a proof.");
  if (Engine* e = Engines::instance().at(env)) e->appendNote("report", md);
  st["reported"] = true;
  st["total"] = sum(a);
  st["buy_hold"] = sum(b);
  writeForward(env, st);
  if (!a.isEmpty()) {
    trials::Trial tr;
    tr.at = QDateTime::currentSecsSinceEpoch();
    tr.kind = "agent";
    tr.origin = "agent";
    tr.who = QString("environment %1 · %2").arg(env + 1).arg(model);
    tr.place = "forward test";
    tr.family = QString("agent|%1|%2h").arg(st.value("market").toString()).arg(st.value("horizon_hours").toInt());
    tr.config = trials::fingerprint((agent0 + "\n" + st.value("prompt").toString()).toUtf8());
    tr.label = model + " · " + T("forward test");
    tr.market = st.value("market").toString();
    tr.timeframe = QString("%1h").arg(st.value("horizon_hours").toInt());
    tr.from = qint64(st.value("created").toDouble());
    tr.to = QDateTime::currentSecsSinceEpoch();
    tr.n = int(a.size());
    tr.netPct = sum(a) * 100;
    trials::Registry::instance().add(tr);
  }
}

// ------------------------------------------------------------------------------------------------ knowledge probe
QString Evaluation::startProbe(int env) {
  if (m_running) return T("An evaluation is already running.");
  if (env < 0 || env >= Slots::kCount || Slots::instance().slot(env).isEmpty()) return T("This environment holds no model.");
  m_spec = EvalSpec{};
  m_spec.env = env;
  m_running = true;
  m_probe = true;
  m_total = 1;
  m_done = 0;
  m_prevAsOf = Slots::instance().asOf(env);
  m_prevBlind = Slots::instance().blindSeed(env);
  m_probeSkills = Slots::instance().skills(env);
  emit progress();
  // the truth first: Binance's monthly closes (the months over), then the question
  charts::data::getCandles("BTCUSDT", "1M", 40, this, [this, env](const charts::data::Result<charts::data::CandlesResult>& r) {
    if (!m_running || !m_probe) return;
    m_truth.clear();
    const QDate thisMonth = QDate::currentDate().addDays(1 - QDate::currentDate().day());
    if (r.ok())
      for (const auto& c : r.value.candles) {
        const QDate d = QDateTime::fromSecsSinceEpoch(c.time, QTimeZone::UTC).date();
        if (d.year() >= 2024 && d < thisMonth) m_truth.insert(d.toString("yyyy-MM"), c.close);
      }
    if (m_truth.size() < 6) {
      m_running = false;
      m_probe = false;
      if (Engine* e = Engines::instance().at(env))
        e->appendNote("report", "### " + T("Knowledge probe") + "\n\n" + T("Binance's monthly closes could not be read: no probe."));
      emit progress();
      emit finished(env);
      return;
    }
    QStringList months = m_truth.keys();
    std::sort(months.begin(), months.end());
    const QString q = "Knowledge probe, asked by the user to measure what your training data holds (no live data is involved "
                      "and you have no tool now). From your own memory, give Bitcoin's closing price in US dollars at the end of "
                      "each month below (BTC/USDT on Binance), one line per month in the form YYYY-MM: price. When you do not "
                      "remember a month, write YYYY-MM: unknown. Do not guess.\n" + months.join("\n");
    Engine* e = Engines::instance().at(env);
    watch();
    if (e->state() == Engine::State::Busy || e->pending()) e->cancel();
    // in the present, with no tool, in a fresh context: in evaluation mode a model may obey "do not use what you
    // remember" and refuse, which would measure its obedience, not its memory
    m_probeSkills = Slots::instance().skills(env);
    for (const auto& sk : skills()) Slots::instance().setSkill(env, sk.id, false);
    Slots::instance().setBlind(env, 0);
    Slots::instance().setAsOf(env, 0);
    e->freshContext();
    auto restore = [this, env] {
      for (const QString& sk : m_probeSkills) Slots::instance().setSkill(env, sk, true);
      Slots::instance().setAsOf(env, m_prevAsOf);
    };
    if (!e->ask(q)) { m_running = false; m_probe = false; restore(); emit progress(); emit finished(env); return; }
    m_askIndex = int(e->messages().size()) - 1;
    m_waiting = true;
    m_timeout.start(kDecisionTimeoutMs);
  });
  return {};
}

void Evaluation::finishProbe(const QString& answer) {
  const int env = m_spec.env;
  m_running = false;
  m_probe = false;
  QObject::disconnect(m_conn);
  for (const QString& sk : m_probeSkills) Slots::instance().setSkill(env, sk, true);   // its tools back
  Slots::instance().setBlind(env, m_prevBlind);
  Slots::instance().setAsOf(env, m_prevAsOf);
  const QHash<QString, double> said = ae::parseMonthly(answer);
  const ae::Probe p = ae::probe(said, m_truth, 10);
  const QString model = Slots::instance().slot(env).displayName();
  QJsonArray months;
  QString md = "### " + T("Knowledge probe of %1").arg(model) + "\n\n" +
               T("Bitcoin's monthly closes asked from memory, tools useless, against Binance's. Known = within 10 %.") + "\n\n| " +
               T("Month") + " | Binance | " + T("Said") + " | " + T("Gap") + " | |\n|---|---|---|---|---|\n";
  for (const auto& m : p.months) {
    months.append(QJsonObject{{"month", m.month}, {"truth", m.truth}, {"said", std::isfinite(m.said) ? QJsonValue(m.said) : QJsonValue()}, {"known", m.known}});
    md += "| " + m.month + " | " + QString::number(m.truth, 'f', 0) + " | " + (std::isfinite(m.said) ? QString::number(m.said, 'f', 0) : T("unknown")) + " | " +
          (std::isfinite(m.errPct) ? QString::number(m.errPct, 'f', 1) + " %" : QStringLiteral("—")) + " | " + (m.known ? "✓" : "") + " |\n";
  }
  md += "\n" + (p.frontier > 0 ? T("Last month it still knows: %1. Decisions from %2 on are tier A in its evaluations.")
                                     .arg(p.lastKnown, day(QDateTime::fromSecsSinceEpoch(p.frontier, QTimeZone::UTC).addMonths(1).toSecsSinceEpoch()).left(10))
                               : said.isEmpty() ? T("Inconclusive: it gave no price at all (a refusal looks the same as no memory). Every decision counts as tier B until a probe answers.")
                                                : T("It gave prices but none close enough: no month is known. Every decision counts as tier A."));
  if (Engine* e = Engines::instance().at(env)) e->appendNote("report", md);
  // none known although prices were given: the frontier is the start of the first month asked (every decision after it
  // is tier A); no price given at all: inconclusive (frontier 0: every decision tier B, closed by default)
  qint64 frontier = p.frontier;
  if (!frontier && !said.isEmpty() && !p.months.isEmpty())
    frontier = QDateTime(QDate::fromString(p.months.first().month + "-01", "yyyy-MM-dd"), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  QSaveFile f(envFile(env, "knowledge-probe.json"));
  if (f.open(QIODevice::WriteOnly)) {
    f.write(QJsonDocument(QJsonObject{{"model", model}, {"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                                      {"last_known", p.lastKnown.isEmpty() ? (said.isEmpty() ? QStringLiteral("inconclusive") : QStringLiteral("none")) : p.lastKnown},
                                      {"frontier", double(frontier)}, {"months", months}}).toJson());
    f.commit();
  }
  m_done = 1;
  emit progress();
  emit finished(env);
}

}  // namespace ai
