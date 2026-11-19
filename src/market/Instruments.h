#pragma once
#include <QString>
#include <QList>

// Catalogue of the markets the app knows (Markets Watch shows them all; other pages pick from it).
// symbol: the app's own id ("EURUSD", "SPX", "XAUUSD", "AAPL", "BTCUSD"); yahoo: Yahoo Finance ticker.
namespace market {
enum class Category { Forex, Index, Commodity, Stock, Crypto, Bond };
struct Instrument {
  QString symbol;
  QString yahoo;
  QString name;
  Category category;
  QString group;      // finer group: "Majors", "Crosses", "Exotics", "US", "Europe", "Asia", "Energy", "Metals", "Agriculture", "Tech"…
  int decimals = 2;
  QString base, quote; // forex legs ("EUR", "USD"); empty otherwise
};
const QList<Instrument>& instruments();
const Instrument* find(const QString& symbol);        // by app symbol, Yahoo ticker or "EUR/USD"
QList<Instrument> byCategory(Category c);
QString categoryName(Category c);
QString display(const QString& symbol);                // "EUR/USD" for pairs, symbol otherwise
QList<QString> majorCurrencies();                      // USD EUR GBP JPY CHF CAD AUD NZD
}
