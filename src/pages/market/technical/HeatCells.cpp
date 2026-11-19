#include "pages/market/technical/HeatCells.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <cmath>

using namespace analytics;

namespace heat {
namespace {
// measured heat palette (see docs/reference/Market/technical-heatmap.md §4.5)
QColor kCyanText() { return theme::map("#22d3ee"); }
QColor kRedText() { return theme::map("#f87171"); }
QColor kGold() { return theme::map("#facc15"); }
QColor kGrey() { return theme::map("#a3a09a"); }
QColor kViolet() { return theme::map("#a78bfa"); }
QColor mix(const char* a, const char* b, double t) { return theme::mix(theme::map(a), theme::map(b), std::clamp(t, 0.0, 1.0)); }
QString pct2(double v) { return fmt::pct(v, 2); }
QString arrowPct(double v) { return (v >= 0 ? QStringLiteral("↑ ") : QStringLiteral("↓ ")) + fmt::pct(v, 2); }
}

QString columnTitle(Column c) {
  static const char* t[] = {QT_TRANSLATE_NOOP("technical", "TREND"), QT_TRANSLATE_NOOP("technical", "MOMENTUM"),
                            QT_TRANSLATE_NOOP("technical", "VOLATILITY"), QT_TRANSLATE_NOOP("technical", "VOLUME"),
                            QT_TRANSLATE_NOOP("technical", "STRUCTURE"), QT_TRANSLATE_NOOP("technical", "LIQUIDITY")};
  return QCoreApplication::translate("technical", t[c]);
}

double sortKey(const TechRead& r, Column c) {
  switch (c) {
    case Trend: return std::isfinite(r.trendPct) ? r.trendPct : -1e9;
    case Momentum: return std::isfinite(r.rsi) ? r.rsi : -1e9;
    case Volatility: return std::isfinite(r.atrPctile) ? r.atrPctile : -1e9;
    case Volume: return std::isfinite(r.volRatio) ? r.volRatio : -1e9;
    case Structure: return r.structure == Lean::Bull ? 1 : r.structure == Lean::Bear ? -1 : 0;
    case Liquidity: return r.liq.state == LiqRead::None ? -1e9 : r.liq.closeness;
    default: return 0;
  }
}

Cell cell(const TechRead& r, Column c) {
  Cell o;
  switch (c) {
    case Trend: {
      o.detail = pct2(r.trendPct);
      o.fileDetail = o.detail;
      o.tip = QCoreApplication::translate("technical", "EMA20 %1 · EMA50 %2\ngap %3 · intensity %4\nNeutral band ±0.10%")
                  .arg(fmt::num(r.ema20, 4, true), fmt::num(r.ema50, 4, true), pct2(r.trendPct)).arg(r.trendIntensity, 0, 'f', 2);
      const double t = r.trendIntensity;
      if (r.trend == Lean::Bull) {
        o.word = QCoreApplication::translate("technical", "BULLISH"); o.fill = mix("#06292d", "#11909c", std::pow(t, 0.8) * 1.15);
        o.text = mix("#6fd3de", "#bff4f8", t); o.fileValue = QCoreApplication::translate("technical", "Bullish"); o.fileColor = kCyanText();
      } else if (r.trend == Lean::Bear) {
        o.word = QCoreApplication::translate("technical", "BEARISH"); o.fill = mix("#2f1a12", "#a4544e", std::pow(t, 0.8) * 1.15);
        o.text = theme::map("#ffb4ae"); o.fileValue = QCoreApplication::translate("technical", "Bearish"); o.fileColor = kRedText();
      } else {
        const bool pos = !(r.trendPct < 0);
        o.word = QCoreApplication::translate("technical", "NEUTRAL"); o.fill = theme::map(pos ? "#062c2f" : "#2f1a12");
        o.text = theme::map(pos ? "#5fc9d4" : "#e9827c"); o.fileValue = QCoreApplication::translate("technical", "Neutral"); o.fileColor = pos ? kCyanText() : kRedText();
      }
      break;
    }
    case Momentum: {
      o.detail = std::isfinite(r.rsi) ? QString("RSI %1").arg(qRound(r.rsi)) : fmt::dash();
      o.fileDetail = o.detail;
      o.tip = QCoreApplication::translate("technical", "RSI(14) %1 · ROC(10) %2\n≥ 70 overbought, ≤ 30 oversold").arg(r.rsi, 0, 'f', 1).arg(fmt::pct(r.roc10, 2));
      if (r.momentum == Momentum::Overbought) {
        o.word = QCoreApplication::translate("technical", "OVERBOUGHT"); o.fill = mix("#6a3431", "#8a4643", (r.rsi - 70) / 20); o.text = theme::map("#f0a39e");
        o.fileValue = QCoreApplication::translate("technical", "Overbought"); o.fileColor = kRedText();
      } else if (r.momentum == Momentum::Oversold) {
        o.word = QCoreApplication::translate("technical", "OVERSOLD"); o.fill = mix("#0a4f53", "#0e6469", (30 - r.rsi) / 20); o.text = theme::map("#7fd7e2");
        o.fileValue = QCoreApplication::translate("technical", "Oversold"); o.fileColor = kCyanText();
      } else {
        o.word = QCoreApplication::translate("technical", "NEUTRAL"); o.fileValue = QCoreApplication::translate("technical", "Neutral");
        if (!(r.rsi > 50)) { o.fill = mix("#0a3133", "#0d4245", (50 - r.rsi) / 20); o.text = theme::map("#6fd0db"); o.fileColor = kCyanText(); }
        else { o.fill = mix("#3d1f1b", "#63302e", (r.rsi - 50) / 20); o.text = theme::map("#ec8f88"); o.fileColor = kRedText(); }
      }
      break;
    }
    case Volatility: {
      o.detail = std::isfinite(r.volAnn) ? QCoreApplication::translate("technical", "%1% ann.").arg(fmt::num(r.volAnn, 1)) : fmt::dash();
      o.fileDetail = o.detail;
      o.tip = QCoreApplication::translate("technical", "ATR(14) %1% of price, %2th percentile of its last 252 days\n20-day realised volatility %3")
                  .arg(r.atrPct, 0, 'f', 2).arg(qRound(r.atrPctile)).arg(o.detail);
      if (r.vol == Level3::High) {
        o.word = QCoreApplication::translate("technical", "HIGH"); o.fill = mix("#7a6806", "#9a8306", (r.atrPctile - 80) / 20); o.text = theme::map("#fbe571");
        o.fileValue = QCoreApplication::translate("technical", "High"); o.fileColor = kGold();
      } else if (r.vol == Level3::Low) {
        o.word = QCoreApplication::translate("technical", "LOW"); o.fill = mix("#3c4142", "#505556", (20 - r.atrPctile) / 20); o.text = theme::map("#d8d8d8");
        o.fileValue = QCoreApplication::translate("technical", "Low"); o.fileColor = kGrey();
      } else {
        o.word = QCoreApplication::translate("technical", "NORMAL"); o.fileValue = QCoreApplication::translate("technical", "Normal");
        if (r.atrPctile > 50) { o.fill = mix("#2a2203", "#3a2f04", (r.atrPctile - 50) / 30); o.text = theme::map("#f4d651"); o.fileColor = kGold(); }
        else { o.fill = theme::map("#2c302f"); o.text = theme::map("#c9c9c9"); o.fileColor = kGrey(); }
      }
      break;
    }
    case Volume: {
      if (r.volume == VolumeRead::NA) {
        o.word = QCoreApplication::translate("technical", "N/A"); o.text = theme::map("#73726e"); o.fileValue = QCoreApplication::translate("technical", "N/A"); o.fileColor = kGrey();
        o.tip = QCoreApplication::translate("technical", "No usable volume for this instrument or a proxy");
        break;
      }
      o.detail = QCoreApplication::translate("technical", "×%1 · %2").arg(fmt::num(r.volRatio, 2), r.volTicker);
      o.fileDetail = o.detail;
      o.tip = QCoreApplication::translate("technical", "Last complete day's volume of %1 = ×%2 its 20-day average\n≥ 1.50 above, < 0.75 below")
                  .arg(r.volTicker, fmt::num(r.volRatio, 2));
      if (r.volume == VolumeRead::Above) {
        o.word = QCoreApplication::translate("technical", "ABOVE AVG"); o.fill = mix("#8a7608", "#a08a0a", (r.volRatio - 1.5) / 1.0); o.text = theme::map("#ffe066");
        o.fileValue = QCoreApplication::translate("technical", "Above avg"); o.fileColor = kGold();
      } else if (r.volume == VolumeRead::Below) {
        o.word = QCoreApplication::translate("technical", "BELOW AVG"); o.fill = mix("#363b3c", "#5a5b5b", (0.75 - r.volRatio) / 0.6); o.text = theme::map("#d0d0d0");
        o.fileValue = QCoreApplication::translate("technical", "Below avg"); o.fileColor = kGrey();
      } else {
        o.word = QCoreApplication::translate("technical", "AVERAGE"); o.fileValue = QCoreApplication::translate("technical", "Average");
        if (r.volRatio > 1) { o.fill = mix("#3a3103", "#4f4304", (r.volRatio - 1) / 0.5); o.text = theme::map("#f5d64e"); o.fileColor = kGold(); }
        else { o.fill = theme::map("#2f3436"); o.text = theme::map("#c8c8c8"); o.fileColor = kGrey(); }
      }
      break;
    }
    case Structure: {
      o.tip = QCoreApplication::translate("technical", "Last two 5-bar swing highs and lows on daily bars:\nhigher highs + higher lows = bullish, lower + lower = bearish");
      if (r.structure == Lean::Bull) { o.word = QCoreApplication::translate("technical", "BULLISH"); o.fill = theme::map("#0f747f"); o.text = theme::map("#bdf3f7"); o.fileValue = QCoreApplication::translate("technical", "Bullish"); o.fileColor = kCyanText(); }
      else if (r.structure == Lean::Bear) { o.word = QCoreApplication::translate("technical", "BEARISH"); o.fill = theme::map("#8a4441"); o.text = theme::map("#ffb0aa"); o.fileValue = QCoreApplication::translate("technical", "Bearish"); o.fileColor = kRedText(); }
      else { o.word = QCoreApplication::translate("technical", "RANGING"); o.text = theme::map("#7d7c76"); o.fileValue = QCoreApplication::translate("technical", "Ranging"); o.fileColor = kGrey(); }
      break;
    }
    case Liquidity: {
      const auto& l = r.liq;
      o.tip = QCoreApplication::translate("technical", "%1 pools on daily bars, %2 untested\nNear = within 1 × ATR(14); recently swept = last 3 days").arg(l.total).arg(l.untested);
      if (l.state == LiqRead::None) {
        o.word = QCoreApplication::translate("technical", "NO POOL"); o.text = theme::map("#73726e"); o.fileValue = QCoreApplication::translate("technical", "No pool"); o.fileColor = kGrey();
        break;
      }
      o.detail = arrowPct(l.distPct); o.fileDetail = o.detail;
      const double k = l.closeness / 100.0;
      switch (l.state) {
        case LiqRead::RecentlySwept: o.word = QCoreApplication::translate("technical", "RECENTLY SWEPT"); o.fill = theme::map("#897406"); o.text = theme::map("#ffea85"); o.fileValue = QCoreApplication::translate("technical", "Recently swept"); o.fileColor = kGold(); break;
        case LiqRead::NearPool: o.word = QCoreApplication::translate("technical", "NEAR POOL"); o.fill = mix("#4f4470", "#665a8c", k); o.text = theme::map("#d6ccf5"); o.fileValue = QCoreApplication::translate("technical", "Near pool"); o.fileColor = kViolet(); break;
        case LiqRead::PoolAbove: o.word = QCoreApplication::translate("technical", "POOL ABOVE"); o.fill = mix("#221f2c", "#353047", k); o.text = theme::map("#c5bde0"); o.fileValue = QCoreApplication::translate("technical", "Pool above"); o.fileColor = kViolet(); break;
        case LiqRead::PoolBelow: o.word = QCoreApplication::translate("technical", "POOL BELOW"); o.fill = mix("#211d2d", "#332d48", k); o.text = theme::map("#c5bde0"); o.fileValue = QCoreApplication::translate("technical", "Pool below"); o.fileColor = kViolet(); break;
        default: break;
      }
      break;
    }
    default: break;
  }
  return o;
}
}
