// Skill "read_data", the order flow of Data › Technical data (user's request 2026-11-20: "il faut" an agent reading the
// footprint of a bar and the live book the way a trader reads them on a chart, not only as ML columns):
//
//   get_footprint      per bar of 1m … 1D: volume, buyers / sellers, delta, CVD, POC, value area, diagonal imbalances
//                      and stacks; with ladder, one bar level by level (sold × bought, POC, value area, imbalances,
//                      stacked zones). From the stored history (Binance's daily archives, fp_days: up to yesterday)
//                      and, for the bars after it, the live footprint kept in memory (LiveBook: the trades of the
//                      pairs whose "live book" is on, the last 48 hours). Never a bar that is not whole.
//   get_order_book     the live book of the pair's spot market and USD-M perpetual (LiveBook, memory only): best
//                      bid / ask, depth bands ±0.2 … ±5 % (bookDepth's form), how they moved over 1 min … 1 h, the
//                      largest resting orders within ±0.5 % and how long they have stood; or, with end, the perpetual's
//                      stored bands at a past time (depth_days).
//   get_open_interest  the perpetual's open interest, long / short ratios and takers' ratio (metrics, 5-minute rows).
//
// Figures come from the stored or live data only, computed here (the same definitions as the Charts footprint and the
// ML columns, src/technicaldata/OrderFlowFeatures.h); what is not covered is said, never filled in.
#include "ai/Tools.h"
#include "charts/core/Footprint.h"
#include "charts/core/HeatmapStore.h"
#include "technicaldata/FootprintArchive.h"
#include "technicaldata/LiveBook.h"
#include "technicaldata/PerpArchive.h"
#include "technicaldata/TechnicalDataBars.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "technicaldata/TechnicalDataStore.h"
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <optional>

namespace ai {
namespace {
constexpr int kMaxChars = 6000;
using namespace technicaldata;

QString num(double v, int dec = 2) { return std::isfinite(v) ? QString::number(v, 'f', dec) : QStringLiteral("n/a"); }
QString signedNum(double v, int dec = 2) { return std::isfinite(v) ? (v > 0 ? "+" : "") + QString::number(v, 'f', dec) : QStringLiteral("n/a"); }
ToolResult okText(QString s) {
  if (s.size() > kMaxChars) s = s.left(kMaxChars) + "\n[cut: result longer than " + QString::number(kMaxChars) + " characters; ask for less]";
  return {true, s};
}
ToolResult fail(const QString& s) { return {false, s}; }
QString strArg(const QJsonObject& a, const char* k) { return a.value(k).isString() ? a.value(k).toString().trimmed() : QString(); }
int intArg(const QJsonObject& a, const char* k, int def, int lo, int hi) {
  const QJsonValue v = a.value(k);
  const int n = v.isDouble() ? v.toInt() : v.isString() && !v.toString().isEmpty() ? v.toString().toInt() : def;
  return qBound(lo, n, hi);
}
// decimals a price needs: two on BTC, more on a coin worth cents
int priceDec(double p) { return p > 0 ? std::clamp(6 - int(std::floor(std::log10(p))), 2, 8) : 2; }
qint64 floorTo(qint64 t, qint64 step) { return (t >= 0 ? t / step : -((-t + step - 1) / step)) * step; }
QString utc(qint64 t, const char* f = "yyyy-MM-dd HH:mm") { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).toString(f); }
QString ago(qint64 s) {
  if (s < 90) return QString::number(s) + " s";
  if (s < 5400) return QString::number(s / 60) + " min";
  return QString::number(s / 3600.0, 'f', 1) + " h";
}
// "2024-03-01", "2024-03-01 14:00" … → Unix s; nullopt when unreadable
std::optional<qint64> timeArg(const QString& v) {
  for (const char* f : {"yyyy-MM-dd HH:mm:ss", "yyyy-MM-ddTHH:mm:ss", "yyyy-MM-dd HH:mm", "yyyy-MM-ddTHH:mm", "yyyy-MM-dd"}) {
    const QString fs = QString::fromLatin1(f);
    if (v.size() < fs.size()) continue;
    QDateTime t = QDateTime::fromString(v.left(fs.size()), fs);
    if (!t.isValid()) continue;
    t.setTimeZone(QTimeZone::UTC);
    return t.toSecsSinceEpoch();
  }
  return std::nullopt;
}

// the pair asked for, among the recorded ones; empty + error text when not
QString pairArg(const QJsonObject& a, QString& error) {
  auto& st = Store::instance();
  QString q = strArg(a, "market").toUpper().remove('/').remove('-');
  if (q.isEmpty()) q = "BTCUSDT";
  if (q == "BTCUSD" || q == "BINANCE:BTCUSD") q = "BTCUSDT";
  const QString m = storedMarket(q);
  if (m.isEmpty() || !st.pairs().contains(m)) {
    error = "error: no recorded pair \"" + strArg(a, "market") + "\". Recorded: " + (st.pairs().isEmpty() ? QStringLiteral("none") : st.pairs().join(", "));
    return {};
  }
  if (!m.startsWith("BINANCE:")) { error = "error: " + m + " is forex (Dukascopy): no order flow, only Binance pairs have it"; return {}; }
  return m;
}
QString venueArg(const QJsonObject& a, const QString& def, bool allowBoth, QString& error) {
  QString v = strArg(a, "venue").toLower();
  if (v.isEmpty()) v = def;
  if (v == "perpetual" || v == "futures" || v == "usdm" || v == "usd-m") v = "perp";
  if (v == "spot" || v == "perp" || (allowBoth && v == "both")) return v;
  error = "error: venue must be spot or perp" + QString(allowBoth ? " or both" : "");
  return {};
}
QString venueName(const QString& v) { return v == "perp" ? QStringLiteral("USD-M perpetual") : QStringLiteral("spot"); }

struct Tf { const char* name; int seconds; };
constexpr Tf kTfs[] = {{"1m", 60}, {"5m", 300}, {"15m", 900}, {"30m", 1800}, {"1h", 3600}, {"4h", 14400}, {"1D", 86400}};
int tfArg(const QString& q) {
  for (const auto& t : kTfs) if (q == t.name) return t.seconds;
  for (const auto& t : kTfs) if (q.compare(t.name, Qt::CaseInsensitive) == 0 && q != "1M") return t.seconds;
  return 0;
}

// ------------------------------------------------------------------------------------------------ footprint
// The minutes of one pair's market: the stored days, then the live footprint for what they do not hold.
class Minutes {
public:
  Minutes(const QString& pair, const QString& venue) : m_pair(pair), m_venue(venue) {
    for (const FpDay& d : Store::instance().footprintDays(pair, venue)) m_days.insert(d.day);
    m_live = LiveBook::instance().footprint(pair, venue);
  }
  // Evaluation mode: the stored days begun by asOf only (the caller reads no minute at or after it), no live memory (it
  // is the present)
  void evaluation(qint64 asOf) {
    m_live = nullptr;
    for (auto it = m_days.begin(); it != m_days.end();) it = *it < asOf ? std::next(it) : m_days.erase(it);
  }
  bool hasStored() const { return !m_days.isEmpty(); }
  qint64 firstStored() const { return m_days.isEmpty() ? 0 : *std::min_element(m_days.cbegin(), m_days.cend()); }
  qint64 lastStored() const { return m_days.isEmpty() ? 0 : *std::max_element(m_days.cbegin(), m_days.cend()); }
  const LiveFootprint* live() const { return m_live; }
  qint64 liveFrom() const { return m_live && !m_live->covered.empty() ? m_live->covered.front().first : 0; }
  qint64 liveTo() const { return m_live && !m_live->covered.empty() ? m_live->covered.back().second : 0; }

  enum class From { None, Stored, Live };
  // The bar [from, to) (inside one UTC day) merged, or None when not whole.
  From bar(qint64 from, qint64 to, fp::Minute& out) {
    out = fp::Minute{};
    const qint64 day = floorTo(from, 86400);
    if (m_days.contains(day)) {
      const auto& ms = dayOf(day);
      auto it = std::lower_bound(ms.begin(), ms.end(), from, [](const fp::Minute& m, qint64 t) { return m.t < t; });
      for (; it != ms.end() && it->t < to; ++it) fp::merge(out, *it);
      return From::Stored;
    }
    if (m_live && m_live->covers(from, to)) {
      for (auto it = m_live->minutes.lower_bound(from); it != m_live->minutes.end() && it->first < to; ++it) fp::merge(out, it->second);
      return From::Live;
    }
    return From::None;
  }

private:
  const std::vector<fp::Minute>& dayOf(qint64 day) {
    auto it = m_cache.find(day);
    if (it == m_cache.end()) {
      if (m_cache.size() > 3) m_cache.clear();
      std::vector<fp::Minute> ms;
      fp::decodeDay(Store::instance().footprintData(m_pair, m_venue, day), day, ms);
      it = m_cache.insert(day, std::move(ms));
    }
    return *it;
  }
  QString m_pair, m_venue;
  QSet<qint64> m_days;
  QHash<qint64, std::vector<fp::Minute>> m_cache;
  const LiveFootprint* m_live = nullptr;
};

// one bar, level by level: rows of a few basis-point levels, top (highest price) first
QString ladderOf(const fp::Minute& bar, int maxRows) {
  const fp::Stats s = fp::statsOf(bar);
  const charts::flow::FootprintCandle c = fp::levelCandle(bar);
  const int64_t span = c.high - c.low + 1;
  const int64_t group = std::max<int64_t>(1, (span + maxRows - 1) / maxRows);
  charts::flow::FootprintParams p;
  p.groupTicks = group;
  charts::flow::FootprintView v;
  charts::flow::analyze(c, p, v);
  const int dec = priceDec(bar.close);
  const double bp = (std::pow(1.0001, double(group)) - 1) * 1e4;
  QString out = "ladder: " + QString::number(v.rows.size()) + " rows of " + QString::number(group) + " level" + (group > 1 ? "s" : "") + " (≈" + num(bp, 1) +
                " bp, " + num(bar.close * bp / 1e4, dec) + " each), highest price first. sold = aggressive sells hitting the bid, bought = "
                "aggressive buys lifting the ask (base asset). marks: POC (most volume), VA (inside the 70 % value area), "
                "BUY IMB (bought ≥ 3 × sold one row below), SELL IMB (sold ≥ 3 × bought one row above), STACK (3+ imbalances "
                "of one side in a row), O / C (open / close row)\n";
  out += "price from | price to | sold | bought | delta | volume | marks\n";
  QVector<int> inStack(int(v.rows.size()), 0);
  for (const auto& st : v.stacks)
    for (int r = st.fromRow; r <= st.toRow; ++r) inStack[r] = st.side == charts::flow::Side::Buy ? 1 : -1;
  const int32_t openLevel = fp::levelOf(bar.open), closeLevel = fp::levelOf(bar.close);
  for (int r = int(v.rows.size()) - 1; r >= 0; --r) {
    const auto& row = v.rows[size_t(r)];
    const double lo = fp::levelLow(int32_t(row.ticks)), hi = fp::levelLow(int32_t(row.ticks + group));
    QStringList marks;
    if (r == v.poc) marks << "POC";
    if (hi > s.vaLow && lo <= s.vaHigh) marks << "VA";
    if (row.flags & charts::flow::BuyImbalance) marks << "BUY IMB";
    if (row.flags & charts::flow::SellImbalance) marks << "SELL IMB";
    if (inStack[r]) marks << (inStack[r] > 0 ? "BUY STACK" : "SELL STACK");
    if (openLevel >= row.ticks && openLevel < row.ticks + group) marks << "O";
    if (closeLevel >= row.ticks && closeLevel < row.ticks + group) marks << "C";
    out += num(lo, dec) + " | " + num(hi, dec) + " | " + num(row.bid, 3) + " | " + num(row.ask, 3) + " | " + signedNum(row.delta(), 3) + " | " +
           num(row.volume(), 3) + " | " + marks.join(" ") + "\n";
  }
  return out;
}

void footprintTool(const QJsonObject& a, ToolDone done) {
  QString err;
  const QString pair = pairArg(a, err);
  if (pair.isEmpty()) { done(fail(err)); return; }
  const QString venue = venueArg(a, "spot", false, err);
  if (venue.isEmpty()) { done(fail(err)); return; }
  const QString tfName = strArg(a, "timeframe").isEmpty() ? QStringLiteral("15m") : strArg(a, "timeframe");
  const int tf = tfArg(tfName);
  if (!tf) { done(fail("error: timeframe must be 1m, 5m, 15m, 30m, 1h, 4h or 1D")); return; }
  const PairSpec spec = specFromJson(Store::instance().pairSpec(pair));
  Minutes mins(pair, venue);
  const qint64 asOf = asOfFor("get_footprint");   // evaluation mode: the bars closed by then, from the stored days
  if (asOf > 0) mins.evaluation(asOf);
  if (!mins.hasStored() && !mins.live()) {
    done(fail("error: no footprint of " + pair + " " + venueName(venue) + ": " +
              (spec.footprint.contains(venue) ? QStringLiteral("its history is being collected, no day stored yet") :
               QStringLiteral("not recorded (Data › Technical data, the pair's settings: Footprint)")) +
              (spec.liveBook ? QString() : QStringLiteral("; the live footprint needs the pair's \"Live book\" setting on"))));
    return;
  }
  const int want = intArg(a, "bars", 12, 1, 48);
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  // the newest bar: the one in progress when the live footprint reaches now, else the newest the sources hold; with
  // end, the one holding end (or older)
  const qint64 newest = std::max(mins.hasStored() ? mins.lastStored() + 86400 : 0, mins.liveTo());
  qint64 last = mins.liveTo() >= floorTo(now, 60) - 120 ? floorTo(now, tf) : floorTo(newest - 1, tf);
  if (asOf > 0) last = std::min(last, floorTo(asOf, tf) - tf);   // t + tf <= asOf: closed by then
  const QString endArg = strArg(a, "end");
  if (!endArg.isEmpty()) {
    const auto e = timeArg(endArg);
    if (!e) { done(fail("error: end must be a UTC date or time like 2026-11-18 or 2026-11-18 14:00")); return; }
    if (asOf > 0 && *e > asOf) { done(fail(afterClock(asOf, "end " + endArg))); return; }
    last = std::min(floorTo(*e, tf), last);
  }
  struct Row { qint64 t = 0; fp::Minute bar; Minutes::From from = Minutes::From::None; bool partial = false; qint64 partialTo = 0; };
  auto rowAt = [&](qint64 t) {
    Row r;
    r.t = t;
    r.from = mins.bar(t, t + tf, r.bar);
    if (r.from == Minutes::From::None && t + tf > now && mins.live()) {   // the bar in progress: its whole minutes so far
      const qint64 to = std::min(floorTo(now, 60), mins.liveTo());
      if (to > t && mins.live()->covers(t, to)) { r.from = mins.bar(t, to, r.bar); r.partial = true; r.partialTo = to; }
    }
    return r;
  };
  std::vector<Row> rows;
  for (qint64 t = last; int(rows.size()) < want && t > last - qint64(want + 60) * tf; t -= tf) {
    Row r = rowAt(t);
    if (r.from == Minutes::From::None && rows.empty()) continue;   // up to the newest covered bar
    rows.push_back(std::move(r));
  }
  std::reverse(rows.begin(), rows.end());
  while (!rows.empty() && rows.front().from == Minutes::From::None) rows.erase(rows.begin());
  QString out = pair + " " + venueName(venue) + " footprint, " + tfName + " bars (Binance aggregate trades on 1 basis-point price levels; "
                "computed by Deepwater), oldest first, times UTC (bar open), volumes in base asset.\n"
                "sources: stored history " + (mins.hasStored() ? utc(mins.firstStored(), "yyyy-MM-dd") + " to " +
                (asOf > 0 ? utc(std::min(mins.lastStored() + 86399, asOf - 1)) : utc(mins.lastStored() + 86399, "yyyy-MM-dd")) +
                " (Binance's daily archives, a day published the next day)" : QStringLiteral("none")) + "; live (memory) " +
                (asOf > 0 ? QStringLiteral("off in evaluation mode")
                 : mins.live() && mins.liveTo() ? utc(mins.liveFrom()) + " to " + utc(mins.liveTo()) : spec.liveBook ? QStringLiteral("starting (trades loading)") : QStringLiteral("off (the pair's \"Live book\" setting)")) + "\n";
  if (asOf > 0) out = "EVALUATION MODE: as of " + asOfStamp(asOf) + " (the bars closed by then).\n" + out;
  if (rows.empty()) { done(okText(out + "no bar covered up to " + utc(last) + " (outside both sources)\n")); return; }
  out += "time | open | high | low | close | volume | buy % | delta | CVD | POC | value area | imbalances buy/sell | stacks buy/sell | source\n";
  double cvd = 0;
  int uncovered = 0;
  for (const Row& r : rows) {
    if (r.from == Minutes::From::None) { out += utc(r.t) + " | not covered (no stored day, not in the live memory)\n"; ++uncovered; continue; }
    const fp::Minute& b = r.bar;
    if (b.trades == 0) { out += utc(r.t) + " | no trade\n"; continue; }
    const fp::Stats s = fp::statsOf(b);
    cvd += b.delta();
    const int d = priceDec(b.close);
    out += utc(r.t) + " | " + num(b.open, d) + " | " + num(b.high, d) + " | " + num(b.low, d) + " | " + num(b.close, d) + " | " + num(b.volume(), 3) + " | " +
           num(b.buy / b.volume() * 100, 1) + " | " + signedNum(b.delta(), 3) + " | " + signedNum(cvd, 3) + " | " + num(s.poc, d) + " | " + num(s.vaLow, d) + "–" +
           num(s.vaHigh, d) + " | " + QString::number(s.buyImbalances) + "/" + QString::number(s.sellImbalances) + " | " + QString::number(s.buyStacks) + "/" +
           QString::number(s.sellStacks) + " | " + (r.from == Minutes::From::Stored ? "stored" : "live") +
           (r.partial ? " (in progress, minutes up to " + utc(r.partialTo, "HH:mm") + ")" : QString()) + "\n";
  }
  out += "CVD: cumulative delta from the first bar shown. POC and value area: levels' middle prices. Imbalances: diagonal, ratio 3, on "
         "1 bp levels (as the Charts footprint and the ML columns).\n";
  if (uncovered) out += QString::number(uncovered) + " bar(s) not covered: left out, never estimated.\n";
  if (int(rows.size()) < want) {
    const qint64 storedTo = mins.hasStored() ? (asOf > 0 ? std::min(mins.lastStored() + 86400, asOf) : mins.lastStored() + 86400) : 0;
    out += "only " + QString::number(rows.size()) + " of the " + QString::number(want) + " bars asked: older ones are not covered without a hole";
    if (mins.liveFrom() > storedTo && rows.front().t <= mins.liveFrom() + tf)
      out += " (the stored history ends " + utc(storedTo) + ", Binance publishes a day's archive the next day; the live memory starts " +
             utc(mins.liveFrom()) + ", from the trades loaded since Deepwater started)";
    out += "; an older end reads the stored days.\n";
  }
  // ladder of one bar
  const QString lad = strArg(a, "ladder");
  if (!lad.isEmpty()) {
    const Row* pick = nullptr;
    Row extra;
    if (lad.compare("last", Qt::CaseInsensitive) == 0) {
      for (auto it = rows.rbegin(); it != rows.rend(); ++it) if (it->from != Minutes::From::None && it->bar.trades) { pick = &*it; break; }
    } else if (const auto t = timeArg(lad)) {
      const qint64 bt = floorTo(*t, tf);
      if (asOf > 0 && bt + tf > asOf) { done(fail(afterClock(asOf, "the ladder's bar " + lad + " closes"))); return; }
      for (const Row& r : rows) if (r.t == bt) pick = &r;
      if (!pick) {
        extra = rowAt(bt);
        if (extra.from != Minutes::From::None) pick = &extra;
      }
    } else {
      done(fail("error: ladder must be \"last\" or a bar's UTC open time like 2026-11-18 14:00"));
      return;
    }
    if (!pick || pick->bar.trades == 0) out += "\nladder: that bar is not covered\n";
    else out += "\n" + tfName + " bar " + utc(pick->t) + (pick->partial ? " (in progress)" : "") + " " + ladderOf(pick->bar, intArg(a, "rows", 30, 5, 60));
  }
  done(okText(out));
}

// ------------------------------------------------------------------------------------------------ order book
using Bands = std::array<float, perp::kBands>;   // −5 … −0.2, +0.2 … +5
struct BandsAt { qint64 t = 0; Bands depth{}; double mid = 0, bestBid = 0, bestAsk = 0; float coverBid = 0, coverAsk = 0; };

const charts::flow::BookBands* liveBandsAt(const std::deque<charts::flow::BookBands>& d, int64_t ms) {
  auto it = std::upper_bound(d.begin(), d.end(), ms, [](int64_t t, const charts::flow::BookBands& b) { return t < b.timeMs; });
  if (it == d.begin()) return nullptr;
  return &*std::prev(it);
}
double share(const Bands& b, int askBand) {   // bids' share out to ±p, %
  const double bid = b[size_t(11 - askBand)], ask = b[size_t(askBand)];
  return std::isfinite(bid) && std::isfinite(ask) && bid + ask > 0 ? bid / (bid + ask) * 100 : NAN;
}
QString bandsTable(const BandsAt& now, bool live) {
  QString out = "band | bids (base) | asks (base) | bids' share %" + QString(live ? " | note" : "") + "\n";
  const auto& pc = perp::bandPercents();
  for (int k = 6; k < 12; ++k) {
    const double p = pc[size_t(k)];
    QString note;
    if (live && ((now.coverBid > 0 && p > now.coverBid + 1e-9) || (now.coverAsk > 0 && p > now.coverAsk + 1e-9)))
      note = " | partial: beyond what the book snapshot reached (bids ±" + num(now.coverBid, 2) + " %, asks ±" + num(now.coverAsk, 2) + " %), may be short";
    else if (live) note = " | ";
    out += "±" + num(p, p < 1 ? 1 : 0) + " % | " + num(now.depth[size_t(11 - k)], 2) + " | " + num(now.depth[size_t(k)], 2) + " | " + num(share(now.depth, k), 1) + note + "\n";
  }
  return out;
}
QString changesTable(const BandsAt& now, const std::function<std::optional<BandsAt>(qint64)>& at) {
  QString out = "since | bids −1 % change | asks +1 % change | bids' share ±1 % then → now | mid change\n";
  for (int s : {60, 300, 900, 3600}) {
    const auto then = at(now.t - s);
    if (!then || now.t - s - then->t > std::max(60, s / 5)) { out += ago(s) + " | not covered\n"; continue; }
    auto chg = [](double a, double b) { return std::isfinite(a) && std::isfinite(b) && b > 0 ? signedNum((a / b - 1) * 100, 1) + " %" : QStringLiteral("n/a"); };
    out += ago(s) + " | " + chg(now.depth[4], then->depth[4]) + " | " + chg(now.depth[7], then->depth[7]) + " | " + num(share(then->depth, 7), 1) + " → " +
           num(share(now.depth, 7), 1) + " | " + (then->mid > 0 && now.mid > 0 ? signedNum((now.mid / then->mid - 1) * 100, 3) + " %" : QStringLiteral("n/a")) + "\n";
  }
  return out;
}

// The largest resting orders near the price in the newest heatmap column, and how long each has stood.
QString wallsOf(const charts::data::DepthStream& s) {
  const auto& live = s.live();
  if (live.empty()) return "walls: no book column yet\n";
  const charts::flow::DepthRecord& c = live.back();
  if ((c.flags & charts::flow::DepthColumn::Missing) || !c.bestBid || !c.bestAsk) return "walls: the book is not synced now\n";
  const auto& sc = s.ticks();
  struct W { int64_t tick; double qty; bool bid; };
  std::vector<W> bids, asks;
  for (int i = 0; i < charts::flow::DepthRecord::kRows; ++i) {
    const double q = charts::flow::decodeQty(c.codes[i]);
    if (!(q > 0)) continue;
    const int64_t t = c.baseTicks + int64_t(i) * c.rowTicks;
    if (t + c.rowTicks - 1 <= c.bestBid) bids.push_back({t, q, true});
    else if (t >= c.bestAsk) asks.push_back({t, q, false});
  }
  auto top = [](std::vector<W>& v) {
    std::sort(v.begin(), v.end(), [](const W& a, const W& b) { return a.qty > b.qty; });
    if (v.size() > 5) v.resize(5);
  };
  top(bids);
  top(asks);
  const double mid = (sc.priceOf(c.bestBid) + sc.priceOf(c.bestAsk)) / 2;
  const int dec = priceDec(mid);
  // how long it stood: the share of the last 10 minutes' columns (one in 10 = a second apart) where that row held at least half of it
  auto held = [&](const W& w) {
    int n = 0, yes = 0;
    for (int64_t k = live.end() - 1; k >= live.begin(); k -= 10) {
      const auto& r = live.at(k);
      if (r.flags & charts::flow::DepthColumn::Missing) continue;
      ++n;
      const int64_t row = r.rowTicks > 0 ? (w.tick - r.baseTicks) / r.rowTicks : -1;
      if (row >= 0 && row < charts::flow::DepthRecord::kRows && r.rowTicks == c.rowTicks && charts::flow::decodeQty(r.codes[row]) >= w.qty / 2) ++yes;
    }
    return n ? 100.0 * yes / n : NAN;
  };
  const qint64 span = (c.timeMs - live.at(live.begin()).timeMs) / 1000;
  QString out = "largest resting orders within the heatmap's window (±0.5 % around the price), rows of " + num(sc.priceOf(c.rowTicks), dec) +
                ", in base asset; stood = share of the last " + ago(span) + " where that row held at least half of it\n";
  out += "side | price | distance % | quantity | stood %\n";
  for (const auto* v : {&asks, &bids})
    for (const W& w : *v) {
      const double p = sc.priceOf(w.tick);
      out += QString(w.bid ? "bid" : "ask") + " | " + num(p, dec) + " | " + signedNum((p / mid - 1) * 100, 3) + " | " + num(w.qty, 3) + " | " + num(held(w), 0) + "\n";
    }
  return out;
}

void orderBookTool(const QJsonObject& a, ToolDone done) {
  QString err;
  const QString pair = pairArg(a, err);
  if (pair.isEmpty()) { done(fail(err)); return; }
  const QString venueA = venueArg(a, "both", true, err);
  if (venueA.isEmpty()) { done(fail(err)); return; }
  const PairSpec spec = specFromJson(Store::instance().pairSpec(pair));
  QString endArg = strArg(a, "end");
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  // evaluation mode: the stored snapshot at that moment (or an earlier end); the live book is the present
  const qint64 asOf = asOfFor("get_order_book");
  if (asOf > 0 && endArg.isEmpty()) endArg = utc(asOf, "yyyy-MM-dd HH:mm:ss");
  QString out;
  if (!endArg.isEmpty()) {
    // a past time: the live bands while they reach back, else the perpetual's stored snapshots
    const auto e = timeArg(endArg);
    if (!e) { done(fail("error: end must be a UTC date or time like 2026-11-18 14:00")); return; }
    if (asOf > 0 && *e > asOf) { done(fail(afterClock(asOf, "end " + endArg))); return; }
    QHash<qint64, std::vector<perp::DepthSample>> days;
    QSet<qint64> stored;
    for (const ArchDay& d : Store::instance().archiveDays(pair, "depth")) stored.insert(d.day);
    auto at = [&](qint64 t) -> std::optional<BandsAt> {
      for (qint64 day : {floorTo(t, 86400), floorTo(t, 86400) - 86400}) {
        if (!stored.contains(day)) continue;
        auto it = days.find(day);
        if (it == days.end()) {
          std::vector<perp::DepthSample> v;
          perp::decodeDepth(Store::instance().depthData(pair, day), day, v);
          it = days.insert(day, std::move(v));
        }
        auto p = std::upper_bound(it->begin(), it->end(), t, [](qint64 x, const perp::DepthSample& s) { return x < s.t; });
        if (p != it->begin()) { --p; BandsAt b; b.t = p->t; b.depth = p->depth; return b; }
      }
      return std::nullopt;
    };
    const auto b = at(*e);
    if (!b || *e - b->t > 300) {
      done(fail("error: no stored book snapshot of the " + pair + " perpetual within 5 minutes before " + utc(*e) +
                (spec.depth ? QString(" (stored days: Data › Technical data, Order flow › Depth)") : QString(" (book depth is not recorded for this pair)"))));
      return;
    }
    out = pair + " USD-M perpetual order book at " + utc(b->t, "yyyy-MM-dd HH:mm:ss") + " UTC (Binance's bookDepth archive, a snapshot about every 30 s; "
          "cumulative depth from the mid price; the spot book is never stored)\n" + bandsTable(*b, false);
    if (asOf > 0) out = "EVALUATION MODE: as of " + asOfStamp(asOf) + " (the live book is off; the perpetual's stored snapshots).\n" + out;
    QString ch = "since | bids −1 % change | asks +1 % change | bids' share ±1 % then → now\n";
    for (int s : {300, 900, 3600}) {
      const auto then = at(b->t - s);
      if (!then || b->t - s - then->t > 300) { ch += ago(s) + " | not covered\n"; continue; }
      auto chg = [](double x, double y) { return std::isfinite(x) && std::isfinite(y) && y > 0 ? signedNum((x / y - 1) * 100, 1) + " %" : QStringLiteral("n/a"); };
      ch += ago(s) + " | " + chg(b->depth[4], then->depth[4]) + " | " + chg(b->depth[7], then->depth[7]) + " | " + num(share(then->depth, 7), 1) + " → " + num(share(b->depth, 7), 1) + "\n";
    }
    done(okText(out + ch));
    return;
  }
  auto& lb = LiveBook::instance();
  if (!spec.liveBook || !lb.started()) {
    done(fail("error: the live book of " + pair + " is not kept: " +
              (spec.liveBook ? QStringLiteral("the collection has not started yet (it starts 20 s after launch)") :
               QStringLiteral("turn on \"Live book (memory)\" in the pair's settings, Data › Technical data")) +
              ". A past time (end) reads the perpetual's stored snapshots."));
    return;
  }
  out = pair + " live order book (Binance, kept in memory by Deepwater, never stored), now " + utc(now, "yyyy-MM-dd HH:mm:ss") + " UTC; depth "
        "cumulative from the mid price, base asset\n";
  for (const QString& v : venueA == "both" ? QStringList{"spot", "perp"} : QStringList{venueA}) {
    out += "\n== " + venueName(v) + "\n";
    const auto s = lb.stream(pair, v);
    if (!s) { out += "not kept\n"; continue; }
    if (!s->ready() || !s->status().synced || s->bands().empty()) {
      out += "not synced yet" + (s->status().error.isEmpty() ? QString() : " (" + s->status().error + ")") + "\n";
      continue;
    }
    const auto& d = s->bands();
    const auto& nb = d.back();
    BandsAt b;
    b.t = nb.timeMs / 1000; b.depth = nb.depth; b.mid = nb.mid; b.bestBid = nb.bestBid; b.bestAsk = nb.bestAsk; b.coverBid = nb.coverBid; b.coverAsk = nb.coverAsk;
    const int dec = priceDec(b.mid);
    out += "best bid " + num(b.bestBid, dec) + " | best ask " + num(b.bestAsk, dec) + " | spread " + num(b.bestAsk - b.bestBid, dec) + " (" + num((b.bestAsk - b.bestBid) / b.mid * 1e4, 3) +
           " bp) | as of " + ago(std::max<qint64>(0, now - b.t)) + " ago | memory holds " + ago((d.back().timeMs - d.front().timeMs) / 1000) + " of bands\n";
    out += bandsTable(b, true);
    out += changesTable(b, [&](qint64 t) -> std::optional<BandsAt> {
      const auto* x = liveBandsAt(d, t * 1000);
      if (!x || !x->ok()) return std::nullopt;
      BandsAt r; r.t = x->timeMs / 1000; r.depth = x->depth; r.mid = x->mid;
      return r;
    });
    out += wallsOf(*s);
  }
  done(okText(out));
}

// ------------------------------------------------------------------------------------------------ open interest
void openInterestTool(const QJsonObject& a, ToolDone done) {
  QString err;
  const QString pair = pairArg(a, err);
  if (pair.isEmpty()) { done(fail(err)); return; }
  auto& st = Store::instance();
  const QVector<ArchDay> days = st.archiveDays(pair, "oi");
  if (days.isEmpty()) {
    done(fail("error: no open interest of " + pair + " stored" + (specFromJson(st.pairSpec(pair)).openInterest ? QString(" yet (being collected)") :
                                                                  QString(" (not recorded: the pair's settings, Open interest)"))));
    return;
  }
  const QString stepName = strArg(a, "step").isEmpty() ? QStringLiteral("1h") : strArg(a, "step");
  const int step = tfArg(stepName);
  if (!step) { done(fail("error: step must be 5m, 15m, 30m, 1h, 4h or 1D")); return; }
  if (step < 300) { done(fail("error: open interest rows are 5 minutes apart: step 5m or more")); return; }
  const int want = intArg(a, "rows", 24, 1, 96);
  const qint64 asOf = asOfFor("get_open_interest");   // evaluation mode: the rows up to that moment
  qint64 end = days.last().day + 86400;
  if (asOf > 0) end = std::min(end, asOf);   // rows before the moment (a row at it is the next day's archive's first)
  if (const QString e = strArg(a, "end"); !e.isEmpty()) {
    const auto t = timeArg(e);
    if (!t) { done(fail("error: end must be a UTC date or time like 2026-11-18 14:00")); return; }
    if (asOf > 0 && *t > asOf) { done(fail(afterClock(asOf, "end " + e))); return; }
    end = std::min(end, asOf > 0 ? std::min(*t + 1, asOf) : *t + 1);
  }
  const qint64 from = end - qint64(want + 1) * step - 600;
  const std::vector<perp::OiRow> rows = st.openInterest(pair, from, end);
  if (rows.empty()) { done(fail("error: no open interest row of " + pair + " before " + utc(end))); return; }
  // one row per step: the last row at or before each step's end
  std::vector<const perp::OiRow*> pick;
  for (qint64 t = floorTo(rows.back().t, step); int(pick.size()) < want + 1 && t >= rows.front().t; t -= step) {
    auto it = std::upper_bound(rows.begin(), rows.end(), t, [](qint64 x, const perp::OiRow& r) { return x < r.t; });
    if (it == rows.begin()) break;
    const perp::OiRow* r = &*std::prev(it);
    if (t - r->t > 600) continue;
    pick.push_back(r);
  }
  std::reverse(pick.begin(), pick.end());
  qint64 storedTo = days.last().day;   // evaluation mode: the last day begun before the moment
  if (asOf > 0) {
    storedTo = days.first().day;
    for (const ArchDay& d : days) if (d.day < asOf) storedTo = d.day;
  }
  QString out = QString(asOf > 0 ? "EVALUATION MODE: as of " + asOfStamp(asOf) + ".\n" : QString()) +
                pair + " USD-M perpetual positioning (Binance's metrics archive, 5-minute rows, a day published the next day; stored " +
                utc(days.first().day, "yyyy-MM-dd") + " to " + utc(storedTo, "yyyy-MM-dd") + "), one row per " + stepName + ", oldest first, UTC\n"
                "time | open interest (base) | value (USD) | change % | top traders long/short (positions) | top traders long/short (accounts) | "
                "all accounts long/short | takers buy/sell volume\n";
  for (size_t i = pick.empty() ? 0 : 1; i < pick.size(); ++i) {
    const auto& r = *pick[i];
    const auto& p = *pick[i - 1];
    out += utc(r.t) + " | " + num(r.oi, 1) + " | " + num(r.oiValue / 1e6, 1) + " M | " + (p.oi > 0 ? signedNum((r.oi / p.oi - 1) * 100, 2) : QStringLiteral("n/a")) + " | " +
           num(r.topPositions, 3) + " | " + num(r.topAccounts, 3) + " | " + num(r.accounts, 3) + " | " + num(r.takerRatio, 3) + "\n";
  }
  out += "A ratio above 1: more longs (or more taker buys). n/a: Binance left it empty that day.\n";
  done(okText(out));
}

QJsonObject schema(std::initializer_list<std::pair<const char*, QJsonObject>> props) {
  QJsonObject p;
  for (const auto& [k, v] : props) p[k] = v;
  return QJsonObject{{"type", "object"}, {"properties", p}};
}
QJsonObject str(const char* d) { return {{"type", "string"}, {"description", d}}; }
QJsonObject integer(const char* d) { return {{"type", "integer"}, {"description", d}}; }
}  // namespace

void registerOrderFlowTools(Tools& t) {
  const QString S = "read_data";
  t.add({"get_footprint", S, "Data › Technical data: the footprint of a Binance pair's bars (spot or USD-M perpetual), 1m to 1D: volume, "
                             "aggressive buyers / sellers, delta, CVD, point of control, value area, diagonal imbalances and stacks; with "
                             "ladder, one bar level by level (sold × bought per price, POC, imbalances, stacked zones), as a trader reads a "
                             "footprint chart. Stored history up to yesterday, then the live trades kept in memory (the last 48 h).",
         schema({{"market", str("a recorded Binance pair, e.g. BINANCE:BTCUSDT (default), ETHUSDT")},
                 {"venue", str("spot (default) or perp (the USD-M perpetual)")},
                 {"timeframe", str("1m, 5m, 15m (default), 30m, 1h, 4h or 1D")},
                 {"bars", integer("how many bars, default 12, max 48")},
                 {"end", str("optional: the newest bar is the one holding this UTC time, e.g. 2026-11-18 14:00; empty = now (the bar in progress included when live)")},
                 {"ladder", str("optional: \"last\" or a bar's UTC open time: that bar level by level")},
                 {"rows", integer("ladder rows, default 30, max 60 (levels grouped to fit)")}}),
         footprintTool});
  t.add({"get_order_book", S, "Data › Technical data: the live order book of a Binance pair (spot and USD-M perpetual, kept in memory): best "
                              "bid / ask, spread, depth out to ±0.2 … ±5 %, how bids and asks moved over 1 min … 1 h, the largest resting "
                              "orders near the price and how long they stood. With end: the perpetual's stored book at a past time.",
         schema({{"market", str("a recorded Binance pair, e.g. BINANCE:BTCUSDT (default)")},
                 {"venue", str("spot, perp or both (default)")},
                 {"end", str("optional: a past UTC time, e.g. 2026-11-18 14:00 (the perpetual's stored snapshots); empty = live now")}}),
         orderBookTool});
  t.add({"get_open_interest", S, "Data › Technical data: a Binance pair's USD-M perpetual open interest and its change, top traders' and all "
                                 "accounts' long/short ratios, takers' buy/sell ratio (Binance's 5-minute metrics, stored up to yesterday).",
         schema({{"market", str("a recorded Binance pair, e.g. BINANCE:BTCUSDT (default)")},
                 {"step", str("5m, 15m, 30m, 1h (default), 4h or 1D between rows")},
                 {"rows", integer("how many rows, default 24, max 96")},
                 {"end", str("optional: last row at or before this UTC time; empty = the newest stored")}}),
         openInterestTool});
}

}  // namespace ai
