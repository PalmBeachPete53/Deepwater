#pragma once
#include <QWidget>
#include <QTimer>
#include <QList>
#include <QRectF>
#include <QPixmap>

class QScrollArea;
class QVariantAnimation;

namespace banks::pos {
class MarketCard;
class CallsTable;
class DeskBody;
class ForecastOverlay;


// Positioning › Bank Positioning: DESK view (six asset-class cards counting LONG / SHORT / RANGE calls derived by
// Deepwater from the banks' published forecasts vs spot), ALL CALLS list, FX BANK FORECASTS overlay.
// Env hooks: DW_BANKPOS_HOVER=<card 0..5> shows a card hovered, DW_BANKPOS_OVERLAY=<pair or 1> opens the overlay,
// DW_BANKPOS_VIEW=1 opens ALL CALLS, DW_BANKPOS_BTNHOVER=1 shows the FX BANK FORECASTS button hovered,
// DW_BANKPOS_PROGRESS=<0..1> freezes the overlay lines' draw-in animation.
class BankPositioningPage : public QWidget {
  Q_OBJECT
public:
  explicit BankPositioningPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
  bool event(QEvent*) override;
private:
  QString titleText() const;            // full title, or the short one on narrow windows
  void layoutDesk();
  void paintNotes(QPainter& p);
  QRectF forecastBtn() const;
  QRectF deskBtn() const;
  QRectF callsBtn() const;
  void setView(int v);
  void recompute();
  void openOverlay(const QString& pair = {});
  QList<MarketCard*> m_cards;
  QScrollArea* m_deskScroll;
  DeskBody* m_desk;
  CallsTable* m_deskCalls;               // ALL CALLS shown under the cards on wide screens (≥ 1900 px)
  QVariantAnimation* m_wheel;
  qreal m_notesTop = 0, m_notesWidth = 0;
  CallsTable* m_calls;
  QScrollArea* m_callsScroll;
  ForecastOverlay* m_overlay;
  QTimer m_blink, m_throttle;
  bool m_cursorOn = true;
  int m_view = 0;                  // 0 DESK, 1 ALL CALLS
  int m_hoverBtn = -1;             // 0 forecasts, 1 desk, 2 calls
  QPixmap m_texture;
};
}  // namespace banks::pos
