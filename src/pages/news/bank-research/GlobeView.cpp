#include "pages/news/bank-research/GlobeView.h"
#include "pages/news/bank-research/Look.h"
#include "banks/Registry.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QPainterPath>
#include <QMouseEvent>
#include <QImage>
#include <QRandomGenerator>
#include <QToolTip>
#include <QtMath>
#include <cmath>
#include <algorithm>

namespace banks::res {

namespace {
constexpr float kDist = 3.0f, kHorizon = 1.0f / kDist;
QVector3D unit(double latDeg, double lonDeg) {
  const double la = qDegreesToRadians(latDeg), lo = qDegreesToRadians(lonDeg);
  return QVector3D(float(std::cos(la) * std::sin(lo)), float(std::sin(la)), float(std::cos(la) * std::cos(lo)));
}
QVector3D slerp(const QVector3D& a, const QVector3D& b, float t) {
  const float d = qBound(-1.0f, QVector3D::dotProduct(a, b), 1.0f);
  const float th = std::acos(d);
  if (th < 1e-4f) return a;
  const float s = std::sin(th);
  return a * (std::sin((1 - t) * th) / s) + b * (std::sin(t * th) / s);
}
const QColor kGrey(70, 72, 78), kGold(214, 190, 92), kGoldHot(238, 214, 110);
}

GlobeView::GlobeView(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_hint = QCoreApplication::translate("banks", "SELECT A BEACON OR A BANK TO OPEN ITS FILE");
  bool ok = false;
  const double lon = qEnvironmentVariable("DW_GLOBE_LON").toDouble(&ok);
  if (ok) { m_frozen = true; m_lon0 = lon; }
  m_spin = m_lon0;
  m_sweep = 221;                        // leading edge at the lower left (reference t = 0)
  buildDots();
  // label priority when chips overlap: the reference shows TD (not RY) in Toronto and JPM / GS in New York
  QList<banks::Bank> order = banks::systemic();
  std::stable_sort(order.begin(), order.end(), [](const banks::Bank& a, const banks::Bank& b) {
    static const QStringList first{"jpm", "td", "gs", "bac", "wfc", "nwg", "ing", "lyg", "ubs", "san"};
    const int ia = first.indexOf(a.id), ib = first.indexOf(b.id);
    return (ia < 0 ? 99 : ia) < (ib < 0 ? 99 : ib);
  });
  for (const auto& b : order) m_beacons << Beacon{b.id, b.ticker, unit(b.lat, b.lon)};
  const QFontMetricsF lfm(theme::mono(10, QFont::Bold, 0.5));   // the chips' font (paintEvent)
  for (const auto& b : std::as_const(m_beacons)) m_labelW << lfm.horizontalAdvance(b.label);
  auto idx = [this](const QString& id) { for (int i = 0; i < m_beacons.size(); ++i) if (m_beacons[i].bankId == id) return i; return -1; };
  const QList<QPair<QString, QString>> arcs{{"jpm", "hsbc"}, {"wfc", "mufg"}, {"db", "mufg"}, {"ing", "nmr"}, {"bac", "san"}};
  for (const auto& [a, b] : arcs) { const int i = idx(a), j = idx(b); if (i >= 0 && j >= 0) m_arcs << qMakePair(i, j); }
  m_timer.setInterval(66);   // 15 fps: the spin is slow enough; measured 25.6 % CPU at 25 fps full repaint
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, &GlobeView::tick);
}

void GlobeView::buildDots() {
  QImage mask(":/banks/land.pbm");
  if (mask.isNull()) return;
  mask = mask.convertToFormat(QImage::Format_Grayscale8);
  const double step = 1.1;   // ≈ 7 px pitch at the centre of the perspective view, as in the reference
  for (double lat = -84; lat <= 84; lat += step) {
    const int n = qMax(1, int(std::round(360.0 * std::cos(qDegreesToRadians(lat)) / step)));
    for (int k = 0; k < n; ++k) {
      const double lon = -180.0 + (k + 0.5) * 360.0 / n;
      const int px = qBound(0, int((lon + 180.0) / 360.0 * mask.width()), mask.width() - 1);
      const int py = qBound(0, int((90.0 - lat) / 180.0 * mask.height()), mask.height() - 1);
      if (mask.constScanLine(py)[px] < 128) m_dots << unit(lat, lon);   // PBM: 1 (black) = land
    }
  }
}

QPointF GlobeView::centre() const { return QPointF(width() / 2.0, height() / 2.0 - 5); }
// 0.30 × the smaller side (269 px at the reference size). Capped at 330 px: the frame cost grows with the area,
// and on 1440p / ultrawide screens a bigger globe would only add pixels, not detail.
qreal GlobeView::radius() const { return qMin<qreal>(330, 0.30 * qMin(width(), height())); }

// The camera of the frame: spin and tilt cosines, centre and projection scale, computed once per frame (view() and
// project() run ~12,000 times a frame) with the same expressions as before, so the same values.
void GlobeView::setupFrame() {
  const double c = qDegreesToRadians(m_spin), t = qDegreesToRadians(m_tilt);
  m_cosS = std::cos(c); m_sinS = std::sin(c); m_cosT = std::cos(t); m_sinT = std::sin(t);
  m_pc = centre();
  m_pk = radius() * std::sqrt(kDist * kDist - 1.0);
}

QVector3D GlobeView::view(const QVector3D& v) const {
  const double x = v.x() * m_cosS - v.z() * m_sinS;
  const double z = v.z() * m_cosS + v.x() * m_sinS;
  const double y = v.y();
  return QVector3D(float(x), float(y * m_cosT - z * m_sinT), float(y * m_sinT + z * m_cosT));
}

// Perspective projection (camera kDist sphere radii from the centre), fitted on the reference's beacon positions:
// the silhouette has radius R on screen, points beyond the horizon (z ≤ 1 / kDist) are hidden.
QPointF GlobeView::project(const QVector3D& v) const {
  const qreal w = kDist - v.z();
  return QPointF(m_pc.x() + m_pk * v.x() / w, m_pc.y() - m_pk * v.y() / w);
}

// Where the frame draws what moves: the disc (sphere, graticule, land), the raised arcs that leave it, and the beacons'
// bars and chips. setupFrame() first. Same tests as paintEvent.
QRect GlobeView::contentRect() const {
  const QPointF c = m_pc; const qreal R = radius();
  qreal x0 = c.x() - R - 3, y0 = c.y() - R - 3, x1 = c.x() + R + 3, y1 = c.y() + R + 3;
  auto add = [&](const QRectF& r) { x0 = std::min(x0, r.left()); y0 = std::min(y0, r.top()); x1 = std::max(x1, r.right()); y1 = std::max(y1, r.bottom()); };
  for (const auto& [i, j] : m_arcs) {
    const QVector3D A = m_beacons[i].p, B = m_beacons[j].p;
    const float ang = std::acos(qBound(-1.0f, QVector3D::dotProduct(A, B), 1.0f));
    const float h = 0.03f + 0.035f * ang;
    for (int k = 0; k <= 40; ++k) {
      const float t = k / 40.0f;
      const QVector3D v = view(slerp(A, B, t).normalized() * (1.0f + h * std::sin(float(M_PI) * t)));
      const QPointF s = project(v);
      if (v.z() > kHorizon * v.length() || QLineF(s, c).length() > R * 1.01) add(QRectF(s, QSizeF(1, 1)));
    }
  }
  for (int i = 0; i < m_beacons.size(); ++i) {
    const QVector3D v = view(m_beacons[i].p);
    if (v.z() < kHorizon + 0.06f) continue;
    const QPointF s = project(v);
    add(QRectF(s.x() - 1, s.y() - 16, 2, 16));
    add(QRectF(s.x() + 3, s.y() - 16, m_labelW.value(i) + 8, 15));
  }
  return QRectF(QPointF(x0, y0), QPointF(x1, y1)).adjusted(-2, -2, 2, 2).toAlignedRect();
}

// Bounding box of the radar wedge at the current sweep angle (slices from m_sweep − 60° to m_sweep, radius 1.48 R).
QRect GlobeView::wedgeRect() const {
  const QPointF c = m_pc; const qreal rr = radius() * 1.48;
  qreal x0 = c.x(), y0 = c.y(), x1 = c.x(), y1 = c.y();
  auto add = [&](double deg) {
    const double a = qDegreesToRadians(deg);
    const qreal x = c.x() + rr * std::cos(a), y = c.y() - rr * std::sin(a);   // Qt angles: counter-clockwise from 3 o'clock
    x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
  };
  const double lo = m_sweep - 61, hi = m_sweep + 1;
  add(lo); add(hi);
  for (double q = std::ceil(lo / 90) * 90; q < hi; q += 90) add(q);
  return QRectF(QPointF(x0, y0), QPointF(x1, y1)).adjusted(-2, -2, 2, 2).toAlignedRect();
}

void GlobeView::setPaused(bool p) {
  m_paused = p;
  if (p) m_timer.stop();
  else if (isVisible() && !m_frozen) { m_clock.restart(); m_timer.start(); }
}

void GlobeView::tick() {
  const double dt = m_clock.restart() / 1000.0;
  m_spin -= 14.0 * dt;                 // land drifts right (≈14°/s, as in the reference)
  if (m_spin < -180) m_spin += 360;
  m_sweep -= 23.0 * dt;                // clockwise, ≈23°/s measured on the reference
  if (m_sweep < 0) m_sweep += 360;
  // what the last painted frame drew where things move, and what this one will draw: the starfield, rings and the
  // rest of the square around the globe don't change
  // one rectangle: a region of several rects makes every fill clip against each of them (the 4,500 land dots cost twice)
  setupFrame();
  update(m_painted | contentRect() | wedgeRect());
  emit frame();
}

void GlobeView::showEvent(QShowEvent*) { if (!m_frozen && !m_paused) { m_clock.restart(); m_timer.start(); } }
void GlobeView::hideEvent(QHideEvent*) { m_timer.stop(); }
void GlobeView::resizeEvent(QResizeEvent*) { m_bg = {}; m_body = {}; m_bgBody = {}; }

void GlobeView::buildBackground() {
  const qreal dpr = devicePixelRatioF();
  m_bg = QPixmap(size() * dpr); m_bg.setDevicePixelRatio(dpr);
  m_bg.fill(banks::res::look::page);
  QPainter p(&m_bg); p.setRenderHint(QPainter::Antialiasing);
  const QPointF c = centre(); const qreal R = radius();
  // hint box: the starfield and the dotted ring leave it clear (short centres: the ring crossed the text)
  const QFont hintFont = theme::mono(11, QFont::Normal, 2.7);
  const QString hint = QFontMetricsF(hintFont).elidedText(m_hint, Qt::ElideRight, width() - 20);
  const qreal hintW = QFontMetricsF(hintFont).horizontalAdvance(hint);
  const QRectF hintBox(width() / 2.0 - hintW / 2 - 14, height() - 34 - 6, hintW + 28, 22 + 12);
  // starfield (decoration, fixed seed so it does not jump between frames or runs)
  QRandomGenerator rng(20261114);
  p.setPen(Qt::NoPen);
  const int stars = int(width() * height() / 7000);
  for (int i = 0; i < stars; ++i) {
    const QPointF s(rng.bounded(double(width())), rng.bounded(double(height())));
    if (QLineF(s, c).length() < R * 1.05 || hintBox.contains(s)) continue;
    const qreal sz = 1.6 + rng.bounded(1.4);
    p.setBrush(theme::map(43, 41, 39, 120 + rng.bounded(110)));
    p.drawRect(QRectF(s, QSizeF(sz, sz)));
  }
  // outer dotted ring
  const qreal r2 = R * 1.59;
  const int nd = int(2 * M_PI * r2 / 15);
  for (int i = 0; i < nd; ++i) {
    const double a = 2 * M_PI * i / nd;
    const QPointF d = c + QPointF(std::cos(a) * r2, std::sin(a) * r2);
    if (hintBox.contains(d)) continue;
    p.setBrush(theme::map(39, 38, 35));
    p.drawEllipse(d, 1.3, 1.3);
  }
  // orbit ring: thin gold circle fading round the circumference
  const qreal r1 = R * 1.48;
  QConicalGradient cg(c, 30);
  cg.setColorAt(0.0, theme::map(141, 132, 82, 230));
  cg.setColorAt(0.25, theme::map(109, 106, 81, 120));
  cg.setColorAt(0.5, theme::map(109, 106, 81, 40));
  cg.setColorAt(0.7, theme::map(109, 106, 81, 15));
  cg.setColorAt(0.9, theme::map(109, 106, 81, 120));
  cg.setColorAt(1.0, theme::map(141, 132, 82, 230));
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QBrush(cg), 1.2));
  p.drawEllipse(c, r1, r1);
  // halo around the sphere (themes other than Deepwater, like the preview)
  if (!theme::isDefault()) {
    QRadialGradient hg(c, R * 1.25);
    const QColor h = theme::roles().halo;
    hg.setColorAt(0.78, h); hg.setColorAt(1.0, theme::alpha(h, 0));
    p.setPen(Qt::NoPen); p.setBrush(hg);
    p.drawEllipse(c, R * 1.25, R * 1.25);
  }
  // hint
  p.setFont(hintFont); p.setPen(theme::map("#52514f"));
  p.drawText(QRectF(0, height() - 34, width(), 22), Qt::AlignCenter, hint);
  p.end();

  // globe body: dark navy sphere lit from the upper left, darker rim
  m_body = QPixmap(QSize(int(2 * R + 4), int(2 * R + 4)) * dpr); m_body.setDevicePixelRatio(dpr);
  m_body.fill(Qt::transparent);
  QPainter b(&m_body); b.setRenderHint(QPainter::Antialiasing);
  const QPointF bc(R + 2, R + 2);
  QRadialGradient rg(bc + QPointF(-R * 0.35, -R * 0.4), R * 1.45);
  if (theme::isDefault()) {          // Deepwater: the measured navy sphere, unchanged
    rg.setColorAt(0.0, QColor(34, 38, 48));
    rg.setColorAt(0.45, QColor(20, 23, 31));
    rg.setColorAt(0.85, QColor(12, 14, 19));
    rg.setColorAt(1.0, QColor(8, 9, 12));
  } else {                           // other themes: the preview's ocean roles, as ui::DotGlobe
    const auto& r = theme::roles();
    rg.setColorAt(0.0, theme::mix(r.oceanInner, Qt::white, theme::isDark() ? 0.06 : 0.0));
    rg.setColorAt(0.5, r.oceanInner);
    rg.setColorAt(0.9, r.oceanOuter);
    rg.setColorAt(1.0, theme::mix(r.oceanOuter, QColor(r.limbShade.red(), r.limbShade.green(), r.limbShade.blue()), r.limbShade.alphaF()));
  }
  b.setPen(Qt::NoPen); b.setBrush(rg);
  b.drawEllipse(bc, R, R);
  b.setBrush(Qt::NoBrush); b.setPen(QPen(theme::isDefault() ? QColor(60, 62, 68, 120) : theme::roles().landBorder, 1.0));
  b.drawEllipse(bc, R, R);
  b.end();
  // the background with the sphere on it: frames copy it, and redo background + wedge + sphere only where the wedge is
  m_bgBody = m_bg.copy();
  QPainter bb(&m_bgBody);
  bb.drawPixmap(QPoint(qRound(c.x() - R - 2), qRound(c.y() - R - 2)), m_body);
}

void GlobeView::paintEvent(QPaintEvent*) {
  if (m_bg.isNull() || m_bg.size() != size() * devicePixelRatioF() || m_bgBody.isNull()) buildBackground();
  setupFrame();
  const QRect wedge = wedgeRect();
  m_painted = contentRect() | wedge;   // the next frame repaints this (tick)
  QPainter p(this);
  p.setCompositionMode(QPainter::CompositionMode_Source);   // opaque background: plain copy
  p.drawPixmap(0, 0, m_bgBody);
  // where the wedge is: background, wedge, then the sphere over it, as everywhere else the sphere sits on the background
  p.save();
  p.setClipRect(wedge, Qt::IntersectClip);
  p.drawPixmap(0, 0, m_bg);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setRenderHint(QPainter::Antialiasing);
  const QPointF c = centre(); const qreal R = radius();

  // radar sweep behind the globe: wedge with its bright leading edge in the direction of travel (clockwise)
  {
    // Flat slices of fading alpha, no antialiasing: a conical gradient evaluates an angle per pixel per frame
    // (it was the page's largest cost). Edges between slices are invisible at these alphas.
    const qreal rr = R * 1.48;
    const QRectF box(c.x() - rr, c.y() - rr, 2 * rr, 2 * rr);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    // Measured on the reference: ≈60° wide; the sharp, brightest edge (#2c2605, alpha ≈ 70 of this gold on the
    // page) is the trailing one at m_sweep, the wedge fades out towards the leading edge (clockwise, smaller angles).
    constexpr qreal kWidth = 60.0;
    constexpr int kSlices = 16; constexpr qreal kSpan = kWidth / kSlices;
    for (int i = 0; i < kSlices; ++i) {
      const qreal t = (i + 0.5) / kSlices;                       // 0 leading … 1 trailing
      const int a = int(std::lround(70 * std::pow(1.0 - t, 1.25)));
      if (a <= 0) continue;
      p.setBrush(theme::map(150, 122, 24, a));
      p.drawPie(box, int(std::lround((m_sweep - (i + 1) * kSpan) * 16)), int(std::ceil(kSpan * 16)));
    }
    p.setRenderHint(QPainter::Antialiasing, true);
  }
  p.drawPixmap(QPoint(qRound(c.x() - R - 2), qRound(c.y() - R - 2)), m_body);   // whole pixels: no interpolation
  p.restore();
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setRenderHint(QPainter::Antialiasing);

  // graticule every 15°
  {
    // polylines (much cheaper to stroke than one big antialiased path)
    QVector<QPointF> run; run.reserve(64);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(theme::isDefault() ? QColor(58, 62, 72) : theme::roles().graticule, 1.0)); p.setBrush(Qt::NoBrush);
    auto flush = [&] { if (run.size() > 1) p.drawPolyline(run.constData(), int(run.size())); run.clear(); };
    auto trace = [&](auto pointAt, int n) {
      for (int i = 0; i <= n; ++i) {
        const QVector3D v = view(pointAt(i));
        if (v.z() <= kHorizon) { flush(); continue; }
        run << project(v);
      }
      flush();
    };
    for (int lat = -75; lat <= 75; lat += 15) trace([lat](int i) { return unit(lat, -180 + i * 6.0); }, 60);
    for (int lon = -180; lon < 180; lon += 15) trace([lon](int i) { return unit(-90 + i * 5.0, lon); }, 36);
    p.setRenderHint(QPainter::Antialiasing, true);
  }

  // land dots, batched by colour level
  {
    const QVector3D L = QVector3D(-0.50f, 0.30f, 0.81f).normalized();   // gold = North America at t = 0, as in the reference
    constexpr int kLevels = 10;
    QVector<QRectF>* buckets = m_buckets;   // cleared, not freed: no reallocation every frame
    for (int i = 0; i < kLevels; ++i) buckets[i].clear();
    for (const auto& d : m_dots) {
      const QVector3D v = view(d);
      if (v.z() <= kHorizon + 0.01f) continue;
      const float lit = QVector3D::dotProduct(v, L);
      const float f = qBound(0.0f, (lit - 0.74f) / 0.14f, 1.0f);           // 0 grey … 1 gold
      const float limb = qMin(1.0f, (v.z() - kHorizon) * 3.4f);
      int lvl = int(f * (kLevels / 2 - 1) + 0.5f) * 2 + (limb > 0.6f ? 1 : 0);
      lvl = qBound(0, lvl, kLevels - 1);
      const qreal sz = 1.5 + 1.25 * limb;
      const QPointF q = project(v);
      buckets[lvl] << QRectF(q.x() - sz / 2, q.y() - sz / 2, sz, sz);
    }
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::Antialiasing, false);
    // Deepwater: grey → gold; other themes: land border (unlit) → accent (lit), like ui::DotGlobe
    const QColor grey = theme::isDefault() ? kGrey : theme::roles().landBorder;
    const QColor gold = theme::isDefault() ? kGold : theme::roles().accent;
    const QColor hot = theme::isDefault() ? kGoldHot : theme::mix(theme::roles().accent, theme::roles().text, 0.2);
    for (int i = 0; i < kLevels; ++i) {
      if (buckets[i].isEmpty()) continue;
      const qreal f = (i / 2) / qreal(kLevels / 2 - 1);
      QColor col = theme::mix(grey, f > 0.85 ? hot : gold, f);
      col.setAlpha(i % 2 ? 255 : 150);
      p.setBrush(col);
      p.drawRects(buckets[i].constData(), int(buckets[i].size()));
    }
    p.setRenderHint(QPainter::Antialiasing, true);
  }

  // arcs between financial centres (raised great circles)
  {
    QVector<QPointF> run; run.reserve(50);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme::map(120, 112, 66), 1.0));
    auto flush = [&] { if (run.size() > 1) p.drawPolyline(run.constData(), int(run.size())); run.clear(); };
    for (const auto& [i, j] : m_arcs) {
      const QVector3D A = m_beacons[i].p, B = m_beacons[j].p;
      const float ang = std::acos(qBound(-1.0f, QVector3D::dotProduct(A, B), 1.0f));
      const float h = 0.03f + 0.035f * ang;
      for (int k = 0; k <= 40; ++k) {
        const float t = k / 40.0f;
        const QVector3D v = view(slerp(A, B, t).normalized() * (1.0f + h * std::sin(float(M_PI) * t)));
        const QPointF s = project(v);
        if (!(v.z() > kHorizon * v.length() || QLineF(s, c).length() > R * 1.01)) { flush(); continue; }
        run << s;
      }
      flush();
    }
    p.setRenderHint(QPainter::Antialiasing, true);
  }

  // beacons: bar + ticker chip; labels that would overlap a bigger bank's label are skipped
  m_hits.clear();
  {
    const QFont f = theme::mono(10, QFont::Bold, 0.5);
    p.setFont(f);
    QVector<QRectF> placed;
    for (int i = 0; i < m_beacons.size(); ++i) {
      const QVector3D v = view(m_beacons[i].p);
      if (v.z() < kHorizon + 0.06f) continue;
      const QPointF s = project(v);
      const bool hot = i == m_hover || m_beacons[i].bankId == m_selected;
      p.setPen(Qt::NoPen);
      p.setBrush(theme::map(238, 227, 148, hot ? 255 : 230));
      p.drawRect(QRectF(s.x() - 1, s.y() - 16, 2, 16));
      const QRectF chip(s.x() + 3, s.y() - 16, m_labelW.value(i) + 8, 15);
      bool clash = false;
      for (const auto& r : placed) if (r.intersects(chip.adjusted(-1, -1, 1, 1))) { clash = true; break; }
      if (clash && !hot) continue;
      placed << chip;
      m_hits << qMakePair(chip.united(QRectF(s.x() - 4, s.y() - 18, 8, 20)), i);
      p.setBrush(hot ? theme::map(72, 62, 1, 240) : theme::map(10, 9, 4, 215));
      p.drawRect(chip);
      p.setPen(hot ? theme::map("#fff2b8") : theme::map("#d1c587"));
      p.drawText(chip.adjusted(4, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, m_beacons[i].label);
    }
  }
}

void GlobeView::mouseMoveEvent(QMouseEvent* e) {
  int h = -1;
  for (const auto& [r, i] : m_hits) if (r.contains(e->position())) { h = i; break; }
  if (h != m_hover) {
    m_hover = h;
    setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (h >= 0) {
      if (const banks::Bank* b = banks::bank(m_beacons[h].bankId))
        QToolTip::showText(e->globalPosition().toPoint(), QString("%1 · %2").arg(b->legalName, banks::trs(b->city)), this);
    } else QToolTip::hideText();
    update();
  }
}
void GlobeView::mousePressEvent(QMouseEvent* e) {
  for (const auto& [r, i] : m_hits) if (r.contains(e->position())) { emit bankClicked(m_beacons[i].bankId); return; }
}
void GlobeView::leaveEvent(QEvent*) { if (m_hover >= 0) { m_hover = -1; update(); } }
}  // namespace banks::res
