// Quant checks of a Pine strategy. See PineQuant.h.
#include "charts/core/PineQuant.h"
#include "charts/core/JsStack.h"
#include "charts/core/PineStrategy.h"
#include "charts/core/Series.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <algorithm>
#include <cmath>

namespace charts::pine {

namespace {
constexpr double TICK = 0.01;   // PineTS's tick when the data says none (syminfo.mintick ?? 0.01)

struct Tun { QString title; double base; };

// the first two whole-number inputs (defaults of 2 or more, as the rule strategies' lengths), with the base run's values
QVector<Tun> tunables(const Script& sc, const QJsonObject& given) {
  QVector<Tun> out;
  for (const QJsonValue& v : sc.inputs) {
    const QJsonObject m = v.toObject();
    if (m.value("type").toString() != "int") continue;
    const QString title = m.value("title").toString().isEmpty() ? m.value("name").toString() : m.value("title").toString();
    const double base = given.contains(title) ? given.value(title).toDouble() : m.value("defval").toDouble();
    if (title.isEmpty() || !(base >= 2)) continue;
    out.push_back({title, base});
    if (out.size() == 2) break;
  }
  return out;
}

double median(QVector<double> v) {
  if (v.isEmpty()) return NaN;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}
}  // namespace

QuantResult quantChecks(const QString& source, const Bars& bars, const Options& optionsIn, int mcRuns, const std::atomic<bool>* cancel) {
  QuantResult out;
  QElapsedTimer clock;
  clock.start();
  // every run reads the flag through its Options (non-owning: the flag outlives this blocking call)
  Options options = optionsIn;
  if (cancel) options.cancel = std::shared_ptr<const std::atomic<bool>>(std::shared_ptr<void>(), cancel);
  const auto stopped = [&] { return options.cancel && options.cancel->load(std::memory_order_relaxed); };
  const auto cancelledResult = [&] {
    QuantResult c;
    c.cancelled = true;
    c.error = QCoreApplication::translate("charts", "The checks were stopped.");
    c.seconds = clock.elapsed() / 1000.0;
    return c;
  };
  if (!isStrategySource(source)) {
    out.error = QCoreApplication::translate("charts", "This script is not a strategy(): it places no orders. Add it to the chart as an indicator.");
    return out;
  }
  Script sc;
  js::withJsStack([&] { sc = describe(source); });   // callers run this on pool threads of 512 KB (audit D19)
  if (!sc.ok()) { out.error = sc.error; return out; }
  const QVector<Tun> tun = tunables(sc, options.inputs);

  // the grid of settings, as the rule strategies' (×0.6 … ×1.4, rounded, at least 2, no repeats)
  static const double MULT[] = {0.6, 0.8, 1, 1.2, 1.4};
  quant::RunSet runs;
  QVector<QVector<double>> axes;
  for (const Tun& t : tun) {
    QVector<double> ax;
    for (double m : MULT) {
      const double v = std::max(2.0, std::floor(t.base * m + 0.5));
      if (!ax.contains(v)) ax.push_back(v);
    }
    axes.push_back(ax);
    runs.tunables.push_back({t.title, t.base, ax});
  }
  QVector<QVector<double>> grid;
  if (tun.isEmpty()) grid = {{}};
  else if (tun.size() == 1) for (double v : axes[0]) grid.push_back({v});
  else for (double a : axes[0]) for (double b : axes[1]) grid.push_back({a, b});

  // every run: index 0 = the base, then the grid's other settings
  struct Job { QJsonObject inputs; QJsonObject props; bool isBase = false; QVector<double> values; StrategyResult result; };
  QVector<Job> jobs;
  jobs.push_back({options.inputs, options.props, true, {}, {}});
  for (const QVector<double>& vals : grid) {
    bool isBase = true;
    QJsonObject in = options.inputs;
    for (int k = 0; k < tun.size(); ++k) {
      in.insert(tun[k].title, vals[k]);
      if (vals[k] != tun[k].base) isBase = false;
    }
    if (isBase) { jobs[0].values = vals; continue; }
    jobs.push_back({in, options.props, false, vals, {}});
  }
  auto runAll = [&](int from) {
    QThreadPool pool;
    pool.setStackSize(16u << 20);
    pool.setMaxThreadCount(std::max(2, QThread::idealThreadCount()));
    for (int i = from; i < jobs.size(); ++i) {
      Job* j = &jobs[i];
      pool.start(QRunnable::create([j, &source, &bars, &options, &stopped] {
        if (stopped()) return;   // superseded: the runs not started yet are skipped
        Options o = options;
        o.inputs = j->inputs;
        o.props = j->props;
        j->result = runStrategy(source, bars, o);
      }));
    }
    pool.waitForDone();
  };
  runAll(0);
  if (stopped()) return cancelledResult();
  if (!jobs[0].result.ok()) { out.error = jobs[0].result.error; return out; }
  runs.base = jobs[0].result.result;
  for (const QVector<double>& vals : grid) {
    quant::RunSet::One one;
    one.values = vals;
    for (const Job& j : jobs)
      if (j.values == vals) {
        one.isBase = j.isBase;
        if (!j.isBase) {
          if (!j.result.ok()) { out.error = j.result.error; return out; }
          one.result = j.result.result;
        }
        break;
      }
    runs.variants.push_back(one);
  }

  // cost stress: the script's costs raised, the base settings run again
  const QJsonObject cfg = jobs[0].result.config;
  const double commission = cfg.value("commission_value").toDouble(0), slippage = cfg.value("slippage").toDouble(0);
  QVector<double> closes;
  for (const Bar& b : bars) closes << b.close;
  const double med = median(closes);
  const double ticks = std::max(slippage * 3, std::isfinite(med) ? std::round(med * 0.0005 / TICK) : 0.0);
  const double slipPct = std::isfinite(med) && med > 0 ? ticks * TICK / med * 100 : NaN;
  Job stress{options.inputs, options.props, false, {}, {}};
  stress.props.insert("commission_value", commission * 2);
  stress.props.insert("slippage", ticks);
  jobs.push_back(stress);
  runAll(int(jobs.size()) - 1);
  if (stopped()) return cancelledResult();
  if (!jobs.last().result.ok()) { out.error = jobs.last().result.error; return out; }
  runs.stressed = jobs.last().result.result;
  const QString type = cfg.value("commission_type").toString();
  runs.costText = [commission, type, ticks, slipPct](double np) {
    const QString npText = std::isnan(np) ? QStringLiteral("n/a") : (np >= 0 ? QStringLiteral("+") : QString()) + series::numStr(np);
    const QString fee = commission == 0 ? QCoreApplication::translate("charts", "no commission to double")
                       : type == "percent" ? QCoreApplication::translate("charts", "%1% commission").arg(series::numStr(commission * 2))
                                           : QCoreApplication::translate("charts", "%1 commission").arg(series::numStr(commission * 2));
    return QCoreApplication::translate("charts", "%1% with %2 and %3 ticks of slippage, %4% a side (needs above 0)")
        .arg(npText, fee, series::numStr(ticks), series::numStr(series::toFixed(slipPct, 3).toDouble()));
  };
  // Monte Carlo: a trade's % is on the money it put in; the account moves by the share the script put in (its own
  // sizing: all of it with percent_of_equity 100, a little with a fixed quantity), measured on the base run's trades
  {
    QHash<qint64, double> equityAt;
    for (const auto& e : runs.base.equity) equityAt.insert(e.time, e.value);
    double sum = 0;
    int n = 0;
    for (const auto& t : runs.base.trades) {
      const double eq = equityAt.value(t.entryTime, NaN);
      if (std::isfinite(eq) && eq > 0) { sum += t.entryPrice * t.qty / eq * 100; ++n; }
    }
    runs.sizePct = n ? sum / n : 100;
  }

  out.checks = quant::checksFrom(bars, runs, mcRuns);
  out.runs = int(jobs.size());
  out.seconds = clock.elapsed() / 1000.0;
  return out;
}

}  // namespace charts::pine
