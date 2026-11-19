#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

// Claude Code (Anthropic's command-line agent) as an integration of AI › LLM › Integration, loadable into an
// AI › Agents environment like a local model (slot repo "integration", file "claude-code").
//
// Deepwater never holds a key for it: the `claude` program installed on this computer runs with the user's own
// sign-in (claude.ai account or its own API key). In a cube, ai::Engine runs one `claude -p` per question with every
// built-in tool switched off (no shell, no files, no web) and only Deepwater's MCP server (ai::McpServer), which offers
// the tools of the skills the cube grants. What it reads is sent to Anthropic: the cube and the Integration page say so.
namespace ai::claudecode {

constexpr const char* kRepo = "integration";
constexpr const char* kFile = "claude-code";
bool isKey(const QString& repo, const QString& file);

struct Status {
  bool probed = false;          // a check has finished at least once
  bool checking = false;
  QString path;                 // empty: not found
  QString version;              // "2.1.325"
  bool loggedIn = false;
  QString authMethod;           // "claude.ai", "api key", … (never the account's e-mail)
  QString error;
};

class Probe : public QObject {
  Q_OBJECT
public:
  static Probe& instance();
  const Status& status() const { return m_status; }
  void refresh();               // runs `claude --version` and `claude auth status` (a few hundred ms)
signals:
  void changed();
private:
  Probe() = default;
  Status m_status;
  int m_run = 0;
};

QString binary();                       // the `claude` program, searched where its installers put it; empty if none
QProcessEnvironment environment();      // PATH completed for a GUI app (Finder starts apps with a bare PATH)

// Settings of AI › LLM › Integration (ai.claude.model / ai.claude.effort); empty = Claude Code's own default.
QStringList modelAliases();             // "", "opus", "sonnet", "fable", "haiku"
QStringList effortLevels();             // "", "low", "medium", "high", "xhigh", "max"
QString model();
QString effort();
void setModel(const QString& alias);
void setEffort(const QString& level);
}  // namespace ai::claudecode
