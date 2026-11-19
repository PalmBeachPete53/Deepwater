#pragma once
#include <QObject>
#include <QDate>
#include <QVector>
#include <QHash>
#include <functional>
#include "news/Calendar.h"

// Release history of a calendar indicator (News › Calendar inline chart; usable by the Fundamental Heatmap).
//
//   news::History::instance().fetch(event, this, [this](const news::Series& s) { chart->setSeries(s); });
//
// Official statistics first (OECD SDMX prices / labour / national accounts, Bank of Canada Valet core CPI
// measures, BLS payrolls), dated by reference period. Indicators without an official mapping fall back to the
// actuals Deepwater recorded from the calendar ("recorded calendar releases"), dated by release time.
namespace news {

struct Series {
  QString title;            // "CPI"
  QString country;          // "Canada"
  QString unit;             // "%" or "K"
  QString source;           // "OECD · consumer prices, all items, y/y" / "recorded calendar releases"
  QVector<QDate> dates;     // period start (or release day when recorded), oldest first
  QVector<double> values;
  bool recorded = false;    // true: Deepwater's own recordings
  bool quarterly = false;
  QString error;            // set when nothing could be loaded
  bool empty() const { return values.isEmpty(); }
};

class History : public QObject {
  Q_OBJECT
public:
  static History& instance();
  bool hasOfficial(const Event& e) const;          // an official series is mapped to this indicator
  // Async; the callback runs on the GUI thread (skipped if ctx is gone).
  void fetch(const Event& e, QObject* ctx, std::function<void(const Series&)> cb);

  // The official figure of a past release, when an official series publishes it: the observation of the period
  // the release covers (monthly: the month before the release month; quarterly: the last quarter that ended
  // before it), accepted only when the observation before it equals the calendar's "previous" at its precision
  // (proves the period and the unit). cb gets an empty Value when there is none. Never estimates.
  // note = "<publisher> official series\n<series, period = raw value>".
  bool canOfficial(const Event& e) const;
  void officialActual(const Event& e, QObject* ctx, std::function<void(const Value& v, const QString& note)> cb);
private:
  History() = default;
  QHash<QString, Series> m_cache;                   // by definition id
  QHash<QString, qint64> m_cachedAt;                // by definition id: when it was stored (ms), for the TTL
};
}
