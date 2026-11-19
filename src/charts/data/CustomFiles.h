#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// The custom indicator files and the signal import. Port of quant-terminal server.js (slug, listCustom,
// loadCustom, customErrors, POST /api/custom, DELETE /api/custom/:file, POST /api/import/signal)
// UI-free and shared: the indicator editor, the strategy tester and the local HTTP API (charts::api) all go through it,
// so there is one customErrors map, as in the single-process JS server.
//
// Files live in charts::data::customDir() (paths::data("charts/custom"), the JS custom/ folder).
// GUI thread only: loading a file runs it in the custom-JS engine (charts::custom), which is single-threaded.
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <functional>
#include "charts/data/Imports.h"

namespace charts::data {

// JS slug(name): lower case, runs of other characters to "-", trimmed, at most 60 characters, "indicator" if empty.
QString customSlug(const QString& name);

// GET /api/custom: every *.js and *.pine file of the folder (readdir order = name order here). A .pine file is a Pine
// Script indicator (charts/core/PineIndicators.h), id "pine:<name>".
struct CustomEntry {
  QString file, id;     // "my-indicator.js", "custom:my-indicator"
  double mtime = 0;     // ms since 1970 (fs.statSync().mtimeMs)
  QString error;        // customErrors[file] (empty = null)
  QJsonObject toJson() const;   // {file, id, mtime, error}
};
QVector<CustomEntry> listCustom();
QJsonArray listCustomJson();

// The indicator id of a file of the folder: "custom:<name>" for .js, "pine:<name>" for .pine.
QString customIdOf(const QString& file);
// server.js loadCustom(file): loads and registers custom/<file> (.js or .pine); remembers or clears customErrors[file].
// Returns the error message (empty = registered).
QString loadCustom(const QString& file);
// server.js start: every *.js file of the folder (listCustom order). GUI thread.
void loadAllCustom();
// The same without making the GUI wait for the Pine scripts (audit P1: a .pine file with an endless loop froze every
// launch for 20 s): the .js files load at once (their engine is the GUI thread's), the .pine files are checked on a
// Pine thread and registered on ctx's thread; then done(the .pine files loaded, in name order), customErrors set.
// Nothing is called if ctx is gone.
void loadAllCustomAsync(QObject* ctx, std::function<void(const QStringList& pineFiles)> done);
// customErrors[file] (empty = none).
QString customError(const QString& file);

// POST /api/custom {name, code}: CommonJS `module.exports =` is taken as `export default`; Pine Script (a //@version
// line or an indicator() / strategy() call, no `export default`) is written to custom/<slug(name)>.pine; other code
// without `export default` is refused; JavaScript is written to custom/<slug(name)>.js; then loaded, and a refused
// file puts back what was there before (or nothing).
struct CustomSave {
  int status = 200;     // 200 ok, 400 refused
  QString file, id, error;
  QJsonObject toJson() const;   // 200: {ok: true, file, id}; 400 before writing: {error}; 400 after: {error, file}
};
CustomSave saveCustom(const QString& name, const QString& code);

// DELETE /api/custom/<file>: path.basename(file) removed from the folder (no error when missing).
void deleteCustom(const QString& file);

// GET /custom/<file>: the file's text (empty when missing).
QString readCustom(const QString& file);

// POST /api/import/signal?name=<rawName> with the CSV text: parse (charts::data::parseSignals), write
// custom/signal-<name>.js, load it, put the old file back when it fails. Throws charts::data::ImportError
// (status 400) with the JS message; returns the JS answer {ok, name, id, file, rows, changes, from, to, strategy}.
QJsonObject importSignal(const QString& rawName, const QString& text);
// Same, with the CSV already parsed (charts::data::parseSignals, e.g. on a worker thread): only the GUI-thread part.
QJsonObject importSignal(const QString& rawName, const Signals& parsed);

}  // namespace charts::data
