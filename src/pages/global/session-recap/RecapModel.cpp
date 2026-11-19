#include "pages/global/session-recap/RecapModel.h"
#include <QCoreApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <cmath>

namespace wire::recap {

const QVector<Asset>& assets() {
  // Index futures (around the clock) so Tokyo and London sessions have index bars too.
  static const QVector<Asset> a{
    {"ES=F", "S&P 500", "INDICES", 1, "E-mini S&P 500 futures (Yahoo ES=F)"},
    {"XAUUSD", QT_TRANSLATE_NOOP("wire", "Gold"), "METALS", 1, "Gold futures (Yahoo GC=F)"},
    {"EURUSD", "EUR/USD", "FOREX", 4, "EUR/USD spot (Yahoo EURUSD=X)"},
    {"BTCUSD", QT_TRANSLATE_NOOP("wire", "Bitcoin"), "CRYPTO", 1, "Bitcoin in USD (Yahoo BTC-USD)"},
    {"NQ=F", "Nasdaq 100", "INDICES", 1, "E-mini Nasdaq 100 futures (Yahoo NQ=F)"},
    {"XAGUSD", QT_TRANSLATE_NOOP("wire", "Silver"), "METALS", 2, "Silver futures (Yahoo SI=F)"},
    {"USDJPY", "USD/JPY", "FOREX", 2, "USD/JPY spot (Yahoo USDJPY=X)"},
    {"ETHUSD", QT_TRANSLATE_NOOP("wire", "Ethereum"), "CRYPTO", 1, "Ether in USD (Yahoo ETH-USD)"},
    {"YM=F", "Dow Jones", "INDICES", 1, "E-mini Dow futures (Yahoo YM=F)"},
    {"WTI", QT_TRANSLATE_NOOP("wire", "WTI Crude"), "ENERGY", 2, "WTI crude futures (Yahoo CL=F)"},
    {"EURGBP", "EUR/GBP", "FOREX", 4, "EUR/GBP spot (Yahoo EURGBP=X)"},
    {"NATGAS", QT_TRANSLATE_NOOP("wire", "Natural Gas"), "ENERGY", 3, "Henry Hub natural gas futures (Yahoo NG=F)"},
  };
  return a;
}
QString assetName(int i) { return QCoreApplication::translate("wire", assets()[i].name.toUtf8().constData()); }
QStringList sections() { return {"INDICES", "METALS", "FOREX", "CRYPTO", "ENERGY"}; }

// ------------------------------------------------------------------------------------------ windows
QString Window::key() const { return week ? "W" + monday.toString("yyyy-MM-dd") : session.key(); }
QString Window::code() const { return week ? QString("WEEK") : session.code(); }
QString Window::flag() const { return week ? QString() : session.flag(); }
QString Window::title() const {
  if (!week) return QCoreApplication::translate("wire", "%1 SESSION").arg(session.name());
  return QCoreApplication::translate("wire", "WEEK %1").arg(monday.weekNumber());
}

QList<Window> sessionWindows(int days) {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  QList<Window> out;
  for (const auto& s : wire::sessionsBetween(now.addDays(-days), now)) {
    Window w; w.session = s; w.start = s.start; w.end = s.end;
    out << w;
  }
  return out;
}
QList<Window> weekWindows(int weeks) {
  QList<Window> out;
  // a week starts with Tokyo's Monday session: take "today" in Tokyo, not the local date (west of Tokyo the local
  // date is still Sunday while the new week has opened, and the current week was missing)
  const QDate today = QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Tokyo")).date();
  QDate mon = today.addDays(1 - today.dayOfWeek());
  for (int i = 0; i < weeks; ++i, mon = mon.addDays(-7)) {
    Window w; w.week = true; w.monday = mon;
    w.start = wire::session(wire::Market::Tokyo, mon).start;
    w.end = wire::session(wire::Market::NewYork, mon.addDays(4)).end;
    if (w.start > QDateTime::currentDateTimeUtc()) continue;
    out << w;
  }
  return out;
}

// ------------------------------------------------------------------------------------------ model
Model::Model(QObject* parent) : QObject(parent) { m_series.resize(assets().size()); }

void Model::load(bool live) {
  m_started = true;
  m_pending = int(assets().size());
  m_ok = 0; m_error.clear();
  // A generation per load: answers of an older load still running would count down this load's m_pending (it then
  // read "loaded" early) — they are ignored, this load asks the same series. One changed() per load, when all the
  // series have answered: every changed() rebuilds the list and the centre view.
  const int gen = ++m_gen;
  for (int i = 0; i < assets().size(); ++i) {
    market::candles(assets()[i].symbol, "15m", "60d", this, [this, i, gen](const market::Series& s) {
      if (gen != m_gen) return;
      if (s.ok() || m_series[i].bars.isEmpty()) m_series[i] = s;
      if (!s.ok() && m_error.isEmpty()) m_error = assets()[i].name + ": " + s.error;
      m_ok = 0;
      for (const auto& x : m_series) if (x.ok()) ++m_ok;
      if (--m_pending <= 0) { m_pending = 0; emit changed(); }
    }, live ? 90 : 900);
  }
}

QDateTime Model::dataStart() const {
  qint64 t = 0;
  for (const auto& s : m_series) if (!s.bars.isEmpty()) t = t ? qMin(t, s.bars.first().t) : s.bars.first().t;
  return t ? QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC) : QDateTime();
}

QVector<Stat> Model::stats(const Window& w) const {
  QVector<Stat> out(assets().size());
  const qint64 a = w.start.toSecsSinceEpoch(), b = w.end.toSecsSinceEpoch();
  for (int i = 0; i < m_series.size(); ++i) {
    const auto& bars = m_series[i].bars;
    auto it = std::lower_bound(bars.begin(), bars.end(), a, [](const market::Bar& x, qint64 t) { return x.t < t; });
    Stat& s = out[i];
    for (; it != bars.end() && it->t < b; ++it) {
      if (s.t.isEmpty()) s.open = it->o;
      s.t << it->t; s.c << it->c;
      s.close = it->c;
      s.hi = std::isfinite(s.hi) ? qMax(s.hi, it->c) : it->c;
      s.lo = std::isfinite(s.lo) ? qMin(s.lo, it->c) : it->c;
    }
    if (s.t.size() >= 2 && s.open > 0) s.chg = (s.close / s.open - 1) * 100.0;
  }
  return out;
}

int Model::topMover(const QVector<Stat>& s) const {
  int best = -1;
  for (int i = 0; i < s.size(); ++i) if (s[i].ok() && (best < 0 || std::abs(s[i].chg) > std::abs(s[best].chg))) best = i;
  return best;
}

QList<wire::Release> windowReleases(const Window& w) {
  QList<wire::Release> out;
  auto* src = wire::releaseSource();
  if (!src) return out;
  for (const auto& r : src->releases(w.start, w.end)) if (r.impact >= 2) out << r;
  std::sort(out.begin(), out.end(), [](const wire::Release& a, const wire::Release& b) { return a.time < b.time; });
  return out;
}
QList<wire::Item> windowHeadlines(const Window& w) {
  QList<wire::Item> out;
  for (const auto& i : wire::Wire::instance().between(w.start, w.end)) if (i.kind == wire::Kind::Headline) out << i;
  return out;
}
QString assetTag(const QString& h) {
  static const QList<QPair<QRegularExpression, QString>> m{
    {QRegularExpression("\\b(WTI|crude|oil)\\b", QRegularExpression::CaseInsensitiveOption), "WTI"},
    {QRegularExpression("\\bnat(ural)?\\s?gas\\b", QRegularExpression::CaseInsensitiveOption), "NATGAS"},
    {QRegularExpression("\\bgold\\b", QRegularExpression::CaseInsensitiveOption), "GOLD"},
    {QRegularExpression("\\bsilver\\b", QRegularExpression::CaseInsensitiveOption), "SILVER"},
    {QRegularExpression("\\b(bitcoin|BTC)\\b", QRegularExpression::CaseInsensitiveOption), "BTC"},
    {QRegularExpression("\\b(ether|ethereum|ETH)\\b", QRegularExpression::CaseInsensitiveOption), "ETH"},
    {QRegularExpression("\\b(S&P 500|S&P|SPX|ES)\\b"), "SPX"},
    {QRegularExpression("\\b(Nasdaq|NQ)\\b"), "NDX"},
    {QRegularExpression("\\b(Dow|YM)\\b"), "DOW"},
    {QRegularExpression("EUR/?USD"), "EURUSD"},
    {QRegularExpression("USD/?JPY|\\byen\\b", QRegularExpression::CaseInsensitiveOption), "USDJPY"},
    {QRegularExpression("EUR/?GBP"), "EURGBP"},
  };
  for (const auto& [re, tag] : m) if (re.match(h).hasMatch()) return tag;
  return {};
}

// ------------------------------------------------------------------------------------------ automated read
namespace {
double notable(const QString& section) {
  if (section == "INDICES") return 0.75;
  if (section == "METALS") return 1.0;
  if (section == "FOREX") return 0.35;
  if (section == "CRYPTO") return 2.5;
  return 1.5;
}
bool fr(const QString& l) { return l == "FR"; }
QString pctAbs(double v, const QString& lang) {
  const QString n = QString::number(std::abs(v), 'f', 2);
  return fr(lang) ? QString(n).replace('.', ',') + " %" : n + "%";
}
QString cap(QString s) { if (!s.isEmpty()) s[0] = s[0].toUpper(); return s; }

QString nameIn(int i, const QString& lang, bool first) {
  const QString n = assets()[i].name;
  if (fr(lang)) {
    static const QHash<QString, QString> m{{"S&P 500", "le S&P 500"}, {"Nasdaq 100", "le Nasdaq 100"}, {"Dow Jones", "le Dow Jones"},
      {"Gold", "l'or"}, {"Silver", "l'argent"}, {"EUR/USD", "l'EUR/USD"}, {"USD/JPY", "l'USD/JPY"}, {"EUR/GBP", "l'EUR/GBP"},
      {"Bitcoin", "le bitcoin"}, {"Ethereum", "l'ether"}, {"WTI Crude", "le pétrole WTI"}, {"Natural Gas", "le gaz naturel"}};
    return m.value(n, n);
  }
  static const QHash<QString, QString> m{{"S&P 500", "the S&P 500"}, {"Nasdaq 100", "the Nasdaq 100"}, {"Dow Jones", "the Dow Jones"},
    {"Gold", "gold"}, {"Silver", "silver"}, {"Ethereum", "Ethereum"}, {"Natural Gas", "natural gas"}};
  if (n == "WTI Crude") return first ? "WTI crude oil" : "oil";
  return m.value(n, n);
}

// "rose 0.18%" / "a progressé de 0,18 %"; k varies the synonym
QString moveVerb(double c, const QString& section, const QString& lang, int k) {
  const double r = std::abs(c) / notable(section);
  const QString p = pctAbs(c, lang);
  if (std::abs(c) < 0.005) return fr(lang) ? "est resté stable" : "was flat";
  if (fr(lang)) {
    if (c > 0) {
      if (r < 0.2) return "a légèrement progressé de " + p;
      if (r < 1) return QStringList{"a progressé de ", "a gagné ", "a pris "}[k % 3] + p;
      if (r < 2) return "a grimpé de " + p;
      if (r < 3) return "a bondi de " + p;
      return "s'est envolé de " + p;
    }
    if (r < 0.2) return "a légèrement reculé de " + p;
    if (r < 1) return QStringList{"a reculé de ", "a cédé ", "a perdu "}[k % 3] + p;
    if (r < 2) return "a chuté de " + p;
    if (r < 3) return "a plongé de " + p;
    return "s'est effondré de " + p;
  }
  if (c > 0) {
    if (r < 0.2) return "edged up " + p;
    if (r < 1) return QStringList{"rose ", "gained ", "added "}[k % 3] + p;
    if (r < 2) return "climbed " + p;
    if (r < 3) return "jumped " + p;
    return "surged " + p;
  }
  if (r < 0.2) return "eased " + p;
  if (r < 1) return QStringList{"fell ", "declined ", "slipped "}[k % 3] + p;
  if (r < 2) return "dropped " + p;
  if (r < 3) return "slid " + p;
  return "tumbled " + p;
}

QString joinList(const QStringList& l, const QString& lang) {
  if (l.isEmpty()) return {};
  if (l.size() == 1) return l.first();
  const QString et = fr(lang) ? " et " : ", and ";
  if (l.size() == 2) return l[0] + (fr(lang) ? " et " : " and ") + l[1];
  return QStringList(l.mid(0, l.size() - 1)).join(", ") + et + l.last();
}

double parseNum(QString s, bool* ok) {
  s = s.trimmed();
  s.remove('%'); s.remove(','); s.remove('$'); s.remove(QChar(0x20AC)); s.remove(QChar(0xA3));
  double mult = 1;
  // the suffix scales the value: "1.02M" vs "980K" must compare as 1,020,000 vs 980,000, not 1.02 vs 980
  if (!s.isEmpty()) {
    const QChar u = s.back().toUpper();
    if (u == 'K') mult = 1e3; else if (u == 'M') mult = 1e6; else if (u == 'B') mult = 1e9; else if (u == 'T') mult = 1e12;
    if (mult != 1) s.chop(1);
  }
  bool k = false;
  const double v = s.toDouble(&k) * mult;
  if (ok) *ok = k;
  return v;
}

QString releaseSentence(const wire::Release& r, const QString& lang) {
  const QString who = wire::currencyRegion(r.currency) + " " + r.title;
  bool okA = false, okF = false;
  const double a = parseNum(r.actual, &okA), f = parseNum(r.forecast, &okF);
  if (r.actual.isEmpty()) return {};
  if (fr(lang)) {
    if (okA && okF) {
      if (std::abs(a - f) < 1e-9) return QString("%1 est ressorti à %2, conforme à la prévision").arg(who, r.actual);
      return QString("%1 est ressorti à %2, %3 de la prévision %4").arg(who, r.actual, a > f ? "au-dessus" : "en dessous", r.forecast);
    }
    return QString("%1 est ressorti à %2%3").arg(who, r.actual, r.previous.isEmpty() ? "" : " (précédent " + r.previous + ")");
  }
  if (okA && okF) {
    if (std::abs(a - f) < 1e-9) return QString("%1 matched the %2 forecast").arg(who, r.forecast);
    return QString("%1 came in at %2, %3 the %4 forecast").arg(who, r.actual, a > f ? "above" : "below", r.forecast);
  }
  return QString("%1 printed %2%3").arg(who, r.actual, r.previous.isEmpty() ? "" : " (previous " + r.previous + ")");
}

QString headlineClause(int i, double c, const QString& lang) {
  const double r = std::abs(c) / notable(assets()[i].section);
  const QString n = assets()[i].name;
  const bool strong = r >= 2;
  if (fr(lang)) {
    static const QHash<QString, QString> s{{"WTI Crude", "Le pétrole"}, {"Natural Gas", "Le gaz"}, {"Gold", "L'or"}, {"Silver", "L'argent"},
      {"Bitcoin", "Le bitcoin"}, {"Ethereum", "L'ether"}};
    if (n == "EUR/USD") return c > 0 ? "l'euro se raffermit" : "l'euro recule";
    if (n == "USD/JPY") return c > 0 ? "le yen faiblit" : "le yen se raffermit";
    if (n == "EUR/GBP") return c > 0 ? "la livre recule" : "la livre se raffermit";
    const QString subj = s.value(n, n);
    return subj + (c > 0 ? (strong ? " s'envole" : " monte") : (strong ? " plonge" : " recule"));
  }
  static const QHash<QString, QString> s{{"WTI Crude", "Oil"}, {"Natural Gas", "Gas"}, {"Ethereum", "Ether"}};
  if (n == "EUR/USD") return c > 0 ? "Euro Firms" : "Euro Slips";
  if (n == "USD/JPY") return c > 0 ? "Yen Weakens" : "Yen Firms";
  if (n == "EUR/GBP") return c > 0 ? "Sterling Slips" : "Sterling Firms";
  return s.value(n, n) + (c > 0 ? (strong ? " Surges" : " Climbs") : (strong ? " Slides" : " Falls"));
}
}

Read compose(const Window& w, const QVector<Stat>& stIn, const QList<wire::Release>& rel, const QList<wire::Item>& wireItems,
             const QString& lang) {
  Read out;
  QVector<Stat> st = stIn;
  st.resize(assets().size());
  const bool F = fr(lang);
  // ---- indices
  QList<int> idx, others;
  for (int i = 0; i < assets().size(); ++i) {
    if (!st[i].ok()) continue;
    (assets()[i].section == "INDICES" ? idx : others) << i;
  }
  int ups = 0, downs = 0; double avg = 0;
  for (int i : idx) { (st[i].chg >= 0 ? ups : downs)++; avg += st[i].chg; }
  if (!idx.isEmpty()) avg /= idx.size();
  const bool usSession = !w.week && w.session.market == wire::Market::NewYork;
  QString eq;
  if (!idx.isEmpty()) {
    if (F) {
      const QString who = usSession || w.week ? "Actions US" : "Futures US";
      eq = downs == 0 ? who + (avg >= 0.75 ? " en net rebond" : " en hausse") : ups == 0 ? who + (avg <= -0.75 ? " en net repli" : " en baisse")
                      : who + " en ordre dispersé";
    } else {
      const QString who = usSession || w.week ? "US Equities" : "US Futures";
      eq = downs == 0 ? who + (avg >= 0.75 ? " Rally" : " Rise") : ups == 0 ? who + (avg <= -0.75 ? " Slide" : " Fall") : who + " Mixed";
    }
  }
  // ---- top non-index mover (relative to its class)
  int top = -1; double topR = 0;
  for (int i : others) { const double r = std::abs(st[i].chg) / notable(assets()[i].section); if (r > topR) { topR = r; top = i; } }
  if (top >= 0 && topR >= 1) {
    QString clause = headlineClause(top, st[top].chg, lang);
    if (F && !eq.isEmpty() && !clause.isEmpty()) clause[0] = clause[0].toLower();
    out.headline = eq.isEmpty() ? cap(clause) : eq + (F ? " ; " : "; ") + clause;
  } else if (!eq.isEmpty()) {
    out.headline = eq + (F ? " ; le reste du marché reste calme" : "; Other Markets Calm");
  } else {
    out.headline = F ? "Pas encore de données pour cette fenêtre" : "No Market Data for This Window Yet";
  }
  if (!F) {   // Title Case for the clause after "; "
    const int k = out.headline.indexOf("; ");
    if (k >= 0 && k + 2 < out.headline.size()) out.headline[k + 2] = out.headline[k + 2].toUpper();
  }
  // ---- lead
  QStringList lead;
  if (!idx.isEmpty()) {
    QStringList up, dn;
    for (int i : idx) (st[i].chg >= 0 ? up : dn) << nameIn(i, lang, true);
    QString s;
    if (F) {
      if (dn.isEmpty()) s = cap(joinList(up, lang)) + (up.size() > 1 ? " ont progressé ensemble" : " a progressé");
      else if (up.isEmpty()) s = cap(joinList(dn, lang)) + (dn.size() > 1 ? " ont reculé ensemble" : " a reculé");
      else s = cap(joinList(dn, lang)) + (dn.size() > 1 ? " ont reculé" : " a reculé") + " tandis que " + joinList(up, lang) +
               (up.size() > 1 ? " ont progressé" : " a progressé");
    } else {
      if (dn.isEmpty()) s = cap(joinList(up, lang)) + " rose together";
      else if (up.isEmpty()) s = cap(joinList(dn, lang)) + " fell together";
      else s = cap(joinList(dn, lang)) + " declined while " + joinList(up, lang) + (up.size() > 1 ? " moved higher" : " edged higher");
    }
    lead << s + ".";
  }
  int beats = 0, misses = 0, inline_ = 0;
  for (const auto& r : rel) {
    bool a = false, f = false;
    const double va = parseNum(r.actual, &a), vf = parseNum(r.forecast, &f);
    if (!a || !f || r.actual.isEmpty()) continue;
    if (std::abs(va - vf) < 1e-9) inline_++; else if (va > vf) beats++; else misses++;
  }
  if (!rel.isEmpty()) {
    const int scored = beats + misses + inline_;
    if (scored == 0) {
      if (F) lead << QString("%1 publication%2 au calendrier dans la fenêtre, sans résultat enregistré.").arg(rel.size()).arg(rel.size() > 1 ? "s" : "");
      else lead << QString("%1 release%2 scheduled in the window, no actual recorded.").arg(rel.size()).arg(rel.size() > 1 ? "s were" : " was");
    } else if (F) {
      QStringList parts;
      if (beats) parts << QString("%1 au-dessus des attentes").arg(beats);
      if (misses) parts << QString("%1 en dessous").arg(misses);
      if (inline_) parts << QString("%1 conforme%2").arg(inline_).arg(inline_ > 1 ? "s" : "");
      lead << QString("%1 publication%2 dans la fenêtre : %3.").arg(rel.size()).arg(rel.size() > 1 ? "s" : "").arg(joinList(parts, lang));
    } else {
      QStringList parts;
      if (beats) parts << QString("%1 above forecast").arg(beats);
      if (misses) parts << QString("%1 below").arg(misses);
      if (inline_) parts << QString("%1 in line").arg(inline_);
      lead << QString("%1 release%2 fell in the window: %3.").arg(rel.size()).arg(rel.size() > 1 ? "s" : "").arg(joinList(parts, lang));
    }
  }
  QList<int> sorted = others;
  std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
    return std::abs(st[a].chg) / notable(assets()[a].section) > std::abs(st[b].chg) / notable(assets()[b].section); });
  QStringList moves;
  for (int k = 0; k < sorted.size() && k < 3; ++k) {
    const int i = sorted[k];
    moves << nameIn(i, lang, false) + " " + moveVerb(st[i].chg, assets()[i].section, lang, k);
  }
  if (!moves.isEmpty()) lead << cap(joinList(moves, lang)) + ".";
  if (!wireItems.isEmpty()) {
    QHash<qint64, int> buckets;
    const qint64 b = w.bucketMinutes() * 60;
    for (const auto& it : wireItems) buckets[it.time.toSecsSinceEpoch() / b]++;
    qint64 peak = 0; int peakN = 0;
    for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) if (it.value() > peakN) { peakN = it.value(); peak = it.key(); }
    const QString when = QDateTime::fromSecsSinceEpoch(peak * b, QTimeZone::UTC).toLocalTime().toString(w.week ? "ddd HH:mm" : "HH:mm");
    if (peakN >= 3)
      lead << (F ? QString("Le fil a porté %1 titres, avec un pic vers %2 (%3).").arg(wireItems.size()).arg(when).arg(peakN)
                 : QString("The wire carried %1 headlines, busiest around %2 (%3).").arg(wireItems.size()).arg(when).arg(peakN));
    else
      lead << (F ? QString("Le fil a porté %1 titre%2.").arg(wireItems.size()).arg(wireItems.size() > 1 ? "s" : "")
                 : QString("The wire carried %1 headline%2.").arg(wireItems.size()).arg(wireItems.size() > 1 ? "s" : ""));
  }
  out.lead = lead.join(" ");
  // ---- sections
  QHash<QString, QStringList> relBySection;
  for (const auto& r : rel) {
    const QString s = releaseSentence(r, lang);
    if (s.isEmpty()) continue;
    relBySection[r.currency == "USD" ? "INDICES" : "FOREX"] << s;
  }
  static const QHash<QString, QString> frLabel{{"METALS", "MÉTAUX"}, {"ENERGY", "ÉNERGIE"}};
  for (const QString& sec : sections()) {
    Read::Section s;
    s.label = F ? frLabel.value(sec, sec) : sec;
    QStringList parts;
    int k = 0;
    for (int i = 0; i < assets().size(); ++i) {
      if (assets()[i].section != sec) continue;
      s.assets << i;
      if (st[i].ok()) parts << nameIn(i, lang, k == 0) + " " + moveVerb(st[i].chg, sec, lang, k);
      ++k;
    }
    QString text = parts.isEmpty() ? (F ? "Aucune barre de 15 minutes dans cette fenêtre." : "No 15-minute bars in this window.")
                                   : cap(joinList(parts, lang)) + ".";
    const QStringList rs = relBySection.value(sec).mid(0, 4);
    if (!rs.isEmpty()) text += " " + cap(rs.join(F ? " ; " : "; ")) + ".";
    s.text = text;
    out.sections << s;
  }
  return out;
}
}
