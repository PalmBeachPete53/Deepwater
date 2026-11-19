#include "account/Importers.h"
#include <QCoreApplication>
#include "account/Contracts.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextDocumentFragment>
#include <QHash>
#include <QTimeZone>
#include <QCryptographicHash>
#include <algorithm>

namespace account {

// ---------------------------------------------------------------- helpers
namespace {
// "1,234,567" / "157,250": every group after the first has exactly 3 digits, the first 1 to 3 digits without a
// leading zero ("0,500" is a decimal), optional sign in front.
bool thousandsGroups(const QString& s, QChar sep) {
  QString body = s;
  if (body.startsWith('-') || body.startsWith('+')) body.remove(0, 1);
  const QStringList g = body.split(sep);
  if (g.size() < 2 || g[0].isEmpty() || g[0].size() > 3 || g[0][0] == '0') return false;
  for (const auto& part : g) for (QChar ch : part) if (!ch.isDigit()) return false;
  for (int i = 1; i < g.size(); ++i) if (g[i].size() != 3) return false;
  return true;
}
}

double parseNumber(QString s, bool decimalComma) {
  s = s.trimmed();
  if (s.isEmpty() || s == "-" || s == "—") return NAN;
  bool neg = false;
  if (s.startsWith('(') && s.endsWith(')')) { neg = true; s = s.mid(1, s.size() - 2); }
  s.replace(QChar(0x2212), '-');                         // unicode minus
  static const QRegularExpression spaces("[\\s\\x{00A0}\\x{202F}$€£¥']"), suffix("(?i)(lots?|units?|usd|eur|gbp|jpy|chf|cad|aud|nzd)$");
  s.remove(spaces);
  s.remove(suffix);
  const int lc = s.lastIndexOf(','), ld = s.lastIndexOf('.');
  if (lc >= 0 && ld >= 0) {                              // both: the last one is the decimal mark
    if (lc > ld) { s.remove('.'); s.replace(',', '.'); } else s.remove(',');
  } else if (lc >= 0) {                                  // only commas (rule in Importers.h)
    if (s.count(',') > 1) { if (!thousandsGroups(s, ',')) return NAN; s.remove(','); }
    else if (!decimalComma && thousandsGroups(s, ',')) s.remove(',');
    else s.replace(',', '.');
  } else if (ld >= 0 && decimalComma && s.count('.') > 1 && thousandsGroups(s, '.')) {
    s.remove('.');                                       // "1.234.567" where the comma is the decimal mark
  }
  bool ok = false;
  const double v = s.toDouble(&ok);
  return ok ? (neg ? -v : v) : NAN;
}

QDateTime parseTime(const QString& in, int off, bool dayFirst) {
  QString s = in.trimmed();
  if (s.isEmpty()) return {};
  bool numeric = false;
  const qint64 epoch = s.toLongLong(&numeric);
  if (numeric && epoch > 100000000) return QDateTime::fromSecsSinceEpoch(epoch > 100000000000LL ? epoch / 1000 : epoch, QTimeZone::UTC);
  // ISO with zone
  static const QRegularExpression zone("[+-]\\d\\d:?\\d\\d$"), blanks("\\s+");
  if (s.contains('T') && (s.endsWith('Z') || s.contains(zone))) {
    auto t = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!t.isValid()) t = QDateTime::fromString(s, Qt::ISODate);
    if (t.isValid()) return t.toUTC();
  }
  s.replace('T', ' ');
  // milliseconds, only after a time with seconds ("10:20:30.123"): "16.11.2026" and "2026.11.16" are dates
  static const QRegularExpression ms("(:\\d{2}:\\d{2})\\.\\d{1,6}$");
  s.replace(ms, "\\1");
  s.replace(blanks, " ");
  static const QRegularExpression ymd(R"(^(\d{4})[.\-/](\d{1,2})[.\-/](\d{1,2})(?: (\d{1,2}):(\d{2})(?::(\d{2}))?)?$)");
  static const QRegularExpression dmy(R"(^(\d{1,2})[.\-/](\d{1,2})[.\-/](\d{2,4})(?: (\d{1,2}):(\d{2})(?::(\d{2}))?)?$)");
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
  if (auto m = ymd.match(s); m.hasMatch()) {
    y = m.captured(1).toInt(); mo = m.captured(2).toInt(); d = m.captured(3).toInt();
    h = m.captured(4).toInt(); mi = m.captured(5).toInt(); se = m.captured(6).toInt();
  } else if (auto m2 = dmy.match(s); m2.hasMatch()) {
    int a = m2.captured(1).toInt(), b = m2.captured(2).toInt();
    y = m2.captured(3).toInt(); if (y < 100) y += 2000;
    if (a > 12) dayFirst = true; else if (b > 12) dayFirst = false;
    d = dayFirst ? a : b; mo = dayFirst ? b : a;
    h = m2.captured(4).toInt(); mi = m2.captured(5).toInt(); se = m2.captured(6).toInt();
  } else {
    // "14 Oct 2026 10:20" and other English forms
    for (const char* f : {"d MMM yyyy HH:mm:ss", "d MMM yyyy HH:mm", "d MMM yyyy, HH:mm", "MMM d, yyyy HH:mm:ss", "MMM d yyyy HH:mm"}) {
      QDateTime t = QLocale(QLocale::English).toDateTime(s, f);
      if (t.isValid()) { t.setTimeZone(QTimeZone::UTC); return t.addSecs(-qint64(off) * 60); }
    }
    return {};
  }
  const QDate date(y, mo, d);
  const QTime time(h, mi, se);
  if (!date.isValid() || !time.isValid()) return {};
  return QDateTime(date, time, QTimeZone::UTC).addSecs(-qint64(off) * 60);
}

namespace {
QString cellText(QString html) {
  static const QRegularExpression br("(?i)<br\\s*/?>"), tag("<[^>]*>");
  if (html.contains('<')) { html.replace(br, " "); html.remove(tag); }
  // entities (&nbsp; &amp; &#160;) need the HTML parser; plain cells (almost all of them) don't: a 20,000-trade
  // statement has 300,000 cells
  QString t = html.contains('&') ? QTextDocumentFragment::fromHtml(html).toPlainText() : html;
  t.replace(QChar(0x00A0), ' ');
  return t.simplified();
}

// Column roles shared by the HTML and CSV readers.
enum Role { None, Ticket, Symbol, Type, Direction, OpenTime, CloseTime, Time, Entry, Exit, Price, Lots, Units, SL, TP,
            Commission, Fee, Swap, Profit, Net, Strategy, Comment, DealCol, BalanceCol, MarketPrice };
Role roleOf(QString h) {
  h = h.toLower();
  h.replace('_', ' ');
  h = h.simplified();
  static const QRegularExpression paren("\\(.*\\)"), money("[$€£]");
  h.remove(paren);                                       // "Opening time (UTC+0)" → "opening time"
  h.remove(money);
  h = h.trimmed();
  static const QHash<QString, Role> m{
    {"ticket", Ticket}, {"position", Ticket}, {"position id", Ticket}, {"id", Ticket}, {"order", None}, {"deal", DealCol},
    {"trade id", Ticket}, {"symbol", Symbol}, {"item", Symbol}, {"instrument", Symbol}, {"pair", Symbol}, {"market", Symbol},
    {"type", Type}, {"side", Type}, {"opening direction", Type}, {"direction", Direction}, {"action", Type}, {"trade type", Type},
    {"open time", OpenTime}, {"opening time", OpenTime}, {"open date", OpenTime}, {"entry time", OpenTime}, {"time open", OpenTime},
    {"opened", OpenTime}, {"open", OpenTime}, {"entry date", OpenTime}, {"date open", OpenTime}, {"opening date", OpenTime},
    {"date close", CloseTime}, {"closing date", CloseTime}, {"date closed", CloseTime}, {"date opened", OpenTime},
    {"close time", CloseTime}, {"closing time", CloseTime}, {"close date", CloseTime}, {"exit time", CloseTime},
    {"time close", CloseTime}, {"closed", CloseTime}, {"exit date", CloseTime}, {"time", Time},
    {"entry", Entry}, {"entry price", Entry}, {"open price", Entry}, {"opening price", Entry}, {"price open", Entry},
    {"exit", Exit}, {"exit price", Exit}, {"close price", Exit}, {"closing price", Exit}, {"price close", Exit}, {"price", Price},
    {"size", Lots}, {"volume", Lots}, {"lots", Lots}, {"lot", Lots}, {"quantity", Lots}, {"closing quantity", Lots},
    {"qty", Lots}, {"volume lots", Lots}, {"units", Units}, {"amount", Units},
    {"s / l", SL}, {"s/l", SL}, {"sl", SL}, {"stop", SL}, {"stop loss", SL},
    {"t / p", TP}, {"t/p", TP}, {"tp", TP}, {"target", TP}, {"take profit", TP},
    {"commission", Commission}, {"commissions", Commission}, {"fee", Fee}, {"fees", Fee}, {"taxes", Fee}, {"swap", Swap},
    {"swaps", Swap}, {"rollover", Swap},
    {"profit", Profit}, {"gross", Profit}, {"gross profit", Profit}, {"gross usd", Profit}, {"p&l", Profit}, {"pnl", Profit},
    {"p/l", Profit}, {"result", Profit}, {"net", Net}, {"net profit", Net}, {"net usd", Net},
    {"strategy", Strategy}, {"setup", Strategy}, {"tag", Strategy}, {"comment", Comment}, {"comments", Comment}, {"label", Comment},
    {"balance", BalanceCol}, {"market price", MarketPrice}};
  if (auto it = m.constFind(h); it != m.cend()) return *it;
  if (h.startsWith("gross")) return Profit;
  if (h.startsWith("net")) return Net;
  if (h.startsWith("balance")) return BalanceCol;
  return None;
}

struct Mapping {
  QHash<Role, int> col;
  bool has(Role r) const { return col.contains(r); }
  int at(Role r) const { return col.value(r, -1); }
};
Mapping mapHeader(const QStringList& cells) {
  Mapping m;
  for (int i = 0; i < cells.size(); ++i) {
    Role r = roleOf(cells[i]);
    if (r == Time) r = m.has(OpenTime) ? CloseTime : OpenTime;        // MT5: Time … Time
    if (r == Price) r = m.has(Entry) ? Exit : Entry;                   // MT4/MT5: Price … Price
    if (r == None || m.has(r)) continue;
    m.col.insert(r, i);
  }
  return m;
}

// Builds a trade from one row; returns false when the row is not a closed buy/sell.
bool rowToTrade(const QStringList& c, const Mapping& m, int off, bool dayFirst, bool decComma, Trade& t, QString* why) {
  auto get = [&](Role r) { const int i = m.at(r); return i >= 0 && i < c.size() ? c[i] : QString(); };
  QString type = get(Type).toLower();
  if (type.isEmpty() && m.has(Direction)) type = get(Direction).toLower();
  static const QRegularExpression notLetter("[^a-z ]"), letters("[A-Za-z]+");
  type.remove(notLetter);
  type = type.simplified();
  if (type == "buy" || type == "long" || type == "b") t.side = Side::Long;
  else if (type == "sell" || type == "short" || type == "s") t.side = Side::Short;
  else return false;
  t.symbol = get(Symbol).trimmed();
  t.open = parseTime(get(OpenTime), off, dayFirst);
  t.close = parseTime(get(CloseTime), off, dayFirst);
  t.entry = parseNumber(get(Entry), decComma);
  t.exit = parseNumber(get(Exit), decComma);
  {
    // "0.5", "0.5 / 0.5" (MT5), "0.5 Lots", "7 Oz" / "50 000 Units" (cTrader quantities in units)
    const QString raw = get(Lots).section('/', 0, 0).trimmed();
    t.lots = parseNumber(QString(raw).remove(letters), decComma);
    if (raw.contains(letters) && !raw.contains("lot", Qt::CaseInsensitive) && std::isfinite(t.lots)) {
      const double cs = spec(get(Symbol)).contract;
      if (cs > 0) t.lots /= cs;
    }
  }
  t.sl = parseNumber(get(SL), decComma); if (t.sl == 0) t.sl = NAN;
  t.tp = parseNumber(get(TP), decComma); if (t.tp == 0) t.tp = NAN;
  const double comm = parseNumber(get(Commission), decComma), fee = parseNumber(get(Fee), decComma);
  const double swap = parseNumber(get(Swap), decComma);
  t.commission = (std::isfinite(comm) ? comm : 0) + (std::isfinite(fee) ? fee : 0);
  t.swap = std::isfinite(swap) ? swap : 0;
  double pnl = parseNumber(get(Profit), decComma);
  if (!std::isfinite(pnl) && m.has(Net)) {                      // only a net figure: keep it whole
    pnl = parseNumber(get(Net), decComma); t.commission = 0; t.swap = 0;
  }
  t.pnl = pnl;
  t.ticket = get(Ticket).trimmed();
  t.strategy = get(Strategy).trimmed();
  t.comment = get(Comment).trimmed();
  if (t.symbol.isEmpty() || !t.close.isValid() || !std::isfinite(t.pnl)) {
    if (why) *why = t.symbol.isEmpty() ? "no symbol" : !t.close.isValid() ? "no close time" : "no profit";
    return false;
  }
  if (!t.open.isValid()) t.open = t.close;
  return true;
}

// Id of a trade without a ticket, from the row as written in the file (raw times, prices, size, profit): it does
// not depend on the time offset chosen at import, so importing the same file again with another offset finds
// the same trades (A7). Identical rows (two real trades opened and closed in the same second for the same
// profit) get an occurrence number, so they are not merged: "csv:<hash>", "csv:<hash>-2".
QString rowKey(const QStringList& c, const Mapping& m) {
  QStringList parts;
  for (Role r : {Symbol, Type, Direction, OpenTime, CloseTime, Entry, Exit, Lots, Units, Profit, Net}) {
    const int i = m.at(r);
    parts << (i >= 0 ? c.value(i).trimmed() : QString());
  }
  return QString::fromLatin1(QCryptographicHash::hash(parts.join(QChar(0x1f)).toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}
QString occurrenceId(const QString& source, const QString& key, QHash<QString, int>& seen) {
  const int n = ++seen[key];
  return source + ":" + key + (n > 1 ? "-" + QString::number(n) : QString());
}

QString decode(const QByteArray& data) {
  if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF") || (data.size() > 4 && data[1] == 0 && data[3] == 0)) {
    QStringDecoder d(data.startsWith("\xFE\xFF") ? QStringConverter::Utf16BE : QStringConverter::Utf16LE);
    return d(data);
  }
  QStringDecoder u(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
  QString s = u(data);
  if (u.hasError()) s = QString::fromLatin1(data);
  if (s.startsWith(QChar(0xFEFF))) s.remove(0, 1);
  return s;
}
}

// ---------------------------------------------------------------- HTML (MT4 / MT5)
ImportResult parseHtmlStatement(const QString& html, int off) {
  ImportResult r;
  const bool mt5 = html.contains("Trade History Report", Qt::CaseInsensitive) || html.contains(">Positions<", Qt::CaseInsensitive)
                   || (html.contains("Deals", Qt::CaseSensitive) && html.contains("Direction"));
  r.format = mt5 ? QT_TRANSLATE_NOOP("account", "MT5 report") : QT_TRANSLATE_NOOP("account", "MT4 detailed statement");
  r.platform = mt5 ? "MT5" : "MT4";
  static const QRegularExpression trRe("<tr[^>]*>(.*?)</tr>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression tdRe("<t([dh])([^>]*)>(.*?)</t[dh]>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression spanRe("colspan\\s*=\\s*\"?(\\d+)", QRegularExpression::CaseInsensitiveOption);

  enum Mode { Off, Trades, Deals, Balance } mode = Off;
  Mapping map;
  double deposits = 0; bool sawDeposit = false;
  QHash<QString, int> seenRows;
  r.offsetMin = off; r.offsetFromUser = true;
  for (auto it = trRe.globalMatch(html); it.hasNext();) {
    const auto row = it.next();
    QStringList cells;
    for (auto ct = tdRe.globalMatch(row.captured(1)); ct.hasNext();) {
      const auto cm = ct.next();
      const QString attrs = cm.captured(2);
      if (attrs.contains("hidden", Qt::CaseInsensitive)) continue;
      const QString text = cellText(cm.captured(3));
      int span = 1;
      if (auto sm = spanRe.match(attrs); sm.hasMatch()) span = std::clamp(sm.captured(1).toInt(), 1, 30);
      cells << text;
      for (int k = 1; k < span; ++k) cells << QString();
    }
    QStringList nonEmpty; for (const auto& c : cells) if (!c.isEmpty()) nonEmpty << c;
    if (nonEmpty.isEmpty()) continue;
    const QString joined = nonEmpty.join(' ');
    // account header
    if (r.login.isEmpty()) {
      static const QRegularExpression acc("Account:\\s*(\\d+)"), cur("Currency:\\s*([A-Z]{3})"), cur5("\\(([A-Z]{3}),");
      if (auto m = acc.match(joined); m.hasMatch()) {
        r.login = m.captured(1);
        if (auto c5 = cur5.match(joined); c5.hasMatch()) r.currency = c5.captured(1);
      }
      else if (nonEmpty.size() >= 2 && nonEmpty[0] == "Account:" ) {
        r.login = nonEmpty[1].section(' ', 0, 0);
        if (auto c5 = cur5.match(nonEmpty[1]); c5.hasMatch()) r.currency = c5.captured(1);
      }
      if (auto m = cur.match(joined); m.hasMatch()) r.currency = m.captured(1);
      static const QRegularExpression nm("Name:\\s*(.+?)(?:\\s+Account:|\\s+Currency:|$)");
      if (auto m = nm.match(joined); m.hasMatch()) r.name = m.captured(1).trimmed();
    }
    if (nonEmpty.size() >= 2 && nonEmpty[0] == "Company:") r.broker = nonEmpty[1];
    if (nonEmpty.size() >= 2 && nonEmpty[0] == "Name:" && r.name.isEmpty()) r.name = nonEmpty[1];
    if (r.currency.isEmpty() && nonEmpty.size() >= 2 && nonEmpty[0] == "Currency:") r.currency = nonEmpty[1];

    // section titles
    if (nonEmpty.size() == 1) {
      const QString t = nonEmpty[0].toLower();
      if (t.startsWith("open trades") || t.startsWith("working orders") || t.startsWith("open positions") ||
          t.startsWith("orders") || t.startsWith("summary") || t.startsWith("results") || t.startsWith("details"))
        { mode = Off; continue; }
      if (t.startsWith("closed transactions") || t.startsWith("positions")) { mode = Off; continue; }
      if (t.startsWith("deals")) { mode = Off; continue; }
    }
    // header rows
    int roles = 0; for (const auto& c : cells) if (roleOf(c) != None) ++roles;
    if (roles >= 5) {
      Mapping m = mapHeader(cells);
      if (m.has(DealCol) || m.has(Direction)) { map = m; mode = Deals; continue; }
      if (m.has(Profit) && m.has(Symbol) && m.has(CloseTime) && !m.has(MarketPrice)) { map = m; mode = Trades; continue; }
      mode = Off; continue;
    }
    // balance rows (deposits / withdrawals) in MT4 closed section and MT5 deals
    QString lowerCells = cells.join('|').toLower();
    const bool isBalance = cells.size() >= 3 && (cells.value(2).toLower() == "balance" || cells.value(3).toLower() == "balance");
    if (isBalance && mode != Off) {
      double amt = NAN;
      if (mode == Deals && map.has(Profit)) amt = parseNumber(cells.value(map.at(Profit)));
      if (!std::isfinite(amt)) for (int i = cells.size() - 1; i >= 0; --i) { amt = parseNumber(cells[i]); if (std::isfinite(amt)) break; }
      if (std::isfinite(amt)) { deposits += amt; sawDeposit = true; }
      continue;
    }
    if (mode != Trades) continue;
    Trade t; QString why;
    if (rowToTrade(cells, map, off, false, false, t, &why)) {
      t.source = mt5 ? "mt5" : "mt4";
      if (t.ticket.isEmpty()) {
        t.id = occurrenceId(t.source, rowKey(cells, map), seenRows);
        t.legacyId = t.source + ":" + QString::number(t.close.toSecsSinceEpoch()) + t.symbol;   // ids before 2026-11-16
      } else t.id = t.source + ":" + t.ticket;
      r.trades << t;
    } else if (lowerCells.contains("buy") || lowerCells.contains("sell")) {
      if (!lowerCells.contains("cancel") && !lowerCells.contains("limit") && !lowerCells.contains("stop")) ++r.skipped;
    }
  }
  if (sawDeposit) r.deposits = deposits;
  if (r.trades.isEmpty()) r.error = QCoreApplication::translate("account", "No closed trades found. Export the full history (MT4: Account History › right click › "
                                    "All History, then Save as Detailed Report; MT5: History › right click › Report › HTML).");
  return r;
}

// ---------------------------------------------------------------- CSV
static QList<QStringList> splitCsv(const QString& text, QChar sep) {
  QList<QStringList> rows; QStringList cur; QString field; bool q = false;
  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text[i];
    if (q) {
      if (ch == '"') { if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; } else q = false; }
      else field += ch;
    } else if (ch == '"') q = true;
    else if (ch == sep) { cur << field.trimmed(); field.clear(); }
    else if (ch == '\n' || ch == '\r') {
      if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
      cur << field.trimmed(); field.clear();
      if (!(cur.size() == 1 && cur[0].isEmpty())) rows << cur;
      cur.clear();
    } else field += ch;
  }
  if (!field.isEmpty() || !cur.isEmpty()) { cur << field.trimmed(); rows << cur; }
  return rows;
}

ImportResult parseCsv(const QString& text, int off) {
  ImportResult r;
  const QString first = text.section('\n', 0, 0);
  QChar sep = ',';
  if (first.count(';') > first.count(sep)) sep = ';';
  if (first.count('\t') > first.count(sep)) sep = '\t';
  const auto rows = splitCsv(text, sep);
  int h = -1; Mapping map;
  for (int i = 0; i < std::min<int>(rows.size(), 15); ++i) {
    Mapping m = mapHeader(rows[i]);
    if (m.has(Symbol) && (m.has(Profit) || m.has(Net)) && (m.has(CloseTime) || m.has(OpenTime))) { h = i; map = m; break; }
  }
  if (h < 0) { r.error = QCoreApplication::translate("account", "No header row with symbol, time and profit columns."); return r; }
  const QString hdr = rows[h].join(' ').toLower();
  const bool ctrader = hdr.contains("opening direction") || hdr.contains("closing quantity") || hdr.contains("opening time");
  r.format = ctrader ? QT_TRANSLATE_NOOP("account", "cTrader CSV") : QT_TRANSLATE_NOOP("account", "CSV");
  r.platform = ctrader ? "cTrader" : "";
  if (!map.has(CloseTime)) { map.col.insert(CloseTime, map.at(OpenTime)); r.warnings << QCoreApplication::translate("account", "No close time column: open time used."); }
  // the time zone written in the column titles wins over the one chosen by the user ("Closing time (UTC+3)")
  {
    static const QRegularExpression tz("UTC\\s*([+\\-−])\\s*(\\d{1,2})(?::?(\\d\\d))?");
    const QString tc = rows[h].value(map.at(CloseTime));
    r.offsetFromUser = true;
    if (auto m = tz.match(tc); m.hasMatch()) {
      off = (m.captured(2).toInt() * 60 + m.captured(3).toInt()) * (m.captured(1) == "+" ? 1 : -1);
      r.offsetFromUser = false;
    } else if (tc.contains("UTC", Qt::CaseInsensitive) || tc.contains("GMT", Qt::CaseInsensitive)) { off = 0; r.offsetFromUser = false; }
    r.offsetMin = off;
  }
  static const QRegularExpression ccy("(?:[Nn]et|[Gg]ross|[Bb]alance)\\s*\\(?([A-Z]{3})\\)?");
  if (auto m = ccy.match(rows[h].join(' ')); m.hasMatch()) r.currency = m.captured(1);
  // day/month order: look for a first or second date field above 12
  bool dayFirst = true;
  const int tc = map.at(CloseTime);
  for (int i = h + 1; i < rows.size(); ++i) {
    static const QRegularExpression d("^(\\d{1,2})[./-](\\d{1,2})[./-]\\d{2,4}");
    const auto m = d.match(rows[i].value(tc));
    if (!m.hasMatch()) continue;
    if (m.captured(1).toInt() > 12) { dayFirst = true; break; }
    if (m.captured(2).toInt() > 12) { dayFirst = false; break; }
  }
  // decimal comma or thousands comma: decided from the file's own numbers (A4). "12,5", "0,500", "1,08523" or
  // "1.234,50" say decimal comma; "1,234.50", "1.10250" (a dot decimal) or "1,234,567" say the comma groups
  // thousands. Without any such number: decimal comma in ';' files, thousands in ',' files (a decimal comma
  // there would have to be quoted), and the system's convention in tab-separated files.
  bool decComma = sep == ';';
  {
    int dec = 0, grp = 0;
    static const QRegularExpression noise("[\\s\\x{00A0}\\x{202F}'()+-]");
    static const QRegularExpression numCell(R"(^[-+(]?[\d.,\s\x{00A0}\x{202F}']*\d\)?$)");
    const QList<Role> numeric{Entry, Exit, Lots, SL, TP, Commission, Fee, Swap, Profit, Net, BalanceCol, Units};
    for (int i = h + 1; i < rows.size() && dec + grp < 400; ++i)
      for (Role ro : numeric) {
        QString v = rows[i].value(map.at(ro)).trimmed();
        if (v.isEmpty() || !numCell.match(v).hasMatch()) continue;
        v.remove(noise);
        const int lc = v.lastIndexOf(','), ld = v.lastIndexOf('.');
        if (lc >= 0 && ld >= 0) (lc > ld ? dec : grp)++;
        else if (lc >= 0) { if (v.count(',') > 1) grp++; else if (!thousandsGroups(v, ',')) dec++; }
        else if (ld >= 0 && v.count('.') == 1 && v.size() - ld - 1 != 3) grp++;     // "1.10250", "0.5": dot decimals
      }
    if (dec > grp) decComma = true;
    else if (grp > dec) decComma = false;
    else if (sep == '\t') decComma = QLocale::system().decimalPoint() == QStringLiteral(",");
  }
  const bool unitsCol = map.has(Units) && !map.has(Lots);
  QHash<QString, int> seenRows;
  QDateTime firstClose; double firstBalance = NAN, firstNet = 0;
  for (int i = h + 1; i < rows.size(); ++i) {
    const auto& c = rows[i];
    if (c.size() < 3) continue;
    Trade t; QString why;
    if (!rowToTrade(c, map, off, dayFirst, decComma, t, &why)) {
      const QString l = c.join(' ').toLower();
      if (l.contains("buy") || l.contains("sell") || l.contains("long") || l.contains("short")) ++r.skipped;
      continue;
    }
    if (unitsCol) t.lots = parseNumber(c.value(map.at(Units)), decComma) / 100000.0;
    else if (ctrader && std::isfinite(t.lots) && t.lots >= 1000 && !c.value(map.at(Lots)).contains("lot", Qt::CaseInsensitive))
      t.lots /= 100000.0;                                  // cTrader quantity in units
    t.source = ctrader ? "ctrader" : "csv";
    // without a ticket the id comes from the row as written (rowKey), so importing the same file twice adds
    // nothing, whatever the offset chosen each time
    if (t.ticket.isEmpty()) {
      t.id = occurrenceId(t.source, rowKey(c, map), seenRows);
      t.legacyId = t.source + ":" + QString("%1-%2-%3-%4").arg(t.symbol).arg(t.open.toSecsSinceEpoch())
                                        .arg(t.close.toSecsSinceEpoch()).arg(t.pnl, 0, 'f', 2);       // ids before 2026-11-16
    } else t.id = t.source + ":" + t.ticket;
    if (map.has(BalanceCol) && (!firstClose.isValid() || t.close < firstClose)) {
      const double b = parseNumber(c.value(map.at(BalanceCol)), decComma);
      if (std::isfinite(b)) { firstClose = t.close; firstBalance = b; firstNet = t.net(); }
    }
    r.trades << t;
  }
  if (std::isfinite(firstBalance)) r.deposits = firstBalance - firstNet;   // balance before the first trade
  if (r.trades.isEmpty()) r.error = QCoreApplication::translate("account", "The header was read but no closed buy/sell rows were found.");
  return r;
}

// ---------------------------------------------------------------- bridge
namespace {
// Any program able to write the watched folder can write a bridge file, and names / symbols / comments end up
// in the journal and in the AI track-record tool (A15): control and bidi-format characters are dropped and
// every field is length-limited before it is stored.
QString cleanField(const QString& in, int maxLen) {
  QString out;
  out.reserve(std::min<int>(in.size(), maxLen));
  for (QChar ch : in) {
    if (out.size() >= maxLen) break;
    const auto cat = ch.category();
    if (cat == QChar::Separator_Line || cat == QChar::Separator_Paragraph || ch == '\t') { out += ' '; continue; }
    if (cat == QChar::Other_Control || cat == QChar::Other_Format || cat == QChar::Other_NotAssigned || ch.isSurrogate()) continue;
    if (ch == '<' || ch == '>') continue;   // no markup reaches a label, a tooltip or a model (names are never written with them)
    out += ch;
  }
  return out.simplified();
}
}

ImportResult parseBridge(const QString& text) {
  ImportResult r;
  r.format = QT_TRANSLATE_NOOP("account", "Deepwater bridge");
  const auto lines = text.split('\n');
  for (const auto& raw : lines) {
    const QString l = raw.trimmed();
    if (l.isEmpty()) continue;
    if (l.startsWith('#')) {
      for (const auto& kv : l.mid(1).split(';')) {
        const QString k = kv.section('=', 0, 0).trimmed().toLower(), v = kv.section('=', 1).trimmed();
        if (k == "platform") r.platform = cleanField(v, 16);
        else if (k == "login") r.login = cleanField(v, 32);
        else if (k == "currency") r.currency = cleanField(v, 8).toUpper();
        else if (k == "broker") r.broker = cleanField(v, 64);
        else if (k == "name") r.name = cleanField(v, 64);
        else if (k == "deposits") r.deposits = parseNumber(v);
        else if (k == "times") r.timesVerified = v == "verified";   // written by the 2026-11-16 EAs / cBot
      }
      continue;
    }
    const auto f = l.split(';');
    if (f.size() < 13 || f[0] == "ticket") continue;
    Trade t;
    t.ticket = cleanField(f[0], 32); t.symbol = cleanField(f[1], 32);
    const QString side = f[2].toLower();
    if (side != "buy" && side != "sell") continue;
    if (t.ticket.isEmpty() || t.symbol.isEmpty()) { ++r.skipped; continue; }
    t.side = side == "buy" ? Side::Long : Side::Short;
    t.lots = parseNumber(f[3]);
    t.open = QDateTime::fromSecsSinceEpoch(f[4].toLongLong(), QTimeZone::UTC);
    t.entry = parseNumber(f[5]);
    t.close = QDateTime::fromSecsSinceEpoch(f[6].toLongLong(), QTimeZone::UTC);
    t.exit = parseNumber(f[7]);
    t.sl = parseNumber(f[8]); if (t.sl == 0) t.sl = NAN;
    t.tp = parseNumber(f[9]); if (t.tp == 0) t.tp = NAN;
    t.commission = parseNumber(f[10]); if (!std::isfinite(t.commission)) t.commission = 0;
    t.swap = parseNumber(f[11]); if (!std::isfinite(t.swap)) t.swap = 0;
    t.pnl = parseNumber(f[12]);
    t.comment = cleanField(f.mid(13).join(','), 128);   // a ';' left in a comment by another writer stays text
    if (!std::isfinite(t.pnl) || f[6].toLongLong() <= 0) { ++r.skipped; continue; }
    t.source = "bridge-" + r.platform.toLower();
    t.id = t.source + ":" + t.ticket;
    r.trades << t;
  }
  r.offsetMin = 0; r.offsetFromUser = false;          // UTC seconds in the file
  if (r.trades.isEmpty() && r.login.isEmpty()) r.error = QCoreApplication::translate("account", "Not a Deepwater bridge file.");
  return r;
}

// ---------------------------------------------------------------- entry points
ImportResult importData(const QByteArray& data, const QString& fileName, int off) {
  const QString text = decode(data);
  if (text.startsWith("# Deepwater bridge")) return parseBridge(text);
  const bool html = text.contains("<table", Qt::CaseInsensitive) || fileName.endsWith(".htm", Qt::CaseInsensitive) ||
                    fileName.endsWith(".html", Qt::CaseInsensitive);
  ImportResult r = html ? parseHtmlStatement(text, off) : parseCsv(text, off);
  std::stable_sort(r.trades.begin(), r.trades.end(), [](const Trade& a, const Trade& b) { return a.close < b.close; });
  // a statement's header fields are text from the file, like a bridge file's: same cleaning (the bridge does it itself)
  r.login = cleanField(r.login, 32);
  r.name = cleanField(r.name, 64);
  r.broker = cleanField(r.broker, 64);
  r.currency = cleanField(r.currency, 8).toUpper();
  return r;
}

ImportResult importFile(const QString& path, int off) {
  QFile f(path);
  ImportResult r;
  if (!f.open(QIODevice::ReadOnly)) { r.error = f.errorString(); return r; }
  if (f.size() > 64 * 1024 * 1024) { r.error = QCoreApplication::translate("account", "File too large (over 64 MB)."); return r; }
  if (path.endsWith(".xlsx", Qt::CaseInsensitive) || path.endsWith(".pdf", Qt::CaseInsensitive)) {
    r.error = QCoreApplication::translate("account", "Excel and PDF reports are not read: save the report as HTML (MT4/MT5) or CSV (cTrader).");
    return r;
  }
  return importData(f.readAll(), QFileInfo(path).fileName(), off);
}
}
