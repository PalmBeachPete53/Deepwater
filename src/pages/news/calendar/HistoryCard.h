#pragma once
#include <QRectF>
#include <QPointF>
#include <QString>
#include "news/History.h"

class QPainter;

namespace news::pages {

// "<COUNTRY> · RELEASE HISTORY" card painted inside the calendar table's inline expansion.
// Not a widget: the table owns the geometry, forwards mouse events and repaints when a call returns true.
class HistoryCard {
public:
  enum Mode { Bars, Line };
  enum Span { Y5, Y10, Y20, Max };
  static constexpr int kHeight = 473;

  void setLoading(const QString& title, const QString& country);
  void setSeries(const news::Series& s);
  bool loaded() const { return m_loaded; }

  void paint(QPainter& p, const QRectF& card) const;
  bool mouseMove(const QRectF& card, const QPointF& pos);
  bool mousePress(const QRectF& card, const QPointF& pos);
  bool mouseRelease(const QRectF& card, const QPointF& pos);
  bool mouseDoubleClick(const QRectF& card, const QPointF& pos);
  bool leave();
  bool wantsPointer(const QRectF& card, const QPointF& pos) const;   // over a button
  bool dragging() const { return m_dragFrom >= 0; }
  // test hooks: hover the point at fraction f of the visible range
  void hoverFraction(const QRectF& card, double f);
  void setMode(Mode m) { m_mode = m; }
  void setSpan(Span s) { m_span = s; m_zoomA = m_zoomB = -1; }

private:
  QRectF plotRect(const QRectF& card) const;
  QRectF barsRect(const QRectF& card) const;
  void visible(int& a, int& b) const;          // index range shown (inclusive a, exclusive b)
  int indexAt(const QRectF& card, double x) const;
  struct Btn { QRectF r; int id; QString text; };
  QList<Btn> buttons(const QRectF& card) const;

  news::Series m_s;
  QString m_title, m_country;
  bool m_loaded = false;
  Mode m_mode = Bars;
  Span m_span = Y20;
  int m_zoomA = -1, m_zoomB = -1;   // zoomed index range
  int m_hover = -1;
  int m_dragFrom = -1, m_dragTo = -1;
  int m_hoverBtn = -1;
};
}  // namespace news::pages
