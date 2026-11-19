#include "ai/Tools.h"
#include <QCoreApplication>

namespace ai {

QList<Skill> skills() {
  return {
      {"read_data", QCoreApplication::translate("ai", "Read Deepwater's data"),
       QCoreApplication::translate("ai", "The model may read what Deepwater's pages show (markets, news, calendar, economy, "
                                         "positioning…), through tools checked at each call and written to the "
                                         "environment's journal. Never the Sources and AI pages, never keys or passwords.")},
      {"ml", QCoreApplication::translate("ai", "Machine learning"),
       QCoreApplication::translate("ai", "The model may use AI › ML: load data sets (prices, macro, COT), read them, train models of the "
                                         "five families in the same sandbox as the page (one at a time, named after this environment) and "
                                         "read their results. It cannot delete, stop or install anything, nor read files outside the data sets.")},
      {"backtest", QCoreApplication::translate("ai", "Backtest strategies"),
       QCoreApplication::translate("ai", "The model may backtest strategies with the Strategy tester (rule strategies, or Pine strategies it "
                                         "writes, run in PineTS's sandbox) and run the quant checks, on Data › Technical data' history or Charts' "
                                         "sources. Every figure comes from the engine; it saves nothing and places no order. Its tries are "
                                         "counted so the best one's Sharpe is deflated by them.")},
  };
}

Tools& Tools::instance() { static Tools* t = new Tools; return *t; }

Tools::Tools() { registerDataTools(*this); registerMlTools(*this); registerBacktestTools(*this); }
ToolCaller& currentCaller() { static ToolCaller c; return c; }

void Tools::add(const ToolDef& t) { m_tools << t; }

const ToolDef* Tools::find(const QString& name) const {
  for (const auto& t : m_tools) if (t.name == name) return &t;
  return nullptr;
}

QList<ToolDef> Tools::forSkills(const QStringList& ids) const {
  QList<ToolDef> out;
  for (const auto& t : m_tools) if (ids.contains(t.skill)) out << t;
  return out;
}
}  // namespace ai
