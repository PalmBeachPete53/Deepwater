#pragma once
#include <QWidget>
#include "app/Preload.h"
#include <QTimer>

namespace global { class PageScroll; }

namespace overview {

class MarketTape;
class SquawkPanel;
class RiskPanel;
class StrengthPanel;
class CentralBanksPanel;
class GlobePanel;
class RegistryPanel;
class OverviewData;

// Global › Overview: the command post. Market tape with session clocks, Squawk wire, Risk Sentiment, Currency
// Strength, Central Banks, rotating globe and Asset Registry. The panels of other packages (Squawk, Risk,
// Central Banks, Registry) are fed through the structs of OverviewFeeds.h by OverviewData (wire, risk index,
// central banks, registry services).
class OverviewPage : public QWidget, public app::Preloadable {
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit OverviewPage(QWidget* parent = nullptr);
  SquawkPanel* squawk() const { return m_squawk; }
  RiskPanel* risk() const { return m_risk; }
  CentralBanksPanel* banks() const { return m_banks; }
  RegistryPanel* registry() const { return m_registry; }
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  MarketTape* m_tape;
  SquawkPanel* m_squawk;
  RiskPanel* m_risk;
  StrengthPanel* m_strength;
  CentralBanksPanel* m_banks;
  GlobePanel* m_globe;
  RegistryPanel* m_registry;
  OverviewData* m_data;
  global::PageScroll* m_scroll;
  int layoutPanels(int w, int viewportH);
  QTimer m_blink;
};
}  // namespace overview
