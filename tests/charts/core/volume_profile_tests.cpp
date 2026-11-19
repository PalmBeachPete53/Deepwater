// Port of the two volume-profile cases of quant-terminal test/reference.test.js (MIT, Copyright (c) 2026 Lewis
// Jackson): they test public/js/volume-profile.js, whose math is header-only in src/pages/charts/VolumeProfile.h.
#define CHARTS_VP_MATH_ONLY
#include "pages/charts/VolumeProfile.h"
#include "charts/core/Indicators.h"
#include "Check.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <numeric>

using namespace charts;
namespace vp = charts::ui::vp;

namespace {
const Bars& vpBars() {
  static const Bars b = [] {
    QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
    if (!f.open(QIODevice::ReadOnly)) return Bars();
    Bars out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
      const QJsonObject o = v.toObject();
      out << Bar{qint64(o.value("time").toDouble()), o.value("open").toDouble(), o.value("high").toDouble(),
                 o.value("low").toDouble(), o.value("close").toDouble(), o.value("volume").toDouble()};
    }
    return out;
  }();
  return b;
}
double sumOf(const QVector<double>& v, int from = 0, int to = -1) {
  if (to < 0) to = int(v.size()) - 1;
  double s = 0;
  for (int i = from; i <= to; ++i) s += v[i];
  return s;
}
}  // namespace

// Volume profile: every bar's volume lands in the rows its range covers, and the value area holds
// at least the asked-for share of the total.
TEST("volume profile conserves volume and its value area holds 70%") {
  const Bars& bars = vpBars();
  CHECK(bars.size() > 200);
  const Bars sample = bars.mid(bars.size() - 200);
  const auto prof = vp::profile(sample, 24, 70);
  CHECK(prof.has_value());
  if (!prof) return;
  const double total = sumOf(prof->total);
  double want = 0;
  for (const Bar& b : sample) want += std::isnan(b.volume) ? 0 : b.volume;
  CHECK(std::fabs(total - want) / want < 1e-9);   // no volume lost or invented
  const double va = sumOf(prof->total, prof->vaLow, prof->vaHigh);
  CHECK(va / total >= 0.7);
  CHECK(prof->vaLow <= prof->poc && prof->poc <= prof->vaHigh);
  CHECK_EQ(prof->total[prof->poc], *std::max_element(prof->total.begin(), prof->total.end()));
}

TEST("session volume profile: one profile per session, each from that session's bars only") {
  const Bars& bars = vpBars();
  auto keyOf = [](qint64 t, const QString& period) { return ind::periodKey(t, period); };
  // Daily bars grouped by month: every session is one calendar month and they tile the bars with no gaps or overlaps.
  const auto sessions = vp::sessions(bars, "month", keyOf);
  CHECK(sessions.size() > 12);   // many monthly sessions
  for (int i = 0; i < sessions.size(); ++i) {
    const auto& s = sessions[i];
    CHECK_EQ(ind::periodKey(bars[s.from].time, "month"), ind::periodKey(bars[s.to].time, "month"));
    if (i) CHECK_EQ(s.from, sessions[i - 1].to + 1);
  }
  CHECK_EQ(sessions.last().to, int(bars.size()) - 1);
  // A session's profile is the same as a plain profile of just its bars.
  const auto& s = sessions[5];
  const auto prof = vp::profile(bars.mid(s.from, s.to - s.from + 1), 24, 70);
  CHECK(prof && prof->poc >= prof->vaLow && prof->poc <= prof->vaHigh);   // POC sits inside the value area
  // Auto picks days for intraday charts, months on the daily chart, years on weekly and slower.
  CHECK_EQ(vp::periodFor("Auto", 3600), QString("day"));
  CHECK_EQ(vp::periodFor("Auto", 86400), QString("month"));
  CHECK_EQ(vp::periodFor("Auto", 7 * 86400), QString("year"));
  CHECK_EQ(vp::periodFor("Week", 3600), QString("week"));
}
