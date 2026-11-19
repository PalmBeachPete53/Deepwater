#pragma once
#include <QColor>
#include "ui/Theme.h"
#include "fundamental/Fundamental.h"

// Colours measured on the Fundamental Heatmap reference frames (docs/reference/Market/fundamental-heatmap.md,
// tools/pick, JPEG ±4). Page-specific shades the base palette has no token for, kept in one place.
namespace fundamental {
struct Colors {
  QColor page{"#040302"}, strip{"#120d04"}, stripRule{"#443b24"}, stripText{"#5a574d"}, gold{"#f2d900"},
      goldDot{"#c9bf38"}, badgeBorder{"#6c6229"}, badgeBg{"#1a1503"}, badgeText{"#e8c84a"};
  QColor card{"#0c0601"}, cardBorder{"#3e381d"}, panelBorder{"#413b27"}, bracket{"#8a7d60"}, diamond{"#d4b020"};
  QColor label{"#5a564e"}, label2{"#7d7f6a"}, dim{"#4a463e"}, sub{"#8a877e"}, white{"#fffdf1"}, text{"#e8e6e0"},
      body{"#c8c4bc"};
  QColor cyan{"#40d0e3"}, cyanText{"#4fd0dc"}, cyanScore{"#329ca1"}, red{"#ea837e"}, redScore{"#f08784"};
  QColor accentCyan{"#73cad1"}, accentNeutral{"#dfdfdb"}, accentRed{"#e39a8d"};
  QColor pillBull{"#144e54"}, pillBullBorder{"#1d5557"}, pillNeutral{"#292825"}, pillNeutralText{"#cdcdcd"},
      pillBear{"#552924"}, pillBearText{"#c16762"};
  QColor selGold{"#bbad7c"}, selIdle{"#6a6b66"}, track{"#1b1b0f"};
  QColor hikeA{"#148996"}, hikeB{"#26c4d5"}, hikeText{"#c8f4f8"}, holdBg{"#2a2826"}, holdText{"#e0ddd6"},
      cutA{"#8a3a30"}, cutB{"#c8584c"}, cutText{"#fbe0dc"}, meetBorder{"#1a150a"}, meetGold{"#f0d040"};
  QColor gaugeTeal{"#0e4b4b"}, gaugeOlive{"#4f4a0b"}, gaugeBrown{"#663b2d"}, gaugeRed{"#6c3c36"}, gaugeLabel{"#4a463e"},
      headline{"#fcda0b"}, legendGold{"#eed75d"};
  QColor segActive{"#ffe611"};
  QColor chartBg{"#060502"}, chartBorder{"#14120b"}, chartTitle{"#5a564e"}, chartSub{"#3f3d36"}, axis{"#6f6c63"},
      grid{"#110f08"}, zero{"#2a2618"}, policyLine{"#3cc3d4"}, policyValue{"#83cedb"}, policyFill{"#0b2120"},
      inflLine{"#efd66e"}, inflAbove{"#3f1e14"}, inflBelow{"#0b2120"}, target{"#d0d0c8"}, inflRed{"#e06050"},
      inflGold{"#f0c840"};
  QColor tipBg{"#0d0601"}, tipBorder{"#5d4b2a"}, tipDate{"#8a857c"}, tipCyan{"#2ec8d0"}, crosshair{"#c8c4bc"};
  QColor readBg{"#151004"}, readBorder{"#2a2010"}, readRule{"#3a3020"}, badgeGoldBg{"#533c00"}, badgeGold{"#f0c040"};
  QColor tabActiveBg{"#332901"}, tabActive{"#f2e6b3"}, baseCode{"#f2d060"}, rowRule{"#1d180d"}, better{"#20c8dc"},
      verdictBg{"#0a1512"}, verdictBorder{"#364d44"}, verdictLabel{"#4a5a52"}, verdictCyan{"#3ad0e0"},
      verdictNeutral{"#e8e6e0"};
  QColor cellCurrency{"#0f0a02"}, cellSelected{"#362d05"}, cellSelBorder{"#938243"}, rowSelBorder{"#c8a840"},
      scoreDim{"#4e4428"}, biasHeader{"#f0c840"};
};
// Measured Deepwater colours (static constants) mapped onto the current theme at each call: the result is a
// temporary, never stored, so a theme change is picked up on the next paint. theme::map() is cached and cheap.
inline Colors col() {
  static const Colors base;
  if (theme::isDefault()) return base;
  static_assert(sizeof(Colors) % sizeof(QColor) == 0, "Colors must hold QColor members only");
  Colors c = base;
  QColor* p = reinterpret_cast<QColor*>(&c);
  for (size_t i = 0; i < sizeof(Colors) / sizeof(QColor); ++i) p[i] = theme::map(p[i]);
  return c;
}

// Cell fill / border / word / sub-line colours for a heatmap shade.
struct CellLook { QColor fill, border, word, sub; };
inline CellLook lookBase(fundamental::Shade s);
// Strong cells are filled with the theme's up/down colour: in other themes the mapped light word colour can sit on
// a fill of the same hue, so their text becomes white (dark themes) or the theme's text colour (light themes).
inline CellLook look(fundamental::Shade s) {
  CellLook l = lookBase(s);
  if (!theme::isDefault() && (s == fundamental::Shade::StrongPos || s == fundamental::Shade::StrongNeg)) {
    l.word = theme::isDark() ? QColor(255, 255, 255) : theme::roles().text;
    l.sub = theme::alpha(l.word, 200);
  }
  return l;
}
inline CellLook lookBase(fundamental::Shade s) {
  using fundamental::Shade;
  switch (s) {
    case Shade::StrongPos: return {theme::map("#1b8a95"), theme::map("#4fa7a4"), theme::map("#9cecf4"), theme::map("#5ccad6")};
    case Shade::WeakPos: return {theme::map("#0f525a"), theme::map("#226a6e"), theme::map("#7fd6df"), theme::map("#4aa8b2")};
    case Shade::Neutral: return {theme::map("#282724"), theme::map("#3a3934"), theme::map("#e8e6e0"), theme::map("#a8a59e")};
    case Shade::WeakNeg: return {theme::map("#5e2d27"), theme::map("#7a423a"), theme::map("#f08070"), theme::map("#c86a5e")};
    case Shade::StrongNeg: return {theme::map("#8a453c"), theme::map("#a8625a"), theme::map("#ffb0a2"), theme::map("#f08070")};
    default: return {theme::map("#1a1813"), theme::map("#2a2820"), theme::map("#8a877e"), theme::map("#5a564e")};
  }
}
}
