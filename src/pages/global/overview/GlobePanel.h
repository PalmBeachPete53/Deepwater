#pragma once
#include <QWidget>
#include <QPixmap>
#include "ui/DotGlobe.h"

namespace overview {

// "GLOBAL MAP": the dot-matrix globe of News › Bank Research (ui::DotGlobe), sized to this panel, inside the
// panel's amber haze and atmosphere, with the four FX-session centres marked in their session colours.
// Rotates one turn per 2 min on the page clock at 15 fps, stopped when hidden.
class GlobePanel : public QWidget {
public:
  explicit GlobePanel(QWidget* parent);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  void prepare();             // background and sphere for the current size
  QRectF globeRect() const;
  double centreLon() const;   // degrees
  QPixmap m_bg;               // panel, haze, atmosphere glow
  QPixmap m_body;             // static sphere (gradient + rim)
  ui::DotGlobe m_globe;
  bool m_running = false;
  qreal m_R = 0, m_dpr = 2;
  QPointF m_c;
};
}  // namespace overview
