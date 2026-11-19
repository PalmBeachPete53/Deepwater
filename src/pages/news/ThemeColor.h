#pragma once
#include <QColor>
#include <QRegularExpression>
#include <QString>
#include "ui/Theme.h"

namespace news::pages {
// A measured Deepwater colour, projected onto the current theme each time it is used (never cached mapped):
//   const Dw kRowBg{"#040302"};   p.fillRect(r, kRowBg);   // → theme::map("#040302")
struct Dw {
  const char* hex;
  operator QColor() const { return theme::map(hex); }
  QColor c() const { return theme::map(hex); }
};
// A style sheet written in Deepwater colours with every #rrggbb projected onto the theme.
inline QString mapQss(QString qss) {
  static const QRegularExpression hex("#[0-9a-fA-F]{6}\\b");
  QString out; qsizetype last = 0;
  for (auto it = hex.globalMatch(qss); it.hasNext();) {
    const auto m = it.next();
    out += qss.mid(last, m.capturedStart() - last) + theme::map(QColor(m.captured(0))).name(QColor::HexArgb);
    last = m.capturedEnd();
  }
  return out + qss.mid(last);
}
}
