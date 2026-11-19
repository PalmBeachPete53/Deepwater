#include "ui/Format.h"
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace fmt {
static const QLocale& en() { static QLocale l(QLocale::English, QLocale::UnitedStates); return l; }
QString num(double v, int d, bool group) {
  if (!std::isfinite(v)) return dash();
  QString s = en().toString(v, 'f', d);
  if (!group) s.remove(',');
  return s;
}
QString signedNum(double v, int d) {
  if (!std::isfinite(v)) return dash();
  const QString s = num(std::abs(v), d);
  const bool zero = num(0, d) == s;
  return zero ? s : (v > 0 ? "+" : "-") + s;
}
QString pct(double v, int d, bool sign) { return (sign ? signedNum(v, d) : num(v, d)) + "%"; }
QString compact(double v, int d) {
  if (!std::isfinite(v)) return dash();
  const double a = std::abs(v);
  if (a >= 1e12) return num(v / 1e12, d) + "T";
  if (a >= 1e9) return num(v / 1e9, d) + "B";
  if (a >= 1e6) return num(v / 1e6, d) + "M";
  if (a >= 1e3) return num(v / 1e3, d) + "K";
  return num(v, d);
}
int priceDecimals(double v, const QString& sym) {
  const QString s = sym.toUpper();
  if (s.contains("JPY") || s.contains("HUF")) return 3;
  if (s.size() == 6 && !s.contains("XAU") && !s.contains("XAG") && !s.contains("BTC") && !s.contains("ETH")) return 5;
  const double a = std::abs(v);
  if (a >= 1000) return 2;
  if (a >= 10) return 2;
  if (a >= 1) return 4;
  return 5;
}
QString price(double v, const QString& sym) { return num(v, priceDecimals(v, sym)); }
QString hms(const QDateTime& t) { return t.toLocalTime().toString("HH:mm:ss"); }
QString hm(const QDateTime& t) { return t.toLocalTime().toString("HH:mm"); }
QString ago(const QDateTime& t, const QDateTime& now) {
  const qint64 s = t.secsTo(now);
  if (s < 60) return QString("%1s ago").arg(qMax<qint64>(0, s));
  if (s < 3600) return QString("%1m ago").arg(s / 60);
  if (s < 86400) return QString("%1h ago").arg(s / 3600);
  return QString("%1d ago").arg(s / 86400);
}
QString dash() { return QStringLiteral("—"); }
QString whole(double v) { return std::isfinite(v) ? QString::number(roundInt(v)) : dash(); }
int roundInt(double v) {
  if (!std::isfinite(v)) return 0;
  return int(std::lround(std::clamp(v, -2147483648.0, 2147483647.0)));
}
}
