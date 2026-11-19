#pragma once
#include <QWidget>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <functional>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTextBrowser;

// AI › Agents: the conversation with the model of one environment (ai::Engine), shown in place of the cubes.
// Header: back to the cubes, the environment's small cube (lit by the engine's state), model, state, device and speed,
// switch to another lit environment, clear. Transcript rendered from Markdown (reasoning blocks dimmed). Enter sends,
// Shift+Enter breaks the line, STOP ends an answer.
namespace aipage {
class MiniCube;
class SkillBox;

class ChatView : public QWidget {
public:
  explicit ChatView(QWidget* parent);
  std::function<void()> onBack;
  void setEnvironment(int index);
  int environment() const { return m_index; }
  void focusInput();

protected:
  bool eventFilter(QObject* o, QEvent* e) override;
  void paintEvent(QPaintEvent*) override;
  void showEvent(QShowEvent*) override;

private:
  void send();
  void syncHeader();
  void render();                 // transcript → HTML (throttled while streaming)
  QString fragment(int i);       // one message's body as HTML (cached once the message is complete)

  int m_index = -1;
  QList<QMetaObject::Connection> m_conns;
  QPushButton *m_back, *m_clear, *m_send, *m_retry;
  QList<QPushButton*> m_envButtons;
  QList<SkillBox*> m_skillBoxes;
  MiniCube* m_cube;
  QLabel *m_title, *m_model, *m_state, *m_info, *m_error, *m_hint;
  QWidget* m_errorBox;
  QTextBrowser* m_view;
  QPlainTextEdit* m_input;
  QTimer m_renderTimer;
  QHash<int, QPair<size_t, QString>> m_cache;   // message index → (text hash, html)
  QHash<int, QPair<size_t, QString>> m_figCache;   // message index → (hash of the answer + tool data, figures-check html)
  QSet<int> m_openThinking;                      // finished reasoning blocks the user unfolded
  bool m_toBottom = false;
  bool m_stale = false;   // a render skipped while hidden: done when shown
};
}  // namespace aipage
