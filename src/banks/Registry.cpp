#include "banks/Registry.h"
#include "net/Http.h"
#include "net/Sources.h"
#include <QJsonDocument>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QFontDatabase>
#include "ui/Theme.h"
#include <algorithm>

namespace banks {

static const sources::Registrar regSec({"sec-edgar", "U.S. Securities and Exchange Commission (EDGAR XBRL API)",
    {"data.sec.gov"}, sources::Access::Keyless, 250,
    "Total assets of the systemic banks (companyconcept us-gaap/ifrs-full Assets, latest 10-K/10-Q/20-F/6-K fact)",
    "Public domain (US government data). SEC fair-access policy: ≤ 10 requests/s and a descriptive User-Agent; "
    "the app sends its own UA (no e-mail). www.sec.gov answered 403 to that UA, data.sec.gov answers 200.",
    {"news/bank-research"}});

// Research links: research home pages the app only links to, declared so the registry knows them.
static const sources::Registrar regLinks({"bank-research-links", "Banks' research home pages (links only)",
    {}, sources::Access::Keyless, 0,
    "Nothing downloaded: the pages show a link that opens the bank's site in the browser",
    "Terms of 19 banks forbid reproduction or redistribution → links only; 9 more forbid automated access, deep "
    "links or citing → not linked (see banks::all(), checked 2026-11-14)",
    {"news/bank-research", "positioning/bank-positioning"}});

static Bank mk(QString id, QString name, QString legal, QString shortName, QString ticker, int cik, QString tax,
               QString ccy, Region reg, QString country, QString city, double lat, double lon, bool systemic,
               QString fsb, Logo logo) {
  Bank b;
  b.id = id; b.name = name; b.legalName = legal; b.shortName = shortName; b.ticker = ticker; b.cik = cik;
  b.taxonomy = tax; b.currency = ccy; b.region = reg; b.country = country; b.city = city; b.lat = lat; b.lon = lon;
  b.systemic = systemic; b.fsb = fsb; b.logo = logo;
  return b;
}

const QList<Bank>& all() {
  static const QList<Bank> list = [] {
    // FSB G-SIB membership: list of November 2024 (bundled by hand: fsb.org refuses automated requests with 403).
    const QString G = QT_TRANSLATE_NOOP("banks", "FSB G-SIB (Nov 2024 list)");
    const QString N = QT_TRANSLATE_NOOP("banks", "Not on the FSB G-SIB list (domestic systemic bank)");
    using R = Region;
    QList<Bank> l{
      mk("jpm", "JPMORGAN", "JPMorgan Chase & Co.", "JPM", "JPM", 19617, "us-gaap", "USD", R::Americas, "us", QT_TRANSLATE_NOOP("banks", "New York"), 40.7557, -73.9755, true, G, {QColor("#6e4b33"), QColor("#f3e6d6"), "J", 0}),
      mk("bac", "BANK OF AMERICA", "Bank of America Corp.", "BOFA", "BAC", 70858, "us-gaap", "USD", R::Americas, "us", QT_TRANSLATE_NOOP("banks", "Charlotte"), 35.2271, -80.8431, true, G, {QColor("#f6f6f6"), QColor("#d4202c"), "BA", 0}),
      mk("c", "CITI", "Citigroup Inc.", "CITI", "C", 831001, "us-gaap", "USD", R::Americas, "us", QT_TRANSLATE_NOOP("banks", "New York"), 40.7206, -74.0110, true, G, {QColor("#f6f6f6"), QColor("#1a5fb4"), "citi", 0}),
      mk("wfc", "WELLS FARGO", "Wells Fargo & Company", "WFC", "WFC", 72971, "us-gaap", "USD", R::Americas, "us", QT_TRANSLATE_NOOP("banks", "San Francisco"), 37.7924, -122.4005, true, G, {QColor("#d71e28"), QColor("#ffcd41"), "WF", 0}),
      mk("gs", "GOLDMAN SACHS", "The Goldman Sachs Group, Inc.", "GS", "GS", 886982, "us-gaap", "USD", R::Americas, "us", QT_TRANSLATE_NOOP("banks", "New York"), 40.7145, -74.0146, true, G, {QColor("#f6f6f6"), QColor("#4b6c94"), "GS", 0}),
      mk("ms", "MORGAN STANLEY", "Morgan Stanley", "MS", "MS", 895421, "us-gaap", "USD", R::Americas, "us", QT_TRANSLATE_NOOP("banks", "New York"), 40.7601, -73.9857, true, G, {QColor("#f6f6f6"), QColor("#1c3f74"), "MS", 0}),
      mk("ry", "RBC", "Royal Bank of Canada", "RBC", "RY", 1000275, "ifrs-full", "CAD", R::Americas, "ca", QT_TRANSLATE_NOOP("banks", "Toronto"), 43.6490, -79.3810, true, G, {QColor("#005daa"), QColor("#ffd200"), "RBC", 0}),
      mk("td", "TD", "The Toronto-Dominion Bank", "TD", "TD", 947263, "ifrs-full", "CAD", R::Americas, "ca", QT_TRANSLATE_NOOP("banks", "Toronto"), 43.6475, -79.3810, true, G, {QColor("#2e8b32"), QColor("#ffffff"), "TD", 0}),
      mk("hsbc", "HSBC", "HSBC Holdings plc", "HSBC", "HSBC", 1089113, "ifrs-full", "USD", R::Europe, "gb", QT_TRANSLATE_NOOP("banks", "London"), 51.5048, -0.0195, true, G, {QColor("#f6f6f6"), QColor("#db0011"), "◆", 0}),
      mk("bcs", "BARCLAYS", "Barclays PLC", "BARCLAYS", "BCS", 312069, "ifrs-full", "GBP", R::Europe, "gb", QT_TRANSLATE_NOOP("banks", "London"), 51.5050, -0.0170, true, G, {QColor("#00aeef"), QColor("#ffffff"), "B", 0}),
      mk("lyg", "LLOYDS", "Lloyds Banking Group plc", "LLOYDS", "LYG", 1160106, "ifrs-full", "GBP", R::Europe, "gb", QT_TRANSLATE_NOOP("banks", "London"), 51.5152, -0.0920, true, N, {QColor("#f6f6f6"), QColor("#111111"), "L", 0}),
      mk("nwg", "NATWEST", "NatWest Group plc", "NATWEST", "NWG", 844150, "ifrs-full", "GBP", R::Europe, "gb", QT_TRANSLATE_NOOP("banks", "Edinburgh"), 55.9533, -3.1883, true, N, {QColor("#42145f"), QColor("#e90c2e"), "N", 0}),
      mk("db", "DEUTSCHE BANK", "Deutsche Bank AG", "DB", "DB", 1159508, "ifrs-full", "EUR", R::Europe, "de", QT_TRANSLATE_NOOP("banks", "Frankfurt"), 50.1109, 8.6700, true, G, {QColor("#f6f6f6"), QColor("#0018a8"), "/", 0}),
      mk("ubs", "UBS", "UBS Group AG", "UBS", "UBS", 1610520, "ifrs-full", "USD", R::Europe, "ch", QT_TRANSLATE_NOOP("banks", "Zurich"), 47.3700, 8.5390, true, G, {QColor("#f6f6f6"), QColor("#e60000"), "UBS", 0}),
      mk("san", "SANTANDER", "Banco Santander, S.A.", "SANTANDER", "SAN", 891478, "ifrs-full", "EUR", R::Europe, "es", QT_TRANSLATE_NOOP("banks", "Madrid"), 40.4000, -3.8700, true, G, {QColor("#ec0000"), QColor("#ffffff"), "S", 0}),
      mk("bbva", "BBVA", "Banco Bilbao Vizcaya Argentaria, S.A.", "BBVA", "BBVA", 842180, "ifrs-full", "EUR", R::Europe, "es", QT_TRANSLATE_NOOP("banks", "Bilbao"), 43.2630, -2.9350, true, N, {QColor("#f6f6f6"), QColor("#004481"), "BB", 0}),
      mk("ing", "ING", "ING Groep N.V.", "ING", "ING", 1039765, "ifrs-full", "EUR", R::Europe, "nl", QT_TRANSLATE_NOOP("banks", "Amsterdam"), 52.3360, 4.8900, true, G, {QColor("#ff6200"), QColor("#ffffff"), "ING", 0}),
      mk("mufg", "MUFG", "Mitsubishi UFJ Financial Group, Inc.", "MUFG", "MUFG", 67088, "us-gaap", "JPY", R::Asia, "jp", QT_TRANSLATE_NOOP("banks", "Tokyo"), 35.6812, 139.7600, true, G, {QColor("#f6f6f6"), QColor("#e60000"), "", 1}),
      mk("smfg", "SMBC GROUP", "Sumitomo Mitsui Financial Group, Inc.", "SMBC", "SMFG", 1022837, "ifrs-full", "JPY", R::Asia, "jp", QT_TRANSLATE_NOOP("banks", "Tokyo"), 35.6860, 139.7580, true, G, {QColor("#f6f6f6"), QColor("#c8102e"), "", 3}),
      mk("mfg", "MIZUHO", "Mizuho Financial Group, Inc.", "MIZUHO", "MFG", 1335730, "us-gaap", "JPY", R::Asia, "jp", QT_TRANSLATE_NOOP("banks", "Tokyo"), 35.6860, 139.7700, true, G, {QColor("#f6f6f6"), QColor("#1b2a7a"), "M", 0}),
      mk("nmr", "NOMURA", "Nomura Holdings, Inc.", "NOMURA", "NMR", 1163653, "us-gaap", "JPY", R::Asia, "jp", QT_TRANSLATE_NOOP("banks", "Tokyo"), 35.6800, 139.7720, true, N, {QColor("#e60012"), QColor("#ffffff"), "", 4}),
      // Research-only banks (not in the 21-bank registry)
      mk("danske", "DANSKE BANK", "Danske Bank A/S", "DANSKE", "", 0, "", "DKK", R::Europe, "dk", QT_TRANSLATE_NOOP("banks", "Copenhagen"), 55.6800, 12.5900, false, {}, {QColor("#f6f6f6"), QColor("#003755"), "", 2}),
      mk("kbc", "KBC", "KBC Group NV", "KBC", "", 0, "", "EUR", R::Europe, "be", QT_TRANSLATE_NOOP("banks", "Brussels"), 50.8600, 4.3500, false, {}, {QColor("#00aeef"), QColor("#ffffff"), "KBC", 0}),
      mk("bnp", "BNP PARIBAS", "BNP Paribas SA", "BNP", "", 0, "", "EUR", R::Europe, "fr", QT_TRANSLATE_NOOP("banks", "Paris"), 48.8700, 2.3300, false, {}, {QColor("#00915a"), QColor("#ffffff"), "BNP", 0}),
      mk("wbc", "WESTPAC", "Westpac Banking Corporation", "WESTPAC", "", 0, "", "AUD", R::Asia, "au", QT_TRANSLATE_NOOP("banks", "Sydney"), -33.8650, 151.2090, false, {}, {QColor("#da1710"), QColor("#ffffff"), "W", 0}),
      mk("abn", "ABN AMRO", "ABN AMRO Bank N.V.", "ABN AMRO", "", 0, "", "EUR", R::Europe, "nl", QT_TRANSLATE_NOOP("banks", "Amsterdam"), 52.3380, 4.8720, false, {}, {QColor("#00857a"), QColor("#f2c500"), "AA", 0}),
      mk("swed", "SWEDBANK", "Swedbank AB", "SWEDBANK", "", 0, "", "SEK", R::Europe, "se", QT_TRANSLATE_NOOP("banks", "Stockholm"), 59.3500, 18.0300, false, {}, {QColor("#ff5f00"), QColor("#ffffff"), "S", 0}),
      mk("cagr", "CREDIT AGRICOLE", "Crédit Agricole S.A.", "CA", "", 0, "", "EUR", R::Europe, "fr", QT_TRANSLATE_NOOP("banks", "Montrouge"), 48.8160, 2.3160, false, {}, {QColor("#006a4e"), QColor("#ffffff"), "CA", 0}),
      mk("cbk", "COMMERZBANK", "Commerzbank AG", "CBK", "", 0, "", "EUR", R::Europe, "de", QT_TRANSLATE_NOOP("banks", "Frankfurt"), 50.1100, 8.6740, false, {}, {QColor("#ffcc00"), QColor("#002e3c"), "C", 0}),
      mk("socgen", "SOCIETE GENERALE", "Société Générale S.A.", "SG", "", 0, "", "EUR", R::Europe, "fr", QT_TRANSLATE_NOOP("banks", "Paris"), 48.8900, 2.2400, false, {}, {QColor("#e60028"), QColor("#ffffff"), "SG", 0}),
      mk("ucg", "UNICREDIT", "UniCredit S.p.A.", "UNICREDIT", "", 0, "", "EUR", R::Europe, "it", QT_TRANSLATE_NOOP("banks", "Milan"), 45.4840, 9.1900, false, {}, {QColor("#e2001a"), QColor("#ffffff"), "U", 0}),
      mk("nordea", "NORDEA", "Nordea Bank Abp", "NORDEA", "", 0, "", "EUR", R::Europe, "fi", QT_TRANSLATE_NOOP("banks", "Helsinki"), 60.1700, 24.9400, false, {}, {QColor("#0000a0"), QColor("#ffffff"), "N", 0}),
      mk("seb", "SEB", "Skandinaviska Enskilda Banken AB", "SEB", "", 0, "", "SEK", R::Europe, "se", QT_TRANSLATE_NOOP("banks", "Stockholm"), 59.3330, 18.0700, false, {}, {QColor("#60cd18"), QColor("#ffffff"), "SEB", 0}),
      mk("dnb", "DNB", "DNB Bank ASA", "DNB", "", 0, "", "NOK", R::Europe, "no", QT_TRANSLATE_NOOP("banks", "Oslo"), 59.9080, 10.7600, false, {}, {QColor("#007272"), QColor("#ffffff"), "DNB", 0}),
      mk("bns", "SCOTIABANK", "The Bank of Nova Scotia", "SCOTIA", "", 0, "", "CAD", R::Americas, "ca", QT_TRANSLATE_NOOP("banks", "Toronto"), 43.6490, -79.3790, false, {}, {QColor("#ec111a"), QColor("#ffffff"), "S", 0}),
      mk("cibc", "CIBC", "Canadian Imperial Bank of Commerce", "CIBC", "", 0, "", "CAD", R::Americas, "ca", QT_TRANSLATE_NOOP("banks", "Toronto"), 43.6480, -79.3800, false, {}, {QColor("#c41f3e"), QColor("#ffffff"), "CIBC", 0}),
    };
    auto set = [&](const QString& id, Use u, QString name, QString url, QString why) {
      for (auto& b : l) if (b.id == id) { b.research = u; b.researchName = name; b.researchUrl = url; b.researchWhy = why; }
    };
    set("ing", Use::Full, "ING THINK", "https://think.ing.com/",
        QT_TRANSLATE_NOOP("banks", "Terms of use: ING grants a licence to use, distribute and reproduce its content when ING Bank N.V. is named as copyright owner"));
    // Links only: terms forbid reproduction / redistribution (checked 2026-11-14)
    set("danske", Use::LinkOnly, "DANSKE BANK RESEARCH", "https://research.danskebank.com/research/",
        QT_TRANSLATE_NOOP("banks", "\"Any duplication or redistribution of the information on this site is prohibited\"; notes load through a private API"));
    set("bnp", Use::LinkOnly, "BNP PARIBAS ECONOMIC RESEARCH", "https://economic-research.bnpparibas.com/",
        QT_TRANSLATE_NOOP("banks", "Terms: content may not be copied, downloaded or reproduced without written permission"));
    set("wbc", Use::LinkOnly, "WESTPAC IQ", "https://www.westpaciq.com.au/economics",
        QT_TRANSLATE_NOOP("banks", "Material may not be copied or distributed to any other party without written permission"));
    set("abn", Use::LinkOnly, "ABN AMRO RESEARCH", "https://www.abnamro.com/research/en/our-research",
        QT_TRANSLATE_NOOP("banks", "Commercial use needs permission; the PDFs may not be reproduced or redistributed"));
    set("swed", Use::LinkOnly, "SWEDBANK MACRO RESEARCH", "https://www.swedbank-research.com/english/archive.csp",
        QT_TRANSLATE_NOOP("banks", "\"This material may not be reproduced without permission from Swedbank Macro Research\""));
    set("cagr", Use::LinkOnly, "CREDIT AGRICOLE ECONOMIC RESEARCH", "https://etudes-economiques.credit-agricole.com/",
        QT_TRANSLATE_NOOP("banks", "Any reproduction of the site, in whole or in part, is forbidden"));
    set("cbk", Use::LinkOnly, "COMMERZBANK RESEARCH", "https://www.commerzbank.de/group/research/",
        QT_TRANSLATE_NOOP("banks", "Any reproduction of information or data is prohibited without prior consent (forecast PDF public, not parsed)"));
    set("socgen", Use::LinkOnly, "SOCIETE GENERALE ECONOMIC RESEARCH", "https://www.societegenerale.com/en/economic-research",
        QT_TRANSLATE_NOOP("banks", "Any reproduction, including caching, is prohibited"));
    set("ucg", Use::LinkOnly, "UNICREDIT INVESTMENT INSTITUTE", "https://the-investment-institute.unicredit.eu/",
        QT_TRANSLATE_NOOP("banks", "Contents may not be reproduced without written authorisation, personal use only"));
    set("nordea", Use::LinkOnly, "NORDEA ECONOMIC OUTLOOK", "https://www.nordea.com/en/economic-outlook",
        QT_TRANSLATE_NOOP("banks", "Reproduction or storage forbidden except personal use; no parseable public table"));
    set("seb", Use::LinkOnly, "SEB RESEARCH", "https://research.sebgroup.com/",
        QT_TRANSLATE_NOOP("banks", "Reproduction subject to the originator's consent; data only through an API that robots.txt disallows"));
    set("dnb", Use::LinkOnly, "DNB CARNEGIE ECONOMIC OUTLOOK", "https://www.dnb.no/en/markets/research/economic-outlook",
        QT_TRANSLATE_NOOP("banks", "Site terms allow reproduction with DNB named as source, but the PDFs restrict reuse and robots.txt disallows *.pdf (borderline: ask DNB)"));
    set("bns", Use::LinkOnly, "SCOTIABANK ECONOMICS", "https://www.scotiabank.com/ca/en/about/economics/forecast-snapshot.html",
        QT_TRANSLATE_NOOP("banks", "Copying only for personal use; forecast tables may not be reproduced without written consent"));
    set("cibc", Use::LinkOnly, "CIBC ECONOMICS", "https://thoughtleadership.cibc.com/series/cibc-economics/",
        QT_TRANSLATE_NOOP("banks", "May not be copied or downloaded for public or commercial purposes without written permission"));
    set("wfc", Use::LinkOnly, "WELLS FARGO ECONOMICS", "https://www.wellsfargo.com/cib/insights/economics/",
        QT_TRANSLATE_NOOP("banks", "Reports may not be redistributed; downloads for personal or internal business purposes only"));
    set("jpm", Use::LinkOnly, "J.P. MORGAN ASSET MANAGEMENT", "https://am.jpmorgan.com/us/en/asset-management/adv/insights/market-insights/guide-to-the-markets/",
        QT_TRANSLATE_NOOP("banks", "May not be copied, transmitted or stored for subsequent use without written consent"));
    set("ms", Use::LinkOnly, "MORGAN STANLEY INSIGHTS", "https://www.morganstanley.com/insights",
        QT_TRANSLATE_NOOP("banks", "Personal and non-commercial use only"));
    set("bac", Use::LinkOnly, "BANK OF AMERICA INSTITUTE", "https://institute.bankofamerica.com/economic-insights.html",
        QT_TRANSLATE_NOOP("banks", "Bank of America terms forbid republishing or redistributing the content"));
    set("bcs", Use::LinkOnly, "BARCLAYS INSIGHTS", "https://www.ib.barclays/our-insights.html",
        QT_TRANSLATE_NOOP("banks", "No copying or commercial use of information or data without written permission; research behind a login"));
    // Excluded: automated access, deep links or citing forbidden by name, or no public content
    set("kbc", Use::Excluded, "KBC ECONOMICS", {}, QT_TRANSLATE_NOOP("banks", "Terms forbid transfer or reproduction and even deep links without consent"));
    set("mufg", Use::Excluded, "MUFG RESEARCH", {}, QT_TRANSLATE_NOOP("banks", "Research \"may not be quoted, circulated or otherwise referred to\" without written consent"));
    set("db", Use::Excluded, "DEUTSCHE BANK RESEARCH", {}, QT_TRANSLATE_NOOP("banks", "Terms forbid scraping, extracting or downloading by any automated program"));
    set("td", Use::Excluded, "TD ECONOMICS", {}, QT_TRANSLATE_NOOP("banks", "Terms forbid copying content by robot, spider, scraper or any automated means"));
    set("ry", Use::Excluded, "RBC ECONOMICS", {}, QT_TRANSLATE_NOOP("banks", "Terms forbid gathering or extracting content by spiders or screen scraping"));
    set("gs", Use::Excluded, "GOLDMAN SACHS INSIGHTS", {}, QT_TRANSLATE_NOOP("banks", "Terms forbid robots or spiders that retrieve, index or scrape the site"));
    set("c", Use::Excluded, "CITI INSTITUTE", {}, QT_TRANSLATE_NOOP("banks", "Terms: data scraping is strictly prohibited"));
    set("ubs", Use::Excluded, "UBS", {}, QT_TRANSLATE_NOOP("banks", "ubs.com refused the app's requests (HTTP 403)"));
    set("hsbc", Use::Excluded, "HSBC GLOBAL RESEARCH", {}, QT_TRANSLATE_NOOP("banks", "Research behind a client login; website terms not verified"));
    return l;
  }();
  return list;
}

QString trs(const QString& t) { return QCoreApplication::translate("banks", t.toUtf8().constData()); }

QList<Bank> systemic() { QList<Bank> out; for (const auto& b : all()) if (b.systemic) out << b; return out; }
QList<Bank> researchBanks(Use u) { QList<Bank> out; for (const auto& b : all()) if (b.research == u) out << b; return out; }
const Bank* bank(const QString& key) {
  const QString k = key.toLower();
  for (const auto& b : all()) if (b.id == k || b.ticker.toLower() == k || b.shortName.toLower() == k) return &b;
  return nullptr;
}
QString regionName(Region r) {
  return r == Region::Americas ? QCoreApplication::translate("banks", "Americas")
       : r == Region::Europe ? QCoreApplication::translate("banks", "Europe") : QCoreApplication::translate("banks", "Asia");
}

QPixmap logo(const QString& id, int px, qreal dpr) {
  static QHash<QString, QPixmap> cache;
  const QString key = QString("%1_%2_%3").arg(id).arg(px).arg(dpr);
  if (auto it = cache.constFind(key); it != cache.cend()) return *it;
  QPixmap pm(QSize(px, px) * dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent);
  const Bank* b = bank(id);
  QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  const QRectF r(0, 0, px, px);
  const Logo lg = b ? b->logo : Logo{QColor("#f6f6f6"), QColor("#333333"), "?", 0};
  QPainterPath path; path.addRoundedRect(r, px * 0.14, px * 0.14);
  p.fillPath(path, lg.bg);
  if (lg.shape == 1) {            // ring
    p.setPen(QPen(lg.fg, px * 0.16)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(r.center(), px * 0.28, px * 0.28);
    p.setPen(Qt::NoPen); p.setBrush(lg.fg); p.drawEllipse(r.center(), px * 0.1, px * 0.1);
  } else if (lg.shape == 2) {     // horizontal bar (Danske-like tile)
    p.fillRect(QRectF(px * 0.16, px * 0.42, px * 0.68, px * 0.18), lg.fg);
    p.fillRect(QRectF(px * 0.16, px * 0.42, px * 0.3, px * 0.18), QColor("#ffffff"));
    p.setPen(QPen(lg.fg, 0.8)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(px * 0.16, px * 0.42, px * 0.68, px * 0.18));
  } else if (lg.shape == 3) {     // dot
    p.setPen(Qt::NoPen); p.setBrush(lg.fg); p.drawEllipse(r.center(), px * 0.22, px * 0.22);
  } else if (lg.shape == 4) {     // flat square
    p.fillRect(r.adjusted(px * 0.2, px * 0.2, -px * 0.2, -px * 0.2), QColor(0, 0, 0, 40));
  } else {
    const int n = lg.mark.size();
    QFont f = theme::mono(n <= 1 ? int(px * 0.62) : n == 2 ? int(px * 0.46) : n == 3 ? int(px * 0.34) : int(px * 0.3), QFont::Bold);
    p.setFont(f); p.setPen(lg.fg);
    p.drawText(r, Qt::AlignCenter, lg.mark);
  }
  p.end();
  cache.insert(key, pm);
  return pm;
}

QString moneyShort(double v, const QString& ccy) {
  if (!std::isfinite(v)) return QString::fromUtf8("—");
  const QString sym = ccy == "USD" ? "$" : ccy == "CAD" ? "C$" : ccy == "GBP" ? QString::fromUtf8("£")
                    : ccy == "EUR" ? QString::fromUtf8("€") : ccy == "JPY" ? QString::fromUtf8("¥")
                    : ccy == "CHF" ? "CHF " : ccy + " ";
  double x = v; QString suf;
  if (x >= 1e12) { x /= 1e12; suf = "T"; } else if (x >= 1e9) { x /= 1e9; suf = "B"; } else if (x >= 1e6) { x /= 1e6; suf = "M"; }
  const int dec = x >= 100 ? 0 : x >= 10 ? 1 : 2;
  return sym + QString::number(x, 'f', dec) + suf;
}
QString Assets::text() const { return valid() ? moneyShort(value, currency) : QString::fromUtf8("—"); }

Registry& Registry::instance() { static Registry r; return r; }
Registry::Registry() {}

void Registry::refresh() {
  if (m_started) return;
  m_started = true;
  for (const auto& b : all()) {
    if (!b.systemic || b.cik == 0) continue;
    net::Request rq;
    rq.url = QUrl(QString("https://data.sec.gov/api/xbrl/companyconcept/CIK%1/%2/Assets.json")
                     .arg(b.cik, 10, 10, QChar('0')).arg(b.taxonomy));
    rq.ttlSec = 24 * 3600;
    rq.headers << qMakePair(QByteArray("Accept"), QByteArray("application/json"));
    const QString id = b.id, ccy = b.currency;
    net::get(rq, this, [this, id, ccy](const net::Reply& r) {
      if (!r.ok()) { emit updated(); return; }
      const QJsonObject units = QJsonDocument::fromJson(r.body).object().value("units").toObject();
      // Prefer the filing currency; otherwise the unit with the most recent fact.
      Assets best;
      for (auto it = units.begin(); it != units.end(); ++it) {
        for (const auto& v : it.value().toArray()) {
          const QJsonObject o = v.toObject();
          const QString form = o.value("form").toString();
          if (!(form == "10-K" || form == "10-Q" || form == "20-F" || form == "40-F" || form == "6-K")) continue;
          const QDate end = QDate::fromString(o.value("end").toString(), Qt::ISODate);
          const QDate filed = QDate::fromString(o.value("filed").toString(), Qt::ISODate);
          const bool preferred = it.key() == ccy;
          const bool better = !best.valid() || end > best.asOf || (end == best.asOf && (preferred && best.currency != ccy))
                           || (end == best.asOf && filed > best.filed && it.key() == best.currency);
          if (better) { best.value = o.value("val").toDouble(); best.currency = it.key(); best.asOf = end; best.form = form; best.filed = filed; }
        }
      }
      if (best.valid()) m_assets.insert(id, best);
      emit updated();
    });
  }
}

Assets Registry::assets(const QString& id) const { return m_assets.value(id); }

void Registry::refreshFilings() {
  if (m_filingsStarted) return;
  m_filingsStarted = true;
  for (const auto& b : all()) {
    if (!b.systemic || b.cik == 0) continue;
    net::Request rq;
    rq.url = QUrl(QString("https://data.sec.gov/submissions/CIK%1.json").arg(b.cik, 10, 10, QChar('0')));
    rq.ttlSec = 24 * 3600;
    const QString id = b.id; const int cik = b.cik;
    net::get(rq, this, [this, id, cik](const net::Reply& r) {
      if (!r.ok()) return;
      const QJsonObject recent = QJsonDocument::fromJson(r.body).object().value("filings").toObject().value("recent").toObject();
      const QJsonArray form = recent.value("form").toArray(), date = recent.value("filingDate").toArray(),
                       acc = recent.value("accessionNumber").toArray(), doc = recent.value("primaryDocument").toArray(),
                       desc = recent.value("primaryDocDescription").toArray();
      static const QStringList keep{"10-K", "10-Q", "20-F", "40-F", "6-K", "8-K"};
      QList<Filing> out;
      for (int i = 0; i < form.size() && out.size() < 40; ++i) {
        const QString f = form[i].toString();
        if (!keep.contains(f)) continue;
        Filing x; x.bankId = id; x.form = f; x.filed = QDate::fromString(date[i].toString(), Qt::ISODate);
        x.description = desc[i].toString();
        x.url = QString("https://www.sec.gov/Archives/edgar/data/%1/%2/%3").arg(cik).arg(acc[i].toString().remove('-'), doc[i].toString());
        out << x;
      }
      m_filings.insert(id, out);
      emit filingsUpdated();
    });
  }
}
QList<Filing> Registry::filings() const {
  QList<Filing> out; for (const auto& l : m_filings) out << l;
  std::sort(out.begin(), out.end(), [](const Filing& a, const Filing& b) { return a.filed > b.filed; });
  return out;
}
}
