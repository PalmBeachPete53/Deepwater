#pragma once
#include <QImage>
#include <QList>
#include <QSizeF>
#include <QString>

// PDF reading without QtPdf (not installed): CoreGraphics renders pages to QImages, PDFKit extracts text.
// macOS only (Pdf.mm); on other platforms every call returns empty results.
//
//   banks::pdf::Info i = banks::pdf::info(file);          // page count and page sizes in points
//   QImage img = banks::pdf::render(file, 0, 1.5, 2.0);   // page 0 at 150 %, device pixel ratio 2
//   QString t = banks::pdf::text(file);                   // whole text (PDFKit)
// render() opens its own document, so it may run on a worker thread (QtConcurrent).
namespace banks::pdf {
struct Info {
  int pages = 0;
  QList<QSizeF> sizes;   // points (1/72 in), rotation applied
  QString title;         // document title metadata, may be empty
  bool ok() const { return pages > 0; }
};
Info info(const QString& file);
QImage render(const QString& file, int page, double scale, double dpr);
QString text(const QString& file, int page = -1);
// Opens the system print panel for the file (PDFKit print operation, scaled to fit). GUI thread only.
bool print(const QString& file);
}
