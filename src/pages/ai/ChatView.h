#pragma once
#include <QWidget>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <functional>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QSplitter;
class QTextBrowser;
class QVariantAnimation;
class QVBoxLayout;
class QHBoxLayout;

// AI › Agents: the conversation with the model of one environment (ai::Engine), shown in place of the cubes.
// Header: back to the cubes, the environment's small cube (lit by the engine's state), model, state, device and speed,
// switch to another lit environment, clear. Transcript rendered from Markdown (reasoning blocks dimmed). Enter sends,
// Shift+Enter breaks the line, STOP ends an answer.
// Phones (narrower than ui::kCompactWidth, 2026-11-21): the header on two rows and the skill boxes under it; the
// activity panel (or the WEB nodes) below the conversation instead of beside it, the page scrolling down to them.
namespace aipage {
class MiniCube;
class AgentActivity;
class SkillBox;
class Constellation;
class TickBox;

class ChatView : public QWidget {
public:
  explicit ChatView(QWidget* parent);
  std::function<void()> onBack;
  std::function<void()> onLayoutChanged;   // phoneExtra() changed (WEB ticked or not): the page lays out again
  static constexpr int kPhoneActivityH = 420;
  // Height added under the conversation at this width: on a phone, the activity panel or the WEB nodes stand below it.
  int phoneExtra(int width) const;
  void setEnvironment(int index);
  int environment() const { return m_index; }
  void focusInput();

protected:
  bool eventFilter(QObject* o, QEvent* e) override;
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;

private:
  void send();
  void syncHeader();
  void render();                 // transcript → HTML (throttled while streaming)
  QString fragment(int i);       // one message's body as HTML (cached once the message is complete)
  void followBottom(bool now);   // scroll to the end: slides, or jumps when now or more than a screen away

  int m_index = -1;
  QList<QMetaObject::Connection> m_conns;
  QPushButton *m_back, *m_clear, *m_send, *m_retry;
  QList<QPushButton*> m_envButtons;
  QList<SkillBox*> m_skillBoxes;
  MiniCube* m_cube;
  AgentActivity* m_activity;
  // CONSTELLATION ticked (ai.constellation): the conversation in the middle of its nodes instead of beside the activity panel
  TickBox* m_constBox;
  Constellation* m_constellation;
  QSplitter* m_split;
  QWidget* m_chatHost;           // transcript + input: in the splitter, or in the constellation
  void setConstellation(bool on);
  void setPhone(bool on);
  void syncPhoneSizes();   // phones: the activity panel's height under the conversation
  bool m_phone = false;
  QVBoxLayout* m_root = nullptr;
  QVBoxLayout* m_titles = nullptr;
  QHBoxLayout* m_head = nullptr;
  QHBoxLayout* m_skillRow = nullptr;
  QHBoxLayout* m_head2 = nullptr;
  QLabel *m_title, *m_model, *m_state, *m_info, *m_error, *m_hint;
  QWidget* m_errorBox;
  QTextBrowser* m_view;
  QPlainTextEdit* m_input;
  QTimer m_renderTimer;
  QHash<int, QPair<size_t, QString>> m_cache;   // message index → (text hash, html)
  QHash<int, QPair<size_t, QString>> m_figCache;   // message index → (hash of the answer + tool data, figures-check html)
  QSet<int> m_openThinking;                      // finished reasoning blocks the user unfolded
  QStringList m_parts;            // the HTML parts in the document (one per message or notice)
  QList<int> m_partPos;           // where each part's block break starts in the document
  QVariantAnimation* m_scrollAnim = nullptr;
  bool m_follow = true;           // the view keeps to the end of the conversation
  bool m_ownScroll = false;       // a scroll made by render() or the slide, not by the user
  bool m_toBottom = false;
  bool m_stale = false;   // a render skipped while hidden: done when shown
};
}  // namespace aipage
