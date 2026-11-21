// Makes the iOS app icon (ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png) from the Deepwater logo
// (resources/icon/deepwater.svg). The logo is drawn on the macOS icon grid: a rounded 824 px tile inside 100 px
// margins. iOS wants a full square, opaque, with no rounded corners (it applies its own mask): the tile's square is
// kept (viewBox 100 100 824 824), its gradient filled to the corners, its hairline edge left out, and the result is
// rendered at 1024 x 1024 without alpha.
//   cmake -S tools/ios_icon -B <dir> -DCMAKE_PREFIX_PATH=~/Qt/6.12.0/macos && cmake --build <dir>
//   <dir>/ios_icon resources/icon/deepwater.svg ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png
#include <QGuiApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <cstdio>

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  if (argc != 3) { std::fprintf(stderr, "usage: ios_icon <logo.svg> <out.png>\n"); return 2; }
  QFile f(QString::fromLocal8Bit(argv[1]));
  if (!f.open(QIODevice::ReadOnly)) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  QString svg = QString::fromUtf8(f.readAll());
  // the tile: square, to the corners; its hairline edge: gone
  svg.replace(QRegularExpression(R"re(<rect x="100" y="100" width="824" height="824" rx="[0-9.]+" fill="url\(#tile\)"/>)re"),
              R"re(<rect x="0" y="0" width="1024" height="1024" fill="url(#tile)"/>)re");
  svg.replace(QRegularExpression(R"re(<rect x="101.5"[^>]*stroke-opacity[^>]*/>)re"), QString());
  svg.replace(R"re(viewBox="0 0 1024 1024")re", R"re(viewBox="100 100 824 824")re");
  QSvgRenderer r(svg.toUtf8());
  if (!r.isValid()) { std::fprintf(stderr, "invalid svg\n"); return 1; }
  QImage img(1024, 1024, QImage::Format_RGB32);
  img.fill(QColor("#0a0702"));
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  r.render(&p, QRectF(0, 0, 1024, 1024));
  p.end();
  if (!img.convertToFormat(QImage::Format_RGB888).save(QString::fromLocal8Bit(argv[2]), "PNG")) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
  return 0;
}
