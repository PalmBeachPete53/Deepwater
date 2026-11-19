#include "charts/core/FootprintSettings.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace charts::flow {

namespace {
double numOr(const QJsonValue& v, double def) {
  if (v.isDouble() && std::isfinite(v.toDouble())) return v.toDouble();
  if (v.isString()) {
    bool ok = false;
    const double d = v.toString().toDouble(&ok);
    if (ok && std::isfinite(d)) return d;
  }
  return def;
}
QString colorOr(const QJsonValue& v) {
  static const QRegularExpression hex("^#[0-9a-fA-F]{6}$");
  const QString s = v.toString().trimmed();
  return hex.match(s).hasMatch() ? s.toLower() : QString();
}
}  // namespace

QString FootprintSettings::modeKey(Mode m) {
  switch (m) {
    case Mode::Delta: return QStringLiteral("delta");
    case Mode::Profile: return QStringLiteral("profile");
    default: return QStringLiteral("bidask");
  }
}

FootprintSettings FootprintSettings::fromJson(const QJsonObject& o) {
  FootprintSettings s;
  const QString m = o.value("mode").toString();
  s.mode = m == "delta" ? Mode::Delta : m == "profile" ? Mode::Profile : Mode::BidAsk;
  const double rows = numOr(o.value("rows"), 0);
  s.rows = rows >= 1 ? int64_t(std::min(1e9, std::floor(rows))) : 0;
  s.ratio = std::clamp(numOr(o.value("ratio"), 3.0), 1.0, 100.0);
  s.minVolume = std::max(0.0, numOr(o.value("minVolume"), 0.0));
  s.stackMin = int(std::clamp(numOr(o.value("stack"), 3.0), 2.0, 20.0));
  s.showPoc = o.contains("poc") ? o.value("poc").toBool(true) : true;
  s.showTotals = o.contains("totals") ? o.value("totals").toBool(true) : true;
  s.buyColor = colorOr(o.value("buyColor"));
  s.sellColor = colorOr(o.value("sellColor"));
  s.pocColor = colorOr(o.value("pocColor"));
  return s;
}

QJsonObject FootprintSettings::toJson() const {
  const FootprintSettings d;
  QJsonObject o;
  if (mode != d.mode) o["mode"] = modeKey(mode);
  if (rows != d.rows) o["rows"] = double(rows);
  if (ratio != d.ratio) o["ratio"] = ratio;
  if (minVolume != d.minVolume) o["minVolume"] = minVolume;
  if (stackMin != d.stackMin) o["stack"] = stackMin;
  if (showPoc != d.showPoc) o["poc"] = showPoc;
  if (showTotals != d.showTotals) o["totals"] = showTotals;
  if (!buyColor.isEmpty()) o["buyColor"] = buyColor;
  if (!sellColor.isEmpty()) o["sellColor"] = sellColor;
  if (!pocColor.isEmpty()) o["pocColor"] = pocColor;
  return o;
}

FootprintParams FootprintSettings::params() const {
  FootprintParams p;
  p.groupTicks = rows > 0 ? rows : 1;
  p.ratio = ratio;
  p.minVolume = minVolume;
  p.stackMin = stackMin;
  return p;
}

}  // namespace charts::flow
