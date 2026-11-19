#include "account/Contracts.h"
#include "market/Instruments.h"
#include "market/QuoteHub.h"
#include <QHash>
#include <QRegularExpression>
#include <cmath>

namespace account {
namespace {
// Broker aliases → app symbol.
const QHash<QString, QString>& aliases() {
  static const QHash<QString, QString> m{
    {"GOLD", "XAUUSD"}, {"SILVER", "XAGUSD"}, {"XAUUSD", "XAUUSD"}, {"XAGUSD", "XAGUSD"},
    {"US100", "NDX"}, {"NAS100", "NDX"}, {"USTEC", "NDX"}, {"NDX100", "NDX"}, {"NASDAQ100", "NDX"}, {"NQ", "NDX"},
    {"US500", "SPX"}, {"SPX500", "SPX"}, {"SP500", "SPX"}, {"US500CASH", "SPX"}, {"ES", "SPX"},
    {"US30", "DOW"}, {"DJ30", "DOW"}, {"DJI30", "DOW"}, {"WS30", "DOW"}, {"US2000", "RUT"},
    {"GER40", "DAX"}, {"DE40", "DAX"}, {"GER30", "DAX"}, {"DE30", "DAX"}, {"DAX40", "DAX"},
    {"UK100", "FTSE"}, {"FRA40", "CAC"}, {"FR40", "CAC"}, {"EU50", "STOXX50"}, {"ESTX50", "STOXX50"},
    {"JP225", "NIKKEI"}, {"JPN225", "NIKKEI"}, {"HK50", "HSI"}, {"AUS200", "ASX200"}, {"SPA35", "IBEX"},
    {"ES35", "IBEX"}, {"SWI20", "SMI"}, {"NETH25", "AEX"},
    {"USOIL", "WTI"}, {"WTI", "WTI"}, {"XTIUSD", "WTI"}, {"UKOIL", "BRENT"}, {"XBRUSD", "BRENT"}, {"BRENT", "BRENT"},
    {"NGAS", "NATGAS"}, {"XNGUSD", "NATGAS"}, {"XPTUSD", "XPTUSD"}, {"XPDUSD", "XPDUSD"}, {"COPPER", "COPPER"},
    {"BTCUSD", "BTCUSD"}, {"ETHUSD", "ETHUSD"}, {"BTCUSDT", "BTCUSD"}, {"ETHUSDT", "ETHUSD"},
  };
  return m;
}
QString indexCcy(const QString& app) {
  static const QHash<QString, QString> m{
    {"DAX", "EUR"}, {"CAC", "EUR"}, {"STOXX50", "EUR"}, {"IBEX", "EUR"}, {"FTSEMIB", "EUR"}, {"AEX", "EUR"},
    {"FTSE", "GBP"}, {"SMI", "CHF"}, {"NIKKEI", "JPY"}, {"HSI", "HKD"}, {"SSEC", "CNY"}, {"ASX200", "AUD"},
    {"KOSPI", "KRW"}, {"SENSEX", "INR"}, {"TSX", "CAD"}, {"BOVESPA", "BRL"}};
  return m.value(app, "USD");
}
QString cleanSymbol(QString s) {
  s = s.trimmed(); s.remove('/');
  auto known = [](const QString& t) { const QString u = t.toUpper(); return market::find(u) || aliases().contains(u); };
  if (known(s)) return s.toUpper();
  static const QRegularExpression lead("^[#.]+"), sepRe("[._#\\-+!]"), lowerTail("[a-z]+$");
  s.remove(lead);
  if (known(s)) return s.toUpper();
  // EURUSD.r, EURUSD-pro, EURUSD_i, EURUSD#
  const int sep = s.indexOf(sepRe);
  if (sep > 1 && known(s.left(sep))) return s.left(sep).toUpper();
  // EURUSDm, EURUSDpro (lower-case broker suffix)
  QString t = s; t.remove(lowerTail);
  if (t.size() > 1 && known(t)) return t.toUpper();
  const QString u = s.toUpper();
  if (u.size() > 6 && market::find(u.left(6))) return u.left(6);
  return u;
}
}

static ContractSpec specOf(const QString& raw);
// spec() is a pure function of the raw broker symbol (the catalogue and aliases are static), called per trade by the
// Track Record's reloads, filters, statistics, sort comparator and painting: computed once per symbol and thread.
ContractSpec spec(const QString& raw) {
  static thread_local QHash<QString, ContractSpec> memo;
  if (const auto it = memo.constFind(raw); it != memo.cend()) return *it;
  if (memo.size() > 4096) memo.clear();
  return *memo.insert(raw, specOf(raw));
}
static ContractSpec specOf(const QString& raw) {
  ContractSpec c;
  const QString clean = cleanSymbol(raw);
  const QString app = aliases().value(clean, clean);
  const market::Instrument* in = market::find(app);
  if (!in) {
    // unknown 6-letter FX pair (exotics the catalogue lacks)
    static const QRegularExpression sixUpper("^[A-Z]{6}$");
    if (clean.size() == 6 && sixUpper.match(clean).hasMatch() &&
        market::majorCurrencies().contains(clean.right(3))) {
      c.display = clean.left(3) + "/" + clean.right(3); c.tag = "FX"; c.contract = 100000; c.unit = "units";
      c.quoteCcy = clean.right(3); c.pip = c.quoteCcy == "JPY" ? 0.01 : 0.0001; c.decimals = c.quoteCcy == "JPY" ? 3 : 5;
      return c;
    }
    c.display = raw.trimmed(); c.tag = "—"; c.quoteCcy = "USD"; c.decimals = 2; c.pip = 0.01; c.unit = "units";
    return c;
  }
  c.app = in->symbol; c.decimals = in->decimals;
  switch (in->category) {
    case market::Category::Forex:
      c.display = in->base + "/" + in->quote; c.tag = "FX"; c.contract = 100000; c.unit = "units";
      c.quoteCcy = in->quote; c.pip = (in->quote == "JPY" || in->quote == "HUF") ? 0.01 : 0.0001;
      break;
    case market::Category::Index:
      c.display = in->name; c.tag = "IDX"; c.contract = 1; c.unit = "units"; c.quoteCcy = indexCcy(in->symbol); c.pip = 1;
      break;
    case market::Category::Commodity:
      c.display = in->name; c.quoteCcy = "USD"; c.pip = 0.01;
      if (in->symbol == "XAUUSD") { c.contract = 100; c.unit = "oz"; c.tag = "METAL"; }
      else if (in->symbol == "XAGUSD") { c.contract = 5000; c.unit = "oz"; c.tag = "METAL"; c.pip = 0.001; }
      else if (in->symbol == "XPTUSD" || in->symbol == "XPDUSD") { c.contract = 100; c.unit = "oz"; c.tag = "METAL"; }
      else if (in->symbol == "COPPER") { c.contract = 25000; c.unit = "lb"; c.tag = "METAL"; c.pip = 0.0001; }
      else if (in->symbol == "NATGAS") { c.contract = 10000; c.unit = "MMBtu"; c.tag = "ENERGY"; c.pip = 0.001; }
      else if (in->symbol == "GASOLINE" || in->symbol == "HEATOIL") {   // quoted in USD per gallon (RB=F, HO=F)
        c.contract = 42000; c.unit = "gallons"; c.tag = "ENERGY"; c.pip = 0.0001;
      }
      else if (in->group == "Energy") { c.contract = 1000; c.unit = "barrels"; c.tag = "ENERGY"; c.pip = 0.01; }
      else {
        // exchange contract sizes (CBOT / ICE / CME) and the quote unit of the prices shown (Yahoo futures):
        // grains in US cents per bushel, softs and cattle in cents per pound, cocoa in USD per tonne
        c.tag = "AGRI";
        struct Agri { const char* sym; double contract; const char* unit; double priceUnit; };
        static const Agri agri[] = {
          {"CORN", 5000, "bushels", 0.01}, {"WHEAT", 5000, "bushels", 0.01}, {"SOYBEAN", 5000, "bushels", 0.01},
          {"COFFEE", 37500, "lb", 0.01}, {"SUGAR", 112000, "lb", 0.01}, {"COTTON", 50000, "lb", 0.01},
          {"CATTLE", 40000, "lb", 0.01}, {"COCOA", 10, "tonnes", 1}};
        c.contract = 1; c.unit = "contracts";
        for (const auto& a : agri) if (in->symbol == QLatin1String(a.sym)) { c.contract = a.contract; c.unit = a.unit; c.priceUnit = a.priceUnit; }
      }
      break;
    case market::Category::Crypto:
      c.display = in->name; c.tag = "CRYPTO"; c.contract = 1; c.unit = "coins"; c.quoteCcy = "USD"; c.pip = 1;
      break;
    case market::Category::Stock:
      c.display = in->name; c.tag = "STOCK"; c.contract = 1; c.unit = "shares"; c.quoteCcy = "USD"; c.pip = 0.01;
      break;
    case market::Category::Bond:
      c.display = in->name; c.tag = "BOND"; c.contract = 1; c.unit = "units"; c.quoteCcy = "USD"; c.pip = 0.001;
      break;
  }
  return c;
}

QString displaySymbol(const QString& raw) { return spec(raw).display; }

static double pairPrice(const QString& sym) {
  const auto q = market::QuoteHub::instance().quote(sym);
  return q.valid() && q.price > 0 ? q.price : NAN;
}

double fxRate(const QString& from, const QString& to) {
  if (from.isEmpty() || to.isEmpty() || from == to) return 1.0;
  if (market::find(from + to)) { const double p = pairPrice(from + to); if (std::isfinite(p)) return p; }
  if (market::find(to + from)) { const double p = pairPrice(to + from); if (std::isfinite(p)) return 1.0 / p; }
  if (from != "USD" && to != "USD") {
    const double a = fxRate(from, "USD"), b = fxRate("USD", to);
    if (std::isfinite(a) && std::isfinite(b)) return a * b;
  }
  return NAN;
}

QStringList fxSymbolsFor(const QString& from, const QString& to) {
  if (from.isEmpty() || to.isEmpty() || from == to) return {};
  if (market::find(from + to)) return {from + to};
  if (market::find(to + from)) return {to + from};
  QStringList out;
  if (from != "USD") out << fxSymbolsFor(from, "USD");
  if (to != "USD") out << fxSymbolsFor("USD", to);
  return out;
}
}
