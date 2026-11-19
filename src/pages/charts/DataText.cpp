// Charts tab — display-time French for texts made in src/charts (see fmt::tx in Format.h).
// src/charts builds its texts in English because they are also data: saved files, the local API's answers
// (identical to quant-terminal's) and the tests. The UI translates them here, when it shows them:
//  - a whole text marked QT_TRANSLATE_NOOP("charts", "…") where it is defined (indicator names, notes, errors…);
//  - a text built with runtime parts, matched against one of the English templates below ("%1 returned HTTP %2"):
//    the template is translated and its parts are put back, each one translated in turn when it can be.
// A text that matches nothing stays English. The English UI gets every text unchanged.
#include "pages/charts/Format.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QVector>
#include <iterator>

namespace charts::ui::fmt {

QVector<const char*> coreTemplates();   // DataTextCore.cpp

namespace {
// English templates of runtime texts from src/charts/data, exactly as built there (%1, %2… are the runtime parts);
// those of src/charts/core are in DataTextCore.cpp. Keep each one in step with the code that builds it: a template
// that no longer matches just leaves the text English.
// Order matters: the first template that matches wins, so the general ones ("%1: %2", "%1 and %2") come last.
// A plain text (no %) here is just an exact text that has no other place to be marked (e.g. one built in JS source).
const char* const kTemplates[] = {
    // Fetch.cpp: one request
    QT_TRANSLATE_NOOP("charts", "%1 couldn't be reached (%2)"),
    QT_TRANSLATE_NOOP("charts", "%1 returned HTTP %2"),
    QT_TRANSLATE_NOOP("charts", "%1 isn't reachable from here (trying again in a minute)"),
    QT_TRANSLATE_NOOP("charts", "%1 sent an answer that isn't JSON (%2)"),
    // Fetch.cpp: the chain of sources of a chart
    QT_TRANSLATE_NOOP("charts", "No source could load this chart. %1"),
    QT_TRANSLATE_NOOP("charts", "%1: no data"),
    // Providers.cpp / Feeds.cpp: candles
    QT_TRANSLATE_NOOP("charts", "Polygon can't build a %1 futures chart"),
    QT_TRANSLATE_NOOP("charts", "%1 can't build a %2 chart"),
    QT_TRANSLATE_NOOP("charts", "Nasdaq has no data for %1"),
    QT_TRANSLATE_NOOP("charts", "\"%1\" isn't a spread"),
    QT_TRANSLATE_NOOP("charts", "\"%1\" isn't a symbol"),
    QT_TRANSLATE_NOOP("charts", "%1 sent no usable bid and ask"),
    // Feeds.cpp: your own keys
    QT_TRANSLATE_NOOP("charts", "%1 couldn't be reached"),
    QT_TRANSLATE_NOOP("charts", "%1 refused your key"),
    QT_TRANSLATE_NOOP("charts", "your %1 plan doesn't include this"),
    QT_TRANSLATE_NOOP("charts", "Your %1 plan doesn't include this"),
    QT_TRANSLATE_NOOP("charts", "%1 is rate limiting your key"),
    QT_TRANSLATE_NOOP("charts", "%1 returned an error"),
    // Providers.cpp: the notes under a chart's legend
    QT_TRANSLATE_NOOP("charts", "%1 isn't answering right now, so this chart uses %2."),
    QT_TRANSLATE_NOOP("charts", "%1, so this chart uses %2."),
    QT_TRANSLATE_NOOP("charts", "Front contract %1 from your Polygon key, not joined to older contracts. Whether it's live depends on your Polygon futures plan."),
    QT_TRANSLATE_NOOP("charts", "This chart follows %1, a token backed by real gold, until Yahoo's gold feed is back."),
    // Providers.cpp: symbol search
    QT_TRANSLATE_NOOP("charts", "Your file, %1 %2 bars"),
    QT_TRANSLATE_NOOP("charts", "%1 / USDT perpetual"),
    QT_TRANSLATE_NOOP("charts", "stock"),
    QT_TRANSLATE_NOOP("charts", "stocks"),
    QT_TRANSLATE_NOOP("charts", "equity"),
    QT_TRANSLATE_NOOP("charts", "index"),
    QT_TRANSLATE_NOOP("charts", "future"),
    QT_TRANSLATE_NOOP("charts", "mutualfund"),
    QT_TRANSLATE_NOOP("charts", "currency"),
    // Imports.cpp: your own bars and signals
    QT_TRANSLATE_NOOP("charts", "There's no imported file called %1. Import it from the strategy tester's \"Your own data\" row."),
    QT_TRANSLATE_NOOP("charts", "Row %1: couldn't read the time \"%2\". %3"),
    QT_TRANSLATE_NOOP("charts", "Row %1: the position \"%2\" isn't one of 1 (long), 0 (flat) or -1 (short)."),
    QT_TRANSLATE_NOOP("charts", "The file has %1 symbols (%2...). Import one symbol per file."),
    QT_TRANSLATE_NOOP("charts", "Only %1 usable bars in the file. It needs at least 10."),
    QT_TRANSLATE_NOOP("charts", "%1 has %2 bars, so it charts at %2 and longer."),
    QT_TRANSLATE_NOOP("charts", "Your file %1.csv"),
    QT_TRANSLATE_NOOP("charts", "Your own bars from %1.csv, built from its %2 bars."),
    QT_TRANSLATE_NOOP("charts", "Your own bars from %1.csv."),
    QT_TRANSLATE_NOOP("charts", "Signal: %1"),
    QT_TRANSLATE_NOOP("charts", "Position"),
    // MarketInfo.cpp: headlines and financials
    QT_TRANSLATE_NOOP("charts", "Few headlines mention %1 right now, so these are the latest crypto headlines"),
    QT_TRANSLATE_NOOP("charts", "Headlines mentioning %1"),
    QT_TRANSLATE_NOOP("charts", "No headlines found for %1"),
    QT_TRANSLATE_NOOP("charts", "Headlines for %1 from Nasdaq"),
    QT_TRANSLATE_NOOP("charts", "Nasdaq has no financials for %1 (ETFs and funds don't file them)."),
    // Alerts.cpp: a fired alert ("Alert: BTCUSDT 1h: close crosses above 50000 (price 50012.5)") and its webhook
    QT_TRANSLATE_NOOP("charts", "Alert: %1 (price %2)"),
    QT_TRANSLATE_NOOP("charts", "Unknown operator \"%1\". Use one of %2"),
    QT_TRANSLATE_NOOP("charts", "Webhook returned HTTP %1"),
    QT_TRANSLATE_NOOP("charts", "failed: %1"),
    QT_TRANSLATE_NOOP("charts", "%1 is at or above %2"),
    QT_TRANSLATE_NOOP("charts", "%1 is at or below %2"),
    QT_TRANSLATE_NOOP("charts", "%1 is above %2"),
    QT_TRANSLATE_NOOP("charts", "%1 is below %2"),
    QT_TRANSLATE_NOOP("charts", "%1 crosses above %2"),
    QT_TRANSLATE_NOOP("charts", "%1 crosses below %2"),
    QT_TRANSLATE_NOOP("charts", "%1 crosses %2"),
};
// general shapes, tried after every other template (DataTextCore.cpp's too): "Kraken: <reason>",
// "Binance and Coinbase", the conditions of an alert
const char* const kGeneric[] = {
    QT_TRANSLATE_NOOP("charts", "%1: %2"),
    QT_TRANSLATE_NOOP("charts", "%1 and %2"),
    QT_TRANSLATE_NOOP("charts", "%1 or %2"),
};

struct Template {
  QRegularExpression re;
  QVector<int> order;   // capture k holds placeholder %order[k]
  const char* source;
};

const QVector<Template>& templates() {
  static const QVector<Template> list = [] {
    QVector<Template> out;
    static const QRegularExpression ph(QStringLiteral("%(\\d)"));
    QVector<const char*> all(std::begin(kTemplates), std::end(kTemplates));
    all += coreTemplates();
    all += QVector<const char*>(std::begin(kGeneric), std::end(kGeneric));
    for (const char* src : all) {
      const QString s = QString::fromUtf8(src);
      Template t;
      t.source = src;
      QString pattern = QStringLiteral("^");
      qsizetype last = 0;
      for (auto it = ph.globalMatch(s); it.hasNext();) {
        const auto m = it.next();
        pattern += QRegularExpression::escape(s.mid(last, m.capturedStart() - last)) + QStringLiteral("(.*?)");
        t.order << m.captured(1).toInt();
        last = m.capturedEnd();
      }
      pattern += QRegularExpression::escape(s.mid(last)) + QStringLiteral("$");
      t.re = QRegularExpression(pattern, QRegularExpression::DotMatchesEverythingOption);
      out << t;
    }
    return out;
  }();
  return list;
}

QString exact(const QString& s) {
  const QByteArray u = s.toUtf8();
  return QCoreApplication::translate("charts", u.constData());
}

QString translateText(const QString& s, int depth) {
  if (s.isEmpty()) return s;
  const QString e = exact(s);
  if (e != s || depth > 12) return e;
  // a list of reasons ("No source could load this chart. Binance: … | Coinbase: …"): each one on its own
  static const QString kSep = QStringLiteral(" | ");
  if (s.contains(kSep)) {
    QStringList parts = s.split(kSep);
    for (QString& p : parts) p = translateText(p, depth + 1);
    return parts.join(kSep);
  }
  for (const Template& t : templates()) {
    const auto m = t.re.match(s);
    if (!m.hasMatch()) continue;
    const QString fr = QCoreApplication::translate("charts", t.source);
    QHash<int, QString> args;
    for (int k = 0; k < t.order.size(); ++k) args.insert(t.order[k], translateText(m.captured(k + 1), depth + 1));
    // put the parts back by number (not QString::arg: a part may hold "%")
    QString out;
    out.reserve(fr.size() + 32);
    for (qsizetype i = 0; i < fr.size(); ++i) {
      if (fr[i] == QLatin1Char('%') && i + 1 < fr.size() && fr[i + 1].isDigit() && args.contains(fr[i + 1].digitValue())) {
        out += args.value(fr[i + 1].digitValue());
        ++i;
      } else {
        out += fr[i];
      }
    }
    return out;
  }
  return s;
}
}  // namespace

QString tx(const QString& english) {
  if (i18n::current() != i18n::Lang::French || english.isEmpty()) return english;
  static QHash<QString, QString> cache;
  if (auto it = cache.constFind(english); it != cache.cend()) return *it;
  const QString out = translateText(english, 0);
  if (cache.size() > 4000) cache.clear();
  cache.insert(english, out);
  return out;
}

}  // namespace charts::ui::fmt
