// The agents' evaluation protocol (src/trials/AgentEval): answers read mechanically, decisions scored by the app.
#include "Check.h"
#include "trials/AgentEval.h"

using namespace trials::agenteval;

TEST("agent eval: the last DECISION line counts, whatever its markdown") {
  CHECK(parseDecision("I think it goes up.\nDECISION: LONG") == 1);
  CHECK(parseDecision("End with DECISION: LONG, SHORT or FLAT … weak signal.\n**DECISION: FLAT**") == 0);
  CHECK(parseDecision("decision : short") == -1);
  CHECK(!parseDecision("I would buy here.").has_value());
  CHECK(!parseDecision("DECISION: MAYBE").has_value());
}

TEST("agent eval: each decision held to the horizon after costs, against the references, by tier") {
  // a price that rises 1 % an hour from 100, its 20-bar average below it
  Prices pr;
  pr.closeAt = [](qint64 t) -> std::optional<double> { return t < 0 ? std::nullopt : std::optional<double>(100 * std::pow(1.01, t / 3600)); };
  pr.smaAt = [](qint64 t) -> std::optional<double> { return 100 * std::pow(1.01, t / 3600) * 0.9; };
  const qint64 H = 4 * 3600;
  const double r = std::pow(1.01, 4) - 1, cost = 0.001;
  QList<Decision> d{{10 * 3600, 0, 1, true, 0, false}, {20 * 3600, 0, -1, true, 1, true}, {30 * 3600, 0, 0, false, 0, false},
                    {40 * 3600, 1, 1, true, 1, true}};
  const Summary s = score(d, pr, H, cost, 25 * 3600);   // tier A from hour 25
  CHECK_EQ(s.decisions, 4);
  CHECK_EQ(s.all.agent.n, 4);
  CHECK_NEAR(s.all.agent.total, (r - 2 * cost) + (-r - 2 * cost) + 0 + (r - 2 * cost), 1e-12);
  CHECK_NEAR(s.all.buyHold.total, 4 * (r - 2 * cost), 1e-12);
  CHECK_NEAR(s.all.trend.total, 4 * (r - 2 * cost), 1e-12);   // above its average: long each time
  CHECK_NEAR(s.all.flat.total, 0, 1e-12);
  CHECK_EQ(s.all.ml.n, 2);                                     // the ML model is known at two moments only
  CHECK_NEAR(s.all.ml.total, 2 * (r - 2 * cost), 1e-12);
  CHECK_EQ(s.all.unparsed, 1);
  CHECK_NEAR(s.all.agent.exposure, 0.75, 1e-12);
  CHECK_NEAR(s.all.agent.hit, 2.0 / 3.0, 1e-12);
  CHECK_EQ(s.a.agent.n, 2);
  CHECK_EQ(s.b.agent.n, 2);
  CHECK_EQ(s.repeats, 2);
  CHECK_NEAR(s.repeatTotals[1], r - 2 * cost, 1e-12);
  CHECK(s.all.vsBuyHold.meanDiff < 0);
  CHECK(std::isnan(s.all.vsBuyHold.p));   // 4 decisions: too few for a test
  // no price at the horizon: left out
  QList<Decision> late{{-3600, 0, 1, true, 0, false}};
  CHECK_EQ(score(late, pr, H, cost, 0).skipped, 1);
}

TEST("agent eval: the knowledge probe finds the last month the model still knows") {
  const QHash<QString, double> claude = parseMonthly(QString::fromUtf8("2024-01: ~42 580\n2025-10: ~109\u202f600 (souvenir peu sûr)\n2025-11: ≈ $90,400\n2025-12: unknown"));
  CHECK_EQ(claude.size(), 3);
  CHECK_NEAR(claude.value("2024-01"), 42580, 1e-9);
  CHECK_NEAR(claude.value("2025-10"), 109600, 1e-9);
  CHECK_NEAR(claude.value("2025-11"), 90400, 1e-9);
  const QHash<QString, double> said = parseMonthly("2025-11: 91,250\n2025-12: $87 500\n2026-01: 95000\n2026-02: unknown\n2026-03: 60000");
  CHECK_EQ(said.size(), 4);
  CHECK_NEAR(said.value("2025-12"), 87500, 1e-9);
  const QHash<QString, double> truth{{"2025-11", 90000}, {"2025-12", 88000}, {"2026-01", 94000}, {"2026-02", 80000}, {"2026-03", 85000}};
  const Probe p = probe(said, truth, 10);
  CHECK_EQ(p.months.size(), 5);
  CHECK_EQ(p.lastKnown, QString("2026-01"));
  CHECK(!p.months[3].known);   // not said
  CHECK(!p.months[4].known);   // said, 29 % off
  CHECK_EQ(p.frontier, qint64(1769904000));   // 2026-02-01 00:00 UTC
  // one month right by luck among wrong ones is no memory
  const QHash<QString, double> guess{{"2025-11", 50000}, {"2025-12", 87000}, {"2026-01", 50000}};
  CHECK(probe(guess, truth, 10).lastKnown.isEmpty());
}
