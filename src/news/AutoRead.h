#pragma once
#include <QString>
#include <QStringList>

// "AUTOMATED READ": rule-based reading of a news text, no language model (News Feed reader; usable by Squawk).
//
//   const news::AutoRead r = news::autoRead(title, plainText);
//   r.points      → up to 5 sentences carrying figures (%, bp, $, "vs" expectations)
//   r.banks       → central banks mentioned ("Fed", "ECB" …), most mentioned first
//   r.countries   → countries / regions mentioned
//   r.symbols     → market::instruments() symbols the text is about ("XAUUSD", "NVDA", "USDJPY"), for live quotes
//   r.tone        → -1 dovish/weak … +1 hawkish/strong, from word counts (0 when no signal); toneWords explains it
namespace news {
struct AutoRead {
  QStringList points;
  QStringList banks;
  QStringList countries;
  QStringList symbols;
  QStringList figures;     // raw figures found ("0.4%", "25 bp", "$24 billion")
  int tone = 0;
  QString toneWords;       // "hawkish words: hike ×3, inflation ×2"
  int words = 0;
  int readMinutes = 0;
};
AutoRead autoRead(const QString& title, const QString& text);
}
