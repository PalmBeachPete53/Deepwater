// fundamental::Source over the Economy package (macro::MacroStore, macro::CentralBanks). Mapping documented in
// Fundamental.h (macroSource()).
#include "fundamental/Fundamental.h"
#include "macro/CentralBanks.h"
#include "macro/MacroStore.h"
#include "macro/Registry.h"
#include <QRegularExpression>
#include <QTimer>
#include <QCoreApplication>

namespace fundamental {
namespace {

const char* kCoreNote = QT_TRANSLATE_NOOP("seasonal", "Core CPI (excl. food and energy): no series published for this economy yet");
const char* kRetailNote = QT_TRANSLATE_NOOP("seasonal", "Retail sales volume y/y: no series published for this economy yet");

Series toSeries(const macro::Series& s) {
  Series out;
  out.reserve(s.pts.size());
  for (const auto& p : s.pts) if (std::isfinite(p.value)) out << Point{p.date, p.value};
  return out;
}

Release toRelease(const macro::Series& s, const char* missingKey) {
  const QString missing = QCoreApplication::translate("seasonal", missingKey);
  Release r;
  if (!s.ok()) { r.note = missing; return r; }
  r.history = toSeries(s);
  if (r.history.isEmpty()) { r.note = missing; return r; }
  r.value = r.history.last().value;
  r.period = r.history.last().date;
  if (r.history.size() >= 2) r.previous = r.history[r.history.size() - 2].value;
  r.source = s.source;
  return r;
}

// "Raised the target range by 25bp to …" → +25; "Cut … by 25bp" → -25; "Held …" → 0; unknown → INT_MIN
int decisionBp(const QString& text) {
  static const QRegularExpression mv("^(Raised|Cut|Lowered|Reduced|Increased)\\b.*?by\\s+(\\d+)\\s*bp", QRegularExpression::CaseInsensitiveOption);
  const auto m = mv.match(text);
  if (m.hasMatch()) {
    const int bp = m.captured(2).toInt();
    const QString w = m.captured(1).toLower();
    return (w == "raised" || w == "increased") ? bp : -bp;
  }
  if (text.startsWith("Held", Qt::CaseInsensitive) || text.contains("unchanged", Qt::CaseInsensitive) ||
      text.startsWith("Maintained", Qt::CaseInsensitive)) return 0;
  return INT_MIN;
}

class MacroSource : public Source {
public:
  explicit MacroSource(QObject* parent) : Source(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);   // data sets arrive one by one: rebuild once per burst
    connect(&m_debounce, &QTimer::timeout, this, [this] { rebuild(); emit changed(); });
    connect(&macro::MacroStore::instance(), &macro::MacroStore::updated, this, [this] { m_debounce.start(); });
    connect(&macro::CentralBanks::instance(), &macro::CentralBanks::updated, this, [this] { m_debounce.start(); });
    rebuild();
  }
  void activate() override {
    macro::MacroStore::instance().ensureLoaded();
    macro::CentralBanks::instance().ensureLoaded();
  }
  QList<Inputs> inputs() const override { return m_inputs; }
  bool ready() const override { return m_ready; }
  QString status() const override {
    const auto& m = macro::MacroStore::instance();
    if (m.loading()) return QCoreApplication::translate("seasonal", "Loading macro data (%1 data sets so far)…").arg(m.datasetsLoaded());
    if (!m.lastError().isEmpty()) return QCoreApplication::translate("seasonal", "Macro data unavailable: %1").arg(m.lastError());
    return QCoreApplication::translate("seasonal", "Waiting for macro data…");
  }

private:
  void rebuild() {
    const auto& ms = macro::MacroStore::instance();
    const auto& cb = macro::CentralBanks::instance();
    m_inputs.clear();
    int scored = 0;
    for (const auto& meta : currencies()) {
      Inputs in; in.ccy = meta.ccy;
      const macro::Economy* e = macro::economyForCurrency(meta.ccy);
      const QString iso2 = e ? e->iso2 : QString();
      // monetary policy
      const macro::Bank* b = cb.bank(meta.ccy);
      const macro::Series ph = ms.series(iso2, macro::Ind::PolicyRate);
      in.policyHistory = toSeries(ph);
      if (ph.ok()) in.policySource = ph.freq == 'M' ? QCoreApplication::translate("seasonal", "%1 monthly").arg(ph.source) : ph.source;
      if (b && std::isfinite(b->rate)) {
        in.policyRate = b->rate; in.rateSource = b->rateSource; in.rateText = b->rateText;
        if (b->rateAsOf.isValid() && (in.policyHistory.isEmpty() || in.policyHistory.last().date < QDate(b->rateAsOf.year(), b->rateAsOf.month(), 1)))
          in.policyHistory << Point{b->rateAsOf, b->rate};   // last point = latest decision
      } else {
        const macro::PolicyRate pr = ms.policyRate(iso2);
        if (pr.valid()) { in.policyRate = pr.rate; in.rateSource = pr.sourceName; }
      }
      if (b) {
        if (!b->decisions.isEmpty()) {
          const auto& d = b->decisions.first();
          const int bp = decisionBp(d.textEn.isEmpty() ? d.text : d.textEn);   // English: d.text is in the interface language
          in.lastDecision = d.date;
          if (bp != INT_MIN && std::isfinite(in.policyRate)) in.priorRate = in.policyRate - bp / 100.0;
        }
        const macro::MeetingDate nm = b->nextMeeting();
        if (nm.decision.isValid()) in.nextMeeting = nm.decision;
      }
      if (!std::isfinite(in.policyRate)) in.policyNote = QCoreApplication::translate("seasonal", "No policy rate from the central-bank feed or BIS yet");
      else if (!std::isfinite(in.priorRate)) in.policyNote = QCoreApplication::translate("seasonal", "Last decision not published in the central-bank feed yet");
      // prices and activity
      const macro::Series cpi = ms.series(iso2, macro::Ind::Inflation);
      in.cpi = toRelease(cpi, QT_TRANSLATE_NOOP("seasonal", "Headline CPI: no BIS series yet"));
      in.cpiHistory = in.cpi.history;
      if (cpi.ok()) in.cpiSource = cpi.freq == 'M' ? QCoreApplication::translate("seasonal", "%1 monthly").arg(cpi.source) : cpi.source;
      in.coreCpi = toRelease(ms.series(iso2, macro::Ind::CoreInflation), kCoreNote);
      in.retail = toRelease(ms.series(iso2, macro::Ind::RetailSalesYoy), kRetailNote);
      in.gdp = toRelease(ms.series(iso2, macro::Ind::GdpQoq), QT_TRANSLATE_NOOP("seasonal", "GDP q/q: no OECD series yet"));
      macro::Series u = ms.series(iso2, macro::Ind::Unemployment);
      in.unemployment = toRelease(u, QT_TRANSLATE_NOOP("seasonal", "Unemployment: no OECD monthly series"));
      if (!in.unemployment.ok()) {   // World Bank (ILO modelled) annual figure, flagged "(yr)"
        in.unemployment = toRelease(ms.series(iso2, macro::Ind::UnemploymentAnnual), QT_TRANSLATE_NOOP("seasonal", "Unemployment: no OECD monthly or World Bank annual series"));
        if (in.unemployment.ok()) { in.unemployment.source += " " + QCoreApplication::translate("seasonal", "annual"); in.unemployment.annual = true; }
      }
      in.activity = toRelease(ms.series(iso2, macro::Ind::BusinessConf), QT_TRANSLATE_NOOP("seasonal", "Business confidence: no OECD series"));
      in.activityName = "BCI"; in.activityNeutral = 100;
      in.confidence = toRelease(ms.series(iso2, macro::Ind::ConsumerConf), QT_TRANSLATE_NOOP("seasonal", "Consumer confidence: no OECD series"));
      if (scoreOne(in).available >= 3) ++scored;
      m_inputs << in;
    }
    m_ready = scored > 0;
  }
  QList<Inputs> m_inputs;
  bool m_ready = false;
  QTimer m_debounce;
};
}  // namespace

Source* macroSource(QObject* parent) { return new MacroSource(parent); }
}
