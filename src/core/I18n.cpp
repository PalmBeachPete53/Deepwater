#include "core/I18n.h"
#include "core/Settings.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QDirIterator>
#include <QList>

namespace i18n {
namespace {
Lang g_lang = Lang::English;

// English has no .qm file, so plural strings would show their source text ("%n trade(s)"). This translator
// resolves "(s)" from the count in English; French plurals come from the .ts numerus forms.
class EnglishPlurals : public QTranslator {
public:
  QString translate(const char*, const char* source, const char*, int n) const override {
    if (n < 0 || !source) return {};
    QString s = QString::fromUtf8(source);
    if (!s.contains(QLatin1String("(s)"))) return {};
    s.replace(QLatin1String("(s)"), n == 1 ? QString() : QStringLiteral("s"));
    return s;      // Qt replaces %n afterwards
  }
  bool isEmpty() const override { return false; }
};
QList<QTranslator*>& translators() { static QList<QTranslator*> t; return t; }

void install(Lang l) {
  for (auto* t : translators()) { QCoreApplication::removeTranslator(t); delete t; }
  translators().clear();
  g_lang = l;
  QLocale::setDefault(locale());
  if (l != Lang::French) {
    auto* t = new EnglishPlurals;
    QCoreApplication::installTranslator(t); translators() << t;
    return;
  }
  // one .qm per package: translations/<package>_fr.ts → :/i18n/<package>_fr.qm
  QDirIterator it(":/i18n", {"*_fr.qm"}, QDir::Files);
  while (it.hasNext()) {
    auto* t = new QTranslator;
    if (t->load(it.next())) { QCoreApplication::installTranslator(t); translators() << t; }
    else delete t;
  }
}
}

Lang current() { return g_lang; }
QString code() { return g_lang == Lang::French ? "fr" : "en"; }
QLocale locale() {
  return g_lang == Lang::French ? QLocale(QLocale::French, QLocale::France) : QLocale(QLocale::English, QLocale::UnitedStates);
}
void installSaved() {
  install(settings::value("ui.language", "en").toString() == "fr" ? Lang::French : Lang::English);
}
void setLanguage(Lang l) {
  if (l == g_lang) return;
  settings::setValue("ui.language", l == Lang::French ? "fr" : "en");
  install(l);
  emit Notifier::instance().changed();
}
Notifier& Notifier::instance() { static Notifier* n = new Notifier; return *n; }
}
