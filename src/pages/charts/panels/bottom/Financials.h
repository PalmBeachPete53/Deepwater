#pragma once
// Charts tab — bottom panel, Financials. Port of quant-terminal public/js/research.js renderFinancials
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). Data: charts::data::financials (Nasdaq).
#include <QObject>
#include <QString>

namespace charts::ui {
class BottomPanel;
namespace bottom {

class FinancialsTab : public QObject {
  Q_OBJECT
public:
  explicit FinancialsTab(BottomPanel* panel);
  void render() { render("annual", "income"); }                 // renderFinancials(bottom, app)
  void render(const QString& freq, const QString& tab);         // renderFinancials(el, app, { freq, tab })

private:
  BottomPanel* P;
  int seq_ = 0;
};

}  // namespace bottom
}  // namespace charts::ui
