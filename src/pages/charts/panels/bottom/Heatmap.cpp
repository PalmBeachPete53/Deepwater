// Charts tab — Heatmap. Port of quant-terminal public/js/panels.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Heatmap.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/bottom/Common.h"
#include "pages/charts/panels/bottom/Screener.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace charts::ui::bottom {

static double jn(const QJsonObject& r, const char* k) {
  const QJsonValue v = r.value(k);
  return v.isDouble() ? v.toDouble() : NaN;
}

QList<Tile> squarify(const QList<Tile>& items, double x, double y, double w, double h) {
  QList<Tile> out;
  QList<Tile> rest;
  for (const Tile& i : items)
    if (i.value > 0) rest << i;
  double total = 0;
  for (const Tile& i : rest) total += i.value;
  if (!total) return out;
  const double scale = (w * h) / total;
  double rx = x, ry = y, rw = w, rh = h;
  while (!rest.isEmpty()) {
    const double shortSide = std::min(rw, rh);
    QList<Tile> row;
    double best = INFINITY;
    for (const Tile& it : rest) {
      QList<Tile> test = row;
      test << it;
      double s = 0, mx = -INFINITY, mn = INFINITY;
      for (const Tile& t : test) {
        s += t.value * scale;
        mx = std::max(mx, t.value * scale);
        mn = std::min(mn, t.value * scale);
      }
      const double worst = std::max((shortSide * shortSide * mx) / (s * s), (s * s) / (shortSide * shortSide * mn));
      if (worst > best) break;
      best = worst;
      row << it;
    }
    double area = 0;
    for (const Tile& t : row) area += t.value * scale;
    const double thick = area / shortSide;
    double off = 0;
    for (Tile it : row) {
      const double len = (it.value * scale) / thick;
      if (rw >= rh) { it.x = rx; it.y = ry + off; it.w = thick; it.h = len; }
      else { it.x = rx + off; it.y = ry; it.w = len; it.h = thick; }
      out << it;
      off += len;
    }
    if (rw >= rh) { rx += thick; rw -= thick; }
    else { ry += thick; rh -= thick; }
    rest = rest.mid(row.size());
  }
  return out;
}

namespace {
// <canvas id="hmC"> + the floating .tip
class HeatCanvas : public QWidget {
public:
  explicit HeatCanvas(HeatmapTab* t) : tab(t) { setMouseTracking(true); }
  HeatmapTab* tab;
  QList<Tile> tiles;
  QPointer<QLabel> tip;

protected:
  QColor color(double ch) const {
    const double cap = tab->universe == "us" ? 3 : 8;
    const double t = std::max(-1.0, std::min(1.0, (std::isnan(ch) ? 0 : ch) / cap));
    // JS: base rgb(66,69,80) toward up rgb(8,153,129) / down rgb(242,54,69); here the theme's neutral, up and down
    const auto& V = style::v();
    const QColor base = theme_base(), tgt = t >= 0 ? V.up : V.down;
    const double k = std::fabs(t);
    return QColor(int(std::round(base.red() + (tgt.red() - base.red()) * k)),
                  int(std::round(base.green() + (tgt.green() - base.green()) * k)),
                  int(std::round(base.blue() + (tgt.blue() - base.blue()) * k)));
  }
  static QColor theme_base() {
    const auto& V = style::v();   // #424550 on #131722: a grey between the ground and the muted text
    const auto mix = [](const QColor& a, const QColor& b, double t) {
      return QColor(int(a.red() + (b.red() - a.red()) * t), int(a.green() + (b.green() - a.green()) * t),
                    int(a.blue() + (b.blue() - a.blue()) * t));
    };
    return mix(V.bg, V.muted, 0.42);
  }
  // JS Math.max(1, …): a row without a usable value is NaN, which squarify drops (value > 0 fails), so it has no tile.
  double val(const QJsonObject& r) const {
    const auto num = [&](const char* k) {   // r[k] in arithmetic: null is 0, missing / not a number is NaN
      const QJsonValue x = r.value(k);
      return x.isNull() ? 0.0 : jn(r, k);
    };
    double v;
    if (tab->size == "volume") v = num("volume") * (tab->universe == "us" ? num("price") : 1);
    else {
      const double cap = num("marketCap");
      v = std::isnan(cap) || cap == 0 ? num("volume") : cap;   // r.marketCap || r.volume
    }
    return std::isnan(v) ? v : std::max(1.0, v);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const auto& V = style::v();
    tiles.clear();
    if (!tab->rows) return;
    const double W = width(), H = height();
    if (tab->universe == "us") {
      QList<QString> order;
      QHash<QString, QList<QJsonObject>> bySector;
      for (const auto& r : *tab->rows) {
        QString s = r.value("sector").toString();
        if (s.isEmpty()) s = "Other";
        if (!bySector.contains(s)) order << s;
        bySector[s] << r;
      }
      QList<Tile> groups;
      for (const QString& n : order) {
        Tile g;
        g.name = n;
        g.rs = bySector[n];
        // rows without a value add nothing (in the JS one NaN row made the sum NaN and hid its whole sector)
        for (const auto& r : g.rs)
          if (const double x = val(r); !std::isnan(x)) g.value += x;
        groups << g;
      }
      std::stable_sort(groups.begin(), groups.end(), [](const Tile& a, const Tile& b) { return b.value < a.value; });
      for (const Tile& g : squarify(groups, 0, 0, W, H)) {
        QList<Tile> items;
        for (const auto& r : g.rs) {
          Tile t;
          t.row = r;
          t.value = val(r);
          items << t;
        }
        std::stable_sort(items.begin(), items.end(), [](const Tile& a, const Tile& b) { return b.value < a.value; });
        // a sector thinner than its title strip has no room for tiles (negative sizes divided by zero in squarify)
        if (g.w > 2 && g.h > 15) tiles << squarify(items, g.x + 1, g.y + 14, g.w - 2, g.h - 15);
        p.fillRect(QRectF(g.x, g.y, g.w, g.h), V.panel);
        p.setPen(V.muted);
        p.setFont(style::font(10, QFont::DemiBold));
        if (g.w > 50)
          p.drawText(QPointF(g.x + 4, g.y + 2 + QFontMetricsF(p.font()).ascent()),
                     (g.name == "Other" ? QCoreApplication::translate("charts", "Other") : g.name)
                         .toUpper()
                         .left(int(std::floor(g.w / 7))));
      }
    } else {
      QList<Tile> items;
      for (const auto& r : *tab->rows) {
        Tile t;
        t.row = r;
        t.value = val(r);
        items << t;
      }
      std::stable_sort(items.begin(), items.end(), [](const Tile& a, const Tile& b) { return b.value < a.value; });
      tiles = squarify(items, 0, 0, W, H);
    }
    static const QRegularExpression usdt("USDT$");
    for (const Tile& t : tiles) {
      p.fillRect(QRectF(t.x + 0.5, t.y + 0.5, t.w - 1, t.h - 1), color(jn(t.row, "changePct")));
      if (t.w > 34 && t.h > 22) {
        const double fs = std::max(10.0, std::min(22.0, std::sqrt(t.w * t.h) / 5));
        p.setPen(Qt::white);   // #fff on the tiles, as the JS
        QFont f = style::font(13, QFont::DemiBold);
        f.setPixelSize(int(std::round(fs)));
        p.setFont(f);
        const double cx = t.x + t.w / 2, cy = t.y + t.h / 2 - (t.h > 40 ? fs * 0.45 : 0);
        auto centered = [&](const QString& s, double x, double y) {
          const QFontMetricsF fm(p.font());
          p.drawText(QPointF(x - fm.horizontalAdvance(s) / 2, y + (fm.ascent() - fm.descent()) / 2), s);
        };
        centered(t.row.value("ticker").toString().remove(usdt), cx, cy);
        if (t.h > 40) {
          QFont f2 = style::font(13);
          f2.setPixelSize(std::max(1, int(std::round(fs * 0.75))));
          p.setFont(f2);
          centered(fmtPct(jn(t.row, "changePct")), cx, t.y + t.h / 2 + fs * 0.6);
        }
      }
    }
  }
  const Tile* at(const QPointF& pt) const {
    for (const Tile& q : tiles)
      if (pt.x() >= q.x && pt.x() <= q.x + q.w && pt.y() >= q.y && pt.y() <= q.y + q.h) return &q;
    return nullptr;
  }
  void hideTip() {
    if (tip) tip->deleteLater();
    tip = nullptr;
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    const Tile* t = at(e->position());
    setCursor(t ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (!t) return hideTip();
    QWidget* host = window();
    if (!tip) {
      tip = new QLabel(host);
      tip->setAttribute(Qt::WA_TransparentForMouseEvents);
      tip->setTextFormat(Qt::RichText);
      tip->setFont(style::font(12));
      const auto& V = style::v();
      tip->setStyleSheet(QString("QLabel{background:%1;border:1px solid %2;border-radius:6px;padding:6px 9px;color:%3;}")
                             .arg(V.panel.name(QColor::HexArgb), V.border.name(QColor::HexArgb),
                                  V.text.name(QColor::HexArgb)));
    }
    const QJsonObject& r = t->row;
    const double ch = jn(r, "changePct"), cap = jn(r, "marketCap");
    tip->setText(QString("<b>%1</b> %2<br>%3 <span style=\"color:%4\">%5</span>%6")
                     .arg(fmt::esc(r.value("ticker").toString()), fmt::esc(r.value("name").toString()),
                          fmtPrice(jn(r, "price")), style::colorOf(ch >= 0 ? "up" : "down").name(), fmtPct(ch),
                          !std::isnan(cap) && cap ? "<br>" + QCoreApplication::translate("charts", "Market cap") + " " + fmtNum(cap) : QString()));
    tip->adjustSize();
    tip->move(mapTo(host, e->position().toPoint()) + QPoint(14, 14));
    tip->raise();
    tip->show();
  }
  void leaveEvent(QEvent*) override { hideTip(); }
  void hideEvent(QHideEvent*) override { hideTip(); }
  void mousePressEvent(QMouseEvent* e) override {
    if (e->button() != Qt::LeftButton) return;
    if (const Tile* t = at(e->position())) {
      const QString sym = t->row.value("symbol").toString();
      hideTip();
      tab->P->app->setSymbol(sym);
    }
  }
};
}  // namespace

HeatmapTab::HeatmapTab(BottomPanel* panel) : QObject(panel), P(panel) {}

void HeatmapTab::setUniverse(const QString& u) {
  // JS renderHeatmap puts a blank canvas until the new market's rows arrive (and keeps it blank if the load fails):
  // never paint the old market's tiles meanwhile.
  if (u != universe) rows.reset();
  universe = u;
  render();
}

void HeatmapTab::render() {
  QWidget* body = P->newBody();
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  bl->setSpacing(0);
  auto* pad = new QWidget;
  auto* pl = new QVBoxLayout(pad);
  pl->setContentsMargins(12, 10, 12, 10);
  QComboBox* u = optSelect({{QCoreApplication::translate("charts", "US stocks (200 largest)"), "us"}, {QCoreApplication::translate("charts", "Crypto (100 most traded)"), "crypto"}}, universe,
                           false);
  u->setProperty("qtc", "in");
  u->setFixedHeight(28);
  QComboBox* s = optSelect({{QCoreApplication::translate("charts", "Market cap"), "marketCap"}, {QCoreApplication::translate("charts", "Volume"), "volume"}}, size, false);
  s->setProperty("qtc", "in");
  s->setFixedHeight(28);
  auto* h = new QLabel(QCoreApplication::translate("charts", "Colour = today's change. Click a tile to open its chart."));
  h->setProperty("qtc", "hint");
  alignEnd(h);
  pl->addWidget(rowFlex({style::field(QCoreApplication::translate("charts", "Market"), u), style::field(QCoreApplication::translate("charts", "Size by"), s), h}));
  bl->addWidget(pad);
  auto* c = new HeatCanvas(this);
  canvas_ = c;
  bl->addWidget(c, 1);   // height: calc(100% - 64px)
  connect(u, &QComboBox::activated, this, [this, u](int) { setUniverse(optValue(u)); });
  connect(s, &QComboBox::activated, this, [this, s](int) {
    size = optValue(s);
    draw();
  });
  const int seq = ++seq_;
  const QString uni = universe;
  loadScreenerRows(uni, this, [this, seq, uni](const QList<QJsonObject>& all, const QString& err) {
    if (seq != seq_) return;
    if (!err.isEmpty()) return P->app->toast(fmt::esc(fmt::tx(err)), "err");
    QList<QJsonObject> rs;
    if (uni == "us") {
      for (const auto& r : all)
        if (jn(r, "marketCap") > 0) rs << r;
      std::stable_sort(rs.begin(), rs.end(),
                       [](const QJsonObject& a, const QJsonObject& b) { return jn(b, "marketCap") < jn(a, "marketCap"); });
    } else {
      rs = all;
      std::stable_sort(rs.begin(), rs.end(),
                       [](const QJsonObject& a, const QJsonObject& b) { return jn(b, "volume") < jn(a, "volume"); });
    }
    rows = rs.mid(0, uni == "us" ? 200 : 100);
    draw();
  });
}

void HeatmapTab::draw() {
  if (canvas_) canvas_->update();
}

}  // namespace charts::ui::bottom
