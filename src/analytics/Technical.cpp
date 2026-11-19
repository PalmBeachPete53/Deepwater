#include "analytics/Technical.h"
#include "market/Instruments.h"
#include <QHash>
#include <QCoreApplication>
#include <algorithm>

namespace analytics {

double volumeRatio(const QVector<Bar>& b, qint64 today) {
  int k = b.size() - 1;
  while (k >= 0 && (b[k].t / 86400 >= today || !(b[k].v > 0))) --k;   // last complete day with volume
  if (k < 15) return NAN;
  double s = 0; int n = 0;
  for (int j = std::max(0, k - 20); j < k; ++j) if (b[j].v > 0) { s += b[j].v; ++n; }
  if (n < 15 || s <= 0) return NAN;
  return b[k].v / (s / n);
}

TechRead technicalRead(const QVector<Bar>& d, const QVector<VolumeInput>& volume, bool crypto, qint64 today) {
  TechRead r;
  if (d.size() < 60) { r.error = QCoreApplication::translate("technical", "not enough history"); return r; }
  const auto c = closes(d);
  r.last = d.last().c; r.lastTime = d.last().t;

  // trend
  const auto e20 = ema(c, 20), e50 = ema(c, 50);
  r.ema20 = last(e20); r.ema50 = last(e50);
  const auto a = atr(d, 14);
  QVector<double> atrp(d.size(), NAN);
  for (int i = 0; i < d.size(); ++i) if (finite(a[i]) && c[i] > 0) atrp[i] = a[i] / c[i] * 100.0;
  r.atrPct = last(atrp);
  if (finite(r.ema20) && finite(r.ema50) && r.ema50 != 0) {
    r.trendPct = (r.ema20 - r.ema50) / r.ema50 * 100.0;
    r.trend = r.trendPct >= 0.10 ? Lean::Bull : r.trendPct <= -0.10 ? Lean::Bear : Lean::Neutral;
    r.trendIntensity = finite(r.atrPct) && r.atrPct > 0 ? std::tanh(std::abs(r.trendPct) / (2.0 * r.atrPct)) : 0;
  }

  // momentum
  r.rsi = last(rsi(c, 14));
  r.roc10 = last(roc(c, 10));
  if (finite(r.rsi)) r.momentum = r.rsi >= 70 ? Momentum::Overbought : r.rsi <= 30 ? Momentum::Oversold : Momentum::Neutral;

  // volatility
  const int n = d.size();
  r.atrPctile = percentileRank(atrp, n - 252, n, r.atrPct);
  if (finite(r.atrPctile)) r.vol = r.atrPctile >= 80 ? Level3::High : r.atrPctile <= 20 ? Level3::Low : Level3::Normal;
  r.volAnn = last(realizedVol(c, 20, crypto ? 365.0 : 252.0));

  // volume
  QStringList tick; double sum = 0; int cnt = 0;
  for (const auto& v : volume) {
    const double x = volumeRatio(v.bars, today);
    if (finite(x)) { sum += x; ++cnt; tick << v.ticker; }
  }
  if (cnt > 0 && cnt == volume.size()) {
    r.volRatio = sum / cnt;
    r.volTicker = tick.join('+');
    r.volume = r.volRatio >= 1.5 ? VolumeRead::Above : r.volRatio < 0.75 ? VolumeRead::Below : VolumeRead::Average;
  }

  // structure: last two 5-bar swing highs and lows
  const auto sw = swings(d, 2);
  QVector<double> hs, ls;
  for (const auto& s : sw) (s.high ? hs : ls) << s.price;
  if (hs.size() >= 2 && ls.size() >= 2) {
    const bool hh = hs.last() > hs[hs.size() - 2], hl = ls.last() > ls[ls.size() - 2];
    const bool lh = hs.last() < hs[hs.size() - 2], ll = ls.last() < ls[ls.size() - 2];
    r.structure = hh && hl ? Lean::Bull : lh && ll ? Lean::Bear : Lean::Neutral;
  }

  r.liq = liquidityRead(d);

  // radar: trend, liquidity, structure, volume, volatility, momentum
  const double sgn = r.trend == Lean::Bull ? 1 : r.trend == Lean::Bear ? -1 : (finite(r.trendPct) && r.trendPct < 0 ? -1 : 1);
  r.radar[0] = 50 + 50 * sgn * r.trendIntensity;
  r.radar[1] = r.liq.state == LiqRead::None ? 0 : r.liq.closeness;
  r.radar[2] = r.structure == Lean::Bull ? 85 : r.structure == Lean::Bear ? 15 : 50;
  r.radar[3] = finite(r.volRatio) ? std::clamp(50 * r.volRatio, 0.0, 100.0) : 50;
  r.radar[4] = finite(r.atrPctile) ? r.atrPctile : 50;
  r.radar[5] = finite(r.rsi) ? r.rsi : 50;
  for (double& x : r.radar) x = std::clamp(x, 0.0, 100.0);
  r.ok = true;
  return r;
}

static const QHash<QString, QString>& legEtf() {
  static const QHash<QString, QString> m{{"EUR", "FXE"}, {"GBP", "FXB"}, {"JPY", "FXY"}, {"CHF", "FXF"},
                                         {"CAD", "FXC"}, {"AUD", "FXA"}, {"USD", "UUP"}};
  return m;
}
// No proxy may share its ticker with an app symbol (market::candles() maps app symbols to Yahoo): the Teucrium
// CORN ETF is therefore not used, corn reads its own futures volume (ZC=F).
static const QHash<QString, QString>& symbolProxy() {
  static const QHash<QString, QString> m{
    {"DXY", "UUP"}, {"SPX", "SPY"}, {"NDX", "QQQ"}, {"DOW", "DIA"}, {"RUT", "IWM"},
    {"XAUUSD", "GLD"}, {"XAGUSD", "SLV"}, {"XPTUSD", "PPLT"}, {"XPDUSD", "PALL"}, {"COPPER", "CPER"},
    {"WTI", "USO"}, {"BRENT", "BNO"}, {"NATGAS", "UNG"}, {"GASOLINE", "UGA"},
    {"WHEAT", "WEAT"}, {"SOYBEAN", "SOYB"}, {"SUGAR", "CANE"}};
  return m;
}

QStringList volumeSources(const QString& sym) {
  const auto* ins = market::find(sym);
  if (!ins) return {"-"};
  if (ins->category == market::Category::Forex) {
    const auto& m = legEtf();
    const QString b = ins->base, q = ins->quote;
    const bool hb = m.contains(b) && b != "USD", hq = m.contains(q) && q != "USD";
    if (hb && hq) return {m[b], m[q]};               // cross: both legs (GBP/CHF → FXB+FXF)
    if (hb) return {m[b]};                           // EUR/USD → FXE, GBP/NZD → FXB
    if (hq) return {m[q]};                           // USD/JPY → FXY
    return {"UUP"};                                  // USD vs a currency without ETF
  }
  if (auto it = symbolProxy().constFind(ins->symbol); it != symbolProxy().cend()) return {*it};
  if (ins->category == market::Category::Bond || ins->symbol == "VIX") return {"-"};
  return {};
}

QStringList proxyTickers() {
  QStringList out;
  for (const auto& v : legEtf()) if (!out.contains(v)) out << v;
  for (const auto& v : symbolProxy()) if (!out.contains(v)) out << v;
  std::sort(out.begin(), out.end());
  return out;
}
}
