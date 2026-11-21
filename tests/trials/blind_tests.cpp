// Blind trials (src/ai/Blind): what the model sees of the tools' texts, and what the tools get of its arguments.
#include "Check.h"
#include "ai/Blind.h"
#include <QJsonObject>

using namespace ai::blind;

TEST("blind: dates shifted by whole years, names masked, the price level scaled, the rest left alone") {
  Mask m;
  m.on = true; m.years = 11; m.factor = 0.01; m.refPrice = 66000; m.market = "BINANCE:BTCUSDT";
  const QString t = out(m, "BINANCE:BTCUSDT 1h bars, as of 2026-11-14 13:20 UTC (Binance spot)\n"
                           "2026-11-14 12:00 | 67272.79 | 66998.18 | 620.591 | 46.4 | rsi 32.19 | stored 2026-08 to 2026-11\n"
                           "Bitcoin's open: 6.45 % · BTC/USDT · BTCUSD · ref 4500-12");
  CHECK(t.contains("MARKET:ASSET1 1h bars, as of 2037-11-14 13:20 UTC (the exchange spot)"));
  CHECK(t.contains("2037-11-14 12:00 | 672.73 | 669.98 | 620.591 | 46.4 | rsi 32.19 | stored 2037-08 to 2037-11"));
  CHECK(t.contains("the asset's open: 6.45 % · ASSET1 · ASSET1 · ref 4500-12"));
  CHECK(!t.contains("BTC") && !t.contains("2026") && !t.contains("Binance"));
}

TEST("blind: the model's arguments go back to the real market and dates") {
  Mask m;
  m.on = true; m.years = 11; m.market = "BINANCE:BTCUSDT";
  const QJsonObject a = in(m, QJsonObject{{"market", "ASSET1"}, {"end", "2037-11-14 13:20"}, {"bars", 12},
                                          {"strategy", QJsonObject{{"from", "2037-01-02"}}}});
  CHECK_EQ(a.value("market").toString(), QString("BTCUSDT"));
  CHECK_EQ(a.value("end").toString(), QString("2026-11-14 13:20"));
  CHECK_EQ(a.value("bars").toInt(), 12);
  CHECK_EQ(a.value("strategy").toObject().value("from").toString(), QString("2026-01-02"));
  CHECK_EQ(in(m, QJsonObject{{"key", "MARKET:ASSET1"}}).value("key").toString(), QString("BINANCE:BTCUSDT"));
  CHECK_EQ(shiftDate(m, 1794662400), QString("2037-11-14 13:20"));
}

TEST("blind: the seed sets the shift and the factor, within their bounds") {
  for (quint32 s : {1u, 7u, 12345u, 4000000000u}) {
    const Mask m = make(s, "BINANCE:BTCUSDT", 66000);
    CHECK(m.years >= 9 && m.years <= 13);
    CHECK(m.factor >= 0.005 && m.factor <= 0.05);
  }
  CHECK(make(1, "BINANCE:BTCUSDT", 1).factor != make(2, "BINANCE:BTCUSDT", 1).factor);
}
