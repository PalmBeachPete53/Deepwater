// Live trades folded into 1-minute footprints. See LiveFootprint.h.
#include "technicaldata/LiveFootprint.h"
#include <algorithm>
#include <limits>
#include <memory>

namespace technicaldata {

namespace {
qint64 floorDiv(qint64 a, qint64 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
using Spans = std::vector<std::pair<qint64, qint64>>;
void addSpan(Spans& s, qint64 a, qint64 b) {
  if (b <= a) return;
  s.push_back({a, b});
  std::sort(s.begin(), s.end());
  Spans out;
  for (const auto& x : s) {
    if (!out.empty() && x.first <= out.back().second) out.back().second = std::max(out.back().second, x.second);
    else out.push_back(x);
  }
  s = std::move(out);
}
void cutSpan(Spans& s, qint64 a, qint64 b) {
  if (b <= a) return;
  Spans out;
  for (const auto& x : s) {
    if (x.second <= a || x.first >= b) { out.push_back(x); continue; }
    if (x.first < a) out.push_back({x.first, a});
    if (x.second > b) out.push_back({b, x.second});
  }
  s = std::move(out);
}
}  // namespace

bool LiveFootprint::covers(qint64 from, qint64 to) const {
  for (const auto& x : covered) if (x.first <= from && to <= x.second) return true;
  return false;
}

void foldTrades(const charts::flow::TradeTape& tape, const charts::flow::TickScale& ticks, LiveFootprint& f, LiveFold& st, qint64 nowSec) {
  if (!tape.empty() && ticks.valid()) {
    const qint64 frontS = floorDiv(tape.front().timeMs, 1000), backS = floorDiv(tape.back().timeMs, 1000);
    const qint64 from = frontS - (frontS - floorDiv(frontS, 60) * 60) + 60;   // the first trade's minute may be partial
    const qint64 to = floorDiv(backS, 60) * 60;                             // the last trade's minute is still open
    // only the minutes after the last fold, unless older trades came in (history, a hole filled) since
    qint64 start = from;
    if (st.firstId >= 0 && tape.firstId() >= st.firstId && tape.missing() == st.missing && st.lastTo > from) start = std::max(from, st.lastTo);
    if (to > start) {
      std::vector<fp::Minute> built;
      std::unique_ptr<fp::DayBuilder> day;
      qint64 dayStart = 0;
      auto flush = [&] { if (day) for (auto& m : day->finish()) built.push_back(std::move(m)); };
      for (size_t i = tape.lowerBoundTime(start * 1000); i < tape.size(); ++i) {
        const charts::flow::Trade& t = tape[i];
        if (t.timeMs >= to * 1000) break;
        if (t.side != charts::flow::Side::Buy && t.side != charts::flow::Side::Sell) continue;
        const qint64 d = floorDiv(floorDiv(t.timeMs, 1000), 86400) * 86400;
        if (!day || d != dayStart) { flush(); day = std::make_unique<fp::DayBuilder>(d); dayStart = d; }
        day->add(fp::AggTrade{t.id, ticks.priceOf(t.ticks), t.qty, t.timeMs, t.side == charts::flow::Side::Sell});
      }
      flush();
      f.minutes.erase(f.minutes.lower_bound(start), f.minutes.lower_bound(to));
      for (auto& m : built) { const qint64 t = m.t; f.minutes[t] = std::move(m); }
      addSpan(f.covered, start, to);
      // the minutes around a hole of the tape (ids missing) are not whole
      for (const auto& g : tape.gaps()) {
        const size_t after = tape.lowerBound(g.from);
        if (after == 0 || after >= tape.size()) continue;
        const qint64 a = floorDiv(floorDiv(tape[after - 1].timeMs, 1000), 60) * 60, b = floorDiv(floorDiv(tape[after].timeMs, 1000), 60) * 60 + 60;
        cutSpan(f.covered, a, b);
        f.minutes.erase(f.minutes.lower_bound(a), f.minutes.lower_bound(b));
      }
    }
    st = LiveFold{tape.firstId(), tape.missing(), std::max(st.lastTo, to)};
  }
  const qint64 cut = floorDiv(nowSec - LiveFootprint::kKept, 60) * 60;
  f.minutes.erase(f.minutes.begin(), f.minutes.lower_bound(cut));
  cutSpan(f.covered, std::numeric_limits<qint64>::min() / 2, cut);
}

}  // namespace technicaldata
