#pragma once
#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QTimer>

namespace overview {
class SquawkPanel;
class RiskPanel;
class CentralBanksPanel;
class RegistryPanel;

// Connects the Overview's four panels to the other packages' services and maps their data to OverviewFeeds.h:
//   Squawk        ← wire::Wire (items, counts, newest time) + EN→FR through wire::translator() when registered,
//                   else news::Translate
//   Risk          ← positioning::RiskIndex (value, 1W / 1M ago, one-year history, components for the ⓘ text)
//   Central Banks ← macro::CentralBanks (rate, next meeting, next-meeting odds when priced, else "—" + note)
//   Registry      ← macro::registry(Group) + rateBadge() + hasPolicyRate()
class OverviewData : public QObject {
public:
  OverviewData(SquawkPanel* sq, RiskPanel* risk, CentralBanksPanel* banks, RegistryPanel* reg, QObject* parent);
  void setVisible(bool on);        // wire polling + service refresh only while the page shows
private:
  void pushSquawk();
  void translateMissing();
  void pushRisk();
  void pushBanks();
  void pushRegistry();
  SquawkPanel* m_sq;
  RiskPanel* m_risk;
  CentralBanksPanel* m_banks;
  RegistryPanel* m_reg;
  QHash<QString, QString> m_fr;    // en → fr (wire translator answers)
  QSet<QString> m_asked;
  bool m_visible = false;
  bool m_newsHooked = false;
  QTimer m_trTimer;
  QTimer m_riskTimer;   // refreshes the risk index while the page shows
};
}  // namespace overview
