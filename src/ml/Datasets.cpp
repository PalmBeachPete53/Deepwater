#include "ml/Datasets.h"
#include "ml/Runs.h"
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataIndicators.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "technicaldata/Dukascopy.h"
#include "technicaldata/OrderFlowFeatures.h"
#include "technicaldata/TechnicalDataStore.h"
#include <QSaveFile>
#include <QThread>
#include <QLocale>
#include "core/Paths.h"
#include "macro/MacroStore.h"
#include "macro/Registry.h"
#include "market/Candles.h"
#include "market/Instruments.h"
#include "positioning/Cot.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QTimeZone>
#include <QTimer>
#include <cmath>
#include <memory>

namespace ml {
namespace {
QString num(double v) { return std::isfinite(v) ? QString::number(v, 'g', 10) : QString(); }
QString snake(QString s) {
  s = s.toLower();
  static const QRegularExpression runs("[^a-z0-9]+"), ends("^_+|_+$");
  s.replace(runs, "_");
  return s.remove(ends);
}

struct Table {
  QStringList cols;               // without "date"
  QStringList dates;
  QVector<QVector<double>> rows;  // rows[i][j]
};
bool writeTable(const QString& path, const Table& t, QString* err) {
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) { *err = f.errorString(); return false; }
  QByteArray out = ("date," + t.cols.join(',') + "\n").toUtf8();
  for (int i = 0; i < t.rows.size(); ++i) {
    QStringList cells{t.dates[i]};
    for (double v : t.rows[i]) cells << num(v);
    out += cells.join(',').toUtf8() + "\n";
  }
  f.write(out);
  if (!f.commit()) { *err = f.errorString(); return false; }
  return true;
}
Dataset fail(const QString& e) { Dataset d; d.error = e; return d; }

// ---------------------------------------------------------------------------------------------------- prices
Table priceTable(const market::Series& s, bool intraday) {
  Table t;
  t.cols = {"open", "high", "low", "close", "volume", "ret_1", "ret_5", "ret_20", "vol_20", "rsi_14",
            "ma20_dist", "ma50_dist", "ma200_dist", "range_pct", "body_pct"};
  const auto& b = s.bars;
  const int n = int(b.size());
  bool anyVolume = false;
  for (const auto& x : b) if (x.v > 0) { anyVolume = true; break; }
  if (!anyVolume) t.cols.removeAll("volume");
  auto ma = [&](int i, int len) -> double {
    if (i + 1 < len) return NAN;
    double s = 0; for (int k = i - len + 1; k <= i; ++k) s += b[k].c;
    return s / len;
  };
  double avgGain = NAN, avgLoss = NAN;
  for (int i = 0; i < n; ++i) {
    const auto& x = b[i];
    QVector<double> r{x.o, x.h, x.l, x.c};
    if (anyVolume) r << x.v;
    auto ret = [&](int k) { return i >= k && b[i - k].c > 0 ? (x.c / b[i - k].c - 1) * 100 : NAN; };
    r << ret(1) << ret(5) << ret(20);
    double vol = NAN;
    if (i >= 20) {
      double m = 0, q = 0;
      for (int k = i - 19; k <= i; ++k) m += (b[k].c / b[k - 1].c - 1) * 100;
      m /= 20;
      for (int k = i - 19; k <= i; ++k) { const double d = (b[k].c / b[k - 1].c - 1) * 100 - m; q += d * d; }
      vol = std::sqrt(q / 19);
    }
    r << vol;
    // RSI 14, Wilder's smoothing
    double rsi = NAN;
    if (i >= 1) {
      const double ch = x.c - b[i - 1].c, g = qMax(ch, 0.0), l = qMax(-ch, 0.0);
      if (i == 14) {
        double sg = 0, sl = 0;
        for (int k = 1; k <= 14; ++k) { const double c2 = b[k].c - b[k - 1].c; sg += qMax(c2, 0.0); sl += qMax(-c2, 0.0); }
        avgGain = sg / 14; avgLoss = sl / 14;
      } else if (i > 14) {
        avgGain = (avgGain * 13 + g) / 14; avgLoss = (avgLoss * 13 + l) / 14;
      }
      if (i >= 14) rsi = avgLoss == 0 ? 100 : 100 - 100 / (1 + avgGain / avgLoss);
    }
    r << rsi;
    for (int len : {20, 50, 200}) { const double m = ma(i, len); r << (std::isfinite(m) && m > 0 ? (x.c / m - 1) * 100 : NAN); }
    r << (x.c > 0 ? (x.h - x.l) / x.c * 100 : NAN) << (x.o > 0 ? (x.c / x.o - 1) * 100 : NAN);
    t.rows << r;
    t.dates << QDateTime::fromSecsSinceEpoch(x.t, QTimeZone::UTC).toString(intraday ? "yyyy-MM-dd HH:mm" : "yyyy-MM-dd");
  }
  return t;
}

// ---------------------------------------------------------------------------------------------------- macro
struct MacroCol { macro::Ind ind; const char* name; };
const QList<MacroCol>& macroCols() {
  static const QList<MacroCol> c{
      {macro::Ind::PolicyRate, "policy_rate"}, {macro::Ind::Inflation, "inflation"}, {macro::Ind::CoreInflation, "core_inflation"},
      {macro::Ind::Unemployment, "unemployment"}, {macro::Ind::GdpQoq, "gdp_qoq"}, {macro::Ind::GdpYoy, "gdp_yoy"},
      {macro::Ind::BusinessConf, "business_confidence"}, {macro::Ind::ConsumerConf, "consumer_confidence"},
      {macro::Ind::Yield10y, "yield_10y"}, {macro::Ind::Rate3m, "rate_3m"}, {macro::Ind::RetailSalesYoy, "retail_sales_yoy"}};
  return c;
}
Table macroTable(const QString& iso2) {
  auto& st = macro::MacroStore::instance();
  QList<macro::Series> ss; QStringList names;
  QDate first, last;
  for (const auto& mc : macroCols()) {
    const macro::Series s = st.series(iso2, mc.ind);
    if (!s.ok() || s.freq == 'A') continue;
    ss << s; names << mc.name;
    const QDate a(s.pts.first().date.year(), s.pts.first().date.month(), 1), b(s.last().date.year(), s.last().date.month(), 1);
    if (!first.isValid() || a < first) first = a;
    if (!last.isValid() || b > last) last = b;
  }
  Table t; t.cols = names;
  if (ss.isEmpty()) return t;
  if (first < QDate(2000, 1, 1)) first = QDate(2000, 1, 1);
  QVector<int> pos(ss.size(), 0);
  for (QDate m = first; m <= last; m = m.addMonths(1)) {
    QVector<double> r;
    for (int k = 0; k < ss.size(); ++k) {
      const auto& p = ss[k].pts;
      while (pos[k] + 1 < p.size() && p[pos[k] + 1].date <= m) ++pos[k];
      double v = NAN;
      if (p[pos[k]].date <= m) {
        // carried forward only over the period it covers: one month for monthly data, three for quarterly
        const int span = ss[k].freq == 'Q' ? 3 : 1;
        const QDate d = p[pos[k]].date;
        const int age = (m.year() - d.year()) * 12 + m.month() - d.month();
        if (age < span) v = p[pos[k]].value;
      }
      r << v;
    }
    t.rows << r; t.dates << m.toString("yyyy-MM-dd");
  }
  return t;
}

// ---------------------------------------------------------------------------------------------------- COT
Table cotTable(const positioning::CotHistory& h, const market::Series& fut) {
  Table t;
  static const QStringList legacy{"commercials", "large_speculators", "total_reportable", "small_traders"};
  t.cols << "open_interest";
  for (int k = 0; k < 4; ++k) if (k != 2) t.cols << legacy[k] + "_net" << legacy[k] + "_net_pct";
  const QStringList second = h.secondNames();
  QList<int> secondIdx;
  for (int k = 0; k < second.size(); ++k) {
    if (second[k].contains("Total")) continue;
    secondIdx << k; t.cols << snake(second[k]) + "_net";
  }
  if (fut.ok()) t.cols << "futures_close";
  int fi = 0;
  for (const auto& w : h.weeks) {
    QVector<double> r{w.oi};
    for (int k = 0; k < 4; ++k) {
      if (k == 2) continue;
      const bool has = k < w.legacy.size();
      r << (has ? w.legacy[k].net() : NAN) << (has ? w.legacy[k].netPct() : NAN);
    }
    for (int k : secondIdx) r << (k < w.second.size() ? w.second[k].net() : NAN);
    if (fut.ok()) {
      // close of the report day (Tuesday) or the last trading day before it: never a later price
      const qint64 cut = QDateTime(w.date, QTime(23, 59), QTimeZone::UTC).toSecsSinceEpoch();
      while (fi + 1 < fut.bars.size() && fut.bars[fi + 1].t <= cut) ++fi;
      r << (fut.bars[fi].t <= cut ? fut.bars[fi].c : NAN);
    }
    t.rows << r; t.dates << w.date.toString("yyyy-MM-dd");
  }
  return t;
}

QStringList splitCsv(const QString& line) {
  QStringList out; QString cur; bool q = false;
  for (int i = 0; i < line.size(); ++i) {
    const QChar c = line[i];
    if (c == '"') { if (q && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; } else q = !q; }
    else if (c == ',' && !q) { out << cur; cur.clear(); }
    else cur += c;
  }
  out << cur;
  return out;
}
}  // namespace

QString datasetsDir() { return paths::data("ml/datasets"); }

QStringList csvCells(const QString& line) {
  QStringList c = splitCsv(line);
  for (auto& x : c) x = x.trimmed();
  return c;
}

QString idOf(const Dataset& d) { return QFileInfo(d.path).completeBaseName(); }

namespace {
QString metaPath(const QString& csvPath) { return QFileInfo(csvPath).absolutePath() + "/" + QFileInfo(csvPath).completeBaseName() + ".meta.json"; }
// <id>.meta.json next to each data set: what the list shows (the CSV alone does not say where it came from), and its
// description with the CSV's size and date, so the tools and the list read it instead of parsing up to a million rows
// on the window's thread. A CSV changed since (size or date) is described again.
void writeMeta(const Dataset& d) {
  const QFileInfo fi(d.path);
  QJsonObject o{{"title", d.title}, {"source", d.source}};
  if (d.ok()) {
    o.insert("columns", QJsonArray::fromStringList(d.columns));
    o.insert("numeric", QJsonArray::fromStringList(d.numeric));
    o.insert("date_column", d.dateColumn);
    o.insert("rows", d.rows);
    o.insert("first", d.first);
    o.insert("last", d.last);
    o.insert("size", double(fi.size()));
    o.insert("mtime", double(fi.lastModified().toMSecsSinceEpoch()));
  }
  paths::dropLink(metaPath(d.path));   // QSaveFile writes through a link
  QSaveFile f(metaPath(d.path));
  if (!f.open(QIODevice::WriteOnly)) return;
  f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
  f.commit();
}
Dataset withMeta(const QString& path) {
  QFile m(metaPath(path));
  const QJsonObject o = m.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(m.readAll()).object() : QJsonObject{};
  const QString title = o.value("title").toString(QFileInfo(path).completeBaseName()), source = o.value("source").toString();
  const QFileInfo fi(path);
  if (o.contains("rows") && qint64(o.value("size").toDouble(-1)) == fi.size() &&
      qint64(o.value("mtime").toDouble(-1)) == fi.lastModified().toMSecsSinceEpoch()) {
    Dataset d;
    d.path = path; d.title = title; d.source = source;
    for (const auto& v : o.value("columns").toArray()) d.columns << v.toString();
    for (const auto& v : o.value("numeric").toArray()) d.numeric << v.toString();
    d.dateColumn = o.value("date_column").toString();
    d.rows = o.value("rows").toInt();
    d.first = o.value("first").toString();
    d.last = o.value("last").toString();
    return d;
  }
  // written before the description was kept, or changed since: read once, then kept
  const Dataset d = describe(path, title, source);
  if (d.ok()) writeMeta(d);
  return d;
}

// Past DATASETS_MAX_BYTES, the oldest data sets go (a looping model could otherwise fill the disk): never `keep` (the
// one just written) nor the one a training is reading now.
QStringList pruneDatasets(const QString& keep) {
  QStringList gone;
  const QFileInfoList files = QDir(datasetsDir()).entryInfoList({"*.csv"}, QDir::Files, QDir::Time);   // newest first
  qint64 total = 0;
  for (const QFileInfo& f : files) total += f.size();
  const QString busy = Runs::instance().runningDataset();
  for (auto it = files.crbegin(); it != files.crend() && total > DATASETS_MAX_BYTES; ++it) {
    const QString path = it->absoluteFilePath();
    if (path == QFileInfo(keep).absoluteFilePath() || (!busy.isEmpty() && it->canonicalFilePath() == busy)) continue;
    const QString title = withMeta(path).title;   // read before the file goes: the user is told which ones
    if (!QFile::remove(path)) continue;
    QFile::remove(metaPath(path));
    total -= it->size();
    gone << (title.isEmpty() ? it->completeBaseName() : title);
  }
  return gone;
}
}  // namespace

QList<Dataset> listDatasets() {
  QList<Dataset> out;
  QDir d(datasetsDir());
  for (const QFileInfo& f : d.entryInfoList({"*.csv"}, QDir::Files, QDir::Time)) out << withMeta(f.absoluteFilePath());
  return out;
}

qint64 datasetAsOf(const QString& pathOrId) {
  static const QRegularExpression re("-asof-(\\d{12})(?:\\.csv)?$");
  const auto m = re.match(QFileInfo(pathOrId).fileName());
  if (!m.hasMatch()) return 0;
  QDateTime t = QDateTime::fromString(m.captured(1), "yyyyMMddHHmm");
  t.setTimeZone(QTimeZone::UTC);
  return t.isValid() ? t.toSecsSinceEpoch() : 0;
}

int datasetCount() { return int(QDir(datasetsDir()).entryList({"*.csv"}, QDir::Files).size()); }

int snapTechnicalDataRows(int rows) {
  if (rows <= 0) return TECHNICAL_DATA_ROWS_DEFAULT;
  for (int n : {50000, 200000, 500000}) if (rows <= n) return n;
  return TECHNICAL_DATA_ROWS_MAX;
}

QStringList lastLines(const QString& path, int n) {
  QFile f(path);
  if (n <= 0 || !f.open(QIODevice::ReadOnly)) return {};
  // backwards by blocks until n non-blank whole lines are behind the read position, or the file's start is reached
  constexpr qint64 BLOCK = 64 * 1024;
  qint64 pos = f.size();
  QByteArray buf;
  QStringList out;
  do {
    const qint64 from = std::max<qint64>(0, pos - BLOCK);
    f.seek(from);
    buf.prepend(f.read(pos - from));
    pos = from;
    QList<QByteArray> lines = buf.split('\n');
    if (!lines.isEmpty()) lines.removeFirst();   // cut in the middle (pos > 0), or the header (the whole file is read)
    out.clear();
    for (auto it = lines.crbegin(); it != lines.crend() && out.size() < n; ++it) {
      QByteArray l = *it;
      if (l.endsWith('\r')) l.chop(1);
      const QString line = QString::fromUtf8(l);
      if (line.trimmed().isEmpty()) continue;   // describe() does not count blank lines either
      out.prepend(line);
    }
  } while (pos > 0 && out.size() < n);
  return out;
}

Dataset datasetById(const QString& id) {
  // a file name the app made (snake() gives "_"): no "/", "\\" nor "." so nothing else is reachable
  static const QRegularExpression ok("^[a-z0-9][a-z0-9_-]*$");
  if (!ok.match(id).hasMatch()) return fail(QCoreApplication::translate("ai", "Unknown data set."));
  const QString path = datasetsDir() + "/" + id + ".csv";
  if (!QFileInfo::exists(path)) return fail(QCoreApplication::translate("ai", "Unknown data set."));
  return withMeta(path);
}

QString DatasetSpec::id() const {
  switch (kind) {
    case SourceKind::Prices: return "prices-" + snake(key) + "-" + interval;
    case SourceKind::Macro: return "macro-" + key.toLower();
    case SourceKind::Cot: return "cot-" + snake(key);
    case SourceKind::TechnicalData:
      return "technical-data-" + snake(key) + "-" + interval.toLower() + "-" + QString::number(snapTechnicalDataRows(rows)) +
             (keepWarm ? "-warm" : "") + (orderFlow ? "-of" : "") +
             (asOf > 0 ? "-asof-" + QDateTime::fromSecsSinceEpoch(asOf, QTimeZone::UTC).toString("yyyyMMddHHmm") : QString());
    case SourceKind::Csv: {
      const QString h = QString::fromLatin1(QCryptographicHash::hash(QFileInfo(key).absoluteFilePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
      return "csv-" + snake(QFileInfo(key).completeBaseName()) + "-" + h;
    }
  }
  return "data";
}

Dataset describe(const QString& path, const QString& title, const QString& source) {
  Dataset d; d.path = path; d.title = title; d.source = source;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return fail(QCoreApplication::translate("ai", "The file cannot be read."));
  QTextStream in(&f);
  const QString header = in.readLine();
  d.columns = splitCsv(header);
  for (auto& c : d.columns) c = c.trimmed();
  if (d.columns.size() < 2) return fail(QCoreApplication::translate("ai", "The file needs a header row and at least two columns, separated by commas."));
  for (int k = 0; k < d.columns.size(); ++k)
    if (QStringList{"date", "time", "datetime", "timestamp"}.contains(d.columns[k].toLower())) { d.dateColumn = d.columns[k]; break; }
  QVector<int> numbers(d.columns.size(), 0), filled(d.columns.size(), 0);
  const int dc = int(d.columns.indexOf(d.dateColumn));
  while (!in.atEnd()) {
    const QString line = in.readLine();
    if (line.trimmed().isEmpty()) continue;
    const QStringList cells = splitCsv(line);
    ++d.rows;
    if (dc >= 0 && dc < cells.size()) { if (d.first.isEmpty()) d.first = cells[dc]; d.last = cells[dc]; }
    for (int k = 0; k < cells.size() && k < d.columns.size(); ++k) {
      const QString c = cells[k].trimmed();
      if (c.isEmpty()) continue;
      ++filled[k];
      bool ok = false; c.toDouble(&ok);
      if (ok) ++numbers[k];
    }
  }
  for (int k = 0; k < d.columns.size(); ++k)
    if (k != dc && filled[k] > 0 && numbers[k] >= 0.8 * filled[k]) d.numeric << d.columns[k];
  if (d.rows == 0) d.error = QCoreApplication::translate("ai", "The file holds no data row.");
  else if (d.numeric.isEmpty()) d.error = QCoreApplication::translate("ai", "No column of numbers in this file.");
  return d;
}

const QStringList& levelColumns() {
  static const QStringList c{"open", "high", "low", "close", "volume", "futures_close",
                             // Data › Technical data: volumes and indicator levels follow the price
                             "quote_volume", "taker_buy_volume", "trades", "obv", "sma20", "sma50", "sma200", "ema20", "ema50",
                             "ema200", "bb_upper", "bb_basis", "bb_lower", "vwap", "atr14", "macd", "macd_signal", "macd_hist"};
  return c;
}

bool isLevelColumn(const QString& c) {
  if (levelColumns().contains(c)) return true;
  // Data › Technical data: a pair's own indicator columns, by how they follow the market's scale
  auto& st = technicaldata::Store::instance();
  for (const QString& m : st.pairs())
    for (const auto& col : technicaldata::columnsOf(technicaldata::specFromJson(st.pairSpec(m))))
      if (col.name == c) return technicaldata::scaleOf(col) != technicaldata::Scale::Free;
  return false;
}

namespace {
// Data › Technical data → CSV, written as it is read (a million rows would not fit in one string). Worker thread.
Dataset marketTable(const DatasetSpec& spec, const QString& path) {
  using namespace technicaldata;
  int tf = 0;
  for (const auto& t : timeframes()) if (t.name.compare(spec.interval, Qt::CaseInsensitive) == 0 && t.name.size() == spec.interval.size()) tf = t.seconds;
  if (!tf) for (const auto& t : timeframes()) if (t.name.compare(spec.interval, Qt::CaseInsensitive) == 0) tf = t.seconds;
  if (!tf) return fail(QCoreApplication::translate("ai", "Unknown timeframe."));
  auto& st = Store::instance();
  const SyncState s = st.state(spec.key, tf);
  if (s.count == 0) return fail(QCoreApplication::translate("ai", "No bars stored yet for this market and timeframe (Data › Technical data)."));
  const PairSpec pair = specFromJson(st.pairSpec(spec.key));
  const int WARM = warmupOf(pair);
  // a short series (BTCUSDT 1W: 483 bars) can lie wholly inside the warm-up: say so rather than give an empty table
  if (!spec.keepWarm && s.last < s.first + qint64(WARM) * tf)
    return fail(QCoreApplication::translate("ai", "All %1 stored bars of this timeframe are inside the indicators' warm-up (the "
                                                  "first %2 bars of a series, where long averages have not settled), so none is "
                                                  "left. Pick a shorter timeframe.").arg(s.count).arg(WARM));
  const int want = snapTechnicalDataRows(spec.rows);
  // the newest bars, one more for the first return; warm-up bars among them (a short series) are skipped
  // evaluation mode: the bars closed by the moment (t + tf <= asOf), nothing after
  const qint64 lastOpen = spec.asOf > 0 ? std::min(s.last, spec.asOf - tf) : s.last;
  if (lastOpen < s.first) return fail(QCoreApplication::translate("ai", "No bar of this timeframe had closed by the evaluation moment."));
  const QVector<Kline> bars = st.before(spec.key, tf, lastOpen + 1, want + 1);
  // the pair's columns (IndRow::v order) on this timeframe; levels related to the close: BTCUSDT's as they were, the
  // others by how they follow the price (an average drawn on the price → % from it; an ATR → % of the close)
  const QVector<IndColumn> icol = columnsOf(pair);
  QStringList ic;
  for (const IndColumn& c : icol) ic << c.name;
  const QStringList rel{"sma20", "sma50", "sma200", "ema20", "ema50", "ema200", "bb_upper", "bb_lower", "vwap"};
  const QStringList perClose{"atr14", "macd", "macd_signal", "macd_hist"};
  const QStringList legacy = indicatorColumnNames();
  const bool intraday = tf < 86400;
  // forex (Dukascopy): its own volume only, no quote volume, trade count or taker buys; the weekend is no gap
  const bool fx = dukascopy::isForex(spec.key);
  QStringList cols = fx ? QStringList{"open", "high", "low", "close", "volume", "ret_1", "gap_before"}
                        : QStringList{"open", "high", "low", "close", "volume", "quote_volume", "trades", "taker_buy_volume", "taker_buy_pct", "ret_1", "gap_before"};
  QList<int> indIdx;
  for (int i = 0; i < icol.size(); ++i) if (icol[i].appliesTo(tf)) { cols << ic[i]; indIdx << i; }
  QList<int> relIdx, pctIdx;
  for (const QString& r : rel) if (ic.contains(r) && icol[ic.indexOf(r)].appliesTo(tf)) { cols << r + "_dist"; relIdx << int(ic.indexOf(r)); }
  for (const QString& r : perClose) if (ic.contains(r)) { cols << r + "_pct"; pctIdx << int(ic.indexOf(r)); }
  for (int i = 0; i < icol.size(); ++i) {
    if (legacy.contains(ic[i]) || !icol[i].appliesTo(tf) || scaleOf(icol[i]) != Scale::Price) continue;
    if (isOverlay(icol[i].id)) { cols << ic[i] + "_dist"; relIdx << i; }
    else { cols << ic[i] + "_pct"; pctIdx << i; }
  }
  // the order flow's columns, and only the bars they cover (OrderFlowFeatures.h)
  std::unique_ptr<OrderFlowFeatures> flow;
  if (spec.orderFlow) {
    flow = std::make_unique<OrderFlowFeatures>(st, spec.key, tf);
    if (!flow->available())
      return fail(QCoreApplication::translate("ai", "This pair records no order flow (footprint, book depth or open interest) usable on this "
                                                    "timeframe (1m to 1D; set it in Data › Technical data › SETTINGS)."));
    cols << flow->columns();
  }
  std::vector<double> flowRow;
  int uncovered = 0;
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return fail(f.errorString());
  f.write(("date," + cols.join(',') + "\n").toUtf8());
  const qint64 warmEnd = spec.keepWarm ? LLONG_MIN : s.first + qint64(WARM) * tf;
  int written = 0, noInd = 0;
  constexpr int CHUNK = 50000;
  for (int from = 1; from < bars.size() && written < want; from += CHUNK) {
    const int to = std::min<int>(int(bars.size()), from + CHUNK);
    QHash<qint64, IndRow> ind;
    for (const IndRow& r : st.indicators(spec.key, tf, bars[from].t, bars[to - 1].t)) ind.insert(r.t, r);
    QByteArray out;
    for (int i = from; i < to; ++i) {
      const Kline& k = bars[i];
      if (k.t < warmEnd) continue;
      if (flow && !flow->row(k.t, k.c, k.v, flowRow)) { ++uncovered; continue; }
      const Kline& p = bars[i - 1];
      QStringList cells{QDateTime::fromSecsSinceEpoch(k.t, QTimeZone::UTC).toString(intraday ? "yyyy-MM-dd HH:mm" : "yyyy-MM-dd")};
      cells << num(k.o) << num(k.h) << num(k.l) << num(k.c) << num(k.v);
      if (!fx) cells << num(k.qv) << num(double(k.n)) << num(k.tbv) << num(k.v > 0 ? k.tbv / k.v * 100 : NAN);
      const qint64 next = fx ? dukascopy::nextExpected(p.t, tf) : p.t + tf;
      cells << num(p.c > 0 ? (k.c / p.c - 1) * 100 : NAN) << QString::number(k.t > next ? std::max<qint64>(1, (k.t - next) / tf) : 0);
      const auto r = ind.constFind(k.t);
      if (r == ind.cend()) ++noInd;
      auto v = [&](int c) { return r == ind.cend() ? NAN : r->v.value(c, NAN); };
      for (int c : indIdx) cells << num(v(c));
      for (int c : relIdx) { const double x = v(c); cells << num(std::isfinite(x) && x > 0 ? (k.c / x - 1) * 100 : NAN); }
      for (int c : pctIdx) cells << num(k.c > 0 ? v(c) / k.c * 100 : NAN);
      if (flow) for (double x : flowRow) cells << num(x);
      out += cells.join(',').toUtf8() + "\n";
      if (++written >= want) break;
    }
    f.write(out);
  }
  if (!f.commit()) return fail(f.errorString());
  QString source = (fx ? QStringLiteral("Dukascopy (bid)") : QStringLiteral("Binance")) + " · " + QCoreApplication::translate("ai", "indicators: Charts' engine");
  if (noInd) source += " · " + QCoreApplication::translate("ai", "%1 bars without indicators yet").arg(noInd);
  if (flow) {
    if (written == 0)
      return fail(QCoreApplication::translate("ai", "No bar of this timeframe is covered by the stored order flow yet (Data › Technical data)."));
    source += " · " + QCoreApplication::translate("ai", "order flow: the %1 bars it covers (%2 others left out)").arg(written).arg(uncovered);
  }
  return describe(path, binanceSymbol(spec.key) + " · " + spec.interval +
                            (spec.asOf > 0 ? " · as of " + QDateTime::fromSecsSinceEpoch(spec.asOf, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm") : QString()),
                  source);
}
}  // namespace

void build(const DatasetSpec& spec, QObject* ctx, DatasetCallback cb) {
  QDir().mkpath(datasetsDir());
  const QString path = datasetsDir() + "/" + spec.id() + ".csv";
  QPointer<QObject> guard(ctx);
  auto finish = [guard, cb](Dataset d) {
    if (d.ok()) { writeMeta(d); d.pruned = pruneDatasets(d.path); }
    if (guard) cb(d);
  };

  if (spec.kind == SourceKind::TechnicalData) {   // up to a million rows: written on a worker thread
    QThread* t = QThread::create([spec, path, finish] {
      const Dataset d = marketTable(spec, path);
      QMetaObject::invokeMethod(qApp, [d, finish] { finish(d); }, Qt::QueuedConnection);
    });
    t->setObjectName("ml-market-dataset");
    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start(QThread::LowPriority);
    return;
  }

  if (spec.kind == SourceKind::Csv) {   // any size: read and copied on a worker thread, not the window's
    const QString src = spec.key;
    QThread* t = QThread::create([src, path, finish] {
      Dataset d = describe(src);
      if (d.ok()) {
        QFile::remove(path);
        if (!QFile::copy(src, path)) d = fail(QCoreApplication::translate("ai", "The file could not be copied."));
        else {
          QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
          // the copy holds the same bytes: its description is the source's
          d.path = path;
          d.title = QFileInfo(src).fileName();
          d.source = QCoreApplication::translate("ai", "your file");
        }
      }
      QMetaObject::invokeMethod(qApp, [d, finish] { finish(d); }, Qt::QueuedConnection);
    });
    t->setObjectName("ml-csv-dataset");
    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start(QThread::LowPriority);
    return;
  }

  if (spec.kind == SourceKind::Prices) {
    const auto* ins = market::find(spec.key);
    if (!ins) { finish(fail(QCoreApplication::translate("ai", "Unknown market."))); return; }
    const QString interval = QStringList{"1d", "1wk", "1h"}.contains(spec.interval) ? spec.interval : "1d";
    const QString title = market::display(ins->symbol) + " · " + interval;
    auto got = [=](const market::Series& s) {
      if (!s.ok()) { finish(fail(QCoreApplication::translate("ai", "No price history for this market.") + (s.error.isEmpty() ? QString() : " (" + s.error + ")"))); return; }
      QString err;
      if (!writeTable(path, priceTable(s, interval == "1h"), &err)) { finish(fail(err)); return; }
      finish(describe(path, title, "Yahoo Finance"));
    };
    // Yahoo answers range=max with monthly bars whatever the interval: ask by dates instead (hourly: 729 days at most)
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (interval == "1h") market::candlesBetween(ins->symbol, interval, now - 729LL * 86400, now, ctx, got);
    else market::candlesBetween(ins->symbol, interval, QDateTime(QDate(1990, 1, 1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(), now, ctx, got);
    return;
  }

  if (spec.kind == SourceKind::Macro) {
    const auto* e = macro::economy(spec.key);
    if (!e) { finish(fail(QCoreApplication::translate("ai", "Unknown economy."))); return; }
    const QString iso2 = e->iso2, title = macro::displayName(*e);
    auto& st = macro::MacroStore::instance();
    st.ensureLoaded();
    // wait for the data sets (a first load takes up to a minute), then write what is there
    auto waited = std::make_shared<int>(0);
    auto write = [=] {
      Table t = macroTable(iso2);
      if (t.rows.isEmpty()) { finish(fail(QCoreApplication::translate("ai", "No monthly or quarterly series for this economy."))); return; }
      QString err;
      if (!writeTable(path, t, &err)) { finish(fail(err)); return; }
      finish(describe(path, title, "BIS · OECD · " + QCoreApplication::translate("ai", "central banks")));
    };
    if (!st.loading()) { write(); return; }
    auto* timer = new QTimer(ctx);   // owned by ctx, deleted once done (a shared_ptr captured by its own slot never was)
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, waited, write] {
      if (macro::MacroStore::instance().loading() && ++*waited < 120) return;
      timer->stop();
      timer->deleteLater();
      write();
    });
    timer->start();
    return;
  }

  // COT: the whole CFTC history, and the continuous future's daily closes to put the price next to it
  const auto* m = positioning::Cot::market(spec.key);
  if (!m) { finish(fail(QCoreApplication::translate("ai", "Unknown COT market."))); return; }
  const QString fut = m->futures, title = "COT · " + m->shortName;
  positioning::Cot::instance().deepHistory(m->id, ctx, [=](const positioning::CotHistory& h) {
    if (!h.ok()) { finish(fail(QCoreApplication::translate("ai", "The COT history could not be read.") + (h.error.isEmpty() ? QString() : " (" + h.error + ")"))); return; }
    auto withPrice = [=](const market::Series& s) {
      QString err;
      if (!writeTable(path, cotTable(h, s), &err)) { finish(fail(err)); return; }
      finish(describe(path, title, "CFTC" + QString(s.ok() ? " · Yahoo Finance" : "")));
    };
    if (fut.isEmpty()) { withPrice(market::Series{}); return; }
    market::candlesBetween(fut, "1d", QDateTime(QDate(1985, 1, 1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch(),
                           QDateTime::currentSecsSinceEpoch(), ctx, withPrice);
  });
}
}  // namespace ml
