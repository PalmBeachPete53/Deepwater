// Connects the News package to the wire and starts the calendar's recording at app launch.
//
// - wire::ReleaseSource over news::Calendar (Squawk DATA/RELEASES, compact calendar, countdowns; Session Recap).
// - wire::Translator over news::Translate (EN/FR toggles of the wire pages).
// - Data headlines of the wire (FinancialJuice / investingLive) fill calendar actuals (news::parseDataHeadline).
// Started by Q_COREAPP_STARTUP_FUNCTION: no shared file to edit. Test hook: DW_NEWS_SELFTEST=1 runs the headline
// parser on its real samples, prints the result to stderr and quits (exit code = failures).
#include "news/Calendar.h"
#include "core/Startup.h"
#include "news/DataHeadline.h"
#include "news/Translate.h"
#include "wire/ReleaseSource.h"
#include "wire/Wire.h"
#include <QCoreApplication>
#include <QTimer>
#include <QTimeZone>

namespace news {
namespace {

class CalendarReleases : public wire::ReleaseSource {
public:
  CalendarReleases() {
    auto& cal = Calendar::instance();
    connect(&cal, &Calendar::weekLoaded, this, [this] { m_coalesce.start(); });
    connect(&cal, &Calendar::actualArrived, this, [this] { m_coalesce.start(); });
    m_coalesce.setSingleShot(true); m_coalesce.setInterval(200);
    connect(&m_coalesce, &QTimer::timeout, this, &wire::ReleaseSource::changed);
  }
  QList<wire::Release> releases(const QDateTime& from, const QDateTime& to) const override {
    QList<wire::Release> out;
    auto& cal = Calendar::instance();
    const QTimeZone ny("America/New_York");
    for (QDate w = Calendar::weekStart(from.toTimeZone(ny).date()); w <= Calendar::weekStart(to.toTimeZone(ny).date()); w = w.addDays(7))
      for (const Event& e : cal.events(w)) {
        if (e.time < from || e.time >= to) continue;
        wire::Release r;
        r.id = e.key; r.time = e.time; r.currency = e.currency; r.title = e.title;
        r.impact = int(e.impact);
        r.actual = e.actual.text; r.forecast = e.forecast.text; r.previous = e.previous.text;
        r.frequency = e.period; r.frequencyInferred = false;
        r.rangeLow = e.low.text; r.rangeHigh = e.high.text;
        r.detail = Calendar::describe(e).join(" ");
        r.allDay = e.allDay || e.impact == Impact::Holiday;
        out << r;
      }
    return out;
  }
  void ensure(const QDateTime& from, const QDateTime& to) override {
    const QTimeZone ny("America/New_York");
    for (QDate w = Calendar::weekStart(from.toTimeZone(ny).date()); w <= Calendar::weekStart(to.toTimeZone(ny).date()); w = w.addDays(7))
      if (!Calendar::instance().hasData(w)) Calendar::instance().load(w);
  }
  QDateTime lastRefresh() const override { return Calendar::instance().lastRefresh(); }
  QString attribution() const override { return QCoreApplication::translate("news", "ForexFactory weekly calendar · actuals: Myfxbook, wire headlines, official series"); }
private:
  QTimer m_coalesce;
};

class TranslateAdapter : public wire::Translator {
public:
  TranslateAdapter() {
    // the pages light their FR toggle from available(): tell them when the helper becomes ready or fails
    QObject::connect(&Translate::instance(), &Translate::stateChanged, this, [] { emit wire::Hooks::instance().translatorChanged(); });
  }
  bool available() const override { return Translate::instance().usable(); }
  void translate(const QStringList& texts, const QString& lang, QObject* ctx, std::function<void(const QStringList&)> cb) override {
    if (lang != "fr") { QTimer::singleShot(0, ctx, [cb, texts] { cb(texts); }); return; }
    Translate::instance().request(texts, ctx, [texts, cb] {
      QStringList out;
      for (const auto& t : texts) out << Translate::instance().text(t);
      cb(out);
    });
  }
};

void readHeadlines(const QList<wire::Item>& items) {
  for (const auto& i : items) {
    if (i.kind != wire::Kind::Headline) continue;
    if (const auto h = parseDataHeadline(i.text)) Calendar::instance().applyHeadline(*h, i.time, i.source, i.text);
  }
}

void start() {
  if (qEnvironmentVariableIsSet("DW_NEWS_SELFTEST")) {
    const int fails = selfTestDataHeadlines();
    QTimer::singleShot(0, qApp, [fails] { QCoreApplication::exit(fails); });
    return;
  }
  QTimer::singleShot(0, qApp, [] {
    auto& cal = Calendar::instance();
    wire::setReleaseSource(new CalendarReleases);
    wire::setTranslator(new TranslateAdapter);
    cal.startBackground();
    auto& w = wire::Wire::instance();
    QObject::connect(&w, &wire::Wire::itemsAdded, &cal, [](const QList<wire::Item>& fresh) { readHeadlines(fresh); });
    // headlines archived earlier this week, once the week is known
    QObject::connect(&cal, &Calendar::weekLoaded, &cal, [](const QDate& wk) {
      static QSet<QDate> scanned;
      if (scanned.contains(wk) || !Calendar::instance().hasData(wk)) return;
      scanned.insert(wk);
      const QDateTime from(wk, QTime(0, 0), QTimeZone("America/New_York"));
      readHeadlines(wire::Wire::instance().between(from.toUTC(), std::min(from.addDays(7).toUTC(), QDateTime::currentDateTimeUtc())));
    });
  });
}
}
}

// the macro needs an unqualified function name at global scope (internal linkage: no clash with other packages)
static void dw_news_startup() { news::start(); }
DW_STARTUP_FUNCTION(dw_news_startup)
