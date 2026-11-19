#include "app/MainWindow.h"
#include "app/LayoutAudit.h"
#include "app/PerfAudit.h"
#include "ui/Theme.h"
#include "account/BridgeSync.h"
#include "core/I18n.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QSurfaceFormat>
#include <QScreen>
#include <QDir>
#include <QFileInfo>
#include <QPainter>

// Dev flags (all optional, unknown flags are ignored):
//   --page tab/sub          open this page (e.g. market/seasonality)
//   --goto tab/sub --goto-at ms   open that page later (tests: what a visit finds after the background preloads)
//   --size WxH              window size in logical pixels (1600x1042 gives a 1600x966 page, the reference size)
//   --screenshot file.png   grab the window after --delay ms (default 6000) and quit
//   --quit-after ms         quit after this many ms (profiling runs)
//   --lang en|fr            interface language (saved, as from the Options dialog)
//   --options               open the Options dialog
//   --theme id              theme (deepwater, ember, abyss, jade, oxide, nova, bloomberg, glacier, dune, sakura; saved)
//   --layout-audit dir      every page alone at each size: captures + report.txt (see app/LayoutAudit.h); with
//                           --audit-sizes WxH,WxH (default 1024x600,1280x720,1440x900,1728x1052), --audit-pages a/b,c/d,
//                           --audit-load ms (wait for data on each page, default 5000). Use with DW_PROFILE.
//   --perf-audit dir        every page in turn: build / first-paint time, GUI-thread stalls, idle CPU, memory, and what the
//                           hidden pages cost (see app/PerfAudit.h); --perf-load ms, --perf-idle ms, --audit-pages.
// Env: DW_PROFILE=<dir> isolates settings writes and user data (tests), DW_CACHE_DIR=<dir> shares an HTTP cache.
int main(int argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
  // The Facts 3D views use OpenGL 3.3 core: on macOS a core context only shares with core contexts, so the default
  // format (Qt's own compositing context included) must be core too, set before the application exists.
  {
    QSurfaceFormat f = QSurfaceFormat::defaultFormat();
    f.setVersion(3, 3); f.setProfile(QSurfaceFormat::CoreProfile); f.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(f);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  }
  QApplication app(argc, argv);
  QApplication::setOrganizationName("deepwater-terminal");
  QApplication::setOrganizationDomain("deepwater-terminal.com");
  QApplication::setApplicationName("Deepwater");
  QApplication::setApplicationVersion("0.1.0");

  QCommandLineParser cli;
  cli.addOptions({{"lang", "en | fr (saved, like the Options dialog)", "lang"}, {"theme", "theme id (saved)", "id"}, {"options", "open the Options dialog"},
                  {"page", "page id", "id"}, {"size", "WxH", "size"}, {"screenshot", "png path", "file"},
                  {"delay", "ms", "ms", "6000"}, {"goto", "page", "page"}, {"goto-at", "ms", "ms", "5000"}, {"quit-after", "ms", "ms"}, {"layout-audit", "dir", "dir"},
                  {"audit-sizes", "WxH,...", "sizes", "1024x600,1280x720,1440x900,1728x1052"}, {"audit-pages", "ids", "ids"},
                  {"audit-load", "ms", "ms", "5000"}, {"perf-audit", "dir", "dir"}, {"perf-load", "ms", "ms", "6000"},
                  {"perf-idle", "ms", "ms", "8000"}});
  cli.parse(app.arguments());   // parse(), not process(): unknown flags are ignored

  theme::install();
  i18n::installSaved();
  if (cli.isSet("theme")) theme::setCurrent(cli.value("theme"));
  if (cli.isSet("lang")) i18n::setLanguage(cli.value("lang") == "fr" ? i18n::Lang::French : i18n::Lang::English);
  MainWindow w;
  account::BridgeSync::instance();   // live MT4/MT5/cTrader sync runs from launch, not from the first visit
  if (cli.isSet("page") && !w.openPage(cli.value("page"))) qWarning("unknown page %s", qPrintable(cli.value("page")));
  if (cli.isSet("size")) {
    const auto wh = cli.value("size").split('x');
    if (wh.size() == 2) w.resize(wh[0].toInt(), wh[1].toInt());
  } else {
    const QRect avail = w.screen()->availableGeometry();
    w.resize(qMin(1600, avail.width()), qMin(1042, avail.height()));
  }
  w.show();
  if (cli.isSet("options")) QTimer::singleShot(300, &w, [&w] { w.showOptions(); });
  if (cli.isSet("goto"))
    QTimer::singleShot(cli.value("goto-at").toInt(), &w, [&w, id = cli.value("goto")] {
      if (!w.openPage(id)) qWarning("unknown page %s", qPrintable(id));
      qInfo("goto %s", qPrintable(id));
    });

  if (cli.isSet("screenshot")) {
    const QString file = cli.value("screenshot");
    QTimer::singleShot(cli.value("delay").toInt(), &w, [&w, file] {
      QDir().mkpath(QFileInfo(file).absolutePath());
      QPixmap shot = w.grab();
      // dialogs are separate windows: paint the visible ones over the capture
      for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top == &w || !top->isVisible() || !top->inherits("QDialog")) continue;
        QPainter p(&shot);
        p.drawPixmap(top->geometry().topLeft() - w.geometry().topLeft(), top->grab());
      }
      shot.save(file);
      qApp->quit();
    });
  }
  // the audits measure each page alone: no page built and loading behind them
  if (cli.isSet("layout-audit") || cli.isSet("perf-audit")) w.setPreload(false);
  if (cli.isSet("layout-audit")) {
    QList<QSize> sizes;
    for (const QString& s : cli.value("audit-sizes").split(',', Qt::SkipEmptyParts)) {
      const auto wh = s.split('x');
      if (wh.size() == 2) sizes << QSize(wh[0].toInt(), wh[1].toInt());
    }
    layoutaudit::run(&w, cli.value("layout-audit"), sizes, cli.value("audit-pages").split(',', Qt::SkipEmptyParts),
                     cli.value("audit-load").toInt());
  }
  if (cli.isSet("perf-audit"))
    perfaudit::run(&w, cli.value("perf-audit"), cli.value("audit-pages").split(',', Qt::SkipEmptyParts),
                   cli.value("perf-load").toInt(), cli.value("perf-idle").toInt());
  if (cli.isSet("quit-after")) QTimer::singleShot(cli.value("quit-after").toInt(), &app, &QApplication::quit);
  return app.exec();
}
