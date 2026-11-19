#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/feeds.js. Your own market data keys, optional. The free public sources stay the default; a key you
// add takes over the charts it covers, and the public sources stay behind it as the fallback.
//
// Keys live in <data>/charts/feeds.json on this computer (paths::data("charts"), the JS data/feeds.json). Each
// key is only ever sent to its own provider, in a request header:
//
//   {
//     "alpaca":  { "keyId": "PK...", "secretKey": "...", "feed": "iex" },
//     "polygon": { "apiKey": "..." },
//     "oanda":   { "token": "...", "account": "practice" }
//   }
//
//   Alpaca   US stocks. A free Alpaca account gives live prices on the IEX feed ("feed": "sip" if
//            you pay for Alpaca's full feed).
//   Polygon  US stocks, and CME futures (NQ, MNQ, ES, MES) on a Polygon futures plan. Polygon is
//            now called Massive; same keys. Live or delayed depends on your Polygon plan: the free
//            plan is end-of-day only.
//   OANDA    Forex and metals, plus anything your account lists as OANDA:XAU_USD or OANDA:NAS100_USD.
//            These are OANDA's own prices, not exchange data. "account" is "practice" for a free
//            demo account's token or "live" for a real account's; each only works on its own server.
#include "charts/data/Fetch.h"
#include <QJsonObject>

namespace charts::data::feeds {

QString feedsFile();                 // paths::data("charts") + "/feeds.json" (JS FEEDS_FILE)
// The parsed file, re-read when its modification time changes (a key you add works without a restart).
// Invalid JSON: {} and a warning "data/feeds.json isn't valid JSON, so your own keys are off: <why>".
QJsonObject feedConfig();
bool hasOanda();
QStringList configured();            // names only ("alpaca", "polygon", "oanda"): the keys never leave this module
QString alpacaName();                // "Alpaca (IEX)" / "Alpaca (SIP)"
bool isKeyProvider(const QString& name);   // /^(Alpaca|Polygon|OANDA)\b/

// New York's UTC offset (4 or 5) for the day of `sec`, worked out once per day.
int nyOffsetHours(qint64 sec);
// 09:30 to 16:00 New York, the same session the free sources chart.
bool inUsSession(qint64 sec);

void alpacaCandles(const QString& ticker, const QString& tf, int limit, Callback<Bars> cb);
void polygonCandles(const QString& ticker, const QString& tf, int limit, Callback<Bars> cb);
// Sources to try before the public ones for a US stock, in this order (Alpaca, then Polygon).
Chain stockChain(const QString& ticker, const QString& tf, int limit);

// Front contract of an equity index future ("NQZ6"): rolls 8 days before the third Friday of Mar/Jun/Sep/Dec.
// nowMs < 0 = now.
QString frontContract(const QString& root, qint64 nowMs = -1);
void polygonFuturesCandles(const QString& contract, const QString& tf, int limit, Callback<Bars> cb);
// [["Polygon NQZ6", …]] with a Polygon key, else empty.
Chain futuresChain(const QString& root, const QString& tf, int limit);

void oandaCandles(const QString& instrument, const QString& tf, int limit, Callback<Bars> cb);
// Your OANDA key in front of the public sources for a forex pair or metal: EURUSD is EUR_USD there.
Chain fxChain(const QString& pair, const QString& tf, int limit);

// For the doctor / probeAll: does each key you added work? Name -> check.
QList<QPair<QString, std::function<void(Callback<QJsonDocument>)>>> probes();

namespace testing {
void reset();   // forget the memo of feeds.json, resting providers and the New York offsets
}
}  // namespace charts::data::feeds
