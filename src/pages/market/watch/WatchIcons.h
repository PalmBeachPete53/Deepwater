#pragma once
#include <QPixmap>
#include <QString>

namespace watch {
// Row icon of an instrument (pair flags, index country flag, commodity glyph, coin logo, stock monogram),
// cached. Width varies (pairs are wider); height is `h`.
QPixmap instrumentIcon(const QString& symbol, int h = 12, qreal dpr = 2.0);
// ISO-2 country of an index / bond (empty if unknown).
QString countryOf(const QString& symbol);
// Currency of a market's home economy ("USD" for US stocks, "EUR" for the DAX…).
QString currencyOf(const QString& symbol);
// Human name of a currency ("Euro", "U.S. Dollar").
QString currencyName(const QString& ccy);
// Label of a market in lists and headers: "EUR/USD", "BTC/USD", "S&P 500", "AAPL" (stocks: ticker).
QString marketLabel(const QString& symbol);
}
