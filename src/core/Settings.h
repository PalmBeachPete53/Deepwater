#pragma once
#include <QString>
#include <QVariant>

// App preferences. Normal runs use the native store (macOS domain com.deepwater-terminal.Deepwater).
// Test runs set DW_PROFILE=<dir>: reads and writes use <dir>/settings.ini only; the one exception is API keys
// ("keys.*"), read (never written) from the native store. Sessions and other state are never borrowed.
namespace settings {
QVariant value(const QString& key, const QVariant& def = {});
void setValue(const QString& key, const QVariant& v);
void remove(const QString& key);
bool contains(const QString& key);
// API key saved by the user (key "keys.<name>"), empty if none. Never log the result.
QString apiKey(const QString& name);
bool isTestProfile();
}
