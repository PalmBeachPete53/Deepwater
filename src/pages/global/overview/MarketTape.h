#pragma once
#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include <QElapsedTimer>

namespace overview {

// Overview top bar: LIVE pill, scrolling quotes (market::QuoteHub, 37 px/s, seamless loop, faded ends) and the
// four FX session clocks (global::sessions()).
class MarketTape : public QWidget {
public:
  explicit MarketTape(QWidget* parent);
  static QStringList symbols();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  QRectF scrollRect() const;
  qreal clocksLeft() const;
  bool showClocks() const;
  void rebuildStrip();
  QPixmap m_strip;            // one loop of the items
  QPixmap m_scrollBg;         // the bar's gradient under the scroll area (frames copy it)
  qreal m_stripW = 0;
  qreal m_offset = 0;         // px scrolled
  QTimer m_slow;
  bool m_running = false;
  QElapsedTimer m_since;
  qint64 m_lastMs = 0;
  QString m_sessionKey;
};
}  // namespace overview
