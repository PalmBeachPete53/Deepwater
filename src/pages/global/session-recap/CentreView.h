#pragma once
#include "pages/global/session-recap/RecapWidgets.h"

namespace wire::recap {
// The session file: title + meta, AUTOMATED READ card (EN/FR), WINDOW TIMELINE (traced market, release and
// wire lanes, hover tooltip) and MOVES OF THE WINDOW (4x3 cards, click to trace). One painted, scrolling view.
class CentreView : public SmoothScroll {
  Q_OBJECT
public:
  explicit CentreView(QWidget* parent = nullptr);
  void setWindow(const Window& w, const QVector<Stat>& st, const QList<wire::Release>& rel,
                 const QList<wire::Item>& headlines, bool releasesConnected, bool loading, const QString& note);
  void setTraced(int asset);
  void setLang(const QString& l);
  void setHoverX(int viewportX);   // test hook: crosshair at this x inside the chart
  int traced() const { return m_traced; }
  int timelineTop() const { return m_geo.timeline.top(); }
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  struct Geo {
    int w = 0;
    QRect card, timeline, plot, grid;
    QList<int> secTop, secH, secX, secRight;   // sections flow in 1 column, or 2 on wide views
    int extra = 0;                            // extra timeline height on wide views
    int leadH = 0, height = 0;
    QList<QRectF> chips;
    QList<QRect> cards;
    QRectF en, fr;
  };
  void relayout();
  void updateTimeline();   // repaint of the timeline card only (crosshair)
  void paintTitle(QPainter& p);
  void paintRead(QPainter& p);
  void paintTimeline(QPainter& p);
  void paintGrid(QPainter& p);
  QList<int> chipAssets() const;
  QVector<QPointF> points(int asset, const QRectF& r, double lo, double hi) const;
  int hoverIndex() const;
  qreal xOf(qint64 t) const;

  Window m_w;
  QVector<Stat> m_st;
  QList<wire::Release> m_rel;
  QList<wire::Item> m_wire;
  bool m_connected = false, m_loading = true;
  QString m_note;
  Read m_read;
  QString m_lang = "EN";
  int m_traced = -1;
  bool m_userTraced = false;     // a chip / card was clicked: keep it across refreshes
  int m_hoverX = -1;               // content x of the crosshair, -1 none
  Geo m_geo;
};
}
