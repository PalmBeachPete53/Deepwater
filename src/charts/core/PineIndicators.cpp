// Pine Script files as Charts indicators. See PineIndicators.h.
#include "charts/core/PineIndicators.h"
#include "charts/core/IndicatorsValidate.h"
#include "charts/core/JsStack.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <cmath>

namespace charts::pine {

namespace {
thread_local QString tlSymbol;
thread_local int tlChart = 0;   // > 0 inside a ChartScope: the chart may draw "loading…" and come back later
constexpr int MAX_RUNS = 2;
constexpr int MAX_DONE = 48;
// Off-chart runs on the GUI thread (alerts every 15 s, /api/scan, a rule's check): the GUI waits for them, so a much
// shorter limit than the 60 s / 1 GB of a run on a worker (audit P1). A script stopped by it is not run again from the
// GUI thread for kSlowPauseMs: an alert re-checking it every 15 s would otherwise freeze the window 5 s in 15.
int g_guiTimeoutMs = 5000;
int g_guiMemoryMB = 256;
constexpr qint64 kSlowPauseMs = 5 * 60 * 1000;

// 400 daily bars of a random walk (fixed seed): enough for long lookbacks to show values, quick to run
const Bars& sampleBars() {
  static const Bars b = [] {
    Bars out;
    quint32 s = 12345;
    auto rnd = [&s] { s = s * 1664525u + 1013904223u; return double(s) / 4294967296.0; };
    double price = 100;
    for (int i = 0; i < 400; ++i) {
      const double open = price, close = std::max(1.0, open * (1 + (rnd() - 0.5) * 0.04));
      const double high = std::max(open, close) * (1 + rnd() * 0.01), low = std::min(open, close) * (1 - rnd() * 0.01);
      out << Bar{1704067200 + qint64(i) * 86400, open, high, low, close, 1000 + rnd() * 9000};
      price = close;
    }
    return out;
  }();
  return b;
}

QString styleOf(const Plot& p) {
  QString s = p.options.value("style").toString();
  if (s.startsWith("style_")) s = s.mid(6);
  return s;
}

// A Charts key (letters, digits, _, starting with a letter) from a Pine title or plot key, unique in `taken`
QString safeKey(const QString& text, QSet<QString>& taken) {
  QString k;
  for (const QChar ch : text) k += (ch.isLetterOrNumber() && ch.unicode() < 128) ? ch : QChar('_');
  while (k.contains("__")) k.replace("__", "_");
  while (k.startsWith('_')) k.remove(0, 1);
  while (k.endsWith('_')) k.chop(1);
  if (k.isEmpty() || !k.at(0).isLetter()) k.prepend("p");
  QString out = k;
  for (int n = 2; taken.contains(out); ++n) out = k + "_" + QString::number(n);
  taken.insert(out);
  return out;
}

QString cleanColor(const QString& c) { return c == "na" ? QStringLiteral("rgba(0,0,0,0)") : c; }

quint64 barsKey(const Bars& bars) {
  if (bars.isEmpty()) return 0;
  return quint64(qHashBits(bars.constData(), size_t(bars.size()) * sizeof(Bar), 0x5eed)) ^ (quint64(bars.size()) << 40);
}

QString inputType(const QJsonObject& m) {
  const QString t = m.value("type").toString();
  if (t == "int") return "int";
  if (t == "float" || t == "price") return "float";
  if (t == "bool") return "select";
  if (t == "string" && !m.value("options").toArray().isEmpty()) return "select";
  if (t == "source") return "source";
  if (t == "time") return "time";
  return {};   // colour, timeframe, symbol, session, free text: the script's default
}

struct Built {
  ind::Def def;
  std::shared_ptr<Info> info;
  QString error;
};

Built buildFrom(const QString& source, const QString& file) {
  Built b;
  const Script sc = describe(source);
  if (!sc.ok()) { b.error = sc.error; return b; }
  static const QRegularExpression libraryCall(QStringLiteral("^\\s*library\\s*\\("), QRegularExpression::MultilineOption);
  if (sc.declaration == "library" || (sc.declaration.isEmpty() && libraryCall.match(source).hasMatch())) {
    b.error = QCoreApplication::translate("charts", "This is a Pine library: it gives functions to other scripts and draws nothing. Add a script that uses it.");
    return b;
  }
  auto info = std::make_shared<Info>();
  info->id = idFor(file);
  info->file = file;
  info->source = source;
  info->sourceKey = QString::number(qHash(source, 0x5eed), 16);

  ind::Def d;
  d.id = info->id;
  d.custom = true;
  d.file = file;
  d.group = QStringLiteral("Pine Script");
  // inputs
  bool sourceTaken = false;
  QSet<QString> keys;
  for (const QJsonValue& v : sc.inputs) {
    const QJsonObject m = v.toObject();
    const QString type = inputType(m);
    if (type.isEmpty()) continue;
    const QString title = m.value("title").toString().isEmpty() ? m.value("name").toString() : m.value("title").toString();
    if (title.isEmpty()) continue;
    // Charts keys are identifiers, and only one input may be the "source" one: later source inputs become a select
    // of the bar fields
    QString key;
    QString t = type;
    if (type == "source" && !sourceTaken) { key = QStringLiteral("source"); sourceTaken = true; keys.insert(key); }
    else { key = safeKey(title, keys); if (type == "source") t = QStringLiteral("select"); }
    ind::Input in;
    in.key = key;
    in.label = title;
    in.type = t;
    const QString pineType = m.value("type").toString();
    if (pineType == "source" && t == "select") {
      in.options = {"open", "high", "low", "close", "hl2", "hlc3", "ohlc4", "hlcc4"};
      in.def = m.value("defval").toString();
    } else if (pineType == "bool") {
      in.options = {"true", "false"};
      in.def = m.value("defval").toBool() ? "true" : "false";
    } else if (type == "select") {
      for (const QJsonValue& o : m.value("options").toArray()) in.options << o.toVariant().toString();
      in.def = m.value("defval").toVariant().toString();
    } else {
      in.def = m.value("defval");
    }
    if (m.value("minval").isDouble()) in.min = m.value("minval").toDouble();
    if (m.value("maxval").isDouble()) in.max = m.value("maxval").toDouble();
    if (m.value("step").isDouble()) in.step = m.value("step").toDouble();
    d.inputs << in;
    info->inputs.append({key, title, pineType});
  }
  // a run on sample bars: the plots, the title, overlay, the levels
  Options o;
  o.timeoutMs = 20000;
  const Result r = run(source, sampleBars(), o);
  if (!r.ok()) { b.error = r.error; return b; }
  // indicator() arguments, or a strategy()'s (title, shorttitle, overlay are in its config)
  const QJsonObject decl = !r.indicator.isEmpty() ? r.indicator : r.strategy.value("config").toObject();
  const QString title = decl.value("title").toString();
  const QString shortTitle = decl.value("shorttitle").toString();
  QString base = QFileInfo(file).completeBaseName();
  // the legend shows what is in brackets at the end of a name (Charts' convention, "Relative Strength Index (RSI)"):
  // there, the script's shorttitle, as TradingView's legend
  d.name = !title.isEmpty() ? title : !shortTitle.isEmpty() ? shortTitle : base;
  if (!shortTitle.isEmpty() && shortTitle != d.name) d.name += " (" + shortTitle + ")";
  // the registry takes names of at most 80 characters; TradingView takes longer titles, so a valid script was refused
  // (audit D19): cut, with an ellipsis
  if (d.name.size() > 80) d.name = d.name.left(79) + QChar(0x2026);
  d.overlay = decl.value("overlay").toBool();
  QSet<QString> outKeys;
  for (const Plot& p : r.plots) {
    const QString style = styleOf(p);
    if (style == "hline") {
      for (qsizetype i = p.values.size() - 1; i >= 0; --i)
        if (std::isfinite(p.values[i])) { d.levels << p.values[i]; break; }
      continue;
    }
    if (style == "shape" || style == "char" || style == "arrow") { info->markers.insert(p.key); continue; }
    if (style == "barcolor") { info->barcolor = p.key; continue; }
    if (style == "fill" || style == "background" || style == "bgcolor") continue;
    ind::Output out;
    out.key = safeKey(p.key, outKeys);
    info->outKey.insert(p.key, out.key);
    out.label = p.title.isEmpty() ? p.key : p.title;
    out.kind = (style == "histogram" || style == "columns") ? QStringLiteral("histogram") : QStringLiteral("line");
    const QString c = p.options.value("color").toString();
    out.color = c.isEmpty() || c == "NaN" ? QStringLiteral("#2962ff") : c;
    // Charts draws 1 to 4 px (the registry refuses more); Pine allows any width: kept within the range, not refused
    const int lw = p.options.value("linewidth").toInt();
    out.lineWidth = lw > 0 ? std::min(lw, 4) : 0;
    if (style == "circles" || style == "cross") out.extra.insert("dots", true);
    d.outputs << out;
    info->lines.insert(p.key);
  }
  if (d.outputs.isEmpty() && info->markers.isEmpty() && info->barcolor.isEmpty()) {
    b.error = QCoreApplication::translate("charts", "This script draws nothing Deepwater can show yet: it needs a plot, plotshape, plotchar, "
                                          "plotarrow or barcolor (bgcolor, fill, labels, lines, boxes and tables come later).");
    return b;
  }
  std::shared_ptr<const Info> shared = info;
  d.calc = [shared](const Bars& bars, const Params& params) { return Cache::instance().get(shared, bars, params); };
  b.def = d;
  b.info = info;
  return b;
}

QString readFile(const QString& path, bool* ok) {
  QFile f(path);
  *ok = f.open(QIODevice::ReadOnly);
  return *ok ? QString::fromUtf8(f.readAll()) : QString();
}
}  // namespace

QString idFor(const QString& fileName) {
  QString f = QFileInfo(fileName).fileName();
  if (f.endsWith(".pine")) f.chop(5);
  return "pine:" + f;
}

ChartScope::ChartScope(const QString& symbol) : previous_(tlSymbol) { tlSymbol = symbol; ++tlChart; }
ChartScope::~ChartScope() { tlSymbol = previous_; --tlChart; }
QString currentSymbol() { return tlSymbol; }

namespace {
// The part of a load that runs the script (describe + the sample run, up to 20 s for a slow one): any thread
Built buildFile(const QString& path, Loaded& l) {
  l.file = QFileInfo(path).fileName();
  l.id = idFor(l.file);
  bool ok = false;
  const QString source = readFile(path, &ok);
  if (!ok) {
    l.error = QCoreApplication::translate("charts", "Couldn't read %1").arg(l.file);
    return {};
  }
  Built b = buildFrom(source, l.file);
  if (!b.error.isEmpty()) l.error = b.error;
  return b;
}
// The registration: GUI thread (the indicator registry and the cache live there)
void registerBuilt(const Built& b, Loaded& l) {
  if (!l.error.isEmpty()) return;
  const QStringList pr = ind::registerIndicator(b.def, /*run=*/false);
  if (!pr.isEmpty()) { l.error = ind::formatProblems(pr); return; }
  Cache::instance().forget(l.id);
}
}  // namespace

Loaded loadFile(const QString& path) {
  Loaded l;
  const Built b = buildFile(path, l);
  registerBuilt(b, l);
  return l;
}

void loadFilesAsync(const QStringList& paths, QObject* ctx, std::function<void(const QVector<Loaded>&)> done) {
  struct Out { QVector<Loaded> loaded; QVector<Built> built; };
  auto out = std::make_shared<Out>();
  runTracked(
      [paths, out] {
        for (const QString& p : paths) {
          Loaded l;
          out->built << buildFile(p, l);
          out->loaded << l;
        }
      },
      ctx,
      [out, done] {
        for (qsizetype i = 0; i < out->loaded.size(); ++i) registerBuilt(out->built[i], out->loaded[i]);
        if (done) done(out->loaded);
      });
}

// The same verdict as loadFile without registering: the build, then the registry's structural rules (no trial run, as
// loadFile's registration) — before, check() passed scripts that registration then refused.
QString check(const QString& source) {
  const Built b = buildFrom(source, QStringLiteral("check.pine"));
  if (!b.error.isEmpty()) return b.error;
  const QStringList pr = ind::problems(ind::withCustomDefaults(b.def), /*run=*/false);
  return pr.isEmpty() ? QString() : ind::formatProblems(pr);
}

QJsonObject inputsOf(const Info& info, const Params& params) {
  QJsonObject o;
  for (const Info::In& m : info.inputs) {
    if (!params.contains(m.key)) continue;
    const QJsonValue v = params.value(m.key);
    if (m.type == "int") o.insert(m.title, std::trunc(num(params, m.key)));
    else if (m.type == "float" || m.type == "price" || m.type == "time") o.insert(m.title, num(params, m.key));
    else if (m.type == "bool") o.insert(m.title, v.isBool() ? v.toBool() : v.toString() == "true");
    else if (m.type == "source") {
      const QString s = str(params, m.key);
      o.insert(m.title, s.startsWith("ind:") ? QStringLiteral("close") : s);   // another output: given as the bars' close
    } else o.insert(m.title, v.toVariant().toString());
  }
  return o;
}

ind::Result toIndicator(const Info& info, const Result& run, const Bars& bars) {
  ind::Result r;
  if (!run.ok()) { r.error = run.error; return r; }
  const qsizetype n = bars.size();
  QVector<ind::Marker> marks;
  for (const Plot& p : run.plots) {
    if (info.lines.contains(p.key)) {
      const QString key = info.outKey.value(p.key, p.key);
      Series v = p.values;
      v.resize(n, NaN);
      r.put(key, v);
      if (!p.colors.isEmpty()) {
        QStringList c;
        c.reserve(n);
        for (qsizetype i = 0; i < n; ++i) c << cleanColor(p.colors.value(i));
        r.seriesColors.insert(key, c);
      }
    } else if (info.markers.contains(p.key)) {
      const QString style = styleOf(p);
      for (const QJsonValue& pv : p.points) {
        const QJsonObject pt = pv.toObject();
        const int i = pt.value("i").toInt(-1);
        if (i < 0 || i >= n) continue;
        const auto opt = [&](const char* k) { const QJsonValue x = pt.value(k); return x.isUndefined() || x.isNull() ? p.options.value(k) : x; };
        ind::Marker m;
        m.time = bars[i].time;
        QString shape = opt("shape").toString();
        if (style == "arrow" && shape.isEmpty()) shape = pt.value("value").toDouble() >= 0 ? "shape_arrow_up" : "shape_arrow_down";
        if (shape.contains("up")) m.shape = "arrowUp";
        else if (shape.contains("down")) m.shape = "arrowDown";
        else if (shape.contains("square") || shape.contains("diamond") || shape.contains("flag")) m.shape = "square";
        else m.shape = "circle";
        QString loc = opt("location").toString().toLower();
        if (loc.isEmpty()) loc = m.shape == "arrowUp" ? "belowbar" : "abovebar";
        m.position = loc.contains("below") || loc == "bottom" ? "belowBar" : loc == "absolute" ? "inBar" : "aboveBar";
        const QString c = opt("color").toString();
        m.color = c.isEmpty() || c == "NaN" ? QStringLiteral("#2962ff") : c;
        const QString ch = style == "char" ? opt("char").toString() : QString();
        const QString text = opt("text").toString();
        m.text = ch.isEmpty() ? text : text.isEmpty() ? ch : ch + " " + text;
        marks << m;
      }
    } else if (p.key == info.barcolor) {
      QStringList bc;
      bc.reserve(n);
      const QString all = p.options.value("color").toString();
      for (qsizetype i = 0; i < n; ++i) {
        QString c = p.colors.isEmpty() ? all : p.colors.value(i);
        if (c == "na" || c == "NaN") c.clear();
        bc << c;
      }
      r.putBarColors(bc);
    }
  }
  if (!marks.isEmpty()) {
    std::stable_sort(marks.begin(), marks.end(), [](const ind::Marker& a, const ind::Marker& b) { return a.time < b.time; });
    r.putMarkers(marks);
  }
  return r;
}

// ------------------------------------------------------------------------------------------------ cache
Cache& Cache::instance() {
  static Cache* c = [] {
    auto* x = new Cache;
    if (QCoreApplication::instance()) x->moveToThread(QCoreApplication::instance()->thread());
    return x;
  }();
  return *c;
}

void Cache::forget(const QString& id) {
  const QString prefix = id + "|";
  for (auto it = slowUntil_.begin(); it != slowUntil_.end();)
    if (it.key().startsWith(prefix)) it = slowUntil_.erase(it); else ++it;
  for (auto it = done_.begin(); it != done_.end();)
    if (it.key().startsWith(prefix)) it = done_.erase(it); else ++it;
  // requests waiting for a run of the old script: they would run it again (audit D19). A run in flight finishes and
  // stores its result under the old script's key, which the new script never asks for.
  for (auto it = waiting_.begin(); it != waiting_.end();)
    if (it.key().startsWith(prefix)) it = waiting_.erase(it); else ++it;
  queue_.removeIf([&](const QString& k) { return k.startsWith(prefix); });
}

ind::Result Cache::get(const std::shared_ptr<const Info>& info, const Bars& bars, const Params& params) {
  const QString symbol = currentSymbol();
  const bool gui = QThread::currentThread() == thread();
  // the script's source is in the key: after a reload, a result of the old script (kept, or of a run still in flight)
  // is never served for the new one (audit D19)
  const QString key = info->id + "|" + info->sourceKey + "|" + symbol + "|" +
                      QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
  const quint64 bk = barsKey(bars);
  if (gui) {
    auto hit = done_.find(key);
    if (hit != done_.end() && hit->barsKey == bk) { hit->used = ++clock_; return hit->result; }
  }
  if (!gui || tlChart == 0) {
    // a worker, or the strategy tester / alerts / screener (they need the values now): run here and wait; on the GUI
    // thread the result is kept, so the chart showing the same bars finds it
    Options o;
    if (!symbol.isEmpty()) o.symbol = symbol;
    o.inputs = inputsOf(*info, params);
    const QString script = info->id + "|" + info->sourceKey;
    if (gui) {
      // the GUI waits for this run: short limits, and a script they stopped is not run again here for a while
      const auto slow = slowUntil_.constFind(script);
      if (slow != slowUntil_.constEnd() && QDateTime::currentMSecsSinceEpoch() < slow->first) {
        ind::Result r;
        r.error = slow->second;
        return r;
      }
      o.timeoutMs = g_guiTimeoutMs;
      o.memoryMB = g_guiMemoryMB;
    }
    Result ran;
    js::withJsStack([&] { ran = run(info->source, bars, o); });   // pool threads have 512 KB: too little for PineTS
    ind::Result r = toIndicator(*info, ran, bars);
    if (gui) {
      if (ran.stopped)
        slowUntil_.insert(script, {QDateTime::currentMSecsSinceEpoch() + kSlowPauseMs, ran.error});
      Done d;
      d.barsKey = bk;
      d.firstTime = bars.isEmpty() ? 0 : bars.first().time;
      d.n = int(bars.size());
      d.result = r;
      d.used = ++clock_;
      keep(key, d);   // audit G10: this path never evicted (an alert's forming bar adds a key… a result every 15 s)
    }
    return r;
  }
  auto it = done_.find(key);
  Want w{info, bars, params, symbol, bk};
  if (running_.contains(key) || queue_.contains(key)) {
    if (running_.value(key) != bk || queue_.contains(key)) waiting_.insert(key, w);
  } else {
    start(key, w);
  }
  // meanwhile: the last result of the same series (same first bar, no more bars than now), padded to these bars
  if (it != done_.end() && !bars.isEmpty() && it->firstTime == bars.first().time && it->n <= bars.size()) {
    it->used = ++clock_;
    ind::Result r = it->result;
    for (auto s = r.series.begin(); s != r.series.end(); ++s) s->resize(bars.size(), NaN);
    for (auto c = r.seriesColors.begin(); c != r.seriesColors.end(); ++c) while (c->size() < bars.size()) c->append(QString());
    while (!r.barColors.isEmpty() && r.barColors.size() < bars.size()) r.barColors.append(QString());
    return r;
  }
  ind::Result pending;
  pending.pending = true;
  return pending;
}

void Cache::start(const QString& key, const Want& w) {
  if (active_ >= MAX_RUNS) {
    waiting_.insert(key, w);
    if (!queue_.contains(key)) queue_ << key;
    return;
  }
  ++active_;
  running_.insert(key, w.barsKey);
  Options o;
  if (!w.symbol.isEmpty()) o.symbol = w.symbol;
  o.inputs = inputsOf(*w.info, w.params);
  const auto info = w.info;
  const Bars bars = w.bars;
  const quint64 bk = w.barsKey;
  runAsync(info->source, bars, o, this, [this, key, info, bars, bk](const Result& res) {
    --active_;
    running_.remove(key);
    Done d;
    d.barsKey = bk;
    d.firstTime = bars.isEmpty() ? 0 : bars.first().time;
    d.n = int(bars.size());
    d.result = toIndicator(*info, res, bars);
    d.used = ++clock_;
    keep(key, d);
    // a newer request for the same key waited for this run
    if (!queue_.contains(key)) {
      const auto wi = waiting_.constFind(key);
      if (wi != waiting_.constEnd()) {
        const Want next = *wi;
        waiting_.remove(key);
        if (next.barsKey != bk) start(key, next);
      }
    }
    startQueued();
    emit ready(info->id);
  });
}

void Cache::keep(const QString& key, const Done& d) {
  done_.insert(key, d);
  while (done_.size() > MAX_DONE) {   // the least recently used goes
    auto oldest = done_.begin();
    for (auto i = done_.begin(); i != done_.end(); ++i) if (i->used < oldest->used) oldest = i;
    done_.erase(oldest);
  }
}

void Cache::setGuiLimitsForTests(int timeoutMs, int memoryMB) {
  g_guiTimeoutMs = timeoutMs > 0 ? timeoutMs : 5000;
  g_guiMemoryMB = memoryMB > 0 ? memoryMB : 256;
}

void Cache::startQueued() {
  while (active_ < MAX_RUNS && !queue_.isEmpty()) {
    const QString key = queue_.takeFirst();
    const auto wi = waiting_.constFind(key);
    if (wi == waiting_.constEnd()) continue;
    const Want w = *wi;
    waiting_.remove(key);
    if (running_.contains(key)) { waiting_.insert(key, w); continue; }
    start(key, w);
  }
}

}  // namespace charts::pine
