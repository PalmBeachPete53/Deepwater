#include "trials/Stats.h"
#include "charts/core/Quant.h"
#include "charts/core/Series.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>

namespace trials {

QString backtestFamily(const QString& market, int tfSeconds) { return QString("bt|%1|%2").arg(market).arg(tfSeconds); }

QString fingerprint(const QByteArray& what) {
  return QString::fromLatin1(QCryptographicHash::hash(what, QCryptographicHash::Sha1).toHex().left(12));
}

QString strategyConfig(const QJsonObject& strategy) {
  QJsonObject s = strategy;
  s.remove("name");
  return fingerprint(QJsonDocument(s).toJson(QJsonDocument::Compact));   // QJsonObject keeps its keys sorted
}

QString pineConfig(const QString& source) { return fingerprint(source.toUtf8()); }

QString ruleLabel(const QJsonObject& strategy) {
  QStringList r;
  for (const char* k : {"entryLong", "exitLong", "entryShort", "exitShort"})
    for (const QJsonValue& x : strategy.value(k).toArray()) {
      const QJsonObject o = x.toObject();
      r << QString("%1: %2 %3 %4").arg(k, charts::bt::jsString(o.value("left")), charts::bt::jsString(o.value("op")),
                                       charts::bt::jsString(o.value("right")));
    }
  QStringList risk;   // what changes the trades besides the rules
  for (const char* k : {"stopLossPct", "takeProfitPct", "trailingStopPct", "maxBarsInTrade", "positionPct", "feePct", "slippagePct", "spreadPct", "exitMatch"})
    if (strategy.contains(k)) risk << QString(k) + " " + charts::bt::jsString(strategy.value(k));
  return "rules: " + r.join("; ").left(220) + (risk.isEmpty() ? QString() : " | " + risk.join(", "));
}

void setReturns(Trial& t, const QVector<charts::bt::EquityPoint>& e) {
  QVector<double> r;
  r.reserve(e.size());
  for (int i = 1; i < e.size(); ++i) r << (e[i - 1].value > 0 ? e[i].value / e[i - 1].value - 1 : 0.0);
  const auto m = charts::quant::moments(r);
  const double sr = charts::quant::sharpe(r);
  t.n = int(r.size());
  t.sr = std::isfinite(sr) ? sr : 0.0;
  t.skew = std::isfinite(m.skew) ? m.skew : 0.0;
  t.kurt = std::isfinite(m.kurt) ? m.kurt : 3.0;
  if (e.isEmpty()) return;
  t.from = e.first().time;
  t.to = e.last().time;
  const double ppy = e.size() > 1 && t.to > t.from ? (365.0 * 86400 * (e.size() - 1)) / double(t.to - t.from) : 252;
  t.sharpe = t.sr * std::sqrt(ppy);
  // compounded per UTC day: each day's last equity against the day before's (the first day against the first point)
  auto dayOf = [](qint64 s) { return s >= 0 ? s / 86400 : (s - 86399) / 86400; };
  t.day0 = dayOf(t.from);
  const qint64 days = dayOf(t.to) - t.day0 + 1;
  if (days <= 0 || days > 200000) return;
  QVector<double> last(days, NAN);
  for (const auto& p : e) last[dayOf(p.time) - t.day0] = p.value;
  t.daily.resize(int(days));
  double prev = e.first().value;
  for (int d = 0; d < days; ++d) {
    if (std::isnan(last[d])) { t.daily[d] = 0; continue; }
    t.daily[d] = prev > 0 ? float(last[d] / prev - 1) : 0.0f;
    prev = last[d];
  }
}

double psrOf(double sr, double n, double skew, double kurt, double benchmark) {
  if (!std::isfinite(sr) || n < 10) return NAN;
  const double denom = std::sqrt(std::max(1e-12, 1 - skew * sr + ((kurt - 1) / 4) * sr * sr));
  return charts::quant::normCdf(((sr - benchmark) * std::sqrt(n - 1)) / denom);
}

Deflated deflate(const QList<Trial>& latest) {
  Deflated d;
  if (latest.isEmpty()) return d;
  QVector<double> srs;
  int best = -1;
  for (int i = 0; i < latest.size(); ++i) {
    if (!std::isfinite(latest[i].sr)) continue;
    srs << latest[i].sr;
    if (best < 0 || latest[i].sr > latest[best].sr) best = i;
  }
  d.ok = true;
  d.tries = int(latest.size());
  if (best < 0) { d.best = latest.first(); return d; }
  d.best = latest[best];
  double var = 0;
  if (srs.size() > 1) {
    const auto m = charts::quant::moments(srs);
    var = m.sd * m.sd * (double(srs.size()) / (srs.size() - 1));
  }
  d.sr0 = charts::quant::expectedMaxSharpe(std::max<double>(d.tries, 1), var);
  d.dsr = psrOf(d.best.sr, d.best.n, d.best.skew, d.best.kurt, d.sr0);
  return d;
}

Tried tried(const QList<Trial>& rows, const QString& me) {
  QSet<QString> all, user, mine, others;
  for (const Trial& t : rows) {
    all.insert(t.config);
    if (t.origin == "user") user.insert(t.config);
    else if (!me.isEmpty() && t.who.startsWith(me)) mine.insert(t.config);
    else others.insert(t.config);
  }
  return {int(all.size()), int(user.size()), int(mine.size()), int(others.size())};
}

std::optional<TriesPbo> pboAcross(qint64 day0, const QVector<float>& self, const QList<QPair<qint64, QVector<float>>>& others) {
  const int T = int(self.size());
  if (T < 40) return std::nullopt;
  QVector<QVector<double>> matrix;
  matrix.push_back(QVector<double>(self.cbegin(), self.cend()));
  for (const auto& [d0, d] : others) {
    QVector<double> row(T, 0.0);
    int covered = 0;
    for (int i = 0; i < T; ++i) {
      const qint64 k = day0 + i - d0;
      if (k < 0 || k >= d.size()) continue;
      row[i] = d[int(k)];
      ++covered;
    }
    if (covered >= 0.75 * T) matrix.push_back(row);   // days it was not run: no position
  }
  if (matrix.size() < 3) return std::nullopt;
  const auto p = charts::quant::pbo(matrix, 8);
  if (!p) return std::nullopt;
  return TriesPbo{*p, int(matrix.size()), T};
}

int familySeconds(const QString& family) { return family.startsWith("bt|") ? family.section('|', -1).toInt() : 0; }

QList<Trial> visibleAt(const QList<Trial>& rows, qint64 asOf) {
  if (asOf <= 0) return rows;
  QList<Trial> out;
  for (const Trial& t : rows)
    if (t.to > 0 && t.to + familySeconds(t.family) <= asOf) out << t;
  return out;
}

void withRegistry(charts::quant::Checks& c, const QVector<charts::bt::EquityPoint>& baseEquity, const QString& family,
                  const QString& config, const Registry& reg, qint64 asOf) {
  if (c.cancelled || family.isEmpty()) return;
  QList<Trial> others;
  for (const Trial& t : latestPerConfig(visibleAt(reg.family(family), asOf)))
    if (t.config != config) others << t;
  if (others.size() > 200) others = others.mid(others.size() - 200);   // the 200 strategies tried last
  if (others.isEmpty()) return;
  Trial self;
  setReturns(self, baseEquity);
  QVector<double> baseR;
  for (int i = 1; i < baseEquity.size(); ++i)
    baseR << (baseEquity[i - 1].value > 0 ? baseEquity[i].value / baseEquity[i - 1].value - 1 : 0.0);

  // the deflated Sharpe: the nearby settings and the other strategies, all tries on this data
  const int own = int(c.trialSharpes.size());
  QVector<double> srs = c.trialSharpes;
  int counted = 0;
  for (const Trial& t : others)
    if (std::isfinite(t.sr)) { srs << t.sr; ++counted; }
  const double var = srs.size() > 1 ? std::pow(charts::quant::moments(srs).sd, 2) * (double(srs.size()) / (srs.size() - 1)) : 0;
  const double sr0 = charts::quant::expectedMaxSharpe(std::max<double>(srs.size(), 1), var);
  const double deflated = charts::quant::psr(baseR, sr0);
  const double raw = charts::quant::sharpe(baseR);
  const double ann = std::isfinite(raw) && raw != 0 && std::isfinite(c.base.sharpe) && c.base.sharpe != 0 ? c.base.sharpe / raw
                     : std::isfinite(self.sharpe) && self.sr != 0 ? self.sharpe / self.sr : 1;
  for (auto& g : c.gates) {
    if (g.key != "dsr" || std::isnan(deflated)) continue;
    g.value = QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1 probability the edge is real after %2 tries on this data: %3 nearby settings and %4 other strategies (needs above 0.95)"))
                  .arg(charts::series::toFixed(deflated, 3))
                  .arg(srs.size())
                  .arg(own)
                  .arg(counted);
    g.pass = deflated > 0.95;
    c.dsr = deflated;
    c.sr0 = sr0 * ann;
    c.trials = int(srs.size());
  }

  // overfitting across the strategies tried on this data
  QList<QPair<qint64, QVector<float>>> dailies;
  for (const Trial& t : others) {
    qint64 d0 = 0;
    QVector<float> d = reg.daily(t.id, &d0);
    if (!d.isEmpty()) dailies << qMakePair(d0, d);
  }
  if (const auto p = pboAcross(self.day0, self.daily, dailies))
    c.gates.push_back({"pbo_tries", QT_TRANSLATE_NOOP("charts", "Overfitting across tries (PBO)"),
                       QStringLiteral(QT_TRANSLATE_NOOP("charts", "%1% chance the best of the %2 strategies tried on this data is below average on unseen data (needs under 50%)"))
                           .arg(charts::series::toFixed(p->pbo.pbo * 100, 1))
                           .arg(p->strategies),
                       p->pbo.pbo < 0.5});
  c.passed = 0;
  for (const auto& g : c.gates)
    if (g.pass) c.passed++;
}

}  // namespace trials
