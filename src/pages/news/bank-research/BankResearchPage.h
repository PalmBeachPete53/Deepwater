#pragma once
#include <QWidget>
#include "app/Preload.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>

class QScrollArea;
class QLabel;

namespace banks::res {
class Strip;
class LeftColumn;
class RegistryColumn;
class GlobeView;
class PdfPanel;


// News › Bank Research: banks' research notes (left), rotating globe with the banks' head offices or the PDF
// viewer (centre), registry of systemic banks with total assets from SEC EDGAR (right).
// Env hooks: DW_BANKRES_OPEN=<row index> opens that note at start, DW_BANKRES_HOVER=<row index> shows a row hovered,
// DW_BANKRES_SCROLL=<px> scrolls the PDF, DW_BANKRES_TAB=<0..3> picks the left tab, DW_GLOBE_LON=<deg> freezes the globe.
class BankResearchPage : public QWidget, public app::Preloadable {
  Q_OBJECT
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit BankResearchPage(QWidget* parent = nullptr);
protected:
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void paintEvent(QPaintEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
private:
  void openNote(const QString& noteId);
  void closeNote();
  void selectBank(const QString& bankId);
  void applyTestHooks();
  Strip* m_strip;
  LeftColumn* m_left;
  RegistryColumn* m_right;
  QWidget* m_centre;
  GlobeView* m_globe;
  PdfPanel* m_pdf;
  QTimer m_blink;
  QElapsedTimer m_blinkClock;   // since the page was shown
  qint64 m_nextBlink = 500;     // when the cursors toggle next (ms on m_blinkClock)
  bool m_hooksDone = false;
};
}  // namespace banks::res
