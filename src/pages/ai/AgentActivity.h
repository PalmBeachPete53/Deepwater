#pragma once
#include "pages/ai/MlCharts.h"
#include <QHash>
#include <QSet>
#include <QPolygonF>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class QPushButton;
class QVariantAnimation;
namespace ml { struct Run; }

// AI › Agents, beside an environment's conversation: what its agent is doing, in small. From the top: what it does at this
// moment (the tool it calls, how long it has waited for it, or that it writes / waits), its ML trainings as a stack of
// tiles, and its last tool calls. Painted, read-only; it refreshes from ai::Engine and ml::Runs signals and, while
// something runs, twice a second (clocks).
//
// The trainings stack: one tile per training this agent started (name, data, progress bar and stage, the trainer's live
// curve while it runs; then the ML page's result chart, small, its result or why it failed, STOP while it runs). A new
// training slides in on top of the stack; ← and → (or the ‹ › beside the count) go back through the older ones and
// forward again. The edges of the tiles underneath show below the one on top.
namespace aipage {

QString toolLabel(const QString& toolName);   // what a tool call does, in a few words (the interface language)

class AgentActivity : public QWidget {
public:
  static constexpr int kMinWidth = 240;
  explicit AgentActivity(QWidget* parent);
  void setEnvironment(int index);
  // only the trainings' stack (no ACTIVITY / NOW header, no LAST ACTIONS): the Entraînement node of the constellation
  void setTrainingsOnly(bool on) { m_trainingsOnly = on; update(); }
  QSize sizeHint() const override { return {300, 400}; }

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void mousePressEvent(QMouseEvent*) override;

private:
  QStringList runIds() const;     // the trainings this environment's agent started, newest first
  QString shownId() const;        // the tile on top
  void sync();                    // timer on while something runs, a new training slides in, STOP for a running one
  void go(int step);              // +1 an older tile, -1 a newer one
  void slide(const QString& from, const QString& to, bool push);
  // The run's small result chart, built once per run and result. Its files are read on a worker thread (a real run holds
  // up to 80 000 rows): nullptr until they are; the tile shows an empty frame meanwhile and is drawn again when ready.
  MlMiniChart* miniFor(const ml::Run& r);
  QSet<QString> m_loading;        // runs whose chart files are being read
  // Draws the tile of r at area's top-left, area.width() wide; returns its height. Live: the chart and STOP are the real
  // widgets, placed there; Picture: the chart is painted into p (the pictures that slide); Measure: the height only.
  // Quick / QuickMeasure: during fast browsing, the tile without its chart (an empty frame in its place): no chart widget
  // built, no rows read
  enum TileMode { Live, Picture, Measure, Quick, QuickMeasure };
  int paintTile(QPainter& p, const QRect& area, const ml::Run& r, TileMode mode);
  QPixmap tilePicture(const QString& id, int width, int* height);
  // "3D" ticked: the stack drawn in isometric view, one slab per training (its tile on the top face), the selected one on
  // top and up to four older ones under it; returns the y below it.
  int paint3D(QPainter& p, int y, const QStringList& ids);
  QPixmap facePicture(const QString& id, int width, int* height);   // tilePicture, kept while the run does not change
  void prefetch();                // draws the neighbouring tiles' pictures ahead, while idle

  int m_index = -1;
  bool m_trainingsOnly = false;
  QList<QMetaObject::Connection> m_conns;
  QTimer m_tick;
  QPushButton* m_stop;
  int m_pos = 0;                  // the tile on top: 0 = the newest training
  QString m_newest;               // the newest training seen: a newer one slides in
  QHash<QString, MlMiniChart> m_minis;
  QHash<QString, QString> m_miniKeys;
  // the slide between two tiles: their pictures, heights, and whether the new one comes on top (push) or the top one
  // leaves (pop)
  QVariantAnimation* m_anim;
  QPixmap m_fromPic, m_toPic;
  int m_fromH = 0, m_toH = 0;
  bool m_push = true;
  // browsing fast (presses under 250 ms apart): only the place moves; tiles show from their pictures or quick, and the
  // full tile comes once the presses stop (m_settle)
  bool m_fast = false;
  QTimer m_settle;
  qint64 m_lastGoMs = 0;
  bool hasFace(const QString& id, int width) const;   // its picture is ready and current
  QRect m_olderRect, m_newerRect;   // ‹ and › beside the count
  bool m_3d = false;                // ai.activity3d
  QRect m_3dRect;                   // the "3D" box
  QHash<QString, QPair<QString, QPixmap>> m_faces;   // run id → (what it was drawn from, picture)
  QList<QPair<QString, QPolygonF>> m_slabHits;       // the slabs drawn, topmost first: a click picks one
};

}  // namespace aipage
