#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of lib/store.js. Everything you save (layouts, watchlists, drawings, alerts, paper account) lives in
// <data>/charts as plain JSON files you own. Writes go to a temp file first so a crash can't corrupt them.
#include <QString>
#include <QJsonValue>
#include <QJsonObject>

namespace charts::store {

QString dataDir();   // paths::data("charts")   (JS DATA_DIR)

// What read() found: the fallback comes for Missing (no file) as for Unreadable (it exists, can't be opened) and
// Unparsable (not JSON); writers must tell them apart before overwriting the file.
enum class ReadState { Ok, Missing, Unreadable, Unparsable };
// <name>.json parsed, or fallback when it is missing or isn't valid JSON (any JSON value, scalars too).
QJsonValue read(const QString& name, const QJsonValue& fallback = QJsonValue(), ReadState* state = nullptr);
// Writes <name>.json as JSON.stringify(value, null, 2) through QSaveFile (temporary file, synced, renamed over the
// file at commit). false (+ error) when the file can't be written; the old file is then untouched.
bool write(const QString& name, const QJsonValue& value, QString* error = nullptr);
// Copies <name>.json to <name>.json.bad-<UTC yyyyMMdd-HHmmss>; the copy's path, or "" when it failed.
QString keepAside(const QString& name);

// server.js /api/state PUT: next = { ...read("state", {}), ...patch }, written back; returns next. A state.json that
// exists but can't be read or parsed is never merged into: an unparsable one is copied aside (keepAside) and
// removed, so the next save starts afresh; error says what happened and nothing is written by this call.
QJsonObject mergeState(const QJsonObject& patch, QString* error = nullptr);

// JSON.stringify(value, null, indent) (indent 0 = compact): 2-space layout, "key": value, JS number
// formatting (1e21, 0.1, 1.5e-7), JS string escaping. Keys come out in QJsonObject's (sorted) order.
QString stringify(const QJsonValue& value, int indent = 2);
QString numberToString(double v);   // JS String(number); NaN/Infinity as JS ("NaN", "Infinity")

}  // namespace charts::store
