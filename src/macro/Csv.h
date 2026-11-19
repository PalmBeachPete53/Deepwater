#pragma once
#include <QByteArray>
#include <QList>
#include <QStringList>

// Minimal CSV reader (RFC 4180 quotes, CRLF or LF) used by the macro parsers; runs in worker threads.
namespace macro::csv {
inline QList<QStringList> parse(const QByteArray& body, char sep = ',') {
  QList<QStringList> rows;
  QStringList row; QByteArray cell; bool q = false;
  const int n = body.size();
  for (int i = 0; i < n; ++i) {
    const char c = body[i];
    if (q) {
      if (c == '"') { if (i + 1 < n && body[i + 1] == '"') { cell += '"'; ++i; } else q = false; }
      else cell += c;
      continue;
    }
    if (c == '"') q = true;
    else if (c == sep) { row << QString::fromUtf8(cell); cell.clear(); }
    else if (c == '\n' || c == '\r') {
      if (c == '\r' && i + 1 < n && body[i + 1] == '\n') ++i;
      row << QString::fromUtf8(cell); cell.clear();
      if (!(row.size() == 1 && row[0].isEmpty())) rows << row;
      row.clear();
    } else cell += c;
  }
  if (!cell.isEmpty() || !row.isEmpty()) { row << QString::fromUtf8(cell); rows << row; }
  if (!rows.isEmpty() && !rows[0].isEmpty() && rows[0][0].startsWith(QChar(0xFEFF))) rows[0][0].remove(0, 1);
  return rows;
}
}
