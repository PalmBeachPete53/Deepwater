#pragma once
#include "ui/Theme.h"
#include <QColor>
#include <QPainter>
#include <QString>
#include <QCoreApplication>
#include <QFontMetricsF>

// Tokens measured on Reference/News/squawk.mp4 (docs/reference/News/squawk.md). Where the app palette has the
// same role the palette value is used; the others are the squawk's own warm shades.
namespace wire::squawk {
struct Look {   // QColor members only (look() maps them one by one)
  QColor page{"#030201"}, head{"#0f0a04"}, ticker{"#080603"}, divider{"#1b1806"}, edge{"#6f5b14"};
  QColor sep{"#211d12"}, sepStrong{"#2d2a1e"}, hover{"#1a1002"};
  QColor amber{"#fcd700"}, amberText{"#e5b112"}, amberDim{"#332802"}, amberPill{"#3d2901"};
  QColor time{"#dededc"}, age{"#403e38"}, body{"#bbbab6"}, subtitle{"#5c584f"}, items{"#4f4c43"};
  QColor tabOff{"#78756e"}, tabOffCount{"#4c4940"}, tabHover{"#a09d95"}, tabOn{"#dfbf4d"}, tabOnCount{"#6a5c30"};
  QColor tickText{"#dbd9d8"}, up{"#3ec0d6"}, down{"#e0605a"};
  QColor coin{"#c6aa3c"}, coinCore{"#bb9300"};
  QColor tabs{"#080603"}, rule{"#2c2517"}, tabHoverBg{"#17110a"};
  QColor footer{"#4e4d46"}, livePill{"#332600"}, liveText{"#ceaf49"};
  QColor btnBorder{"#292410"}, btnText{"#767266"};
};
// Deepwater values above; look() returns them mapped to the current theme (rebuilt when the theme changes).
inline const Look& look() {
  static Look mapped; static int rev = -1;
  if (rev != theme::revision()) {
    const Look raw;
    mapped = raw;
    QColor* m = reinterpret_cast<QColor*>(&mapped);
    const QColor* r = reinterpret_cast<const QColor*>(&raw);
    for (size_t i = 0; i < sizeof(Look) / sizeof(QColor); ++i) m[i] = theme::map(r[i]);
    rev = theme::revision();
  }
  return mapped;
}

// width of the footer pill for the current language
inline qreal livePillWidth() {
  return QFontMetricsF(theme::mono(10, QFont::Bold, 0.5)).horizontalAdvance(QCoreApplication::translate("wire", "LIVE")) + 22;
}
// "● LIVE" footer pill (static: the reference does not pulse it).
inline void livePill(QPainter& p, const QRectF& r) {
  const auto& L = look();
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(theme::alpha(L.amberText, 170), 1));
  p.setBrush(L.livePill);
  p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 3, 3);
  p.setPen(Qt::NoPen); p.setBrush(L.amberText);
  p.drawEllipse(QPointF(r.left() + 9, r.center().y()), 2.2, 2.2);
  p.setFont(theme::mono(10, QFont::Bold, 0.5)); p.setPen(L.liveText);
  p.drawText(r.adjusted(15, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("wire", "LIVE"));
  p.restore();
}
}
