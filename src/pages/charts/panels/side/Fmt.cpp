// Charts tab — side panel formats. Port of quant-terminal public/js/api.js helpers
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). See Fmt.h.
#include "pages/charts/panels/side/Fmt.h"
#include "charts/core/Backtest.h"
#include "core/I18n.h"
#include <QLocale>
#include <cmath>

namespace charts::ui::side {

namespace {
const QLocale& en() {
  static const QLocale l(QLocale::English, QLocale::UnitedStates);
  return l;
}
bool english() { return i18n::locale().language() == QLocale::English; }
}  // namespace

QString fmtNum(double v, int d) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  const double a = std::abs(v);
  if (a >= 1e12) return QString::number(v / 1e12, 'f', 2) + "T";
  if (a >= 1e9) return QString::number(v / 1e9, 'f', 2) + "B";
  if (a >= 1e6) return QString::number(v / 1e6, 'f', 2) + "M";
  if (a >= 1e4) return QString::number(v / 1e3, 'f', 1) + "K";
  return QString::number(v, 'f', d);
}

QString fmtPrice(double v) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  const double a = std::abs(v);
  if (a >= 1e6) return fmtNum(v);
  const int d = a >= 1000 ? 2 : a >= 1 ? (a >= 100 ? 2 : 4) : a >= 0.01 ? 6 : 8;
  const int minD = std::min(d, 2);
  // toLocaleString({minimumFractionDigits, maximumFractionDigits}): round to d, then drop zeros down to minD.
  QString s = en().toString(v, 'f', d);
  if (d > minD) {
    int cut = 0;
    while (cut < d - minD && s.endsWith('0')) { s.chop(1); ++cut; }
    if (s.endsWith('.')) s.chop(1);
  }
  return s;
}

QString fmtPct(double v) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  return (v >= 0 ? "+" : "") + QString::number(v, 'f', 2) + "%";
}

QString esc(const QString& s) {
  QString o;
  o.reserve(s.size());
  for (QChar c : s) {
    switch (c.unicode()) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      case '\'': o += "&#39;"; break;
      default: o += c;
    }
  }
  return o;
}

QString escv(const QJsonValue& v) {
  if (v.isNull() || v.isUndefined()) return {};
  return esc(bt::jsString(v));
}

QString ticker(const QString& id) {
  if (id.startsWith("SPREAD:")) {
    QStringList parts;
    for (const QString& x : id.mid(7).split(' ')) parts << (x.contains(':') ? x.split(':').last() : x);
    return parts.join("");
  }
  return id.split(':').last();
}

QString exchange(const QString& id) { return id.split(':').first(); }

QString localeDayLabel(const QDate& d) {
  if (english()) return en().toString(d, "dddd, MMM d");
  return i18n::locale().toString(d, "dddd d MMM");
}

QString localeTime(const QDateTime& t) {
  if (english()) return en().toString(t, "hh:mm AP");
  return i18n::locale().toString(t, "HH:mm");
}

QString localeDateTime(const QDateTime& t) {
  if (english()) return en().toString(t, "M/d/yyyy, h:mm:ss AP");
  return i18n::locale().toString(t, "dd/MM/yyyy HH:mm:ss");
}

QDateTime parseJsDate(const QString& s) {
  QDateTime t = QDateTime::fromString(s, Qt::ISODateWithMs);
  if (!t.isValid()) t = QDateTime::fromString(s, Qt::ISODate);
  if (!t.isValid()) t = QDateTime::fromString(s, Qt::RFC2822Date);
  return t.isValid() ? t.toLocalTime() : t;
}

}  // namespace charts::ui::side
