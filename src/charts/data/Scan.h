#pragma once
// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of server.js mapLimit and the /api/scan route body: rules checked on the latest bar of many symbols at once.
// UI-free, so the local HTTP API and any panel can use it. GUI thread (candles come through Providers).
#include "charts/data/Fetch.h"
#include <QJsonArray>
#include <QJsonObject>
#include <functional>

namespace charts::data {

// Runs fn over every item with at most n running at once; done(results in item order) when all finished.
// fn(index, report): call report(value) once (any time later, on the GUI thread).
void mapLimit(int count, int n, std::function<void(int index, std::function<void(const QJsonValue&)> report)> fn,
              std::function<void(const QJsonArray&)> done);

// /api/scan: the first 150 symbols, 4 at a time, 400 bars of `tf` each; for each symbol
// {symbol, pass, passes: [bool per rule], close} or {symbol, error}. `symbol` is echoed as sent.
void scan(const QJsonArray& symbols, const QString& tf, const QJsonObject& indicators, const QJsonArray& rules,
          QObject* ctx, std::function<void(const QJsonArray&)> done);

}  // namespace charts::data
