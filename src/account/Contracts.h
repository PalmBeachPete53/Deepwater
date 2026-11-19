#pragma once
#include <QString>
#include <QStringList>

// Contract specifications used by the journal and the risk calculator, on top of market::instruments().
// Values are the common MT4/MT5/cTrader CFD conventions (1 FX lot = 100,000 base units, 1 gold lot = 100 oz,
// index CFDs 1 unit per point, crypto 1 coin, stocks 1 share). Brokers can differ: the pages always print
// the contract size they assume.
namespace account {

struct ContractSpec {
  QString app;          // market::instruments() symbol ("EURUSD", "XAUUSD", "NDX"); empty when unknown
  QString display;      // "EUR/USD", "Gold", "Nasdaq 100"
  QString tag;          // short category tag: FX, IDX, METAL, ENERGY, AGRI, CRYPTO, STOCK, BOND
  double contract = 1;  // units per 1.00 lot
  QString unit;         // "units", "oz", "barrels", "gallons", "bushels", "lb", "tonnes", "coins", "shares", "contracts"
  double priceUnit = 1; // money per 1.0 of price per unit: 0.01 when the price is in cents (grains, softs, cattle)
                        // → P&L = price move × lots × contract × priceUnit (in quoteCcy)
  double pip = 0.0001;  // pip / point size used for default stop distances
  int decimals = 5;
  QString quoteCcy;     // currency the price is quoted in (USD when unknown)
};

// Broker symbol ("EURUSD.r", "XAUUSDm", "US100", "NAS100", "GER40") → spec. Unknown symbols keep the raw name.
ContractSpec spec(const QString& brokerSymbol);
QString displaySymbol(const QString& brokerSymbol);

// Live conversion rate: 1 unit of `from` in `to`, from market::QuoteHub (NAN while not loaded).
double fxRate(const QString& from, const QString& to);
// QuoteHub symbols needed to convert `from` into `to` (direct, inverse or through USD).
QStringList fxSymbolsFor(const QString& from, const QString& to);
}
