// Stand-alone host of the Charts tab (see CMakeLists.txt). Same window chrome-less page as inside Deepwater.
#include "pages/charts/ChartsApp.h"
#include "ui/Theme.h"
#include "charts/data/Providers.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

// Optional hook implemented by the page shell: runs a named UI step for screenshots
// ("side:alerts", "bottom:tester", "modal:indicators", "modal:settings", "modal:symbol", ...). Weak so the harness
// builds before the shell defines it.
namespace charts::ui { __attribute__((weak)) bool runUiStep(ChartsApp*, const QString&); }

int main(int argc, char** argv) {
  // The harness never serves the local API nor runs the alert checker (the real app may already hold port 3777).
  if (qEnvironmentVariableIsEmpty("DW_CHARTS_API")) qputenv("DW_CHARTS_API", "0");
  QApplication app(argc, argv);
  QApplication::setOrganizationName("deepwater-terminal");
  QApplication::setOrganizationDomain("deepwater-terminal.com");
  QApplication::setApplicationName("Deepwater");
  QCommandLineParser cli;
  cli.addOptions({{"size", "WxH", "size", "1600x1000"}, {"screenshot", "png", "file"}, {"delay", "ms", "ms", "6000"},
                  {"offline", "fixtures only"}, {"eval", "ui steps, ';' separated", "steps"}, {"theme", "theme id", "id"}});
  cli.parse(app.arguments());
  if (cli.isSet("offline")) {   // the JS QT_OFFLINE=1 mode: providers answer from quant-terminal/test/fixtures
    qputenv("QT_OFFLINE", "1");
    charts::data::testing::setOffline(QStringLiteral(CHARTS_FIXTURES));
  }
  theme::install();
  if (cli.isSet("theme")) theme::setCurrent(cli.value("theme"));
  auto* w = new charts::ui::ChartsApp;
  const auto wh = cli.value("size").split('x');
  w->resize(wh.value(0).toInt(), wh.value(1).toInt());
  w->show();
  if (cli.isSet("eval")) {
    const QStringList steps = cli.value("eval").split(';', Qt::SkipEmptyParts);
    QTimer::singleShot(cli.value("delay").toInt() / 2, w, [w, steps] {
      for (const auto& s : steps)
        if (!charts::ui::runUiStep || !charts::ui::runUiStep(w, s.trimmed())) qWarning("ui step failed: %s", qPrintable(s));
    });
  }
  if (cli.isSet("screenshot")) {
    const QString file = cli.value("screenshot");
    QTimer::singleShot(cli.value("delay").toInt(), w, [w, file] {
      QDir().mkpath(QFileInfo(file).absolutePath());
      w->grab().save(file);
      qApp->quit();
    });
  }
  return app.exec();
}
