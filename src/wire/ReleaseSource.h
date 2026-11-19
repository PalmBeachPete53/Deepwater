#pragma once
#include <QObject>
#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <functional>

// The two hooks the wire needs from the News package (economic calendar and translation).
// Nothing in src/wire/ fetches calendar data itself: until news::Calendar connects with
// wire::setReleaseSource(), every DATA/RELEASES part shows a quiet "calendar connecting" state.
//
// After the merge, the News package does (once, e.g. in news::Calendar's constructor):
//   class CalendarFeed : public wire::ReleaseSource { ... };      // adapter over news::Calendar
//   wire::setReleaseSource(new CalendarFeed(&calendar));
//   wire::setTranslator(new TranslateAdapter(...));                // optional, for the EN/FR toggle
namespace wire {

// One economic calendar event as the wire needs it.
struct Release {
  QString id;                 // stable id across refreshes (same event → same id), e.g. "2026-10-27T16:00Z|USD|CPI m/m"
  QDateTime time;             // scheduled release time (UTC, to the second)
  QString currency;           // "USD", "EUR" … (drives the flag and the country filter)
  QString title;              // event name as published: "CPI m/m", "5-Year Note Auction"
  int impact = 0;             // 0 = holiday/none, 1 = low, 2 = medium, 3 = high
  QString actual;             // as published with units ("3.1%", "23.34T"); empty until released
  QString forecast;           // consensus, empty when none
  QString previous;           // previous value, empty when none
  // Optional extras the calendar may provide (shown as chips in the compact calendar):
  QString frequency;          // "QUARTERLY", "ANNUAL", "MONTHLY" … empty when unknown
  bool frequencyInferred = false;   // the calendar inferred the frequency itself ("inferred" in the chip)
  QString rangeLow, rangeHigh;      // economists' range of forecasts ("1.0%", "1.3%"), empty when none
  QString detail;             // optional one-paragraph description shown by the row expander
  bool allDay = false;        // only the date is known (bank holiday, "All Day" / "Tentative"): `time` is a stamp
                              // at the day's start, not a release time — no clock time, no countdown
  bool released() const { return !actual.isEmpty(); }
};

// Implemented by the News package. All calls happen on the GUI thread.
class ReleaseSource : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  // Every known event with from <= time < to, any order. Must answer from memory (no blocking I/O).
  virtual QList<Release> releases(const QDateTime& from, const QDateTime& to) const = 0;
  // Ask the calendar to load this range if it can (week navigation, past sessions). Answer later with changed().
  virtual void ensure(const QDateTime& from, const QDateTime& to) { Q_UNUSED(from) Q_UNUSED(to) }
  // When the calendar last refreshed successfully (shown in the footer "Terminal Data · HH:MM:SS").
  virtual QDateTime lastRefresh() const { return {}; }
  // Name of the publisher(s) for tooltips ("ForexFactory weekly calendar, Myfxbook actuals").
  virtual QString attribution() const { return {}; }
signals:
  void changed();             // events added, times moved or actuals filled
};

// EN → FR (or other) translation of short headlines. Implemented by the News package (news::Translate).
class Translator : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  // Translate each text into `lang` ("fr"); cb receives the same number of strings (originals when it fails).
  // Called with up to ~50 short strings at a time; cb runs on the GUI thread, skipped if ctx is gone.
  virtual void translate(const QStringList& texts, const QString& lang, QObject* ctx,
                         std::function<void(const QStringList&)> cb) = 0;
  // false while translation cannot work (e.g. the language pack is missing): pages then keep the toggle on EN.
  // The implementation emits Hooks::translatorChanged when this changes.
  virtual bool available() const { return true; }
};

// Registry. The wire and its pages listen to hub().changed to light up when a source connects.
void setReleaseSource(ReleaseSource* s);
ReleaseSource* releaseSource();          // nullptr until the calendar connects
void setTranslator(Translator* t);
Translator* translator();                // nullptr until translation connects

class Hooks : public QObject {
  Q_OBJECT
public:
  static Hooks& instance();
signals:
  void releaseSourceChanged();
  void translatorChanged();
};

// "US", "EU", "UK" … prefix used in release headlines ("US CPI m/m Actual 0.3% …").
QString currencyRegion(const QString& ccy);
}
