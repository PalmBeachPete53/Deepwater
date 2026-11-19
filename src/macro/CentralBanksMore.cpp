// Odds for the BoE, BoJ, SNB, RBA and RBNZ (the Fed, ECB and BoC are in CentralBanks.cpp).
#include "macro/CentralBanks.h"
#include "macro/CentralBanksImpl.h"
#include "macro/Csv.h"
#include "macro/Ocr.h"
#include "macro/Xlsx.h"
#include "net/Http.h"
#include "net/Sources.h"
#include "core/Paths.h"
#include "wire/Wire.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <QTimer>
#include <QCoreApplication>
#include <memory>

namespace macro {
namespace {
const sources::Registrar regJpx({"jpx", "Japan Exchange Group (JPX / OSE) daily settlement prices", {"www.jpx.co.jp"},
    sources::Access::Keyless, 5000, "3-month TONA futures settlement prices (daily CSV of all OSE derivatives)",
    "Public file on jpx.co.jp (robots.txt allows all); JPX terms: information for personal use, source credited; "
    "one index page + one CSV per day, cached", {"economy/rates", "global/overview"}});
const sources::Registrar regBojApi({"boj-stat", "Bank of Japan Time-Series Data Search (API)", {"www.stat-search.boj.or.jp"},
    sources::Access::Keyless, 3000, "Uncollateralized overnight call rate (TONA), daily",
    "BoJ statistics: free use with the source credited", {"economy/rates", "global/overview"}});
const sources::Registrar regEurex({"eurex", "Eurex (Deutsche Börse) public product statistics", {"www.eurex.com"},
    sources::Access::Keyless, 30000, "3M SARON futures (FSR3) daily settlement prices",
    "Delayed public data from Eurex's own product page API, personal display; robots.txt crawl-delay 30 s respected "
    "(requests 30 s apart), past days cached 30 days", {"economy/rates", "global/overview"}});

QDate parseYmd(const QString& s) { return QDate::fromString(s, "yyyyMMdd"); }

template <class T>
void runAsync(QObject* ctx, std::function<T()> work, std::function<void(T)> done) {
  auto* w = new QFutureWatcher<T>(ctx);
  QObject::connect(w, &QFutureWatcher<T>::finished, ctx, [w, done] { done(w->result()); w->deleteLater(); });
  w->setFuture(QtConcurrent::run(std::move(work)));
}

// rates after each meeting → path + odds (shared by the curve-based and table-based sources)
void applyRates(Bank& b, const QList<MeetingDate>& meets, double r0, const QVector<double>& r) {
  const QVector<Odds> odds = oddsFromPath(r0, r);
  b.path.clear();
  double prev = r0;
  for (int i = 0; i < odds.size() && i < meets.size() && i < 10; ++i) {
    PathPoint p; p.meeting = meets[i]; p.rate = b.rate + (r[i] - r0); p.bp = (r[i] - prev) * 100; p.odds = odds[i];
    prev = r[i]; b.path << p;
  }
  b.priced = !b.path.isEmpty();
  b.impliedBp = b.priced ? b.path.first().bp : NAN;
  b.pricedAt = QDateTime::currentDateTime();
}
}

namespace detail {
QJsonDocument readJson(const QString& file, bool* ok) {
  *ok = true;
  QFile f(file);
  if (!f.exists()) return {};
  if (!f.open(QIODevice::ReadOnly)) { *ok = false; return {}; }
  const QByteArray b = f.readAll();
  if (b.trimmed().isEmpty()) return {};
  QJsonParseError err;
  const QJsonDocument d = QJsonDocument::fromJson(b, &err);
  if (err.error != QJsonParseError::NoError) {   // set aside, never overwritten: the history starts again beside it
    f.close();
    const QString aside = file + ".unreadable-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmss");
    *ok = QFile::rename(file, aside);
    qWarning("[macro] %s does not parse, moved to %s", qPrintable(file), qPrintable(aside));
    return {};
  }
  return d;
}
bool writeJson(const QString& file, const QJsonDocument& doc) {
  QSaveFile f(file);
  if (!f.open(QIODevice::WriteOnly)) return false;
  f.write(doc.toJson(QJsonDocument::Compact));
  return f.commit();
}

void recordSnapshots(const QString& name, qint64 fetched, Gather& g) {
  const QString file = paths::data("macro") + "/" + name;
  bool readOk = true;
  QJsonArray hist = readJson(file, &readOk).array();
  const qint64 lastT = hist.isEmpty() ? 0 : qint64(hist.last().toObject().value("t").toDouble());
  if (readOk && std::llabs(fetched - lastT) > 50 * 60 && fetched > lastT) {
    QJsonObject snap; snap.insert("t", double(fetched));
    QJsonObject px; for (auto it = g.daily.cbegin(); it != g.daily.cend(); ++it) if (!it->bars.isEmpty()) px.insert(it.key(), it->bars.last().c);
    snap.insert("px", px); hist.append(snap);
    while (!hist.isEmpty() && fetched - qint64(hist.first().toObject().value("t").toDouble()) > 120 * 86400) hist.removeFirst();
    writeJson(file, QJsonDocument(hist));
  }
  for (const auto& h : hist) {
    const auto o = h.toObject(); const qint64 t = qint64(o.value("t").toDouble());
    if (t == fetched) continue;
    const auto px = o.value("px").toObject();
    for (auto it = px.begin(); it != px.end(); ++it)
      if (g.daily.contains(it.key())) { const double v = it.value().toDouble(); g.daily[it.key()].bars.append({t, v, v, v, v, 0}); }
  }
  for (auto& ser : g.daily) std::sort(ser.bars.begin(), ser.bars.end(), [](auto& a, auto& b) { return a.t < b.t; });
}
}

// ------------------------------------------------------------------------------------------------ BoE
void CentralBanks::loadBoePricing() {
  Bank* b = mut("BOE");
  // today's level is SONIA (IADB, loadRates): wait for it a minute, then go on with the curve's own level (and say so)
  if ((!std::isfinite(b->rate) || m_sonia.isEmpty()) && m_boeWaits < 12) {
    ++m_boeWaits;
    QTimer::singleShot(5000, this, [this] { loadBoePricing(); });
    return;
  }
  m_boeWaits = 0;
  if (!std::isfinite(b->rate)) return;   // no Bank Rate from IADB or BIS: the next refresh (ensureLoaded) tries again
  net::Request r; r.ttlSec = 6 * 3600; r.timeoutMs = 60000;
  r.url = QUrl("https://www.bankofengland.co.uk/-/media/boe/files/statistics/yield-curves/latest-yield-curve-data.zip");
  net::get(r, this, [this](const net::Reply& rep) {
    Bank* bb = mut("BOE");
    if (!rep.ok()) { bb->pricingNote = QCoreApplication::translate("economy", "Bank of England yield-curve file did not answer (%1).").arg(rep.error); changed(); return; }
    using Row = detail::OisCurve;
    runAsync<QVector<Row>>(this, [body = rep.body]() {
      QVector<Row> out;
      const auto files = xlsx::unzip(body);
      QByteArray file;
      for (auto it = files.cbegin(); it != files.cend(); ++it) if (it.key().startsWith("OIS daily data")) file = *it;
      bool ok = false;
      const auto rows = xlsx::sheet(file, "1. fwds, short end", &ok);
      QVector<double> months;
      for (const auto& row : rows) {
        if (row.isEmpty()) continue;
        if (row[0].toString() == "months:") { for (int c = 1; c < row.size(); ++c) months << row[c].toDouble(); continue; }
        if (row[0].typeId() != QMetaType::Double || months.isEmpty()) continue;
        Row rr; rr.date = QDate(1899, 12, 30).addDays(qint64(row[0].toDouble()));
        for (int c = 1; c < row.size() && c - 1 < months.size(); ++c)
          if (row[c].typeId() == QMetaType::Double) { rr.months << months[c - 1]; rr.fwd << row[c].toDouble(); }
        if (rr.fwd.size() >= 6) out << rr;
      }
      return out;
    }, [this](QVector<Row> rows) {
      Bank* bb = mut("BOE");
      if (rows.isEmpty()) { bb->pricingNote = QCoreApplication::translate("economy", "Bank of England OIS forward curve: sheet not found in the file."); changed(); return; }
      const QList<MeetingDate> meets = bb->upcoming(10);
      if (meets.isEmpty()) return;
      QVector<QDate> dec; for (const auto& m : meets) dec << m.decision;
      // SONIA of a curve's day (the latest fixing on or before it, at most a week old)
      auto soniaOn = [this](const QDate& d) {
        double v = NAN;
        for (const auto& [dt, x] : m_sonia) if (dt <= d && dt.daysTo(d) <= 7) v = x;
        return v;
      };
      // expected Bank-Rate level after each meeting; today's level = SONIA of the curve's day, except for a curve older
      // than a meeting that has passed since (its SONIA predates that move: the curve's level after it is used)
      auto solve = [&](const Row& row, double* r0) {
        bool pastMeeting = false;
        for (const auto& m : bb->meetings) if (m.decision > row.date && m.decision < dec.first()) pastMeeting = true;
        return detail::boeMeetingRates(row, dec, pastMeeting ? NAN : soniaOn(row.date), r0);
      };
      double r0 = NAN;
      const QVector<double> r = solve(rows.last(), &r0);
      if (!std::isfinite(r0)) return;
      const bool noSonia = !std::isfinite(soniaOn(rows.last().date));
      m_boeNoSonia = noSonia;
      applyRates(*bb, meets, r0, r);
      bb->impliedBpWeekAgo = NAN;
      for (int i = rows.size() - 1; i >= 0; --i)
        if (rows[i].date <= rows.last().date.addDays(-7)) {
          double w0 = NAN; const QVector<double> rw = solve(rows[i], &w0);
          if (!rw.isEmpty() && std::isfinite(rw[0])) bb->impliedBpWeekAgo = (rw[0] - w0) * 100;
          break;
        }
      bb->history.clear();
      for (const auto& row : rows) {
        double h0 = NAN; const QVector<double> rh = solve(row, &h0);
        OddsSample s; s.t = QDateTime(row.date, QTime(16, 30), QTimeZone::UTC);
        s.meetings = oddsFromPath(h0, rh).mid(0, bb->path.size());
        if (!s.meetings.isEmpty()) bb->history << s;
      }
      bb->historySource = "daily curves of the current month";
      // Facts: the source without its date (the date is the observation's, the source stays the same)
      bb->pricingSourceEn = QStringLiteral("computed by Deepwater from the Bank of England's daily OIS (SONIA) instantaneous forward curve");
      bb->pricingSource = QCoreApplication::translate("economy", "computed by Deepwater from the Bank of England's daily OIS (SONIA) instantaneous forward curve "
                          "(Bank calculations from Bloomberg data; BoE terms: personal display), curve of %1").arg(rows.last().date.toString("yyyy-MM-dd"));
      bb->pricingNote = noSonia ? QCoreApplication::translate("economy", "SONIA (Bank of England IADB) did not answer: today's level is read from the curve, "
                                  "so a move priced for a meeting less than a month away can show as Hold.")
                                : QString();
      changed();
    });
  });
}

// ------------------------------------------------------------------------------------------------ BoJ
void CentralBanks::loadBojPricing() {
  // 1) today's TONA (BoJ), 2) JPX settlement file
  net::Request t; t.ttlSec = 6 * 3600;
  t.url = QUrl("https://www.stat-search.boj.or.jp/api/v1/getDataCode?format=json&lang=en&db=FM01&code=STRDCLUCON&startDate=" +
               QDate::currentDate().addMonths(-1).toString("yyyyMM"));
  net::get(t, this, [this](const net::Reply& rep) {
    const auto rs = QJsonDocument::fromJson(rep.body).object().value("RESULTSET").toArray();
    if (rep.ok() && !rs.isEmpty()) {
      const auto vals = rs[0].toObject().value("VALUES").toObject().value("VALUES").toArray();
      for (int i = vals.size() - 1; i >= 0; --i) if (!vals[i].isNull()) { mut("BOJ")->overnight = vals[i].toDouble(); break; }
    }
    Bank* b = mut("BOJ");
    if (!std::isfinite(b->overnight)) { b->pricingNote = QCoreApplication::translate("economy", "Bank of Japan TONA series did not answer."); changed(); return; }
    net::Request ix; ix.ttlSec = 3 * 3600;
    ix.url = QUrl("https://www.jpx.co.jp/english/markets/derivatives/settlement-price/index.html");
    net::get(ix, this, [this](const net::Reply& ir) {
      const auto m = QRegularExpression("href=\"([^\"]*rb_e(\\d{8})\\.csv)\"").match(QString::fromUtf8(ir.body));
      if (!ir.ok() || !m.hasMatch()) { mut("BOJ")->pricingNote = QCoreApplication::translate("economy", "JPX settlement-price page did not answer."); changed(); return; }
      const QDate fileDate = parseYmd(m.captured(2));
      net::Request c; c.ttlSec = 14 * 24 * 3600; c.timeoutMs = 90000;
      c.url = QUrl("https://www.jpx.co.jp" + m.captured(1));
      net::get(c, this, [this, fileDate](const net::Reply& cr) {
        Bank* bb = mut("BOJ");
        if (!cr.ok()) { bb->pricingNote = QCoreApplication::translate("economy", "JPX settlement file did not answer (%1).").arg(cr.error); changed(); return; }
        if (!std::isfinite(bb->rate)) { QTimer::singleShot(5000, this, [this] { m_started.remove("bojpricing"); ensureLoaded(); }); return; }
        QList<QPair<QString, RateContract>> contracts;
        detail::Gather g;
        const qint64 t = QDateTime(fileDate, QTime(6, 15), QTimeZone::UTC).toSecsSinceEpoch();
        for (const QByteArray& line : cr.body.split('\n')) {
          if (!line.contains("FUT_TOA3M_")) continue;
          const QList<QByteArray> f = line.split(',');
          if (f.size() < 6) continue;
          const QString name = QString::fromLatin1(f[1]);
          const QDate last = QDate::fromString("20" + name.section('_', 2, 2), "yyyyMMdd");
          const QString cm = QString::fromLatin1(f[3]);
          bool ok = false; const double px = f[5].toDouble(&ok);
          if (!ok || !last.isValid() || cm.size() != 6) continue;
          const QDate start = detail::immDate(cm.left(4).toInt(), cm.mid(4).toInt());
          contracts.append({name, RateContract{start, last.addDays(1), NAN}});
          market::Series s; s.bars.push_back({t, px, px, px, px, 0}); g.daily[name] = s;
        }
        if (contracts.isEmpty()) { bb->pricingNote = QCoreApplication::translate("economy", "No 3-month TONA futures in today's JPX file."); changed(); return; }
        detail::recordSnapshots("odds-boj.json", t, g);
        const QList<MeetingDate> meets = bb->upcoming(10);
        QVector<QDate> eff; for (const auto& md : meets) eff << md.decision.addDays(1);
        detail::computeFromQuotes(*bb, eff, meets, contracts, g, 1);
        bb->pricingSourceEn = QStringLiteral("computed by Deepwater from JPX/OSE 3-month TONA futures settlement prices and the BoJ's TONA");
        bb->pricingSource = QCoreApplication::translate("economy", "computed by Deepwater from JPX/OSE 3-month TONA futures settlement prices of %1 (jpx.co.jp) and the BoJ's TONA")
                              .arg(fileDate.toString("yyyy-MM-dd"));
        bb->pricingNote = bb->priced ? QString() : QCoreApplication::translate("economy", "3-month TONA futures did not cover the next meeting.");
        bb->pricingNote = detail::withSharedContractCaveat(bb->pricingNote, *bb, eff, contracts);
        bb->historySource = "daily settlements recorded by this app";
        changed();
      });
    });
  });
}

// ------------------------------------------------------------------------------------------------ SNB
void CentralBanks::loadSnbPricing() {
  struct Day { QDate date; QHash<QString, double> px; };   // contract start yyyyMMdd → settlement
  auto days = std::make_shared<QMap<QDate, QHash<QString, double>>>();
  auto compute = [this, days] {
    Bank* b = mut("SNB");
    if (days->isEmpty() || !std::isfinite(b->overnight) || !std::isfinite(b->rate)) return;
    QList<QPair<QString, RateContract>> contracts;
    detail::Gather g;
    QSet<QString> keys;
    for (const auto& d : *days) for (auto it = d.cbegin(); it != d.cend(); ++it) keys.insert(it.key());
    QList<QString> sorted(keys.begin(), keys.end()); std::sort(sorted.begin(), sorted.end());
    for (int i = 0; i < sorted.size(); ++i) {
      const QDate s = parseYmd(sorted[i]);
      const QDate e = i + 1 < sorted.size() ? parseYmd(sorted[i + 1]) : detail::immDate(s.addMonths(3).year(), s.addMonths(3).month());
      contracts.append({sorted[i], RateContract{s, e, NAN}});
    }
    for (auto it = days->cbegin(); it != days->cend(); ++it) {
      const qint64 t = QDateTime(it.key(), QTime(16, 15), QTimeZone::UTC).toSecsSinceEpoch();
      for (auto jt = it->cbegin(); jt != it->cend(); ++jt) g.daily[jt.key()].bars.push_back({t, jt.value(), jt.value(), jt.value(), jt.value(), 0});
    }
    const QList<MeetingDate> meets = b->upcoming(10);
    QVector<QDate> eff; for (const auto& md : meets) eff << md.decision.addDays(1);
    detail::computeFromQuotes(*b, eff, meets, contracts, g, 1);
    b->pricingSourceEn = QStringLiteral("computed by Deepwater from Eurex 3M SARON futures (FSR3) daily settlement prices and the SNB's SARON");
    b->pricingSource = QCoreApplication::translate("economy", "computed by Deepwater from Eurex 3M SARON futures (FSR3) daily settlement prices of %1 (eurex.com, delayed public data) and the SNB's SARON")
                         .arg(days->lastKey().toString("yyyy-MM-dd"));
    b->pricingNote = b->priced ? QCoreApplication::translate("economy", "Settlements come from bid/ask quotes on most days (few trades).")
                               : QCoreApplication::translate("economy", "3M SARON futures did not cover the next meeting.");
    b->pricingNote = detail::withSharedContractCaveat(b->pricingNote, *b, eff, contracts);
    b->historySource = "daily settlements";
    changed();
  };
  auto parse = [](const QByteArray& body, QDate* date, QStringList* tradingDates) {
    QHash<QString, double> px;
    const auto o = QJsonDocument::fromJson(body).object();
    for (const auto& r : o.value("dataRows").toArray()) {
      const auto ro = r.toObject();
      const double v = ro.value("dSettle").toDouble();
      if (v > 50 && ro.value("contractType").toString() == "M") px.insert(ro.value("date").toString(), v);
    }
    const auto td = o.value("header").toObject().value("tradingDates").toArray();
    for (const auto& d : td) if (tradingDates) *tradingDates << d.toString().left(10);
    if (date && !td.isEmpty()) *date = QDate::fromString(td[0].toString().left(10), "dd-MM-yyyy");
    return px;
  };
  net::Request r; r.ttlSec = 6 * 3600;
  r.url = QUrl("https://www.eurex.com/api/v1/overallstatistics/1410330?filtertype=overview");
  net::get(r, this, [this, days, compute, parse](const net::Reply& rep) {
    if (!rep.ok()) { mut("SNB")->pricingNote = QCoreApplication::translate("economy", "Eurex statistics did not answer (%1).").arg(rep.error); changed(); return; }
    QDate d; QStringList td;
    const auto px = parse(rep.body, &d, &td);
    if (px.isEmpty() || !d.isValid()) { mut("SNB")->pricingNote = QCoreApplication::translate("economy", "Eurex returned no 3M SARON settlements."); changed(); return; }
    (*days)[d] = px;
    compute();
    // back-fill the previous trading days (one request every 30 s, past days cached 30 days)
    for (int k = 1; k < std::min<qsizetype>(td.size(), 23); ++k) {
      const QDate pd = QDate::fromString(td[k], "dd-MM-yyyy");
      if (!pd.isValid()) continue;
      net::Request h; h.ttlSec = 30 * 24 * 3600;
      h.url = QUrl("https://www.eurex.com/api/v1/overallstatistics/1410330?busdate=" + pd.toString("yyyyMMdd") + "&filtertype=overview");
      net::get(h, this, [days, compute, parse, pd](const net::Reply& hr) {
        if (!hr.ok()) return;
        const auto p2 = parse(hr.body, nullptr, nullptr);
        if (!p2.isEmpty()) { (*days)[pd] = p2; compute(); }
      });
    }
  });
}

// ------------------------------------------------------------------------------------------------ RBA / RBNZ
namespace {
struct FjRow { QDate meeting; double implied = NAN, move = NAN, total = NAN; };
struct FjTable { QString bank; QDate date; QString url; double effective = NAN; QList<FjRow> rows; QString reason; };

QDate fjDate(const QString& token, const QDate& ref) {
  static const QStringList months{"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
  const QString t = token.toLower();
  auto m1 = QRegularExpression("(\\d{1,2})[\\s/-]*([a-z]{3})[a-z]*[\\s/-]*'?(\\d{2,4})?").match(t);
  auto m2 = QRegularExpression("([a-z]{3})[a-z]*[\\s/-]*(\\d{1,2})(?:[\\s,/-]+'?(\\d{2,4}))?").match(t);
  int d = 0, mo = 0, y = 0;
  if (m1.hasMatch() && months.contains(m1.captured(2))) { d = m1.captured(1).toInt(); mo = months.indexOf(m1.captured(2)) + 1; y = m1.captured(3).toInt(); }
  else if (m2.hasMatch() && months.contains(m2.captured(1))) { mo = months.indexOf(m2.captured(1)) + 1; d = m2.captured(2).toInt(); y = m2.captured(3).toInt(); }
  else return {};
  if (y > 0 && y < 100) y += 2000;
  if (y == 0) { y = ref.year(); if (QDate(y, mo, 1) < ref.addMonths(-1)) ++y; }
  return QDate(y, mo, d);
}

// Rows of OCR text → table, validated: totals = running sum of moves (±1.5 bp), implied - total/100 constant
// (the effective rate, ±0.03) and close to the policy rate (±0.35).
FjTable parseFj(const QList<ocr::Line>& lines, const QString& bank, const QDate& published, double policy) {
  FjTable t; t.bank = bank; t.date = published;
  QList<QList<ocr::Line>> rows;
  QList<ocr::Line> sorted = lines;
  std::sort(sorted.begin(), sorted.end(), [](const ocr::Line& a, const ocr::Line& b) { return a.box.center().y() < b.box.center().y(); });
  for (const auto& l : sorted) {
    if (!rows.isEmpty() && std::fabs(rows.last().first().box.center().y() - l.box.center().y()) < l.box.height() * 0.6) rows.last() << l;
    else rows << QList<ocr::Line>{l};
  }
  QRegularExpression num("([-+]?\\d+(?:\\.\\d+)?)");
  for (auto& r : rows) {
    std::sort(r.begin(), r.end(), [](const ocr::Line& a, const ocr::Line& b) { return a.box.left() < b.box.left(); });
    QString text; for (const auto& l : r) text += l.text + "  ";
    const QDate md = fjDate(r.first().text, published);
    if (!md.isValid()) continue;
    QVector<double> nums;
    QString rest = text.mid(r.first().text.size());
    for (auto it = num.globalMatch(rest); it.hasNext();) nums << it.next().captured(1).toDouble();
    if (nums.size() < 3) continue;
    FjRow fr; fr.meeting = md;
    // implied rate: the value closest to the policy rate with a decimal point
    int bi = -1; double bd = 1e9;
    for (int k = 0; k < nums.size(); ++k) if (std::fabs(nums[k] - policy) < bd && std::fabs(nums[k]) < 25) { bd = std::fabs(nums[k] - policy); bi = k; }
    if (bi < 0 || bd > 1.5) continue;
    fr.implied = nums[bi];
    QVector<double> others; for (int k = 0; k < nums.size(); ++k) if (k != bi) others << nums[k];
    fr.move = others.value(0, NAN); fr.total = others.value(1, NAN);
    t.rows << fr;
  }
  if (t.rows.size() < 2) { t.reason = QCoreApplication::translate("economy", "fewer than two meeting rows recognised"); t.rows.clear(); return t; }
  // validate (try both orders of move/total)
  for (int pass = 0; pass < 2; ++pass) {
    bool good = true; double run = 0, eff = NAN;
    for (auto& r : t.rows) {
      const double mv = pass == 0 ? r.move : r.total, tot = pass == 0 ? r.total : r.move;
      if (!std::isfinite(mv) || !std::isfinite(tot)) { good = false; break; }
      run += mv;
      if (std::fabs(run - tot) > 1.5) { good = false; break; }
      const double e = r.implied - tot / 100.0;
      if (!std::isfinite(eff)) eff = e; else if (std::fabs(e - eff) > 0.03) { good = false; break; }
    }
    if (good && std::fabs(eff - policy) <= 0.35) {
      if (pass == 1) for (auto& r : t.rows) std::swap(r.move, r.total);
      t.effective = eff;
      return t;
    }
  }
  t.reason = QCoreApplication::translate("economy", "the recognised table did not validate (moves do not add up to the totals or rates are inconsistent)");
  t.rows.clear();
  return t;
}
}

void CentralBanks::applyFjTable(const QString& id) {
  Bank* b = mut(id);
  QFile f(paths::data("macro") + "/fj-tables.json");
  if (!f.open(QIODevice::ReadOnly)) return;
  const auto o = QJsonDocument::fromJson(f.readAll()).object().value(id).toObject();
  const auto rows = o.value("rows").toArray();
  if (rows.isEmpty() || !std::isfinite(b->rate)) return;
  const double eff = o.value("effective").toDouble();
  QList<MeetingDate> meets; QVector<double> r;
  for (const auto& rv : rows) {
    const auto ro = rv.toObject();
    const QDate d = QDate::fromString(ro.value("meeting").toString(), "yyyy-MM-dd");
    if (d < QDate::currentDate()) continue;
    MeetingDate md; md.decision = d; md.first = d;
    for (const auto& known : b->meetings) if (known.decision == d) md = known;
    meets << md; r << eff + ro.value("total").toDouble() / 100.0;
  }
  if (meets.isEmpty()) { b->priced = false; b->pricingNote = QCoreApplication::translate("economy", "The last FinancialJuice table only covers past meetings."); return; }
  b->overnight = eff;
  applyRates(*b, meets, eff, r);
  b->impliedBpWeekAgo = NAN;
  b->history.clear();
  b->historySource = "one table per published picture";
  b->pricingSourceEn = QStringLiteral("FinancialJuice probability pictures, read by Deepwater (OCR, validated)");
  b->pricingSource = QCoreApplication::translate("economy", "source: FinancialJuice picture of %1, OCR by Deepwater (validated)").arg(o.value("published").toString());
  b->pricingNote.clear();
}

void CentralBanks::loadFjPricing() {
  for (const QString id : {"RBA", "RBNZ"}) {
    applyFjTable(id);
    Bank* b = mut(id);
    if (!b->priced)
      b->pricingNote = QCoreApplication::translate("economy", "Waiting for a FinancialJuice \"%1 Interest Rate Probabilities\" picture: none on the wire "
                       "in the last 7 days (ASX and the RBA/RBNZ sites forbid programs).").arg(b->code);
  }
  changed();
  const auto items = wire::Wire::instance().between(QDateTime::currentDateTimeUtc().addDays(-7), QDateTime::currentDateTimeUtc().addSecs(60));
  QRegularExpression rx("^(RBA|RBNZ)\\b.*Interest Rate Probabilit", QRegularExpression::CaseInsensitiveOption);
  for (const auto& it : items) {
    if (it.source != "FinancialJuice" || m_fjSeen.contains(it.id)) continue;
    const auto m = rx.match(it.text);
    if (!m.hasMatch() || it.url.isEmpty()) continue;
    m_fjSeen.insert(it.id);
    const QString id = m.captured(1).toUpper();
    const QDate published = it.time.date();
    net::Request a; a.ttlSec = 30 * 24 * 3600; a.url = QUrl(it.url);
    net::get(a, this, [this, id, published, url = it.url](const net::Reply& ar) {
      if (!ar.ok()) return;
      const QString html = QString::fromUtf8(ar.body);
      auto mi = QRegularExpression("property=\"og:image\"\\s+content=\"([^\"]+)\"").match(html);
      if (!mi.hasMatch()) mi = QRegularExpression("content=\"([^\"]+)\"\\s+property=\"og:image\"").match(html);
      if (!mi.hasMatch()) return;
      net::Request im; im.ttlSec = 30 * 24 * 3600; im.url = QUrl(mi.captured(1));
      net::get(im, this, [this, id, published, url](const net::Reply& ir) {
        if (!ir.ok()) return;
        const double policy = mut(id)->rate;
        runAsync<FjTable>(this, [img = ir.body, id, published, policy]() {
          return parseFj(ocr::recognize(img), id, published, policy);
        }, [this, id, url](FjTable t) {
          Bank* b = mut(id);
          if (t.rows.isEmpty()) {
            if (!b->priced) b->pricingNote = QCoreApplication::translate("economy", "Latest FinancialJuice picture (%1) rejected: %2.").arg(t.date.toString("yyyy-MM-dd"), t.reason);
            changed(); return;
          }
          const QString file = paths::data("macro") + "/fj-tables.json";
          bool readOk = true;
          QJsonObject all = detail::readJson(file, &readOk).object();
          if (!readOk) return;   // the file can't be read: never overwrite it
          const QString prev = all.value(id).toObject().value("published").toString();
          if (prev > t.date.toString("yyyy-MM-dd")) return;   // keep the newest valid table
          QJsonObject o; o.insert("published", t.date.toString("yyyy-MM-dd")); o.insert("url", url); o.insert("effective", t.effective);
          QJsonArray rows;
          for (const auto& r : t.rows) {
            QJsonObject ro; ro.insert("meeting", r.meeting.toString("yyyy-MM-dd")); ro.insert("implied", r.implied);
            ro.insert("move", r.move); ro.insert("total", r.total); rows.append(ro);
          }
          o.insert("rows", rows); all.insert(id, o);
          detail::writeJson(file, QJsonDocument(all));
          applyFjTable(id);
          changed();
        });
      });
    });
  }
}
}
