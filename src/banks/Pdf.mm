#include "banks/Pdf.h"
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#import <PDFKit/PDFKit.h>
#include <CoreGraphics/CoreGraphics.h>

namespace banks::pdf {

static CGPDFDocumentRef open(const QString& file) {
  @autoreleasepool {
    NSURL* url = [NSURL fileURLWithPath:file.toNSString()];
    return CGPDFDocumentCreateWithURL((__bridge CFURLRef)url);
  }
}

static QSizeF pageSize(CGPDFPageRef page) {
  const CGRect box = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
  const int rot = ((CGPDFPageGetRotationAngle(page) % 360) + 360) % 360;
  return (rot == 90 || rot == 270) ? QSizeF(box.size.height, box.size.width) : QSizeF(box.size.width, box.size.height);
}

Info info(const QString& file) {
  Info out;
  CGPDFDocumentRef doc = open(file);
  if (!doc) return out;
  const size_t n = CGPDFDocumentGetNumberOfPages(doc);
  for (size_t i = 1; i <= n; ++i) {
    if (CGPDFPageRef p = CGPDFDocumentGetPage(doc, i)) out.sizes << pageSize(p);
    else out.sizes << QSizeF(595, 842);
  }
  out.pages = int(n);
  CGPDFDictionaryRef infoDict = CGPDFDocumentGetInfo(doc);
  CGPDFStringRef t = nullptr;
  if (infoDict && CGPDFDictionaryGetString(infoDict, "Title", &t) && t) {
    CFStringRef s = CGPDFStringCopyTextString(t);
    if (s) { out.title = QString::fromCFString(s); CFRelease(s); }
  }
  CGPDFDocumentRelease(doc);
  return out;
}

QImage render(const QString& file, int pageIndex, double scale, double dpr) {
  CGPDFDocumentRef doc = open(file);
  if (!doc) return {};
  QImage img;
  if (CGPDFPageRef page = CGPDFDocumentGetPage(doc, size_t(pageIndex) + 1)) {
    const QSizeF sz = pageSize(page);
    // the crop box comes from the file: a zero, NaN or huge box would divide by zero below or overflow the int
    // conversion; pages outside 1…16384 px are not drawn
    const double fw = sz.width() * scale * dpr + 0.5, fh = sz.height() * scale * dpr + 0.5;
    if (!(sz.width() > 0 && sz.height() > 0 && fw < 16385 && fh < 16385)) { CGPDFDocumentRelease(doc); return {}; }
    const int w = qMax(1, int(fw)), h = qMax(1, int(fh));
    img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(img.bits(), w, h, 8, img.bytesPerLine(), cs,
                                             uint32_t(kCGImageAlphaPremultipliedFirst) | uint32_t(kCGBitmapByteOrder32Little));
    CGColorSpaceRelease(cs);
    if (ctx) {
      CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
      CGContextSetRenderingIntent(ctx, kCGRenderingIntentDefault);
      const CGRect box = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
      const double s = w / sz.width();
      CGContextScaleCTM(ctx, s, s);
      // The rect has the page's own size: the transform only applies the rotation and the box origin.
      CGContextConcatCTM(ctx, CGPDFPageGetDrawingTransform(page, kCGPDFCropBox, CGRectMake(0, 0, sz.width(), sz.height()), 0, true));
      CGContextClipToRect(ctx, box);
      CGContextDrawPDFPage(ctx, page);
      CGContextRelease(ctx);
    }
    img.setDevicePixelRatio(dpr);
  }
  CGPDFDocumentRelease(doc);
  return img;
}

QString text(const QString& file, int page) {
  @autoreleasepool {
    PDFDocument* doc = [[PDFDocument alloc] initWithURL:[NSURL fileURLWithPath:file.toNSString()]];
    if (!doc) return {};
    if (page < 0) return QString::fromNSString(doc.string ?: @"");
    PDFPage* p = [doc pageAtIndex:NSUInteger(page)];
    return p ? QString::fromNSString(p.string ?: @"") : QString();
  }
}

bool print(const QString& file) {
#if TARGET_OS_IPHONE
  Q_UNUSED(file);
  return false;   // iOS: no print panel from a widgets app yet
#else
  @autoreleasepool {
    PDFDocument* doc = [[PDFDocument alloc] initWithURL:[NSURL fileURLWithPath:file.toNSString()]];
    if (!doc) return false;
    NSPrintOperation* op = [doc printOperationForPrintInfo:[NSPrintInfo sharedPrintInfo]
                                               scalingMode:kPDFPrintPageScaleToFit autoRotate:YES];
    if (!op) return false;
    op.showsPrintPanel = YES;
    op.showsProgressPanel = YES;
    return [op runOperation];
  }
#endif
}
}
