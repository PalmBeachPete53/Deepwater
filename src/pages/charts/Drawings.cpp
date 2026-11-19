// Port of quant-terminal public/js/drawings.js (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include "pages/charts/Drawings.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/Style.h"
#include "pages/charts/pane/Util.h"
#include "pages/charts/plot/Chart.h"
#include "charts/core/Series.h"
#include <QJsonValue>
#include <QPainterPath>
#include <QCoreApplication>
#include <QHash>
#include <QSvgRenderer>
#include <cmath>

namespace charts::ui {

const QList<Tool>& tools() {
  static const QList<Tool> t = {
      {"cursor", QT_TRANSLATE_NOOP("charts", "Cursor"), 0, R"(<path d="M5 3l14 7-6 2-2 6z"/>)"},
      {"trend", QT_TRANSLATE_NOOP("charts", "Trend line"), 2, R"(<path d="M4 19L20 5"/><circle cx="4" cy="19" r="1.6"/><circle cx="20" cy="5" r="1.6"/>)"},
      {"ray", QT_TRANSLATE_NOOP("charts", "Ray"), 2, R"(<path d="M4 19L21 4"/><circle cx="4" cy="19" r="1.6"/><circle cx="11" cy="13" r="1.6"/>)"},
      {"extended", QT_TRANSLATE_NOOP("charts", "Extended line"), 2, R"(<path d="M2 21L22 3"/><circle cx="8" cy="16" r="1.6"/><circle cx="16" cy="9" r="1.6"/>)"},
      {"hline", QT_TRANSLATE_NOOP("charts", "Horizontal line"), 1, R"(<path d="M3 12h18"/><circle cx="12" cy="12" r="1.6"/>)"},
      {"hray", QT_TRANSLATE_NOOP("charts", "Horizontal ray"), 1, R"(<path d="M8 12h13"/><circle cx="8" cy="12" r="1.6"/>)"},
      {"vline", QT_TRANSLATE_NOOP("charts", "Vertical line"), 1, R"(<path d="M12 3v18"/><circle cx="12" cy="12" r="1.6"/>)"},
      {"channel", QT_TRANSLATE_NOOP("charts", "Parallel channel"), 3, R"(<path d="M3 15L15 5M9 21l12-10"/>)"},
      {"rect", QT_TRANSLATE_NOOP("charts", "Rectangle"), 2, R"(<rect x="4" y="6" width="16" height="12" rx="1"/>)"},
      {"fib", QT_TRANSLATE_NOOP("charts", "Fib retracement"), 2, R"(<path d="M3 5h18M3 10h18M3 14h18M3 19h18" stroke-dasharray="2 2"/>)"},
      {"measure", QT_TRANSLATE_NOOP("charts", "Price range / measure"), 2, R"(<path d="M12 3v18M8 7l4-4 4 4M8 17l4 4 4-4"/>)"},
      {"long", QT_TRANSLATE_NOOP("charts", "Long position"), 2,
       R"(<rect x="4" y="5" width="16" height="7" fill="currentColor" fill-opacity=".25"/><rect x="4" y="12" width="16" height="7"/>)"},
      {"short", QT_TRANSLATE_NOOP("charts", "Short position"), 2,
       R"(<rect x="4" y="5" width="16" height="7"/><rect x="4" y="12" width="16" height="7" fill="currentColor" fill-opacity=".25"/>)"},
      {"text", QT_TRANSLATE_NOOP("charts", "Text note"), 1, R"(<path d="M5 6h14M12 6v13"/>)"},
  };
  return t;
}

const Tool* toolById(const QString& id) {
  for (const Tool& t : tools())
    if (t.id == id) return &t;
  return nullptr;
}

const QVector<double>& fibLevels() {
  static const QVector<double> l = {0, 0.236, 0.382, 0.5, 0.618, 0.786, 1, 1.618};
  return l;
}

QByteArray svgIcon(const QString& inner, const QColor& color, double strokeWidth) {
  const QString c = color.name(QColor::HexRgb);
  QString body = inner;
  body.replace("currentColor", c);
  const QString op = color.alpha() < 255 ? QString(" opacity=\"%1\"").arg(color.alphaF()) : QString();
  return QString(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24"><g fill="none" stroke="%1" stroke-width="%2" stroke-linecap="round" stroke-linejoin="round"%3>%4</g></svg>)")
      .arg(c)
      .arg(strokeWidth)
      .arg(op, body)
      .toUtf8();
}

QPixmap renderIcon(const QString& inner, const QColor& color, int px, double dpr, double strokeWidth) {
  // the legend asks for its icons at every paint: parsing the SVG each time showed in the profile. A handful of icons
  // × colours × sizes exist, so a small cache (GUI thread only, like QPixmap) cleared when it fills is enough.
  static QHash<QString, QPixmap> cache;
  const QString key = QString::number(color.rgba(), 16) + '|' + QString::number(px) + '|' + QString::number(dpr) + '|' +
                      QString::number(strokeWidth) + '|' + inner;
  if (const auto it = cache.constFind(key); it != cache.constEnd()) return *it;
  if (cache.size() >= 256) cache.clear();
  QPixmap pm(int(std::ceil(px * dpr)), int(std::ceil(px * dpr)));
  pm.fill(Qt::transparent);
  QSvgRenderer r(svgIcon(inner, color, strokeWidth));
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  r.render(&p, QRectF(0, 0, pm.width(), pm.height()));
  p.end();
  pm.setDevicePixelRatio(dpr);
  cache.insert(key, pm);
  return pm;
}

QString legendIcon(const QString& name) {
  if (name == "gear")
    return R"(<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"/>)";
  if (name == "eye") return R"(<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>)";
  if (name == "x") return R"(<path d="M6 6l12 12M18 6L6 18"/>)";
  return {};
}

// ---------------- items ----------------

QJsonObject DrawingLayer::Item::toJson() const {
  QJsonArray pts;
  for (const Point& p : points) pts.append(QJsonObject{{"time", p.time}, {"price", p.price}});
  QJsonObject o{{"id", id}, {"type", type}, {"points", pts}};
  if (!text.isEmpty()) o["text"] = text;
  if (!color.isEmpty()) o["color"] = color;
  if (!std::isnan(rr)) o["rr"] = rr;
  return o;
}

DrawingLayer::Item DrawingLayer::Item::fromJson(const QJsonObject& o) {
  Item it;
  it.id = o.value("id").toString();
  it.type = o.value("type").toString();
  for (const QJsonValue& v : o.value("points").toArray()) {
    const QJsonObject p = v.toObject();
    it.points.append({p.value("time").toDouble(), p.value("price").toDouble()});
  }
  it.text = o.value("text").toString();
  it.color = o.value("color").toString();
  if (o.value("rr").isDouble()) it.rr = o.value("rr").toDouble();
  return it;
}

namespace {
const QString kLine = "#2962ff", kFib = "#787b86";
// COLORS.up / COLORS.down + "<alpha>)"
QColor up(double a) { return QColor(8, 153, 129, int(std::lround(a * 255))); }
QColor down(double a) { return QColor(242, 54, 69, int(std::lround(a * 255))); }
}  // namespace

DrawingLayer::DrawingLayer(ChartPane* p) : pane(p) {}

void DrawingLayer::attached(const plot::AttachedParams& p) {
  series = p.series;
  requestUpdate = p.requestUpdate;
}
void DrawingLayer::detached() { series = nullptr; }
void DrawingLayer::update() {
  if (requestUpdate) requestUpdate();
}

std::optional<double> DrawingLayer::x(double t) const { return pane->timeToXF(t); }
std::optional<double> DrawingLayer::y(double p) const {
  if (!series) return std::nullopt;
  return series->priceToCoordinate(p);
}

void DrawingLayer::setItems(const QJsonArray& arr) {
  items.clear();
  for (const QJsonValue& v : arr) items.append(Item::fromJson(v.toObject()));
  selected.clear();
  update();
}

QJsonArray DrawingLayer::itemsJson() const {
  QJsonArray a;
  for (const Item& it : items) a.append(it.toJson());
  return a;
}

const DrawingLayer::Item* DrawingLayer::selectedItem() const {
  if (selected.isEmpty()) return nullptr;
  for (const Item& it : items)
    if (it.id == selected) return &it;
  return nullptr;
}

void DrawingLayer::draw(QPainter& ctx, const QSize& size, const plot::PrimitiveContext&) {
  ctx.save();
  ctx.setFont(style::font(11));
  for (const Item& d : items) drawOne(ctx, size, d, !selected.isEmpty() && d.id == selected);
  if (temp) drawOne(ctx, size, *temp, true);
  ctx.restore();
}

void DrawingLayer::drawOne(QPainter& ctx, const QSize& size, const Item& d, bool active) {
  struct P { double x, y, time, price; };
  QVector<P> pts;
  for (const Point& p : d.points) {
    const auto xx = x(p.time);
    const auto yy = y(p.price);
    if (!xx || !yy) return;
    pts.append({*xx, *yy, p.time, p.price});
  }
  if (pts.isEmpty()) return;
  const QColor color = plot::cssColor(d.color.isEmpty() ? kLine : d.color);
  QColor stroke = color, fill = color;
  double lineWidth = active ? 2 : 1.5;
  QVector<qreal> dash;
  double alpha = 1;
  const P a = pts[0];
  const P b = pts.size() > 1 ? pts[1] : pts[0];
  const P* c = pts.size() > 2 ? &pts[2] : nullptr;

  auto line = [&](double x1, double y1, double x2, double y2) {
    QPen pen(stroke, lineWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);   // canvas defaults: butt, miter
    if (!dash.isEmpty()) {
      QVector<qreal> d2;
      for (qreal v : dash) d2.append(v / lineWidth);
      pen.setDashPattern(d2);
    }
    ctx.setOpacity(alpha);
    ctx.setPen(pen);
    ctx.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    ctx.setOpacity(1);
  };
  auto fillRect = [&](double x0, double y0, double w, double h, const QColor& col, double op = 1) {
    ctx.setOpacity(op);
    ctx.fillRect(QRectF(x0, y0, w, h).normalized(), col);
    ctx.setOpacity(1);
  };
  // textBaseline "middle": the middle of the em box sits at y
  auto textAt = [&](const QString& txt, double tx, double ty, const QString& align, const QColor& col) {
    const QFontMetricsF fm(ctx.font());
    const double w = fm.horizontalAdvance(txt);
    const double left = align == "center" ? tx - w / 2 : align == "right" ? tx - w : tx;
    ctx.setOpacity(alpha);
    ctx.setPen(col);
    ctx.drawText(QPointF(left, ty + (fm.ascent() - fm.descent()) / 2), txt);
    ctx.setOpacity(1);
  };
  auto label = [&](const QString& txt, double lx, double ly, const QString& align = "left", const QColor& bg = {}) {
    if (bg.isValid()) {
      const double w = QFontMetricsF(ctx.font()).horizontalAdvance(txt) + 10;
      ctx.fillRect(QRectF(align == "center" ? lx - w / 2 : align == "right" ? lx - w : lx, ly - 9, w, 18), bg);
      textAt(txt, align == "center" ? lx : align == "right" ? lx - 5 : lx + 5, ly, align, QColor(Qt::white));
      fill = color;
    } else textAt(txt, lx, ly, align, fill);
  };
  struct XY { double x, y; };
  auto extend = [](const P& p, const P& q, bool both) -> std::pair<XY, XY> {
    const double dx = q.x - p.x, dy = q.y - p.y;
    if (dx == 0 && dy == 0) return {{p.x, p.y}, {q.x, q.y}};
    const double far = 1e5, len = std::hypot(dx, dy);
    const XY e2{p.x + dx / len * far, p.y + dy / len * far};
    const XY e1 = both ? XY{p.x - dx / len * far, p.y - dy / len * far} : XY{p.x, p.y};
    return {e1, e2};
  };
  auto price = [](double v) { return pane::fmtPrice(v); };

  const QString& t = d.type;
  if (t == "trend") {
    line(a.x, a.y, b.x, b.y);
  } else if (t == "ray" || t == "extended") {
    const auto [p, q] = extend(a, b, t == "extended");
    line(p.x, p.y, q.x, q.y);
  } else if (t == "hline") {
    line(0, a.y, size.width(), a.y);
    label(price(a.price), size.width() - 4, a.y - 10, "right");
  } else if (t == "hray") {
    line(a.x, a.y, size.width(), a.y);
    label(price(a.price), size.width() - 4, a.y - 10, "right");
  } else if (t == "vline") {
    line(a.x, 0, a.x, size.height());
  } else if (t == "channel") {
    line(a.x, a.y, b.x, b.y);
    if (c) {
      const double dxab = (b.x - a.x) != 0 ? (b.x - a.x) : 1;
      const double off = c->y - (a.y + (b.y - a.y) * (c->x - a.x) / dxab);
      line(a.x, a.y + off, b.x, b.y + off);
      QPainterPath path;
      path.moveTo(a.x, a.y);
      path.lineTo(b.x, b.y);
      path.lineTo(b.x, b.y + off);
      path.lineTo(a.x, a.y + off);
      path.closeSubpath();
      ctx.setOpacity(0.08);
      ctx.fillPath(path, fill);
      ctx.setOpacity(1);
    }
  } else if (t == "rect") {
    fillRect(a.x, a.y, b.x - a.x, b.y - a.y, fill, 0.12);
    ctx.setPen(QPen(stroke, lineWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    ctx.setBrush(Qt::NoBrush);
    ctx.drawRect(QRectF(a.x, a.y, b.x - a.x, b.y - a.y).normalized());
  } else if (t == "fib") {
    const double x1 = std::min(a.x, b.x), x2 = std::max(a.x, b.x);
    stroke = plot::cssColor(d.color.isEmpty() ? kFib : d.color);
    for (double lv : fibLevels()) {
      const double p = b.price + (a.price - b.price) * lv;
      const auto yy = y(p);
      if (!yy) continue;
      alpha = lv == 0.5 || lv == 0.618 ? 1 : 0.7;
      line(x1, *yy, x2, *yy);
      alpha = 1;
      fill = stroke;
      label(QString("%1 (%2)").arg(series::numStr(lv), price(p)), x1 + 2, *yy - 8);
    }
    dash = {3, 3};
    line(a.x, a.y, b.x, b.y);
    dash.clear();
  } else if (t == "measure") {
    const bool isUp = b.price >= a.price;
    fillRect(a.x, a.y, b.x - a.x, b.y - a.y, isUp ? up(0.15) : down(0.15));
    stroke = isUp ? up(1) : down(1);
    line((a.x + b.x) / 2, a.y, (a.x + b.x) / 2, b.y);
    const double la = pane->logicalOfF(b.time).value_or(NaN), lb = pane->logicalOfF(a.time).value_or(NaN);
    const double bars = std::round(la - lb);
    const double diff = b.price - a.price, pct = diff / a.price * 100;
    fill = isUp ? up(1) : down(1);
    // n only picks the French singular/plural form ("barre" / "barres"); the English text is the source
    const int nBars = std::isfinite(bars) ? int(std::min(std::fabs(bars), 1e9)) : 2;
    label(QCoreApplication::translate("charts", "%1%2 (%3%4%)  %5 bars", nullptr, nBars)
              .arg(diff >= 0 ? "+" : "", price(diff), pct >= 0 ? "+" : "", series::toFixed(pct, 2), series::numStr(bars)),
          (a.x + b.x) / 2, b.y + (isUp ? -14 : 14), "center", isUp ? up(0.9) : down(0.9));
  } else if (t == "long" || t == "short") {
    // first point = entry, second point sets the stop (its price) and the width (its time); target = 2R
    const double dir = t == "long" ? 1 : -1;
    double risk = std::fabs(a.price - b.price);
    if (!(risk != 0) || std::isnan(risk)) risk = a.price * 0.01;
    const double rr = (std::isnan(d.rr) || d.rr == 0) ? 2 : d.rr;
    const double stop = a.price - dir * risk, target = a.price + dir * risk * rr;
    const double ys = y(stop).value_or(0), yt = y(target).value_or(0);   // JS null → 0 in arithmetic
    const double x1 = std::min(a.x, b.x), w = std::max(std::fabs(b.x - a.x), 40.0);
    fillRect(x1, a.y, w, yt - a.y, up(0.18));
    fillRect(x1, a.y, w, ys - a.y, down(0.18));
    stroke = QColor("#787b86");
    line(x1, a.y, x1 + w, a.y);
    fill = up(1);
    label(QCoreApplication::translate("charts", "Target %1 (%2%)").arg(price(target), series::toFixed((target - a.price) / a.price * 100 * dir, 2)),
          x1 + w / 2, yt + (dir > 0 ? 10 : -10), "center", up(0.9));
    label(QCoreApplication::translate("charts", "Stop %1 (%2%)").arg(price(stop), series::toFixed((a.price - stop) / a.price * 100 * dir, 2)),
          x1 + w / 2, ys + (dir > 0 ? -10 : 10), "center", down(0.9));
    fill = QColor("#787b86");
    label(QCoreApplication::translate("charts", "Entry %1  R:R %2").arg(price(a.price), series::toFixed(rr, 1)), x1 + 4, a.y - 9);
  } else if (t == "text") {
    ctx.setFont(style::font(13));
    label(d.text.isEmpty() ? QCoreApplication::translate("charts", "Note") : d.text, a.x, a.y, "left", color);
    ctx.setFont(style::font(11));
  }
  if (active) {
    ctx.setPen(QPen(color, 1.5));
    ctx.setBrush(Qt::white);
    for (const P& p : pts) ctx.drawEllipse(QPointF(p.x, p.y), 4, 4);
    ctx.setBrush(Qt::NoBrush);
  }
}

QString DrawingLayer::drawingAt(double hx, double hy) const {
  QString best;
  double bestD = 7;
  struct XY { double x, y; };
  auto segDist = [&](XY p, XY q) {
    const double dx = q.x - p.x, dy = q.y - p.y, l2 = dx * dx + dy * dy;
    const double t = l2 ? std::max(0.0, std::min(1.0, ((hx - p.x) * dx + (hy - p.y) * dy) / l2)) : 0;
    return std::hypot(hx - (p.x + t * dx), hy - (p.y + t * dy));
  };
  for (const Item& d : items) {
    QVector<XY> pts;
    bool ok = true;
    for (const Point& p : d.points) {
      const auto xx = x(p.time);
      const auto yy = y(p.price);
      if (!xx || !yy) { ok = false; break; }
      pts.append({*xx, *yy});
    }
    if (!ok || pts.isEmpty()) continue;
    const XY a = pts[0];
    const bool hasB = pts.size() > 1;
    const XY b = hasB ? pts[1] : pts[0];
    double dist = std::numeric_limits<double>::infinity();
    const QString& t = d.type;
    if (t == "hline") dist = std::fabs(hy - a.y);
    else if (t == "hray") dist = hx >= a.x - 4 ? std::fabs(hy - a.y) : dist;
    else if (t == "vline") dist = std::fabs(hx - a.x);
    else if (t == "text") dist = hx >= a.x - 4 && hx <= a.x + 120 && std::fabs(hy - a.y) < 12 ? 0 : dist;
    else if (t == "rect" || t == "fib" || t == "measure" || t == "long" || t == "short") {
      if (hasB) {
        const bool inside = hx >= std::min(a.x, b.x) - 4 && hx <= std::max({a.x, b.x, a.x + 40}) + 4 &&
                            hy >= std::min(a.y, b.y) - 40 && hy <= std::max(a.y, b.y) + 40;
        dist = inside ? 3 : dist;
      }
    } else if (hasB) {
      auto far = [](XY p, XY q, bool both) -> std::pair<XY, XY> {
        const double dx = q.x - p.x, dy = q.y - p.y;
        double len = std::hypot(dx, dy);
        if (!len) len = 1;
        return {both ? XY{p.x - dx / len * 1e5, p.y - dy / len * 1e5} : p, XY{p.x + dx / len * 1e5, p.y + dy / len * 1e5}};
      };
      const auto [p, q] = t == "ray" ? far(a, b, false) : t == "extended" ? far(a, b, true) : std::pair<XY, XY>{a, b};
      dist = segDist(p, q);
    }
    if (dist < bestD) {
      bestD = dist;
      best = d.id;
    }
  }
  return best;
}

}  // namespace charts::ui
