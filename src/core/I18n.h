#pragma once
#include <QObject>
#include <QLocale>
#include <QString>

// Interface language (English / French). Saved under settings key "ui.language" ("en" | "fr").
// Every user-visible string goes through Qt's translation system:
//   QObject subclasses:  tr("Text")
//   anything else:       QCoreApplication::translate("context", "Text")   // context = the package, e.g. "news"
//   static tables:       QT_TRANSLATE_NOOP("context", "Text"), then translate at display time
// French translations live in translations/<package>_fr.ts (Qt Linguist), compiled into :/i18n at build time.
// Changing the language re-creates the pages (MainWindow listens to i18n::Notifier::changed).
// Numbers keep the English format (1,234.56) in both languages, as traders read them; dates and day/month
// names follow i18n::locale().
namespace i18n {
enum class Lang { English, French };
Lang current();
QString code();                // "en" | "fr"
void setLanguage(Lang l);      // saves, installs translators, emits Notifier::changed
void installSaved();           // at startup (main.cpp)
QLocale locale();              // English (US) or French (France) for dates and month/day names

class Notifier : public QObject {
  Q_OBJECT
public:
  static Notifier& instance();
signals:
  void changed();
};
}
