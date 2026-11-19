#pragma once
// Charts tab — bottom panel, Seasonals. Port of quant-terminal public/js/research.js renderSeasonals
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). Chart drawn with charts::plot.
#include <QObject>

namespace charts::ui {
class BottomPanel;
namespace bottom {

class SeasonalsTab : public QObject {
  Q_OBJECT
public:
  explicit SeasonalsTab(BottomPanel* panel);
  void render() { render(years_); }   // renderSeasonals(bottom, app)
  void render(int years);             // renderSeasonals(el, app, { years })

private:
  BottomPanel* P;
  int years_ = 6;                     // the JS forgets the choice on the next render: opts.years || 6
  int seq_ = 0;
};

}  // namespace bottom
}  // namespace charts::ui
