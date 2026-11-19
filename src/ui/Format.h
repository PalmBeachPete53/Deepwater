#pragma once
#include <QString>
#include <QDateTime>

// Number and time formats used across pages (English grouping, '−' never used: plain '-').
namespace fmt {
QString num(double v, int decimals, bool group = true);           // 52,215.20
QString signedNum(double v, int decimals);                         // +0.41 / -0.47
QString pct(double v, int decimals = 2, bool sign = true);         // +0.41%
QString compact(double v, int decimals = 1);                       // 30.3K, 1.2M, 4.5B
QString price(double v, const QString& symbol = {});               // decimals picked from the magnitude / pair
int priceDecimals(double v, const QString& symbol = {});
QString hms(const QDateTime& t);                                   // 16:37:43 local
QString hm(const QDateTime& t);                                    // 16:37
QString ago(const QDateTime& t, const QDateTime& now = QDateTime::currentDateTime());  // 12s ago, 3m ago, 1h ago, 2d ago
QString dash();                                                    // "—" for missing values
// Rounded to a whole number for display: "—" when not finite (std::lround / qRound of NaN or ±inf is undefined
// behaviour, and data from outside can be NaN).
QString whole(double v);
// The same as an int for arithmetic and geometry: 0 when not finite, clamped to the int range.
int roundInt(double v);
}
