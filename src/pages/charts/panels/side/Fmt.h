#pragma once
// Charts tab — side panel helpers: the formats of quant-terminal public/js/api.js (fmtPrice, fmtNum, fmtPct, esc,
// ticker, exchange) and the browser's toLocale*String the side tabs use (MIT, Copyright (c) 2026 Lewis Jackson —
// docs/charts/NOTICE.md). Numbers keep the English format (Deepwater rule, and the reference browser is en-US);
// dates follow i18n::locale() (en-US by default, like the reference browser).
#include <QDate>
#include <QDateTime>
#include <QJsonValue>
#include <QString>

namespace charts::ui::side {

QString fmtPrice(double v);                 // NaN / inf -> "—"
QString fmtNum(double v, int d = 2);
QString fmtPct(double v);
QString esc(const QString& s);
// esc(v) of a JSON value: String(v ?? "") then HTML escape.
QString escv(const QJsonValue& v);
QString ticker(const QString& id);
QString exchange(const QString& id);

// d.toLocaleDateString(undefined, { weekday: "long", day: "numeric", month: "short" }) -> "Monday, Nov 16"
QString localeDayLabel(const QDate& d);
// d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" }) -> "08:30 AM"
QString localeTime(const QDateTime& local);
// new Date(x).toLocaleString() -> "11/16/2026, 3:45:12 PM"
QString localeDateTime(const QDateTime& local);
// new Date(string) for the ISO strings of the feeds and the alert log (local time; invalid when unparsable).
QDateTime parseJsDate(const QString& s);

}  // namespace charts::ui::side
