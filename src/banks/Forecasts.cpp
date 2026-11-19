#include "banks/Forecasts.h"
#include "banks/Registry.h"
#include "net/Http.h"
#include <QRegularExpression>
#include <QCoreApplication>
#include <QTextDocumentFragment>
#include <QSet>
#include <algorithm>

namespace banks {

// --------------------------------------------------------------------------------------------- classes
QList<AssetClass> assetClasses() {
  return {AssetClass::FX, AssetClass::AsiaEmFx, AssetClass::Commodities, AssetClass::Indices, AssetClass::Rates, AssetClass::Crypto};
}
QString className(AssetClass c) {
  switch (c) {
    case AssetClass::FX: return QCoreApplication::translate("banks", "FX");
    case AssetClass::AsiaEmFx: return QCoreApplication::translate("banks", "ASIA & EM FX");
    case AssetClass::Commodities: return QCoreApplication::translate("banks", "COMMODITIES");
    case AssetClass::Indices: return QCoreApplication::translate("banks", "INDICES");
    case AssetClass::Rates: return QCoreApplication::translate("banks", "RATES");
    case AssetClass::Crypto: return QCoreApplication::translate("banks", "CRYPTO");
  }
  return {};
}
QString classSubtitle(AssetClass c) {
  switch (c) {
    case AssetClass::FX: return QCoreApplication::translate("banks", "G10 MAJORS, CROSSES & THE DOLLAR");
    case AssetClass::AsiaEmFx: return QCoreApplication::translate("banks", "ASIA, SCANDIES, CEE, LATAM");
    case AssetClass::Commodities: return QCoreApplication::translate("banks", "METALS, ENERGY");
    case AssetClass::Indices: return QCoreApplication::translate("banks", "US, EUROPE, ASIA");
    case AssetClass::Rates: return QCoreApplication::translate("banks", "GOVERNMENT BOND YIELDS");
    case AssetClass::Crypto: return QCoreApplication::translate("banks", "MAJORS");
  }
  return {};
}
double classThreshold(AssetClass c) {
  switch (c) {
    case AssetClass::FX: return 1.0;          // %
    case AssetClass::AsiaEmFx: return 1.5;    // %
    case AssetClass::Commodities: return 3.0; // %
    case AssetClass::Indices: return 3.0;     // %
    case AssetClass::Rates: return 0.15;      // yield points (15 bp)
    case AssetClass::Crypto: return 5.0;      // %
  }
  return 1.0;
}
QString biasName(Bias b) { return b == Bias::Long ? "LONG" : b == Bias::Short ? "SHORT" : "RANGE"; }

// --------------------------------------------------------------------------------------------- quarters
QDate Quarter::end() const { return valid() ? QDate(year, q * 3, 1).addMonths(1).addDays(-1) : QDate(); }
QString Quarter::label() const {
  return valid() ? QCoreApplication::translate("banks", "Q%1 '%2").arg(q).arg(year % 100, 2, 10, QChar('0')) : QString();
}
Quarter Quarter::of(const QDate& d) { return {d.year(), (d.month() - 1) / 3 + 1}; }
Quarter Quarter::parse(const QString& s) {
  static const QRegularExpression a(R"(^\s*([1-4])Q(\d{2})F?\s*$)"), b(R"(^\s*Q([1-4])\s*'?(\d{2})\s*$)");
  if (auto m = a.match(s); m.hasMatch()) return {2000 + m.captured(2).toInt(), m.captured(1).toInt()};
  if (auto m = b.match(s); m.hasMatch()) return {2000 + m.captured(2).toInt(), m.captured(1).toInt()};
  return {};
}
QList<Quarter> Quarter::upcoming(int n, const QDate& today) {
  QList<Quarter> out;
  Quarter q = of(today);
  for (int i = 0; i < qMax(1, n - 1); ++i) { out << q; q = q.q == 4 ? Quarter{q.year + 1, 1} : Quarter{q.year, q.q + 1}; }
  out << Quarter{of(today).year + 2, 4};
  return out;
}

// --------------------------------------------------------------------------------------------- universe
const QList<Market>& markets() {
  static const QList<Market> l = [] {
    using C = AssetClass;
    auto fx = [](QString id, C c, QString spot, int dec) {
      return Market{id, id.left(3) + "/" + id.mid(3), c, spot, 1.0, {}, dec};
    };
    QList<Market> m{
      fx("EURUSD", C::FX, "EURUSD", 4), fx("USDJPY", C::FX, "USDJPY", 2), fx("GBPUSD", C::FX, "GBPUSD", 4),
      fx("AUDUSD", C::FX, "AUDUSD", 4), fx("NZDUSD", C::FX, "NZDUSD", 4), fx("USDCAD", C::FX, "USDCAD", 4),
      fx("USDCHF", C::FX, "USDCHF", 4), fx("EURGBP", C::FX, "EURGBP", 4), fx("EURJPY", C::FX, "EURJPY", 2),
      fx("EURCHF", C::FX, "EURCHF", 4), fx("EURDKK", C::FX, "EURDKK=X", 4),
      fx("EURSEK", C::AsiaEmFx, "EURSEK", 4), fx("EURNOK", C::AsiaEmFx, "EURNOK", 4), fx("EURPLN", C::AsiaEmFx, "EURPLN", 4),
      fx("EURHUF", C::AsiaEmFx, "EURHUF=X", 2), fx("EURCZK", C::AsiaEmFx, "EURCZK=X", 3), fx("EURRON", C::AsiaEmFx, "EURRON=X", 4),
      fx("USDTRY", C::AsiaEmFx, "USDTRY", 3), fx("USDZAR", C::AsiaEmFx, "USDZAR", 3), fx("USDILS", C::AsiaEmFx, "USDILS=X", 4),
      fx("USDMXN", C::AsiaEmFx, "USDMXN", 3), fx("USDBRL", C::AsiaEmFx, "USDBRL", 3), fx("USDCLP", C::AsiaEmFx, "USDCLP=X", 1),
      fx("USDCNY", C::AsiaEmFx, "USDCNY=X", 4), fx("USDINR", C::AsiaEmFx, "USDINR", 2), fx("USDIDR", C::AsiaEmFx, "USDIDR=X", 0),
      fx("USDKRW", C::AsiaEmFx, "USDKRW=X", 1), fx("USDPHP", C::AsiaEmFx, "USDPHP=X", 2), fx("USDSGD", C::AsiaEmFx, "USDSGD", 4),
      fx("USDTWD", C::AsiaEmFx, "USDTWD=X", 3), fx("USDTHB", C::AsiaEmFx, "USDTHB=X", 2), fx("USDMYR", C::AsiaEmFx, "USDMYR=X", 4),
      {"XAUUSD", QT_TRANSLATE_NOOP("banks", "Gold"), C::Commodities, "XAUUSD", 1.0, "$/oz", 0},
      {"XAGUSD", QT_TRANSLATE_NOOP("banks", "Silver"), C::Commodities, "XAGUSD", 1.0, "$/oz", 2},
      {"COPPER", QT_TRANSLATE_NOOP("banks", "Copper"), C::Commodities, "COPPER", 2204.62262, "$/t", 0},
      {"ALUMINIUM", QT_TRANSLATE_NOOP("banks", "Aluminium"), C::Commodities, "ALI=F", 1.0, "$/t", 0},
      {"BRENT", QT_TRANSLATE_NOOP("banks", "Brent"), C::Commodities, "BRENT", 1.0, "$/bbl", 1},
      {"WTI", QT_TRANSLATE_NOOP("banks", "WTI"), C::Commodities, "WTI", 1.0, "$/bbl", 1},
      {"TTF", QT_TRANSLATE_NOOP("banks", "Dutch TTF gas"), C::Commodities, "TTF=F", 1.0, "EUR/MWh", 1},
      {"SPX", QT_TRANSLATE_NOOP("banks", "S&P 500"), C::Indices, "SPX", 1.0, {}, 0}, {"NDX", QT_TRANSLATE_NOOP("banks", "Nasdaq 100"), C::Indices, "NDX", 1.0, {}, 0},
      {"DOW", QT_TRANSLATE_NOOP("banks", "Dow Jones"), C::Indices, "DOW", 1.0, {}, 0}, {"STOXX50", QT_TRANSLATE_NOOP("banks", "Euro Stoxx 50"), C::Indices, "STOXX50", 1.0, {}, 0},
      {"DAX", QT_TRANSLATE_NOOP("banks", "DAX"), C::Indices, "DAX", 1.0, {}, 0}, {"FTSE", QT_TRANSLATE_NOOP("banks", "FTSE 100"), C::Indices, "FTSE", 1.0, {}, 0},
      {"NIKKEI", QT_TRANSLATE_NOOP("banks", "Nikkei 225"), C::Indices, "NIKKEI", 1.0, {}, 0}, {"HSI", QT_TRANSLATE_NOOP("banks", "Hang Seng"), C::Indices, "HSI", 1.0, {}, 0},
      {"US5Y", QT_TRANSLATE_NOOP("banks", "UST 5Y"), C::Rates, "US5Y", 1.0, "%", 2}, {"US10Y", QT_TRANSLATE_NOOP("banks", "UST 10Y"), C::Rates, "US10Y", 1.0, "%", 2},
      {"US30Y", QT_TRANSLATE_NOOP("banks", "UST 30Y"), C::Rates, "US30Y", 1.0, "%", 2},
      {"BTCUSD", QT_TRANSLATE_NOOP("banks", "Bitcoin"), C::Crypto, "BTCUSD", 1.0, {}, 0}, {"ETHUSD", QT_TRANSLATE_NOOP("banks", "Ether"), C::Crypto, "ETHUSD", 1.0, {}, 0},
      {"SOLUSD", QT_TRANSLATE_NOOP("banks", "Solana"), C::Crypto, "SOLUSD", 1.0, {}, 2},
    };
    return m;
  }();
  return l;
}
QString Market::name() const { return QCoreApplication::translate("banks", display.toUtf8().constData()); }

const Market* market(const QString& id) {
  for (const auto& m : markets()) if (m.id == id) return &m;
  return nullptr;
}
QList<Market> markets(AssetClass c) { QList<Market> o; for (const auto& m : markets()) if (m.cls == c) o << m; return o; }

// --------------------------------------------------------------------------------------------- service
Forecasts& Forecasts::instance() { static Forecasts f; return f; }
Forecasts::Forecasts() {}

void Forecasts::refresh() {
  net::get(QUrl("https://think.ing.com/forecasts/"), 6 * 3600, this, [this](const net::Reply& r) {
    if (!r.ok()) { m_error = QCoreApplication::translate("banks", "ING THINK forecasts unavailable: %1").arg(r.error); emit updated(); return; }
    readIng(r.body, r.fetched);
  });
}

namespace {
struct Row { QStringList cells; };
struct Table { QString title; QDate updated; QStringList head; QList<Row> rows; };

QString cellText(QString s) {
  static const QRegularExpression tag("<[^>]*>");
  s.remove(tag);
  s.replace("&nbsp;", " ").replace("&amp;", "&").replace("&#039;", "'").replace("&quot;", "\"").replace("&lt;", "<").replace("&gt;", ">");
  return s.simplified();
}

QList<Table> parseTables(const QString& h, const QDate& today) {
  static const QRegularExpression tbl(R"(<table.*?</table>)", QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression rowRe(R"(<tr.*?</tr>)", QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression td(R"(<t[hd][^>]*>(.*?)</t[hd]>)", QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression h2(R"(<h2[^>]*>([^<]*)</h2>\s*<p>\s*Last updated:\s*([^<]+)</p>)");
  QList<QPair<qsizetype, QPair<QString, QDate>>> heads;
  for (auto it = h2.globalMatch(h); it.hasNext();) {
    const auto m = it.next();
    QDate d = QDate::fromString(m.captured(2).trimmed() + " " + QString::number(today.year()), "d MMMM yyyy");
    if (d.isValid() && d > today) d = d.addYears(-1);
    heads << qMakePair(m.capturedStart(), qMakePair(cellText(m.captured(1)), d));
  }
  QList<Table> out;
  for (auto it = tbl.globalMatch(h); it.hasNext();) {
    const auto m = it.next();
    Table t;
    for (const auto& hd : heads) if (hd.first < m.capturedStart()) { t.title = hd.second.first; t.updated = hd.second.second; }
    bool first = true;
    for (auto rit = rowRe.globalMatch(m.captured(0)); rit.hasNext();) {
      QStringList cells;
      for (auto cit = td.globalMatch(rit.next().captured(0)); cit.hasNext();) cells << cellText(cit.next().captured(1));
      if (first) { t.head = cells; first = false; } else t.rows << Row{cells};
    }
    out << t;
  }
  return out;
}

double num(QString s) { s.remove(','); s = s.trimmed(); bool ok = false; const double v = s.toDouble(&ok); return ok ? v : NAN; }
}

void Forecasts::readIng(const QByteArray& body, const QDateTime& fetched) {
  const QString html = QString::fromUtf8(body);
  const QDate today = fetched.isValid() ? fetched.date() : QDate::currentDate();
  const QString src = "https://think.ing.com/forecasts/";
  QList<Forecast> out;
  auto add = [&](const QString& market, const Table& t, const QStringList& cells, int firstValue, bool avg) {
    for (int c = firstValue; c < cells.size() && c < t.head.size(); ++c) {
      const Quarter q = Quarter::parse(t.head[c]);
      const double v = num(cells[c]);
      if (!q.valid() || !std::isfinite(v)) continue;     // yearly columns (2026F) are skipped
      out << Forecast{"ing", market, q, v, avg, t.updated, src};
    }
  };
  static const QRegularExpression pair(R"(^([A-Z]{3})/([A-Z]{3})$)");
  static const QHash<QString, QString> commodity{
    {"Brent ($/bbl)", "BRENT"}, {"Dutch TTF (EUR/MWh)", "TTF"}, {"Copper ($/t)", "COPPER"}, {"Aluminium ($/t)", "ALUMINIUM"},
    {"Gold ($/oz)", "XAUUSD"}, {"Silver ($/oz)", "XAGUSD"}};
  static const QHash<QString, QString> bonds{{"UST 5Y", "US5Y"}, {"UST 10Y", "US10Y"}, {"UST 30Y", "US30Y"}};
  for (const Table& t : parseTables(html, today)) {
    for (const Row& r : t.rows) {
      if (r.cells.size() < 3) continue;
      if (t.title == "FX") {
        const auto m = pair.match(r.cells.value(1));
        if (m.hasMatch() && market(m.captured(1) + m.captured(2))) add(m.captured(1) + m.captured(2), t, r.cells, 2, false);
      } else if (t.title == "Commodities") {
        if (commodity.contains(r.cells[0])) add(commodity.value(r.cells[0]), t, r.cells, 1, true);
      } else if (t.title.startsWith("Bonds")) {
        if (bonds.contains(r.cells[0])) add(bonds.value(r.cells[0]), t, r.cells, 1, false);
      }
    }
  }
  if (out.isEmpty()) m_error = QCoreApplication::translate("banks", "ING THINK forecasts: no table recognised (page layout changed?)");
  else { m_error.clear(); m_all.erase(std::remove_if(m_all.begin(), m_all.end(), [](const Forecast& f) { return f.bankId == "ing"; }), m_all.end()); m_all << out; }
  m_loaded = true;
  emit updated();
}

QList<Forecast> Forecasts::forecasts(const QString& id) const {
  if (id.isEmpty()) return m_all;
  QList<Forecast> o; for (const auto& f : m_all) if (f.marketId == id) o << f;
  return o;
}
QStringList Forecasts::banksFor(const QString& id) const {
  QStringList b; for (const auto& f : m_all) if (f.marketId == id && !b.contains(f.bankId)) b << f.bankId;
  std::sort(b.begin(), b.end(), [](const QString& x, const QString& y) {
    const Bank* a = bank(x); const Bank* c = bank(y);
    return (a ? a->legalName : x).compare(c ? c->legalName : y, Qt::CaseInsensitive) < 0;
  });
  return b;
}
QStringList Forecasts::banksIncluded() const { QStringList b; for (const auto& f : m_all) if (!b.contains(f.bankId)) b << f.bankId; return b; }

QList<Call> Forecasts::calls(AssetClass c, const std::function<double(const QString&)>& spotOf) const {
  QList<Call> out;
  const QDate today = QDate::currentDate();
  for (const Market& m : markets(c)) {
    if (m.spot.isEmpty()) continue;
    const double spot = spotOf(m.spot) * m.spotToForecast;
    if (!std::isfinite(spot) || spot <= 0) continue;
    // per bank: the forecast of the nearest quarter that has not ended yet
    QHash<QString, Forecast> best;
    for (const auto& f : m_all) {
      if (f.marketId != m.id || f.quarter.end() < today) continue;
      auto it = best.find(f.bankId);
      if (it == best.end() || f.quarter < it->quarter) best.insert(f.bankId, f);
    }
    for (const auto& f : best) {
      Call k; k.bankId = f.bankId; k.marketId = m.id; k.horizon = f.quarter; k.forecast = f.value; k.spot = spot; k.revised = f.revised;
      const double th = classThreshold(c);
      if (c == AssetClass::Rates) {
        k.change = f.value - spot;                       // yield points
        k.bias = k.change < -th ? Bias::Long : k.change > th ? Bias::Short : Bias::Range;
      } else {
        k.change = (f.value / spot - 1.0) * 100.0;
        k.bias = k.change > th ? Bias::Long : k.change < -th ? Bias::Short : Bias::Range;
      }
      out << k;
    }
  }
  return out;
}

ClassSummary Forecasts::summary(AssetClass c, const std::function<double(const QString&)>& spotOf) const {
  ClassSummary s;
  s.universe = int(markets(c).size());
  QSet<QString> bk, mk;
  for (const auto& k : calls(c, spotOf)) {
    (k.bias == Bias::Long ? s.longs : k.bias == Bias::Short ? s.shorts : s.ranges)++;
    bk.insert(k.bankId); mk.insert(k.marketId);
  }
  s.banks = int(bk.size()); s.covered = int(mk.size());
  return s;
}

QStringList Forecasts::spotSymbols() const { QStringList o; for (const auto& m : markets()) if (!m.spot.isEmpty()) o << m.spot; return o; }

QStringList Forecasts::fxPairs() const {
  QHash<QString, QSet<QString>> cover;
  for (const auto& f : m_all) {
    const Market* m = market(f.marketId);
    if (m && (m->cls == AssetClass::FX || m->cls == AssetClass::AsiaEmFx)) cover[f.marketId].insert(f.bankId);
  }
  QStringList ids = cover.keys();
  // most banks first, then the universe order (majors first)
  auto order = [](const QString& id) { int i = 0; for (const auto& m : markets()) { if (m.id == id) return i; ++i; } return 999; };
  std::sort(ids.begin(), ids.end(), [&](const QString& a, const QString& b) {
    if (cover[a].size() != cover[b].size()) return cover[a].size() > cover[b].size();
    return order(a) < order(b);
  });
  return ids;
}
}
