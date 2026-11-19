#include "pages/charts/flow/SmcLayer.h"
#include "charts/core/Resampler.h"
#include "charts/core/Series.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/Style.h"
#include "pages/charts/plot/Series.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace charts::ui::flow {

using namespace ::charts::smc;

bool SmcLayer::Config::operator==(const Config& o) const {
  const EngineParams &a = engine, &b = o.engine;
  bool sess = a.periods.sessions.size() == b.periods.sessions.size();
  for (size_t i = 0; sess && i < a.periods.sessions.size(); ++i)
    sess = a.periods.sessions[i].kind == b.periods.sessions[i].kind && a.periods.sessions[i].tz == b.periods.sessions[i].tz &&
           a.periods.sessions[i].start == b.periods.sessions[i].start && a.periods.sessions[i].end == b.periods.sessions[i].end;
  return sess && detectTf == o.detectTf && chartTfSeconds == o.chartTfSeconds && a.swing.method == b.swing.method && a.swing.left == b.swing.left &&
         a.swing.right == b.swing.right && a.swing.percent == b.swing.percent && a.swing.atrMult == b.swing.atrMult &&
         a.toleranceUnit == b.toleranceUnit && a.tolerance == b.tolerance && a.thicknessUnit == b.thicknessUnit && a.thickness == b.thickness &&
         a.tick == b.tick && a.sweep == b.sweep && a.periodLevels == b.periodLevels && a.periods.day == b.periods.day &&
         a.periods.week == b.periods.week && a.periods.tz == b.periods.tz && a.periods.dayStartMinutes == b.periods.dayStartMinutes;
}

QString smcLabel(const Level& l) {
  const bool hi = l.side == LevelSide::BuySide;
  switch (l.kind) {
    case LevelKind::Equal: return QString(hi ? "EQH ×%1" : "EQL ×%1").arg(l.members);
    case LevelKind::PrevDay: return hi ? "PDH" : "PDL";
    case LevelKind::PrevWeek: return hi ? "PWH" : "PWL";
    case LevelKind::Asia: return hi ? "Asia H" : "Asia L";
    case LevelKind::London: return hi ? "London H" : "London L";
    case LevelKind::NewYork: return hi ? "NY H" : "NY L";
    default: return hi ? "BSL" : "SSL";
  }
}

SmcLayer::SmcLayer(ChartPane* pane) : pane_(pane) {}

void SmcLayer::setConfig(const Config& c) {
  if (configured_ && c == cfg_) return;
  cfg_ = c;
  configured_ = true;
  engine_.reset();   // a full run with the new settings
  if (!bars_.isEmpty()) {
    rebuild();
    publish();
  }
}

void SmcLayer::setLook(const Look& l) {
  if (l == look_) return;
  look_ = l;
  requestRepaint();
}

void SmcLayer::setBars(const Bars& bars) {
  QByteArray key;
  auto add = [&](auto v) { key.append(reinterpret_cast<const char*>(&v), sizeof v); };
  add(qsizetype(bars.size()));
  if (!bars.isEmpty()) {
    add(bars.front().time);
    add(bars.back().time);
    add(bars.back().high);
    add(bars.back().low);
    add(bars.back().close);
  }
  if (key == key_) return;
  key_ = key;
  bars_ = bars;
  if (!configured_ || bars_.isEmpty()) return;
  // the same history as what the engine has (same first bar, the last fed bar unchanged): only the new closed bars
  const bool same = engine_ && fed_ <= bars_.size() && fed_ > 0 && bars_.front().time == firstTime_ && bars_[fed_ - 1].time == lastFed_.time &&
                    bars_[fed_ - 1].high == lastFed_.high && bars_[fed_ - 1].low == lastFed_.low && bars_[fed_ - 1].close == lastFed_.close;
  if (!same) rebuild();
  else {
    QElapsedTimer t;
    t.start();
    feed(closedCount());
    stats_.computeMs = double(t.nsecsElapsed()) / 1e6;
  }
  publish();
}

int SmcLayer::closedCount() const {
  // every bar but the last, and the last once its time is over
  int n = int(bars_.size());
  if (n && bars_.back().time + cfg_.chartTfSeconds > QDateTime::currentSecsSinceEpoch()) --n;
  return n;
}

void SmcLayer::rebuild() {
  QElapsedTimer t;
  t.start();
  ++stats_.fullRuns;
  htfSec_ = 0;
  resampler_.reset();
  if (!cfg_.detectTf.isEmpty()) {
    try { htfSec_ = qint64(series::parseTf(cfg_.detectTf).seconds); } catch (...) { htfSec_ = 0; }
    if (htfSec_ <= cfg_.chartTfSeconds) htfSec_ = 0;
    else resampler_.emplace(cfg_.detectTf, cfg_.engine.periods.tz, cfg_.engine.periods.dayStartMinutes);
  }
  EngineParams ep = cfg_.engine;
  ep.tfSeconds = htfSec_ ? int(htfSec_) : cfg_.chartTfSeconds;
  engine_ = std::make_unique<LiquidityLevelEngine>(ep);
  engine_->reserve(size_t(bars_.size()));
  fed_ = 0;
  pendingFrom_ = 0;
  firstTime_ = bars_.isEmpty() ? 0 : bars_.front().time;
  feed(closedCount());
  stats_.computeMs = double(t.nsecsElapsed()) / 1e6;
}

void SmcLayer::feed(int upTo) {
  for (int i = fed_; i < upTo; ++i) {
    const Bar& b = bars_[i];
    if (resampler_) {
      // a larger timeframe: its bar goes in when the next one starts (only closed ones make levels)
      if (auto c = resampler_->push(b)) {
        engine_->step(*c);
        pendingFrom_ = i;   // this chart bar opened a new detection bar: it and those after are not in the engine
      }
    } else {
      engine_->step(b);
      pendingFrom_ = i + 1;
    }
    lastFed_ = b;
    fed_ = i + 1;
  }
  stats_.fed = fed_;
}

void SmcLayer::publish() {
  if (!engine_) return;
  shown_.clear();
  shown_.reserve(engine_->levels().size());
  const EngineParams& ep = cfg_.engine;
  auto barAt = [&](qint64 time) {
    return int(std::upper_bound(bars_.begin(), bars_.end(), time, [](qint64 x, const Bar& b) { return x < b.time; }) - bars_.begin()) - 1;
  };
  auto crosses = [&](const Level& l, const Bar& b) {
    const bool buy = l.side == LevelSide::BuySide;
    if (ep.sweep == SweepRule::Wick) return buy ? b.high > l.price : b.low < l.price;
    return buy ? b.close > l.price : b.close < l.price;
  };
  for (const Level& l : engine_->levels()) {
    Shown s;
    s.level = l;
    s.score = engine_->score(l);
    s.fromTime = l.originTime;
    if (l.swept) {
      s.sweptTime = l.sweptTime;
      // on a larger timeframe: the first chart bar inside the sweeping bar that went through the level
      if (htfSec_) {
        for (int i = std::max(0, barAt(l.sweptTime)); i < bars_.size() && bars_[i].time < l.sweptTime + htfSec_; ++i)
          if (crosses(l, bars_[i])) { s.sweptTime = bars_[i].time; break; }
      }
      s.sweptBar = barAt(s.sweptTime);
    } else {
      // the chart bars not in the engine yet (the forming bar; on a larger timeframe, its forming bar's chart bars)
      for (int i = pendingFrom_; i < bars_.size(); ++i)
        if (crosses(l, bars_[i])) {
          s.sweptTime = bars_[i].time;
          s.sweptBar = i;
          s.level.swept = true;
          break;
        }
    }
    shown_.push_back(s);
  }
  stats_.total = int(shown_.size());
  stats_.active = int(std::count_if(shown_.begin(), shown_.end(), [](const Shown& s) { return !s.level.swept; }));
  requestRepaint();
}

void SmcLayer::draw(QPainter& p, const QSize& size, const ::charts::plot::PrimitiveContext&) {
  QElapsedTimer t;
  t.start();
  stats_.drawn = 0;
  if (!series_ || shown_.empty() || bars_.isEmpty()) return;
  const int W = size.width(), H = size.height();
  const auto top = series_->coordinateToPrice(0), bottom = series_->coordinateToPrice(H);
  if (!top || !bottom) return;
  const double pMax = std::max(*top, *bottom), pMin = std::min(*top, *bottom);
  const int lastBar = int(bars_.size()) - 1;
  const auto& sv = style::v();
  const QColor buy = look_.buy.isValid() ? look_.buy : sv.down, sell = look_.sell.isValid() ? look_.sell : sv.up;
  QColor grey = sv.muted;
  // the strongest level on screen is opaque, the others by their score
  double maxScore = 0;
  for (const Shown& s : shown_) maxScore = std::max(maxScore, s.score);
  QFont f = p.font();
  f.setPixelSize(10);
  p.setFont(f);
  p.setRenderHint(QPainter::Antialiasing, false);
  std::vector<QRectF> labelsDrawn;   // labels never overlap: moved a line away, or left out
  for (const Shown& s : shown_) {
    const Level& l = s.level;
    if (s.score < look_.minScore) continue;
    if (l.swept && (look_.keepAfterSweep <= 0 || lastBar - s.sweptBar > look_.keepAfterSweep)) continue;
    if (l.zoneHigh < pMin || l.zoneLow > pMax) continue;   // outside the visible prices
    const auto xa = pane_->timeToX(s.fromTime);
    double x0 = xa ? *xa : 0, x1 = W;
    if (l.swept) {
      const auto xb = pane_->timeToX(s.sweptTime);
      if (!xb) continue;
      x1 = *xb;
    }
    if (x1 < 0 || x0 > W) continue;
    x0 = std::max(x0, 0.0);
    const auto yp = series_->priceToCoordinate(l.price), yz = series_->priceToCoordinate(l.side == LevelSide::BuySide ? l.zoneHigh : l.zoneLow);
    if (!yp || !yz) continue;
    const double alpha = l.swept ? 0.35 : 0.25 + 0.75 * (maxScore > 0 ? s.score / maxScore : 1);
    QColor c = l.swept ? grey : (l.side == LevelSide::BuySide ? buy : sell);
    if (look_.zones) {
      QColor fill = c;
      fill.setAlphaF(0.18 * alpha);
      const double ya = std::min(*yp, *yz), yb = std::max(*yp, *yz);
      p.fillRect(QRectF(x0, ya, x1 - x0, std::max(1.0, yb - ya)), fill);
    }
    QColor line = c;
    line.setAlphaF(std::min(1.0, 0.9 * alpha));
    QPen pen(line, l.kind == LevelKind::Equal || l.kind == LevelKind::PrevWeek ? 2 : 1);
    if (l.kind >= LevelKind::PrevDay) pen.setStyle(Qt::DashLine);   // period and session levels: dashed
    p.setPen(pen);
    p.drawLine(QPointF(x0, *yp), QPointF(x1, *yp));
    if (look_.labels) {
      const QString text = smcLabel(l);
      const double tw = p.fontMetrics().horizontalAdvance(text);
      // std::clamp needs lo <= hi: a level starting near the right edge had x0 + 2 > W - tw - 2
      const double tx = std::clamp(x1 - tw - 4, x0 + 2, std::max(x0 + 2, double(W) - tw - 2));
      const double lh = 11, dir = l.side == LevelSide::BuySide ? -1 : 1;
      double ty = l.side == LevelSide::BuySide ? *yp - 3 : *yp + 11;
      for (int tries = 0; tries < 3; ++tries) {
        const QRectF r(tx, ty - lh + 2, tw, lh);
        if (std::none_of(labelsDrawn.begin(), labelsDrawn.end(), [&](const QRectF& o) { return o.intersects(r); })) {
          p.drawText(QPointF(tx, ty), text);
          labelsDrawn.push_back(r);
          break;
        }
        ty += dir * lh;   // away from the line, on the level's side
      }
    }
    ++stats_.drawn;
  }
  stats_.drawMs = double(t.nsecsElapsed()) / 1e6;
}

}  // namespace charts::ui::flow
