#pragma once
#include <QObject>
#include <QDate>
#include <QList>
#include <QHash>
#include <QStringList>
#include <cmath>
#include <functional>

// Banks' published forecasts per market and quarter, and the bias Deepwater derives from them.
//
//   auto& f = banks::Forecasts::instance();
//   f.refresh();                                          // reads every included bank's forecast tables
//   f.forecasts("EURUSD")                                 // every bank × quarter value for a market
//   f.calls(banks::AssetClass::FX)                        // one call per bank × market (needs spot quotes)
//   banks::Quarter::upcoming(6)                           // Q4 '26 … Q4 '27 + Q4 '28 (the overlay's columns)
//
// A "call" is NOT a bank's own trade recommendation: Deepwater compares the bank's forecast for the nearest
// quarter end with the current spot price (market::QuoteHub) and labels it LONG (forecast above spot by more than
// the class threshold), SHORT (below) or RANGE (within). Rates are read as bond calls: a forecast yield lower
// than today's yield is LONG (bond prices up). Averages and counts are computed by Deepwater too.
namespace banks {

enum class AssetClass { FX, AsiaEmFx, Commodities, Indices, Rates, Crypto };
QList<AssetClass> assetClasses();
QString className(AssetClass c);        // "FX", "ASIA & EM FX", …
QString classSubtitle(AssetClass c);    // "G10 MAJORS, CROSSES & THE DOLLAR", …
double classThreshold(AssetClass c);    // % move (yield points for rates) that separates LONG/SHORT from RANGE

struct Quarter {
  int year = 0, q = 0;                  // calendar quarter
  bool valid() const { return year > 0 && q >= 1 && q <= 4; }
  QDate end() const;                    // last day of the quarter
  QString label() const;                // "Q4 '26"
  bool operator==(const Quarter& o) const { return year == o.year && q == o.q; }
  bool operator<(const Quarter& o) const { return year != o.year ? year < o.year : q < o.q; }
  static Quarter of(const QDate& d);
  static Quarter parse(const QString& s); // "4Q26", "4Q26F", "Q4 '26"
  // The overlay's six columns: the current quarter and the next four, plus Q4 two years ahead.
  static QList<Quarter> upcoming(int n = 6, const QDate& today = QDate::currentDate());
};

struct Market {
  QString id;            // "EURUSD", "XAUUSD", "US10Y", "COPPER"
  QString display;       // "EUR/USD", "Gold", "UST 10Y" (English; name() translates)
  QString name() const;  // display in the interface language
  AssetClass cls;
  QString spot;          // symbol for market::QuoteHub / candles (app symbol or Yahoo ticker), empty if none
  double spotToForecast = 1.0;   // multiply the spot quote to get the forecast unit (copper $/lb → $/t)
  QString unit;          // "$/oz", "%", empty for FX
  int decimals = 4;
};
const QList<Market>& markets();                 // the universe of each class (coverage denominators)
const Market* market(const QString& id);
QList<Market> markets(AssetClass c);

struct Forecast {
  QString bankId;
  QString marketId;
  Quarter quarter;
  double value = NAN;
  bool average = false;  // quarterly average (commodities at ING) rather than end of period
  QDate revised;         // "Last updated" of the bank's table
  QString source;        // URL of the table
};

enum class Bias { Long, Short, Range };
struct Call {
  QString bankId, marketId;
  Bias bias = Bias::Range;
  Quarter horizon;
  double forecast = NAN, spot = NAN;
  double change = NAN;   // % (or yield points for rates) from spot to forecast
  QDate revised;
};

struct ClassSummary {
  int longs = 0, shorts = 0, ranges = 0;
  int banks = 0;          // banks with at least one call in the class
  int covered = 0;        // markets with at least one call
  int universe = 0;       // markets in the class
  int positions() const { return longs + shorts + ranges; }
};

class Forecasts : public QObject {
  Q_OBJECT
public:
  static Forecasts& instance();
  void refresh();
  QList<Forecast> forecasts(const QString& marketId = {}) const;
  QStringList banksFor(const QString& marketId) const;         // sorted by bank name
  // Calls need spot prices: pass the latest quote of each market's spot symbol (NaN when unknown).
  QList<Call> calls(AssetClass c, const std::function<double(const QString& spotSymbol)>& spotOf) const;
  ClassSummary summary(AssetClass c, const std::function<double(const QString& spotSymbol)>& spotOf) const;
  QStringList spotSymbols() const;                             // every spot symbol of the universe
  QStringList fxPairs() const;                                 // FX + Asia/EM pairs with forecasts, most covered first
  QStringList banksIncluded() const;                           // bank ids with at least one forecast
  bool loaded() const { return m_loaded; }
  QString error() const { return m_error; }
signals:
  void updated();
private:
  Forecasts();
  void readIng(const QByteArray& html, const QDateTime& fetched);
  QList<Forecast> m_all;
  bool m_loaded = false;
  QString m_error;
};

QString biasName(Bias b);   // LONG / SHORT / RANGE
}
