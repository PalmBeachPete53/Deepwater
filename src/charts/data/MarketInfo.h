#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/market-info.js. Earnings and dividend calendars, news, financial statements and key stats. Nasdaq's
// public API for US stocks; Cointelegraph's and Decrypt's RSS feeds for crypto headlines.
//
// Asynchronous like the JS promises: requests go through net::get (Fetch.h send), answers come back on the GUI
// thread, never synchronously. Results are the exact JSON shapes of the JS (the local HTTP API returns them as is).
// Caches are the JS `cached(...)` ones (Fetch.h): earnings/dividends per day 30 min, news 10 min, financials 6 h.
// QT_OFFLINE=1 in the environment returns empty answers as the JS does.
#include "charts/data/Fetch.h"
#include "charts/data/Providers.h"
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDate>
#include <functional>

class QObject;

namespace charts::data {

// The next n weekdays starting today (UTC), "YYYY-MM-DD". `today` for tests (invalid = now).
QStringList weekdays(int n, QDate today = {});

// Calendars of the next `days` weekdays (clamped to 1..10), every day in parallel, a day that fails gives [].
// earnings items: {date, symbol:"US:T", ticker, name, when, marketCap, epsForecast, quarter, lastYearEps}
// dividends items: {date, symbol, ticker, name, exDate, payDate, recordDate, amount, annual}
using ArrayCb = std::function<void(const QJsonArray&)>;
void earnings(int days, QObject* ctx, ArrayCb cb);
void dividends(int days, QObject* ctx, ArrayCb cb);
// One day's Nasdaq answer to items (pure: the parsing of the two calls above).
QJsonArray earningsRows(const QJsonDocument& j, const QString& date);
QJsonArray dividendRows(const QJsonDocument& j, const QString& date);

// ---------- news ----------
// RSS <item>s: {title, url, source, time (ms or null), summary (280 chars)}.
QJsonArray parseRss(const QString& xml, const QString& source);
// Cointelegraph + Decrypt headlines, newest first (cached 10 min).
void cryptoNews(QObject* ctx, ArrayCb cb);
// {items, note}: crypto headlines for BINANCE/COINBASE/KRAKEN symbols, Nasdaq headlines for US ones.
// The string overload resolves the symbol first (Providers' resolve), as the JS does; its errors come back as is.
using ObjectCb = std::function<void(const QJsonObject&)>;
void news(const Symbol& sym, QObject* ctx, ObjectCb cb);
void news(const QString& symbol, QObject* ctx, Callback<QJsonObject> cb);
// Pure parts of news(): the coin a crypto ticker is about ("BTCUSDT" -> "BTC"), the {items, note} picked from all
// crypto headlines, and Nasdaq's articlebysymbol rows to items.
QString coinBase(const QString& ticker);
QJsonObject cryptoHeadlines(const QString& base, const QJsonArray& all);
QJsonArray nasdaqNewsRows(const QJsonDocument& j);

// ---------- fundamentals ----------
// {symbol, frequency, stats:[{label,value}], nextEarnings, income, balance, cashflow, ratios, note}; each table is
// {periods:[...], rows:[{label, values:[...]}]} or null. Errors: 422 not a US stock, 404 Nasdaq has nothing.
// Offline: success with a null value (the JS returns null).
void financials(const Symbol& sym, const QString& frequency, QObject* ctx, Callback<QJsonValue> cb);
void financials(const QString& symbol, const QString& frequency, QObject* ctx, Callback<QJsonValue> cb);   // resolves first
// Pure part: the three Nasdaq answers (raw bodies, empty = the request failed) to the JS object.
Result<QJsonValue> financialsFrom(const Symbol& sym, const QString& frequency, const QByteArray& fin,
                                  const QByteArray& summary, const QByteArray& earningsDate);

// ---------- JS helpers (exposed for tests and the other data modules) ----------
double jsParseFloat(const QString& s);   // parseFloat
double jsDateParse(const QString& s);    // Date.parse (ISO, RFC 822 "Mon, 16 Nov 2026 13:31:49 +0000", "Nov 15, 2026"), ms or NaN
// Keys of the object at `path` in raw JSON text, in document order (JS Object.keys order; QJsonObject sorts them).
QStringList jsonKeysInOrder(const QByteArray& json, const QStringList& path);

}  // namespace charts::data
