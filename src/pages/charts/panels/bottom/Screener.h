#pragma once
// Charts tab — bottom panel, Screener. Port of quant-terminal public/js/panels.js (sc, loadScreener, filtered,
// renderScreener, drawScreener) (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
// The technical filter runs charts::data::scan (src/charts/data/Scan.h), the port of the server's POST /api/scan.
#include "charts/data/Providers.h"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <functional>
#include <optional>

class QScrollArea;
class QVBoxLayout;
class QPushButton;
class QComboBox;

namespace charts::ui {
class BottomPanel;
namespace rules { class RuleList; }
namespace bottom {

// GET /api/screener?universe=<crypto|us|etf>: the JS row objects ({ticker, symbol, name, price, changePct, volume,
// marketCap?, sector?, ...}), or an error message. cb on the GUI thread, skipped once ctx is gone.
void loadScreenerRows(const QString& universe, QObject* ctx,
                      std::function<void(const QList<QJsonObject>& rows, const QString& error)> cb);

class ScreenerTab : public QObject {
  Q_OBJECT
public:
  explicit ScreenerTab(BottomPanel* panel);
  void render();                                    // renderScreener
  void setUniverse(const QString& u);               // the Market select (screenshots: "bt:screener:us")

private:
  void load();                                      // loadScreener
  QList<QJsonObject> filtered(bool withScan = true) const;
  void draw();                                      // drawScreener
  void runScan();

  BottomPanel* P;
  struct {
    QString universe = "crypto";
    QList<QJsonObject> rows;              // the JS row objects (ScreenerRow::toJson)
    QString sort = "volume";
    int dir = -1;
    QHash<QString, QJsonValue> filters;             // q, minPrice, maxPrice, minChg, maxChg, minVol, minCap, sector
    std::optional<QHash<QString, QJsonObject>> scan;   // symbol -> {symbol, pass, passes, close} | {symbol, error}
    QJsonObject rule;
    QString tf = "1D";
  } sc;
  bool loading_ = false;
  int loadSeq_ = 0;
  int scanSeq_ = 0;   // bumped by each scan and each change of market: an older scan's answer is dropped
  QPointer<QWidget> body_;
  QPointer<QWidget> box_;                           // #scTable
  QVBoxLayout* boxLay_ = nullptr;
  QPointer<QScrollArea> area_;
  QPointer<rules::RuleList> rule_;
  QPointer<QComboBox> tfSel_;
  QPointer<QPushButton> scanBtn_;
};

}  // namespace bottom
}  // namespace charts::ui
