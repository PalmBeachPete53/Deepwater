#pragma once
#include <QAbstractButton>
#include <QElapsedTimer>
#include <QLineF>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

// AI › Agents, the conversation with one environment's agent, WEB ticked (TOILE in French; first called CONSTELLATION, renamed by the user 2026-11-19; user's request 2026-11-19, from a
// hand drawing: the conversation in the middle, linked by lines to what the agent does around it). Three nodes:
//   COLLECTE      (left)         the data the agent read: its data tools' calls (get_*, read_*, ml_load_dataset…), newest
//                                first, with what each asked for; the one running now on top.
//   ENTRAÎNEMENT  (upper right)  its ML trainings: the activity panel's stack of tiles (live curve, small result chart, ← →).
//   COMPARAISON   (lower right)  its last ml_compare: the same comparison, done again by the app from the call's arguments
//                                (ml::comparison), each group's best and verdict, in the interface language.
// A line lights up and carries a moving point while its node's work runs (a data tool, a training, a comparison).
namespace aipage {
class AgentActivity;

// A labelled tick box, the chat header's style (SkillBox) in the accent colour.
class TickBox : public QAbstractButton {
public:
  TickBox(const QString& text, QWidget* parent);
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
  void enterEvent(QEnterEvent*) override { update(); }
  void leaveEvent(QEvent*) override { update(); }
};

class CollectNode;
class CompareNode;

class Constellation : public QWidget {
public:
  explicit Constellation(QWidget* parent);
  // Phones (narrower than ui::kCompactWidth): the conversation on top, the three nodes stacked under it (their links
  // in the left margin); this much height is theirs, under the conversation.
  static int phoneNodesHeight();
  void setEnvironment(int index);
  // The conversation card (transcript and input), placed in the middle; ChatView takes it back with takeChat().
  void setChat(QWidget* chat);
  QWidget* takeChat();

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;

private:
  void place();
  void syncTick();                  // the clock runs while a line is lit (and the page shows)
  enum Link { Collect, Train, Compare };
  bool active(Link k) const;
  QLineF linkLine(Link k) const;    // from the conversation's edge to the node
  QRect linkRect(Link k) const;     // what a lit link repaints
  qint64 m_lastSecond = -1;

  int m_index = -1;
  QList<QMetaObject::Connection> m_conns;
  QWidget* m_chat = nullptr;
  AgentActivity* m_train;
  CollectNode* m_collect;
  CompareNode* m_compare;
  QRect m_chatR, m_collectR, m_trainR, m_compareR;
  QTimer m_tick;
  QElapsedTimer m_clock;
  QPixmap m_stars;                  // the faint star field, drawn once per size
  qreal m_starsDpr = 0;
};

}  // namespace aipage
