// Keeps BTCUSDT's futures statistics (open interest, long/short ratio, funding, real liquidations) recording while
// Deepwater runs, so the liquidation heatmap's history grows past the 30 days Binance keeps (chantier E, step E2).
// Not in test profiles (DW_PROFILE) unless DW_FUTURES_COLLECT=1; not when Binance futures is switched off in Sources.
// It costs one request per series every 5 minutes (weight 0) and one quiet WebSocket.
#include "charts/data/FuturesStats.h"
#include "core/Startup.h"
#include "core/Settings.h"
#include "net/Sources.h"
#include <QCoreApplication>
#include <QTimer>

namespace {
std::shared_ptr<charts::data::FuturesStats>& held() {
  static std::shared_ptr<charts::data::FuturesStats> s;
  return s;
}

void startFuturesCollector() {
  QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { held().reset(); });   // writes what is pending
  QTimer::singleShot(25000, qApp, [] {
    if (settings::isTestProfile() && qEnvironmentVariable("DW_FUTURES_COLLECT") != "1") return;
    if (sources::isDisabled("binance-futures")) return;
    held() = charts::data::FuturesStats::acquire("BTCUSDT");
  });
}
}  // namespace
DW_STARTUP_FUNCTION(startFuturesCollector)
