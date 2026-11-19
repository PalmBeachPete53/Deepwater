#pragma once
#include <QObject>
#include <QString>
#include <QColor>
#include <QDate>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <cmath>

// Banks known to the app: the systemic-bank registry (total assets from SEC EDGAR XBRL filings) and the
// banks whose research the app reads or links to.
//
//   for (const banks::Bank& b : banks::systemic()) …                 // the 21 registry rows, in display order
//   const banks::Bank* b = banks::bank("ing");                       // by id or US ticker
//   banks::Registry::instance().refresh();                           // EDGAR total assets (cached 1 day)
//   connect(&banks::Registry::instance(), &banks::Registry::updated, …);
//   banks::Assets a = banks::Registry::instance().assets("jpm");      // a.valid(), a.text() → "$5.09T"
//
// Total assets: the latest "Assets" fact of each bank's filings (us-gaap for US GAAP filers, ifrs-full for IFRS
// filers: 10-K/10-Q, 20-F, 40-F/6-K), from data.sec.gov companyconcept. Values are in the filing currency.
namespace banks {

// Translates a string that was marked with QT_TRANSLATE_NOOP("banks", …) in a static table (reasons, cities…).
QString trs(const QString& markedText);

enum class Region { Americas, Europe, Asia };

// How the app may use a bank's research (from its website terms, checked 2026-11).
enum class Use {
  Full,        // notes listed, PDFs cached and shown in the app, forecasts parsed
  LinkOnly,    // only a link to the bank's own research page (terms forbid copying / reuse)
  Excluded,    // not even linked: terms forbid automated access, deep links or citing, or the research is behind a login
  None         // not a research source (registry only)
};

struct Logo { QColor bg, fg; QString mark; int shape = 0; };   // shape 0 text, 1 ring, 2 bar, 3 dot

struct Bank {
  QString id;          // "jpm", "ing", "danske"
  QString name;        // "JPMORGAN" as shown in the registry (uppercase short name)
  QString legalName;   // "JPMorgan Chase & Co."
  QString shortName;   // "JPM" style label for notes ("DANSKE", "ING")
  QString ticker;      // US listing / ADR ticker, empty when none
  int cik = 0;         // SEC EDGAR CIK, 0 when the bank does not file with the SEC
  QString taxonomy;    // "us-gaap" or "ifrs-full"
  QString currency;    // filing currency ISO code
  Region region = Region::Europe;
  QString country;     // ISO-2
  QString city;
  double lat = NAN, lon = NAN;   // head office
  bool systemic = false;         // shown in the registry
  QString fsb;                   // FSB G-SIB list membership note ("G-SIB" or empty)
  Use research = Use::None;
  QString researchName;          // "ING THINK"
  QString researchUrl;           // the bank's research home page
  QString researchWhy;           // why it is included / excluded (terms)
  Logo logo;
};

const QList<Bank>& all();
QList<Bank> systemic();                 // registry rows in display order (US, Canada, UK, Europe, Japan)
QList<Bank> researchBanks(Use u);       // banks with that research use
const Bank* bank(const QString& idOrTicker);
QString regionName(Region r);

// Plain monogram tile (no trademark artwork is bundled): colour tile + initials, or a simple shape.
QPixmap logo(const QString& bankId, int px, qreal dpr = 2.0);

struct Assets {
  double value = NAN;      // in `currency`
  QString currency;        // ISO
  QDate asOf;              // balance sheet date
  QString form;            // 10-Q, 10-K, 20-F, 6-K
  QDate filed;
  bool valid() const { return std::isfinite(value); }
  QString text() const;    // "$5.09T", "C$2.34T", "£944B", "¥426T"; "—" when invalid
};
QString moneyShort(double v, const QString& ccy);

struct Filing {
  QString bankId;
  QString form;          // 10-K, 10-Q, 20-F, 40-F, 6-K, 8-K
  QDate filed;
  QString description;   // the filing's primary document description, may be empty
  QString url;           // primary document on www.sec.gov (opened in the browser)
};

class Registry : public QObject {
  Q_OBJECT
public:
  static Registry& instance();
  void refresh();                               // one request per filer, paced; served from cache within a day
  Assets assets(const QString& bankId) const;
  int loaded() const { return int(m_assets.size()); }
  QString source() const { return "SEC EDGAR XBRL (data.sec.gov companyconcept)"; }
  // Recent periodic and current reports of the registry banks (data.sec.gov submissions, cached 1 day).
  void refreshFilings();
  QList<Filing> filings() const;          // newest first
signals:
  void updated();
  void filingsUpdated();
private:
  Registry();
  QHash<QString, Assets> m_assets;
  QHash<QString, QList<Filing>> m_filings;
  bool m_started = false, m_filingsStarted = false;
};
}
