#pragma once
// Data › Technical data as backtest bars (user's request, 2026-11-18): the Strategy tester of the Charts tab and the agents'
// backtest tools test on the stored history (every closed bar since the exchange's first day) instead of the bars a
// chart or a source hands out, and the bar magnifier walks stops and targets through the stored lower-timeframe bars.
// Reads only; any thread (the store's readers are per thread).
#include "charts/core/Types.h"
#include <QString>

namespace technicaldata {

// The pair Technical data records for a symbol, else empty: "BTCUSDT", "binance:btcusdt", "BTC/USDT" → BINANCE:BTCUSDT;
// "EURUSD", "EUR/USD", "FX:EURUSD" (Charts), "OANDA:EURUSD" → DUKASCOPY:EURUSD. Another exchange's crypto
// ("COINBASE:BTCUSD") is not Binance's market: empty.
QString storedMarket(const QString& symbol);
// where a pair's bars come from, for a sentence: "Binance spot" / "Dukascopy bid prices"
QString sourceOf(const QString& market);
// The bar length in seconds when it is one of Technical data's timeframes (1m 5m 15m 1h 4h 1D 1W), else 0.
int storedTimeframe(int seconds);
qint64 storedCount(const QString& market, int tfSeconds);

// the newest `max` closed bars, oldest first
charts::Bars newestBars(const QString& market, int tfSeconds, int max);

// Lower-timeframe bars for the bar magnifier over the test bars: the finest stored timeframe below tfSeconds that covers
// [bars.first, bars.last + tfSeconds) in at most `maxBars` bars (1m when it fits, else 5m, 15m, 1h, 4h). Empty when no
// finer timeframe is stored or none fits. tfName gets the one used.
charts::Bars magnifierBars(const QString& market, int tfSeconds, const charts::Bars& bars, int maxBars = 1000000,
                           QString* tfName = nullptr);

}  // namespace technicaldata
