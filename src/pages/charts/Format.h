#pragma once
// Charts tab — the formatters of quant-terminal public/js/api.js (MIT, Copyright (c) 2026 Lewis Jackson —
// docs/charts/NOTICE.md): fmtPrice, fmtNum, fmtPct, esc, ticker, exchange, download.
// Numbers use the English grouping (1,234.56) like the rest of Deepwater (core/I18n.h) and like the JS in en-US.
#include <QString>
#include <QByteArray>

class QWidget;

namespace charts::ui::fmt {

QString price(double v);            // fmtPrice: "—" for NaN; 2..8 decimals by size; >= 1e6 → num()
QString num(double v, int d = 2);   // fmtNum: 1.23T / 4.56B / 7.89M / 12.3K / v.toFixed(d)
QString pct(double v);              // fmtPct: "+1.23%" / "-0.50%" / "—"
QString esc(const QString& s);      // HTML escape (& < > " ')
// "BINANCE:BTCUSDT" -> "BTCUSDT"; "SPREAD:BINANCE:BTCUSDT / BINANCE:ETHUSDT" -> "BTCUSDT/ETHUSDT".
QString ticker(const QString& id);
QString exchange(const QString& id);   // "BINANCE:BTCUSDT" -> "BINANCE"
// JS Number.prototype.toLocaleString(undefined, {minimumFractionDigits, maximumFractionDigits}) in en-US.
QString localeNumber(double v, int minFrac, int maxFrac);

// French, at display time, of a text made in src/charts (DataText.cpp): src/charts keeps its texts English (saved
// files, local API, tests), so the UI translates what it shows — a whole text marked QT_TRANSLATE_NOOP("charts", …)
// where it is defined, or a runtime text matching one of DataText.cpp's templates. English UI: unchanged.
QString tx(const QString& english);

// JS download(name, content): the browser saves a file. Here: a save dialog (asynchronous, non-blocking) opened in
// the Downloads folder with `name` proposed; writes `content` there. `parent` = the window the dialog belongs to.
void download(QWidget* parent, const QString& name, const QByteArray& content);

}  // namespace charts::ui::fmt
