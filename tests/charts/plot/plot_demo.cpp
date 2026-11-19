// Demo of charts::plot: renders the PNGs of docs/charts/shots/plot offscreen and measures scrolling speed.
// The set-up code follows quant-terminal/public/js/chart.js (createChart, makeMainSeries, renderIndicators) so the
// shots can be compared with the original app on the same fixture (quant-terminal/test/fixtures/btc-1d.json).
#include "pages/charts/plot/Chart.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <cstdio>
#include <sys/resource.h>

using namespace charts::plot;

namespace {

struct Bar { qint64 time; double open, high, low, close, volume; };

QVector<Bar> loadFixture() {
  QFile f(QStringLiteral(CHARTS_FIXTURES) + "/btc-1d.json");
  if (!f.open(QIODevice::ReadOnly)) qFatal("fixture missing");
  QVector<Bar> out;
  for (const auto& v : QJsonDocument::fromJson(f.readAll()).array()) {
    const auto o = v.toObject();
    out.append({qint64(o["time"].toDouble()), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(),
                o["close"].toDouble(), o["volume"].toDouble()});
  }
  return out;
}

// Perf only: the 1000 fixture bars tiled 5 times (each copy rescaled to start where the previous one ended).
QVector<Bar> tiled(const QVector<Bar>& b, int copies) {
  QVector<Bar> out;
  double k = 1;
  qint64 t = b.first().time;
  for (int c = 0; c < copies; ++c) {
    if (c) k = out.last().close / b.first().open;
    for (const Bar& x : b) {
      out.append({t, x.open * k, x.high * k, x.low * k, x.close * k, x.volume});
      t += 86400;
    }
  }
  return out;
}

QVector<double> closes(const QVector<Bar>& b) { QVector<double> c; for (auto& x : b) c.append(x.close); return c; }

QVector<double> ema(const QVector<double>& v, int n) {
  QVector<double> out(v.size(), qQNaN());
  double e = 0, sum = 0;
  for (int i = 0; i < v.size(); ++i) {
    if (i < n) { sum += v[i]; if (i == n - 1) { e = sum / n; out[i] = e; } continue; }
    e = (v[i] - e) * 2.0 / (n + 1) + e;
    out[i] = e;
  }
  return out;
}

QVector<double> sma(const QVector<double>& v, int n) {
  QVector<double> out(v.size(), qQNaN());
  double sum = 0; int cnt = 0, start = -1;
  for (int i = 0; i < v.size(); ++i) {
    if (!std::isfinite(v[i])) continue;
    if (start < 0) start = i;
    sum += v[i]; ++cnt;
    if (cnt > n) sum -= v[i - n];
    if (cnt >= n) out[i] = sum / n;
  }
  return out;
}

QVector<double> rsi(const QVector<double>& c, int n) {   // Wilder
  QVector<double> out(c.size(), qQNaN());
  double up = 0, dn = 0;
  for (int i = 1; i < c.size(); ++i) {
    const double d = c[i] - c[i - 1], u = qMax(d, 0.0), w = qMax(-d, 0.0);
    if (i <= n) { up += u; dn += w; if (i == n) { up /= n; dn /= n; out[i] = dn == 0 ? 100 : 100 - 100 / (1 + up / dn); } continue; }
    up = (up * (n - 1) + u) / n; dn = (dn * (n - 1) + w) / n;
    out[i] = dn == 0 ? 100 : 100 - 100 / (1 + up / dn);
  }
  return out;
}

// chart.js lineData(): a blank can't be skipped, so the point before a gap is transparent and the gap's points sit
// on the next real value.
QVector<DataPoint> lineData(const QVector<double>& vals, const QVector<qint64>& times) {
  const QColor clear(0, 0, 0, 0);
  QVector<DataPoint> out;
  QVector<double> next(vals.size());
  double nv = qQNaN();
  for (int i = vals.size() - 1; i >= 0; --i) { if (std::isfinite(vals[i])) nv = vals[i]; next[i] = nv; }
  bool started = false;
  for (int i = 0; i < vals.size(); ++i) {
    const double v = vals[i];
    if (!std::isfinite(v)) {
      out.append(started && std::isfinite(next[i]) ? DataPoint::val(times[i], next[i], clear) : DataPoint::whitespace(times[i]));
      continue;
    }
    started = true;
    const bool gapAhead = i + 1 < vals.size() && !std::isfinite(vals[i + 1]);
    out.append(gapAhead ? DataPoint::val(times[i], v, clear) : DataPoint::val(times[i], v));
  }
  return out;
}

// Colours: quant-terminal's dark CSS (to compare with the original) or Deepwater's theme (what the pane will pass).
struct Colors { QColor bg, text, border, grid, muted, accent, accentSoft, up, down; };
Colors qtDark() {
  return {QColor("#131722"), QColor("#d1d4dc"), QColor("#2a2e39"), cssColor("rgba(42, 46, 57, 0.6)"), QColor("#787b86"),
          QColor("#2962ff"), cssColor("rgba(41, 98, 255, 0.18)"), QColor("#089981"), QColor("#f23645")};
}
Colors deepwater() {
  const auto& p = theme::pal();
  return {p.bg, p.text, p.border, p.grid, p.textMute, QColor("#2962ff"), theme::alpha(p.accent, 46), p.up, p.down};
}

// chart.js fmtPrice
QString fmtPrice(double v) {
  if (!std::isfinite(v)) return "—";
  const double a = std::fabs(v);
  const int d = a >= 1000 ? 2 : a >= 1 ? (a >= 100 ? 2 : 4) : a >= 0.01 ? 6 : 8;
  QLocale loc(QLocale::English);
  QString s = loc.toString(v, 'f', d);
  // minimumFractionDigits = min(d, 2): trim zeros beyond 2 decimals
  if (d > 2) { while (s.endsWith('0') && s.indexOf('.') >= 0 && s.size() - s.indexOf('.') - 1 > 2) s.chop(1); }
  return s;
}

void themeChart(Chart* c, const Colors& k) {
  c->applyOptions([&](ChartOptions& o) {
    o.layout.background = k.bg;
    o.layout.textColor = k.text;
    o.layout.fontSize = 11;
    o.layout.panes.separatorColor = k.border;
    o.layout.panes.separatorHoverColor = k.accentSoft;
    o.layout.panes.enableResize = true;
    o.grid.vertLines.color = k.grid;
    o.grid.horzLines.color = k.grid;
    o.rightPriceScale.borderColor = k.border;
    o.timeScale.borderColor = k.border;
  });
}

// createChart() + makeMainSeries() + render() of chart.js for one chart.
struct Built {
  Chart* chart; Series* main; Series* volume = nullptr; QVector<qint64> times;
};

Built buildChart(QWidget* parent, const QVector<Bar>& bars, const Colors& k, const QString& type = "candles",
                 bool indicators = true, bool macd = false) {
  Built b;
  b.chart = new Chart(parent);
  Chart* c = b.chart;
  themeChart(c, k);
  c->applyOptions([&](ChartOptions& o) {
    o.crosshair.mode = CrosshairMode::Normal;
    o.timeScale.rightOffset = 8;
    o.timeScale.timeVisible = true;
    o.timeScale.secondsVisible = false;
    o.localization.priceFormatter = fmtPrice;
  });
  auto common = [](SeriesOptions& o) { o.priceLineVisible = true; o.lastValueVisible = true; };
  if (type == "bars")
    b.main = c->addSeries(BarSeries, [&](SeriesOptions& o) { common(o); o.upColor = k.up; o.downColor = k.down; });
  else if (type == "line" || type == "linemarkers" || type == "step")
    b.main = c->addSeries(LineSeries, [&](SeriesOptions& o) {
      common(o); o.color = k.accent; o.lineWidth = 2; o.lineType = type == "step" ? LineType::WithSteps : LineType::Simple;
      o.pointMarkersVisible = type == "linemarkers";
    });
  else if (type == "area")
    b.main = c->addSeries(AreaSeries, [&](SeriesOptions& o) {
      common(o); o.lineColor = k.accent; o.topColor = cssColor("rgba(41,98,255,0.35)"); o.bottomColor = cssColor("rgba(41,98,255,0.02)"); o.lineWidth = 2;
    });
  else if (type == "baseline")
    b.main = c->addSeries(BaselineSeries, [&](SeriesOptions& o) {
      common(o); o.topLineColor = k.up; o.bottomLineColor = k.down;
      o.topFillColor1 = cssColor("rgba(8,153,129,0.25)"); o.topFillColor2 = cssColor("rgba(8,153,129,0.02)");
      o.bottomFillColor1 = cssColor("rgba(242,54,69,0.02)"); o.bottomFillColor2 = cssColor("rgba(242,54,69,0.25)");
    });
  else if (type == "columns")
    b.main = c->addSeries(HistogramSeries, [&](SeriesOptions& o) { common(o); });
  else if (type == "hollow")
    b.main = c->addSeries(CandlestickSeries, [&](SeriesOptions& o) {
      common(o); o.upColor = QColor(0, 0, 0, 0); o.downColor = k.down; o.borderUpColor = k.up; o.borderDownColor = k.down;
      o.wickUpColor = k.up; o.wickDownColor = k.down; o.borderVisible = true;
    });
  else
    b.main = c->addSeries(CandlestickSeries, [&](SeriesOptions& o) {
      common(o); o.upColor = k.up; o.downColor = k.down; o.borderVisible = false; o.wickUpColor = k.up; o.wickDownColor = k.down;
    });
  b.volume = c->addSeries(HistogramSeries, [](SeriesOptions& o) {
    o.priceScaleId = "vol"; o.priceFormat = PriceFormat::volume(); o.lastValueVisible = false; o.priceLineVisible = false;
  });
  c->priceScale("vol")->applyOptions([](PriceScaleOptions& o) { o.scaleMargins = {0.82, 0}; });

  // render(true)
  const double last = bars.last().close;
  const int precision = last >= 100 ? 2 : last >= 1 ? 4 : last >= 0.01 ? 6 : 8;
  b.main->applyOptions([&](SeriesOptions& o) { o.priceFormat = PriceFormat::price(precision, std::pow(10.0, -precision)); });
  if (type == "baseline") b.main->applyOptions([&](SeriesOptions& o) { o.baseValue = bars[qMax(0, int(bars.size()) - 150)].close; });
  QVector<DataPoint> pts, vol;
  for (const Bar& x : bars) {
    b.times.append(x.time);
    if (type == "candles" || type == "hollow" || type == "bars") pts.append(DataPoint::ohlc(x.time, x.open, x.high, x.low, x.close));
    else if (type == "columns") pts.append(DataPoint::val(x.time, x.close, x.close >= x.open ? k.up : k.down));
    else pts.append(DataPoint::val(x.time, x.close));
    vol.append(DataPoint::val(x.time, x.volume, cssColor(x.close >= x.open ? "rgba(8,153,129,0.45)" : "rgba(242,54,69,0.45)")));
  }
  b.main->setData(pts);
  b.volume->setData(vol);
  c->applyOptions([](ChartOptions& o) { o.timeScale.timeVisible = false; o.timeScale.secondsVisible = false; });   // 1D: not intraday
  if (indicators) {
    const auto cl = closes(bars);
    auto* e = c->addSeries(LineSeries, [&](SeriesOptions& o) {
      o.color = QColor("#2962ff"); o.priceLineVisible = false; o.lastValueVisible = false; o.crosshairMarkerVisible = false; o.title = ""; o.lineWidth = 1;
    }, 0);
    e->setData(lineData(ema(cl, 21), b.times));
    const auto r = rsi(cl, 14);
    auto* rs = c->addSeries(LineSeries, [&](SeriesOptions& o) {
      o.color = QColor("#7e57c2"); o.priceLineVisible = false; o.lastValueVisible = true; o.crosshairMarkerVisible = false; o.lineWidth = 1;
    }, 1);
    rs->setData(lineData(r, b.times));
    auto* ma = c->addSeries(LineSeries, [&](SeriesOptions& o) {
      o.color = QColor("#f7c600"); o.priceLineVisible = false; o.lastValueVisible = true; o.crosshairMarkerVisible = false; o.lineWidth = 1;
    }, 1);
    ma->setData(lineData(sma(r, 14), b.times));
    for (double lv : {30.0, 50.0, 70.0}) {
      PriceLineOptions pl;
      pl.price = lv; pl.color = k.muted; pl.lineWidth = 1; pl.lineStyle = LineStyle::Dashed; pl.axisLabelVisible = false;
      rs->createPriceLine(pl);
    }
    if (macd) {
      const auto f = ema(cl, 12), s = ema(cl, 26);
      QVector<double> m(cl.size()), h(cl.size());
      for (int i = 0; i < cl.size(); ++i) m[i] = f[i] - s[i];
      QVector<double> mm; int first = 0; while (first < m.size() && !std::isfinite(m[first])) ++first;
      QVector<double> tail = m.mid(first);
      const auto sigT = ema(tail, 9);
      QVector<double> sig(cl.size(), qQNaN());
      for (int i = 0; i < sigT.size(); ++i) sig[first + i] = sigT[i];
      QVector<DataPoint> hist;
      for (int i = 0; i < cl.size(); ++i) {
        h[i] = m[i] - sig[i];
        if (!std::isfinite(h[i])) { hist.append(DataPoint::whitespace(b.times[i])); continue; }
        const bool rising = i > 0 && std::isfinite(h[i - 1]) && h[i] > h[i - 1];
        hist.append(DataPoint::val(b.times[i], h[i], h[i] >= 0 ? (rising ? QColor("#26a69a") : QColor("#b2dfdb")) : (rising ? QColor("#ffcdd2") : QColor("#ff5252"))));
      }
      auto* hs = c->addSeries(HistogramSeries, [&](SeriesOptions& o) { o.priceLineVisible = false; o.lastValueVisible = true; o.title = ""; }, 2);
      hs->setData(hist);
      auto* ml = c->addSeries(LineSeries, [&](SeriesOptions& o) { o.color = QColor("#2962ff"); o.lineWidth = 1; o.priceLineVisible = false; o.crosshairMarkerVisible = false; }, 2);
      ml->setData(lineData(m, b.times));
      auto* sl = c->addSeries(LineSeries, [&](SeriesOptions& o) { o.color = QColor("#ff6d00"); o.lineWidth = 1; o.priceLineVisible = false; o.crosshairMarkerVisible = false; }, 2);
      sl->setData(lineData(sig, b.times));
    }
  }
  const auto panes = c->panes();
  for (int i = 0; i < panes.size(); ++i) panes[i]->setStretchFactor(i == 0 ? 1 : 0.28);
  const double n = bars.size();
  c->timeScale()->setVisibleLogicalRange({qMax(0.0, n - 160), n + 8});
  return b;
}

QImage shot(QWidget* w) {
  w->show();
  QApplication::processEvents();
  return w->grab().toImage();
}

void save(const QImage& img, const QString& dir, const QString& name, const QString& suffix) {
  const QString path = dir + "/" + name + suffix + ".png";
  if (!img.save(path)) std::fprintf(stderr, "could not write %s\n", qPrintable(path));
  else std::printf("wrote %s (%dx%d)\n", qPrintable(path), img.width(), img.height());
}

void moveMouse(Chart* c, QPointF pos) {
  QMouseEvent ev(QEvent::MouseMove, pos, c->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(c, &ev);
}

// drawings.js subset: trend line, ray, horizontal line, fib retracement. Points are {time, price}.
class Drawings : public Primitive {
public:
  struct Item { QString type; QVector<QPair<qint64, double>> points; };
  QVector<Item> items;
  Series* series = nullptr;
  Chart* chart = nullptr;
  std::function<void()> requestUpdate;
  void attached(const AttachedParams& p) override { series = p.series; chart = p.chart; requestUpdate = p.requestUpdate; }
  void detached() override { series = nullptr; }
  PrimitiveZOrder zOrder() const override { return PrimitiveZOrder::Top; }
  void draw(QPainter& ctx, const QSize& size, const PrimitiveContext&) override {
    for (const Item& d : items) {
      QVector<QPointF> pts;
      for (const auto& [t, pr] : d.points) {
        const auto x = chart->timeScale()->timeToCoordinate(t);
        const auto y = series->priceToCoordinate(pr);
        if (!x || !y) return;
        pts.append({*x, *y});
      }
      const QColor color("#2962ff");
      ctx.setPen(QPen(color, 1.5));
      if (d.type == "trend") ctx.drawLine(pts[0], pts[1]);
      if (d.type == "ray") {
        const QPointF dlt = pts[1] - pts[0];
        const double len = std::hypot(dlt.x(), dlt.y());
        ctx.drawLine(pts[0], pts[0] + dlt / len * 1e5);
      }
      if (d.type == "hline") {
        ctx.drawLine(QPointF(0, pts[0].y()), QPointF(size.width(), pts[0].y()));
        ctx.setPen(color);
        const QString s = fmtPrice(d.points[0].second);
        ctx.drawText(QPointF(size.width() - 4 - ctx.fontMetrics().horizontalAdvance(s), pts[0].y() - 10 + 4), s);
      }
      if (d.type == "fib") {
        const double x1 = qMin(pts[0].x(), pts[1].x()), x2 = qMax(pts[0].x(), pts[1].x());
        for (double lv : {0.0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0, 1.618}) {
          const double p = d.points[1].second + (d.points[0].second - d.points[1].second) * lv;
          const auto y = series->priceToCoordinate(p);
          if (!y) continue;
          QColor c("#787b86");
          c.setAlphaF(lv == 0.5 || lv == 0.618 ? 1 : 0.7);
          ctx.setPen(QPen(c, 1.5));
          ctx.drawLine(QPointF(x1, *y), QPointF(x2, *y));
          ctx.setPen(QColor("#787b86"));
          ctx.drawText(QPointF(x1 + 2, *y - 8 + 4), QString("%1 (%2)").arg(lv).arg(fmtPrice(p)));
        }
        QPen dash(QColor("#787b86"), 1.5);
        dash.setDashPattern({2, 2});
        ctx.setPen(dash);
        ctx.drawLine(pts[0], pts[1]);
      }
    }
  }
};

double cpuSeconds() {
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
  return u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1e6 + u.ru_stime.tv_sec + u.ru_stime.tv_usec / 1e6;
}

void perf(const QVector<Bar>& fixture) {
  const QVector<Bar> bars = tiled(fixture, 5);
  QWidget host;
  host.resize(1600, 1000);
  auto* grid = new QGridLayout(&host);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(1);
  QVector<Chart*> charts;
  for (int i = 0; i < 16; ++i) {
    Built b = buildChart(&host, bars, deepwater());
    grid->addWidget(b.chart, i / 4, i % 4);
    charts.append(b.chart);
  }
  host.show();
  QApplication::processEvents();
  host.repaint();
  auto run = [&](const char* label, int frames, const std::function<void(int)>& step) {
    QElapsedTimer t;
    const double c0 = cpuSeconds();
    t.start();
    for (int f = 0; f < frames; ++f) {
      step(f);
      host.repaint();   // synchronous paint of the 16 charts (raster backing store)
    }
    const double ms = t.nsecsElapsed() / 1e6, cpu = (cpuSeconds() - c0) * 1000;
    qint64 paints = 0;
    for (Chart* c : charts) paints += c->paintCount();
    std::printf("  (paints so far: %lld)\n", paints);
    std::printf("%-46s %4d frames  %7.2f ms/frame  %6.1f fps  CPU %6.2f ms/frame\n", label, frames, ms / frames,
                1000.0 * frames / ms, cpu / frames);
  };
  run("16 charts x 5000 bars, scroll (168 visible)", 200, [&](int f) {
    for (Chart* c : charts) c->timeScale()->scrollChart(f % 100 < 50 ? 6 : -6);
  });
  for (Chart* c : charts) c->timeScale()->setVisibleLogicalRange({0, 5007});
  run("16 charts x 5000 bars, scroll (all visible)", 100, [&](int f) {
    for (Chart* c : charts) c->timeScale()->scrollChart(f % 50 < 25 ? 2 : -2);
  });
  for (Chart* c : charts) c->timeScale()->setVisibleLogicalRange({4840, 5008});
  run("16 charts x 5000 bars, wheel zoom", 100, [&](int f) {
    for (Chart* c : charts) c->timeScale()->zoom(c->timeScale()->width() / 2.0, f % 50 < 25 ? 0.5 : -0.5);
  });
  Chart* one = charts[5];
  run("crosshair move on 1 chart (overlay only)", 400, [&](int f) {
    moveMouse(one, QPointF(20 + (f * 3) % 300, 60 + (f * 7) % 150));
  });
}

int failures = 0;
void check(bool ok, const QString& what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
  if (!ok) ++failures;
}

void sendMouse(QWidget* w, QEvent::Type t, QPointF pos, Qt::MouseButton b, Qt::MouseButtons bs) {
  QMouseEvent ev(t, pos, w->mapToGlobal(pos), b, bs, Qt::NoModifier);
  QApplication::sendEvent(w, &ev);
}
void drag(QWidget* w, QPointF a, QPointF b) {
  sendMouse(w, QEvent::MouseButtonPress, a, Qt::LeftButton, Qt::LeftButton);
  for (int i = 1; i <= 10; ++i) sendMouse(w, QEvent::MouseMove, a + (b - a) * i / 10.0, Qt::NoButton, Qt::LeftButton);
  sendMouse(w, QEvent::MouseButtonRelease, b, Qt::LeftButton, Qt::NoButton);
}

// Interaction checks (events sent to the widget, results read back through the public API).
int interact(const QVector<Bar>& bars) {
  QWidget host;
  host.resize(1000, 700);
  Built b = buildChart(&host, bars, deepwater());
  Chart* c = b.chart;
  c->resize(host.size());
  host.show();
  QApplication::processEvents();
  TimeScale* ts = c->timeScale();
  const int n = bars.size();
  auto r0 = *ts->getVisibleLogicalRange();
  check(std::fabs(r0.from - (n - 160)) < 1e-6 && std::fabs(r0.to - (n + 8)) < 1e-6, QString("initial range %1..%2").arg(r0.from).arg(r0.to));
  // crosshair + callbacks
  MouseEventParams last;
  int moves = 0, clicks = 0;
  c->subscribeCrosshairMove([&](const MouseEventParams& p) { last = p; ++moves; });
  c->subscribeClick([&](const MouseEventParams& p) { last = p; ++clicks; });
  const double x = *ts->logicalToCoordinate(n - 10);
  moveMouse(c, QPointF(x, 100));
  check(moves == 1 && last.logical && int(*last.logical) == n - 10 && last.time && *last.time == bars[n - 10].time,
        QString("crosshair move: logical %1 time ok").arg(last.logical.value_or(-1)));
  check(last.seriesData.contains(b.main) && last.seriesData[b.main].close == bars[n - 10].close, "crosshair seriesData has the bar");
  check(last.point && std::fabs(last.point->y() - 100) < 1e-9 && last.paneIndex == 0, "crosshair point / pane");
  sendMouse(c, QEvent::MouseButtonPress, QPointF(x, 100), Qt::LeftButton, Qt::LeftButton);
  sendMouse(c, QEvent::MouseButtonRelease, QPointF(x, 100), Qt::LeftButton, Qt::NoButton);
  check(clicks == 1 && last.logical && int(*last.logical) == n - 10, "click callback");
  const int paneY = c->panes()[1]->rect().top() + 20;
  moveMouse(c, QPointF(x, paneY));
  check(last.paneIndex == 1, "crosshair in pane 1");
  // wheel zoom around the cursor: the bar under the cursor stays put, spacing +10 %
  const double sp0 = ts->barSpacing();
  const double xc = 500;
  const double lBefore = ts->coordinateToLogicalExact(xc);
  QWheelEvent we(QPointF(xc, 200), c->mapToGlobal(QPointF(xc, 200)), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(c, &we);
  check(std::fabs(ts->barSpacing() / sp0 - 1.1) < 1e-6, QString("wheel up zooms in 10%: %1 -> %2").arg(sp0).arg(ts->barSpacing()));
  check(std::fabs(ts->coordinateToLogicalExact(xc) - lBefore) < 1e-6, "wheel zoom keeps the bar under the cursor");
  // drag right in the pane: older bars come in (right offset decreases by dx / spacing)
  const double ro0 = ts->scrollPosition(), sp1 = ts->barSpacing();
  drag(c, QPointF(400, 200), QPointF(500, 200));
  check(std::fabs(ts->scrollPosition() - (ro0 - 100 / sp1)) < 1e-6, QString("pan: right offset %1 -> %2").arg(ro0).arg(ts->scrollPosition()));
  check(clicks == 1, "a drag is not a click");
  // horizontal wheel scrolls
  const double ro1 = ts->scrollPosition();
  QWheelEvent hw(QPointF(xc, 200), c->mapToGlobal(QPointF(xc, 200)), QPoint(50, 0), QPoint(), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(c, &hw);
  check(ts->scrollPosition() < ro1, QString("trackpad horizontal scroll moves the view: %1 -> %2").arg(ro1).arg(ts->scrollPosition()));
  // price axis drag scales and turns autoscale off; double-click turns it back on
  PriceScale* right = c->priceScale("right");
  const auto pr0 = *right->getVisibleRange();
  const double ax = c->width() - c->priceAxisWidth() / 2.0;
  drag(c, QPointF(ax, 300), QPointF(ax, 250));
  const auto pr1 = *right->getVisibleRange();
  check(!right->options().autoScale && (pr1.maxValue - pr1.minValue) < (pr0.maxValue - pr0.minValue),
        QString("price axis drag up zooms in: span %1 -> %2").arg(pr0.maxValue - pr0.minValue).arg(pr1.maxValue - pr1.minValue));
  check(c->priceScale("right", 1)->options().autoScale, "pane 1 keeps autoscale");
  // vertical pan works now that autoscale is off
  drag(c, QPointF(400, 200), QPointF(400, 260));
  const auto pr2 = *right->getVisibleRange();
  check(pr2.maxValue > pr1.maxValue, "vertical pan moves the price range when not autoscaled");
  sendMouse(c, QEvent::MouseButtonDblClick, QPointF(ax, 300), Qt::LeftButton, Qt::LeftButton);
  sendMouse(c, QEvent::MouseButtonRelease, QPointF(ax, 300), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  const auto pr3 = *right->getVisibleRange();
  check(right->options().autoScale && std::fabs(pr3.maxValue - pr0.maxValue) / pr0.maxValue < 0.2, "double-click on the price axis restores autoscale");
  // time axis drag (anchored at the right edge, as the JS library): left = wider bars, right = narrower
  const double sp2 = ts->barSpacing();
  const int ty = c->height() - 10;
  drag(c, QPointF(500, ty), QPointF(400, ty));
  check(ts->barSpacing() > sp2, QString("time axis drag left zooms in: %1 -> %2").arg(sp2).arg(ts->barSpacing()));
  const double sp3 = ts->barSpacing();
  drag(c, QPointF(400, ty), QPointF(600, ty));
  check(ts->barSpacing() < sp3, QString("time axis drag right zooms out: %1 -> %2").arg(sp3).arg(ts->barSpacing()));
  sendMouse(c, QEvent::MouseButtonDblClick, QPointF(500, ty), Qt::LeftButton, Qt::LeftButton);
  sendMouse(c, QEvent::MouseButtonRelease, QPointF(500, ty), Qt::LeftButton, Qt::NoButton);
  check(std::fabs(ts->barSpacing() - 6) < 1e-9 && std::fabs(ts->scrollPosition() - 8) < 1e-9, "double-click on the time axis resets spacing 6 and right offset 8");
  // separator drag resizes panes
  Pane* p0 = c->panes()[0];
  Pane* p1 = c->panes()[1];
  const int h0 = p0->getHeight(), h1 = p1->getHeight();
  const int sy = p0->rect().bottom() + 1;
  drag(c, QPointF(300, sy), QPointF(300, sy - 60));
  check(std::abs(p0->getHeight() - (h0 - 60)) <= 1 && std::abs(p1->getHeight() - (h1 + 60)) <= 1,
        QString("separator drag: %1/%2 -> %3/%4").arg(h0).arg(h1).arg(p0->getHeight()).arg(p1->getHeight()));
  // live update: a new bar at the right edge keeps the view glued to it
  ts->setVisibleLogicalRange({n - 100.0, n + 8.0});
  const double roLive = ts->scrollPosition();
  b.main->update(DataPoint::ohlc(bars.last().time + 86400, 1, 2, 0.5, 1.5));
  check(ts->getVisibleLogicalRange()->to == n + 9 && std::fabs(ts->scrollPosition() - roLive) < 1e-9, "new bar: view follows (shiftVisibleRangeOnNewBar)");
  b.main->update(DataPoint::ohlc(bars.last().time + 86400, 1, 3, 0.5, 2.5));
  check(b.main->dataSize() == n + 1 && b.main->data().last().close == 2.5, "same-time update replaces the last bar");
  // scrolled into history: a new bar does not move the view
  ts->setVisibleLogicalRange({100, 300});
  b.main->update(DataPoint::ohlc(bars.last().time + 2 * 86400, 1, 2, 0.5, 1.5));
  const auto rr = *ts->getVisibleLogicalRange();
  check(std::fabs(rr.from - 100) < 1e-6 && std::fabs(rr.to - 300) < 1e-6, "new bar while in history: view stays");
  // coordinates round trip
  const auto y = b.main->priceToCoordinate(70000);
  check(y && std::fabs(*b.main->coordinateToPrice(*y) - 70000) < 1e-6, "priceToCoordinate / coordinateToPrice round trip");
  check(ts->coordinateToLogical(*ts->logicalToCoordinate(150)) == 150.0, "logicalToCoordinate / coordinateToLogical round trip");
  // removing the indicator series removes their pane
  for (Series* s : c->panes()[1]->getSeries()) c->removeSeries(s);
  check(c->panes().size() == 1, "empty pane removed");
  // robustness: empty chart, empty series, whitespace only, one point, tiny widget
  {
    Chart e;
    e.resize(300, 200);
    e.show();
    e.grab();
    Series* s1 = e.addSeries(LineSeries);
    e.grab();
    s1->setData({DataPoint::whitespace(1000), DataPoint::whitespace(2000)});
    e.grab();
    s1->setData({DataPoint::val(1000, 5)});
    e.grab();
    e.resize(3, 3);
    e.grab();
    e.timeScale()->fitContent();
    e.priceScale("right")->applyOptions([](PriceScaleOptions& o) { o.mode = PriceScaleMode::Logarithmic; });
    s1->setData({DataPoint::val(1000, -5), DataPoint::val(2000, 0)});
    e.resize(300, 200);
    e.grab();
    e.removeSeries(s1);
    e.grab();
    check(true, "empty / degenerate charts paint without crashing");
  }
  std::printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
  const QString profile = QDir::tempPath() + "/dw-plot-demo-profile";
  qputenv("DW_PROFILE", profile.toUtf8());   // never touch the user's settings
  QApplication app(argc, argv);
  theme::install();
  const QStringList args = app.arguments();
  const QString out = args.size() > 1 ? args[1] : QString(".");
  const int si = args.indexOf("--suffix");
  const QString suffix = si > 0 && si + 1 < args.size() ? args[si + 1] : QString();
  QDir().mkpath(out);
  const QVector<Bar> bars = loadFixture();
  if (args.contains("--perf")) { perf(bars); return 0; }
  if (args.contains("--interact")) return interact(bars);

  // 1. quant-terminal's default layout (EMA 21 + RSI 14), original colours, same size as its chart area at 1600x1000
  {
    QWidget host;
    host.resize(1256, 868);
    Built b = buildChart(&host, bars, qtDark());
    b.chart->resize(host.size());
    save(shot(&host), out, "01-default-qtcolors", suffix);
    moveMouse(b.chart, QPointF(790 - 44, 500 - 92));
    save(shot(&host), out, "02-crosshair-qtcolors", suffix);
  }
  // 1b. the same with a log scale and fitContent, then percentage mode + markers (as the JS run on the original)
  {
    QWidget host;
    host.resize(1256, 868);
    Built b = buildChart(&host, bars, qtDark());
    b.chart->resize(host.size());
    b.chart->priceScale("right")->applyOptions([](PriceScaleOptions& o) { o.mode = PriceScaleMode::Logarithmic; });
    b.chart->timeScale()->fitContent();
    save(shot(&host), out, "10-log-qtcolors", suffix);
    b.chart->priceScale("right")->applyOptions([](PriceScaleOptions& o) { o.mode = PriceScaleMode::Percentage; });
    b.chart->timeScale()->setVisibleLogicalRange({840, 1008});
    createSeriesMarkers(b.main, {{bars[950].time, MarkerPosition::AboveBar, MarkerShape::ArrowDown, QColor("#ef5350"), "Short", 1, {}},
                                 {bars[950].time, MarkerPosition::AboveBar, MarkerShape::Circle, QColor("#ff9800"), "", 1, {}},
                                 {bars[960].time, MarkerPosition::BelowBar, MarkerShape::ArrowUp, QColor("#26a69a"), "Long", 1, {}},
                                 {bars[970].time, MarkerPosition::InBar, MarkerShape::Square, QColor("#e91e63"), "", 1, {}}});
    save(shot(&host), out, "11-percentage-markers-qtcolors", suffix);
  }
  // 2. Deepwater colours, three panes (EMA overlay, RSI with levels, MACD histogram with per-point colours)
  {
    QWidget host;
    host.resize(1256, 868);
    Built b = buildChart(&host, bars, deepwater(), "candles", true, true);
    b.chart->resize(host.size());
    save(shot(&host), out, "03-panes-deepwater", suffix);
    moveMouse(b.chart, QPointF(600, 300));
    save(shot(&host), out, "04-crosshair-deepwater", suffix);
  }
  // 3. every main series type
  {
    const QStringList types{"candles", "hollow", "bars", "line", "linemarkers", "step", "area", "baseline", "columns"};
    QImage sheet(QSize(3 * 520, 3 * 320) * qApp->devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
    sheet.setDevicePixelRatio(qApp->devicePixelRatio());
    sheet.fill(Qt::black);
    QPainter sp(&sheet);
    for (int i = 0; i < types.size(); ++i) {
      QWidget host;
      host.resize(518, 318);
      Built b = buildChart(&host, bars, deepwater(), types[i], false);
      b.chart->resize(host.size());
      b.chart->timeScale()->setVisibleLogicalRange({bars.size() - 60.0, bars.size() + 8.0});
      const QImage img = shot(&host);
      sp.drawImage(QPointF((i % 3) * 520, (i / 3) * 320), img);
      sp.setPen(Qt::white);
      sp.drawText(QPointF((i % 3) * 520 + 10, (i / 3) * 320 + 18), types[i]);
    }
    sp.end();
    save(sheet, out, "05-series-types", suffix);
  }
  // 4. log and percentage scales
  {
    QWidget host;
    host.resize(900, 500);
    Built b = buildChart(&host, bars, deepwater(), "candles", false);
    b.chart->resize(host.size());
    b.chart->priceScale("right")->applyOptions([](PriceScaleOptions& o) { o.mode = PriceScaleMode::Logarithmic; });
    b.chart->timeScale()->fitContent();
    save(shot(&host), out, "06-log", suffix);
    // compare-style percentage mode: main + a second line on the same scale (EMA 50 of the same closes)
    b.chart->priceScale("right")->applyOptions([](PriceScaleOptions& o) { o.mode = PriceScaleMode::Percentage; });
    auto* cmp = b.chart->addSeries(LineSeries, [](SeriesOptions& o) { o.color = QColor("#ff9800"); o.lineWidth = 2; o.priceLineVisible = false; o.title = "EMA50"; });
    cmp->setData(lineData(ema(closes(bars), 50), b.times));
    b.chart->timeScale()->setVisibleLogicalRange({bars.size() - 200.0, bars.size() + 8.0});
    save(shot(&host), out, "07-percentage", suffix);
  }
  // 5. markers (EMA 9/21 crosses, as indicator markers and strategy-tester trades look)
  {
    QWidget host;
    host.resize(900, 500);
    Built b = buildChart(&host, bars, deepwater(), "candles", false);
    b.chart->resize(host.size());
    const auto cl = closes(bars);
    const auto f = ema(cl, 9), s = ema(cl, 21);
    QVector<SeriesMarker> ms;
    for (int i = 22; i < bars.size(); ++i) {
      const bool upX = f[i - 1] <= s[i - 1] && f[i] > s[i], dnX = f[i - 1] >= s[i - 1] && f[i] < s[i];
      if (upX) ms.append({bars[i].time, MarkerPosition::BelowBar, MarkerShape::ArrowUp, QColor("#26a69a"), "Long", 1, {}});
      if (dnX) {
        ms.append({bars[i].time, MarkerPosition::AboveBar, MarkerShape::ArrowDown, QColor("#ef5350"), "Short", 1, {}});
        ms.append({bars[i].time, MarkerPosition::AboveBar, MarkerShape::Circle, QColor("#ff9800"), "", 1, {}});
      }
      if (i % 37 == 0) ms.append({bars[i].time, MarkerPosition::InBar, MarkerShape::Square, QColor("#e91e63"), "", 1, {}});
    }
    createSeriesMarkers(b.main, ms);
    b.chart->timeScale()->setVisibleLogicalRange({bars.size() - 120.0, bars.size() + 8.0});
    save(shot(&host), out, "08-markers", suffix);
  }
  // 6. a primitive (drawings.js): trend line, ray, horizontal line, fib retracement
  {
    QWidget host;
    host.resize(900, 500);
    Built b = buildChart(&host, bars, deepwater(), "candles", false);
    b.chart->resize(host.size());
    Drawings d;
    const int n = bars.size();
    d.items = {{"trend", {{bars[n - 140].time, bars[n - 140].low}, {bars[n - 60].time, bars[n - 60].low}}},
               {"ray", {{bars[n - 100].time, bars[n - 100].high}, {bars[n - 70].time, bars[n - 70].high}}},
               {"hline", {{bars[n - 30].time, bars[n - 30].close}}},
               {"fib", {{bars[n - 45].time, 87500.0}, {bars[n - 20].time, 75500.0}}}};
    b.main->attachPrimitive(&d);
    save(shot(&host), out, "09-primitive", suffix);
    b.main->detachPrimitive(&d);
  }
  return 0;
}
