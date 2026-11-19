// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/imports.js.
#include "charts/data/Imports.h"
#include "charts/data/Store.h"
#include "charts/core/Series.h"
#include "core/Paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QJsonValue>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <memory>
#include <cmath>

namespace charts::data {

QString importsDir() { return paths::data("charts/imports"); }
QString customDir() { return paths::data("charts/custom"); }

namespace {
using RE = QRegularExpression;

ImportError bad(const QString& message, int status = 400) { return ImportError(message, status); }

// JS String.prototype.trim: whitespace and line terminators (QChar::isSpace) plus the BOM.
QString jsTrim(const QString& s) {
  int a = 0, b = s.size();
  auto ws = [](QChar c) { return c.isSpace() || c.unicode() == 0xFEFF; };
  while (a < b && ws(s.at(a))) ++a;
  while (b > a && ws(s.at(b - 1))) --b;
  return s.mid(a, b - a);
}

// JS days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's days_from_civil).
qint64 daysFromCivil(qint64 y, int m, int d) {
  y -= m <= 2;
  const qint64 era = (y >= 0 ? y : y - 399) / 400;
  const qint64 yoe = y - era * 400;
  const qint64 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const qint64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
// Date.UTC(y, mo, d, hh, mi, ss) / 1000, overflowing fields carried over as JS does (month 13 = next January).
double dateUtcSec(double y, double mo, double d, double hh, double mi, double ss) {
  if (y >= 0 && y <= 99) y += 1900;   // Date.UTC maps two-digit years to 19xx
  const double ym = y * 12 + mo;
  const double yy = std::floor(ym / 12);
  const int mm = int(ym - yy * 12);
  const double days = double(daysFromCivil(qint64(yy), mm + 1, 1)) + (d - 1);
  return days * 86400 + hh * 3600 + mi * 60 + ss;
}

// A cell, or JS undefined when the row is shorter.
bool has(const Row& row, int i) { return i >= 0 && i < row.size(); }
QString cellStr(const Row& row, int i) { return has(row, i) ? row.at(i) : QStringLiteral("undefined"); }   // String(x)
double cellNum(const Row& row, int i) { return has(row, i) ? jsNumber(row.at(i)) : NaN; }                // Number(x)
QString cellOrEmpty(const Row& row, int i) { return has(row, i) ? row.at(i) : QString(); }

const QString TIME_HELP =
    QStringLiteral(QT_TRANSLATE_NOOP("charts", "Use a time like 2026-10-14 14:30:00 (UTC), an ISO time like 2026-10-14T14:30:00Z, or Unix seconds."));

// ---------- the parsed-file cache: name -> { mtime, bars, baseSeconds, byTf } ----------
// A file that can't be read is cached too (error, status), by its mtime: the symbol search re-listed the imports at
// every keystroke and parsed a broken 100 MB file each time (audit P8). byTf holds a few resampled copies (audit G10:
// one full copy per timeframe ever asked, for the session).
constexpr int kMaxResampled = 3;
struct Parsed {
  qint64 mtime = 0;
  ParsedBars p;
  QString error;
  int errorStatus = 0;
  QHash<QString, Bars> byTf;   // under cacheLock()
};
QMutex& cacheLock() { static QMutex m; return m; }
QHash<QString, std::shared_ptr<Parsed>>& parsed() { static QHash<QString, std::shared_ptr<Parsed>> h; return h; }
QSet<QString>& parsing() { static QSet<QString> s; return s; }   // name|mtime parsed in the background (search)

QString fileOf(const QString& name) { return importsDir() + "/" + name + ".csv"; }
qint64 mtimeOf(const QFileInfo& st) { return st.lastModified().toMSecsSinceEpoch(); }

// Without the lock held: it takes it only to look up and store, so a long parse (seconds for a big file) never blocks
// the other callers (audit P8). Throws the file's error, cached or new. Two threads may parse the same new file at
// once; the later one's result replaces an equal one.
std::shared_ptr<Parsed> load(const QString& name) {
  // names are what cleanName() makes ([A-Z0-9_]): "CSV:DIR/../X" from a symbol or the API would leave imports/
  static const RE safe(QStringLiteral("^[A-Z0-9_]{1,40}$"));
  if (!safe.match(name).hasMatch())
    throw bad(QString("There's no imported file called %1. Import it from the strategy tester's \"Your own data\" row.").arg(name.left(60)), 404);
  const QString file = fileOf(name);
  const QFileInfo st(file);
  if (!st.exists()) {
    QMutexLocker l(&cacheLock());
    parsed().remove(name);   // a deleted file's bars go with it
    throw bad(QString("There's no imported file called %1. Import it from the strategy tester's \"Your own data\" row.").arg(name), 404);
  }
  const qint64 mtime = mtimeOf(st);
  std::shared_ptr<Parsed> hit;
  {
    QMutexLocker l(&cacheLock());
    hit = parsed().value(name);
  }
  if (!hit || hit->mtime != mtime) {
    auto p = std::make_shared<Parsed>();
    p->mtime = mtime;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) throw bad(f.errorString(), 500);   // not cached: may be passing (a lock, a copy)
    try {
      p->p = parseBars(QString::fromUtf8(f.readAll()));
    } catch (const ImportError& e) {
      p->error = e.message;
      p->errorStatus = e.status;
    }
    QMutexLocker l(&cacheLock());
    parsed().insert(name, p);
    hit = p;
  }
  if (!hit->error.isEmpty()) throw bad(hit->error, hit->errorStatus);
  return hit;
}

ImportSummary summary(const QString& name, const ParsedBars& p) {
  ImportSummary s;
  s.name = name;
  s.symbol = "CSV:" + name;
  s.bars = int(p.bars.size());
  s.tf = tfLabel(p.baseSeconds);
  s.from = p.bars.first().time;
  s.to = p.bars.last().time;
  s.skipped = p.skipped;
  return s;
}

// JS arr.slice(-limit) (limit 0 -> everything, negative -> drops the first |limit|).
Bars sliceTail(const Bars& b, int limit) {
  const qint64 len = b.size();
  qint64 start = -qint64(limit);
  start = start < 0 ? std::max<qint64>(len + start, 0) : std::min<qint64>(start, len);
  return b.mid(start);
}
}  // namespace

// ---------- JS Number(string) ----------
double jsNumber(const QString& raw) {
  const QString s = jsTrim(raw);
  if (s.isEmpty()) return 0;
  if (s == "Infinity" || s == "+Infinity") return INFINITY;
  if (s == "-Infinity") return -INFINITY;
  static const RE hex("^0[xX]([0-9a-fA-F]+)$"), oct("^0[oO]([0-7]+)$"), bin("^0[bB]([01]+)$");
  static const RE dec(R"(^[+-]?(?:\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)$)");
  auto radix = [&](const QString& digits, int base) {
    double v = 0;
    for (QChar c : digits) v = v * base + QString(c).toInt(nullptr, 16);
    return v;
  };
  if (auto m = hex.match(s); m.hasMatch()) return radix(m.captured(1), 16);
  if (auto m = oct.match(s); m.hasMatch()) return radix(m.captured(1), 8);
  if (auto m = bin.match(s); m.hasMatch()) return radix(m.captured(1), 2);
  if (!dec.match(s).hasMatch()) return NaN;
  bool ok = false;
  const double v = s.toDouble(&ok);   // C locale; overflow gives ±inf like JS
  if (ok) return v;
  // toDouble refuses "1e400" (out of range): JS gives Infinity, and underflow gives 0.
  const bool neg = s.startsWith('-');
  const int e = s.indexOf(RE("[eE]"));
  const bool bigExp = e >= 0 && !s.mid(e + 1).startsWith('-');
  return bigExp ? (neg ? -INFINITY : INFINITY) : (neg ? -0.0 : 0.0);
}

// "btc 1m (Alpaca).csv" -> "BTC_1M_ALPACA": safe as a file name and as a ticker.
QString cleanName(const QString& raw) {
  static const RE ext(R"(\.(csv|txt)\z)", RE::CaseInsensitiveOption), nonAlnum("[^A-Z0-9]+"), edges("^_+|_+$");
  QString n = raw;
  n.remove(ext);
  n = n.toUpper();
  n.replace(nonAlnum, "_");
  n.remove(edges);
  n = n.left(30);
  if (n.isEmpty()) throw bad(QT_TRANSLATE_NOOP("charts", "Give the file a name made of letters or digits"));
  return n;
}

// ---------- CSV ----------
QVector<Row> rowsOf(const QString& text) {
  QString t = text;
  if (t.startsWith(QChar(0xFEFF))) t.remove(0, 1);
  static const RE eol("\\r?\\n");
  QStringList lines;
  for (const QString& l : t.split(eol))
    if (!jsTrim(l).isEmpty() && !l.startsWith('#')) lines << l;
  if (lines.isEmpty()) throw bad(QT_TRANSLATE_NOOP("charts", "The file is empty"));
  const QString first = lines.first();
  // The delimiter that splits the first line into the most cells (ties: , then ; then tab).
  QString delim = ",";
  qsizetype best = first.count(',');
  for (const QString& d : {QStringLiteral(";"), QStringLiteral("\t")})
    if (first.count(d) > best) { best = first.count(d); delim = d; }
  QVector<Row> rows;
  rows.reserve(lines.size());
  for (const QString& l : lines) {
    Row r = l.split(delim);
    for (QString& c : r) {
      c = jsTrim(c);
      if (c.size() >= 2 && c.startsWith('"') && c.endsWith('"')) c = c.mid(1, c.size() - 2);
    }
    rows << r;
  }
  return rows;
}

namespace {
QString norm(const QString& h) {
  static const RE re("[^a-z0-9]");
  QString s = h.toLower();
  s.remove(re);
  return s;
}
}  // namespace

Columns columns(const Row& header) {
  QStringList h;
  for (const QString& c : header) h << norm(c);
  auto find = [&](std::initializer_list<const char*> names) {
    for (const char* n : names) {
      const int i = int(h.indexOf(QLatin1String(n)));
      if (i >= 0) return i;
    }
    return -1;
  };
  Columns col;
  col.time = find({"time", "timestamp", "datetime", "date", "t", "opentime", "datetimeutc", "dateutc", "timeutc", "ts", "unix", "barstart"});
  col.open = find({"open", "o", "openprice"});
  col.high = find({"high", "h", "highprice"});
  col.low = find({"low", "l", "lowprice"});
  col.close = find({"close", "c", "closeprice", "last", "price"});
  col.volume = find({"volume", "v", "vol", "basevolume"});
  col.position = find({"position", "pos", "signal", "side", "target", "direction", "value"});
  col.symbol = find({"symbol", "ticker"});
  // MetaTrader-style exports: a date column and a separate time-of-day column.
  if (h.contains("date") && h.contains("time")) {
    col.time = int(h.indexOf("date"));
    col.timeOfDay = int(h.indexOf("time"));
  }
  return col;
}

double parseTime(const QString& cell, const QString& timeOfDay) {
  const QString s = jsTrim(cell);
  static const RE unix(R"(^\d+(\.\d+)?$)");
  if (timeOfDay.isEmpty() && unix.match(s).hasMatch()) {
    const double n = s.toDouble();
    if (n > 1e17) return std::floor(n / 1e9);
    if (n > 1e14) return std::floor(n / 1e6);
    if (n > 1e11) return std::floor(n / 1e3);
    if (n > 1e8) return std::floor(n);
  }
  static const RE iso(
      R"(^(\d{4})[-/.](\d{1,2})[-/.](\d{1,2})(?:[T ]+(\d{1,2}):(\d{2})(?::(\d{2})(?:\.\d+)?)?)?\s*(Z|[+-]\d{2}:?\d{2}|UTC)?$)",
      RE::CaseInsensitiveOption);
  const auto m = iso.match(timeOfDay.isEmpty() ? s : s + " " + jsTrim(timeOfDay));
  if (!m.hasMatch()) return NaN;
  auto g = [&](int i) { return m.captured(i).isEmpty() ? 0.0 : m.captured(i).toDouble(); };
  double t = dateUtcSec(g(1), g(2) - 1, g(3), g(4), g(5), g(6));
  const QString zone = m.captured(7);
  static const RE zulu("^(Z|UTC)$", RE::CaseInsensitiveOption), off(R"(([+-])(\d{2}):?(\d{2}))");
  if (!zone.isEmpty() && !zulu.match(zone).hasMatch()) {
    const auto z = off.match(zone);
    t -= (z.captured(1) == "-" ? -1 : 1) * (z.captured(2).toDouble() * 3600 + z.captured(3).toDouble() * 60);
  }
  return t;
}

QString tfLabel(qint64 sec) {
  if (sec % 2592000 == 0) return QString("%1M").arg(sec / 2592000);
  if (sec % 604800 == 0) return QString("%1W").arg(sec / 604800);
  if (sec % 86400 == 0) return QString("%1D").arg(sec / 86400);
  if (sec % 3600 == 0) return QString("%1h").arg(sec / 3600);
  if (sec % 60 == 0) return QString("%1m").arg(sec / 60);
  return QString("%1s").arg(sec);
}

ParsedBars parseBars(const QString& text) {
  const QVector<Row> rows = rowsOf(text);
  Columns col = columns(rows[0]);
  int start = 1;
  if (col.time < 0 || col.close < 0) {
    if (isNum(parseTime(rows[0][0])) && rows[0].size() >= 5) {
      col = Columns{};
      col.time = 0; col.open = 1; col.high = 2; col.low = 3; col.close = 4;
      col.volume = rows[0].size() > 5 ? 5 : -1;
      start = 0;
    } else
      throw bad(QT_TRANSLATE_NOOP("charts", "Couldn't find the time and close columns. Name them time, open, high, low, close and volume in the first row."));
  }
  QStringList symbols;
  Bars bars;
  int skipped = 0;
  for (int r = start; r < rows.size(); ++r) {
    const Row& row = rows[r];
    const double time = parseTime(cellStr(row, col.time), col.timeOfDay >= 0 ? cellOrEmpty(row, col.timeOfDay) : QString());
    // beyond ±8.64e12 s (year ±275,000) the qint64 conversion below is out of range (undefined behaviour)
    if (!isNum(time) || std::fabs(time) > 8.64e12)
      throw bad(QString("Row %1: couldn't read the time \"%2\". %3").arg(r + 1).arg(cellStr(row, col.time).left(40), TIME_HELP));
    const double close = cellNum(row, col.close);
    const double open = col.open >= 0 ? cellNum(row, col.open) : close;
    const double high = col.high >= 0 ? cellNum(row, col.high) : std::max(open, close);
    const double low = col.low >= 0 ? cellNum(row, col.low) : std::min(open, close);
    if (!(isNum(open) && isNum(high) && isNum(low) && isNum(close)) || (has(row, col.close) && row[col.close].isEmpty())) {
      ++skipped;
      continue;
    }
    if (col.symbol >= 0 && has(row, col.symbol) && !row[col.symbol].isEmpty() && !symbols.contains(row[col.symbol]))
      symbols << row[col.symbol];
    double vol = 0;
    if (col.volume >= 0) { vol = cellNum(row, col.volume); if (std::isnan(vol) || vol == 0) vol = 0; }
    bars.push_back(Bar{qint64(time), open, std::max({high, open, close}), std::min({low, open, close}), close, vol});
  }
  if (symbols.size() > 1)
    throw bad(QString("The file has %1 symbols (%2...). Import one symbol per file.").arg(symbols.size()).arg(symbols.mid(0, 3).join(", ")));
  std::stable_sort(bars.begin(), bars.end(), [](const Bar& a, const Bar& b) { return a.time < b.time; });
  Bars out;
  out.reserve(bars.size());
  for (const Bar& b : bars) {
    if (!out.isEmpty() && b.time == out.last().time) out.last() = b;
    else out.push_back(b);
  }
  if (out.size() < 10) throw bad(QString("Only %1 usable bars in the file. It needs at least 10.").arg(out.size()));
  // The file's own timeframe: the most common gap between bars.
  QMap<qint64, int> gaps;
  for (int i = 1; i < std::min<qsizetype>(out.size(), 5000); ++i) gaps[out[i].time - out[i - 1].time]++;
  qint64 base = 0;
  int bestCount = -1;
  for (auto it = gaps.cbegin(); it != gaps.cend(); ++it)   // ascending gaps: the first with the top count wins
    if (it.value() > bestCount) { bestCount = it.value(); base = it.key(); }
  return ParsedBars{out, base, skipped};
}

namespace {
// Writes through QSaveFile (temporary file renamed over the target at commit): "" or the error, the target untouched
// on an error.
QString saveBytes(const QString& path, const QByteArray& bytes) {
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return f.errorString();
  if (f.write(bytes) != bytes.size()) {
    const QString why = f.errorString();
    f.cancelWriting();
    return why;
  }
  return f.commit() ? QString() : f.errorString();
}
}  // namespace

// ---------- bars: store, list, chart ----------
QJsonObject ImportSummary::toJson() const {
  if (!error.isEmpty()) return {{"name", name}, {"symbol", symbol}, {"error", error}};
  return {{"name", name}, {"symbol", symbol}, {"bars", bars}, {"tf", tf},
          {"from", double(from)}, {"to", double(to)}, {"skipped", skipped}};
}

ImportSummary importBars(const QString& rawName, const QString& text) {
  const QString name = cleanName(rawName);
  auto p = std::make_shared<Parsed>();
  p->p = parseBars(text);   // validated before the file is touched
  // QSaveFile: a re-import that fails (full disk) leaves the earlier file whole instead of truncating, then deleting it
  // (audit D22)
  if (const QString why = saveBytes(fileOf(name), text.toUtf8()); !why.isEmpty()) throw bad(why, 500);
  // the bars just parsed are the file's (audit P8: they were dropped and the file parsed again at the first chart)
  p->mtime = mtimeOf(QFileInfo(fileOf(name)));
  {
    QMutexLocker l(&cacheLock());
    parsed().insert(name, p);
  }
  return summary(name, p->p);
}

QVector<ImportSummary> listImports() {
  QDir dir(importsDir());
  QStringList files = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Unsorted);
  // Node's readdir order (libuv sorts names bytewise)
  std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) { return a.toUtf8() < b.toUtf8(); });
  QVector<ImportSummary> out;
  {
    // the entries of files deleted since they were read (audit G10)
    QMutexLocker l(&cacheLock());
    for (auto it = parsed().begin(); it != parsed().end();)
      if (!files.contains(it.key() + ".csv")) it = parsed().erase(it); else ++it;
  }
  for (const QString& f : files) {
    if (!f.endsWith(".csv")) continue;
    const QString name = f.chopped(4);
    try {
      out << summary(name, load(name)->p);
    } catch (const ImportError& e) {
      ImportSummary s;
      s.name = name;
      s.symbol = "CSV:" + name;
      s.error = e.message;
      out << s;
    }
  }
  return out;
}

QJsonArray listImportsJson() {
  QJsonArray a;
  for (const auto& s : listImports()) a.append(s.toJson());
  return a;
}

QJsonArray barsToJson(const Bars& bars) {
  QJsonArray a;
  for (const Bar& b : bars)
    a.append(QJsonObject{{"time", double(b.time)}, {"open", b.open}, {"high", b.high}, {"low", b.low},
                         {"close", b.close}, {"volume", b.volume}});
  return a;
}

QJsonObject CsvCandles::toJson() const {
  return {{"symbol", symbol}, {"tf", tf}, {"provider", provider}, {"candles", barsToJson(candles)},
          {"notes", QJsonArray::fromStringList(notes)}};
}

QVector<ImportSummary> listImportsReady() {
  QVector<ImportSummary> out;
  const QStringList files = QDir(importsDir()).entryList({"*.csv"}, QDir::Files, QDir::Name);
  for (const QString& f : files) {
    const QString name = f.chopped(4);
    const qint64 mtime = mtimeOf(QFileInfo(fileOf(name)));
    std::shared_ptr<Parsed> hit;
    {
      QMutexLocker l(&cacheLock());
      hit = parsed().value(name);
      if (!hit || hit->mtime != mtime) {
        // not read yet, or changed: read on the pool, once per version of the file; listed once it is
        const QString job = name + "|" + QString::number(mtime);
        if (!parsing().contains(job)) {
          parsing().insert(job);
          (void)QtConcurrent::run([name, job] {
            try {
              load(name);
            } catch (const ImportError&) {
              // cached with its error: listImports shows it, the search leaves it out
            }
            QMutexLocker l(&cacheLock());
            parsing().remove(job);
          });
        }
        continue;
      }
    }
    if (hit->error.isEmpty()) out << summary(name, hit->p);
  }
  return out;
}

CsvCandles csvCandles(const QString& name, const QString& tf, int limit) {
  const std::shared_ptr<Parsed> pp = load(name);
  const Parsed& p = *pp;
  qint64 want = 0;
  try {
    want = charts::series::parseTf(tf).seconds;
  } catch (const std::exception& e) {
    throw bad(QString::fromUtf8(e.what()), 500);   // a plain Error in the JS: the API answers 500
  }
  const qint64 base = p.p.baseSeconds;
  if (want < base)
    throw bad(QString("%1 has %2 bars, so it charts at %2 and longer.").arg(name, tfLabel(base)));
  Bars bars;
  {
    QMutexLocker l(&cacheLock());
    bars = pp->byTf.value(tf);
  }
  if (bars.isEmpty() && !p.p.bars.isEmpty()) {
    bars = charts::series::resample(p.p.bars, tf);   // outside the lock: a big file takes a while
    QMutexLocker l(&cacheLock());
    if (pp->byTf.size() >= kMaxResampled && !pp->byTf.contains(tf)) pp->byTf.clear();   // the few in use come back
    pp->byTf.insert(tf, bars);
  }
  const bool built = want > base;
  CsvCandles c;
  c.symbol = "CSV:" + name;
  c.tf = tf;
  c.provider = QString("Your file %1.csv").arg(name);
  c.candles = sliceTail(bars, limit);
  c.notes << (built ? QString("Your own bars from %1.csv, built from its %2 bars.").arg(name, tfLabel(base))
                    : QString("Your own bars from %1.csv.").arg(name));
  return c;
}

void clearCache() {
  QMutexLocker l(&cacheLock());
  parsed().clear();
}

int cachedResampledForTests(const QString& name) {
  QMutexLocker l(&cacheLock());
  const auto p = parsed().value(name);
  return p ? int(p->byTf.size()) : -1;
}

// ---------- signals ----------
QJsonObject Signals::toJson() const {
  QJsonArray ch;
  for (const auto& [t, p] : changes) ch.append(QJsonArray{double(t), p});
  return {{"rows", rows}, {"changes", ch}, {"from", double(from)}, {"to", double(to)}};
}

// Time and position per row; only the changes are kept (the position holds until the next one).
Signals parseSignals(const QString& text) {
  static const QHash<QString, double> POSITION_WORDS = {
      {"long", 1}, {"buy", 1}, {"short", -1}, {"sell", -1}, {"flat", 0}, {"none", 0}, {"exit", 0}, {"close", 0}};
  const QVector<Row> rows = rowsOf(text);
  Columns col = columns(rows[0]);
  int start = 1;
  if (col.time < 0 || col.position < 0) {
    if (isNum(parseTime(rows[0][0])) && rows[0].size() >= 2) {
      col = Columns{};
      col.time = 0;
      col.position = int(rows[0].size()) - 1;
      start = 0;
    } else
      throw bad(QT_TRANSLATE_NOOP("charts", "Couldn't find the time and position columns. Name them time and position in the first row."));
  }
  QVector<QPair<qint64, double>> pts;
  for (int r = start; r < rows.size(); ++r) {
    const Row& row = rows[r];
    const double time = parseTime(cellStr(row, col.time), col.timeOfDay >= 0 ? cellOrEmpty(row, col.timeOfDay) : QString());
    // beyond ±8.64e12 s (year ±275,000) the qint64 conversion below is out of range (undefined behaviour)
    if (!isNum(time) || std::fabs(time) > 8.64e12)
      throw bad(QString("Row %1: couldn't read the time \"%2\". %3").arg(r + 1).arg(cellStr(row, col.time).left(40), TIME_HELP));
    const QString cell = jsTrim(cellOrEmpty(row, col.position)).toLower();
    const double pos = POSITION_WORDS.contains(cell) ? POSITION_WORDS.value(cell) : jsNumber(cell);
    if (!isNum(pos) || pos < -1 || pos > 1)
      throw bad(QString("Row %1: the position \"%2\" isn't one of 1 (long), 0 (flat) or -1 (short).")
                    .arg(r + 1).arg(cellStr(row, col.position).left(20)));
    pts.push_back({qint64(time), pos});
  }
  std::stable_sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  Signals s;
  for (const auto& [t, p] : pts)
    if (s.changes.isEmpty() || s.changes.last().second != p) s.changes.push_back({t, p});
  if (pts.isEmpty()) throw bad(QT_TRANSLATE_NOOP("charts", "The file has no rows"));
  s.rows = int(pts.size());
  s.from = pts.first().first;
  s.to = pts.last().first;
  return s;
}

QString signalIndicatorSource(const QString& name, const Signals& s, const QString& fileName) {
  QJsonArray T, P;
  for (const auto& [t, p] : s.changes) { T.append(double(t)); P.append(p); }
  QString from = fileName.isEmpty() ? name : fileName;
  // the name goes into a // comment of generated code: every control, format and line / paragraph separator character
  // goes (U+2028 / U+2029 end a JS comment line just like \n, and what follows would run as code)
  from.replace(RE("[\\p{C}\\p{Zl}\\p{Zp}]"), " ");
  const QString date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd");
  return QString(
R"(// Position signal imported from %1 on %2.
// +1 long, 0 flat, -1 short, each holding until the next change. Import the CSV again to replace it.
const T = %3;
const P = %4;

export default {
  name: %5,
  overlay: false,
  inputs: [],
  outputs: [{ key: "position", label: "Position", kind: "histogram", color: "#2962ff" }],
  levels: [0],
  calc(bars) {
    const position = new Array(bars.length).fill(NaN);
    const colors = new Array(bars.length).fill("#787b86");
    let k = -1;
    for (let i = 0; i < bars.length; i++) {
      const end = i + 1 < bars.length ? bars[i + 1].time : bars[i].time + (i ? bars[i].time - bars[i - 1].time : 86400);
      while (k + 1 < T.length && T[k + 1] < end) k++;
      if (k < 0) continue;
      position[i] = P[k];
      colors[i] = P[k] > 0 ? "#089981" : P[k] < 0 ? "#f23645" : "#787b86";
    }
    return { position, colors };
  },
};
)")
      .arg(from, date, store::stringify(T, 0), store::stringify(P, 0), store::stringify(QJsonValue("Signal: " + name), 0));
}

SignalFile writeSignalIndicator(const QString& name, const Signals& s, const QString& fileName) {
  SignalFile f;
  f.file = "signal-" + name.toLower() + ".js";
  f.full = customDir() + "/" + f.file;
  f.id = "custom:" + f.file.chopped(3);
  QFile old(f.full);
  if (old.exists() && old.open(QIODevice::ReadOnly)) { f.existed = true; f.before = old.readAll(); old.close(); }
  // through QSaveFile: a failed write (full disk) leaves the old indicator whole
  if (const QString why = saveBytes(f.full, signalIndicatorSource(name, s, fileName).toUtf8()); !why.isEmpty()) throw bad(why, 500);
  return f;
}

void restoreSignalIndicator(const SignalFile& f) {
  if (!f.existed) { QFile::remove(f.full); return; }
  saveBytes(f.full, f.before);
}

QJsonObject signalImportResult(const QString& name, const SignalFile& f, const Signals& s) {
  return {{"ok", true}, {"name", name}, {"id", f.id}, {"file", f.file}, {"rows", s.rows},
          {"changes", int(s.changes.size())}, {"from", double(s.from)}, {"to", double(s.to)},
          {"strategy", signalStrategy(f.id)}};
}

QJsonObject signalStrategy(const QString& id) {
  auto rule = [](const char* op) { return QJsonArray{QJsonObject{{"left", "signal"}, {"op", op}, {"right", 0}}}; };
  return {{"indicators", QJsonObject{{"signal", QJsonObject{{"id", id}}}}},
          {"entryLong", rule(">")}, {"exitLong", rule("<=")}, {"entryShort", rule("<")}, {"exitShort", rule(">=")}};
}

}  // namespace charts::data
