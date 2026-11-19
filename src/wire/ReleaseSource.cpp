#include "wire/ReleaseSource.h"
#include "core/Settings.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>

namespace wire {
namespace {
QPointer<ReleaseSource> g_source;
QPointer<Translator> g_translator;

// Test hook DW_WIRE_RELEASES=<file.json>: a ReleaseSource read from a JSON array of real events captured
// from a calendar ([{"time":"2026-11-13T13:30:00Z","currency":"USD","title":"CPI m/m","impact":3,
// "actual":"0.3%","forecast":"0.2%","previous":"0.4%"}, …]). Only for screenshots before the News calendar
// is merged; it never runs unless the variable is set.
class FileSource : public ReleaseSource {
public:
  explicit FileSource(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto& v : arr) {
      const auto o = v.toObject();
      Release r;
      r.time = QDateTime::fromString(o["time"].toString(), Qt::ISODate).toUTC();
      r.currency = o["currency"].toString();
      r.title = o["title"].toString();
      r.impact = o["impact"].toInt();
      r.actual = o["actual"].toString(); r.forecast = o["forecast"].toString(); r.previous = o["previous"].toString();
      r.frequency = o["frequency"].toString(); r.frequencyInferred = o["inferred"].toBool();
      r.rangeLow = o["rangeLow"].toString(); r.rangeHigh = o["rangeHigh"].toString();
      r.id = o["time"].toString() + "|" + r.currency + "|" + r.title;
      if (r.time.isValid()) m_list << r;
    }
    m_loaded = QDateTime::currentDateTimeUtc();
  }
  QList<Release> releases(const QDateTime& from, const QDateTime& to) const override {
    QList<Release> out;
    for (const auto& r : m_list) if (r.time >= from && r.time < to) out << r;
    return out;
  }
  QDateTime lastRefresh() const override { return m_loaded; }
  QString attribution() const override { return "test file (DW_WIRE_RELEASES)"; }
private:
  QList<Release> m_list;
  QDateTime m_loaded;
};
}

Hooks& Hooks::instance() {
  static Hooks* h = [] {
    auto* x = new Hooks;
    const QByteArray file = qgetenv("DW_WIRE_RELEASES");
    // test profiles only: a fixture must never feed the user's real wire archive
    if (!file.isEmpty() && settings::isTestProfile()) g_source = new FileSource(QString::fromUtf8(file));
    return x;
  }();
  return *h;
}

void setReleaseSource(ReleaseSource* s) { Hooks::instance(); g_source = s; emit Hooks::instance().releaseSourceChanged(); }
ReleaseSource* releaseSource() { Hooks::instance(); return g_source.data(); }
void setTranslator(Translator* t) { g_translator = t; emit Hooks::instance().translatorChanged(); }
Translator* translator() { return g_translator.data(); }

QString currencyRegion(const QString& ccy) {
  static const QHash<QString, QString> m{{"USD", "US"}, {"EUR", "EU"}, {"GBP", "UK"}, {"JPY", "JP"}, {"CHF", "CH"},
    {"CAD", "CA"}, {"AUD", "AU"}, {"NZD", "NZ"}, {"CNY", "CN"}, {"CNH", "CN"}, {"SEK", "SE"}, {"NOK", "NO"},
    {"MXN", "MX"}, {"ZAR", "ZA"}, {"TRY", "TR"}, {"INR", "IN"}, {"BRL", "BR"}, {"KRW", "KR"}, {"SGD", "SG"},
    {"HKD", "HK"}, {"PLN", "PL"}};
  return m.value(ccy.toUpper(), ccy.toUpper());
}
}
