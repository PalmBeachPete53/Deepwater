// Starts the market collector with the app (kept out of the tests, which link Collector.cpp: no test may reach Binance).
#include "technicaldata/Dukascopy.h"
#include "technicaldata/ForexCollector.h"
#include "core/Settings.h"
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataIndicators.h"
#include "net/Sources.h"
#include <QCoreApplication>
#include <QTimer>

// At launch: catch up what was missed while Deepwater was closed, then keep up every minute. Not in test profiles
// unless asked (DW_TECHNICALDATA_COLLECT=1): a first catch-up is thousands of requests.
static bool g_started = false;

static void startMarketCollector() {
  // closing: the downloads stop (an answer still coming writes nothing), the indicators finish their piece, then the
  // writes queued so far are done (Store waits for them too); every write is a transaction, so a run killed outright
  // loses at most what was not committed, and the next one goes on from what is stored
  QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
    if (!g_started) return;
    technicaldata::Collector::instance().stop();
    technicaldata::ForexCollector::instance().stop();
    technicaldata::Indicators::instance().shutdown();
    technicaldata::Store::instance().wait();
  });
  QTimer::singleShot(20000, qApp, [] {
    if (settings::isTestProfile() && qEnvironmentVariable("DW_TECHNICALDATA_COLLECT") != "1") return;
    if (qEnvironmentVariableIsSet("DW_TECHNICALDATA_ONLY")) {   // test hook: only these pairs (comma-separated)
      const QStringList only = qEnvironmentVariable("DW_TECHNICALDATA_ONLY").split(',', Qt::SkipEmptyParts);
      QStringList crypto, fx;
      for (const QString& m : only) (technicaldata::dukascopy::isForex(m) ? fx : crypto) << m;
      technicaldata::Collector::instance().setMarkets(crypto.isEmpty() ? QStringList{"NONE:NONE"} : crypto);
      technicaldata::ForexCollector::instance().setMarkets(fx.isEmpty() ? QStringList{"NONE:NONE"} : fx);
      technicaldata::Indicators::instance().setMarkets(only);
    }
    g_started = true;
    if (!sources::isDisabled("binance")) technicaldata::Collector::instance().start();
    if (!sources::isDisabled("dukascopy")) technicaldata::ForexCollector::instance().start();   // forex: Dukascopy
    technicaldata::Indicators::instance().start();   // the key indicators of every bar stored, by Charts' engine
  });
}
Q_COREAPP_STARTUP_FUNCTION(startMarketCollector)
