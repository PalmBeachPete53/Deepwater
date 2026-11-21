#pragma once
// Blind trials (follow-up of step 6, 2026-11-20): an environment in evaluation mode can also be blind. Between the tools
// and the model, Deepwater rewrites what would let the model recognise the market or the moment, so its memory of what
// followed cannot help it (the literature's "anonymised" trials, arXiv 2504.14765: a model recognised companies in 100 %
// of anonymised earnings calls when only names were masked):
//   dates     every date shifted by a whole number of years into the future (9 to 13, from the seed): no memory exists
//   names     the market and its exchange replaced (BINANCE:BTCUSDT → MARKET:ASSET1, Bitcoin → the asset…)
//   prices    the numbers of the market's price level (half to twice the close at the moment) multiplied by a secret
//             factor (0.005 to 0.05 from the seed), so 66 000 does not give Bitcoin away; percentages, ratios, small
//             quantities are left alone
// The model's arguments go the other way (its dates shifted back, ASSET1 back to the market) before a tool runs.
// Limits, said in the report: a number of the price's order that is not a price (an open interest in coins) is scaled
// too; a price written in a strategy rule by the model is not scaled back; the asset's behaviour itself (its volatility)
// is not masked. Pure QtCore: tested in tests/trials.
#include <QJsonObject>
#include <QString>

namespace ai::blind {

struct Mask {
  bool on = false;
  int years = 11;           // the date shift
  double factor = 0.02;     // prices × factor
  double refPrice = 0;      // the close at the moment: the price level's numbers are within [ref / 2, ref × 2]
  QString market;           // "BINANCE:BTCUSDT"
};
Mask make(quint32 seed, const QString& market, double refPrice);   // years and factor drawn from the seed

QString out(const Mask& m, const QString& text);            // the tool's text → what the model sees
QJsonObject in(const Mask& m, const QJsonObject& args);      // the model's arguments → what the tool gets
QString alias(const Mask& m);                                // "MARKET:ASSET1"
QString shiftDate(const Mask& m, qint64 t);                  // "2037-11-14 13:20" for the prompts

}  // namespace ai::blind
