#include "macro/Ocr.h"
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>

namespace macro::ocr {
QList<Line> recognize(const QByteArray& imageData) {
  QList<Line> out;
  if (imageData.isEmpty()) return out;
  @autoreleasepool {
    NSData* data = [NSData dataWithBytes:imageData.constData() length:NSUInteger(imageData.size())];
    VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithData:data options:@{}];
    VNRecognizeTextRequest* req = [[VNRecognizeTextRequest alloc] init];
    req.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
    req.usesLanguageCorrection = NO;
    NSError* err = nil;
    if (![handler performRequests:@[req] error:&err]) return out;
    for (VNRecognizedTextObservation* o in req.results) {
      VNRecognizedText* t = [[o topCandidates:1] firstObject];
      if (!t) continue;
      const CGRect b = o.boundingBox;   // normalized, origin bottom-left
      Line l;
      l.text = QString::fromNSString(t.string);
      l.box = QRectF(b.origin.x, 1.0 - b.origin.y - b.size.height, b.size.width, b.size.height);
      out << l;
    }
  }
  return out;
}
}
