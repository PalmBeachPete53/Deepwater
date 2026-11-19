#pragma once
#include "fundamental/Fundamental.h"
#include <QWidget>
#include "app/Preload.h"
#include <QTimer>

namespace seasonality { class SmoothScroll; }
namespace fundamental {
class CardStrip; class PolicyCard; class ReadCard; class ComparatorCard; class HeatmapCard; class Content;

// Market › Fundamental Heatmap: the eight major currencies scored on six fundamentals (rates, inflation, growth,
// labour, activity, sentiment), a macro file per currency (policy rate, last move, next meeting, inflation vs
// target, 5Y–MAX history charts), a rule-based AUTOMATED READ, a pair comparator and the 8x6 heatmap.
// Scoring rules: src/fundamental/Fundamental.h. Inputs: fundamental::Source (Economy services; phase 1 none →
// loading state, no number is shown).
//
// Test hooks (env): DW_FUND_INPUT=<json> (inputs from a file, see fundamental::loadJson), DW_FUND_CCY=<ccy>,
// DW_FUND_SCROLL=<px>, DW_FUND_HOVER=x,y (page coordinates: inflation/policy chart or heatmap cell),
// DW_FUND_DETAIL=1 (release detail open), DW_FUND_OTHER=<ccy> (comparator), DW_FUND_FRESH=1 (keep the gold
// "just selected" card border), DW_FUND_REVEAL=<0..1> (freeze the chart draw-in, needs a currency change: DW_FUND_CCY).
class FundamentalPage : public QWidget, public app::Preloadable {
  Q_OBJECT
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit FundamentalPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  bool event(QEvent*) override;
private:
  void refresh(bool animate);
  void select(const QString& ccy);
  void tick();
  void applyHooks();
  const fundamental::Inputs* inputsOf(const QString& ccy) const;

  fundamental::Source* m_source;
  QList<fundamental::Inputs> m_inputs;
  QVector<fundamental::Score> m_scores;
  QString m_sel;
  bool m_selAuto = false;   // m_sel is the USD fallback picked before any score: replace it once scores exist
  seasonality::SmoothScroll* m_scroll;
  fundamental::Content* m_content;
  QTimer m_tick, m_fade;
  qint64 m_t0 = 0;
  bool m_cursorOn = true, m_hooksDone = false;
  double m_pulse = 0;
};
}  // namespace fundamental
