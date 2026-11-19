#include "market/Instruments.h"
#include <QHash>

namespace market {
const QList<Instrument>& instruments() {
  static const QList<Instrument> list{
  {"EURGBP", "EURGBP=X", "EUR/GBP", Category::Forex, "Crosses", 5, "EUR", "GBP"},
  {"EURAUD", "EURAUD=X", "EUR/AUD", Category::Forex, "Crosses", 5, "EUR", "AUD"},
  {"EURNZD", "EURNZD=X", "EUR/NZD", Category::Forex, "Crosses", 5, "EUR", "NZD"},
  {"EURUSD", "EURUSD=X", "EUR/USD", Category::Forex, "Majors", 5, "EUR", "USD"},
  {"EURCAD", "EURCAD=X", "EUR/CAD", Category::Forex, "Crosses", 5, "EUR", "CAD"},
  {"EURCHF", "EURCHF=X", "EUR/CHF", Category::Forex, "Crosses", 5, "EUR", "CHF"},
  {"EURJPY", "EURJPY=X", "EUR/JPY", Category::Forex, "Crosses", 3, "EUR", "JPY"},
  {"GBPAUD", "GBPAUD=X", "GBP/AUD", Category::Forex, "Crosses", 5, "GBP", "AUD"},
  {"GBPNZD", "GBPNZD=X", "GBP/NZD", Category::Forex, "Crosses", 5, "GBP", "NZD"},
  {"GBPUSD", "GBPUSD=X", "GBP/USD", Category::Forex, "Majors", 5, "GBP", "USD"},
  {"GBPCAD", "GBPCAD=X", "GBP/CAD", Category::Forex, "Crosses", 5, "GBP", "CAD"},
  {"GBPCHF", "GBPCHF=X", "GBP/CHF", Category::Forex, "Crosses", 5, "GBP", "CHF"},
  {"GBPJPY", "GBPJPY=X", "GBP/JPY", Category::Forex, "Crosses", 3, "GBP", "JPY"},
  {"AUDNZD", "AUDNZD=X", "AUD/NZD", Category::Forex, "Crosses", 5, "AUD", "NZD"},
  {"AUDUSD", "AUDUSD=X", "AUD/USD", Category::Forex, "Majors", 5, "AUD", "USD"},
  {"AUDCAD", "AUDCAD=X", "AUD/CAD", Category::Forex, "Crosses", 5, "AUD", "CAD"},
  {"AUDCHF", "AUDCHF=X", "AUD/CHF", Category::Forex, "Crosses", 5, "AUD", "CHF"},
  {"AUDJPY", "AUDJPY=X", "AUD/JPY", Category::Forex, "Crosses", 3, "AUD", "JPY"},
  {"NZDUSD", "NZDUSD=X", "NZD/USD", Category::Forex, "Majors", 5, "NZD", "USD"},
  {"NZDCAD", "NZDCAD=X", "NZD/CAD", Category::Forex, "Crosses", 5, "NZD", "CAD"},
  {"NZDCHF", "NZDCHF=X", "NZD/CHF", Category::Forex, "Crosses", 5, "NZD", "CHF"},
  {"NZDJPY", "NZDJPY=X", "NZD/JPY", Category::Forex, "Crosses", 3, "NZD", "JPY"},
  {"USDCAD", "USDCAD=X", "USD/CAD", Category::Forex, "Majors", 5, "USD", "CAD"},
  {"USDCHF", "USDCHF=X", "USD/CHF", Category::Forex, "Majors", 5, "USD", "CHF"},
  {"USDJPY", "USDJPY=X", "USD/JPY", Category::Forex, "Majors", 3, "USD", "JPY"},
  {"CADCHF", "CADCHF=X", "CAD/CHF", Category::Forex, "Crosses", 5, "CAD", "CHF"},
  {"CADJPY", "CADJPY=X", "CAD/JPY", Category::Forex, "Crosses", 3, "CAD", "JPY"},
  {"CHFJPY", "CHFJPY=X", "CHF/JPY", Category::Forex, "Crosses", 3, "CHF", "JPY"},
  {"USDCNH", "USDCNH=X", "USD/CNH", Category::Forex, "Exotics", 4, "USD", "CNH"},
  {"USDMXN", "USDMXN=X", "USD/MXN", Category::Forex, "Exotics", 3, "USD", "MXN"},
  {"USDZAR", "USDZAR=X", "USD/ZAR", Category::Forex, "Exotics", 3, "USD", "ZAR"},
  {"USDTRY", "USDTRY=X", "USD/TRY", Category::Forex, "Exotics", 3, "USD", "TRY"},
  {"USDSEK", "USDSEK=X", "USD/SEK", Category::Forex, "Exotics", 4, "USD", "SEK"},
  {"USDNOK", "USDNOK=X", "USD/NOK", Category::Forex, "Exotics", 4, "USD", "NOK"},
  {"USDSGD", "USDSGD=X", "USD/SGD", Category::Forex, "Exotics", 4, "USD", "SGD"},
  {"USDHKD", "USDHKD=X", "USD/HKD", Category::Forex, "Exotics", 4, "USD", "HKD"},
  {"EURSEK", "EURSEK=X", "EUR/SEK", Category::Forex, "Exotics", 4, "EUR", "SEK"},
  {"EURNOK", "EURNOK=X", "EUR/NOK", Category::Forex, "Exotics", 4, "EUR", "NOK"},
  {"EURPLN", "EURPLN=X", "EUR/PLN", Category::Forex, "Exotics", 4, "EUR", "PLN"},
  {"USDPLN", "USDPLN=X", "USD/PLN", Category::Forex, "Exotics", 4, "USD", "PLN"},
  {"USDINR", "USDINR=X", "USD/INR", Category::Forex, "Exotics", 3, "USD", "INR"},
  {"USDBRL", "USDBRL=X", "USD/BRL", Category::Forex, "Exotics", 3, "USD", "BRL"},
  {"SPX", "^GSPC", "S&P 500", Category::Index, "US", 2, "", ""},
  {"NDX", "^NDX", "Nasdaq 100", Category::Index, "US", 2, "", ""},
  {"NASDAQ", "^IXIC", "Nasdaq Composite", Category::Index, "US", 2, "", ""},
  {"DOW", "^DJI", "Dow Jones 30", Category::Index, "US", 2, "", ""},
  {"RUT", "^RUT", "Russell 2000", Category::Index, "US", 2, "", ""},
  {"VIX", "^VIX", "CBOE Volatility", Category::Index, "US", 2, "", ""},
  {"DXY", "DX-Y.NYB", "US Dollar Index", Category::Index, "US", 3, "", ""},
  {"TSX", "^GSPTSE", "S&P/TSX", Category::Index, "Americas", 2, "", ""},
  {"BOVESPA", "^BVSP", "Ibovespa", Category::Index, "Americas", 0, "", ""},
  {"DAX", "^GDAXI", "DAX 40", Category::Index, "Europe", 2, "", ""},
  {"FTSE", "^FTSE", "FTSE 100", Category::Index, "Europe", 2, "", ""},
  {"CAC", "^FCHI", "CAC 40", Category::Index, "Europe", 2, "", ""},
  {"STOXX50", "^STOXX50E", "Euro Stoxx 50", Category::Index, "Europe", 2, "", ""},
  {"IBEX", "^IBEX", "IBEX 35", Category::Index, "Europe", 2, "", ""},
  {"FTSEMIB", "FTSEMIB.MI", "FTSE MIB", Category::Index, "Europe", 0, "", ""},
  {"SMI", "^SSMI", "SMI", Category::Index, "Europe", 2, "", ""},
  {"AEX", "^AEX", "AEX", Category::Index, "Europe", 2, "", ""},
  {"NIKKEI", "^N225", "Nikkei 225", Category::Index, "Asia", 2, "", ""},
  {"HSI", "^HSI", "Hang Seng", Category::Index, "Asia", 2, "", ""},
  {"SSEC", "000001.SS", "Shanghai Composite", Category::Index, "Asia", 2, "", ""},
  {"ASX200", "^AXJO", "ASX 200", Category::Index, "Asia", 2, "", ""},
  {"KOSPI", "^KS11", "KOSPI", Category::Index, "Asia", 2, "", ""},
  {"SENSEX", "^BSESN", "BSE Sensex", Category::Index, "Asia", 2, "", ""},
  {"XAUUSD", "GC=F", "Gold", Category::Commodity, "Metals", 2, "", ""},
  {"XAGUSD", "SI=F", "Silver", Category::Commodity, "Metals", 3, "", ""},
  {"XPTUSD", "PL=F", "Platinum", Category::Commodity, "Metals", 2, "", ""},
  {"XPDUSD", "PA=F", "Palladium", Category::Commodity, "Metals", 2, "", ""},
  {"COPPER", "HG=F", "Copper", Category::Commodity, "Metals", 4, "", ""},
  {"WTI", "CL=F", "WTI Crude", Category::Commodity, "Energy", 2, "", ""},
  {"BRENT", "BZ=F", "Brent Crude", Category::Commodity, "Energy", 2, "", ""},
  {"NATGAS", "NG=F", "Natural Gas", Category::Commodity, "Energy", 3, "", ""},
  {"GASOLINE", "RB=F", "RBOB Gasoline", Category::Commodity, "Energy", 4, "", ""},
  {"HEATOIL", "HO=F", "Heating Oil", Category::Commodity, "Energy", 4, "", ""},
  {"CORN", "ZC=F", "Corn", Category::Commodity, "Agriculture", 2, "", ""},
  {"WHEAT", "ZW=F", "Wheat", Category::Commodity, "Agriculture", 2, "", ""},
  {"SOYBEAN", "ZS=F", "Soybeans", Category::Commodity, "Agriculture", 2, "", ""},
  {"COFFEE", "KC=F", "Coffee", Category::Commodity, "Agriculture", 2, "", ""},
  {"SUGAR", "SB=F", "Sugar", Category::Commodity, "Agriculture", 2, "", ""},
  {"COCOA", "CC=F", "Cocoa", Category::Commodity, "Agriculture", 0, "", ""},
  {"COTTON", "CT=F", "Cotton", Category::Commodity, "Agriculture", 2, "", ""},
  {"CATTLE", "LE=F", "Live Cattle", Category::Commodity, "Agriculture", 3, "", ""},
  {"US13W", "^IRX", "US 13-Week Yield", Category::Bond, "US", 3, "", ""},
  {"US5Y", "^FVX", "US 5-Year Yield", Category::Bond, "US", 3, "", ""},
  {"US10Y", "^TNX", "US 10-Year Yield", Category::Bond, "US", 3, "", ""},
  {"US30Y", "^TYX", "US 30-Year Yield", Category::Bond, "US", 3, "", ""},
  {"AAPL", "AAPL", "Apple", Category::Stock, "US", 2, "", ""},
  {"MSFT", "MSFT", "Microsoft", Category::Stock, "US", 2, "", ""},
  {"NVDA", "NVDA", "NVIDIA", Category::Stock, "US", 2, "", ""},
  {"AMZN", "AMZN", "Amazon", Category::Stock, "US", 2, "", ""},
  {"GOOGL", "GOOGL", "Alphabet", Category::Stock, "US", 2, "", ""},
  {"META", "META", "Meta Platforms", Category::Stock, "US", 2, "", ""},
  {"TSLA", "TSLA", "Tesla", Category::Stock, "US", 2, "", ""},
  {"AVGO", "AVGO", "Broadcom", Category::Stock, "US", 2, "", ""},
  {"BRK.B", "BRK-B", "Berkshire Hathaway", Category::Stock, "US", 2, "", ""},
  {"JPM", "JPM", "JPMorgan Chase", Category::Stock, "US", 2, "", ""},
  {"V", "V", "Visa", Category::Stock, "US", 2, "", ""},
  {"MA", "MA", "Mastercard", Category::Stock, "US", 2, "", ""},
  {"UNH", "UNH", "UnitedHealth", Category::Stock, "US", 2, "", ""},
  {"XOM", "XOM", "Exxon Mobil", Category::Stock, "US", 2, "", ""},
  {"JNJ", "JNJ", "Johnson & Johnson", Category::Stock, "US", 2, "", ""},
  {"WMT", "WMT", "Walmart", Category::Stock, "US", 2, "", ""},
  {"PG", "PG", "Procter & Gamble", Category::Stock, "US", 2, "", ""},
  {"HD", "HD", "Home Depot", Category::Stock, "US", 2, "", ""},
  {"BAC", "BAC", "Bank of America", Category::Stock, "US", 2, "", ""},
  {"KO", "KO", "Coca-Cola", Category::Stock, "US", 2, "", ""},
  {"NFLX", "NFLX", "Netflix", Category::Stock, "US", 2, "", ""},
  {"AMD", "AMD", "AMD", Category::Stock, "US", 2, "", ""},
  {"INTC", "INTC", "Intel", Category::Stock, "US", 2, "", ""},
  {"ORCL", "ORCL", "Oracle", Category::Stock, "US", 2, "", ""},
  {"CRM", "CRM", "Salesforce", Category::Stock, "US", 2, "", ""},
  {"GS", "GS", "Goldman Sachs", Category::Stock, "US", 2, "", ""},
  {"DIS", "DIS", "Walt Disney", Category::Stock, "US", 2, "", ""},
  {"BA", "BA", "Boeing", Category::Stock, "US", 2, "", ""},
  {"BTCUSD", "BTC-USD", "Bitcoin", Category::Crypto, "Crypto", 2, "", ""},
  {"ETHUSD", "ETH-USD", "Ethereum", Category::Crypto, "Crypto", 2, "", ""},
  {"SOLUSD", "SOL-USD", "Solana", Category::Crypto, "Crypto", 2, "", ""},
  {"XRPUSD", "XRP-USD", "XRP", Category::Crypto, "Crypto", 4, "", ""},
  {"BNBUSD", "BNB-USD", "BNB", Category::Crypto, "Crypto", 2, "", ""},
  {"ADAUSD", "ADA-USD", "Cardano", Category::Crypto, "Crypto", 4, "", ""},
  {"DOGEUSD", "DOGE-USD", "Dogecoin", Category::Crypto, "Crypto", 4, "", ""},
  {"AVAXUSD", "AVAX-USD", "Avalanche", Category::Crypto, "Crypto", 2, "", ""},
  {"DOTUSD", "DOT-USD", "Polkadot", Category::Crypto, "Crypto", 2, "", ""},
  {"LINKUSD", "LINK-USD", "Chainlink", Category::Crypto, "Crypto", 2, "", ""},
  {"LTCUSD", "LTC-USD", "Litecoin", Category::Crypto, "Crypto", 2, "", ""},
  {"TRXUSD", "TRX-USD", "TRON", Category::Crypto, "Crypto", 4, "", ""},
  };
  return list;
}

const Instrument* find(const QString& s) {
  static const QHash<QString, int> idx = [] {
    QHash<QString, int> h;
    const auto& l = instruments();
    for (int i = 0; i < l.size(); ++i) { h.insert(l[i].symbol.toUpper(), i); h.insert(l[i].yahoo.toUpper(), i); }
    return h;
  }();
  QString k = s.toUpper(); k.remove('/');
  auto it = idx.constFind(k);
  return it == idx.cend() ? nullptr : &instruments()[*it];
}
QList<Instrument> byCategory(Category c) {
  QList<Instrument> out;
  for (const auto& i : instruments()) if (i.category == c) out << i;
  return out;
}
QString categoryName(Category c) {
  switch (c) {
    case Category::Forex: return "Forex";
    case Category::Index: return "Indices";
    case Category::Commodity: return "Commodities";
    case Category::Stock: return "Stocks";
    case Category::Crypto: return "Crypto";
    case Category::Bond: return "Bonds";
  }
  return {};
}
QString display(const QString& symbol) {
  const auto* i = find(symbol);
  if (i && i->category == Category::Forex) return i->base + "/" + i->quote;
  return i ? i->symbol : symbol;
}
QList<QString> majorCurrencies() { return {"USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD"}; }
}
