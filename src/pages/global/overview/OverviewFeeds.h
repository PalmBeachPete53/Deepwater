#pragma once
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cmath>

// Data the Overview panels that belong to other packages expect (phase 2 connects them; until then each panel
// shows a quiet "connecting…" state and never invents a number). Every field is real data from its service,
// or NaN / empty when the service has no value (the panel then shows "—").
namespace overview {

// ---- Squawk wire (news:: / wire services) ----------------------------------------------------------------
struct SquawkItem {
  // Tabs: DATA = headlines, MOVERS = price-move alerts, RELEASES = economic data releases; ALL = everything.
  enum Kind { Headline, Mover, Release };
  Kind kind = Headline;
  QDateTime time;              // when the item was published / the move was measured (shown in local time)
  QString text;                // headline or release text, original language
  QString textFr;              // machine translation (empty when not translated yet)
  // Mover only: "Nikkei 225 -3.43% — now 62,693.16"
  QString name;
  double changePct = NAN;
  double price = NAN;
  int priceDecimals = 2;
  QString id;                  // stable id (dedup, hover)
};
struct SquawkFeed {
  QList<SquawkItem> items;     // newest first
  QDateTime newest;            // "Terminal Data · HH:MM:SS"
  bool live = false;           // service connected and polling
  QString status;              // shown instead of the list when not live ("connecting…", an error)
  QString sourceNote;          // tooltip of the footer: which feeds, terms
  QString translateNote;       // tooltip of the EN/FR button (translator state)
};

// ---- Risk Sentiment (positioning::RiskIndex) ---------------------------------------------------------------
struct RiskReading {
  double value = NAN;          // 0..100 today
  double weekAgo = NAN;        // value one week ago (chip "1W ▼ 1 (WAS 33)")
  double monthAgo = NAN;       // value one month ago
  QVector<QPair<QDate, double>> history;   // daily values, last 12 months, oldest first
  QString method;              // ⓘ tooltip: "computed by Deepwater from …"
  QString status;              // non-empty while connecting / on error
};

// ---- Central Banks (Economy package: policy rates, meeting calendar, market-implied odds) ---------------------
struct CentralBankRow {
  QString name;                // "Fed"
  QString currency;            // "USD"
  QString flag;                // iso2 / currency code for ui::flag ("us", "EUR")
  double rate = NAN;           // policy rate, %
  QDate nextMeeting;
  double cut = NAN, hold = NAN, hike = NAN;   // market-implied probabilities of the next meeting, % (sum ≈ 100)
  QString oddsSource;          // tooltip: where the odds come from (or why they are missing)
  QString rateSource;          // tooltip of the rate: publisher and date
};
struct CentralBanks {
  QList<CentralBankRow> rows;  // Fed ECB BoE BoJ RBA RBNZ BoC SNB
  QString status;
};

// ---- Asset Registry (Economy package: per-country policy-rate source) -----------------------------------------
struct RegistryRow {
  QString iso2;                // flag
  QString name;                // "UNITED STATES" (uppercase display name)
  QString institution;         // badge: "FED", "ECB", "BIS", "WB" …
  bool primary = false;        // rate from a central bank / BIS series (highlighted row); false = World Bank only
  QString bankName;            // tooltip: "Federal Reserve", "BIS policy-rate series", "World Bank data only"
};
struct Registry {
  QList<QList<RegistryRow>> tabs;   // 6 lists in tab order: G20, AMERICA, EUROPE, AFRICA, ASIA, OCEANIA
  QString status;
};
}
