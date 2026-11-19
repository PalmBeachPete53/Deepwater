// charts_data_tests: ports of quant-terminal's test/*.test.js for the data services (src/charts/data).
// User data is never touched: DW_PROFILE points at a temporary folder for the whole run, so paths::data(...)
// (imports, custom indicators, state.json) lands there. TZ is fixed because JS Date.parse reads dates without a
// zone ("Nov 15, 2026") as local time and the expected values were produced in Europe/Paris.
#include "Check.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <ctime>

int main(int argc, char** argv) {
  qputenv("TZ", "Europe/Paris");
  tzset();
  QTemporaryDir profile;
  qputenv("DW_PROFILE", profile.path().toUtf8());
  qputenv("QT_OFFLINE", "");
  QCoreApplication app(argc, argv);
  return check::runAll(argc, argv);
}
