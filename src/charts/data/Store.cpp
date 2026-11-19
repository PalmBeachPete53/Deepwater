// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/store.js.
#include "charts/data/Store.h"
#include "core/Paths.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QCoreApplication>
#include <filesystem>
#include <cmath>

namespace charts::store {

QString dataDir() { return paths::data("charts"); }

QJsonValue read(const QString& name, const QJsonValue& fallback, ReadState* state) {
  QFile f(dataDir() + "/" + name + ".json");
  auto set = [state](ReadState s) { if (state) *state = s; };
  if (!f.exists()) { set(ReadState::Missing); return fallback; }
  if (!f.open(QIODevice::ReadOnly)) { set(ReadState::Unreadable); return fallback; }
  // Node reads the file as UTF-8 (bad bytes become U+FFFD) and JSON.parse accepts any value at the top level,
  // scalars too; QJsonDocument only takes an object or array, so the text is parsed inside [ ... ].
  const QByteArray text = QString::fromUtf8(f.readAll()).toUtf8();
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson("[" + text + "]", &err);
  if (err.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size() != 1) { set(ReadState::Unparsable); return fallback; }
  set(ReadState::Ok);
  return doc.array().at(0);
}

QString keepAside(const QString& name) {
  const QString file = dataDir() + "/" + name + ".json";
  const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss");
  QString bad = file + ".bad-" + stamp;
  for (int n = 2; QFile::exists(bad); ++n) bad = file + ".bad-" + stamp + "-" + QString::number(n);
  return QFile::copy(file, bad) ? bad : QString();
}

bool write(const QString& name, const QJsonValue& value, QString* error) {
  const QString file = dataDir() + "/" + name + ".json";
  const QByteArray text = stringify(value, 2).toUtf8();
  // QSaveFile: a temporary file in the same folder, synced to disk and renamed over the old one at commit(), so a
  // crash or a full disk leaves the old file whole (audit D22: the rename had no fsync, and a refused rename fell back
  // to writing in place, which a crash could cut). A refused rename (Windows: antivirus or indexer holding the file) now
  // fails this save with its error; the caller reports it and the next save tries again.
  QSaveFile f(file);
  if (!f.open(QIODevice::WriteOnly)) {
    if (error) *error = f.errorString();
    return false;
  }
  if (f.write(text) != text.size()) {
    if (error) *error = f.errorString();
    f.cancelWriting();
    return false;
  }
  if (!f.commit()) {
    if (error) *error = f.errorString();
    return false;
  }
  return true;
}

QJsonObject mergeState(const QJsonObject& patch, QString* error) {
  // { ...old, ...patch }: an object's own keys; an array spreads its indexes, a string its characters.
  ReadState rs = ReadState::Ok;
  const QJsonValue old = read("state", QJsonObject(), &rs);
  if (rs == ReadState::Unreadable || rs == ReadState::Unparsable) {
    // A state.json that exists but can't be read (permissions, a lock) or parsed (a hand edit, a cut write) used to be
    // taken as {} and overwritten: layouts, watchlists, drawings and the paper account gone (audit D22). Nothing is
    // merged into it. An unparsable one is copied to state.json.bad-<UTC time> and replaced by this save next time;
    // an unreadable one is left alone and this save is refused until it can be read.
    if (rs == ReadState::Unparsable) {
      const QString bad = keepAside("state");
      if (bad.isEmpty()) {
        if (error) *error = QStringLiteral("state.json can't be read and couldn't be copied aside; nothing was saved");
        return patch;
      }
      // the copy is safe: the next save starts from an empty state, the file's content stays in the copy
      if (QFile::remove(dataDir() + "/state.json") && error)
        *error = QStringLiteral("state.json couldn't be read; it was kept as %1 and this save was skipped").arg(QFileInfo(bad).fileName());
      else if (error)
        *error = QStringLiteral("state.json couldn't be read; a copy is in %1; nothing was saved").arg(QFileInfo(bad).fileName());
      return patch;
    }
    if (error) *error = QStringLiteral("state.json exists but can't be read; nothing was saved");
    return patch;
  }
  QJsonObject next;
  if (old.isObject()) next = old.toObject();
  else if (old.isArray()) {
    const QJsonArray a = old.toArray();
    for (int i = 0; i < a.size(); ++i) next.insert(QString::number(i), a.at(i));
  } else if (old.isString()) {
    const QString s = old.toString();
    for (int i = 0; i < s.size(); ++i) next.insert(QString::number(i), QString(s.at(i)));
  }
  for (auto it = patch.begin(); it != patch.end(); ++it) next.insert(it.key(), it.value());
  write("state", next, error);
  return next;
}

QString numberToString(double v) {
  if (std::isnan(v)) return QStringLiteral("NaN");
  if (std::isinf(v)) return v > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
  if (v == 0) return QStringLiteral("0");
  // Shortest round-trip digits, then ECMAScript Number::toString layout.
  const QString e = QString::number(std::fabs(v), 'e', QLocale::FloatingPointShortest);   // "1.2345e+02"
  const int ePos = e.indexOf('e');
  QString digits = e.left(ePos);
  digits.remove('.');
  while (digits.size() > 1 && digits.endsWith('0')) digits.chop(1);
  const int k = digits.size();
  const int n = e.mid(ePos + 1).toInt() + 1;   // value = 0.digits × 10^n
  QString out;
  if (k <= n && n <= 21) out = digits + QString(n - k, '0');
  else if (0 < n && n <= 21) out = digits.left(n) + "." + digits.mid(n);
  else if (-6 < n && n <= 0) out = "0." + QString(-n, '0') + digits;
  else {
    const int x = n - 1;
    out = digits.left(1) + (k > 1 ? "." + digits.mid(1) : QString()) + "e" + (x >= 0 ? "+" : "-") + QString::number(std::abs(x));
  }
  return v < 0 ? "-" + out : out;
}

namespace {
QString quote(const QString& s) {
  QString o;
  o.reserve(s.size() + 2);
  o += '"';
  for (int i = 0; i < s.size(); ++i) {
    const QChar c = s.at(i);
    const ushort u = c.unicode();
    switch (u) {
      case '"': o += QLatin1String("\\\""); continue;
      case '\\': o += QLatin1String("\\\\"); continue;
      case '\b': o += QLatin1String("\\b"); continue;
      case '\f': o += QLatin1String("\\f"); continue;
      case '\n': o += QLatin1String("\\n"); continue;
      case '\r': o += QLatin1String("\\r"); continue;
      case '\t': o += QLatin1String("\\t"); continue;
      default: break;
    }
    bool lone = false;
    if (c.isHighSurrogate()) lone = !(i + 1 < s.size() && s.at(i + 1).isLowSurrogate());
    else if (c.isLowSurrogate()) lone = !(i > 0 && s.at(i - 1).isHighSurrogate());
    if (u < 0x20 || lone) o += QString("\\u%1").arg(u, 4, 16, QChar('0'));
    else o += c;
  }
  o += '"';
  return o;
}

void putValue(QString& out, const QJsonValue& v, int indent, int level) {
  const QString pad = indent ? "\n" + QString(indent * (level + 1), ' ') : QString();
  const QString end = indent ? "\n" + QString(indent * level, ' ') : QString();
  switch (v.type()) {
    case QJsonValue::Null: case QJsonValue::Undefined: out += QLatin1String("null"); break;
    case QJsonValue::Bool: out += v.toBool() ? QLatin1String("true") : QLatin1String("false"); break;
    case QJsonValue::Double: {
      const double d = v.toDouble();
      out += std::isfinite(d) ? numberToString(d) : QStringLiteral("null");
      break;
    }
    case QJsonValue::String: out += quote(v.toString()); break;
    case QJsonValue::Array: {
      const QJsonArray a = v.toArray();
      if (a.isEmpty()) { out += QLatin1String("[]"); break; }
      out += '[';
      for (int i = 0; i < a.size(); ++i) {
        if (i) out += ',';
        out += pad;
        putValue(out, a.at(i), indent, level + 1);
      }
      out += end + ']';
      break;
    }
    case QJsonValue::Object: {
      const QJsonObject o = v.toObject();
      if (o.isEmpty()) { out += QLatin1String("{}"); break; }
      out += '{';
      bool first = true;
      for (auto it = o.begin(); it != o.end(); ++it) {
        if (!first) out += ',';
        first = false;
        out += pad + quote(it.key()) + (indent ? QLatin1String(": ") : QLatin1String(":"));
        putValue(out, it.value(), indent, level + 1);
      }
      out += end + '}';
      break;
    }
  }
}
}  // namespace

QString stringify(const QJsonValue& value, int indent) {
  QString out;
  putValue(out, value, indent, 0);
  return out;
}

}  // namespace charts::store
