// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/alerts.js (see Alerts.h).
#include "charts/data/Alerts.h"
#include "charts/data/Providers.h"
#include "charts/data/Store.h"
#include "charts/core/Backtest.h"
#include "charts/core/Series.h"
#include "net/Sources.h"
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <filesystem>
#include <memory>

namespace charts::alerts {

using sources::Access;
static const QStringList kPage = {"charts/charts"};
// Webhooks are URLs the user types into an alert; these are the three services the JS formats a message for.
static const sources::Registrar regDiscord({"discord-webhook", "Discord (incoming webhooks)", {"discord.com", "discordapp.com"},
    Access::Account, 1000, "Nothing is read: fired alerts are posted to the webhook URL you gave an alert",
    "Your own webhook URL, created in your Discord server's settings; Discord limits a webhook to about 5 messages "
    "per 2 s, an alert sends one message when it fires", kPage});
static const sources::Registrar regSlack({"slack-webhook", "Slack (incoming webhooks)", {"hooks.slack.com"},
    Access::Account, 1000, "Nothing is read: fired alerts are posted to the webhook URL you gave an alert",
    "Your own incoming-webhook URL from a Slack app you installed; Slack allows about one message per second per "
    "webhook, an alert sends one message when it fires", kPage});
static const sources::Registrar regTelegram({"telegram-bot", "Telegram (Bot API)", {"api.telegram.org"},
    Access::Account, 1000, "Nothing is read: fired alerts are posted to the bot URL you gave an alert (sendMessage)",
    "Your own bot token in the URL (BotFather); Telegram allows about 30 messages per second per bot and one per "
    "second per chat, an alert sends one message when it fires", kPage});
// Any other webhook URL: the host is the user's own choice (a home server, n8n, Zapier...). Its host joins this entry
// when an alert first posts to it, so net::Http paces it and the Sources tab shows it instead of an undeclared host.
static const sources::Registrar regWebhook({"webhook", "Your own webhook URLs (alerts)", {},
    Access::Account, 1000, "Nothing is read: fired alerts are posted as JSON to the webhook URL you gave an alert",
    "A URL you chose yourself; Deepwater posts one message per fired alert to it and reads nothing back", kPage});

namespace {
data::Error bad(const QString& m, int status = 400) { return data::Error{m, status, false, {}}; }

// A short synthetic series is enough to check that every operand and indicator in a rule exists.
const Bars& PROBE() {
  static const Bars probe = [] {
    Bars b;
    b.reserve(300);
    for (int t = 0; t < 300; ++t) {
      const double c = 100 + 10 * std::sin(t / 7.0);
      b.push_back(Bar{qint64(t) * 60, c - 1, c + 2, c - 2, c, double(1000 + t)});
    }
    return b;
  }();
  return probe;
}

bool truthy(const QJsonValue& v) { return bt::jsTruthy(v); }
QJsonValue orElse(const QJsonValue& v, const QJsonValue& def) { return truthy(v) ? v : def; }   // JS ||
bool nullish(const QJsonValue& v) { return v.isNull() || v.isUndefined(); }                    // JS ?? test
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QJsonObject indicatorsOf(const QJsonObject& a) {
  const QJsonValue v = a.value("indicators");
  return v.isObject() ? v.toObject() : QJsonObject();
}

// The JS message of a fetch that never got an answer.
QString networkError(const net::Reply& rep) {
  const QString e = rep.error.toLower();
  if (e.contains("cancel") || e.contains("timeout") || e.contains("timed out"))
    return QStringLiteral(QT_TRANSLATE_NOOP("charts", "The operation was aborted due to timeout"));
  return QStringLiteral(QT_TRANSLATE_NOOP("charts", "fetch failed"));
}
}  // namespace

Alerts& Alerts::instance() {
  static Alerts* a = new Alerts;
  return *a;
}

Alerts::Alerts() {
  timer_.setSingleShot(true);
  connect(&timer_, &QTimer::timeout, this, [this] {
    evaluate([this] {
      if (started_) timer_.start(interval_);
    });
  });
  watcher_ = new QFileSystemWatcher(this);
  // Writes replace the file (temp file + rename): the folder's change is what every platform reports.
  connect(watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { syncFromDisk(); });
  connect(watcher_, &QFileSystemWatcher::fileChanged, this, [this] { syncFromDisk(); });
  reload();
}

// ---------------------------------------------------------------------------------------------- the files
// alerts.json is shared by every Deepwater on this data folder (see Alerts.h): read it again whenever it changed.
namespace {
QString fileOf(const QString& name) { return store::dataDir() + "/" + name + ".json"; }
struct Stamp { bool exists = false; qint64 size = -1, mtime = 0; };
Stamp stampOf(const QString& file) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path p(file.toStdU16String());
  if (!fs::is_regular_file(p, ec) || ec) return {};
  const auto size = fs::file_size(p, ec);
  if (ec) return {};
  const auto t = fs::last_write_time(p, ec);   // ns on macOS/Linux, 100 ns on Windows
  if (ec) return {};
  return {true, qint64(size), qint64(t.time_since_epoch().count())};
}
}  // namespace

// Held for the few ms of a read-modify-write; other processes wait for it (up to 2 s, then go ahead without it).
// Taken once per process: a nested change (a slot of fired()...) runs inside the outer one's lock.
class Alerts::Lock {
public:
  explicit Lock(const QString& path) : f_(path) {
    if (depth()++ > 0) return;
    f_.setStaleLockTime(10000);   // a crashed holder's lock is taken over (its pid is gone, or after 10 s)
    held_ = f_.tryLock(2000);
    if (!held_) qWarning().noquote() << "[charts] alerts.lock busy: changing the alerts without it";
  }
  ~Lock() {
    --depth();
    if (held_) f_.unlock();
  }
private:
  static int& depth() { static int d = 0; return d; }
  QLockFile f_;
  bool held_ = false;
};

bool Alerts::syncFile(const QString& name, QJsonArray& into, Disk& d, bool force, bool quiet) const {
  const QString file = fileOf(name);
  const Stamp st = stampOf(file);
  if (!force && d.known && st.exists == d.exists && st.size == d.size && st.mtime == d.mtime) return false;
  QByteArray bytes;
  if (st.exists) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;   // can't read it now: keep what we have, try again next time
    bytes = f.readAll();
  }
  if (d.known && st.exists == d.exists && bytes == d.bytes) {   // touched, same content (our own write, an equal one)
    d.size = st.size;
    d.mtime = st.mtime;
    return false;
  }
  QJsonArray next;
  if (st.exists) {
    // As store::read: UTF-8 (bad bytes become U+FFFD), any JSON value at the top level; not an array -> [].
    const QByteArray text = QString::fromUtf8(bytes).toUtf8();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson("[" + text + "]", &err);
    // Text that isn't JSON: a write caught half way (Windows writes in place when the rename is refused) or a hand
    // edit with a typo. Keep the list we have (the JS would drop it at its next start); read again once it changes.
    if (err.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size() != 1) {
      // The next change of the alerts writes over it: a copy keeps what the user had (audit D22; at a start on such
      // a file the list here is empty and the first save replaced every alert). Once per distinct content.
      if (bytes != d.badCopied && !store::keepAside(name).isEmpty()) d.badCopied = bytes;
      if (!d.known) { d.known = true; d.exists = st.exists; d.size = st.size; d.mtime = st.mtime; d.bytes = bytes; }
      return false;
    }
    const QJsonValue v = doc.array().at(0);
    next = v.isArray() ? v.toArray() : QJsonArray();
  }
  d.known = true;
  d.exists = st.exists;
  d.size = st.size;
  d.mtime = st.mtime;
  d.bytes = bytes;
  if (next == into) return false;
  into = next;
  if (!quiet) queueChanged();
  return true;
}

bool Alerts::writeFile(const QString& name, const QJsonArray& value, Disk& d) {
  const QByteArray bytes = store::stringify(value, 2).toUtf8();   // what store::write writes
  if (d.known && d.exists && bytes == d.bytes) {
    // the file as last read or written: nothing to write, unless it changed since (then it is written over, as before)
    const Stamp st = stampOf(fileOf(name));
    if (st.exists && st.size == d.size && st.mtime == d.mtime) return false;
  }
  if (!store::write(name, value)) return false;   // nothing written: the file is as we last knew it
  const Stamp st = stampOf(fileOf(name));
  d.known = true;
  d.exists = st.exists;
  d.size = st.size;
  d.mtime = st.mtime;
  d.bytes = bytes;
  watch();
  return true;
}

std::unique_ptr<Alerts::Lock> Alerts::lockAndSync() {
  QDir().mkpath(store::dataDir());
  auto lock = std::make_unique<Lock>(store::dataDir() + "/alerts.lock");
  syncFile("alerts", alerts_, alertsDisk_, true);
  syncFile("alert-log", log_, logDisk_, true);
  return lock;
}

void Alerts::queueChanged() const {
  if (changeQueued_) return;
  changeQueued_ = true;
  auto* self = const_cast<Alerts*>(this);
  QMetaObject::invokeMethod(self, [self] {
    self->changeQueued_ = false;
    emit self->changed();
  }, Qt::QueuedConnection);
}

void Alerts::syncFromDisk() {
  syncFile("alerts", alerts_, alertsDisk_, false);
  syncFile("alert-log", log_, logDisk_, false);
  watch();
}

void Alerts::watch() {
  const QString dir = store::dataDir();
  if (dir != watched_) {
    if (!watcher_->directories().isEmpty()) watcher_->removePaths(watcher_->directories());
    if (!watcher_->files().isEmpty()) watcher_->removePaths(watcher_->files());
    watched_ = dir;
    if (QDir(dir).exists()) watcher_->addPath(dir);
    else watched_.clear();
  }
  // The files too (a write in place, where the rename is refused); a rename drops them from the watch: added again.
  for (const QString& name : {QStringLiteral("alerts"), QStringLiteral("alert-log")}) {
    const QString f = fileOf(name);
    if (!watcher_->files().contains(f) && QFile::exists(f)) watcher_->addPath(f);
  }
}

void Alerts::reload() {
  alertsDisk_ = Disk();
  logDisk_ = Disk();
  alerts_ = QJsonArray();
  log_ = QJsonArray();
  syncFile("alerts", alerts_, alertsDisk_, true, true);
  syncFile("alert-log", log_, logDisk_, true, true);
  watched_.clear();
  watch();
}

QJsonArray Alerts::list() const {
  syncFile("alerts", alerts_, alertsDisk_, false);
  return alerts_;
}
QJsonArray Alerts::history() const {
  syncFile("alert-log", log_, logDisk_, false);
  return log_;
}

int Alerts::indexOf(const QString& id) const {
  for (int i = 0; i < alerts_.size(); ++i)
    if (alerts_[i].toObject().value("id").toString() == id) return i;
  return -1;
}
std::optional<QJsonObject> Alerts::find(const QString& id) const {
  syncFile("alerts", alerts_, alertsDisk_, false);
  const int i = indexOf(id);
  if (i < 0) return std::nullopt;
  return alerts_[i].toObject();
}

void Alerts::save(std::unique_ptr<Lock>& lock) {
  writeFile("alerts", alerts_, alertsDisk_);
  lock.reset();   // a slot may change the alerts again
  emit changed();
}
void Alerts::saveQuiet() { writeFile("alerts", alerts_, alertsDisk_); }

QJsonArray Alerts::conditionsOf(const QJsonObject& a) {
  const QJsonValue c = a.value("conditions");
  if (c.isArray() && !c.toArray().isEmpty()) return c.toArray();
  QJsonObject one;
  for (const char* k : {"left", "op", "right"})
    if (a.contains(k)) one.insert(k, a.value(k));
  return QJsonArray{one};
}

std::optional<data::Error> Alerts::validate(const QJsonObject& a) {
  const QJsonValue wl = a.value("watchlist");
  if (!wl.isUndefined() && (!wl.isString() || wl.toString().isEmpty()))
    return bad(QT_TRANSLATE_NOOP("charts", "watchlist must be the name of one of your watchlists"));
  const QJsonValue sym = a.value("symbol");
  if (!truthy(wl) && (!truthy(sym) || !sym.isString())) return bad(QT_TRANSLATE_NOOP("charts", "An alert needs a symbol or a watchlist"));
  const QJsonArray conds = conditionsOf(a);
  if (conds.size() > 5) return bad(QT_TRANSLATE_NOOP("charts", "An alert can have up to 5 conditions"));
  const QJsonValue match = orElse(a.value("match"), QStringLiteral("all"));
  if (!(match == QJsonValue("all") || match == QJsonValue("any"))) return bad(QT_TRANSLATE_NOOP("charts", "match must be \"all\" or \"any\""));
  // the timeframe the candles are loaded with: one that can't be parsed never loads, and the alert never fires
  // (absent: add() gives 1m)
  const QJsonValue tf = a.value("tf");
  if (!tf.isUndefined() && !tf.isString()) return bad(QT_TRANSLATE_NOOP("charts", "tf must be a timeframe such as 1m, 4h or 1D"));
  if (tf.isString()) {
    try {
      series::parseTf(tf.toString());
    } catch (const std::exception& e) {
      return bad(QString::fromUtf8(e.what()));
    }
  }
  const QJsonValue hook = a.value("webhook");
  static const QRegularExpression url(QStringLiteral("^https?://\\S+$"), QRegularExpression::CaseInsensitiveOption);
  if (truthy(hook) && !url.match(bt::jsString(hook)).hasMatch()) return bad(QT_TRANSLATE_NOOP("charts", "Webhook must be an http(s) URL"));
  for (const QJsonValue& cv : conds) {
    const QJsonObject c = cv.toObject();
    const QJsonValue right = c.value("right");
    if (nullish(right) || right == QJsonValue(QString())) return bad(QT_TRANSLATE_NOOP("charts", "An alert needs something to compare against"));
    const QJsonValue op = c.value("op");
    if (!op.isString() || !bt::OPS.contains(op.toString()))
      return bad(QStringLiteral("Unknown operator \"%1\". Use one of %2").arg(bt::jsString(op), bt::OPS.join(", ")));
    try {
      bt::conditionFn(PROBE(), indicatorsOf(a))(c)(299);
    } catch (const std::exception& e) {
      return bad(QString::fromUtf8(e.what()));
    }
  }
  return std::nullopt;
}

data::Result<QJsonObject> Alerts::add(const QJsonValue& inputV) {
  using R = data::Result<QJsonObject>;
  if (!inputV.isObject() && !inputV.isArray()) return R::failure(bad(QT_TRANSLATE_NOOP("charts", "Send the alert as a JSON object")));
  const QJsonObject input = inputV.toObject();   // an array is an object to the JS: it has none of the fields
  const QJsonValue conds = input.value("conditions");
  const QJsonObject first = conds.isArray() ? conds.toArray().at(0).toObject() : QJsonObject();
  QJsonObject a;
  a["id"] = uuid();
  const QJsonValue wl = input.value("watchlist");
  if (truthy(wl)) a["symbol"] = QString();
  else if (input.contains("symbol")) a["symbol"] = input.value("symbol");
  if (truthy(wl)) a["watchlist"] = wl;
  a["tf"] = orElse(input.value("tf"), QStringLiteral("1m"));
  {
    QJsonValue left = input.value("left");
    if (nullish(left)) left = first.value("left");
    if (nullish(left)) left = QStringLiteral("close");
    a["left"] = left;
  }
  a["op"] = orElse(input.value("op"), orElse(first.value("op"), QStringLiteral("crossesAbove")));
  {
    QJsonValue right = input.value("right");
    if (nullish(right)) right = first.value("right");
    if (!right.isUndefined()) a["right"] = right;
  }
  if (conds.isArray()) a["conditions"] = conds;
  a["match"] = orElse(input.value("match"), QStringLiteral("all"));
  a["indicators"] = orElse(input.value("indicators"), QJsonObject());
  a["trigger"] = orElse(input.value("trigger"), QStringLiteral("once"));
  a["webhook"] = orElse(input.value("webhook"), QString());
  a["message"] = orElse(input.value("message"), QString());
  a["active"] = true;
  a["createdAt"] = data::isoNow();
  a["lastBar"] = QJsonValue::Null;
  a["fired"] = 0;
  if (auto e = validate(a)) return R::failure(*e);
  auto lock = lockAndSync();
  alerts_.append(a);
  save(lock);
  return R::success(a);
}

data::Result<QJsonObject> Alerts::update(const QString& id, const QJsonObject& patch) {
  using R = data::Result<QJsonObject>;
  auto lock = lockAndSync();   // the alert as it is now on disk (another Deepwater may have changed it)
  const int i = indexOf(id);
  if (i < 0) return R::failure(bad(QT_TRANSLATE_NOOP("charts", "No such alert"), 404));
  QJsonObject a = alerts_[i].toObject();
  for (auto it = patch.begin(); it != patch.end(); ++it) a.insert(it.key(), it.value());
  if (auto e = validate(a)) return R::failure(*e);
  a["id"] = id;
  alerts_[i] = a;
  save(lock);
  return R::success(a);
}

void Alerts::remove(const QString& id) {
  auto lock = lockAndSync();
  QJsonArray next;
  for (const QJsonValue& v : alerts_)
    if (v.toObject().value("id").toString() != id) next.append(v);
  alerts_ = next;
  save(lock);
}

QString Alerts::describe(const QJsonObject& a) { return describe(a, bt::jsString(a.value("symbol"))); }

QString Alerts::describe(const QJsonObject& a, const QString& symbol) {
  static const QHash<QString, QString> ops = {
      {">", QT_TRANSLATE_NOOP("charts", "is above")},          {"<", QT_TRANSLATE_NOOP("charts", "is below")},
      {">=", QT_TRANSLATE_NOOP("charts", "is at or above")},   {"<=", QT_TRANSLATE_NOOP("charts", "is at or below")},
      {"crossesAbove", QT_TRANSLATE_NOOP("charts", "crosses above")}, {"crossesBelow", QT_TRANSLATE_NOOP("charts", "crosses below")},
      {"crosses", QT_TRANSLATE_NOOP("charts", "crosses")},
  };
  const auto name = [](const QJsonValue& o) {
    if (o.isObject() && truthy(o.toObject().value("line"))) return QStringLiteral(QT_TRANSLATE_NOOP("charts", "your drawn line"));
    return bt::jsString(o);
  };
  QStringList parts;
  for (const QJsonValue& cv : conditionsOf(a)) {
    const QJsonObject c = cv.toObject();
    const QString op = bt::jsString(c.value("op"));
    parts << QStringLiteral("%1 %2 %3").arg(name(c.value("left")), ops.value(op, op), name(c.value("right")));
  }
  if (truthy(a.value("message")))
    return truthy(a.value("watchlist")) ? symbol + ": " + bt::jsString(a.value("message"))
                                        : bt::jsString(a.value("message"));
  return QStringLiteral("%1 %2: %3")
      .arg(symbol, bt::jsString(a.value("tf")), parts.join(a.value("match") == QJsonValue("any") ? " or " : " and "));
}

QJsonObject Alerts::webhookBody(const QString& url, const QString& text, const QJsonObject& payload) {
  const QUrl u(url);
  QString host = u.host();
  if (u.port() >= 0) host += ":" + QString::number(u.port());   // JS URL.host keeps the port
  if (host.contains("discord")) return QJsonObject{{"content", text}};
  if (host.contains("slack.com")) return QJsonObject{{"text", text}};
  if (host.contains("telegram.org")) return QJsonObject{{"text", text}};
  QJsonObject body{{"text", text}, {"content", text}};
  for (auto it = payload.begin(); it != payload.end(); ++it) body.insert(it.key(), it.value());
  return body;
}

void Alerts::postJson(const QString& url, const QJsonObject& body, std::function<void(int, const QString&)> done) {
  const QUrl u(url, QUrl::StrictMode);
  if (!u.isValid() || u.host().isEmpty() || (u.scheme() != "http" && u.scheme() != "https")) {
    data::later([done] { done(0, QStringLiteral(QT_TRANSLATE_NOOP("charts", "Invalid URL"))); });
    return;
  }
  // A host nobody declared is the user's own webhook: it joins the "webhook" source (see regWebhook).
  const QString host = u.host().toLower();
  if (!sources::forHost(host)) {
    if (const sources::Source* w = sources::byId("webhook")) {
      sources::Source s = *w;
      s.hosts = {host};
      sources::declare(s);
    }
  }
  net::Request r;
  r.url = u;
  r.method = "POST";
  r.headers = {{"Content-Type", "application/json"}};
  r.body = store::stringify(body, 0).toUtf8();   // JSON.stringify(body)
  r.ttlSec = 0;
  r.staleOnError = false;
  r.timeoutMs = 10000;
  data::send(r, [done](const net::Reply& rep) { done(rep.status, rep.status == 0 ? networkError(rep) : QString()); });
}

void Alerts::sendWebhook(const QString& url, const QString& text, const QJsonObject& payload,
                         std::function<void(const QString&)> done) {
  postJson(url, webhookBody(url, text, payload), [done](int status, const QString& error) {
    if (status == 0) done(error);
    else if (status < 200 || status >= 300) done(QStringLiteral("Webhook returned HTTP %1").arg(status));
    else done(QString());
  });
}

// The alert's changes are made on the list as it is on disk now (another Deepwater may have edited it since the run
// read it) and written at once; mark() adds the run's own (lastBar, lastBars) to them.
void Alerts::fire(const QString& id, const Bar& bar, double price, const QString& symbol,
                  std::function<void(QJsonObject&)> mark, std::function<void()> done) {
  QJsonObject a;
  {
    const auto lock = lockAndSync();
    const int i = indexOf(id);
    if (i < 0) { done(); return; }   // deleted meanwhile
    a = alerts_[i].toObject();
    if (mark) mark(a);
    a["fired"] = bt::jsNum(a.value("fired")) + 1;
    a["lastFiredAt"] = data::isoNow();
    if (a.value("trigger") == QJsonValue("once") && !truthy(a.value("watchlist"))) a["active"] = false;
    alerts_[i] = a;
    saveQuiet();
  }
  const QString text = QStringLiteral("Alert: %1 (price %2)").arg(describe(a, symbol), store::numberToString(price));
  auto event = std::make_shared<QJsonObject>(QJsonObject{
      {"id", uuid()},
      {"alertId", a.value("id")},
      {"symbol", symbol},
      {"tf", a.value("tf")},
      {"text", text},
      {"price", price},
      {"barTime", double(bar.time)},
      {"at", a.value("lastFiredAt")},
  });
  const auto finish = [this, event, done] {
    {
      const auto lock = lockAndSync();   // the log as it is on disk (the checker may have moved to another instance)
      log_.prepend(*event);
      while (log_.size() > 500) log_.removeLast();
      writeFile("alert-log", log_, logDisk_);
    }
    emit fired(*event);
    done();
  };
  if (truthy(a.value("webhook"))) {
    QJsonObject payload{{"alert", a}, {"event", *event}};
    sendWebhook(bt::jsString(a.value("webhook")), text, payload, [event, finish](const QString& err) {
      (*event)["webhook"] = err.isEmpty() ? QStringLiteral(QT_TRANSLATE_NOOP("charts", "sent")) : "failed: " + err;
      finish();
    });
  } else {
    finish();
  }
}

void Alerts::evaluate(std::function<void()> done) {
  if (done) waiters_ << std::move(done);
  if (running_) return;
  running_ = true;
  evaluateOnce([this] { finishRun(); });
}

void Alerts::finishRun() {
  running_ = false;
  const auto w = std::move(waiters_);
  waiters_.clear();
  for (const auto& fn : w) fn();
}

namespace {
struct Group { QString symbol, tf; QStringList ids; };
struct Run {
  QList<Group> groups;
  int g = 0, k = 0;
  Bars bars;
  std::function<void()> done;
};
}  // namespace

void Alerts::evaluateOnce(std::function<void()> done) {
  auto run = std::make_shared<Run>();
  run->done = std::move(done);
  syncFile("alerts", alerts_, alertsDisk_, true);   // the list as it is on disk
  QJsonObject lists;           // the watchlists in state.json, read only when an active alert follows one
  bool listsRead = false;
  QHash<QString, int> at;
  for (const QJsonValue& v : alerts_) {
    const QJsonObject a = v.toObject();
    if (!truthy(a.value("active"))) continue;
    QStringList symbols;
    if (truthy(a.value("watchlist"))) {
      if (!listsRead) { lists = store::read("state", QJsonObject()).toObject().value("watchlists").toObject(); listsRead = true; }
      QSet<QString> seen;
      for (const QJsonValue& s : lists.value(bt::jsString(a.value("watchlist"))).toArray()) {
        const QString sym = bt::jsString(s);
        if (!seen.contains(sym)) { seen.insert(sym); symbols << sym; }
      }
    } else {
      symbols << bt::jsString(a.value("symbol"));
    }
    const QString tf = bt::jsString(a.value("tf"));
    for (const QString& symbol : symbols) {
      const QString key = symbol + "|" + tf;
      if (!at.contains(key)) { at.insert(key, run->groups.size()); run->groups << Group{symbol, tf, {}}; }
      run->groups[at[key]].ids << a.value("id").toString();
    }
  }

  // The JS loop with its awaits, as continuations: next() moves to the next alert of the group or the next group.
  // `step` is a trampoline around `body`: a continuation called while body is running (the synchronous paths — an
  // alert skipped, an alert that doesn't fire) only asks for one more turn of the loop instead of calling body again,
  // so thousands of alerts on one symbol no longer nest one stack frame each (a stack overflow). Asynchronous
  // continuations (candles loaded, a webhook done) start the loop again.
  auto body = std::make_shared<std::function<void()>>();
  auto step = std::make_shared<std::function<void()>>();
  auto loop = std::make_shared<std::pair<bool, bool>>(false, false);   // running, one more turn
  *step = [body, loop] {
    if (loop->first) { loop->second = true; return; }
    loop->first = true;
    do {
      loop->second = false;
      (*body)();
    } while (loop->second);
    loop->first = false;
  };
  *body = [this, run, step] {
    if (run->g >= run->groups.size()) {
      // as the JS: written at the end of every check that looked at something; with no active alert there is nothing
      // to write (the JS rewrote the same list every 15 s from launch, with a lock, and the Alerts tab rebuilt)
      if (!run->groups.isEmpty()) {
        auto lock = lockAndSync();
        const bool wrote = writeFile("alerts", alerts_, alertsDisk_);   // skipped when nothing changed
        lock.reset();   // a slot may change the alerts again
        if (wrote) emit changed();
      }
      auto d = std::move(run->done);
      // break the step -> closure -> step cycle once this call has returned (never destroy a running closure)
      QTimer::singleShot(0, this, [step] { *step = nullptr; });
      d();
      return;
    }
    const Group& grp = run->groups[run->g];
    if (run->k == 0 && run->bars.isEmpty()) {
      data::getCandles(grp.symbol, grp.tf, 400, this, [this, run, step, ids = grp.ids](const data::Result<data::CandlesResult>& r) {
        // Recorded on the alerts (audit D19): an alert whose candles never load (a bad timeframe stored before tf was
        // validated, a symbol gone) failed silently forever. Cleared when they load again.
        const bool loaded = r.ok() && r.value.candles.size() >= 3;
        noteLoadError(ids, loaded ? QString() : r.ok() ? QStringLiteral("Not enough candles to check this alert") : r.error->message);
        if (!loaded) { ++run->g; run->k = 0; run->bars.clear(); (*step)(); return; }
        run->bars = r.value.candles;
        (*step)();
      });
      return;
    }
    if (run->k >= grp.ids.size()) { ++run->g; run->k = 0; run->bars.clear(); (*step)(); return; }
    const QString id = grp.ids[run->k++];
    const QString symbol = grp.symbol;
    syncFile("alerts", alerts_, alertsDisk_, false);   // edited elsewhere while the candles loaded: check the new rule
    const int idx = indexOf(id);
    if (idx < 0) { (*step)(); return; }
    QJsonObject a = alerts_[idx].toObject();
    const Bars& bars = run->bars;
    try {
      auto make = bt::conditionFn(bars, indicatorsOf(a));
      QList<bt::Condition> tests;
      for (const QJsonValue& c : conditionsOf(a)) tests << make(c.toObject());
      const bool any = a.value("match") == QJsonValue("any");
      const auto test = [&](int i) {
        for (const auto& t : tests) {
          const bool v = t(i);
          if (any && v) return true;
          if (!any && !v) return false;
        }
        return !any;
      };
      const QString trigger = bt::jsString(a.value("trigger"));
      const int i = trigger == "oncePerBarClose" ? bars.size() - 2 : bars.size() - 1;
      const Bar bar = bars[i];
      if (truthy(a.value("watchlist"))) {
        QJsonObject lastBars = a.value("lastBars").isObject() ? a.value("lastBars").toObject() : QJsonObject();
        if (!truthy(a.value("lastBars"))) {   // JS a.lastBars ||= {}: kept even when nothing fires
          const auto lock = lockAndSync();
          const int j = indexOf(id);
          if (j >= 0) {
            QJsonObject b = alerts_[j].toObject();
            if (!truthy(b.value("lastBars"))) { b["lastBars"] = QJsonObject(); alerts_[j] = b; saveQuiet(); }
          }
        }
        const QJsonValue seen = lastBars.value(symbol);
        if (trigger == "once" ? lastBars.contains(symbol) : (seen.isDouble() && seen.toDouble() == double(bar.time))) {
          (*step)();
          return;
        }
        if (!test(i)) { (*step)(); return; }
        const double t = double(bar.time);
        fire(id, bar, bar.close, symbol, [symbol, t](QJsonObject& b) {
          QJsonObject lb = b.value("lastBars").isObject() ? b.value("lastBars").toObject() : QJsonObject();
          lb[symbol] = t;
          b["lastBars"] = lb;
        }, [step] { (*step)(); });
        return;
      }
      if (!truthy(a.value("active"))) { (*step)(); return; }
      const QJsonValue lastBar = a.value("lastBar");
      if (trigger != "once" && lastBar.isDouble() && lastBar.toDouble() == double(bar.time)) { (*step)(); return; }
      if (test(i)) {
        const double t = double(bar.time);
        fire(id, bar, bar.close, bt::jsString(a.value("symbol")), [t](QJsonObject& b) { b["lastBar"] = t; },
             [step] { (*step)(); });
        return;
      }
    } catch (const std::exception& e) {
      const auto lock = lockAndSync();
      const int j = indexOf(id);
      if (j >= 0) {
        QJsonObject b = alerts_[j].toObject();
        b["error"] = QString::fromUtf8(e.what());
        alerts_[j] = b;
        saveQuiet();
      }
    }
    (*step)();
  };
  (*step)();
}

void Alerts::noteLoadError(const QStringList& ids, const QString& error) {
  auto differs = [&](const QJsonObject& b) { return error.isEmpty() ? b.contains("loadError") : b.value("loadError").toString() != error; };
  bool any = false;
  for (const QString& id : ids)
    if (const int j = indexOf(id); j >= 0 && differs(alerts_[j].toObject())) any = true;
  if (!any) return;   // written only when it changes, not at every check
  const auto lock = lockAndSync();
  bool changed = false;
  for (const QString& id : ids) {
    const int j = indexOf(id);
    if (j < 0) continue;
    QJsonObject b = alerts_[j].toObject();
    if (!differs(b)) continue;
    if (error.isEmpty()) b.remove("loadError");
    else b["loadError"] = error;
    alerts_[j] = b;
    changed = true;
  }
  if (changed) saveQuiet();
}

void Alerts::start(int intervalMs) {
  if (started_) return;
  started_ = true;
  interval_ = intervalMs;
  timer_.start(2000);
}

void Alerts::stop() {
  started_ = false;
  timer_.stop();
}

}  // namespace charts::alerts
