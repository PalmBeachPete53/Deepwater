#pragma once
// Charts tab — volume profiles drawn on the chart. Port of quant-terminal public/js/volume-profile.js
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
//
// Volume profile for the visible range (TradingView's VPVR). Volume per price row for the bars on screen, split into
// up and down volume, with the point of control (busiest row) and the value area (the rows around it holding 70% of
// the volume). Redraws as you scroll or zoom.
//
// The math (profile, sessions, periodFor) is header-only and needs QtCore only, so the core tests can include this
// header with CHARTS_VP_MATH_ONLY defined (tests/charts/core/volume_profile_tests.cpp).
#include "charts/core/Types.h"
#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>

namespace charts::ui::vp {

// JS VolumeProfile.profile(): rows of { lo, hi, up, down } plus poc and value-area indexes.
struct Profile {
  double lo = 0, step = 0;
  QVector<double> up, down, total;
  int poc = 0, vaLow = 0, vaHigh = 0;
  double max = 0;
};

inline std::optional<Profile> profile(const Bar* bars, qsizetype n, int rows, double valueAreaPct) {
  double hi = -std::numeric_limits<double>::infinity(), lo = std::numeric_limits<double>::infinity();
  // Math.max / Math.min: a NaN high or low poisons the result (then !isFinite(hi) → null)
  for (qsizetype i = 0; i < n; ++i) {
    const Bar& b = bars[i];
    hi = (std::isnan(hi) || std::isnan(b.high)) ? NaN : std::max(hi, b.high);
    lo = (std::isnan(lo) || std::isnan(b.low)) ? NaN : std::min(lo, b.low);
  }
  if (!std::isfinite(hi) || !(hi > lo)) return std::nullopt;
  const double step = (hi - lo) / rows;
  Profile p;
  p.lo = lo;
  p.step = step;
  p.up = QVector<double>(rows, 0.0);
  p.down = QVector<double>(rows, 0.0);
  for (qsizetype i = 0; i < n; ++i) {
    const Bar& b = bars[i];
    const double v = (std::isnan(b.volume) || b.volume == 0) ? 0 : b.volume;   // b.volume || 0
    if (!v) continue;
    const int a = int(std::min<double>(rows - 1, std::floor((b.low - lo) / step)));
    const int z = int(std::min<double>(rows - 1, std::floor((b.high - lo) / step)));
    const double share = v / (z - a + 1);
    QVector<double>& side = b.close >= b.open ? p.up : p.down;
    for (int k = a; k <= z; ++k) side[k] += share;
  }
  p.total.resize(rows);
  double sum = 0;
  for (int k = 0; k < rows; ++k) { p.total[k] = p.up[k] + p.down[k]; sum += p.total[k]; }
  if (!sum) return std::nullopt;
  int poc = 0;
  for (int k = 0; k < rows; ++k)
    if (p.total[k] > p.total[poc]) poc = k;
  double inside = p.total[poc];
  int a = poc, z = poc;
  while (inside < (sum * valueAreaPct) / 100 && (a > 0 || z < rows - 1)) {
    const double below = a > 0 ? p.total[a - 1] : -1, above = z < rows - 1 ? p.total[z + 1] : -1;
    if (above >= below) inside += p.total[++z];
    else inside += p.total[--a];
  }
  p.poc = poc;
  p.vaLow = a;
  p.vaHigh = z;
  p.max = *std::max_element(p.total.begin(), p.total.end());
  return p;
}
inline std::optional<Profile> profile(const Bars& bars, int rows, double valueAreaPct) {
  return profile(bars.constData(), bars.size(), rows, valueAreaPct);
}

// JS SessionVolumeProfile.periodFor: sessions are days on intraday charts; on daily and slower charts "Auto" uses
// months, then years.
inline QString periodFor(const QString& choice, double tfSeconds) {
  if (!choice.isEmpty() && choice != "Auto") return choice.toLower();
  if (tfSeconds < 86400) return "day";
  if (tfSeconds < 7 * 86400) return "month";
  return "year";
}

// JS SessionVolumeProfile.sessions: [{ from, to }] index ranges of consecutive bars sharing a session.
struct Session { int from = 0, to = 0; };
inline QVector<Session> sessions(const Bars& bars, const QString& period,
                                 const std::function<QString(qint64, const QString&)>& keyOf) {
  QVector<Session> out;
  QString key;
  bool hasKey = false;
  for (int i = 0; i < bars.size(); ++i) {
    const QString k = keyOf(bars[i].time, period);
    if (!hasKey || k != key) {
      out.append({i, i});
      key = k;
      hasKey = true;
    } else out.last().to = i;
  }
  return out;
}

}  // namespace charts::ui::vp

#ifndef CHARTS_VP_MATH_ONLY
#include "pages/charts/plot/Primitive.h"
#include <QJsonObject>

namespace charts::ui {

class ChartPane;

class VolumeProfile : public ::charts::plot::Primitive {
public:
  // params: { rows: 24, width: 30, valueArea: 70 } (JS defaults), overridden by the indicator's res.volumeProfile.
  VolumeProfile(ChartPane* pane, const QJsonObject& params = {});
  void attached(const ::charts::plot::AttachedParams& p) override { series = p.series; chart = p.chart; }
  void detached() override { series = nullptr; }
  ::charts::plot::PrimitiveZOrder zOrder() const override { return ::charts::plot::PrimitiveZOrder::Bottom; }
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;

protected:
  double param(const QString& k) const;
  ChartPane* pane;
  QJsonObject p;
  ::charts::plot::Series* series = nullptr;
  ::charts::plot::Chart* chart = nullptr;
};

// Session volume profile (TradingView's SVP): one profile per session, drawn from the session's first bar, using
// only that session's bars.
class SessionVolumeProfile : public VolumeProfile {
public:
  // params: { period: "Auto", rows: 24, width: 70, valueArea: 70 } (JS defaults).
  SessionVolumeProfile(ChartPane* pane, const QJsonObject& params = {});
  void draw(QPainter& ctx, const QSize& size, const ::charts::plot::PrimitiveContext& c) override;
  std::function<QString(qint64, const QString&)> keyOf;   // charts::ind::periodKey
  double tfSeconds = 0;

private:
  // the sessions of the bars last drawn: finding them makes a QString key per bar, so they are found again only when
  // the bars' times can have changed (count, first or last time) or the period did, not at every frame
  QVector<vp::Session> sessions_;
  qsizetype sessBars_ = -1;
  qint64 sessFirst_ = 0, sessLast_ = 0;
  QString sessPeriod_;
};

}  // namespace charts::ui
#endif
