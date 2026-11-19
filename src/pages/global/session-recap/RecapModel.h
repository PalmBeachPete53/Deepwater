#pragma once
#include "market/Candles.h"
#include "wire/ReleaseSource.h"
#include "wire/Sessions.h"
#include "wire/Wire.h"
#include <QObject>
#include <QVector>

// Data behind Global › Session Recap: 15-minute bars of 12 tracked markets (Yahoo, 60 days), the session /
// week windows (wire::Sessions), releases (wire::releaseSource()) and wire headlines (wire::Wire archive).
namespace wire::recap {

struct Asset { QString symbol, name, section; int decimals; QString hint; };
const QVector<Asset>& assets();
QString assetName(int i);              // display name in the interface language ("Gold" → "Or")          // grid order: row by row (indices · metals+energy · FX · crypto)
QStringList sections();                  // INDICES METALS FOREX CRYPTO ENERGY

struct Window {
  bool week = false;
  wire::Session session;                 // when !week
  QDate monday;                          // when week
  QDateTime start, end;                  // UTC
  QString key() const;
  QString code() const;                  // NYC / LDN / TKY / WEEK
  QString title() const;                 // "NEW YORK SESSION" / "WEEK 43"
  QString flag() const;                  // "us" … ("" for weeks)
  bool live() const { const auto n = QDateTime::currentDateTimeUtc(); return n >= start && n < end; }
  int bucketMinutes() const { return week ? 120 : 15; }
};
QList<Window> sessionWindows(int days);  // newest first, sessions started before now
QList<Window> weekWindows(int weeks);    // newest first

struct Stat {
  double open = NAN, close = NAN, hi = NAN, lo = NAN, chg = NAN;
  QVector<qint64> t;                     // bar open times (UTC s) inside the window
  QVector<double> c;                     // closes
  bool ok() const { return std::isfinite(chg); }
};

class Model : public QObject {
  Q_OBJECT
public:
  explicit Model(QObject* parent = nullptr);
  void load(bool live);                  // (re)fetch the 12 series; live → short cache
  bool loaded() const { return m_pending == 0 && m_started; }
  int loadedCount() const { return m_ok; }
  QString error() const { return m_error; }
  QVector<Stat> stats(const Window& w) const;
  int topMover(const QVector<Stat>& s) const;   // index of the biggest |chg|, -1 if none
  QDateTime dataStart() const;           // first bar available (Yahoo keeps 60 days of 15-min bars)
signals:
  void changed();
private:
  QVector<market::Series> m_series;
  int m_pending = 0, m_ok = 0, m_gen = 0;
  bool m_started = false;
  QString m_error;
};

// Releases (impact >= 2) and headlines inside a window.
QList<wire::Release> windowReleases(const Window& w);
QList<wire::Item> windowHeadlines(const Window& w);
QString assetTag(const QString& headline);   // "WTI", "GOLD" … when a headline names a tracked market

// Rule-based "automated read" (no model): headline, lead and one paragraph per section, EN or FR.
struct Read {
  QString headline, lead;
  struct Section { QString label; QList<int> assets; QString text; };
  QList<Section> sections;
};
Read compose(const Window& w, const QVector<Stat>& st, const QList<wire::Release>& rel, const QList<wire::Item>& wire,
             const QString& lang);
}
