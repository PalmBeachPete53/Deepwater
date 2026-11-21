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

// The evaluation clock (step 3 of the roadmap in reports/Architecture agents ML évaluation.md, 2026-11-20): an
// environment in evaluation mode (ai::Slots::asOf) is told a past date and its tools answer as of that moment. The time
// is imposed here, in the app, never trusted to the model or its prompt. Each tool declares how it stands to the clock:
//   Live      (the default) it reads the present (quotes, news, the live book, a page): REFUSED in evaluation mode. A
//             new tool is therefore refused until someone makes it honour the clock: closed by default.
//   AsOf      it honours ToolCaller::asOf: nothing it reads or shows is later than that moment (a bar counts once it has
//             closed by then), and a time asked after it is refused.
//   Timeless  it holds no market data (the list of indicators, of ML methods, the trainer's state).
// The declarations are in one place, Tools::Tools(); tools/check_asof_leaks.py checks the AsOf ones against a future
// made up at random (the answers must not change) and against a leak planted on purpose (it must be caught).
enum class Clock { Live, AsOf, Timeless };

struct ToolDef {
  QString name;          // read_page
  QString skill;         // read_data
  QString description;   // for the model (English)
  QJsonObject parameters;   // JSON schema of the arguments
  std::function<void(const QJsonObject& args, ToolDone done)> run;   // GUI thread; may answer later (data loading)
  // How long ai::Engine and ai::McpServer wait for the answer before giving the model an error. A backtest can run for
  // minutes: with the 30 s of a data read its answer was lost and the next try was refused while it still ran.
  int timeoutMs = 30000;
  Clock clock = Clock::Live;   // set in Tools::Tools()
  // Third parties' text in the result (news, wire, banks' research): it may carry instructions written for a model. The
  // dispatchers wrap it (ai::wrapExternal) and the system prompts say it is data, never instructions (step 7, 2026-11-20).
  // Deepwater's tools that act on anything are few and checked mechanically (no AI reaches the broker), so such text can
  // mislead an answer but not choose an action.
  bool external = false;
};

class Tools {
public:
  static Tools& instance();
  void add(const ToolDef& t);
  const ToolDef* find(const QString& name) const;
  QList<ToolDef> forSkills(const QStringList& skillIds) const;
private:
  Tools();
  void setClock(Clock c, const QStringList& names);
  QList<ToolDef> m_tools;
};

void registerDataTools(Tools& t);   // skill read_data (src/ai/DataTools.cpp)
void registerMlTools(Tools& t);     // skill ml (src/ai/MlTools.cpp)
void registerBacktestTools(Tools& t);   // skill backtest (src/ai/BacktestTools.cpp)
void registerOrderFlowTools(Tools& t);  // skill read_data, order flow (src/ai/OrderFlowTools.cpp)

// Who calls a tool: set by ai::Engine and ai::McpServer just before ToolDef::run (GUI thread), read by tools that write
// something on the user's side (an ML training is named after its environment).
struct ToolCaller {
  int env = -1;
  QString model;
  qint64 asOf = 0;   // Unix s: the environment's evaluation moment; 0 = the present (no evaluation)
};
ToolCaller& currentCaller();
// The caller's evaluation moment for tool `name` (0 = none). Tools read it through here, at the start of their run.
// Test hook (scratch profiles only, DW_PROFILE set): DW_PLANT_LEAK=<tool> makes that tool ignore the clock, so that
// tools/check_asof_leaks.py proves it catches a leak.
qint64 asOfFor(const QString& name);
// Why a call of `t` is refused at evaluation moment `asOf` (empty = it may run).
QString clockGate(const ToolDef& t, qint64 asOf);
// The result as the model gets it: an external tool's text between <external-text> marks (any such mark inside it
// defused), then a reminder that it is data.
QString wrapExternal(const ToolDef& t, const QString& text);
extern const char* const kExternalRule;   // the system prompts' rule about those marks
QString asOfStamp(qint64 asOf);   // "2026-11-10 14:00 UTC"
// The blind mask of an environment now (ai/Blind.h): on when it is in evaluation mode and blind; its reference price is
// the market's close at the moment (Data › Technical data, 1-hour bars).
namespace blind { struct Mask; }
blind::Mask maskFor(int env);
// "refused: evaluation mode …" for a time asked after the moment
QString afterClock(qint64 asOf, const QString& what);
}  // namespace ai
