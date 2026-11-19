// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/market-info.js.
#include "charts/data/MarketInfo.h"
#include "charts/data/Store.h"
#include "net/Sources.h"
#include <QPointer>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimeZone>
#include <QHash>
#include <QUrl>
#include <memory>
#include <cmath>

namespace charts::data {

using sources::Access;
static const sources::Registrar kNasdaqApi({"nasdaq-api", "Nasdaq", {"api.nasdaq.com"}, Access::Keyless, 1000,
    "Earnings and dividend calendars, headlines by symbol, financial statements and key stats (US stocks)",
    "Public JSON API behind nasdaq.com's own pages, no key; not a documented API and Nasdaq's site terms are for "
    "personal, non-commercial use: one request a second at most, answers cached in memory (30 min to 6 h), shown "
    "with Nasdaq named as the source, never redistributed",
    {"charts/charts"}});
// api.nasdaq.com answers the headline request with a 301 to www.nasdaq.com (followed inside the request).
static const sources::Registrar kNasdaqWww({"nasdaq-rss", "Nasdaq", {"www.nasdaq.com"}, Access::Keyless, 30000,
    "Headlines by symbol (api.nasdaq.com's news request redirects here)", "Public RSS; robots.txt asks a 30 s crawl delay",
    {"charts/charts"}});
static const sources::Registrar kCointelegraph({"cointelegraph-rss", "Cointelegraph", {"cointelegraph.com"},
    Access::Keyless, 30000, "Crypto headlines RSS (cointelegraph.com/rss)",
    "Public RSS feed published for readers; robots.txt allows /rss; fetched at most every 10 min, headlines shown "
    "with the source name and a link back to the article",
    {"charts/charts"}});
static const sources::Registrar kDecrypt({"decrypt-rss", "Decrypt", {"decrypt.co"}, Access::Keyless, 30000,
    "Crypto headlines RSS (decrypt.co/feed)",
    "Public RSS feed published for readers; robots.txt allows everything; fetched at most every 10 min, headlines "
    "shown with the source name and a link back to the article",
    {"charts/charts"}});

namespace {
using RE = QRegularExpression;

bool offline() { return qEnvironmentVariable("QT_OFFLINE") == "1"; }

// ---------- JS value helpers ----------
bool truthy(const QJsonValue& v) {
  switch (v.type()) {
    case QJsonValue::Bool: return v.toBool();
    case QJsonValue::Double: { const double d = v.toDouble(); return d != 0 && !std::isnan(d); }
    case QJsonValue::String: return !v.toString().isEmpty();
    case QJsonValue::Array: case QJsonValue::Object: return true;
    default: return false;
  }
}
QJsonValue orEmpty(const QJsonValue& v) { return truthy(v) ? v : QJsonValue(""); }   // v || ""
QString jsString(const QJsonValue& v) {                                               // String(v), `${v}`
  switch (v.type()) {
    case QJsonValue::Undefined: return "undefined";
    case QJsonValue::Null: return "null";
    case QJsonValue::Bool: return v.toBool() ? "true" : "false";
    case QJsonValue::Double: return store::numberToString(v.toDouble());
    case QJsonValue::String: return v.toString();
    case QJsonValue::Object: return "[object Object]";
    case QJsonValue::Array: {
      QStringList parts;
      for (const auto& x : v.toArray()) parts << (x.isNull() || x.isUndefined() ? QString() : jsString(x));
      return parts.join(',');
    }
  }
  return {};
}
// num(s): parseFloat of the text without $ , % and spaces; null when it isn't a number.
QJsonValue num(const QJsonValue& s) {
  static const RE strip(R"([$,%\s])");
  QString t = s.isNull() || s.isUndefined() ? QString() : jsString(s);
  t.remove(strip);
  const double v = jsParseFloat(t);
  return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}
QJsonValue at(const QJsonValue& v, const char* k) { return v.isObject() ? v.toObject().value(QLatin1String(k)) : QJsonValue(QJsonValue::Undefined); }
QJsonValue msOrNull(double ms) { return std::isfinite(ms) && ms != 0 ? QJsonValue(ms) : QJsonValue(QJsonValue::Null); }   // Date.parse(x) || null


// getJson that also keeps the raw body (QJsonObject sorts keys; the JS shows Nasdaq's key order).
// failure -> empty body: every market-info call catches its errors (`.catch(() => null)`).
struct Raw { QByteArray body; QJsonDocument doc; };
void getRaw(const QString& url, std::function<void(const Raw&)> done) {
  net::Request r;
  r.url = QUrl(url);
  r.headers = {{"Accept", "application/json, text/plain, */*"}, {"Accept-Language", "en-US,en;q=0.9"}};
  r.headers << nqHeaders();   // the JS NQ: Origin / Referer of www.nasdaq.com
  r.staleOnError = false;
  r.timeoutMs = timeoutMs();
  // Same one-minute "down" mark as getJson: with Nasdaq unreachable, a 10-day calendar costs one timeout, not ten,
  // and doesn't hold up the US-stock charts queued behind it on api.nasdaq.com.
  const QString host = r.url.host();
  if (hostDown(host)) { later([done] { done({}); }); return; }
  skipIfDown(r, host);
  send(r, [done, host](const net::Reply& rep) {
    noteHostResult(host, rep);
    if (!rep.ok()) { done({}); return; }
    QJsonParseError err;
    const QJsonDocument d = QJsonDocument::fromJson(rep.body, &err);
    if (err.error != QJsonParseError::NoError) { done({}); return; }
    done({rep.body, d});
  });
}

template <class T>
std::function<void(const Result<T>&)> guardCb(QObject* ctx, std::function<void(const T&)> cb, T onError = T()) {
  QPointer<QObject> p(ctx);
  const bool has = ctx != nullptr;
  return [p, has, cb = std::move(cb), onError](const Result<T>& r) {
    if (has && !p) return;
    cb(r.ok() ? r.value : onError);
  };
}

// Promise.all over the next weekdays, each day cached, a failed day gives [].
void calendar(const char* kind, int days, QObject* ctx, ArrayCb cb,
              std::function<QJsonArray(const QJsonDocument&, const QString&)> rows) {
  QPointer<QObject> p(ctx);
  const bool has = ctx != nullptr;
  auto deliver = [p, has, cb](const QJsonArray& a) { if (!has || p) cb(a); };
  if (offline()) { later([deliver] { deliver({}); }); return; }
  const QStringList dates = weekdays(std::min(10, std::max(1, days)));
  auto perDay = std::make_shared<QVector<QJsonArray>>(dates.size());
  auto join = Join::make(int(dates.size()), [perDay, deliver] {
    QJsonArray all;
    for (const auto& d : *perDay) for (const auto& x : d) all.append(x);
    deliver(all);
  });
  for (int i = 0; i < dates.size(); ++i) {
    const QString date = dates[i];
    cached<QJsonArray>(
        QString("%1:%2").arg(kind, date), 30 * 60000,
        [kind, date, rows](Callback<QJsonArray> done) {
          getRaw(QString("https://api.nasdaq.com/api/calendar/%1?date=%2").arg(kind, date), [done, date, rows](const Raw& r) {
            if (r.body.isEmpty()) done(Result<QJsonArray>::failure(Error{QT_TRANSLATE_NOOP("charts", "request failed"), 0, true, {}}));
            else done(Result<QJsonArray>::success(rows(r.doc, date)));
          });
        },
        [perDay, join, i](const Result<QJsonArray>& r) {
          (*perDay)[i] = r.ok() ? r.value : QJsonArray();
          join->one();
        });
  }
}

const QHash<QString, QStringList>& coinNames() {
  static const QHash<QString, QStringList> m = {
      {"BTC", {"bitcoin", "btc"}}, {"ETH", {"ethereum", "ether", "eth"}}, {"SOL", {"solana", "sol"}},
      {"XRP", {"xrp", "ripple"}}, {"BNB", {"bnb", "binance coin"}}, {"DOGE", {"dogecoin", "doge"}},
      {"ADA", {"cardano", "ada"}}, {"AVAX", {"avalanche", "avax"}}, {"LINK", {"chainlink", "link"}},
      {"DOT", {"polkadot"}}, {"LTC", {"litecoin", "ltc"}}, {"TRX", {"tron", "trx"}}, {"TON", {"toncoin", "ton"}},
      {"SHIB", {"shiba inu", "shib"}}, {"PEPE", {"pepe"}}, {"SUI", {"sui"}}, {"NEAR", {"near protocol", "near"}},
      {"ZEC", {"zcash", "zec"}}};
  return m;
}

QString decode(QString s) {
  static const RE cdata(R"(<!\[CDATA\[|\]\]>)"), apos("&#0?39;|&apos;"), tags("<[^>]+>");
  s.remove(cdata);
  s.replace("&amp;", "&");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace(apos, "'");
  s.remove(tags);
  return s.trimmed();
}

// fundamentals: {periods, rows} of a Nasdaq table, null without headers or rows.
QJsonValue table(const QJsonValue& t, const QStringList& headerKeys) {
  if (!truthy(at(t, "headers")) || !truthy(at(t, "rows"))) return QJsonValue(QJsonValue::Null);
  const QJsonObject headers = at(t, "headers").toObject();
  QStringList keys;
  for (const QString& k : headerKeys) if (k != "value1") keys << k;
  QJsonArray periods;
  for (const QString& k : keys) periods.append(headers.value(k).isUndefined() ? QJsonValue(QJsonValue::Null) : headers.value(k));
  QJsonArray rows;
  for (const auto& rv : at(t, "rows").toArray()) {
    const QJsonObject r = rv.toObject();
    QJsonArray values;
    for (const QString& k : keys) {
      const QJsonValue v = r.value(k);
      values.append(v.isNull() || v.isUndefined() ? QJsonValue("") : v);   // r[k] ?? ""
    }
    QJsonObject row{{"values", values}};
    if (!r.value("value1").isUndefined()) row.insert("label", r.value("value1"));
    rows.append(row);
  }
  return QJsonObject{{"periods", periods}, {"rows", rows}};
}
}  // namespace

// ---------- JS helpers ----------
double jsParseFloat(const QString& s) {
  static const RE re(R"(^[+-]?(?:Infinity|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?))");
  int i = 0;
  while (i < s.size() && (s.at(i).isSpace() || s.at(i).unicode() == 0xFEFF)) ++i;
  const auto m = re.match(s.mid(i));
  if (!m.hasMatch()) return NAN;
  const QString t = m.captured(0);
  if (t.endsWith("Infinity")) return t.startsWith('-') ? -INFINITY : INFINITY;
  bool ok = false;
  const double v = t.toDouble(&ok);
  if (ok) return v;
  const int e = t.indexOf(RE("[eE]"));
  const bool neg = t.startsWith('-');
  return (e >= 0 && t.at(e + 1) != '-') ? (neg ? -INFINITY : INFINITY) : (neg ? -0.0 : 0.0);
}

double jsDateParse(const QString& raw) {
  const QString s = raw.trimmed();
  // ES date-time string format: date-only forms are UTC, date-times without an offset are local time.
  static const RE iso(R"(^([+-]\d{6}|\d{4})(?:-(\d{2})(?:-(\d{2}))?)?(?:T(\d{2}):(\d{2})(?::(\d{2})(?:\.(\d+))?)?(Z|[+-]\d{2}:\d{2})?)?$)");
  if (const auto m = iso.match(s); m.hasMatch()) {
    const int y = m.captured(1).toInt(), mo = m.captured(2).isEmpty() ? 1 : m.captured(2).toInt(),
              d = m.captured(3).isEmpty() ? 1 : m.captured(3).toInt();
    const bool hasTime = !m.captured(4).isEmpty();
    const int hh = m.captured(4).toInt(), mi = m.captured(5).toInt(), ss = m.captured(6).toInt();
    const int ms = m.captured(7).isEmpty() ? 0 : (m.captured(7) + "00").left(3).toInt();
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || hh > 24 || mi > 59 || ss > 59) return NAN;
    const QDate date(y, mo, d);
    if (!date.isValid()) return NAN;
    const QTime time = hh == 24 ? QTime(0, 0) : QTime(hh, mi, ss, ms);
    const QString zone = m.captured(8);
    QDateTime dt;
    if (!hasTime || zone == "Z") dt = QDateTime(date, time, QTimeZone::UTC);
    else if (!zone.isEmpty()) {
      const int off = (zone.at(0) == '-' ? -1 : 1) * (zone.mid(1, 2).toInt() * 3600 + zone.mid(4, 2).toInt() * 60);
      dt = QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(off));
    } else dt = QDateTime(date, time);
    if (hh == 24) dt = dt.addDays(1);
    return double(dt.toMSecsSinceEpoch());
  }
  // V8's legacy parser, for the forms feeds use: "Mon, 16 Nov 2026 13:31:49 +0000", "Nov 15, 2026",
  // "11/16/2026 10:00 PM EST". Unknown words before the first number are skipped.
  static const QStringList months = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
  static const QHash<QString, int> zones = {{"ut", 0}, {"utc", 0}, {"gmt", 0}, {"z", 0}, {"est", -5}, {"edt", -4},
                                            {"cst", -6}, {"cdt", -5}, {"mst", -7}, {"mdt", -6}, {"pst", -8}, {"pdt", -7}};
  static const RE tok(R"((\d+)|([A-Za-z]+)|([+-])|(:)|(\.)|(/)|(\()|(\))|[\s,]+|(.))");
  int month = -1, day = -1, year = -1, hh = -1, mi = 0, ss = 0, ms = 0, ampm = 0;
  bool hasZone = false, sawNumber = false;
  int offsetMin = 0;
  QStringList nums;
  QString str = s;
  str.remove(RE(R"(\([^)]*\))"));   // comments
  auto it = tok.globalMatch(str);
  QList<QRegularExpressionMatch> toks;
  while (it.hasNext()) {
    auto m = it.next();
    if (m.captured(0).trimmed().isEmpty() || m.captured(0).trimmed() == ",") continue;
    toks << m;
  }
  for (int i = 0; i < toks.size(); ++i) {
    const auto& m = toks[i];
    if (!m.captured(1).isEmpty()) {
      sawNumber = true;
      const QString n = m.captured(1);
      if (i + 1 < toks.size() && !toks[i + 1].captured(4).isEmpty() && hh < 0) {   // hh:mm[:ss[.ms]]
        hh = n.toInt();
        int j = i + 2;
        if (j < toks.size() && !toks[j].captured(1).isEmpty()) { mi = toks[j].captured(1).toInt(); ++j; } else return NAN;
        if (j + 1 < toks.size() && !toks[j].captured(4).isEmpty() && !toks[j + 1].captured(1).isEmpty()) {
          ss = toks[j + 1].captured(1).toInt(); j += 2;
          if (j + 1 < toks.size() && !toks[j].captured(5).isEmpty() && !toks[j + 1].captured(1).isEmpty()) {
            ms = (toks[j + 1].captured(1) + "00").left(3).toInt(); j += 2;
          }
        }
        i = j - 1;
        continue;
      }
      nums << n;
      continue;
    }
    if (!m.captured(2).isEmpty()) {
      const QString w = m.captured(2).toLower();
      if (w == "am" || w == "pm") { ampm = w == "am" ? 1 : 2; continue; }
      if (zones.contains(w)) { hasZone = true; offsetMin = zones.value(w) * 60; continue; }
      if (w == "t" && sawNumber) continue;
      if (w.size() >= 3 && months.contains(w.left(3))) { month = int(months.indexOf(w.left(3))); continue; }
      if (sawNumber) return NAN;   // an unknown word after the date started
      continue;                    // weekday names and other leading words
    }
    if (!m.captured(3).isEmpty()) {   // +hhmm / -hh:mm after a time or GMT
      if (i + 1 < toks.size() && !toks[i + 1].captured(1).isEmpty() && (hh >= 0 || hasZone)) {
        const QString n = toks[i + 1].captured(1);
        int h = 0, mm = 0;
        if (i + 3 < toks.size() && !toks[i + 2].captured(4).isEmpty()) { h = n.toInt(); mm = toks[i + 3].captured(1).toInt(); i += 3; }
        else { const int v = n.toInt(); if (n.size() <= 2) h = v; else { h = v / 100; mm = v % 100; } i += 1; }
        offsetMin = (m.captured(3) == "-" ? -1 : 1) * (h * 60 + mm);
        hasZone = true;
        continue;
      }
      continue;   // date separator "-"
    }
  }
  // Numbers left: with a month name they are day and year (a number over 31 or of 3+ digits is the year);
  // without, month/day/year, or year/month/day when the first has 3+ digits.
  if (month >= 0) {
    for (const QString& n : nums) {
      const int v = n.toInt();
      if (n.size() >= 3 || v > 31) { if (year < 0) year = v; else return NAN; }
      else if (day < 0) day = v;
      else if (year < 0) year = v;
      else return NAN;
    }
  } else if (nums.size() == 3) {
    if (nums[0].size() >= 3) { year = nums[0].toInt(); month = nums[1].toInt() - 1; day = nums[2].toInt(); }
    else { month = nums[0].toInt() - 1; day = nums[1].toInt(); year = nums[2].toInt(); }
  }
  if (month < 0 || day < 0 || year < 0) return NAN;
  if (year < 50) year += 2000; else if (year < 100) year += 1900;
  if (hh < 0) hh = 0;
  if (ampm == 2 && hh < 12) hh += 12;
  if (ampm == 1 && hh == 12) hh = 0;
  const QDate date(year, month + 1, day);
  if (!date.isValid() || hh > 24 || mi > 59 || ss > 59) return NAN;
  const QTime time(hh % 24, mi, ss, ms);
  QDateTime dt = hasZone ? QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(offsetMin * 60)) : QDateTime(date, time);
  if (hh == 24) dt = dt.addDays(1);
  return double(dt.toMSecsSinceEpoch());
}

QStringList jsonKeysInOrder(const QByteArray& json, const QStringList& path) {
  // A minimal JSON scanner: walks to the object at `path` (the last duplicate key wins, as JSON.parse) and
  // lists its keys; JS Object.keys puts integer-like keys first, ascending.
  const char* p = json.constData();
  const char* e = p + json.size();
  auto ws = [&] { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; };
  std::function<bool(QString*)> str = [&](QString* out) -> bool {
    if (p >= e || *p != '"') return false;
    ++p;
    QByteArray buf;
    while (p < e && *p != '"') {
      if (*p == '\\' && p + 1 < e) {
        ++p;
        switch (*p) {
          case 'n': buf += '\n'; break; case 't': buf += '\t'; break; case 'r': buf += '\r'; break;
          case 'b': buf += '\b'; break; case 'f': buf += '\f'; break;
          case 'u': {
            if (e - p < 5) return false;
            const ushort u = QByteArray(p + 1, 4).toUShort(nullptr, 16);
            buf += QString(QChar(u)).toUtf8();
            p += 4;
            break;
          }
          default: buf += *p;
        }
        ++p;
      } else buf += *p++;
    }
    if (p >= e) return false;
    ++p;
    if (out) *out = QString::fromUtf8(buf);
    return true;
  };
  std::function<bool(int, QStringList*)> value = [&](int depth, QStringList* found) -> bool {
    ws();
    if (p >= e) return false;
    if (*p == '"') return str(nullptr);
    if (*p == '[') {
      ++p; ws();
      if (p < e && *p == ']') { ++p; return true; }
      while (p < e) {
        if (!value(-1, nullptr)) return false;
        ws();
        if (p < e && *p == ',') { ++p; continue; }
        if (p < e && *p == ']') { ++p; return true; }
        return false;
      }
      return false;
    }
    if (*p == '{') {
      ++p; ws();
      QStringList keys;
      if (p < e && *p == '}') { ++p; if (depth == path.size() && found) *found = keys; return true; }
      while (p < e) {
        ws();
        QString k;
        if (!str(&k)) return false;
        ws();
        if (p >= e || *p != ':') return false;
        ++p;
        if (!keys.contains(k)) keys << k;
        const bool follow = depth >= 0 && depth < path.size() && k == path[depth];
        if (!value(follow ? depth + 1 : -1, follow ? found : nullptr)) return false;
        ws();
        if (p < e && *p == ',') { ++p; continue; }
        if (p < e && *p == '}') { ++p; if (depth == path.size() && found) *found = keys; return true; }
        return false;
      }
      return false;
    }
    while (p < e && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') ++p;
    return true;
  };
  QStringList found;
  value(0, &found);
  static const RE index("^(0|[1-9]\\d{0,9})$");
  QStringList ints, others;
  for (const QString& k : found)
    (index.match(k).hasMatch() && k.toULongLong() < 4294967295ULL ? ints : others) << k;
  std::sort(ints.begin(), ints.end(), [](const QString& a, const QString& b) { return a.toULongLong() < b.toULongLong(); });
  return ints + others;
}

// ---------- calendars ----------
QStringList weekdays(int n, QDate today) {
  QStringList out;
  QDate d = today.isValid() ? today : QDateTime::currentDateTimeUtc().date();
  while (out.size() < n) {
    if (d.dayOfWeek() != 6 && d.dayOfWeek() != 7) out << d.toString("yyyy-MM-dd");
    d = d.addDays(1);
  }
  return out;
}

QJsonArray earningsRows(const QJsonDocument& j, const QString& date) {
  const QJsonValue rows = at(at(j.object(), "data"), "rows");
  QJsonArray out;
  if (!rows.isArray()) return out;   // `|| []`, or .map on a non-array throws and the day gives []
  for (const auto& rv : rows.toArray()) {
    if (!rv.isObject()) return {};
    const QJsonObject r = rv.toObject();
    const QString time = r.value("time").toString();
    QJsonObject o{{"date", date}, {"symbol", "US:" + jsString(r.value("symbol"))},
                  {"when", time == "time-pre-market" ? QT_TRANSLATE_NOOP("charts", "Before the open") : time == "time-after-hours" ? QT_TRANSLATE_NOOP("charts", "After the close") : QT_TRANSLATE_NOOP("charts", "Time not given")},
                  {"marketCap", num(r.value("marketCap"))}, {"epsForecast", orEmpty(r.value("epsForecast"))},
                  {"quarter", orEmpty(r.value("fiscalQuarterEnding"))}, {"lastYearEps", orEmpty(r.value("lastYearEPS"))}};
    o.insert("ticker", r.value("symbol"));   // undefined drops the key, as JSON.stringify
    o.insert("name", r.value("name"));
    out.append(o);
  }
  return out;
}

QJsonArray dividendRows(const QJsonDocument& j, const QString& date) {
  const QJsonValue rows = at(at(at(j.object(), "data"), "calendar"), "rows");
  QJsonArray out;
  if (!rows.isArray()) return out;
  for (const auto& rv : rows.toArray()) {
    if (!rv.isObject()) return {};
    const QJsonObject r = rv.toObject();
    QJsonObject o{{"date", date}, {"symbol", "US:" + jsString(r.value("symbol"))}};
    o.insert("ticker", r.value("symbol"));
    o.insert("name", r.value("companyName"));
    o.insert("exDate", r.value("dividend_Ex_Date"));
    o.insert("payDate", r.value("payment_Date"));
    o.insert("recordDate", r.value("record_Date"));
    o.insert("amount", r.value("dividend_Rate"));
    o.insert("annual", r.value("indicated_Annual_Dividend"));
    out.append(o);
  }
  return out;
}

void earnings(int days, QObject* ctx, ArrayCb cb) { calendar("earnings", days, ctx, std::move(cb), earningsRows); }
void dividends(int days, QObject* ctx, ArrayCb cb) { calendar("dividends", days, ctx, std::move(cb), dividendRows); }

// ---------- news ----------
QJsonArray parseRss(const QString& xml, const QString& source) {
  static const RE item(R"(<item>([\s\S]*?)</item>)");
  QJsonArray items;
  auto it = item.globalMatch(xml);
  while (it.hasNext()) {
    const QString block = it.next().captured(1);
    auto pick = [&](const QString& tag) {
      const RE re(QString("<%1[^>]*>([\\s\\S]*?)<\\/%1>").arg(tag));
      return decode(re.match(block).captured(1));
    };
    const QString title = pick("title");
    if (title.isEmpty()) continue;
    // web links only: the side panel opens this when the card is clicked
    QString link = pick("link").trimmed();
    if (!link.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) && !link.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) link.clear();
    items.append(QJsonObject{{"title", title}, {"url", link}, {"source", source},
                             {"time", msOrNull(jsDateParse(pick("pubDate")))}, {"summary", pick("description").left(280)}});
  }
  return items;
}

void cryptoNews(QObject* ctx, ArrayCb cb) {
  cached<QJsonArray>(
      "news:crypto", 10 * 60000,
      [](Callback<QJsonArray> done) {
        const QList<QPair<QString, QString>> feeds = {{"https://cointelegraph.com/rss", "Cointelegraph"},
                                                      {"https://decrypt.co/feed", "Decrypt"}};
        auto lists = std::make_shared<QVector<QJsonArray>>(feeds.size());
        auto join = Join::make(int(feeds.size()), [lists, done] {
          QVector<QJsonValue> all;
          for (const auto& l : *lists) for (const auto& x : l) all << x;
          std::stable_sort(all.begin(), all.end(), [](const QJsonValue& a, const QJsonValue& b) {
            return a.toObject().value("time").toDouble(0) > b.toObject().value("time").toDouble(0);
          });
          QJsonArray out;
          for (const auto& x : all) out.append(x);
          done(Result<QJsonArray>::success(out));
        });
        for (int i = 0; i < feeds.size(); ++i) {
          net::Request r;
          r.url = QUrl(feeds[i].first);
          r.headers = {{"User-Agent", "Mozilla/5.0 Deepwater"}};
          r.staleOnError = false;
          r.timeoutMs = 10000;
          const QString name = feeds[i].second;
          send(r, [lists, join, i, name](const net::Reply& rep) {
            (*lists)[i] = rep.ok() ? parseRss(QString::fromUtf8(rep.body), name) : QJsonArray();
            join->one();
          });
        }
      },
      guardCb<QJsonArray>(ctx, std::move(cb)));
}

QString coinBase(const QString& ticker) {
  QString t = ticker;
  t.remove(RE(R"(-|\.P$)"));
  t.remove(RE("(USDT|USDC|FDUSD|USD|BTC|ETH|EUR)$"));
  t.replace(RE("^XBT$"), "BTC");
  return t.isEmpty() ? QStringLiteral("BTC") : t;
}

QJsonObject cryptoHeadlines(const QString& base, const QJsonArray& all) {
  const QStringList words = coinNames().contains(base) ? coinNames().value(base) : QStringList{base.toLower()};
  QStringList esc;
  for (const QString& w : words) esc << RE::escape(w);
  const RE re(QString("\\b(%1)\\b").arg(esc.join('|')), RE::CaseInsensitiveOption);
  QJsonArray hits;
  for (const auto& n : all) {
    const QJsonObject o = n.toObject();
    if (re.match(jsString(o.value("title")) + " " + jsString(o.value("summary"))).hasMatch()) hits.append(n);
  }
  auto first30 = [](const QJsonArray& a) { QJsonArray o; for (int i = 0; i < a.size() && i < 30; ++i) o.append(a.at(i)); return o; };
  return hits.size() >= 3
             ? QJsonObject{{"items", first30(hits)}, {"note", "Headlines mentioning " + base}}
             : QJsonObject{{"items", first30(all)},
                           {"note", QString("Few headlines mention %1 right now, so these are the latest crypto headlines").arg(base)}};
}

QJsonArray nasdaqNewsRows(const QJsonDocument& j) {
  const QJsonValue rows = at(at(j.object(), "data"), "rows");
  QJsonArray out;
  if (!rows.isArray()) return out;
  for (const auto& rv : rows.toArray()) {
    const QJsonObject r = rv.toObject();
    const QJsonValue url = r.value("url");
    QJsonObject o{{"url", url.isString() && url.toString().startsWith("http") ? url.toString() : "https://www.nasdaq.com" + jsString(url)},
                  {"time", msOrNull(r.value("created").isString() ? jsDateParse(r.value("created").toString()) : NAN)},
                  {"summary", orEmpty(r.value("description"))}};
    o.insert("title", r.value("title"));
    o.insert("source", r.value("publisher"));
    o.insert("ago", r.value("ago"));
    out.append(o);
  }
  return out;
}

void news(const Symbol& sym, QObject* ctx, ObjectCb cb) {
  QPointer<QObject> p(ctx);
  const bool has = ctx != nullptr;
  auto deliver = [p, has, cb](const QJsonObject& o) { if (!has || p) cb(o); };
  if (offline()) { later([deliver] { deliver({{"items", QJsonArray()}, {"note", ""}}); }); return; }
  if (sym.exchange == "BINANCE" || sym.exchange == "COINBASE" || sym.exchange == "KRAKEN") {
    const QString base = coinBase(sym.ticker);
    cryptoNews(nullptr, [deliver, base](const QJsonArray& all) { deliver(cryptoHeadlines(base, all)); });
    return;
  }
  if (sym.exchange == "US") {
    const QString ticker = sym.ticker;
    cached<QJsonArray>(
        "news:" + ticker, 10 * 60000,
        [ticker](Callback<QJsonArray> done) {
          // stocks, then etf, then index: the first class with headlines wins (one request after the other)
          auto step = std::make_shared<std::function<void(int)>>();
          std::weak_ptr<std::function<void(int)>> weak = step;
          *step = [ticker, done, weak](int k) {
            static const QStringList classes = {"stocks", "etf", "index"};
            auto self = weak.lock();
            if (!self) return;
            if (k >= classes.size()) { done(Result<QJsonArray>::success({})); return; }
            const QString q = encodeUriComponent(ticker.toLower() + "|" + classes[k]);
            getRaw(QString("https://api.nasdaq.com/api/news/topic/articlebysymbol?q=%1&offset=0&limit=30&fallback=true").arg(q),
                   [done, self, k](const Raw& r) {
                     const QJsonArray items = r.body.isEmpty() ? QJsonArray() : nasdaqNewsRows(r.doc);
                     if (!items.isEmpty()) done(Result<QJsonArray>::success(items));
                     else (*self)(k + 1);
                   });
          };
          (*step)(0);
        },
        [deliver, ticker](const Result<QJsonArray>& r) {
          const QJsonArray items = r.ok() ? r.value : QJsonArray();
          deliver({{"items", items},
                   {"note", items.isEmpty() ? "No headlines found for " + ticker : QString("Headlines for %1 from Nasdaq").arg(ticker)}});
        });
    return;
  }
  later([deliver] { deliver({{"items", QJsonArray()}, {"note", QT_TRANSLATE_NOOP("charts", "Headlines are available for crypto and US stocks.")}}); });
}

// ---------- fundamentals ----------
Result<QJsonValue> financialsFrom(const Symbol& sym, const QString& frequency, const QByteArray& finBody,
                                  const QByteArray& sumBody, const QByteArray& edBody) {
  auto parse = [](const QByteArray& b) {
    if (b.isEmpty()) return QJsonValue(QJsonValue::Null);
    QJsonParseError err;
    const QJsonDocument d = QJsonDocument::fromJson(b, &err);
    if (err.error != QJsonParseError::NoError) return QJsonValue(QJsonValue::Null);
    return d.isObject() ? QJsonValue(d.object()) : QJsonValue(d.array());
  };
  const QJsonValue fin = parse(finBody), sum = parse(sumBody), ed = parse(edBody);
  const QJsonValue d = truthy(at(fin, "data")) ? at(fin, "data") : QJsonValue(QJsonObject());
  // stats in Nasdaq's order (QJsonObject would sort them by key)
  QJsonArray stats;
  const QJsonValue sd = at(at(sum, "data"), "summaryData");
  if (sd.isObject()) {
    const QJsonObject o = sd.toObject();
    for (const QString& k : jsonKeysInOrder(sumBody, {"data", "summaryData"})) {
      const QJsonValue x = o.value(k);
      const QJsonValue value = at(x, "value");
      if (!truthy(value) || value == QJsonValue("N/A")) continue;
      QJsonObject s;
      s.insert("label", at(x, "label"));
      s.insert("value", value);
      stats.append(s);
    }
  }
  static const RE spaces("\\s+");
  QString next = truthy(at(at(ed, "data"), "reportText")) ? jsString(at(at(ed, "data"), "reportText")) : QString();
  next = next.replace(spaces, " ").trimmed();
  auto tbl = [&](const char* name) { return table(at(d, name), jsonKeysInOrder(finBody, {"data", name, "headers"})); };
  QJsonObject out{{"symbol", sym.id}, {"frequency", frequency}, {"stats", stats}, {"nextEarnings", next},
                  {"income", tbl("incomeStatementTable")}, {"balance", tbl("balanceSheetTable")},
                  {"cashflow", tbl("cashFlowTable")}, {"ratios", tbl("financialRatiosTable")},
                  {"note", QT_TRANSLATE_NOOP("charts", "Statement figures are in thousands of US dollars, as Nasdaq reports them.")}};
  if (out.value("income").isNull() && stats.isEmpty())
    return Result<QJsonValue>::failure(
        Error{QString("Nasdaq has no financials for %1 (ETFs and funds don't file them).").arg(sym.ticker), 404, false, {}});
  return Result<QJsonValue>::success(out);
}

void financials(const Symbol& sym, const QString& frequency, QObject* ctx, Callback<QJsonValue> cb) {
  cb = guarded<QJsonValue>(ctx, std::move(cb));
  if (offline()) { later([cb] { cb(Result<QJsonValue>::success(QJsonValue(QJsonValue::Null))); }); return; }
  if (sym.exchange != "US") {
    later([cb] { cb(Result<QJsonValue>::failure(Error{QT_TRANSLATE_NOOP("charts", "Financial statements are for US-listed stocks."), 422, false, {}})); });
    return;
  }
  const int f = frequency == "quarterly" ? 2 : 1;
  cached<QJsonValue>(
      QString("fin:%1:%2").arg(sym.ticker).arg(f), 6 * 3600000LL,
      [sym, frequency, f](Callback<QJsonValue> done) {
        auto bodies = std::make_shared<QVector<QByteArray>>(3);
        auto join = Join::make(3, [sym, frequency, bodies, done] {
          done(financialsFrom(sym, frequency, (*bodies)[0], (*bodies)[1], (*bodies)[2]));
        });
        // the ticker comes from the user or the API: encoded, so "A/../../X" stays one path segment
        const QString t = QString::fromLatin1(QUrl::toPercentEncoding(sym.ticker));
        const QStringList urls = {
            QString("https://api.nasdaq.com/api/company/%1/financials?frequency=%2").arg(t).arg(f),
            QString("https://api.nasdaq.com/api/quote/%1/summary?assetclass=stocks").arg(t),
            QString("https://api.nasdaq.com/api/analyst/%1/earnings-date").arg(t)};
        for (int i = 0; i < 3; ++i)
          getRaw(urls[i], [bodies, join, i](const Raw& r) { (*bodies)[i] = r.body; join->one(); });
      },
      cb);
}

void news(const QString& symbol, QObject* ctx, Callback<QJsonObject> cb) {
  cb = guarded<QJsonObject>(ctx, std::move(cb));
  if (offline()) { later([cb] { cb(Result<QJsonObject>::success({{"items", QJsonArray()}, {"note", ""}})); }); return; }
  resolve(symbol, nullptr, [cb](const Result<Symbol>& r) {
    if (!r.ok()) { cb(Result<QJsonObject>::failure(*r.error)); return; }
    news(r.value, nullptr, [cb](const QJsonObject& o) { cb(Result<QJsonObject>::success(o)); });
  });
}

void financials(const QString& symbol, const QString& frequency, QObject* ctx, Callback<QJsonValue> cb) {
  cb = guarded<QJsonValue>(ctx, std::move(cb));
  if (offline()) { later([cb] { cb(Result<QJsonValue>::success(QJsonValue(QJsonValue::Null))); }); return; }
  resolve(symbol, nullptr, [cb, frequency](const Result<Symbol>& r) {
    if (!r.ok()) { cb(Result<QJsonValue>::failure(*r.error)); return; }
    financials(r.value, frequency, nullptr, cb);
  });
}

}  // namespace charts::data
