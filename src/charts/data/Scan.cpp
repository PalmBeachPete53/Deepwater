// Charts tab (C++ port of quant-terminal, MIT, Copyright (c) 2026 Lewis Jackson — see docs/charts/NOTICE.md).
// Port of server.js mapLimit + the /api/scan route (see Scan.h).
#include "charts/data/Scan.h"
#include "charts/data/Providers.h"
#include "charts/core/Backtest.h"
#include <QPointer>
#include <memory>

namespace charts::data {

void mapLimit(int count, int n, std::function<void(int, std::function<void(const QJsonValue&)>)> fn,
              std::function<void(const QJsonArray&)> done) {
  struct State {
    QVector<QJsonValue> out;
    int next = 0, finished = 0, count = 0;
    std::function<void(int, std::function<void(const QJsonValue&)>)> fn;
    std::function<void(const QJsonArray&)> done;
    std::function<void()> worker;
  };
  auto s = std::make_shared<State>();
  s->out.resize(count);
  s->count = count;
  s->fn = std::move(fn);
  s->done = std::move(done);
  if (count == 0) {
    later([s] { s->done(QJsonArray()); });
    return;
  }
  // A worker takes the next item when its previous one reported (the JS `while (i < items.length)` loop).
  std::weak_ptr<State> weak = s;
  s->worker = [weak] {
    auto st = weak.lock();
    if (!st || st->next >= st->count) return;
    const int k = st->next++;
    st->fn(k, [st, k](const QJsonValue& v) {
      st->out[k] = v;
      if (++st->finished == st->count) {
        QJsonArray a;
        for (const QJsonValue& x : st->out) a.append(x);
        auto d = std::move(st->done);
        d(a);   // the state goes once the last report returns (workers only hold it weakly)
        return;
      }
      st->worker();
    });
  };
  for (int w = 0; w < std::min(n, count); ++w) s->worker();
}

void scan(const QJsonArray& symbolsIn, const QString& tf, const QJsonObject& indicators, const QJsonArray& rules,
          QObject* ctx, std::function<void(const QJsonArray&)> done) {
  QJsonArray symbols;   // body.symbols.slice(0, 150)
  for (int i = 0; i < std::min<qsizetype>(150, symbolsIn.size()); ++i) symbols.append(symbolsIn[i]);
  QPointer<QObject> guard(ctx);
  const bool hasCtx = ctx != nullptr;
  mapLimit(symbols.size(), 4,
      [symbols, tf, indicators, rules](int k, std::function<void(const QJsonValue&)> report) {
        const QJsonValue symbol = symbols[k];
        getCandles(bt::jsString(symbol), tf, 400, nullptr, [=](const Result<CandlesResult>& r) {
          if (!r.ok()) { report(QJsonObject{{"symbol", symbol}, {"error", r.error->message}}); return; }
          const Bars& candles = r.value.candles;
          try {
            auto make = bt::conditionFn(candles, indicators);
            const int i = candles.size() - 1;
            if (i < 0) throw std::runtime_error("Cannot read properties of undefined (reading 'close')");
            QJsonArray passes;
            bool all = true;
            for (const QJsonValue& rule : rules) {
              const bool p = make(rule.toObject())(i);
              passes.append(p);
              all = all && p;
            }
            report(QJsonObject{{"symbol", symbol}, {"pass", all}, {"passes", passes}, {"close", candles[i].close}});
          } catch (const std::exception& e) {
            report(QJsonObject{{"symbol", symbol}, {"error", QString::fromUtf8(e.what())}});
          }
        });
      },
      [guard, hasCtx, done](const QJsonArray& out) {
        if (hasCtx && !guard) return;
        done(out);
      });
}

}  // namespace charts::data
