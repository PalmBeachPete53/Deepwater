#pragma once
#include <QWidget>
#include <algorithm>
#include <QRectF>
#include <QList>
#include <QVector>
#include "macro/MacroStore.h"

class QLineEdit;
class QCompleter;

namespace economy {

// Economy › Macro "Asset File": dashboard (policy rate, sovereign curve, indicator table, long-run cards) and the
// per-indicator detail view (history bars, compare, standard deviations, caption, recorded history).
// Painted by hand to match the reference frames; lives inside the page's scroll area.
class AssetFile : public QWidget {
  Q_OBJECT
public:
  explicit AssetFile(QWidget* parent = nullptr);
  void setEconomy(const QString& iso2);
  QString economyIso() const { return m_iso; }
  void showDashboard();
  void showDetail(macro::Ind ind);
  bool inDetail() const { return m_detail; }
  void setViewportHeight(int h) { if (h != m_viewH) { m_viewH = h; relayout(); update(); } }   // scroll viewport height
  void setTab(int t) { m_tab = std::clamp(t, 0, 3); relayout(); update(); }
  // test hooks
  void setHoverPoint(const QPointF& p);
signals:
  void closeRequested();
  void heightChanged();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  bool mouseSensitive(const QPointF& pt) const;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void mouseDoubleClickEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
private:
  struct Row { macro::Ind ind; macro::Ind fallback; QString label; };
  struct Card { macro::Ind ind; };
  QList<Row> rows() const;
  QStringList tabLabels() const;
  macro::Series rowSeries(const Row& r, bool* fellBack = nullptr) const;
  void relayout();
  void paintHeader(QPainter& p);
  void paintDashboard(QPainter& p);
  void paintSummary(QPainter& p, const QRectF& box);
  void paintTable(QPainter& p);
  void paintTablePhone(QPainter& p);
  void paintCards(QPainter& p);
  void paintDetail(QPainter& p);
  void paintChart(QPainter& p, const QRectF& plot);
  QVector<macro::Point> window() const;   // visible points of the detail series
  void applyRange();
  void showTip(const QString& text);
  QString captionText(const QVector<macro::Point>& w) const;   // detail caption (one line on desktop, wrapped on phones)

  QString m_iso = "us";
  bool m_detail = false;
  macro::Ind m_ind = macro::Ind::Inflation;
  macro::Series m_series, m_compare;
  QString m_compareIso;
  int m_tab = 0;         // MAIN PRICES GROWTH LABOUR
  int m_cardRange = 1;   // 5Y 10Y 20Y MAX
  int m_detRange = 1;    // 3Y 5Y 10Y MAX
  bool m_area = false, m_std = false;
  int m_win0 = 0, m_win1 = -1;   // zoom window (indices into m_series.pts), -1 = range
  // geometry
  qreal m_x = 141, m_w = 1318;
  QRectF m_close, m_summary, m_curvePlot, m_table, m_backBtn, m_headBox, m_chartBox, m_plot, m_caption, m_cal;
  QList<QRectF> m_tabRects, m_rowRects, m_cardRects, m_rangeRects, m_detRangeRects, m_typeRects;
  QRectF m_stdRect, m_fcRect, m_compareRect;
  // phones (ui::compact): one column; the blocks below move down by what the wrapped texts and stacked controls need.
  // The desktop values are the reference positions.
  bool m_phone = false;
  qreal m_dashY = 427;     // "MACRO DASHBOARD" label baseline
  qreal m_advY = 0;        // ADVANCED heading
  qreal m_fnH = 30;        // table footnote box
  qreal m_advTextH = 0;    // wrapped ADVANCED note height (0: one line, beside the range buttons)
  qreal m_cardsTop = 0;
  qreal m_hintY = 275;     // detail: drag hint baseline (< 0: not drawn)
  QRectF m_frame;          // detail: chart frame
  // interaction
  QPointF m_mouse{-1, -1};
  bool m_inside = false;
  int m_viewH = 0;
  QString m_captionFull;
  bool m_dragging = false; qreal m_dragX0 = 0, m_dragX1 = 0;
  QLineEdit* m_compareEdit = nullptr;
  QCompleter* m_completer = nullptr;
};
}  // namespace economy
