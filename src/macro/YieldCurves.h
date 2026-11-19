#pragma once
#include <QObject>
#include <QDate>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QString>

// Sovereign yield curves from official publishers (latest day):
//   us — US Treasury daily par yield curve (14 tenors)      ez — ECB AAA euro area spot curve
//   jp — Japan Ministry of Finance JGB yields               ca — Bank of Canada T-bills + benchmark bonds
//   gb — Bank of England nominal gilt par yields (5/10/20Y)
// Other economies: OECD monthly 3-month and 10-year rates (two points, labelled as such).
//   auto& yc = macro::YieldCurves::instance();
//   yc.request("us");  connect(&yc, &macro::YieldCurves::updated, …);  macro::Curve c = yc.curve("us");
namespace macro {
struct CurvePoint { QString label; double years = 0; double yield = 0; };
struct Curve {
  QString iso2;
  QDate asOf;
  QVector<CurvePoint> pts;
  QString source;      // "US Treasury daily par yield curve"
  bool monthly = false; // OECD fallback (monthly averages, two points)
  bool ok() const { return !pts.isEmpty(); }
};

class YieldCurves : public QObject {
  Q_OBJECT
public:
  static YieldCurves& instance();
  void request(const QString& iso2);
  Curve curve(const QString& iso2) const;
  bool pending(const QString& iso2) const { return m_pending.contains(iso2.toLower()); }
signals:
  void updated(const QString& iso2);
private:
  YieldCurves();
  void put(const Curve& c);
  void fallback(const QString& iso2);
  QHash<QString, Curve> m_curves;
  QSet<QString> m_pending, m_waiting;
  QHash<QString, qint64> m_started;
};
}
