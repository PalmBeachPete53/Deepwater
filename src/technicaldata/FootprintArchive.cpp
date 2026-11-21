#include "technicaldata/FootprintArchive.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QTimeZone>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <miniz.h>

namespace technicaldata::fp {

using charts::flow::Decimal;
using charts::flow::parseDecimal;

QString venueId(Venue v) { return v == Venue::Spot ? QStringLiteral("spot") : QStringLiteral("perp"); }
std::optional<Venue> venueFromId(const QString& id) {
  if (id == "spot") return Venue::Spot;
  if (id == "perp") return Venue::Perp;
  return std::nullopt;
}
const std::vector<Venue>& venues() {
  static const std::vector<Venue> v{Venue::Spot, Venue::Perp};
  return v;
}

namespace {
QString archivePath(Venue v, const QString& symbol, const QDate& day) {
  return QString("https://data.binance.vision/data/%1/daily/aggTrades/%2/%2-aggTrades-%3.zip")
      .arg(v == Venue::Spot ? "spot" : "futures/um", symbol, day.toString("yyyy-MM-dd"));
}
}  // namespace

QUrl dailyUrl(Venue v, const QString& symbol, const QDate& day) { return QUrl(archivePath(v, symbol, day)); }
QUrl checksumUrl(Venue v, const QString& symbol, const QDate& day) { return QUrl(archivePath(v, symbol, day) + ".CHECKSUM"); }

QDate firstDay(Venue v, const QString& symbol) {
  // read from the archive listings (2026-11-20); other symbols start later, and a missing day is skipped anyway
  Q_UNUSED(symbol);
  return v == Venue::Spot ? QDate(2017, 8, 17) : QDate(2019, 12, 31);
}

// ------------------------------------------------------------------------------------------------ grid
namespace {
const double kLnG = std::log(kGrowth);
}
int32_t levelOf(double price) {
  if (!(price > 0)) return 0;
  return int32_t(std::floor(std::log(price) / kLnG));
}
double levelLow(int32_t k) { return std::exp(double(k) * kLnG); }
double levelMid(int32_t k) { return std::exp((double(k) + 0.5) * kLnG); }

void merge(Minute& a, const Minute& b) {
  if (b.trades == 0 && b.levels.empty()) return;
  if (a.trades == 0 && a.levels.empty()) { a = b; return; }
  const bool bLater = b.t >= a.t;
  if (bLater) { a.close = b.close; a.lastId = std::max(a.lastId, b.lastId); }
  else { a.open = b.open; a.firstId = std::min(a.firstId, b.firstId); a.t = b.t; }
  a.high = std::max(a.high, b.high);
  a.low = std::min(a.low, b.low);
  a.buy += b.buy;
  a.sell += b.sell;
  a.trades += b.trades;
  if (b.levels.empty()) return;
  const qint32 lo = a.levels.empty() ? b.base : std::min(a.base, b.base);
  const qint32 hi = a.levels.empty() ? b.base + qint32(b.levels.size()) - 1
                                     : std::max(a.base + qint32(a.levels.size()) - 1, b.base + qint32(b.levels.size()) - 1);
  if (a.levels.empty() || lo != a.base || hi != a.base + qint32(a.levels.size()) - 1) {
    std::vector<Level> grown(size_t(hi - lo + 1));
    for (size_t i = 0; i < a.levels.size(); ++i) grown[size_t(a.base - lo) + i] = a.levels[i];
    a.levels = std::move(grown);
    a.base = lo;
  }
  for (size_t i = 0; i < b.levels.size(); ++i) {
    Level& l = a.levels[size_t(b.base - a.base) + i];
    l.bid += b.levels[i].bid;
    l.ask += b.levels[i].ask;
  }
}

// ------------------------------------------------------------------------------------------------ CSV
void CsvReader::feed(const char* p, size_t n) {
  const char* end = p + n;
  while (p < end) {
    const char* nl = static_cast<const char*>(std::memchr(p, '\n', size_t(end - p)));
    if (!nl) { carry_.append(p, size_t(end - p)); return; }
    if (!carry_.empty()) {
      carry_.append(p, size_t(nl - p));
      line(carry_.data(), carry_.data() + carry_.size());
      carry_.clear();
    } else {
      line(p, nl);
    }
    p = nl + 1;
  }
}

void CsvReader::finish() {
  if (!carry_.empty()) line(carry_.data(), carry_.data() + carry_.size());
  carry_.clear();
}

void CsvReader::line(const char* b, const char* e) {
  if (e > b && e[-1] == '\r') --e;
  if (b == e) return;
  if (*b < '0' || *b > '9') return;   // the header line (USD-M), or anything that isn't a trade
  ++lines_;
  // id,price,qty,first,last,time,isBuyerMaker[,isBestMatch]
  const char* f[8];
  const char* fe[8];
  int nf = 0;
  const char* s = b;
  for (const char* q = b; q <= e && nf < 8; ++q)
    if (q == e || *q == ',') { f[nf] = s; fe[nf] = q; ++nf; s = q + 1; }
  if (nf < 7) { ++bad_; return; }
  const char* stop = nullptr;
  auto whole = [&](int i, qint64& v) {
    const Decimal d = parseDecimal(f[i], fe[i], &stop);
    if (!d.ok || d.exp != 0 || stop != fe[i]) return false;
    v = d.mant;
    return true;
  };
  auto real = [&](int i, double& v) {
    const Decimal d = parseDecimal(f[i], fe[i], &stop);
    if (!d.ok || stop != fe[i]) return false;
    v = d.toDouble();
    return true;
  };
  AggTrade t;
  qint64 time = 0;
  if (!whole(0, t.id) || !real(1, t.price) || !real(2, t.qty) || !whole(5, time) || !(t.price > 0) || t.qty < 0) { ++bad_; return; }
  // microseconds (spot since 2025) or milliseconds
  t.timeMs = time > 100000000000000LL ? time / 1000 : time;
  const qsizetype len = fe[6] - f[6];
  const char c = len ? (f[6][0] | 0x20) : 0;   // lower case
  if (len == 4 && c == 't') t.buyerMaker = true;
  else if (len == 5 && c == 'f') t.buyerMaker = false;
  else { ++bad_; return; }
  sink_(t);
}

// ------------------------------------------------------------------------------------------------ zip
bool inflateZip(QByteArrayView zip, const std::function<void(const char*, size_t)>& out, QString* error) {
  auto fail = [&](const QString& why) { if (error) *error = why; return false; };
  const auto* p = reinterpret_cast<const uchar*>(zip.data());
  const size_t n = size_t(zip.size());
  if (n < 30 || qFromLittleEndian<quint32>(p) != 0x04034b50u) return fail(QStringLiteral("not a zip file"));
  const quint16 flags = qFromLittleEndian<quint16>(p + 6), method = qFromLittleEndian<quint16>(p + 8);
  quint32 crc = qFromLittleEndian<quint32>(p + 14);
  const quint32 csize = qFromLittleEndian<quint32>(p + 18);
  const size_t nameLen = qFromLittleEndian<quint16>(p + 26), extraLen = qFromLittleEndian<quint16>(p + 28);
  const size_t start = 30 + nameLen + extraLen;
  if (start > n) return fail(QStringLiteral("truncated zip"));
  mz_ulong running = MZ_CRC32_INIT;
  size_t used = 0;   // compressed bytes read
  if (method == 0) {
    if (flags & 8) return fail(QStringLiteral("stored entry of unknown size"));
    if (start + csize > n) return fail(QStringLiteral("truncated zip"));
    running = mz_crc32(running, p + start, csize);
    out(reinterpret_cast<const char*>(p + start), csize);
    used = csize;
  } else if (method == 8) {
    tinfl_decompressor d;
    tinfl_init(&d);
    std::vector<mz_uint8> dict(TINFL_LZ_DICT_SIZE);
    size_t inPos = start, outPos = 0;
    for (;;) {
      size_t inBytes = n - inPos, outBytes = dict.size() - outPos;
      const tinfl_status st = tinfl_decompress(&d, p + inPos, &inBytes, dict.data(), dict.data() + outPos, &outBytes, 0);
      inPos += inBytes;
      if (outBytes) {
        running = mz_crc32(running, dict.data() + outPos, outBytes);
        out(reinterpret_cast<const char*>(dict.data() + outPos), outBytes);
      }
      outPos = (outPos + outBytes) & (dict.size() - 1);
      if (st == TINFL_STATUS_DONE) break;
      if (st < 0) return fail(QStringLiteral("corrupt zip data (inflate %1)").arg(int(st)));
      if (st == TINFL_STATUS_NEEDS_MORE_INPUT) return fail(QStringLiteral("truncated zip"));
    }
    used = inPos - start;
  } else {
    return fail(QStringLiteral("zip method %1 not supported").arg(method));
  }
  if (flags & 8) {   // the CRC comes after the data, in a descriptor (its signature optional)
    size_t q = start + used;
    if (q + 4 <= n && qFromLittleEndian<quint32>(p + q) == 0x08074b50u) q += 4;
    if (q + 4 > n) return fail(QStringLiteral("truncated zip"));
    crc = qFromLittleEndian<quint32>(p + q);
  }
  if (quint32(running) != crc) return fail(QStringLiteral("zip CRC mismatch"));
  return true;
}

// ------------------------------------------------------------------------------------------------ minutes
void DayBuilder::flushLevels(Minute& m) {
  if (cur_.empty()) return;
  qint32 lo = cur_.front().k, hi = lo;
  for (const Acc& a : cur_) { lo = std::min(lo, a.k); hi = std::max(hi, a.k); }
  m.base = lo;
  m.levels.assign(size_t(hi - lo + 1), Level{});
  for (const Acc& a : cur_) m.levels[size_t(a.k - lo)] = Level{float(a.bid), float(a.ask)};
  cur_.clear();
}

void DayBuilder::add(const AggTrade& t) {
  const qint64 sec = t.timeMs >= 0 ? t.timeMs / 1000 : -((-t.timeMs + 999) / 1000);
  if (sec < day_ || sec >= day_ + 86400) { ++outside_; return; }
  const qint64 minute = sec - ((sec - day_) % 60);
  if (lastId_ >= 0 && t.id > lastId_ + 1) idGaps_ += t.id - lastId_ - 1;
  lastId_ = std::max(lastId_, t.id);
  ++trades_;
  if (minutes_.empty() || minutes_.back().t != minute) {
    if (!minutes_.empty()) {
      if (minute < minutes_.back().t) { ++outside_; return; }   // out of order in the file: never seen
      flushLevels(minutes_.back());
    }
    Minute m;
    m.t = minute;
    m.open = m.high = m.low = m.close = t.price;
    m.firstId = m.lastId = t.id;
    minutes_.push_back(std::move(m));
  }
  Minute& m = minutes_.back();
  m.high = std::max(m.high, t.price);
  m.low = std::min(m.low, t.price);
  m.close = t.price;
  m.lastId = t.id;
  ++m.trades;
  if (t.buyerMaker) m.sell += t.qty; else m.buy += t.qty;
  // the levels of a minute: a few dozen at most; adding to the last one used first (trades come in runs at one price)
  const int32_t k = levelOf(t.price);
  auto it = std::find_if(cur_.rbegin(), cur_.rend(), [k](const Acc& x) { return x.k == k; });
  if (it == cur_.rend()) { cur_.push_back({k, 0, 0}); it = cur_.rbegin(); }
  if (t.buyerMaker) it->bid += t.qty; else it->ask += t.qty;
}

std::vector<Minute> DayBuilder::finish() {
  if (!minutes_.empty()) flushLevels(minutes_.back());
  return std::move(minutes_);
}

DayResult buildDay(QByteArrayView zip, const QDate& day) {
  DayResult r;
  const qint64 dayStart = QDateTime(day, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  DayBuilder b(dayStart);
  CsvReader csv([&b](const AggTrade& t) { b.add(t); });
  QString err;
  if (!inflateZip(zip, [&csv](const char* p, size_t n) { csv.feed(p, n); }, &err)) { r.error = err; return r; }
  csv.finish();
  r.minutes = b.finish();
  r.trades = b.trades();
  r.bad = csv.bad();
  r.outside = b.outside();
  r.idGaps = b.idGaps();
  if (r.trades == 0) r.error = QStringLiteral("no trades in the file");
  return r;
}

QByteArray checksumOf(QByteArrayView file) {
  const QByteArray s = file.toByteArray().trimmed();
  const qsizetype sp = s.indexOf(' ');
  const QByteArray hex = (sp < 0 ? s : s.left(sp)).toLower();
  if (hex.size() != 64) return {};
  for (char c : hex)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return {};
  return hex;
}

// ------------------------------------------------------------------------------------------------ storage
// v1, little-endian: "FPD1", u32 n, then each column for all minutes: u16 minute of day, f64 open, high, low, close,
// buy, sell, u32 trades, i64 first id, u32 last − first, i32 base, u16 levels; then per minute its bids then its asks
// (f32). Columns compress far better than rows (zlib through qCompress).
namespace {
template <class T> void put(QByteArray& b, T v) {
  char raw[sizeof(T)];
  qToLittleEndian(v, raw);
  b.append(raw, sizeof(T));
}
struct Reader {
  const char* p;
  const char* end;
  bool ok = true;
  template <class T> T get() {
    if (end - p < qsizetype(sizeof(T))) { ok = false; p = end; return T{}; }
    const T v = qFromLittleEndian<T>(p);
    p += sizeof(T);
    return v;
  }
};
}  // namespace

QByteArray encodeDay(const std::vector<Minute>& ms) {
  QByteArray b;
  size_t levels = 0;
  for (const Minute& m : ms) levels += m.levels.size();
  b.reserve(qsizetype(8 + ms.size() * 72 + levels * 8));
  b.append("FPD1", 4);
  put<quint32>(b, quint32(ms.size()));
  for (const Minute& m : ms) put<quint16>(b, quint16(((m.t % 86400) + 86400) % 86400 / 60));
  for (const Minute& m : ms) put<double>(b, m.open);
  for (const Minute& m : ms) put<double>(b, m.high);
  for (const Minute& m : ms) put<double>(b, m.low);
  for (const Minute& m : ms) put<double>(b, m.close);
  for (const Minute& m : ms) put<double>(b, m.buy);
  for (const Minute& m : ms) put<double>(b, m.sell);
  for (const Minute& m : ms) put<quint32>(b, m.trades);
  for (const Minute& m : ms) put<qint64>(b, m.firstId);
  for (const Minute& m : ms) put<quint32>(b, quint32(m.lastId - m.firstId));
  for (const Minute& m : ms) put<qint32>(b, m.base);
  for (const Minute& m : ms) put<quint16>(b, quint16(std::min<size_t>(m.levels.size(), 65535)));
  for (const Minute& m : ms) {
    const size_t n = std::min<size_t>(m.levels.size(), 65535);
    for (size_t i = 0; i < n; ++i) put<float>(b, m.levels[i].bid);
    for (size_t i = 0; i < n; ++i) put<float>(b, m.levels[i].ask);
  }
  return qCompress(b, 9);
}

bool decodeDay(QByteArrayView blob, qint64 dayStart, std::vector<Minute>& out) {
  out.clear();
  const QByteArray raw = qUncompress(reinterpret_cast<const uchar*>(blob.data()), blob.size());
  if (raw.size() < 8 || !raw.startsWith("FPD1")) return false;
  Reader r{raw.constData() + 4, raw.constData() + raw.size()};
  const quint32 n = r.get<quint32>();
  if (!r.ok || n > 1440) return false;
  out.resize(n);
  for (Minute& m : out) m.t = dayStart + qint64(r.get<quint16>()) * 60;
  for (Minute& m : out) m.open = r.get<double>();
  for (Minute& m : out) m.high = r.get<double>();
  for (Minute& m : out) m.low = r.get<double>();
  for (Minute& m : out) m.close = r.get<double>();
  for (Minute& m : out) m.buy = r.get<double>();
  for (Minute& m : out) m.sell = r.get<double>();
  for (Minute& m : out) m.trades = r.get<quint32>();
  for (Minute& m : out) m.firstId = r.get<qint64>();
  for (Minute& m : out) m.lastId = m.firstId + r.get<quint32>();
  for (Minute& m : out) m.base = r.get<qint32>();
  for (Minute& m : out) m.levels.resize(r.get<quint16>());
  if (!r.ok) { out.clear(); return false; }
  for (Minute& m : out) {
    for (Level& l : m.levels) l.bid = r.get<float>();
    for (Level& l : m.levels) l.ask = r.get<float>();
  }
  if (!r.ok || r.p != r.end) { out.clear(); return false; }
  return true;
}

// ------------------------------------------------------------------------------------------------ reading
charts::flow::FootprintCandle levelCandle(const Minute& m) {
  charts::flow::FootprintCandle c;
  c.startMs = m.t * 1000;
  c.cells.reserve(m.levels.size());
  for (size_t i = 0; i < m.levels.size(); ++i)
    if (m.levels[i].bid > 0 || m.levels[i].ask > 0) c.cells.push_back({m.base + int64_t(i), m.levels[i].bid, m.levels[i].ask});
  c.open = levelOf(m.open);
  c.high = levelOf(m.high);
  c.low = levelOf(m.low);
  c.close = levelOf(m.close);
  c.openId = m.firstId;
  c.closeId = m.lastId;
  c.bidVolume = m.sell;
  c.askVolume = m.buy;
  c.trades = m.trades;
  return c;
}

charts::flow::FootprintCandle tickCandle(const Minute& m, const charts::flow::TickScale& ticks) {
  charts::flow::FootprintCandle c;
  c.startMs = m.t * 1000;
  c.open = ticks.ticksOf(m.open);
  c.high = ticks.ticksOf(m.high);
  c.low = ticks.ticksOf(m.low);
  c.close = ticks.ticksOf(m.close);
  for (size_t i = 0; i < m.levels.size(); ++i) {
    const Level& l = m.levels[i];
    if (!(l.bid > 0 || l.ask > 0)) continue;
    // the middle of the lowest (highest) level can lie below the bar's low (above its high): its trades were at or
    // above the low, so the level goes to the low (analyze() expects every cell inside the bar's range)
    const int64_t t = std::clamp(ticks.ticksOf(levelMid(m.base + int32_t(i))), c.low, std::max(c.low, c.high));
    if (!c.cells.empty() && c.cells.back().ticks == t) { c.cells.back().bid += l.bid; c.cells.back().ask += l.ask; }
    else c.cells.push_back({t, l.bid, l.ask});
  }
  c.openId = m.firstId;
  c.closeId = m.lastId;
  c.bidVolume = m.sell;
  c.askVolume = m.buy;
  c.trades = m.trades;
  return c;
}

Stats statsOf(const Minute& m, double valueArea, const charts::flow::FootprintParams& p) {
  Stats s;
  s.levels = int(m.levels.size());
  if (m.levels.empty()) return s;
  charts::flow::FootprintView v;
  charts::flow::analyze(levelCandle(m), p, v);
  for (const auto& r : v.rows) {
    s.buyImbalances += (r.flags & charts::flow::BuyImbalance) != 0;
    s.sellImbalances += (r.flags & charts::flow::SellImbalance) != 0;
  }
  for (const auto& st : v.stacks) (st.side == charts::flow::Side::Buy ? s.buyStacks : s.sellStacks)++;
  // POC and value area on the dense levels
  auto vol = [&](size_t i) { return double(m.levels[i].bid) + double(m.levels[i].ask); };
  size_t poc = 0;
  double total = 0;
  for (size_t i = 0; i < m.levels.size(); ++i) {
    total += vol(i);
    if (vol(i) > vol(poc)) poc = i;
  }
  s.poc = levelMid(m.base + int32_t(poc));
  s.pocVolume = vol(poc);
  size_t lo = poc, hi = poc;
  double in = vol(poc);
  while (in < valueArea * total && (lo > 0 || hi + 1 < m.levels.size())) {
    const double down = lo > 0 ? vol(lo - 1) : -1, up = hi + 1 < m.levels.size() ? vol(hi + 1) : -1;
    if (up >= down) in += vol(++hi);
    else in += vol(--lo);
  }
  s.vaLow = levelLow(m.base + int32_t(lo));
  s.vaHigh = levelLow(m.base + int32_t(hi) + 1);
  return s;
}

}  // namespace technicaldata::fp

