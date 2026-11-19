#pragma once
#include <QColor>
#include <QBrush>
#include <QRegularExpression>
#include <QString>
#include "ui/Theme.h"
#include <QWidget>

// Colours measured on the Bank Research reference frames (docs/reference/News/bank-research.md). Kept in one
// place so a theme mapping can be applied here later.
namespace banks {
// A Deepwater colour constant that converts to the current theme's colour where it is used (theme::map at paint
// time, so nothing mapped outlives a theme change). Identity for the Deepwater theme.
struct DwColor {
  const char* hex;
  QColor dw() const { return QColor(hex); }
  QColor get() const { return theme::map(hex); }
  operator QColor() const { return get(); }
  QString name() const { return get().name(); }
};
// Style sheets / rich text written with Deepwater colours: every #rrggbb is replaced by its themed colour.
inline QString mapHex(QString text) {
  if (theme::isDefault()) return text;
  static const QRegularExpression re("#[0-9a-fA-F]{6}\\b");
  QString out; qsizetype last = 0;
  for (auto it = re.globalMatch(text); it.hasNext();) {
    const auto m = it.next();
    out += text.mid(last, m.capturedStart() - last) + theme::map(QColor(m.captured(0))).name();
    last = m.capturedEnd();
  }
  return out + text.mid(last);
}
// Text drawn on a filled colour (quarter pills, price tags): the Deepwater ink in Deepwater, otherwise black or
// white by contrast with the fill (themes move the fills' lightness).
inline QColor inkOn(const QColor& fill, const char* deepwaterInk) {
  if (theme::isDefault()) return QColor(deepwaterInk);
  const double l = 0.2126 * fill.redF() + 0.7152 * fill.greenF() + 0.0722 * fill.blueF();
  return l > 0.55 ? QColor(18, 16, 12) : QColor(250, 250, 248);
}
// Categorical colours (quarter series, the RANGE yellow): theme::map() can fold them onto the theme's accent (e.g.
// RANGE yellow → Abyss teal = the LONG colour), so other themes keep the hue and only fit the lightness.
inline QColor seriesColour(const char* dw, bool text = false) {
  if (theme::isDefault()) return QColor(dw);
  QColor c(dw);
  float h, sat, l, a; c.getHslF(&h, &sat, &l, &a);
  if (theme::isDark()) l = text ? qMax(l, 0.70f) : qBound(0.50f, l, 0.62f);
  else { l = text ? 0.36f : 0.46f; sat = qMin(1.0f, sat * 1.1f); }
  return QColor::fromHslF(h, sat, l, a);
}
inline void setQss(QWidget* w, const QString& deepwaterQss) { w->setStyleSheet(mapHex(deepwaterQss)); }
}
namespace banks::res {
namespace look {
inline constexpr DwColor page{"#050402"};
inline constexpr DwColor strip{"#120d04"};
inline constexpr DwColor stripBorder{"#3e381d"};
inline constexpr DwColor leftBg{"#050402"};
inline constexpr DwColor rightBg{"#080603"};
inline constexpr DwColor colBorder{"#1c1a17"};
inline constexpr DwColor band{"#231b04"};
inline constexpr DwColor bandBorder{"#3a3214"};
inline constexpr DwColor dim{"#5f5b51"};
inline constexpr DwColor dimmer{"#4f4d49"};
inline constexpr DwColor footer{"#4b4a45"};
inline constexpr DwColor cursor{"#f5e392"};
inline constexpr DwColor dot{"#948954"};
inline constexpr DwColor gold{"#d1c497"};
inline constexpr DwColor goldTitle{"#f9e8ab"};
inline constexpr DwColor goldBright{"#f7e069"};
inline constexpr DwColor groupLabel{"#e1d5a6"};
inline constexpr DwColor white{"#fefdf8"};
inline constexpr DwColor age{"#54534c"};
inline constexpr DwColor rowSep{"#171511"};
inline constexpr DwColor rowSel{"#2a2104"};
inline constexpr DwColor selBar{"#f5e093"};
inline constexpr DwColor tabInactive{"#625e49"};
inline constexpr DwColor badgeText{"#eddc98"};
inline constexpr DwColor badgeBg{"#4a3f05"};
inline constexpr DwColor badgeBorder{"#736935"};
inline constexpr DwColor chipBg{"#483e01"};
inline constexpr DwColor chipBorder{"#6b5d1a"};
inline constexpr DwColor chipText{"#f3e4a5"};
inline constexpr DwColor chipOffBg{"#110f0b"};
inline constexpr DwColor chipOffText{"#54514c"};
inline constexpr DwColor regIndex{"#535249"};
inline constexpr DwColor regValue{"#828073"};
inline constexpr DwColor regSep{"#1b170c"};
inline constexpr DwColor regHeader{"#6e6958"};
inline constexpr DwColor dropBg{"#040501"};
inline constexpr DwColor dropBorder{"#18170d"};
inline constexpr DwColor panelBg{"#070503"};
inline constexpr DwColor panelBorder{"#3e3b2c"};
inline constexpr DwColor btnText{"#85827b"};
inline constexpr DwColor btnBorder{"#2a2820"};
inline constexpr DwColor pdfToolbar{"#4a4a49"};
inline constexpr DwColor pdfToolbarHi{"#4d4d4d"};
inline constexpr DwColor pdfGutter{"#2d3130"};
inline constexpr DwColor pdfText{"#f1f1f1"};
}
}
