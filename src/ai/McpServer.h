#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QVector>

class QTcpServer;
class QTcpSocket;

// Deepwater's MCP server for the Claude Code integration (ai::claudecode): MCP "Streamable HTTP" with plain JSON replies,
// on 127.0.0.1 only, on a free port chosen at the first use. One endpoint per environment, /mcp/<n>, each with its own
// random token (new at every app launch, written only to that environment's private mcp.json, mode 0600).
//
// What it offers is the cube's permission and nothing else: tools/list = the tools of the skills the environment grants
// now (ai::Tools::forSkills); tools/call checks the skill again, refuses when no question of that environment is being
// answered by Claude Code, caps the calls per question, and hands each call to ai::Engine::recordTool (chat line +
// tool journal). Requests with an Origin header (browsers) or a Host other than 127.0.0.1/localhost are refused.
namespace ai {

class McpServer : public QObject {
  Q_OBJECT
public:
  static McpServer& instance();
  // Starts listening if needed and writes <env folder>/mcp.json for `claude --mcp-config`; empty on failure.
  QString writeConfig(int env);
  quint16 port() const;

private:
  McpServer();
  bool listen();
  void onReadable(QTcpSocket* s);
  // 0 when a request may go on, else the HTTP status to refuse it with: decided from the head alone, before any body
  int gate(const QByteArray& path, const QHash<QByteArray, QByteArray>& headers) const;
  void handle(QTcpSocket* s, const QByteArray& method, const QByteArray& path, const QHash<QByteArray, QByteArray>& headers,
              const QByteArray& body);
  void reply(QTcpSocket* s, int code, const QByteArray& body, const QByteArray& extraHeaders = {});
  QTcpServer* m_srv = nullptr;
  QVector<QString> m_tokens;
  QHash<QTcpSocket*, QByteArray> m_buf;
};
}  // namespace ai
