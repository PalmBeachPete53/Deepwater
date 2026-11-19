#pragma once
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

// Minimal ZIP + XLSX reader (stored/deflate entries, shared strings, numeric cells) for official spreadsheets
// such as the Bank of England yield-curve archive. Portable (own inflate), runs in worker threads.
namespace macro::xlsx {
QByteArray inflateRaw(const QByteArray& deflated, bool* ok = nullptr);   // RFC 1951 raw DEFLATE
QHash<QString, QByteArray> unzip(const QByteArray& zip, const QStringList& onlyNames = {});   // name → content
// Sheet by its visible name: rows of cells (QString for text, double for numbers, invalid for empty).
QList<QList<QVariant>> sheet(const QByteArray& xlsxFile, const QString& sheetName, bool* ok = nullptr);
}
