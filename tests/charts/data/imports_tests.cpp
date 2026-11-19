// Port of quant-terminal/test/imports.test.js.
// Your own data files: OHLCV bars from a CSV (chart and backtest them on any longer timeframe) and a
// position signal (+1/0/-1) that the strategy tester and quant checks run exactly as written.
// From a Skool comment: 1-minute bars exported from Alpaca, and a strategy written elsewhere.
//
// The JS goes through the local HTTP server (/api/import/bars, /api/candles, /api/import/signal, /api/backtest);
// the port calls the functions those routes call (ApiServer is another module). /api/search and the quant checks
// belong to other modules and are not checked here.
#include "Check.h"
#include "charts/data/Imports.h"
#include "charts/data/Store.h"
#include "charts/core/Series.h"
#include "charts/core/Indicators.h"
#include "charts/core/Backtest.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJSValue>
#include <QTemporaryDir>
#include <QThread>
#include <cmath>

using namespace charts;
using namespace charts::data;

namespace {
const qint64 T0 = QDateTime(QDate(2026, 10, 14), QTime(13, 30), QTimeZone::UTC).toSecsSinceEpoch();

// 1-minute bars the way alpaca-py's DataFrame.to_csv() writes them.
const Bars& minute() {
  static const Bars m = [] {
    Bars b;
    for (int i = 0; i < 600; ++i) {
      const double c = 60000 + 50 * std::sin(i / 20.0) + i;
      b.push_back(Bar{T0 + i * 60, c - 3, c + 10, c - 10, c, double(1 + (i % 7))});
    }
    return b;
  }();
  return m;
}
QString iso(qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).toString("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"); }
QString n(double v) { return store::numberToString(v); }
const QString& alpacaCsv() {
  static const QString s = [] {
    QStringList lines{"symbol,timestamp,open,high,low,close,volume,trade_count,vwap"};
    for (const Bar& b : minute())
      lines << QString("BTC/USD,%1,%2,%3,%4,%5,%6,12,%7")
                   .arg(iso(b.time).replace("T", " ").replace(".000Z", "+00:00"), n(b.open), n(b.high), n(b.low),
                        n(b.close), n(b.volume), n(b.close));
    return lines.join("\n");
  }();
  return s;
}
bool sameBar(const Bar& a, const Bar& b) {
  return a.time == b.time && a.open == b.open && a.high == b.high && a.low == b.low && a.close == b.close && a.volume == b.volume;
}
template <class F>
QString errorOf(F f, int* status = nullptr) {
  try { f(); } catch (const ImportError& e) { if (status) *status = e.status; return e.message; }
  return {};
}

// The signal indicator as the custom-indicator loader would run it: the generated ES module in QJSEngine.
struct JsIndicator {
  QJSEngine engine;
  QJSValue def;
  QString error;
  explicit JsIndicator(const QString& source) {
    QTemporaryDir dir;
    const QString file = dir.path() + "/signal.mjs";
    QFile f(file);
    (void)f.open(QIODevice::WriteOnly);
    f.write(source.toUtf8());
    f.close();
    const QJSValue mod = engine.importModule(file);
    if (mod.isError()) error = mod.toString();
    else def = mod.property("default");
  }
  Series position(const Bars& bars, QStringList* colors = nullptr) {
    QJSValue arr = engine.newArray(uint(bars.size()));
    for (int i = 0; i < bars.size(); ++i) {
      QJSValue b = engine.newObject();
      b.setProperty("time", double(bars[i].time));
      b.setProperty("open", bars[i].open);
      b.setProperty("high", bars[i].high);
      b.setProperty("low", bars[i].low);
      b.setProperty("close", bars[i].close);
      b.setProperty("volume", bars[i].volume);
      arr.setProperty(uint(i), b);
    }
    const QJSValue res = def.property("calc").callWithInstance(def, {arr});
    Series s;
    const QJSValue pos = res.property("position");
    for (int i = 0; i < bars.size(); ++i) s << pos.property(uint(i)).toNumber();
    if (colors) for (int i = 0; i < bars.size(); ++i) *colors << res.property("colors").property(uint(i)).toString();
    return s;
  }
  // A registry entry for the backtester, built from the module's fields (what CustomJs does for custom/*.js).
  ind::Def toDef(const QString& id, const QString& file) {
    ind::Def d;
    d.id = id;
    d.name = def.property("name").toString();
    d.overlay = def.property("overlay").toBool();
    d.custom = true;
    d.file = file;
    const QJSValue outs = def.property("outputs");
    for (uint i = 0; i < outs.property("length").toUInt(); ++i) {
      ind::Output o;
      o.key = outs.property(i).property("key").toString();
      o.label = outs.property(i).property("label").toString();
      o.kind = outs.property(i).property("kind").toString();
      o.color = outs.property(i).property("color").toString();
      d.outputs << o;
    }
    const QJSValue lv = def.property("levels");
    for (uint i = 0; i < lv.property("length").toUInt(); ++i) d.levels << lv.property(i).toNumber();
    d.calc = [this](const Bars& bars, const Params&) {
      ind::Result r;
      QStringList colors;
      r.put("position", position(bars, &colors));
      r.putColors(colors);
      return r;
    };
    return d;
  }
};
}  // namespace

TEST("times: ISO with or without a zone, Unix seconds or milliseconds, dotted dates with a separate time") {
  const double t = double(QDateTime(QDate(2026, 10, 14), QTime(14, 30), QTimeZone::UTC).toSecsSinceEpoch());
  for (const QString s : {QStringLiteral("2026-10-14 14:30:00"), QStringLiteral("2026-10-14T14:30:00Z"),
                          QStringLiteral("2026-10-14T14:30:00.000Z"), QStringLiteral("2026-10-14 14:30:00+00:00"),
                          QStringLiteral("2026-10-14T10:30:00-04:00"), n(t), n(t * 1000)})
    CHECK_NEAR(parseTime(s), t, 0);
  CHECK_NEAR(parseTime("2026.10.14", "14:30"), t, 0);
  CHECK_NEAR(parseTime("2026-10-14"), double(QDateTime(QDate(2026, 10, 14), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch()), 0);
  CHECK(std::isnan(parseTime("14/10/2026")));
  // Unix micro- and nanoseconds; Date.UTC carries overflowing fields over
  CHECK_NEAR(parseTime(n(t) + "000000"), t, 0);
  CHECK_NEAR(parseTime(n(t) + "000000000"), t, 0);
  CHECK_NEAR(parseTime("2026-13-01"), double(QDateTime(QDate(2027, 1, 1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch()), 0);
}

TEST("an Alpaca 1-minute export parses, and its timeframe is found from the bars") {
  const ParsedBars p = parseBars(alpacaCsv());
  CHECK_EQ(p.bars.size(), 600);
  CHECK_EQ(p.baseSeconds, 60);
  CHECK(sameBar(p.bars[0], minute()[0]));
}

TEST("headerless TradingView-style rows and semicolon MetaTrader rows parse too") {
  QStringList tv;
  for (int i = 0; i < 20; ++i) {
    const Bar& b = minute()[i];
    tv << QStringList{n(double(b.time)), n(b.open), n(b.high), n(b.low), n(b.close), n(b.volume)}.join(",");
  }
  CHECK_EQ(parseBars(tv.join("\n")).bars.size(), 20);
  QStringList mt{"Date;Time;Open;High;Low;Close;Volume"};
  for (int i = 0; i < 20; ++i) {
    const Bar& b = minute()[i];
    const QString d = iso(b.time);
    mt << QString("%1;%2;%3;%4;%5;%6;%7").arg(d.left(10).replace("-", "."), d.mid(11, 5), n(b.open), n(b.high), n(b.low), n(b.close), n(b.volume));
  }
  const ParsedBars p = parseBars(mt.join("\n"));
  CHECK_EQ(p.bars[5].time, minute()[5].time);
}

TEST("a file that can't be read is refused with the row and what to change") {
  CHECK(errorOf([] { parseBars("time,open,high,low,close\n14/10/2026,1,2,0,1\n"); })
            .startsWith("Row 2: couldn't read the time \"14/10/2026\". Use a time like"));
  QString two = alpacaCsv();
  const int at = int(two.indexOf("\nBTC/USD,"));
  two.replace(at, 9, "\nETH/USD,");
  const QString e2 = errorOf([&] { parseBars(two); });
  CHECK(e2.contains("2 symbols") && e2.contains("Import one symbol per file"));
  CHECK(errorOf([] { parseBars(alpacaCsv().split("\n").mid(0, 5).join("\n")); }).contains("It needs at least 10"));
  CHECK(errorOf([] { parseBars("a,b\n1,2"); }).startsWith("Couldn't find the time and close columns"));
  CHECK_EQ(errorOf([] { parseBars("\n# only a comment\n"); }), QString("The file is empty"));
}

TEST("imported bars chart as CSV:NAME on their own timeframe and longer, never shorter") {
  const ImportSummary r = importBars("btc 1m (alpaca).csv", alpacaCsv());
  CHECK_EQ(r.symbol, QString("CSV:BTC_1M_ALPACA"));
  CHECK_EQ(r.tf, QString("1m"));
  CHECK_EQ(r.bars, 600);
  const CsvCandles m1 = csvCandles("BTC_1M_ALPACA", "1m", 1000);
  CHECK_EQ(m1.candles.size(), 600);
  CHECK(m1.provider.contains("Your file BTC_1M_ALPACA.csv"));
  const CsvCandles m5 = csvCandles("BTC_1M_ALPACA", "5m", 1000);
  CHECK_EQ(m5.candles.size(), 120);
  Bar want{minute()[0].time, minute()[0].open, -INFINITY, INFINITY, minute()[4].close, 0};
  for (int i = 0; i < 5; ++i) {
    want.high = std::max(want.high, minute()[i].high);
    want.low = std::min(want.low, minute()[i].low);
    want.volume += minute()[i].volume;
  }
  CHECK(sameBar(m5.candles[0], want));
  CHECK(m5.notes[0].contains("built from its 1m bars"));
  int status = 0;
  const QString e = errorOf([] { csvCandles("BTC_1M_ALPACA", "30s"); }, &status);
  CHECK_EQ(status, 400);
  CHECK(e.contains("BTC_1M_ALPACA has 1m bars, so it charts at 1m and longer"));
  bool listed = false;
  for (const auto& x : listImports()) listed = listed || (x.symbol == "CSV:BTC_1M_ALPACA" && x.bars == 600);
  CHECK(listed);
  // the JSON shapes the local API sends
  const QJsonObject j = r.toJson();
  CHECK_EQ(QStringList(j.keys()), (QStringList{"bars", "from", "name", "skipped", "symbol", "tf", "to"}));
  CHECK_EQ(m1.toJson().value("candles").toArray().size(), 600);
  CHECK_EQ(csvCandles("BTC_1M_ALPACA", "1m", 10).candles.size(), 10);
  CHECK_EQ(csvCandles("BTC_1M_ALPACA", "1m", 0).candles.size(), 600);   // slice(-0) = everything
  errorOf([] { csvCandles("NOPE", "1m"); }, &status);
  CHECK_EQ(status, 404);
}

TEST("signals: words or numbers, only the changes are kept, anything else is refused") {
  const Signals s = parseSignals(
      "time,position\n2026-10-14 13:30,0\n2026-10-14 13:31,1\n2026-10-14 13:32,1\n2026-10-14 13:33,short\n2026-10-14 13:34,flat\n");
  CHECK_EQ(s.rows, 5);
  QVector<double> pos;
  for (const auto& c : s.changes) pos << c.second;
  CHECK_EQ(pos, (QVector<double>{0, 1, -1, 0}));
  CHECK(errorOf([] { parseSignals("time,position\n2026-10-14 13:30,2\n"); })
            .startsWith("Row 2: the position \"2\" isn't one of 1 (long), 0 (flat) or -1 (short)"));
}

TEST("a signal becomes a valid indicator: each bar takes the position set by its close") {
  QStringList lines{"time,position"};
  for (const auto& [k, p] : QVector<QPair<int, int>>{{0, 0}, {10, 1}, {17, -1}, {22, 0}})
    lines << QString("%1,%2").arg(minute()[k].time).arg(p);
  const Signals s = parseSignals(lines.join("\n"));
  JsIndicator js(signalIndicatorSource("TEST", s, "test.csv"));
  CHECK(js.error.isEmpty());
  if (!js.error.isEmpty()) return;
  ind::Def def = js.toDef("custom:signal-test", "signal-test.js");
  CHECK_EQ(def.name, QString("Signal: TEST"));
  CHECK_EQ(ind::problems(def), QStringList());   // validateIndicator
  const Bars bars = minute().mid(0, 30);
  const Series pos = js.position(bars);
  CHECK_EQ(pos[9], 0.0);
  CHECK_EQ(pos[10], 1.0);
  CHECK_EQ(pos[16], 1.0);
  CHECK_EQ(pos[17], -1.0);
  CHECK_EQ(pos[22], 0.0);
  // 5-minute bars: bar 2 covers minutes 10-14 (long by its close), bar 3 covers 15-19 (short by its close).
  const Series five = js.position(series::resample(bars, "5m"));
  CHECK_EQ(five, (Series{0, 0, 1, -1, 0, 0}));
}

TEST("the signal file's text is the JS one") {
  Signals s;
  s.changes = {{1760448600, 0}, {1760449200, 1}, {1760449800, -0.5}};
  const QString src = signalIndicatorSource("X_1", s, "a\r\nb.csv");
  const QString today = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd");
  CHECK(src.startsWith("// Position signal imported from a  b.csv on " + today + ".\n"));
  CHECK(src.contains("const T = [1760448600,1760449200,1760449800];\nconst P = [0,1,-0.5];\n"));
  CHECK(src.contains("  name: \"Signal: X_1\",\n"));
}

TEST("an imported signal backtests exactly as written") {
  // Long from minute 100's close, short from 300's, flat from 450's.
  QStringList lines{"time,position"};
  for (const auto& [k, p] : QVector<QPair<int, int>>{{0, 0}, {100, 1}, {300, -1}, {450, 0}})
    lines << QString("%1,%2").arg(iso(minute()[k].time)).arg(p);
  const QString name = cleanName("test v10");
  const Signals sig = parseSignals(lines.join("\n"));
  const SignalFile f = writeSignalIndicator(name, sig, "test v10");
  const QJsonObject r = signalImportResult(name, f, sig);
  CHECK_EQ(r.value("id").toString(), QString("custom:signal-test_v10"));
  CHECK_EQ(r.value("changes").toInt(), 4);
  CHECK(QFile::exists(customDir() + "/signal-test_v10.js"));
  QFile src(f.full);
  (void)src.open(QIODevice::ReadOnly);
  static JsIndicator* js = new JsIndicator(QString::fromUtf8(src.readAll()));
  CHECK(js->error.isEmpty());
  if (!js->error.isEmpty()) return;
  CHECK_EQ(ind::registerIndicator(js->toDef(f.id, f.file)), QStringList());
  QJsonObject strategy = r.value("strategy").toObject();
  strategy.insert("feePct", 0);
  strategy.insert("slippagePct", 0);
  const Bars bars = csvCandles("BTC_1M_ALPACA", "1m", 1000).candles;
  const bt::Result res = bt::backtest(bars, strategy);
  CHECK_EQ(res.trades.size(), 2);
  if (res.trades.size() == 2) {
    // fills at the next bar's open after each change
    CHECK_EQ(res.trades[0].side, QString("long"));
    CHECK_EQ(res.trades[0].entryTime, minute()[101].time);
    CHECK_EQ(res.trades[0].exitTime, minute()[301].time);
    CHECK_EQ(res.trades[1].side, QString("short"));
    CHECK_EQ(res.trades[1].entryTime, minute()[301].time);
    CHECK_EQ(res.trades[1].exitTime, minute()[451].time);
    CHECK_EQ(res.trades[0].entryPrice, minute()[101].open);
  }
  ind::unregisterIndicator(f.id);
  int status = 0;
  errorOf([] { parseSignals("time,position\n2026-10-14,9\n"); }, &status);
  CHECK_EQ(status, 400);
  CHECK(!QFile::exists(customDir() + "/signal-broken.js"));
}

TEST("a signal file that fails to load is rolled back") {
  const Signals sig = parseSignals("time,position\n2026-10-14,1\n");
  const SignalFile a = writeSignalIndicator("ROLL", sig, {});
  CHECK(QFile::exists(a.full));
  restoreSignalIndicator(a);
  CHECK(!QFile::exists(a.full));
  QFile f(a.full);
  (void)f.open(QIODevice::WriteOnly);
  f.write("old");
  f.close();
  const SignalFile b = writeSignalIndicator("ROLL", sig, {});
  CHECK(b.existed);
  restoreSignalIndicator(b);
  QFile g(a.full);
  (void)g.open(QIODevice::ReadOnly);
  CHECK_EQ(g.readAll(), QByteArray("old"));
  QFile::remove(a.full);
}

TEST("names, labels and JS numbers") {
  CHECK_EQ(cleanName("btc 1m (Alpaca).csv"), QString("BTC_1M_ALPACA"));
  CHECK_EQ(cleanName("__a.b.TXT"), QString("A_B"));
  CHECK_EQ(cleanName(QString(40, 'x')).size(), 30);
  CHECK_EQ(errorOf([] { cleanName("...csv"); }), QString("Give the file a name made of letters or digits"));
  CHECK_EQ(tfLabel(60), QString("1m"));
  CHECK_EQ(tfLabel(7200), QString("2h"));
  CHECK_EQ(tfLabel(86400), QString("1D"));
  CHECK_EQ(tfLabel(604800), QString("1W"));
  CHECK_EQ(tfLabel(2592000), QString("1M"));
  CHECK_EQ(tfLabel(90), QString("90s"));
  CHECK_EQ(jsNumber(""), 0.0);
  CHECK_EQ(jsNumber(" 12.5 "), 12.5);
  CHECK_EQ(jsNumber("0x1F"), 31.0);
  CHECK_EQ(jsNumber("1e3"), 1000.0);
  CHECK_EQ(jsNumber(".5"), 0.5);
  CHECK_EQ(jsNumber("5."), 5.0);
  CHECK(std::isnan(jsNumber("1,5")));
  CHECK(std::isnan(jsNumber("12abc")));
  CHECK(std::isinf(jsNumber("-Infinity")));
  // the delimiter that splits the first line most; quotes around cells go
  const auto rows = rowsOf("\xEF\xBB\xBF\"a\";\"b\",c;d\n1;2;3\n");
  CHECK_EQ(rows[0], (QStringList{"a", "\"b\",c", "d"}));
  const Columns c = columns({"Date", "Time", "Open", "High", "Low", "Close", "Tick Volume"});
  CHECK_EQ(c.time, 0);
  CHECK_EQ(c.timeOfDay, 1);
  CHECK_EQ(c.close, 5);
  CHECK_EQ(c.volume, -1);
  const Signals s = parseSignals("2026-10-14,buy\n2026-10-15,SELL\n2026-10-16,\n");
  CHECK_EQ(s.rows, 3);
  CHECK_EQ(s.changes.size(), 3);
  CHECK_EQ(s.changes[2].second, 0.0);   // an empty position is Number("") = 0
}

TEST("a row whose close is empty or not a number is skipped and counted") {
  QStringList lines{"time,open,high,low,close"};
  for (int i = 0; i < 12; ++i) lines << QString("%1,1,2,0,%2").arg(T0 + i * 3600).arg(i == 3 ? "" : i == 4 ? "x" : "1");
  const ParsedBars p = parseBars(lines.join("\n"));
  CHECK_EQ(p.bars.size(), 10);
  CHECK_EQ(p.skipped, 2);
  CHECK_EQ(p.baseSeconds, 3600);
}

TEST("imports: a file name cannot break out of the generated indicator's comment (audit S6)") {
  Signals s;
  s.rows = 2;
  s.changes = {{1700000000, 1}, {1700001200, 0}};
  const QString evil = QString("x") + QChar(0x2028) + "globalThis.pwned = 1;" + QChar(0x2029) + "y\r\n.csv";
  const QString src = signalIndicatorSource("SIG", s, evil);
  CHECK(!src.contains(QChar(0x2028)) && !src.contains(QChar(0x2029)));
  // the whole name stays on the first (comment) line
  CHECK(src.section('\n', 0, 0).contains("globalThis.pwned"));
  CHECK(!src.section('\n', 1).contains("globalThis.pwned"));
}

TEST("imports: names outside imports/ are refused (audit S9)") {
  bool refused = false;
  try { (void)csvCandles("DIR/../X", "1D"); } catch (const std::exception&) { refused = true; }
  CHECK(refused);
}

// Audit P8 / G10: the parsed bars of an import are kept, a file that can't be read is not parsed again until it
// changes, a few resampled copies at most are kept, a deleted file's entry goes, and the search's listing never parses.
TEST("imports: the parse cache keeps imports, remembers failures, caps resampled copies, forgets deleted files") {
  clearCache();
  importBars("cache test", alpacaCsv());
  CHECK_EQ(cachedResampledForTests("CACHE_TEST"), 0);   // in the cache straight from the import (it was dropped)
  for (const QString tf : {"5m", "15m", "30m", "1h", "2h", "4h"}) csvCandles("CACHE_TEST", tf);
  CHECK(cachedResampledForTests("CACHE_TEST") <= 3);
  CHECK(cachedResampledForTests("CACHE_TEST") >= 1);

  // a broken file: its error is cached by the file's time; a new content with the same time is not read again
  const QString broken = importsDir() + "/BROKEN_TEST.csv";
  {
    QFile f(broken);
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("time,open,high,low,close\nnot a time,1,2,0,1\n");
  }
  QFile bf(broken);
  CHECK(bf.open(QIODevice::ReadWrite));
  const QDateTime stamp = QFileInfo(broken).lastModified();
  bf.close();
  int status = 0;
  const QString first = errorOf([] { csvCandles("BROKEN_TEST", "1m"); }, &status);
  CHECK(!first.isEmpty());
  CHECK_EQ(status, 400);
  {
    QFile f(broken);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(alpacaCsv().toUtf8());
    CHECK(f.setFileTime(stamp, QFileDevice::FileModificationTime));
  }
  CHECK_EQ(errorOf([] { csvCandles("BROKEN_TEST", "1m"); }), first);   // the cached failure, not a new parse
  bool listedWithError = false;
  for (const auto& x : listImports()) listedWithError = listedWithError || (x.name == "BROKEN_TEST" && x.error == first);
  CHECK(listedWithError);

  // deleted: the next listing drops its entry
  QFile::remove(importsDir() + "/CACHE_TEST.csv");
  listImports();
  CHECK_EQ(cachedResampledForTests("CACHE_TEST"), -1);
  QFile::remove(broken);
  listImports();
  CHECK_EQ(cachedResampledForTests("BROKEN_TEST"), -1);

  // the search's listing: a file not read yet is read on the pool and listed by a later call
  clearCache();
  {
    QFile f(importsDir() + "/READY_TEST.csv");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write(alpacaCsv().toUtf8());
  }
  auto listed = [] {
    for (const auto& x : listImportsReady()) if (x.name == "READY_TEST" && x.bars == 600) return true;
    return false;
  };
  CHECK(!listed());
  QElapsedTimer t;
  t.start();
  bool ok = false;
  while (!(ok = listed()) && t.elapsed() < 5000) QThread::msleep(5);
  CHECK(ok);
  QFile::remove(importsDir() + "/READY_TEST.csv");
}
