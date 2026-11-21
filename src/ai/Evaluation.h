#pragma once
// The agents' evaluation protocol (step 6 of the roadmap in reports/Architecture agents ML évaluation.md, 2026-11-20).
// It runs an environment's model through past decision moments with the evaluation clock (ai::Slots::asOf), reads its
// decisions mechanically and scores them with the app's own figures (trials/AgentEval.h):
//
//  KNOWLEDGE PROBE   in a fresh context, in the present and with no tool at all (its skills off for the probe: in
//                    evaluation mode a model may obey "do not use what you remember" and refuse), the model is asked
//                    Bitcoin's monthly closes from memory; nothing given = inconclusive (every decision tier B); compared with Binance's, the last month it still knows is its measured
//                    frontier, saved in <env>/knowledge-probe.json. A date in the prompt does not erase what the weights
//                    remember (arXiv 2504.14765, 2611.06808): only decisions after that frontier can count.
//  PROTOCOL          N moments spread over a period (at least one horizon apart, so the decisions do not overlap), R
//                    repeats; at each moment the environment's clock is set there (a fresh context, ai::Engine::
//                    syncClock), the model is asked LONG / SHORT / FLAT for the next H hours with its tools, and its last
//                    "DECISION:" line is taken (none: FLAT, counted as unparsed). Scored on Data › Technical data's
//                    1-hour bars after costs, against buy & hold, flat, the trend rule and, when an ML run answers the
//                    same question (direction, H hours, the same market on 1-hour bars), the same model without LLM.
//                    Reported apart: tier A (after the frontier + one month), tier B (before: memory leakage).
//  FORWARD (tier C)  the same question asked in the present at scheduled times (every `step` hours, N times) while the
//                    app runs: no memory can know what follows. Each decision is scored once its horizon has passed on
//                    the stored 1-hour bars; the report comes when the last one is scored. A time missed (the app
//                    closed, the environment busy or in evaluation mode) is recorded as missed, never asked late. The
//                    agent is meant to stay frozen: a change of model, version, system prompt or skills is reported.
//                    <env>/forward-test.json.
// Results: <env>/evaluations/<stamp>.json, a report in the environment's conversation, a try in the trial registry.
// One evaluation at a time; the environment's own clock is put back when it ends or is stopped.
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include "trials/AgentEval.h"

namespace ai {

struct EvalSpec {
  int env = 0;
  QString market = "BINANCE:BTCUSDT";
  int horizonHours = 4;
  qint64 from = 0, to = 0;     // the period of the decision moments (Unix s, UTC)
  int decisions = 12;
  int repeats = 1;
  double costBp = 15;          // a side (Binance: 0.10 % fee + 0.05 % slippage, as the Strategy tester)
  int mode = 0;                // 0 named, 1 blind (ai/Blind.h: dates, name and price level masked), 2 both at each moment
};

class Evaluation : public QObject {
  Q_OBJECT
public:
  static Evaluation& instance();
  bool running() const { return m_running; }
  int environment() const { return m_spec.env; }
  int done() const { return m_done; }
  int total() const { return m_total; }
  bool probing() const { return m_probe; }
  // Empty when started, else why not (translated).
  QString startProtocol(const EvalSpec& spec);
  QString startProbe(int env);
  void stop();
  // FORWARD: asks now, then every stepHours, `decisions` times in all (spec.from/to unused)
  QString startForward(const EvalSpec& spec, int stepHours);
  void stopForward(int env);
  void resumeForward();                 // at launch: the scheduler (every minute); missed times are recorded
  QString forwardStatus(int env) const; // "" when none (translated)
  static QString prompt(const QString& market, int horizonHours);   // the question asked at each moment (English)
  static qint64 frontierOf(int env, QString* lastKnown = nullptr);  // from the saved probe; 0 when none

signals:
  void progress();             // a decision was read, or it started / ended
  void finished(int env);

private:
  Evaluation() = default;
  void askNext();
  void collect();
  void finishProtocol(const QString& stopped = {});
  void finishProbe(const QString& answer);
  void watch();
  QString answerSince(int index) const;
  EvalSpec m_spec;
  bool m_running = false, m_probe = false, m_waiting = false;
  qint64 m_prevAsOf = 0;
  QList<QPair<int, qint64>> m_queue;   // repeat, moment
  QPair<int, qint64> m_current;
  int m_askIndex = -1, m_done = 0, m_total = 0;
  QList<trials::agenteval::Decision> m_decisions;
  QStringList m_questionIds;           // per decision: its question in the environment's journal (tool-journal.jsonl)
  QList<bool> m_queueBlind, m_decBlind;   // blind trials: per queued item, per decision
  bool m_currentBlind = false;
  quint32 m_blindSeed = 0, m_prevBlind = 0;
  int m_idleTries = 0;                 // seconds waited for the environment to be free before the next question
  QStringList m_probeSkills;           // the probe runs without tools: the skills put back after it
  QHash<QString, double> m_truth;      // probe: month → close
  QTimer m_timeout, m_settle, m_forwardTimer;
  int m_forwardEnv = -1;                // a forward decision is being asked in this environment
  void forwardTick();
  void finishForwardDecision(const QString& answer);
  void reportForward(int env, QJsonObject& st);
  QMetaObject::Connection m_conn;
};

}  // namespace ai
