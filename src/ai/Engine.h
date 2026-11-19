#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QJsonObject>
#include <QProcess>
#include "ai/Tools.h"

// Runs the model of one AI › Agents environment: one deepwater-llm process per lit cube (helpers/llm), started inside
// a macOS sandbox (sandbox-exec) that allows only: reading the app bundle and this model file, reading/writing the
// environment's own folder, the GPU. No network, no other file, no other process, nothing of another environment.
//
// The conversation belongs to the environment: kept in <data>/ai/env-N/conversation.json, sent whole at each turn
// (the helper reuses its cache for the part it has already read). The model is told it is offline and sees no data.
//
// Skills: the system prompt lists the tools of the skills the environment grants (ai::Tools), in the <tool_call> format
// most open models are trained on (Hermes / Qwen). When an answer holds tool calls, the app checks the skill again, runs
// them, records them (role "tool", <env>/tool-journal.jsonl) and asks the model to go on, at most kMaxRounds times.
//
// Claude Code in a cube (ai::claudecode): no resident process. Each question runs `claude -p` in the environment's folder,
// stream-json out, with every built-in tool off (--tools ""), no settings, hooks, plugins or MCP servers of the user's own
// (--setting-sources "", --strict-mcp-config) except Deepwater's (ai::McpServer: the tools of the cube's skills, checked
// again at each call, recorded through recordTool). The conversation goes on with --resume (<env>/claude-session.json).
//
// Start policy: dropping a model on a cube starts its engine (the model loads in memory); at app start the cubes are
// restored but their engines start only when needed (first message, or opening the chat). EJECT stops the process.
namespace ai {

struct ChatMessage {
  QString role;       // "user" | "assistant" | "tool" (a tool call made by the app for the model, and its result)
  QString text;
  QString model;      // assistant: file name of the model that answered
  QDateTime at;
  QJsonObject stats;  // assistant: the helper's done event (tokens, ms, reason)
  QJsonObject call;   // tool: {"name", "arguments"}
  bool ok = true;     // tool: false when refused or failed
};

class Engine : public QObject {
  Q_OBJECT
public:
  enum class State { Off, Starting, Ready, Busy, Failed };
  Engine(int index, QObject* parent);
  ~Engine() override;

  State state() const { return m_state; }
  QString error() const { return m_error; }
  QString device() const { return m_device; }      // "MTL0 · Apple M3 Pro" / "CPU"
  int context() const { return m_ctx; }            // tokens
  QString modelFile() const { return m_modelFile; }
  double lastTokensPerSecond() const { return m_tps; }
  const QVector<ChatMessage>& messages() const { return m_messages; }

  void start();                       // for the environment's current model; no-op when already running it
  void stop(bool wait = false);       // ends the process (EJECT, model change); wait: block until it is gone (app quit)
  bool ask(const QString& text);      // adds the user's message and asks for an answer (starts the engine if needed)
  void cancel();                      // ends the answer being written
  void clear();                       // empties the conversation
  bool isClaude() const;              // the environment holds the Claude Code integration
  bool pending() const { return m_pending; }   // a question waits for the model to load (cancel() forgets it)

  // Claude Code's tool calls arrive through ai::McpServer: toolGate() says why a call is refused (empty = go on, and
  // counts it), recordTool() adds the call and its result to the conversation and the journal.
  QString toolGate();
  void recordTool(const QJsonObject& call, const ToolResult& r);

signals:
  void stateChanged();
  void messagesChanged();             // a message was added or removed
  void textArrived();                 // the last message grew (streaming)

private:
  void onStdout();
  void onFinished(int code, QProcess::ExitStatus st);
  void setState(State s, const QString& err = {});
  void sendPending();
  void runTools();                    // the last answer asked for tools: run them, then let the model go on
  void journal(const QJsonObject& call, bool ok, const QString& result);
  static constexpr int kMaxRounds = 6;
  int m_round = 0, m_failedRounds = 0;
  bool m_cancelTools = false;
  quint64 m_gen = 0;                  // bumped by stop(), the runner's exit and each question: ends older tool rounds
  void load();
  void save();
  QString profile(const QString& helper, const QString& model) const;
  QString systemPrompt() const;
  // Claude Code
  void runClaude(bool freshSession = false);
  void onClaudeLine(const QJsonObject& j);
  void onClaudeFinished(int code, QProcess::ExitStatus st);
  QString claudeSystemPrompt() const;
  QString claudeSession(bool* used) const;     // the conversation's Claude Code session id (created on demand)
  void setClaudeSession(const QString& id, bool used);
  QProcess* m_cc = nullptr;
  QByteArray m_ccBuf, m_ccErr;
  bool m_ccThinking = false, m_ccResult = false, m_ccRetried = false, m_ccStopped = false;
  int m_ccCalls = 0;
  static constexpr int kMaxClaudeCalls = 20;   // tool calls per question

  int m_index;
  QProcess* m_proc = nullptr;
  QList<QProcess*> m_exiting;         // stopped processes still exiting (a model freeing its memory)
  bool m_startWhenFree = false;       // start() waits for m_exiting to empty before loading a model
  void retire(QProcess* p, bool wait);
  State m_state = State::Off;
  QString m_error, m_device, m_modelFile, m_modelPath;
  int m_ctx = 0, m_reqId = 0;
  double m_tps = 0;
  bool m_pending = false;             // a question waits for the engine to be ready
  QByteArray m_buf, m_err;
  QVector<ChatMessage> m_messages;
};

class Engines : public QObject {
  Q_OBJECT
public:
  static Engines& instance();
  Engine* at(int i) const { return m_engines.value(i); }
private:
  Engines();
  QVector<Engine*> m_engines;
};

QString stateLabel(Engine::State s);   // translated short label for pills
}  // namespace ai
