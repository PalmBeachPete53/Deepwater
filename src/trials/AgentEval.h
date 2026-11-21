#pragma once
// The agents' evaluation protocol (step 6 of the roadmap in reports/Architecture agents ML évaluation.md, 2026-11-20):
// the figures, computed by the app, never by a model. Pure QtCore: tested in tests/trials.
//
//  - parseDecision: the agent's answer → LONG (+1), SHORT (−1) or FLAT (0): its last "DECISION: …" line only.
//  - score: each decision held from the evaluation moment D (the close of the last bar closed by D) to D + horizon
//    (the close of the bar closing then), paying cost a side on entry and exit; the same moments for the references:
//    buy & hold, flat, trend (long above the 20-bar average at D), and an ML model without LLM when one is given.
//  - the knowledge probe: the model's remembered monthly closes against the real ones → the last month it still
//    knows (its measured frontier; publishers' announced cut-offs differ by up to 8 months, arXiv 2607.18867). Decisions
//    after frontier + margin are tier A (the only ones that count), before it tier B (they measure memory leakage).
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cmath>
#include <functional>
#include <optional>

namespace trials::agenteval {

struct Decision {
  qint64 t = 0;          // the evaluation moment (Unix s)
  int repeat = 0;        // 0 … repeats − 1
  int pos = 0;           // +1 long, −1 short, 0 flat
  bool parsed = false;   // false: no DECISION line (counted as flat)
  int ml = 0;            // the ML model's position at t (0 when none); mlKnown: it had a prediction there
  bool mlKnown = false;
};

std::optional<int> parseDecision(const QString& answer);   // nullopt when no DECISION: LONG|SHORT|FLAT line

// Prices: the close of the last bar that had closed by t (nullopt when none), and the 20-bar average there.
struct Prices {
  std::function<std::optional<double>(qint64 t)> closeAt;
  std::function<std::optional<double>(qint64 t)> smaAt;
};

struct Line {            // one rule over the decisions
  int n = 0;
  double total = 0;      // sum of the per-decision returns (each decision is its own trade)
  double mean = NAN, sd = NAN, hit = NAN;   // hit: share of the non-flat decisions that made money
  double exposure = NAN; // share of the decisions in the market
};
// paired bootstrap of agent − rule; no p (NaN) under kMinTested decisions: resampling a handful of values means nothing
constexpr int kMinTested = 6;
struct Versus { double meanDiff = NAN; double p = NAN, pWorse = NAN; int n = 0; };
struct Tier {
  QString name;          // "A", "B"
  Line agent, buyHold, flat, trend, ml;
  Versus vsBuyHold, vsTrend, vsMl;
  int unparsed = 0;
};
struct Summary {
  int decisions = 0, repeats = 0, skipped = 0;   // skipped: no price at t or t + horizon
  Tier all, a, b;
  QVector<double> repeatTotals;                  // the agent's total per repeat (its dispersion)
  double repeatSd = NAN;
};

// returns per decision (agent and rules) at cost a side; frontier: decisions with t >= frontier are tier A (0 = none)
Summary score(const QList<Decision>& decisions, const Prices& prices, qint64 horizonSec, double costFraction, qint64 frontier);

// ---- knowledge probe
QHash<QString, double> parseMonthly(const QString& answer);   // "2025-03: 82350" lines → month → value ("unknown" left out)
struct ProbeMonth { QString month; double truth = NAN, said = NAN, errPct = NAN; bool known = false; };
// lastKnown: the last month known (within tolerance) with at least 2 of the 3 months ending there known (a constant
// guess matches a few months by luck); frontier: the end of that month (Unix s)
struct Probe { QList<ProbeMonth> months; QString lastKnown; qint64 frontier = 0; };
Probe probe(const QHash<QString, double>& said, const QHash<QString, double>& truth, double tolerancePct = 10);

}  // namespace trials::agenteval
