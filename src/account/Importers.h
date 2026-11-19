#pragma once
#include "account/Journal.h"
#include <QStringList>

// Statement importers. No broker password is ever needed: the user exports a file from the trading platform.
//   MT4  — Account History › Save as Detailed Report (DetailedStatement.htm)
//   MT5  — History › Report › HTML (ReportHistory-<login>.html, UTF-16 or UTF-8)
//   cTrader — History tab › export to CSV
//   Generic CSV — any file with a header row (symbol, side/type, open/close time, prices, size, profit…)
//   Deepwater bridge — the file written by the EA / cBot in resources/account/bridge (live sync)
// Statement times are in the broker's server time unless the file says otherwise; serverUtcOffsetMin moves
// them to UTC (MT4/MT5 servers are often UTC+2 / UTC+3).
namespace account {

struct ImportResult {
  QString format;                 // "MT4 detailed statement", "MT5 report", "cTrader CSV", "CSV", "Deepwater bridge"
  QList<Trade> trades;
  double deposits = NAN;          // net deposits found in the file (starting balance), NAN if none
  QString currency, login, name, broker, platform;
  QStringList warnings;
  int skipped = 0;                // rows that looked like trades but could not be read
  QString error;                  // non-empty: nothing usable
  int offsetMin = 0;              // server offset applied to the times (the file's own zone wins over the user's)
  bool offsetFromUser = false;    // the times were moved by the offset the user chose (it can be changed afterwards)
  bool timesVerified = false;     // bridge: the EA converted to UTC with a checked offset ("times=verified")
  bool ok() const { return error.isEmpty() && !trades.isEmpty(); }
};

ImportResult importFile(const QString& path, int serverUtcOffsetMin = 0);
ImportResult importData(const QByteArray& data, const QString& fileName, int serverUtcOffsetMin = 0);

// Parsers, exposed for tests.
ImportResult parseHtmlStatement(const QString& html, int serverUtcOffsetMin);
ImportResult parseCsv(const QString& text, int serverUtcOffsetMin);
ImportResult parseBridge(const QString& text);
// Numbers as written in statements and typed in forms ("1 234.50", "-0,500", "(12.30)", "1.234,5", "157,250").
// Rule for the comma (documented here because "157,250" is ambiguous):
//  - a comma and a dot together: the last of the two is the decimal mark ("1,234.5", "1.234,5");
//  - several commas: thousands groups when every group after the first has 3 digits ("1,234,567"), else unreadable;
//  - a single comma: decimalComma = true → always the decimal mark (forms, risk calculator, files whose other
//    numbers use a decimal comma); decimalComma = false → thousands only when it is a valid group ("157,250",
//    not "0,500", "12,5" or "1,08523", which are decimals).
//  - decimalComma = true and several dots forming thousands groups ("1.234.567"): grouping.
double parseNumber(QString s, bool decimalComma = false);
QDateTime parseTime(const QString& s, int utcOffsetMin, bool dayFirst = true);
}
