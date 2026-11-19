// Data › Technical data as backtest bars. See TechnicalDataBars.h.
#include "technicaldata/TechnicalDataBars.h"
#include "technicaldata/TechnicalDataStore.h"
#include "technicaldata/Collector.h"

namespace technicaldata {

namespace {
charts::Bars toBars(const QVector<Kline>& k) {
  charts::Bars out;
  out.reserve(k.size());
  for (const Kline& x : k) out.push_back(charts::Bar{x.t, x.o, x.h, x.l, x.c, x.v});
  return out;
}
}  // namespace

QString storedMarket(const QString& symbol) {
  QString s = symbol.trimmed().toUpper();
  s.remove('/').remove('-').remove('_');
  const QStringList pairs = Store::instance().pairs();
  if (s.contains(':')) {
    const QString ex = s.section(':', 0, 0), t = s.section(':', 1);
    if (ex == "BINANCE") return pairs.contains(s) ? s : QString();
    if (QStringList{"DUKASCOPY", "FX", "FOREX", "OANDA", "FXCM", "FX_IDC"}.contains(ex)) {
      const QString k = "DUKASCOPY:" + t;
      return pairs.contains(k) ? k : QString();
    }
    return {};
  }
  for (const char* prefix : {"BINANCE:", "DUKASCOPY:"})
    if (pairs.contains(prefix + s)) return prefix + s;
  return {};
}

QString sourceOf(const QString& market) {
  return market.startsWith("DUKASCOPY:") ? QStringLiteral("Dukascopy bid prices") : QStringLiteral("Binance spot");
}

int storedTimeframe(int seconds) {
  for (const auto& t : timeframes()) if (t.seconds == seconds) return seconds;
  return 0;
}

qint64 storedCount(const QString& market, int tfSeconds) {
  return market.isEmpty() || !tfSeconds ? 0 : Store::instance().state(market, tfSeconds).count;
}

charts::Bars newestBars(const QString& market, int tfSeconds, int max) {
  auto& st = Store::instance();
  const SyncState s = st.state(market, tfSeconds);
  if (!s.count || max <= 0) return {};
  return toBars(st.before(market, tfSeconds, s.last + 1, max));
}

charts::Bars magnifierBars(const QString& market, int tfSeconds, const charts::Bars& bars, int maxBars, QString* tfName) {
  if (bars.isEmpty()) return {};
  const qint64 from = bars.first().time, to = bars.last().time + tfSeconds - 1;
  auto& st = Store::instance();
  // finest first; a timeframe whose stored history covers the whole test wins over a finer one still being collected
  int partial = 0;
  for (bool whole : {true, false}) {
    for (const auto& t : timeframes()) {
      if (t.seconds >= tfSeconds) break;
      if (!whole && t.seconds != partial) continue;
      const SyncState s = st.state(market, t.seconds);
      if (!s.count || double(to - from) / t.seconds > maxBars) continue;
      // from the first test bar (the exchange's first day opened at 04:00 inside its 1D bar) to inside the last one
      const bool covers = s.first < from + tfSeconds && s.last + t.seconds > bars.last().time;
      if (whole && !covers) { if (!partial) partial = t.seconds; continue; }
      if (tfName) *tfName = t.name;
      return toBars(st.range(market, t.seconds, from, to));
    }
  }
  return {};
}

}  // namespace technicaldata
