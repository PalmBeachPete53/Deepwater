#include "macro/YieldCurves.h"
#include "macro/MacroStore.h"
#include "macro/Csv.h"
#include "net/Http.h"
#include "net/Sources.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QLocale>
#include <memory>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QRegularExpression>

namespace macro {
namespace {
const sources::Registrar regUst({"ustreasury", "U.S. Department of the Treasury (daily interest rate data)", {"home.treasury.gov"},
    sources::Access::Keyless, 2000, "Daily Treasury par yield curve rates", "US government work, public domain",
    {"economy/macro"}});
const sources::Registrar regMof({"mof-japan", "Ministry of Finance Japan (JGB interest rates)", {"www.mof.go.jp"},
    sources::Access::Keyless, 3000, "Japanese government bond yields by maturity (daily, CSV)",
    "Public statistics, reuse with source (Government of Japan standard terms, CC BY 4.0 compatible)", {"economy/macro"}});

double yearsOf(const QString& l) {
  const QString s = l.toLower();
  const double n = QString(s).remove(QRegularExpression("[^0-9.]")).toDouble();
  if (s.contains("mo") || s.endsWith('m')) return n / 12.0;
  return n;
}
}

YieldCurves& YieldCurves::instance() { static YieldCurves* y = new YieldCurves; return *y; }

YieldCurves::YieldCurves() {
  // language changed: re-request the curves already shown (served from the HTTP cache) so their labels follow
  connect(&i18n::Notifier::instance(), &i18n::Notifier::changed, this, [this] {
    const auto isos = m_curves.keys();
    m_started.clear();
    for (const auto& iso : isos) request(iso);
  });
}

Curve YieldCurves::curve(const QString& iso2) const { return m_curves.value(iso2.toLower()); }

void YieldCurves::put(const Curve& c) {
  m_pending.remove(c.iso2);
  m_curves[c.iso2] = c;
  emit updated(c.iso2);
}

void YieldCurves::fallback(const QString& iso) {
  auto& ms = MacroStore::instance();
  const Latest s = ms.latest(iso, Ind::Rate3m), l = ms.latest(iso, Ind::Yield10y);
  Curve c; c.iso2 = iso; c.monthly = true; c.source = QCoreApplication::translate("economy", "OECD monthly averages (3-month interbank, 10-year government)");
  if (s.valid()) c.pts << CurvePoint{"3 Mo", 0.25, s.value};
  if (l.valid()) c.pts << CurvePoint{"10 Yr", 10, l.value};
  c.asOf = l.valid() ? l.period : s.period;
  m_pending.remove(iso);
  m_curves[iso] = c;
  emit updated(iso);
  if (c.ok() && !ms.loading()) return;
  // OECD rates not (all) in yet: refresh when the store updates, once per economy
  if (m_waiting.contains(iso)) return;
  m_waiting.insert(iso);
  auto conn = std::make_shared<QMetaObject::Connection>();
  *conn = connect(&ms, &MacroStore::updated, this, [this, iso, conn] {
    auto& m = MacroStore::instance();
    if (!m_curves.value(iso).monthly) { disconnect(*conn); m_waiting.remove(iso); return; }
    const Latest s2 = m.latest(iso, Ind::Rate3m), l2 = m.latest(iso, Ind::Yield10y);
    Curve c2; c2.iso2 = iso; c2.monthly = true; c2.source = m_curves.value(iso).source;
    if (s2.valid()) c2.pts << CurvePoint{"3 Mo", 0.25, s2.value};
    if (l2.valid()) c2.pts << CurvePoint{"10 Yr", 10, l2.value};
    c2.asOf = l2.valid() ? l2.period : s2.period;
    if (c2.pts.size() != m_curves.value(iso).pts.size()) { m_curves[iso] = c2; emit updated(iso); }
    if (!m.loading()) { disconnect(*conn); m_waiting.remove(iso); }
  });
}

void YieldCurves::request(const QString& code) {
  const QString iso = code.toLower();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (m_started.contains(iso) && now - m_started[iso] < 30 * 60 * 1000) return;
  m_started[iso] = now;
  m_pending.insert(iso);
  const QLocale en(QLocale::English);

  if (iso == "us") {
    net::Request r; r.ttlSec = 3 * 3600;
    const int y = QDate::currentDate().year();
    r.url = QUrl(QString("https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/%1/all"
                         "?type=daily_treasury_yield_curve&field_tdr_date_value=%1&page&_format=csv").arg(y));
    net::get(r, this, [this](const net::Reply& rep) {
      const auto rows = csv::parse(rep.body);
      if (!rep.ok() || rows.size() < 2 || rows[0].value(0) != "Date") { fallback("us"); return; }
      // newest first
      Curve c; c.iso2 = "us"; c.source = QCoreApplication::translate("economy", "US Treasury daily par yield curve");
      c.asOf = QDate::fromString(rows[1][0], "MM/dd/yyyy");
      for (int k = 1; k < rows[0].size() && k < rows[1].size(); ++k) {
        bool ok = false; const double v = rows[1][k].toDouble(&ok);
        if (!ok) continue;
        QString lab = rows[0][k]; lab.replace("Month", "Mo");
        c.pts << CurvePoint{lab, yearsOf(lab), v};
      }
      put(c);
    });
    return;
  }
  if (iso == "ez") {
    net::Request r; r.ttlSec = 3 * 3600;
    r.url = QUrl("https://data-api.ecb.europa.eu/service/data/YC/B.U2.EUR.4F.G_N_A.SV_C_YM."
                 "SR_3M+SR_6M+SR_1Y+SR_2Y+SR_3Y+SR_5Y+SR_7Y+SR_10Y+SR_15Y+SR_20Y+SR_30Y?lastNObservations=1&format=csvdata");
    net::get(r, this, [this](const net::Reply& rep) {
      const auto rows = csv::parse(rep.body);
      if (!rep.ok() || rows.size() < 2) { fallback("ez"); return; }
      const int ik = rows[0].indexOf("KEY"), it = rows[0].indexOf("TIME_PERIOD"), iv = rows[0].indexOf("OBS_VALUE");
      // a 200 answer that is not the CSV (an error page): without the three columns, rows[i][-1] was read below
      if (ik < 0 || it < 0 || iv < 0) { fallback("ez"); return; }
      Curve c; c.iso2 = "ez"; c.source = QCoreApplication::translate("economy", "ECB euro area yield curve (AAA-rated central government bonds, spot rates)");
      for (int i = 1; i < rows.size(); ++i) {
        if (rows[i].size() <= qMax(ik, qMax(it, iv))) continue;
        const QString t = rows[i][ik].section("SR_", 1);
        const bool m = t.endsWith('M');
        const int n = QString(t).chopped(1).toInt();
        c.pts << CurvePoint{QString("%1 %2").arg(n).arg(m ? "Mo" : "Yr"), m ? n / 12.0 : n, rows[i][iv].toDouble()};
        c.asOf = QDate::fromString(rows[i][it], "yyyy-MM-dd");
      }
      std::sort(c.pts.begin(), c.pts.end(), [](auto& a, auto& b) { return a.years < b.years; });
      if (c.ok()) put(c); else fallback("ez");
    });
    return;
  }
  if (iso == "jp") {
    auto parse = [](const QByteArray& body) {
      Curve c; c.iso2 = "jp"; c.source = QCoreApplication::translate("economy", "Ministry of Finance Japan, JGB yields (constant maturity)");
      const auto rows = csv::parse(body);
      int hdr = -1;
      for (int i = 0; i < rows.size(); ++i) if (rows[i].value(0) == "Date") { hdr = i; break; }
      if (hdr < 0) return c;
      for (int i = rows.size() - 1; i > hdr; --i) {
        const QDate d = QDate::fromString(rows[i].value(0), "yyyy/M/d");
        if (!d.isValid()) continue;
        for (int k = 1; k < rows[hdr].size() && k < rows[i].size(); ++k) {
          bool ok = false; const double v = rows[i][k].toDouble(&ok);
          if (ok) c.pts << CurvePoint{QString(rows[hdr][k]).replace("Y", " Yr"), yearsOf(rows[hdr][k]), v};
        }
        c.asOf = d;
        break;
      }
      return c;
    };
    net::Request r; r.ttlSec = 3 * 3600;
    r.url = QUrl("https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/jgbcme.csv");
    net::get(r, this, [this, parse](const net::Reply& rep) {
      Curve c = rep.ok() ? parse(rep.body) : Curve{};
      if (c.ok()) { put(c); return; }
      net::Request a; a.ttlSec = 24 * 3600; a.timeoutMs = 60000;
      a.url = QUrl("https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/historical/jgbcme_all.csv");
      net::get(a, this, [this, parse](const net::Reply& r2) {
        Curve c2 = r2.ok() ? parse(r2.body) : Curve{};
        if (c2.ok()) put(c2); else fallback("jp");
      });
    });
    return;
  }
  if (iso == "ca") {
    auto curve = std::make_shared<Curve>();
    curve->iso2 = "ca"; curve->source = QCoreApplication::translate("economy", "Bank of Canada (Treasury bills and benchmark bond yields)");
    auto left = std::make_shared<int>(2);
    auto done = [this, curve, left] {
      if (--*left) return;
      std::sort(curve->pts.begin(), curve->pts.end(), [](auto& a, auto& b) { return a.years < b.years; });
      if (curve->ok()) put(*curve); else fallback("ca");
    };
    net::Request b; b.ttlSec = 3 * 3600;
    b.url = QUrl("https://www.bankofcanada.ca/valet/observations/group/bond_yields_all/json?recent=1");
    net::get(b, this, [curve, done](const net::Reply& rep) {
      const auto obs = QJsonDocument::fromJson(rep.body).object().value("observations").toArray();
      if (rep.ok() && !obs.isEmpty()) {
        const auto o = obs.last().toObject();
        static const QList<QPair<QString, QPair<QString, double>>> m{
          {"BD.CDN.2YR.DQ.YLD", {"2 Yr", 2}}, {"BD.CDN.3YR.DQ.YLD", {"3 Yr", 3}}, {"BD.CDN.5YR.DQ.YLD", {"5 Yr", 5}},
          {"BD.CDN.7YR.DQ.YLD", {"7 Yr", 7}}, {"BD.CDN.10YR.DQ.YLD", {"10 Yr", 10}}, {"BD.CDN.LONG.DQ.YLD", {"Long", 30}}};
        for (const auto& [k, lab] : m) {
          const QString v = o.value(k).toObject().value("v").toString();
          if (!v.isEmpty()) curve->pts << CurvePoint{lab.first, lab.second, v.toDouble()};
        }
        curve->asOf = QDate::fromString(o.value("d").toString(), "yyyy-MM-dd");
      }
      done();
    });
    net::Request t; t.ttlSec = 3 * 3600;
    t.url = QUrl("https://www.bankofcanada.ca/valet/observations/group/tbill_all/json?recent=1");
    net::get(t, this, [curve, done](const net::Reply& rep) {
      const auto obs = QJsonDocument::fromJson(rep.body).object().value("observations").toArray();
      if (rep.ok() && !obs.isEmpty()) {
        const auto o = obs.last().toObject();
        static const QList<QPair<QString, QPair<QString, double>>> m{
          {"V80691303", {"3 Mo", 0.25}}, {"V80691304", {"6 Mo", 0.5}}, {"V80691305", {"1 Yr", 1}}};
        for (const auto& [k, lab] : m) {
          const QString v = o.value(k).toObject().value("v").toString();
          if (!v.isEmpty()) curve->pts << CurvePoint{lab.first, lab.second, v.toDouble()};
        }
      }
      done();
    });
    return;
  }
  if (iso == "gb") {
    net::Request r; r.ttlSec = 3 * 3600;
    const QString from = en.toString(QDate::currentDate().addDays(-20), "dd/MMM/yyyy");
    r.url = QUrl("https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp?csv.x=yes&Datefrom=" + from +
                 "&Dateto=now&SeriesCodes=IUDSNPY,IUDMNPY,IUDLNPY&CSVF=TN&UsingCodes=Y&VPD=Y&VFD=N");
    net::get(r, this, [this, en](const net::Reply& rep) {
      const auto rows = csv::parse(rep.body);
      if (!rep.ok() || rows.size() < 2 || rows[0].value(0) != "DATE") { fallback("gb"); return; }
      Curve c; c.iso2 = "gb"; c.source = QCoreApplication::translate("economy", "Bank of England nominal gilt par yields");
      for (int i = rows.size() - 1; i >= 1; --i) {
        bool ok = false; rows[i].value(1).toDouble(&ok);
        if (!ok) continue;
        const QStringList labs{"5 Yr", "10 Yr", "20 Yr"};
        for (int k = 0; k < 3; ++k) c.pts << CurvePoint{labs[k], double(QList<int>{5, 10, 20}[k]), rows[i].value(k + 1).toDouble()};
        c.asOf = en.toDate(rows[i][0], "dd MMM yyyy");
        break;
      }
      if (c.ok()) put(c); else fallback("gb");
    });
    return;
  }
  fallback(iso);
}
}
