#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QList>
#include <functional>

// Skills and the tools they grant to the models of AI › Agents.
//
// A skill is a tick box on a cube (ai::Slots::setSkill); each tool belongs to one skill. A model sees in its system prompt
// only the tools of the skills its environment grants, and ai::Engine checks the skill again when the call arrives
// (a box unticked meanwhile → refused). Every call is written to <env>/tool-journal.jsonl.
//
// The user's rule: data reaches a model only through a skill's tool, never pasted into the prompt or the question.
namespace ai {

struct Skill {
  QString id;
  QString label;         // translated
  QString description;   // translated, for the tooltip
};
QList<Skill> skills();

struct ToolResult {
  bool ok = true;
  QString text;          // what the model reads (plain text / small tables / JSON)
};
using ToolDone = std::function<void(const ToolResult&)>;

struct ToolDef {
  QString name;          // read_page
  QString skill;         // read_data
  QString description;   // for the model (English)
  QJsonObject parameters;   // JSON schema of the arguments
  std::function<void(const QJsonObject& args, ToolDone done)> run;   // GUI thread; may answer later (data loading)
  // How long ai::Engine and ai::McpServer wait for the answer before giving the model an error. A backtest can run for
  // minutes: with the 30 s of a data read its answer was lost and the next try was refused while it still ran.
  int timeoutMs = 30000;
};

class Tools {
public:
  static Tools& instance();
  void add(const ToolDef& t);
  const ToolDef* find(const QString& name) const;
  QList<ToolDef> forSkills(const QStringList& skillIds) const;
private:
  Tools();
  QList<ToolDef> m_tools;
};

void registerDataTools(Tools& t);   // skill read_data (src/ai/DataTools.cpp)
void registerMlTools(Tools& t);     // skill ml (src/ai/MlTools.cpp)
void registerBacktestTools(Tools& t);   // skill backtest (src/ai/BacktestTools.cpp)

// Who calls a tool: set by ai::Engine and ai::McpServer just before ToolDef::run (GUI thread), read by tools that write
// something on the user's side (an ML training is named after its environment).
struct ToolCaller { int env = -1; QString model; };
ToolCaller& currentCaller();
}  // namespace ai
