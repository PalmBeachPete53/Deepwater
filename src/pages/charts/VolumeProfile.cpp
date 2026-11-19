// Port of quant-terminal public/js/volume-profile.js (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include "pages/charts/VolumeProfile.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/plot/Chart.h"
#include "ui/Theme.h"

namespace charts::ui {

namespace {
QColor rgba(int r, int g, int b, double a) { return QColor(r, g, b, int(std::lround(a * 255))); }
const QColor kOrange("#ff9800");
}  // namespace

VolumeProfile::VolumeProfile(ChartPane* pane_, const QJsonObject& params) : pane(pane_) {
  p = QJsonObject{{"rows", 24}, {"width", 30}, {"valueArea", 70}};
  for (auto it = params.begin(); it != params.end(); ++it) p.insert(it.key(), it.value());
}

double VolumeProfile::param(const QString& k) const { return charts::num(p, k, NaN); }

void VolumeProfile::draw(QPainter& ctx, const QSize& size, const plot::PrimitiveContext&) {
  const Bars& bars = pane->shown;
  if (!chart || !series || bars.isEmpty()) return;
  const auto range = chart->timeScale()->getVisibleLogicalRange();
  if (!range) return;
  // the visible range is on the time scale: with a compared symbol on another calendar its indexes are not bar indexes
  int from = pane->barAtLogical(std::floor(range->from));
  if (from + 1 < bars.size() && pane->logicalOfBar(from) < std::floor(range->from)) ++from;
  const int to = pane->barAtLogical(std::ceil(range->to));
  if (to <= from) return;
  const int rows = std::max(4, std::min(200, int(param("rows"))));
  const auto prof = vp::profile(bars.constData() + from, to - from + 1, rows, param("valueArea"));
  if (!prof) return;
  const double W = size.width() * std::max(5.0, std::min(80.0, param("width"))) / 100;
  const double x0 = size.width() - W;
  for (int k = 0; k < prof->total.size(); ++k) {
    const auto yTop = series->priceToCoordinate(prof->lo + (k + 1) * prof->step);
    const auto yBot = series->priceToCoordinate(prof->lo + k * prof->step);
    if (!yTop || !yBot) continue;
    const double h = std::max(1.0, *yBot - *yTop - 1);
    const bool inVa = k >= prof->vaLow && k <= prof->vaHigh;
    const double uw = prof->up[k] / prof->max * W, dw = prof->down[k] / prof->max * W;
    ctx.fillRect(QRectF(x0 + W - uw - dw, *yTop, uw, h), inVa ? rgba(8, 153, 129, 0.45) : rgba(8, 153, 129, 0.2));
    ctx.fillRect(QRectF(x0 + W - dw, *yTop, dw, h), inVa ? rgba(242, 54, 69, 0.45) : rgba(242, 54, 69, 0.2));
  }
  const auto yPoc = series->priceToCoordinate(prof->lo + (prof->poc + 0.5) * prof->step);
  if (yPoc) {
    QPen pen(kOrange, 1);
    pen.setDashPattern({4, 3});
    ctx.setPen(pen);
    ctx.drawLine(QPointF(0, *yPoc), QPointF(size.width(), *yPoc));
    ctx.setPen(kOrange);
    ctx.setFont(theme::sans(10));
    const QString t = "POC";
    const double w = QFontMetricsF(ctx.font()).horizontalAdvance(t);
    ctx.drawText(QPointF(size.width() - 4 - w, *yPoc - 4), t);   // textAlign right, alphabetic baseline
  }
}

SessionVolumeProfile::SessionVolumeProfile(ChartPane* pane_, const QJsonObject& params) : VolumeProfile(pane_) {
  p = QJsonObject{{"period", "Auto"}, {"rows", 24}, {"width", 70}, {"valueArea", 70}};
  for (auto it = params.begin(); it != params.end(); ++it) p.insert(it.key(), it.value());
}

void SessionVolumeProfile::draw(QPainter& ctx, const QSize&, const plot::PrimitiveContext&) {
  const Bars& bars = pane->shown;
  if (!chart || !series || bars.isEmpty() || !keyOf) return;
  plot::TimeScale* ts = chart->timeScale();
  const auto range = ts->getVisibleLogicalRange();
  if (!range) return;
  const QString period = vp::periodFor(p.value("period").toString(), tfSeconds ? tfSeconds : 86400);
  const int rows = std::max(4, std::min(200, int(param("rows"))));
  // ts.options().barSpacing: Lightweight Charts keeps it equal to the current spacing (zoom included)
  const double spacing = ts->barSpacing() ? ts->barSpacing() : 6;
  if (bars.size() != sessBars_ || bars.front().time != sessFirst_ || bars.back().time != sessLast_ || period != sessPeriod_) {
    sessions_ = vp::sessions(bars, period, keyOf);
    sessBars_ = bars.size();
    sessFirst_ = bars.front().time;
    sessLast_ = bars.back().time;
    sessPeriod_ = period;
  }
  for (const vp::Session& s : sessions_) {
    // s.from / s.to are bar indexes; the range and the coordinates are on the time scale (D2: other calendars)
    const double la = pane->logicalOfBar(s.from), lz = pane->logicalOfBar(s.to);
    if (lz < range->from - 1 || la > range->to + 1) continue;
    const auto prof = vp::profile(bars.constData() + s.from, s.to - s.from + 1, rows, param("valueArea"));
    const auto xa = ts->logicalToCoordinate(la), xz = ts->logicalToCoordinate(lz);
    if (!prof || !xa || !xz) continue;
    const double x0 = *xa - spacing / 2, span = std::max(spacing, *xz - *xa + spacing);
    const double W = span * std::max(5.0, std::min(100.0, param("width"))) / 100;
    for (int k = 0; k < prof->total.size(); ++k) {
      const auto yTop = series->priceToCoordinate(prof->lo + (k + 1) * prof->step);
      const auto yBot = series->priceToCoordinate(prof->lo + k * prof->step);
      if (!yTop || !yBot) continue;
      const double h = std::max(1.0, *yBot - *yTop - 1);
      const bool inVa = k >= prof->vaLow && k <= prof->vaHigh;
      const double uw = prof->up[k] / prof->max * W, dw = prof->down[k] / prof->max * W;
      ctx.fillRect(QRectF(x0, *yTop, uw, h), inVa ? rgba(8, 153, 129, 0.4) : rgba(8, 153, 129, 0.16));
      ctx.fillRect(QRectF(x0 + uw, *yTop, dw, h), inVa ? rgba(242, 54, 69, 0.4) : rgba(242, 54, 69, 0.16));
    }
    const auto yPoc = series->priceToCoordinate(prof->lo + (prof->poc + 0.5) * prof->step);
    if (yPoc) {
      ctx.setPen(QPen(kOrange, 1.5));
      ctx.drawLine(QPointF(x0, *yPoc), QPointF(x0 + span, *yPoc));
    }
  }
}

}  // namespace charts::ui
