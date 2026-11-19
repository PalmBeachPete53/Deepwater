#include <QDateTime>
#include "news/History.h"
#include <QCoreApplication>
#include "net/Http.h"
#include "net/Sources.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>
#include <QLocale>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <map>
#include <memory>
#include <cmath>

namespace news {

static const sources::Registrar regOecd({"oecd", "OECD", {"sdmx.oecd.org"}, sources::Access::Keyless, 3500,
    "Consumer prices, unemployment rates and GDP growth by country (release histories)",
    "OECD data are free to reuse with attribution (CC BY 4.0); the API allows 20 requests per minute", {"news/calendar"}});
static const sources::Registrar regValet({"boc-valet", "Bank of Canada", {"www.bankofcanada.ca"}, sources::Access::Keyless,
    1000, "Core CPI measures (CPI-trim, CPI-median, CPI-common)",
    "Bank of Canada terms: data may be reproduced with attribution", {"news/calendar"}});
static const sources::Registrar regBls({"bls", "US Bureau of Labor Statistics", {"api.bls.gov"}, sources::Access::Keyless,
    2000, "Nonfarm payrolls (CES0000000001)", "US government work, public domain; keyless API v1 (25 queries a day)",
    {"news/calendar"}});

namespace {
enum class Prov { Oecd, Valet, Bls };
enum class Tf { None, PctChange, Annualize, DiffK };
struct Def {
  QString id; QString ccy; QStringList titles; Prov prov; QString query; Tf tf; QString unit; QString source; bool q = false;
};

const QString kPrices = "OECD.SDD.TPS,DSD_PRICES@DF_PRICES_ALL,1.0/";
const QString kLfs = "OECD.SDD.TPS,DSD_LFS@DF_IALFS_UNE_M,1.0/";
const QString kQna = "OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA_EXPENDITURE_GROWTH_OECD,1.1/";

QList<Def> defs() {
  QList<Def> d;
  struct C { QString ccy, iso; };
  const QList<C> cs{{"USD", "USA"}, {"EUR", "EA20"}, {"GBP", "GBR"}, {"JPY", "JPN"}, {"CHF", "CHE"},
                    {"CAD", "CAN"}, {"AUD", "AUS"}, {"NZD", "NZL"}};
  for (const auto& c : cs) {
    QStringList yy{"cpi y/y"}, core{"core cpi y/y"};
    if (c.ccy == "EUR") { yy << "cpi flash estimate y/y" << "final cpi y/y"; core << "core cpi flash estimate y/y" << "final core cpi y/y"; }
    if (c.ccy == "AUD") yy << "monthly cpi indicator y/y";
    d << Def{"cpi-yy-" + c.iso, c.ccy, yy, Prov::Oecd, kPrices + c.iso + ".M.N.CPI.PA._T.N.GY", Tf::None, "%",
             QT_TRANSLATE_NOOP("news", "OECD · consumer prices, all items, y/y")};
    d << Def{"core-yy-" + c.iso, c.ccy, core, Prov::Oecd, kPrices + c.iso + ".M.N.CPI.PA._TXCP01_NRG.N.GY", Tf::None, "%",
             QT_TRANSLATE_NOOP("news", "OECD · consumer prices less food and energy, y/y")};
    if (c.ccy == "USD")
      d << Def{"cpi-mm-USA", "USD", {"cpi m/m"}, Prov::Oecd, kPrices + "USA.M.N.CPI.IX._T.S._Z", Tf::PctChange, "%",
               QT_TRANSLATE_NOOP("news", "OECD · consumer prices, seasonally adjusted index, m/m (computed)")};
    else if (c.ccy != "NZD" && c.ccy != "AUD")
      d << Def{"cpi-mm-" + c.iso, c.ccy, {"cpi m/m"}, Prov::Oecd, kPrices + c.iso + ".M.N.CPI.PC._T.N.G1", Tf::None, "%",
               QT_TRANSLATE_NOOP("news", "OECD · consumer prices, all items, m/m")};
    if (c.ccy == "NZD")
      d << Def{"cpi-qq-NZL", "NZD", {"cpi q/q"}, Prov::Oecd, kPrices + "NZL.Q.N.CPI.PC._T.N.G1", Tf::None, "%",
               QT_TRANSLATE_NOOP("news", "OECD · consumer prices, all items, q/q"), true};
    if (c.ccy != "CHF" && c.ccy != "NZD")
      d << Def{"une-" + c.iso, c.ccy, {"unemployment rate"}, Prov::Oecd,
               kLfs + c.iso + ".UNE_LF_M.PT_LF_SUB._Z.Y._T.Y_GE15._Z.M", Tf::None, "%",
               QT_TRANSLATE_NOOP("news", "OECD · harmonised unemployment rate, % of labour force")};
    QStringList gdp{"gdp q/q", "prelim gdp q/q", "flash gdp q/q", "prelim flash gdp q/q", "revised gdp q/q",
                    "final gdp q/q", "second estimate gdp q/q"};
    if (c.ccy == "USD") gdp = {"advance gdp q/q", "prelim gdp q/q", "final gdp q/q"};
    d << Def{"gdp-" + c.iso, c.ccy, gdp, Prov::Oecd, kQna + "Q.Y." + c.iso + ".S1.S1.B1GQ._Z._Z._Z.PC.L.G1.T0102",
             c.ccy == "USD" ? Tf::Annualize : Tf::None, "%",
             c.ccy == "USD" ? QT_TRANSLATE_NOOP("news", "OECD · real GDP, q/q annualised (computed)") : QT_TRANSLATE_NOOP("news", "OECD · real GDP, q/q"), true};
  }
  d << Def{"cpi-trim", "CAD", {"trimmed cpi y/y"}, Prov::Valet, "CPI_TRIM", Tf::None, "%", QT_TRANSLATE_NOOP("news", "Bank of Canada · CPI-trim, y/y")};
  d << Def{"cpi-median", "CAD", {"median cpi y/y"}, Prov::Valet, "CPI_MEDIAN", Tf::None, "%", QT_TRANSLATE_NOOP("news", "Bank of Canada · CPI-median, y/y")};
  d << Def{"cpi-common", "CAD", {"common cpi y/y"}, Prov::Valet, "CPI_COMMON", Tf::None, "%", QT_TRANSLATE_NOOP("news", "Bank of Canada · CPI-common, y/y")};
  d << Def{"nfp", "USD", {"non-farm employment change"}, Prov::Bls, "CES0000000001", Tf::DiffK, "K",
           QT_TRANSLATE_NOOP("news", "BLS · total nonfarm payrolls, monthly change")};
  return d;
}

const Def* defFor(const Event& e) {
  static const QList<Def> all = defs();
  const QString t = e.title.toLower();
  for (const auto& d : all) if (d.ccy == e.currency && d.titles.contains(t)) return &d;
  return nullptr;
}

QDate periodDate(const QString& p) {
  if (p.size() == 7 && p[4] == '-' && p[5] == 'Q') return QDate(p.left(4).toInt(), (p.mid(6).toInt() - 1) * 3 + 1, 1);
  if (p.size() == 7 && p[4] == '-') return QDate(p.left(4).toInt(), p.mid(5).toInt(), 1);
  return QDate::fromString(p.left(10), Qt::ISODate);
}

// OECD SDMX csv: keep the series with the most observations.
QVector<QPair<QDate, double>> parseOecd(const QByteArray& body) {
  const QList<QByteArray> lines = body.split('\n');
  if (lines.isEmpty()) return {};
  const QList<QByteArray> head = lines[0].trimmed().split(',');
  const int ti = head.indexOf("TIME_PERIOD"), vi = head.indexOf("OBS_VALUE");
  if (ti < 0 || vi < 0) return {};
  std::map<QByteArray, std::map<QDate, double>> series;
  for (int i = 1; i < lines.size(); ++i) {
    const QList<QByteArray> f = lines[i].trimmed().split(',');
    if (f.size() <= std::max(ti, vi)) continue;
    bool ok = false; const double v = f[vi].toDouble(&ok);
    const QDate d = periodDate(QString::fromLatin1(f[ti]));
    if (!ok || !d.isValid()) continue;
    QByteArray key; for (int k = 0; k < ti; ++k) key += f[k] + ',';
    series[key][d] = v;
  }
  const std::map<QDate, double>* best = nullptr;
  for (const auto& [k, s] : series) if (!best || s.size() > best->size()) best = &s;
  QVector<QPair<QDate, double>> out;
  if (best) for (const auto& [d, v] : *best) out << qMakePair(d, v);
  return out;
}

QVector<QPair<QDate, double>> parseValet(const QByteArray& body, const QString& id) {
  QVector<QPair<QDate, double>> out;
  for (const auto& o : QJsonDocument::fromJson(body).object()["observations"].toArray()) {
    const auto obj = o.toObject();
    bool ok = false; const double v = obj[id].toObject()["v"].toString().toDouble(&ok);
    const QDate d = QDate::fromString(obj["d"].toString(), Qt::ISODate);
    if (ok && d.isValid()) out << qMakePair(d, v);
  }
  return out;
}

QVector<QPair<QDate, double>> parseBls(const QByteArray& body) {
  QVector<QPair<QDate, double>> out;
  const auto series = QJsonDocument::fromJson(body).object()["Results"].toObject()["series"].toArray();
  for (const auto& s : series)
    for (const auto& o : s.toObject()["data"].toArray()) {
      const auto obj = o.toObject();
      const QString per = obj["period"].toString();
      if (!per.startsWith('M') || per == "M13") continue;
      bool ok = false; const double v = obj["value"].toString().toDouble(&ok);
      if (ok) out << qMakePair(QDate(obj["year"].toString().toInt(), per.mid(1).toInt(), 1), v);
    }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  return out;
}

void transform(QVector<QPair<QDate, double>>& s, Tf tf) {
  if (tf == Tf::None) return;
  QVector<QPair<QDate, double>> out;
  for (int i = 0; i < s.size(); ++i) {
    if (tf == Tf::Annualize) { out << qMakePair(s[i].first, (std::pow(1 + s[i].second / 100, 4) - 1) * 100); continue; }
    if (i == 0) continue;
    if (tf == Tf::PctChange) out << qMakePair(s[i].first, (s[i].second / s[i - 1].second - 1) * 100);
    else out << qMakePair(s[i].first, s[i].second - s[i - 1].second);   // payrolls are in thousands already
  }
  s = out;
}
}

History& History::instance() { static History* h = new History; return *h; }
bool History::hasOfficial(const Event& e) const { return defFor(e) != nullptr; }

void History::fetch(const Event& e, QObject* ctx, std::function<void(const Series&)> cb) {
  QPointer<QObject> guard(ctx);
  Series base; base.title = e.name; base.country = Calendar::countryOf(e.currency);
  const Def* d = defFor(e);
  auto recorded = [e, base]() {
    Series s = base; s.recorded = true; s.unit = e.actual.has() ? e.actual.suffix : e.previous.suffix;
    s.source = QT_TRANSLATE_NOOP("news", "recorded calendar releases");   // translated when shown
    for (const auto& [t, v] : Calendar::instance().recordedReleases(e.seriesKey)) { s.dates << t.date(); s.values << v.v; }
    if (s.values.isEmpty())
      s.error = QCoreApplication::translate("news", "No release history yet: no official series is mapped to this indicator, and Deepwater records "
                "each actual from now on.");
    return s;
  };
  if (!d) { QTimer::singleShot(0, ctx, [cb, s = recorded()] { cb(s); }); return; }
  // the memory copy follows the request's 12 h TTL: kept for good, a session left open for days never showed the
  // releases published meanwhile
  if (m_cachedAt.value(d->id) && QDateTime::currentMSecsSinceEpoch() - m_cachedAt.value(d->id) > 12LL * 3600 * 1000) {
    m_cache.remove(d->id); m_cachedAt.remove(d->id);
  }
  if (auto it = m_cache.constFind(d->id); it != m_cache.cend()) {
    Series s = *it; s.title = base.title; s.country = base.country;
    QTimer::singleShot(0, ctx, [cb, s] { cb(s); });
    return;
  }
  const Def def = *d;
  auto done = [this, def, base, cb, guard, recorded](QVector<QPair<QDate, double>> pts, const QString& err) {
    transform(pts, def.tf);
    Series s = base; s.unit = def.unit; s.source = def.source; s.quarterly = def.q;
    for (const auto& [dt, v] : pts) if (dt.year() >= 2000) { s.dates << dt; s.values << v; }
    if (s.values.isEmpty()) { s = recorded(); if (!err.isEmpty() && s.values.isEmpty()) s.error = QCoreApplication::translate("news", "Official series unavailable (%1).").arg(err); }
    else { m_cache.insert(def.id, s); m_cachedAt.insert(def.id, QDateTime::currentMSecsSinceEpoch()); }
    if (guard) cb(s);
  };
  net::Request r; r.ttlSec = 12 * 3600;
  if (def.prov == Prov::Oecd) {
    r.url = QUrl("https://sdmx.oecd.org/public/rest/data/" + def.query +
                 "?startPeriod=2000-01&dimensionAtObservation=AllDimensions&format=csv");
    r.headers << qMakePair(QByteArray("Accept-Language"), QByteArray("en"));   // Qt's default "en-US,*" → HTTP 500
    // the OECD API answers 500/429 now and then under load: one retry after a few seconds
    net::get(r, this, [this, done, r](const net::Reply& rep) {
      if (rep.body.isEmpty() && (rep.status >= 500 || rep.status == 429 || rep.status == 0)) {
        QTimer::singleShot(5000, this, [this, done, r] {
          net::get(r, this, [done](const net::Reply& rep2) {
            done(parseOecd(rep2.body), rep2.ok() || rep2.stale ? QString() : QString("OECD HTTP %1").arg(rep2.status));
          });
        });
        return;
      }
      done(parseOecd(rep.body), rep.ok() || rep.stale ? QString() : QString("OECD HTTP %1").arg(rep.status));
    });
  } else if (def.prov == Prov::Valet) {
    r.url = QUrl("https://www.bankofcanada.ca/valet/observations/" + def.query + "/json?start_date=2000-01-01");
    const QString id = def.query;
    net::get(r, this, [done, id](const net::Reply& rep) { done(parseValet(rep.body, id), rep.ok() ? QString() : QString("Bank of Canada HTTP %1").arg(rep.status)); });
  } else {
    // BLS v1 answers at most 10 years per request: two requests cover 20 years
    const int y = QDate::currentDate().year();
    auto parts = std::make_shared<QVector<QPair<QDate, double>>>();
    auto left = std::make_shared<int>(2);
    for (int k = 0; k < 2; ++k) {
      net::Request rk = r;
      rk.url = QUrl(QString("https://api.bls.gov/publicAPI/v1/timeseries/data/%1?startyear=%2&endyear=%3")
                        .arg(def.query).arg(y - 19 + 10 * k).arg(y - 10 + 10 * k));
      net::get(rk, this, [done, parts, left](const net::Reply& rep) {
        *parts += parseBls(rep.body);
        if (--*left == 0) {
          std::sort(parts->begin(), parts->end(), [](const auto& a, const auto& b) { return a.first < b.first; });
          QVector<QPair<QDate, double>> u;
          for (const auto& p : *parts) if (u.isEmpty() || u.last().first != p.first) u << p;
          done(u, {});
        }
      });
    }
  }
}

// ------------------------------------------------------------------------------------------------ official actuals
namespace {
struct ActualDef { Def def; QString provLabel; };
// Series whose observations are exactly the published release figure (no estimate in between).
QList<ActualDef> actualDefs() {
  QList<ActualDef> out;
  for (const auto& d : defs()) {
    if (d.tf != Tf::None) continue;                          // computed transforms are not the published figure
    if (d.ccy == "GBP" && d.id.startsWith("une-")) continue; // UK rate is a 3-month average labelled differently
    out << ActualDef{d, d.prov == Prov::Valet ? "Bank of Canada Valet" : "OECD"};
  }
  // BLS computes CPI percent changes from indexes rounded to three decimals: the same computation here gives
  // exactly the published m/m figure.
  out << ActualDef{Def{"bls-cpi-sa", "USD", {"cpi m/m"}, Prov::Bls, "CUSR0000SA0", Tf::PctChange, "%", QT_TRANSLATE_NOOP("news", "BLS · CPI-U, SA, m/m")}, "BLS"};
  out << ActualDef{Def{"bls-core-sa", "USD", {"core cpi m/m"}, Prov::Bls, "CUSR0000SA0L1E", Tf::PctChange, "%", QT_TRANSLATE_NOOP("news", "BLS · core CPI-U, SA, m/m")}, "BLS"};
  out << ActualDef{Def{"bls-ppi-sa", "USD", {"ppi m/m"}, Prov::Bls, "WPSFD4", Tf::PctChange, "%", QT_TRANSLATE_NOOP("news", "BLS · PPI final demand, SA, m/m")}, "BLS"};
  out << ActualDef{Def{"bls-coreppi-sa", "USD", {"core ppi m/m"}, Prov::Bls, "WPSFD49104", Tf::PctChange, "%", QT_TRANSLATE_NOOP("news", "BLS · PPI final demand less foods and energy, SA, m/m")}, "BLS"};
  out << ActualDef{Def{"deu-cpi-mm", "EUR", {"german prelim cpi m/m", "german final cpi m/m"}, Prov::Oecd,
                       kPrices + "DEU.M.N.CPI.PC._T.N.G1", Tf::None, "%", QT_TRANSLATE_NOOP("news", "OECD · Germany consumer prices, m/m")}, "OECD"};
  out << ActualDef{Def{"deu-cpi-yy", "EUR", {"german prelim cpi y/y", "german final cpi y/y"}, Prov::Oecd,
                       kPrices + "DEU.M.N.CPI.PA._T.N.GY", Tf::None, "%", QT_TRANSLATE_NOOP("news", "OECD · Germany consumer prices, y/y")}, "OECD"};
  return out;
}
const ActualDef* actualDefFor(const Event& e) {
  static const QList<ActualDef> all = actualDefs();
  const QString t = e.title.toLower();
  for (const auto& d : all) if (d.def.ccy == e.currency && d.def.titles.contains(t)) return &d;
  return nullptr;
}
}

bool History::canOfficial(const Event& e) const { return actualDefFor(e) != nullptr; }

void History::officialActual(const Event& e, QObject* ctx, std::function<void(const Value&, const QString&)> cb) {
  const ActualDef* ad = actualDefFor(e);
  QPointer<QObject> guard(ctx);
  if (!ad || e.actual.has() || e.time > QDateTime::currentDateTimeUtc() || (!e.previous.num() && !e.forecast.num())) {
    QTimer::singleShot(0, ctx, [cb] { cb({}, {}); }); return;
  }
  const Def def = ad->def;
  const QString provLabel = ad->provLabel;
  auto decide = [e, def, provLabel, cb, guard](QVector<QPair<QDate, double>> pts) {
    if (!guard) return;
    if (def.tf == Tf::PctChange) {   // from 3-decimal indexes, as BLS does
      for (auto& p : pts) p.second = std::round(p.second * 1000) / 1000;
      transform(pts, def.tf);
    }
    const QDate rel = e.time.toUTC().date();
    QDate target;
    if (def.q) {
      const QDate qs(rel.year(), ((rel.month() - 1) / 3) * 3 + 1, 1);
      target = qs.addMonths(-3);
      if (target.addMonths(3).daysTo(rel) > 100) { cb({}, {}); return; }
    } else {
      target = QDate(rel.year(), rel.month(), 1).addMonths(-1);
      // flash / preliminary monthly figures come out at the end of the month they measure (German prelim CPI on
      // the 30th, euro-area flash HICP on the last working day) or in the first days of the next one
      const bool early = e.title.contains("flash", Qt::CaseInsensitive) || e.title.contains("prelim", Qt::CaseInsensitive);
      if (early && rel.day() >= 15) target = QDate(rel.year(), rel.month(), 1);
    }
    int i = -1;
    for (int k = 0; k < pts.size(); ++k) if (pts[k].first == target) i = k;
    if (i < 0) { cb({}, {}); return; }
    const Value& like = e.previous.num() ? e.previous : e.forecast;
    const int dec = std::max(e.previous.num() ? e.previous.decimals : 0, e.forecast.num() ? e.forecast.decimals : 0);
    if (like.suffix != def.unit && !(def.unit == "%" && like.suffix == "%")) { cb({}, {}); return; }
    // the observation before must equal the calendar's previous at its precision ("final" releases: their previous
    // is the flash estimate of the same period, so the period's own observation is checked against it)
    const bool final = e.title.startsWith("Final", Qt::CaseInsensitive) || e.title.contains(" Final ", Qt::CaseInsensitive);
    if (e.previous.num()) {
      const int j = final ? i : i - 1;
      if (j < 0) { cb({}, {}); return; }
      const double f = std::pow(10.0, e.previous.decimals);
      if (std::abs(std::round(pts[j].second * f) / f - e.previous.v) > 0.5 / f + 1e-9) { cb({}, {}); return; }
    } else if (final) { cb({}, {}); return; }
    // too close to a rounding boundary to know the published digit: leave it empty
    {
      const double f = std::pow(10.0, dec), x = std::abs(pts[i].second) * f;
      if (std::abs(x - std::floor(x) - 0.5) < 0.02) { cb({}, {}); return; }
    }
    Value out = like; out.decimals = dec;
    out.text = formatLike(pts[i].second, out);
    out = parseValue(out.text);
    const QString per = def.q ? QString("Q%1 %2").arg((target.month() - 1) / 3 + 1).arg(target.year())
                              : QLocale(QLocale::English).toString(target, "MMM yyyy");
    cb(out, QString("%1 official series\n%2, %3 = %4 (previous checked against the calendar)")
                .arg(provLabel, def.source, per, QString::number(pts[i].second, 'f', 3)));
  };
  net::Request r; r.ttlSec = 3 * 3600;
  if (def.prov == Prov::Oecd) {
    r.url = QUrl("https://sdmx.oecd.org/public/rest/data/" + def.query +
                 "?startPeriod=2000-01&dimensionAtObservation=AllDimensions&format=csv");
    r.headers << qMakePair(QByteArray("Accept-Language"), QByteArray("en"));
    net::get(r, this, [decide](const net::Reply& rep) { decide(parseOecd(rep.body)); });
  } else if (def.prov == Prov::Valet) {
    r.url = QUrl("https://www.bankofcanada.ca/valet/observations/" + def.query + "/json?start_date=2000-01-01");
    const QString id = def.query;
    net::get(r, this, [decide, id](const net::Reply& rep) { decide(parseValet(rep.body, id)); });
  } else {
    const int y = QDate::currentDate().year();
    r.url = QUrl(QString("https://api.bls.gov/publicAPI/v1/timeseries/data/%1?startyear=%2&endyear=%3").arg(def.query).arg(y - 1).arg(y));
    net::get(r, this, [decide](const net::Reply& rep) { decide(parseBls(rep.body)); });
  }
}
}
