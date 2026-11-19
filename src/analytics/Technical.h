#pragma once
#include "analytics/Indicators.h"
#include "analytics/Liquidity.h"
#include <QStringList>

// Technical reads of one instrument from its daily candles, computed by Deepwater (Technical Heatmap page).
//
// RULES (the page footnote repeats them; keep both in sync):
//  TREND      gap = (EMA20 − EMA50) / EMA50 × 100 on daily closes. gap ≥ +0.10 % BULLISH, ≤ −0.10 % BEARISH,
//             otherwise NEUTRAL. Intensity = tanh(|gap| / (2 × ATR14%)).
//  MOMENTUM   RSI(14) (Wilder). ≥ 70 OVERBOUGHT, ≤ 30 OVERSOLD, otherwise NEUTRAL. ROC(10) is given as context.
//  VOLATILITY state from the percentile of today's ATR14% among its last 252 daily values: ≥ 80th HIGH, ≤ 20th
//             LOW, otherwise NORMAL ("elevated" when above the 50th). Detail = 20-day realised volatility of
//             log returns, annualised (√365 for crypto, √252 otherwise).
//  VOLUME     ratio = volume of the last complete day / mean of the 20 days before it, for the instrument or its
//             proxy (ETF volumes for FX legs and some commodities; two legs → mean of both ratios).
//             ≥ 1.50 ABOVE AVG, < 0.75 BELOW AVG, otherwise AVERAGE; no usable volume → N/A.
//  STRUCTURE  5-bar fractal swings (2 bars each side) on daily bars: last two swing highs and lows both higher →
//             BULLISH, both lower → BEARISH, otherwise RANGING.
//  LIQUIDITY  analytics::liquidityRead() (pool engine of the Liquidity page on daily bars).
//  RADAR      0..100, 50 neutral: trend 50 ± 50 × intensity; momentum = RSI; volatility = ATR percentile;
//             volume = 50 × ratio (capped, N/A = 50); structure 85 / 15 / 50; liquidity = closeness of the nearest
//             pool, 100 × exp(−|distance| / (2 × ATR14%)).
namespace analytics {

enum class Lean { Neutral, Bull, Bear };
enum class Momentum { Neutral, Overbought, Oversold };
enum class Level3 { Normal, Low, High };          // volatility
enum class VolumeRead { NA, Below, Average, Above };

struct VolumeInput { QString ticker; QVector<Bar> bars; };

struct TechRead {
  bool ok = false;
  QString error;
  double last = NAN;
  qint64 lastTime = 0;
  // trend
  double ema20 = NAN, ema50 = NAN, trendPct = NAN, trendIntensity = 0;
  Lean trend = Lean::Neutral;
  // momentum
  double rsi = NAN, roc10 = NAN;
  Momentum momentum = Momentum::Neutral;
  // volatility
  double atrPct = NAN, atrPctile = NAN, volAnn = NAN;
  Level3 vol = Level3::Normal;
  // volume
  double volRatio = NAN;
  QString volTicker;
  VolumeRead volume = VolumeRead::NA;
  // structure
  Lean structure = Lean::Neutral;
  // liquidity
  LiquidityRead liq;
  // radar, axis order: trend, liquidity, structure, volume, volatility, momentum (clockwise from the top)
  double radar[6] = {50, 50, 50, 50, 50, 50};
};

// `volume` holds one or two series (proxy legs) or the instrument's own bars; empty → N/A.
// `todayUtcDays`: current UTC day number (secs / 86400); a bar of that day is incomplete for the volume read.
TechRead technicalRead(const QVector<Bar>& daily, const QVector<VolumeInput>& volume, bool crypto, qint64 todayUtcDays);

// Volume ratio of one series (helper, exposed for tests): NAN when fewer than 15 usable days.
double volumeRatio(const QVector<Bar>& b, qint64 todayUtcDays);

// Which Yahoo tickers give the volume read of an instrument (app symbol from market::instruments()).
// Empty list → the instrument's own volume (if any). {"-"} → none (N/A).
QStringList volumeSources(const QString& appSymbol);
// All proxy tickers used by volumeSources() (to prefetch).
QStringList proxyTickers();
}
