#include "macrodata/Importer.h"
#include "macrodata/MacroDataStore.h"
#include "banks/Forecasts.h"
#include "banks/Registry.h"
#include "macro/CentralBanks.h"
#include "macro/MacroStore.h"
#include "macro/Registry.h"
#include "market/Candles.h"
#include "market/Instruments.h"
#include "news/Calendar.h"
#include "positioning/Cot.h"
#include "positioning/Retail.h"
#include "positioning/RiskIndex.h"
#include <QCoreApplication>
#include <QTimeZone>
#include <QRegularExpression>
#include <memory>

namespace macrodata {
namespace {
QDateTime day(const QDate& d) { return d.isValid() ? QDateTime(d, QTime(0, 0), QTimeZone::UTC) : QDateTime(); }
QString freqOf(char f) { return QString(QChar(f)); }

// waits until ready() (re-checked at each signal) or ms, then calls then() once
template <typename Sender, typename Signal>
void waitUntil(Sender* sender, Signal signal, std::function<bool()> ready, int ms, std::function<void()> then) {
  if (ready()) { then(); return; }
  auto* ctx = new QObject(qApp);
  auto once = std::make_shared<bool>(false);
  auto fire = [ctx, once, then] { if (*once) return; *once = true; ctx->deleteLater(); then(); };
  QObject::connect(sender, signal, ctx, [fire, ready] { if (ready()) fire(); });
  QTimer::singleShot(ms, ctx, [fire] { fire(); });
}

// Fixed English names and keys of the macro indicators (chain keys never depend on the interface language; the
// Macro data page translates the names, context "data").
struct IndName { const char* key; const char* name; };
IndName indName(macro::Ind i) {
  using I = macro::Ind;
  switch (i) {
    case I::PolicyRate: return {"policy-rate", QT_TRANSLATE_NOOP("data", "Policy rate")};
    case I::GdpQoq: return {"gdp-qoq", QT_TRANSLATE_NOOP("data", "GDP growth q/q")};
    case I::GdpYoy: return {"gdp-yoy", QT_TRANSLATE_NOOP("data", "GDP growth y/y")};
    case I::Inflation: return {"inflation", QT_TRANSLATE_NOOP("data", "Inflation (CPI y/y)")};
    case I::Unemployment: return {"unemployment", QT_TRANSLATE_NOOP("data", "Unemployment rate")};
    case I::BusinessConf: return {"business-confidence", QT_TRANSLATE_NOOP("data", "Business confidence (OECD)")};
    case I::ConsumerConf: return {"consumer-confidence", QT_TRANSLATE_NOOP("data", "Consumer confidence (OECD)")};
    case I::Yield10y: return {"yield-10y", QT_TRANSLATE_NOOP("data", "10-year government bond yield")};
    case I::Rate3m: return {"rate-3m", QT_TRANSLATE_NOOP("data", "3-month interbank rate")};
    case I::GdpUsd: return {"gdp-usd", QT_TRANSLATE_NOOP("data", "GDP (current US$)")};
    case I::GdpPerCapita: return {"gdp-per-capita", QT_TRANSLATE_NOOP("data", "GDP per capita (US$)")};
    case I::Population: return {"population", QT_TRANSLATE_NOOP("data", "Population")};
    case I::RealGrowth: return {"real-growth", QT_TRANSLATE_NOOP("data", "Real GDP growth (annual)")};
    case I::InflationAnnual: return {"inflation-annual", QT_TRANSLATE_NOOP("data", "Inflation (annual average)")};
    case I::UnemploymentAnnual: return {"unemployment-annual", QT_TRANSLATE_NOOP("data", "Unemployment (ILO, annual)")};
    case I::CurrentAccount: return {"current-account", QT_TRANSLATE_NOOP("data", "Current account (% of GDP)")};
    case I::GovDebt: return {"gov-debt", QT_TRANSLATE_NOOP("data", "Government debt (% of GDP)")};
    case I::FxReserves: return {"fx-reserves", QT_TRANSLATE_NOOP("data", "FX reserves (US$)")};
    case I::CoreInflation: return {"core-inflation", QT_TRANSLATE_NOOP("data", "Core inflation (y/y)")};
    case I::RetailSalesYoy: return {"retail-sales", QT_TRANSLATE_NOOP("data", "Retail sales volume (y/y)")};
    default: return {"other", "Other"};
  }
}
// labels of the other fixed chains (translated by the Macro data page)
[[maybe_unused]] const char* kLabels[] = {
    QT_TRANSLATE_NOOP("data", "Decisions"), QT_TRANSLATE_NOOP("data", "Open"), QT_TRANSLATE_NOOP("data", "High"),
    QT_TRANSLATE_NOOP("data", "Low"), QT_TRANSLATE_NOOP("data", "Close"), QT_TRANSLATE_NOOP("data", "Quarter-end forecast"),
    QT_TRANSLATE_NOOP("data", "Retail traders long"), QT_TRANSLATE_NOOP("data", "Composite (0-100)"),
    QT_TRANSLATE_NOOP("data", "Risk sentiment"), QT_TRANSLATE_NOOP("data", "Open interest")};

// the chain of a policy rate: one per BIS area (the euro members share the ECB's)
QString rateKey(const QString& bisArea) { return "rate:" + bisArea.toLower(); }
const macro::Economy* economyOfBis(const QString& bis) {
  const macro::Economy* best = nullptr;
  for (const auto& e : macro::economies()) {
    if (e.bis != bis) continue;
    if (!best || e.iso2 == "ez") best = &e;
  }
  return best;
}
// One chain = one source (user's rule, 2026-11-16): a revision is only ever the same source changing its figure; another
// source's figure for the same period goes to that source's own chain, never in as a revision.
QString slug(const QString& s) {
  QString o = s.toLower();
  static const QRegularExpression runs("[^a-z0-9]+"), ends("^-+|-+$");
  o.replace(runs, "-");
  o.remove(ends);
  return o.isEmpty() ? QStringLiteral("unknown") : o;
}
ChainDef rateDef(const macro::Economy& e) {   // the BIS policy-rate series
  ChainDef d;
  d.key = rateKey(e.bis); d.family = "rates";
  d.subject = e.bankName.isEmpty() ? e.name : e.bankName + (e.bis == "XM" ? QString() : " (" + e.name + ")");
  d.indicator = "Policy rate"; d.kind = "actual"; d.unit = "%"; d.freq = "E"; d.mode = "changes";
  d.source = "BIS WS_CBPOL";
  return d;
}
// a calendar actual, in the chain of the source that gave it ("Myfxbook", "investingLive headline", "BLS official series"…)
ChainDef calActualDef(const news::Event& e, const QString& source) {
  ChainDef d;
  d.key = "cal:" + e.seriesKey + ":actual:" + slug(source); d.family = "calendar"; d.subject = e.currency; d.indicator = e.title;
  d.kind = "actual"; d.unit = e.actual.has() ? e.actual.suffix : e.forecast.suffix; d.freq = "E"; d.source = source;
  return d;
}
}  // namespace

Importer& Importer::instance() { static Importer* i = new Importer; return *i; }

Importer::Importer() {
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, &Importer::start);
  // a calendar actual is a fact the moment it arrives
  connect(&news::Calendar::instance(), &news::Calendar::actualArrived, this, [](const news::Event& e) {
    if (!e.actual.num()) return;
    const ChainDef d = calActualDef(e, e.actualSource.isEmpty() ? QStringLiteral("unknown") : e.actualSource);
    Obs o; o.chain = d.key; o.period = e.time; o.at = e.actualAt.isValid() ? e.actualAt : e.time; o.value = e.actual.v;
    o.note = e.actualDetail.left(200);
    Store::instance().add({d}, {o});
  });
}

QStringList Importer::families() { return {"rates", "macro", "calendar", "prices", "forecasts", "positioning"}; }
QString Importer::familyName(const QString& f) {
  if (f == "rates") return QCoreApplication::translate("data", "Policy rates");
  if (f == "macro") return QCoreApplication::translate("data", "Macro indicators");
  if (f == "calendar") return QCoreApplication::translate("data", "Calendar releases");
  if (f == "prices") return QCoreApplication::translate("data", "Market prices");
  if (f == "forecasts") return QCoreApplication::translate("data", "Forecasts");
  if (f == "positioning") return QCoreApplication::translate("data", "Positioning");
  return f;
}

bool Importer::deepDone(const QString& family) const { return !Store::instance().meta("deep." + family).isEmpty(); }

QDateTime Importer::lastPass(const QString& family) const {
  // a database from before "last." existed: its first import counts as the last pass
  QString v = Store::instance().meta("last." + family);
  if (v.isEmpty()) v = Store::instance().meta("deep." + family);
  return QDateTime::fromString(v, Qt::ISODate);
}

QStringList Importer::due() const {
  QStringList out;
  const QDateTime now = QDateTime::currentDateTimeUtc();
  for (const QString& f : families()) {
    const QDateTime last = lastPass(f);
    if (!deepDone(f) || !last.isValid() || last.msecsTo(now) >= kPassMs) out << f;
  }
  return out;
}

void Importer::scheduleNext() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  qint64 wait = kPassMs;
  for (const QString& f : families()) {
    const QDateTime last = lastPass(f);
    wait = qMin(wait, last.isValid() ? qMax<qint64>(60000, kPassMs - last.msecsTo(now)) : 60000);
  }
  m_timer.start(int(qMin<qint64>(wait, kPassMs)));
  const qint64 min = (wait + 59999) / 60000;
  say(min >= 60 ? QCoreApplication::translate("data", "Up to date · next pass in %1 h %2").arg(min / 60).arg(min % 60, 2, 10, QChar('0'))
                : QCoreApplication::translate("data", "Up to date · next pass in %1 min").arg(min));
}

void Importer::start() {
  if (m_running) return;
  m_queue = due();
  if (m_queue.isEmpty()) { scheduleNext(); emit finished(); return; }   // everything fresh: nothing to download
  m_running = true;
  m_steps = int(m_queue.size()); m_step = 0;
  next();
}

void Importer::say(const QString& s) { m_status = s; emit progress(); }

void Importer::done(const QString& family) {
  const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  if (!deepDone(family)) Store::instance().setMeta("deep." + family, now);
  Store::instance().setMeta("last." + family, now);
}

void Importer::next() {
  if (m_queue.isEmpty()) {
    m_running = false;
    // the "last." metas are written by the writer thread: read them back once it has had them
    QTimer::singleShot(500, this, [this] { scheduleNext(); emit finished(); });
    return;
  }
  const QString f = m_queue.takeFirst();
  ++m_step;
  say(QCoreApplication::translate("data", "%1 of %2 · %3…").arg(m_step).arg(m_steps).arg(familyName(f)));
  // a family whose source was not ready (wait timed out) is not marked done: the next pass, a minute later, tries again
  m_incomplete = false;
  auto then = [this, f] { if (!m_incomplete) done(f); m_incomplete = false; QTimer::singleShot(200, this, &Importer::next); };
  if (f == "rates") rates(then);
  else if (f == "macro") macro(then);
  else if (f == "calendar") calendar(then);
  else if (f == "prices") prices(then);
  else if (f == "forecasts") forecasts(then);
  else positioning(then);
}

// ------------------------------------------------------------------------------------------------ rates
void Importer::rates(std::function<void()> then) {
  auto& deep = macro::MacroStore::deep();
  deep.ensureLoaded();
  auto& cb = macro::CentralBanks::instance();
  cb.ensureLoaded();
  waitUntil(&deep, &macro::MacroStore::updated, [&deep] { return deep.datasetsLoaded() > 0 && !deep.loading(); }, 240000, [this, then] {
    auto& st = macro::MacroStore::deep();
    if (st.datasetsLoaded() == 0 || st.loading()) m_incomplete = true;   // timed out: what is there is still stored
    QList<ChainDef> defs; QList<Obs> obs;
    QSet<QString> seen;
    QHash<QString, QHash<qint64, double>> bisPoints;   // one-time repair of the BIS chains (Store::repairBisChains)
    const bool repair = Store::instance().meta("repair.bisfeed") != "1";
    for (const auto& e : macro::economies()) {
      if (e.bis.isEmpty() || seen.contains(e.bis)) continue;
      const macro::Economy* owner = economyOfBis(e.bis);
      if (!owner) continue;
      // raw BIS months: series() would add the central-bank feed's current value as this month's BIS figure
      const macro::Series s = st.rawSeries(owner->iso2, macro::Ind::PolicyRate);
      if (!s.ok()) continue;
      seen.insert(e.bis);
      ChainDef d = rateDef(*owner);
      defs << d;
      for (const auto& p : s.pts) { Obs o; o.chain = d.key; o.period = day(p.date); o.value = p.value; o.note = "BIS " + s.dataset; obs << o; }
      if (repair)
        for (const auto& p : s.pts) if (day(p.date).isValid()) bisPoints[d.key].insert(day(p.date).toMSecsSinceEpoch(), p.value);
    }
    if (repair && !bisPoints.isEmpty()) Store::instance().repairBisChains(bisPoints);   // before this pass's add()
    // the followed banks: today's official rate, decisions, market-implied path
    for (const auto& b : macro::CentralBanks::instance().banks()) {
      const macro::Economy* e = nullptr;
      for (const auto& x : macro::economies()) if (x.iso2 == b.iso2.toLower() || (b.id == "ECB" && x.iso2 == "ez")) { e = &x; break; }
      if (!e || e->bis.isEmpty()) continue;
      const macro::Economy* owner = economyOfBis(e->bis);
      ChainDef rd = rateDef(*owner);
      if (!seen.contains(e->bis)) { defs << rd; seen.insert(e->bis); }
      if (std::isfinite(b.rate) && b.rateAsOf.isValid() && !b.rateSourceEn.isEmpty()) {   // the feed it was read from: its own chain
        ChainDef cb = rd; cb.key = rd.key + ":" + slug(b.rateSourceEn); cb.source = b.rateSourceEn;
        defs << cb;
        Obs o; o.chain = cb.key; o.period = day(b.rateAsOf); o.value = b.rate; o.text = b.rateText;
        o.at = b.rateFetched; o.note = b.rateSourceEn; obs << o;
      }
      // decisions: read from the bank's statements, or inferred by Deepwater from the rate series: two sources, two chains
      ChainDef dd;
      dd.key = "decision:" + b.id.toLower(); dd.family = "rates"; dd.subject = b.name; dd.indicator = "Decisions";
      dd.kind = "actual"; dd.freq = "E"; dd.source = b.name + " statements";
      ChainDef di = dd; di.key += ":inferred"; di.source = "Deepwater, inferred from the policy-rate series";
      bool anyStatement = false, anyInferred = false;
      for (const auto& dec : b.decisions) (dec.inferred ? anyInferred : anyStatement) = true;
      if (anyStatement) defs << dd;
      if (anyInferred) defs << di;
      for (const auto& dec : b.decisions) {
        Obs o; o.chain = dec.inferred ? di.key : dd.key; o.period = day(dec.date);
        // English whatever the interface language: a fact's text must not change with the language
        o.text = (dec.textEn.isEmpty() ? dec.text : dec.textEn) + (dec.vote.isEmpty() ? QString() : " (vote " + dec.vote + ")");
        o.note = dec.url; obs << o;
      }
      if (b.priced && !b.path.isEmpty() && !b.pricingSourceEn.isEmpty()) {
        ChainDef id;
        id.key = "implied:" + b.id.toLower(); id.family = "rates"; id.subject = b.name; id.indicator = "Policy rate";
        id.kind = "implied"; id.unit = "%"; id.freq = "E"; id.targetChain = rd.key;
        id.source = b.pricingSourceEn;   // English and without a date: the same source keeps the same chain
        // another way of pricing would be another source: its own chain
        if (const Chain c = Store::instance().chain(id.key); c.id && !c.def.source.isEmpty() && c.def.source != id.source)
          id.key += ":" + slug(id.source);
        defs << id;
        // each pricing is an observation of its own, to the hour: the market moving during a day is not a revision
        QDateTime made = (b.pricedAt.isValid() ? b.pricedAt : QDateTime::currentDateTimeUtc()).toUTC();
        made = QDateTime(made.date(), QTime(made.time().hour(), 0), QTimeZone::UTC);
        for (const auto& pt : b.path) {
          if (!std::isfinite(pt.rate) || !pt.meeting.decision.isValid()) continue;
          Obs o; o.chain = id.key; o.period = made; o.target = day(pt.meeting.decision); o.value = pt.rate;
          o.at = b.pricedAt; o.note = QString("implied change %1 bp at this meeting").arg(pt.bp, 0, 'f', 1); obs << o;
        }
      }
    }
    Store::instance().add(defs, obs, [then](int, int) { then(); });
  });
}

// ------------------------------------------------------------------------------------------------ macro
void Importer::macro(std::function<void()> then) {
  auto& deep = macro::MacroStore::deep();
  deep.ensureLoaded();
  waitUntil(&deep, &macro::MacroStore::updated, [&deep] { return deep.datasetsLoaded() > 0 && !deep.loading(); }, 300000, [this, then] {
    auto& st = macro::MacroStore::deep();
    if (st.datasetsLoaded() == 0 || st.loading()) m_incomplete = true;   // timed out: what is there is still stored
    QList<ChainDef> defs; QList<Obs> obs;
    QHash<QString, QString> sourceOf;   // existing chain key → its source
    for (const auto& c : Store::instance().chains()) if (c.def.family == "macro") sourceOf.insert(c.def.key, c.def.source);
    for (const auto& e : macro::economies()) {
      for (int k = 0; k < int(macro::Ind::Count); ++k) {
        const auto ind = macro::Ind(k);
        if (ind == macro::Ind::PolicyRate) continue;   // in the rates family
        const macro::Series s = st.series(e.iso2, ind);
        if (!s.ok()) continue;
        const auto& inf = macro::info(ind);
        const IndName nm = indName(ind);
        ChainDef d;
        d.key = QString("macro:%1:%2").arg(e.iso2, nm.key);
        d.family = "macro"; d.subject = e.name; d.indicator = nm.name; d.kind = "actual"; d.unit = inf.unit;
        d.freq = freqOf(s.freq); d.source = s.source + " " + s.dataset;
        // the store may switch an indicator to a fresher publisher: that series starts its own chain
        if (sourceOf.contains(d.key) && sourceOf.value(d.key) != d.source) d.key += ":" + slug(d.source);
        defs << d;
        for (const auto& p : s.pts) { Obs o; o.chain = d.key; o.period = day(p.date); o.value = p.value; obs << o; }
      }
    }
    Store::instance().add(defs, obs, [then](int, int) { then(); });
  });
}

// ------------------------------------------------------------------------------------------------ calendar
void Importer::calendar(std::function<void()> then) {
  auto& cal = news::Calendar::instance();
  QList<QDate> weeks = cal.recordedWeeks();
  const QDate cur = news::Calendar::currentWeek();
  if (!weeks.contains(cur)) weeks << cur;
  for (const QDate& w : weeks) if (!cal.hasData(w)) cal.load(w);
  waitUntil(&cal, &news::Calendar::weekLoaded, [weeks] {
    for (const QDate& w : weeks) if (!news::Calendar::instance().hasData(w)) return false;
    return true;
  }, 8000, [weeks, then] {
    auto& c = news::Calendar::instance();
    QHash<QString, QList<news::Event>> bySeries;
    for (const QDate& w : weeks) for (const auto& e : c.events(w)) if (!e.isSpeech() && e.impact != news::Impact::Holiday) bySeries[e.seriesKey] << e;
    QList<ChainDef> defs; QList<Obs> obs;
    for (auto it = bySeries.begin(); it != bySeries.end(); ++it) {
      auto& list = it.value();
      std::sort(list.begin(), list.end(), [](const news::Event& a, const news::Event& b) { return a.time < b.time; });
      const news::Event& f = list.first();
      // ForexFactory: its consensus, and the actuals it states as "previous" of the next release (revisions included)
      ChainDef ff = calActualDef(f, "ForexFactory");
      ff.source = "ForexFactory (\"previous\" of the next release)";
      ChainDef k = ff; k.key = "cal:" + it.key() + ":consensus"; k.kind = "consensus"; k.source = "ForexFactory consensus"; k.targetChain = ff.key;
      QHash<QString, ChainDef> actualChains;   // one per source of actuals
      bool anyForecast = false, anyPrevious = false;
      for (int i = 0; i < list.size(); ++i) {
        const auto& e = list[i];
        if (e.forecast.num()) anyForecast = true;
        if (e.actual.num()) {
          const QString src = e.actualSource.isEmpty() ? QStringLiteral("unknown") : e.actualSource;
          if (!actualChains.contains(src)) actualChains.insert(src, calActualDef(f, src));
          Obs o; o.chain = actualChains.value(src).key; o.period = e.time; o.at = e.actualAt.isValid() ? e.actualAt : e.time;
          o.value = e.actual.v; o.note = e.actualDetail.left(200); obs << o;
        }
        if (e.forecast.num()) { Obs o; o.chain = k.key; o.period = e.time; o.target = e.time; o.value = e.forecast.v; o.note = e.forecastSource; obs << o; }
        if (i > 0 && e.previous.num()) {
          Obs o; o.chain = ff.key; o.period = list[i - 1].time; o.at = e.time; o.value = e.previous.v;
          o.note = "value given as \"previous\" by the " + e.time.toString("yyyy-MM-dd") + " release"; obs << o;
          anyPrevious = true;
        }
      }
      for (const auto& d : actualChains) defs << d;
      if (anyPrevious) defs << ff;
      if (anyForecast) defs << k;
    }
    Store::instance().add(defs, obs, [then](int, int) { then(); });
  });
}

// ------------------------------------------------------------------------------------------------ prices
void Importer::prices(std::function<void()> then) {
  auto list = std::make_shared<QList<market::Instrument>>(market::instruments());
  // last day already stored per instrument (its close chain): only what follows it is fetched again
  auto lastDay = std::make_shared<QHash<QString, QDate>>();
  if (deepDone("prices"))
    for (const auto& c : Store::instance().chains())
      if (c.def.family == "prices" && c.def.key.endsWith(":close") && c.last > 0)
        lastDay->insert(c.def.key.section(':', 1, 1), QDateTime::fromMSecsSinceEpoch(c.last, QTimeZone::UTC).date());
  auto idx = std::make_shared<int>(0);
  auto step = std::make_shared<std::function<void()>>();
  *step = [this, list, idx, lastDay, step, then] {
    if (*idx >= list->size()) {
      // the closure holds its own shared_ptr (step captures step): break the cycle once the pass is over, after this
      // call has returned, or each pass leaks it with its captured lists (as the technical-data Collector does)
      QTimer::singleShot(0, this, [step] { *step = nullptr; });
      then(); return;
    }
    const market::Instrument ins = list->at((*idx)++);
    // daily bars by date range (Yahoo answers range=max with monthly bars); 1928 covers the oldest indices. Later passes:
    // from a week before the last stored day (the latest days settle), at midnight UTC so the request is the same all day
    // (the HTTP cache then answers a relaunch); an instrument never stored gets its whole history.
    const QDate known = lastDay->value(ins.symbol);
    const qint64 from = QDateTime(known.isValid() ? known.addDays(-7) : QDate(1928, 1, 1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
    say(QCoreApplication::translate("data", "%1 of %2 · %3 · %4 (%5 / %6)").arg(m_step).arg(m_steps)
            .arg(familyName("prices"), ins.name).arg(*idx).arg(list->size()));
    market::candlesBetween(ins.symbol, "1d", from, QDateTime::currentSecsSinceEpoch(), this, [ins, step](const market::Series& s) {
      if (!s.ok()) { QTimer::singleShot(0, [step] { (*step)(); }); return; }
      QList<ChainDef> defs; QList<Obs> obs;
      const char* fields[] = {"open", "high", "low", "close"};
      const char* names[] = {"Open", "High", "Low", "Close"};
      for (int f = 0; f < 4; ++f) {
        ChainDef d; d.key = QString("px:%1:%2").arg(ins.symbol, fields[f]); d.family = "prices"; d.subject = ins.name;
        d.indicator = names[f]; d.kind = "price"; d.unit = ins.quote; d.freq = "D"; d.source = "Yahoo Finance daily bars";
        defs << d;
      }
      const QDate today = QDateTime::currentDateTimeUtc().date();
      // the bar's trading day: Yahoo stamps the session open; sessions opening after 20:00 UTC (FX, futures) belong to
      // the next day. One bar per day (the last one), never today's unfinished bar.
      QMap<QDate, market::Bar> byDay;
      QSet<QDate> flatDays;
      for (const auto& b : s.bars) {
        const QDateTime t = QDateTime::fromSecsSinceEpoch(b.t, QTimeZone::UTC);
        const QDate d = t.time().hour() >= 20 ? t.date().addDays(1) : t.date();
        if (d >= today) continue;
        // Yahoo turns a futures' past days into flat settlement bars (open = high = low = close, no volume): their close is
        // the settlement, a real figure; their open, high and low are only copies of it, not observed values
        const bool flat = b.o == b.h && b.h == b.l && b.l == b.c && !(b.v > 0);
        byDay.insert(d, b);
        if (flat) flatDays.insert(d); else flatDays.remove(d);
      }
      for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) {
        const QDate d = it.key();
        const market::Bar& b = it.value();
        const double v[] = {b.o, b.h, b.l, b.c};
        for (int f = 0; f < 4; ++f) {
          if (!std::isfinite(v[f])) continue;
          if (flatDays.contains(d) && f != 3) continue;   // close only
          Obs o; o.chain = defs[f].key; o.period = day(d); o.value = v[f];
          if (f == 0) o.at = QDateTime::fromSecsSinceEpoch(b.t, QTimeZone::UTC);
          obs << o;
        }
      }
      Store::instance().add(defs, obs, [step](int, int) { (*step)(); });
    });
  };
  (*step)();
}

// ------------------------------------------------------------------------------------------------ forecasts
void Importer::forecasts(std::function<void()> then) {
  auto& f = banks::Forecasts::instance();
  if (!f.loaded()) f.refresh();
  waitUntil(&f, &banks::Forecasts::updated, [] { return banks::Forecasts::instance().loaded(); }, 30000, [this, then] {
    if (!banks::Forecasts::instance().loaded()) m_incomplete = true;   // timed out: the pass is asked again
    QList<ChainDef> defs; QList<Obs> obs;
    QSet<QString> made;
    for (const auto& x : banks::Forecasts::instance().forecasts()) {
      const auto* m = banks::market(x.marketId);
      const auto* bk = banks::bank(x.bankId);
      if (!m || !x.quarter.valid() || !std::isfinite(x.value)) continue;
      const QString key = "fc:" + x.bankId + ":" + x.marketId;
      if (!made.contains(key)) {
        ChainDef d; d.key = key; d.family = "forecasts"; d.subject = m->display; d.indicator = "Quarter-end forecast";
        d.kind = "forecast"; d.unit = m->unit; d.freq = "Q"; d.source = bk ? bk->name : x.bankId;
        if (const auto* ins = market::find(m->spot)) d.targetChain = "px:" + ins->symbol + ":close";
        defs << d; made.insert(key);
      }
      Obs o; o.chain = key; o.period = day(x.revised.isValid() ? x.revised : QDate::currentDate());
      o.target = day(x.quarter.end()); o.value = x.value; o.note = x.average ? "quarterly average" : "end of quarter";
      obs << o;
    }
    Store::instance().add(defs, obs, [then](int, int) { then(); });
  });
}

// ------------------------------------------------------------------------------------------------ positioning
void Importer::positioning(std::function<void()> then) {
  // retail sentiment recordings and the risk index first (already in memory or quick), then the COT markets one by one
  {
    QList<ChainDef> defs; QList<Obs> obs; QSet<QString> made;
    auto& r = positioning::Retail::instance();
    QVector<positioning::RetailSnapshot> snaps = r.recordings();
    if (!r.latest().isEmpty()) snaps << r.latest();
    for (const auto& s : snaps) for (const auto& x : s.symbols) {
      const QString key = "retail:" + x.name + ":long";
      if (!made.contains(key)) {
        ChainDef d; d.key = key; d.family = "positioning"; d.subject = x.display(); d.indicator = "Retail traders long";
        d.kind = "actual"; d.unit = "%"; d.freq = "E"; d.source = "Myfxbook community outlook"; defs << d; made.insert(key);
      }
      Obs o; o.chain = key; o.period = s.time; o.at = s.time; o.value = x.longPct; obs << o;
    }
    const auto& rd = positioning::RiskIndex::instance().data();
    if (rd.ok()) {
      ChainDef d; d.key = "risk:composite"; d.family = "positioning"; d.subject = "Risk sentiment"; d.indicator = "Composite (0-100)";
      d.kind = "actual"; d.unit = "index"; d.freq = "D"; d.source = "Computed by Deepwater (Yahoo inputs)"; defs << d;
      for (int i = 0; i < rd.dates.size() && i < rd.composite.size(); ++i)
        if (std::isfinite(rd.composite[i])) { Obs o; o.chain = d.key; o.period = day(rd.dates[i]); o.value = rd.composite[i]; obs << o; }
    } else {
      positioning::RiskIndex::instance().load();
    }
    if (!obs.isEmpty()) Store::instance().add(defs, obs);
  }
  auto markets = std::make_shared<QList<positioning::CotMarket>>(positioning::Cot::markets());
  auto idx = std::make_shared<int>(0);
  auto step = std::make_shared<std::function<void()>>();
  const bool deep = !deepDone("positioning");
  *step = [this, markets, idx, step, then, deep] {
    if (*idx >= markets->size()) {
      // the closure holds its own shared_ptr (step captures step): break the cycle once the pass is over, after this
      // call has returned, or each pass leaks it with its captured lists (as the technical-data Collector does)
      QTimer::singleShot(0, this, [step] { *step = nullptr; });
      then(); return;
    }
    const auto mk = markets->at((*idx)++);
    say(QCoreApplication::translate("data", "%1 of %2 · %3 · COT %4 (%5 / %6)").arg(m_step).arg(m_steps)
            .arg(familyName("positioning"), mk.shortName).arg(*idx).arg(markets->size()));
    auto cb = [mk, step](const positioning::CotHistory& h) {
      if (!h.ok()) { QTimer::singleShot(0, [step] { (*step)(); }); return; }
      QList<ChainDef> defs; QList<Obs> obs;
      auto def = [&](const QString& key, const QString& indicator) {
        ChainDef d; d.key = key; d.family = "positioning"; d.subject = "COT " + mk.shortName; d.indicator = indicator;
        d.kind = "actual"; d.unit = "contracts"; d.freq = "W"; d.source = "CFTC Commitments of Traders (" + mk.contract + ")";
        defs << d; return key;
      };
      const QString oi = def("cot:" + mk.id.toLower() + ":oi", "Open interest");
      QHash<QString, QString> keys;
      for (const auto& w : h.weeks) {
        { Obs o; o.chain = oi; o.period = day(w.date); o.value = w.oi; obs << o; }
        auto add = [&](const QVector<positioning::CotCategory>& cats, const QString& report) {
          for (const auto& c : cats) {
            if (c.name.contains("Total")) continue;
            const QString id = report + ":" + c.name;
            if (!keys.contains(id)) keys.insert(id, def("cot:" + mk.id.toLower() + ":" + report.toLower() + ":" + c.name.toLower().replace(' ', '-') + ":net",
                                                         c.name + " net (" + report + ")"));
            Obs o; o.chain = keys.value(id); o.period = day(w.date); o.value = c.net(); obs << o;
          }
        };
        add(w.legacy, "Legacy");
        add(w.second, h.secondName() == "TFF" ? "TFF" : "Disaggregated");
      }
      Store::instance().add(defs, obs, [step](int, int) { (*step)(); });
    };
    if (deep) positioning::Cot::instance().deepHistory(mk.id, this, cb);
    else positioning::Cot::instance().history(mk.id, this, cb);
  };
  (*step)();
}
}  // namespace macrodata

// After the first import (started from the Macro data page), the facts are kept up to date from every launch.
static void startMacroDataAtLaunch() {
  QTimer::singleShot(20000, qApp, [] {
    if (!macrodata::Store::instance().meta("deep.rates").isEmpty()) macrodata::Importer::instance().start();
  });
}
Q_COREAPP_STARTUP_FUNCTION(startMacroDataAtLaunch)
