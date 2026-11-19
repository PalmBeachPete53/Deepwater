#include "pages/global/overview/GlobePanel.h"
#include <QCoreApplication>
#include "global/Look.h"
#include "global/Sessions.h"
#include "global/FrameClock.h"
#include "ui/Theme.h"
#include <QDateTime>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>
#include <cmath>

namespace look = global::look;

namespace overview {

static const double kTiltDeg = 18.0;          // centre latitude: the north pole leans toward the viewer
static const double kTurnMs = 120000.0;       // one revolution per 2 min (reference: ≈20 s in a 6x time-lapse)

GlobePanel::GlobePanel(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  connect(&global::FrameClock::instance(), &global::FrameClock::tick, this, [this](int frame) {
    if (!m_running || frame % 2) return;   // every other frame of the 30 Hz page clock: 15 fps
    update(globeRect().adjusted(-8, -8, 8, 8).toAlignedRect());
  });
}

void GlobePanel::showEvent(QShowEvent*) { m_running = true; global::FrameClock::instance().acquire(this); }
void GlobePanel::hideEvent(QHideEvent*) { m_running = false; global::FrameClock::instance().release(this); }
void GlobePanel::resizeEvent(QResizeEvent*) { prepare(); }

QRectF GlobePanel::globeRect() const { return QRectF(m_c.x() - m_R, m_c.y() - m_R, 2 * m_R, 2 * m_R); }

double GlobePanel::centreLon() const {
  // tied to the wall clock so the globe does not jump when the page comes back
  const double t = std::fmod(double(QDateTime::currentMSecsSinceEpoch()), kTurnMs) / kTurnMs;
  return 20.0 - 360.0 * t;     // the surface moves west → east (left → right)
}

void GlobePanel::prepare() {
  if (width() < 40 || height() < 60) return;
  m_dpr = devicePixelRatioF();
  // radius capped at 190 px: on big screens the globe stays crisp but its per-frame cost stays near the reference
  m_R = qBound<qreal>(30, qMin(width() - 60.0, height() - 70.0) / 2.0, 190);
  m_c = QPointF(width() / 2.0, 30 + (height() - 30) / 2.0);

  // ---- static background
  m_bg = QPixmap(size() * m_dpr);
  m_bg.setDevicePixelRatio(m_dpr);
  {
    QPainter p(&m_bg);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF R(rect());
    const qreal far = std::hypot(qMax(m_c.x(), width() - m_c.x()), qMax(m_c.y(), height() - m_c.y()));
    QRadialGradient haze(m_c, far);
    haze.setColorAt(0, theme::map("#3a2405"));
    haze.setColorAt(m_R / far, theme::map("#2a1a04"));
    haze.setColorAt(qMin(0.99, (m_R + 60) / far), theme::map("#140d03"));
    haze.setColorAt(1, theme::map("#0a0802"));
    p.fillRect(R, haze);
    p.setPen(QPen(theme::map("#433d2c"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(R.adjusted(0.5, 0.5, -0.5, -0.5));
    look::brackets(p, R);
    look::panelTitle(p, R, QCoreApplication::translate("global", "GLOBAL MAP"));
    // atmosphere: orange-gold rim glow outside the limb
    // soft, wide glow (reference): no bright ring at the limb, a warm haze fading over ~28 px
    const qreal ext = 30;
    // Deepwater: the measured amber glow; other themes: the theme's halo role at the same strengths
    auto atmC = [](int r, int g, int b, int a) {
      if (theme::isDefault()) return QColor(r, g, b, a);
      QColor h = theme::roles().halo; h.setAlpha(a); return h;
    };
    QRadialGradient atm(m_c, m_R + ext);
    const qreal k = m_R / (m_R + ext);
    atm.setColorAt(0, atmC(196, 138, 26, 0));
    atm.setColorAt(k - 0.03, atmC(196, 138, 26, 70));
    atm.setColorAt(k, atmC(200, 140, 28, 125));
    atm.setColorAt(k + (1 - k) * 0.25, atmC(196, 138, 26, 75));
    atm.setColorAt(k + (1 - k) * 0.55, atmC(196, 138, 26, 30));
    atm.setColorAt(k + (1 - k) * 0.8, atmC(196, 138, 26, 9));
    atm.setColorAt(1, atmC(196, 138, 26, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(atm);
    p.drawEllipse(m_c, m_R + ext, m_R + ext);
  }

  // ---- the sphere: static, cached for this radius
  ui::DotGlobe::Camera cam; cam.centre = m_c; cam.R = m_R; cam.lon = centreLon(); cam.tilt = kTiltDeg;
  m_globe.setCamera(cam);
  m_body = m_globe.body(m_dpr);
  // the sphere never moves on the panel: composed on the background once, not blended at every frame
  {
    QPainter p(&m_bg);
    p.drawPixmap(QPoint(qRound(m_c.x() - m_R - 2), qRound(m_c.y() - m_R - 2)), m_body);
  }
}

void GlobePanel::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  p.setClipRegion(e->region());
  p.drawPixmap(0, 0, m_bg);
  if (m_R <= 0) return;
  ui::DotGlobe::Camera cam = m_globe.camera();
  cam.lon = centreLon();
  m_globe.setCamera(cam);
  m_globe.paintGraticule(p);
  m_globe.paintLand(p);

  // session centres on the visible hemisphere
  p.setRenderHint(QPainter::Antialiasing);
  const QDateTime utc = global::nowUtc();
  for (const auto& s : global::sessions()) {
    const QVector3D v = m_globe.view(ui::DotGlobe::unit(s.lat, s.lon));
    const float z = v.z() - ui::DotGlobe::kHorizon;
    if (z < 0.06f) continue;
    const QPointF at = m_globe.projectView(v);
    const int a = int(255 * qBound(0.0, (z - 0.06) / 0.2, 1.0));
    const bool open = global::isOpen(s, utc);
    QColor ring = global::sessionColour(s, global::Tone::Bright); ring.setAlpha(a * (open ? 1.0 : 0.8));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ring, 1.4));
    p.drawEllipse(at, open ? 5.0 : 4.0, open ? 5.0 : 4.0);
    p.setPen(Qt::NoPen);
    QColor dot = theme::mix(global::sessionColour(s, global::Tone::Bright), theme::map("#ffffff"), 0.15); dot.setAlpha(a);
    p.setBrush(dot);
    p.drawEllipse(at, 1.8, 1.8);
  }
}
}  // namespace overview
