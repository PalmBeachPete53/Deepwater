#pragma once
#include <QColor>
#include "ui/Theme.h"

// Colours measured on the Seasonality reference frames (docs/reference/Market/seasonality.md, tools/pick, JPEG ±4).
// The base palette (theme::pal()) has no tokens for these page-specific shades (violet period pill, teal band,
// volatility states…), so they live here, in one place, instead of literals spread through the painters.
namespace seasonality {
struct Colors {
  QColor page{"#040302"}, strip{"#0f0a03"}, stripRule{"#2d2a1a"}, toolRule{"#2b2814"};
  QColor btnBorder{"#251f12"}, btnBg{"#0f0a03"}, btnText{"#cbcab4"}, open{"#3ec6d8"};
  QColor segActiveBg{"#00324a"}, segActiveText{"#3cc8d8"}, ring{"#c9b070"}, ringBright{"#d4b22a"};
  QColor menuBg{"#110b03"}, menuBorder{"#28281a"}, menuSel{"#352b05"}, menuSelSoft{"#2a2010"}, searchFocus{"#d4b020"};
  QColor title{"#e8e6e0"}, sub{"#8a877e"}, dim{"#6a665e"}, hint{"#62605a"}, label{"#696862"}, caption{"#96938d"};
  QColor gold{"#f2d632"}, sectionRule{"#1e1d17"};
  QColor card{"#080603"}, cardBorder{"#232217"}, rowRule{"#242418"};
  QColor pillBg{"#1f1741"}, pillText{"#9490a9"};
  QColor bandOdd{"#0f0d06"}, grid{"#14120a"}, zero{"#656345"};
  QColor cyan{"#3bc8de"}, cyanHi{"#6ec9d4"}, fillTop{"#265b63"}, fillBottom{"#101206"};
  QColor today{"#4f706e"}, todayText{"#406b6c"}, crosshair{"#8b8b83"}, tipBg{"#0f0a03"}, tipBorder{"#2a2518"};
  QColor tipDate{"#a8a49c"};
  QColor selFill{"#132827"}, selEdge{"#425e59"}, badgeBg{"#000f11"}, badgeBorder{"#1c3c40"};
  QColor badgeRedBg{"#2a0a05"}, badgeRedBorder{"#5a2620"}, clearText{"#afada2"};
  QColor up{"#1acde1"}, down{"#e4665c"}, valUp{"#2dd5ee"}, valDown{"#e9605b"};
  QColor barZero{"#3e3a20"}, barGrid{"#27261b"}, barLabel{"#bdbcb8"};
  QColor pieRedTop{"#f96c6b"}, pieRedSide{"#5a2220"}, pieCyanTop{"#1cd5ec"}, pieCyanSide{"#05474e"};
  QColor volBg{"#0f0805"}, volBorder{"#2c2617"}, volText{"#737069"};
  QColor volYellow{"#f8da3c"}, volYellowPillBg{"#3a2d00"}, volYellowPill{"#c0a030"};
  QColor volCyan{"#26ddee"}, volCyanPillBg{"#013130"}, volCyanPill{"#4e7174"};
  QColor volRed{"#e97065"}, volRedPillBg{"#1a0805"}, volRedPill{"#9c4a40"};
  QColor tileBg{"#040302"}, tileBorder{"#252118"}, tileLabel{"#6f6c66"}, tileUp{"#5dc0d0"}, tileDown{"#e05a50"},
      tileNeutral{"#b8b6b0"};
  QColor compare{"#e3c23a"}, actual{"#e8e6e0"};
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
}
