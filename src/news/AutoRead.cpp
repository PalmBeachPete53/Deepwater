#include <QCoreApplication>
#include "news/AutoRead.h"
#include "market/Instruments.h"
#include <QRegularExpression>
#include <QHash>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace news {
namespace {
using RE = QRegularExpression;
struct Rule { QString name; RE re; };

const QList<Rule>& bankRules() {
  static const auto ci = RE::CaseInsensitiveOption;
  static const QList<Rule> r{
    {"Fed", RE(R"(\b(fed|feds|fomc|federal reserve|powell|warsh)\b)", ci)},
    {"ECB", RE(R"(\b(ecb|european central bank|lagarde)\b)", ci)},
    {"BoE", RE(R"(\b(boe|bank of england|bailey)\b)", ci)},
    {"BoJ", RE(R"(\b(boj|bank of japan|ueda)\b)", ci)},
    {"SNB", RE(R"(\b(snb|swiss national bank)\b)", ci)},
    {"BoC", RE(R"(\b(boc|bank of canada|macklem)\b)", ci)},
    {"RBA", RE(R"(\b(rba|reserve bank of australia|bullock)\b)", ci)},
    {"RBNZ", RE(R"(\b(rbnz|reserve bank of new zealand)\b)", ci)},
    {"PBoC", RE(R"(\b(pboc|people's bank of china)\b)", ci)},
  };
  return r;
}
const QList<Rule>& countryRules() {
  static const auto ci = RE::CaseInsensitiveOption;
  static const QList<Rule> r{
    {QT_TRANSLATE_NOOP("news", "United States"), RE(R"(\b(US|U\.S\.|[Uu]nited [Ss]tates|American|America|Washington|Wall Street)\b)")},
    {QT_TRANSLATE_NOOP("news", "Euro area"), RE(R"(\b(euro ?zone|euro area|eurozone)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "Germany"), RE(R"(\b(germany|german)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "France"), RE(R"(\b(france|french)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "Italy"), RE(R"(\b(italy|italian)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "United Kingdom"), RE(R"(\b(uk|u\.k\.|britain|british|england)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "Japan"), RE(R"(\b(japan|japanese|tokyo)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "China"), RE(R"(\b(china|chinese|beijing)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "Canada"), RE(R"(\b(canada|canadian)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "Australia"), RE(R"(\b(australia|australian)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "New Zealand"), RE(R"(\b(new zealand)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "Switzerland"), RE(R"(\b(switzerland|swiss)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "Russia"), RE(R"(\b(russia|russian|kremlin)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "Ukraine"), RE(R"(\b(ukraine|ukrainian)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "Iran"), RE(R"(\b(iran|iranian|tehran)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "Saudi Arabia"), RE(R"(\b(saudi)\b)", ci)},
    {QT_TRANSLATE_NOOP("news", "India"), RE(R"(\b(india|indian)\b)", ci)}, {QT_TRANSLATE_NOOP("news", "Mexico"), RE(R"(\b(mexico|mexican)\b)", ci)},
  };
  return r;
}
// Words → app instrument symbols (market::instruments()).
const QList<Rule>& symbolRules() {
  static const auto ci = RE::CaseInsensitiveOption;
  static const QList<Rule> r{
    {"XAUUSD", RE(R"(\b(gold|bullion|xau)\b)", ci)}, {"XAGUSD", RE(R"(\bsilver\b)", ci)},
    {"WTI", RE(R"(\b(oil|crude|wti|opec)\b)", ci)}, {"BRENT", RE(R"(\bbrent\b)", ci)},
    {"NATGAS", RE(R"(\bnatural gas\b)", ci)}, {"COPPER", RE(R"(\bcopper\b)", ci)},
    {"DXY", RE(R"(\b(dollar index|dxy|greenback|the dollar|us dollar|u\.s\. dollar)\b)", ci)},
    {"EURUSD", RE(R"(\b(eur/usd|eurusd|the euro)\b)", ci)}, {"USDJPY", RE(R"(\b(usd/jpy|usdjpy|the yen|japanese yen)\b)", ci)},
    {"GBPUSD", RE(R"(\b(gbp/usd|gbpusd|cable|sterling|the pound)\b)", ci)},
    {"AUDUSD", RE(R"(\b(aud/usd|audusd|aussie dollar|australian dollar)\b)", ci)},
    {"USDCAD", RE(R"(\b(usd/cad|usdcad|loonie|canadian dollar)\b)", ci)},
    {"USDCHF", RE(R"(\b(usd/chf|usdchf|swiss franc)\b)", ci)}, {"NZDUSD", RE(R"(\b(nzd/usd|kiwi|new zealand dollar)\b)", ci)},
    {"SPX", RE(R"(\b(s&p 500|s&p500|s&p|spx|wall street)\b)", ci)}, {"NDX", RE(R"(\b(nasdaq 100|nasdaq-100|ndx|nasdaq)\b)", ci)},
    {"DOW", RE(R"(\b(dow jones|the dow|djia)\b)", ci)}, {"VIX", RE(R"(\b(vix|volatility index)\b)", ci)},
    {"US10Y", RE(R"(\b(10-year|ten-year|treasury yields?|treasuries|bond yields?)\b)", ci)},
    {"US13W", RE(R"(\b(t-bills?|bill auction)\b)", ci)},
    {"DAX", RE(R"(\bdax\b)", ci)}, {"FTSE", RE(R"(\bftse\b)", ci)}, {"NIKKEI", RE(R"(\bnikkei\b)", ci)},
    {"STOXX50", RE(R"(\b(euro stoxx|stoxx 50|european stocks)\b)", ci)}, {"HSI", RE(R"(\bhang seng\b)", ci)},
    {"BTCUSD", RE(R"(\b(bitcoin|btc)\b)", ci)}, {"ETHUSD", RE(R"(\b(ethereum|ether)\b)", ci)},
  };
  return r;
}
}

AutoRead autoRead(const QString& title, const QString& text) {
  AutoRead out;
  const QString all = title + ". " + text;
  static const RE wsplit(R"(\s+)");
  out.words = int(text.split(wsplit, Qt::SkipEmptyParts).size());
  out.readMinutes = std::max(1, (out.words + 220) / 230);

  auto rank = [&](const QList<Rule>& rules, int max) {
    QList<QPair<int, QString>> hits;
    for (const auto& r : rules) {
      int n = 0; for (auto it = r.re.globalMatch(all); it.hasNext(); it.next()) ++n;
      if (n) hits << qMakePair(n, r.name);
    }
    std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    QStringList o; for (const auto& h : hits) if (o.size() < max) o << h.second;
    return o;
  };
  out.banks = rank(bankRules(), 4);
  out.countries = rank(countryRules(), 5);
  QStringList syms = rank(symbolRules(), 6);
  // explicit tickers and company names from the catalogue
  for (const auto& in : market::instruments()) {
    if (in.category != market::Category::Stock || syms.size() >= 8) continue;
    const RE tick("(\\(|\\$|:)" + RE::escape(in.symbol) + "\\b");
    // full company name, or its first word when it is distinctive ("JPMorgan", "Exxon", "Goldman")
    static const QSet<QString> firstOk{"JPMorgan", "Exxon", "Goldman", "Procter", "Coca-Cola", "Johnson"};
    const QString first = in.name.section(' ', 0, 0);
    const bool one = !in.name.contains(' ');
    const RE name("\\b(" + RE::escape(in.name) + (firstOk.contains(first) ? "|" + RE::escape(first) : QString()) + ")\\b",
                  one && in.name.size() >= 5 ? RE::CaseInsensitiveOption : RE::NoPatternOption);
    if (tick.match(all).hasMatch() || (in.name.size() > 2 && name.match(all).hasMatch())) if (!syms.contains(in.symbol)) syms << in.symbol;
  }
  if (out.banks.contains("Fed") && !syms.contains("DXY")) syms << "DXY";
  if (out.banks.contains("ECB") && !syms.contains("EURUSD")) syms << "EURUSD";
  if (out.banks.contains("BoJ") && !syms.contains("USDJPY")) syms << "USDJPY";
  if (out.banks.contains("BoE") && !syms.contains("GBPUSD")) syms << "GBPUSD";
  out.symbols = syms.mid(0, 8);

  // figures and the sentences that carry them
  static const RE fig(R"((?:[$€£¥]\s?\d[\d,.]*\s?(?:billion|million|trillion|bn|mn|[kmbt]\b)?)|(?:[-+]?\d[\d,.]*\s?(?:%|percent|bps?\b|basis points?|pips?\b))|(?:\b\d[\d,.]*\s?(?:billion|million|trillion)\b))",
                    RE::CaseInsensitiveOption);
  static const RE sentence(R"((?<=[.!?])\s+(?=[A-Z“"']))");
  QSet<QString> seen;
  for (auto it = fig.globalMatch(all); it.hasNext();) {
    const QString f = it.next().captured(0).trimmed();
    if (!seen.contains(f) && out.figures.size() < 12) { seen.insert(f); out.figures << f; }
  }
  static const RE expect(R"(\b(vs|versus|expected|estimate|forecast|prior|consensus)\b)", RE::CaseInsensitiveOption);
  QList<QPair<int, QString>> scored;
  const QStringList sents = text.split(sentence, Qt::SkipEmptyParts);
  for (int i = 0; i < sents.size(); ++i) {
    QString s = sents[i].simplified();
    if (s.size() < 30) continue;
    int sc = 0; for (auto it = fig.globalMatch(s); it.hasNext(); it.next()) sc += 2;
    if (expect.match(s).hasMatch()) sc += 2;
    for (const auto& r : bankRules()) if (r.re.match(s).hasMatch()) { sc += 1; break; }
    if (sc < 2) continue;
    if (s.size() > 190) s = s.left(187).section(' ', 0, -2) + "…";
    scored << qMakePair(sc * 1000 - i, s);   // higher score first, then earlier in the text
  }
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (const auto& s : scored) if (out.points.size() < 5 && !out.points.contains(s.second)) out.points << s.second;

  // tone from word counts
  static const QList<QPair<QString, int>> words{
    {"hike", 1}, {"hikes", 1}, {"hawkish", 1}, {"tighten", 1}, {"tightening", 1}, {"inflation", 1}, {"strong", 1},
    {"beat", 1}, {"beats", 1}, {"surge", 1}, {"surges", 1}, {"higher", 1}, {"rally", 1},
    {"cut", -1}, {"cuts", -1}, {"dovish", -1}, {"easing", -1}, {"weak", -1}, {"weaker", -1}, {"slowdown", -1},
    {"recession", -1}, {"miss", -1}, {"misses", -1}, {"falls", -1}, {"drop", -1}, {"lower", -1}, {"slump", -1}};
  QMap<QString, int> cnt; int tone = 0;
  const QString low = all.toLower();
  for (const auto& [w, s] : words) {
    const RE re("\\b" + w + "\\b");
    int n = 0; for (auto it = re.globalMatch(low); it.hasNext(); it.next()) ++n;
    if (n) { cnt[w] = n * s; tone += n * s; }
  }
  out.tone = tone > 1 ? 1 : tone < -1 ? -1 : 0;
  QStringList parts;
  QList<QPair<int, QString>> top;
  for (auto it = cnt.cbegin(); it != cnt.cend(); ++it) top << qMakePair(std::abs(it.value()), it.key());
  std::sort(top.begin(), top.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (int i = 0; i < top.size() && i < 4; ++i) parts << QString("%1 ×%2").arg(top[i].second).arg(top[i].first);
  out.toneWords = parts.join(", ");
  return out;
}
}
