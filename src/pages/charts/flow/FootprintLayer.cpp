#include "pages/charts/flow/FootprintLayer.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/Style.h"
#include "pages/charts/flow/FootprintFeed.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/Series.h"
#include "ui/Theme.h"
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <cmath>

namespace charts::ui::flow {

using ::charts::flow::FootprintRow;
using ::charts::flow::FootprintSettings;
using ::charts::flow::Side;
using Mode = FootprintSettings::Mode;

QString cellVolume(double v) {
  if (!(v > 0)) return {};
  if (v < 0.001) return QStringLiteral("<.001");
  if (v < 1) return QString::number(v, 'f', 3);
  if (v < 10) return QString::number(v, 'f', 2);
  if (v < 100) return QString::number(v, 'f', 1);
  if (v < 1000) return QString::number(v, 'f', 0);
  if (v < 1e4) return QString::number(v / 1e3, 'f', 2) + 'K';
  if (v < 1e5) return QString::number(v / 1e3, 'f', 1) + 'K';
  if (v < 1e6) return QString::number(v / 1e3, 'f', 0) + 'K';
  if (v < 1e7) return QString::number(v / 1e6, 'f', 2) + 'M';
  if (v < 1e8) return QString::number(v / 1e6, 'f', 1) + 'M';
  return QString::number(v / 1e6, 'f', 0) + 'M';
}

QString signedVolume(double v) {
  if (v == 0) return QStringLiteral("0");
  return (v > 0 ? QStringLiteral("+") : QStringLiteral("−")) + cellVolume(std::fabs(v));
}

FootprintLayer::FootprintLayer(ChartPane* pane, FootprintFeed* feed) : pane_(pane), feed_(feed) {}

void FootprintLayer::setSettings(const FootprintSettings& s) {
  if (s.rows != settings_.rows) lastGText_ = lastGColour_ = 0;
  settings_ = s;
}

// Smallest 1-2-5 × 10^k grouping whose rows are at least minPx tall; the current one stays while its rows are within
// [lo, hi] px (no flicker while zooming).
int64_t FootprintLayer::pickGroup(double pxPerTick, double minPx, double lo, double hi, int64_t& last) {
  if (last > 0) {
    const double px = pxPerTick * double(last);
    if (px >= lo && px <= hi) return last;
  }
  for (int64_t mag = 1; mag <= 1000000000; mag *= 10)
    for (int64_t s : {1, 2, 5}) {
      const int64_t g = s * mag;
      if (pxPerTick * double(g) >= minPx) return last = g;
    }
  return last = 1000000000;
}

void FootprintLayer::draw(QPainter& p, const QSize& size, const ::charts::plot::PrimitiveContext& c) {
  QElapsedTimer timer;
  timer.start();
  barsDrawn_ = 0;
  detail_ = Detail::Candles;
  struct Done {
    FootprintLayer* l;
    QElapsedTimer& t;
    ~Done() { l->lastMs_ = double(t.nsecsElapsed()) / 1e6; }
  } done{this, timer};
  if (!feed_ || !feed_->ready() || !series_ || !c.chart) return;
  const Bars& bars = pane_->shown;
  const int n = int(bars.size());
  const auto* ts = c.chart->timeScale();
  const auto range = ts->getVisibleLogicalRange();
  if (!range || !n) return;
  const double tick = feed_->ticks().step;
  const double H = size.height();
  const auto midPrice = series_->coordinateToPrice(H / 2);
  if (!midPrice) return;
  const auto ya = series_->priceToCoordinate(*midPrice), yb = series_->priceToCoordinate(*midPrice + tick);
  if (!ya || !yb) return;
  const double pxPerTick = std::fabs(*yb - *ya);
  if (!(pxPerTick > 0)) return;
  const Mode mode = settings_.mode;
  const bool autoRows = settings_.rows <= 0;
  const double spacing = ts->barSpacing();
  const double slotW = std::floor(spacing * 0.9);
  const double candleW = slotW >= 24 ? 3 : 1;
  const double cellsW = slotW - candleW - 2;

  // Level of detail. Numbers need rows ≥ 14 px (Auto) and room for the widest text of the mode; colours alone are
  // read on rows from 4 px. A fixed row size is used as is (no numbers if its rows are under 11 px).
  const int64_t gText = autoRows ? pickGroup(pxPerTick, 14, 11, 34, lastGText_) : settings_.rows;
  const int fontPx = std::clamp(clampToInt(pxPerTick * double(gText) * 0.62), 8, 12);
  const QFont font = theme::mono(fontPx), bold = theme::mono(fontPx, QFont::Bold), totalsFont = theme::mono(10);
  const QFontMetricsF fm(font), fmt(totalsFont);
  const double w5 = fm.horizontalAdvance(QStringLiteral("88.8K")), w6 = fm.horizontalAdvance(QStringLiteral("−88.8K"));
  const double needText = mode == Mode::BidAsk ? 2 * w5 + 10 : mode == Mode::Delta ? w6 + 8 : w5 + 24;
  Detail detail;
  int64_t g = gText;
  if (pxPerTick * double(gText) >= 11 && cellsW >= needText) detail = Detail::Numbers;
  else if (cellsW >= 3) {
    detail = Detail::Colours;
    if (autoRows) g = pickGroup(pxPerTick, 4, 3, 10, lastGColour_);
    if (pxPerTick * double(g) < 1.5) return;
  } else return;
  // bars from the stored history have levels of one basis point: rows never finer (a 1-2-5 step at least that tall)
  {
    const int from = std::max(0, clampToInt(std::floor(range->from))), to = std::min(n - 1, clampToInt(std::ceil(range->to), -1));
    bool stored = false;
    for (int i = from; i <= to && !stored; ++i) stored = feed_->fromArchive(bars[i].time);
    if (const int64_t lv = stored ? feed_->archiveMinGroup(*midPrice) : 0; lv > g) {
      int64_t m = 1;
      for (;; m *= 10) {
        if (m >= lv) { g = m; break; }
        if (2 * m >= lv) { g = 2 * m; break; }
        if (5 * m >= lv) { g = 5 * m; break; }
      }
      if (pxPerTick * double(g) < 1.5) return;
    }
  }
  lastG_ = g;
  detail_ = detail;
  const bool numbers = detail == Detail::Numbers;
  const bool totals = settings_.showTotals && spacing >= fmt.horizontalAdvance(QStringLiteral("Δ+88.8K")) + 4;

  ::charts::flow::FootprintParams prm = settings_.params();
  prm.groupTicks = g;
  const auto& sv = style::v();
  const QColor bg = c.chart->options().layout.background, text = sv.text, muted = sv.muted;
  const QColor buy = settings_.buyColor.isEmpty() ? sv.up : QColor(settings_.buyColor);
  const QColor sell = settings_.sellColor.isEmpty() ? sv.down : QColor(settings_.sellColor);
  const QColor poc = settings_.pocColor.isEmpty() ? sv.warn : QColor(settings_.pocColor);
  auto alpha = [](QColor col, double a) {
    col.setAlphaF(std::clamp(a, 0.0, 1.0));
    return col;
  };
  auto yOf = [&](double price) { return series_->priceToCoordinate(price); };
  // a row covers its ticks: from half a tick below its first one to half a tick above its last one
  auto rowSpan = [&](int64_t ticks, double& top, double& bot) {
    const auto a = yOf((double(ticks + g) - 0.5) * tick), b = yOf((double(ticks) - 0.5) * tick);
    if (!a || !b) return false;
    top = std::min(*a, *b);
    bot = std::max(*a, *b);
    return true;
  };

  const int from = std::max(0, clampToInt(std::floor(range->from)));
  const int to = std::min(n - 1, clampToInt(std::ceil(range->to), -1));
  p.setRenderHint(QPainter::Antialiasing, false);
  for (int i = from; i <= to; ++i) {
    const auto* cd = feed_->complete(bars[i].time);
    if (!cd) continue;
    const auto xo = ts->logicalToCoordinate(i);
    if (!xo) continue;
    ::charts::flow::analyze(*cd, prm, view_);
    if (view_.rows.empty()) continue;
    double top, bot, unused;
    if (!rowSpan(view_.rows.back().ticks, top, unused) || !rowSpan(view_.rows.front().ticks, unused, bot)) continue;
    if (bot < -40 || top > H) continue;
    const double left = std::round(*xo - slotW / 2);
    // the plain candle underneath goes: this slot is the footprint's
    p.fillRect(QRectF(left - 1, top - 1, slotW + 2, bot - top + 2), bg);

    // thin candle: open/close body, high/low wick (prices of the trades themselves)
    const auto yO = yOf(double(view_.open) * tick), yC = yOf(double(view_.close) * tick);
    const auto yH = yOf(double(view_.high) * tick), yL = yOf(double(view_.low) * tick);
    if (yO && yC && yH && yL) {
      const QColor cc = view_.close >= view_.open ? buy : sell;
      p.fillRect(QRectF(left + std::floor(candleW / 2), *yH, 1, std::max(1.0, *yL - *yH)), cc);
      p.fillRect(QRectF(left, std::min(*yO, *yC), candleW, std::max(1.0, std::fabs(*yC - *yO))), cc);
    }

    const double cx = left + candleW + 2, mid = std::round(cx + cellsW / 2), right = cx + cellsW;
    const double maxV = view_.maxRowVolume > 0 ? view_.maxRowVolume : 1;
    const double maxD = view_.maxAbsRowDelta > 0 ? view_.maxAbsRowDelta : 1;
    const double mark = cellsW >= 12 ? 3 : 1;   // imbalance marks of the Delta and Profile modes
    for (size_t k = 0; k < view_.rows.size(); ++k) {
      const FootprintRow& r = view_.rows[k];
      double rT, rB;
      if (!rowSpan(r.ticks, rT, rB) || rB < 0 || rT > H) continue;
      const double h = rB - rT > 3 ? rB - rT - 1 : rB - rT;   // 1 px between rows when there is room
      const bool sellImb = r.flags & ::charts::flow::SellImbalance, buyImb = r.flags & ::charts::flow::BuyImbalance;
      const bool isPoc = settings_.showPoc && int(k) == view_.poc;
      if (mode == Mode::BidAsk) {
        // two columns: sells hitting the bid | buys lifting the ask, shaded by volume, imbalances solid
        if (r.bid > 0) {
          const double a = sellImb ? (numbers ? 0.62 : 0.9) : (numbers ? 0.08 + 0.32 * r.bid / maxV : 0.15 + 0.6 * r.bid / maxV);
          p.fillRect(QRectF(cx, rT, mid - cx, h), alpha(sell, a));
        }
        if (r.ask > 0) {
          const double a = buyImb ? (numbers ? 0.62 : 0.9) : (numbers ? 0.08 + 0.32 * r.ask / maxV : 0.15 + 0.6 * r.ask / maxV);
          p.fillRect(QRectF(mid, rT, right - mid, h), alpha(buy, a));
        }
        if (numbers && (r.bid > 0 || r.ask > 0)) {
          p.setFont(sellImb ? bold : font);
          p.setPen(r.bid > 0 ? text : muted);
          p.drawText(QRectF(cx + 2, rT, mid - cx - 5, h), Qt::AlignRight | Qt::AlignVCenter, r.bid > 0 ? cellVolume(r.bid) : QStringLiteral("0"));
          p.setFont(buyImb ? bold : font);
          p.setPen(r.ask > 0 ? text : muted);
          p.drawText(QRectF(mid + 3, rT, right - mid - 5, h), Qt::AlignLeft | Qt::AlignVCenter, r.ask > 0 ? cellVolume(r.ask) : QStringLiteral("0"));
        }
      } else if (mode == Mode::Delta) {
        // one value per level: ask − bid, green or red by its sign, stronger the larger it is
        const double dlt = r.delta();
        if (dlt != 0) {
          const double a = (numbers ? 0.10 + 0.50 : 0.15 + 0.75) * std::fabs(dlt) / maxD + (numbers ? 0.0 : 0.05);
          p.fillRect(QRectF(cx, rT, cellsW, h), alpha(dlt > 0 ? buy : sell, a));
        }
        if (numbers && r.volume() > 0) {
          p.setFont(font);
          p.setPen(text);
          p.drawText(QRectF(cx + mark + 2, rT, cellsW - 2 * mark - 4, h), Qt::AlignCenter, signedVolume(dlt));
        }
      } else {
        // volume profile of the candle: one bar per level, its length the level's volume, sells then buys
        const double lb = cellsW * r.bid / maxV, la = cellsW * r.ask / maxV;
        const double a = isPoc ? 1.0 : (numbers ? 0.55 : 0.8);
        if (lb > 0) p.fillRect(QRectF(cx, rT, lb, h), alpha(sell, a));
        if (la > 0) p.fillRect(QRectF(cx + lb, rT, la, h), alpha(buy, a));
        if (numbers && r.volume() > 0) {
          p.setFont(isPoc ? bold : font);
          p.setPen(text);
          p.drawText(QRectF(cx + mark + 2, rT, cellsW - mark - 4, h), Qt::AlignLeft | Qt::AlignVCenter, cellVolume(r.volume()));
        }
      }
      if (mode != Mode::BidAsk) {   // imbalances: a mark on the side that wins (sellers left, buyers right)
        if (sellImb) p.fillRect(QRectF(cx, rT, mark, h), sell);
        if (buyImb) p.fillRect(QRectF(right - mark, rT, mark, h), buy);
      }
    }
    if (numbers && mode == Mode::BidAsk) p.fillRect(QRectF(mid, top, 1, bot - top), alpha(muted, 0.35));   // bid | ask
    // POC
    if (settings_.showPoc && view_.poc >= 0) {
      double a, b;
      if (rowSpan(view_.rows[size_t(view_.poc)].ticks, a, b)) {
        p.setPen(QPen(poc, numbers ? 1.5 : 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(cx - 0.5, a - 0.5, cellsW + 1, b - a));
      }
    }
    // stacked imbalances: a strip just right of the cells, over the rows of the zone
    for (const auto& s : view_.stacks) {
      double a, unusedB, unusedT, b;
      if (!rowSpan(view_.rows[size_t(s.toRow)].ticks, a, unusedB) || !rowSpan(view_.rows[size_t(s.fromRow)].ticks, unusedT, b)) continue;
      p.fillRect(QRectF(right + 1, a, 2, b - a), s.side == Side::Buy ? buy : sell);
    }
    // delta and volume of the candle, under it
    if (totals && bot + 4 < H) {
      p.setFont(totalsFont);
      const QRectF slot(left - 4, bot + 3, slotW + 8, fmt.height());
      p.setPen(view_.delta > 0 ? buy : view_.delta < 0 ? sell : muted);
      p.drawText(slot, Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("Δ") + signedVolume(view_.delta));
      p.setPen(muted);
      p.drawText(slot.translated(0, fmt.height()), Qt::AlignHCenter | Qt::AlignTop, cellVolume(view_.volume));
    }
    ++barsDrawn_;
  }
}

}  // namespace charts::ui::flow
