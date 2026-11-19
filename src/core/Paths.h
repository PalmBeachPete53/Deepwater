#pragma once
#include <QString>

// Where the app keeps files. DW_PROFILE=<dir> moves user data (journal, archives, recordings) under <dir>/data.
// DW_CACHE_DIR overrides the HTTP cache folder (tests may share one cache to spare the sources).
namespace paths {
QString data(const QString& sub = {});    // durable user data, created on demand
QString cache(const QString& sub = {});   // re-downloadable data
// A database moved to a new name (2026-11-18: <data>/facts/ became macro-data/ and technical-data/): when `to` does not exist
// yet and `from` does, renames from (and its -wal / -shm, which go with it) to `to`, then removes from's folder if it is
// empty. Call before the database is opened. False only when a rename failed (the old files are then left as they were).
bool moveDatabase(const QString& from, const QString& to);

// Files in a folder that a sandboxed child process can write (an ML run's work folder, an AI environment): the app runs
// unsandboxed, so it must never follow a link the child planted there (to ~/.zshrc, or a private file it would then hand
// to a model).
// True when `path` is no symbolic link and lies inside `folder` once resolved (a missing file: its parent folder does).
bool insideNoLink(const QString& folder, const QString& path);
// Removes a symbolic link sitting at `path` before the app writes there: QSaveFile and appends both follow links.
void dropLink(const QString& path);
}
