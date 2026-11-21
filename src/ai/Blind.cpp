#include "ai/Blind.h"
#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTimeZone>
#include <cmath>
#include <cstring>

namespace ai::blind {

namespace {
// the names to hide, longest first (a shorter one inside a longer one is replaced with it)
QList<QPair<QString, QString>> names(const Mask& m) {
  const QString sym = m.market.section(':', -1);                    // BTCUSDT
  QString base = sym;
  for (const char* q : {"USDT", "USDC", "BUSD", "USD", "EUR"})
    if (base.endsWith(q) && base.size() > int(strlen(q))) { base.chop(int(strlen(q))); break; }   // BTC
  QList<QPair<QString, QString>> n{{m.market, alias(m)}, {sym, "ASSET1"}, {base + "/USDT", "ASSET1"}, {base + "/USD", "ASSET1"},
                                   {base + "USD", "ASSET1"}, {"Bitcoin", "the asset"}, {"bitcoin", "the asset"}, {"Binance", "the exchange"},
                                   {"BINANCE", "EXCHANGE"}, {base, "ASSET1"}};
  return n;
}
QString shifted(const QRegularExpressionMatch& d, int years) {
  const int y = d.captured(1).toInt(), mo = d.captured(2).toInt();
  if (d.captured(3).isEmpty()) return QString("%1-%2").arg(y + years, 4, 10, QChar('0')).arg(mo, 2, 10, QChar('0'));   // a month
  QDate day(y, mo, d.captured(3).toInt());
  if (!day.isValid()) return d.captured(0);
  return day.addYears(years).toString("yyyy-MM-dd") + d.captured(4);   // the time, if any, unchanged
}
// "2026-11-14", "2026-11-14 13:20", "2026-11-14T13:20:05", "2026-11": years shifted (the time of day kept)
const QRegularExpression& dateRe() {
  static const QRegularExpression re(R"((?<![\d.])(\d{4})-(\d{2})(?:-(\d{2})((?:[ T]\d{2}:\d{2}(?::\d{2})?)?))?(?![\d-]))");
  return re;
}
QString shiftAll(const QString& text, int years) {
  QString out;
  qsizetype last = 0;
  auto it = dateRe().globalMatch(text);
  while (it.hasNext()) {
    const auto m = it.next();
    const int y = m.captured(1).toInt();
    if (y < 1990 || y > 2100) continue;   // not a year: a number like 4500-12
    out += text.mid(last, m.capturedStart() - last) + shifted(m, years);
    last = m.capturedEnd();
  }
  return out + text.mid(last);
}
}  // namespace

Mask make(quint32 seed, const QString& market, double refPrice) {
  Mask m;
  m.on = true;
  m.market = market;
  m.refPrice = refPrice;
  quint32 x = seed * 2654435761u + 1013904223u;
  m.years = 9 + int(x % 5);
  x = x * 1664525u + 1013904223u;
  m.factor = std::pow(10.0, -2.3 + 1.0 * (double(x % 10000) / 10000.0));   // 0.005 … 0.05
  return m;
}

QString alias(const Mask&) { return QStringLiteral("MARKET:ASSET1"); }

QString shiftDate(const Mask& m, qint64 t) {
  return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).addYears(m.years).toString("yyyy-MM-dd HH:mm");
}

QString out(const Mask& m, const QString& textIn) {
  if (!m.on) return textIn;
  QString text = shiftAll(textIn, m.years);
  for (const auto& [from, to] : names(m)) text.replace(from, to);
  if (m.refPrice > 0) {   // the price level's numbers, scaled, with as many decimals as they had
    static const QRegularExpression num(R"((?<![\w.\-:])(\d{1,7}(?:\.\d+)?)(?![\w.]|-\d))");
    QString o;
    qsizetype last = 0;
    auto it = num.globalMatch(text);
    while (it.hasNext()) {
      const auto mm = it.next();
      const double v = mm.captured(1).toDouble();
      if (v < m.refPrice / 2 || v > m.refPrice * 2) continue;
      const int dot = int(mm.captured(1).indexOf('.'));
      const int dec = dot < 0 ? 0 : int(mm.captured(1).size()) - dot - 1;
      const double w = v * m.factor;
      // a scaled price keeps enough digits to stay a price (66998 × 0.0137 = 917.87, not 918)
      const int keep = std::max(dec, w < 10 ? 4 : w < 1000 ? 2 : 0);
      o += text.mid(last, mm.capturedStart() - last) + QString::number(w, 'f', keep);
      last = mm.capturedEnd();
    }
    text = o + text.mid(last);
  }
  return text;
}

QJsonObject in(const Mask& m, const QJsonObject& args) {
  if (!m.on) return args;
  QJsonObject out;
  for (auto it = args.begin(); it != args.end(); ++it) {
    QJsonValue v = it.value();
    if (v.isString()) {
      QString s = shiftAll(v.toString(), -m.years);
      if (s.contains("ASSET1") || s.compare("the asset", Qt::CaseInsensitive) == 0)
        s = s.contains("MARKET:") ? m.market : m.market.section(':', -1);
      v = s;
    } else if (v.isObject()) {
      v = in(m, v.toObject());
    }
    out.insert(it.key(), v);
  }
  return out;
}

}  // namespace ai::blind
